#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/View.h>

void View::SetSize(float w, float h) {
	Width = w;
	Height = h;
	Stride = Math::CeilPOT(w);

	if (UseStencil) {
		ReallocStencil();
	}
}

void View::SetStencilEnabled(bool enabled) {
	UseStencil = enabled;
	if (!UseStencil) {
		return;
	}

	ReallocStencil();
}
void View::ReallocStencil() {
	if (DrawTarget == nullptr) {
		return;
	}

	size_t bufSize = DrawTarget->Width * DrawTarget->Height;
	if (StencilBuffer == NULL || bufSize > StencilBufferSize) {
		StencilBufferSize = bufSize;
		StencilBuffer = (Uint8*)Memory::Realloc(
			StencilBuffer, StencilBufferSize * sizeof(*StencilBuffer));
		ClearStencil();
	}
}
void View::ClearStencil() {
	if (StencilBuffer) {
		memset(StencilBuffer, 0x00, StencilBufferSize * sizeof(*StencilBuffer));
	}
}
void View::DeleteStencil() {
	Memory::Free(StencilBuffer);
	StencilBuffer = NULL;
	StencilBufferSize = 0;
}
