#if INTERFACE
class Log {
public:
    enum LogLevels {
        LOG_VERBOSE = -1,
        LOG_INFO = 0,
        LOG_WARN = 1,
        LOG_ERROR = 2,
        LOG_IMPORTANT = 3,
    };

    static int         LogLevel;
    static const char* LogFilename;
    static bool        WriteToFile;
};
#endif

#include <Engine/Includes/Standard.h>
#include <Engine/Diagnostics/Log.h>

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

int         Log::LogLevel = -1;
bool        Log::WriteToFile = false;
const char* Log::LogFilename = TARGET_NAME ".log";

bool        Log_Initialized = false;

#if WIN32 || LINUX
#define USING_COLOR_CODES 1
#endif

PUBLIC STATIC void Log::Init() {
    if (Log_Initialized)
        return;

    // Set environment
    #ifdef MACOSX
        char appSupportPath[1024];
        int isBundle = MacOS_GetApplicationSupportDirectory(appSupportPath, 512);
        if (isBundle) {
            strcat(appSupportPath, "/" TARGET_NAME);
            if (!Directory::Exists(appSupportPath)) {
                Directory::Create(appSupportPath);
            }
            chdir(appSupportPath);
        }
    #endif

    #if WIN32 || MACOSX || LINUX || SWITCH
    WriteToFile = true;
    #endif

    FILE* f = NULL;

    if (WriteToFile) {
        f = fopen(LogFilename, "w");
        if (!f) {
            perror("Error ");
        }
    }

    if (WriteToFile && f) {
        fclose(f);
    }

    Log_Initialized = true;
}

PUBLIC STATIC void Log::SetLogLevel(int sev) {
    Log::LogLevel = sev;
}

PUBLIC STATIC void Log::Print(int sev, const char* format, ...) {
    #ifdef USING_COLOR_CODES
    int ColorCode = 0;
    #endif

    if (sev < Log::LogLevel)
        return;

    static char* stringBuffer = NULL;
    static size_t stringBufferSize = 0;
    const char* severityText = NULL;

    va_list args;
    va_start(args, format);
    int written_chars = vsnprintf(stringBuffer, stringBufferSize, format, args);
    va_end(args);

    if (written_chars <= 0)
        return;
    else if (written_chars + 1 >= stringBufferSize) {
        stringBufferSize = written_chars + 1024;

        char* newStringBuffer = (char*)realloc(stringBuffer, stringBufferSize * sizeof(char));

        // If the reallocation failed, try again with a smaller buffer size
        if (!newStringBuffer) {
            stringBufferSize = 1024;
            newStringBuffer = (char*)realloc(stringBuffer, stringBufferSize * sizeof(char));

            // If THAT failed too, just give up
            if (!newStringBuffer)
                return;
        }
        else
            stringBuffer = newStringBuffer;

        va_start(args, format);
        vsnprintf(stringBuffer, stringBufferSize, format, args);
        va_end(args);
    }

    const char *string = stringBuffer;

    #if defined(ANDROID)
        switch (sev) {
            case   LOG_VERBOSE: __android_log_print(ANDROID_LOG_VERBOSE, TARGET_NAME, "%s", string); return;
            case      LOG_INFO: __android_log_print(ANDROID_LOG_INFO,    TARGET_NAME, "%s", string); return;
            case      LOG_WARN: __android_log_print(ANDROID_LOG_WARN,    TARGET_NAME, "%s", string); return;
            case     LOG_ERROR: __android_log_print(ANDROID_LOG_ERROR,   TARGET_NAME, "%s", string); return;
            case LOG_IMPORTANT: __android_log_print(ANDROID_LOG_FATAL,   TARGET_NAME, "%s", string); return;
        }
    #endif

    FILE* f = NULL;
    if (WriteToFile) {
        f = fopen(LogFilename, "a");
    }

    #if defined(WIN32)
        switch (sev) {
            case   LOG_VERBOSE: ColorCode = 0xD; break;
            case      LOG_INFO: ColorCode = 0x8; break;
            case      LOG_WARN: ColorCode = 0xE; break;
            case     LOG_ERROR: ColorCode = 0xC; break;
            case LOG_IMPORTANT: ColorCode = 0xB; break;
        }
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
            WORD wColor = (csbi.wAttributes & 0xF0) + ColorCode;
            SetConsoleTextAttribute(hStdOut, wColor);
        }
    #elif USING_COLOR_CODES
        switch (sev) {
            case   LOG_VERBOSE: ColorCode = 94; break;
            case      LOG_INFO: ColorCode = 00; break;
            case      LOG_WARN: ColorCode = 93; break;
            case     LOG_ERROR: ColorCode = 91; break;
            case LOG_IMPORTANT: ColorCode = 96; break;
        }
        printf("\x1b[%d;1m", ColorCode);
    #endif

    switch (sev) {
        case   LOG_VERBOSE: severityText = "  VERBOSE: "; break;
        case      LOG_INFO: severityText = "     INFO: "; break;
        case      LOG_WARN: severityText = "  WARNING: "; break;
        case     LOG_ERROR: severityText = "    ERROR: "; break;
        case LOG_IMPORTANT: severityText = "IMPORTANT: "; break;
    }

    printf("%s", severityText);
    if (WriteToFile && f)
        fprintf(f, "%s", severityText);

    #if WIN32
		WORD wColor = (csbi.wAttributes & 0xF0) | 0x07;
        SetConsoleTextAttribute(hStdOut, wColor);
    #elif USING_COLOR_CODES
        printf("\x1b[0m");
    #endif

    printf("%s\n", string);
    fflush(stdout);

    if (WriteToFile && f) {
        fprintf(f, "%s\n", string);
        fclose(f);
    }
}
