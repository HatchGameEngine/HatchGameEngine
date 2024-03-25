#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Includes/HashMap.h>

class ResourceManager {
public:
    static bool      UsingDataFolder;
    static bool      UsingModPack;
};
#endif

#include <Engine/ResourceTypes/ResourceManager.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Includes/StandardSDL2.h>
#include <Engine/IO/Compression/ZLibStream.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/SDLStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/Stream.h>
#include <Engine/Application.h>

#define KEEP_DATA_PACKS_IN_MEMORY

struct      StreamNode {
    Stream*            Table;
    struct StreamNode* Next;
};
StreamNode* StreamNodeHead = NULL;

struct  ResourceRegistryItem {
    Stream* Table;
    Uint64  Offset;
    Uint64  Size;
    Uint32  DataFlag;
    Uint64  CompressedSize;
};
HashMap<ResourceRegistryItem>* ResourceRegistry = NULL;

bool                 ResourceManager::UsingDataFolder = true;
bool                 ResourceManager::UsingModPack = false;

PUBLIC STATIC void   ResourceManager::PrefixResourcePath(char* out, size_t outSize, const char* path) {
    snprintf(out, outSize, "Resources/%s", path);
}
PUBLIC STATIC void   ResourceManager::PrefixParentPath(char* out, size_t outSize, const char* path) {
#if defined(SWITCH_ROMFS)
    snprintf(out, outSize, "romfs:/%s", path);
#else
    snprintf(out, outSize, "%s", path);
#endif
}

