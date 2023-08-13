// dear imgui: Renderer Backend for CrossRenderer, referenced of the imgui backend base for Vulkan.


#include "imgui_impl_CrossRenderer.h"
#include "Backend/ShaderCompiler.h"
#include "Frontend/RenderFrontend.h"

#include "Program.h"
using namespace BB;

constexpr size_t IMGUI_ALLOCATOR_SIZE = 1028;
constexpr size_t IMGUI_FRAME_UPLOAD_BUFFER = mbSize * 4;

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplCross_RenderDrawData()
struct ImGui_ImplCross_FrameRenderBuffers
{
    uint64_t vertexSize = 0;
    uint64_t indexSize = 0;
    RBufferHandle vertexBuffer;
    RBufferHandle indexBuffer;

    UploadBuffer uploadBuffer{ IMGUI_FRAME_UPLOAD_BUFFER, "Imgui Upload Buffer"};
};

// Each viewport will hold 1 ImGui_ImplCross_FrameRenderBuffers
struct ImGui_ImplCross_WindowRenderBuffers
{
    uint32_t            Index;
    uint32_t            Count;
    ImGui_ImplCross_FrameRenderBuffers*   FrameRenderBuffers;
};

// CrossRenderer data
struct ImGui_ImplCrossRenderer_Data
{
    uint64_t                    BufferMemoryAlignment;
    PipelineHandle              Pipeline;
    uint32_t                    Subpass;

    // Font data
    RImageHandle                fontImage;
    RSamplerHandle              fontSampler;
    RDescriptor                 fontDescriptor;
    DescriptorAllocation        descAllocation;

    uint32_t                    imageCount;
    uint32_t                    minImageCount;

    // Render buffers for main window
    ImGui_ImplCross_WindowRenderBuffers MainWindowRenderBuffers;

    ImGui_ImplCrossRenderer_Data()
    {
        memset((void*)this, 0, sizeof(*this));
        BufferMemoryAlignment = 256;
    }
};

struct ImGui_ImplBB_Data
{
    BB::WindowHandle            window;
    int                         MouseTrackedArea;   // 0: not tracked, 1: client are, 2: non-client area
    int                         MouseButtonsDown;
    int64_t                       Time;
    int64_t                       TicksPerSecond;
    ImGuiMouseCursor            LastMouseCursor;

    ImGui_ImplBB_Data() { memset((void*)this, 0, sizeof(*this)); }
};

// Forward Declarations
bool ImGui_ImplCross_CreateDeviceObjects();
void ImGui_ImplCross_DestroyDeviceObjects();
void ImGui_ImplCross_AddTexture(const RImageHandle a_Image);

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

static ImGui_ImplCrossRenderer_Data* ImGui_ImplCross_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplCrossRenderer_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static ImGui_ImplBB_Data* ImGui_ImplBB_GetPlatformData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplBB_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

//Will likely just create and after that check for resizes.
static void CreateOrResizeBuffer(RBufferHandle& a_Buffer, uint64_t& a_BufferSize, const uint64_t a_NewSize, const RENDER_BUFFER_USAGE a_Usage)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    if (a_Buffer.handle != 0)
    {
        RenderBackend::DestroyBuffer(a_Buffer);
    }


    RenderBufferCreateInfo t_CreateInfo{};
    t_CreateInfo.name = "Imgui buffer";
    t_CreateInfo.usage = a_Usage;
    t_CreateInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
    t_CreateInfo.size = a_NewSize;

    a_Buffer = RenderBackend::CreateBuffer(t_CreateInfo);
    //Do some alignment here.
    a_BufferSize = a_NewSize;
}

static void ImGui_ImplCross_SetupRenderState(const ImDrawData& a_DrawData, 
    const PipelineHandle a_Pipeline, 
    const CommandListHandle a_CmdList,
    const ImGui_ImplCross_FrameRenderBuffers& a_RenderBuffers, 
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

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scale[2];
        scale[0] = 2.0f / a_DrawData.DisplaySize.x;
        scale[1] = 2.0f / a_DrawData.DisplaySize.y;
        float translate[2];
        translate[0] = -1.0f - a_DrawData.DisplayPos.x * scale[0];
        translate[1] = -1.0f - a_DrawData.DisplayPos.y * scale[1];
        //Constant index will always be 0 if we use it. Imgui pipeline will always use it.
        RenderBackend::BindConstant(a_CmdList, 0, _countof(scale), 0, &scale);
        RenderBackend::BindConstant(a_CmdList, 0, _countof(translate), sizeof(translate) / 4, &translate);
    }
}

