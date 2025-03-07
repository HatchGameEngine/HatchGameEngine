#include <Engine/ResourceTypes/SoundFormats/WAV.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/IO/ResourceStream.h>

struct WAVheader {
	Uint32 FMTSize; // Size of the fmt chunk
	Uint16 AudioFormat; // Audio format 1=PCM,6=mulaw,7=alaw,
		// 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
	Uint16 Channels;
	Uint32 Frequency;
	Uint16 BitsPerSample;
	Uint32 DATASize; // Sampled data length
};

SoundFormat* WAV::Load(const char* filename) {
	WAV* wav = NULL;
	class Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "Could not open file '%s'!", filename);
		goto WAV_Load_FAIL;
	}

	wav = new (std::nothrow) WAV;
	if (!wav) {
		goto WAV_Load_FAIL;
	}

	wav->StreamPtr = stream;

	WAVheader header;
	stream->Seek(0x10);
	// fmt Header
	header.FMTSize = stream->ReadUInt32();
	header.AudioFormat = stream->ReadUInt16();
	header.Channels = stream->ReadUInt16();
	header.Frequency = stream->ReadUInt32();
	stream->Skip(0x6);
	header.BitsPerSample = stream->ReadUInt16();
	// data Header
	// 0xC [RIFF chunk] + (FMTSize + 8
	// [sizeof(FMT)+sizeof(FMTSize)]) + 4 [sizeof(DATA)]
	stream->Seek(0xC + (header.FMTSize + 8) + 4);
	header.DATASize = stream->ReadUInt32();

	memset(&wav->InputFormat, 0, sizeof(SDL_AudioSpec));
	wav->InputFormat.format = (header.BitsPerSample & 0xFF);
	wav->InputFormat.channels = header.Channels;
	wav->InputFormat.freq = header.Frequency;
	wav->InputFormat.samples = 4096;

	switch (header.AudioFormat) {
	case 0x0001:
		wav->InputFormat.format |= 0x8000;
		break;
	case 0x0003:
		wav->InputFormat.format |= 0x8000;
		// Allow only 32-bit floats
		if (header.BitsPerSample == 0x20) {
			wav->InputFormat.format |= 0x0100;
		}
		break;
	}

	wav->DataStart = (int)stream->Position();

	wav->TotalPossibleSamples =
		(int)(header.DATASize / (((header.BitsPerSample & 0xFF) >> 3) * header.Channels));
	// stream->Seek(wav->DataStart);

	// Common
	wav->LoadFinish();

	goto WAV_Load_SUCCESS;

WAV_Load_FAIL:
	if (wav) {
		wav->Dispose();
		delete wav;
	}
	wav = NULL;

WAV_Load_SUCCESS:
	return wav;
}

int WAV::LoadSamples(size_t count) {
	size_t read, bytesForSample = 0, total = 0;

	if (StreamPtr == nullptr) {
		return 0;
	}

	if (SampleBuffer == NULL) {
		SampleBuffer = (Uint8*)Memory::TrackedMalloc(
			"SoundData::SampleBuffer", TotalPossibleSamples * SampleSize);
		Samples.reserve(TotalPossibleSamples);
	}

	char* buffer = (char*)SampleBuffer + Samples.size() * SampleSize;
	char* bufferStartSample = buffer;

	if ((size_t)count > TotalPossibleSamples - Samples.size()) {
		count = TotalPossibleSamples - Samples.size();
	}

	StreamPtr->Seek(DataStart + Samples.size() * SampleSize);
	read = StreamPtr->ReadBytes(buffer, count * SampleSize);

	if (read == 0) {
		return 0;
	}

	bytesForSample = read;
	while (bytesForSample >= SampleSize) {
		Samples.push_back((Uint8*)bufferStartSample);
		bufferStartSample += SampleSize;
		bytesForSample -= SampleSize;
		total++;
	}

	return (int)total;
}

void WAV::Dispose() {
	// WAV specific clean up functions
	// Common cleanup
	SoundFormat::Dispose();
}
