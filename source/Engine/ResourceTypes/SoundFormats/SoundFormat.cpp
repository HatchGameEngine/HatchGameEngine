#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

#include <Engine/Application.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>

int SoundFormat::LoadSamples(size_t count) {
	return 0;
}
int SoundFormat::GetSamples(Uint8* buffer, size_t count, Sint32 loopIndex) {
	if (SampleIndex >= Samples.size()) {
		if (LoadSamples(count) == 0) { // If we've reached end of file
			return 0;
		}
	}

	if (count > Samples.size() - SampleIndex) {
		count = Samples.size() - SampleIndex;
	}

	size_t samplecount = 0;
	for (size_t i = SampleIndex; i < SampleIndex + count && i < Samples.size(); i++) {
		memcpy(buffer, Samples[i], SampleSize);
		buffer += SampleSize;
		samplecount++;
	}
	SampleIndex += samplecount;
	return (int)samplecount;
}
size_t SoundFormat::SeekSample(int index) {
	SampleIndex = (size_t)index;
	return SampleIndex;
}
size_t SoundFormat::TellSample() {
	return SampleIndex;
}
void SoundFormat::LoadAllSamples() {
	LoadSamples(TotalPossibleSamples - Samples.size());
}

void SoundFormat::CopySamples(SoundFormat* dest) {
	// Load the entire sound
	if (Samples.size() < TotalPossibleSamples) {
		LoadAllSamples();
		Close();
	}

	// The destination SoundData's Samples are the same as the
	// source SoundData's Samples. Why? Because Samples is just a
	// list of pointers to the sample buffer, which the destination
	// SoundData does not need to own a copy of. So as long as the
	// source SampleBuffer hasn't been freed, this is completely
	// fine to do.
	dest->Samples = Samples;
	dest->SampleSize = SampleSize;
	dest->TotalPossibleSamples = TotalPossibleSamples;
	dest->InputFormat = InputFormat;
}

double SoundFormat::GetPosition() {
	return (double)SampleIndex / InputFormat.freq;
}
double SoundFormat::SetPosition(double seconds) {
	SampleIndex = (size_t)(seconds * InputFormat.freq);
	return GetPosition();
}
double SoundFormat::GetDuration() {
	return (double)TotalPossibleSamples / InputFormat.freq;
}

void SoundFormat::LoadFinish() {
	SampleIndex = 0;
	SampleSize = ((InputFormat.format & 0xFF) >> 3) * InputFormat.channels;
	SampleBuffer = NULL;
}

SoundFormat::~SoundFormat() {
	// Do not add anything to a base class' destructor.
}

void SoundFormat::Close() {
	if (StreamPtr) {
		StreamPtr->Close();
		StreamPtr = NULL;
	}
}
void SoundFormat::Dispose() {
	Samples.clear();
	Samples.shrink_to_fit();

	if (SampleBuffer) {
		Memory::Free(SampleBuffer);
		SampleBuffer = NULL;
	}

	if (StreamPtr) {
		StreamPtr->Close();
		StreamPtr = NULL;
	}
}
