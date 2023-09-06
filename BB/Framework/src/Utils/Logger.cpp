#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBThreadScheduler.hpp"
#include "Allocators.h"
#include "BBString.h"
#include <stdarg.h>

using namespace BB;

static void WriteLoggerToFile(void*);

static LinearAllocator_t s_Allocator(mbSize * 4, "Log Allocator");

//dirty singleton
class LoggerSingleton
{
private:
	uint32_t m_MaxLoggerBufferSize;
	WarningTypeFlags m_EnabledWarningFlags = 0;
	//create a fixed string class for this.
	String m_CacheString;
	//create a fixed string class for this.
	String m_UploadString;
	ThreadTask m_LastThreadTask = 0;
	const BBMutex m_WriteToFileMutex;
	const OSFileHandle m_LogFile;

	static LoggerSingleton* m_LoggerInst;
public:

	LoggerSingleton()
		: m_MaxLoggerBufferSize(2024),
		  m_CacheString(s_Allocator, m_MaxLoggerBufferSize),
		  m_UploadString(s_Allocator, m_MaxLoggerBufferSize),
		  m_LogFile(CreateOSFile(L"logger.txt")),
		  m_WriteToFileMutex(OSCreateMutex())
	{
		//set them all to true at the start.
		m_EnabledWarningFlags = UINT32_MAX;
	};

	~LoggerSingleton()
	{
		//write the last logger information
		LoggerWriteToFile();
		//clear it to avoid issues related to reporting memory leaks.
		s_Allocator.Clear();
		DestroyMutex(m_WriteToFileMutex);
	};

	static LoggerSingleton* GetInstance()
	{
		if (!m_LoggerInst)
			m_LoggerInst = BBnew(s_Allocator, LoggerSingleton);

		return m_LoggerInst;
	}

	void WriteLogInfoToFile(const char* a_Msg, const size_t a_Size)
	{
		OSWaitAndLockMutex(m_WriteToFileMutex);
		WriteToConsole(a_Msg, static_cast<uint32_t>(a_Size));

		if (m_CacheString.size() + a_Size > m_MaxLoggerBufferSize)
		{
			m_UploadString.clear();
			m_UploadString.append(m_CacheString);
			//async upload to file.
			Threads::WaitForTask(m_LastThreadTask);
			m_LastThreadTask = Threads::StartTaskThread(WriteLoggerToFile, nullptr);
			//clear the cache string for new logging infos
			m_CacheString.clear();
		}

		m_CacheString.append(a_Msg, a_Size);

		OSUnlockMutex(m_WriteToFileMutex);
	}

	void LoggerWriteToFile()
	{
		const Buffer t_Buffer{ m_UploadString.data(), m_UploadString.size() };
		WriteToFile(m_LogFile, t_Buffer);
	}

	void EnableLogType(const WarningType a_WarningType)
	{
		m_EnabledWarningFlags |= static_cast<WarningTypeFlags>(a_WarningType);
	}

	void EnableLogTypes(const WarningTypeFlags a_WarningTypes)
	{
		m_EnabledWarningFlags = a_WarningTypes;
	}

	bool IsLogEnabled(const WarningType a_Type)
	{
		return (m_EnabledWarningFlags & (WarningTypeFlags)a_Type) == (WarningTypeFlags)a_Type;
	}
};
LoggerSingleton* LoggerSingleton::LoggerSingleton::m_LoggerInst = nullptr;

static void WriteLoggerToFile(void*)
{
	LoggerSingleton::GetInstance()->LoggerWriteToFile();
}

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

	LoggerSingleton::GetInstance()->WriteLogInfoToFile(t_String.data(), t_String.size());
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::INFO))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Info", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::OPTIMALIZATION))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Optimalization Warning", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::LOW))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (LOW)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::MEDIUM))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (MEDIUM)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::HIGH))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (HIGH)", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const char* a_Formats, ...)
{
	if (LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::ERROR))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Critical", a_Formats, t_vl);
	va_end(t_vl);
}

void Logger::EnableLogType(const WarningType a_WarningType)
{
	LoggerSingleton::GetInstance()->EnableLogType(a_WarningType);
}

void Logger::EnableLogTypes(const WarningTypeFlags a_WarningTypes)
{
	LoggerSingleton::GetInstance()->EnableLogTypes(a_WarningTypes);
}