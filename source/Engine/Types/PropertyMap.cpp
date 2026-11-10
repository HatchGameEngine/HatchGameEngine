#include <Engine/Diagnostics/Memory.h>
#include <Engine/Types/PropertyMap.h>
#include <Engine/Utilities/StringUtils.h>

PropertyMap::PropertyMap() {
	Keys = new OrderedHashMap<char*>(nullptr, 4);
	Values = new HashMap<Property>(nullptr, 4);
}

PropertyMap::~PropertyMap() {
	Clear();

	delete Keys;
	delete Values;
}

Property PropertyMap::Get(const char* key) {
	return Values->Get(key);
}

void PropertyMap::Put(const char* key, Property data) {
	if (!Keys->Exists(key)) {
		Keys->Put(key, StringUtils::Duplicate(key));
	}

	Values->Put(key, data);
}

bool PropertyMap::GetIfExists(const char* key, Property* result) {
	if (Exists(key)) {
		*result = Get(key);
		return true;
	}
	return false;
}

bool PropertyMap::Exists(const char* key) {
	return Values->Exists(key);
}

bool PropertyMap::Remove(const char* key) {
	if (!Exists(key)) {
		return false;
	}

	Uint32 hash = Values->HashFunction(key, strlen(key));

	Keys->Remove(hash);
	Values->Remove(hash);

	return true;
}

void PropertyMap::ForAll(void (*forFunc)(char*, Property)) {
	for (auto it = Keys->Data.begin(); it != Keys->Data.end(); it++) {
		char* key = it->second;
		forFunc(key, Get(key));
	}
}

void PropertyMap::WithAll(std::function<void(char*, Property)> withFunc) {
	for (auto it = Keys->Data.begin(); it != Keys->Data.end(); it++) {
		char* key = it->second;
		withFunc(key, Get(key));
	}
}

void PropertyMap::Clear() {
	Keys->ForAll([](Uint32, char* key) -> void {
		Memory::Free(key);
	});
	Values->ForAll([](Uint32, Property property) -> void {
		Property::Delete(property);
	});

	Keys->Clear();
	Values->Clear();
}