// Render function
void ImGui_ImplCross_RenderDrawData(const ImDrawData& a_DrawData, const BB::CommandListHandle a_CmdList, const BB::PipelineHandle a_Pipeline)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(a_DrawData.DisplaySize.x * a_DrawData.FramebufferScale.x);
    int fb_height = (int)(a_DrawData.DisplaySize.y * a_DrawData.FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    BB::PipelineHandle t_UsedPipeline = a_Pipeline;

    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    if (t_UsedPipeline == BB_INVALID_HANDLE)
        t_UsedPipeline = bd->Pipeline;

    // Allocate array to store enough vertex/index buffers
    ImGui_ImplCross_WindowRenderBuffers& wrb = bd->MainWindowRenderBuffers;

    IM_ASSERT(wrb.Count == bd->imageCount);
    wrb.Index = (wrb.Index + 1) % wrb.Count;
    ImGui_ImplCross_FrameRenderBuffers& rb = wrb.FrameRenderBuffers[wrb.Index];

    rb.uploadBuffer.Clear();

    if (a_DrawData.TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        const size_t vertex_size = a_DrawData.TotalVtxCount * sizeof(ImDrawVert);
        const size_t index_size = a_DrawData.TotalIdxCount * sizeof(ImDrawIdx);
        if (rb.vertexBuffer.ptrHandle == nullptr || rb.vertexSize < vertex_size)
            CreateOrResizeBuffer(rb.vertexBuffer, rb.vertexSize, vertex_size, RENDER_BUFFER_USAGE::VERTEX);
        if (rb.indexBuffer.ptrHandle == nullptr || rb.indexSize < index_size)
            CreateOrResizeBuffer(rb.indexBuffer, rb.indexSize, index_size, RENDER_BUFFER_USAGE::INDEX);

        UploadBufferChunk t_UpVert = rb.uploadBuffer.Alloc(vertex_size);
        UploadBufferChunk t_UpIndex = rb.uploadBuffer.Alloc(index_size);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = reinterpret_cast<ImDrawVert*>(t_UpVert.memory);
        ImDrawIdx* idx_dst = reinterpret_cast<ImDrawIdx*>(t_UpIndex.memory);
       
        for (int n = 0; n < a_DrawData.CmdListsCount; n++)
        {
            const ImDrawList* cmd_list = a_DrawData.CmdLists[n];
            memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        //copy vertex
        RenderCopyBufferInfo t_CopyInfo{};
        t_CopyInfo.src = rb.uploadBuffer.Buffer();
        t_CopyInfo.srcOffset = t_UpVert.offset;
        t_CopyInfo.dst = rb.vertexBuffer;
        t_CopyInfo.dstOffset = 0;
        t_CopyInfo.size = vertex_size;
        RenderBackend::CopyBuffer(a_CmdList, t_CopyInfo);

        //copy index
        t_CopyInfo.srcOffset = t_UpIndex.offset;
        t_CopyInfo.dst = rb.indexBuffer;
        t_CopyInfo.dstOffset = 0;
        t_CopyInfo.size = index_size;
        RenderBackend::CopyBuffer(a_CmdList, t_CopyInfo);
    }

    StartRenderingInfo t_ImguiStart;
    t_ImguiStart.viewportWidth = a_DrawData.FramebufferScale.x;
    t_ImguiStart.viewportHeight = a_DrawData.FramebufferScale.y;
    t_ImguiStart.colorLoadOp = RENDER_LOAD_OP::CLEAR;
    t_ImguiStart.colorStoreOp = RENDER_STORE_OP::STORE;
    t_ImguiStart.colorInitialLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
    t_ImguiStart.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
    RenderBackend::StartRendering(a_CmdList, t_ImguiStart);

    const uint32_t t_IsSampler = false;
    const size_t t_HeapOffset = bd->descAllocation.offset;
    RenderBackend::SetDescriptorHeapOffsets(a_CmdList, RENDER_DESCRIPTOR_SET::PER_PASS, 1, &t_IsSampler, &t_HeapOffset);

    // Setup desired CrossRenderer state
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
                ImVec2 clip_min(pcmd->ClipRect.x - clip_off.x, pcmd->ClipRect.y - clip_off.y);
                ImVec2 clip_max(pcmd->ClipRect.z - clip_off.x, pcmd->ClipRect.w - clip_off.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                ScissorInfo t_SciInfo;
                t_SciInfo.offset.x = (int32_t)(clip_min.x);
                t_SciInfo.offset.y = (int32_t)(clip_min.y);
                t_SciInfo.extent.x = (uint32_t)(clip_max.x);
                t_SciInfo.extent.y = (uint32_t)(clip_max.y);
                RenderBackend::SetScissor(a_CmdList, t_SciInfo);

                // Draw
                RenderBackend::DrawIndexed(a_CmdList, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    // Since we dynamically set our scissor lets set it back to the full viewport. 
    // This might be bad to do since this can leak into different system's code. 
    ScissorInfo t_SciInfo{};
    t_SciInfo.offset = { 0, 0 };
    t_SciInfo.extent = { (uint32_t)fb_width, (uint32_t)fb_height };
    RenderBackend::SetScissor(a_CmdList, t_SciInfo);

    EndRenderingInfo t_ImguiEnd;
    t_ImguiEnd.colorInitialLayout = t_ImguiStart.colorFinalLayout;
    t_ImguiEnd.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;
    RenderBackend::EndRendering(a_CmdList, t_ImguiEnd);
}

bool ImGui_ImplCross_CreateFontsTexture(const CommandListHandle a_CmdList, UploadBuffer& a_UploadBuffer)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    size_t upload_size = static_cast<size_t>(width) * height * 4;

    // Create the Image:
    {
        RenderImageCreateInfo t_Info = {};
        t_Info.name = "Imgui font image";
        t_Info.type = RENDER_IMAGE_TYPE::TYPE_2D;
        t_Info.format = RENDER_IMAGE_FORMAT::RGBA8_UNORM;
        t_Info.tiling = RENDER_IMAGE_TILING::OPTIMAL;
        t_Info.width = width;
        t_Info.height = height;
        t_Info.depth = 1;
        t_Info.mipLevels = 1;
        t_Info.arrayLayers = 1;
        bd->fontImage = RenderBackend::CreateImage(t_Info);
    }

    ImGui_ImplCross_AddTexture(bd->fontImage);

    // Upload to buffer then copy to Image:
    {
        {
            PipelineBarrierImageInfo t_WriteTransition;
            t_WriteTransition.srcMask = RENDER_ACCESS_MASK::NONE;
            t_WriteTransition.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
            t_WriteTransition.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
            t_WriteTransition.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
            t_WriteTransition.image = bd->fontImage;
            t_WriteTransition.layerCount = 1;
            t_WriteTransition.levelCount = 1;
            t_WriteTransition.baseArrayLayer = 0;
            t_WriteTransition.baseMipLevel = 0;
            t_WriteTransition.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
            t_WriteTransition.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;
            PipelineBarrierInfo t_PipelineInfos{};
            t_PipelineInfos.imageInfoCount = 1;
            t_PipelineInfos.imageInfos = &t_WriteTransition;
            RenderBackend::SetPipelineBarriers(a_CmdList, t_PipelineInfos);
        }

        UploadBufferChunk t_Chunk = a_UploadBuffer.Alloc(upload_size);
        memcpy(t_Chunk.memory, pixels, upload_size);

        RenderCopyBufferImageInfo t_CopyImage{};
        t_CopyImage.srcBuffer = a_UploadBuffer.Buffer();
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

        {
            PipelineBarrierImageInfo t_ReadonlyTransition;
            t_ReadonlyTransition.srcMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
            t_ReadonlyTransition.dstMask = RENDER_ACCESS_MASK::SHADER_READ;
            t_ReadonlyTransition.oldLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
            t_ReadonlyTransition.newLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
            t_ReadonlyTransition.image = bd->fontImage;
            t_ReadonlyTransition.layerCount = 1;
            t_ReadonlyTransition.levelCount = 1;
            t_ReadonlyTransition.baseArrayLayer = 0;
            t_ReadonlyTransition.baseMipLevel = 0;
            t_ReadonlyTransition.srcStage = RENDER_PIPELINE_STAGE::TRANSFER;
            t_ReadonlyTransition.dstStage = RENDER_PIPELINE_STAGE::FRAGMENT_SHADER;
            PipelineBarrierInfo t_PipelineInfos{};
            t_PipelineInfos.imageInfoCount = 1;
            t_PipelineInfos.imageInfos = &t_ReadonlyTransition;
            RenderBackend::SetPipelineBarriers(a_CmdList, t_PipelineInfos);
        }
    }

    // Store our identifier
    io.Fonts->SetTexID((ImTextureID)bd->fontDescriptor.ptrHandle);

    return true;
}

bool ImGui_ImplCross_Init(const ImGui_ImplCross_InitInfo& a_Info)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    { // WIN implementation
        // Setup backend capabilities flags
        ImGui_ImplBB_Data* bdWin = IM_NEW(ImGui_ImplBB_Data)();
        io.BackendPlatformUserData = (void*)bdWin;
        io.BackendPlatformName = "imgui_impl_BB";
        io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;         // We can honor GetMouseCursor() values (optional)
        io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;          // We can honor io.WantSetMousePos requests (optional, rarely used)

        bdWin->window = a_Info.window;
        //bd->TicksPerSecond = perf_frequency;
        //bd->Time = perf_counter;
        bdWin->LastMouseCursor = ImGuiMouseCursor_COUNT;

        // Set platform dependent data in viewport
        ImGui::GetMainViewport()->PlatformHandleRaw = a_Info.window.ptrHandle;

    }
    // Setup backend capabilities flags
    ImGui_ImplCrossRenderer_Data* bd = IM_NEW(ImGui_ImplCrossRenderer_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_crossrenderer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

    IM_ASSERT(a_Info.minImageCount >= 2);
    IM_ASSERT(a_Info.imageCount >= a_Info.minImageCount);

    bd->minImageCount = a_Info.minImageCount;
    bd->imageCount = a_Info.imageCount;

    {
        FixedArray<DescriptorBinding, 1> t_DescBinds;
        t_DescBinds[0].binding = 0;
        t_DescBinds[0].descriptorCount = 1;
        t_DescBinds[0].stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
        t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::IMAGE;

        RenderDescriptorCreateInfo t_Info{};
        t_Info.name = "Imgui descriptor";
        t_Info.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());
        bd->fontDescriptor = RenderBackend::CreateDescriptor(t_Info);
    }

    PipelineRenderTargetBlend t_BlendInfo{};
    t_BlendInfo.blendEnable = true;
    t_BlendInfo.srcBlend = RENDER_BLEND_FACTOR::SRC_ALPHA;
    t_BlendInfo.dstBlend = RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
    t_BlendInfo.blendOp = RENDER_BLEND_OP::ADD;
    t_BlendInfo.srcBlendAlpha = RENDER_BLEND_FACTOR::ONE;
    t_BlendInfo.dstBlendAlpha = RENDER_BLEND_FACTOR::ONE_MINUS_SRC_ALPHA;
    t_BlendInfo.blendOpAlpha = RENDER_BLEND_OP::ADD;

    PipelineInitInfo t_PipeInitInfo{};
    t_PipeInitInfo.name = "Imgui pipeline";
    t_PipeInitInfo.renderTargetBlends = &t_BlendInfo;
    t_PipeInitInfo.renderTargetBlendCount = 1;
    t_PipeInitInfo.blendLogicOp = RENDER_LOGIC_OP::CLEAR;
    t_PipeInitInfo.blendLogicOpEnable = false;
    t_PipeInitInfo.rasterizerState.cullMode = RENDER_CULL_MODE::NONE;
    t_PipeInitInfo.rasterizerState.frontCounterClockwise = true;

    // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
    t_PipeInitInfo.constantData.shaderStage = RENDER_SHADER_STAGE::ALL;
    //2 vec2's so 4 dwords.
    t_PipeInitInfo.constantData.dwordSize = 4;

    SamplerCreateInfo t_SamplerInfo{};
    t_SamplerInfo.name = "Imgui font sampler";
    t_SamplerInfo.addressModeU = SAMPLER_ADDRESS_MODE::REPEAT;
    t_SamplerInfo.addressModeV = SAMPLER_ADDRESS_MODE::REPEAT;
    t_SamplerInfo.addressModeW = SAMPLER_ADDRESS_MODE::REPEAT;
    t_SamplerInfo.filter = SAMPLER_FILTER::LINEAR;
    t_SamplerInfo.maxAnistoropy = 1.0f;
    t_SamplerInfo.maxLod = 100.f;
    t_SamplerInfo.minLod = -100.f;
    t_PipeInitInfo.immutableSamplers = Slice(&t_SamplerInfo, 1);

    PipelineBuilder t_Builder{ t_PipeInitInfo };
    ShaderCreateInfo t_ShaderInfos[2]{};
    VertexAttributeDesc t_VertexAttributes[3]{};
    {
        t_ShaderInfos[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
        Shader::GetShaderCodeBuffer(a_Info.vertexShader, t_ShaderInfos[0].buffer);

        t_ShaderInfos[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
        Shader::GetShaderCodeBuffer(a_Info.fragmentShader, t_ShaderInfos[1].buffer);

        t_Builder.BindShaders(BB::Slice(t_ShaderInfos, _countof(t_ShaderInfos)));
    }

    {
        t_VertexAttributes[0].semanticName = "POSITION";
        t_VertexAttributes[0].location = 0;
        t_VertexAttributes[0].format = RENDER_INPUT_FORMAT::RG32;
        t_VertexAttributes[0].offset = IM_OFFSETOF(ImDrawVert, pos);

        t_VertexAttributes[1].semanticName = "TEXCOORD";
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

    t_Builder.BindDescriptor(Render::GetGlobalDescriptorSet());
    t_Builder.BindDescriptor(bd->fontDescriptor);
    bd->Pipeline = t_Builder.BuildPipeline();

    //create framebuffers.
    {
        ImGui_ImplCross_WindowRenderBuffers& t_FrameBuffers = bd->MainWindowRenderBuffers;
        t_FrameBuffers.Index = 0;
        t_FrameBuffers.Count = bd->imageCount;
        t_FrameBuffers.FrameRenderBuffers = (ImGui_ImplCross_FrameRenderBuffers*)IM_ALLOC(sizeof(ImGui_ImplCross_FrameRenderBuffers) * t_FrameBuffers.Count);

        for (size_t i = 0; i < t_FrameBuffers.Count; i++)
        {
            //I love C++
            new (&t_FrameBuffers.FrameRenderBuffers[i])(ImGui_ImplCross_FrameRenderBuffers);
        }
    }

    return true;
}

void ImGui_ImplCross_Shutdown()
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    //delete my things here.

    ImGui_ImplBB_Data* pd = ImGui_ImplBB_GetPlatformData();
    IM_ASSERT(pd != nullptr && "No platform backend to shutdown, or already shutdown?");

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
    IM_DELETE(pd);
}

void ImGui_ImplCross_NewFrame()
{
    ImGui_ImplBB_Data* bd = ImGui_ImplBB_GetPlatformData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplCross_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    int x, y;
    GetWindowSize(bd->window, x, y);
    io.DisplaySize = ImVec2((float)(x), (float)(y));

    IM_UNUSED(bd);
}

void ImGui_ImplCross_SetMinImageCount(uint32_t min_image_count)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    IM_ASSERT(min_image_count >= 2);
    if (bd->minImageCount == min_image_count)
        return;



    //RenderBackend::WaitCommands();
    //ImGui_ImplCross_DestroyWindowRenderBuffers(&bd->MainWindowRenderBuffers);
    bd->minImageCount = min_image_count;
}

// Register a texture
// TODO: Make this a bindless descriptor and handle the free slots with a freelist.
void ImGui_ImplCross_AddTexture(const RImageHandle a_Image)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();

    {
        bd->descAllocation = Render::AllocateDescriptor(bd->fontDescriptor);

        WriteDescriptorInfos t_ImguiDescData{};
        WriteDescriptorData t_WriteData{};
        t_WriteData.binding = 0;
        t_WriteData.descriptorIndex = 0;
        t_WriteData.type = RENDER_DESCRIPTOR_TYPE::IMAGE;

        t_WriteData.image.layout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
        t_WriteData.image.image = a_Image;
        t_WriteData.image.sampler = BB_INVALID_HANDLE;

        t_ImguiDescData.data = Slice(&t_WriteData, 1);
        t_ImguiDescData.descriptorHandle = bd->fontDescriptor;
        t_ImguiDescData.allocation = bd->descAllocation;
        RenderBackend::WriteDescriptors(t_ImguiDescData);
    }
}

void ImGui_ImplCross_RemoveTexture(const RDescriptor a_Set)
{
    ImGui_ImplCrossRenderer_Data* bd = ImGui_ImplCross_GetBackendData();
    RenderBackend::DestroyDescriptor(a_Set);
}


//BB FRAMEWORK TEMPLATE, MAY CHANGE THIS.
static ImGuiKey ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(const KEYBOARD_KEY a_Key)
{
    switch (a_Key)
    {
    case KEYBOARD_KEY::_TAB: return ImGuiKey_Tab;
    case KEYBOARD_KEY::_BACKSPACE: return ImGuiKey_Backspace;
    case KEYBOARD_KEY::_SPACEBAR: return ImGuiKey_Space;
    case KEYBOARD_KEY::_RETURN: return ImGuiKey_Enter;
    case KEYBOARD_KEY::_ESCAPE: return ImGuiKey_Escape;
    case KEYBOARD_KEY::_APOSTROPHE: return ImGuiKey_Apostrophe;
    case KEYBOARD_KEY::_COMMA: return ImGuiKey_Comma;
    case KEYBOARD_KEY::_MINUS: return ImGuiKey_Minus;
    case KEYBOARD_KEY::_PERIOD: return ImGuiKey_Period;
    case KEYBOARD_KEY::_SLASH: return ImGuiKey_Slash;
    case KEYBOARD_KEY::_SEMICOLON: return ImGuiKey_Semicolon;
    case KEYBOARD_KEY::_EQUALS: return ImGuiKey_Equal;
    case KEYBOARD_KEY::_BRACKETLEFT: return ImGuiKey_LeftBracket;
    case KEYBOARD_KEY::_BACKSLASH: return ImGuiKey_Backslash;
    case KEYBOARD_KEY::_BRACKETRIGHT: return ImGuiKey_RightBracket;
    case KEYBOARD_KEY::_GRAVE: return ImGuiKey_GraveAccent;
    case KEYBOARD_KEY::_CAPSLOCK: return ImGuiKey_CapsLock;
    case KEYBOARD_KEY::_NUMPAD_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case KEYBOARD_KEY::_SHIFTLEFT: return ImGuiKey_LeftShift;
    case KEYBOARD_KEY::_CONTROLLEFT: return ImGuiKey_LeftCtrl;
    case KEYBOARD_KEY::_ALTLEFT: return ImGuiKey_LeftAlt;
    case KEYBOARD_KEY::_SHIFTRIGHT: return ImGuiKey_RightShift;
    case KEYBOARD_KEY::_0: return ImGuiKey_0;
    case KEYBOARD_KEY::_1: return ImGuiKey_1;
    case KEYBOARD_KEY::_2: return ImGuiKey_2;
    case KEYBOARD_KEY::_3: return ImGuiKey_3;
    case KEYBOARD_KEY::_4: return ImGuiKey_4;
    case KEYBOARD_KEY::_5: return ImGuiKey_5;
    case KEYBOARD_KEY::_6: return ImGuiKey_6;
    case KEYBOARD_KEY::_7: return ImGuiKey_7;
    case KEYBOARD_KEY::_8: return ImGuiKey_8;
    case KEYBOARD_KEY::_9: return ImGuiKey_9;
    case KEYBOARD_KEY::_A: return ImGuiKey_A;
    case KEYBOARD_KEY::_B: return ImGuiKey_B;
    case KEYBOARD_KEY::_C: return ImGuiKey_C;
    case KEYBOARD_KEY::_D: return ImGuiKey_D;
    case KEYBOARD_KEY::_E: return ImGuiKey_E;
    case KEYBOARD_KEY::_F: return ImGuiKey_F;
    case KEYBOARD_KEY::_G: return ImGuiKey_G;
    case KEYBOARD_KEY::_H: return ImGuiKey_H;
    case KEYBOARD_KEY::_I: return ImGuiKey_I;
    case KEYBOARD_KEY::_J: return ImGuiKey_J;
    case KEYBOARD_KEY::_K: return ImGuiKey_K;
    case KEYBOARD_KEY::_L: return ImGuiKey_L;
    case KEYBOARD_KEY::_M: return ImGuiKey_M;
    case KEYBOARD_KEY::_N: return ImGuiKey_N;
    case KEYBOARD_KEY::_O: return ImGuiKey_O;
    case KEYBOARD_KEY::_P: return ImGuiKey_P;
    case KEYBOARD_KEY::_Q: return ImGuiKey_Q;
    case KEYBOARD_KEY::_R: return ImGuiKey_R;
    case KEYBOARD_KEY::_S: return ImGuiKey_S;
    case KEYBOARD_KEY::_T: return ImGuiKey_T;
    case KEYBOARD_KEY::_U: return ImGuiKey_U;
    case KEYBOARD_KEY::_V: return ImGuiKey_V;
    case KEYBOARD_KEY::_W: return ImGuiKey_W;
    case KEYBOARD_KEY::_X: return ImGuiKey_X;
    case KEYBOARD_KEY::_Y: return ImGuiKey_Y;
    case KEYBOARD_KEY::_Z: return ImGuiKey_Z;
    default: return ImGuiKey_None;
    }
}

//On true means that imgui takes the input and doesn't give it to the engine.
bool ImGui_ImplCross_ProcessInput(const BB::InputEvent& a_InputEvent)
{
    ImGuiIO& io = ImGui::GetIO();
    if (a_InputEvent.inputType == INPUT_TYPE::MOUSE)
    {
        const BB::MouseInfo& t_Mi = a_InputEvent.mouseInfo;
        io.AddMousePosEvent(t_Mi.mousePos.x, t_Mi.mousePos.y);
        if (a_InputEvent.mouseInfo.wheelMove != 0)
        {
            io.AddMouseWheelEvent(0.0f, (float)a_InputEvent.mouseInfo.wheelMove);
        }

        constexpr int leftButton = 0;
        constexpr int rightButton = 1;
        constexpr int middleButton = 2;

        if (t_Mi.left_pressed)
            io.AddMouseButtonEvent(leftButton, true);
        if (t_Mi.right_pressed)
            io.AddMouseButtonEvent(rightButton, true);
        if (t_Mi.middle_pressed)
            io.AddMouseButtonEvent(middleButton, true);

        if (t_Mi.left_released)
            io.AddMouseButtonEvent(leftButton, false);
        if (t_Mi.right_released)
            io.AddMouseButtonEvent(rightButton, false);
        if (t_Mi.middle_released)
            io.AddMouseButtonEvent(middleButton, false);

        return io.WantCaptureMouse;
    }
    else if (a_InputEvent.inputType == INPUT_TYPE::KEYBOARD)
    {
        const BB::KeyInfo& t_Ki = a_InputEvent.keyInfo;
        const ImGuiKey t_ImguiKey = ImGui_ImplBB_KEYBOARD_KEYToImGuiKey(t_Ki.scancode);

        io.AddKeyEvent(t_ImguiKey, t_Ki.keyPressed);
        //THIS IS WRONG! It gives no UTF16 character.
        //But i'll keep it in here to test if imgui input actually works.
        io.AddInputCharacterUTF16((ImWchar16)t_Ki.scancode);
        //We want unused warnings.
        //IM_UNUSED(t_ImguiKey);

        return io.WantCaptureKeyboard;
    }

    return false;
}