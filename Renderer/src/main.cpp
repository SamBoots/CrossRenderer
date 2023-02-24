#include "BBMain.h"
#include "OS/Program.h"
#include "Frontend/RenderFrontend.h"

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
	BBInitInfo t_BBInitInfo;
	t_BBInitInfo.exePath = argv[0];
	t_BBInitInfo.programName = L"Crossrenderer";
	InitBB(t_BBInitInfo);
	BB_LOG(argv[0]);

	int t_WindowWidth = 1280;
	int t_WindowHeight = 720;

	RenderInitInfo t_RenderInfo{};
	t_RenderInfo.windowHandle = BB::CreateOSWindow(
		BB::OS_WINDOW_STYLE::MAIN,
		250,
		200,
		t_WindowWidth,
		t_WindowHeight,
		L"CrossRenderer");
	//Set the pointers later since resize event gets called for some reason on window creation.
	SetCloseWindowPtr(WindowQuit);
	SetResizeEventPtr(WindowResize);
#ifdef _DEBUG
	t_RenderInfo.debug = true;
#else
	t_RenderInfo.debug = false;
#endif //_DEBUG
#ifdef USE_VULKAN
	t_RenderInfo.renderAPI = RENDER_API::VULKAN;
	//load DLL
	t_RenderInfo.renderDll = BB::LoadLib(L"BB_VulkanDLL");
#elif USE_DIRECTX12
	t_RenderInfo .renderAPI = RENDER_API::DX12;
	//load DLL
	t_RenderInfo.renderDll = BB::LoadLib(L"BB_DirectXDLL");
#endif //choose graphicsAPI.

	Render::InitRenderer(t_RenderInfo);
	CameraBufferInfo info;
	info.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f));
	info.projection = glm::perspective(glm::radians(45.0f),
		t_WindowWidth / (float)t_WindowHeight,
		0.1f,
		10.0f);

	uint32_t t_MatrixSize;
	void* t_MemRegion = Render::GetMatrixBufferSpace(t_MatrixSize);
	TransformPool t_TransformPool(m_ScopeAllocator, t_MemRegion, t_MatrixSize);

	TransformHandle t_TransHandle1 = t_TransformPool.CreateTransform(glm::vec3(0, -1, 0));
	Transform& t_Transform1 = t_TransformPool.GetTransform(t_TransHandle1);

	TransformHandle t_TransHandle2 = t_TransformPool.CreateTransform(glm::vec3(0, 1, 0));
	Transform& t_Transform2 = t_TransformPool.GetTransform(t_TransHandle2);

	Render::SetProjection(info.projection);
	Render::SetView(info.view);

	Vertex t_Vertex[4];
	t_Vertex[0] = { {-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f} };
	t_Vertex[1] = { {0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f} };
	t_Vertex[3] = { {-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f} };

	const uint32_t t_Indices[] = {
	0, 1, 2, 2, 3, 0
	};

	CreateRawModelInfo t_ModelInfo;
	t_ModelInfo.vertices = Slice(t_Vertex, _countof(t_Vertex));
	t_ModelInfo.indices = Slice(t_Indices, _countof(t_Indices));

	LoadModelInfo t_LoadInfo{};
	t_LoadInfo.modelType = MODEL_TYPE::GLTF;
	t_LoadInfo.path = "../Resources/Models/cube.gltf";
	

	//Start frame before we upload.
	Render::StartFrame();

	//t_ModelInfo.pipeline = 
	RModelHandle t_Model = Render::CreateRawModel(t_ModelInfo);
	RModelHandle t_gltfCube = Render::LoadModel(t_LoadInfo);
	DrawObjectHandle t_DrawObj1 = Render::CreateDrawObject(t_gltfCube,
		t_TransHandle1);
	DrawObjectHandle t_DrawObj2 = Render::CreateDrawObject(t_Model,
		t_TransHandle2);

	static auto t_StartTime = std::chrono::high_resolution_clock::now();
	auto t_CurrentTime = std::chrono::high_resolution_clock::now();
	while (!t_Quit)
	{
		ProcessMessages();

		float t_DeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(t_CurrentTime - t_StartTime).count();

		t_Transform1.SetRotation(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(-90.0f * t_DeltaTime));
		t_Transform2.SetRotation(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(20.0f * t_DeltaTime));
		t_TransformPool.UpdateTransforms();

		Render::Update(t_DeltaTime);
		Render::EndFrame();

		t_CurrentTime = std::chrono::high_resolution_clock::now();
		Render::StartFrame();
	}

	//Move this to the renderer?
	BB::UnloadLib(t_RenderInfo.renderDll);

	return 0;
}
