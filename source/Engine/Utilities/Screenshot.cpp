#include <Engine/Application.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Utilities/Screenshot.h>

std::vector<ScreenshotOperation> Queue;

std::string Screenshot::GetFilename() {
	const char* identifier = Application::GetGameIdentifier();
	if (identifier == nullptr) {
		identifier = "hatch";
	}

	time_t timeInfo;
	time(&timeInfo);

	char timeString[256];
	strftime(timeString, sizeof(timeString), "%Y-%m-%d-%H-%M-%S", localtime(&timeInfo));

	char filenameBuffer[MAX_FILENAME_LENGTH];
	snprintf(filenameBuffer,
		MAX_FILENAME_LENGTH,
		"%s-%s",
		identifier,
		timeString);

	std::string filename = std::string(filenameBuffer);

	return filename;
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

bool Screenshot::Exists(std::string path) {
	if (Queue.size() > 0) {
		for (size_t i = Queue.size(); i > 0; i--) {
			if (Queue[i - 1].Path == path) {
				return true;
			}
		}
	}

	return File::ProtectedExists(path.c_str(), true);
}

void Screenshot::QueueOperation(ScreenshotOperation operation) {
	Queue.insert(Queue.begin(), operation);
}
bool Screenshot::IsQueueEmpty() {
	return Queue.empty();
}
void Screenshot::TakeQueued() {
	while (!IsQueueEmpty()) {
		Screenshot::RunOperation(Queue.front());
		Queue.erase(Queue.begin());
	}
}

bool Screenshot::RunOperation(ScreenshotOperation& operation) {
	const char* path = operation.Path.c_str();

	bool success = SaveFile(path);

	if (operation.OnFinish.Callback) {
		OperationResult result;
		result.Success = success;
		result.Input = operation.OnFinish.Data;
		result.Output = (void*)path;

		operation.OnFinish.Callback(result);
	}

	return success;
}
bool Screenshot::SaveFile(const char* path) {
	if (!Graphics::UpdateFramebufferTexture()) {
		return false;
	}

	std::vector<PNGMetadata> metadata = GetPNGMetadata();

	return PNG::Save(Graphics::FramebufferTexture, path, &metadata);
}
