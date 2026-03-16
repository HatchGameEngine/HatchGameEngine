#ifndef ENGINE_SCENE_LAYERGROUP_H
#define ENGINE_SCENE_LAYERGROUP_H

class LayerGroup {
public:
	bool Visible;
	float Opacity = 1.0;
	float OffsetX = 0.0;
	float OffsetY = 0.0;
	float ParallaxX = 1.0;
	float ParallaxY = 1.0;
};

#endif /* ENGINE_SCENE_LAYERGROUP_H */
