#include "BBGlobal.h"
#include "Logger.h"
#include "Program.h"

#include "BBThreadScheduler.hpp"
#include "Allocators.h"
#include "BBString.h"
#include <stdarg.h>

using namespace BB;
using namespace Logger;

static void WriteLoggerToFile(void*);

static LinearAllocator_t s_Allocator(mbSize * 4, "Log Allocator");

//dirty singleton
class LoggerSingleton
{
private:
	struct Channel
	{
		const char* name;
		OSFileHandle file;
		BBMutex mutex;
		bool overwriteWarningFlags = false;
		WarningTypeFlags overwriteFlags = 0;
		//create a fixed string class for this.
		String* cacheString = nullptr;
	};

	uint32_t m_MaxLoggerBufferSize;
	uint32_t m_NextFreeChannel = 0;
	Channel m_Channels[32];
	//create a fixed string class for this.
	//One upload string, maybe have an upload strng per channel.
	String m_UploadString;
	ThreadTask m_LastThreadTask = 0;
	const BBMutex m_WriteConsoleMut;

	static LoggerSingleton* m_LoggerInst;
public:

	LoggerSingleton()
		: m_MaxLoggerBufferSize(2024),
		  m_UploadString(s_Allocator, m_MaxLoggerBufferSize),
	      m_WriteConsoleMut(OSCreateMutex())
	{
		Channel& t_Chan = m_Channels[m_NextFreeChannel++];
		t_Chan.name = "Default";
		StackString<128> t_FileName("Default");
		t_FileName.append(".txt");
		t_Chan.file = CreateOSFile(t_FileName.c_str());
		t_Chan.mutex = OSCreateMutex();
		t_Chan.overwriteWarningFlags = UINT32_MAX;
		t_Chan.overwriteFlags = true;
		t_Chan.cacheString = BBnew(s_Allocator, String)(s_Allocator, m_MaxLoggerBufferSize);
	};

	~LoggerSingleton()
	{
		//Can't use the uploadstring safetly if a task is not finished.
		Threads::WaitForTask(m_LastThreadTask);
		//write the last logger information to file
		for (size_t i = 0; i < _countof(m_Channels); i++)
		{
			if (m_Channels[i].cacheString)
			{
				const Buffer t_String{ m_Channels[i].cacheString->data(), m_Channels[i].cacheString->size() };
				WriteToFile(m_Channels[i].file, t_String);
			}
		}
		//clear it to avoid issues related to reporting memory leaks.
		s_Allocator.Clear();
		DestroyMutex(m_WriteConsoleMut);
	};

	static LoggerSingleton* GetInstance()
	{
		if (!m_LoggerInst)
			m_LoggerInst = BBnew(s_Allocator, LoggerSingleton);

		return m_LoggerInst;
	}

	LogChannel CreateChannel(const char* a_Name, const WarningTypeFlags a_WarningFlags)
	{
		//set them all to true at the start.
		BB_ASSERT(m_NextFreeChannel >= 32, "Over 32 channels! This is not supported)");
		const LogChannel t_Return(m_NextFreeChannel);
		Channel& t_Chan = m_Channels[m_NextFreeChannel++];

		t_Chan.name = a_Name;
		StackString<128> t_FileName(a_Name);
		t_FileName.append(".txt");
		t_Chan.file = CreateOSFile(t_FileName.c_str());
		t_Chan.mutex = OSCreateMutex();
		t_Chan.overwriteWarningFlags = a_WarningFlags;
		t_Chan.overwriteFlags = a_WarningFlags;
		t_Chan.cacheString = BBnew(s_Allocator, String)(s_Allocator, m_MaxLoggerBufferSize);
		return t_Return;
	}

	void Log(const char* a_Msg, const size_t a_Size, const LogChannel a_Channel)
	{
		OSWaitAndLockMutex(m_WriteConsoleMut);
		WriteToConsole(a_Msg, static_cast<uint32_t>(a_Size));
		OSUnlockMutex(m_WriteConsoleMut);

		Channel& t_Chan = m_Channels[a_Channel.index];
		OSWaitAndLockMutex(t_Chan.mutex);
		if (t_Chan.cacheString->size() + a_Size > m_MaxLoggerBufferSize)
		{
			//if the task is finished then the upload string is free to use.
			Threads::WaitForTask(m_LastThreadTask);
			m_UploadString.clear();
			m_UploadString.append(*t_Chan.cacheString);
			//async upload to file.
			m_LastThreadTask = Threads::StartTaskThread(WriteLoggerToFile, t_Chan.file.ptrHandle);
			//clear the cache string for new logging infos
			t_Chan.cacheString->clear();
		}
		t_Chan.cacheString->append(a_Msg, a_Size);
		OSUnlockMutex(t_Chan.mutex);
	}

