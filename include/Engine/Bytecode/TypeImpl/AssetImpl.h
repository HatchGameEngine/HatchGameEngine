#ifndef ENGINE_BYTECODE_TYPEIMPL_ASSETIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_ASSETIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Asset.h>

#define IS_ASSET(value) IsObjectType(value, OBJ_ASSET)
#define AS_ASSET(value) ((ObjAsset*)AS_OBJECT(value))

#define GET_ASSET(object) (Asset*)(((ObjAsset*)object)->AssetPtr)

class AssetImpl {
public:
	static ObjClass* Class;

	static void Init();
	static void* New(void* ptr);
	static void Dispose(Obj* object);

	static ValueGetFn GetGetter(AssetType type);
	static ValueSetFn GetSetter(AssetType type);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_ASSETIMPL_H */
