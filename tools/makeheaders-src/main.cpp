#include <cstring>
#include <stdio.h>
#include <vector>
#include <string>
#include <time.h>
#include <unordered_map>
#include <tuple>
#include <utility>
#include <regex>
#include <sys/types.h>
#include <sys/stat.h>
#if !MSVC
    #include <dirent.h>
	#include <unistd.h>
    #include "crc32.h"
#else
    #include <stdlib.h>
    #include <stdafx.h>
    #include <dirent.h>
    #include <direct.h>
	#include <io.h>
    #include <crc32.h>
#endif

using namespace std;

char finalpath[4096];

static void MakeDir(const char *dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1)
        mkdir(dir, 0700);
}

static void RecursiveMakeDir(const char *path) {
    char tmp[4096];

    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);

    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            MakeDir(tmp);
            *p = '/';
        }
    }

    MakeDir(tmp);
}

struct VARIABLE {
    std::string class_type;
    std::string variable_name;
};

struct CLASSFILE {
    std::vector<std::string>    includes;
    std::string                 class_name;
    std::string                 parent_class_name;
    std::string                 file_name;
    std::vector<VARIABLE>       public_vars;
    std::vector<VARIABLE>       private_vars;
    std::vector<VARIABLE>       protected_vars;
    std::vector<std::string>    public_funcs;
    std::vector<std::string>    private_funcs;
    std::vector<std::string>    protected_funcs;
    vector<string>              classes_needed;
};

int class_count_alpha = 0;
bool any_class_changed = false;

int FindChar(char* str, char c) {
    char* strln = str;
    do {
        if (*strln == c)
            return strln - str;
        strln++;
    } while (*strln != 0);
    return -1;
}
int FindLastChar(char* str, char c) {
    char* strln = str + strlen(str);
    do {
        if (*strln == c)
            return strln - str;
        strln--;
    } while (strln != str);
    return -1;
}

string replaceFirstOccurrence(string& s, const string& toReplace, const string& replaceWith) {
    std::size_t pos = s.find(toReplace);
    if (pos == string::npos) return s;
    return s.replace(pos, toReplace.length(), replaceWith);
}