PUBLIC STATIC void   ResourceManager::Init(const char* filename) {
    StreamNodeHead = NULL;
    ResourceRegistry = new HashMap<ResourceRegistryItem>(CRC32::EncryptData, 16);

    if (filename == NULL)
        filename = "Data.hatch";

    if (File::Exists(filename)) {
        ResourceManager::UsingDataFolder = false;

        Log::Print(Log::LOG_INFO, "Using \"%s\"", filename);
        ResourceManager::Load(filename);
    }
    else {
        Log::Print(Log::LOG_WARN, "Cannot find \"%s\".", filename);
    }

    char modpacksString[1024];
    if (Application::Settings->GetString("game", "modpacks", modpacksString, sizeof modpacksString)) {
        if (File::Exists(modpacksString)) {
            // ResourceManager::UsingDataFolder = false;
            ResourceManager::UsingModPack = true;

            Log::Print(Log::LOG_IMPORTANT, "Using \"%s\"", modpacksString);
            ResourceManager::Load(modpacksString);
        }
    }
}
PUBLIC STATIC void   ResourceManager::Load(const char* filename) {
    if (!ResourceRegistry)
        return;

    // Load directly from Resource folder
    char resourcePath[4096];
    ResourceManager::PrefixParentPath(resourcePath, sizeof resourcePath, filename);

    SDLStream* dataTableStream = SDLStream::New(resourcePath, SDLStream::READ_ACCESS);
    if (!dataTableStream) {
        Log::Print(Log::LOG_ERROR, "Could not open MemoryStream!");
        return;
    }

    Uint16 fileCount;
    Uint8 magicHATCH[5];
    dataTableStream->ReadBytes(magicHATCH, 5);
    if (memcmp(magicHATCH, "HATCH", 5)) {
        Log::Print(Log::LOG_ERROR, "Invalid HATCH data file \"%s\"! (%02X %02X %02X %02X %02X)", filename, magicHATCH[0], magicHATCH[1], magicHATCH[2], magicHATCH[3], magicHATCH[4]);
        dataTableStream->Close();
        return;
    }

    // Uint8 major, minor, pad;
    dataTableStream->ReadByte();
    dataTableStream->ReadByte();
    dataTableStream->ReadByte();

    // Add stream to list for closure on disposal
    StreamNode* streamNode = new StreamNode;
    streamNode->Table = dataTableStream;
    streamNode->Next = StreamNodeHead;
    StreamNodeHead = streamNode;

    fileCount = dataTableStream->ReadUInt16();
    Log::Print(Log::LOG_VERBOSE, "Loading resource table from \"%s\"...", filename);
    for (int i = 0; i < fileCount; i++) {
        Uint32 crc32 = dataTableStream->ReadUInt32();
        Uint64 offset = dataTableStream->ReadUInt64();
        Uint64 size = dataTableStream->ReadUInt64();
        Uint32 dataFlag = dataTableStream->ReadUInt32();
        Uint64 compressedSize = dataTableStream->ReadUInt64();

        ResourceRegistryItem item { dataTableStream, offset, size, dataFlag, compressedSize };
        ResourceRegistry->Put(crc32, item);
        // Log::Print(Log::LOG_VERBOSE, "%08X: Offset: %08llX Size: %08llX Comp Size: %08llX Data Flag: %08X", crc32, offset, size, compressedSize, dataFlag);
    }
}
PUBLIC STATIC bool   ResourceManager::LoadResource(const char* filename, Uint8** out, size_t* size) {
    Uint8* memory;
    char resourcePath[4096];
    ResourceRegistryItem item;

    if (ResourceManager::UsingDataFolder && !ResourceManager::UsingModPack)
        goto DATA_FOLDER;

    if (!ResourceRegistry)
        goto DATA_FOLDER;

    if (!ResourceRegistry->Exists(filename))
        goto DATA_FOLDER;

    item = ResourceRegistry->Get(filename);

    memory = (Uint8*)Memory::Malloc(item.Size + 1);
    if (!memory)
        goto DATA_FOLDER;

    memory[item.Size] = 0;

    item.Table->Seek(item.Offset);
    if (item.Size != item.CompressedSize) {
        Uint8* compressedMemory = (Uint8*)Memory::Malloc(item.Size);
        if (!compressedMemory) {
            Memory::Free(memory);
            goto DATA_FOLDER;
        }
        item.Table->ReadBytes(compressedMemory, item.CompressedSize);

        ZLibStream::Decompress(memory, (size_t)item.Size, compressedMemory, (size_t)item.CompressedSize);
        Memory::Free(compressedMemory);
    }
    else {
        item.Table->ReadBytes(memory, item.Size);
    }

    if (item.DataFlag == 2) {
        Uint8 keyA[16];
        Uint8 keyB[16];
        Uint32 filenameHash = CRC32::EncryptString(filename);
        Uint32 sizeHash = CRC32::EncryptData(&item.Size, sizeof(item.Size));

        // Populate Key A
        Uint32* keyA32 = (Uint32*)&keyA[0];
        keyA32[0] = filenameHash;
        keyA32[1] = filenameHash;
        keyA32[2] = filenameHash;
        keyA32[3] = filenameHash;

        // Populate Key B
        Uint32* keyB32 = (Uint32*)&keyB[0];
        keyB32[0] = sizeHash;
        keyB32[1] = sizeHash;
        keyB32[2] = sizeHash;
        keyB32[3] = sizeHash;

        int swapNibbles = 0;
        int indexKeyA = 0;
        int indexKeyB = 8;
        int xorValue = (item.Size >> 2) & 0x7F;
        for (Uint32 x = 0; x < item.Size; x++) {
            Uint8 temp = memory[x];

            temp ^= xorValue ^ keyB[indexKeyB++];

            if (swapNibbles)
                temp = (((temp & 0x0F) << 4) | ((temp & 0xF0) >> 4));

            temp ^= keyA[indexKeyA++];

            memory[x] = temp;

            if (indexKeyA <= 15) {
                if (indexKeyB > 12) {
                    indexKeyB = 0;
                    swapNibbles ^= 1;
                }
            }
            else if (indexKeyB <= 8) {
                indexKeyA = 0;
                swapNibbles ^= 1;
            }
            else {
                xorValue = (xorValue + 2) & 0x7F;
                if (swapNibbles) {
                    swapNibbles = false;
                    indexKeyA = xorValue % 7;
                    indexKeyB = (xorValue % 12) + 2;
                }
                else {
                    swapNibbles = true;
                    indexKeyA = (xorValue % 12) + 3;
                    indexKeyB = xorValue % 7;
                }
            }
        }
    }

    *out = memory;
    *size = (size_t)item.Size;
    return true;

    DATA_FOLDER:
    ResourceManager::PrefixResourcePath(resourcePath, sizeof resourcePath, filename);

    SDL_RWops* rw = SDL_RWFromFile(resourcePath, "rb");
    if (!rw) {
        // Log::Print(Log::LOG_ERROR, "ResourceManager::LoadResource: No RW!: %s", resourcePath, SDL_GetError());
        return false;
    }

    Sint64 rwSize = SDL_RWsize(rw);
    if (rwSize < 0) {
        Log::Print(Log::LOG_ERROR, "Could not get size of file \"%s\": %s", resourcePath, SDL_GetError());
        return false;
    }

    memory = (Uint8*)Memory::Malloc(rwSize + 1);
    if (!memory)
        return false;
    memory[rwSize] = 0;

    SDL_RWread(rw, memory, rwSize, 1);
    SDL_RWclose(rw);

    *out = memory;
    *size = rwSize;
    return true;
}
PUBLIC STATIC bool   ResourceManager::ResourceExists(const char* filename) {
    char resourcePath[4096];
    if (ResourceManager::UsingDataFolder && !ResourceManager::UsingModPack)
        goto DATA_FOLDER;

    if (!ResourceRegistry)
        goto DATA_FOLDER;

    if (!ResourceRegistry->Exists(filename))
        goto DATA_FOLDER;

    return true;

    DATA_FOLDER:
    ResourceManager::PrefixResourcePath(resourcePath, sizeof resourcePath, filename);

    SDL_RWops* rw = SDL_RWFromFile(resourcePath, "rb");
    if (!rw) {
        return false;
    }
    SDL_RWclose(rw);
    return true;
}
PUBLIC STATIC void   ResourceManager::Dispose() {
    if (StreamNodeHead) {
        for (StreamNode *old, *streamNode = StreamNodeHead; streamNode; ) {
            old = streamNode;
            streamNode = streamNode->Next;

            old->Table->Close();
            delete old;
        }
    }
    if (ResourceRegistry) {
        delete ResourceRegistry;
    }
}
