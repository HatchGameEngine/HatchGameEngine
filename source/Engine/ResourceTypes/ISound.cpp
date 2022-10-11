#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>

#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class ISound {
public:
    SDL_AudioSpec     Format;
    int               BytesPerSample;

    Uint8*            Buffer = NULL;
    Uint8*            UnconvertedSampleBuffer = NULL;
    Uint32            BufferedSamples = 0;

    char              Filename[256];
    SDL_AudioStream*  Stream = NULL;
    bool              LoadFailed = false;
    bool              StreamFromFile = false;

    SoundFormat*      SoundData = NULL;
};
#endif

#include <Engine/ResourceTypes/ISound.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
// Import sound formats
#include <Engine/ResourceTypes/SoundFormats/OGG.h>
#include <Engine/ResourceTypes/SoundFormats/WAV.h>
#include <Engine/Utilities/StringUtils.h>

#define FIRST_LOAD_SAMPLE_BOOST 4

PUBLIC      ISound::ISound(const char* filename) {
    ISound::Load(filename, true);
}
PUBLIC      ISound::ISound(const char* filename, bool streamFromFile) {
    ISound::Load(filename, streamFromFile);
}

PUBLIC void ISound::Load(const char* filename, bool streamFromFile) {
    LoadFailed = true;
    StreamFromFile = streamFromFile;
    strcpy(Filename, filename);

    double ticks = Clock::GetTicks();

    // .OGG format
    if (StringUtils::StrCaseStr(Filename, ".ogg")) {
        ticks = Clock::GetTicks();

        SoundData = OGG::Load(Filename);
        if (!SoundData)
            return;

        Format = SoundData->InputFormat;

        Log::Print(Log::LOG_VERBOSE, "OGG load took %.3f ms", Clock::GetTicks() - ticks);
    }
    // .WAV format
    else if (StringUtils::StrCaseStr(Filename, ".wav")) {
        ticks = Clock::GetTicks();

        SoundData = WAV::Load(Filename);
        if (!SoundData)
            return;

        Format = SoundData->InputFormat;

        Log::Print(Log::LOG_VERBOSE, "WAV load took %.3f ms", Clock::GetTicks() - ticks);
    }
    // Unsupported format
    else {
        Log::Print(Log::LOG_ERROR, "Unsupported audio format from file \"%s\"!", Filename);
        return;
    }

    // If we're not streaming, then load all samples now
    if (!StreamFromFile) {
        ticks = Clock::GetTicks();

        SoundData->LoadSamples(SoundData->TotalPossibleSamples);
        SoundData->Close();

        Log::Print(Log::LOG_VERBOSE, "Full sample load took %.3f ms", Clock::GetTicks() - ticks);
    }

    // Create sample buffers
    int requiredSamples, deviceBytesPerSample;

    requiredSamples      = AudioManager::DeviceFormat.samples * FIRST_LOAD_SAMPLE_BOOST;

    BytesPerSample       = ((Format.format & 0xFF) >> 3) * Format.channels;
    deviceBytesPerSample = AudioManager::BytesPerSample;

    Buffer = (Uint8*)Memory::TrackedMalloc("ISound::Buffer", requiredSamples * deviceBytesPerSample);
    UnconvertedSampleBuffer = (Uint8*)Memory::TrackedMalloc("ISound::UnconvertedSampleBuffer", requiredSamples * BytesPerSample);

    // Create sound conversion stream
    Stream = SDL_NewAudioStream(Format.format, Format.channels, Format.freq, AudioManager::DeviceFormat.format, AudioManager::DeviceFormat.channels, AudioManager::DeviceFormat.freq);
    if (Stream == NULL) {
        Log::Print(Log::LOG_ERROR, "Conversion stream failed to create: %s", SDL_GetError());
        Log::Print(Log::LOG_INFO, "Source Format:");
        Log::Print(Log::LOG_INFO, "Format:   %04X", Format.format);
        Log::Print(Log::LOG_INFO, "Channels: %d", Format.channels);
        Log::Print(Log::LOG_INFO, "Freq:     %d", Format.freq);
        Log::Print(Log::LOG_INFO, "Device Format:");
        Log::Print(Log::LOG_INFO, "Format:   %04X", AudioManager::DeviceFormat.format);
        Log::Print(Log::LOG_INFO, "Channels: %d", AudioManager::DeviceFormat.channels);
        Log::Print(Log::LOG_INFO, "Freq:     %d", AudioManager::DeviceFormat.freq);
		return;
    }

    LoadFailed = false;
}

