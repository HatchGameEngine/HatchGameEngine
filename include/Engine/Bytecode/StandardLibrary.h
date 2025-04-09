#ifndef ENGINE_BYTECODE_STANDARDLIBRARY_H
#define ENGINE_BYTECODE_STANDARDLIBRARY_H

#include <Engine/Bytecode/Types.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>

namespace StandardLibrary {
//public:
	int GetInteger(VMValue* args, int index, Uint32 threadID);
	float GetDecimal(VMValue* args, int index, Uint32 threadID);
	char* GetString(VMValue* args, int index, Uint32 threadID);
	ObjString* GetVMString(VMValue* args, int index, Uint32 threadID);
	ObjArray* GetArray(VMValue* args, int index, Uint32 threadID);
	ObjMap* GetMap(VMValue* args, int index, Uint32 threadID);
	ISprite* GetSprite(VMValue* args, int index, Uint32 threadID);
	ISound* GetSound(VMValue* args, int index, Uint32 threadID);
	ObjInstance* GetInstance(VMValue* args, int index, Uint32 threadID);
	ObjFunction* GetFunction(VMValue* args, int index, Uint32 threadID);
	void CheckArgCount(int argCount, int expects);
	void CheckAtLeastArgCount(int argCount, int expects);
	void Link();
	void Dispose();
};

#endif /* ENGINE_BYTECODE_STANDARDLIBRARY_H */
