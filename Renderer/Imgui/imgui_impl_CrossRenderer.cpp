// dear imgui: Renderer Backend for CrossRenderer


#include "imgui_impl_CrossRenderer.h"
#include "Backend/ShaderCompiler.h"
#include "Backend/RenderBackend.h"
using namespace BB;

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
struct ImGui_ImplVulkanH_FrameRenderBuffers
{
    uint64_t vertexSize;
    uint64_t indexSize;
    RBufferHandle vertexBuffer;
    RBufferHandle indexBuffer;
};

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct ImGui_ImplVulkanH_WindowRenderBuffers
{
    uint32_t            Index;
    uint32_t            Count;
    ImGui_ImplVulkanH_FrameRenderBuffers*   FrameRenderBuffers;
};

// Vulkan data
struct ImGui_ImplCrossRenderer_Data
{
    uint64_t                    BufferMemoryAlignment;
    PipelineHandle              Pipeline;
    uint32_t                    Subpass;

    // Font data
    RImageHandle                fontImage;
    RDescriptorHandle           fontDescriptor;
    UploadBuffer*               uploadBuffer;

    uint32_t                    imageCount;
    uint32_t                    minImageCount;

    // Render buffers for main window
    ImGui_ImplVulkanH_WindowRenderBuffers MainWindowRenderBuffers;

    ImGui_ImplCrossRenderer_Data()
    {
        memset((void*)this, 0, sizeof(*this));
        BufferMemoryAlignment = 256;
    }
};

// Forward Declarations
bool ImGui_ImplCross_CreateDeviceObjects();
void ImGui_ImplCross_DestroyDeviceObjects();
void ImGui_ImplVulkanH_DestroyFrame(VkDevice device, ImGui_ImplVulkanH_Frame* fd, const VkAllocationCallbacks* allocator);
void ImGui_ImplVulkanH_DestroyFrameSemaphores(VkDevice device, ImGui_ImplVulkanH_FrameSemaphores* fsd, const VkAllocationCallbacks* allocator);
void ImGui_ImplVulkanH_DestroyFrameRenderBuffers(VkDevice device, ImGui_ImplVulkanH_FrameRenderBuffers* buffers, const VkAllocationCallbacks* allocator);
void ImGui_ImplVulkanH_DestroyWindowRenderBuffers(VkDevice device, ImGui_ImplVulkanH_WindowRenderBuffers* buffers, const VkAllocationCallbacks* allocator);
void ImGui_ImplVulkanH_CreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, ImGui_ImplVulkanH_Window* wd, const VkAllocationCallbacks* allocator, int w, int h, uint32_t min_image_count);
void ImGui_ImplVulkanH_CreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device, ImGui_ImplVulkanH_Window* wd, uint32_t queue_family, const VkAllocationCallbacks* allocator);

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

static ImGui_ImplCrossRenderer_Data* ImGui_ImplCross_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplCrossRenderer_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static void ImGui_ImplCross_SetupRenderState(const ImDrawData& a_DrawData, 
    const PipelineHandle a_Pipeline, 
    const RecordingCommandListHandle a_CmdList, 
    const ImGui_ImplVulkanH_FrameRenderBuffers& a_RenderBuffers, 
    const RENDER_IMAGE_LAYOUT a_InitialLayout,
    const RENDER_IMAGE_LAYOUT a_FinalLayout,
    const int a_FbWidth, 
    const int a_FbHeight)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

   
    // Bind pipeline:
    {
        RenderBackend::BindPipeline(a_CmdList, a_Pipeline);
    }

    // Bind Vertex And Index Buffer:
    if (a_DrawData.TotalVtxCount > 0)
    {
        RBufferHandle vertex_buffers[1] = { a_RenderBuffers.vertexBuffer };
        uint64_t vertex_offset[1] = { 0 };
        RenderBackend::BindVertexBuffers(a_CmdList, vertex_buffers, vertex_offset, 1);
        RenderBackend::BindIndexBuffer(a_CmdList, a_RenderBuffers.indexBuffer, 0);
    }

    // Setup viewport:
    {
        StartRenderingInfo t_Info{};
        t_Info.viewportWidth = static_cast<uint32_t>(a_FbWidth);
        t_Info.viewportHeight = static_cast<uint32_t>(a_FbHeight);
        t_Info.colorInitialLayout = a_InitialLayout;
        t_Info.colorFinalLayout = a_FinalLayout;
        t_Info.colorLoadOp = RENDER_LOAD_OP::LOAD;
        t_Info.colorStoreOp = RENDER_STORE_OP::STORE;
        RenderBackend::StartRendering(a_CmdList, t_Info);
    }

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scale[2];
        scale[0] = 2.0f / a_DrawData.DisplaySize.x;
        scale[1] = 2.0f / a_DrawData.DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - a_DrawData.DisplayPos.x * scale[0];
        translate[1] = -1.0f - a_DrawData.DisplayPos.y * scale[1];
        //Index is 0 I think????
        RenderBackend::BindConstant(a_CmdList, 0, _countof(scale), 0, &scale);
        RenderBackend::BindConstant(a_CmdList, 0, _countof(translate), sizeof(translate), &translate);
        //vkCmdPushConstants(command_buffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 0, sizeof(float) * 2, scale);
        //vkCmdPushConstants(command_buffer, bd->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float) * 2, sizeof(float) * 2, translate);
    }
}

