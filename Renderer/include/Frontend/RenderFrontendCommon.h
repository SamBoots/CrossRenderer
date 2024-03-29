#pragma once
#include "../Backend/RenderBackend.h"
#include "Transform.h"

namespace BB
{
	using RMaterialHandle = FrameworkHandle<struct RMaterialHandleTag>;
	using RModelHandle = FrameworkHandle<struct RModelHandleTag>;
	using LightHandle = FrameworkHandle<struct LightHandleTag>;

	using RTexture = FrameworkHandle<struct RTexturetag>;


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
			RTexture baseColorIndex = BB_INVALID_HANDLE;
			RTexture normalIndex = BB_INVALID_HANDLE;
		};

		struct Mesh
		{
			uint32_t primitiveOffset = 0;
			uint32_t primitiveCount = 0;
		};

		struct Node
		{
			Mat4x4 transform;
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
		RDescriptor meshDescriptor;
	};

	enum class MODEL_TYPE
	{
		GLTF
	};

	struct LoadModelInfo
	{
		const char* path = nullptr;
		MODEL_TYPE modelType{};
		RDescriptor meshDescriptor;
		PipelineHandle pipeline;
	};

	struct Light
	{
		float3 pos;
		float radius;
		float4 color;
	};
}