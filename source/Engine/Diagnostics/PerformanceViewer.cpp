#include <Engine/Application.h>
#include <Engine/Diagnostics/PerformanceViewer.h>
#include <Engine/Graphics.h>

void PerformanceViewer::DrawDetailed(Font* font) {
	int ww, wh;
	SDL_GetWindowSize(Application::Window, &ww, &wh);
	Graphics::SetViewport(0.0, 0.0, ww, wh);
	Graphics::UpdateOrthoFlipped(ww, wh);
	Graphics::SetBlendMode(BlendFactor_SRC_ALPHA,
		BlendFactor_INV_SRC_ALPHA,
		BlendFactor_ONE,
		BlendFactor_INV_SRC_ALPHA);

	TextDrawParams textParams;
	textParams.FontSize = font->Size;
	textParams.Ascent = font->Ascent;
	textParams.Descent = font->Descent;
	textParams.Leading = font->Leading;

	char textBuffer[256];

	size_t typeCount = Application::AllMetrics.size();

	float infoW = 400.0;
	float infoH = 150.0;
	float infoPadding = 20.0;

	for (size_t i = 0; i < typeCount; i++) {
		infoH += 20.0;
	}

	if (Memory::IsTracking) {
		infoH += 20.0;
	}

	Graphics::Save();
	Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.75);
	Graphics::FillRectangle(0.0, 0.0, infoW, infoH);

	float textX = 0.0;
	float textY = font->Descent;

	Graphics::Save();
	Graphics::Translate(infoPadding - 2.0, infoPadding, 0.0);
	Graphics::Scale(0.85, 0.85, 1.0);
	snprintf(textBuffer, sizeof textBuffer, "Frame Information");
	Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
	Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
	Graphics::Restore();

	Graphics::Save();
	Graphics::Translate(infoW - infoPadding - (8 * 16.0 * 0.85), infoPadding, 0.0);
	Graphics::Scale(0.85, 0.85, 1.0);
	snprintf(textBuffer, sizeof textBuffer, "FPS: %03.1f", Application::CurrentFPS);
	Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
	Graphics::Restore();

	// Draw bar
	double total = 0.0001;
	for (size_t i = 0; i < typeCount; i++) {
		PerformanceMeasure* measure = Application::AllMetrics[i];
		float time = measure->Time;
		if (time < 0.0) {
			time = 0.0;
		}
		total += time;
	}

	Graphics::Save();
	Graphics::Translate(infoPadding, 50.0, 0.0);
	Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.25);
	Graphics::FillRectangle(0.0, 0.0, infoW - infoPadding * 2, 30.0);
	Graphics::Restore();

	double rectx = 0.0;
	for (size_t i = 0; i < typeCount; i++) {
		PerformanceMeasure* measure = Application::AllMetrics[i];

		Graphics::Save();
		Graphics::Translate(infoPadding, 50.0, 0.0);
		Graphics::SetBlendColor(measure->Colors.R, measure->Colors.G, measure->Colors.B, 0.5);
		Graphics::FillRectangle(
			rectx, 0.0, measure->Time / total * (infoW - infoPadding * 2), 30.0);
		Graphics::Restore();

		rectx += measure->Time / total * (infoW - infoPadding * 2);
	}

	// Draw list
	float listY = 90.0;
	double totalFrameCount = 0.0;
	infoPadding += infoPadding;
	for (size_t i = 0; i < typeCount; i++) {
		PerformanceMeasure* measure = Application::AllMetrics[i];

		Graphics::Save();
		Graphics::Translate(infoPadding, listY, 0.0);
		Graphics::SetBlendColor(measure->Colors.R, measure->Colors.G, measure->Colors.B, 0.5);
		Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
		Graphics::Scale(0.6, 0.6, 1.0);
		snprintf(textBuffer, sizeof textBuffer, "%s: %3.3f ms", measure->Name, measure->Time);
		Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
		Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
		listY += 20.0;
		Graphics::Restore();

		totalFrameCount += measure->Time;
	}

	// Draw total
	Graphics::Save();
	Graphics::Translate(infoPadding, listY, 0.0);
	Graphics::SetBlendColor(1.0, 1.0, 1.0, 0.5);
	Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
	Graphics::Scale(0.6, 0.6, 1.0);
	snprintf(textBuffer, sizeof textBuffer, "Total Frame Time: %.3f ms", totalFrameCount);
	Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
	Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
	listY += 20.0;
	Graphics::Restore();

	// Draw Overdelay
	Graphics::Save();
	Graphics::Translate(infoPadding, listY, 0.0);
	Graphics::SetBlendColor(1.0, 1.0, 1.0, 0.5);
	Graphics::FillRectangle(-infoPadding / 2.0, 0.0, 12.0, 12.0);
	Graphics::Scale(0.6, 0.6, 1.0);
	snprintf(textBuffer, sizeof textBuffer, "Overdelay: %.3f ms", Application::GetOverdelay());
	Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
	Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
	listY += 20.0;
	Graphics::Restore();

	if (Memory::IsTracking) {
		float count = (float)Memory::MemoryUsage;
		const char* moniker = "B";

		if (count >= 1000000000) {
			count /= 1000000000;
			moniker = "GB";
		}
		else if (count >= 1000000) {
			count /= 1000000;
			moniker = "MB";
		}
		else if (count >= 1000) {
			count /= 1000;
			moniker = "KB";
		}

		listY += 30.0 - 20.0;

		Graphics::Save();
		Graphics::Translate(infoPadding / 2.0, listY, 0.0);
		Graphics::Scale(0.6, 0.6, 1.0);
		snprintf(textBuffer, sizeof textBuffer, "RAM Usage: %.3f %s", count, moniker);
		Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
		Graphics::DrawText(font, textBuffer, textX, textY, &textParams);
		Graphics::Restore();

		listY += 10.0;
	}

	listY += 30.0;

	vector<ObjectList*> objListPerf = Scene::GetObjectListPerformance();
	for (size_t i = 0; i < objListPerf.size(); i++) {
		ObjectList* list = objListPerf[i];

		Graphics::Save();
		Graphics::Translate(infoPadding / 2.0, listY, 0.0);
		Graphics::Scale(0.6, 0.6, 1.0);

		char textBufferXXX[1024];
		snprintf(textBufferXXX,
			sizeof textBufferXXX,
			"Object \"%s\": Avg Render %.1f mcs (Total %.1f mcs, Count %d)",
			list->ObjectName,
			list->Performance.Render.GetAverageTime(),
			list->Performance.Render.GetTotalAverageTime(),
			(int)list->Performance.Render.AverageItemCount);

		float maxW = 0.0, maxH = 0.0;
		TextDrawParams locTextParams = textParams;
		Graphics::SetBlendColor(0.0, 0.0, 0.0, 0.75);
		Graphics::MeasureText(font, textBufferXXX, &locTextParams, maxW, maxH);
		Graphics::FillRectangle(textX, textY, maxW, maxH);

		Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
		Graphics::DrawText(font, textBufferXXX, textX, textY, &locTextParams);
		Graphics::Restore();

		listY += maxH * 0.6;

		if (listY >= wh) {
			break;
		}
	}

	Graphics::Restore();
}
