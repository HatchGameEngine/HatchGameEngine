#ifndef ENGINE_RESOURCETYPES_MEDIABAG_H
#define ENGINE_RESOURCETYPES_MEDIABAG_H

#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/Resourceable.h>

class MediaBag : public Resourceable {
private:
	bool Load(const char* filename);

public:
	Texture* VideoTexture;
	MediaSource* Source;
	MediaPlayer* Player;
	bool Dirty;

	MediaBag(const char* filename);
	void Close();
	void Dispose();
	~MediaBag();
};

#endif /* ENGINE_RESOURCETYPES_MEDIABAG_H */
