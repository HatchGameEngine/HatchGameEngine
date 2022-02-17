#if INTERFACE
#include <Engine/Includes/Standard.h>

class INI {
public:
    void* Struct;
    char* Filename;
};
#endif

#include <Engine/TextFormats/INI.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Utilities/StringUtils.h>
#include <Engine/Application.h>

#define INI_IMPLEMENTATION

#include <Libraries/ini.h>

PUBLIC STATIC INI* INI::New(const char* filename) {
    INI* ini = new INI;

    ini->Struct = (void*)ini_create(NULL);
    ini->Filename = StringUtils::Duplicate(filename);

    return ini;
}
PUBLIC STATIC INI* INI::Load(const char* filename) {
    INI* ini = new INI;

    SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
    if (!rw) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
        delete ini;
        return NULL;
    }

    Sint64 rwSize = SDL_RWsize(rw);
    if (rwSize < 0) {
        Log::Print(Log::LOG_ERROR, "Could not get size of file \"%s\": %s", filename, SDL_GetError());
        delete ini;
        return NULL;
    }

    char* data = (char*)malloc(rwSize + 1);
    if (!data)
    {
        Log::Print(Log::LOG_ERROR, "Out of memory saving '%s'!");
        delete ini;
        return NULL;
    }

    SDL_RWread(rw, data, rwSize, 1);
    data[rwSize] = 0;
    SDL_RWclose(rw);

    ini->Struct = (void*)ini_load(data, NULL);
    ini->Filename = StringUtils::Duplicate(filename);

    free(data);

    return ini;
}
PUBLIC bool INI::Save() {
    ini_t* iniData = (ini_t*)this->Struct;

    size_t size = ini_save(iniData, NULL, 0);
    char* data = (char*)malloc(size);
    if (!data) {
        Log::Print(Log::LOG_ERROR, "Out of memory saving '%s'!", Filename);
        return false;
    }

    // Get the data to write
    size = ini_save(iniData, data, size);

    SDL_RWops* rw = SDL_RWFromFile(Filename, "wb");
    if (!rw) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", Filename);
        free(data);
        return false;
    }

    if (size >= 0)
        size--;

    SDL_RWwrite(rw, data, size, 1);
    SDL_RWclose(rw);

    free(data);

    Log::Print(Log::LOG_VERBOSE, "Wrote %d bytes to file '%s'", size, Filename);

    return true;
}

PUBLIC bool INI::GetString(const char* section, const char* key, char* dest, size_t destSize) {
    if (!dest || !destSize)
        return false;

    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND)
        return false;

    char const* string = ini_property_value(iniData, secnum, property);
    StringUtils::Copy(dest, string, destSize);
    return true;
}
PUBLIC bool INI::GetInteger(const char* section, const char* key, int* dest) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND)
        return false;

    return StringUtils::ToNumber(dest, ini_property_value(iniData, secnum, property));
}
PUBLIC bool INI::GetBool(const char* section, const char* key, bool* dest) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND)
        return false;

    char const* string = ini_property_value(iniData, secnum, property);
    (*dest) = (!strcmp(string, "true") || !strcmp(string, "1"));
    return true;
}

PUBLIC bool INI::SetString(const char* section, const char* key, const char* value) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            secnum = ini_section_add(iniData, section, 0);
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND) {
        ini_property_add(iniData, secnum, key, 0, value, 0);
        return true;
    }

    ini_property_value_set(iniData, secnum, property, value);
    return true;
}
PUBLIC bool INI::SetInteger(const char* section, const char* key, int value) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            secnum = ini_section_add(iniData, section, 0);
    }

    char toStr[21];
    snprintf(toStr, sizeof toStr, "%d", value);

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND) {
        ini_property_add(iniData, secnum, key, 0, toStr, 0);
        return true;
    }

    ini_property_value_set(iniData, secnum, property, toStr);
    return true;
}
PUBLIC bool INI::SetBool(const char* section, const char* key, bool value) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            secnum = ini_section_add(iniData, section, 0);
    }

    const char* toStr = value ? "true" : "false";

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND) {
        ini_property_add(iniData, secnum, key, 0, toStr, 0);
        return true;
    }

    ini_property_value_set(iniData, secnum, property, toStr);
    return true;
}

PUBLIC bool INI::AddSection(const char* section) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = ini_find_section(iniData, section);
    if (secnum == INI_NOT_FOUND) {
        ini_section_add(iniData, section, 0);
        return true;
    }

    return false;
}
PUBLIC bool INI::RemoveSection(const char* section) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    ini_section_remove(iniData, secnum);
    return true;
}
PUBLIC bool INI::SectionExists(const char* section) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    if (section == NULL)
        return false;

    return (ini_find_section(iniData, section) != INI_NOT_FOUND);
}
PUBLIC int INI::GetSectionCount() {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return 0;
    return ini_section_count(iniData);
}

PUBLIC char const* INI::GetProperty(const char* section, const char* key) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return NULL;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return NULL;
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND)
        return NULL;

    return ini_property_value(iniData, secnum, property);
}
PUBLIC bool INI::PropertyExists(const char* section, const char* key) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    return (ini_find_property(iniData, secnum, key) != INI_NOT_FOUND);
}
PUBLIC bool INI::RemoveProperty(const char* section, const char* key) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return false;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return false;
    }

    int property = ini_find_property(iniData, secnum, key);
    if (property == INI_NOT_FOUND)
        return false;

    ini_property_remove(iniData, secnum, property);
    return true;
}
PUBLIC int INI::GetPropertyCount(const char* section) {
    ini_t* iniData = (ini_t*)this->Struct;
    if (!iniData)
        return 0;

    int secnum = INI_GLOBAL_SECTION;
    if (section) {
        secnum = ini_find_section(iniData, section);
        if (secnum == INI_NOT_FOUND)
            return 0;
    }

    return ini_property_count(iniData, secnum);
}

PUBLIC void INI::Dispose() {
    ini_t* iniData = (ini_t*)this->Struct;
    if (iniData)
        ini_destroy(iniData);

    free(this->Filename);
    delete this;
}
