#include <Engine/Audio/AudioManager.h>
#include <Engine/Audio/AudioPlayback.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/ResourceTypes/SoundFormats/SoundFormat.h>

SDL_AudioDeviceID AudioManager::Device;
SDL_AudioSpec AudioManager::DeviceFormat;
bool AudioManager::AudioEnabled = false;

Uint8 AudioManager::BytesPerSample;
Uint8* AudioManager::MixBuffer;
size_t AudioManager::MixBufferSize;

deque<AudioChannel*> AudioManager::MusicStack;
AudioChannel* AudioManager::SoundArray = NULL;
int AudioManager::SoundArrayLength = 512;

float AudioManager::MasterVolume = 1.0f;
float AudioManager::MusicVolume = 1.0f;
float AudioManager::SoundVolume = 1.0f;

float AudioManager::LowPassFilter = 0.0f;

Uint8* AudioManager::AudioQueue = NULL;
size_t AudioManager::AudioQueueSize = 0;
size_t AudioManager::AudioQueueMaxSize = 0;

enum {
	FILTER_TYPE_LOW_PASS,
	FILTER_TYPE_HIGH_PASS,
};
double a0 = 0.0;
double a1 = 0.0;
double a2 = 0.0;
double b1 = 0.0;
double b2 = 0.0;
double mQuality = 1.0;
double mGain = 1.0;
double mNormalizedFreq = 1.0 / 64.0; // Good for low pass
int mFilterType = FILTER_TYPE_LOW_PASS;
// double mNormalizedFreq = 1.0 / 8.0;
// int    mFilterType = FILTER_TYPE_HIGH_PASS;
Sint16 mZx[2 * 2]; // 2 per channel
Sint16 mZy[2 * 2]; // 2 per channel
float mZxF[2 * 2]; // 2 per channel
float mZyF[2 * 2]; // 2 per channel

void AudioManager::CalculateCoeffs() {
	double theta = 2.0 * M_PI * mNormalizedFreq; // normalized frequency has been
		// precalculated as fc/fs
	double d = 0.5 * (1.0 / mQuality) * sin(theta);
	double beta = 0.5 * ((1.0 - d) / (1.0 + d));
	double gamma = (0.5 + beta) * cos(theta);

	mZx[0] = 0;
	mZx[1] = 0;
	mZy[0] = 0;
	mZy[1] = 0;

	mZxF[0] = 0;
	mZxF[1] = 0;
	mZyF[0] = 0;
	mZyF[1] = 0;

	if (mFilterType == FILTER_TYPE_LOW_PASS) {
		a0 = 0.5 * (0.5 + beta - gamma);
		a1 = 0.5 + beta - gamma;
	}
	else {
		a0 = 0.5 * (0.5 + beta + gamma);
		a1 = -(0.5 + beta + gamma);
	}

	a2 = a0;
	b1 = -2.0 * gamma;
	b2 = 2.0 * beta;
}
Sint16 AudioManager::ProcessSample(Sint16 inSample, int channel) {
	Sint16 outSample;
	int idx0 = 2 * channel;
	int idx1 = idx0 + 1;

	outSample = (Sint16)((a0 * inSample) + (a1 * mZx[idx0]) + (a2 * mZx[idx1]) -
		(b1 * mZy[idx0]) - (b2 * mZy[idx1]));

	// if (outSample > 0x3FFF)
	//     outSample = 0x3FFF;
	// if (outSample < -0x3FFF)
	//     outSample = -0x3FFF;

	mZx[idx1] = mZx[idx0];
	mZx[idx0] = inSample;
	mZy[idx1] = mZy[idx0];
	mZy[idx0] = outSample;

	return (Sint16)(inSample * (1.0 - AudioManager::LowPassFilter) +
		outSample * mGain * AudioManager::LowPassFilter);
}
float AudioManager::ProcessSampleFloat(float inSample, int channel) {
	float outSample;
	int idx0 = 2 * channel;
	int idx1 = idx0 + 1;

	outSample = (float)((a0 * inSample) + (a1 * mZxF[idx0]) + (a2 * mZxF[idx1]) -
		(b1 * mZyF[idx0]) - (b2 * mZyF[idx1]));

	mZxF[idx1] = mZxF[idx0];
	mZxF[idx0] = inSample;
	mZyF[idx1] = mZyF[idx0];
	mZyF[idx0] = outSample;

	return (float)(inSample * (1.0 - AudioManager::LowPassFilter) +
		outSample * mGain * AudioManager::LowPassFilter);
}

