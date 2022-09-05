#pragma once
#pragma warning (push, 0)
#include <Vulkan/vulkan.h>
#pragma warning (pop)

namespace VkInit
{
	inline VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfoEXT(PFN_vkDebugUtilsMessengerCallbackEXT a_Callback)
	{
		VkDebugUtilsMessengerCreateInfoEXT t_CreateInfo{};
		t_CreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		t_CreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		t_CreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		t_CreateInfo.pfnUserCallback = a_Callback;
		t_CreateInfo.pUserData = nullptr;
		return t_CreateInfo;
	}

	inline VkDescriptorBufferInfo DescriptorBufferInfo(VkBuffer& r_UniformBuffer, VkDeviceSize a_Offset, VkDeviceSize a_BufferSize)
	{
		VkDescriptorBufferInfo t_BufferInfo{};
		t_BufferInfo.buffer = r_UniformBuffer;
		t_BufferInfo.offset = a_Offset;
		t_BufferInfo.range = a_BufferSize;

		return t_BufferInfo;
	}

	inline VkDescriptorImageInfo DescriptorImageInfo(VkImageLayout a_ImageLayout, VkImageView& r_ImageView, VkSampler& r_Sampler)
	{
		VkDescriptorImageInfo t_ImageInfo{};
		t_ImageInfo.imageLayout = a_ImageLayout;
		t_ImageInfo.imageView = r_ImageView;
		t_ImageInfo.sampler = r_Sampler;
		
		return t_ImageInfo;
	}


