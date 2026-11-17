#include <Engine/ResourceTypes/ModLoader.h>

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/Path.h>
#include <Engine/Filesystem/VFS/HatchVFS.h>
#include <Engine/IO/Stream.h>
#include <Engine/TextFormats/INI/INI.h>
#include <Engine/Utilities/StringUtils.h>

#include <cctype>

std::vector<ModInfo> ModLoader::Mods;
std::string ModLoader::ModsPath = MODLOADER_DIR_NAME;
std::string ModLoader::CustomUserFileDir = "";

static INI* LoadModINI(const char* filename) {
	Stream* stream = File::Open(filename, File::READ_ACCESS);
	if (!stream) {
		return nullptr;
	}

	INI* ini = INI::New(filename);
	ini->Read(stream);
	stream->Close();

	return ini;
}
static std::string GetModINIString(INI* ini, const char* key, const char* defaultValue) {
	const char* value = ini->GetProperty(nullptr, key);
	return value ? value : defaultValue;
}
static std::string GetLowercasePath(const char* filename) {
	std::string pathLower = filename;
	for (size_t c = 0; c < pathLower.size(); c++) {
		pathLower[c] = (char)tolower((unsigned char)pathLower[c]);
	}
	return pathLower;
}

void ModLoader::Init() {
	std::string resolved = "";
	if (Path::FromURL(PATHLOCATION_GAME_URL MODLOADER_DIR_NAME, resolved)) {
		ModsPath = resolved;
	}
	else {
		ModsPath = MODLOADER_DIR_NAME;
	}

	if (!ModsPath.empty() && ModsPath.back() == '/') {
		ModsPath.pop_back();
	}

	LoadMods(false);

	int activeCount = GetModCount(true);
	if (Mods.size() > 0) {
		Log::Print(Log::LOG_INFO, "[MOD] %d of %d mod(s) active.", activeCount, (int)Mods.size());
	}
	for (size_t i = 0; i < Mods.size(); i++) {
		if (Mods[i].Active) {
			Log::Print(Log::LOG_INFO, "[MOD] #%d: %s (%s) v%s by %s, %d file(s), %d data pack(s)", (int)i + 1, Mods[i].Name.c_str(), Mods[i].FolderName.c_str(), Mods[i].Version.c_str(), Mods[i].Author.c_str(), (int)Mods[i].FileMap.size(), (int)Mods[i].Packs.size());
		}
	}
}

void ModLoader::LoadMods(bool newOnly) {
	if (!newOnly) {
		UnloadMods();
	}

	if (Directory::Exists(ModsPath.c_str())) {
		std::string modConfig = ModsPath + "/" + MODLOADER_CONFIG_NAME;
		INI* ini = LoadModINI(modConfig.c_str());
		INISection* section = ini ? ini->FindSection("Mods") : nullptr;
		if (section) {
			for (size_t m = 0; m < section->Properties.size(); m++) {
				std::string folder = section->Properties[m]->Name;

				if (newOnly) {
					bool alreadyLoaded = false;
					for (size_t i = 0; i < Mods.size(); i++) {
						if (Mods[i].FolderName == folder) {
							alreadyLoaded = true;
							break;
						}
					}
					if (alreadyLoaded) {
						continue;
					}
				}

				ModInfo info;
				bool active = false;
				ini->GetBool("Mods", folder.c_str(), &active);
				if (!LoadMod(&info, ModsPath, folder, active)) {
					Log::Print(Log::LOG_WARN, "[MOD] Mod %s doesn't exist!", folder.c_str());
					continue;
				}
				Log::Print(Log::LOG_INFO, "[MOD] Loaded mod %s! Active: %s", info.ID.c_str(), active ? "Y" : "N");
				Mods.push_back(info);
			}
		}
		if (ini) {
			ini->Dispose();
		}

		std::vector<std::filesystem::path> folderList;
		Directory::GetDirectories(&folderList, ModsPath.c_str(), "*", false);
		Directory::SortEntries(&folderList);

		for (size_t f = 0; f < folderList.size(); f++) {
			std::string folder = Path::ToString(folderList[f].filename());

			bool listed = false;
			for (size_t i = 0; i < Mods.size(); i++) {
				if (Mods[i].FolderName == folder) {
					listed = true;
					break;
				}
			}
			if (listed) {
				continue;
			}

			std::string modIni = ModsPath + "/" + folder + "/" + MODLOADER_INFO_NAME;
			if (File::Exists(modIni.c_str())) {
				ModInfo info;
				LoadMod(&info, ModsPath, folder, false);
				Mods.push_back(info);
			}
		}
	}

	SortMods();
	LoadModSettings();
}

