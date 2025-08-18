#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Error.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

void Error::Fatal(const char* errorMessage, ...) {
	va_list args;
	char errorString[2048];
	va_start(args, errorMessage);
	vsnprintf(errorString, sizeof(errorString), errorMessage, args);
	va_end(args);

	Log::Print(Log::LOG_FATAL, "%s", errorString);

	// This doesn't check the return code because the error is already logged.
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", errorString, nullptr);

	Application::Cleanup();
	exit(-1);
}
