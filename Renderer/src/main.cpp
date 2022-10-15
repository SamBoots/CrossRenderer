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

	RenderAPI api = RenderAPI::VULKAN;
	Render::InitRenderer(t_MainWindow, api, debugRenderer);

	while (!t_Quit)
	{
		Render::Update();
		OS::ProcessMessages();
	}

	return 0;
}