	const char* GetChannelName(const LogChannel a_Channel) const
	{
		return m_Channels[a_Channel.index].name;
	}

	void LoggerWriteToFile(const OSFileHandle a_File)
	{
		const Buffer t_Buffer{ m_UploadString.data(), m_UploadString.size() };
		WriteToFile(a_File, t_Buffer);
	}

	void EnableLogType(const WarningType a_WarningType, const LogChannel a_Channel)
	{
		m_Channels[a_Channel.index].overwriteWarningFlags |= static_cast<WarningTypeFlags>(a_WarningType);
	}

	void EnableLogTypes(const WarningTypeFlags a_WarningTypes, const LogChannel a_Channel)
	{
		m_Channels[a_Channel.index].overwriteWarningFlags = a_WarningTypes;
	}

	bool IsLogEnabled(const WarningType a_Type, const LogChannel a_Channel) const
	{
		return (m_Channels[a_Channel.index].overwriteWarningFlags & (WarningTypeFlags)a_Type) == (WarningTypeFlags)a_Type;
	}
};
LoggerSingleton* LoggerSingleton::LoggerSingleton::m_LoggerInst = nullptr;

static void WriteLoggerToFile(void* a_File)
{
	//jank
	LoggerSingleton::GetInstance()->LoggerWriteToFile(reinterpret_cast<uint64_t>(a_File));
}

static void Log_to_Console(const char* a_FileName, int a_Line, const char* a_WarningLevel, const char* a_Formats, va_list a_Args, const LogChannel a_Channel)
{
	constexpr const char LOG_MESSAGE_ERROR_LEVEL_1[]{ " Error Level: " };
	constexpr const char LOG_MESSAGE_FILE_2[]{ "\nFile: " };
	constexpr const char LOG_MESSAGE_LINE_NUMBER_3[]{ "\nLine Number: " };
	constexpr const char LOG_MESSAGE_MESSAGE_TXT_4[]{ "\nThe Message: " };

	//Format the message
	StackString<1024> t_String{};
	{	//Start with the channel
		t_String.append('[');
		t_String.append(LoggerSingleton::GetInstance()->GetChannelName(a_Channel));
		t_String.append(']');
	}

	{	//add warning level
		t_String.append(LOG_MESSAGE_ERROR_LEVEL_1, sizeof(LOG_MESSAGE_ERROR_LEVEL_1) - 1);
		t_String.append(a_WarningLevel);
	}
	{ //Get the file.
		t_String.append(LOG_MESSAGE_FILE_2, sizeof(LOG_MESSAGE_FILE_2) - 1);
		t_String.append(a_FileName);
	}
	{ //Get the line number into the buffer
		char lineNumString[8]{};
		sprintf_s(lineNumString, 8, "%u", a_Line);

		t_String.append(LOG_MESSAGE_LINE_NUMBER_3, sizeof(LOG_MESSAGE_LINE_NUMBER_3) - 1);
		t_String.append(lineNumString);
	}
	{ //Get the message(s).
		t_String.append(LOG_MESSAGE_MESSAGE_TXT_4, sizeof(LOG_MESSAGE_MESSAGE_TXT_4) - 1);
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

	LoggerSingleton::GetInstance()->Log(t_String.data(), t_String.size(), a_Channel);
}

void Logger::Log_Message(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::INFO, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Info", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::Log_Warning_Optimization(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::OPTIMALIZATION, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Optimalization Warning", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::Log_Warning_Low(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::LOW, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (LOW)", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::Log_Warning_Medium(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::MEDIUM, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (MEDIUM)", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::Log_Warning_High(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::HIGH, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Warning (HIGH)", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::Log_Error(const char* a_FileName, int a_Line, const LogChannel a_Channel, const char* a_Formats, ...)
{
	if (!LoggerSingleton::GetInstance()->IsLogEnabled(WarningType::ERROR, a_Channel))
		return;
	va_list t_vl;
	va_start(t_vl, a_Formats);
	Log_to_Console(a_FileName, a_Line, "Critical", a_Formats, t_vl, a_Channel);
	va_end(t_vl);
}

void Logger::EnableLogType(const WarningType a_WarningType, const LogChannel a_Channel)
{
	LoggerSingleton::GetInstance()->EnableLogType(a_WarningType, a_Channel);
}

void Logger::EnableLogTypes(const WarningTypeFlags a_WarningTypes, const LogChannel a_Channel)
{
	LoggerSingleton::GetInstance()->EnableLogTypes(a_WarningTypes, a_Channel);
}