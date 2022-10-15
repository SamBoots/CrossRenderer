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
		"Unit Test Main Window");

	OS::SetCloseWindowPtr(WindowQuit);
	OS::SetResizeEventPtr(WindowResize);

#ifdef _DEBUG
	Render::InitRenderer(t_MainWindow, RenderAPI::VULKAN, true);
#else
	Render::InitRenderer(t_MainWindow, RenderAPI::VULKAN, false);
#endif //_DEBUG

	while (!t_Quit)
	{
		Render::Update();
		OS::ProcessMessages();
	}

	return 0;
}
