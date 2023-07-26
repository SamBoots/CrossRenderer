#include "RenderFrontend.h"
#include "ShaderCompiler.h"

#include "Transform.h"
#include "OS/Program.h"
#include "imgui_impl_CrossRenderer.h"

#include "Storage/Slotmap.h"
#include "Editor.h"

#include "AssetLoader.hpp"

using namespace BB;
using namespace BB::Render;

void LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path);
FreelistAllocator_t s_SystemAllocator{ mbSize * 4, "Render Frontend freelist allocator" };
static TemporaryAllocator s_TempAllocator{ s_SystemAllocator };

struct TextureManager
{
	TextureManager(UploadBuffer& a_UploadBuffer, RecordingCommandListHandle t_CommandList)
	{
		RenderImageCreateInfo t_ImageInfo{};
		t_ImageInfo.name = "debug texture";
		t_ImageInfo.width = 1;
		t_ImageInfo.height = 1;
		t_ImageInfo.depth = 1;
		t_ImageInfo.arrayLayers = 1;
		t_ImageInfo.mipLevels = 1;
		t_ImageInfo.tiling = RENDER_IMAGE_TILING::OPTIMAL;
		t_ImageInfo.type = RENDER_IMAGE_TYPE::TYPE_2D;
		t_ImageInfo.format = RENDER_IMAGE_FORMAT::RGBA8_SRGB;

		debugTexture = RenderBackend::CreateImage(t_ImageInfo);

		RenderTransitionImageInfo t_ImageTransInfo{};
		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::NONE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.image = debugTexture;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::UNDEFINED;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.layerCount = 1;
		t_ImageTransInfo.levelCount = 1;
		t_ImageTransInfo.baseArrayLayer = 0;
		t_ImageTransInfo.baseMipLevel = 0;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TOP_OF_PIPELINE;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::TRANSFER;
		RenderBackend::TransitionImage(t_CommandList, t_ImageTransInfo);

		UploadBufferChunk t_DebugImage = a_UploadBuffer.Alloc(sizeof(uint32_t));
		uint8_t r = 209;
		uint8_t g = 106;
		uint8_t b = 255;
		uint8_t a = 255;
		uint32_t t_Purple = (a << 24) | (r << 16) | (g << 8) | (b << 0);
		memcpy(t_DebugImage.memory, &t_Purple, sizeof(uint32_t));

		RenderCopyBufferImageInfo t_CopyImage{};
		t_CopyImage.srcBuffer = a_UploadBuffer.Buffer();
		t_CopyImage.srcBufferOffset = t_DebugImage.offset;
		t_CopyImage.dstImage = debugTexture;
		t_CopyImage.dstImageInfo.sizeX = t_ImageInfo.width;
		t_CopyImage.dstImageInfo.sizeY = t_ImageInfo.height;
		t_CopyImage.dstImageInfo.sizeZ = t_ImageInfo.depth;
		t_CopyImage.dstImageInfo.offsetX = 0;
		t_CopyImage.dstImageInfo.offsetY = 0;
		t_CopyImage.dstImageInfo.offsetZ = 0;
		t_CopyImage.dstImageInfo.layerCount = 1;
		t_CopyImage.dstImageInfo.mipLevel = 0;
		t_CopyImage.dstImageInfo.baseArrayLayer = 0;
		t_CopyImage.dstImageInfo.layout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;

		RenderBackend::CopyBufferImage(t_CommandList, t_CopyImage);

		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::SHADER_READ;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TRANSFER;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::FRAGMENT_SHADER;
		RenderBackend::TransitionImage(t_CommandList, t_ImageTransInfo);

		for (uint32_t i = 0; i < MAX_TEXTURES - 1; i++)
		{
			textures[i].image = debugTexture;
			textures[i].nextFree = i + 1;
		}
	}

	struct TextureSlot
	{
		RImageHandle image;
		//when loading in the texture.
		union 
		{
			//fence value reached until it's loaded.
			//Also used as generation value
			uint64_t fenceValue;
			uint32_t nextFree;
		
		
		};
	};

	uint32_t nextFree = 0;
	TextureSlot textures[MAX_TEXTURES]{};

	//purple color
	RImageHandle debugTexture;
};

struct Render_inst
{
	Render_inst(
		const RecordingCommandListHandle a_SetupCommandList,
		const RenderBufferCreateInfo& a_VertexBufferInfo,
		const RenderBufferCreateInfo& a_IndexBufferInfo,
		const DescriptorHeapCreateInfo& a_DescriptorManagerInfo,
		const size_t a_UploadBufferSize,
		const uint32_t a_BackbufferAmount)
		:	uploadBuffer(a_UploadBufferSize, "Render instance upload buffer"),
			textureManager(uploadBuffer, a_SetupCommandList),
			vertexBuffer(a_VertexBufferInfo),
			indexBuffer(a_IndexBufferInfo),
			descriptorManager(s_SystemAllocator, a_DescriptorManagerInfo, a_BackbufferAmount),
			models(s_SystemAllocator, 64)
	{
		io.frameBufferAmount = a_BackbufferAmount;
	}
	Render_IO io;

	UploadBuffer uploadBuffer;
	TextureManager textureManager;
	DescriptorManager descriptorManager;
	LinearRenderBuffer vertexBuffer;
	LinearRenderBuffer indexBuffer;

	RenderQueue graphicsQueue{ RENDER_QUEUE_TYPE::GRAPHICS, "graphics queue" };
	RenderQueue computeQueue{ RENDER_QUEUE_TYPE::COMPUTE, "compute queue" };
	RenderQueue transferQueue{ RENDER_QUEUE_TYPE::TRANSFER, "transfer queue" };

