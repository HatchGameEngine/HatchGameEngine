#include <Engine/IO/ResourceStream.h>

#include <Engine/ResourceTypes/ResourceManager.h>

ResourceStream* ResourceStream::New(const char* filename) {
	ResourceStream* stream = new (std::nothrow) ResourceStream;
	if (!stream) {
		return NULL;
	}

	if (!filename) {
		goto FREE;
	}

	if (!ResourceManager::LoadResource(filename, &stream->pointer_start, &stream->size)) {
		goto FREE;
	}

	stream->pointer = stream->pointer_start;

	return stream;

FREE:
	delete stream;
	return NULL;
}

bool ResourceStream::IsReadable() {
	return true;
}
bool ResourceStream::IsWritable() {
	return false;
}
bool ResourceStream::MakeReadable(bool readable) {
	return readable;
}
bool ResourceStream::MakeWritable(bool writable) {
	return !writable;
}

void ResourceStream::Close() {
	Memory::Free(pointer_start);
	Stream::Close();
}
void ResourceStream::Seek(Sint64 offset) {
	pointer = pointer_start + offset;
}
void ResourceStream::SeekEnd(Sint64 offset) {
	pointer = pointer_start + size + offset;
}
void ResourceStream::Skip(Sint64 offset) {
	pointer = pointer + offset;
}
size_t ResourceStream::Position() {
	return pointer - pointer_start;
}
size_t ResourceStream::Length() {
	return size;
}

size_t ResourceStream::ReadBytes(void* data, size_t n) {
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
size_t ResourceStream::WriteBytes(void* data, size_t n) {
	// Cannot write to a resource.
	return 0;
}
