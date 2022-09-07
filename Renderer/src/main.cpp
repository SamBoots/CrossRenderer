#include "OS/OSDevice.h"
#include "Backend/RenderBackend.h"

int main()
{
	BB::WindowHandle t_MainWindow = BB::AppOSDevice().CreateOSWindow(BB::OS_WINDOW_STYLE::MAIN, 250, 200, 250, 200, "Unit Test Main Window");
	RenderBackend t_Backend;
	t_Backend.InitBackend(t_MainWindow, RenderAPI::VULKAN, true);

	//while (BB::AppOSDevice().ProcessMessages())
	//{
	t_Backend.Update();
	//}
	t_Backend.DestroyBackend();
	return 0;
}
