#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Includes/Standard.h>

#ifdef HSL_STANDALONE
#include <Engine/Bytecode/StandaloneMain.h>
#endif

#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif

#ifdef MACOSX
extern "C" {
#include <Engine/Platforms/MacOS/Filesystem.h>
}
#include <Engine/Filesystem/Directory.h>
#include <Engine/Includes/StandardSDL2.h>
#include <unistd.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include <stdarg.h>

#define DEFAULT_LOG_FILENAME TARGET_NAME ".log"

int Log::LogLevel = -1;
bool Log::WriteToFile = true;
FILE* Log::File = nullptr;
bool Log::Initialized = false;
char* Log::Buffer = nullptr;
size_t Log::BufferSize = 0;
LogCallback Log::Callback;

#if WIN32 || LINUX
#define USING_COLOR_CODES 1
#endif

void Log::Init() {
	if (Initialized) {
		return;
	}

	LogLevel = -1;
	WriteToFile = true;
	Initialized = true;
}

void Log::OpenFile(const char* filename) {
	if (!Initialized || !WriteToFile) {
		return;
	}

	if (File) {
		fclose(File);
		File = nullptr;
	}

#ifdef HSL_STANDALONE
	File = fopen(filename, "w");

	if (!File) {
		Log::Print(Log::LOG_ERROR, "Couldn't open log file '%s' for writing!", filename);
		StandaloneExit("Couldn't open log file");
	}
#else
	std::string pathToLogFile;
	bool pathIsValid;

	const char* logFilename = filename;
	if (logFilename == nullptr || logFilename[0] == '\0') {
		logFilename = DEFAULT_LOG_FILENAME;
	}

	pathIsValid = Path::FromLocation(logFilename, PathLocation::LOGFILE, pathToLogFile, true);
	if (pathToLogFile.size() > 0) {
		logFilename = pathToLogFile.c_str();
	}
	else {
		pathIsValid = false;
	}

	if (pathIsValid) {
		Log::Print(Log::LOG_VERBOSE, "Log file: %s", logFilename);

		File = fopen(logFilename, "w");
	}

	if (!File) {
		Log::Print(Log::LOG_ERROR, "Couldn't open log file '%s' for writing!", logFilename);
		WriteToFile = false;
	}
#endif
}

bool Log::IsLoggingToFile() {
	return File != nullptr;
}

void Log::SetLogLevel(int sev) {
	Log::LogLevel = sev;
}

void Log::SetCallback(LogCallback callback) {
	Log::Callback = callback;
}

void Log::Close() {
	if (Initialized) {
		return;
	}

	Initialized = false;
	Callback = nullptr;

	if (File) {
		fclose(File);
		File = nullptr;
	}
	if (Buffer) {
		free(Buffer);
		Buffer = nullptr;
	}
}

bool Log::ResizeBuffer(int written_chars) {
	BufferSize = written_chars + 1024;
	Buffer = (char*)realloc(Buffer, BufferSize * sizeof(char));

	// If the reallocation failed, try again with a smaller buffer
	// size
	if (Buffer == nullptr) {
		BufferSize = 1024;
		Buffer = (char*)realloc(Buffer, BufferSize * sizeof(char));

		// If that failed too, just give up.
		if (!Buffer) {
			return false;
		}
	}

	return true;
}

