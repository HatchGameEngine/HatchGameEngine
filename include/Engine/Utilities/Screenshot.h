#ifndef ENGINE_UTILITIES_SCREENSHOT_H
#define ENGINE_UTILITIES_SCREENSHOT_H

#include <Engine/Includes/Standard.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>

struct ScreenshotMetadata {
	std::string Key;
	std::string Value;
};

class Screenshot {
public:
	static std::string GetFilename(int imageFormat);
	static std::string GetFilename();
	static std::vector<ScreenshotMetadata> GetMetadata();
	static std::vector<PNGMetadata> GetPNGMetadata();
	static std::string GetTimeString();
};

#endif /* ENGINE_UTILITIES_SCREENSHOT_H */
