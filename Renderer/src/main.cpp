#include "OS/OSDevice.h"
#include "Backend/RenderBackend.h"

using namespace BB;


RenderBackend t_Backend;
bool t_Quit = false;

void WindowQuit(WindowHandle a_Handle)
{
	t_Quit = true;

	t_Backend.DestroyBackend();
}

void WindowResize(WindowHandle a_Handle, uint32_t a_X, uint32_t a_Y)
{
	t_Backend.ResizeWindow(a_X, a_Y);
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
	t_Backend.InitBackend(t_MainWindow, RenderAPI::VULKAN, true);
#else
	t_Backend.InitBackend(t_MainWindow, RenderAPI::VULKAN, false);
#endif //_DEBUG

	while (!t_Quit)
	{
		t_Backend.Update();
		OS::ProcessMessages();
	}

	return 0;
}
