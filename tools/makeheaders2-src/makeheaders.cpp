#include <vector>
#include <cstring>
#include <time.h>
#include <regex>
#include <sys/stat.h>
#ifndef _WIN32
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

static char finalpath[4096];

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
#ifdef _WIN32
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

#ifdef _WIN32
#include <errno.h>

static ssize_t getline(char** string, size_t* n, FILE* f) {
    if (string == NULL || f == NULL || n == NULL) {
        errno = EINVAL;
        return -1;
    }

    int c = getc(f);
    if (c == EOF)
        return -1;

    size_t pos = 0;
    size_t baseSize = 128;

    if (*string == NULL) {
        *string = malloc(baseSize);
        if (!*string)
            abort();
        *n = baseSize;
    }

    for (; c != EOF; c = getc(f)) {
        if (pos + 1 >= *n) {
            size_t size = max(*n + (*n >> 2), baseSize);
            *string = realloc(*string, size);
            if (!*string)
                abort();
            *n = size;
        }

        ((unsigned char *)(*string))[pos++] = c;
        if (c == '\n')
            break;
    }

    (*string)[pos] = '\0';
    return pos;
}
#endif

class Scanner {
private:
    char* start;
    char* current;

    FILE* file;
    char* buffer;
    size_t buffer_len;

    bool readLine() {
        ssize_t result = getline(&buffer, &buffer_len, file);
        if (result == -1) {
            isDone = true;
            return false;
        }

        start = current = buffer;
        line++;

        return true;
    }

    char peek() {
        return current[0];
    }

    char peekNext() {
        if (isAtEnd())
            return '\0';
        return current[1];
    }

    char advance() {
        current++;
        return current[-1];
    }

    bool isWhitespace(char c) {
        if (c == '\0')
            return false;
        return isspace(c);
    }

    void captureToken() {
        while (!isWhitespace(peek()))
            advance();
    }

    std::string getLineString() {
        std::string output(start, current - start);

        output.erase(output.find_last_not_of("\r\n")+1);

        return output;
    }

public:
    int line;
    bool isDone;

    Scanner(FILE *f) {
        start = current = nullptr;
        isDone = false;
        line = 0;

        buffer = nullptr;
        buffer_len = 0;

        file = f;
        readLine();
    }

    bool isAtEnd() {
        return current[0] == '\0';
    }

    bool peekToken(const char *token) {
        if (isAtEnd() && !readLine())
            return "";

        char *last = current;
        captureToken();

        int length = strlen(token);
        if (current - start == length && !memcmp(start, token, length)) {
            current = last;
            return true;
        }

        current = last;
        return false;
    }

    bool matchToken(const char *token) {
        if (isAtEnd() && !readLine())
            return "";

        char *last = current;
        captureToken();

        int length = strlen(token);
        if (current - start == length && !memcmp(start, token, length)) {
            skipWhitespace();
            start = current;
            return true;
        }

        current = last;
        return false;
    }

    std::string getToken() {
        if (isAtEnd() && !readLine())
            return "";

        captureToken();

        std::string output(start, current - start);

        skipWhitespace();
        start = current;

        return output;
    }

    std::string peekRestOfLine() {
        if (isAtEnd() && !readLine())
            return "";

        char *last = current;
        while (!isAtEnd())
            advance();

        std::string output = getLineString();
        current = last;
        return output;
    }

    std::string getRestOfLine() {
        if (isAtEnd())
            return "";
        while (!isAtEnd())
            advance();
        return getLineString();
    }

    int findCharPos(char match) {
        if (isAtEnd() && !readLine())
            return -1;

        char *last = current;
        while (peek() != match) {
            advance();
            if (isAtEnd()) {
                current = last;
                return -1;
            }
        }

        int pos = current - last;
        current = last;
        return pos;
    }

    std::string getLineUntilChar(char match) {
        if (isAtEnd() && !readLine())
            return "";

        while (peek() != match) {
            advance();
            if (isAtEnd())
                break;
        }

        std::string output(start, current - start);
        return output;
    }

    bool gotoNextLine() {
        return readLine();
    }

    bool skipWhitespace() {
        while (true) {
            switch (peek()) {
                case ' ':
                case '\r':
                case '\t':
                case '\n':
                    advance();
                    break;
                default:
                    return true;
            }
        }

        return true;
    }

    ~Scanner() {
        free(buffer);
    }
};

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
    int         block_depth;
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

int num_generated_headers = 0;

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