	Slotmap<Model> models;

	uint32_t t_workingAssetLoaderCount = 0;
	//max of 8 asset loaders working together
	AssetLoader* workingAssetLoaders[8];
};

CommandAllocatorHandle t_CommandAllocators[3];
CommandAllocatorHandle t_TransferAllocator[3];

CommandListHandle t_GraphicCommands[3];
CommandListHandle t_TransferCommands[3];

RecordingCommandListHandle t_RecordingGraphics;
RecordingCommandListHandle t_RecordingTransfer;

static FrameIndex s_CurrentFrame;
static Render_inst* s_RenderInst;

Render_IO& BB::Render::GetIO()
{
	return s_RenderInst->io;
}

void BB::Render::InitRenderer(const RenderInitInfo& a_InitInfo)
{
	Shader::InitShaderCompiler();

	BB::Array<RENDER_EXTENSIONS> t_Extensions{ s_TempAllocator };
	t_Extensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_INSTANCE);
	//t_Extensions.emplace_back(RENDER_EXTENSIONS::PHYSICAL_DEVICE_EXTRA_PROPERTIES); Now all these are included in STANDARD_VULKAN_INSTANCE
	if (a_InitInfo.debug)
	{
		t_Extensions.emplace_back(RENDER_EXTENSIONS::DEBUG);
	}
	BB::Array<RENDER_EXTENSIONS> t_DeviceExtensions{ s_TempAllocator };
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::STANDARD_VULKAN_DEVICE);
	t_DeviceExtensions.emplace_back(RENDER_EXTENSIONS::PIPELINE_EXTENDED_DYNAMIC_STATE);

	int t_WindowWidth;
	int t_WindowHeight;
	GetWindowSize(a_InitInfo.windowHandle, t_WindowWidth, t_WindowHeight);

	RenderBackendCreateInfo t_BackendCreateInfo;
	t_BackendCreateInfo.getApiFuncPtr = (PFN_RenderGetAPIFunctions)LibLoadFunc(a_InitInfo.renderDll, "GetRenderAPIFunctions");
	t_BackendCreateInfo.extensions = t_Extensions;
	t_BackendCreateInfo.deviceExtensions = t_DeviceExtensions;
	t_BackendCreateInfo.windowHandle = a_InitInfo.windowHandle;
	t_BackendCreateInfo.validationLayers = a_InitInfo.debug;
	t_BackendCreateInfo.appName = "TestName";
	t_BackendCreateInfo.engineName = "TestEngine";
	t_BackendCreateInfo.windowWidth = static_cast<uint32_t>(t_WindowWidth);
	t_BackendCreateInfo.windowHeight = static_cast<uint32_t>(t_WindowHeight);

	RenderBackend::InitBackend(t_BackendCreateInfo, s_SystemAllocator);

	{//init main command lists and allocators
		RenderCommandAllocatorCreateInfo t_AllocatorCreateInfo{};
		t_AllocatorCreateInfo.name = "Graphics command allocator";
		t_AllocatorCreateInfo.commandListCount = 10;
		t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::GRAPHICS;
		t_CommandAllocators[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
		t_CommandAllocators[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
		t_CommandAllocators[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

		t_AllocatorCreateInfo.name = "Transfer command allocator";
		t_AllocatorCreateInfo.queueType = RENDER_QUEUE_TYPE::TRANSFER;
		t_TransferAllocator[0] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
		t_TransferAllocator[1] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);
		t_TransferAllocator[2] = RenderBackend::CreateCommandAllocator(t_AllocatorCreateInfo);

		RenderCommandListCreateInfo t_CmdCreateInfo{};
		t_CmdCreateInfo.name = "Graphic Commandlist";
		t_CmdCreateInfo.commandAllocator = t_CommandAllocators[0];
		t_GraphicCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
		t_CmdCreateInfo.commandAllocator = t_CommandAllocators[1];
		t_GraphicCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
		t_CmdCreateInfo.commandAllocator = t_CommandAllocators[2];
		t_GraphicCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);

		//just reuse the struct above.
		t_CmdCreateInfo.name = "Transfer Commandlist";
		t_CmdCreateInfo.commandAllocator = t_TransferAllocator[0];
		t_TransferCommands[0] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
		t_CmdCreateInfo.commandAllocator = t_TransferAllocator[1];
		t_TransferCommands[1] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
		t_CmdCreateInfo.commandAllocator = t_TransferAllocator[2];
		t_TransferCommands[2] = RenderBackend::CreateCommandList(t_CmdCreateInfo);
	}

	RecordingCommandListHandle t_SetupCmdList = RenderBackend::StartCommandList(t_GraphicCommands[s_CurrentFrame]);

	{
		//Write some background info.
		constexpr size_t SUBHEAPSIZE = 64 * 1024;
		DescriptorHeapCreateInfo t_HeapInfo;
		t_HeapInfo.name = "Resource Heap";
		t_HeapInfo.descriptorCount = SUBHEAPSIZE;
		t_HeapInfo.isSampler = false;

		RenderBufferCreateInfo t_VertexBufferInfo;
		t_VertexBufferInfo.name = "Big vertex buffer";
		t_VertexBufferInfo.size = mbSize * 128;
		t_VertexBufferInfo.usage = RENDER_BUFFER_USAGE::STORAGE;
		t_VertexBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;

		RenderBufferCreateInfo t_IndexBufferInfo;
		t_IndexBufferInfo.name = "Big index buffer";
		t_IndexBufferInfo.size = mbSize * 32;
		t_IndexBufferInfo.usage = RENDER_BUFFER_USAGE::INDEX;
		t_IndexBufferInfo.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;

		constexpr const uint64_t UPLOAD_BUFFER_SIZE = static_cast<uint64_t>(mbSize * 32);
		s_RenderInst = BBnew(s_SystemAllocator, Render_inst)(t_SetupCmdList, t_VertexBufferInfo, t_IndexBufferInfo, t_HeapInfo, UPLOAD_BUFFER_SIZE, RenderBackend::GetFrameBufferAmount());
		s_RenderInst->io.renderAPI = a_InitInfo.renderAPI;
		s_RenderInst->io.swapchainWidth = t_BackendCreateInfo.windowWidth;
		s_RenderInst->io.swapchainHeight = t_BackendCreateInfo.windowHeight;
	}

	{
		RenderDescriptorCreateInfo t_CreateInfo{};
		t_CreateInfo.name = "global engine descriptor set";
		FixedArray<DescriptorBinding, 1> t_DescBinds;
		t_CreateInfo.bindings = BB::Slice(t_DescBinds.data(), t_DescBinds.size());
		{	//Image Binds
			t_DescBinds[0].binding = 0;
			t_DescBinds[0].descriptorCount = DESCRIPTOR_IMAGE_MAX;
			t_DescBinds[0].stage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
			t_DescBinds[0].type = RENDER_DESCRIPTOR_TYPE::IMAGE;
		}

		s_RenderInst->io.globalDescriptor = RenderBackend::CreateDescriptor(t_CreateInfo);
		s_RenderInst->io.globalDescAllocation = Render::AllocateDescriptor(s_RenderInst->io.globalDescriptor);
	}

	{	//Set all descriptors to debug purple.
		WriteDescriptorData t_WriteDatas[MAX_TEXTURES]{};
		t_WriteDatas[0].binding = 0;
		t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::IMAGE;
		t_WriteDatas[0].image.image = s_RenderInst->textureManager.debugTexture;
		t_WriteDatas[0].image.layout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_WriteDatas[0].image.sampler = nullptr;

		for (uint32_t i = 0; i < MAX_TEXTURES; i++)
		{
			t_WriteDatas[i] = t_WriteDatas[0];
			t_WriteDatas[i].descriptorIndex = i;
		}

		//Write image back to pink
		WriteDescriptorInfos t_WriteInfos{};
		t_WriteInfos.allocation = s_RenderInst->io.globalDescAllocation;
		t_WriteInfos.descriptorHandle = s_RenderInst->io.globalDescriptor;
		t_WriteInfos.data = Slice(t_WriteDatas, MAX_TEXTURES);
		RenderBackend::WriteDescriptors(t_WriteInfos);
	}

	{//implement imgui here.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsClassic();

		const wchar_t* t_ShaderPath[2]{};
		t_ShaderPath[0] = L"Resources/Shaders/HLSLShaders/ImguiVert.hlsl";
		t_ShaderPath[1] = L"Resources/Shaders/HLSLShaders/ImguiFrag.hlsl";

		ShaderCodeHandle t_ImguiShaders[2];
		t_ImguiShaders[0] = Shader::CompileShader(
			t_ShaderPath[0],
			L"main",
			RENDER_SHADER_STAGE::VERTEX,
			s_RenderInst->io.renderAPI);
		t_ImguiShaders[1] = Shader::CompileShader(
			t_ShaderPath[1],
			L"main",
			RENDER_SHADER_STAGE::FRAGMENT_PIXEL,
			s_RenderInst->io.renderAPI);

		ShaderCreateInfo t_ShaderInfos[2];
		t_ShaderInfos[0].shaderStage = RENDER_SHADER_STAGE::VERTEX;
		Shader::GetShaderCodeBuffer(t_ImguiShaders[0], t_ShaderInfos[0].buffer);

		t_ShaderInfos[1].shaderStage = RENDER_SHADER_STAGE::FRAGMENT_PIXEL;
		Shader::GetShaderCodeBuffer(t_ImguiShaders[1], t_ShaderInfos[1].buffer);

		ImGui_ImplCross_InitInfo t_ImguiInfo{};
		t_ImguiInfo.imageCount = s_RenderInst->io.frameBufferAmount;
		t_ImguiInfo.minImageCount = s_RenderInst->io.frameBufferAmount;
		t_ImguiInfo.vertexShader = t_ImguiShaders[0];
		t_ImguiInfo.fragmentShader = t_ImguiShaders[1];

		t_ImguiInfo.window = a_InitInfo.windowHandle;
		ImGui_ImplCross_Init(t_ImguiInfo);

		
		ImGui_ImplCross_CreateFontsTexture(t_SetupCmdList, s_RenderInst->uploadBuffer);
		ExecuteCommandsInfo* t_ExecuteInfo = BBnew(
			s_TempAllocator,
			ExecuteCommandsInfo);
		RenderBackend::EndCommandList(t_SetupCmdList);

		uint64_t t_SignalValue = s_RenderInst->graphicsQueue.GetNextFenceValue();
		t_ExecuteInfo[0] = {};
		t_ExecuteInfo[0].commands = &t_GraphicCommands[s_CurrentFrame];
		t_ExecuteInfo[0].commandCount = 1;
		t_ExecuteInfo[0].signalFences = &s_RenderInst->graphicsQueue.GetFence();
		t_ExecuteInfo[0].signalValues = &t_SignalValue;
		t_ExecuteInfo[0].signalCount = 1;

		s_RenderInst->graphicsQueue.ExecuteCommands(t_ExecuteInfo, 1);

		RenderWaitCommandsInfo t_WaitInfo;
		t_WaitInfo.waitCount = 1;
		t_WaitInfo.waitFences = &s_RenderInst->graphicsQueue.GetFence();
		t_WaitInfo.waitValues = &t_SignalValue;
		RenderBackend::WaitCommands(t_WaitInfo);

		for (size_t i = 0; i < _countof(t_ImguiShaders); i++)
		{
			Shader::ReleaseShaderCode(t_ImguiShaders[i]);
		}
	}
}

