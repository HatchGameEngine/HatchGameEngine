#include <Engine/Application.h>
#include <Engine/Bytecode/Bytecode.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/SourceFileMap.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Exceptions/CompilerErrorException.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/ResourceTypes/ModLoader.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/Utilities/StringUtils.h>

#define SOURCEFILEMAP_NAME "cache://SourceFileMap.bin"

#define OBJECTS_HCM_NAME OBJECTS_DIR_NAME "Objects.hcm"
#define MOD_OBJECTS_HCM_NAME OBJECTS_DIR_NAME "ModObjects.hcm"

bool SourceFileMap::Initialized = false;
char SourceFileMap::Path[MAX_PATH_LENGTH];
bool SourceFileMap::Loaded = false;
bool SourceFileMap::AllowCompilation = false;
HashMap<Uint32>* SourceFileMap::Checksums = NULL;
HashMap<vector<Uint32>*>* SourceFileMap::ClassMap = NULL;
Uint32 SourceFileMap::DirectoryChecksum = 0;
Uint32 SourceFileMap::Magic = MAGIC_LE32("HMAP");
vector<Uint32> SourceFileMap::ModFilenameHashes;

void SourceFileMap::Init() {
	if (SourceFileMap::Initialized) {
		return;
	}

	if (SourceFileMap::Checksums == NULL) {
		SourceFileMap::Checksums = new HashMap<Uint32>(CombinedHash::EncryptData, 16);
	}
	if (SourceFileMap::ClassMap == NULL) {
		SourceFileMap::ClassMap = new HashMap<vector<Uint32>*>(Murmur::EncryptData, 16);
	}

	#if __APPLE__
		Path::FromURL(PATHLOCATION_GAME_URL SCRIPTS_DIRECTORY_NAME, SourceFileMap::Path, sizeof SourceFileMap::Path);
	#else
		StringUtils::Copy(SourceFileMap::Path, SCRIPTS_DIRECTORY_NAME, sizeof SourceFileMap::Path);
	#endif

	SourceFileMap::Initialized = true;
}

