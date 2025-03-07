#ifndef ENGINE_MEDIA_DECODERS_VIDEODECODER_H
#define ENGINE_MEDIA_DECODERS_VIDEODECODER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Media/Decoder.h>
#include <Engine/Media/Includes/SWScale.h>
#include <Engine/Rendering/Texture.h>

class VideoDecoder : public Decoder {
public:
	SwsContext* SWS;
	AVFrame* ScratchFrame;
	int Width;
	int Height;

	VideoDecoder(MediaSource* src, int stream_index);
	void* CreateVideoPacket(AVFrame* frame, double pts);
	static void FreeVideoPacket(void* p);
	AVPixelFormat FindAVPixelFormat(Uint32 format);
	int FindSDLPixelFormat(AVPixelFormat fmt);
	int GetOutputFormat(OutputFormat* output);
	static void ReadVideo(void* ptr);
	static int DecodeFunction(void* ptr, AVPacket* in_packet);
	static void CloseFunction(void* ptr);
	double GetPTS();
	int GetVideoDecoderData(Texture* texture);
};

#endif /* ENGINE_MEDIA_DECODERS_VIDEODECODER_H */