void print_class(const char* directory, CLASSFILE data) {
    string class_name(data.class_name);
    if (class_name == string("\x01\x01\x01\x01"))
        return;
    if (class_name == string(""))
        return;

    FILE *fp;
    char filename[4096];

    string direc = string(directory);
	string basedirec = string(finalpath);

	direc = replaceFirstOccurrence(direc, "source", "include");
	basedirec = replaceFirstOccurrence(basedirec, "source", "include");


    struct stat st = {0};

	if (direc.find("include") != std::string::npos) {
		if (stat(direc.substr(0, direc.find("include") + 7).c_str(), &st) == -1) {
			RecursiveMakeDir(direc.substr(0, direc.find("include") + 7).c_str());
		}
	}

    if (stat(direc.c_str(), &st) == -1)
        RecursiveMakeDir(direc.c_str());

    snprintf(filename, 4096, "%s%s.h", direc.c_str(), data.file_name.c_str());
    // printf("%s\n", filename);
    fp = fopen(filename, "wb");
    int err = errno;
    if (!fp) {
        printf("Could not open %s because %s\n", filename, strerror(err));
        return;
    }

    class_count_alpha++;

	class_name = (direc.c_str() + basedirec.length() + 1);
	class_name += data.file_name;
    std::transform(class_name.begin(), class_name.end(), class_name.begin(), [](unsigned char c) -> unsigned char { 
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
			return '_';
		return ::toupper(c); 
	});

    fprintf(fp, "#ifndef %s_H\n", class_name.c_str());
    fprintf(fp, "#define %s_H\n", class_name.c_str());
    fprintf(fp, "\n");
    fprintf(fp, "#define PUBLIC\n");
    fprintf(fp, "#define PRIVATE\n");
    fprintf(fp, "#define PROTECTED\n");
    fprintf(fp, "#define STATIC\n");
    fprintf(fp, "#define VIRTUAL\n");
    fprintf(fp, "#define EXPOSED\n");
    fprintf(fp, "\n");
    for (int i = 0; i < data.classes_needed.size(); i++) {
        fprintf(fp, "class %s;\n", data.classes_needed[i].c_str());
    }
    fprintf(fp, "\n");

    for (std::vector<std::string>::iterator it = data.includes.begin(); it != data.includes.end(); ++it) {
        fprintf(fp, "#include %s\n", (*it).c_str());
    }
    fprintf(fp, "\n");
    fprintf(fp, "class %s%s%s {\n", data.class_name.c_str(), data.parent_class_name != string("") ? " : public " : "", data.parent_class_name != string("") ? data.parent_class_name.c_str() : "");
    bool damada = 0;
    if (data.private_vars.size() > 0 || data.private_funcs.size() > 0) {
        fprintf(fp, "private:\n");
        bool yamero = 0;
        for (std::vector<VARIABLE>::iterator it =
            data.private_vars.begin(); it !=
            data.private_vars.end(); ++it) {
            fprintf(fp, "    %s %s\n", (*it).class_type.c_str(), (*it).variable_name.c_str());
            yamero = 1;
        }

        if (yamero)
            fprintf(fp, "\n");

        for (std::vector<string>::iterator it =
            data.private_funcs.begin(); it !=
            data.private_funcs.end(); ++it) {
            fprintf(fp, "    %s\n", (*it).c_str());
        }
        damada = 1;
    }
    if (data.public_vars.size() > 0 || data.public_funcs.size() > 0) {
        if (damada)
            fprintf(fp, "\n");

        fprintf(fp, "public:\n");
        bool yamero = 0;
        for (std::vector<VARIABLE>::iterator it =
            data.public_vars.begin(); it !=
            data.public_vars.end(); ++it) {
            fprintf(fp, "    %s %s\n", (*it).class_type.c_str(), (*it).variable_name.c_str());
            yamero = 1;
        }

        if (yamero)
            fprintf(fp, "\n");

        for (std::vector<string>::iterator it =
            data.public_funcs.begin(); it !=
            data.public_funcs.end(); ++it) {
            fprintf(fp, "    %s\n", (*it).c_str());
        }
    }
    if (data.protected_vars.size() > 0 || data.protected_funcs.size() > 0) {
        if (damada)
            fprintf(fp, "\n");

        fprintf(fp, "protected:\n");
        bool yamero = 0;
        for (std::vector<VARIABLE>::iterator it =
            data.protected_vars.begin(); it !=
            data.protected_vars.end(); ++it) {
            fprintf(fp, "    %s %s\n", (*it).class_type.c_str(), (*it).variable_name.c_str());
            yamero = 1;
        }

        if (yamero)
            fprintf(fp, "\n");

        for (std::vector<string>::iterator it =
            data.protected_funcs.begin(); it !=
            data.protected_funcs.end(); ++it) {
            fprintf(fp, "    %s\n", (*it).c_str());
        }
    }
    fprintf(fp, "};\n");

    fprintf(fp, "\n");
    fprintf(fp, "#endif /* %s_H */\n", class_name.c_str());

    fclose(fp);
}

vector<string> ClassNames;
string find_classname(char* filename) {
    FILE *fp;
    fp = fopen(filename, "rb");
    if (!fp) {
        printf("Could not load file (find_classname): %s!\n", filename);
        exit(EXIT_FAILURE);
    }

    bool in_interface = false;
    bool in_interfaced = false;
    int  in_variables = 0;

    // printf("find: %s\n", filename);

    while (true) {
        char first_token[255];
        if (fscanf(fp, "%s", first_token) <= 0) {
            break;
        }

        if (!strcmp(first_token, "#if")) {
            char rest_of_line[255];
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 255, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            if (!strncmp(rest_of_line, "INTERFACE", 9)) {
                in_interface = true;
                in_interfaced = true;
            }
        }
        else {
            if (!in_interfaced) {
                fclose(fp);
                return string("");
            }
        }
        if (!strcmp(first_token, "#endif")) {
            char rest_of_line[255];
            fgets(rest_of_line, 255, fp);
            if (in_interface) {
                in_interface = false;
                in_variables = 0;
            }
        }
        else if (!strcmp(first_token, "class")) {
            char rest_of_line[255];
            fscanf(fp, "%s", rest_of_line);
            fclose(fp);

            return string(rest_of_line);
        }
    }
    return string("");
}