void AudioManager::Init() {
	CalculateCoeffs();

	SoundArray = (AudioChannel*)Memory::Calloc(SoundArrayLength, sizeof(AudioChannel));
	for (int i = 0; i < SoundArrayLength; i++) {
		SoundArray[i].Paused = true;
	}

	SDL_AudioSpec Want;
	memset(&Want, 0, sizeof(Want));

	Want.freq = 44100;
	Want.format = AUDIO_S16SYS;
	Want.samples = 0x800;
	Want.channels = 2;
	Want.callback = AudioManager::AudioCallback;

	// Lower sample mix rate for weaker devices
	switch (Application::Platform) {
	case Platforms::Android:
		Want.samples = 0x200;
		break;
	default:
		break;
	}

	if (Application::Platform != Platforms::Android) {
		if (SDL_OpenAudio(&Want, &DeviceFormat) >= 0) {
			AudioEnabled = true;
			SDL_PauseAudio(0);
			Device = 1;
		}
		else {
			Log::Print(Log::LOG_ERROR, "Could not open audio device!");
		}
	}
	else {
		if ((Device = SDL_OpenAudioDevice(
			     NULL, 0, &Want, &DeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE))) {
			AudioEnabled = true;
			SDL_PauseAudioDevice(Device, 0);
		}
		else {
			Log::Print(Log::LOG_ERROR, "Could not open audio device!");
		}
	}

	Log::Print(Log::LOG_VERBOSE, "%s", "Audio Device:");
	Log::Print(Log::LOG_VERBOSE, "%s = %d", "Freq", DeviceFormat.freq);
	Log::Print(Log::LOG_VERBOSE,
		"%s = %s%d-bit%s",
		"Format",
		SDL_AUDIO_ISSIGNED(DeviceFormat.format) ? "signed " : "unsigned ",
		SDL_AUDIO_BITSIZE(DeviceFormat.format),
		SDL_AUDIO_ISFLOAT(DeviceFormat.format)               ? " (float)"
			: SDL_AUDIO_ISBIGENDIAN(DeviceFormat.format) ? " BE"
								     : " LE");
	Log::Print(Log::LOG_VERBOSE, "%s = %X", "Samples", DeviceFormat.samples);
	Log::Print(Log::LOG_VERBOSE, "%s = %X", "Channels", DeviceFormat.channels);

	BytesPerSample = ((DeviceFormat.format & 0xFF) >> 3) * DeviceFormat.channels;

	if (DeviceFormat.channels == 2) {
		MixBufferSize = DeviceFormat.samples;
		MixBuffer = (Uint8*)Memory::Malloc(MixBufferSize * BytesPerSample);
	}

	// AudioQueueMaxSize = DeviceFormat.samples *
	// DeviceFormat.channels *
	// (SDL_AUDIO_BITSIZE(DeviceFormat.format) >> 3);
	AudioQueueMaxSize = 0x1000;
	AudioQueueSize = 0;
	AudioQueue = (Uint8*)Memory::Calloc(8, AudioQueueMaxSize);
}

void AudioManager::ClampParams(float& pan, float& speed, float& volume) {
	if (pan < -1.0f) {
		pan = -1.0f;
	}
	else if (pan > 1.0f) {
		pan = 1.0f;
	}

	if (speed < 0.0f) {
		speed = 0.0f;
	}

	if (volume > 1.0f) {
		volume = 1.0f;
	}
	else if (volume < 0.0f) {
		volume = 0.0f;
	}
}

