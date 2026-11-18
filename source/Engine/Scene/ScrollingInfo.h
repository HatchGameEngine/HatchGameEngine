#ifndef ENGINE_SCENE_SCROLLINGINFO_H
#define ENGINE_SCENE_SCROLLINGINFO_H

class ScrollingInfo {
public:
	float RelativeParallax;
	float ConstantParallax;
	bool CanDeform;
	float Position;
	float Offset;
};

#endif /* ENGINE_SCENE_SCROLLINGINFO_H */
