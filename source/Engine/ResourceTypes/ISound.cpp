#include <Engine/Audio/AudioIncludes.h>
#include <Engine/Audio/AudioManager.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/Utilities/StringUtils.h>
// Import sound formats
#include <Engine/ResourceTypes/SoundFormats/OGG.h>
#include <Engine/ResourceTypes/SoundFormats/WAV.h>

ISound::ISound(const char* filename) {
	ISound::Load(filename, true);
}
ISound::ISound(const char* filename, bool streamFromFile) {
	ISound::Load(filename, streamFromFile);
}

Uint8 ISound::DetectFormat(Stream* stream) {
	Uint8 magic[12] = {0};
	stream->ReadBytes(magic, sizeof magic);

	// OGG
	if (memcmp(magic, "\x4F\x67\x67\x53", 4) == 0) {
		return AUDIO_FORMAT_OGG;
	}
	// WAV
	else if (memcmp(magic, "RIFF", 4) == 0 && memcmp(magic + 8, "WAVE", 4) == 0) {
		return AUDIO_FORMAT_WAV;
	}

	return AUDIO_FORMAT_UNKNOWN;
}
bool ISound::IsFile(Stream* stream) {
	return DetectFormat(stream) != AUDIO_FORMAT_UNKNOWN;
}

void ISound::Load(const char* filename, bool streamFromFile) {
	LoadFailed = true;
	StreamFromFile = streamFromFile;
	Filename = StringUtils::NormalizePath(filename);

	double ticks = Clock::GetTicks();

	Uint8 format = AUDIO_FORMAT_UNKNOWN;
	Stream* stream = ResourceStream::New(Filename);
	if (stream) {
		format = DetectFormat(stream);
		stream->Seek(0);
	}
	else {
		return;
	}

	// .OGG format
	if (format == AUDIO_FORMAT_OGG) {
		ticks = Clock::GetTicks();

		SoundData = OGG::Load(stream);
		if (!SoundData) {
			return;
		}

		Format = SoundData->InputFormat;

		LoopPoint = SoundData->LoopPoint;

		Log::Print(Log::LOG_VERBOSE,
			"OGG load took %.3f ms (%s)",
			Clock::GetTicks() - ticks,
			Filename);
	}
	// .WAV format
	else if (format == AUDIO_FORMAT_WAV) {
		ticks = Clock::GetTicks();

		SoundData = WAV::Load(stream);
		if (!SoundData) {
			return;
		}

		Format = SoundData->InputFormat;

		Log::Print(Log::LOG_VERBOSE,
			"WAV load took %.3f ms (%s)",
			Clock::GetTicks() - ticks,
			Filename);
	}
	// Unsupported format
	else {
		stream->Close();
		Log::Print(Log::LOG_ERROR, "Unsupported audio format for file \"%s\"!", Filename);
		return;
	}

	// If we're not streaming, then load all samples now
	if (!StreamFromFile) {
		ticks = Clock::GetTicks();

		SoundData->LoadSamples(SoundData->TotalPossibleSamples);
		SoundData->Close();

		Log::Print(Log::LOG_VERBOSE,
			"Full sample load took %.3f ms",
			Clock::GetTicks() - ticks);
	}

	BytesPerSample = ((Format.format & 0xFF) >> 3) * Format.channels;
	LoadFailed = false;
}

AudioPlayback* ISound::CreatePlayer() {
	int requiredSamples = AudioManager::DeviceFormat.samples * AUDIO_FIRST_LOAD_SAMPLE_BOOST;

	AudioPlayback* playback = new AudioPlayback(
		Format, requiredSamples, BytesPerSample, AudioManager::BytesPerSample);
	playback->SoundData = SoundData;
	playback->OwnsSoundData = false;

	return playback;
}

void ISound::Dispose() {
	if (SoundData) {
		SoundData->Dispose();
		delete SoundData;
		SoundData = nullptr;
	}

	if (Filename) {
		Memory::Free(Filename);
		Filename = nullptr;
	}
}
