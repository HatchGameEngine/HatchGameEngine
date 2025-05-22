#include <Engine/Rendering/TextureReference.h>

TextureReference::TextureReference(Texture* ptr) {
	TexturePtr = ptr;
	References = 0;
	TakeRef();
}

void TextureReference::TakeRef() {
	References++;
}

bool TextureReference::ReleaseRef() {
	if (References == 0) {
		abort();
	}

	References--;

	return References == 0;
}
