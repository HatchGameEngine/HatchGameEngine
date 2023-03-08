#ifndef SOFTWAREENUMS_H
#define SOFTWAREENUMS_H

enum BlendFlags {
    BlendFlag_OPAQUE = 0,
    BlendFlag_TRANSPARENT,
    BlendFlag_ADDITIVE,
    BlendFlag_SUBTRACT,
    BlendFlag_MATCH_EQUAL,
    BlendFlag_MATCH_NOT_EQUAL,

    BlendFlag_MODE_MASK = 7, // 0b111
    BlendFlag_TINT_BIT = 8,
    BlendFlag_FILTER_BIT = 16
};

struct BlendState {
    int Opacity;
    int Flags;
    struct {
        bool Enabled;
        Uint32 Color;
        Uint16 Amount;
        Uint8 Mode;
    } Tint;
    int *FilterTable;
};

#endif /* SOFTWAREENUMS_H */