void AudioManager::UpdateChannelPlayer(AudioPlayback* playback, ISound* sound) {
	if (!playback->SoundData) {
		playback->SoundData = new SoundFormat;
		playback->OwnsSoundData = true;
	}

	SoundFormat* srcSndData = sound->SoundData;
	if (srcSndData) {
		srcSndData->CopySamples(playback->SoundData);
	}
	else {
		playback->SoundData->Samples.clear();
		playback->SoundData->Samples.shrink_to_fit();
	}

	playback->SoundData->SampleIndex = 0;
}

void AudioManager::SetSound(int channel, ISound* music) {
	AudioManager::SetSound(channel, music, false, 0, 0.0f, 1.0f, 1.0f, nullptr);
}
void AudioManager::SetSound(int channel,
	ISound* sound,
	bool loop,
	int loopPoint,
	float pan,
	float speed,
	float volume,
	void* origin) {
	AudioManager::Lock();

	AudioChannel* audio = &SoundArray[channel];
	AudioPlayback* playback = audio->Playback;

	AudioManager::ClampParams(pan, speed, volume);

	audio->Audio = sound;
	audio->Stopped = false;
	audio->Paused = false;
	audio->Origin = origin;
	audio->Loop = loop;
	audio->LoopPoint = loopPoint;
	audio->Fading = MusicFade_None;
	audio->Pan = pan;
	audio->Speed = (Uint32)(speed * 0x10000);
	audio->Volume = volume;

	int requiredSamples = AudioManager::DeviceFormat.samples * AUDIO_FIRST_LOAD_SAMPLE_BOOST;
	if (playback == nullptr) {
		playback = new AudioPlayback(sound->Format,
			requiredSamples,
			sound->BytesPerSample,
			AudioManager::BytesPerSample);
		audio->Playback = playback;
	}
	else {
		playback->Change(sound->Format,
			requiredSamples,
			sound->BytesPerSample,
			AudioManager::BytesPerSample);
	}

	AudioManager::UpdateChannelPlayer(playback, sound);

	playback->Seek(0);
	if (playback->ConversionStream) {
		SDL_AudioStreamClear(playback->ConversionStream);
	}

	AudioManager::Unlock();
}
int AudioManager::PlaySound(ISound* music) {
	return AudioManager::PlaySound(music, false, 0, 0.0f, 1.0f, 1.0f, nullptr);
}
int AudioManager::PlaySound(ISound* music,
	bool loop,
	int loopPoint,
	float pan,
	float speed,
	float volume,
	void* origin) {
	for (int i = 0; i < SoundArrayLength; i++) {
		AudioChannel* audio = &SoundArray[i];
		if (!audio->Audio || audio->Stopped) {
			AudioManager::SetSound(
				i, music, loop, loopPoint, pan, speed, volume, origin);
			return i;
		}
	}

	return -1;
}

