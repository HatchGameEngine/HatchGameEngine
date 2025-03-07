#ifndef ENGINE_MEDIA_MEDIASOURCE_H
#define ENGINE_MEDIA_MEDIASOURCE_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Media/Includes/AVFormat.h>

class MediaSource {
public:
	enum {
		STREAMTYPE_UNKNOWN,
		STREAMTYPE_VIDEO,
		STREAMTYPE_AUDIO,
		STREAMTYPE_DATA,
		STREAMTYPE_SUBTITLE,
		STREAMTYPE_ATTACHMENT
	};
	void* FormatCtx;
	void* AvioCtx;
	Stream* StreamPtr;

	static MediaSource* CreateSourceFromUrl(const char* url);
	static MediaSource* CreateSourceFromStream(Stream* stream);
	int GetStreamInfo(Uint32* info, int index);
	int GetStreamCount();
	int GetBestStream(Uint32 type);
	void Close();
};

#endif /* ENGINE_MEDIA_MEDIASOURCE_H */
