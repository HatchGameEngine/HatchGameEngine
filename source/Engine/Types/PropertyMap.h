#ifndef ENGINE_TYPES_PROPERTYMAP_H
#define ENGINE_TYPES_PROPERTYMAP_H

#include <Engine/Includes/HashMap.h>
#include <Engine/Includes/OrderedHashMap.h>
#include <Engine/Types/Property.h>

class PropertyMap {
private:
	OrderedHashMap<char*>* Keys;
	HashMap<Property>* Values;

public:
	PropertyMap();
	~PropertyMap();

	Property Get(const char* key);
	void Put(const char* key, Property data);
	bool Exists(const char* key);
	bool GetIfExists(const char* key, Property* result);
	bool Remove(const char* key);
	void ForAll(void (*forFunc)(char*, Property));
	void WithAll(std::function<void(char*, Property)> withFunc);
	void Clear();
};

#endif /* ENGINE_TYPES_PROPERTYMAP_H */
