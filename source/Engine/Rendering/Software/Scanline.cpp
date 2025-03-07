#include <Engine/Rendering/Software/Contour.h>
#include <Engine/Rendering/Software/Scanline.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>

#define GET_SCANLINE_CLIP_BOUNDS(x1, y1, x2, y2) \
	if (Graphics::CurrentClip.Enabled) { \
		x1 = Graphics::CurrentClip.X; \
		y1 = Graphics::CurrentClip.Y; \
		x2 = Graphics::CurrentClip.X + Graphics::CurrentClip.Width; \
		y2 = Graphics::CurrentClip.Y + Graphics::CurrentClip.Height; \
		if (x1 < 0) \
			x1 = 0; \
		if (y1 < 0) \
			y1 = 0; \
		if (x2 > (int)Graphics::CurrentRenderTarget->Width) \
			x2 = (int)Graphics::CurrentRenderTarget->Width; \
		if (y2 > (int)Graphics::CurrentRenderTarget->Height) \
			y2 = (int)Graphics::CurrentRenderTarget->Height; \
		if (x2 < 0 || y2 < 0 || x1 >= x2 || y1 >= y2) \
			return; \
	} \
	else { \
		x1 = 0; \
		y1 = 0; \
		x2 = (int)Graphics::CurrentRenderTarget->Width; \
		y2 = (int)Graphics::CurrentRenderTarget->Height; \
	}

void Scanline::Prepare(int y1, int y2) {
	int scanLineCount = y2 - y1;
	Contour* contourPtr = &SoftwareRenderer::ContourBuffer[y1];
	while (scanLineCount--) {
		contourPtr->MinX = INT_MAX;
		contourPtr->MaxX = INT_MIN;
		contourPtr++;
	}
}

// Simple
void Scanline::Process(int x1, int y1, int x2, int y2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	int linePointSubpxX = xStart * 0x10000;
	int dx = ((xEnd - xStart) * 0x10000) / (yEnd - yStart);

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = minX;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = maxX;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = linePointX;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = linePointX;
				}
			}

			linePointSubpxX += dx;
			contour++;
		}
	}
}

// Blended
void Scanline::Process(int color1, int color2, int x1, int y1, int x2, int y2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int cStart = color1;
	int cEnd = color2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		cStart = color2;
		cEnd = color1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	int colorBegRED = (cStart & 0xFF0000);
	int colorEndRED = (cEnd & 0xFF0000);
	int colorBegGREEN = (cStart & 0xFF00) << 8;
	int colorEndGREEN = (cEnd & 0xFF00) << 8;
	int colorBegBLUE = (cStart & 0xFF) << 16;
	int colorEndBLUE = (cEnd & 0xFF) << 16;

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;
	int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

	if (colorBegRED != colorEndRED) {
		dxRED = (colorEndRED - colorBegRED) / yDiff;
	}
	if (colorBegGREEN != colorEndGREEN) {
		dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
	}
	if (colorBegBLUE != colorEndBLUE) {
		dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		colorBegRED -= yStart * dxRED;
		colorBegGREEN -= yStart * dxGREEN;
		colorBegBLUE -= yStart * dxBLUE;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = minX;
				contour->MinR = colorBegRED;
				contour->MinG = colorBegGREEN;
				contour->MinB = colorBegBLUE;
				contour->MapLeft = linePointX;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = maxX;
				contour->MaxR = colorBegRED;
				contour->MaxG = colorBegGREEN;
				contour->MaxB = colorBegBLUE;
				contour->MapRight = linePointX;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinR = colorBegRED;
					contour->MinG = colorBegGREEN;
					contour->MinB = colorBegBLUE;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxR = colorBegRED;
					contour->MaxG = colorBegGREEN;
					contour->MaxB = colorBegBLUE;
				}
			}

			linePointSubpxX += dx;
			colorBegRED += dxRED;
			colorBegGREEN += dxGREEN;
			colorBegBLUE += dxBLUE;
			contour++;
		}
	}
}

// With depth
void Scanline::ProcessDepth(int x1, int y1, int z1, int x2, int y2, int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		invZStart -= yStart * dxZ;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = minX;
				contour->MinZ = invZStart;
				contour->MapLeft = linePointX;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = maxX;
				contour->MaxZ = invZStart;
				contour->MapRight = linePointX;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinZ = invZStart;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxZ = invZStart;
				}
			}

			linePointSubpxX += dx;
			invZStart += dxZ;
			contour++;
		}
	}
}

