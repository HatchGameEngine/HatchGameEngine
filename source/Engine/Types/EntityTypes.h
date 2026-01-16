#ifndef ENTITYTYPES_H
#define ENTITYTYPES_H

#include <Engine/Types/Collision.h>

enum { Persistence_NONE, Persistence_SCENE, Persistence_GAME };

enum {
	ACTIVE_NEVER = 0, // Never updates
	ACTIVE_ALWAYS = 1, // Updates no matter what
	ACTIVE_NORMAL = 2, // Updates no matter where the object is in
	// the scene, but not if scene is paused
	ACTIVE_PAUSED = 3, // Only updates when the scene is paused
	ACTIVE_BOUNDS = 4, // Updates only when the object is within bounds
	ACTIVE_XBOUNDS = 5, // Updates within an x bound (not
	// accounting for y bound)
	ACTIVE_YBOUNDS = 6, // Updates within a y bound (not accounting
	// for x bound)
	ACTIVE_RBOUNDS = 7, // Updates within a radius (UpdateRegionW)
	ACTIVE_DISABLED =
		0XFF, // For stopping entities from even checking for an update in some cases
};

namespace CollideSide {
enum {
	NONE = 0,
	LEFT = 8,
	BOTTOM = 4,
	RIGHT = 2,
	TOP = 1,

	SIDES = 10,
	TOP_SIDES = 11,
	BOTTOM_SIDES = 14,
};
};

struct Sensor {
	int X;
	int Y;
	int Collided;
	int Angle;
};

struct CollisionSensor {
	float X;
	float Y;
	int Collided;
	int Angle;
};

struct ObjectListPerformanceStats {
	double AverageTime = 0.0;
	double AverageItemCount = 0;

	void DoAverage(double elapsed) {
		double count = AverageItemCount;
		if (count < 60.0 * 60.0) {
			count += 1.0;
			if (count == 1.0) {
				AverageTime = elapsed;
			}
			else {
				AverageTime = AverageTime + (elapsed - AverageTime) / count;
			}
			AverageItemCount = count;
		}
	}

	double GetAverageTime() { return AverageTime * 1000.0; }

	double GetTotalAverageTime() { return GetAverageTime() * AverageItemCount; }

	void Clear() {
		AverageTime = 0.0;
		AverageItemCount = 0;
	}
};

struct ObjectListPerformance {
	ObjectListPerformanceStats EarlyUpdate;
	ObjectListPerformanceStats Update;
	ObjectListPerformanceStats LateUpdate;
	ObjectListPerformanceStats Render;

	void Clear() {
		EarlyUpdate.Clear();
		Update.Clear();
		LateUpdate.Clear();
		Render.Clear();
	}
};

#define VIEWABLE_HITBOX_COUNT 0x400

struct ViewableHitbox {
	int Type;
	int Collision;
	Entity* Instance;
	CollisionBox Hitbox;
	int X;
	int Y;
};

#endif /* ENTITYTYPES_H */
