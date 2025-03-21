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

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