CLASSFILE load_class(char* filename) {
    FILE *fp;
    CLASSFILE test;
    fp = fopen(filename, "rb");
    if (!fp) {
        printf("Could not load file (load_class): %s!\n", filename);
        exit(EXIT_FAILURE);
    }
    //printf("Loading file: %s\n", filename);

    bool in_interface = false;
    bool in_interfaced = false;
    int  in_variables = 0;

    char ObjectName[256];
    int found = FindLastChar(filename, '/');
    memset(ObjectName, 0, 256);
    strcpy(ObjectName, filename + found + 1);
    int nextfind = FindChar(ObjectName, '.');
    memset(ObjectName + nextfind, 0, 256 - nextfind);

    test.file_name = string(ObjectName);

    int brackscope = 0;

    while (true) {
        char first_token[255];
        if (fscanf(fp, "%s", first_token) <= 0)
            break;

        if (!strcmp(first_token, "#if")) {
            char rest_of_line[255];
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 255, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            if (!strncmp(rest_of_line, "INTERFACE", 9)) {
                in_interface = true;
                in_interfaced = true;
            }
        }
        else {
            if (!in_interfaced) {
                test.class_name = string("\x01\x01\x01\x01");
                break;
            }
        }
        if (!strcmp(first_token, "#endif")) {
            char rest_of_line[255];
            fgets(rest_of_line, 255, fp);
            if (in_interface) {
                in_interface = false;
                in_variables = 0;
            }
        }
        else if (!strcmp(first_token, "#include")) {
            char rest_of_line[255];
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 255, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            if (in_interface)
                test.includes.push_back(rest_of_line);
        }
        else if (!strcmp(first_token, "need_t")) {
            char rest_of_line[255];
            fscanf(fp, " ");
            fscanf(fp, "%[^;]", rest_of_line);

            test.classes_needed.push_back(string(rest_of_line));
        }
        else if (!strcmp(first_token, "class")) {
            if (test.class_name.empty()) {
                char rest_of_line[255];
                fscanf(fp, "%s", rest_of_line);

                test.class_name = std::string(rest_of_line);

                fgets(rest_of_line, 255, fp);
                rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

                char enws[255];
                if (sscanf(rest_of_line, " : public %s", enws) > 0) {
                    test.parent_class_name = std::string(enws);
                }

                in_variables = 1;
            }
        }
        else if (!strcmp(first_token, "{") || !strcmp(first_token, "}") || !strcmp(first_token, "};")) {
            if (!strcmp(first_token, "};")) {
                if (brackscope > 0) {
                    char rest_of_line[255];
                    fscanf(fp, "%c", rest_of_line); // clears space
                    fgets(rest_of_line, 255, fp);
                    rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

                    if (in_interface) {
                        if (in_variables == 1)
                            test.private_vars.push_back(VARIABLE { string(first_token), string() });
                        else if (in_variables == 2)
                            test.public_vars.push_back(VARIABLE { string(first_token), string() });
                        else if (in_variables == 3)
                            test.protected_vars.push_back(VARIABLE { string(first_token), string() });
                    }
                    brackscope--;
                }
                else {
                    in_variables = 0;
                    continue;
                }
            }
        }
        else if (!strcmp(first_token, "private:")) {
            in_variables = 1;
        }
        else if (!strcmp(first_token, "public:")) {
            in_variables = 2;
        }
        else if (!strcmp(first_token, "protected:")) {
            in_variables = 3;
        }
        else if (in_variables) {
            char nom[255];
            sscanf(first_token, "%[A-Za-z0-9_]", nom);

            if (!strcmp(nom, "struct") || !strcmp(nom, "enum"))
                brackscope++;

            char rest_of_line[255];
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 255, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            for (int i = 0; i < ClassNames.size(); i++) {
                if (string(nom) == ClassNames[i]) {
                    test.classes_needed.push_back(string(nom));
                    break;
                }
            }

            if (in_interface) {
                if (in_variables == 1)
                    test.private_vars.push_back(VARIABLE { string(first_token), string(rest_of_line) });
                else if (in_variables == 2)
                    test.public_vars.push_back(VARIABLE { string(first_token), string(rest_of_line) });
                else if (in_variables == 3)
                    test.protected_vars.push_back(VARIABLE { string(first_token), string(rest_of_line) });
            }
        }
        else if ((!strcmp(first_token, "PUBLIC") || !strcmp(first_token, "PRIVATE") || !strcmp(first_token, "PROTECTED")) && !in_interface) {
            char* rest_of_line = (char*)malloc(512);
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 512, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            bool statik = false;
            bool virtua = false;
            if (!strncmp(rest_of_line, "STATIC", 6)) {
                statik = true;
                char temp[512];
                sprintf(temp, "%s", rest_of_line + 7);
                strcpy(rest_of_line, temp);
            }
            if (!strncmp(rest_of_line, "VIRTUAL", 7)) {
                virtua = true;
                char temp[512];
                sprintf(temp, "%s", rest_of_line + 8);
                strcpy(rest_of_line, temp);
            }

            if (FindChar(rest_of_line, ':') < 0) {
                printf("\x1b[1;32mFatal Error: \x1b[0mCould not find class name for line '%s'.\n", rest_of_line);
                exit(1);
            }

            char* wherefunc = strstr(rest_of_line, (test.class_name + "::").c_str());
            if (!wherefunc) {
                printf("\x1b[1;32mFatal Error: \x1b[0mIncorrect class name `%.*s` (should be `%s`.)\n", FindChar(rest_of_line, ':'), rest_of_line, test.class_name.c_str());
                exit(1);
            }
            char* function_name = wherefunc + test.class_name.size() + 2;
            int ind = strlen(function_name) - 2;
            char* couldbe = strstr(function_name, ": ");
            if (couldbe != NULL) {
                ind = couldbe - function_name - 1;
            }
            if (function_name[ind] != ' ')
                ind++;
            function_name[ind] = ';';
            function_name[ind + 1] = 0;

            char type[500];
            if (wherefunc - rest_of_line > 0) {
                sprintf(type, "%s", rest_of_line);
                //printf("%s\n", type);
                type[wherefunc - rest_of_line] = 0;
            }
            else {
                sprintf(type, "%s", "");
            }

            char finalfunname[500];
            if (statik)
                sprintf(finalfunname, "%s%s%s", "static ", type, function_name);
            else if (virtua)
                sprintf(finalfunname, "%s%s%s", "virtual ", type, function_name);
            else
                sprintf(finalfunname, "%s%s", type, function_name);

            if (!strcmp(first_token, "PRIVATE"))
                test.private_funcs.push_back(string(finalfunname));
            else if (!strcmp(first_token, "PUBLIC"))
                test.public_funcs.push_back(string(finalfunname));
            else if (!strcmp(first_token, "PROTECTED"))
                test.protected_funcs.push_back(string(finalfunname));

            free(rest_of_line);
        }
        else if (!strcmp(first_token, "//") && !in_interface) {
            char rest_of_line[512];
            fscanf(fp, "%c", rest_of_line); // clears space
            fgets(rest_of_line, 512, fp);
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;
        }
    }

    fclose(fp);
    return test;
}

