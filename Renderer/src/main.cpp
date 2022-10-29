#include "OS/OSDevice.h"
#include "Frontend/RenderFrontend.h"

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

int main()
{
	//load DLL
	BB::LibHandle t_RenderDLL = BB::OS::LoadLib("BB_VulkanDLL");

	BB::WindowHandle t_MainWindow = BB::OS::CreateOSWindow(
		BB::OS::OS_WINDOW_STYLE::MAIN, 
		250, 
		200, 
		1200, 
		800, 
		"CrossRenderer");

	OS::SetCloseWindowPtr(WindowQuit);
	OS::SetResizeEventPtr(WindowResize);

#ifdef _DEBUG
	bool debugRenderer = true;
#else
	bool debugRenderer = false;
#endif //_DEBUG
#ifdef USE_VULKAN
	RenderAPI api = RenderAPI::VULKAN;
#elif USE_DIRECTX12
	RenderAPI api = RenderAPI::DX12;
#endif //choose graphicsAPI.

	Render::InitRenderer(t_MainWindow, t_RenderDLL, debugRenderer);

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

	while (!t_Quit)
	{
		Render::StartFrame();
		//Record rendering commands.
		auto t_Recording = Render::StartRecordCmds();
		Render::DrawModel(t_Recording, t_Model);
		Render::EndRecordCmds(t_Recording);

		Render::EndFrame();
		OS::ProcessMessages();
	}

	BB::OS::UnloadLib(t_RenderDLL);

	return 0;
}
