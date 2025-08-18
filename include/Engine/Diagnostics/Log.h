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
		LOG_FATAL = 4
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

#define STR_IFY(num) #num

#ifndef ERROR_FILEINFO
#define ERROR_FILEINFO "In file " __FILE__ ", line " STR_IFY(__LINE__) ": "
#endif

//Error checks that will be active no matter what

#ifndef ERROR_RET_VOID
//If cond is true, prints message to log and the function returns void
#define ERROR_RET_VOID(cond, ...)\
	if (!(cond)){\
		Log::Print(Log::LOG_ERROR, ERROR_FILEINFO __VA_ARGS__);\
		return;\
	}
#endif

#ifndef ERROR_RET_VAL
//like ERROR_RET_VOID, but will return ret_val
#define ERROR_RET_VAL(cond, ret_val, ...)\
	if (!(cond)){\
		Log::Print(Log::LOG_ERROR, ERROR_FILEINFO __VA_ARGS__);\
		return ret_val;\
	}
#endif

//If obj is null, the function returns void
#ifndef ERROR_IS_NULL_RET_VOID
#define ERROR_IS_NULL_RET_VOID(obj, ...)\
	if (obj == nullptr){\
		Log::Print(Log::LOG_ERROR, ERROR_FILEINFO __VA_ARGS__);\
		return;\
	}
#endif

//If obj is null, the function returns ret_val
#ifndef ERROR_IS_NULL_RET_VAL
#define ERROR_IS_NULL_RET_VAL(obj, ret_val, ...)\
	if (obj == nullptr){\
		Log::Print(Log::LOG_ERROR, ERROR_FILEINFO __VA_ARGS__);\
		return ret_val;\
	}
#endif

//Error checks that are nonessential, but may aid in debugging

#ifdef ERROR_CHECKING

#ifndef VERB_ERROR_RET_VOID
//If cond is true, prints message to log and the function returns void
#define VERB_ERROR_RET_VOID(cond, ...) ERROR_RET_VOID(cond, __VA_ARGS__)
#endif

#ifndef VERB_ERROR_RET_VAL
//like VERB_ERROR_RET_VOID, but will return ret_val
#define VERB_ERROR_RET_VAL(cond, ret_val, ...) ERROR_RET_VAL(cond, ret_val, __VA_ARGS__)
#endif

#ifndef VERB_ERROR_IS_NULL_RET_VOID
//If obj is null, the function returns void
#define VERB_ERROR_IS_NULL_RET_VOID(obj, ...) ERROR_IS_NULL_RET_VOID(obj, __VA_ARGS__)
#endif

#ifndef VERB_ERROR_IS_NULL_RET_VAL
//If obj is null, the function returns ret_val
#define VERB_ERROR_IS_NULL_RET_VAL(obj, ret_val, ...) ERROR_IS_NULL_RET_VAL(obj, ret_val, __VA_ARGS__)
#endif

#else

#ifndef VERB_ERROR_RET_VOID
//If cond is true, prints message to log and the function returns void
#define VERB_ERROR_RET_VOID(cond, ...)
#endif

#ifndef VERB_ERROR_RET_VAL
//like VERB_ERROR_RET_VOID, but will return ret_val
#define VERB_ERROR_RET_VAL(cond, ret_val, ...)
#endif

#ifndef VERB_ERROR_IS_NULL_RET_VOID
//If obj is null, the function returns void
#define VERB_ERROR_IS_NULL_RET_VOID(obj)
#endif

#ifndef VERB_ERROR_IS_NULL_RET_VAL
//If obj is null, the function returns ret_val
#define VERB_ERROR_IS_NULL_RET_VAL(obj, ret_val)
#endif

#endif

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
