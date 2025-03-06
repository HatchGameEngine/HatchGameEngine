#include <Engine/Diagnostics/RemoteDebug.h>
#include <Engine/Includes/Standard.h>

bool RemoteDebug::Initialized = false;
bool RemoteDebug::UsingRemoteDebug = false;

void RemoteDebug::Init() {
	if (RemoteDebug::Initialized) {
		return;
	}

	RemoteDebug::Initialized = true;
}

bool RemoteDebug::AwaitResponse() {
	// To be used inside a for loop while awaiting a response from
	// the remote debugger
	return true;
}
