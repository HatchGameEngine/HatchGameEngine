#include <Engine/Diagnostics/Memory.h>
#include <Engine/Math/Math.h>
#include <Engine/Scene/View.h>

void View::SetSize(float w, float h) {
	Width = w;
	Height = h;
	Stride = Math::CeilPOT(w);

	if (Software) {
		ReallocStencil();
	}
}

float View::GetScaledWidth() {
	float scale = fabs(ScaleX);
	if (scale < 0.01) {
		scale = 0.01;
	}

	return Width * (1.0 / scale);
}
float View::GetScaledHeight() {
	float scale = fabs(ScaleY);
	if (scale < 0.01) {
		scale = 0.01;
	}

	return Height * (1.0 / scale);
}

void View::SetStencilEnabled(bool enabled) {
	UseStencil = enabled;
}
void View::ReallocStencil() {
	if (!UseStencil || DrawTarget == nullptr) {
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
