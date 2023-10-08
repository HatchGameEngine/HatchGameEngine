#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/TextFormats/INI/INIStructs.h>
#include <Engine/IO/Stream.h>

#include <vector>

class INI {
public:
    char*               Filename = nullptr;
    vector<INISection*> Sections;
};
#endif

#include <Engine/TextFormats/INI/INI.h>
#include <Engine/Diagnostics/Log.h>
#include <Engine/Utilities/StringUtils.h>
#include <Engine/IO/SDLStream.h>
#include <Engine/Application.h>

#define INI_IMPLEMENTATION

PUBLIC STATIC INI* INI::New(const char* filename) {
    INI* ini = new INI;

    ini->SetFilename(filename);
    ini->AddSection(nullptr);

    return ini;
}
PUBLIC STATIC INI* INI::Load(const char* filename) {
    INI* ini = INI::New(filename);

    SDLStream* stream = SDLStream::New(filename, SDLStream::READ_ACCESS);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
        delete ini;
        return nullptr;
    }

    if (!ini->Read(stream)) {
        Log::Print(Log::LOG_ERROR, "Error reading '%s'!");
        stream->Close();
        delete ini;
        return nullptr;
    }

    stream->Close();

    return ini;
}
PUBLIC bool INI::Reload() {
    SDLStream* stream = SDLStream::New(Filename, SDLStream::READ_ACCESS);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", Filename);
        return false;
    }

    if (!Read(stream)) {
        Log::Print(Log::LOG_ERROR, "Error reading '%s'!");
        stream->Close();
        return false;
    }

    stream->Close();

    return true;
}
PUBLIC bool INI::Save(const char* filename) {
    SDLStream* stream = SDLStream::New(filename, SDLStream::WRITE_ACCESS);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
        return false;
    }

    if (!Write(stream)) {
        Log::Print(Log::LOG_ERROR, "Couldn't write to file '%s'!", filename);
        stream->Close();
        return false;
    }

    stream->Close();
    return true;
}
PUBLIC bool INI::Save() {
    return Save(Filename);
}
PUBLIC void INI::SetFilename(const char* filename) {
    Memory::Free(Filename);
    Filename = StringUtils::Duplicate(filename);
}

PUBLIC bool INI::Read(Stream* stream) {
    INISection* section = nullptr;

    size_t length = stream->Length();
    void* data = Memory::Malloc(length + 1);
    stream->ReadBytes(data, length);

    char* ptr = (char*)data;
    ptr[length] = '\0';

    for (size_t i = 0; i < Sections.size(); i++)
        delete Sections[i];
    Sections.clear();

    while (*ptr) {
        // trim leading whitespace
        while (*ptr && *ptr <= ' ')
            ptr++;

        // done?
        if (!*ptr)
            break;
        // comment
        else if (*ptr == ';') {
            while (*ptr && *ptr != '\n')
                ptr++;
        }
        // section
        else if (*ptr == '[') {
            ptr++;
            char* start = ptr;
            while (*ptr && *ptr != ']' && *ptr != '\n')
                ptr++;

            if (*ptr == ']') {
                std::string name(start, (int)(ptr - start));
                section = AddSection(name.c_str());
                ptr++;
            }
        }
        // property
        else {
            char* keyPtr = ptr;
            int l = -1;
            while (*ptr) {
                if (*ptr == '=' || *ptr == ' ') {
                    l = (int)(ptr - keyPtr);
                    break;
                } else if (*ptr == '\n')
                    break;
                ptr++;
            }

            if (l < 1) {
                while (*ptr != '\n')
                    ptr++;
                ++ptr;
                continue;
            }

            while (*ptr == ' ')
                ptr++;

            if (*ptr == '=') {
                ptr++;
                while (*ptr && *ptr <= ' ' && *ptr != '\n')
                    ptr++;

                char* valuePtr = ptr;
                while (*ptr && *ptr != '\n')
                    ptr++;
                while (*(--ptr) <= ' ')
                    ;
                ptr++;

                std::string key(keyPtr, l);
                std::string value(valuePtr, (int)(ptr - valuePtr));

                if (section == nullptr)
                    Sections[0]->AddProperty(key.c_str(), value.c_str());
                else
                    section->AddProperty(key.c_str(), value.c_str());
            }
        }
    }

    Memory::Free(data);

    return true;
}
PUBLIC bool INI::Write(Stream* stream) {
    for (INISection* section: Sections) {
        if (!section->Name && section->Properties.size() == 0)
            continue;

        if (section->Name) {
            stream->WriteByte('[');
            stream->WriteBytes(section->Name, strlen(section->Name));
            stream->WriteByte(']');
            stream->WriteByte('\n');
        }

        for (INIProperty* property: section->Properties) {
            stream->WriteBytes(property->Name, strlen(property->Name));
            stream->WriteByte(' ');
            stream->WriteByte('=');
            stream->WriteByte(' ');
            stream->WriteBytes(property->Value, strlen(property->Value));
            stream->WriteByte('\n');
        }

        if (section != Sections.back())
            stream->WriteByte('\n');
    }

    return true;
}

