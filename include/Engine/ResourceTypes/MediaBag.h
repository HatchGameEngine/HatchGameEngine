#ifndef ENGINE_RESOURCETYPES_MEDIABAG_H
#define ENGINE_RESOURCETYPES_MEDIABAG_H

#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Asset.h>

class MediaBag : public Asset {
private:
	bool Load(const char* filename);

public:
	Texture* VideoTexture;
	MediaSource* Source;
	MediaPlayer* Player;
	bool Dirty;

	MediaBag(const char* filename);
	void Unload();
	~MediaBag();
};

#endif /* ENGINE_RESOURCETYPES_MEDIABAG_H */
