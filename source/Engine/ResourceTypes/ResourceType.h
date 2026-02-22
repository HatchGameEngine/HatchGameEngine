#ifndef ENGINE_RESOURCETYPES_RESOURCETYPE_H
#define ENGINE_RESOURCETYPES_RESOURCETYPE_H

#include <Engine/Media/MediaPlayer.h>
#include <Engine/Media/MediaSource.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/Asset.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/Image.h>
#include <Engine/ResourceTypes/MediaBag.h>

inline const char* GetResourceScopeString(Uint8 scope) {
	switch (scope) {
	case SCOPE_GAME:
		return "game";
	case SCOPE_SCENE:
		return "scene";
	default:
		break;
	}

	return "unknown";
}

struct ResourceType {
	AssetType Type = ASSET_NONE;
	char* Filename = nullptr;
	Uint32 FilenameHash = 0;
	bool Loaded = false;
	bool Unique = false;
	int RefCount = 0;
	void* VMObject;
	Uint32 UnloadPolicy;
	union {
		Asset* AsAsset;
		// All types below can be casted as Asset
		ISprite* AsSprite;
		Image* AsImage;
		ISound* AsAudio;
		IModel* AsModel;
		MediaBag* AsMedia;
	};
};

#endif /* ENGINE_RESOURCETYPES_RESOURCETYPE_H */
