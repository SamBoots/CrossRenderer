#pragma once
#include "RenderFrontendCommon.h"
#include "BBMemory.h"

namespace BB
{
	struct SceneCreateInfo
	{
		const char* sceneName = nullptr;
		BB::Slice<Light> lights{};
		uint32_t sceneWindowWidth = 0;
		uint32_t sceneWindowHeight = 0;
	};

	struct SceneObjectCreateInfo
	{
		char* name; 
		RModelHandle model;
		RTexture texture;
	};

	struct SceneObject
	{
		const char* name;
		RModelHandle modelHandle{};
		TransformHandle transformHandle{};
		RTexture texture1;
	};

	using SceneObjectHandle = FrameworkHandle<struct RDrawObjectHandleTag>;

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

		SceneObjectHandle CreateSceneObject(const SceneObjectCreateInfo& a_CreateInfo, const glm::vec3 a_Position = glm::vec3(0), const glm::vec3 a_Axis = glm::vec3(0), const float a_Radians = 0, const glm::vec3 a_Scale = glm::vec3(1));
		void DestroySceneObject(const SceneObjectHandle a_Handle);

		Transform& GetTransform(const SceneObjectHandle a_Handle) const;
		Transform& GetTransform(const TransformHandle a_Handle) const;
		
		//TEMP, should be local to the scenegraph.cpp
		BB::Slice<SceneObject> GetSceneObjects();
		//TEMP, should be local to the scenegraph.cpp
		BB::Slice<Light> GetLights();
		const RDescriptor GetSceneDescriptor() const;
		const char* GetSceneName() const;

		//scuffed, temporarily.
		const RDescriptor GetMeshDescriptor() const;
		//scuffed, temporarily.
		const PipelineHandle GetPipelineHandle() const;

	private:
		struct SceneGraph_inst* inst;
	};
}