void BB::Render::DestroyRenderer()
{
	{//Don't know what to do here yet.
		
		//CommandQueueHandle t_Queues[2]{ graphicsQueue, transferQueue };
		//RenderWaitCommandsInfo t_WaitInfo{};
		//t_WaitInfo.queues = Slice(t_Queues, _countof(t_Queues));
		//RenderBackend::WaitCommands(t_WaitInfo);
	}

	for (auto it = s_RenderInst->models.begin(); it < s_RenderInst->models.end(); it++)
	{
		
	}

	RenderBackend::DestroyDescriptor(s_RenderInst->io.globalDescriptor);

	for (size_t i = 0; i < _countof(t_GraphicCommands); i++)
	{
		RenderBackend::DestroyCommandList(t_GraphicCommands[i]);
	}
	for (size_t i = 0; i < _countof(t_TransferCommands); i++)
	{
		RenderBackend::DestroyCommandList(t_TransferCommands[i]);
	}
	for (size_t i = 0; i < _countof(t_CommandAllocators); i++)
	{
		RenderBackend::DestroyCommandAllocator(t_CommandAllocators[i]);
	}
	for (size_t i = 0; i < _countof(t_TransferAllocator); i++)
	{
		RenderBackend::DestroyCommandAllocator(t_TransferAllocator[i]);
	}

	RenderBackend::DestroyBackend();
}