bool prelistdir(const char *name, int indent) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return false;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            char path[4096];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            prelistdir(path, indent + 2);
        }
        else if (indent == 0) {
            continue;
        }
        else if (strstr(entry->d_name, ".cpp")) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

            char newname[1024];
            snprintf(newname, sizeof(newname), "%s", name);
            size_t sz = strlen(name);
            if (newname[sz - 1] != '/') {
                newname[sz] = '/';
                newname[sz + 1] = 0;
            }

            string str = find_classname(path);
            if (str != "") {
                ClassNames.push_back(str);
            }
        }
    }
    closedir(dir);
    return true;
}

struct ClassHash {
    uint32_t FilenameHash;
    uint32_t InterfaceChecksum;
};
vector<ClassHash> ClassHashes;
int GetClassHash(uint32_t filenameHash) {
    for (int i = 0; i < ClassHashes.size(); i++) {
        if (ClassHashes[i].FilenameHash == filenameHash)
            return i;
    }
    return -1;
}
int PutClassHash(uint32_t filenameHash, uint32_t checkSum) {
    int idx = GetClassHash(filenameHash);
    if (idx >= 0) {
        ClassHashes[idx].FilenameHash = filenameHash;
        ClassHashes[idx].InterfaceChecksum = checkSum;
        return 0;
    }

    ClassHash ch;
    ch.FilenameHash = filenameHash;
    ch.InterfaceChecksum = checkSum;
    ClassHashes.push_back(ch);
    return 1;
}
void LoadClassHashTable() {
    #if MSVC
    FILE* f = fopen("../makeheaders.bin", "rb");
    #else
	FILE* f = fopen("makeheaders.bin", "rb");
    #endif
    if (f) {
        for (;;) {
            ClassHash ch;
            if (fread(&ch, sizeof(ClassHash), 1, f) <= 0)
                break;

            ClassHashes.push_back(ch);
        }
        fclose(f);
    }
}
void SaveClassHashTable() {
    #if MSVC
	FILE* f = fopen("../makeheaders.bin", "wb");
    #else
	FILE* f = fopen("makeheaders.bin", "wb");
    #endif
    if (f) {
        for (int i = 0; i < ClassHashes.size(); i++) {
            fwrite(&ClassHashes[i], sizeof(ClassHash), 1, f);
        }
        fclose(f);
    }
}

