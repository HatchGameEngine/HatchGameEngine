#ifndef ENGINE_BYTECODE_STANDALONEMAIN_H
#define ENGINE_BYTECODE_STANDALONEMAIN_H

#ifdef HSL_STANDALONE
#include <string>

#ifdef HSL_LIBRARY
#include <Engine/Bytecode/API.h>
#else
int StandaloneMain(int argc, char* args[]);
#endif

bool InStandaloneREPL();

void InitSubsystems();
void DisposeSubsystems();

void StandaloneExit(std::string error);

#ifdef HSL_LIBRARY
class ScriptManager;

hsl_ErrorResponse HandleVMRuntimeError(ScriptManager* manager, hsl_Result error, std::string text);
#endif

bool ShouldShowGarbageCollectionOutput();
const char* GetScriptsDirectory();
void SetScriptsDirectory(const char* directory);
const char* GetVersionText();
#endif

#endif /* ENGINE_BYTECODE_STANDALONEMAIN_H */
