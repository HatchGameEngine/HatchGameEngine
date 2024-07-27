#if INTERFACE

#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Scene/ScrollingInfo.h>
#include <Engine/Scene/ScrollingIndex.h>

class SceneLayer {
public:
    char              Name[50];
    bool              Visible = true;

    int               Width = 0;
    int               Height = 0;
    Uint32            WidthMask = 0;
    Uint32            HeightMask = 0;
    Uint32            WidthInBits = 0;
    Uint32            HeightInBits = 0;
    Uint32            WidthData = 0;
    Uint32            HeightData = 0;
    Uint32            DataSize = 0;
    Uint32            ScrollIndexCount = 0;

    int               RelativeY = 0x0100;
    int               ConstantY = 0x0000;
    int               OffsetX = 0x0000;
    int               OffsetY = 0x0000;

    Uint32*           Tiles = NULL;
    Uint32*           TilesBackup = NULL;
    Uint16*           TileOffsetY = NULL;

    int               DeformOffsetA = 0;
    int               DeformOffsetB = 0;
    int               DeformSetA[MAX_DEFORM_LINES];
    int               DeformSetB[MAX_DEFORM_LINES];
    int               DeformSplitLine = 0;

    int               Flags = 0x0000;
    int               DrawGroup = 0;
    Uint8             DrawBehavior = 0;

    HashMap<VMValue>* Properties = NULL;

    bool              Blending = false;
    Uint8             BlendMode = 0; // BlendMode_NORMAL
    float             Opacity = 1.0f;

    bool              UsePaletteIndexLines = false;

    bool              UsingCustomScanlineFunction = false;
    ObjFunction       CustomScanlineFunction;

    bool              UsingCustomRenderFunction = false;
    ObjFunction       CustomRenderFunction;

    int               ScrollInfoCount = 0;
    ScrollingInfo*    ScrollInfos = NULL;
    int               ScrollInfosSplitIndexesCount = 0;
    Uint16*           ScrollInfosSplitIndexes = NULL;
    Uint8*            ScrollIndexes = NULL;

    Uint32            BufferID = 0;
    int               VertexCount = 0;
    void*             TileBatches = NULL;

    enum {
        FLAGS_COLLIDEABLE = 1,
        FLAGS_REPEAT_X = 2,
        FLAGS_REPEAT_Y = 4,
    };
};
#endif

#include <Engine/Scene/SceneLayer.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>

PUBLIC         SceneLayer::SceneLayer() {

}
PUBLIC         SceneLayer::SceneLayer(int w, int h) {
    Width = w;
    Height = h;

    w = Math::CeilPOT(w);
    WidthMask = w - 1;
    WidthInBits = 0;
    for (int f = w >> 1; f > 0; f >>= 1)
        WidthInBits++;

    h = Math::CeilPOT(h);
    HeightMask = h - 1;
    HeightInBits = 0;
    for (int f = h >> 1; f > 0; f >>= 1)
        HeightInBits++;

    WidthData = w;
    HeightData = h;
    DataSize = w * h * sizeof(Uint32);

    ScrollIndexCount = WidthData;
    if (ScrollIndexCount < HeightData)
        ScrollIndexCount = HeightData;

    memset(DeformSetA, 0, sizeof(DeformSetA));
    memset(DeformSetB, 0, sizeof(DeformSetB));

    Tiles = (Uint32*)Memory::TrackedCalloc("SceneLayer::Tiles", w * h, sizeof(Uint32));
    TilesBackup = (Uint32*)Memory::TrackedCalloc("SceneLayer::TilesBackup", w * h, sizeof(Uint32));
    ScrollIndexes = (Uint8*)Memory::Calloc(ScrollIndexCount * 16, sizeof(Uint8));
}
PUBLIC bool SceneLayer::PropertyExists(char* property) {
    return Properties && Properties->Exists(property);
}
PUBLIC VMValue SceneLayer::PropertyGet(char* property) {
    if (!PropertyExists(property))
        return NULL_VAL;
    return Properties->Get(property);
}
PUBLIC void    SceneLayer::Dispose() {
    if (Properties)
        delete Properties;
    if (ScrollInfos)
        Memory::Free(ScrollInfos);
    if (ScrollInfosSplitIndexes)
        Memory::Free(ScrollInfosSplitIndexes);

    Memory::Free(Tiles);
    Memory::Free(TilesBackup);
    Memory::Free(ScrollIndexes);
}
