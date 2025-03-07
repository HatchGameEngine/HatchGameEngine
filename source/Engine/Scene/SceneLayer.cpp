#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/SceneLayer.h>

SceneLayer::SceneLayer() {}
SceneLayer::SceneLayer(int w, int h) {
	Width = w;
	Height = h;

	w = Math::CeilPOT(w);
	WidthMask = w - 1;
	WidthInBits = 0;
	for (int f = w >> 1; f > 0; f >>= 1) {
		WidthInBits++;
	}

	h = Math::CeilPOT(h);
	HeightMask = h - 1;
	HeightInBits = 0;
	for (int f = h >> 1; f > 0; f >>= 1) {
		HeightInBits++;
	}

	WidthData = w;
	HeightData = h;
	DataSize = w * h * sizeof(Uint32);

	ScrollIndexCount = WidthData;
	if (ScrollIndexCount < HeightData) {
		ScrollIndexCount = HeightData;
	}

	memset(DeformSetA, 0, sizeof(DeformSetA));
	memset(DeformSetB, 0, sizeof(DeformSetB));

	Tiles = (Uint32*)Memory::TrackedCalloc("SceneLayer::Tiles", w * h, sizeof(Uint32));
	TilesBackup =
		(Uint32*)Memory::TrackedCalloc("SceneLayer::TilesBackup", w * h, sizeof(Uint32));
	ScrollIndexes = (Uint8*)Memory::Calloc(ScrollIndexCount * 16, sizeof(Uint8));
}
bool SceneLayer::PropertyExists(char* property) {
	return Properties && Properties->Exists(property);
}
VMValue SceneLayer::PropertyGet(char* property) {
	if (!PropertyExists(property)) {
		return NULL_VAL;
	}
	return Properties->Get(property);
}
void SceneLayer::Dispose() {
	if (Properties) {
		delete Properties;
	}
	if (ScrollInfos) {
		Memory::Free(ScrollInfos);
	}
	if (ScrollInfosSplitIndexes) {
		Memory::Free(ScrollInfosSplitIndexes);
	}

	Memory::Free(Tiles);
	Memory::Free(TilesBackup);
	Memory::Free(ScrollIndexes);
}
