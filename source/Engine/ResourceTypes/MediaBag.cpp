#include <Engine/Audio/AudioManager.h>
#include <Engine/ResourceTypes/MediaBag.h>
#include <Engine/Scene.h>

MediaBag::MediaBag(const char* filename) {
	Type = ASSET_VIDEO;

	Loaded = Load(filename);
}

bool MediaBag::Load(const char* filename) {
#ifdef USING_FFMPEG
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		return false;
	}

	Source = MediaSource::CreateSourceFromStream(stream);
	if (!Source) {
		return false;
	}

	Player = MediaPlayer::Create(Source,
		Source->GetBestStream(MediaSource::STREAMTYPE_VIDEO),
		Source->GetBestStream(MediaSource::STREAMTYPE_AUDIO),
		Source->GetBestStream(MediaSource::STREAMTYPE_SUBTITLE),
		Scene::Views[0].Width,
		Scene::Views[0].Height);
	if (!Player) {
		Source->Close();
		Source = nullptr;
		return false;
	}

	PlayerInfo playerInfo;
	Player->GetInfo(&playerInfo);
	VideoTexture = Graphics::CreateTexture(playerInfo.Video.Output.Format,
		SDL_TEXTUREACCESS_STATIC,
		playerInfo.Video.Output.Width,
		playerInfo.Video.Output.Height);
	if (!VideoTexture) {
		Player->Close();
		Source->Close();
		Player = nullptr;
		Source = nullptr;
		return nullptr;
	}

	if (Player->GetVideoStream() > -1) {
		Log::Print(Log::LOG_WARN, "VIDEO STREAM:");
		Log::Print(Log::LOG_INFO,
			"    Resolution:  %d x %d",
			playerInfo.Video.Output.Width,
			playerInfo.Video.Output.Height);
	}
	if (Player->GetAudioStream() > -1) {
		Log::Print(Log::LOG_WARN, "AUDIO STREAM:");
		Log::Print(
			Log::LOG_INFO, "    Sample Rate: %d", playerInfo.Audio.Output.SampleRate);
		Log::Print(Log::LOG_INFO,
			"    Bit Depth:   %d-bit",
			playerInfo.Audio.Output.Format & 0xFF);
		Log::Print(Log::LOG_INFO, "    Channels:    %d", playerInfo.Audio.Output.Channels);
	}

	return true;
#else
	return false;
#endif
}

void MediaBag::Unload() {
	if (!Loaded) {
		return;
	}

#ifdef USING_FFMPEG
	AudioManager::Lock();

	if (Player) {
		Player->Close();
		Player = nullptr;
	}

	if (Source) {
		Source->Close();
		Source = nullptr;
	}

	AudioManager::Unlock();
#endif

	Loaded = false;
}

MediaBag::~MediaBag() {
	Unload();
}
