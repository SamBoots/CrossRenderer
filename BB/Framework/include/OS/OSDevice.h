#pragma once
#include <cstdlib>
#include <cstdint>
#include "Common.h"
#include "AllocTypes.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

namespace BB
{
	struct OSDevice_o;


	enum class OS_WINDOW_STYLE
	{
		MAIN, //This window has a menu bar.
		CHILD //This window does not have a menu bar.
	};


	class OSDevice
	{
	public:
		OSDevice();
		~OSDevice();

		//just delete these for safety, copies might cause errors.
		OSDevice(const OSDevice&) = delete;
		OSDevice(const OSDevice&&) = delete;
		OSDevice& operator =(const OSDevice&) = delete;
		OSDevice& operator =(OSDevice&&) = delete;

		//The size of a virtual memory page on the OS.
		const size_t VirtualMemoryPageSize() const;
		//The minimum virtual allocation size you can do. 
		//TODO: Get the linux variant of this.
		const size_t VirtualMemoryMinimumAllocation() const;

		//Prints the latest OS error and returns the error code, if it has no error code it returns 0.
		const uint32_t LatestOSError() const;

		//Reads an external file from path.
		Buffer ReadFile(Allocator a_SysAllocator, const char* a_Path);

		WindowHandle CreateOSWindow(OS_WINDOW_STYLE a_Style, int a_X, int a_Y, int a_Width, int a_Height, const char* a_WindowName);
		//Get the OS window handle (hwnd for windows as en example. Reinterpret_cast the void*.
		void* GetOSWindowHandle(WindowHandle a_Handle);
		void GetWindowSize(WindowHandle a_Handle, int& a_X, int& a_Y);
		void DestroyOSWindow(WindowHandle a_Handle);

		//Exits the application.
		void ExitApp() const;

		bool ProcessMessages() const;

		//Get the path where the project's exe file is located.
		char* GetExePath(Allocator a_SysAllocator) const;

	private:
		OSDevice_o* m_OSDevice;
	};


	OSDevice& AppOSDevice();
}