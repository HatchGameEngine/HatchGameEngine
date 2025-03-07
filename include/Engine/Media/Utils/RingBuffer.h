#ifndef ENGINE_MEDIA_UTILS_RINGBUFFER_H
#define ENGINE_MEDIA_UTILS_RINGBUFFER_H

#include <Engine/Includes/Standard.h>

class RingBuffer {
public:
	int Size;
	int Length;
	int WritePos;
	int ReadPos;
	char* Data;

	RingBuffer(Uint32 size);
	~RingBuffer();
	int Write(const char* data, int len);
	void ReadData(char* data, const int len);
	int Read(char* data, int len);
	int Peek(char* data, int len);
	int Advance(int len);
	int GetLength();
	int GetSize();
	int GetFree();
};

#endif /* ENGINE_MEDIA_UTILS_RINGBUFFER_H */
