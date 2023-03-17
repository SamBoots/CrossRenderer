////// PROGRAM.h //////
/// Program abstracts most OS calls and handles creation of windows,
/// loading of external files and book keeping of data that is part of
/// the program. Such as the exe location and name.
/// 
/// Replaces OSDevice.h from the older BB version
////// PROGRAM.h //////

#pragma once
#include <cstdlib>
#include <cstdint>
#include "Common.h"
#include "BBMemory.h"
#include "BBString.h"

namespace BB
{
	using LibFuncPtr = void*;
	typedef void (*PFN_WindowResizeEvent)(const WindowHandle a_WindowHandle, const uint32_t a_X, const uint32_t a_Y);
	typedef void (*PFN_WindowCloseEvent)(const WindowHandle a_WindowHandle);

	enum class OS_WINDOW_STYLE
	{
		MAIN, //This window has a menu bar.
		CHILD //This window does not have a menu bar.
	};

	//This should match the interger defines for the specific OS
	enum class OS_FILE_READ_POINT : uint32_t
	{
#ifdef WIN32
		BEGIN = 0,
		CURRENT = 1,
		END = 2
#elif _LINUX //Need to test for linux
		BEGIN = 0,
		CURRENT = 1,
		END = 2
#endif
	};

	//Hide this in the future so that users cannot access it.
	void InitProgram();

	//The size of a virtual memory page on the OS.
	const size_t VirtualMemoryPageSize();
	//The minimum virtual allocation size you can do. 
	//TODO: Get the linux variant of this.
	const size_t VirtualMemoryMinimumAllocation();

	void* ReserveVirtualMemory(const size_t a_Size);
	bool CommitVirtualMemory(void* a_Ptr, const size_t a_Size);
	bool ReleaseVirtualMemory(void* a_Ptr);

	//Prints the latest OS error and returns the error code, if it has no error code it returns 0.
	const uint32_t LatestOSError();

	//Load a dynamic library
	LibHandle LoadLib(const wchar* a_LibName);
	//Unload a dynamic library
	void UnloadLib(const LibHandle a_Handle);
	//Load dynamic library function
	LibFuncPtr LibLoadFunc(const LibHandle a_Handle, const char* a_FuncName);

	//Write to the standard C++ console if it's available.
	void WriteToConsole(const char* a_String, uint32_t a_StrLength);
	//Write to the standard C++ console if it's available.
	void WriteToConsole(const wchar_t* a_String, uint32_t a_StrLength);

	//char replaced with string view later on.
	//handle is 0 if it failed to create the file, it will assert on failure.
	OSFileHandle CreateOSFile(const wchar* a_FileName);
	//char replaced with string view later on.
	//handle is 0 if it failed to load the file.
	OSFileHandle LoadOSFile(const wchar* a_FileName);
	//char replaced with string view later on.
	void WriteToFile(const OSFileHandle a_FileHandle, const Buffer& a_Buffer);
	//Reads a loaded file.
	//Buffer.data will have a dynamic allocation from the given allocator.
	Buffer ReadOSFile(Allocator a_SysAllocator, const OSFileHandle a_FileHandle);
	//Reads an external file from path.
	//Buffer.data will have a dynamic allocation from the given allocator.
	Buffer ReadOSFile(Allocator a_SysAllocator, const wchar* a_Path);
	//Get a file's size in bytes.
	uint64_t GetOSFileSize(const OSFileHandle a_FileHandle);
	//Set the file position, a_Offset can be 0 if you just want to move it to BEGIN or END.
	void SetOSFilePosition(const OSFileHandle a_FileHandle, const uint32_t a_Offset, const OS_FILE_READ_POINT a_FileReadPoint);

	void CloseOSFile(const OSFileHandle a_FileHandle);

	WindowHandle CreateOSWindow(const OS_WINDOW_STYLE a_Style, const int a_X, const int a_Y, const int a_Width, const int a_Height, const wchar* a_WindowName);
	//Get the OS window handle (hwnd for windows as en example. Reinterpret_cast the void* to the hwnd).
	void* GetOSWindowHandle(const WindowHandle a_Handle);
	void GetWindowSize(const WindowHandle a_Handle, int& a_X, int& a_Y);
	void DirectDestroyOSWindow(const WindowHandle a_Handle);
	void FreezeMouseOnWindow(const WindowHandle a_Handle);
	void UnfreezeMouseOnWindow();

	//The function that will be called when a window is closed.
	void SetCloseWindowPtr(PFN_WindowCloseEvent a_Func);
	//The function that will be called when a window is resized.
	void SetResizeEventPtr(PFN_WindowResizeEvent a_Func);

	//Exits the application.
	void ExitApp();

	//Process the OS (or window) messages
	bool ProcessMessages(const WindowHandle a_WindowHandle);

	//Get the program name.
	const wchar* ProgramName();
	//Get the path where the project's exe file is located.
	const char* ProgramPath();
}