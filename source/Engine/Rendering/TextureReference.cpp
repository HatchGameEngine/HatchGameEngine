#include <Engine/Rendering/TextureReference.h>

TextureReference::TextureReference(Texture* ptr) {
	TexturePtr = ptr;
	References = 0;
	AddRef();
}

void TextureReference::AddRef() {
	References++;
}

bool TextureReference::TakeRef() {
	if (References == 0) {
		abort();
	}

	References--;

	return References == 0;
}
