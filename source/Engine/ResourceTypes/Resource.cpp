#include <Engine/Bytecode/TypeImpl/ResourceImpl.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/Utilities/StringUtils.h>

static vector<ResourceType*> List;

ResourceType* Resource::New(Uint8 type, const char* filename, Uint32 hash, int unloadPolicy, bool unique) {
	ResourceType* resource = new (std::nothrow) ResourceType();
	resource->Type = type;
	resource->Filename = StringUtils::Duplicate(filename);
	resource->FilenameHash = hash;
	resource->UnloadPolicy = unloadPolicy;
	resource->Unique = unique;
	AddRef(resource);
	return resource;
}

std::vector<ResourceType*>* Resource::GetList() {
	return &List;
}

void Resource::DisposeInScope(Uint32 scope) {
	for (size_t i = 0; i < List.size();) {
		ResourceType* resource = List[i];
		if (resource->UnloadPolicy > scope) {
			i++;
			continue;
		}
		Resource::Unload(resource);
		Resource::Release(resource);
		List.erase(List.begin() + i);
	}
}

void Resource::DisposeAll() {
	DisposeInScope(SCOPE_GAME);
}

void* Resource::GetVMObject(ResourceType* resource) {
	if (resource->VMObject == nullptr) {
		Obj* resourceObj = ResourceImpl::New((void*)resource);
		resource->VMObject = (void*)resourceObj;
		Resource::TakeRef(resource);
	}

	return resource->VMObject;
}

void Resource::SetVMObject(ResourceType* resource, void* obj) {
	ReleaseVMObject(resource);

	if (obj != nullptr) {
		ResourceImpl::SetOwner((Obj*)obj, (void*)resource);
		Resource::TakeRef(resource);
		resource->VMObject = obj;
	}
}

void Resource::ReleaseVMObject(ResourceType* resource) {
	if (resource->VMObject != nullptr) {
		resource->VMObject = nullptr;
		Resource::Release(resource);
	}
}

bool Resource::CompareVMObjects(void* a, void* b) {
	ObjResource* resource = resource = (ObjResource*)a;
	ObjResourceable* resourceable = (ObjResourceable*)b;

	if (!resource->ResourcePtr || !resourceable->ResourceablePtr) {
		return false;
	}

	return ((ResourceType*)resource->ResourcePtr)->AsResourceable == resourceable->ResourceablePtr;
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

	// Try loading it.
	Resourceable* data = LoadData(resource->Type, resource->Filename);
	if (data != nullptr) {
		UnloadData(resource); // Unload only if loading succeeded
		data->TakeRef();
		resource->AsResourceable = data;
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

int Resource::Search(Uint8 type, const char* filename, Uint32 hash) {
	for (size_t i = 0, listSz = List.size(); i < listSz; i++) {
		if (List[i]->Type == type
		&& List[i]->FilenameHash == hash
		&& List[i]->Unique == false
		&& strcmp(List[i]->Filename, filename) == 0) {
			return (int)i;
		}
	}

	return -1;
}

ResourceType* Resource::Load(Uint8 type, const char* filename, int unloadPolicy, bool unique) {
	// Guess resource type if none was given
	if (type == RESOURCE_NONE) {
		type = GuessType(filename);
		if (type == RESOURCE_NONE) {
			return nullptr;
		}
	}

	Uint32 hash = CRC32::EncryptString(filename);

	// If not unique, find a resource that already exists.
	if (!unique) {
		int result = Search(type, filename, hash);
		if (result != -1) {
			return List[result];
		}
	}

	// Try loading it.
	Resourceable* data = LoadData(type, filename);
	if (data == nullptr) {
		return nullptr;
	}

	// Allocate a new resource.
	ResourceType* resource = New(type, filename, hash, unloadPolicy, unique);
	resource->AsResourceable = data;
	resource->Loaded = true;

	// Add it to the list.
	List.push_back(resource);

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

Resourceable* Resource::LoadData(Uint8 type, const char* filename) {
	Resourceable* data = nullptr;

	switch (type) {
	case RESOURCE_SPRITE:
		data = new (std::nothrow) ISprite(filename);
		break;
	case RESOURCE_IMAGE:
		data = new (std::nothrow) Image(filename);
		break;
	case RESOURCE_AUDIO:
		data = new (std::nothrow) ISound(filename);
		break;
	case RESOURCE_MODEL:
		data = new (std::nothrow) IModel(filename);
		break;
	case RESOURCE_MEDIA:
		data = new (std::nothrow) MediaBag(filename);
		break;
	default:
		break;
	}

	if (data && !data->IsLoaded()) {
		delete data;
		return nullptr;
	}

	data->TakeRef();

	return data;
}

bool Resource::UnloadData(ResourceType* resource) {
	Resourceable* resourceable = resource->AsResourceable;

	if (resourceable != nullptr) {
		resourceable->Unload();
		resourceable->Release();
		resource->AsResourceable = nullptr;
		return true;
	}

	return false;
}
