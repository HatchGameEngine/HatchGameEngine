#include <Engine/Diagnostics/Memory.h>
#include <Engine/FontFace.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/Scene.h>
#include <Engine/Utilities/StringUtils.h>

static vector<ResourceType*> List;

ResourceType* Resource::New(Uint8 type, const char* filename, Uint32 hash, int unloadPolicy) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->Type = type;
	resource->Filename = StringUtils::Duplicate(filename);
	resource->FilenameHash = hash;
	resource->UnloadPolicy = unloadPolicy;
	AddRef(resource);
	return resource;
}

std::vector<ResourceType*>* Resource::GetList(Uint32 scope) {
	if (scope == SCOPE_SCENE) {
		return &Scene::ResourceList;
	}

	return &List;
}

void Resource::DisposeInList(std::vector<ResourceType*>* list, Uint32 scope) {
	for (size_t i = 0; i < list->size();) {
		ResourceType* resource = (*list)[i];
		if (resource->UnloadPolicy > scope) {
			i++;
			continue;
		}
		Resource::Unload(resource);
		Resource::Release(resource);
		list->erase(list->begin() + i);
	}
}

void Resource::DisposeGlobal() {
	const Uint32 scope = SCOPE_GAME;

	DisposeInList(GetList(scope), scope);
}

void* Resource::GetVMObject(ResourceType* resource) {
	if (resource->VMObject == nullptr) {
		resource->VMObject = (void*)(NewResource(resource));
		Resource::TakeRef(resource);
	}

	return resource->VMObject;
}

void Resource::AddRef(ResourceType* resource) {
	resource->RefCount++;
}

void Resource::DecRef(ResourceType* resource) {
	resource->RefCount--;
	if (resource->RefCount == 0) {
		Delete(resource);
	}
}

void Resource::TakeRef(ResourceType* resource) {
	if (resource) {
		AddRef(resource);
	}
}

bool Resource::Reload(ResourceType* resource) {
	if (!resource) {
		return false;
	}

	UnloadData(resource);

	resource->Loaded = false;

	// Try loading it.
	void* resData = LoadData(resource->Type, resource->Filename);
	if (resData != nullptr) {
		resource->AsAny = resData;
		resource->Loaded = true;
	}

	return resource->Loaded;
}

void Resource::Unload(ResourceType* resource) {
	if (resource && resource->Loaded) {
		UnloadData(resource);

		resource->Loaded = false;
	}
}

void Resource::Release(ResourceType* resource) {
	if (resource) {
		DecRef(resource);
	}
}

void Resource::Delete(ResourceType* resource) {
	if (resource) {
		if (resource->Loaded) {
			Unload(resource);
		}

		Memory::Free(resource->Filename);

		delete resource;
	}
}

int Resource::Search(vector<ResourceType*>* list, Uint8 type, const char* filename, Uint32 hash) {
	for (size_t i = 0, listSz = list->size(); i < listSz; i++) {
		if ((*list)[i]->Type == type
		&& (*list)[i]->FilenameHash == hash
		&& strcmp((*list)[i]->Filename, filename) == 0) {
			return (int)i;
		}
	}

	return -1;
}

ResourceType* Resource::Load(vector<ResourceType*>* list, Uint8 type, const char* filename, int unloadPolicy) {
	switch (type) {
	case RESOURCE_NONE:
	case RESOURCE_SPRITE:
	case RESOURCE_IMAGE:
	case RESOURCE_AUDIO:
	case RESOURCE_MODEL:
	case RESOURCE_MEDIA:
		return LoadInternal(list, type, filename, unloadPolicy);
	default:
		break;
	}

	return nullptr;
}

ResourceType* Resource::LoadInternal(vector<ResourceType*>* list, Uint8 type, const char* filename, int unloadPolicy) {
	// Guess resource type if none was given
	if (type == RESOURCE_NONE) {
		type = GuessType(filename);
		if (type == RESOURCE_NONE) {
			return nullptr;
		}
	}

	// Find a resource that already exists.
	Uint32 hash = CRC32::EncryptString(filename);
	int result = Search(list, type, filename, hash);
	if (result != -1) {
		return (*list)[result];
	}

	// Try loading it.
	void* resData = LoadData(type, filename);
	if (resData == nullptr) {
		return nullptr;
	}

	// Allocate a new resource.
	ResourceType* resource = New(type, filename, hash, unloadPolicy);
	resource->AsAny = resData;
	resource->Loaded = true;

	// Add it to the list.
	list->push_back(resource);

	return resource;
}

ResourceType* Resource::LoadFont(vector<ResourceType*>* list, const char* filename, int pixel_sz, int unloadPolicy) {
	// Find a resource that already exists.
	Uint32 hash = CRC32::EncryptString(filename);
	hash = CRC32::EncryptData(&pixel_sz, sizeof(int), hash);

	int result = Search(list, RESOURCE_FONT, filename, hash);
	if (result != -1) {
		return (*list)[result];
	}

	// Try loading it.
	ISprite* resData = LoadFontData(filename, pixel_sz);
	if (!resData) {
		return nullptr;
	}

	// Allocate a new resource.
	ResourceType* resource = New(RESOURCE_FONT, filename, hash, unloadPolicy);
	resource->AsSprite = resData;
	resource->Loaded = true;

	// Add it to the list.
	list->push_back(resource);

	return resource;
}

