#ifndef ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H
#define ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H

#include <Engine/Includes/Standard.h>

class MediaPlayerState {
public:
	static Uint32 InitFlags;
	static Uint32 ThreadCount;
	static Uint32 FontHinting;
	static Uint32 VideoBufFrames;
	static Uint32 AudioBufFrames;
	static Uint32 SubtitleBufFrames;
	static void* LibassHandle;
	static void* AssSharedObjectHandle;

	static double GetSystemTime();
	static bool AttachmentIsFont(void* p);
};

#endif /* ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H */