PUBLIC INISection* INI::FindSection(const char* name) {
    if (name == nullptr)
        return Sections[0];

    for (INISection* section: Sections) {
        if (section->Name && !strcmp(section->Name, name))
            return section;
    }

    return nullptr;
}

PUBLIC bool INI::GetString(const char* sectionName, const char* key, char* dest, size_t destSize) {
    if (!dest || !destSize)
        return false;

    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr)
        return false;

    StringUtils::Copy(dest, property->Value, destSize);
    return true;
}
PUBLIC bool INI::GetInteger(const char* sectionName, const char* key, int* dest) {
    if (!dest)
        return false;

    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr)
        return false;

    return StringUtils::ToNumber(dest, property->Value);
}
PUBLIC bool INI::GetDecimal(const char* sectionName, const char* key, double* dest) {
    if (!dest)
        return false;

    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr)
        return false;

    return StringUtils::ToDecimal(dest, property->Value);
}
PUBLIC bool INI::GetBool(const char* sectionName, const char* key, bool* dest) {
    if (!dest)
        return false;

    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr)
        return false;

    (*dest) = !strcmp(property->Value, "true") || !strcmp(property->Value, "1");
    return true;
}

PUBLIC bool INI::SetString(const char* sectionName, const char* key, const char* value) {
    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        section = AddSection(sectionName);

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr) {
        property = section->AddProperty(key, value);
        return true;
    }

    property->SetValue(value);
    return true;
}
PUBLIC bool INI::SetInteger(const char* section, const char* key, int value) {
    char toStr[21];
    snprintf(toStr, sizeof toStr, "%d", value);
    return SetString(section, key, toStr);
}
PUBLIC bool INI::SetDecimal(const char* section, const char* key, double value) {
    char toStr[512];
    snprintf(toStr, sizeof toStr, "%lf", value);
    return SetString(section, key, toStr);
}
PUBLIC bool INI::SetBool(const char* section, const char* key, bool value) {
    const char* toStr = value ? "true" : "false";
    return SetString(section, key, toStr);
}

PUBLIC INISection* INI::AddSection(const char* name) {
    INISection* sec = new INISection(name);
    Sections.push_back(sec);
    return sec;
}
PUBLIC bool INI::RemoveSection(const char* sectionName) {
    if (sectionName == nullptr) {
        Sections[0]->Properties.clear();
        return true;
    }
    else {
        for (size_t i = 1; i < Sections.size(); i++) {
            if (!strcmp(Sections[i]->Name, sectionName)) {
                delete Sections[i];
                Sections.erase(Sections.begin() + i);
                return true;
            }
        }
    }

    return false;
}
PUBLIC bool INI::SectionExists(const char* sectionName) {
    return FindSection(sectionName) != nullptr;
}
PUBLIC int INI::GetSectionCount() {
    return Sections.size() - 1;
}

PUBLIC char* INI::GetProperty(const char* sectionName, const char* key) {
    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return nullptr;

    INIProperty* property = section->FindProperty(key);
    if (property == nullptr)
        return nullptr;

    return property->Value;
}
PUBLIC bool INI::PropertyExists(const char* sectionName, const char* key) {
    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    return section->FindProperty(key) != nullptr;
}
PUBLIC bool INI::RemoveProperty(const char* sectionName, const char* key) {
    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return false;

    return section->RemoveProperty(key);
}
PUBLIC int INI::GetPropertyCount(const char* sectionName) {
    INISection* section = FindSection(sectionName);
    if (section == nullptr)
        return 0;

    return section->Properties.size();
}

PUBLIC void INI::Dispose() {
    Memory::Free(Filename);

    for (size_t i = 0; i < Sections.size(); i++)
        delete Sections[i];

    delete this;
}