RDescriptorHeap BB::Render::GetGPUHeap(const uint32_t a_FrameNum)
{
	return s_RenderInst->descriptorManager.GetGPUHeap(a_FrameNum);
}

DescriptorAllocation BB::Render::AllocateDescriptor(const RDescriptor a_Descriptor)
{
	return s_RenderInst->descriptorManager.Allocate(a_Descriptor);
}

void BB::Render::UploadDescriptorsToGPU(const uint32_t a_FrameNum)
{
	s_RenderInst->descriptorManager.UploadToGPUHeap(a_FrameNum);
}

RenderBufferPart BB::Render::AllocateFromVertexBuffer(const size_t a_Size)
{
	return s_RenderInst->vertexBuffer.SubAllocate(a_Size, __alignof(Vertex));
}

RenderBufferPart BB::Render::AllocateFromIndexBuffer(const size_t a_Size)
{
	return s_RenderInst->indexBuffer.SubAllocate(a_Size, __alignof(uint32_t));
}

struct UploadTextureParameter
{
	const char* texturePath;
	RTexture returnValue;
};

static void UploadTexture_s(void* a_Param)
{
	UploadTextureParameter* t_Parameters = reinterpret_cast<UploadTextureParameter*>(a_Param);

	const uint32_t t_DescriptorIndex = s_RenderInst->textureManager.nextFree;
	TextureManager::TextureSlot& t_FreeSlot = s_RenderInst->textureManager.textures[t_DescriptorIndex];
	s_RenderInst->textureManager.nextFree = t_FreeSlot.nextFree;

	AssetLoaderInfo t_LoadInfo{};
	t_LoadInfo.assetType = ASSET_TYPE::TEXTURE;
	t_LoadInfo.path = t_Parameters->texturePath;
	t_LoadInfo.imageData.image = &t_FreeSlot.image;
	AssetLoader* t_Loader = BBnew(s_SystemAllocator, AssetLoader)(t_LoadInfo);

	RTexture t_Texture;
	t_Texture.index = t_DescriptorIndex;
	//will overflow, does not matter. This is only for debug checking.
	t_Texture.extraIndex = static_cast<uint32_t>(t_FreeSlot.fenceValue);

	{
		WriteDescriptorData t_WriteData{};
		t_WriteData.binding = 0;
		t_WriteData.descriptorIndex = t_DescriptorIndex;
		t_WriteData.type = RENDER_DESCRIPTOR_TYPE::IMAGE;
		t_WriteData.image.image = t_FreeSlot.image;
		t_WriteData.image.layout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_WriteData.image.sampler = nullptr;

		WriteDescriptorInfos t_WriteInfos;
		t_WriteInfos.allocation = s_RenderInst->io.globalDescAllocation;
		t_WriteInfos.descriptorHandle = s_RenderInst->io.globalDescriptor;
		t_WriteInfos.data = Slice(&t_WriteData, 1);
		RenderBackend::WriteDescriptors(t_WriteInfos);

	}
	{
		//Transfer image to prepare for transfer
		RenderTransitionImageInfo t_ImageTransInfo{};
		t_ImageTransInfo.srcMask = RENDER_ACCESS_MASK::TRANSFER_WRITE;
		t_ImageTransInfo.dstMask = RENDER_ACCESS_MASK::SHADER_READ;
		t_ImageTransInfo.image = t_FreeSlot.image;
		t_ImageTransInfo.oldLayout = RENDER_IMAGE_LAYOUT::TRANSFER_DST;
		t_ImageTransInfo.newLayout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
		t_ImageTransInfo.srcQueue = RENDER_QUEUE_TRANSITION::TRANSFER;
		t_ImageTransInfo.dstQueue = RENDER_QUEUE_TRANSITION::GRAPHICS;
		t_ImageTransInfo.layerCount = 1;
		t_ImageTransInfo.levelCount = 1;
		t_ImageTransInfo.baseArrayLayer = 0;
		t_ImageTransInfo.baseMipLevel = 0;
		t_ImageTransInfo.srcStage = RENDER_PIPELINE_STAGE::TRANSFER;
		t_ImageTransInfo.dstStage = RENDER_PIPELINE_STAGE::FRAGMENT_SHADER;
		RenderBackend::TransitionImage(t_RecordingGraphics, t_ImageTransInfo);
	}

	t_Parameters->returnValue = t_DescriptorIndex;
}

