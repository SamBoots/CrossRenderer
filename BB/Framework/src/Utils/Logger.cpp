#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBString.h"
#include <stdarg.h>

using namespace BB;

static void Log_to_Console(const char* a_FileName, int a_Line, const char* a_WarningLevel, const char* a_Formats, va_list a_Args)
{
	constexpr const char LOG_MESSAGE_ERROR_LEVEL_0[]{ "Error Level: " };
	constexpr const char LOG_MESSAGE_FILE_0[]{ "\nFile: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\nThe Message: " };

	//Format the message
	StackString<1024> t_String{};
	{	//Start with the warning level
		t_String.append(LOG_MESSAGE_ERROR_LEVEL_0, sizeof(LOG_MESSAGE_ERROR_LEVEL_0) - 1);
		t_String.append(a_WarningLevel);
	}
	{ //Get the file.
		t_String.append(LOG_MESSAGE_FILE_0, sizeof(LOG_MESSAGE_FILE_0) - 1);
		t_String.append(a_FileName);
	}
	{ //Get the line number into the buffer
		char lineNumString[8]{};
		sprintf_s(lineNumString, 8, "%u", a_Line);

		t_String.append(LOG_MESSAGE_LINE_NUMBER_1, sizeof(LOG_MESSAGE_LINE_NUMBER_1) - 1);
		t_String.append(lineNumString);
	}
	{ //Get the message(s).
		t_String.append(LOG_MESSAGE_MESSAGE_TXT_2, sizeof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
		for (size_t i = 0; i < a_Formats[i] != '\0'; i++)
		{
			switch (a_Formats[i])
			{
			case 's':
				t_String.append(va_arg(a_Args, char*));
				break;
			case 'S': //convert it to a char first.
			{
				const wchar_t* t_wChar = va_arg(a_Args, const wchar_t*);
				const size_t t_CharSize = wcslen(t_wChar);
				//check to see if we do not go over bounds, largly to deal with non-null-terminated wchar strings.
				BB_ASSERT(t_CharSize < t_String.capacity() - t_String.size(), "error log string size exceeds 1024 characters!");
				char* t_Char = BBstackAlloc(t_CharSize, char);
				wcstombs(t_Char, t_wChar, t_CharSize);
				t_String.append(t_Char);
			}
				break;
			default:
				BB_ASSERT(false, "va arg format not yet supported");
				break;
			}

			t_String.append(" ", 1);
		}
		//double line ending for better reading.
		t_String.append("\n\n", 2);
	}

	Buffer t_LogBuffer{};
	t_LogBuffer.data = t_String.data();
	t_LogBuffer.size = t_String.size();

	WriteToFile(g_LogFile, t_LogBuffer);
	WriteToConsole(t_String.c_str(), static_cast<uint32_t>(t_String.size()));
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Info", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Optimalization Warning", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (LOW)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (MEDIUM)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (HIGH)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Exception", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Critical", a_Formats, t_vl);
	va_end(t_vl);
}
