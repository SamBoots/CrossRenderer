#include "OS/OSDevice.h"
#include "Backend/RenderBackend.h"

using namespace BB;

int main()
{
	BB::WindowHandle t_MainWindow = BB::AppOSDevice().CreateOSWindow(BB::OS_WINDOW_STYLE::MAIN, 250, 200, 1200, 800, "Unit Test Main Window");
	RenderBackend t_Backend;

	APIRenderBackendHandle t_RenderBackend;
	t_RenderBackend = t_Backend.InitBackend(t_MainWindow, RenderAPI::VULKAN, true);

	//while (BB::AppOSDevice().ProcessMessages())
	//{
		t_Backend.Update();
	//}
	t_Backend.DestroyBackend(t_RenderBackend);
	return 0;
}
