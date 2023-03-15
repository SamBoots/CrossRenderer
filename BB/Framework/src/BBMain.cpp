#include "BBGlobal.h"
#include "BBmain.h"
#include "Program.h"

using namespace BB;

const wchar* BB::g_ProgramName;
const char* BB::g_ExePath;

OSFileHandle BB::g_LogFile;
#ifdef _DEBUG
OSFileHandle BB::g_AllocationLogFile;
#endif //_DEBUG

void BB::InitBB(const BBInitInfo& a_BBInfo)
{
	g_ProgramName = a_BBInfo.programName;
	g_ExePath = a_BBInfo.exePath;


	g_LogFile = CreateOSFile(L"logger.txt");
#ifdef _DEBUG
	g_AllocationLogFile = CreateOSFile(L"allocationLogger.txt");
#endif //_DEBUG

	InitProgram();
}

const wchar* BB::GetProgramName()
{
	return g_ProgramName;
}

const char* BB::GetProgramPath()
{
	return g_ExePath;
}