#ifndef ENGINE_SCENE_SCENELAYER_H
#define ENGINE_SCENE_SCENELAYER_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>
#include <Engine/Scene/ScrollingIndex.h>
#include <Engine/Scene/ScrollingInfo.h>

class SceneLayer {
public:
	char Name[50];
	bool Visible = true;
	int Width = 0;
	int Height = 0;
	Uint32 WidthMask = 0;
	Uint32 HeightMask = 0;
	Uint32 WidthInBits = 0;
	Uint32 HeightInBits = 0;
	Uint32 WidthData = 0;
	Uint32 HeightData = 0;
	Uint32 DataSize = 0;
	Uint32 ScrollIndexCount = 0;
	int RelativeY = 0x0100;
	int ConstantY = 0x0000;
	int OffsetX = 0x0000;
	int OffsetY = 0x0000;
	Uint32* Tiles = NULL;
	Uint32* TilesBackup = NULL;
	Uint16* TileOffsetY = NULL;
	int DeformOffsetA = 0;
	int DeformOffsetB = 0;
	int DeformSetA[MAX_DEFORM_LINES];
	int DeformSetB[MAX_DEFORM_LINES];
	int DeformSplitLine = 0;
	int Flags = 0x0000;
	int DrawGroup = 0;
	Uint8 DrawBehavior = 0;
	HashMap<VMValue>* Properties = NULL;
	bool Blending = false;
	Uint8 BlendMode = 0; // BlendMode_NORMAL
	float Opacity = 1.0f;
	bool UsePaletteIndexLines = false;
	bool UsingCustomScanlineFunction = false;
	ObjFunction CustomScanlineFunction;
	bool UsingCustomRenderFunction = false;
	ObjFunction CustomRenderFunction;
	int ScrollInfoCount = 0;
	ScrollingInfo* ScrollInfos = NULL;
	int ScrollInfosSplitIndexesCount = 0;
	Uint16* ScrollInfosSplitIndexes = NULL;
	Uint8* ScrollIndexes = NULL;
	Uint32 BufferID = 0;
	int VertexCount = 0;
	void* TileBatches = NULL;
	enum {
		FLAGS_COLLIDEABLE = 1,
		FLAGS_REPEAT_X = 2,
		FLAGS_REPEAT_Y = 4,
	};

	SceneLayer();
	SceneLayer(int w, int h);
	bool PropertyExists(char* property);
	VMValue PropertyGet(char* property);
	void Dispose();
};

#endif /* ENGINE_SCENE_SCENELAYER_H */
