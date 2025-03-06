#ifndef ENGINE_RESOURCETYPES_RESOURCETYPE_H
#define ENGINE_RESOURCETYPES_RESOURCETYPE_H

#include <Engine/Media/MediaPlayer.h>
#include <Engine/Media/MediaSource.h>
#include <Engine/Rendering/Texture.h>
#include <Engine/ResourceTypes/IModel.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/ResourceTypes/ISprite.h>
#include <Engine/ResourceTypes/Image.h>

struct MediaBag {
	Texture* VideoTexture;
	MediaSource* Source;
	MediaPlayer* Player;
	bool Dirty;
};

struct ResourceType {
	Uint32 FilenameHash;
	union {
		ISprite* AsSprite;
		Image* AsImage;
		ISound* AsSound;
		ISound* AsMusic;
		void* AsFont;
		void* AsShader;
		IModel* AsModel;
		MediaBag* AsMedia;
	};
	Uint32 UnloadPolicy;
};

#endif /* ENGINE_RESOURCETYPES_RESOURCETYPE_H */
