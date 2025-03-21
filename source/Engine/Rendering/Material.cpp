#include <Engine/Rendering/Material.h>

#include <Engine/Bytecode/Types.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Scene.h>
#include <Engine/Utilities/StringUtils.h>

std::vector<Material*> Material::List;

Material* Material::Create(char* name) {
	Material* material = new Material(name);

	material->Object = (void*)NewMaterial(material);

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

Image* Material::TryLoadForModel(std::string imagePath, const char* parentDirectory) {
	std::string filename = imagePath;

	if (parentDirectory) {
		filename = Path::Concat(std::string(parentDirectory), filename);
	}

	int resourceID = Scene::LoadImageResource(filename.c_str(), SCOPE_SCENE);
	if (resourceID == -1) {
		return nullptr;
	}

	Image* image = Scene::GetImageResource(resourceID)->AsImage;

	image->AddRef();

	return image;
}

Image* Material::LoadForModel(string imagePath, const char* parentDirectory) {
	// Try possible combinations
	Image* image = nullptr;

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

Image* Material::LoadForModel(const char* imagePath, const char* parentDirectory) {
	return LoadForModel(std::string(imagePath), parentDirectory);
}

void Material::ReleaseImage(Image* imagePtr) {
	if (imagePtr && imagePtr->TakeRef()) {
		delete imagePtr;
	}
}

void Material::Dispose() {
	ReleaseImage(TextureDiffuse);
	ReleaseImage(TextureSpecular);
	ReleaseImage(TextureAmbient);
	ReleaseImage(TextureEmissive);

	Memory::Free(Name);

	Memory::Free(TextureDiffuseName);
	Memory::Free(TextureSpecularName);
	Memory::Free(TextureAmbientName);
	Memory::Free(TextureEmissiveName);
}

Material::~Material() {
	Dispose();
}