// With depth and blending
void Scanline::ProcessDepth(int color1,
	int color2,
	int x1,
	int y1,
	int z1,
	int x2,
	int y2,
	int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	int cStart = color1;
	int cEnd = color2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
		cStart = color2;
		cEnd = color1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	int colorBegRED = (cStart & 0xFF0000);
	int colorEndRED = (cEnd & 0xFF0000);
	int colorBegGREEN = (cStart & 0xFF00) << 8;
	int colorEndGREEN = (cEnd & 0xFF00) << 8;
	int colorBegBLUE = (cStart & 0xFF) << 16;
	int colorEndBLUE = (cEnd & 0xFF) << 16;

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;
	int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

	if (colorBegRED != colorEndRED) {
		dxRED = (colorEndRED - colorBegRED) / yDiff;
	}
	if (colorBegGREEN != colorEndGREEN) {
		dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
	}
	if (colorBegBLUE != colorEndBLUE) {
		dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;
	}

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		colorBegRED -= yStart * dxRED;
		colorBegGREEN -= yStart * dxGREEN;
		colorBegBLUE -= yStart * dxBLUE;
		invZStart -= yStart * dxZ;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = contour->MapLeft = minX;
				contour->MinZ = invZStart;
				contour->MinR = colorBegRED;
				contour->MinG = colorBegGREEN;
				contour->MinB = colorBegBLUE;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = contour->MapRight = maxX;
				contour->MaxZ = invZStart;
				contour->MaxR = colorBegRED;
				contour->MaxG = colorBegGREEN;
				contour->MaxB = colorBegBLUE;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinZ = invZStart;
					contour->MinR = colorBegRED;
					contour->MinG = colorBegGREEN;
					contour->MinB = colorBegBLUE;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxZ = invZStart;
					contour->MaxR = colorBegRED;
					contour->MaxG = colorBegGREEN;
					contour->MaxB = colorBegBLUE;
				}
			}

			linePointSubpxX += dx;
			invZStart += dxZ;
			colorBegRED += dxRED;
			colorBegGREEN += dxGREEN;
			colorBegBLUE += dxBLUE;
			contour++;
		}
	}
}

// Textured affine
void Scanline::ProcessUVAffine(Vector2 uv1,
	Vector2 uv2,
	int x1,
	int y1,
	int z1,
	int x2,
	int y2,
	int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	Vector2 tStart = uv1;
	Vector2 tEnd = uv2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
		tStart = uv2;
		tEnd = uv1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	int texBegU = tStart.X;
	int texBegV = tStart.Y;
	int texEndU = tEnd.X;
	int texEndV = tEnd.Y;

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;

	int dxU = 0, dxV = 0;

	if (texBegU != texEndU) {
		dxU = (texEndU - texBegU) / yDiff;
	}
	if (texBegV != texEndV) {
		dxV = (texEndV - texBegV) / yDiff;
	}

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		invZStart -= yStart * dxZ;
		texBegU -= yStart * dxU;
		texBegV -= yStart * dxV;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = contour->MapLeft = linePointX;
				contour->MinZ = invZStart;
				contour->MinU = texBegU;
				contour->MinV = texBegV;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = contour->MapRight = linePointX;
				contour->MaxZ = invZStart;
				contour->MaxU = texBegU;
				contour->MaxV = texBegV;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinZ = invZStart;
					contour->MinU = texBegU;
					contour->MinV = texBegV;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxZ = invZStart;
					contour->MaxU = texBegU;
					contour->MaxV = texBegV;
				}
			}

			linePointSubpxX += dx;
			invZStart += dxZ;
			texBegU += dxU;
			texBegV += dxV;
			contour++;
		}
	}
}

