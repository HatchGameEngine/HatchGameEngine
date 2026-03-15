#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/TileLayer.h>

TileLayer::TileLayer() {}
TileLayer::TileLayer(int w, int h) {
	Type = SceneLayer::TYPE_TILE;

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

	Tiles = (Uint32*)Memory::TrackedCalloc("TileLayer::Tiles", w * h, sizeof(Uint32));
	TilesBackup =
		(Uint32*)Memory::TrackedCalloc("TileLayer::TilesBackup", w * h, sizeof(Uint32));
	UsingScrollIndexes = false;
}
TileLayer::~TileLayer() {
	if (ScrollInfos) {
		Memory::Free(ScrollInfos);
	}

	Memory::Free(Tiles);
	Memory::Free(TilesBackup);
	Memory::Free(ScrollIndexes);
}
