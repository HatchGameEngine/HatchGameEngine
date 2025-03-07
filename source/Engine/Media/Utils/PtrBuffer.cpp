#include <Engine/Media/Utils/PtrBuffer.h>

#include <Engine/Diagnostics/Log.h>

PtrBuffer::PtrBuffer(Uint32 size, void (*freeFunc)(void*)) {
	this->ReadPtr = 0;
	this->WritePtr = 0;
	this->Size = size;
	this->FreeFunc = freeFunc;

	this->Data = (void**)calloc(this->Size, sizeof(void*));
	if (this->Data == NULL) {
		Log::Print(Log::LOG_ERROR,
			"Something went horribly wrong. (Ran out of memory at PtrBuffer::PtrBuffer)");
		exit(-1);
	}
}
PtrBuffer::~PtrBuffer() {
	Clear();
	free(this->Data);
}

Uint32 PtrBuffer::GetLength() {
	return this->WritePtr - this->ReadPtr;
}
void PtrBuffer::Clear() {
	if (FreeFunc == NULL) {
		return;
	}

	void* data;
	while ((data = Read()) != NULL) {
		FreeFunc(data);
	}
}
void* PtrBuffer::Read() {
	if (ReadPtr < WritePtr) {
		void* out = Data[ReadPtr % Size];
		Data[ReadPtr % Size] = NULL;
		ReadPtr++;
		if (ReadPtr >= Size) {
			ReadPtr = ReadPtr % Size;
			WritePtr = WritePtr % Size;
		}
		return out;
	}
	return NULL;
}
void* PtrBuffer::Peek() {
	if (ReadPtr < WritePtr) {
		return Data[ReadPtr % Size];
	}
	return NULL;
}
void PtrBuffer::Advance() {
	if (ReadPtr < WritePtr) {
		Data[ReadPtr % Size] = NULL;
		ReadPtr++;
		if (ReadPtr >= Size) {
			ReadPtr = ReadPtr % Size;
			WritePtr = WritePtr % Size;
		}
	}
}
int PtrBuffer::Write(void* ptr) {
	if (!ptr) {
		Log::Print(Log::LOG_ERROR, "PtrBuffer::Write: ptr == NULL");
		exit(-1);
	}

	if (!IsFull()) {
		Data[WritePtr % Size] = ptr;
		WritePtr++;
		return 0;
	}
	return 1;
}
void PtrBuffer::ForEachItemInBuffer(void (*callback)(void*, void*), void* userdata) {
	Uint32 read_p = ReadPtr;
	Uint32 write_p = WritePtr;
	while (read_p < write_p) {
		callback(Data[read_p++ % Size], userdata);
		if (read_p >= Size) {
			read_p = read_p % Size;
			write_p = write_p % Size;
		}
	}
}
void PtrBuffer::WithEachItemInBuffer(std::function<void(void*, void*)> callback, void* userdata) {
	Uint32 read_p = ReadPtr;
	Uint32 write_p = WritePtr;
	while (read_p < write_p) {
		callback(Data[read_p++ % Size], userdata);
		if (read_p >= Size) {
			read_p = read_p % Size;
			write_p = write_p % Size;
		}
	}
}
int PtrBuffer::IsFull() {
	int len = WritePtr - ReadPtr;
	int k = (len >= (int)Size);
	return k;
}
