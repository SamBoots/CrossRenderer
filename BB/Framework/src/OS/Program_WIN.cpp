#include "BBGlobal.h"
#include "Program.h"
#include "Utils/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fileapi.h>
#include <memoryapi.h>
#include <libloaderapi.h>

using namespace BB;

void DefaultClose(WindowHandle a_WindowHandle) {}
void DefaultResize(WindowHandle a_WindowHandle, uint32_t a_X, uint32_t a_Y) {}

static PFN_WindowCloseEvent sPFN_CloseEvent = DefaultClose;
static PFN_WindowResizeEvent sPFN_ResizeEvent = DefaultResize;

//The OS window for Windows.
struct OSWindow
{
	HWND hwnd = nullptr;
	const wchar* windowName = nullptr;
	HINSTANCE hInstance = nullptr;
};

//Custom callback for the Windows proc.
LRESULT CALLBACK WindowProc(HWND a_Hwnd, UINT a_Msg, WPARAM a_WParam, LPARAM a_LParam)
{
	switch (a_Msg)
	{
	case WM_QUIT:
		break;
	case WM_DESTROY:
		sPFN_CloseEvent(a_Hwnd);
		break;
	case WM_SIZE:
	{
		int t_X = static_cast<uint32_t>(LOWORD(a_LParam));
		int t_Y = static_cast<uint32_t>(HIWORD(a_LParam));
		sPFN_ResizeEvent(a_Hwnd, t_X, t_Y);
	}
	break;
	}

	return DefWindowProcW(a_Hwnd, a_Msg, a_WParam, a_LParam);
}

const size_t BB::VirtualMemoryPageSize()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwPageSize;
}

const size_t BB::VirtualMemoryMinimumAllocation()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwAllocationGranularity;
}

void* BB::ReserveVirtualMemory(const size_t a_Size)
{
	return VirtualAlloc(nullptr, a_Size, MEM_RESERVE, PAGE_NOACCESS);
}

bool BB::CommitVirtualMemory(void* a_Ptr, const size_t a_Size)
{
	void* t_Ptr = VirtualAlloc(a_Ptr, a_Size, MEM_COMMIT, PAGE_READWRITE);
	return t_Ptr;
}

bool BB::ReleaseVirtualMemory(void* a_Ptr)
{
	return VirtualFree(a_Ptr, 0, MEM_RELEASE);
}

const uint32_t BB::LatestOSError()
{
	DWORD t_ErrorMsg = GetLastError();
	if (t_ErrorMsg == 0)
		return 0;
	LPWSTR t_Message = nullptr;

	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, t_ErrorMsg, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), t_Message, 0, NULL);

	//BB_WARNING(false, t_Message, WarningType::HIGH);

	LocalFree(t_Message);

	return static_cast<uint32_t>(t_ErrorMsg);
}

LibHandle BB::LoadLib(const wchar* a_LibName)
{
	HMODULE t_Mod = LoadLibraryW(a_LibName);
	if (t_Mod == NULL)
	{
		LatestOSError();
		BB_ASSERT(false, "Failed to load .DLL");
	}
	return LibHandle(t_Mod);
}

void BB::UnloadLib(const LibHandle a_Handle)
{
	FreeLibrary(reinterpret_cast<HMODULE>(a_Handle.ptrHandle));
}

LibFuncPtr BB::LibLoadFunc(const LibHandle a_Handle, const char* a_FuncName)
{
	LibFuncPtr t_Func = GetProcAddress(reinterpret_cast<HMODULE>(a_Handle.ptrHandle), a_FuncName);
	if (t_Func == NULL)
	{
		LatestOSError();
		BB_ASSERT(false, "Failed to load function from .dll");
	}
	return t_Func;
}

