#include <Engine/Bytecode/SourceFileMap.h>

#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ResourceManager.h>

#define SOURCEFILEMAP_NAME "cache://SourceFileMap.bin"

#define OBJECTS_HCM_NAME OBJECTS_DIR_NAME "Objects.hcm"

bool SourceFileMap::Initialized = false;
HashMap<Uint32>* SourceFileMap::Checksums = NULL;
HashMap<vector<Uint32>*>* SourceFileMap::ClassMap = NULL;
Uint32 SourceFileMap::DirectoryChecksum = 0;
Uint32 SourceFileMap::Magic = MAGIC_LE32("HMAP");

void SourceFileMap::CheckInit() {
	if (SourceFileMap::Initialized) {
		return;
	}

	if (SourceFileMap::Checksums == NULL) {
		SourceFileMap::Checksums = new HashMap<Uint32>(CombinedHash::EncryptData, 16);
	}
	if (SourceFileMap::ClassMap == NULL) {
		SourceFileMap::ClassMap = new HashMap<vector<Uint32>*>(Murmur::EncryptData, 16);
	}

	if (ResourceManager::ResourceExists(OBJECTS_HCM_NAME)) {
		ResourceStream* stream = ResourceStream::New(OBJECTS_HCM_NAME);
		if (stream) {
			Uint32 magic_got;
			if ((magic_got = stream->ReadUInt32()) == SourceFileMap::Magic) {
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
				Log::Print(Log::LOG_ERROR,
					"Invalid ClassMap! (Expected %08X, was %08X)",
					SourceFileMap::Magic,
					magic_got);
			}
			stream->Close();
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Could not find ClassMap!");
	}

#ifndef NO_SCRIPT_COMPILING
	if (File::Exists(SOURCEFILEMAP_NAME, true)) {
		char* bytes;
		size_t len = File::ReadAllBytes(SOURCEFILEMAP_NAME, &bytes, true);
		SourceFileMap::Checksums->FromBytes(
			(Uint8*)bytes, (len - 4) / (sizeof(Uint32) + sizeof(Uint32)));
		SourceFileMap::DirectoryChecksum = *(Uint32*)(bytes + len - 4);
		Memory::Free(bytes);
	}
#endif

	SourceFileMap::Initialized = true;
}
void SourceFileMap::CheckForUpdate() {
	SourceFileMap::CheckInit();

	VFSProvider* mainVfs = ResourceManager::GetMainResource();
	if (!mainVfs || !mainVfs->IsWritable()) {
		return;
	}

#ifndef NO_SCRIPT_COMPILING
	bool anyChanges = false;

	const char* scriptFolder = "Scripts";
	size_t scriptFolderNameLen = strlen(scriptFolder);

	if (!Directory::Exists(scriptFolder)) {
		return;
	}

	vector<std::filesystem::path> list;
	Directory::GetFiles(&list, scriptFolder, "*.hsl", true);

	if (list.size() == 0) {
		list.clear();
		list.shrink_to_fit();
		return;
	}

	if (!mainVfs->HasFile(OBJECTS_HCM_NAME)) {
		anyChanges = true;
	}

	Uint32 oldDirectoryChecksum = SourceFileMap::DirectoryChecksum;

	SourceFileMap::DirectoryChecksum = 0x0;
	for (size_t i = 0; i < list.size(); i++) {
		std::string asStr = list[i].u8string();
		const char* listEntry = asStr.c_str();
		const char* filename = listEntry + scriptFolderNameLen;
		SourceFileMap::DirectoryChecksum = FNV1A::EncryptData(
			filename, (Uint32)strlen(filename), SourceFileMap::DirectoryChecksum);
	}

	if (oldDirectoryChecksum != SourceFileMap::DirectoryChecksum &&
		SourceFileMap::DirectoryChecksum) {
		Log::Print(Log::LOG_VERBOSE,
			"Detected new/deleted file: (%08X -> %08X)",
			oldDirectoryChecksum,
			SourceFileMap::DirectoryChecksum);
		anyChanges = true;

		SourceFileMap::Checksums->Clear();
		SourceFileMap::ClassMap->WithAll([](Uint32, vector<Uint32>* list) -> void {
			list->clear();
			list->shrink_to_fit();
			delete list;
		});
		SourceFileMap::ClassMap->Clear();
	}

	std::string scriptFolderPathStr = std::string(scriptFolder) + "/";
	const char* scriptFolderPath = scriptFolderPathStr.c_str();
	size_t scriptFolderPathLen = scriptFolderPathStr.size();

	for (size_t i = 0; i < list.size(); i++) {
		std::string asStr = list[i].u8string();
		const char* listEntry = asStr.c_str();
		const char* filename = strrchr(listEntry, '/');
		Uint32 filenameHash = 0;
		if (filename) {
			filenameHash = ScriptManager::MakeFilenameHash(
				listEntry + scriptFolderNameLen + 1);
		}
		if (!filenameHash) {
			continue;
		}

		Uint32 newChecksum = 0;
		Uint32 oldChecksum = 0;
		bool doRecompile = false;

		char* source;
		File::ReadAllBytes(listEntry, &source, false);
		newChecksum = Murmur::EncryptString(source);

		Memory::Track(source, "SourceFileMap::SourceText");

		if (SourceFileMap::Checksums->Exists(filenameHash)) {
			oldChecksum = SourceFileMap::Checksums->Get(filenameHash);
		}
		doRecompile = newChecksum != oldChecksum;
		anyChanges |= doRecompile;

		std::string filenameForHash =
			ScriptManager::GetBytecodeFilenameForHash(filenameHash);
		const char* outFile = filenameForHash.c_str();

		// If changed, then compile.
		if (doRecompile || !mainVfs->HasFile(outFile)) {
			Compiler::PrepareCompiling();

			const char* scriptFilename = listEntry;
			if (StringUtils::StartsWith(scriptFilename, scriptFolderPath)) {
				scriptFilename += scriptFolderPathLen;
			}

			if (Compiler::DoLogging) {
				if (doRecompile) {
					Log::Print(Log::LOG_VERBOSE,
						"Recompiling %s...",
						scriptFilename);
				}
				else {
					Log::Print(Log::LOG_VERBOSE,
						"Compiling %s...",
						scriptFilename);
				}
			}

			Compiler* compiler = nullptr;
			bool didCompile = false;

			Stream* stream = mainVfs->OpenWriteStream(outFile);
			if (stream) {
				compiler = new Compiler;

				didCompile = compiler->Compile(scriptFilename, source, stream);

				mainVfs->CloseStream(stream);
			}
			else {
				Log::Print(Log::LOG_ERROR,
					"Couldn't open file '%s' for writing compiled script!",
					outFile);
			}

			// Add this file to the list
			if (didCompile) {
				AddToList(compiler, filenameHash);
			}

			delete compiler;
			Compiler::FinishCompiling();
		}

		Memory::Free(source);

		SourceFileMap::Checksums->Put(filenameHash, newChecksum);
	}

	if (anyChanges) {
		Stream* stream =
			FileStream::New(SOURCEFILEMAP_NAME, FileStream::WRITE_ACCESS, true);
		if (stream) {
			Uint8* data = SourceFileMap::Checksums->GetBytes(true);
			stream->WriteBytes(data,
				SourceFileMap::Checksums->Count *
					(sizeof(Uint32) + sizeof(Uint32)));
			Memory::Free(data);

			stream->WriteUInt32(SourceFileMap::DirectoryChecksum);

			stream->Close();
		}

		stream = mainVfs->OpenWriteStream(OBJECTS_HCM_NAME);
		if (stream) {
			stream->WriteUInt32(SourceFileMap::Magic);
			stream->WriteByte(0x00); // Version
			stream->WriteByte(0x01); // Version
			stream->WriteByte(0x02); // Version
			stream->WriteByte(0x03); // Version

			stream->WriteUInt32((Uint32)SourceFileMap::ClassMap->Count); // Count
			SourceFileMap::ClassMap->WithAll(
				[stream](Uint32 hash, vector<Uint32>* list) -> void {
					stream->WriteUInt32(hash); // ClassHash
					stream->WriteUInt32((Uint32)list->size()); // Count
					for (size_t fn = 0; fn < list->size(); fn++) {
						stream->WriteUInt32((*list)[fn]);
					}
				});

			mainVfs->CloseStream(stream);
		}
	}

	list.clear();
	list.shrink_to_fit();
#endif
}
void SourceFileMap::AddToList(Compiler* compiler, Uint32 filenameHash) {
	for (size_t h = 0; h < compiler->ClassHashList.size(); h++) {
		Uint32 classHash = compiler->ClassHashList[h];
		Uint32 classExtended = compiler->ClassExtendedList[h];
		if (SourceFileMap::ClassMap->Exists(classHash)) {
			vector<Uint32>* filenameHashList = SourceFileMap::ClassMap->Get(classHash);
			if (std::count(filenameHashList->begin(),
				    filenameHashList->end(),
				    filenameHash) == 0) {
				// NOTE: We need a better way of sorting
				if (classExtended == 0) {
					filenameHashList->insert(
						filenameHashList->begin(), filenameHash);
				}
				else if (classExtended == 1) {
					filenameHashList->push_back(filenameHash);
				}
			}
		}
		else {
			vector<Uint32>* filenameHashList = new vector<Uint32>();
			filenameHashList->push_back(filenameHash);
			SourceFileMap::ClassMap->Put(classHash, filenameHashList);
		}
	}
}

void SourceFileMap::Dispose() {
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
