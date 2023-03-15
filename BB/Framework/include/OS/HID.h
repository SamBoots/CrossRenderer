#pragma once
#include "Common.h"

namespace BB
{
	constexpr size_t INPUT_EVENT_BUFFER_MAX = 64;

	enum class KEY
	{
		W,
		A,
		S,
		D
	};

	struct MouseInfo
	{
		float xMove;
		float yMove;
		//Might add more here.
		int16_t wheelMove;
		bool left_pressed;
		bool left_released;
		bool right_pressed;
		bool right_released;
		bool middle_pressed;
		bool middle_released;
	};

	struct KeyInfo
	{
		uint16_t scancode;

		bool keyPressed;
	};

	enum class INPUT_TYPE : int32_t
	{
		MOUSE,
		KEYBOARD
	};

	struct InputEvent
	{
		INPUT_TYPE inpytType;
		union
		{
			KeyInfo keyInfo;
			MouseInfo mouseInfo;
		};
	};

	void PollInputEvents(InputEvent* a_EventBuffers, size_t& a_InputEventAmount);
}
