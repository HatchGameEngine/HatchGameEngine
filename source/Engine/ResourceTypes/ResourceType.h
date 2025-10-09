#ifndef ENGINE_RESOURCETYPES_RESOURCETYPE_H
#define ENGINE_RESOURCETYPES_RESOURCETYPE_H

#include <Engine/Media/MediaPlayer.h>
#include <Engine/Media/MediaSource.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/Image.h>
#include <Engine/ResourceTypes/MediaBag.h>
#include <Engine/ResourceTypes/Asset.h>

enum {
	RESOURCE_NONE,
	RESOURCE_SPRITE,
	RESOURCE_IMAGE,
	RESOURCE_AUDIO,
	RESOURCE_MODEL,
	RESOURCE_MEDIA
};

inline const char* GetResourceTypeString(Uint8 type) {
	switch (type) {
	case RESOURCE_SPRITE:
		return "sprite";
	case RESOURCE_IMAGE:
		return "image";
	case RESOURCE_AUDIO:
		return "audio";
	case RESOURCE_MODEL:
		return "model";
	case RESOURCE_MEDIA:
		return "media";
	default:
		break;
	}

	return "unknown";
}

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
	Uint8 Type = RESOURCE_NONE;
	char* Filename = nullptr;
	Uint32 FilenameHash = 0;
	bool Loaded = false;
	bool Unique = false;
	int RefCount = 0;
	void* VMObject;
	Uint32 UnloadPolicy;
	union {
		ISprite* AsSprite;
		Image* AsImage;
		ISound* AsAudio;
		IModel* AsModel;
		MediaBag* AsMedia;
		Asset* AsAsset;
	};
};

#endif /* ENGINE_RESOURCETYPES_RESOURCETYPE_H */
