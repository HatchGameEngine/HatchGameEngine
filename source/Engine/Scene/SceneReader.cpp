#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Scene/SceneReader.h>

bool SceneReader::Read(Stream* stream) {
	throw std::runtime_error("SceneReader::Read() called without an implementation");
}

bool SceneReader::Read(const char* filename) {
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
		return false;
	}

	bool result = Read(stream);

	stream->Close();

	return result;
}

bool SceneReader::LoadIntoScene(const char* parentFolder) {
	throw std::runtime_error("SceneReader::LoadIntoScene() called without an implementation");
}

void SceneReader::Dispose() {}

SceneReader::~SceneReader() {
	Dispose();
}
