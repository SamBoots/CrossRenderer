#include "BBGlobal.h"
#include "Program.h"
#include "HID.inl"
#include "Math.inl"
#include "Utils/Logger.h"
#include "RingAllocator.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <fileapi.h>
#include <memoryapi.h>
#include <libloaderapi.h>
#include <WinUser.h>
#include <hidusage.h>

#include <mutex>

using namespace BB;

void DefaultClose(WindowHandle a_WindowHandle) {}
void DefaultResize(WindowHandle a_WindowHandle, uint32_t a_X, uint32_t a_Y) {}

static PFN_WindowCloseEvent sPFN_CloseEvent = DefaultClose;
static PFN_WindowResizeEvent sPFN_ResizeEvent = DefaultResize;

struct InputBuffer
{
	InputEvent inputBuff[INPUT_EVENT_BUFFER_MAX];
	uint32_t start = 0;
	uint16_t pos = 0;
	uint16_t used = 0;
};

struct GlobalProgramInfo
{
	bool trackingMouse = true;
};

static GlobalProgramInfo s_ProgramInfo{};
static InputBuffer s_InputBuffer{};
static std::mutex s_InputMutex{};

static size_t s_OSRingAllocatorSize = 0; //This will be changed to the OS granulary minimum..
static LocalRingAllocator s_OSRingAllocator{ s_OSRingAllocatorSize };

void PushInput(const InputEvent& a_Input)
{
	s_InputMutex.lock();
	if (s_InputBuffer.pos + 1 > INPUT_EVENT_BUFFER_MAX)
		s_InputBuffer.pos = 0;

	s_InputBuffer.inputBuff[s_InputBuffer.pos++] = a_Input;

	//Since when we get the input we get all of it. 
	if (s_InputBuffer.used < INPUT_EVENT_BUFFER_MAX)
	{
		++s_InputBuffer.used;
	}
	s_InputMutex.unlock();
}

//Returns false if no input is left.
void GetAllInput(InputEvent* a_InputBuffer)
{
	s_InputMutex.lock();
	int t_FirstIndex = s_InputBuffer.start;
	for (size_t i = 0; i < s_InputBuffer.used; i++)
	{
		a_InputBuffer[i] = s_InputBuffer.inputBuff[t_FirstIndex];
		//We go back to zero the read the data.
		if (++t_FirstIndex > INPUT_EVENT_BUFFER_MAX)
			t_FirstIndex = 0;
	}

	s_InputBuffer.start = s_InputBuffer.pos;
	s_InputBuffer.used = 0;
	s_InputMutex.unlock();
}

void BB::InitProgram()
{
	SetupHIDTranslates();
}