void WriteVariable(FILE* fp, VARIABLE var) {
    int num = var.block_depth;
    do {
        fprintf(fp, "    ");
    } while (num-- > 0);
    fprintf(fp, "%s %s\n", var.class_type.c_str(), var.variable_name.c_str());
}

void WriteClass(const char* directory, CLASSFILE data) {
    string class_name(data.class_name);
    if (class_name == string("\x01\x01\x01\x01"))
        return;
    if (class_name == string(""))
        return;

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

    std::string filename = direc + data.file_name + ".h";
    FILE *fp = fopen(filename.c_str(), "wb");
    int err = errno;
    if (!fp) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        printf("Could not open %s because %s\n", filename.c_str(), strerror(err));
        return;
    }

    num_generated_headers++;

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
    for (size_t i = 0; i < data.classes_needed.size(); i++) {
        fprintf(fp, "class %s;\n", data.classes_needed[i].c_str());
    }
    fprintf(fp, "\n");

    for (std::vector<std::string>::iterator it = data.includes.begin(); it != data.includes.end(); ++it) {
        fprintf(fp, "#include %s\n", (*it).c_str());
    }
    fprintf(fp, "\n");
    fprintf(fp, "class %s%s%s {\n", data.class_name.c_str(), data.parent_class_name != string("") ? " : public " : "", data.parent_class_name != string("") ? data.parent_class_name.c_str() : "");

    bool did_private = false;
    if (data.private_vars.size() > 0 || data.private_funcs.size() > 0) {
        fprintf(fp, "private:\n");
        bool wrote_any = false;
        for (std::vector<VARIABLE>::iterator it =
            data.private_vars.begin(); it !=
            data.private_vars.end(); ++it) {
            WriteVariable(fp, (*it));
            wrote_any = true;
        }

        if (wrote_any)
            fprintf(fp, "\n");

        for (std::vector<string>::iterator it =
            data.private_funcs.begin(); it !=
            data.private_funcs.end(); ++it) {
            fprintf(fp, "    %s\n", (*it).c_str());
        }
        did_private = true;
    }
    if (data.public_vars.size() > 0 || data.public_funcs.size() > 0) {
        if (did_private)
            fprintf(fp, "\n");

        fprintf(fp, "public:\n");
        bool wrote_any = false;
        for (std::vector<VARIABLE>::iterator it =
            data.public_vars.begin(); it !=
            data.public_vars.end(); ++it) {
            WriteVariable(fp, (*it));
            wrote_any = true;
        }

        if (wrote_any)
            fprintf(fp, "\n");

        for (std::vector<string>::iterator it =
            data.public_funcs.begin(); it !=
            data.public_funcs.end(); ++it) {
            fprintf(fp, "    %s\n", (*it).c_str());
        }
    }
    if (data.protected_vars.size() > 0 || data.protected_funcs.size() > 0) {
        if (did_private)
            fprintf(fp, "\n");

        fprintf(fp, "protected:\n");
        bool wrote_any = false;
        for (std::vector<VARIABLE>::iterator it =
            data.protected_vars.begin(); it !=
            data.protected_vars.end(); ++it) {
            WriteVariable(fp, (*it));
            wrote_any = true;
        }

        if (wrote_any)
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
string FindClassName(char* filename) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        fclose(fp);
        printf("Could not load file (FindClassName): %s!\n", filename);
        exit(EXIT_FAILURE);
    }

    bool found_interface = false;
    std::string token;

    Scanner scanner(fp);
    if (scanner.isDone) {
        fclose(fp);
        return string("");
    }

    token = scanner.getToken();

    while (!scanner.isDone) {
        if (token == "#if") {
            token = scanner.getToken();

            if (scanner.isDone) {
                fclose(fp);
                return string("");
            }
            else if (token == "INTERFACE")
                found_interface = true;
        }
        else if (!found_interface) {
            fclose(fp);
            return string("");
        }

        if (token == "class") {
            token = scanner.getToken();
            fclose(fp);
            return token;
        }

        token = scanner.getToken();
    }

    fclose(fp);
    return string("");
}

