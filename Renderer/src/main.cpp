#include "OS/OSDevice.h"
#include "Backend/RenderBackend.h"

using namespace BB;


RenderBackend t_Backend;

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

	OS::SetResizeEventPtr(WindowResize);

	t_Backend.InitBackend(t_MainWindow, RenderAPI::VULKAN, true);

	bool hasWindows = true;
	while (hasWindows)
	{
		t_Backend.Update();

		hasWindows = OS::ProcessMessages();
	}

	t_Backend.DestroyBackend();
	return 0;
}
