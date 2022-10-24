#pragma once
#include "../Backend/RenderBackendCommon.h"

namespace BB
{
	using RModelHandle = FrameworkHandle<struct RModelHandleTag>;

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
		};

		struct Node
		{
			Node* parent = nullptr;
			Mesh* mesh = nullptr;
			Model::Node* childeren;
			uint32_t childCount;
		};

		PipelineHandle pipelineHandle;

		RBufferHandle vertexBuffer{};
		RDeviceBufferView vertexBufferView{};

		RBufferHandle indexBuffer{};
		RDeviceBufferView indexBufferView{};
		uint32_t indexCount = 0;

		Node* nodes;
		Node* linearNodes;
		uint32_t nodeCount;
		uint32_t linearNodeCount;
	};

	struct CreateRawModelInfo
	{
		BB::Slice<Vertex> vertices;
		BB::Slice<uint32_t> indices;
		PipelineHandle pipeline;
	};
}