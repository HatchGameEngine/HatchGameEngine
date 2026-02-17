#ifdef HSL_STANDALONE
#include <Engine/Bytecode/StandaloneMain.h>
#else
#include <Engine/Application.h>
#endif

int main(int argc, char* args[]) {
	int result = 0;

#if SWITCH
	Log::Init();
	socketInitializeDefault();
	nxlinkStdio();
#if defined(SWITCH_ROMFS)
	romfsInit();
#endif
#endif

#ifndef HSL_STANDALONE
	Application::Run(argc, args);
#else
	result = StandaloneMain(argc, args);
#endif

#if SWITCH
#if defined(SWITCH_ROMFS)
	romfsExit();
#endif
	socketExit();
#endif

	return result;
}
