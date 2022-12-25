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
	BB_LOG(argv[0]);


	int t_WindowWidth = 1200;
	int t_WindowHeight = 800;

	BB::WindowHandle t_MainWindow = BB::Program::CreateOSWindow(
		BB::Program::OS_WINDOW_STYLE::MAIN, 
		250,
		200, 
		t_WindowWidth,
		t_WindowHeight,
		"CrossRenderer");

	Program::SetCloseWindowPtr(WindowQuit);
	Program::SetResizeEventPtr(WindowResize);

#ifdef _DEBUG
	bool debugRenderer = true;
#else
	bool debugRenderer = false;
#endif //_DEBUG
#ifdef USE_VULKAN
	RenderAPI api = RenderAPI::VULKAN;
	//load DLL
	BB::LibHandle t_RenderDLL = BB::Program::LoadLib("BB_VulkanDLL");
#elif USE_DIRECTX12
	RenderAPI api = RenderAPI::DX12;
	//load DLL
	BB::LibHandle t_RenderDLL = BB::Program::LoadLib("BB_DirectXDLL");
#endif //choose graphicsAPI.

	Render::InitRenderer(t_MainWindow, t_RenderDLL, debugRenderer);
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
	t_Vertex[0] = { {-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} };
	t_Vertex[1] = { {0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} };
	t_Vertex[2] = { {0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} };
	t_Vertex[3] = { {-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f} };

	const uint32_t t_Indices[] = {
	0, 1, 2, 2, 3, 0
	};

	CreateRawModelInfo t_ModelInfo;
	t_ModelInfo.vertices = Slice(t_Vertex, _countof(t_Vertex));
	t_ModelInfo.indices = Slice(t_Indices, _countof(t_Indices));

	//t_ModelInfo.pipeline = 
	RModelHandle t_Model = Render::CreateRawModel(t_ModelInfo);
	DrawObjectHandle t_DrawObj1 = Render::CreateDrawObject(t_Model, 
		t_TransHandle1);
	DrawObjectHandle t_DrawObj2 = Render::CreateDrawObject(t_Model,
		t_TransHandle2);

	static auto t_StartTime = std::chrono::high_resolution_clock::now();
	auto t_CurrentTime = std::chrono::high_resolution_clock::now();
	while (!t_Quit)
	{
		float t_DeltaTime = std::chrono::duration<float, std::chrono::seconds::period>(t_CurrentTime - t_StartTime).count();

		t_Transform1.SetRotation(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(-90.0f * t_DeltaTime));
		t_Transform2.SetRotation(glm::vec3(0.0f, 0.0f, 1.0f), glm::radians(20.0f * t_DeltaTime));
		t_TransformPool.UpdateTransforms();
		Render::Update(t_DeltaTime);
		Program::ProcessMessages();

		t_CurrentTime = std::chrono::high_resolution_clock::now();
	}

	BB::Program::UnloadLib(t_RenderDLL);

	return 0;
}
