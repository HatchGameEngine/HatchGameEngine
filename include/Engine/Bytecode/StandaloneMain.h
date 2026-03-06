#ifndef ENGINE_BYTECODE_STANDALONEMAIN_H
#define ENGINE_BYTECODE_STANDALONEMAIN_H

#ifdef HSL_STANDALONE
#include <string>

#include <Engine/Bytecode/API.h>

#ifndef HSL_LIBRARY
int StandaloneMain(int argc, char* args[]);
#endif

bool InStandaloneREPL();

void InitSubsystems();
void DisposeSubsystems();

bool LockScriptManager(void* manager);
bool UnlockScriptManager(void* manager);

void SetScriptManagerLockFunction(hsl_LockFunction function);
void SetScriptManagerUnlockFunction(hsl_UnlockFunction function);

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
