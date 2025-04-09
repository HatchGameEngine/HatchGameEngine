#ifndef ENGINE_DIAGNOSTICS_LOG_H
#define ENGINE_DIAGNOSTICS_LOG_H

#include <Engine/Includes/Standard.h>

namespace Log {
//private:
	extern FILE* File;
	extern bool Initialized;
	extern char* Buffer;
	extern size_t BufferSize;

	bool ResizeBuffer(int written_chars);

//public:
	enum LogLevels {
		LOG_VERBOSE = -1,
		LOG_INFO = 0,
		LOG_WARN = 1,
		LOG_ERROR = 2,
		LOG_IMPORTANT = 3,
	};
	extern int LogLevel;
	extern bool WriteToFile;

	void Init();
	void OpenFile(const char* filename);
	void Close();
	void SetLogLevel(int sev);
	void Print(int sev, const char* format, ...);
	void PrintSimple(const char* format, ...);
};

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
