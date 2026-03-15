#ifndef ENGINE_SCENE_SCENELAYER_H
#define ENGINE_SCENE_SCENELAYER_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Rendering/Enums.h>
#include <Engine/Types/Property.h>

class SceneLayer {
public:
	enum {
		TYPE_TILE,
		TYPE_IMAGE,
	};

	enum {
		FLAGS_COLLIDEABLE = 1,
		FLAGS_REPEAT_X = 2,
		FLAGS_REPEAT_Y = 4,
	};

	char* Name;
	Uint8 Type;
	bool Visible = true;

	int Width = 0;
	int Height = 0;

	float RelativeY = 1.0;
	float ConstantY = 0.0;
	float OffsetX = 0.0;
	float OffsetY = 0.0;

	Uint32 Flags = 0;
	int DrawGroup = 0;
	Uint8 DrawBehavior = 0;

	HashMap<Property>* Properties = NULL;

	bool UseBlending = false;
	Uint8 BlendMode = BlendMode_NORMAL;
	float Opacity = 1.0f;

	void* CurrentShader = nullptr;
	bool UsePaletteIndexLines = false;

	bool UsingCustomRenderFunction = false;
	ObjFunction CustomRenderFunction;

	bool PropertyExists(char* property);
	Property PropertyGet(char* property);
	~SceneLayer();
};

#endif /* ENGINE_SCENE_SCENELAYER_H */