	//Layout Creation.
	inline VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType a_Type, VkShaderStageFlags a_Flags, uint32_t a_Binding)
	{
		VkDescriptorSetLayoutBinding t_LayoutBinding{};
		t_LayoutBinding.binding = a_Binding;
		t_LayoutBinding.descriptorType = a_Type;
		t_LayoutBinding.descriptorCount = 1;
		t_LayoutBinding.stageFlags = a_Flags;
		t_LayoutBinding.pImmutableSamplers = nullptr; // Optional
		return t_LayoutBinding;
	}


	inline VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(uint32_t a_BindingCount, VkDescriptorSetLayoutBinding* a_LayoutBinding, 
		VkDescriptorSetLayoutCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkDescriptorSetLayoutCreateInfo t_LayoutInfo{};
		t_LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		t_LayoutInfo.bindingCount = a_BindingCount;
		t_LayoutInfo.pBindings = a_LayoutBinding;
		t_LayoutInfo.pNext = a_Next;
		t_LayoutInfo.flags = a_Flags;
		return t_LayoutInfo;
	}


	//Shader Creation
	inline VkShaderModuleCreateInfo ShaderModuleCreateInfo(const void* a_ShaderCode, const size_t a_CodeSize,
		VkShaderModuleCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkShaderModuleCreateInfo t_CreateInfo{};
		t_CreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		t_CreateInfo.codeSize = a_CodeSize;
		t_CreateInfo.pCode = reinterpret_cast<const uint32_t*>(a_ShaderCode);
		t_CreateInfo.flags = a_Flags;
		t_CreateInfo.pNext = a_Next;
		return t_CreateInfo;
	}

	inline VkFramebufferCreateInfo FramebufferCreateInfo()
	{
		VkFramebufferCreateInfo t_FramebufferInfo{};
		t_FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

		return t_FramebufferInfo;
	}

	inline VkCommandPoolCreateInfo CommandPoolCreateInfo(uint32_t a_QueueFamilyIndex, 
		VkCommandPoolCreateFlags a_Flags, const void* a_Next = nullptr)
	{
		VkCommandPoolCreateInfo t_CommandPoolCreateInfo{};
		t_CommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		t_CommandPoolCreateInfo.queueFamilyIndex = a_QueueFamilyIndex;
		t_CommandPoolCreateInfo.flags = a_Flags;
		t_CommandPoolCreateInfo.pNext = a_Next;
		return t_CommandPoolCreateInfo;
	}

	inline VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool a_CommandPool, uint32_t a_Count, 
		VkCommandBufferLevel a_Level, const void* a_Next = nullptr)
	{
		VkCommandBufferAllocateInfo t_CommandBufferAllocateInfo{};
		t_CommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		t_CommandBufferAllocateInfo.commandPool = a_CommandPool;
		t_CommandBufferAllocateInfo.commandBufferCount = a_Count;
		t_CommandBufferAllocateInfo.level = a_Level;
		t_CommandBufferAllocateInfo.pNext = a_Next;
		return t_CommandBufferAllocateInfo;
	}

	inline VkCommandBufferBeginInfo CommandBufferBeginInfo(const VkCommandBufferInheritanceInfo* a_InheritInfo,
		const VkCommandBufferUsageFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkCommandBufferBeginInfo t_CommandBufferBeginInfo{};
		t_CommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		t_CommandBufferBeginInfo.pInheritanceInfo = a_InheritInfo;
		t_CommandBufferBeginInfo.flags = a_Flags;
		t_CommandBufferBeginInfo.pNext = a_Next;

		return t_CommandBufferBeginInfo;
	}

	inline VkRenderPassBeginInfo RenderPassBeginInfo(VkRenderPass a_RenderPass, 
		VkFramebuffer a_FrameBuffer, VkRect2D a_RenderArea, 
		uint32_t a_ClearValueCount, VkClearValue* a_ClearValue, const void* a_Next = nullptr)
	{
		VkRenderPassBeginInfo t_RenderPassBeginInfo{};
		t_RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		t_RenderPassBeginInfo.renderPass = a_RenderPass;
		t_RenderPassBeginInfo.framebuffer = a_FrameBuffer;
		t_RenderPassBeginInfo.renderArea = a_RenderArea;
		t_RenderPassBeginInfo.clearValueCount = a_ClearValueCount;
		t_RenderPassBeginInfo.pClearValues = a_ClearValue;
		t_RenderPassBeginInfo.pNext = a_Next;

		return t_RenderPassBeginInfo;
	}

	inline VkAttachmentDescription AttachmentDescription(VkFormat a_Format, VkSampleCountFlagBits a_Samples,
		VkAttachmentLoadOp a_LoadOp, VkAttachmentStoreOp a_StoreOp,
		VkAttachmentLoadOp a_StencilLoadOp, VkAttachmentStoreOp a_StencilStoreOp,
		VkImageLayout a_InitialLayout, VkImageLayout a_FinalLayout,
		VkAttachmentDescriptionFlags a_Flags = 0)
	{
		VkAttachmentDescription t_Attachment{};
		t_Attachment.format = a_Format;
		t_Attachment.samples = a_Samples;
		t_Attachment.loadOp = a_LoadOp;
		t_Attachment.storeOp = a_StoreOp;
		t_Attachment.stencilLoadOp = a_StencilLoadOp;
		t_Attachment.stencilStoreOp = a_StencilStoreOp;
		t_Attachment.initialLayout = a_InitialLayout;
		t_Attachment.finalLayout = a_FinalLayout;
		t_Attachment.flags = a_Flags;

		return t_Attachment;
	}

	inline VkAttachmentReference AttachmentReference(uint32_t a_Attachment,
		VkImageLayout a_ImageLayout)
	{
		VkAttachmentReference t_AttachmentReference{};
		t_AttachmentReference.attachment = a_Attachment;
		t_AttachmentReference.layout = a_ImageLayout;

		return t_AttachmentReference;
	}

	inline VkSubpassDescription SubpassDescription(VkPipelineBindPoint a_BindPoint,
		uint32_t a_ColorAttachCount, VkAttachmentReference* a_ColorAttachs,
		VkAttachmentReference* a_StencilAttach,
		uint32_t a_PreserveAttachCount = 0, uint32_t* a_PreserveAttachs = nullptr,
		uint32_t a_InputAttachCount = 0, VkAttachmentReference* a_InputAttachs = nullptr,
		VkAttachmentReference* a_ResolveAttachs = nullptr,
		VkSubpassDescriptionFlags a_Flags = 0)
	{
		VkSubpassDescription t_SubpassDescription{};
		t_SubpassDescription.pipelineBindPoint = a_BindPoint;
		t_SubpassDescription.colorAttachmentCount = a_ColorAttachCount;
		t_SubpassDescription.pColorAttachments = a_ColorAttachs;
		t_SubpassDescription.pDepthStencilAttachment = a_StencilAttach;
		t_SubpassDescription.preserveAttachmentCount = a_PreserveAttachCount;
		t_SubpassDescription.pPreserveAttachments = a_PreserveAttachs;
		t_SubpassDescription.inputAttachmentCount = a_InputAttachCount;
		t_SubpassDescription.pInputAttachments = a_InputAttachs;
		t_SubpassDescription.pResolveAttachments = a_ResolveAttachs;
		t_SubpassDescription.flags = a_Flags;

		return t_SubpassDescription;
	}

	inline VkSubpassDependency SubpassDependancy(uint32_t a_SrcSubPass, uint32_t a_DstSubPass,
		VkPipelineStageFlags a_SrcStageMask, VkAccessFlags a_SrcAccessMask,
		VkPipelineStageFlags a_DstStageMask, VkAccessFlags a_DstAccessMask,
		VkDependencyFlags a_Flags = 0)
	{
		VkSubpassDependency t_SubpassDependancy{};
		t_SubpassDependancy.srcSubpass = a_SrcSubPass;
		t_SubpassDependancy.dstSubpass = a_DstSubPass;
		t_SubpassDependancy.srcStageMask = a_SrcStageMask;
		t_SubpassDependancy.srcAccessMask = a_SrcAccessMask;
		t_SubpassDependancy.dstStageMask = a_DstStageMask;
		t_SubpassDependancy.dstAccessMask = a_DstAccessMask;
		t_SubpassDependancy.dependencyFlags = a_Flags;

		return t_SubpassDependancy;
	}

	inline VkRenderPassCreateInfo RenderPassCreateInfo(
		uint32_t a_AttachCount, VkAttachmentDescription* a_Attachs, 
		uint32_t a_SubPassCount, VkSubpassDescription* a_SubPasses,
		uint32_t a_DependancyCount, VkSubpassDependency* a_Dependancies,
		VkRenderPassCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkRenderPassCreateInfo t_RenderPassInfo{};
		t_RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		t_RenderPassInfo.attachmentCount = a_AttachCount;
		t_RenderPassInfo.pAttachments = a_Attachs;
		t_RenderPassInfo.subpassCount = a_SubPassCount;
		t_RenderPassInfo.pSubpasses = a_SubPasses;
		t_RenderPassInfo.dependencyCount = a_DependancyCount;
		t_RenderPassInfo.pDependencies = a_Dependancies;
		t_RenderPassInfo.flags = a_Flags;
		t_RenderPassInfo.pNext = a_Next;

		return t_RenderPassInfo;
	}

	inline VkSemaphoreCreateInfo SemaphoreCreationInfo(
		VkSemaphoreCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkSemaphoreCreateInfo t_SemaphoreInfo{};
		t_SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		t_SemaphoreInfo.flags = a_Flags;
		t_SemaphoreInfo.pNext = a_Next;

		return t_SemaphoreInfo;
	}

	inline VkFenceCreateInfo FenceCreationInfo(VkFenceCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkFenceCreateInfo t_FenceInfo{};
		t_FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		t_FenceInfo.flags = a_Flags;
		t_FenceInfo.pNext = a_Next;

		return t_FenceInfo;
	}

	inline VkViewport ViewPort(float a_Width, float a_Height, float a_minDepth, float a_maxDepth)
	{
		VkViewport t_Viewport{};
		t_Viewport.width = a_Width;
		t_Viewport.height = a_Height;
		t_Viewport.minDepth = a_minDepth;
		t_Viewport.maxDepth = a_maxDepth;

		return t_Viewport;
	}

	inline VkRect2D Rect2D(int32_t a_Width, int32_t a_Height, VkExtent2D a_Extend2D)
	{
		VkRect2D t_Rect2D{};
		t_Rect2D.offset.x = a_Width;
		t_Rect2D.offset.y = a_Height;
		t_Rect2D.extent = a_Extend2D;

		return t_Rect2D;
	}

	inline VkRect2D Rect2D(int32_t a_Width, int32_t a_Height, int32_t a_OffsetX, int32_t a_OffsetY)
	{
		VkRect2D t_Rect2D{};
		t_Rect2D.offset.x = a_Width;
		t_Rect2D.offset.y = a_Height;
		t_Rect2D.extent.width = a_OffsetX;
		t_Rect2D.extent.height = a_OffsetY;

		return t_Rect2D;
	}

	inline VkImageCreateInfo ImageCreateInfo()
	{
		VkImageCreateInfo t_ImageInfo{};
		t_ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		return t_ImageInfo;
	}