//Custom callback for the Windows proc.
LRESULT wm_input(HWND a_Hwnd, WPARAM a_WParam, LPARAM a_LParam)
{
	HRAWINPUT t_HRawInput = reinterpret_cast<HRAWINPUT>(a_LParam);

	//Allocate an input event.
	InputEvent t_Event{};

	UINT t_Size = sizeof(RAWINPUT);
	RAWINPUT t_Input{};
	GetRawInputData(t_HRawInput, RID_INPUT, &t_Input, &t_Size, sizeof(RAWINPUTHEADER));

	if (t_Input.header.dwType == RIM_TYPEKEYBOARD)
	{
		t_Event.inputType = INPUT_TYPE::KEYBOARD;
		uint16_t scanCode = t_Input.data.keyboard.MakeCode;

		// Scan codes could contain 0xe0 or 0xe1 one-byte prefix.
		//scanCode |= (t_Input->data.keyboard.Flags & RI_KEY_E0) ? 0xe000 : 0;
		//scanCode |= (t_Raw->data.keyboard.Flags & RI_KEY_E1) ? 0xe100 : 0;

		t_Event.keyInfo.scancode = s_translate_key[scanCode];
		t_Event.keyInfo.keyPressed = !(t_Input.data.keyboard.Flags & RI_KEY_BREAK);
		PushInput(t_Event);
	}
	else if (t_Input.header.dwType == RIM_TYPEMOUSE && s_ProgramInfo.trackingMouse)
	{
		t_Event.inputType = INPUT_TYPE::MOUSE;
		const float2 t_MoveInput{ 
			static_cast<float>(t_Input.data.mouse.lLastX), 
			static_cast<float>(t_Input.data.mouse.lLastY) };
		if (t_Input.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
		{
			BB_ASSERT(false, "Windows Input, not using MOUSE_MOVE_ABSOLUTE currently.");
			//t_Event.mouseInfo.moveOffset = t_MoveInput - s_InputInfo.mouse.oldPos;
			//s_InputInfo.mouse.oldPos = t_MoveInput;
		}
		else
		{
			t_Event.mouseInfo.moveOffset = t_MoveInput;
			POINT t_Point;
			GetCursorPos(&t_Point);
			ScreenToClient(a_Hwnd, &t_Point);
			t_Event.mouseInfo.mousePos = { (float)t_Point.x, (float)t_Point.y };
		}

		t_Event.mouseInfo.left_pressed = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN;
		t_Event.mouseInfo.left_released = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP;
		t_Event.mouseInfo.right_pressed = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN;
		t_Event.mouseInfo.right_released = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP;
		t_Event.mouseInfo.middle_pressed = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN;
		t_Event.mouseInfo.middle_released = t_Input.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP;
		if (t_Input.data.mouse.usButtonFlags & (RI_MOUSE_WHEEL | WM_MOUSEHWHEEL))
		{
			const int16_t t_MouseMove = *reinterpret_cast<const int16_t*>(&t_Input.data.mouse.usButtonData);
			t_Event.mouseInfo.wheelMove = t_MouseMove / WHEEL_DELTA;
		}
		PushInput(t_Event);
	}


	return DefWindowProcW(a_Hwnd, WM_INPUT, a_WParam, a_LParam);
}

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
		break;
	}
	case WM_MOUSELEAVE:
		s_ProgramInfo.trackingMouse = false;
		break;
	case WM_MOUSEMOVE:
		s_ProgramInfo.trackingMouse = true;
		break;
	case WM_INPUT:
		return wm_input(a_Hwnd, a_WParam, a_LParam);
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

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, t_ErrorMsg, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), t_Message, 0, NULL);

	BB_WARNING(false, t_Message, WarningType::HIGH);

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

void BB::WriteToConsole(const char* a_String, uint32_t a_StrLength)
{
	DWORD t_Written = 0;
	//Maybe check if a console is available, it could be null.
	if (FALSE == WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),
		a_String,
		a_StrLength,
		&t_Written,
		NULL))
	{
		BB_WARNING(false,
			"OS, failed to write to console! This can be severe.",
			WarningType::HIGH);
		LatestOSError();
	}
}

