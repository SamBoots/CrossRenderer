#pragma once
namespace BB
{
	enum class WarningType
	{
		INFO, //No warning, just a message.
		OPTIMALIZATION, //Indicates a possible issue that might cause problems with performance.
		LOW, //Low chance of breaking the application or causing undefined behaviour.
		MEDIUM, //Medium chance of breaking the application or causing undefined behaviour.
		HIGH //High chance of breaking the application or causing undefined behaviour.
	};

	namespace Logger
	{
		void Log_Message(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Warning_Optimization(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Warning_Low(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Warning_Medium(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Warning_High(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Exception(const char* a_FileName, int a_Line, const char* a_Message);
		void Log_Error(const char* a_FileName, int a_Line, const char* a_Message);
	}
}

#define BB_LOG(a_Msg) BB::Logger::Log_Message(__FILE__, __LINE__, a_Msg) \

#ifdef _DEBUG
		/*  Check for unintented behaviour at compile time, if a_Check is false the program will stop and post a message.
			@param a_Check, If false the program will print the message and assert.
			@param a_Msg, The message that will be printed. */
#define BB_STATIC_ASSERT(a_Check, a_Msg) static_assert(a_Check, a_Msg)\

			/*  Check for unintented behaviour at runetime, if a_Check is false the program will stop and post a message.
			@param a_Check, If false the program will print the message and assert.
			@param a_Msg, The message that will be printed. */
#define BB_ASSERT(a_Check, a_Msg)\
			if (!(a_Check)) \
			{\
				BB::Logger::Log_Error(__FILE__, __LINE__, a_Msg);\
			}

			/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
			@param a_Check, If false the program will print the message and assert.
			@param a_Msg, The message that will be printed. */
#define BB_EXCEPTION(a_Check, a_Msg)\
			if (!(a_Check)) \
			{\
				BB::Logger::Log_Exception(__FILE__, __LINE__, a_Msg);\
			}

			/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
			@param a_Check, If false the program will print the message and assert.
			@param a_Msg, The message that will be printed.
			@param a_WarningType, The warning level, enum found at WarningType. */
#define BB_WARNING(a_Check, a_Msg, a_WarningType)\
			if (!(a_Check)) \
			{\
				WarningType TYPE{}; switch(a_WarningType){ \
			case WarningType::OPTIMALIZATION:           \
				BB::Logger::Log_Warning_Optimization(__FILE__, __LINE__, a_Msg);\
				break;\
            case WarningType::LOW:\
				BB::Logger::Log_Warning_Low(__FILE__, __LINE__, a_Msg);\
				break;\
            case WarningType::MEDIUM:\
				BB::Logger::Log_Warning_Medium(__FILE__, __LINE__, a_Msg);\
				break;\
            case WarningType::HIGH:\
				BB::Logger::Log_Warning_High(__FILE__, __LINE__, a_Msg);\
				break;\
         }; TYPE; \
			}
#else
/*  Check for unintented behaviour at compile time, if a_Check is false the program will stop and post a message.
	@param a_Check, If false the program will print the message and assert.
	@param a_Msg, The message that will be printed. */
#define BB_STATIC_ASSERT(a_Check, a_Msg) a_Check
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_Msg, The message that will be printed. */
#define BB_ASSERT(a_Check, a_Msg) a_Check
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_Msg, The message that will be printed. */
#define BB_EXCEPTION(a_Check, a_Msg) a_Check
/*  Check for unintented behaviour at runtime, if a_Check is false the program will post a warning message.
@param a_Check, If false the program will print the message and assert.
@param a_Msg, The message that will be printed.
@param a_WarningType, The warning level, enum found at WarningType. */
#define BB_WARNING(a_Check, a_Msg, a_WarningType) a_Check
#endif //_DEBUG