bool ModLoader::LoadMod(ModInfo* info, std::string modsPath, std::string folder, bool active) {
	if (!info) {
		return false;
	}

	Log::Print(Log::LOG_VERBOSE, "[MOD] Trying to load mod %s...", folder.c_str());

	info->FileMap.clear();
	info->Name = "";
	info->Description = "";
	info->Author = "";
	info->Version = "";
	info->ID = "";
	info->Active = false;
	info->RedirectSave = false;

	std::string modDir = modsPath + "/" + folder;
	std::string modIniPath = modDir + "/" + MODLOADER_INFO_NAME;

	if (!File::Exists(modIniPath.c_str())) {
		return false;
	}

	INI* modIni = LoadModINI(modIniPath.c_str());
	if (!modIni) {
		return false;
	}

	info->Path = modDir;
	info->FolderName = folder;
	info->ID = GetModINIString(modIni, "ModID", folder.c_str());
	info->Active = active;

	info->Name = GetModINIString(modIni, "Name", "Unnamed Mod");
	info->Description = GetModINIString(modIni, "Description", "");
	info->Author = GetModINIString(modIni, "Author", "Unknown Author");
	info->Version = GetModINIString(modIni, "Version", "1.0.0");
	modIni->GetBool(nullptr, "RedirectSave", &info->RedirectSave);

	modIni->Dispose();

	if (!active) {
		return true;
	}

	ScanModFolder(info);

	return true;
}

bool ModLoader::ScanModFolder(ModInfo* info) {
	if (!info) {
		return false;
	}

	info->FileMap.clear();

	std::string dataPath = info->Path + "/" + MODLOADER_RESOURCES_DIR_NAME;
	if (Directory::Exists(dataPath.c_str())) {
		std::vector<std::filesystem::path> fileList;
		Directory::GetFiles(&fileList, dataPath.c_str(), "*", true);

		for (size_t i = 0; i < fileList.size(); i++) {
			std::string fullPath = Path::ToString(fileList[i]);
			std::string folderPath = GetLowercasePath(fullPath.substr(dataPath.length() + 1).c_str());

			info->FileMap.insert(std::pair<std::string, std::string>(folderPath, fullPath));
		}
	}

	OpenPacks(info);

	return true;
}

void ModLoader::OpenPacks(ModInfo* info) {
	ClosePacks(info);

	if (!Directory::Exists(info->Path.c_str())) {
		return;
	}

	std::vector<std::filesystem::path> packList;
	Directory::GetFiles(&packList, info->Path.c_str(), MODLOADER_PACK_PATTERN, false);
	Directory::SortEntries(&packList);

	for (size_t i = 0; i < packList.size(); i++) {
		std::string packPath = Path::ToString(packList[i]);

		Stream* stream = File::Open(packPath.c_str(), File::READ_ACCESS);
		if (!stream) {
			Log::Print(Log::LOG_ERROR, "[MOD] Couldn't open \"%s\"!", packPath.c_str());
			continue;
		}

		// The pack needs to keep the stream once it opens
		HatchVFS* pack = new HatchVFS(VFS_READABLE);
		if (!pack->Open(stream) || !pack->IsOpen()) {
			Log::Print(Log::LOG_ERROR, "[MOD] \"%s\" isn't a valid data pack!", packPath.c_str());
			delete pack;
			stream->Close();
			continue;
		}

		Log::Print(Log::LOG_VERBOSE, "[MOD] Opened data pack %s", StringUtils::GetFilename(packPath.c_str()));
		info->Packs.push_back(pack);
	}
}

void ModLoader::ClosePacks(ModInfo* info) {
	for (size_t i = 0; i < info->Packs.size(); i++) {
		delete info->Packs[i];
	}
	info->Packs.clear();
}

void ModLoader::SortMods() {
	std::stable_sort(Mods.begin(), Mods.end(), [](const ModInfo& a, const ModInfo& b) { return a.Active && !b.Active; });
}

