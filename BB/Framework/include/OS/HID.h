#pragma once
#include "Common.h"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 64;

	//These will be translated, the are already close to their real counterpart.
	//Translation table: https://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
	enum class KEYBOARD_KEY : uint32_t
	{
		NO_KEY = 0x00,
		_ESCAPE = 0x01,
		_1 = 0x02,
		_2 = 0x03,
		_3 = 0x04,
		_4 = 0x05,
		_5 = 0x06,
		_6 = 0x07,
		_7 = 0x08,
		_8 = 0x09,
		_9 = 0x0A,
		_0 = 0x0B,
		_MINUS = 0x0C,
		_EQUALS = 0x0D,
		_BACKSPACE = 0x0E,
		_TAB = 0x0F,
		_Q = 0x10,
		_W = 0x11,
		_E = 0x12,
		_R = 0x13,
		_T = 0x14,
		_Y = 0x15,
		_U = 0x16,
		_I = 0x17,
		_O = 0x18,
		_P = 0x19,
		_BRACKETLEFT = 0x1A,
		_BRACKETRIGHT = 0x1B,
		_RETURN = 0x1C, //I think this is non-numpad enter?
		_CONTROLLEFT = 0x1D,
		_A = 0x1E,
		_S = 0x1F,
		_D = 0x20,
		_F = 0x21,
		_G = 0x22,
		_H = 0x23,
		_J = 0x24,
		_K = 0x25,
		_L = 0x26,
		_SEMICOLON = 0x27,
		_APOSTROPHE = 0x28,
		_GRAVE = 0x29,
		_SHIFTLEFT = 0x2A,
		_BACKSLASH = 0x2B,
		_Z = 0x2C,
		_X = 0x2D,
		_C = 0x2E,
		_V = 0x2F,
		_B = 0x30,
		_N = 0x31,
		_M = 0x32,
		_COMMA = 0x33,
		_PERIOD = 0x34,
		_SLASH = 0x35,
		_SHIFTRIGHT = 0x36,
		_NUMPAD_MULTIPLY = 0x37,
		_ALTLEFT = 0x38,
		_SPACEBAR = 0x39,
		_CAPSLOCK = 0x3A,
	};

	struct MouseInfo
	{
		float2 moveOffset;
		float2 mousePos;
		//Might add more here.
		int16_t wheelMove;
		bool left_pressed;
		bool left_released;
		bool right_pressed;
		bool right_released;
		bool middle_pressed;
		bool middle_released;
	};

	//7 byte struct (assuming bool is 1 byte.)
	struct KeyInfo
	{
		KEYBOARD_KEY scancode;
		wchar utf16; //NOT IN USE;
		bool keyPressed;
	};

	enum class INPUT_TYPE : int32_t
	{
		MOUSE,
		KEYBOARD
	};

	struct InputEvent
	{
		InputEvent() {}; //WHY C++, had to do this as it appearantly deleted the standard constructor.
		INPUT_TYPE inputType{};
		union
		{
			MouseInfo mouseInfo{};
			KeyInfo keyInfo;
		};
	};

	void PollInputEvents(InputEvent* a_EventBuffers, size_t& a_InputEventAmount);
}