void AudioManager::PushMusic(ISound* music,
	bool loop,
	Uint32 lp,
	float pan,
	float speed,
	float volume,
	double fadeInAfterFinished) {
	PushMusicAt(music, 0.0, loop, lp, pan, speed, volume, fadeInAfterFinished);
}
void AudioManager::PushMusicAt(ISound* music,
	double at,
	bool loop,
	Uint32 lp,
	float pan,
	float speed,
	float volume,
	double fadeInAfterFinished) {
	if (music->LoadFailed) {
		return;
	}

	AudioManager::Lock();

	if (MusicStack.size() > 0 && fadeInAfterFinished > 0.0) {
		AudioChannel* front = MusicStack.front();
		front->Fading = MusicFade_In;
		front->FadeTimer = 0.0f;
		front->FadeTimerMax = fadeInAfterFinished;
	}

	AudioManager::ClampParams(pan, speed, volume);

	AudioChannel* newms = new AudioChannel();
	newms->Audio = music;
	newms->Playback = music->CreatePlayer();
	newms->Loop = loop;
	newms->LoopPoint = lp;
	newms->Fading = MusicFade_None;
	newms->Pan = pan;
	newms->Speed = (Uint32)(speed * 0x10000);
	newms->Volume = volume;
	newms->FadeTimer = 1.0;
	newms->FadeTimerMax = 1.0;

	int start_sample = (int)std::ceil(at * music->Format.freq);

	newms->Playback->Seek(start_sample);
	if (newms->Playback->ConversionStream) {
		SDL_AudioStreamClear(newms->Playback->ConversionStream);
	}

	if (loop) {
		newms->Playback->LoopIndex = (Sint32)lp;
	}

	MusicStack.push_front(newms);

	AudioManager::Unlock();
}
void AudioManager::RemoveMusic(ISound* music) {
	AudioManager::Lock();
	for (size_t i = 0; i < MusicStack.size(); i++) {
		if (MusicStack[i]->Audio == music) {
			delete MusicStack[i];
			MusicStack.erase(MusicStack.begin() + i);
		}
	}
	AudioManager::Unlock();
}
bool AudioManager::IsPlayingMusic() {
	return MusicStack.size() > 0;
}
bool AudioManager::IsPlayingMusic(ISound* music) {
	for (size_t i = 0; i < MusicStack.size(); i++) {
		if (MusicStack[i]->Audio == music) {
			return true;
		}
	}
	return false;
}
void AudioManager::ClearMusic() {
	AudioManager::Lock();
	for (size_t i = 0; i < MusicStack.size(); i++) {
		delete MusicStack[i];
	}
	MusicStack.clear();
	AudioManager::Unlock();
}
void AudioManager::ClearSounds() {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		SoundArray[i].Stopped = true;
		if (SoundArray[i].Playback) {
			delete SoundArray[i].Playback;
			SoundArray[i].Playback = NULL;
		}
	}
	AudioManager::Unlock();
}
void AudioManager::FadeOutMusic(double seconds) {
	AudioManager::Lock();
	if (MusicStack.size() > 0) {
		MusicStack[0]->Fading = MusicFade_Out;
		MusicStack[0]->FadeTimer = seconds;
		MusicStack[0]->FadeTimerMax = seconds;
	}
	AudioManager::Unlock();
}
void AudioManager::AlterMusic(float pan, float speed, float volume) {
	AudioManager::Lock();
	if (MusicStack.size() > 0) {
		AudioManager::ClampParams(pan, speed, volume);
		MusicStack[0]->Pan = pan;
		MusicStack[0]->Speed = (Uint32)(speed * 0x10000);
		MusicStack[0]->Volume = volume;
	}
	AudioManager::Unlock();
}
double AudioManager::GetMusicPosition(ISound* music) {
	AudioManager::Lock();
	double position = 0.0;
	for (size_t i = 0; i < MusicStack.size(); i++) {
		if (MusicStack[i]->Audio == music) {
			position = MusicStack[i]->Playback->SoundData->GetPosition();
			break;
		}
	}
	AudioManager::Unlock();
	return position;
}

void AudioManager::Lock() {
	SDL_LockAudioDevice(Device);
}
void AudioManager::Unlock() {
	SDL_UnlockAudioDevice(Device);
}

