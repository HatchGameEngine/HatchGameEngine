#ifndef SOFTWAREENUMS_H
#define SOFTWAREENUMS_H

enum BlendFlags {
    BlendFlag_OPAQUE = 0,
    BlendFlag_TRANSPARENT,
    BlendFlag_ADDITIVE,
    BlendFlag_SUBTRACT,
    BlendFlag_MATCH_EQUAL,
    BlendFlag_MATCH_NOT_EQUAL,

    BlendFlag_MODE_MASK = 7,
    BlendFlag_TINT_BIT = 8,
    BlendFlag_FILTER_BIT = 16
};

#endif /* SOFTWAREENUMS_H */