//char replaced with string view later on.
OSFileHandle BB::CreateOSFile(const wchar* a_FileName)
{
	HANDLE t_CreatedFile = CreateFileW(a_FileName,
		GENERIC_WRITE | GENERIC_READ,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (t_CreatedFile == INVALID_HANDLE_VALUE)
	{
		LatestOSError();
		BB_WARNING(false, 
			"OS, failed to create file! This can be severe.",
			WarningType::HIGH);
	}
	
	return OSFileHandle(t_CreatedFile);
}

//char replaced with string view later on.
OSFileHandle BB::LoadOSFile(const wchar* a_FileName)
{
	HANDLE t_LoadedFile = CreateFileW(a_FileName,
		GENERIC_WRITE | GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (t_LoadedFile == INVALID_HANDLE_VALUE)
	{
		LatestOSError();
		BB_WARNING(false,
			"OS, failed to load file! This can be severe.",
			WarningType::HIGH);
	}

	return OSFileHandle(t_LoadedFile);
}

//Reads a loaded file.
//Buffer.data will have a dynamic allocation from the given allocator.
Buffer BB::ReadOSFile(Allocator a_SysAllocator, const OSFileHandle a_FileHandle)
{
	Buffer t_FileBuffer{};

	t_FileBuffer.size = GetOSFileSize(a_FileHandle.ptrHandle);
	t_FileBuffer.data = BBalloc(a_SysAllocator, t_FileBuffer.size);
	DWORD t_BytesRead = 0;

	if (FALSE == ReadFile(reinterpret_cast<HANDLE>(a_FileHandle.ptrHandle),
		t_FileBuffer.data,
		static_cast<DWORD>(t_FileBuffer.size),
		&t_BytesRead,
		NULL))
	{
		LatestOSError();
		BB_WARNING(false,
			"OS, failed to load file! This can be severe.",
			WarningType::HIGH);
	}

	return t_FileBuffer;
}

Buffer BB::ReadOSFile(Allocator a_SysAllocator, const wchar* a_Path)
{
	Buffer t_FileBuffer{};
	OSFileHandle t_ReadFile = LoadOSFile(a_Path);

	t_FileBuffer.size = GetOSFileSize(t_ReadFile);
	t_FileBuffer.data = BBalloc(a_SysAllocator, t_FileBuffer.size);
	DWORD t_BytesRead = 0;

	if (FALSE == ReadFile(reinterpret_cast<HANDLE>(t_ReadFile.ptrHandle),
		t_FileBuffer.data,
		static_cast<DWORD>(t_FileBuffer.size),
		&t_BytesRead,
		NULL))
	{
		LatestOSError();
		BB_WARNING(false,
			"OS, failed to load file! This can be severe.",
			WarningType::HIGH);
	}

	CloseOSFile(t_ReadFile);

	return t_FileBuffer;
}

//char replaced with string view later on.
void BB::WriteToFile(const OSFileHandle a_FileHandle, const Buffer& a_Buffer)
{
	DWORD t_BytesWriten = 0;
	if (FALSE == WriteFile(reinterpret_cast<HANDLE>(a_FileHandle.ptrHandle),
		a_Buffer.data,
		static_cast<const DWORD>(a_Buffer.size),
		&t_BytesWriten,
		NULL))
	{
		LatestOSError();
		BB_WARNING(false,
			"OS, failed to write to file!",
			WarningType::HIGH);
	}
}

//Get a file's size in bytes.
uint64_t BB::GetOSFileSize(const OSFileHandle a_FileHandle)
{
	return GetFileSize(reinterpret_cast<HANDLE>(a_FileHandle.ptrHandle), NULL);
}

void BB::SetOSFilePosition(const OSFileHandle a_FileHandle, const uint32_t a_Offset, const OS_FILE_READ_POINT a_FileReadPoint)
{
	DWORD t_Err = SetFilePointer(reinterpret_cast<HANDLE>(a_FileHandle.ptrHandle), a_Offset, NULL, static_cast<DWORD>(a_FileReadPoint));
#ifdef DEBUG
	if (t_Err == INVALID_SET_FILE_POINTER && 
		LatestOSError() == ERROR_NEGATIVE_SEEK)
	{
		BB_WARNING(false,
			"OS, Setting the file position failed by putting it in negative! WIN ERROR: ERROR_NEGATIVE_SEEK.",
			WarningType::HIGH);
	}
#endif
}

void BB::CloseOSFile(const OSFileHandle a_FileHandle)
{
	CloseHandle(reinterpret_cast<HANDLE>(a_FileHandle.ptrHandle));
}

WindowHandle BB::CreateOSWindow(const OS_WINDOW_STYLE a_Style, const int a_X, const int a_Y, const int a_Width, const int a_Height, const wchar* a_WindowName)
{
	OSWindow t_ReturnWindow;
	t_ReturnWindow.windowName = a_WindowName;

	WNDCLASSW t_WndClass = {};
	t_WndClass.lpszClassName = t_ReturnWindow.windowName;
	t_WndClass.hInstance = t_ReturnWindow.hInstance;
	t_WndClass.hIcon = LoadIconW(NULL, IDI_WINLOGO);
	t_WndClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
	t_WndClass.lpfnWndProc = WindowProc;

	RegisterClassW(&t_WndClass);
	//DWORD t_Style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;

	DWORD t_Style;
	switch (a_Style)
	{
	case OS_WINDOW_STYLE::MAIN:
		t_Style = WS_OVERLAPPEDWINDOW;
		break;
	case OS_WINDOW_STYLE::CHILD:
		t_Style = WS_OVERLAPPED | WS_THICKFRAME;
		break;
	default:
		t_Style = 0;
		BB_ASSERT(false, "Tried to create a window with a OS_WINDOW_STYLE it does not accept.");
		break;
	}

	RECT t_Rect{};
	t_Rect.left = a_X;
	t_Rect.top = a_Y;
	t_Rect.right = t_Rect.left + a_Width;
	t_Rect.bottom = t_Rect.top + a_Height;

	AdjustWindowRect(&t_Rect, t_Style, false);

	t_ReturnWindow.hwnd = CreateWindowEx(
		0,
		t_ReturnWindow.windowName,
		g_ProgramName,
		t_Style,
		t_Rect.left,
		t_Rect.top,
		t_Rect.right - t_Rect.left,
		t_Rect.bottom - t_Rect.top,
		NULL,
		NULL,
		t_ReturnWindow.hInstance,
		NULL);
	ShowWindow(t_ReturnWindow.hwnd, SW_SHOW);

	return WindowHandle(t_ReturnWindow.hwnd);
}

void* BB::GetOSWindowHandle(const WindowHandle a_Handle)
{
	return reinterpret_cast<HWND>(a_Handle.handle);
}

void BB::GetWindowSize(const WindowHandle a_Handle, int& a_X, int& a_Y)
{
	RECT t_Rect;
	GetClientRect(reinterpret_cast<HWND>(a_Handle.handle), &t_Rect);

	a_X = t_Rect.right;
	a_Y = t_Rect.bottom;
}

void BB::DirectDestroyOSWindow(const WindowHandle a_Handle)
{
	DestroyWindow(reinterpret_cast<HWND>(a_Handle.ptrHandle));
}

void BB::SetCloseWindowPtr(PFN_WindowCloseEvent a_Func)
{
	sPFN_CloseEvent = a_Func;
}

void BB::SetResizeEventPtr(PFN_WindowResizeEvent a_Func)
{
	sPFN_ResizeEvent = a_Func;
}

void BB::ExitApp()
{
	exit(EXIT_SUCCESS);
}

bool BB::ProcessMessages()
{
	MSG t_Msg{};

	while (PeekMessage(&t_Msg, NULL, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&t_Msg);
		DispatchMessage(&t_Msg);
	}

	return true;
}