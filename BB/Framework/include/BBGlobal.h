#pragma once
#include "Common.h"

namespace BB
{
	extern const wchar* g_ProgramName;
	extern const char* g_ExePath;

	extern OSFileHandle g_LogFile;
#ifdef _DEBUG
	extern OSFileHandle g_AllocationLogFile;
#endif //_DEBUG
}