CLASSFILE ReadClass(char* filename) {
    FILE *fp;
    CLASSFILE test;
    fp = fopen(filename, "rb");
    if (!fp) {
        printf("Could not load file (ReadClass): %s!\n", filename);
        exit(EXIT_FAILURE);
    }

    bool in_interface = false;
    bool found_interface = false;
    int  in_variables = 0;

    std::string class_filename = std::string(filename);
    std::string class_name;

    std::size_t first = class_filename.find_last_of("/");
    if (first == std::string::npos)
        first = 0;
    else
        first++;

    std::string split = class_filename.substr(first);
    std::size_t last = split.find_last_of(".");
    if (last == std::string::npos)
        last = split.length();

    test.file_name = split.substr(0, last);

    int ifScope = 0;
    int brackscope = 0;
    int in_struct_or_enum = 0;

    Scanner scanner(fp);
    if (scanner.isDone) {
        fclose(fp);
        return test;
    }

    std::string token = scanner.getToken();

#define throw_err_start() { \
    char fmt[4096 + 32]; \
    snprintf(fmt, sizeof(fmt), "error in '%s', line %d: ", filename, scanner.line); \
    PrintHeader(stderr, fmt, PrintColor::Red); \
}
#define throw_err_end() exit(EXIT_FAILURE)
#define throw_err(msg)  \
    throw_err_start(); \
    fprintf(stderr, msg " in class '%s'!\n", test.file_name.c_str()); \
    throw_err_end();

    while (!scanner.isDone) {
        std::string first_token = token;
        if (token == "#if") {
            token = scanner.getToken();
            if (scanner.isDone) {
                throw_err("Unexpected end of file");
            }

            if (token == "INTERFACE") {
                in_interface = true;
                found_interface = true;
            }

            ifScope++;
        }
        else if (!found_interface) {
            test.class_name = string("\x01\x01\x01\x01");
            break;
        }
        if (token == "#endif") {
            if (!scanner.gotoNextLine())
                break;
            if (in_interface) {
                in_interface = false;
                in_variables = 0;
            }
            if (ifScope > 0) {
                ifScope--;
                if (ifScope < 0 || brackscope != 0) {
                    throw_err("Malformed source code");
                }
            }
        }
        else if (token == "#include") {
            std::string rest_of_line = scanner.getRestOfLine();
            if (rest_of_line == "") {
                throw_err("Malformed source code");
            }
            if (in_interface)
                test.includes.push_back(rest_of_line);
        }
        else if (token == "need_t") {
            if (scanner.findCharPos(';') == -1) {
                throw_err("Missing ';' after class name for 'need_t'");
            }

            std::string rest_of_line = scanner.getLineUntilChar(';');
            if (scanner.isDone) {
                throw_err("Unexpected end of file");
            }
            else if (rest_of_line == "") {
                throw_err("Missing class name for 'need_t'");
            }

            test.classes_needed.push_back(rest_of_line);
        }
        else if (token == "class") {
            if (test.class_name.empty()) {
                test.class_name = scanner.getToken();
                if (scanner.isDone) {
                    throw_err("Unexpected end of file after class name");
                }

                if (scanner.matchToken(":") && scanner.matchToken("public")) {
                    token = scanner.getToken();
                    if (scanner.isDone) {
                        throw_err("Unexpected end of file after 'public'");
                    }
                    test.parent_class_name = token;
                }
                if (scanner.isDone) {
                    throw_err("Unexpected end of file");
                }

                in_variables = 1;
            }
        }
        else if (token == "{" || token == "}" || token == "};") {
            if (token == "{") {
                if (in_interface)
                    brackscope++;
            }
            else if (token == "}" || token == "};") {
                if (in_interface) {
                    brackscope--;
                    if (brackscope < 0) {
                        throw_err("Malformed source code");
                    }
                }

                if (in_struct_or_enum > 0) {
                    std::string var_name = "";
                    if (token == "}")
                        var_name = scanner.getToken();
                    VARIABLE var = VARIABLE { token, var_name, 0 };
                    if (in_variables == 1)
                        test.private_vars.push_back(var);
                    else if (in_variables == 2)
                        test.public_vars.push_back(var);
                    else if (in_variables == 3)
                        test.protected_vars.push_back(var);
                    in_struct_or_enum--;
                }
            }
        }
        else if (token.rfind("private", 0) == 0) {
            if (token.find(":") == std::string::npos && !scanner.matchToken(":")) {
                throw_err("Unexpected token after 'private'");
            }
            if (scanner.isDone) {
                throw_err("Unexpected end of file after 'private:'");
            }
            in_variables = 1;
        }
        else if (token.rfind("public", 0) == 0) {
            if (token.find(":") == std::string::npos && !scanner.matchToken(":")) {
                throw_err("Unexpected token after 'public'");
            }
            if (scanner.isDone) {
                throw_err("Unexpected end of file after 'public:'");
            }
            in_variables = 2;
        }
        else if (token.rfind("protected", 0) == 0) {
            if (token.find(":") == std::string::npos && !scanner.matchToken(":")) {
                throw_err("Unexpected token after 'protected'");
            }
            if (scanner.isDone) {
                throw_err("Unexpected end of file after 'protected:'");
            }
            in_variables = 3;
        }
        else if (in_variables) {
            if (token == "") {
                token = scanner.getToken();
                continue;
            }

            bool token_is_struct = false;
            if (token == "struct" || token == "enum") {
                in_struct_or_enum++;
                token_is_struct = true;
            }
            else for (size_t i = 0; i < ClassNames.size(); i++) {
                if (token == ClassNames[i]) {
                    test.classes_needed.push_back(token);
                    break;
                }
            }

            std::string rest_of_line;
            if (token_is_struct) {
                if (!scanner.peekToken("{"))
                    rest_of_line = scanner.getToken() + " {";
                else
                    rest_of_line = "{";
                if (scanner.isDone) {
                    throw_err("Unexpected end of file after 'struct' or 'enum'");
                }
            } else {
                rest_of_line = scanner.getRestOfLine();
                if (scanner.isDone) {
                    throw_err("Unexpected end of file");
                }
            }

            if (in_interface) {
                VARIABLE var = VARIABLE { token, rest_of_line, token_is_struct ? 0 : in_struct_or_enum };
                if (in_variables == 1)
                    test.private_vars.push_back(var);
                else if (in_variables == 2)
                    test.public_vars.push_back(var);
                else if (in_variables == 3)
                    test.protected_vars.push_back(var);
            }
        }
        else if ((token == "PUBLIC" || token == "PRIVATE" || token == "PROTECTED") && !in_interface) {
            bool statik = false;
            bool virtua = false;
            if (scanner.peekToken("STATIC")) {
                statik = true;
                scanner.getToken();
                if (scanner.isDone) {
                    throw_err("Unexpected end of file");
                }
            }
            else if (scanner.peekToken("VIRTUAL")) {
                virtua = true;
                scanner.getToken();
                if (scanner.isDone) {
                    throw_err("Unexpected end of file");
                }
            }

            token = scanner.getToken();

            std::string type = "";
            do {
                if (token.find("{") != std::string::npos) {
                    throw_err("Malformed source code");
                }
                else if (token.find(":") != std::string::npos)
                    break;

                type += token + " ";

                token = scanner.getToken();
                if (scanner.isDone) {
                    throw_err("Unexpected end of file");
                }
            } while (true);

            std::string rest_of_line = token + " " + scanner.peekRestOfLine();
            size_t where = rest_of_line.find("::");
            if (where == std::string::npos) {
                throw_err("Could not find class name");
            }

            std::string this_class_name = rest_of_line.substr(0, where);
            if (test.class_name != this_class_name) {
                throw_err_start();
                fprintf(stderr, "Incorrect class name '%s' (should be '%s'!)\n", this_class_name.c_str(), test.file_name.c_str());
                throw_err_end();
            }

            std::string function_name = rest_of_line.substr(where + 2);
            function_name.erase(function_name.find_last_not_of(" {")+1);
            function_name = type + function_name + ";";
            if (statik)
                function_name = "static " + function_name;
            else if (virtua)
                function_name = "virtual " + function_name;

            if (first_token == "PRIVATE")
                test.private_funcs.push_back(string(function_name));
            else if (first_token == "PUBLIC")
                test.public_funcs.push_back(string(function_name));
            else if (first_token == "PROTECTED")
                test.protected_funcs.push_back(string(function_name));

            scanner.gotoNextLine();
        }

        token = scanner.getToken();
    }