void Log::Print(int sev, const char* format, ...) {
	if (!Initialized) {
		return;
	}

#ifdef USING_COLOR_CODES
	int ColorCode = 0;
#endif

	if (sev < Log::LogLevel) {
		return;
	}

	va_list args;
	va_start(args, format);
	int written_chars = vsnprintf(Buffer, BufferSize, format, args);
	va_end(args);

	if (written_chars <= 0) {
		return;
	}
	else if (written_chars + 1 >= BufferSize) {
		if (!ResizeBuffer(written_chars)) {
			return;
		}

		va_start(args, format);
		vsnprintf(Buffer, BufferSize, format, args);
		va_end(args);
	}

	if (Log::Callback) {
		Log::HandleCallback(sev, Buffer);
		return;
	}

#if defined(ANDROID)
	switch (sev) {
	case LOG_VERBOSE:
		__android_log_print(ANDROID_LOG_VERBOSE, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_INFO:
	case LOG_IMPORTANT:
	case LOG_API:
		__android_log_print(ANDROID_LOG_INFO, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_WARN:
		__android_log_print(ANDROID_LOG_WARN, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_ERROR:
		__android_log_print(ANDROID_LOG_ERROR, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_FATAL:
		__android_log_print(ANDROID_LOG_FATAL, TARGET_NAME, "%s", Buffer);
		return;
	}
#endif

#if defined(WIN32)
	switch (sev) {
	case LOG_VERBOSE:
		ColorCode = 0xD;
		break;
	case LOG_INFO:
		ColorCode = 0x8;
		break;
	case LOG_WARN:
		ColorCode = 0xE;
		break;
	case LOG_ERROR:
	case LOG_FATAL:
		ColorCode = 0xC;
		break;
	case LOG_IMPORTANT:
		ColorCode = 0xB;
		break;
	case LOG_API:
		ColorCode = 0x2;
		break;
	}
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
		WORD wColor = (csbi.wAttributes & 0xF0) + ColorCode;
		SetConsoleTextAttribute(hStdOut, wColor);
	}
#elif USING_COLOR_CODES
	switch (sev) {
	case LOG_VERBOSE:
		ColorCode = 94;
		break;
	case LOG_INFO:
		ColorCode = 00;
		break;
	case LOG_WARN:
		ColorCode = 93;
		break;
	case LOG_ERROR:
	case LOG_FATAL:
		ColorCode = 91;
		break;
	case LOG_IMPORTANT:
		ColorCode = 96;
		break;
	case LOG_API:
		ColorCode = 92;
		break;
	}
	printf("\x1b[%d;1m", ColorCode);
#endif

	const char* severityText = "";

	switch (sev) {
	case LOG_VERBOSE:
		severityText = "  VERBOSE: ";
		break;
	case LOG_INFO:
		severityText = "     INFO: ";
		break;
	case LOG_WARN:
		severityText = "  WARNING: ";
		break;
	case LOG_ERROR:
		severityText = "    ERROR: ";
		break;
	case LOG_IMPORTANT:
		severityText = "IMPORTANT: ";
		break;
	case LOG_FATAL:
		severityText = "    FATAL: ";
		break;
	case LOG_API:
		severityText = "      API: ";
		break;
	}

	printf("%s", severityText);
	if (File) {
		fprintf(File, "%s", severityText);
	}

#if WIN32
	WORD wColor = (csbi.wAttributes & 0xF0) | 0x07;
	SetConsoleTextAttribute(hStdOut, wColor);
#elif USING_COLOR_CODES
	printf("\x1b[0m");
#endif

	printf("%s\n", Buffer);
	fflush(stdout);

	if (File) {
		fprintf(File, "%s\n", Buffer);
	}
}

#define PRINT_VARIADIC_ARGS(fmt) { \
	va_list args; \
	va_start(args, fmt); \
	int written_chars = vsnprintf(Buffer, BufferSize, fmt, args); \
	va_end(args); \
	if (written_chars <= 0) { \
		return; \
	} \
	else if (written_chars + 1 >= BufferSize) { \
		if (!ResizeBuffer(written_chars)) { \
			return; \
		} \
		va_start(args, fmt); \
		vsnprintf(Buffer, BufferSize, fmt, args); \
		va_end(args); \
	} \
}

void Log::PrintSimple(const char* format, ...) {
	if (!Initialized) {
		return;
	}

	PRINT_VARIADIC_ARGS(format);

	if (Log::Callback) {
		Log::HandleCallback(LOG_INFO, Buffer);
		return;
	}

	printf("%s", Buffer);
	fflush(stdout);

	if (File) {
		fprintf(File, "%s", Buffer);
	}
}

void Log::HandleCallback(int sev, const char* text) {
#ifdef HSL_LIBRARY
	switch (sev) {
	case LOG_VERBOSE:
		sev = HSL_LOG_VERBOSE;
		break;
	case LOG_INFO:
		sev = HSL_LOG_INFO;
		break;
	case LOG_WARN:
		sev = HSL_LOG_WARN;
		break;
	case LOG_ERROR:
		sev = HSL_LOG_ERROR;
		break;
	case LOG_FATAL:
		sev = HSL_LOG_FATAL;
		break;
	case LOG_IMPORTANT:
		sev = HSL_LOG_IMPORTANT;
		break;
	case LOG_API:
		sev = HSL_LOG_API;
		break;
	}
#endif

	Log::Callback(sev, text);
}

void Log::Write(const char* format, ...) {
	if (!Initialized || !File) {
		return;
	}

	PRINT_VARIADIC_ARGS(format);

	fprintf(File, "%s", Buffer);
}
