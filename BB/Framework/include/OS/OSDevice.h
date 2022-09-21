#pragma once
#include <cstdlib>
#include <cstdint>
#include "Common.h"
#include "AllocTypes.h"
#include "Utils/Slice.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

namespace BB
{
	namespace OS
	{
		enum class OS_WINDOW_STYLE
		{
			MAIN, //This window has a menu bar.
			CHILD //This window does not have a menu bar.
		};

		enum class OS_OPERATION_TYPE
		{
			CLOSE_WINDOW,
			RESIZE_WINDOW
		};

		struct OSOperation
		{
			OS_OPERATION_TYPE operation;
			void* window;
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
		void DestroyOSWindow(WindowHandle a_Handle);

		//Add an OS operation that will be done during the user defined event queue of the engine.
		void AddOSOperation(OSOperation t_Operation);
		bool PeekOSOperations(OSOperation& t_Operation);
		void ProcessOSOperation(const OSOperation& t_Operation);
		void ClearOSOperations();

		//Exits the application.
		void ExitApp();

		bool ProcessMessages();

		//Get the path where the project's exe file is located.
		char* GetExePath(Allocator a_SysAllocator);
	}
}