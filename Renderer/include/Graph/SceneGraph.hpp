#pragma once
#include "FrameGraphCommon.hpp"
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

	class SceneGraph
	{
	public:
		SceneGraph(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo);
		SceneGraph(Allocator a_Allocator, Allocator a_TemporaryAllocator, const char* a_JsonPath);
		~SceneGraph();

		operator FrameGraphRenderPass();

		void StartScene(const CommandListHandle a_GraphicList);
		void RenderScene(const CommandListHandle a_GraphicList, const RENDER_IMAGE_LAYOUT a_CurrentLayout, const RENDER_IMAGE_LAYOUT a_RenderLayout, const RENDER_IMAGE_LAYOUT a_EndLayout);
		void EndScene(const CommandListHandle a_GraphicList);

		void SetProjection(const Mat4x4& a_Proj);
		void SetView(const Mat4x4& a_View);

		void RenderModel(const RModelHandle a_Model, const Mat4x4& a_Transform);
		void RenderModels(const RModelHandle* a_Models, const Mat4x4* a_Transforms, const uint32_t a_ObjectCount);

		//TEMP, should be local to the scenegraph.cpp
		BB::Slice<Light> GetLights();
		const RDescriptor GetSceneDescriptor() const;
		const char* GetSceneName() const;

		//scuffed, temporarily.
		const RDescriptor GetMeshDescriptor() const;
		//scuffed, temporarily.
		const PipelineHandle GetPipelineHandle() const;

	private:
		void Init(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo);
		struct SceneGraph_inst* inst;
	};
}