void ModLoader::LoadModSettings() {
	CustomUserFileDir = "";

	int start = (int)Mods.size() - 1;
	while (start != -1 && !Mods[start].Active) {
		start--;
	}

	for (int i = start; i >= 0; i--) {
		ModInfo* mod = &Mods[i];

		if (mod->RedirectSave) {
			CustomUserFileDir = ModsPath + "/" + mod->FolderName + "/";
		}
	}
}

void ModLoader::SaveMods() {
	SortMods();

	Log::Print(Log::LOG_INFO, "[MOD] Saving mods...");

	if (!Directory::Exists(ModsPath.c_str())) {
		return;
	}

	std::string modConfig = ModsPath + "/" + MODLOADER_CONFIG_NAME;
	Stream* stream = File::Open(modConfig.c_str(), File::WRITE_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "[MOD] Couldn't open \"%s\" for writing!", modConfig.c_str());
		return;
	}

	INI* ini = INI::New(modConfig.c_str());
	ini->AddSection("Mods");
	for (size_t m = 0; m < Mods.size(); m++) {
		ini->SetBool("Mods", Mods[m].FolderName.c_str(), Mods[m].Active);
	}

	ini->Write(stream);
	stream->Close();
	ini->Dispose();
}

void ModLoader::UnloadMods() {
	for (size_t m = 0; m < Mods.size(); m++) {
		ClosePacks(&Mods[m]);
	}
	Mods.clear();
	CustomUserFileDir = "";
}

static bool FindInMod(ModInfo* mod, const char* filename, std::string& pathLower, std::string& outPath, VFSProvider*& outPack) {
	std::map<std::string, std::string>::const_iterator iter = mod->FileMap.find(pathLower);
	if (iter != mod->FileMap.cend()) {
		outPath = iter->second;
		outPack = nullptr;
		return true;
	}

	for (size_t p = 0; p < mod->Packs.size(); p++) {
		if (mod->Packs[p]->HasFile(filename)) {
			outPack = mod->Packs[p];
			return true;
		}
	}

	return false;
}
static bool ReadModFile(const char* filename, std::string& path, VFSProvider* pack, Uint8** out, size_t* size) {
	if (pack) {
		return pack->ReadFile(filename, out, size);
	}

	Stream* stream = File::Open(path.c_str(), File::READ_ACCESS);
	if (!stream) {
		Log::Print(Log::LOG_ERROR, "[MOD] File not found: %s", path.c_str());
		return false;
	}

	size_t length = stream->Length();
	Uint8* memory = (Uint8*)Memory::Malloc(length);
	if (!memory) {
		stream->Close();
		return false;
	}

	stream->ReadBytes(memory, length);
	stream->Close();

	*out = memory;
	*size = length;

	return true;
}
bool ModLoader::FileExists(const char* filename) {
	if (!filename || Mods.empty()) {
		return false;
	}

	std::string pathLower = GetLowercasePath(filename);
	std::string path;
	VFSProvider* pack = nullptr;
	for (size_t m = 0; m < Mods.size(); m++) {
		if (Mods[m].Active && FindInMod(&Mods[m], filename, pathLower, path, pack)) {
			return true;
		}
	}

	return false;
}

bool ModLoader::LoadFile(const char* filename, Uint8** out, size_t* size) {
	if (!filename || Mods.empty()) {
		return false;
	}

	std::string pathLower = GetLowercasePath(filename);
	std::string path;
	VFSProvider* pack = nullptr;
	for (size_t m = 0; m < Mods.size(); m++) {
		if (Mods[m].Active && FindInMod(&Mods[m], filename, pathLower, path, pack)) {
			return ReadModFile(filename, path, pack, out, size);
		}
	}

	return false;
}

bool ModLoader::LoadModFile(ModInfo* mod, const char* filename, Uint8** out, size_t* size) {
	if (!mod || !filename) {
		return false;
	}

	std::string pathLower = GetLowercasePath(filename);
	std::string path;
	VFSProvider* pack = nullptr;
	if (FindInMod(mod, filename, pathLower, path, pack)) {
		return ReadModFile(filename, path, pack, out, size);
	}

	return false;
}

int ModLoader::GetModCount(bool active) {
	int count = 0;
	for (size_t m = 0; m < Mods.size(); m++) {
		if (!active || Mods[m].Active) {
			count++;
		}
	}
	return count;
}

void ModLoader::Dispose() {
	UnloadMods();
}