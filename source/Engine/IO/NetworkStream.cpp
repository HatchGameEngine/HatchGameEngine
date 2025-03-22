#include <Engine/IO/NetworkStream.h>
#include <Engine/Includes/StandardSDL2.h>

// TODO: Implement

NetworkStream* NetworkStream::New(const char* filename, Uint32 access) {
	return NULL;
}

bool NetworkStream::IsReadable() {
	return true;
}
bool NetworkStream::IsWritable() {
	return true;
}
bool NetworkStream::MakeReadable(bool readable) {
	return true;
}
bool NetworkStream::MakeWritable(bool writable) {
	return true;
}

void NetworkStream::Close() {
	Stream::Close();
}
void NetworkStream::Seek(Sint64 offset) {}
void NetworkStream::SeekEnd(Sint64 offset) {}
void NetworkStream::Skip(Sint64 offset) {}
size_t NetworkStream::Position() {
	return 0;
}
size_t NetworkStream::Length() {
	return 0;
}
size_t NetworkStream::ReadBytes(void* data, size_t n) {
	return 0;
}
size_t NetworkStream::WriteBytes(void* data, size_t n) {
	return 0;
}