void BB::WriteToConsole(const wchar_t* a_String, uint32_t a_StrLength)
{
	DWORD t_Written = 0;
	//Maybe check if a console is available, it could be null.
	if (FALSE == WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE),
		a_String,
		a_StrLength,
		&t_Written,
		NULL))
	{
		BB_WARNING(false,
			"OS, failed to write to console! This can be severe.",
			WarningType::HIGH);
		LatestOSError();
	}
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
		BB_WARNING(false,
			"OS, failed to load file! This can be severe.",
			WarningType::HIGH);
		LatestOSError();
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
#ifdef _DEBUG
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
	HWND t_Window;
	HINSTANCE t_HInstance{};

	WNDCLASSW t_WndClass = {};
	t_WndClass.lpszClassName = a_WindowName;
	t_WndClass.hInstance = t_HInstance;
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

	t_Window = CreateWindowEx(
		0,
		a_WindowName,
		g_ProgramName,
		t_Style,
		t_Rect.left,
		t_Rect.top,
		t_Rect.right - t_Rect.left,
		t_Rect.bottom - t_Rect.top,
		NULL,
		NULL,
		t_HInstance,
		NULL);
	ShowWindow(t_Window, SW_SHOW);

	//Get the mouse and keyboard.
	RAWINPUTDEVICE t_Rid[2]{};

	t_Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	t_Rid[0].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	t_Rid[0].dwFlags = RIDEV_NOLEGACY;
	t_Rid[0].hwndTarget = t_Window;

	t_Rid[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	t_Rid[1].usUsage = HID_USAGE_GENERIC_MOUSE;
	t_Rid[1].dwFlags = 0;
	t_Rid[1].hwndTarget = t_Window;

	BB_ASSERT(RegisterRawInputDevices(t_Rid, 2, sizeof(RAWINPUTDEVICE)),
		"Failed to register raw input devices!");

	uint32_t t_NumConnectedDevices = 0;
	UINT t_ErrCheck = GetRawInputDeviceList(nullptr, &t_NumConnectedDevices, sizeof(RAWINPUTDEVICELIST));
	BB_ASSERT(t_ErrCheck != -1, "Failed to get the size of raw input devices!");
	BB_ASSERT(t_NumConnectedDevices > 0, "Failed to get the size of raw input devices!");

	RAWINPUTDEVICELIST* t_ConnectedDevices = BBnewArr(
		s_OSRingAllocator,
		t_NumConnectedDevices,
		RAWINPUTDEVICELIST);

	t_ErrCheck = GetRawInputDeviceList(t_ConnectedDevices, &t_NumConnectedDevices, sizeof(RAWINPUTDEVICELIST));
	BB_ASSERT(t_ErrCheck != -1, "Failed to get the raw input devices!");
	
	constexpr size_t MAX_HID_STRING_LENGTH = 126;
	wchar_t* t_ProductNameString = BBnewArr(
		s_OSRingAllocator,
		MAX_HID_STRING_LENGTH,
		wchar_t);

	//Lets log the devices, maybe do this better.
	for (size_t i = 0; i < t_NumConnectedDevices; i++)
	{
		RID_DEVICE_INFO t_DeviceInfo{};
		UINT t_RidDeviceSize = sizeof(RID_DEVICE_INFO);
		UINT t_It = GetRawInputDeviceInfo(t_ConnectedDevices[i].hDevice, RIDI_DEVICEINFO, &t_DeviceInfo, &t_RidDeviceSize);

		if (t_DeviceInfo.dwType == RIM_TYPEMOUSE)
		{
			
		}
		else if (t_DeviceInfo.dwType == RIM_TYPEMOUSE)
		{

		}
	}

	return WindowHandle(t_Window);
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

void BB::FreezeMouseOnWindow(const WindowHandle a_Handle)
{
	RECT t_Rect;
	GetClientRect(reinterpret_cast<HWND>(a_Handle.ptrHandle), &t_Rect);

	POINT t_LeftRightUpDown[2]{};
	t_LeftRightUpDown[0].x = t_Rect.left;
	t_LeftRightUpDown[0].y = t_Rect.top;
	t_LeftRightUpDown[1].x = t_Rect.right;
	t_LeftRightUpDown[1].y = t_Rect.bottom;

	MapWindowPoints(reinterpret_cast<HWND>(a_Handle.ptrHandle), nullptr, t_LeftRightUpDown, _countof(t_LeftRightUpDown));

	t_Rect.left = t_LeftRightUpDown[0].x;
	t_Rect.top = t_LeftRightUpDown[0].y;

	t_Rect.right = t_LeftRightUpDown[1].x;
	t_Rect.bottom = t_LeftRightUpDown[1].y;

	ClipCursor(&t_Rect);
}

void BB::UnfreezeMouseOnWindow()
{
	ClipCursor(nullptr);
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

bool BB::ProcessMessages(const WindowHandle a_WindowHandle)
{
	//TRACKMOUSEEVENT t_MouseTrackE{};
	//t_MouseTrackE.cbSize = sizeof(TRACKMOUSEEVENT);
	//t_MouseTrackE.dwFlags = TME_LEAVE;
	//t_MouseTrackE.hwndTrack = reinterpret_cast<HWND>(a_WindowHandle.ptrHandle);
	//TrackMouseEvent(&t_MouseTrackE);


	MSG t_Msg{};

	while (PeekMessage(&t_Msg, reinterpret_cast<HWND>(a_WindowHandle.ptrHandle), 0u, 0u, PM_REMOVE))
	{
		TranslateMessage(&t_Msg);
		DispatchMessage(&t_Msg);
	}

	return true;
}

void BB::PollInputEvents(InputEvent* a_EventBuffers, size_t& a_InputEventAmount)
{
	a_InputEventAmount = s_InputBuffer.used;
	if (a_EventBuffers == nullptr)
		return;
	
	//Overwrite could happen! But this is user's responsibility.
	GetAllInput(a_EventBuffers);
}