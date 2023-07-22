#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBString.h"

using namespace BB;

static void Log_to_Console(const char* a_FileName, int a_Line, const char* a_Message, const char* a_WarningLevel)
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
	{ //Get the file.
		t_String.append(LOG_MESSAGE_MESSAGE_TXT_2, sizeof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
		t_String.append(a_Message);
		//double line ending for better reading.
		t_String.append("\n\n", 2);
	}

	Buffer t_LogBuffer{};
	t_LogBuffer.data = t_String.data();
	t_LogBuffer.size = t_String.size();

	WriteToFile(g_LogFile, t_LogBuffer);
	WriteToConsole(t_String.c_str(), static_cast<uint32_t>(t_String.size()));
}

static void Log_to_Console(const char* a_FileName, int a_Line, const wchar_t* a_Message, const char* a_WarningLevel)
{
	constexpr const wchar_t LOG_MESSAGE_ERROR_LEVEL_0[]{ L"Error Level: " };
	constexpr const wchar_t LOG_MESSAGE_FILE_0[]{ L"\nFile: " };
	constexpr const wchar_t LOG_MESSAGE_LINE_NUMBER_1[]{ L"\nLine Number: " };
	constexpr const wchar_t LOG_MESSAGE_MESSAGE_TXT_2[]{ L"\nThe Message: " };

	//Format the message
	StackWString<1024> t_String{};
	{	//Start with the warning level
		t_String.append(LOG_MESSAGE_ERROR_LEVEL_0, _countof(LOG_MESSAGE_ERROR_LEVEL_0) - 1);

		wchar t_WarnMessage[64]{};
		size_t t_WarnMesgSize = strnlen_s(a_WarningLevel, 64);
		mbstowcs(t_WarnMessage, a_WarningLevel, t_WarnMesgSize);
		t_String.append(t_WarnMessage, t_WarnMesgSize);
	}
	{ //Get the file.
		t_String.append(LOG_MESSAGE_FILE_0, _countof(LOG_MESSAGE_FILE_0) - 1);

		wchar t_Message[256]{};
		size_t t_MessageSize = strnlen_s(a_FileName, 255);
		mbstowcs(t_Message, a_FileName, t_MessageSize);
		t_String.append(t_Message, t_MessageSize);
	}
	{ //Get the line number into the buffer
		wchar lineNumString[8]{};
		swprintf_s(lineNumString, 8, L"%u", a_Line);

		t_String.append(LOG_MESSAGE_LINE_NUMBER_1, _countof(LOG_MESSAGE_LINE_NUMBER_1) - 1);
		t_String.append(lineNumString);
	}
	{ //Get the file.
		t_String.append(LOG_MESSAGE_MESSAGE_TXT_2, _countof(LOG_MESSAGE_MESSAGE_TXT_2) - 1);
		t_String.append(a_Message);
		//double line ending for better reading.
		t_String.append(L"\n\n", 2);
	}

	Buffer t_LogBuffer{};
	t_LogBuffer.data = t_String.data();
	t_LogBuffer.size = t_String.size();

	WriteToFile(g_LogFile, t_LogBuffer);
	WriteToConsole(t_String.c_str(), static_cast<uint32_t>(t_String.size()));
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Info");
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Info");
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Optimalization Warning");
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Optimalization Warning");
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (LOW)");
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (LOW)");
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (MEDIUM)");
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (MEDIUM)");
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (HIGH)");
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (HIGH)");
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Exception");
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Exception");
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Critical");
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Critical");
}
