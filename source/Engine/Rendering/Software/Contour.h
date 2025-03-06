#ifndef CONTOUR_H
#define CONTOUR_H

struct Contour {
	int MapLeft;
	int MinX;
	int MinR;
	int MinG;
	int MinB;
	float MinU;
	float MinV;
	float MinZ;

	int MapRight;
	int MaxX;
	int MaxR;
	int MaxG;
	int MaxB;
	float MaxU;
	float MaxV;
	float MaxZ;
};

#endif /* CONTOUR_H */
