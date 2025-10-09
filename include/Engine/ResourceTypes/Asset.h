#ifndef ENGINE_RESOURCETYPES_ASSET_H
#define ENGINE_RESOURCETYPES_ASSET_H

#include <Engine/Includes/Standard.h>

class Asset {
protected:
	bool Loaded = false;
	void* VMObject = nullptr;
	int RefCount = 0;

public:
	Uint8 Type = 0;

	bool IsLoaded();
	void TakeRef();
	bool Release();
	void* GetVMObject();
	void* GetVMObjectPtr();
	void ReleaseVMObject();
	virtual void Unload();
	virtual ~Asset();
};

#endif /* ENGINE_RESOURCETYPES_ASSET_H */
