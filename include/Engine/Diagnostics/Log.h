#ifndef ENGINE_DIAGNOSTICS_LOG_H
#define ENGINE_DIAGNOSTICS_LOG_H

#include <Engine/Includes/Standard.h>

typedef void (*LogCallback)(int level, const char* text);

class Log {
private:
	static FILE* File;
	static bool Initialized;
	static char* Buffer;
	static size_t BufferSize;

	static LogCallback Callback;

	static bool ResizeBuffer(int written_chars);

	static void HandleCallback(int sev, const char* text);

public:
	enum LogLevels {
		LOG_VERBOSE = -1,
		LOG_INFO = 0,
		LOG_WARN = 1,
		LOG_ERROR = 2,
		LOG_IMPORTANT = 3,
		LOG_FATAL = 4,
		LOG_API = 5,
	};
	static int LogLevel;
	static bool WriteToFile;

	static void Init();
	static void OpenFile(const char* filename);
	static void Close();
	static bool IsLoggingToFile();
	static void SetLogLevel(int sev);
	static void SetCallback(LogCallback callback);
	static void Print(int sev, const char* format, ...);
	static void PrintSimple(const char* format, ...);
	static void Write(const char* format, ...);
};

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
