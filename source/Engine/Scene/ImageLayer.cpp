#include <Engine/Scene/ImageLayer.h>

ImageLayer::ImageLayer() {}
ImageLayer::ImageLayer(int w, int h) {
	Type = SceneLayer::TYPE_IMAGE;

	Width = w;
	Height = h;
}

SceneLayer* ImageLayer::Duplicate() {
	ImageLayer* copy = new ImageLayer(Width, Height);

	// Copy properties
	CopyFields(copy);

	// Copy image
	if (ImagePtr) {
		copy->ImagePtr = new Image(ImagePtr->Filename);
	}

	return (SceneLayer*)copy;
}

ImageLayer::~ImageLayer() {
	if (ImagePtr) {
		delete ImagePtr;
	}
}
