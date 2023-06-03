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

		RBufferHandle vertexBuffer{};
		RBufferHandle indexBuffer{};

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
		uint32_t lightCount = 0;
		uint3 padding{};

		float3 ambientLight{};
		float ambientStrength = 0.f;
	};

	struct DrawObject
	{
		RModelHandle modelHandle{};
		TransformHandle transformHandle{};
	};

	struct RenderBufferPart
	{
		RBufferHandle bufferHandle{};
		uint32_t size = 0; //the size of the buffer part.
		uint32_t offset = 0; //offset starting from the bufferhandle
	};
}