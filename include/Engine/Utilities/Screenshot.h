#ifndef ENGINE_UTILITIES_SCREENSHOT_H
#define ENGINE_UTILITIES_SCREENSHOT_H

#include <Engine/Includes/Operation.h>
#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>

struct ScreenshotMetadata {
	std::string Key;
	std::string Value;
};

struct ScreenshotOperation {
	std::string Path;
	Operation OnFinish;
};

class Screenshot {
public:
	static std::string GetFilename();
	static std::vector<ScreenshotMetadata> GetMetadata();
	static std::vector<PNGMetadata> GetPNGMetadata();
	static std::string GetTimeString();
	static bool Exists(std::string path);
	static void TakeQueued();
	static void QueueOperation(ScreenshotOperation operation);
	static bool IsQueueEmpty();
	static bool RunOperation(ScreenshotOperation& operation);
	static bool SaveFile(const char* path);
};

#endif /* ENGINE_UTILITIES_SCREENSHOT_H */
