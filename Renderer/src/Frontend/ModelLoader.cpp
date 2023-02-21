#include "ModelLoader.h"
#include "Logger.h"
#include "RenderFrontendCommon.h"
#include "RenderBackend.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

using namespace BB;

static void* GetAccessorDataPtr(const cgltf_accessor* a_Accessor)
{
	void* t_Data = a_Accessor->buffer_view->data;
	t_Data = Pointer::Add(t_Data, a_Accessor->buffer_view->offset);
	t_Data = Pointer::Add(t_Data, a_Accessor->offset);
	return t_Data;
}

static size_t GetChildNodeCount(const cgltf_node& a_Node)
{
	size_t t_NodeCount = 0;
	for (size_t i = 0; i < a_Node.children_count; i++)
	{
		t_NodeCount += GetChildNodeCount(*a_Node.children[i]);
	}
	return t_NodeCount;
}

//Maybe use own allocators for this?
void LoadglTFModel(Allocator a_SystemAllocator, Model& a_Model, const char* a_Path)
{
	cgltf_options t_Options = {};
	cgltf_data* t_Data{};

	cgltf_result t_ParseResult = cgltf_parse_file(&t_Options, a_Path, &t_Data);

	BB_ASSERT(t_ParseResult == cgltf_result_success, "Failed to load glTF model, cgltf_parse_file.");


	cgltf_load_buffers(&t_Options, t_Data, a_Path);

	size_t t_IndexCount = 0;
	size_t t_VertexCount = 0;
	size_t t_NodeCount = t_Data->nodes_count;

	//Get the node count.
	for (size_t nodeIndex = 0; nodeIndex < t_Data->nodes_count; nodeIndex++)
	{
		const cgltf_node& t_Node = t_Data->nodes[nodeIndex];
		t_NodeCount += GetChildNodeCount(t_Node);
	}

	//Get the sizes first for efficient allocation.
	for (size_t meshIndex = 0; meshIndex < t_Data->meshes_count; meshIndex++)
	{
		cgltf_mesh& t_Mesh = t_Data->meshes[meshIndex];
		for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
		{
			cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
			t_IndexCount += t_Mesh.primitives[meshIndex].indices->count;

			for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
			{
				cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
				if (t_Attribute.type = cgltf_attribute_type_position)
				{
					BB_ASSERT(t_Attribute.data->type == cgltf_type_vec3, "GLTF position type is not a vec3!");
					++t_VertexCount;
				}
			}
		}


	}

	Model::Node* t_Nodes = BBnewArr(
		a_SystemAllocator,
		t_NodeCount,
		Model::Node);
	uint32_t* t_Indices = BBnewArr(
		a_SystemAllocator,
		t_IndexCount,
		uint32_t);
	Vertex* t_Vertices = BBnewArr(
		a_SystemAllocator,
		t_VertexCount,
		Vertex);

	uint32_t t_CurrentIndex = 0;
	uint32_t t_CurrentVertex = 0;

	for (size_t meshIndex = 0; meshIndex < t_Data->meshes_count; meshIndex++)
	{
		const cgltf_mesh& t_Mesh = t_Data->meshes[meshIndex];
		for (size_t primitiveIndex = 0; primitiveIndex < t_Mesh.primitives_count; primitiveIndex++)
		{
			const cgltf_primitive& t_Primitive = t_Mesh.primitives[primitiveIndex];
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

			for (size_t attrIndex = 0; attrIndex < t_Primitive.attributes_count; attrIndex++)
			{
				const cgltf_attribute& t_Attribute = t_Primitive.attributes[attrIndex];
				
				switch (t_Attribute.type)
				{
				case cgltf_attribute_type_position:

					BB_ASSERT(t_Attribute.data->type == cgltf_type_vec3, "GLTF position attribute is not a vec3!");
					float* t_PosData = reinterpret_cast<float*>(GetAccessorDataPtr(t_Attribute.data));
					
					t_Vertices[t_CurrentVertex].pos[0] = t_PosData[0];
					t_Vertices[t_CurrentVertex].pos[1] = t_PosData[1];
					t_Vertices[t_CurrentVertex].pos[2] = t_PosData[2];
					//FOR NOW WE JUST DO THIS. LATER WE FIRST CHECK FOR THE STRIDE AND FILL IN THE ENTIRE VERTEX.
					++t_CurrentVertex;
					break;
				}
			}
		}
	}

	cgltf_free(t_Data);
	BBfreeArr(a_SystemAllocator, t_Indices);
	BBfreeArr(a_SystemAllocator, t_Vertices);
}