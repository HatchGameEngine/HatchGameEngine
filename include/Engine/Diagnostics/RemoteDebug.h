#ifndef ENGINE_DIAGNOSTICS_REMOTEDEBUG_H
#define ENGINE_DIAGNOSTICS_REMOTEDEBUG_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED



class RemoteDebug {
public:
    static bool        Initialized;
    static bool        UsingRemoteDebug;

    static void Init();
    static bool AwaitResponse();
};

#endif /* ENGINE_DIAGNOSTICS_REMOTEDEBUG_H */
