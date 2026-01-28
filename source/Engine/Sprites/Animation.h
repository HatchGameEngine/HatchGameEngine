#ifndef ENGINE_SPRITES_ANIMATION_H
#define ENGINE_SPRITES_ANIMATION_H

struct CollisionBox {
	std::string Name;
	int Left = 0;
	int Top = 0;
	int Right = 0;
	int Bottom = 0;
};
struct AnimFrame {
	int X;
	int Y;
	int Width;
	int Height;
	int OffsetX;
	int OffsetY;
	int SheetNumber;
	int Duration;
	int ID;
	int BufferOffset;

	std::vector<CollisionBox> Boxes;
};
struct Animation {
	std::string Name;
	int Speed;
	int FrameToLoop;
	int Flags;
	vector<AnimFrame> Frames;
};

enum {
	ROTSTYLE_NONE = 0,
	ROTSTYLE_FULL = 1,
	ROTSTYLE_45DEG = 2,
	ROTSTYLE_90DEG = 3,
	ROTSTYLE_180DEG = 4,
	ROTSTYLE_STATICFRAMES = 5,
};

#endif /* ENGINE_SPRITES_ANIMATION_H */
