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
			Primitive* primitives = nullptr;
			uint32_t primitiveCount = 0;
			uint32_t uniformBufferOffset = 0; //Offset for it's translate data.
		};

		struct Node
		{
			Node* parent = nullptr;
			Mesh* mesh = nullptr;
			Model::Node* childeren = nullptr;
			uint32_t childCount = 0;
		};

		PipelineHandle pipelineHandle{};

		RBufferHandle vertexBuffer{};
		RBufferHandle indexBuffer{};

		RBufferHandle uniformBuffer;

		Node* nodes = nullptr;
		Node* linearNodes = nullptr;
		uint32_t nodeCount = 0;
		uint32_t linearNodeCount = 0;

		Mesh* meshes;
		uint32_t meshCount;
	};

	struct CreateRawModelInfo
	{
		BB::Slice<Vertex> vertices;
		BB::Slice<const uint32_t> indices;
		PipelineHandle pipeline;
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
}