// Render function
void ImGui_ImplCross_RenderDrawData(const ImDrawData& a_DrawData, const BB::RecordingCommandListHandle a_CmdList, const BB::PipelineHandle a_Pipeline = nullptr)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(a_DrawData.DisplaySize.x * a_DrawData.FramebufferScale.x);
    int fb_height = (int)(a_DrawData.DisplaySize.y * a_DrawData.FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    BB::PipelineHandle t_UsedPipeline = a_Pipeline;

    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    if (t_UsedPipeline.ptrHandle == nullptr)
        t_UsedPipeline = bd->Pipeline;

    // Allocate array to store enough vertex/index buffers
    ImGui_ImplVulkanH_WindowRenderBuffers& wrb = bd->MainWindowRenderBuffers;
    if (wrb.FrameRenderBuffers == nullptr)
    {
        wrb.Index = 0;
        wrb.Count = bd->imageCount;
        wrb.FrameRenderBuffers = (ImGui_ImplVulkanH_FrameRenderBuffers*)IM_ALLOC(sizeof(ImGui_ImplVulkanH_FrameRenderBuffers) * wrb.Count);
        memset(wrb.FrameRenderBuffers, 0, sizeof(ImGui_ImplVulkanH_FrameRenderBuffers) * wrb.Count);
    }
    IM_ASSERT(wrb.Count == bd->imageCount);
    wrb.Index = (wrb.Index + 1) % wrb.Count;
    ImGui_ImplVulkanH_FrameRenderBuffers& rb = wrb.FrameRenderBuffers[wrb.Index];

    if (a_DrawData.TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        size_t vertex_size = a_DrawData.TotalVtxCount * sizeof(ImDrawVert);
        size_t index_size = a_DrawData.TotalIdxCount * sizeof(ImDrawIdx);
        if (rb.vertexBuffer.ptrHandle == nullptr || rb.vertexSize < vertex_size)
            CreateOrResizeBuffer(rb.vertexBuffer, rb.vertexSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if (rb.indexBuffer.ptrHandle == nullptr || rb.indexSize < index_size)
            CreateOrResizeBuffer(rb.indexBuffer, rb.indexSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = nullptr;
        ImDrawIdx* idx_dst = nullptr;
        VkResult err = vkMapMemory(v->Device, rb->VertexBufferMemory, 0, rb->VertexBufferSize, 0, (void**)(&vtx_dst));
        check_vk_result(err);
        err = vkMapMemory(v->Device, rb->IndexBufferMemory, 0, rb->IndexBufferSize, 0, (void**)(&idx_dst));
        check_vk_result(err);
        for (int n = 0; n < a_DrawData.CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = a_DrawData.CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }
    }

    // Setup desired Vulkan state
    ImGui_ImplCross_SetupRenderState(a_DrawData, t_UsedPipeline, a_CmdList, rb, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = a_DrawData.DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = a_DrawData.FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (int n = 0; n < a_DrawData.CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = a_DrawData.CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                    ImGui_ImplCross_SetupRenderState(a_DrawData, t_UsedPipeline, a_CmdList, rb, fb_width, fb_height);
                else
                    pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                ScissorInfo t_SciInfo;
                t_SciInfo.offset.x = (int32_t)(clip_min.x);
                t_SciInfo.offset.y = (int32_t)(clip_min.y);
                t_SciInfo.extent.x = (uint32_t)(clip_max.x - clip_min.x);
                t_SciInfo.extent.y = (uint32_t)(clip_max.y - clip_min.y);
                RenderBackend::SetScissor(a_CmdList, t_SciInfo);

                // Bind DescriptorSet with font or user texture
                RDescriptorHandle t_Set[1] = {(RDescriptorHandle)pcmd->TextureId};
                if (sizeof(ImTextureID) < sizeof(ImU64))
                {
                    // We don't support texture switches if ImTextureID hasn't been redefined to be 64-bit. Do a flaky check that other textures haven't been used.
                    IM_ASSERT(pcmd->TextureId == (ImTextureID)bd->fontDescriptor.ptrHandle);
                    t_Set[0] = (RDescriptorHandle)bd->fontDescriptor.ptrHandle;
                }
                RenderBackend::BindDescriptors(a_CmdList, t_Set, 1, 0, nullptr);

                // Draw
                RenderBackend::DrawIndexed(a_CmdList, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitly set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    ScissorInfo t_SciInfo;
    t_SciInfo.offset = { 0, 0 };
    t_SciInfo.extent = { (uint32_t)fb_width, (uint32_t)fb_height };
    RenderBackend::SetScissor(a_CmdList, t_SciInfo);
}

bool ImGui_ImplVulkan_CreateFontsTexture(RecordingCommandListHandle a_CmdList)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = static_cast<size_t>(width * height) * 4 * sizeof(char);

    // Create the Image:
    {
        RenderImageCreateInfo t_Info = {};
        t_Info.type = RENDER_IMAGE_TYPE::TYPE_2D;
        t_Info.format = RENDER_IMAGE_FORMAT::SRGB;
        t_Info.tiling = RENDER_IMAGE_TILING::OPTIMAL;
        t_Info.width = width;
        t_Info.height = height;
        t_Info.depth = 1;
        t_Info.mipLevels = 1;
        t_Info.arrayLayers = 1;
        bd->fontImage = RenderBackend::CreateImage(t_Info);
    }

    // Create the Descriptor Set:
    bd->fontDescriptor = (RDescriptorHandle)ImGui_ImplCross_AddTexture(bd->fontImage);

    // Upload to buffer then copy to Image:
    {
        UploadBufferChunk t_Chunk = bd->uploadBuffer->Alloc(upload_size);
        memcpy(t_Chunk.memory, pixels, upload_size);

        RenderTransitionImageInfo t_TransitionInfo{};
        t_TransitionInfo.srcMask = RENDER_ACCESS_MASK::NONE;
        t_TransitionInfo.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
        t_TransitionInfo.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
        t_TransitionInfo.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
        t_TransitionInfo.image = bd->fontImage;
        t_TransitionInfo.layerCount = 1;
        t_TransitionInfo.levelCount = 1;
        t_TransitionInfo.baseArrayLayer = 0;
        t_TransitionInfo.baseMipLevel = 0;
        t_TransitionInfo.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
        t_TransitionInfo.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;

        RenderBackend::TransitionImage(a_CmdList, t_TransitionInfo);

        RenderCopyBufferImageInfo t_CopyImage{};
        t_CopyImage.srcBuffer = bd->uploadBuffer->Buffer();
        t_CopyImage.srcBufferOffset = static_cast<uint32_t>(t_Chunk.offset);
        t_CopyImage.dstImage = bd->fontImage;
        t_CopyImage.dstImageInfo.sizeX = static_cast<uint32_t>(width);
        t_CopyImage.dstImageInfo.sizeY = static_cast<uint32_t>(height);
        t_CopyImage.dstImageInfo.sizeZ = 1;
        t_CopyImage.dstImageInfo.offsetX = 0;
        t_CopyImage.dstImageInfo.offsetY = 0;
        t_CopyImage.dstImageInfo.offsetZ = 0;
        t_CopyImage.dstImageInfo.layerCount = 1;
        t_CopyImage.dstImageInfo.mipLevel = 0;
        t_CopyImage.dstImageInfo.baseArrayLayer = 0;
        t_CopyImage.dstImageInfo.layout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
        RenderBackend::CopyBufferImage(a_CmdList, t_CopyImage);

        //reset to 0 again.
        t_TransitionInfo = {};
        t_TransitionInfo.srcMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
        t_TransitionInfo.dstMask = RENDER_ACCESS_MASK::SHADER_READ;
        t_TransitionInfo.oldLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
        t_TransitionInfo.newLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
        t_TransitionInfo.image = bd->fontImage;
        t_TransitionInfo.layerCount = 1;
        t_TransitionInfo.levelCount = 1;
        t_TransitionInfo.baseArrayLayer = 0;
        t_TransitionInfo.baseMipLevel = 0;
        t_TransitionInfo.srcStage = RENDER_PIPELINE_STAGE::TRANSFER;
        t_TransitionInfo.dstStage = RENDER_PIPELINE_STAGE::FRAGMENT_SHADER;

        RenderBackend::TransitionImage(a_CmdList, t_TransitionInfo);
    }

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->fontDescriptor.ptrHandle);

    return true;
}

static void ImGui_ImplCross_CreatePipeline(PipelineHandle& a_Pipeline, uint32_t subpass, RENDER_IMAGE_FORMAT a_RenderingFormat)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    PipelineInitInfo t_PipeInfo{};
    PipelineBuilder t_Builder{t_PipeInfo};
    Shader::ShaderCodeHandle t_ShaderHandles[2];
    {
        const wchar_t* t_ShaderPath[2]{};
        t_ShaderPath[0] = L"Resources/Shaders/HLSLShaders/ImguiVert.hlsl";
        t_ShaderPath[1] = L"Resources/Shaders/HLSLShaders/ImguiFrag.hlsl";

        Shader::ShaderCodeHandle t_ShaderHandles[2];
        t_ShaderHandles[0] = Shader::CompileShader(
            t_ShaderPath[0],
            L"main",
            RENDER_SHADER_STAGE::VERTEX,
            s_RendererInst.renderAPI);
        t_ShaderHandles[1] = Shader::CompileShader(
            t_ShaderPath[1],
            L"main",
            RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
            s_RendererInst.renderAPI);

        ShaderCreateInfo t_ShaderInfos[2];
        t_ShaderInfos[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
        Shader::GetShaderCodeBuffer(t_ShaderHandles[0], t_ShaderInfos[0].buffer);

        t_ShaderInfos[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
        Shader::GetShaderCodeBuffer(t_ShaderHandles[1], t_ShaderInfos[1].buffer);

        t_Builder.BindShaders(BB::Slice(t_ShaderInfos, _countof(t_ShaderInfos)));
    }

    {
        VertexAttributeDesc t_VertexAttributes[3]{};
        t_VertexAttributes[0].semanticName = "POSITION";
        t_VertexAttributes[0].location = 0;
        t_VertexAttributes[0].format = RENDER_INPUT_FORMAT::RG32;
        t_VertexAttributes[0].offset = IM_OFFSETOF(ImDrawVert, pos);

        t_VertexAttributes[2].semanticName = "UV";
        t_VertexAttributes[1].location = 1;
        t_VertexAttributes[1].format = RENDER_INPUT_FORMAT::RG32;
        t_VertexAttributes[1].offset = IM_OFFSETOF(ImDrawVert, uv);

        t_VertexAttributes[2].semanticName = "COLOR";
        t_VertexAttributes[2].location = 2;
        t_VertexAttributes[2].format = RENDER_INPUT_FORMAT::RGBA8;
        t_VertexAttributes[2].offset = IM_OFFSETOF(ImDrawVert, col);

        PipelineAttributes t_Attribs{};
        t_Attribs.stride = sizeof(ImDrawVert);
        t_Attribs.attributes = Slice(t_VertexAttributes, 3);

        t_Builder.BindAttributes(t_Attribs);
    }

    t_Builder.BindDescriptor(bd->fontDescriptor);
    t_Builder.BuildPipeline();

    for (size_t i = 0; i < _countof(t_ShaderHandles); i++)
    {
        Shader::ReleaseShaderCode(t_ShaderHandles[i]);
    }

    ImGui_ImplVulkan_CreateShaderModules(device, allocator);

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = bd->ShaderModuleVert;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = bd->ShaderModuleFrag;
    stage[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = (MSAASamples != 0) ? MSAASamples : VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)IM_ARRAYSIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    //BB CUSTOM!
    //We do dynamic rendering to avoid having to handle renderpasses and such.
    VkPipelineRenderingCreateInfo t_DynamicRenderingInfo{};
    t_DynamicRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    t_DynamicRenderingInfo.colorAttachmentCount = 1;
    t_DynamicRenderingInfo.pColorAttachmentFormats = &renderingFormat;
    t_DynamicRenderingInfo.pNext = nullptr;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = bd->PipelineCreateFlags;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &ia_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = bd->PipelineLayout;
    info.renderPass = nullptr;
    info.subpass = subpass;
    info.pNext = &t_DynamicRenderingInfo;
    VkResult err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, allocator, pipeline);
    check_vk_result(err);
}

//rendering format is BB extension for dynamic_rendering
bool ImGui_ImplVulkan_CreateDeviceObjects()
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    if (!bd->)
    {
        // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(v->Device, &info, v->Allocator, &bd->FontSampler);
        check_vk_result(err);
    }

    if (!bd->DescriptorSetLayout)
    {
        VkDescriptorSetLayoutBinding binding[1] = {};
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(v->Device, &info, v->Allocator, &bd->DescriptorSetLayout);
        check_vk_result(err);
    }

    if (!bd->PipelineLayout)
    {
        // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
        VkPushConstantRange push_constants[1] = {};
        push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constants[0].offset = sizeof(float) * 0;
        push_constants[0].size = sizeof(float) * 4;
        VkDescriptorSetLayout set_layout[1] = { bd->DescriptorSetLayout };
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = push_constants;
        err = vkCreatePipelineLayout(v->Device, &layout_info, v->Allocator, &bd->PipelineLayout);
        check_vk_result(err);
    }

    ImGui_ImplVulkan_CreatePipeline(v->Device, v->Allocator, v->PipelineCache, v->MSAASamples, &bd->Pipeline, bd->Subpass, dynamicRenderingFormat);

    return true;
}

bool ImGui_ImplCross_Init(ImGui_ImplCross_InitInfo* info)
{
    IM_ASSERT(g_FunctionsLoaded && "Need to call ImGui_ImplVulkan_LoadFunctions() if IMGUI_IMPL_VULKAN_NO_PROTOTYPES or VK_NO_PROTOTYPES are set!");

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplCrossRenderer_Data* bd = IM_NEW(ImGui_ImplCrossRenderer_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_vulkan";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    IM_ASSERT(info->minImageCount >= 2);
    IM_ASSERT(info->imageCount >= info->minImageCount);

    ImGui_ImplVulkan_CreateDeviceObjects();

    return true;
}

void ImGui_ImplVulkan_Shutdown()
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    //delete my things here.

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    IM_DELETE(bd);
}

void ImGui_ImplVulkan_NewFrame()
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplCross_Init()?");
    IM_UNUSED(bd);
}

void ImGui_ImplVulkan_SetMinImageCount(uint32_t min_image_count)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(min_image_count >= 2);
    if (bd->minImageCount == min_image_count)
        return;

    RenderBackend::WaitGPUReady();
    ImGui_ImplVulkanH_DestroyWindowRenderBuffers(&bd->MainWindowRenderBuffers);
    bd->minImageCount = min_image_count;
}