#pragma region Pipeline Creation

	inline VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits a_Stage, 
		VkShaderModule a_ShaderModule, const char* a_Name, const VkSpecializationInfo* a_SpecInfo, 
		VkPipelineShaderStageCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineShaderStageCreateInfo t_ShaderStageInfo{};
		t_ShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		t_ShaderStageInfo.stage = a_Stage;
		t_ShaderStageInfo.module = a_ShaderModule;
		t_ShaderStageInfo.pName = a_Name;
		t_ShaderStageInfo.pSpecializationInfo = a_SpecInfo;
		t_ShaderStageInfo.flags = a_Flags;
		t_ShaderStageInfo.pNext = a_Next;
		return t_ShaderStageInfo;
	}

	inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo()
	{
		VkPipelineVertexInputStateCreateInfo t_VertexInputInfo{};
		t_VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return t_VertexInputInfo;
	}

	inline VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo(
		uint32_t a_BindingCount, VkVertexInputBindingDescription* a_Bindings,
		uint32_t a_AttributeCount, VkVertexInputAttributeDescription *a_Attributes,
		VkPipelineVertexInputStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineVertexInputStateCreateInfo t_VertexInputInfo{};
		t_VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		t_VertexInputInfo.vertexBindingDescriptionCount = a_BindingCount;
		t_VertexInputInfo.pVertexBindingDescriptions = a_Bindings;
		t_VertexInputInfo.vertexAttributeDescriptionCount = a_AttributeCount;
		t_VertexInputInfo.pVertexAttributeDescriptions = a_Attributes;
		t_VertexInputInfo.flags = a_Flags;
		t_VertexInputInfo.pNext = a_Next;
		return t_VertexInputInfo;
	}

	inline VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(
		VkPrimitiveTopology a_Topology, VkBool32 a_PrimitiveRestartEnable, 
		VkPipelineInputAssemblyStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineInputAssemblyStateCreateInfo t_InputAssembly{};
		t_InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		t_InputAssembly.topology = a_Topology;
		t_InputAssembly.primitiveRestartEnable = a_PrimitiveRestartEnable;
		t_InputAssembly.flags = a_Flags;
		t_InputAssembly.pNext = a_Next;
		return t_InputAssembly;
	}

	inline VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(
		uint32_t a_ViewportCount, VkViewport* a_ViewPorts, uint32_t a_ScissorCount, VkRect2D* a_Scissor,
		VkPipelineInputAssemblyStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineViewportStateCreateInfo t_ViewportState{};
		t_ViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		t_ViewportState.viewportCount = a_ViewportCount;
		t_ViewportState.pViewports = a_ViewPorts;
		t_ViewportState.scissorCount = a_ScissorCount;
		t_ViewportState.pScissors = a_Scissor;
		t_ViewportState.flags = a_Flags;
		t_ViewportState.pNext = a_Next;
		return t_ViewportState;
	}

	inline VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(
		VkBool32 a_DepthClampEnable, VkBool32 a_DepthBiasEnable, VkBool32 a_RasterizerDiscardEnable, 
		VkPolygonMode a_PolygonMode, VkCullModeFlags a_CullMode, VkFrontFace a_FrontFace, 
		float a_LineWidth = 1.0f, float a_DepthBiasConstantFactor = 0.0f, float a_DepthBiasClamp = 0.0f,
		float a_DepthBiasSlopeFactor = 0.0f, 
		VkPipelineRasterizationStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineRasterizationStateCreateInfo t_Rasterizer{};
		t_Rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		t_Rasterizer.depthClampEnable = a_DepthClampEnable;
		t_Rasterizer.depthBiasEnable = a_DepthBiasEnable;
		t_Rasterizer.rasterizerDiscardEnable = a_RasterizerDiscardEnable;
		t_Rasterizer.polygonMode = a_PolygonMode;
		t_Rasterizer.cullMode = a_CullMode;
		t_Rasterizer.frontFace = a_FrontFace;
		t_Rasterizer.lineWidth = a_LineWidth;
		t_Rasterizer.depthBiasConstantFactor = a_DepthBiasConstantFactor; // Optional
		t_Rasterizer.depthBiasClamp = a_DepthBiasClamp; // Optional
		t_Rasterizer.depthBiasSlopeFactor = a_DepthBiasSlopeFactor; // Optional
		t_Rasterizer.flags = a_Flags;
		t_Rasterizer.pNext = a_Next;
		return t_Rasterizer;
	}

	inline VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(
		VkBool32 a_SampleShadingEnable, VkSampleCountFlagBits a_RasterSamples,
		VkBool32 a_AlphaToCoverageEnable = VK_FALSE, VkBool32 a_AlphaToOneEnable = VK_FALSE,
		float a_MinSampleShading = 1.0f, VkSampleMask* a_SampleMask = nullptr,
		VkPipelineMultisampleStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineMultisampleStateCreateInfo t_Multisampling{};
		t_Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		t_Multisampling.sampleShadingEnable = a_SampleShadingEnable;
		t_Multisampling.rasterizationSamples = a_RasterSamples;
		t_Multisampling.alphaToCoverageEnable = a_AlphaToCoverageEnable; // Optional
		t_Multisampling.alphaToOneEnable = a_AlphaToOneEnable; // Optional
		t_Multisampling.minSampleShading = a_MinSampleShading; // Optional
		t_Multisampling.pSampleMask = a_SampleMask; // Optional
		t_Multisampling.flags = a_Flags;
		t_Multisampling.pNext = a_Next;
		return t_Multisampling;
	}

	inline VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(
		VkBool32 a_DepthTestEnable, VkBool32 a_DepthWriteEnable, VkBool32 a_StencilEnable, VkBool32 a_DepthBoundsTestEnable, 
		VkCompareOp a_DepthCompareOp, float a_MinDepthBounds = 0.0f, float a_MaxDepthBounds = 1.0f,
		VkStencilOpState a_Front = {}, VkStencilOpState a_Back = {}, 
		VkPipelineDepthStencilStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineDepthStencilStateCreateInfo t_DepthStencil{};
		t_DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		t_DepthStencil.depthTestEnable = a_DepthTestEnable;
		t_DepthStencil.depthWriteEnable = a_DepthWriteEnable;
		t_DepthStencil.stencilTestEnable = a_StencilEnable;
		t_DepthStencil.depthBoundsTestEnable = a_DepthBoundsTestEnable;
		t_DepthStencil.depthCompareOp = a_DepthCompareOp;
		t_DepthStencil.minDepthBounds = a_MinDepthBounds; // Optional
		t_DepthStencil.maxDepthBounds = a_MaxDepthBounds; // Optional
		t_DepthStencil.front = a_Front; // Optional
		t_DepthStencil.back = a_Back; // Optional
		t_DepthStencil.flags = a_Flags;
		t_DepthStencil.pNext = a_Next;
		return t_DepthStencil;
	}

	inline VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(
	VkColorComponentFlags a_colorWriteMask, VkBool32 a_BlendEnable, 
		VkBlendFactor a_SrcColor = VK_BLEND_FACTOR_ONE, VkBlendFactor a_dstColor = VK_BLEND_FACTOR_ZERO, VkBlendOp a_ColorBlend = VK_BLEND_OP_ADD,
		VkBlendFactor a_SrcAlpha = VK_BLEND_FACTOR_ONE, VkBlendFactor a_DstAlpha = VK_BLEND_FACTOR_ZERO, VkBlendOp a_AlphaBlend = VK_BLEND_OP_ADD)
	{
		VkPipelineColorBlendAttachmentState t_ColorBlendAttachment{};
		t_ColorBlendAttachment.colorWriteMask = a_colorWriteMask;
		t_ColorBlendAttachment.blendEnable = a_BlendEnable;
		t_ColorBlendAttachment.srcColorBlendFactor = a_SrcColor; // Optional
		t_ColorBlendAttachment.dstColorBlendFactor = a_dstColor; // Optional
		t_ColorBlendAttachment.colorBlendOp = a_ColorBlend; // Optional
		t_ColorBlendAttachment.srcAlphaBlendFactor = a_SrcAlpha; // Optional
		t_ColorBlendAttachment.dstAlphaBlendFactor = a_DstAlpha; // Optional
		t_ColorBlendAttachment.alphaBlendOp = a_AlphaBlend; // Optional
		return t_ColorBlendAttachment;
	}

	inline VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(
		VkBool32 a_LogicOpEnable, VkLogicOp a_LogicOp, uint32_t a_AttachmentCount, 
		VkPipelineColorBlendAttachmentState* a_Attachments, float* a_BlendConstants = nullptr,
		VkPipelineColorBlendStateCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineColorBlendStateCreateInfo t_ColorBlending{};
		t_ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		t_ColorBlending.logicOpEnable = a_LogicOpEnable;
		t_ColorBlending.logicOp = a_LogicOp;
		t_ColorBlending.attachmentCount = a_AttachmentCount;
		t_ColorBlending.pAttachments = a_Attachments;
		if (a_BlendConstants != nullptr)
		{
			t_ColorBlending.blendConstants[0] = a_BlendConstants[0]; // Optional
			t_ColorBlending.blendConstants[1] = a_BlendConstants[1]; // Optional
			t_ColorBlending.blendConstants[2] = a_BlendConstants[2]; // Optional
			t_ColorBlending.blendConstants[3] = a_BlendConstants[3]; // Optional
		}
		t_ColorBlending.flags = a_Flags;
		t_ColorBlending.pNext = a_Next;
		return t_ColorBlending;
	}

	inline VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(
		uint32_t a_LayoutCount = 0, VkDescriptorSetLayout* a_Layouts = nullptr,
		uint32_t a_PushConstantCount = 0, VkPushConstantRange* a_PushConstantRanges = nullptr,
		VkPipelineLayoutCreateFlags a_Flags = 0, const void* a_Next = nullptr)
	{
		VkPipelineLayoutCreateInfo t_LayoutCreateInfo{};
		t_LayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		t_LayoutCreateInfo.setLayoutCount = a_LayoutCount;
		t_LayoutCreateInfo.pSetLayouts = a_Layouts;
		t_LayoutCreateInfo.pushConstantRangeCount = a_PushConstantCount;
		t_LayoutCreateInfo.pPushConstantRanges = a_PushConstantRanges;
		t_LayoutCreateInfo.flags = a_Flags;
		t_LayoutCreateInfo.pNext = a_Next;
		return t_LayoutCreateInfo;
	}

#pragma endregion Pipeline Creation
}