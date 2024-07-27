#if INTERFACE

#include <Engine/Includes/Standard.h>

class File {
private:
};
#endif

#include <Engine/Filesystem/File.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/FileStream.h>

#if WIN32
    #include <io.h>
#else
    #include <unistd.h>
#endif

#if MACOSX || ANDROID
#include <Engine/Includes/StandardSDL2.h>
#endif

PUBLIC STATIC bool File::Exists(const char* path) {
    #if WIN32
        return _access(path, 0) != -1;
    #elif MACOSX
        if (access(path, F_OK) != -1)
            return true;

        // Avoid doing FILE pointer handling in case of multithreaded file access
        SDL_RWops* rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            SDL_RWclose(rw);
            return true;
        }
        return false;
    #elif ANDROID
        SDL_RWops* rw = SDL_RWFromFile(path, "rb");
        if (rw) {
            SDL_RWclose(rw);
            return true;
        }
        return false;
    #elif SWITCH_ROMFS
        char romfsPath[256];
        snprintf(romfsPath, romfsPath, "romfs:/%s", path);
        if (access(romfsPath, F_OK) != -1) {
            Log::Print(Log::LOG_WARN, "YES access \"%s\".", romfsPath);
            return true;
        }

        Log::Print(Log::LOG_WARN, "NO access \"%s\".", romfsPath);

        return false;
    #else
        // access probably works in most systems... right?!
        return access(path, F_OK) != -1;
    #endif
}

PUBLIC STATIC size_t File::ReadAllBytes(const char* path, char** out) {
    FileStream* stream;
    if ((stream = FileStream::New(path, FileStream::READ_ACCESS))) {
        size_t size = stream->Length();
        *out = (char*)Memory::Malloc(size + 1);
        if (!*out)
            return 0;
        (*out)[size] = 0;
        stream->ReadBytes(*out, size);
        stream->Close();
        return size;
    }
    return 0;
}

PUBLIC STATIC bool   File::WriteAllBytes(const char* path, const char* bytes, size_t len) {
    if (!path) return false;
    if (!*path) return false;
    if (!bytes) return false;

    FileStream* stream;
    if ((stream = FileStream::New(path, FileStream::WRITE_ACCESS))) {
        stream->WriteBytes((char*)bytes, len);
        stream->Close();
        return true;
    }
    return false;
}
