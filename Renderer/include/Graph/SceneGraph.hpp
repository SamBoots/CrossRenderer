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

	struct SceneObjectCreateInfo
	{
		char* name; 
		RModelHandle model;
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
		SceneGraph(Allocator a_Allocator, Allocator a_TemporaryAllocator, const char* a_JsonPath);
		~SceneGraph();

		operator FrameGraphRenderPass();

		void StartScene(const CommandListHandle a_GraphicList);
		void RenderScene(const CommandListHandle a_GraphicList, const RENDER_IMAGE_LAYOUT a_CurrentLayout, const RENDER_IMAGE_LAYOUT a_RenderLayout, const RENDER_IMAGE_LAYOUT a_EndLayout);
		void EndScene(const CommandListHandle a_GraphicList);

		void SetProjection(const Mat4x4& a_Proj);
		void SetView(const Mat4x4& a_View);

		SceneObjectHandle CreateSceneObject(const SceneObjectCreateInfo& a_CreateInfo, const float3 a_Position = float3{ 0,0,0 }, const float3 a_Axis = float3{ 0,0,0 }, const float a_Radians = 0, const float3 a_Scale = float3{ 1,1,1 });
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
		void Init(Allocator a_Allocator, const SceneCreateInfo& a_CreateInfo);
		struct SceneGraph_inst* inst;
	};
}