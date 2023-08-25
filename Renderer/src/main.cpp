#include "BBMain.h"
#include "OS/Program.h"
#include "OS/HID.h"
#include "Frontend/Camera.h"
#include "RenderFrontend.h"
#include "Graph/SceneGraph.hpp"
#include "Graph/FrameGraph.hpp"
#include "imgui_impl_CrossRenderer.h"
#include "BBThreadScheduler.hpp"
#include "Editor.h"

#include "AssetLoader.hpp"
#include "Math.inl"

#include <chrono>

using namespace BB;
bool t_Quit = false;

void WindowQuit(WindowHandle a_Handle)
{
	t_Quit = true;

	Render::DestroyRenderer();
}

void WindowResize(WindowHandle a_Handle, uint32_t a_X, uint32_t a_Y)
{
	Render::ResizeWindow(a_X, a_Y);
}

LinearAllocator_t m_ScopeAllocator{2 * kbSize};

int main(int argc, char** argv)
{
	BBInitInfo t_BBInitInfo{};
	t_BBInitInfo.exePath = argv[0];
	t_BBInitInfo.programName = L"Crossrenderer";
	InitBB(t_BBInitInfo);
	BB_LOG(argv[0]);
	BB_LOG(L"Lol, lmao wide char printing works said the scorpion.");
	Threads::InitThreads(4);

	int t_WindowWidth = 1280;
	int t_WindowHeight = 720;
	WindowHandle t_Window = BB::CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		t_WindowWidth,
		t_WindowHeight,
		L"CrossRenderer");
	RenderInitInfo t_RenderInfo{};
	t_RenderInfo.windowHandle = t_Window;
	//Set the pointers later since resize event gets called for some reason on window creation.
	SetCloseWindowPtr(WindowQuit);
	SetResizeEventPtr(WindowResize);
#ifdef _DEBUG
	t_RenderInfo.debug = true;
#else
	t_RenderInfo.debug = false;
