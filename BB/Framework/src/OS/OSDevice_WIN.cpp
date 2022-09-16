#include "Utils/Logger.h"
#include "OSDevice.h"

#include <Windows.h>
#include <fstream>
#include "Storage/Slotmap.h"
#include "Storage/Array.h"

using namespace BB;

typedef FreeListAllocator_t OSAllocator_t;
typedef LinearAllocator_t OSTempAllocator_t;

OSAllocator_t OSAllocator{ mbSize * 8 };
OSTempAllocator_t OSTempAllocator{ mbSize * 4 };

static OSDevice osDevice;

//Custom callback for the Windows proc.
LRESULT CALLBACK WindowProc(HWND a_Hwnd, UINT a_Msg, WPARAM a_WParam, LPARAM a_LParam)
{
	OSOperation windowQuit;
	switch (a_Msg)
	{
	case WM_QUIT:
		windowQuit.operation = OS_OPERATION_TYPE::CLOSE_WINDOW;
		windowQuit.next = a_Hwnd;
		//CloseWindow(a_Hwnd);
		break;
	case WM_DESTROY:
		windowQuit.operation = OS_OPERATION_TYPE::CLOSE_WINDOW;
		windowQuit.next = a_Hwnd;

		//PostQuitMessage(0);
		break;
	}

	return DefWindowProc(a_Hwnd, a_Msg, a_WParam, a_LParam);
}

//The OS window for Windows.
class OSWindow
{
public:
	OSWindow(OS_WINDOW_STYLE a_Style, int a_X, int a_Y, int a_Width, int a_Height, const char* a_WindowName)
	{
		m_WindowName = a_WindowName;

		WNDCLASS t_WndClass = {};
		t_WndClass.lpszClassName = m_WindowName;
		t_WndClass.hInstance = m_HInstance;
		t_WndClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);
		t_WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		t_WndClass.lpfnWndProc = WindowProc;

		RegisterClass(&t_WndClass);
		//DWORD t_Style = WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;

		DWORD t_Style;
		switch (a_Style)
		{
		case BB::OS_WINDOW_STYLE::MAIN:
			t_Style = WS_OVERLAPPEDWINDOW;
			break;
		case BB::OS_WINDOW_STYLE::CHILD:
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

		hwnd = CreateWindowEx(
			0,
			m_WindowName,
			"Memory Studies",
			t_Style,
			t_Rect.left,
			t_Rect.top,
			t_Rect.right - t_Rect.left,
			t_Rect.bottom - t_Rect.top,
			NULL,
			NULL,
			m_HInstance,
			NULL
		);

		ShowWindow(hwnd, SW_SHOW);
	}

	~OSWindow()
	{
		//Delete the window before you unregister the class.
		if (!DestroyWindow(hwnd))
			osDevice.LatestOSError();

		if (!UnregisterClassA(m_WindowName, m_HInstance))
			osDevice.LatestOSError();
	}

	HWND hwnd = nullptr;

private:
	const char* m_WindowName = nullptr;
	HINSTANCE m_HInstance = nullptr;
};

struct BB::OSDevice_o
{
	//Special array for all the windows. Stored seperately 
	Slotmap<OSWindow> OSWindows{ OSAllocator, 8 };
	//Array operations will very likely never exceed 8.
	Array<OSOperation> OSOperations{ OSAllocator , 8 };
};

OSDevice& BB::AppOSDevice()
{
	return osDevice;
}

OSDevice::OSDevice()
{
	m_OSDevice = BBnew<OSDevice_o>(OSAllocator);
}

OSDevice::~OSDevice()
{
	BBfree(OSAllocator, m_OSDevice);
}

const size_t BB::OSDevice::VirtualMemoryPageSize() const
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwPageSize;
}

const size_t BB::OSDevice::VirtualMemoryMinimumAllocation() const
{
	SYSTEM_INFO t_Info;
	GetSystemInfo(&t_Info);
	return t_Info.dwAllocationGranularity;
}

const uint32_t OSDevice::LatestOSError() const
{
	return static_cast<uint32_t>(GetLastError());
}

Buffer OSDevice::ReadFile(Allocator a_SysAllocator, const char* a_Path)
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

WindowHandle OSDevice::CreateOSWindow(OS_WINDOW_STYLE a_Style, int a_X, int a_Y, int a_Width, int a_Height, const char* a_WindowName)
{
	return WindowHandle(static_cast<uint32_t>(m_OSDevice->OSWindows.emplace(a_Style, a_X, a_Y, a_Width, a_Height, a_WindowName)));
}

void* OSDevice::GetOSWindowHandle(WindowHandle a_Handle)
{
	return m_OSDevice->OSWindows.find(a_Handle.index).hwnd;
}

void OSDevice::GetWindowSize(WindowHandle a_Handle, int& a_X, int& a_Y)
{
	RECT t_Rect;
	GetClientRect(m_OSDevice->OSWindows.find(a_Handle.index).hwnd, &t_Rect);

	a_X = t_Rect.right;
	a_Y = t_Rect.bottom;
}

void OSDevice::DestroyOSWindow(WindowHandle a_Handle)
{
	m_OSDevice->OSWindows.erase(a_Handle.index);
}

void OSDevice::AddOSOperation(OSOperation t_Operation)
{
	m_OSDevice->OSOperations.emplace_back(t_Operation);
}

const BB::Slice<OSOperation> OSDevice::GetOSOperations() const
{
	return m_OSDevice->OSOperations;
}

void OSDevice::ClearOSOperations()
{
	m_OSDevice->OSOperations.clear();
}

void OSDevice::ExitApp() const
{
	exit(EXIT_FAILURE);
}

bool BB::OSDevice::ProcessMessages() const
{
	MSG t_Msg{};

	while (PeekMessage(&t_Msg, NULL, 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&t_Msg);
		DispatchMessage(&t_Msg);
	}

	m_OSDevice->OSOperations.clear();
	return true;
}

char* BB::OSDevice::GetExePath(Allocator a_SysAllocator) const
{
	//Can force to use the return value to get the size but I decide for 256 for ease of use.
	char* a_Buffer = reinterpret_cast<char*>(BBalloc(a_SysAllocator, 256));
	GetModuleFileNameA(nullptr, a_Buffer, 256);
	return a_Buffer;
}