#undef throw_err

    fclose(fp);
    return test;
}

bool CheckExtension(const char* filename, const char* extension) {
    char *dot = strrchr((char*)filename, '.');
    return dot && !strcmp(dot, extension);
}

bool FindClasses(const char* name, int depth) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name)))
        return false;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;

            char path[4096 + 256];
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);
            FindClasses(path, depth + 1);
        }
        else if (depth == 0)
            continue;
        else if (CheckExtension(entry->d_name, ".cpp")) {
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

            string class_name = FindClassName(path);
            if (class_name != "")
                ClassNames.push_back(class_name);
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
    for (size_t i = 0; i < ClassHashes.size(); i++) {
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

std::string classpath;

void LoadClassHashTable() {
    FILE* f = fopen(classpath.c_str(), "rb");
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
    FILE* f = fopen(classpath.c_str(), "wb");
    if (f) {
        for (size_t i = 0; i < ClassHashes.size(); i++) {
            fwrite(&ClassHashes[i], sizeof(ClassHash), 1, f);
        }
        fclose(f);
    }
}

bool MakeHeaderCheck(char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Could not open file '%s'!\n", filename);
        exit(EXIT_FAILURE);
    }

    Scanner scanner(f);
    if (scanner.isDone) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Could not find class name in file '%s'!\n", filename);
        fclose(f);
        return false;
    }

    int interfaceStart = -1;
    int interfaceEnd = -1;
    int ifScope = 0;

    std::string token = scanner.getToken();
    while (!scanner.isDone) {
        int64_t tokenStart = ftell(f);

        if (token == "#if") {
            ifScope++;
            token = scanner.getToken();

            if (scanner.isDone) {
                PrintHeader(stderr, "error: ", PrintColor::Red);
                fprintf(stderr, "Unexpected end of file in file '%s'!\n", filename);
                fclose(f);
                return false;
            }
            else if (token == "INTERFACE")
                interfaceStart = ftell(f);
        }
        else if (token == "#endif") {
            if (--ifScope == 0) {
                interfaceEnd = tokenStart;
                break;
            }
        }

        token = scanner.getToken();
    }

    if (interfaceStart == -1)
        return false;
    else if (interfaceEnd == -1) {
        PrintHeader(stderr, "error: ", PrintColor::Red);
        fprintf(stderr, "Missing matching #endif in file '%s'!\n", filename);
        exit(EXIT_FAILURE);
        return false;
    }

    // get function descriptors
    uint32_t FDChecksum = 0x00000000;
    while (!scanner.isDone) {
        if (!scanner.gotoNextLine())
            break;

        std::string token = scanner.getToken();
        if (token == "PUBLIC" || token == "PRIVATE" || token == "PROTECTED") {
            std::string rest_of_line = scanner.getRestOfLine();
            std::size_t found = rest_of_line.find_last_of("{");
            if (found != std::string::npos)
                rest_of_line.erase(found, 1);
            const char* line = rest_of_line.c_str();
            FDChecksum = crc32_inc((char*)line, strlen(line), FDChecksum);
        }
    }

    char* interfaceField = (char*)calloc(1, interfaceEnd - interfaceStart + 1);
    fseek(f, interfaceStart, SEEK_SET);
    fread(interfaceField, interfaceEnd - interfaceStart, 1, f);

    uint32_t NameHash = crc32(filename, strlen(filename));
    uint32_t ComparisonHash = 0x00000000;

    int idx = GetClassHash(NameHash);
    if (idx != -1)
        ComparisonHash = ClassHashes[idx].InterfaceChecksum;

    uint32_t Checksum = crc32_inc(interfaceField, interfaceEnd - interfaceStart, FDChecksum);
    if (Checksum == ComparisonHash) {
        SaveClassHashTable();
        return false;
    }

    PutClassHash(NameHash, Checksum);

    return true;
}
bool ListClassDir(const char* name, const char* parent) {
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(name)))
        return false;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", name, entry->d_name);

        if (entry->d_type == DT_DIR)
            ListClassDir(path, parent);
        else if (CheckExtension(entry->d_name, ".cpp") && MakeHeaderCheck(path)) {
            char parentFolder[4096];
            snprintf(parentFolder, sizeof(parentFolder), "%s", name);
            size_t sz = strlen(name);
            if (parentFolder[sz - 1] != '/') {
                parentFolder[sz] = '/';
                parentFolder[sz + 1] = 0;
            }

            WriteClass(parentFolder, ReadClass(path));
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

#ifdef _WIN32
    _fullpath(finalpath, argv[1], sizeof(finalpath));
#else
    realpath(argv[1], finalpath);
#endif

    std::string direc = std::string(finalpath);
    classpath = replaceFirstOccurrence(direc, "source", "include");

    struct stat st = {0};
    if (stat(direc.c_str(), &st) == -1)
        RecursiveMakeDir(direc.c_str());

    classpath += "/makeheaders.bin";

    clock_t start, stop;
    start = clock();

    LoadClassHashTable();
    if (FindClasses(finalpath, 0))
        ListClassDir(finalpath, finalpath);
    SaveClassHashTable();

    stop = clock();
    PrintHeader(stdout, "makeheaders: ", PrintColor::Green);
    printf("Generated %d header(s) in %.3f milliseconds.\n\n", num_generated_headers, (stop - start) * 1000.f / CLOCKS_PER_SEC);

    return 0;
}
