#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Includes/Standard.h>

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

#if WIN32 || LINUX
#define USING_COLOR_CODES 1
#endif

void Log::Init() {
	Initialized = true;
}

void Log::OpenFile(const char* filename) {
	if (!Initialized || !WriteToFile) {
		return;
	}

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
}

void Log::SetLogLevel(int sev) {
	Log::LogLevel = sev;
}

void Log::Close() {
	if (File) {
		fclose(File);
		File = nullptr;
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

	const char* severityText = NULL;

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

#if defined(ANDROID)
	switch (sev) {
	case LOG_VERBOSE:
		__android_log_print(ANDROID_LOG_VERBOSE, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_INFO:
		__android_log_print(ANDROID_LOG_INFO, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_WARN:
		__android_log_print(ANDROID_LOG_WARN, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_ERROR:
		__android_log_print(ANDROID_LOG_ERROR, TARGET_NAME, "%s", Buffer);
		return;
	case LOG_IMPORTANT:
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
		ColorCode = 0xC;
		break;
	case LOG_IMPORTANT:
		ColorCode = 0xB;
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
		ColorCode = 91;
		break;
	case LOG_IMPORTANT:
		ColorCode = 96;
		break;
	}
	printf("\x1b[%d;1m", ColorCode);
#endif

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

void Log::PrintSimple(const char* format, ...) {
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

	printf("%s", Buffer);
	fflush(stdout);

	if (File) {
		fprintf(File, "%s", Buffer);
	}
}
