#ifndef CONTOUR_H
#define CONTOUR_H

struct Contour {
    int MinX;
    int MinR;
    int MinG;
    int MinB;
    int MinU;
    int MinV;

    int MaxX;
    int MaxR;
    int MaxG;
    int MaxB;
    int MaxU;
    int MaxV;

    int MapLeft;
    int MapRight;
    float MinZ;
    float MaxZ;
};

#endif /* CONTOUR_H */