PUBLIC int  ISound::RequestSamples(int samples, bool loop, int sample_to_loop_to) {
    if (!SoundData)
        return AudioManager::REQUEST_ERROR;

    int bytesPerSample = AudioManager::BytesPerSample;

    // If the format is the same, no need to convert.
    if (Format.freq == AudioManager::DeviceFormat.freq &&
        Format.format == AudioManager::DeviceFormat.format &&
        Format.channels == AudioManager::DeviceFormat.channels) {
        int num_samples = SoundData->GetSamples(Buffer, samples);

        if (num_samples == 0 && loop) {
            SoundData->SeekSample(sample_to_loop_to);
            num_samples = SoundData->GetSamples(Buffer, samples);
        }

        if (num_samples == 0)
            return AudioManager::REQUEST_EOF;

        BufferedSamples = num_samples;

        num_samples *= SoundData->SampleSize;
        return num_samples;
    }

    if (!Stream)
        return AudioManager::REQUEST_ERROR;

    int samplesRequestedInBytes = bytesPerSample * samples;

    int availableBytes = SDL_AudioStreamAvailable(Stream);
	if (availableBytes < samplesRequestedInBytes) {
        int num_samples = SoundData->GetSamples(UnconvertedSampleBuffer, samples * FIRST_LOAD_SAMPLE_BOOST); // Load extra samples if we have none

        if (num_samples == 0 && loop) {
            SoundData->SeekSample(sample_to_loop_to);
            num_samples = SoundData->GetSamples(UnconvertedSampleBuffer, samples);
        }

        if (num_samples == 0) {
            if (availableBytes == 0)
                return AudioManager::REQUEST_EOF;
            else
                goto CONVERT;
        }

        int result = SDL_AudioStreamPut(Stream, UnconvertedSampleBuffer, num_samples * BytesPerSample);
        if (result == -1) {
            Log::Print(Log::LOG_ERROR, "Failed to put samples in conversion stream: %s", SDL_GetError());
            return AudioManager::REQUEST_ERROR;
        }
    }

    CONVERT:
    int received_bytes = SDL_AudioStreamGet(Stream, Buffer, samplesRequestedInBytes);
    if (received_bytes == -1) {
        Log::Print(Log::LOG_ERROR, "Failed to get converted samples: %s", SDL_GetError());
        return AudioManager::REQUEST_ERROR;
    }
    // Wait for data if we got none
    if (received_bytes == 0)
        return AudioManager::REQUEST_CONVERTING;

    BufferedSamples = received_bytes / bytesPerSample;

    return received_bytes;
}
PUBLIC void ISound::Seek(int samples) {
    if (!SoundData)
        return;

    SoundData->SeekSample(samples);
}

PUBLIC void ISound::Dispose() {
    if (Buffer) {
        Memory::Free(Buffer);
        Buffer = NULL;
    }
    if (UnconvertedSampleBuffer) {
        Memory::Free(UnconvertedSampleBuffer);
        UnconvertedSampleBuffer = NULL;
    }
    if (Stream) {
        SDL_FreeAudioStream(Stream);
        Stream = NULL;
    }

    if (SoundData) {
        SoundData->Dispose();
        delete SoundData;
        SoundData = NULL;
    }
}
