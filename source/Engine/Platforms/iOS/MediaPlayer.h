#ifndef ENGINE_IO_IOS_MEDIAPLAYER
#define ENGINE_IO_IOS_MEDIAPLAYER

extern int iOS_InitMediaPlayer();
extern int iOS_UpdateMediaPlayer(const char* title,
	double playbackPosition,
	double playbackDuration,
	double playbackRate);
extern int iOS_ClearMediaPlayer();
extern int iOS_DisposeMediaPlayer();

#endif /* ENGINE_IO_IOS_MEDIAPLAYER */