// Printing
enum class PrintColor {
    Default = 0,
    Red,
    Yellow,
    Green,
    Purple,
};
void PrintHeader(FILE* f, const char* str, PrintColor col) {
    int color = 0;
    #if MSVC
        switch (col) {
            case PrintColor::Red: color = 4; break;
            case PrintColor::Yellow: color = 14; break;
            case PrintColor::Green: color = 2; break;
            case PrintColor::Purple: color = 5; break;
            default: color = 0xF; break;
        }
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hStdOut, &csbi)) {
            SetConsoleTextAttribute(hStdOut, (csbi.wAttributes & 0xF0) | color);
        }
        fprintf(f, str);
        SetConsoleTextAttribute(hStdOut, csbi.wAttributes);
    #else
        switch (col) {
            case PrintColor::Red: color = 91; break;
            case PrintColor::Yellow: color = 93; break;
            case PrintColor::Green: color = 92; break;
            case PrintColor::Purple: color = 95; break;
            default: color = 37; break;
        }
        fprintf(f, "\x1b[1;%dm%s\x1b[0m", color, str);
    #endif
}

bool MakeHeaderCheck(char* filename) {
    if (FindChar(filename, '.') < 0) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Could not find '.' in '%s'!\n", filename);
        exit(EXIT_FAILURE);
        return false;
    }

    bool didChange = false;

    FILE* f = fopen(filename, "rb");
    if (!f) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Could not open file '%s'!\n", filename);
        exit(EXIT_FAILURE);
    }

    int interfaceStart = -1;
    int interfaceEnd = -1;

    int found, nextfind;
    char* HeaderName = (char*)calloc(1, 256);

    found = FindLastChar(filename, '/');
    nextfind = FindChar(filename + found + 1, '.');
    strncpy(HeaderName, filename + found + 1, nextfind);
    memset(HeaderName + nextfind, 0, 256 - nextfind);

    int ifScope = 0;
    while (true) {
        int64_t tokenStart = ftell(f);

        char first_token[255];
        if (fscanf(f, "%s", first_token) <= 0) {
            // printf("reached EOF\n");
            break;
        }

        if (!strcmp(first_token, "#if")) {
            fscanf(f, " ");
            char rest_of_line[255];
            fgets(rest_of_line, 255, f);
            rest_of_line[strcspn(rest_of_line, "\n")] = 0;
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0;

            if (!strncmp(rest_of_line, "INTERFACE", 9)) {
                interfaceStart = ftell(f);
            }

            ifScope++;
        }
        else if (!strcmp(first_token, "#endif")) {
            ifScope--;
            if (ifScope == 0) {
                interfaceEnd = tokenStart;
                break;
            }
        }
    }

    // get function descriptors
    // rewind(f);
    uint32_t FDChecksum = 0x00000000;
    while (true) {
        char rest_of_line[1024];
        if (!fgets(rest_of_line, 1024, f)) {
            break;
        }

        if ((!strncmp(rest_of_line, "PUBLIC", 6) || !strncmp(rest_of_line, "PRIVATE", 7) || !strncmp(rest_of_line, "PROTECTED", 9))) {
            rest_of_line[strcspn(rest_of_line, "\r\n")] = 0; rest_of_line[strcspn(rest_of_line, "\n")] = 0;

            FDChecksum = crc32_inc(rest_of_line, strlen(rest_of_line), FDChecksum);
        }
    }

    if (interfaceStart == -1) {
        return false;
    }
    if (interfaceStart != -1 && interfaceEnd == -1) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Missing matching #endif in file '%s'!\n", filename);
        exit(EXIT_FAILURE);
        return false;
    }

    char* interfaceField = (char*)calloc(1, interfaceEnd - interfaceStart + 1);
    fseek(f, interfaceStart, SEEK_SET);
    fread(interfaceField, interfaceEnd - interfaceStart, 1, f);

    uint32_t Namehash = crc32(filename, strlen(filename));

    uint32_t Checksum = 0x00000000;
    uint32_t ComparisonHash = 0x00000000;

    int idx = -1;
    if ((idx = GetClassHash(Namehash)) != -1)
        ComparisonHash = ClassHashes[idx].InterfaceChecksum;

    Checksum = crc32_inc(interfaceField, interfaceEnd - interfaceStart, FDChecksum);
	didChange = Checksum != ComparisonHash;

    if (!didChange) {
        SaveClassHashTable();
        return didChange;
    }

    PutClassHash(Namehash, Checksum);

    return didChange;
}
bool ListClassDir(const char* name, const char* parent, int indent) {
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(name)))
        return false;

    char path[4096];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        sprintf(path, "%s/%s", name, entry->d_name);

        if (entry->d_type == DT_DIR) {
            ListClassDir(path, parent, indent + 2);
        }
        else if (strstr(entry->d_name, ".cpp")) {
            if (MakeHeaderCheck(path)) {
                char parentFolder[4096];
                snprintf(parentFolder, sizeof(parentFolder), "%s", name);
                size_t sz = strlen(name);
                if (parentFolder[sz - 1] != '/') {
                    parentFolder[sz] = '/';
                    parentFolder[sz + 1] = 0;
                }

                // printf("path: %s    parentFolder: %s\n", path, parentFolder);

                print_class(parentFolder, load_class(path));
            }
        }
    }
    closedir(dir);
    return true;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        printf("No source code path!\n");
        printf("Usage:\n");
		printf("    %s [options] path\n", argv[0]);
        return 0;
    }

    printf("\n");
    // printf("Desired Directory: '%s' (%s)\n", argv[1], argv[1][0] == '/' ? "ABSOLUTE" : "RELATIVE");

    if (argv[1][0] == '/') {
        sprintf(finalpath, "%s%s", argv[1], "");
    }
    else {
        char cwd[4096];
        getcwd(cwd, sizeof(cwd));
        sprintf(finalpath, "%s/%s%s", cwd, argv[1], "");
    }
    #if MSVC
    _fullpath(finalpath, argv[1], 4096);
    #else
    realpath(argv[1], finalpath);
    #endif
    // printf("Final Directory: '%s'\n", finalpath);

    clock_t start, stop;
    start = clock();

    LoadClassHashTable();
    if (prelistdir(finalpath, 0)) {
        ListClassDir(finalpath, finalpath, 0);
    }
    SaveClassHashTable();

    stop = clock();
    PrintHeader(stdout, "makeheaders: ", PrintColor::Green);
    printf("Generated %d header(s) in %.3f milliseconds.\n\n", class_count_alpha, (stop - start) * 1000.f / CLOCKS_PER_SEC);

    return 0;
}
