#include "Logger.h"
#include <iostream>
#include <cassert>

using namespace BB;

static void Log_to_Console(const char* a_FileName, int a_Line, const char* a_Message, const char* a_WarningLevel)
{
	std::cout << a_WarningLevel << 
		"\033 File: "<< a_FileName << 
		" Line Number: " << a_Line << 
		" The Message:" << "\n" << a_Message << "\n \n";
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Info: ");
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Optimalization Warning: ");
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (LOW): ");
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (MEDIUM): ");
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Warning (HIGH): ");
}

void Logger::Log_Exception(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Exception: ");
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const char* a_Message)
{
	Log_to_Console(a_FileName, a_Line, a_Message, "Critical Error: ");
	assert(false);
}
