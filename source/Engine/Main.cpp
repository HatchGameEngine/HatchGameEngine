#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Includes/Standard.h>

#if defined(WIN32) && !defined(WINDOWS_CONSOLE_APP) && !defined(_MSC_VER)
int WinMain(int argc, char* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
#if SWITCH
	Log::Init();
	socketInitializeDefault();
	nxlinkStdio();
#if defined(SWITCH_ROMFS)
	romfsInit();
#endif
	// pcvInitialize();
	// pcvSetClockRate(PcvModule_CpuBus, 1581000000); // normal:
	// 1020000000, overclock: 1581000000, strong overclock:
	// 1785000000
#endif

	Application::Run(argc, argv);

#if SWITCH
// pcvSetClockRate(PcvModule_CpuBus, 1020000000);
// pcvExit();
#if defined(SWITCH_ROMFS)
	romfsExit();
#endif
	socketExit();
#endif
	return 0;
}
