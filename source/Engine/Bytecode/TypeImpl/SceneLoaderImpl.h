#ifndef ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_SCENELOADER "SceneLoader"

class SceneLoaderImpl {
public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static VMValue VM_Initializer(int argCount, VMValue* args, Uint32 threadID);
	static void Dispose(Obj* object);

	static VMValue VM_GetTileWidth(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetTileHeight(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectCount(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectClass(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectX(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectY(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_GetObjectProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_ObjectHasProperty(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_SpawnObject(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_SCENELOADERIMPL_H */
