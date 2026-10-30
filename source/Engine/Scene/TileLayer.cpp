#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/TileLayer.h>

TileLayer::TileLayer() {}
TileLayer::TileLayer(int w, int h) {
	Type = SceneLayer::TYPE_TILE;

	SetSize(w, h);

	memset(DeformSetA, 0, sizeof(DeformSetA));
	memset(DeformSetB, 0, sizeof(DeformSetB));

	Tiles = (Uint32*)Memory::TrackedCalloc("TileLayer::Tiles", WidthData * HeightData, sizeof(Uint32));

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

void TileLayer::MakeTilesBackup() {
	if (TilesBackup) {
		Memory::Free(TilesBackup);
	}

	TilesBackup = (Uint32*)Memory::TrackedMalloc("TileLayer::TilesBackup", DataSize);
	memcpy(TilesBackup, Tiles, DataSize);
}

SceneLayer* TileLayer::Duplicate() {
	TileLayer* copy = new TileLayer(Width, Height);

	// Copy properties
	CopyFields(copy);

	// Copy tile data
	memcpy(copy->Tiles, Tiles, DataSize);

	if (TilesBackup) {
		copy->TilesBackup = (Uint32*)Memory::TrackedMalloc("TileLayer::TilesBackup", DataSize);
		memcpy(copy->TilesBackup, TilesBackup, DataSize);
	}

	// Copy scroll info
	if (UsingScrollIndexes) {
		int length16 = HeightData * 16;
		if (WidthData > HeightData) {
			length16 = WidthData * 16;
		}

		copy->UsingScrollIndexes = true;
		copy->ScrollIndexCount = ScrollIndexCount;
		copy->ScrollIndexes = (Uint8*)Memory::Malloc(ScrollIndexCount * 16);
		memcpy(copy->ScrollIndexes, ScrollIndexes, ScrollIndexCount * 16);
	}

	if (ScrollInfos) {
		copy->ScrollInfoCount = ScrollInfoCount;
		copy->ScrollInfos = (ScrollingInfo*)Memory::Malloc(ScrollInfoCount * sizeof(ScrollingInfo));
		memcpy(copy->ScrollInfos, ScrollInfos, ScrollInfoCount * sizeof(ScrollingInfo));
	}

	// Copy deformation data
	copy->DeformOffsetA = DeformOffsetA;
	copy->DeformOffsetB = DeformOffsetB;
	copy->DeformSplitLine = DeformSplitLine;

	memcpy(copy->DeformSetA, DeformSetA, sizeof(DeformSetA));
	memcpy(copy->DeformSetB, DeformSetB, sizeof(DeformSetB));

	// Copy custom scanline function
	copy->UsingCustomScanlineFunction = UsingCustomScanlineFunction;
	copy->CustomScanlineFunction = CustomScanlineFunction;

	return (SceneLayer*)copy;
}

TileLayer::~TileLayer() {
	Memory::Free(Tiles);
	Memory::Free(TilesBackup);
	Memory::Free(TileBufferIndexes);
	Memory::Free(ScrollIndexes);
	Memory::Free(ScrollInfos);
}