int AudioManager::GetFreeChannel() {
	AudioManager::Lock();
	int channel = -1;
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Stopped) {
			channel = i;
			break;
		}
	}
	AudioManager::Unlock();
	return channel;
}
void AudioManager::AlterChannel(int channel, float pan, float speed, float volume) {
	AudioManager::Lock();
	if (!SoundArray[channel].Stopped && !SoundArray[channel].Paused) {
		AudioManager::ClampParams(pan, speed, volume);
		SoundArray[channel].Pan = pan;
		SoundArray[channel].Speed = (Uint32)(speed * 0x10000);
		SoundArray[channel].Volume = volume;
	}
	AudioManager::Unlock();
}
bool AudioManager::AudioIsPlaying(int channel) {
	bool isPlaying = false;
	AudioManager::Lock();
	isPlaying = !SoundArray[channel].Stopped && !SoundArray[channel].Paused;
	AudioManager::Unlock();
	return isPlaying;
}
bool AudioManager::AudioIsPlaying(ISound* audio) {
	bool isPlaying = false;
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		AudioChannel* channel = &SoundArray[i];
		if (!channel->Stopped && !channel->Paused && channel->Audio == audio) {
			isPlaying = true;
		}
	}
	AudioManager::Unlock();
	return isPlaying;
}
void AudioManager::AudioUnpause(int channel) {
	AudioManager::Lock();
	SoundArray[channel].Paused = false;
	AudioManager::Unlock();
}
void AudioManager::AudioUnpause(ISound* audio) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Audio == audio) {
			SoundArray[i].Paused = false;
		}
	}
	AudioManager::Unlock();
}
void AudioManager::AudioPause(int channel) {
	AudioManager::Lock();
	SoundArray[channel].Paused = true;
	AudioManager::Unlock();
}
void AudioManager::AudioPause(ISound* audio) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Audio == audio) {
			SoundArray[i].Paused = true;
		}
	}
	AudioManager::Unlock();
}
void AudioManager::AudioStop(int channel) {
	AudioManager::Lock();
	SoundArray[channel].Stopped = true;
	AudioManager::Unlock();
}
void AudioManager::AudioStop(ISound* audio) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Audio == audio) {
			SoundArray[i].Stopped = true;
		}
	}
	AudioManager::Unlock();
}
void AudioManager::AudioRemove(ISound* audio) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Audio == audio) {
			SoundArray[i].Stopped = true;
			if (SoundArray[i].Playback) {
				delete SoundArray[i].Playback;
				SoundArray[i].Playback = NULL;
			}
		}
	}
	AudioManager::Unlock();
}
void AudioManager::AudioUnpauseAll() {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		SoundArray[i].Paused = false;
	}
	AudioManager::Unlock();
}
void AudioManager::AudioPauseAll() {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		SoundArray[i].Paused = true;
	}
	AudioManager::Unlock();
}
void AudioManager::AudioStopAll() {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		SoundArray[i].Stopped = true;
	}
	AudioManager::Unlock();
}

bool AudioManager::IsOriginPlaying(void* origin, ISound* audio) {
	bool isPlaying = false;
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		AudioChannel* channel = &SoundArray[i];
		if (!channel->Stopped && !channel->Paused && channel->Audio == audio &&
			channel->Origin == origin) {
			isPlaying = true;
		}
	}
	AudioManager::Unlock();
	return isPlaying;
}
void AudioManager::StopOriginSound(void* origin, ISound* audio) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Audio == audio && SoundArray[i].Origin == origin) {
			SoundArray[i].Stopped = true;
		}
	}
	AudioManager::Unlock();
}
void AudioManager::StopAllOriginSounds(void* origin) {
	AudioManager::Lock();
	for (int i = 0; i < SoundArrayLength; i++) {
		if (SoundArray[i].Origin == origin) {
			SoundArray[i].Stopped = true;
		}
	}
	AudioManager::Unlock();
}

void AudioManager::MixAudioLR(Uint8* dest, Uint8* src, size_t len, float volumeL, float volumeR) {
#define DEFINE_STREAM_PTR(type) \
	type* src##type; \
	type* dest##type
#define PAN_STREAM_PTR(type) \
	src##type = (type*)(src); \
	dest##type = (type*)(dest); \
	for (Uint32 o = 0; o < len; o++) { \
		dest##type[0] = (type)(src##type[0] * volumeL); \
		dest##type[1] = (type)(src##type[1] * volumeR); \
		src##type += 2; \
		dest##type += 2; \
	}

	DEFINE_STREAM_PTR(Uint8);
	DEFINE_STREAM_PTR(Sint8);
	DEFINE_STREAM_PTR(Uint16);
	DEFINE_STREAM_PTR(Sint16);
	DEFINE_STREAM_PTR(Sint32);
	DEFINE_STREAM_PTR(float);

	switch (DeviceFormat.format) {
	case AUDIO_U8:
		PAN_STREAM_PTR(Uint8);
		break;
	case AUDIO_S8:
		PAN_STREAM_PTR(Sint8);
		break;
	case AUDIO_U16SYS:
		PAN_STREAM_PTR(Uint16);
		break;
	case AUDIO_S16SYS:
		PAN_STREAM_PTR(Sint16);
		break;
	case AUDIO_S32SYS:
		PAN_STREAM_PTR(Sint32);
		break;
	case AUDIO_F32SYS:
		PAN_STREAM_PTR(float);
		break;
	}
#undef DEFINE_STREAM_PTR
#undef PAN_STREAM_PTR
}

