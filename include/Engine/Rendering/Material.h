#ifndef ENGINE_RENDERING_MATERIAL_H
#define ENGINE_RENDERING_MATERIAL_H

#include <Engine/Includes/Standard.h>

class Material {
private:
	static void* TryLoadForModel(std::string imagePath, const char* parentDirectory);
	void ReleaseTexture(void** asset);

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
	void* TextureDiffuse = nullptr;
	void* TextureSpecular = nullptr;
	void* TextureAmbient = nullptr;
	void* TextureEmissive = nullptr;
	void* VMObject = nullptr;

	static Material* Create(char* name);
	static void Remove(Material* material);
	static std::vector<Material*> List;

	static void* LoadForModel(string imagePath, const char* parentDirectory);
	static void* LoadForModel(const char* imagePath, const char* parentDirectory);

	Material(char* name);
	void SetTexture(void** asset, void* newImage);
	void Dispose();
	~Material();
};

#endif /* ENGINE_RENDERING_MATERIAL_H */
