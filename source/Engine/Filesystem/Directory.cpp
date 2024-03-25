#if INTERFACE

#include <Engine/Includes/Standard.h>

class Directory {
private:
};
#endif

#include <Engine/Filesystem/Directory.h>
#include <Engine/Utilities/StringUtils.h>

#if WIN32
    #include <windows.h>
    #include <direct.h>
    #define MAX_PATH_SIZE 1024
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #define MAX_PATH_SIZE 4096
#endif

bool CompareFunction(char* i, char* j) {
    return strcmp(i, j) < 0;
}

PUBLIC STATIC bool          Directory::Exists(const char* path) {
    #if WIN32
        DWORD ftyp = GetFileAttributesA(path);
        if (ftyp == INVALID_FILE_ATTRIBUTES) return false;  // Something is wrong with your path
        if (ftyp & FILE_ATTRIBUTE_DIRECTORY) return true;
    #else
        DIR* dir = opendir(path);
        if (dir) {
            closedir(dir);
            return true;
        }
    #endif
    return false;
}
PUBLIC STATIC bool          Directory::Create(const char* path) {
    #if WIN32
        return CreateDirectoryA(path, NULL);
    #else
        return mkdir(path, 0777) == 0;
    #endif
}

PUBLIC STATIC bool          Directory::GetCurrentWorkingDirectory(char* out, size_t sz) {
    #if WIN32
        return _getcwd(out, sz) != NULL;
    #else
        return getcwd(out, sz) != NULL;
    #endif
}

PUBLIC STATIC void          Directory::GetFiles(vector<char*>* files, const char* path, const char* searchPattern, bool allDirs) {
    #if WIN32
        char winPath[MAX_PATH_SIZE];
        snprintf(winPath, MAX_PATH_SIZE, "%s%s*", path, path[strlen(path) - 1] == '/' ? "" : "/");

        for (char* i = winPath; *i; i++) {
            if (*i == '/') *i = '\\';
        }

        WIN32_FIND_DATA data;
        HANDLE hFind = FindFirstFile(winPath, &data);

        int i;
        char fullpath[MAX_PATH_SIZE];
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (data.cFileName[0] == '.' && data.cFileName[1] == 0) continue;
                if (data.cFileName[0] == '.' && data.cFileName[1] == '.' && data.cFileName[2] == 0) continue;

                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (allDirs) {
                        snprintf(fullpath, sizeof fullpath, "%s\\%s", path, data.cFileName);
                        Directory::GetFiles(files, fullpath, searchPattern, true);
                    }
                }
                else if (StringUtils::WildcardMatch(data.cFileName, searchPattern)) {
                    i = strlen(data.cFileName) + strlen(path) + 1;
                    char* str = (char*)calloc(1, i + 1);
                    snprintf(str, i + 1, "%s/%s", path, data.cFileName);
                    str[i] = 0;
                    for (char* istr = str; *istr; istr++) {
                        if (*istr == '\\') *istr = '/';
                    }
                    files->push_back(str);
                }
            }
            while (FindNextFile(hFind, &data));
            FindClose(hFind);
        }
    #else
        char fullpath[MAX_PATH_SIZE];
        DIR* dir = opendir(path);
        if (dir) {
            size_t i;
            struct dirent* d;

            while ((d = readdir(dir)) != NULL) {
                if (d->d_name[0] == '.' && !d->d_name[1]) continue;
                if (d->d_name[0] == '.' && d->d_name[1] == '.' && !d->d_name[2]) continue;

                if (d->d_type == DT_DIR) {
                    if (allDirs) {
                        snprintf(fullpath, sizeof fullpath, "%s/%s", path, d->d_name);
                        Directory::GetFiles(files, fullpath, searchPattern, true);
                    }
                }
                else if (StringUtils::WildcardMatch(d->d_name, searchPattern)) {
                    i = strlen(d->d_name) + strlen(path) + 1;
                    char* str = (char*)calloc(1, i + 1);
                    snprintf(str, i + 1, "%s/%s", path, d->d_name);
                    str[i] = 0;

                    files->push_back(str);
                }
            }
            closedir(dir);
        }

        std::sort(files->begin(), files->end(), CompareFunction);
    #endif
}
PUBLIC STATIC vector<char*> Directory::GetFiles(const char* path, const char* searchPattern, bool allDirs) {
    vector<char*> files;
    Directory::GetFiles(&files, path, searchPattern, allDirs);
    return files;
}

PUBLIC STATIC void          Directory::GetDirectories(vector<char*>* files, const char* path, const char* searchPattern, bool allDirs) {
    #if WIN32
        char winPath[MAX_PATH_SIZE];
        snprintf(winPath, MAX_PATH_SIZE, "%s%s*", path, path[strlen(path) - 1] == '/' ? "" : "/");

        for (char* i = winPath; *i; i++) {
            if (*i == '/') *i = '\\';
        }

        WIN32_FIND_DATA data;
        HANDLE hFind = FindFirstFile(winPath, &data);

        int i;
        char fullpath[MAX_PATH_SIZE];
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (data.cFileName[0] == '.' && !data.cFileName[1]) continue;
                if (data.cFileName[0] == '.' && data.cFileName[1] == '.' && !data.cFileName[2]) continue;

                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (StringUtils::WildcardMatch(data.cFileName, searchPattern)) {
                        i = strlen(data.cFileName) + strlen(path) + 1;
                        char* str = (char*)calloc(1, i + 1);
                        snprintf(str, i + 1, "%s/%s", path, data.cFileName);
                        str[i] = 0;
                        for (char* istr = str; *istr; istr++) {
                            if (*istr == '\\') *istr = '/';
                        }
                        files->push_back(str);
                    }
                    if (allDirs) {
                        snprintf(fullpath, sizeof fullpath, "%s\\%s", path, data.cFileName);
                        Directory::GetFiles(files, fullpath, searchPattern, true);
                    }
                }
            }
            while (FindNextFile(hFind, &data));
            FindClose(hFind);
        }
    #else
        char fullpath[MAX_PATH_SIZE];
        DIR* dir = opendir(path);
        if (dir) {
            int i;
            struct dirent* d;

            while ((d = readdir(dir)) != NULL) {
                if (d->d_name[0] == '.' && !d->d_name[1]) continue;
                if (d->d_name[0] == '.' && d->d_name[1] == '.' && !d->d_name[2]) continue;

                if (d->d_type == DT_DIR) {
                    if (StringUtils::WildcardMatch(d->d_name, searchPattern)) {
                        i = strlen(d->d_name) + strlen(path) + 1;
                        char* str = (char*)calloc(1, i + 1);
                        snprintf(str, i + 1, "%s/%s", path, d->d_name);
                        str[i] = 0;
                        files->push_back(str);
                    }
                    if (allDirs) {
                        snprintf(fullpath, sizeof fullpath, "%s/%s", path, d->d_name);
                        Directory::GetFiles(files, fullpath, searchPattern, true);
                    }
                }
            }
            closedir(dir);
        }
    #endif
}
PUBLIC STATIC vector<char*> Directory::GetDirectories(const char* path, const char* searchPattern, bool allDirs) {
    vector<char*> files;
    Directory::GetDirectories(&files, path, searchPattern, allDirs);
    return files;
}
