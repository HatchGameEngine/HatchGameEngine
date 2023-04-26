#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>

#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class AudioPlayback {
public:
    Uint8*           Buffer = NULL;
    Uint8*           UnconvertedSampleBuffer = NULL;
    Uint32           BufferedSamples = 0;
    SDL_AudioSpec    Format;
    int              BytesPerSample = 0;
    int              RequiredSamples = 0;
    int              DeviceBytesPerSample = 0;
    SDL_AudioStream* ConversionStream = NULL;
    SoundFormat*     SoundData = NULL;
    Sint32           LoopIndex = -1;
};
#endif

#include <Engine/Audio/AudioPlayback.h>
#include <Engine/Audio/AudioManager.h>

PUBLIC      AudioPlayback::AudioPlayback(SDL_AudioSpec format, int requiredSamples, int audioBytesPerSample, int deviceBytesPerSample) {
    Format = format;
    RequiredSamples = requiredSamples;
    BytesPerSample = audioBytesPerSample;
    DeviceBytesPerSample = deviceBytesPerSample;

    // Create sample buffers
    Buffer = (Uint8*)Memory::TrackedMalloc("Playback::Buffer", requiredSamples * deviceBytesPerSample);
    UnconvertedSampleBuffer = (Uint8*)Memory::TrackedMalloc("Playback::UnconvertedSampleBuffer", requiredSamples * audioBytesPerSample);

    // Create sound conversion stream
    CreateConversionStream(format);
}

PUBLIC void AudioPlayback::Change(SDL_AudioSpec format, int requiredSamples, int audioBytesPerSample, int deviceBytesPerSample) {
    bool formatChanged = format.format != Format.format || format.channels != Format.channels || format.freq != Format.freq;

    Format = format;
    RequiredSamples = requiredSamples;
    BytesPerSample = audioBytesPerSample;
    DeviceBytesPerSample = deviceBytesPerSample;

    int bufSize = requiredSamples * deviceBytesPerSample;
    if (bufSize > RequiredSamples * DeviceBytesPerSample)
        Buffer = (Uint8*)Memory::Realloc(Buffer, requiredSamples * deviceBytesPerSample);

    bufSize = requiredSamples * audioBytesPerSample;
    if (bufSize > RequiredSamples * BytesPerSample)
        UnconvertedSampleBuffer = (Uint8*)Memory::Realloc(UnconvertedSampleBuffer, bufSize);

    if (formatChanged) {
        if (ConversionStream)
            SDL_FreeAudioStream(ConversionStream);
        CreateConversionStream(format);
    }
}

PRIVATE void AudioPlayback::CreateConversionStream(SDL_AudioSpec format) {
    ConversionStream = SDL_NewAudioStream(Format.format, Format.channels, Format.freq, AudioManager::DeviceFormat.format, AudioManager::DeviceFormat.channels, AudioManager::DeviceFormat.freq);
    if (ConversionStream == NULL) {
        Log::Print(Log::LOG_ERROR, "Conversion stream failed to create: %s", SDL_GetError());
        Log::Print(Log::LOG_INFO, "Source Format:");
        Log::Print(Log::LOG_INFO, "Format:   %04X", Format.format);
        Log::Print(Log::LOG_INFO, "Channels: %d", Format.channels);
        Log::Print(Log::LOG_INFO, "Freq:     %d", Format.freq);
        Log::Print(Log::LOG_INFO, "Device Format:");
        Log::Print(Log::LOG_INFO, "Format:   %04X", AudioManager::DeviceFormat.format);
        Log::Print(Log::LOG_INFO, "Channels: %d", AudioManager::DeviceFormat.channels);
        Log::Print(Log::LOG_INFO, "Freq:     %d", AudioManager::DeviceFormat.freq);
    }
}

PUBLIC void AudioPlayback::Dispose() {
    if (Buffer) {
        Memory::Free(Buffer);
        Buffer = NULL;
    }
    if (UnconvertedSampleBuffer) {
        Memory::Free(UnconvertedSampleBuffer);
        UnconvertedSampleBuffer = NULL;
    }
    if (ConversionStream) {
        SDL_FreeAudioStream(ConversionStream);
        ConversionStream = NULL;
    }

    if (SoundData) {
        SoundData->Dispose();
        delete SoundData;
        SoundData = NULL;
    }
}

PUBLIC int AudioPlayback::RequestSamples(int samples, bool loop, int sample_to_loop_to) {
    if (!SoundData)
        return AudioManager::REQUEST_ERROR;

    // If the format is the same, no need to convert.
    if (Format.freq == AudioManager::DeviceFormat.freq
    && Format.format == AudioManager::DeviceFormat.format
    && Format.channels == AudioManager::DeviceFormat.channels) {
        int num_samples = SoundData->GetSamples(Buffer, samples, LoopIndex);
        if (num_samples == 0 && loop) {
            SoundData->SeekSample(sample_to_loop_to);
            num_samples = SoundData->GetSamples(Buffer, samples, LoopIndex);
        }

        if (num_samples == 0)
            return AudioManager::REQUEST_EOF;

        BufferedSamples = num_samples;

        num_samples *= SoundData->SampleSize;
        return num_samples;
    }

    if (!ConversionStream)
        return AudioManager::REQUEST_ERROR;

    int samplesRequestedInBytes = AudioManager::BytesPerSample * samples;

    int availableBytes = SDL_AudioStreamAvailable(ConversionStream);
    if (availableBytes < samplesRequestedInBytes) {
        // Load extra samples if we have none
        int num_samples = SoundData->GetSamples(UnconvertedSampleBuffer, samples * AUDIO_FIRST_LOAD_SAMPLE_BOOST, LoopIndex);
        if (num_samples == 0 && loop) {
            SoundData->SeekSample(sample_to_loop_to);
            num_samples = SoundData->GetSamples(UnconvertedSampleBuffer, samples, LoopIndex);
        }

        if (num_samples == 0) {
            if (availableBytes == 0)
                return AudioManager::REQUEST_EOF;
            else
                goto CONVERT;
        }

        int result = SDL_AudioStreamPut(ConversionStream, UnconvertedSampleBuffer, num_samples * BytesPerSample);
        if (result == -1) {
            Log::Print(Log::LOG_ERROR, "Failed to put samples in conversion stream: %s", SDL_GetError());
            return AudioManager::REQUEST_ERROR;
        }
    }

    CONVERT:
    int received_bytes = SDL_AudioStreamGet(ConversionStream, Buffer, samplesRequestedInBytes);
    if (received_bytes == -1) {
        Log::Print(Log::LOG_ERROR, "Failed to get converted samples: %s", SDL_GetError());
        return AudioManager::REQUEST_ERROR;
    }
    // Wait for data if we got none
    if (received_bytes == 0)
        return AudioManager::REQUEST_CONVERTING;

    BufferedSamples = received_bytes / AudioManager::BytesPerSample;

    return received_bytes;
}

PUBLIC void AudioPlayback::Seek(int samples) {
    if (!SoundData)
        return;

    SoundData->SeekSample(samples);
}

PUBLIC AudioPlayback::~AudioPlayback() {
    Dispose();
}
