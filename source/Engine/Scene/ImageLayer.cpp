#include <Engine/Scene/ImageLayer.h>

ImageLayer::ImageLayer() {}
ImageLayer::ImageLayer(int w, int h) {
	Type = SceneLayer::TYPE_IMAGE;

	Width = w;
	Height = h;
}
ImageLayer::~ImageLayer() {
	if (ImagePtr) {
		delete ImagePtr;
	}
}
