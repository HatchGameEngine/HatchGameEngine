#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/Application.h>

#include <Engine/Audio/AudioPlayback.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

class ISound {
public:
    SDL_AudioSpec     Format;
    int               BytesPerSample;

    SoundFormat*      SoundData = NULL;

    char              Filename[256];
    bool              LoadFailed = false;
    bool              StreamFromFile = false;
};
#endif

#include <Engine/ResourceTypes/ISound.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Audio/AudioIncludes.h>
#include <Engine/Utilities/StringUtils.h>
// Import sound formats
#include <Engine/ResourceTypes/SoundFormats/OGG.h>
#include <Engine/ResourceTypes/SoundFormats/WAV.h>

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

    BytesPerSample = ((Format.format & 0xFF) >> 3) * Format.channels;
    LoadFailed = false;
}

PUBLIC AudioPlayback* ISound::CreatePlayer() {
    int requiredSamples = AudioManager::DeviceFormat.samples * AUDIO_FIRST_LOAD_SAMPLE_BOOST;

    AudioPlayback* playback = new AudioPlayback(Format, requiredSamples, BytesPerSample, AudioManager::BytesPerSample);
    playback->SoundData = SoundData;
    playback->OwnsSoundData = false;

    return playback;
}

PUBLIC void ISound::Dispose() {

}
