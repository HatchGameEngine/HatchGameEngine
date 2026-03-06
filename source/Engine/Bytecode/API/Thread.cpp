#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/VMThread.h>

hsl_Thread* hsl_thread_new(hsl_Context* context) {
	ScriptManager* manager = (ScriptManager*)context;
	if (!manager) {
		return nullptr;
	}

	VMThread* vmThread = manager->NewThread();
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Thread*)vmThread;
}

hsl_Context* hsl_thread_get_context(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return nullptr;
	}

	return (hsl_Context*)vmThread->Manager;
}

void hsl_thread_free(hsl_Thread* thread) {
	VMThread* vmThread = (VMThread*)thread;
	if (!vmThread) {
		return;
	}

	vmThread->Manager->DisposeThread(vmThread);
}
#endif
