#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/IO/Stream.h>
class FileStream : public Stream {
public:
    FILE*  f;
    size_t size;
    enum {
        READ_ACCESS = 0,
        WRITE_ACCESS = 1,
        APPEND_ACCESS = 2,
        SAVEGAME_ACCESS = 16,
        PREFERENCES_ACCESS = 32,
    };
};
#endif

#include <Engine/IO/FileStream.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Includes/StandardSDL2.h>

#ifdef MACOSX
extern "C" {
    #include <Engine/Platforms/MacOS/Filesystem.h>
}
#endif

PUBLIC STATIC FileStream* FileStream::New(const char* filename, Uint32 access) {
    FileStream* stream = new (std::nothrow) FileStream;
    if (!stream) {
        return NULL;
    }

    const char* accessString = NULL;
    switch (access & 15) {
        case FileStream::READ_ACCESS: accessString = "rb"; break;
        case FileStream::WRITE_ACCESS: accessString = "wb"; break;
        case FileStream::APPEND_ACCESS: accessString = "ab"; break;
    }

    stream->f = NULL;

    #if 0
        printf("THIS SHOULDN'T HAPPEN\n");
    #elif defined(WIN32)
        if (access & FileStream::SAVEGAME_ACCESS) {
            // Saves in C:/Users/Username/.appdata/Roaming/TARGET_NAME/
            const char* saveDataPath = "%appdata%/" TARGET_NAME "/Saves";
            if (!Directory::Exists(saveDataPath))
                Directory::Create(saveDataPath);

            char documentPath[256];
            snprintf(documentPath, 256, "%s/%s", saveDataPath, filename);
            printf("documentPath: %s\n", documentPath);
            stream->f = fopen(documentPath, accessString);
        }
    #elif defined(MACOSX)
        if (access & FileStream::SAVEGAME_ACCESS) {
            char documentPath[256];
            if (MacOS_GetApplicationSupportDirectory(documentPath, 256)) {
                snprintf(documentPath, 256, "%s/" TARGET_NAME "/Saves", documentPath);
                if (!Directory::Exists(documentPath))
                    Directory::Create(documentPath);

                snprintf(documentPath, 256, "%s/%s", documentPath, filename);
                stream->f = fopen(documentPath, accessString);
            }
        }
    #elif defined(IOS)
        if (access & FileStream::SAVEGAME_ACCESS) {
            char* base_path = SDL_GetPrefPath("aknetk", TARGET_NAME);
            if (base_path) {
                char documentPath[256];
                snprintf(documentPath, 256, "%s%s", base_path, filename);
                stream->f = fopen(documentPath, accessString);
            }
        }
        else if (access & FileStream::PREFERENCES_ACCESS) {
            char* base_path = SDL_GetPrefPath("aknetk", TARGET_NAME);
            if (base_path) {
                char documentPath[256];
                snprintf(documentPath, 256, "%s%s", base_path, filename);
                stream->f = fopen(documentPath, accessString);
            }
        }
    #elif defined(ANDROID)
        const char* internalStorage = SDL_AndroidGetInternalStoragePath();
        if (internalStorage) {
            char androidPath[256];
            if (access & FileStream::SAVEGAME_ACCESS) {
                snprintf(androidPath, 256, "%s/Save", internalStorage);
                if (!Directory::Exists(androidPath))
                    Directory::Create(androidPath);

                char documentPath[256];
                snprintf(documentPath, 256, "%s/%s", androidPath, filename);
                stream->f = fopen(documentPath, accessString);
            }
            else {
                snprintf(androidPath, 256, "%s/%s", internalStorage, filename);
                stream->f = fopen(androidPath, accessString);
            }
        }
    #elif defined(SWITCH)
        if (access & FileStream::SAVEGAME_ACCESS) {
            // Saves in Saves/
            const char* saveDataPath = "Saves";
            if (!Directory::Exists(saveDataPath))
                Directory::Create(saveDataPath);

            char documentPath[256];
            snprintf(documentPath, 256, "%s/%s", saveDataPath, filename);
            stream->f = fopen(documentPath, accessString);
        }
    #endif

    if (!stream->f)
        stream->f = fopen(filename, accessString);

    if (!stream->f)
        goto FREE;

    fseek(stream->f, 0, SEEK_END);
    stream->size = ftell(stream->f);
    fseek(stream->f, 0, SEEK_SET);

    return stream;

    FREE:
        delete stream;
        return NULL;
}

PUBLIC        void        FileStream::Close() {
    fclose(f);
    f = NULL;
    Stream::Close();
}
PUBLIC        void        FileStream::Seek(Sint64 offset) {
    fseek(f, offset, SEEK_SET);
}
PUBLIC        void        FileStream::SeekEnd(Sint64 offset) {
    fseek(f, offset, SEEK_END);
}
PUBLIC        void        FileStream::Skip(Sint64 offset) {
    fseek(f, offset, SEEK_CUR);
}
PUBLIC        size_t      FileStream::Position() {
    return ftell(f);
}
PUBLIC        size_t      FileStream::Length() {
    return size;
}

PUBLIC        size_t      FileStream::ReadBytes(void* data, size_t n) {
    // if (!f) Log::Print(Log::LOG_ERROR, "Attempt to read from closed stream.")
    return fread(data, 1, n, f);
}

PUBLIC        size_t      FileStream::WriteBytes(void* data, size_t n) {
    return fwrite(data, 1, n, f);
}