#include "BBThreadScheduler.hpp"

const RTexture BB::Render::UploadTexture(const char* a_TexturePath)
{
	UploadTextureParameter t_TextureParam;
	t_TextureParam.texturePath = a_TexturePath;
	ThreadHandle t_Handle = Threads::StartThread(UploadTexture_s, &t_TextureParam);
	Threads::WaitForThread(t_Handle);
	return t_TextureParam.returnValue;
}

void BB::Render::FreeTextures(const RTexture* a_Texture, const uint32_t a_Count)
{
	WriteDescriptorData* t_WriteDatas = reinterpret_cast<WriteDescriptorData*>(_alloca(a_Count * sizeof(WriteDescriptorData)));
	t_WriteDatas[0].binding = 0;
	t_WriteDatas[0].type = RENDER_DESCRIPTOR_TYPE::IMAGE;
	t_WriteDatas[0].image.image = s_RenderInst->textureManager.debugTexture;
	t_WriteDatas[0].image.layout = RENDER_IMAGE_LAYOUT::SHADER_READ_ONLY;
	t_WriteDatas[0].image.sampler = nullptr;

	for (uint32_t i = 0; i < a_Count; i++)
	{
		const RTexture t_Texture = a_Texture[i];
		TextureManager::TextureSlot& t_FreeSlot = s_RenderInst->textureManager.textures[t_Texture.index];
		//extra index is the fence value as a 4 byte value. So overflow is expected and will work as expected.
		BB_ASSERT(static_cast<uint32_t>(t_FreeSlot.fenceValue) == t_Texture.extraIndex, "Trying to free a texture that is likely already freed!");
		RenderBackend::DestroyImage(t_FreeSlot.image);
		t_FreeSlot.image = s_RenderInst->textureManager.debugTexture;
		t_FreeSlot.nextFree = s_RenderInst->textureManager.nextFree;
		s_RenderInst->textureManager.nextFree = t_Texture.index;

		t_WriteDatas[i] = t_WriteDatas[0];
		t_WriteDatas[i].descriptorIndex = t_Texture.index;
	}

	//Write image back to pink
	WriteDescriptorInfos t_WriteInfos{};
	t_WriteInfos.allocation = s_RenderInst->io.globalDescAllocation;
	t_WriteInfos.descriptorHandle = s_RenderInst->io.globalDescriptor;
	t_WriteInfos.data = Slice(t_WriteDatas, a_Count);
	RenderBackend::WriteDescriptors(t_WriteInfos);
}

const RDescriptor BB::Render::GetGlobalDescriptorSet()
{
	return s_RenderInst->io.globalDescriptor;
}

RenderQueue& Render::GetGraphicsQueue()
{
	return s_RenderInst->graphicsQueue;
}

RenderQueue& Render::GetComputeQueue()
{
	return s_RenderInst->computeQueue;
}

RenderQueue& Render::GetTransferQueue()
{
	return s_RenderInst->transferQueue;
}

Model& BB::Render::GetModel(const RModelHandle a_Handle)
{
	return s_RenderInst->models[a_Handle.handle];
}

RModelHandle BB::Render::CreateRawModel(const CreateRawModelInfo& a_CreateInfo)
{
	Model t_Model;

	//t_Model.pipelineHandle = a_CreateInfo.pipeline;
	t_Model.pipelineHandle = a_CreateInfo.pipeline;

	{
		UploadBufferChunk t_StageBuffer = s_RenderInst->uploadBuffer.Alloc(a_CreateInfo.vertices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.vertices.data(), a_CreateInfo.vertices.sizeInBytes());

		t_Model.vertexView = AllocateFromVertexBuffer(a_CreateInfo.vertices.sizeInBytes());

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = s_RenderInst->uploadBuffer.Buffer();
		t_CopyInfo.dst = t_Model.vertexView.buffer;
		t_CopyInfo.srcOffset = t_StageBuffer.offset;
		t_CopyInfo.dstOffset = t_Model.vertexView.offset;
		t_CopyInfo.size = t_Model.vertexView.size;

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}

	{
		UploadBufferChunk t_StageBuffer = s_RenderInst->uploadBuffer.Alloc(a_CreateInfo.indices.sizeInBytes());
		memcpy(t_StageBuffer.memory, a_CreateInfo.indices.data(), a_CreateInfo.indices.sizeInBytes());

		t_Model.indexView = AllocateFromIndexBuffer(a_CreateInfo.indices.sizeInBytes());

		RenderCopyBufferInfo t_CopyInfo;
		t_CopyInfo.src = s_RenderInst->uploadBuffer.Buffer();
		t_CopyInfo.dst = t_Model.indexView.buffer;
		t_CopyInfo.srcOffset = t_StageBuffer.offset;
		t_CopyInfo.dstOffset = t_Model.indexView.offset;
		t_CopyInfo.size = t_Model.indexView.size;

		RenderBackend::CopyBuffer(t_RecordingTransfer, t_CopyInfo);
	}

	{ //descriptor allocation
		t_Model.meshDescriptor = a_CreateInfo.meshDescriptor;
		t_Model.descAllocation = Render::AllocateDescriptor(t_Model.meshDescriptor);

		WriteDescriptorData t_WriteData{};
		t_WriteData.binding = 0;
		t_WriteData.descriptorIndex = 0;
		t_WriteData.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteData.buffer.buffer = t_Model.vertexView.buffer;
		t_WriteData.buffer.offset = t_Model.vertexView.offset;
		t_WriteData.buffer.range = t_Model.vertexView.size;
		WriteDescriptorInfos t_WriteInfos{};
		t_WriteInfos.allocation = t_Model.descAllocation;
		t_WriteInfos.descriptorHandle = t_Model.meshDescriptor;
		t_WriteInfos.data = Slice(&t_WriteData, 1);
		RenderBackend::WriteDescriptors(t_WriteInfos);
	}
	
	t_Model.linearNodes = BBnewArr(s_SystemAllocator, 1, Model::Node);
	t_Model.linearNodeCount = 1;
	t_Model.nodes = t_Model.linearNodes;
	t_Model.nodeCount = 1;

	t_Model.primitives = BBnewArr(s_SystemAllocator, 1, Model::Primitive);
	t_Model.primitiveCount = 1;
	t_Model.primitives->indexStart = 0;
	t_Model.primitives->indexCount = static_cast<uint32_t>(a_CreateInfo.indices.size());

	t_Model.meshCount = 1;
	t_Model.meshes = BBnewArr(s_SystemAllocator, 1, Model::Mesh);
	t_Model.meshes->primitiveCount = 1;
	t_Model.meshes->primitiveOffset = 0;

	return RModelHandle(s_RenderInst->models.insert(t_Model).handle);
}

