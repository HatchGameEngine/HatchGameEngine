#ifndef ENGINE_SCENE_SCROLLINGINFO_H
#define ENGINE_SCENE_SCROLLINGINFO_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED



class ScrollingInfo {
public:
    int RelativeParallax;
    int ConstantParallax;
    char CanDeform;
    int Position;
    int Offset;

};

#endif /* ENGINE_SCENE_SCROLLINGINFO_H */
