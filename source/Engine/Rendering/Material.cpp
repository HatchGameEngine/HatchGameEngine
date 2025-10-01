#include <Engine/Rendering/Material.h>

#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/ResourceTypes/Resource.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/Utilities/StringUtils.h>

std::vector<Material*> Material::List;

Material* Material::Create(char* name) {
	Material* material = new Material(name);

	List.push_back(material);

	return material;
}

void Material::Remove(Material* material) {
	auto it = std::find(List.begin(), List.end(), material);
	if (it != List.end()) {
		List.erase(it);
	}
}

Material::Material(char* name) {
	Name = name;

	for (int i = 0; i < 4; i++) {
		ColorDiffuse[i] = ColorSpecular[i] = ColorAmbient[i] = ColorEmissive[i] = 1.0f;
	}
}

void* Material::TryLoadForModel(std::string imagePath, const char* parentDirectory) {
	std::string filename = imagePath;

	if (parentDirectory) {
		filename = Path::Concat(std::string(parentDirectory), filename);
	}

	ResourceType* resource = Resource::Load(RESOURCE_IMAGE, filename.c_str(), SCOPE_GAME, false);
	if (resource) {
		return (void*)resource;
	}

	return nullptr;
}

void* Material::LoadForModel(string imagePath, const char* parentDirectory) {
	// Try possible combinations
	void* image = nullptr;

	if ((image = TryLoadForModel(imagePath, parentDirectory))) {
		return image;
	}
	if ((image = TryLoadForModel(imagePath + ".png", parentDirectory))) {
		return image;
	}
	if ((image = TryLoadForModel("Textures/" + imagePath, parentDirectory))) {
		return image;
	}
	if ((image = TryLoadForModel("Textures/" + imagePath + ".png", parentDirectory))) {
		return image;
	}

	if ((image = TryLoadForModel(imagePath, nullptr))) {
		return image;
	}
	if ((image = TryLoadForModel(imagePath + ".png", nullptr))) {
		return image;
	}
	if ((image = TryLoadForModel("Textures/" + imagePath, nullptr))) {
		return image;
	}
	if ((image = TryLoadForModel("Textures/" + imagePath + ".png", nullptr))) {
		return image;
	}

	// Well, we tried.
	return nullptr;
}

void* Material::LoadForModel(const char* imagePath, const char* parentDirectory) {
	return LoadForModel(std::string(imagePath), parentDirectory);
}

void Material::SetTexture(void** resourceable, void* newImage) {
	Image* image = (Image*)newImage;

	ReleaseTexture(resourceable);

	if (image) {
		image->TakeRef();
		(*resourceable) = image;
	}
}

void Material::ReleaseTexture(void** resourceable) {
	Image* image = *((Image**)resourceable);
	if (image) {
		image->Release();

		(*resourceable) = nullptr;
	}
}

void Material::Dispose() {
	ReleaseTexture(&TextureDiffuse);
	ReleaseTexture(&TextureSpecular);
	ReleaseTexture(&TextureAmbient);
	ReleaseTexture(&TextureEmissive);

	Memory::Free(Name);

	Memory::Free(TextureDiffuseName);
	Memory::Free(TextureSpecularName);
	Memory::Free(TextureAmbientName);
	Memory::Free(TextureEmissiveName);
}

Material::~Material() {
	Dispose();
}