RModelHandle BB::Render::LoadModel(const LoadModelInfo& a_LoadInfo)
{
	Model t_Model;

	t_Model.pipelineHandle = a_LoadInfo.pipeline;

	switch (a_LoadInfo.modelType)
	{
	case BB::MODEL_TYPE::GLTF:
		LoadglTFModel(s_TempAllocator,
			s_SystemAllocator,
			t_Model,
			s_RenderInst->uploadBuffer,
			t_RecordingTransfer,
			a_LoadInfo.path);
		break;
	}

	//temporary
	{ //descriptor allocation
		t_Model.meshDescriptor = a_LoadInfo.meshDescriptor;
		t_Model.descAllocation = Render::AllocateDescriptor(t_Model.meshDescriptor);

		WriteDescriptorData t_WriteData{};
		t_WriteData.binding = 0;
		t_WriteData.descriptorIndex = 0;
		t_WriteData.type = RENDER_DESCRIPTOR_TYPE::READONLY_BUFFER;
		t_WriteData.buffer.buffer = t_Model.vertexView.buffer;
		t_WriteData.buffer.offset = t_Model.vertexView.offset;
		t_WriteData.buffer.range = t_Model.vertexView.size;
		WriteDescriptorInfos t_WriteInfos{};
		t_WriteInfos.allocation = t_Model.descAllocation;
		t_WriteInfos.descriptorHandle = t_Model.meshDescriptor;
		t_WriteInfos.data = Slice(&t_WriteData, 1);
		RenderBackend::WriteDescriptors(t_WriteInfos);
	}

	return RModelHandle(s_RenderInst->models.insert(t_Model).handle);
}

void BB::Render::StartFrame()
{
	StartFrameInfo t_StartInfo{};
	//We do not use these currently.
	//t_StartInfo.fences = &t_SwapchainFence[s_CurrentFrame];
	//t_StartInfo.fenceCount = 1;
	RenderBackend::StartFrame(t_StartInfo);
	//Prepare the commandallocator for a new frame
	//TODO, send a fence that waits until the image was presented.
	RenderBackend::ResetCommandAllocator(t_CommandAllocators[s_CurrentFrame]);
	RenderBackend::ResetCommandAllocator(t_TransferAllocator[s_CurrentFrame]);

	t_RecordingGraphics = RenderBackend::StartCommandList(t_GraphicCommands[s_CurrentFrame]);
	t_RecordingTransfer = RenderBackend::StartCommandList(t_TransferCommands[s_CurrentFrame]);

	RenderBackend::BindDescriptorHeaps(t_RecordingGraphics, Render::GetGPUHeap(s_CurrentFrame), nullptr);

	ImGui_ImplCross_NewFrame();
	ImGui::NewFrame();
}

RecordingCommandListHandle BB::Render::GetRecordingTransfer()
{
	return t_RecordingTransfer;
}

RecordingCommandListHandle BB::Render::GetRecordingGraphics()
{
	return t_RecordingGraphics;
}


void BB::Render::Update(const float a_DeltaTime)
{
	RenderBackend::DisplayDebugInfo();
	Editor::DisplayAllocator(s_SystemAllocator);
	//Draw3DFrame();
	s_TempAllocator.Clear();
}

