#include <Engine/Filesystem/Directory.h>
#include <Engine/IO/FileStream.h>
#include <Engine/Includes/StandardSDL2.h>

#ifdef MACOSX
extern "C" {
#include <Engine/Platforms/MacOS/Filesystem.h>
}

static void getAppName(char* buffer, int maxSize) {
	char* tmp = new char[maxSize];
	char* tmp2 = new char[maxSize];

	if (MacOS_GetSelfPath(tmp, maxSize)) {
		/* this shouldn't happen */
		delete[] tmp;
		delete[] tmp2;

		return;
	}

	memset(tmp2, 0, maxSize);

	int slashcount = 0;
	int end = 0;
	for (int i = maxSize - 1; i >= 0; --i) {
		slashcount += tmp[i] == '/' ? 1 : 0;

		if (slashcount >= 2) {
			end = i;
			break;
		}
	}

	if (slashcount < 2) {
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	int start = -1;
	for (int i = end - 1; i >= 0; --i) {
		if (tmp[i] == '/') {
			start = i + 1;

			break;
		}
	}

	if (start == -1) {
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	if (tmp[end - 1] != 'p' || tmp[end - 2] != 'p' || tmp[end - 3] != 'a' ||
		tmp[end - 4] != '.') {
		/* no .app, it's not packaged */
		memset(buffer, 0, maxSize);

		delete[] tmp;
		delete[] tmp2;

		return;
	}

	memcpy(buffer, (unsigned char*)tmp + start, (end - start) - 4);

	delete[] tmp;
	delete[] tmp2;
}
#endif

FILE* FileStream::OpenFile(const char* filename, Uint32 access) {
	const char* accessString = NULL;
	switch (access & 15) {
	case FileStream::READ_ACCESS:
		accessString = "rb";
		break;
	case FileStream::WRITE_ACCESS:
		accessString = "wb";
		break;
	case FileStream::APPEND_ACCESS:
		accessString = "ab";
		break;
	}

	FILE* file = nullptr;

#if 0
	printf("THIS SHOULDN'T HAPPEN\n");
#elif defined(WIN32)
	if (access & FileStream::SAVEGAME_ACCESS) {
		// Saves in
		// C:/Users/Username/.appdata/Roaming/TARGET_NAME/
		const char* saveDataPath = "%appdata%/" TARGET_NAME "/Saves";
		if (!Directory::Exists(saveDataPath)) {
			Directory::Create(saveDataPath);
		}

		char documentPath[256];
		snprintf(documentPath, 256, "%s/%s", saveDataPath, filename);
		printf("documentPath: %s\n", documentPath);
		file = fopen(documentPath, accessString);
	}
#elif defined(MACOSX)
	if (access & FileStream::SAVEGAME_ACCESS) {
		char documentPath[256];
		char appDirName[256];
		getAppName(appDirName, 256);
		if (MacOS_GetApplicationSupportDirectory(documentPath, 256)) {
			snprintf(documentPath, 256, "%s/" TARGET_NAME "/Saves", documentPath);
			if (!Directory::Exists(documentPath)) {
				Directory::Create(documentPath);
			}

			snprintf(documentPath, 256, "%s/%s", documentPath, filename);
			file = fopen(documentPath, accessString);
		}
	}
#elif defined(IOS)
	if (access & FileStream::SAVEGAME_ACCESS) {
		char* base_path = SDL_GetPrefPath("aknetk", TARGET_NAME);
		if (base_path) {
			char documentPath[256];
			snprintf(documentPath, 256, "%s%s", base_path, filename);
			file = fopen(documentPath, accessString);
		}
	}
	else if (access & FileStream::PREFERENCES_ACCESS) {
		char* base_path = SDL_GetPrefPath("aknetk", TARGET_NAME);
		if (base_path) {
			char documentPath[256];
			snprintf(documentPath, 256, "%s%s", base_path, filename);
			file = fopen(documentPath, accessString);
		}
	}
#elif defined(ANDROID)
	const char* internalStorage = SDL_AndroidGetInternalStoragePath();
	if (internalStorage) {
		char androidPath[256];
		if (access & FileStream::SAVEGAME_ACCESS) {
			snprintf(androidPath, 256, "%s/Save", internalStorage);
			if (!Directory::Exists(androidPath)) {
				Directory::Create(androidPath);
			}

			char documentPath[256];
			snprintf(documentPath, 256, "%s/%s", androidPath, filename);
			file = fopen(documentPath, accessString);
		}
		else {
			snprintf(androidPath, 256, "%s/%s", internalStorage, filename);
			file = fopen(androidPath, accessString);
		}
	}
#elif defined(SWITCH)
	if (access & FileStream::SAVEGAME_ACCESS) {
		// Saves in Saves/
		const char* saveDataPath = "Saves";
		if (!Directory::Exists(saveDataPath)) {
			Directory::Create(saveDataPath);
		}

		char documentPath[256];
		snprintf(documentPath, 256, "%s/%s", saveDataPath, filename);
		file = fopen(documentPath, accessString);
	}
#endif

	if (!file) {
		file = fopen(filename, accessString);
	}

	return file;
}

FileStream* FileStream::New(const char* filename, Uint32 access) {
	FileStream* stream = new (std::nothrow) FileStream;
	if (!stream) {
		return NULL;
	}

	stream->f = OpenFile(filename, access);

	if (!stream->f) {
		goto FREE;
	}

	fseek(stream->f, 0, SEEK_END);
	stream->size = ftell(stream->f);
	fseek(stream->f, 0, SEEK_SET);

	stream->Filename = std::string(filename);
	stream->CurrentAccess = access;

	return stream;

FREE:
	delete stream;
	return NULL;
}

bool FileStream::Reopen(Uint32 newAccess) {
	if (CurrentAccess == newAccess) {
		return true;
	}

	FILE* newFile = OpenFile(Filename.c_str(), newAccess);
	if (!newFile) {
		return false;
	}

	fclose(f);
	f = newFile;

	CurrentAccess = newAccess;

	return true;
}

bool FileStream::IsReadable() {
	return CurrentAccess == FileStream::READ_ACCESS;
}
bool FileStream::IsWritable() {
	return !IsReadable();
}
bool FileStream::MakeReadable(bool readable) {
	if (!readable) {
		return MakeWritable(true);
	}

	return Reopen(READ_ACCESS);
}
bool FileStream::MakeWritable(bool writable) {
	if (!writable) {
		return MakeReadable(true);
	}

	return Reopen(WRITE_ACCESS);
}

void FileStream::Close() {
	fclose(f);
	f = NULL;
	Stream::Close();
}
void FileStream::Seek(Sint64 offset) {
	fseek(f, offset, SEEK_SET);
}
void FileStream::SeekEnd(Sint64 offset) {
	fseek(f, offset, SEEK_END);
}
void FileStream::Skip(Sint64 offset) {
	fseek(f, offset, SEEK_CUR);
}
size_t FileStream::Position() {
	return ftell(f);
}
size_t FileStream::Length() {
	return size;
}

size_t FileStream::ReadBytes(void* data, size_t n) {
	// if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from
	// closed stream.")
	return fread(data, 1, n, f);
}

size_t FileStream::WriteBytes(void* data, size_t n) {
	return fwrite(data, 1, n, f);
}
