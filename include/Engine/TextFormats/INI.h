#ifndef ENGINE_TEXTFORMATS_INI_H
#define ENGINE_TEXTFORMATS_INI_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>

class INI {
public:
    void* Struct;
    char* Filename;

    static INI* New(const char* filename);
    static INI* Load(const char* filename);
    bool Save();
    bool GetString(const char* section, const char* key, char* dest, size_t destSize);
    bool GetInteger(const char* section, const char* key, int* dest);
    bool GetBool(const char* section, const char* key, bool* dest);
    bool SetString(const char* section, const char* key, const char* value);
    bool SetInteger(const char* section, const char* key, int value);
    bool SetBool(const char* section, const char* key, bool value);
    bool AddSection(const char* section);
    bool RemoveSection(const char* section);
    bool SectionExists(const char* section);
    int GetSectionCount();
    char const* GetProperty(const char* section, const char* key);
    bool PropertyExists(const char* section, const char* key);
    bool RemoveProperty(const char* section, const char* key);
    int GetPropertyCount(const char* section);
    void Dispose();
};

#endif /* ENGINE_TEXTFORMATS_INI_H */
