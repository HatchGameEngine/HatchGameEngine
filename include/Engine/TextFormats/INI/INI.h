#ifndef ENGINE_TEXTFORMATS_INI_INI_H
#define ENGINE_TEXTFORMATS_INI_INI_H

#include <Engine/IO/Stream.h>
#include <Engine/Includes/Standard.h>
#include <Engine/TextFormats/INI/INIStructs.h>
#include <vector>

class INI {
public:
	char* Filename = nullptr;
	vector<INISection*> Sections;

	static INI* New(const char* filename);
	static INI* Load(const char* filename);
	bool Reload();
	bool Save(const char* filename);
	bool Save();
	void SetFilename(const char* filename);
	bool Read(Stream* stream);
	bool Write(Stream* stream);
	INISection* FindSection(const char* name);
	bool GetString(const char* sectionName, const char* key, char* dest, size_t destSize);
	bool GetInteger(const char* sectionName, const char* key, int* dest);
	bool GetDecimal(const char* sectionName, const char* key, double* dest);
	bool GetBool(const char* sectionName, const char* key, bool* dest);
	bool SetString(const char* sectionName, const char* key, const char* value);
	bool SetInteger(const char* section, const char* key, int value);
	bool SetDecimal(const char* section, const char* key, double value);
	bool SetBool(const char* section, const char* key, bool value);
	INISection* AddSection(const char* name);
	bool RemoveSection(const char* sectionName);
	bool SectionExists(const char* sectionName);
	int GetSectionCount();
	char* GetProperty(const char* sectionName, const char* key);
	bool PropertyExists(const char* sectionName, const char* key);
	bool RemoveProperty(const char* sectionName, const char* key);
	int GetPropertyCount(const char* sectionName);
	void Dispose();
};

#endif /* ENGINE_TEXTFORMATS_INI_INI_H */