#endif //_DEBUG
	t_RenderInfo.renderAPI = RENDER_API::VULKAN;
	Render::InitRenderer(t_RenderInfo);

	Camera t_Cam{ float3{2.0f, 2.0f, 2.0f}, 0.35f };
	FreelistAllocator_t t_SceneAllocator{ mbSize * 32 };
	TemporaryAllocator t_TempAllocator{ t_SceneAllocator };
	SceneCreateInfo t_SceneCreateInfo;
	SceneGraph t_Scene{ t_SceneAllocator, t_TempAllocator, "Resources/Json/test_scene.json" };

	Mat4x4 t_ProjMat = Mat4x4Perspective(ToRadians(60.0f),
		t_WindowWidth / (float)t_WindowHeight,
		.001f, 10000.0f);

	t_Scene.SetProjection(t_ProjMat);
	t_Scene.SetView(t_Cam.CalculateView());

	Vertex t_Vertex[4];
	t_Vertex[0] = { {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} };
	t_Vertex[1] = { {0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} };
	t_Vertex[3] = { {-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f} };

	const uint32_t t_Indices[] = {0, 1, 2, 2, 3, 0};

	CreateRawModelInfo t_ModelInfo{};
	t_ModelInfo.vertices = Slice(t_Vertex, _countof(t_Vertex));
	t_ModelInfo.indices = Slice(t_Indices, _countof(t_Indices));
	t_ModelInfo.meshDescriptor = t_Scene.GetMeshDescriptor();
	t_ModelInfo.pipeline = t_Scene.GetPipelineHandle();

	LoadModelInfo t_LoadInfo{};
	t_LoadInfo.modelType = MODEL_TYPE::GLTF;
	t_LoadInfo.path = "Resources/Models/Duck.gltf";
	t_LoadInfo.meshDescriptor = t_Scene.GetMeshDescriptor();
	t_LoadInfo.pipeline = t_Scene.GetPipelineHandle();

	FrameGraph t_FrameGraph{};
	t_FrameGraph.RegisterRenderPass(t_Scene);

	TransformPool transformPool(t_SceneAllocator, 256);

	//I do not like this, it should be automated but for now to test I just cheat.
	CommandList* t_CmdList = Render::GetTransferQueue().GetCommandList();
	RModelHandle t_gltfCube = Render::LoadModel(t_CmdList->list, t_LoadInfo);
	RModelHandle t_Model = Render::CreateRawModel(t_CmdList->list, t_ModelInfo);
	RenderBackend::EndCommandList(t_CmdList->list);
	Render::GetTransferQueue().ExecuteCommands(&t_CmdList, 1, nullptr, nullptr, 0);
	Render::GetTransferQueue().WaitIdle();
	//shit code over


	const TransformHandle t_TransformHandle1 = transformPool.CreateTransform(
		float3{ 0, -1, 1 }, float3{ 0, 0, 1 }, 90.f);
	Transform& t_Transform1 = transformPool.GetTransform(t_TransformHandle1);

	const TransformHandle t_TransformHandle2 = transformPool.CreateTransform(float3{ 0, 1, 0 });
	Transform& t_Transform2 = transformPool.GetTransform(t_TransformHandle2);

	static auto t_StartTime = std::chrono::high_resolution_clock::now();
	auto t_CurrentTime = std::chrono::high_resolution_clock::now();

	InputEvent t_InputEvents[INPUT_EVENT_BUFFER_MAX];
	size_t t_InputEventCount = 0;
	bool t_FreezeCam = false;
	float t_DeltaTime = 0;

	while (!t_Quit)
	{
		ProcessMessages(t_Window);
		PollInputEvents(t_InputEvents, t_InputEventCount);

		for (size_t i = 0; i < t_InputEventCount; i++)
		{
			const InputEvent& t_Event = t_InputEvents[i];
			//if imgui wants to take the input then do not send it to the engine.
			if (ImGui_ImplCross_ProcessInput(t_Event))
				continue;
			if (t_Event.inputType == INPUT_TYPE::KEYBOARD)
			{
				float3 t_CamMove{};
				if (t_Event.keyInfo.keyPressed)
					switch (t_Event.keyInfo.scancode)
					{
					case KEYBOARD_KEY::_ESCAPE:
						g_ShowEditor = !g_ShowEditor;
						break;
					case KEYBOARD_KEY::_F:
						t_FreezeCam = !t_FreezeCam;
						break;
					case KEYBOARD_KEY::_W:
						t_CamMove.y = 1;
						break;
					case KEYBOARD_KEY::_S:
						t_CamMove.y = -1;
						break;
					case KEYBOARD_KEY::_A:
						t_CamMove.x = 1;
						break;
					case KEYBOARD_KEY::_D:
						t_CamMove.x = -1;
						break;
					case KEYBOARD_KEY::_X:
						t_CamMove.z = 1;
						break;
					case KEYBOARD_KEY::_Z:
						t_CamMove.z = -1;
						break;
					default:
						break;
					}
				t_Cam.Move(t_CamMove);
			}
			else if (t_Event.inputType == INPUT_TYPE::MOUSE)
			{
				const MouseInfo& t_Mouse = t_Event.mouseInfo;
				const float2 t_MouseMove = (t_Event.mouseInfo.moveOffset * t_DeltaTime) * 0.003f;
				if (!t_FreezeCam)
					t_Cam.Rotate(t_MouseMove.x, t_MouseMove.y);

				if (t_Mouse.right_released)
					FreezeMouseOnWindow(t_Window);
				if (t_Mouse.left_released)
					UnfreezeMouseOnWindow();
			}
		}
		t_Scene.SetView(t_Cam.CalculateView());
		t_FrameGraph.BeginRendering();

		t_Scene.RenderModel(t_gltfCube, t_Transform1.CreateMatrix());
		Mat4x4 t_Matrix = t_Transform2.CreateMatrix();
		t_Scene.RenderModel(t_Model, t_Matrix);
		Editor::StartEditorFrame();
		Editor::DisplaySceneInfo(t_Scene);

		t_DeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(t_CurrentTime - t_StartTime).count();

		t_Transform1.SetRotation(float3{ 0.0f, 1.0f, 0.0f }, 1.1f * t_DeltaTime);
		t_Transform2.SetRotation(float3{ 0.0f, 0.0f, 1.0f }, 1.0f * t_DeltaTime);

		t_FrameGraph.Render();
		t_FrameGraph.EndRendering();

		t_CurrentTime = std::chrono::high_resolution_clock::now();
	}

	//Move this to the renderer?
	BB::UnloadLib(t_RenderInfo.renderDll);
	Threads::DestroyThreads();

	return 0;
}
