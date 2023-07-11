#include "SceneGraph.hpp"
#include "Storage/Array.h"
#include "Storage/Slotmap.h"

#include "glm/glm.hpp"
#include "RenderFrontend.h"

using namespace BB;

struct SceneGraph_inst
{
	SceneGraph_inst(Allocator a_Allocator)
		:	allocator(a_Allocator),
			matrices(a_Allocator, 128),
			drawObjects(a_Allocator, 256)
	{

	}

	Allocator allocator;
	Array<glm::mat4> matrices;
	Slotmap<DrawObject> drawObjects;
};

SceneGraph::SceneGraph(Allocator a_Allocator)
{
	inst = BBnew(a_Allocator, SceneGraph_inst)(a_Allocator);
}

SceneGraph::~SceneGraph()
{
	Allocator t_Allocator = inst->allocator;
	BBfree(t_Allocator, inst);
}

void SceneGraph::StartScene()
{

}

void SceneGraph::RenderScene(RecordingCommandListHandle a_GraphicList)
{
	RModelHandle t_CurrentModel = inst->drawObjects.begin()->modelHandle;
	Model* t_Model = &inst->models.find(t_CurrentModel.handle);

	uint32_t t_BaseFrameInfoOffset = static_cast<uint32_t>(s_GlobalInfo.perFrameBufferSize * s_CurrentFrame);
	uint32_t t_CamOffset = t_BaseFrameInfoOffset + sizeof(BaseFrameInfo);
	uint32_t t_MatrixOffset = t_CamOffset + sizeof(CameraRenderData);
	uint32_t t_DynOffSets[3]{ t_BaseFrameInfoOffset, t_CamOffset, t_MatrixOffset };

	RenderBackend::BindDescriptorHeaps(a_GraphicList, Render::GetGPUHeap(s_CurrentFrame), nullptr);
	RenderBackend::BindPipeline(a_GraphicList, t_Model->pipelineHandle);

	uint32_t t_IsSamplerHeap = false;
	size_t t_HeapOffset = sceneDescAllocation.offset;
	RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::SCENE_SET, 1, &t_IsSamplerHeap, &t_HeapOffset);
	{
		uint64_t t_BufferOffsets[1]{ t_Model->descAllocation.offset };
		RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::PER_FRAME_SET, 1, &t_IsSamplerHeap, t_BufferOffsets);
		RenderBackend::BindIndexBuffer(a_GraphicList, t_Model->indexView.buffer, t_Model->indexView.offset);
	}

	for (auto t_It = inst->drawObjects.begin(); t_It < inst->drawObjects.end(); t_It++)
	{
		if (t_CurrentModel != t_It->modelHandle)
		{
			t_CurrentModel = t_It->modelHandle;
			Model* t_NewModel = &s_RenderInst->models.find(t_CurrentModel.handle);

			if (t_NewModel->pipelineHandle != t_Model->pipelineHandle)
			{
				RenderBackend::BindPipeline(a_GraphicList, t_NewModel->pipelineHandle);
			}

			uint64_t t_BufferOffsets[1]{ t_NewModel->descAllocation.offset };
			RenderBackend::SetDescriptorHeapOffsets(a_GraphicList, RENDER_DESCRIPTOR_SET::PER_FRAME_SET, 1, &t_IsSamplerHeap, t_BufferOffsets);
			RenderBackend::BindIndexBuffer(a_GraphicList, t_NewModel->indexView.buffer, t_NewModel->indexView.offset);

			t_Model = t_NewModel;
		}

		RenderBackend::BindConstant(a_GraphicList, 0, 1, 0, &t_It->transformHandle.index);
		for (uint32_t i = 0; i < t_Model->linearNodeCount; i++)
		{
			const Model::Node& t_Node = t_Model->linearNodes[i];
			if (t_Node.meshIndex != MESH_INVALID_INDEX)
			{
				const Model::Mesh& t_Mesh = t_Model->meshes[t_Node.meshIndex];
				for (size_t t_PrimIndex = 0; t_PrimIndex < t_Mesh.primitiveCount; t_PrimIndex++)
				{
					const Model::Primitive& t_Prim = t_Model->primitives[t_Mesh.primitiveOffset + t_PrimIndex];
					RenderBackend::DrawIndexed(a_GraphicList,
						t_Prim.indexCount,
						1,
						t_Prim.indexStart,
						0,
						0);
				}
			}
		}
	}
}

void SceneGraph::EndScene()
{

}

DrawObjectHandle SceneGraph::CreateDrawObject(const RModelHandle a_Model, const TransformHandle a_TransformHandle)
{
	DrawObject t_DrawObject{ a_Model, a_TransformHandle };
	return DrawObjectHandle(inst->drawObjects.emplace(t_DrawObject).handle);
}

BB::Slice<DrawObject> SceneGraph::GetDrawObjects()
{
	return BB::Slice(inst->drawObjects.data(), inst->drawObjects.size());
}

void SceneGraph::DestroyDrawObject(const DrawObjectHandle a_Handle)
{
	inst->drawObjects.erase(a_Handle.handle);
}