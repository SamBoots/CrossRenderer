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

		DrawObjectHandle CreateDrawObject(const RModelHandle a_Model, const glm::vec3 a_Position = glm::vec3(0), const glm::vec3 a_Axis = glm::vec3(0), const float a_Radians = 0, const glm::vec3 a_Scale = glm::vec3(1));
		BB::Slice<DrawObject> GetDrawObjects();
		void DestroyDrawObject(const DrawObjectHandle a_Handle);

		Transform& GetTransform(const DrawObjectHandle a_Handle) const;
		const RDescriptor GetSceneDescriptor() const;

		//scuffed, temporarily.
		const RDescriptor GetMeshDescriptor() const;
		//scuffed, temporarily.
		const PipelineHandle GetPipelineHandle() const;

	private:
		struct SceneGraph_inst* inst;
	};
}