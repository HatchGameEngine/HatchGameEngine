#if INTERFACE
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Includes/HashMap.h>
class SourceFileMap {
public:
    static bool                      Initialized;
    static HashMap<Uint32>*          Checksums;
    static HashMap<vector<Uint32>*>* ClassMap;
    static Uint32                    DirectoryChecksum;

    static bool                      DoLogging;
};
#endif

#include <Engine/Bytecode/SourceFileMap.h>

#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/ResourceTypes/ResourceManager.h>

bool                      SourceFileMap::Initialized = false;
HashMap<Uint32>*          SourceFileMap::Checksums = NULL;
HashMap<vector<Uint32>*>* SourceFileMap::ClassMap = NULL;
Uint32                    SourceFileMap::DirectoryChecksum = 0;

bool                      SourceFileMap::DoLogging = false;

PUBLIC STATIC void SourceFileMap::CheckInit() {
    if (SourceFileMap::Initialized) return;

    if (SourceFileMap::Checksums == NULL) {
        SourceFileMap::Checksums = new HashMap<Uint32>(CombinedHash::EncryptData, 16);
    }
    if (SourceFileMap::ClassMap == NULL) {
        SourceFileMap::ClassMap = new HashMap<vector<Uint32>*>(Murmur::EncryptData, 16);
    }

    SourceFileMap::DoLogging = false;

    if (ResourceManager::ResourceExists("Objects/Objects.hcm")) {
        ResourceStream* stream = ResourceStream::New("Objects/Objects.hcm");
        if (stream) {
            Uint32 magic_got;
            Uint32 magic = *(Uint32*)"HMAP";
            if ((magic_got = stream->ReadUInt32()) == magic) {
                stream->ReadByte(); // Version
                stream->ReadByte(); // Version
                stream->ReadByte(); // Version
                stream->ReadByte(); // Version

                Uint32 count = stream->ReadUInt32();
                for (Uint32 ch = 0; ch < count; ch++) {
                    Uint32 classHash = stream->ReadUInt32();
                    Uint32 fnCount = stream->ReadUInt32();

                    vector<Uint32>* fnList = new vector<Uint32>();
                    for (Uint32 fn = 0; fn < fnCount; fn++) {
                        fnList->push_back(stream->ReadUInt32());
                    }

                    SourceFileMap::ClassMap->Put(classHash, fnList);
                }
            }
            else {
                Log::Print(Log::LOG_ERROR, "Invalid ClassMap!");
            }
            stream->Close();
        }
    }
    else {
        Log::Print(Log::LOG_ERROR, "Could not find ClassMap!");
    }

    #ifndef NO_SCRIPT_COMPILING

    if (File::Exists("SourceFileMap.bin")) {
        char*  bytes;
        size_t len = File::ReadAllBytes("SourceFileMap.bin", &bytes);
        SourceFileMap::Checksums->FromBytes((Uint8*)bytes, (len - 4) / (sizeof(Uint32) + sizeof(Uint32)));
        SourceFileMap::DirectoryChecksum = *(Uint32*)(bytes + len - 4);
        Memory::Free(bytes);
    }

    #endif

    Application::Settings->GetBool("compiler", "log", &SourceFileMap::DoLogging);
    if (SourceFileMap::DoLogging) {
        Application::Settings->GetBool("compiler", "showWarnings", &Compiler::ShowWarnings);
    }

    Application::Settings->GetBool("compiler", "writeDebugInfo", &Compiler::WriteDebugInfo);
    Application::Settings->GetBool("compiler", "writeSourceFilename", &Compiler::WriteSourceFilename);

    SourceFileMap::Initialized = true;
}
PUBLIC STATIC void SourceFileMap::CheckForUpdate() {
    SourceFileMap::CheckInit();

    #ifndef NO_SCRIPT_COMPILING
    bool anyChanges = false;

    const char* scriptFolder = "Scripts";
    size_t      scriptFolderNameLen = strlen(scriptFolder);

    if (!Directory::Exists(scriptFolder))
        return;

    vector<char*> list;
    Directory::GetFiles(&list, scriptFolder, "*.hsl", true);

    if (list.size() == 0) {
        list.clear();
        list.shrink_to_fit();
        return;
    }

    if (!Directory::Exists("Resources/Objects")) {
        Directory::Create("Resources/Objects");
        anyChanges = true;
    }
    if (!ResourceManager::ResourceExists("Objects/Objects.hcm"))
        anyChanges = true;

    Uint32 oldDirectoryChecksum = SourceFileMap::DirectoryChecksum;

    SourceFileMap::DirectoryChecksum = 0x0;
    for (size_t i = 0; i < list.size(); i++) {
        char* filename = list[i] + scriptFolderNameLen;
        SourceFileMap::DirectoryChecksum = FNV1A::EncryptData(filename, (Uint32)strlen(filename), SourceFileMap::DirectoryChecksum);
    }

    if (oldDirectoryChecksum != SourceFileMap::DirectoryChecksum && SourceFileMap::DirectoryChecksum) {
        Log::Print(Log::LOG_VERBOSE, "Detected new/deleted file: (%08X -> %08X)", oldDirectoryChecksum, SourceFileMap::DirectoryChecksum);
        anyChanges = true;

        SourceFileMap::Checksums->Clear();
        SourceFileMap::ClassMap->WithAll([](Uint32, vector<Uint32>* list) -> void {
            list->clear();
            list->shrink_to_fit();
            delete list;
        });
        SourceFileMap::ClassMap->Clear();
    }

    const char* scriptFolderPath = (std::string(scriptFolder) + "/").c_str();
    size_t scriptFolderPathLen = strlen(scriptFolderPath);

    for (size_t i = 0; i < list.size(); i++) {
        char* filename = strrchr(list[i], '/');
        Uint32 filenameHash = 0;
        if (filename) {
            filename++;
            char* dot = strrchr(filename, '.');
            if (dot)
                *dot = '\0';
            filenameHash = CombinedHash::EncryptString(list[i] + scriptFolderNameLen);
            if (dot)
                *dot = '.';
        }
        if (!filenameHash) {
            Memory::Free(list[i]);
            continue;
        }

        Uint32 newChecksum = 0;
        Uint32 oldChecksum = 0;
        bool doRecompile = false;

        char*  source;
        File::ReadAllBytes(list[i], &source);
        newChecksum = Murmur::EncryptString(source);

        Memory::Track(source, "SourceFileMap::SourceText");

        if (SourceFileMap::Checksums->Exists(filenameHash)) {
            oldChecksum = SourceFileMap::Checksums->Get(filenameHash);
        }
        doRecompile = newChecksum != oldChecksum;
        anyChanges |= doRecompile;

        char outFile[35];
        snprintf(outFile, sizeof outFile, "Resources/Objects/%08X.ibc", filenameHash);

        // If changed, then compile.
        if (doRecompile || !File::Exists(outFile)) {
            Compiler::PrepareCompiling();

            char* scriptFilename = list[i];
            if (StringUtils::StartsWith(scriptFilename, scriptFolderPath))
                scriptFilename += scriptFolderPathLen;

            if (SourceFileMap::DoLogging) {
                if (doRecompile)
                    Log::Print(Log::LOG_VERBOSE, "Recompiling %s...", scriptFilename);
                else
                    Log::Print(Log::LOG_VERBOSE, "Compiling %s...", scriptFilename);
            }

            Compiler* compiler = new Compiler;
            compiler->Compile(scriptFilename, source, outFile);

            // Add this file to the list
            // Log::Print(Log::LOG_INFO, "filename: %s (0x%08X)", filename, filenameHash);
            for (size_t h = 0; h < compiler->ClassHashList.size(); h++) {
                Uint32 classHash = compiler->ClassHashList[h];
                Uint32 classExtended = compiler->ClassExtendedList[h];
                if (SourceFileMap::ClassMap->Exists(classHash)) {
                    vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(classHash);
                    if (std::count(filenameHashList->begin(), filenameHashList->end(), filenameHash) == 0) {
                        // NOTE: We need a better way of sorting
                        if (classExtended == 0)
                            filenameHashList->insert(filenameHashList->begin(), filenameHash);
                        else if (classExtended == 1)
                            filenameHashList->push_back(filenameHash);
                    }
                }
                else {
                    vector<Uint32>* filenameHashList = new vector<Uint32>();
                    filenameHashList->push_back(filenameHash);
                    SourceFileMap::ClassMap->Put(classHash, filenameHashList);
                }
            }

            delete compiler;
            Compiler::FinishCompiling();
        }

        Memory::Free(source);

        // Log::Print(Log::LOG_INFO, "List: %s (%08X) (old: %08X, new: %08X) %d", list[i], filenameHash, oldChecksum, newChecksum, false);

        SourceFileMap::Checksums->Put(filenameHash, newChecksum);
        Memory::Free(list[i]);
    }

    if (anyChanges) {
        FileStream* stream;
        // SourceFileMap.bin
        stream = FileStream::New("SourceFileMap.bin", FileStream::WRITE_ACCESS);
        if (stream) {
            Uint8* data = SourceFileMap::Checksums->GetBytes(true);
            stream->WriteBytes(data, SourceFileMap::Checksums->Count * (sizeof(Uint32) + sizeof(Uint32)));
            Memory::Free(data);

            stream->WriteUInt32(SourceFileMap::DirectoryChecksum);

            stream->Close();
        }

        // Objects.hcm
        stream = FileStream::New("Resources/Objects/Objects.hcm", FileStream::WRITE_ACCESS);
        if (stream) {
            Uint32 magic = *(Uint32*)"HMAP";
            stream->WriteUInt32(magic);
            stream->WriteByte(0x00); // Version
            stream->WriteByte(0x01); // Version
            stream->WriteByte(0x02); // Version
            stream->WriteByte(0x03); // Version

            stream->WriteUInt32((Uint32)SourceFileMap::ClassMap->Count); // Count
            SourceFileMap::ClassMap->WithAll([stream](Uint32 hash, vector<Uint32>* list) -> void {
                stream->WriteUInt32(hash); // ClassHash
                stream->WriteUInt32((Uint32)list->size()); // Count
                for (size_t fn = 0; fn < list->size(); fn++) {
                    stream->WriteUInt32((*list)[fn]);
                }
            });

            stream->Close();
        }
    }

    list.clear();
    list.shrink_to_fit();

    #endif
}

PUBLIC STATIC void SourceFileMap::Dispose() {
    if (SourceFileMap::Checksums) {
        delete SourceFileMap::Checksums;
    }
    if (SourceFileMap::ClassMap) {
        SourceFileMap::ClassMap->WithAll([](Uint32, vector<Uint32>* list) -> void {
            list->clear();
            list->shrink_to_fit();
            delete list;
        });
        delete SourceFileMap::ClassMap;
    }

    SourceFileMap::Initialized = false;
    SourceFileMap::Checksums = NULL;
    SourceFileMap::ClassMap = NULL;
}
