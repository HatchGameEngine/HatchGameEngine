#ifndef ENGINE_SCENE_TILELAYER_H
#define ENGINE_SCENE_TILELAYER_H

#include <Engine/Scene/SceneLayer.h>
#include <Engine/Scene/ScrollingInfo.h>

class TileLayer : public SceneLayer {
public:
	Uint32 WidthMask = 0;
	Uint32 HeightMask = 0;
	Uint32 WidthInBits = 0;
	Uint32 HeightInBits = 0;
	Uint32 WidthData = 0;
	Uint32 HeightData = 0;
	Uint32 DataSize = 0;
	Uint32* Tiles = NULL;
	Uint32* TilesBackup = NULL;

	Uint32 ScrollIndexCount = 0;
	int DeformOffsetA = 0;
	int DeformOffsetB = 0;
	int DeformSetA[MAX_DEFORM_LINES];
	int DeformSetB[MAX_DEFORM_LINES];
	int DeformSplitLine = 0;

	int ScrollInfoCount = 0;
	ScrollingInfo* ScrollInfos = NULL;
	Uint8* ScrollIndexes = NULL;
	bool UsingScrollIndexes = false;

	bool UsingCustomScanlineFunction = false;
	ObjFunction CustomScanlineFunction;

	Uint32 BufferID = 0;
	int VertexCount = 0;
	void* TileBatches = NULL;

	TileLayer();
	TileLayer(int w, int h);
	~TileLayer();
};

#endif /* ENGINE_SCENE_TILELAYER_H */
