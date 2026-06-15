#ifndef ENGINE_MEDIA_DECODER_H
#define ENGINE_MEDIA_DECODER_H

#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Media/Includes/AVCodec.h>
#include <Engine/Media/Includes/AVFormat.h>
#include <Engine/Media/MediaSource.h>
#include <Engine/Media/Utils/Codec.h>
#include <Engine/Media/Utils/PtrBuffer.h>

class Decoder {
public:
	bool Successful;
	int StreamIndex;
	double ClockSync;
	double ClockPos;
	PtrBuffer* Buffer[2];
	SDL_mutex* OutputLock;
	AVCodecContext* CodecCtx;
	AVFormatContext* FormatCtx;
	Uint32 Format; // SDL_Format
	int (*DecodeFunc)(void*, AVPacket*);
	void (*CloseFunc)(void*);

	static void FreeInVideoPacketFunc(void* packet);
	void Create(MediaSource* src,
		int stream_index,
		int outBufferLength,
		void (*freeOutFunc)(void*),
		int thread_count);
	void Close();
	~Decoder();
	int Run();
	int GetCodecInfo(Codec* codec);
	int GetOutputFormat(OutputFormat* output);
	int GetStreamIndex();
	void SetClockSync(double sync);
	void ChangeClockSync(double sync);
	int WriteInput(AVPacket* packet);
	AVPacket* PeekInput();
	AVPacket* ReadInput();
	bool CanWriteInput();
	void AdvanceInput();
	void ClearInput();
	int WriteOutput(void* packet);
	void* PeekOutput();
	void* ReadOutput();
	bool CanWriteOutput();
	void AdvanceOutput();
	void ClearOutput();
	void ForEachOutput(void (*cb)(void*, void*), void* userdata);
	Uint32 GetInputLength();
	Uint32 GetOutputLength();
	void ClearBuffers();
	int LockOutput();
	void UnlockOutput();
};

#endif /* ENGINE_MEDIA_DECODER_H */
