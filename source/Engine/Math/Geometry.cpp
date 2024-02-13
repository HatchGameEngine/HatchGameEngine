#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Math/GeometryTypes.h>

class Geometry {
public:
};
#endif

#include <Engine/Math/Geometry.h>
#include <Engine/Math/Math.h>

PUBLIC STATIC vector<Triangle>* Geometry::Triangulate(vector<FVector2> input) {
    unsigned count = input.size();
    if (!count || count < 3)
        return nullptr;

    vector<Triangle>* output = new vector<Triangle>();

    unsigned i = 0;

    while (count > 3) {
        unsigned prev = (i == 0) ? (count - 1) : (i - 1) % count;
        unsigned curr = i % count;
        unsigned next = (i + 1) % count;

        FVector2 a = input[prev];
        FVector2 b = input[curr];
        FVector2 c = input[next];

        bool isEar = true;

        if (Triangle::StaticGetAngle(a, b, c) < M_PI) {
            int nextNext = (i + 2) % count;
            while (isEar && nextNext != prev) {
                isEar = !Triangle::StaticIsPointInside(a, b, c, input[nextNext]);
                nextNext = (nextNext + 1) % count;
            }
        }

        if (isEar) {
            input.erase(input.begin() + curr);
            Triangle tri(a, b, c);
            output->push_back(tri);
            count--;
        }

        i++;
    }

    Triangle tri(input[0], input[1], input[2]);
    output->push_back(tri);

    return output;
}