void SourceFileMap::Load() {
	if (SourceFileMap::Loaded) {
		return;
	}

	if (ResourceManager::ResourceExists(OBJECTS_HCM_NAME)) {
		ResourceStream* stream = ResourceStream::New(OBJECTS_HCM_NAME);
		if (stream) {
			SourceFileMap::ReadClassMap(stream, SourceFileMap::ClassMap);
			stream->Close();
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Could not find ClassMap!");
	}

#ifndef NO_SCRIPT_COMPILING
	SourceFileMap::AllowCompilation = true;
	SourceFileMap::ReadFileMap();
#endif

	SourceFileMap::Loaded = true;
}
void SourceFileMap::ReadFileMap() {
	Stream* stream = FileStream::New(SOURCEFILEMAP_NAME, FileStream::READ_ACCESS, true);
	if (!stream) {
		return;
	}

	size_t len = stream->Length();
	char* bytes = (char*)Memory::Malloc(len);
	if (!bytes) {
		stream->Close();
		return;
	}

	stream->ReadBytes(bytes, len);
	stream->Close();

	if (len >= sizeof(Uint32) * 3) {
		SourceFileMap::Checksums->FromBytes((Uint8*)bytes,
			(len - sizeof(Uint32)) / (sizeof(Uint32) + sizeof(Uint32)));
		SourceFileMap::DirectoryChecksum = *(Uint32*)(bytes + len - sizeof(Uint32));
	}
	else {
		SourceFileMap::DirectoryChecksum = 0;
	}

	Memory::Free(bytes);
}
void SourceFileMap::WriteFileMap() {
	Stream* stream = FileStream::New(SOURCEFILEMAP_NAME, FileStream::WRITE_ACCESS, true);
	if (!stream) {
		return;
	}

	size_t size = SourceFileMap::Checksums->Count() * (sizeof(Uint32) + sizeof(Uint32));
	Uint8* data = (Uint8*)Memory::Malloc(size);
	if (data) {
		SourceFileMap::Checksums->GetBytes(data);
		stream->WriteBytes(data, size);
		Memory::Free(data);

		stream->WriteUInt32(SourceFileMap::DirectoryChecksum);
	}

	stream->Close();
}
bool SourceFileMap::ReadClassMap(Stream* stream, HashMap<vector<Uint32>*>* classMap) {
	Uint32 magic_got;
	if ((magic_got = stream->ReadUInt32()) != SourceFileMap::Magic) {
		Log::Print(Log::LOG_ERROR,
			"Invalid ClassMap! (Expected %08X, was %08X)",
			SourceFileMap::Magic,
			magic_got);
		return false;
	}

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

		classMap->Put(classHash, fnList);
	}

	return true;
}
void SourceFileMap::WriteClassMap(Stream* stream, HashMap<vector<Uint32>*>* classMap) {
	stream->WriteUInt32(SourceFileMap::Magic);
	stream->WriteByte(0x00); // Version
	stream->WriteByte(0x01); // Version
	stream->WriteByte(0x02); // Version
	stream->WriteByte(0x03); // Version

	stream->WriteUInt32((Uint32)classMap->Count()); // Count
	classMap->WithAll([stream](Uint32 hash, vector<Uint32>* list) -> void {
		stream->WriteUInt32(hash); // ClassHash
		stream->WriteUInt32((Uint32)list->size()); // Count
		for (size_t fn = 0; fn < list->size(); fn++) {
			stream->WriteUInt32((*list)[fn]);
		}
	});
}
void SourceFileMap::ClearClassMap(HashMap<vector<Uint32>*>* classMap) {
	classMap->WithAll([](Uint32, vector<Uint32>* list) -> void {
		list->clear();
		list->shrink_to_fit();
		delete list;
	});
	classMap->Clear();
}
bool SourceFileMap::CheckForUpdate() {
	SourceFileMap::RemoveModScripts();

	bool anyChanges = SourceFileMap::CheckForBaseUpdate();

	// Mods with a higher priority are added last, so their methods are the ones used
	bool anyModChanges = false;
	for (int m = (int)ModLoader::Mods.size() - 1; m >= 0; m--) {
		ModInfo* mod = &ModLoader::Mods[m];
		if (!mod->Active) {
			continue;
		}

		HashMap<vector<Uint32>*>* modClassMap = new HashMap<vector<Uint32>*>(Murmur::EncryptData, 16);

		SourceFileMap::LoadModClassMap(mod, modClassMap);
		anyModChanges |= SourceFileMap::CheckForModUpdate(mod, modClassMap);
		SourceFileMap::AddModScripts(modClassMap);

		SourceFileMap::ClearClassMap(modClassMap);
		delete modClassMap;
	}

	if (anyModChanges) {
		SourceFileMap::WriteFileMap();
	}

	return anyChanges || anyModChanges;
}
bool SourceFileMap::CheckForBaseUpdate() {
	SourceFileMap::Load();

#ifndef NO_SCRIPT_COMPILING
	if (!SourceFileMap::AllowCompilation) {
		return false;
	}

	VFSProvider* mainVfs = ResourceManager::GetMainResource();
	if (!mainVfs || !mainVfs->IsWritable()) {
		return false;
	}

	bool anyChanges = false;

	if (!Directory::Exists(SourceFileMap::Path)) {
		return false;
	}

	vector<std::filesystem::path> list;
	Directory::GetFiles(&list, SourceFileMap::Path, "*.hsl", true);

	if (list.size() == 0) {
		list.clear();
		list.shrink_to_fit();
		return false;
	}

	Directory::SortEntries(&list);

	if (!mainVfs->HasFile(OBJECTS_HCM_NAME)) {
		anyChanges = true;
	}

	size_t scriptFolderNameLen = strlen(SourceFileMap::Path);

	Uint32 oldDirectoryChecksum = SourceFileMap::DirectoryChecksum;

	SourceFileMap::DirectoryChecksum = 0x0;
	for (size_t i = 0; i < list.size(); i++) {
		std::string asStr = Path::ToString(list[i]);
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
		SourceFileMap::ClearClassMap(SourceFileMap::ClassMap);
	}

	std::string scriptFolderPathStr = std::string(SourceFileMap::Path) + "/";
	const char* scriptFolderPath = scriptFolderPathStr.c_str();
	size_t scriptFolderPathLen = scriptFolderPathStr.size();

	for (size_t i = 0; i < list.size(); i++) {
		std::string asStr = Path::ToString(list[i]);
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

		// Read the script file
		char* source = SourceFileMap::ReadSource(listEntry);
		if (source == nullptr) {
			continue;
		}

		newChecksum = Murmur::EncryptString(source);

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

			MemoryStream* memStream = MemoryStream::New(0x100);
			if (memStream) {
				if (SourceFileMap::CompileScript(scriptFilename, source, filenameHash, SourceFileMap::ClassMap, memStream)) {
					Stream* stream = mainVfs->OpenWriteStream(outFile);
					if (stream) {
						stream->WriteBytes(memStream->pointer_start, memStream->Position());
						mainVfs->CloseStream(stream);
					}
					else {
						Log::Print(Log::LOG_ERROR, "Couldn't open \"%s\"!", outFile);
					}

					// Update checksum
					SourceFileMap::Checksums->Put(filenameHash, newChecksum);
				}

				memStream->Close();
			}
			else {
				Log::Print(Log::LOG_ERROR, "Not enough memory for compiling \"%s\"!", outFile);
			}
		}

		Memory::Free(source);
	}

	if (anyChanges) {
		SourceFileMap::WriteFileMap();

		Stream* stream = mainVfs->OpenWriteStream(OBJECTS_HCM_NAME);
		if (stream) {
			SourceFileMap::WriteClassMap(stream, SourceFileMap::ClassMap);
			mainVfs->CloseStream(stream);
		}
	}

	list.clear();
	list.shrink_to_fit();

	return anyChanges;
