#pragma once
#include "../Backend/RenderBackendCommon.h"
#define GLM_FORCE_RADIANS
#define GLM_DEPTH_ZERO_TO_ONE
#include "Transform.h"

namespace BB
{
	using RModelHandle = FrameworkHandle<struct RModelHandleTag>;
	using DrawObjectHandle = FrameworkHandle<struct RDrawObjectHandleTag>;

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
			uint32_t meshIndex = 0;
		};

		PipelineHandle pipelineHandle{};

		RBufferHandle vertexBuffer{};
		RBufferHandle indexBuffer{};

		RBufferHandle uniformBuffer;

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
		BB::Slice<Vertex> vertices;
		BB::Slice<const uint32_t> indices;
		PipelineHandle pipeline;
	};

	enum class MODEL_TYPE
	{
		GLTF
	};

	struct LoadModelInfo
	{
		const char* path;
		MODEL_TYPE modelType;
	};

	struct CameraBufferInfo
	{
		glm::mat4 view;
		glm::mat4 projection;
		glm::mat4 pad[2];
	};

	struct DrawObject
	{
		RModelHandle modelHandle;
		TransformHandle transformHandle;
	};

	struct UploadBufferChunk
	{
		void* memory;
		uint64_t offset;
	};

	class UploadBuffer
	{
	public:
		UploadBuffer(const uint64_t a_Size);
		~UploadBuffer();

		UploadBufferChunk Alloc(const uint64_t a_Size);
		void Clear();

		const RBufferHandle Buffer() const { return buffer; }

	private:
		RBufferHandle buffer;
		const uint64_t size;
		uint64_t offset;
		void* start;
		void* position;
	};
}