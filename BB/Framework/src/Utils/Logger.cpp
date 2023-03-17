#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

using namespace BB;

static void Log_to_Console(const char* a_FileName, int a_Line, const char* a_Message, const char* a_WarningLevel)
{
	constexpr const char LOG_MESSAGE_FILE_0[]{ "File: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_1[]{ "\r\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_2[]{ "\r\nThe Message: " };

	//Format the message
	char t_String[1024]{};
	size_t t_StringSize = 0;
	{ 	//Start with the warning level
		size_t t_WarnMesgSize = strnlen_s(a_WarningLevel, 64);
		Memory::Copy(t_String, a_WarningLevel, t_WarnMesgSize);
		t_StringSize += t_WarnMesgSize;
	}

	{ //Get the file.
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_FILE_0, _countof(LOG_MESSAGE_FILE_0));
		t_StringSize += _countof(LOG_MESSAGE_FILE_0);
		
		size_t t_FilePathSize = strnlen_s(a_FileName, 256);
		Memory::Copy(t_String + t_StringSize, a_FileName, t_FilePathSize);
		t_StringSize += t_FilePathSize;
	}
	{ //Get the line number into the buffer
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_LINE_NUMBER_1, _countof(LOG_MESSAGE_LINE_NUMBER_1));
		t_StringSize += _countof(LOG_MESSAGE_LINE_NUMBER_1);

		char lineNumString[12]{};
		sprintf_s(lineNumString, 12, "%d", a_Line);
		size_t t_LineNumSize = strnlen_s(lineNumString, 256);
		Memory::Copy(t_String + t_StringSize, lineNumString, t_LineNumSize);
		t_StringSize += t_LineNumSize;
	}
	{ //Get the file.
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_MESSAGE_TXT_2, _countof(LOG_MESSAGE_MESSAGE_TXT_2));
		t_StringSize += _countof(LOG_MESSAGE_MESSAGE_TXT_2);

		size_t t_MessageSize = strnlen_s(a_Message, 256);
		Memory::Copy(t_String + t_StringSize, a_Message, t_MessageSize);
		t_StringSize += t_MessageSize;
	}
	//Double skip for the end
	t_String[t_StringSize++] = '\r';
	t_String[t_StringSize++] = '\n';
	t_String[t_StringSize++] = '\r';
	t_String[t_StringSize++] = '\n';

	Buffer t_LogBuffer{};
	t_LogBuffer.data = t_String;
	t_LogBuffer.size = t_StringSize;

	WriteToFile(g_LogFile, t_LogBuffer);
	WriteToConsole(t_String, static_cast<uint32_t>(t_StringSize));
}

static inline size_t strlen_s_towidechar(const char* a_str, const size_t a_MaxCount)
{
	return strnlen_s(a_str, 64);
}

static void Log_to_Console(const char* a_FileName, int a_Line, const wchar_t* a_Message, const char* a_WarningLevel)
{
	constexpr const wchar_t LOG_MESSAGE_FILE_0[]{ L"File: " };
	constexpr const wchar_t LOG_MESSAGE_LINE_NUMBER_1[]{ L"\r\nLine Number: " };
	constexpr const wchar_t LOG_MESSAGE_MESSAGE_TXT_2[]{ L"\r\nThe Message: " };


	//Format the message
	wchar_t t_String[2048]{};
	size_t t_StringSize = 0;
	{ 	//Start with the warning level
		size_t t_WarnMesgSize = strlen_s_towidechar(a_WarningLevel, 64);
		mbstowcs(t_String, a_WarningLevel, t_WarnMesgSize);
		t_StringSize += t_WarnMesgSize;
	}

	{ //Get the file.
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_FILE_0, _countof(LOG_MESSAGE_FILE_0));
		t_StringSize += _countof(LOG_MESSAGE_FILE_0);

		size_t t_FilePathSize = strlen_s_towidechar(a_FileName, 256);
		mbstowcs(t_String + t_StringSize, a_FileName, t_FilePathSize);
		t_StringSize += t_FilePathSize;
	}
	{ //Get the line number into the buffer
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_LINE_NUMBER_1, _countof(LOG_MESSAGE_LINE_NUMBER_1));
		t_StringSize += _countof(LOG_MESSAGE_LINE_NUMBER_1);

		wchar_t lineNumString[12]{};
		swprintf_s(lineNumString, 12, L"%d", a_Line);
		size_t t_LineNumSize = wcsnlen_s(lineNumString, 256);
		Memory::Copy(t_String + t_StringSize, lineNumString, t_LineNumSize);
		t_StringSize += t_LineNumSize;
	}
	{ //Get the file.
		Memory::Copy(t_String + t_StringSize, LOG_MESSAGE_MESSAGE_TXT_2, _countof(LOG_MESSAGE_MESSAGE_TXT_2));
		t_StringSize += _countof(LOG_MESSAGE_MESSAGE_TXT_2);

		size_t t_MessageSize = wcsnlen_s(a_Message, 256);
		Memory::Copy(t_String + t_StringSize, a_Message, t_MessageSize);
		t_StringSize += t_MessageSize;
	}
	//Double skip for the end
	t_String[t_StringSize++] = L'\r';
	t_String[t_StringSize++] = L'\n';
	t_String[t_StringSize++] = L'\r';
	t_String[t_StringSize++] = L'\n';

	Buffer t_LogBuffer{};
	t_LogBuffer.data = t_String;
	t_LogBuffer.size = t_StringSize;

	WriteToFile(g_LogFile, t_LogBuffer);
	WriteToConsole(t_String, static_cast<uint32_t>(t_StringSize));
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Info: ");
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Info: ");
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Optimalization Warning: ");
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Optimalization Warning: ");
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (LOW): ");
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (LOW): ");
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (MEDIUM): ");
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (MEDIUM): ");
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (HIGH): ");
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (HIGH): ");
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Exception: ");
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Exception: ");
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Critical Error: ");
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const wchar_t* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Critical Error: ");
}
