#ifndef INISTRUCTS_H
#define INISTRUCTS_H

#include <vector>

#include <Engine/Diagnostics/Memory.h>
#include <Engine/Utilities/StringUtils.h>

struct INIProperty {
	char* Name;
	char* Value;

	INIProperty(const char* name, const char* value) {
		Name = StringUtils::Duplicate(name);
		Value = StringUtils::Duplicate(value);
	}
	~INIProperty() {
		Memory::Free(Name);
		Memory::Free(Value);
	}

	void SetValue(const char* value) {
		Memory::Free(Value);
		Value = StringUtils::Duplicate(value);
	}
};

struct INISection {
	char* Name = nullptr;
	vector<INIProperty*> Properties;

	INISection(const char* name) {
		if (name) {
			Name = StringUtils::Duplicate(name);
		}
	}
	~INISection() {
		Memory::Free(Name);

		for (size_t i = 0; i < Properties.size(); i++) {
			delete Properties[i];
		}
	}

	INIProperty* AddProperty(const char* name, const char* value) {
		INIProperty* prop = new INIProperty(name, value);
		Properties.push_back(prop);
		return prop;
	}
	INIProperty* FindProperty(const char* name) {
		for (INIProperty* property : Properties) {
			if (!strcmp(property->Name, name)) {
				return property;
			}
		}
		return nullptr;
	}
	bool RemoveProperty(const char* name) {
		for (size_t i = 0; i < Properties.size(); i++) {
			if (!strcmp(Properties[i]->Name, name)) {
				delete Properties[i];
				Properties.erase(Properties.begin() + i);
				return true;
			}
		}
		return false;
	}
};

#endif /* INISTRUCTS_H */
