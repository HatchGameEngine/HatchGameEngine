#ifndef ENGINE_MEDIA_UTILS_CODEC_H
#define ENGINE_MEDIA_UTILS_CODEC_H

#define KIT_CODEC_NAME_MAX 8
#define KIT_CODEC_DESC_MAX 48

struct Codec {
	Uint32 Threads; ///< Currently enabled threads (For all
		///< decoders)
	char Name[KIT_CODEC_NAME_MAX]; ///< Codec short name, eg. "ogg"
		///< or "webm"
	char Description[KIT_CODEC_DESC_MAX]; ///< Codec longer, more
		///< descriptive name
};

struct OutputFormat {
	Uint32 Format; ///< SDL Format (SDL_PixelFormat if
		///< video/subtitle, SDL_AudioFormat if audio)
	int IsSigned; ///< Signedness, 1 = signed, 0 = unsigned (if
		///< audio)
	int Bytes; ///< Bytes per sample per channel (if audio)
	int SampleRate; ///< Sampling rate (if audio)
	int Channels; ///< Channels (if audio)
	int Width; ///< Width in pixels (if video)
	int Height; ///< Height in pixels (if video)
};

struct PlayerStreamInfo {
	struct Codec Codec; ///< Decoder codec information
	struct OutputFormat Output; ///< Information about the output
		///< format
};
struct PlayerInfo {
	PlayerStreamInfo Video; ///< Video stream data
	PlayerStreamInfo Audio; ///< Audio stream data
	PlayerStreamInfo Subtitle; ///< Subtitle stream data
};

#endif /* ENGINE_MEDIA_UTILS_CODEC_H */
