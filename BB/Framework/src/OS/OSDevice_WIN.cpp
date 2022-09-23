#include "Utils/Logger.h"
#include "OSDevice.h"

#include <cstdint>
#include <Windows.h>
#include <fstream>
#include "Storage/Hashmap.h"
#include "Storage/Array.h"

//The OS window for Windows.
struct OSWindow
{
	HWND hwnd = nullptr;
	const char* windowName = nullptr;
	HINSTANCE hInstance = nullptr;
};

using namespace BB;
using namespace BB::OS;

typedef FreeListAllocator_t OSAllocator_t;
typedef LinearAllocator_t OSTempAllocator_t;

OSAllocator_t OSAllocator{ mbSize * 8 };
OSTempAllocator_t OSTempAllocator{ mbSize * 4 };

struct OSDevice
{
	//Special array for all the windows. Stored seperately 
	OL_HashMap<HWND, OSWindow> OSWindows{ OSAllocator, 8 };
	//Array operations will very likely never exceed 8.
	Array<OSOperation> OSOperations{ OSAllocator, 8 };
};

static OSDevice s_OSDevice{};

//Custom callback for the Windows proc.
LRESULT CALLBACK WindowProc(HWND a_Hwnd, UINT a_Msg, WPARAM a_WParam, LPARAM a_LParam)
{
	OSOperation windowMsg;
	switch (a_Msg)
	{
	case WM_QUIT:
		break;
	case WM_DESTROY:
		break;
	}

	return DefWindowProc(a_Hwnd, a_Msg, a_WParam, a_LParam);
}


const size_t BB::OS::VirtualMemoryPageSize()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwPageSize;
}

const size_t BB::OS::VirtualMemoryMinimumAllocation()
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwAllocationGranularity;
}

const uint32_t BB::OS::LatestOSError()
{
	DWORD t_ErrorMsg = GetLastError();
	if (t_ErrorMsg == 0)
		return 0;
	LPSTR t_Message = nullptr;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, t_ErrorMsg, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&t_Message, 0, NULL);

	LocalFree(t_Message);

	BB_WARNING(false, t_Message, WarningType::HIGH);

	return static_cast<uint32_t>(t_ErrorMsg);
}

Buffer BB::OS::ReadFile(Allocator a_SysAllocator, const char* a_Path)
{
	std::ifstream t_File(a_Path, std::ios::ate | std::ios::binary);

	BB_WARNING(t_File.is_open(), "Failed to readfile!", WarningType::HIGH);
	Buffer t_FileBuffer;
	t_FileBuffer.size = static_cast<uint64_t>(t_File.tellg());
	t_FileBuffer.data = BBalloc(a_SysAllocator, t_FileBuffer.size);

	t_File.seekg(0);
	t_File.read(reinterpret_cast<char*>(t_FileBuffer.data), t_FileBuffer.size);
	t_File.close();

	return t_FileBuffer;
}

WindowHandle BB::OS::CreateOSWindow(OS_WINDOW_STYLE a_Style, int a_X, int a_Y, int a_Width, int a_Height, const char* a_WindowName)
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
		"Memory Studies",
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

	s_OSDevice.OSWindows.emplace(t_ReturnWindow.hwnd, t_ReturnWindow);

	return WindowHandle(t_ReturnWindow.hwnd);
}

void* BB::OS::GetOSWindowHandle(WindowHandle a_Handle)
{
	return reinterpret_cast<HWND>(a_Handle.handle);
}

void BB::OS::GetWindowSize(WindowHandle a_Handle, int& a_X, int& a_Y)
{
	RECT t_Rect;
	GetClientRect(reinterpret_cast<HWND>(a_Handle.handle), &t_Rect);

	a_X = t_Rect.right;
	a_Y = t_Rect.bottom;
}

void BB::OS::MarkDestroyOSWindow(WindowHandle a_Handle)
{
	OSOperation t_Operation;
	t_Operation.operation = OS_OPERATION_TYPE::CLOSE_WINDOW;
	t_Operation.window = reinterpret_cast<HWND>(a_Handle.handle);
	s_OSDevice.OSOperations.emplace_back(t_Operation);
}

void BB::OS::AddOSOperation(OSOperation t_Operation)
{
	s_OSDevice.OSOperations.emplace_back(t_Operation);
}

bool BB::OS::PeekOSOperations(OSOperation& t_Operation)
{
	if (s_OSDevice.OSOperations.size() == 0)
		return false;
	
	t_Operation = s_OSDevice.OSOperations[s_OSDevice.OSOperations.size() - 1];
	s_OSDevice.OSOperations.pop();
	return true;
}

void BB::OS::ProcessOSOperation(const OSOperation& t_Operation)
{
	switch (t_Operation.operation)
	{
	case BB::OS::OS_OPERATION_TYPE::CLOSE_WINDOW:
	{
		OSWindow* t_OSWindow = s_OSDevice.OSWindows.find(static_cast<HWND>(t_Operation.window));
		if (!DestroyWindow(t_OSWindow->hwnd))
			OS::LatestOSError();
		
		if (!UnregisterClassA(t_OSWindow->windowName, t_OSWindow->hInstance))
			OS::LatestOSError();
		s_OSDevice.OSWindows.erase(static_cast<HWND>(t_Operation.window));
	}
		break;
	case BB::OS::OS_OPERATION_TYPE::RESIZE_WINDOW:
		break;
	default:
		BB_ASSERT(false, "Sending an invalid OS operation.");
		break;
	}
}


void BB::OS::ClearOSOperations()
{
	s_OSDevice.OSOperations.clear();
}

void BB::OS::ExitApp()
{
	exit(EXIT_FAILURE);
}

bool BB::OS::ProcessMessages()
{
	MSG t_Msg{};

	while (PeekMessage(&t_Msg, NULL, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&t_Msg);
		DispatchMessage(&t_Msg);
	}

	return true;
}

char* BB::OS::GetExePath(Allocator a_SysAllocator)
{
	//Can force to use the return value to get the size but I decide for 256 for ease of use.
	char* a_Buffer = reinterpret_cast<char*>(BBalloc(a_SysAllocator, 256));
	GetModuleFileNameA(nullptr, a_Buffer, 256);
	return a_Buffer;
}