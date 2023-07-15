#pragma once
#include "RenderFrontendCommon.h"
#include "BBMemory.h"

namespace BB
{
	struct SceneCreateInfo
	{
		BB::Slice<Light> lights{};
		uint32_t sceneWindowWidth = 0;
		uint32_t sceneWindowHeight = 0;
	};

	class SceneGraph
	{
	public:
		SceneGraph(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo);
		~SceneGraph();

		void StartScene(RecordingCommandListHandle a_GraphicList);
		void RenderScene(RecordingCommandListHandle a_GraphicList);
		void EndScene();

		void SetProjection(const glm::mat4& a_Proj);
		void SetView(const glm::mat4& a_View);

		DrawObjectHandle CreateDrawObject(const RModelHandle a_Model, const TransformHandle a_TransformHandle);
		BB::Slice<DrawObject> GetDrawObjects();
		void DestroyDrawObject(const DrawObjectHandle a_Handle);

		const RDescriptor GetSceneDescriptor() const;

	private:
		struct SceneGraph_inst* inst;
	};
}