void BB::Render::EndFrame()
{
	{
		StartRenderingInfo t_ImguiStart;
		t_ImguiStart.viewportWidth = s_RenderInst->io.swapchainWidth;
		t_ImguiStart.viewportHeight = s_RenderInst->io.swapchainHeight;
		t_ImguiStart.colorLoadOp = RENDER_LOAD_OP::LOAD;
		t_ImguiStart.colorStoreOp = RENDER_STORE_OP::STORE;
		t_ImguiStart.colorInitialLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		t_ImguiStart.colorFinalLayout = RENDER_IMAGE_LAYOUT::COLOR_ATTACHMENT_OPTIMAL;
		RenderBackend::StartRendering(t_RecordingGraphics, t_ImguiStart);

		ImDrawData* t_DrawData = ImGui::GetDrawData();
		ImGui_ImplCross_RenderDrawData(*t_DrawData, t_RecordingGraphics, t_RecordingTransfer);

		EndRenderingInfo t_ImguiEnd;
		t_ImguiEnd.colorInitialLayout = t_ImguiStart.colorFinalLayout;
		t_ImguiEnd.colorFinalLayout = RENDER_IMAGE_LAYOUT::PRESENT;
		RenderBackend::EndRendering(t_RecordingGraphics, t_ImguiEnd);
	}

	UploadDescriptorsToGPU(s_CurrentFrame);

	ImGui::EndFrame();
	RenderBackend::EndCommandList(t_RecordingGraphics);
	RenderBackend::EndCommandList(t_RecordingTransfer);

	ExecuteCommandsInfo* t_ExecuteInfos = BBnewArr(
		s_TempAllocator,
		2,
		ExecuteCommandsInfo);

	uint64_t t_SignalValue = s_RenderInst->transferQueue.GetNextFenceValue();
	t_ExecuteInfos[0] = {};
	t_ExecuteInfos[0].commands = &t_TransferCommands[s_CurrentFrame];
	t_ExecuteInfos[0].commandCount = 1;
	t_ExecuteInfos[0].signalFences = &s_RenderInst->transferQueue.GetFence();
	t_ExecuteInfos[0].signalValues = &t_SignalValue;
	t_ExecuteInfos[0].signalCount = 1;

	s_RenderInst->transferQueue.ExecuteCommands(&t_ExecuteInfos[0], 1);

	//We write to vertex information (vertex buffer, index buffer and the storage buffer storing all the matrices.)
	RENDER_PIPELINE_STAGE t_WaitStage = RENDER_PIPELINE_STAGE::VERTEX_SHADER;
	t_ExecuteInfos[1] = {};
	t_ExecuteInfos[1].commands = &t_GraphicCommands[s_CurrentFrame];
	t_ExecuteInfos[1].commandCount = 1;
	t_ExecuteInfos[1].waitCount = 1;
	t_ExecuteInfos[1].waitFences = &s_RenderInst->transferQueue.GetFence();
	t_ExecuteInfos[1].waitValues = &t_SignalValue;
	t_ExecuteInfos[1].waitStages = &t_WaitStage;

	RenderBackend::ExecutePresentCommands(s_RenderInst->graphicsQueue.GetQueue(), t_ExecuteInfos[1]);
	PresentFrameInfo t_PresentFrame{};
	s_CurrentFrame = RenderBackend::PresentFrame(t_PresentFrame);
}

void BB::Render::ResizeWindow(const uint32_t a_X, const uint32_t a_Y)
{
	s_RenderInst->io.swapchainWidth = a_X;
	s_RenderInst->io.swapchainHeight = a_Y;
	RenderBackend::ResizeWindow(a_X, a_Y);
}

#pragma warning(push, 0)
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning (pop)

static inline void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	void* t_Data = a_Accessor->buffer_view->buffer->data;
	t_Data = Pointer::Add(t_Data, a_Accessor->buffer_view->offset);
	t_Data = Pointer::Add(t_Data, a_Accessor->offset);
	return t_Data;
}

static inline uint32_t GetChildNodeCount(const cgltf_node& a_Node)
{
	uint32_t t_NodeCount = 0;
	for (size_t i = 0; i < a_Node.children_count; i++)
	{
		t_NodeCount += GetChildNodeCount(*a_Node.children[i]);
	}
	return t_NodeCount;
}

