#pragma once
#include <cstdlib>
#include <cstdint>
#include "Common.h"
#include "BBMemory.h"
#include "Utils/Slice.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

namespace BB
{
	namespace OS
	{
		typedef void (*PFN_WindowResizeEvent)(WindowHandle a_WindowHandle, uint32_t a_X, uint32_t a_Y);
		typedef void (*PFN_WindowCloseEvent)(WindowHandle a_WindowHandle);

		enum class OS_WINDOW_STYLE
		{
			MAIN, //This window has a menu bar.
			CHILD //This window does not have a menu bar.
		};

		//The size of a virtual memory page on the OS.
		const size_t VirtualMemoryPageSize();
		//The minimum virtual allocation size you can do. 
		//TODO: Get the linux variant of this.
		const size_t VirtualMemoryMinimumAllocation();

		//Prints the latest OS error and returns the error code, if it has no error code it returns 0.
		const uint32_t LatestOSError();

		//Reads an external file from path.
		Buffer ReadFile(Allocator a_SysAllocator, const char* a_Path);

		WindowHandle CreateOSWindow(OS_WINDOW_STYLE a_Style, int a_X, int a_Y, int a_Width, int a_Height, const char* a_WindowName);
		//Get the OS window handle (hwnd for windows as en example. Reinterpret_cast the void*.
		void* GetOSWindowHandle(WindowHandle a_Handle);
		void GetWindowSize(WindowHandle a_Handle, int& a_X, int& a_Y);
		void DirectDestroyOSWindow(WindowHandle a_Handle);

		//Function pointer setup.
		void SetCloseWindowPtr(PFN_WindowCloseEvent a_Func);
		void SetResizeEventPtr(PFN_WindowResizeEvent a_Func);

		//Exits the application.
		void ExitApp();

		bool ProcessMessages();

		//Get the path where the project's exe file is located.
		char* GetExePath(Allocator a_SysAllocator);
	}
}