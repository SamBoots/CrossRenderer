#pragma once
#include "../Backend/RenderBackend.h"
#include "Transform.h"

namespace BB
{
	using RMaterialHandle = FrameworkHandle<struct RMaterialHandleTag>;
	using RModelHandle = FrameworkHandle<struct RModelHandleTag>;
	using DrawObjectHandle = FrameworkHandle<struct RDrawObjectHandleTag>;
	using LightHandle = FrameworkHandle<struct LightHandleTag>;


	enum class LIGHT_TYPE
	{
		POINT
	};

	struct RenderBufferPart
	{
		RBufferHandle bufferHandle{};
		uint32_t size = 0; //the size of the buffer part.
		uint32_t offset = 0; //offset starting from the bufferhandle
	};

	class LinearRenderBuffer
	{
	public:
		LinearRenderBuffer(const RenderBufferCreateInfo& a_CreateInfo)
			: m_Size(a_CreateInfo.size), m_MemoryProperties(a_CreateInfo.memProperties)
		{
			m_Buffer = RenderBackend::CreateBuffer(a_CreateInfo);
			m_Used = 0;
		}
		~LinearRenderBuffer() 
		{
			RenderBackend::DestroyBuffer(m_Buffer);
		}

		//Maybe do alignment
		RenderBufferPart SubAllocateFromBuffer(const uint64_t a_Size, const uint32_t a_Alignment)
		{
			//Align the m_Used variable, as it works as the buffer offset.
			m_Used += static_cast<uint32_t>(Pointer::AlignForwardAdjustment(a_Size, a_Alignment));
			BB_ASSERT(m_Size >= static_cast<uint64_t>(m_Used + a_Size), "Not enough memory for a linear render buffer!");

			const RenderBufferPart t_Part{ m_Buffer, 
			static_cast<uint32_t>(a_Size),
			static_cast<uint32_t>(m_Used) };

			m_Used += a_Size;

			return t_Part;
		}

		void MapBuffer() const
		{
			BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
				"Trying to map a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
			RenderBackend::MapMemory(m_Buffer);
		}
		void UnmapBuffer() const
		{
			BB_ASSERT(m_MemoryProperties == RENDER_MEMORY_PROPERTIES::HOST_VISIBLE,
				"Trying to unmap a GPU device local memory region! Create the buffer with HOST_VISIBLE instead!");
			RenderBackend::UnmapMemory(m_Buffer);
		}

		const RENDER_MEMORY_PROPERTIES m_MemoryProperties;
		RBufferHandle m_Buffer;
		const uint64_t m_Size;
		uint64_t m_Used;
	};

	constexpr const uint32_t MESH_INVALID_INDEX = UINT32_MAX;
	struct Model
	{
		struct Primitive
		{
			uint32_t indexStart = 0;
			uint32_t indexCount = 0;
		};

		struct Mesh
		{
			uint32_t primitiveOffset = 0;
			uint32_t primitiveCount = 0;
		};

		struct Node
		{
			Model::Node* childeren = nullptr;
			uint32_t childCount = 0;
			uint32_t meshIndex = MESH_INVALID_INDEX;
		};

		PipelineHandle pipelineHandle{};
		RImageHandle image{}; //This is temp, will be managed by the engine.

		DescriptorAllocation descAllocation;
		RDescriptor meshDescriptor;

		RenderBufferPart vertexView;
		RenderBufferPart indexView;

		Node* nodes = nullptr;
		Node* linearNodes = nullptr;
		uint32_t nodeCount = 0;
		uint32_t linearNodeCount = 0;

		Mesh* meshes = nullptr;
		uint32_t meshCount = 0;

		Primitive* primitives = nullptr;
		uint32_t primitiveCount = 0;
	};

	struct CreateRawModelInfo
	{
		BB::Slice<Vertex> vertices{};
		BB::Slice<const uint32_t> indices{};
		PipelineHandle pipeline{};
		const char* imagePath = nullptr;
	};

	enum class MODEL_TYPE
	{
		GLTF
	};

	struct LoadModelInfo
	{
		const char* path = nullptr;
		MODEL_TYPE modelType{};
	};

	struct CameraRenderData
	{
		glm::mat4 view{};
		glm::mat4 projection{};
	};

	struct BaseFrameInfo
	{
		float3 ambientLight{};
		float ambientStrength = 0.f;

		uint32_t lightCount = 0;
		uint3 padding{};
	};

	struct DrawObject
	{
		RModelHandle modelHandle{};
		TransformHandle transformHandle{};
	};
}