#ifndef ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H
#define ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H

#include <Engine/Includes/Standard.h>

namespace MediaPlayerState {
//public:
	extern Uint32 InitFlags;
	extern Uint32 ThreadCount;
	extern Uint32 FontHinting;
	extern Uint32 VideoBufFrames;
	extern Uint32 AudioBufFrames;
	extern Uint32 SubtitleBufFrames;
	extern void* LibassHandle;
	extern void* AssSharedObjectHandle;

	double GetSystemTime();
	bool AttachmentIsFont(void* p);
};

#endif /* ENGINE_MEDIA_UTILS_MEDIAPLAYERSTATE_H */