Uint8 Resource::GuessType(const char* filename) {
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		return RESOURCE_NONE;
	}

	// Guess sprite
	if (ISprite::IsFile(stream)) {
		stream->Close();
		return RESOURCE_SPRITE;
	}
	stream->Seek(0);

	// Guess audio
	if (ISound::IsFile(stream)) {
		stream->Close();
		return RESOURCE_AUDIO;
	}
	stream->Seek(0);

	// Guess image
	if (Image::IsFile(stream)) {
		stream->Close();
		return RESOURCE_IMAGE;
	}
	stream->Seek(0);

	// Guess model
	if (IModel::IsFile(stream)) {
		stream->Close();
		return RESOURCE_MODEL;
	}

	// Couldn't guess type
	stream->Close();

	return RESOURCE_NONE;
}

void* Resource::LoadData(Uint8 type, const char* filename) {
	switch (type) {
	case RESOURCE_SPRITE: {
		ISprite* resData = new (std::nothrow) ISprite(filename);
		if (resData->LoadFailed) {
			delete resData;
			return nullptr;
		}
		return resData;
	}
	case RESOURCE_IMAGE: {
		Image* resData = new (std::nothrow) Image(filename);
		if (!resData->TexturePtr) {
			delete resData;
			return nullptr;
		}
		return resData;
	}
	case RESOURCE_AUDIO: {
		ISound* resData = new (std::nothrow) ISound(filename);
		if (resData->LoadFailed) {
			delete resData;
			return nullptr;
		}
		return resData;
	}
	case RESOURCE_MODEL: {
		IModel* resData = new (std::nothrow) IModel(filename);
		if (resData->LoadFailed) {
			delete resData;
			return nullptr;
		}

		return resData;
	}
	case RESOURCE_MEDIA:
		return LoadMediaBag(filename);
	default:
		break;
	}

	return nullptr;
}

ISprite* Resource::LoadFontData(const char* filename, int pixel_sz) {
	return FontFace::SpriteFromFont(filename, pixel_sz);
}

MediaBag* Resource::LoadMediaBag(const char* filename) {
#ifdef USING_FFMPEG
	Stream* stream = ResourceStream::New(filename);
	if (!stream) {
		return nullptr;
	}

	MediaSource* Source = MediaSource::CreateSourceFromStream(stream);
	if (!Source) {
		return nullptr;
	}

	MediaPlayer* Player = MediaPlayer::Create(Source,
		Source->GetBestStream(MediaSource::STREAMTYPE_VIDEO),
		Source->GetBestStream(MediaSource::STREAMTYPE_AUDIO),
		Source->GetBestStream(MediaSource::STREAMTYPE_SUBTITLE),
		Scene::Views[0].Width,
		Scene::Views[0].Height);
	if (!Player) {
		Source->Close();
		return nullptr;
	}

	PlayerInfo playerInfo;
	Player->GetInfo(&playerInfo);
	Texture* VideoTexture = Graphics::CreateTexture(playerInfo.Video.Output.Format,
		SDL_TEXTUREACCESS_STATIC,
		playerInfo.Video.Output.Width,
		playerInfo.Video.Output.Height);
	if (!VideoTexture) {
		Player->Close();
		Source->Close();
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

	MediaBag* newMediaBag = new (std::nothrow) MediaBag;
	newMediaBag->Source = Source;
	newMediaBag->Player = Player;
	newMediaBag->VideoTexture = VideoTexture;
	return newMediaBag;
#else
	return nullptr;
#endif
}

bool Resource::UnloadData(ResourceType* resource) {
	switch (resource->Type) {
	case RESOURCE_SPRITE:
	case RESOURCE_FONT:
		if (resource->AsSprite != nullptr) {
			delete resource->AsSprite;
			resource->AsSprite = nullptr;
			return true;
		}
		break;
	case RESOURCE_IMAGE:
		if (resource->AsImage != nullptr) {
			delete resource->AsImage;
			resource->AsImage = nullptr;
			return true;
		}
		break;
	case RESOURCE_AUDIO:
		if (resource->AsAudio != nullptr) {
			AudioManager::Unload(resource->AsAudio);
			delete resource->AsAudio;
			resource->AsModel = nullptr;
			return true;
		}
		break;
	case RESOURCE_MODEL:
		if (resource->AsModel != nullptr) {
			delete resource->AsModel;
			resource->AsModel = nullptr;
			return true;
		}
		break;
	case RESOURCE_MEDIA:
		if (resource->AsMedia != nullptr) {
#ifdef USING_FFMPEG
			AudioManager::Lock();
			resource->AsMedia->Player->Close();
			resource->AsMedia->Source->Close();
			AudioManager::Unlock();
#endif
			delete resource->AsMedia;
			resource->AsMedia = nullptr;
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}
