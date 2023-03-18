#include "ModelLoader.h"
#include "Logger.h"
#include "RenderBackend.h"

#pragma warning(push)
#pragma warning(disable:4996)
#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"
#pragma warning (pop)

using namespace BB;

static void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	void* t_Data = a_Accessor->buffer_view->buffer->data;
	t_Data = Pointer::Add(t_Data, a_Accessor->buffer_view->offset);
	t_Data = Pointer::Add(t_Data, a_Accessor->offset);
	return t_Data;
}

static uint32_t GetChildNodeCount(const cgltf_node& a_Node)
{
	uint32_t t_NodeCount = 0;
	for (size_t i = 0; i < a_Node.children_count; i++)
	{
		t_NodeCount += GetChildNodeCount(*a_Node.children[i]);
	}
	return t_NodeCount;
}

//Maybe use own allocators for this?
void BB::LoadglTFModel(Allocator a_TempAllocator, Allocator a_SystemAllocator, Model& a_Model, UploadBuffer& a_UploadBuffer, const RecordingCommandListHandle a_TransferCmdList, const char* a_Path)
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
		size_t t_VertexBufferSize = t_VertexCount * sizeof(Vertex);

		UploadBufferChunk t_VertChunk = a_UploadBuffer.Alloc(t_VertexBufferSize);
		memcpy(t_VertChunk.memory, t_Vertices, t_VertexBufferSize);

		RenderBufferCreateInfo t_VertBuffer{};
		t_VertBuffer.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_VertBuffer.size = t_VertexBufferSize;
		t_VertBuffer.usage = RENDER_BUFFER_USAGE::VERTEX;
		a_Model.vertexBuffer = RenderBackend::CreateBuffer(t_VertBuffer);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.vertexBuffer;
		t_CopyInfo.srcOffset = t_VertChunk.offset;
		t_CopyInfo.dstOffset = 0;
		t_CopyInfo.size = t_VertexBufferSize;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	{
		size_t t_IndexBufferSize = t_IndexCount * sizeof(uint32_t);

		UploadBufferChunk t_IndexChunk = a_UploadBuffer.Alloc(t_IndexBufferSize);
		memcpy(t_IndexChunk.memory, t_Indices, t_IndexBufferSize);

		RenderBufferCreateInfo t_IndexBuffer{};
		t_IndexBuffer.memProperties = RENDER_MEMORY_PROPERTIES::DEVICE_LOCAL;
		t_IndexBuffer.size = t_IndexBufferSize;
		t_IndexBuffer.usage = RENDER_BUFFER_USAGE::INDEX;
		a_Model.indexBuffer = RenderBackend::CreateBuffer(t_IndexBuffer);

		RenderCopyBufferInfo t_CopyInfo{};
		t_CopyInfo.src = a_UploadBuffer.Buffer();
		t_CopyInfo.dst = a_Model.indexBuffer;
		t_CopyInfo.srcOffset = t_IndexChunk.offset;
		t_CopyInfo.dstOffset = 0;
		t_CopyInfo.size = t_IndexBufferSize;

		RenderBackend::CopyBuffer(a_TransferCmdList, t_CopyInfo);
	}

	cgltf_free(t_Data);
}