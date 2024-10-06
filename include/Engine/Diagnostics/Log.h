#ifndef ENGINE_DIAGNOSTICS_LOG_H
#define ENGINE_DIAGNOSTICS_LOG_H


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

    static void Init();
    static void SetLogLevel(int sev);
    static void Print(int sev, const char* format, ...);
};

#endif /* ENGINE_DIAGNOSTICS_LOG_H */