#else
	return false;
#endif
}
char* SourceFileMap::ReadSource(const char* filename) {
	char* source = nullptr;

	Stream* stream = File::Open(filename, File::READ_ACCESS);
	if (stream) {
		size_t streamSize = stream->Length();
		if (streamSize > 0) {
			source = (char*)Memory::TrackedCalloc("SourceFileMap::SourceText", streamSize + 1, sizeof(char));
			if (source) {
				stream->ReadBytes(source, streamSize);
			}
			else {
				Log::Print(Log::LOG_ERROR, "Not enough memory for reading \"%s\"!", filename);
			}
		}
		stream->Close();
	}
	else {
		Log::Print(Log::LOG_ERROR, "Couldn't open \"%s\"!", filename);
	}

	return source;
}
bool SourceFileMap::CompileScript(const char* scriptFilename, char* source, Uint32 filenameHash, HashMap<vector<Uint32>*>* classMap, Stream* output) {
	Compiler::PrepareCompiling();

	bool didCompile = false;

	Compiler* compiler = new Compiler;
	compiler->CurrentSettings = Compiler::Settings;

	compiler->Initialize();
	compiler->SetupLocals();

	try {
		didCompile = compiler->Compile(scriptFilename, source, output);
	}
	catch (const CompilerErrorException& error) {
		HandleCompileError(error.what());
	}

	if (didCompile) {
		AddToList(classMap, compiler, filenameHash);
	}

	delete compiler;

	Compiler::FinishCompiling();

	return didCompile;
}
void SourceFileMap::HandleCompileError(const char* error) {
	Log::Print(Log::LOG_ERROR, error);

	const SDL_MessageBoxButtonData buttons[] = {
		{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Exit"},
	};

	const SDL_MessageBoxData messageBoxData = {
		SDL_MESSAGEBOX_ERROR,
		NULL,
		"Compile Error",
		error,
		SDL_arraysize(buttons),
		buttons,
		NULL,
	};

	int buttonClicked;
	SDL_ShowMessageBox(&messageBoxData, &buttonClicked);

	Application::Cleanup();
	exit(-1);
}
void SourceFileMap::AddToList(HashMap<vector<Uint32>*>* classMap, Compiler* compiler, Uint32 filenameHash) {
	for (size_t h = 0; h < compiler->ClassHashList.size(); h++) {
		Uint32 classHash = compiler->ClassHashList[h];
		Uint32 classExtended = compiler->ClassExtendedList[h];
		if (classMap->Exists(classHash)) {
			vector<Uint32>* filenameHashList = classMap->Get(classHash);
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
			classMap->Put(classHash, filenameHashList);
		}
	}
}

int SourceFileMap::GetModBytecodeVersion(ModInfo* mod, Uint32 filenameHash) {
	std::string filename = ScriptManager::GetBytecodeFilenameForHash(filenameHash);

	Uint8* data = nullptr;
	size_t size = 0;
	if (!ModLoader::LoadModFile(mod, filename.c_str(), &data, &size)) {
		return -1;
	}

	int version = -1;
	if (size > 4 && memcmp(data, Bytecode::Magic, 4) == 0) {
		version = data[4];
	}

	Memory::Free(data);

	return version;
}
void SourceFileMap::LoadModClassMap(ModInfo* mod, HashMap<vector<Uint32>*>* modClassMap) {
	Uint8* data = nullptr;
	size_t size = 0;
	if (!ModLoader::LoadModFile(mod, MOD_OBJECTS_HCM_NAME, &data, &size)) {
		return;
	}

	MemoryStream* stream = MemoryStream::New(data, size);
	if (stream) {
		SourceFileMap::ReadClassMap(stream, modClassMap);
		stream->Close();
	}

	Memory::Free(data);

    // Prevent scripts compiled by a newer version of the engine from being loaded
	bool isNewer = false;
	modClassMap->WithAll([mod, &isNewer](Uint32, vector<Uint32>* list) -> void {
		for (size_t fn = 0; fn < list->size(); fn++) {
			if (SourceFileMap::GetModBytecodeVersion(mod, (*list)[fn]) > (int)Bytecode::LatestVersion) {
				isNewer = true;
			}
		}
	});

	if (isNewer) {
		Log::Print(Log::LOG_WARN, "[MOD] Mod %s was compiled by a newer version of the engine, its scripts won't be loaded.", mod->FolderName.c_str());
		SourceFileMap::ClearClassMap(modClassMap);
	}
}
bool SourceFileMap::CheckForModUpdate(ModInfo* mod, HashMap<vector<Uint32>*>* modClassMap) {
#ifndef NO_SCRIPT_COMPILING
	if (!SourceFileMap::AllowCompilation) {
		return false;
	}

	std::string scriptsPath = mod->Path + "/" + SCRIPTS_DIRECTORY_NAME;
	if (!Directory::Exists(scriptsPath.c_str())) {
		return false;
	}

	vector<std::filesystem::path> list;
	Directory::GetFiles(&list, scriptsPath.c_str(), "*.hsl", true);

	if (list.size() == 0) {
		return false;
	}

	Directory::SortEntries(&list);

	std::string resourcesPath = mod->Path + "/" + MODLOADER_RESOURCES_DIR_NAME;
	std::string hcmPath = resourcesPath + "/" + MOD_OBJECTS_HCM_NAME;

	bool doRecompile = modClassMap->Count() == 0;

	vector<std::string> scriptFilenames;
	vector<Uint32> filenameHashes;
	vector<Uint32> checksums;
	vector<char*> sources;

	for (size_t i = 0; i < list.size(); i++) {
		std::string asStr = Path::ToString(list[i]);
		std::string relativePath = asStr.substr(scriptsPath.size() + 1);

		// The mod's folder is part of the hash, so a mod's script can't replace one of the game's
		std::string hashName = mod->FolderName + "/" + relativePath;
		Uint32 filenameHash = ScriptManager::MakeFilenameHash(hashName.c_str());

		char* source = SourceFileMap::ReadSource(asStr.c_str());
		if (source == nullptr) {
			continue;
		}

		Uint32 newChecksum = Murmur::EncryptString(source);
		Uint32 oldChecksum = 0;

		if (SourceFileMap::Checksums->Exists(filenameHash)) {
			oldChecksum = SourceFileMap::Checksums->Get(filenameHash);
		}
		if (newChecksum != oldChecksum) {
			doRecompile = true;
		}

		// Missing, or compiled by another version of the engine
		if (SourceFileMap::GetModBytecodeVersion(mod, filenameHash) != (int)Bytecode::LatestVersion) {
			doRecompile = true;
		}

		scriptFilenames.push_back(std::string(MODLOADER_DIR_NAME) + "/" + mod->FolderName + "/" + SCRIPTS_DIRECTORY_NAME + "/" + relativePath);
		filenameHashes.push_back(filenameHash);
		checksums.push_back(newChecksum);
		sources.push_back(source);
	}

	// Check for deleted files
	modClassMap->WithAll([&filenameHashes, &doRecompile](Uint32, vector<Uint32>* list) -> void {
		for (size_t fn = 0; fn < list->size(); fn++) {
			if (std::count(filenameHashes.begin(), filenameHashes.end(), (*list)[fn]) == 0) {
				doRecompile = true;
			}
		}
	});

	if (doRecompile) {
		SourceFileMap::ClearClassMap(modClassMap);

		Directory::Create(resourcesPath.c_str());
		Directory::Create((resourcesPath + "/" + OBJECTS_DIR_NAME).c_str());

		for (size_t i = 0; i < sources.size(); i++) {
			const char* scriptFilename = scriptFilenames[i].c_str();

			if (Compiler::DoLogging) {
				Log::Print(Log::LOG_VERBOSE, "Compiling %s...", scriptFilename);
			}

			MemoryStream* memStream = MemoryStream::New(0x100);
			if (memStream) {
				if (SourceFileMap::CompileScript(scriptFilename, sources[i], filenameHashes[i], modClassMap, memStream)) {
					std::string outFile = ScriptManager::GetBytecodeFilenameForHash(filenameHashes[i]);
					std::string outPath = resourcesPath + "/" + outFile;
					if (File::WriteAllBytes(outPath.c_str(), (const char*)memStream->pointer_start, memStream->Position())) {
						ModLoader::AddFile(mod, outFile.c_str(), outPath);
					}
					else {
						Log::Print(Log::LOG_ERROR, "Couldn't open \"%s\"!", outPath.c_str());
					}

					// Update checksum
					SourceFileMap::Checksums->Put(filenameHashes[i], checksums[i]);
				}

				memStream->Close();
			}
			else {
				Log::Print(Log::LOG_ERROR, "Not enough memory for compiling \"%s\"!", scriptFilename);
			}
		}

		Stream* stream = File::Open(hcmPath.c_str(), File::WRITE_ACCESS);
		if (stream) {
			SourceFileMap::WriteClassMap(stream, modClassMap);
			stream->Close();

			ModLoader::AddFile(mod, MOD_OBJECTS_HCM_NAME, hcmPath);
		}
		else {
			Log::Print(Log::LOG_ERROR, "Couldn't open \"%s\"!", hcmPath.c_str());
		}
	}

	for (size_t i = 0; i < sources.size(); i++) {
		Memory::Free(sources[i]);
	}

	return doRecompile;
#else
	return false;
#endif
}
void SourceFileMap::AddModScripts(HashMap<vector<Uint32>*>* modClassMap) {
	modClassMap->WithAll([](Uint32 classHash, vector<Uint32>* modList) -> void {
		vector<Uint32>* filenameHashList;
		if (SourceFileMap::ClassMap->Exists(classHash)) {
			filenameHashList = SourceFileMap::ClassMap->Get(classHash);
		}
		else {
			filenameHashList = new vector<Uint32>();
			SourceFileMap::ClassMap->Put(classHash, filenameHashList);
		}

		for (size_t fn = 0; fn < modList->size(); fn++) {
			filenameHashList->push_back((*modList)[fn]);
			SourceFileMap::ModFilenameHashes.push_back((*modList)[fn]);
		}
	});
}
void SourceFileMap::RemoveModScripts() {
	if (SourceFileMap::ModFilenameHashes.size() == 0) {
		return;
	}

	vector<Uint32> emptyClasses;
	SourceFileMap::ClassMap->WithAll([&emptyClasses](Uint32 classHash, vector<Uint32>* list) -> void {
		for (size_t fn = list->size(); fn > 0; fn--) {
			if (SourceFileMap::IsModFilename((*list)[fn - 1])) {
				list->erase(list->begin() + (fn - 1));
			}
		}
		if (list->size() == 0) {
			emptyClasses.push_back(classHash);
		}
	});

	for (size_t i = 0; i < emptyClasses.size(); i++) {
		delete SourceFileMap::ClassMap->Get(emptyClasses[i]);
		SourceFileMap::ClassMap->Remove(emptyClasses[i]);
	}

	SourceFileMap::ModFilenameHashes.clear();
}
bool SourceFileMap::IsModFilename(Uint32 filenameHash) {
	return std::count(SourceFileMap::ModFilenameHashes.begin(), SourceFileMap::ModFilenameHashes.end(), filenameHash) != 0;
}

void SourceFileMap::Dispose() {
	if (SourceFileMap::Checksums) {
		delete SourceFileMap::Checksums;
	}
	if (SourceFileMap::ClassMap) {
		SourceFileMap::ClearClassMap(SourceFileMap::ClassMap);
		delete SourceFileMap::ClassMap;
	}

	SourceFileMap::ModFilenameHashes.clear();

	SourceFileMap::Loaded = false;
	SourceFileMap::Initialized = false;
	SourceFileMap::Checksums = NULL;
	SourceFileMap::ClassMap = NULL;
}
