#ifndef ENGINE_RESOURCETYPES_ASSET_H
#define ENGINE_RESOURCETYPES_ASSET_H

#include <Engine/Includes/Standard.h>

enum AssetType {
	ASSET_NONE,
	ASSET_SPRITE,
	ASSET_IMAGE,
	ASSET_AUDIO,
	ASSET_MODEL,
	ASSET_MEDIA
};

inline const char* GetAssetTypeString(AssetType type) {
	switch (type) {
	case ASSET_SPRITE:
		return "sprite";
	case ASSET_IMAGE:
		return "image";
	case ASSET_AUDIO:
		return "audio";
	case ASSET_MODEL:
		return "model";
	case ASSET_MEDIA:
		return "media";
	default:
		break;
	}

	return "unknown";
}

class Asset {
protected:
	bool Loaded = false;
	void* VMObject = nullptr;
	int RefCount = 0;

public:
	AssetType Type = ASSET_NONE;

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