// Textured affine with blending
void Scanline::ProcessUVAffine(int color1,
	int color2,
	Vector2 uv1,
	Vector2 uv2,
	int x1,
	int y1,
	int z1,
	int x2,
	int y2,
	int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	int cStart = color1;
	int cEnd = color2;
	Vector2 tStart = uv1;
	Vector2 tEnd = uv2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
		cStart = color2;
		cEnd = color1;
		tStart = uv2;
		tEnd = uv1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	int colorBegRED = (cStart & 0xFF0000);
	int colorEndRED = (cEnd & 0xFF0000);
	int colorBegGREEN = (cStart & 0xFF00) << 8;
	int colorEndGREEN = (cEnd & 0xFF00) << 8;
	int colorBegBLUE = (cStart & 0xFF) << 16;
	int colorEndBLUE = (cEnd & 0xFF) << 16;

	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	int texBegU = tStart.X;
	int texBegV = tStart.Y;
	int texEndU = tEnd.X;
	int texEndV = tEnd.Y;

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;

	int dxRED = 0, dxGREEN = 0, dxBLUE = 0;

	if (colorBegRED != colorEndRED) {
		dxRED = (colorEndRED - colorBegRED) / yDiff;
	}
	if (colorBegGREEN != colorEndGREEN) {
		dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
	}
	if (colorBegBLUE != colorEndBLUE) {
		dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;
	}

	int dxU = 0, dxV = 0;

	if (texBegU != texEndU) {
		dxU = (texEndU - texBegU) / yDiff;
	}
	if (texBegV != texEndV) {
		dxV = (texEndV - texBegV) / yDiff;
	}

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		invZStart -= yStart * dxZ;
		colorBegRED -= yStart * dxRED;
		colorBegGREEN -= yStart * dxGREEN;
		colorBegBLUE -= yStart * dxBLUE;
		texBegU -= yStart * dxU;
		texBegV -= yStart * dxV;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = contour->MapLeft = linePointX;
				contour->MinR = colorBegRED;
				contour->MinG = colorBegGREEN;
				contour->MinB = colorBegBLUE;
				contour->MinZ = invZStart;
				contour->MinU = texBegU;
				contour->MinV = texBegV;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = contour->MapRight = linePointX;
				contour->MaxR = colorBegRED;
				contour->MaxG = colorBegGREEN;
				contour->MaxB = colorBegBLUE;
				contour->MaxZ = invZStart;
				contour->MaxU = texBegU;
				contour->MaxV = texBegV;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinR = colorBegRED;
					contour->MinG = colorBegGREEN;
					contour->MinB = colorBegBLUE;
					contour->MinZ = invZStart;
					contour->MinU = texBegU;
					contour->MinV = texBegV;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxR = colorBegRED;
					contour->MaxG = colorBegGREEN;
					contour->MaxB = colorBegBLUE;
					contour->MaxZ = invZStart;
					contour->MaxU = texBegU;
					contour->MaxV = texBegV;
				}
			}

			linePointSubpxX += dx;
			invZStart += dxZ;
			colorBegRED += dxRED;
			colorBegGREEN += dxGREEN;
			colorBegBLUE += dxBLUE;
			texBegU += dxU;
			texBegV += dxV;
			contour++;
		}
	}
}

// Perspective correct
void Scanline::ProcessUV(Vector2 uv1, Vector2 uv2, int x1, int y1, int z1, int x2, int y2, int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	Vector2 tStart = uv1;
	Vector2 tEnd = uv2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
		tStart = uv2;
		tEnd = uv1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	// We can interpolate 1/z, u/z, v/z because they're linear in
	// screen space To index the texture, we calculate 1 / (1/z)
	// and multiply that by u/z and v/z
	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	float texBegU = (float)tStart.X / zStart; // u/z
	float texBegV = (float)tStart.Y / zStart; // v/z
	float texEndU = (float)tEnd.X / zEnd; // u/z at end
	float texEndV = (float)tEnd.Y / zEnd; // v/z at end

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;

	float texMapU = texBegU;
	float texMapV = texBegV;
	float dxU = 0, dxV = 0;

	if (texBegU != texEndU) {
		dxU = (texEndU - texBegU) / yDiff;
	}
	if (texBegV != texEndV) {
		dxV = (texEndV - texBegV) / yDiff;
	}

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		invZStart -= yStart * dxZ;
		texMapU -= yStart * dxU;
		texMapV -= yStart * dxV;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = minX;
				contour->MinZ = invZStart;
				contour->MinU = texMapU;
				contour->MinV = texMapV;
				contour->MapLeft = linePointX;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = maxX;
				contour->MaxZ = invZStart;
				contour->MaxU = texMapU;
				contour->MaxV = texMapV;
				contour->MapRight = linePointX;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinZ = invZStart;
					contour->MinU = texMapU;
					contour->MinV = texMapV;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxZ = invZStart;
					contour->MaxU = texMapU;
					contour->MaxV = texMapV;
				}
			}

			linePointSubpxX += dx;
			invZStart += dxZ;
			texMapU += dxU;
			texMapV += dxV;
			contour++;
		}
	}
}