bool AudioManager::HandleFading(AudioChannel* audio) {
	if (audio->Fading == MusicFade_Out) {
		audio->FadeTimer -= (double)DeviceFormat.samples / DeviceFormat.freq;
		if (audio->FadeTimer < 0.0) {
			audio->FadeTimer = 1.0;
			audio->FadeTimerMax = 1.0;
			audio->Fading = MusicFade_None;
			return true; // Stop playing audio.
		}
	}
	else if (audio->Fading == MusicFade_In) {
		audio->FadeTimer += (double)DeviceFormat.samples / DeviceFormat.freq;
		if (audio->FadeTimer > audio->FadeTimerMax) {
			audio->FadeTimer = 1.0;
			audio->FadeTimerMax = 1.0;
			audio->Fading = MusicFade_None; // Stop fading
				// in audio.
		}
	}
	return false;
}
bool AudioManager::AudioPlayMix(AudioChannel* audio, Uint8* stream, int len, float volume) {
	if (AudioManager::HandleFading(audio)) {
		return true;
	}

	AudioPlayback* playback = audio->Playback;
	if (!playback || !audio->Playback->SoundData) {
		return false;
	}

	int bytesPerSample = BytesPerSample;
	int bytes = playback->BufferedSamples * bytesPerSample;

	unsigned advanceAccumulator = 0;
	unsigned advanceReadIndex = 0;
	unsigned advance = 0;
	unsigned speed = audio->Speed;

	// Read more bytes
	if (bytes == 0) {
		bytes = playback->RequestSamples(
			DeviceFormat.samples, audio->Loop, audio->LoopPoint);
	}
	else {
		advanceReadIndex = len - bytes;
	}

	int mixVolume;
	if (audio->Fading) {
		mixVolume = (int)(SDL_MIX_MAXVOLUME * MasterVolume * volume *
			(audio->FadeTimer / audio->FadeTimerMax));
	}
	else {
		mixVolume = (int)(SDL_MIX_MAXVOLUME * MasterVolume * volume);
	}

	float volumeL = 1.0f;
	float volumeR = 1.0f;

	bool doPanning = audio->Pan != 0.0f && DeviceFormat.channels == 2;
	if (doPanning) {
		if (audio->Pan < 0.f) {
			volumeR += audio->Pan;
		}
		else {
			volumeL -= audio->Pan;
		}
	}

	switch (bytes) {
	// End of file
	case REQUEST_EOF:
		return true; // Stop playing audio.
	// Waiting
	case REQUEST_CONVERTING:
		break;
	// Error
	case REQUEST_ERROR:
		break;
	// Normal
	default:
		if (speed == 0x10000) {
			if (!doPanning) {
				SDL_MixAudioFormat(stream,
					playback->Buffer,
					DeviceFormat.format,
					(Uint32)bytes,
					mixVolume);
			}
			else {
				Uint32 mixAdvance = MixBufferSize * bytesPerSample;
				for (Uint32 o = 0; o < (unsigned)len; o += mixAdvance) {
					AudioManager::MixAudioLR(MixBuffer,
						playback->Buffer + advanceReadIndex,
						MixBufferSize,
						volumeL,
						volumeR);
					SDL_MixAudioFormat(stream + o,
						MixBuffer,
						DeviceFormat.format,
						mixAdvance,
						mixVolume);
					advanceReadIndex += mixAdvance;
				}
			}

			playback->BufferedSamples -= bytes / bytesPerSample;
		}
		else {
			for (Uint32 o = 0; o < (unsigned)len; o += bytesPerSample) {
				advanceAccumulator += speed;
				advance = advanceAccumulator >> 16;
				advanceAccumulator &= 0xFFFF;

				if (doPanning) {
					AudioManager::MixAudioLR(MixBuffer,
						playback->Buffer + advanceReadIndex,
						1,
						volumeL,
						volumeR);
					SDL_MixAudioFormat(stream + o,
						MixBuffer,
						DeviceFormat.format,
						(Uint32)bytesPerSample,
						mixVolume);
				}
				else {
					SDL_MixAudioFormat(stream + o,
						playback->Buffer + advanceReadIndex,
						DeviceFormat.format,
						(Uint32)bytesPerSample,
						mixVolume);
				}

				advanceReadIndex += advance * bytesPerSample;
				if (playback->BufferedSamples <= advance) {
					playback->BufferedSamples = 0;
					advanceReadIndex = 0;

					bytes = playback->RequestSamples(DeviceFormat.samples,
						audio->Loop,
						audio->LoopPoint);
				}
				else {
					playback->BufferedSamples -= advance;
				}
			}
		}
	}
	return false;
}

