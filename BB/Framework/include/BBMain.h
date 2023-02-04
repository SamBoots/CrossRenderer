#pragma once
#include "Common.h"

namespace BB
{
	struct BBInitInfo
	{
		const wchar* programName;
		const char* exePath;
	};

	void InitBB(const BBInitInfo& a_BBInfo);

	const wchar* GetProgramName();
	const char* GetProgramPath();
}