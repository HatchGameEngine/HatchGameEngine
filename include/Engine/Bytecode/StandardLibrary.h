#ifndef ENGINE_BYTECODE_STANDARDLIBRARY_H
#define ENGINE_BYTECODE_STANDARDLIBRARY_H

#include <Engine/Bytecode/Types.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>

class StandardLibrary {
public:
	static int GetInteger(VMValue* args, int index, Uint32 threadID);
	static float GetDecimal(VMValue* args, int index, Uint32 threadID);
	static char* GetString(VMValue* args, int index, Uint32 threadID);
	static ObjString* GetVMString(VMValue* args, int index, Uint32 threadID);
	static ObjArray* GetArray(VMValue* args, int index, Uint32 threadID);
	static ObjMap* GetMap(VMValue* args, int index, Uint32 threadID);
	static ISprite* GetSprite(VMValue* args, int index, Uint32 threadID);
	static ISound* GetSound(VMValue* args, int index, Uint32 threadID);
	static ObjInstance* GetInstance(VMValue* args, int index, Uint32 threadID);
	static ObjFunction* GetFunction(VMValue* args, int index, Uint32 threadID);
	static void CheckArgCount(int argCount, int expects);
	static void CheckAtLeastArgCount(int argCount, int expects);
	static void Link();
	static void Dispose();
};

#endif /* ENGINE_BYTECODE_STANDARDLIBRARY_H */