void AudioManager::AudioCallback(void* data, Uint8* stream, int len) {
	memset(stream, 0x00, len);

	if (AudioManager::AudioQueueSize >= (size_t)len) {
		SDL_MixAudioFormat(stream,
			AudioManager::AudioQueue,
			DeviceFormat.format,
			(Uint32)len,
			(int)(SDL_MIX_MAXVOLUME * MasterVolume));

		AudioManager::AudioQueueSize -= len;
		if (AudioManager::AudioQueueSize > 0) {
			memmove(AudioManager::AudioQueue,
				AudioManager::AudioQueue + len,
				AudioManager::AudioQueueSize);
		}
	}

	// Make track system
	if (MusicStack.size() > 0) {
		AudioChannel* audio = MusicStack.front();
		if (!audio->Paused) {
			if (AudioManager::AudioPlayMix(
				    audio, stream, len, audio->Volume * MusicVolume)) {
				delete audio;
				MusicStack.pop_front();
			}
		}
	}

	for (int i = 0; i < SoundArrayLength; i++) {
		AudioChannel* audio = &SoundArray[i];
		if (!audio->Audio || audio->Stopped || audio->Paused) {
			continue;
		}

		if (AudioManager::AudioPlayMix(audio, stream, len, audio->Volume * SoundVolume)) {
			audio->Stopped = true;
		}
	}

	if (LowPassFilter > 0.0) {
		size_t channels = 2;
		if (SDL_AUDIO_ISFLOAT(DeviceFormat.format)) {
			size_t samples = len / sizeof(float) / channels;

			float* in = (float*)stream;
			float* out = (float*)stream;
			for (size_t i = 0; i < samples; i++) {
				*out++ = ProcessSampleFloat(*in++, 1);
				*out++ = ProcessSampleFloat(*in++, 0);
			}
		}
		else {
			size_t samples = len / sizeof(Sint16) / channels;

			Sint16* in = (Sint16*)stream;
			Sint16* out = (Sint16*)stream;
			for (size_t i = 0; i < samples; i++) {
				*out++ = (Sint16)ProcessSample(*in++, 1);
				*out++ = (Sint16)ProcessSample(*in++, 0);
			}
		}
	}
}

void AudioManager::Dispose() {
	Memory::Free(SoundArray);
	Memory::Free(AudioQueue);
	Memory::Free(MixBuffer);

	SDL_PauseAudioDevice(Device, 1);
	SDL_CloseAudioDevice(Device);
}
