#include "Program.h"
#include "Utils/Logger.h"

#include <Windows.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <fstream>

using namespace BB;
using namespace BB::Program;

FreelistAllocator_t OSAllocator{ kbSize * 2 };

void DefaultClose(WindowHandle a_WindowHandle) {}
void DefaultResize(WindowHandle a_WindowHandle, uint32_t a_X, uint32_t a_Y) {}

static PFN_WindowCloseEvent sPFN_CloseEvent = DefaultClose;
static PFN_WindowResizeEvent sPFN_ResizeEvent = DefaultResize;

static const char* exePath;
static const char* programName;

//The OS window for Windows.
struct OSWindow
{
	HWND hwnd = nullptr;
	const char* windowName = nullptr;
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

	return DefWindowProc(a_Hwnd, a_Msg, a_WParam, a_LParam);
}

bool BB::Program::InitProgram(const InitProgramInfo& a_InitProgramInfo)
{
	programName = a_InitProgramInfo.programName;
	exePath = a_InitProgramInfo.exePath;
	return true;
}

const size_t BB::Program::VirtualMemoryPageSize()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwPageSize;
}

const size_t BB::Program::VirtualMemoryMinimumAllocation()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwAllocationGranularity;
}

void* BB::Program::ReserveVirtualMemory(const size_t a_Size)
{
	return VirtualAlloc(nullptr, a_Size, MEM_RESERVE, PAGE_NOACCESS);
}

bool BB::Program::CommitVirtualMemory(void* a_Ptr, const size_t a_Size)
{
	void* t_Ptr = VirtualAlloc(a_Ptr, a_Size, MEM_COMMIT, PAGE_READWRITE);
	return t_Ptr;
}

bool BB::Program::ReleaseVirtualMemory(void* a_Ptr)
{
	return VirtualFree(a_Ptr, 0, MEM_RELEASE);
}

const uint32_t BB::Program::LatestOSError()
{
	DWORD t_ErrorMsg = GetLastError();
	if (t_ErrorMsg == 0)
		return 0;
	LPSTR t_Message = nullptr;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, t_ErrorMsg, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&t_Message, 0, NULL);

	BB_WARNING(false, t_Message, WarningType::HIGH);

	LocalFree(t_Message);

	return static_cast<uint32_t>(t_ErrorMsg);
}

Buffer BB::Program::ReadFile(Allocator a_SysAllocator, const char* a_Path)
{
	Buffer t_FileBuffer;

	std::ifstream t_File(a_Path, std::ios::ate | std::ios::binary);

	if (!t_File.is_open())
	{
		BB_WARNING(false, "Failed to readfile!", WarningType::HIGH);
		t_FileBuffer.size = 0;
		t_FileBuffer.data = nullptr;
		return t_FileBuffer;
	}

	t_FileBuffer.size = static_cast<uint64_t>(t_File.tellg());
	t_FileBuffer.data = BBalloc(a_SysAllocator, t_FileBuffer.size);

	t_File.seekg(0);
	t_File.read(reinterpret_cast<char*>(t_FileBuffer.data), t_FileBuffer.size);
	t_File.close();

	return t_FileBuffer;
}

LibHandle BB::Program::LoadLib(const char* a_LibName)
{
	HMODULE t_Mod = LoadLibrary(a_LibName);
	if (t_Mod == NULL)
	{
		Program::LatestOSError();
		BB_ASSERT(false, "Failed to load .DLL");
	}
	return LibHandle(t_Mod);
}

void BB::Program::UnloadLib(const LibHandle a_Handle)
{
	FreeLibrary(reinterpret_cast<HMODULE>(a_Handle.ptrHandle));
}

LibFuncPtr BB::Program::LibLoadFunc(const LibHandle a_Handle, const char* a_FuncName)
{
	LibFuncPtr t_Func = GetProcAddress(reinterpret_cast<HMODULE>(a_Handle.ptrHandle), a_FuncName);
	if (t_Func == NULL)
	{
		Program::LatestOSError();
		BB_ASSERT(false, "Failed to load function from .dll");
	}
	return t_Func;
}

WindowHandle BB::Program::CreateOSWindow(const OS_WINDOW_STYLE a_Style, const int a_X, const int a_Y, const int a_Width, const int a_Height, const char* a_WindowName)
{
	OSWindow t_ReturnWindow;
	t_ReturnWindow.windowName = a_WindowName;

	WNDCLASS t_WndClass = {};
	t_WndClass.lpszClassName = t_ReturnWindow.windowName;
	t_WndClass.hInstance = t_ReturnWindow.hInstance;
	t_WndClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	t_WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	t_WndClass.lpfnWndProc = WindowProc;

	RegisterClass(&t_WndClass);
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
		programName,
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

void* BB::Program::GetOSWindowHandle(const WindowHandle a_Handle)
{
	return reinterpret_cast<HWND>(a_Handle.handle);
}

void BB::Program::GetWindowSize(const WindowHandle a_Handle, int& a_X, int& a_Y)
{
	RECT t_Rect;
	GetClientRect(reinterpret_cast<HWND>(a_Handle.handle), &t_Rect);

	a_X = t_Rect.right;
	a_Y = t_Rect.bottom;
}

void BB::Program::DirectDestroyOSWindow(const WindowHandle a_Handle)
{
	DestroyWindow(reinterpret_cast<HWND>(a_Handle.ptrHandle));
}

void BB::Program::SetCloseWindowPtr(PFN_WindowCloseEvent a_Func)
{
	sPFN_CloseEvent = a_Func;
}

void BB::Program::SetResizeEventPtr(PFN_WindowResizeEvent a_Func)
{
	sPFN_ResizeEvent = a_Func;
}

void BB::Program::ExitApp()
{
	exit(EXIT_SUCCESS);
}

bool BB::Program::ProcessMessages()
{
	MSG t_Msg{};

	while (PeekMessage(&t_Msg, NULL, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&t_Msg);
		DispatchMessage(&t_Msg);
	}

	return true;
}

const char* BB::Program::ProgramName()
{
	return programName;
}

const char* BB::Program::ProgramPath()
{
	return exePath;
}