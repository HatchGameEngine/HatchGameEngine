#ifndef ENGINE_SCENE_LAYERTILEBUFFERS_H
#define ENGINE_SCENE_LAYERTILEBUFFERS_H

struct LayerTextureBatch {
	size_t NumTiles = 0;
	Uint32 BufferID = 0;
	void* Data = nullptr;
};

struct LayerTileBuffers {
	std::unordered_map<Texture*, LayerTextureBatch> TextureBatches;
};

#endif /* ENGINE_SCENE_LAYERTILEBUFFERS_H */
