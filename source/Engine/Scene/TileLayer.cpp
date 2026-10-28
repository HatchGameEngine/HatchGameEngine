#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/TileLayer.h>

TileLayer::TileLayer() {}
TileLayer::TileLayer(int w, int h, bool allocTilesBackupData) {
	Type = SceneLayer::TYPE_TILE;

	SetSize(w, h);

	memset(DeformSetA, 0, sizeof(DeformSetA));
	memset(DeformSetB, 0, sizeof(DeformSetB));

	Tiles = (Uint32*)Memory::TrackedCalloc("TileLayer::Tiles", WidthData * HeightData, sizeof(Uint32));
	if (allocTilesBackupData) {
		AllocTilesBackup();
	}

	UsingScrollIndexes = false;
}

void TileLayer::SetSize(int w, int h) {
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
}

void TileLayer::Resize(int w, int h) {
	size_t copyWidth = std::min(w, Width) * sizeof(Uint32);
	size_t copyHeight = std::min(h, Height);
	size_t oldWidthData = WidthData;

	SetSize(w, h);

	Uint32* newTileData = (Uint32*)Memory::TrackedCalloc("TileLayer::Tiles", WidthData * HeightData, sizeof(Uint32));
	Uint32* newTileBackupData = nullptr;

	if (TilesBackup) {
		newTileBackupData = (Uint32*)Memory::TrackedCalloc("TileLayer::TilesBackup", WidthData * HeightData, sizeof(Uint32));
	}

	for (size_t i = 0, iH = 0; i < copyHeight; i++) {
		memcpy(&newTileData[iH], &Tiles[i * oldWidthData], copyWidth);

		if (newTileBackupData != nullptr) {
			memcpy(&newTileBackupData[iH], &TilesBackup[i * oldWidthData], copyWidth);
		}

		iH += WidthData;
	}

	Memory::Free(Tiles);
	Memory::Free(TilesBackup);

	Tiles = newTileData;
	if (newTileBackupData != nullptr) {
		TilesBackup = newTileBackupData;
	}
}

void TileLayer::AllocTilesBackup() {
	TilesBackup =
		(Uint32*)Memory::TrackedCalloc("TileLayer::TilesBackup", WidthData * HeightData, sizeof(Uint32));
}

TileLayer::~TileLayer() {
	Memory::Free(Tiles);
	Memory::Free(TilesBackup);
	Memory::Free(TileBufferIndexes);
	Memory::Free(ScrollIndexes);
	Memory::Free(ScrollInfos);
}
