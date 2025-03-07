#ifndef ENGINE_RENDERING_MATERIAL_H
#define ENGINE_RENDERING_MATERIAL_H

#include <Engine/Includes/Standard.h>

#include <Engine/ResourceTypes/Image.h>

class Material {
private:
	static Image* TryLoadForModel(std::string imagePath, const char* parentDirectory);
	void ReleaseImage(Image* imagePtr);

public:
	char* Name = nullptr;
	float ColorDiffuse[4];
	float ColorSpecular[4];
	float ColorAmbient[4];
	float ColorEmissive[4];
	float Shininess = 0.0f;
	float ShininessStrength = 1.0f;
	float Opacity = 1.0f;
	char* TextureDiffuseName = nullptr;
	char* TextureSpecularName = nullptr;
	char* TextureAmbientName = nullptr;
	char* TextureEmissiveName = nullptr;
	Image* TextureDiffuse = nullptr;
	Image* TextureSpecular = nullptr;
	Image* TextureAmbient = nullptr;
	Image* TextureEmissive = nullptr;
	void* Object = nullptr;

	static Material* Create(char* name);
	static void Remove(Material* material);
	static std::vector<Material*> List;

	static Image* LoadForModel(string imagePath, const char* parentDirectory);
	static Image* LoadForModel(const char* imagePath, const char* parentDirectory);

	Material(char* name);
	void Dispose();
	~Material();
};

#endif /* ENGINE_RENDERING_MATERIAL_H */