// Register a texture
// TODO: Make this a bindless descriptor and handle the free slots with a freelist.
RDescriptorHandle ImGui_ImplCross_AddTexture(const RImageHandle a_Image)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    // Create Descriptor Set:
    RDescriptorHandle t_Set;
    {
        DescriptorBinding t_Binding{};
        t_Binding.binding = 0;
        t_Binding.descriptorCount = 1;
        t_Binding.stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
        t_Binding.type = RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER;

        RenderDescriptorCreateInfo t_Info{};
        t_Info.bindingSet = RENDER_BINDING_SET::PER_FRAME;
        t_Info.bindings = BB::Slice(&t_Binding, 1);
        t_Set = RenderBackend::CreateDescriptor(t_Info);
    }

    // Update the Descriptor Set:
    {
        UpdateDescriptorImageInfo t_ImageUpdate{};
        t_ImageUpdate.binding = 0;
        t_ImageUpdate.descriptorIndex = 0;
        t_ImageUpdate.set = t_Set;
        t_ImageUpdate.type = RENDER_DESCRIPTOR_TYPE::COMBINED_IMAGE_SAMPLER;

        t_ImageUpdate.imageLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
        t_ImageUpdate.image = a_Image;

        RenderBackend::UpdateDescriptorImage(t_ImageUpdate);
    }
    return t_Set;
}

void ImGui_ImplVulkan_RemoveTexture(const RDescriptorHandle a_Set)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    RenderBackend::DestroyDescriptor(a_Set);
}