// Perspective correct with blending
void Scanline::ProcessUV(int color1,
	int color2,
	Vector2 uv1,
	Vector2 uv2,
	int x1,
	int y1,
	int z1,
	int x2,
	int y2,
	int z2) {
	int xStart = x1 / 0x10000;
	int xEnd = x2 / 0x10000;
	int yStart = y1 / 0x10000;
	int yEnd = y2 / 0x10000;
	int zStart = z1 / 0x10000;
	int zEnd = z2 / 0x10000;
	int cStart = color1;
	int cEnd = color2;
	Vector2 tStart = uv1;
	Vector2 tEnd = uv2;
	if (yStart == yEnd) {
		return;
	}

	// swap
	if (yStart > yEnd) {
		xStart = x2 / 0x10000;
		xEnd = x1 / 0x10000;
		yStart = y2 / 0x10000;
		yEnd = y1 / 0x10000;
		zStart = z2 / 0x10000;
		zEnd = z1 / 0x10000;
		cStart = color2;
		cEnd = color1;
		tStart = uv2;
		tEnd = uv1;
	}

	int minX, minY, maxX, maxY;
	GET_SCANLINE_CLIP_BOUNDS(minX, minY, maxX, maxY);

	int yEndBound = yEnd + 1;
	if (yEndBound < minY || yStart >= maxY) {
		return;
	}

	if (yEndBound > maxY) {
		yEndBound = maxY;
	}

	float colorBegRED = (cStart & 0xFF0000) / zStart;
	float colorEndRED = (cEnd & 0xFF0000) / zEnd;
	float colorBegGREEN = ((cStart & 0xFF00) << 8) / zStart;
	float colorEndGREEN = ((cEnd & 0xFF00) << 8) / zEnd;
	float colorBegBLUE = ((cStart & 0xFF) * 0x10000) / zStart;
	float colorEndBLUE = ((cEnd & 0xFF) * 0x10000) / zEnd;

	float invZStart = 1.0f / zStart; // 1/z
	float invZEnd = 1.0f / zEnd; // 1/z at end

	float texBegU = (float)tStart.X / zStart; // u/z
	float texBegV = (float)tStart.Y / zStart; // v/z
	float texEndU = (float)tEnd.X / zEnd; // u/z at end
	float texEndV = (float)tEnd.Y / zEnd; // v/z at end

	int linePointSubpxX = xStart * 0x10000;
	int yDiff = (yEnd - yStart);
	int dx = ((xEnd - xStart) * 0x10000) / yDiff;

	float colorRED = colorBegRED;
	float colorGREEN = colorBegGREEN;
	float colorBLUE = colorBegBLUE;
	float dxRED = 0, dxGREEN = 0, dxBLUE = 0;

	if (colorBegRED != colorEndRED) {
		dxRED = (colorEndRED - colorBegRED) / yDiff;
	}
	if (colorBegGREEN != colorEndGREEN) {
		dxGREEN = (colorEndGREEN - colorBegGREEN) / yDiff;
	}
	if (colorBegBLUE != colorEndBLUE) {
		dxBLUE = (colorEndBLUE - colorBegBLUE) / yDiff;
	}

	float texMapU = texBegU;
	float texMapV = texBegV;
	float dxU = 0, dxV = 0;

	if (texBegU != texEndU) {
		dxU = (texEndU - texBegU) / yDiff;
	}
	if (texBegV != texEndV) {
		dxV = (texEndV - texBegV) / yDiff;
	}

	float dxZ = 0;
	if (invZStart != invZEnd) {
		dxZ = (invZEnd - invZStart) / yDiff;
	}

	if (yStart < 0) {
		linePointSubpxX -= yStart * dx;
		colorRED -= yStart * dxRED;
		colorGREEN -= yStart * dxGREEN;
		colorBLUE -= yStart * dxBLUE;
		invZStart -= yStart * dxZ;
		texMapU -= yStart * dxU;
		texMapV -= yStart * dxV;
		yStart = 0;
	}

	Contour* contour = &SoftwareRenderer::ContourBuffer[yStart];
	if (yStart < yEndBound) {
		int lineHeight = yEndBound - yStart;
		while (lineHeight--) {
			int linePointX = linePointSubpxX / 0x10000;

			if (linePointX <= minX) {
				contour->MinX = minX;
				contour->MinR = colorRED;
				contour->MinG = colorGREEN;
				contour->MinB = colorBLUE;
				contour->MinZ = invZStart;
				contour->MinU = texMapU;
				contour->MinV = texMapV;
				contour->MapLeft = linePointX;
			}
			else if (linePointX >= maxX) {
				contour->MaxX = maxX;
				contour->MaxR = colorRED;
				contour->MaxG = colorGREEN;
				contour->MaxB = colorBLUE;
				contour->MaxZ = invZStart;
				contour->MaxU = texMapU;
				contour->MaxV = texMapV;
				contour->MapRight = linePointX;
			}
			else {
				if (linePointX < contour->MinX) {
					contour->MinX = contour->MapLeft = linePointX;
					contour->MinR = colorRED;
					contour->MinG = colorGREEN;
					contour->MinB = colorBLUE;
					contour->MinZ = invZStart;
					contour->MinU = texMapU;
					contour->MinV = texMapV;
				}
				if (linePointX > contour->MaxX) {
					contour->MaxX = contour->MapRight = linePointX;
					contour->MaxR = colorRED;
					contour->MaxG = colorGREEN;
					contour->MaxB = colorBLUE;
					contour->MaxZ = invZStart;
					contour->MaxU = texMapU;
					contour->MaxV = texMapV;
				}
			}

			linePointSubpxX += dx;
			colorRED += dxRED;
			colorGREEN += dxGREEN;
			colorBLUE += dxBLUE;
			invZStart += dxZ;
			texMapU += dxU;
			texMapV += dxV;
			contour++;
		}
	}
}
