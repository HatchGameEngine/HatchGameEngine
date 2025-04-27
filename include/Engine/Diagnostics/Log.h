#ifndef ENGINE_DIAGNOSTICS_LOG_H
#define ENGINE_DIAGNOSTICS_LOG_H

#include <Engine/Includes/Standard.h>

class Log {
private:
	static FILE* File;
	static bool Initialized;
	static char* Buffer;
	static size_t BufferSize;

	static bool ResizeBuffer(int written_chars);

public:
	enum LogLevels {
		LOG_VERBOSE = -1,
		LOG_INFO = 0,
		LOG_WARN = 1,
		LOG_ERROR = 2,
		LOG_IMPORTANT = 3,
	};
	static int LogLevel;
	static bool WriteToFile;

	static void Init();
	static void OpenFile(const char* filename);
	static void Close();
	static void SetLogLevel(int sev);
	static void Print(int sev, const char* format, ...);
	static void PrintSimple(const char* format, ...);
};

#ifndef ERROR_FILEINFO
#define ERROR_FILEINFO "In file " __FILE__ ", line " __LINE__ ": "
#endif

#ifdef ERROR_CHECKING

#ifndef ERROR_RET_VOID
//If cond is true, prints message to log and the function returns void
#define ERROR_RET_VOID(cond, ...)\
	if (!cond){\
		Print(Log::LOG_ERROR, ERROR_FILEINFO ...);\
		return;\
	}
#endif

#ifndef ERROR_RET_VAL
//like ERROR_RET_VOID, but will return ret_val
#define ERROR_RET_VAL(cond, ret_val, ...)\
	if (!cond){\
		Print(Log::LOG_ERROR, ERROR_FILEINFO ...);\
		return ret_val;\
	}
#endif

//If obj is null, the function returns void
#ifndef ERROR_IS_NULL_RET_VOID
#define ERROR_IS_NULL_RET_VOID(obj)\
	if (obj == nullptr){\
		Print(Log::LOG_ERROR, ERROR_FILEINFO "%s is null!", #obj);\
		return;\
	}
#endif

//If obj is null, the function returns ret_val
#ifndef ERROR_IS_NULL_RET_VAL
#define ERROR_IS_NULL_RET_VAL(obj, ret_val)\
	if (obj == nullptr){\
		Print(Log::LOG_ERROR, ERROR_FILEINFO "%s is null!", #obj);\
		return ret_val;\
	}
#endif

#else

#ifndef ERROR_RET_VOID
//If cond is true, prints message to log and the function returns void
#define ERROR_RET_VOID(cond, message)
#endif

#ifndef ERROR_RET_VAL
//like ERROR_RET_VOID, but will return ret_val.
#define ERROR_RET_VAL(cond, ret_val, ...)
#endif

#ifndef ERROR_IS_NULL_RET_VOID
//If obj is null, the function returns void
#define ERROR_IS_NULL_RET_VOID(obj)
#endif

#ifndef ERROR_IS_NULL_RET_VAL
//If obj is null, the function returns ret_val
#define ERROR_IS_NULL_RET_VAL(obj, ret_val)
#endif

#endif

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
