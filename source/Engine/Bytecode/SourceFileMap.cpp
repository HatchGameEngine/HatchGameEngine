#include <Engine/Application.h>
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
#include <Engine/IO/VirtualFileStream.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/Utilities/StringUtils.h>

#define SOURCEFILEMAP_NAME "cache://SourceFileMap.bin"

#define OBJECTS_HCM_NAME OBJECTS_DIR_NAME "/Objects.hcm"

bool SourceFileMap::Initialized = false;
bool SourceFileMap::AllowCompilation = false;
VirtualFileSystem* SourceFileMap::ScriptsVFS = NULL;
HashMap<Uint32>* SourceFileMap::Checksums = NULL;
HashMap<vector<Uint32>*>* SourceFileMap::ClassMap = NULL;
Uint32 SourceFileMap::DirectoryChecksum = 0;
Uint32 SourceFileMap::Magic = MAGIC_LE32("HMAP");

void SourceFileMap::Initialize() {
	if (SourceFileMap::Initialized) {
		return;
	}

	if (SourceFileMap::Checksums == NULL) {
		SourceFileMap::Checksums = new HashMap<Uint32>(CombinedHash::EncryptData, 16);
	}
	if (SourceFileMap::ClassMap == NULL) {
		SourceFileMap::ClassMap = new HashMap<vector<Uint32>*>(Murmur::EncryptData, 16);
	}

	VirtualFileSystem* bytecodeVfs = ScriptManager::BytecodeVFS;
	if (bytecodeVfs && bytecodeVfs->FileExists(OBJECTS_HCM_NAME)) {
		VirtualFileStream* stream = VirtualFileStream::New(bytecodeVfs,
			OBJECTS_HCM_NAME,
			VirtualFileStream::READ_ACCESS);
		if (stream) {
			ReadClassMap(stream);
			stream->Close();
		}
		else {
			Log::Print(Log::LOG_ERROR, "Could not read ClassMap!");
		}
	}
	else {
		Log::Print(Log::LOG_ERROR, "Could not find ClassMap!");
	}

#ifndef NO_SCRIPT_COMPILING
	SourceFileMap::AllowCompilation = true;

	if (File::ProtectedExists(SOURCEFILEMAP_NAME, true)) {
		char* bytes;
		size_t len = File::ReadAllBytes(SOURCEFILEMAP_NAME, &bytes, true);
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

	ScriptsVFS = new VirtualFileSystem();

	const char* scriptsPath = Application::GetScriptsPath();
	if (scriptsPath && !MountScriptsVFS(scriptsPath)) {
		Log::Print(Log::LOG_ERROR, "Could not access scripts path \"%s\"!", scriptsPath);
	}
#endif

	SourceFileMap::Initialized = true;
}
bool SourceFileMap::ReadClassMap(Stream* stream) {
	Uint32 magic = stream->ReadUInt32();
	if (magic != SourceFileMap::Magic) {
		Log::Print(Log::LOG_ERROR,
			"Invalid ClassMap! (Expected %08X, was %08X)",
			SourceFileMap::Magic,
			magic);
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

		SourceFileMap::ClassMap->Put(classHash, fnList);
	}

	return true;
}
bool SourceFileMap::MountScriptsVFS(const char* scriptsPath) {
	if (scriptsPath[strlen(scriptsPath) - 1] == '/') {
		return ScriptsVFS->Mount(
			"scripts", scriptsPath, nullptr, VFSType::FILESYSTEM, VFS_READABLE);
	}
	else {
		return ScriptsVFS->Mount(
			"scripts", scriptsPath, nullptr, VFSType::HATCH, VFS_READABLE);
	}
}
bool SourceFileMap::CheckForUpdate() {
	if (!SourceFileMap::Initialized) {
		SourceFileMap::Initialize();
	}

#ifndef NO_SCRIPT_COMPILING
	bool anyChanges = false;

	if (!SourceFileMap::AllowCompilation) {
		return false;
	}

	if (!ScriptsVFS) {
		return false;
	}

	if (ScriptsVFS->IsEmpty()) {
		Log::Print(Log::LOG_WARN, "No scripts to be compiled.");
		return false;
	}

	VirtualFileSystem* bytecodeVfs = ScriptManager::BytecodeVFS;
	if (!bytecodeVfs || !bytecodeVfs->IsWritable()) {
		return false;
	}

	VFSProvider* provider = ScriptsVFS->GetByMountPoint(DEFAULT_MOUNT_POINT);
	VFSEnumeration enumeration = provider->EnumerateFiles(nullptr, "*.hsl");
	if (enumeration.Result != VFSEnumerationResult::SUCCESS) {
		return false;
	}

	if (!bytecodeVfs->FileExists(OBJECTS_HCM_NAME)) {
		anyChanges = true;
	}

	Uint32 oldDirectoryChecksum = SourceFileMap::DirectoryChecksum;

	SourceFileMap::DirectoryChecksum = 0x0;
	for (size_t i = 0; i < enumeration.Entries.size(); i++) {
		std::string entry = enumeration.Entries[i];
		SourceFileMap::DirectoryChecksum = FNV1A::EncryptData(
			entry.c_str(), entry.size(), SourceFileMap::DirectoryChecksum);
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

	for (size_t i = 0; i < enumeration.Entries.size(); i++) {
		std::string entry = enumeration.Entries[i];
		const char* listEntry = entry.c_str();
		Uint32 filenameHash = ScriptManager::MakeFilenameHash(listEntry);
		if (!filenameHash) {
			continue;
		}

		Uint32 newChecksum = 0;
		Uint32 oldChecksum = 0;
		bool doRecompile = false;

		Uint8* source;
		size_t sourceLength;
		if (!ScriptsVFS->LoadFile(listEntry, &source, &sourceLength)) {
			Log::Print(Log::LOG_ERROR, "Couldn't read script '%s'!", listEntry);
			continue;
		}
		newChecksum = Murmur::EncryptData(source, sourceLength);

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
		if (doRecompile || !bytecodeVfs->FileExists(outFile)) {
			Compiler::PrepareCompiling();

			if (Compiler::DoLogging) {
				if (doRecompile) {
					Log::Print(
						Log::LOG_VERBOSE, "Recompiling %s...", listEntry);
				}
				else {
					Log::Print(Log::LOG_VERBOSE, "Compiling %s...", listEntry);
				}
			}

			MemoryStream* memStream = MemoryStream::New(0x100);
			if (memStream) {
				bool didCompile = false;

				Compiler* compiler = new Compiler;
				compiler->CurrentSettings = Compiler::Settings;

				compiler->Initialize();
				compiler->SetupLocals();

				try {
					didCompile = compiler->Compile(
						listEntry, (char*)source, sourceLength, memStream);
				} catch (const CompilerErrorException& error) {
					HandleCompileError(error.what());
				}

				if (didCompile) {
					Stream* stream = bytecodeVfs->OpenWriteStream(outFile);
					if (stream) {
						stream->WriteBytes(memStream->pointer_start,
							memStream->Position());
						bytecodeVfs->CloseStream(stream);
					}
					else {
						Log::Print(Log::LOG_ERROR,
							"Couldn't open file '%s' for writing compiled script!",
							outFile);
					}

					// Add this file to the list
					AddToList(compiler, filenameHash);

					// Update checksum
					SourceFileMap::Checksums->Put(filenameHash, newChecksum);
				}

				memStream->Close();

				delete compiler;
			}
			else {
				Log::Print(Log::LOG_ERROR,
					"Not enough memory for compiling '%s'!",
					outFile);
			}

			Compiler::FinishCompiling();
		}

		Memory::Free(source);
	}

	if (anyChanges) {
		Stream* stream =
			FileStream::New(SOURCEFILEMAP_NAME, FileStream::WRITE_ACCESS, true);
		if (stream) {
			size_t size = SourceFileMap::Checksums->Count() *
				(sizeof(Uint32) + sizeof(Uint32));
			Uint8* data = (Uint8*)Memory::Malloc(size);
			if (data) {
				SourceFileMap::Checksums->GetBytes(data);
				stream->WriteBytes(data, size);
				Memory::Free(data);

				stream->WriteUInt32(SourceFileMap::DirectoryChecksum);
			}

			stream->Close();
		}

		stream = bytecodeVfs->OpenWriteStream(OBJECTS_HCM_NAME);
		if (stream) {
			stream->WriteUInt32(SourceFileMap::Magic);
			stream->WriteByte(0x00); // Version
			stream->WriteByte(0x01); // Version
			stream->WriteByte(0x02); // Version
			stream->WriteByte(0x03); // Version

			stream->WriteUInt32((Uint32)SourceFileMap::ClassMap->Count()); // Count
			SourceFileMap::ClassMap->WithAll(
				[stream](Uint32 hash, vector<Uint32>* list) -> void {
					stream->WriteUInt32(hash); // ClassHash
					stream->WriteUInt32((Uint32)list->size()); // Count
					for (size_t fn = 0; fn < list->size(); fn++) {
						stream->WriteUInt32((*list)[fn]);
					}
				});

			bytecodeVfs->CloseStream(stream);
		}
	}

	return anyChanges;
#else
	return false;
#endif
}
void SourceFileMap::HandleCompileError(const char* error) {
	Log::Print(Log::LOG_ERROR, error);

	const SDL_MessageBoxButtonData buttons[] = {
		{SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Exit"},
	};

	const SDL_MessageBoxData messageBoxData = {
		SDL_MESSAGEBOX_ERROR,
		NULL,
		"Syntax Error",
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
	if (SourceFileMap::ScriptsVFS) {
		delete SourceFileMap::ScriptsVFS;
		SourceFileMap::ScriptsVFS = nullptr;
	}
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
	SourceFileMap::Checksums = nullptr;
	SourceFileMap::ClassMap = nullptr;
}
