#ifndef COLLISION_H
#define COLLISION_H

struct EntityHitbox {
	float Width = 0.0f;
	float Height = 0.0f;
	float OffsetX = 0.0f;
	float OffsetY = 0.0f;

	inline float GetLeft(bool flip = false) {
		return (-Width / 2) + (flip ? -OffsetX : OffsetX);
	}
	inline float GetRight(bool flip = false) {
		return (Width / 2) + (flip ? -OffsetX : OffsetX);
	}
	inline float GetTop(bool flip = false) {
		return (-Height / 2) + (flip ? -OffsetY : OffsetY);
	}
	inline float GetBottom(bool flip = false) {
		return (Height / 2) + (flip ? -OffsetY : OffsetY);
	}

	void SetLeft(float left) {
		Width = GetRight() - left;
		OffsetX = left + Width * 0.5f;
	}
	void SetRight(float right) {
		Width = right - GetLeft();
		OffsetX = right - Width * 0.5f;
	}
	void SetTop(float top) {
		Height = GetBottom() - top;
		OffsetY = top + Height * 0.5f;
	}
	void SetBottom(float bottom) {
		Height = bottom - GetTop();
		OffsetY = bottom - Height * 0.5f;
	}

	void Clear() {
		Width = 0.0f;
		Height = 0.0f;
		OffsetX = 0.0f;
		OffsetY = 0.0f;
	}

	void Set(CollisionBox box) {
		Width = box.Right - box.Left;
		Height = box.Bottom - box.Top;
		OffsetX = box.Left + Width * 0.5f;
		OffsetY = box.Top + Height * 0.5f;
	}
};

#endif
