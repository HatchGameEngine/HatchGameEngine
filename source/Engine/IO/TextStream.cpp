#include <Engine/IO/TextStream.h>

TextStream* TextStream::New(Stream* other) {
	if (other == nullptr) {
		return nullptr;
	}

	TextStream* stream = new (std::nothrow) TextStream;
	if (!stream) {
		return nullptr;
	}

	size_t source_length = other->Length() + 1;

	Uint8* data = (Uint8*)Memory::Calloc(source_length, sizeof(Uint8));
	if (!data) {
		goto FREE;
	}

	stream->pointer_start = data;
	stream->pointer = stream->pointer_start;
	stream->size = source_length;

	other->CopyTo(stream);
	stream->Seek(0);

	return stream;

FREE:
	delete stream;
	return nullptr;
}
TextStream* TextStream::New(const char* text) {
	if (text == nullptr) {
		return nullptr;
	}

	TextStream* stream = new (std::nothrow) TextStream;
	if (!stream) {
		return nullptr;
	}

	size_t source_length = strlen(text) + 1;

	Uint8* data = (Uint8*)Memory::Calloc(source_length, sizeof(Uint8));
	if (!data) {
		goto FREE;
	}

	stream->pointer_start = data;
	stream->pointer = stream->pointer_start;
	stream->size = source_length;

	memcpy(data, text, source_length);

	return stream;

FREE:
	delete stream;
	return nullptr;
}
TextStream* TextStream::New(std::string text) {
	return TextStream::New(text.c_str());
}

bool TextStream::IsReadable() {
	return true;
}
bool TextStream::IsWritable() {
	return true;
}
bool TextStream::MakeReadable(bool readable) {
	return true;
}
bool TextStream::MakeWritable(bool writable) {
	return true;
}

void TextStream::Close() {
	if (pointer_start != nullptr) {
		Memory::Free(pointer_start);
		pointer_start = nullptr;
	}
	Stream::Close();
}
void TextStream::Seek(Sint64 offset) {
	pointer = pointer_start + offset;
}
void TextStream::SeekEnd(Sint64 offset) {
	pointer = pointer_start + size + offset;
}
void TextStream::Skip(Sint64 offset) {
	pointer = pointer + offset;
}
size_t TextStream::Position() {
	return pointer - pointer_start;
}
size_t TextStream::Length() {
	return size;
}

size_t TextStream::ReadBytes(void* data, size_t n) {
	if (n > size - Position()) {
		n = size - Position();
	}
	if (n == 0) {
		return 0;
	}

	memcpy(data, pointer, n);
	pointer += n;
	return n;
}
Uint32 TextStream::ReadCompressed(void* out) {
	return 0;
}
Uint32 TextStream::ReadCompressed(void* out, size_t outSz) {
	return 0;
}

size_t TextStream::WriteBytes(void* data, size_t n) {
	if (Position() + n > size) {
		size_t pos = Position();
		pointer_start = (unsigned char*)Memory::Realloc(pointer_start, pos + n);
		pointer = pointer_start + pos;
	}
	memcpy(pointer, data, n);
	pointer += n;
	return n;
}