//Maybe use own allocators for this?
void LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path)
{
	cgltf_options t_Options = {};
	cgltf_data* t_Data = { 0 };

	cgltf_result t_ParseResult = cgltf_parse_file(&t_Options, a_Path, &t_Data);

	BB_ASSERT(t_ParseResult == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");

	cgltf_load_buffers(&t_Options, t_Data, a_Path);

	BB_ASSERT(cgltf_validate(t_Data) == cgltf_result_success, "GLTF model validation failed!");

	uint32_t t_IndexCount = 0;
	uint32_t t_VertexCount = 0;
	uint32_t t_LinearNodeCount = static_cast<uint32_t>(t_Data->nodes_count);
	uint32_t t_MeshCount = static_cast<uint32_t>(t_Data->meshes_count);
	uint32_t t_PrimitiveCount = 0;

	//Get the node count.
	for (size_t nodeIndex = 0; nodeIndex < t_Data->nodes_count; nodeIndex++)
	{
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		t_LinearNodeCount += GetChildNodeCount(t_Node);
	}

	//Get the sizes first for efficient allocation.
	for (size_t meshIndex = 0; meshIndex < t_Data->meshes_count; meshIndex++)
	{
		cgltf_mesh& t_Mesh = t_Data->meshes[meshIndex];
		for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
		{
			++t_PrimitiveCount;
			cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
			t_IndexCount += static_cast<uint32_t>(t_Mesh.primitives[meshIndex].indices->count);

			for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
			{
				cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
				if (t_Attribute.type == cgltf_attribute_type_position)
				{
					BB_ASSERT(t_Attribute.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
					t_VertexCount += static_cast<uint32_t>(t_Attribute.data->count);
				}
			}
		}
	}

	//Maybe allocate this all in one go
	Model::Mesh* t_Meshes = BBnewArr(
		a_SystemAllocator,
		t_MeshCount,
		Model::Mesh);
	a_Model.meshes = t_Meshes;
	a_Model.meshCount = t_MeshCount;

	Model::Primitive* t_Primitives = BBnewArr(
		a_SystemAllocator,
		t_PrimitiveCount,
		Model::Primitive);
	a_Model.primitives = t_Primitives;
	a_Model.primitiveCount = t_PrimitiveCount;

	Model::Node* t_LinearNodes = BBnewArr(
		a_SystemAllocator,
		t_LinearNodeCount,
		Model::Node);
	a_Model.linearNodes = t_LinearNodes;
	a_Model.linearNodeCount = t_LinearNodeCount;

	//Temporary stuff
	uint32_t* t_Indices = BBnewArr(
		a_TempAllocator,
		t_IndexCount,
		uint32_t);
	Vertex* t_Vertices = BBnewArr(
		a_TempAllocator,
		t_VertexCount,
		Vertex);

	uint32_t t_CurrentIndex = 0;

	uint32_t t_CurrentNode = 0;
	uint32_t t_CurrentMesh = 0;
	uint32_t t_CurrentPrimitive = 0;

	for (size_t nodeIndex = 0; nodeIndex < t_LinearNodeCount; nodeIndex++)
	{
		//TODO: we do not handle childeren now, we should!
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		Model::Node& t_ModelNode = t_LinearNodes[t_CurrentNode++];
		t_ModelNode.childCount = 0;
		t_ModelNode.childeren = nullptr; //For now we don't care.
		t_ModelNode.meshIndex = MESH_INVALID_INDEX;
		if (t_Node.mesh != nullptr)
		{
			const cgltf_mesh& t_Mesh = *t_Node.mesh;

			Model::Mesh& t_ModelMesh = t_Meshes[t_CurrentMesh];
			t_ModelNode.meshIndex = t_CurrentMesh++;
			t_ModelMesh.primitiveOffset = t_CurrentPrimitive;
			t_ModelMesh.primitiveCount = static_cast<uint32_t>(t_Mesh.primitives_count);

			for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
			{
				const cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
				Model::Primitive& t_MeshPrimitive = t_Primitives[t_CurrentPrimitive++];
				t_MeshPrimitive.indexCount = static_cast<uint32_t>(t_Primitive.indices->count);
				t_MeshPrimitive.indexStart = t_CurrentIndex;

				void* t_IndexData = GetAccessorDataPtr(t_Primitive.indices);
				if (t_Primitive.indices->component_type == cgltf_component_type_r_32u)
				{
					for (size_t i = 0; i < t_Primitive.indices->count; i++)
						t_Indices[t_CurrentIndex++] = reinterpret_cast<uint32_t*>(t_IndexData)[i];
				}
				else if (t_Primitive.indices->component_type == cgltf_component_type_r_16u)
				{
					for (size_t i = 0; i < t_Primitive.indices->count; i++)
						t_Indices[t_CurrentIndex++] = reinterpret_cast<uint16_t*>(t_IndexData)[i];
				}
				else
				{
					BB_ASSERT(false, "GLTF mesh has an index type that is not supported!");
				}

				for (size_t i = 0; i < t_VertexCount; i++)
				{
					t_Vertices[i].color.x = 1.0f;
					t_Vertices[i].color.y = 1.0f;
					t_Vertices[i].color.z = 1.0f;
				}

				for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
				{
					const cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
					float* t_PosData = nullptr;
					size_t t_CurrentVertex = 0;

					switch (t_Attribute.type)
					{
					case cgltf_attribute_type_position:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].pos.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].pos.y = t_PosData[1];
							t_Vertices[t_CurrentVertex].pos.z = t_PosData[2];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}
						break;
					case cgltf_attribute_type_texcoord:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].uv.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].uv.y = t_PosData[1];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}

						break;
					case cgltf_attribute_type_normal:
						t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));

						for (size_t posIndex = 0; posIndex < t_Attribute.data->count; posIndex++)
						{
							t_Vertices[t_CurrentVertex].normal.x = t_PosData[0];
							t_Vertices[t_CurrentVertex].normal.y = t_PosData[1];
							t_Vertices[t_CurrentVertex].normal.z = t_PosData[2];

							t_PosData = reinterpret_cast<float*>(Pointer::Add(t_PosData, t_Attribute.data->stride));
							++t_CurrentVertex;
						}
						break;
					}
					BB_ASSERT(t_VertexCount >= t_CurrentVertex, "Overwriting vertices in the gltf loader!");
				}
			}
		}
	}

	//get it all in GPU buffers now.
	{
		const uint32_t t_VertexBufferSize = t_VertexCount * sizeof(Vertex);

		const UploadBufferChunk t_VertChunk = a_UploadBuffer.Alloc(t_VertexBufferSize);
		memcpy(t_VertChunk.memory, t_Vertices, t_VertexBufferSize);

		a_Model.vertexView = AllocateFromVertexBuffer(t_VertexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.vertexView.buffer;
		t_CopyInfo.srcOffset = t_VertChunk.offset;
		t_CopyInfo.dstOffset = a_Model.vertexView.offset;
		t_CopyInfo.size = a_Model.vertexView.size;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	{
		const uint32_t t_IndexBufferSize = t_IndexCount * sizeof(uint32_t);

		const UploadBufferChunk t_IndexChunk = a_UploadBuffer.Alloc(t_IndexBufferSize);
		memcpy(t_IndexChunk.memory, t_Indices, t_IndexBufferSize);

		a_Model.indexView = AllocateFromIndexBuffer(t_IndexBufferSize);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.indexView.buffer;
		t_CopyInfo.srcOffset = t_IndexChunk.offset;
		t_CopyInfo.dstOffset = a_Model.indexView.offset;
		t_CopyInfo.size = a_Model.indexView.size;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	cgltf_free(t_Data);
}