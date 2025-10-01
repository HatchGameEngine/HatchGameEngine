#include <Engine/Error.h>
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
		Error::Fatal("Tried to release reference of TextureReference when it had none!");
	}

	References--;

	return References == 0;
}
