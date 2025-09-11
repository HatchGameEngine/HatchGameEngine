#ifndef ENGINE_RESOURCETYPES_RESOURCEABLE_H
#define ENGINE_RESOURCETYPES_RESOURCEABLE_H

#include <Engine/Includes/Standard.h>

class Resourceable {
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
	virtual ~Resourceable();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEABLE_H */
