#include <Engine/Application.h>
#include <Engine/ResourceTypes/Image.h>
#include <Engine/Utilities/Screenshot.h>

std::string Screenshot::GetFilename(int imageFormat) {
	const char* identifier = Application::GetGameIdentifier();
	if (identifier == nullptr) {
		identifier = "hatch";
	}

	time_t timeInfo;
	time(&timeInfo);

	char timeString[256];
	strftime(timeString, sizeof(timeString), "%Y-%m-%d-%H-%M-%S", localtime(&timeInfo));

	const char* extension;
	switch (imageFormat) {
	case IMAGE_FORMAT_PNG:
		extension = ".png";
		break;
	case IMAGE_FORMAT_GIF:
		extension = ".gif";
		break;
	case IMAGE_FORMAT_JPEG:
		extension = ".jpg";
		break;
	default:
		extension = "";
		break;
	}

	char filenameBuffer[MAX_FILENAME_LENGTH];
	snprintf(filenameBuffer,
		MAX_FILENAME_LENGTH - strlen(extension),
		"%s-%s",
		identifier,
		timeString);

	std::string filename = std::string(filenameBuffer) + extension;

	return filename;
}
std::string Screenshot::GetFilename() {
	return GetFilename(IMAGE_FORMAT_PNG);
}

std::vector<ScreenshotMetadata> Screenshot::GetMetadata() {
	std::vector<ScreenshotMetadata> metadata = {
		ScreenshotMetadata{"Game", Application::GameTitle},
		ScreenshotMetadata{"Game version", Application::GameVersion},
		ScreenshotMetadata{"Game developer", Application::GameDeveloper},
		ScreenshotMetadata{"Engine version", Application::EngineVersion},
		ScreenshotMetadata{"Date and time", GetTimeString()}};

	return metadata;
}

std::vector<PNGMetadata> Screenshot::GetPNGMetadata() {
	std::vector<ScreenshotMetadata> metadata = GetMetadata();
	std::vector<PNGMetadata> pngMetadata;

	for (size_t i = 0; i < metadata.size(); i++) {
		std::string& key = metadata[i].Key;
		std::string& value = metadata[i].Value;

		PNGMetadata entry;
		snprintf(entry.Keyword, MAX_PNG_TEXT_KEYWORD_LENGTH, "%s", key.c_str());
		entry.Text = value;

		pngMetadata.push_back(entry);
	}

	return pngMetadata;
}

std::string Screenshot::GetTimeString() {
	time_t timeInfo;
	time(&timeInfo);

	char buffer[256];
	strftime(buffer, sizeof(buffer), "%d %b %Y %H:%M:%S %z", localtime(&timeInfo));
	return std::string(buffer);
}
