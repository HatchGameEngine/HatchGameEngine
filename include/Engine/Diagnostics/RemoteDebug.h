#ifndef ENGINE_DIAGNOSTICS_REMOTEDEBUG_H
#define ENGINE_DIAGNOSTICS_REMOTEDEBUG_H

namespace RemoteDebug {
//public:
	extern bool Initialized;
	extern bool UsingRemoteDebug;

	void Init();
	bool AwaitResponse();
};

#endif /* ENGINE_DIAGNOSTICS_REMOTEDEBUG_H */
