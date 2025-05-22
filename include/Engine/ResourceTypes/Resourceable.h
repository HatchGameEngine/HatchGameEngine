#ifndef ENGINE_RESOURCETYPES_RESOURCEABLE_H
#define ENGINE_RESOURCETYPES_RESOURCEABLE_H

#include <Engine/Includes/Standard.h>

class Resourceable {
protected:
	bool LoadFailed = true;
	void* VMObject = nullptr;

public:
	Uint8 Type = 0;

	bool Loaded();
	void* GetVMObject();
	void* GetVMObjectPtr();
	void ReleaseVMObject();
	virtual ~Resourceable();
};

#endif /* ENGINE_RESOURCETYPES_RESOURCEABLE_H */
