#include "HID.h"
namespace BB
{
	//array size is last one of the KEYBOARD_KEY list.
	static KEYBOARD_KEY s_translate_key[0x3A + 1];

	static void SetupHIDTranslates()
	{
		s_translate_key[0x1E] = KEYBOARD_KEY::_A;
		s_translate_key[0x30] = KEYBOARD_KEY::_B;
		s_translate_key[0x2E] = KEYBOARD_KEY::_C;
		s_translate_key[0x20] = KEYBOARD_KEY::_D;
		s_translate_key[0x12] = KEYBOARD_KEY::_E;
		s_translate_key[0x21] = KEYBOARD_KEY::_F;
		s_translate_key[0x22] = KEYBOARD_KEY::_G;
		s_translate_key[0x23] = KEYBOARD_KEY::_H;
		s_translate_key[0x17] = KEYBOARD_KEY::_I;
		s_translate_key[0x24] = KEYBOARD_KEY::_J;
		s_translate_key[0x25] = KEYBOARD_KEY::_K;
		s_translate_key[0x26] = KEYBOARD_KEY::_L;
		s_translate_key[0x32] = KEYBOARD_KEY::_M;
		s_translate_key[0x31] = KEYBOARD_KEY::_N;
		s_translate_key[0x18] = KEYBOARD_KEY::_O;
		s_translate_key[0x19] = KEYBOARD_KEY::_P;
		s_translate_key[0x10] = KEYBOARD_KEY::_Q;
		s_translate_key[0x13] = KEYBOARD_KEY::_R;
		s_translate_key[0x1F] = KEYBOARD_KEY::_S;
		s_translate_key[0x14] = KEYBOARD_KEY::_T;
		s_translate_key[0x16] = KEYBOARD_KEY::_U;
		s_translate_key[0x2F] = KEYBOARD_KEY::_V;
		s_translate_key[0x11] = KEYBOARD_KEY::_W;
		s_translate_key[0x2D] = KEYBOARD_KEY::_X;
		s_translate_key[0x15] = KEYBOARD_KEY::_Y;
		s_translate_key[0x2C] = KEYBOARD_KEY::_Z;
		s_translate_key[0x02] = KEYBOARD_KEY::_1;
		s_translate_key[0x03] = KEYBOARD_KEY::_2;
		s_translate_key[0x04] = KEYBOARD_KEY::_3;
		s_translate_key[0x05] = KEYBOARD_KEY::_4;
		s_translate_key[0x06] = KEYBOARD_KEY::_5;
		s_translate_key[0x07] = KEYBOARD_KEY::_6;
		s_translate_key[0x08] = KEYBOARD_KEY::_7;
		s_translate_key[0x09] = KEYBOARD_KEY::_8;
		s_translate_key[0x0A] = KEYBOARD_KEY::_9;
		s_translate_key[0x0B] = KEYBOARD_KEY::_0;
		s_translate_key[0x1C] = KEYBOARD_KEY::_RETURN;
		s_translate_key[0x01] = KEYBOARD_KEY::_ESCAPE;
		s_translate_key[0x0E] = KEYBOARD_KEY::_BACKSPACE;
		s_translate_key[0x0F] = KEYBOARD_KEY::_TAB;
		s_translate_key[0x39] = KEYBOARD_KEY::_SPACEBAR;
		s_translate_key[0x0C] = KEYBOARD_KEY::_MINUS;
		s_translate_key[0x0D] = KEYBOARD_KEY::_EQUALS;
		s_translate_key[0x1A] = KEYBOARD_KEY::_BRACKETLEFT;
		s_translate_key[0x1B] = KEYBOARD_KEY::_BRACKETRIGHT;
		s_translate_key[0x2B] = KEYBOARD_KEY::_BACKSLASH;

		s_translate_key[0x27] = KEYBOARD_KEY::_SEMICOLON;
		s_translate_key[0x28] = KEYBOARD_KEY::_APOSTROPHE;
		s_translate_key[0x29] = KEYBOARD_KEY::_GRAVE;
		s_translate_key[0x33] = KEYBOARD_KEY::_COMMA;
		s_translate_key[0x34] = KEYBOARD_KEY::_PERIOD;
		s_translate_key[0x35] = KEYBOARD_KEY::_SLASH;
		s_translate_key[0x3A] = KEYBOARD_KEY::_CAPSLOCK;

		//After this we have the F1-12 keys.
		//s_translate_key[0x] = KEYBOARD_KEY::_;
	}
	
}