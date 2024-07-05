#if INTERFACE
#include <Engine/Includes/Standard.h>

need_t Texture;

class TextureReference {
public:
    Texture* TexturePtr;

    unsigned References;
};
#endif

#include <Engine/Rendering/TextureReference.h>

PUBLIC TextureReference::TextureReference(Texture *ptr) {
    TexturePtr = ptr;
    References = 0;
    AddRef();
}

PUBLIC void TextureReference::AddRef() {
    References++;
}

PUBLIC bool TextureReference::TakeRef() {
    if (References == 0)
        abort();

    References--;

    return References == 0;
}
