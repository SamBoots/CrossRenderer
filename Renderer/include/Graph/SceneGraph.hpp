#pragma once
#include "RenderFrontendCommon.h"
#include "BBMemory.h"

class SceneGraph
{
public:
	SceneGraph(Allocator a_Allocator);
	~SceneGraph();

	void StartScene();
	void RenderScene(RecordingCommandListHandle a_GraphicList);
	void EndScene();

	DrawObjectHandle CreateDrawObject(const RModelHandle a_Model, const TransformHandle a_TransformHandle);
	BB::Slice<DrawObject> GetDrawObjects();
	void DestroyDrawObject(const DrawObjectHandle a_Handle);

private:
	struct SceneGraph_inst* inst;
};