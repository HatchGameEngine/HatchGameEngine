#ifndef ENGINE_SCENE_IMAGELAYER_H
#define ENGINE_SCENE_IMAGELAYER_H

#include <Engine/ResourceTypes/Image.h>
#include <Engine/Scene/SceneLayer.h>

class ImageLayer : public SceneLayer {
public:
	Image* ImagePtr = nullptr;

	ImageLayer();
	ImageLayer(int w, int h);

	SceneLayer* Duplicate();

	~ImageLayer();
};

#endif /* ENGINE_SCENE_IMAGELAYER_H */
