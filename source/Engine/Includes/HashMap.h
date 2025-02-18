#ifndef HASHMAP_H
#define HASHMAP_H

#include <Engine/Includes/Standard.h>
#include <Engine/Hashing/Murmur.h>
#include <Engine/Diagnostics/Memory.h>
#include <functional>

template <typename T> class HashMap {
public:
    std::unordered_map<Uint32, T> Data;
    std::vector<Uint32> Keys;

    Uint32 (*HashFunction)(const void*, size_t) = NULL;

    HashMap<T>(Uint32 (*hashFunc)(const void*, size_t) = NULL, int capacity = 16) {
        HashFunction = hashFunc;
        if (HashFunction == NULL)
            HashFunction = Murmur::EncryptData;
    }

    size_t Count() {
        return Keys.size();
    }

    void   Put(Uint32 hash, T data) {
        if (!Exists(hash))
            Keys.push_back(hash);
        Data[hash] = data;
    }
    void   Put(const char* key, T data) {
        Uint32 hash = HashFunction(key, strlen(key));
        Put(hash, data);
    }

    T      Get(Uint32 hash) {
        if (Data.find(hash) != Data.end()) {
            return Data[hash];
        }

#ifdef IOS
        return T();
#else
        return T { 0 };
#endif
    }
    T      Get(const char* key) {
        Uint32 hash = HashFunction(key, strlen(key));
        return Get(hash);
    }
    bool   Exists(Uint32 hash) {
        return std::find(Keys.begin(), Keys.end(), hash) != Keys.end();
    }
    bool   Exists(const char* key) {
        Uint32 hash = HashFunction(key, strlen(key));
        return Exists(hash);
    }

    bool   GetIfExists(Uint32 hash, T* result) {
        if (Data.find(hash) != Data.end()) {
            *result = Data[hash];
            return true;
        }

        return false;
    }
    bool   GetIfExists(const char* key, T* result) {
        Uint32 hash = HashFunction(key, strlen(key));
        return GetIfExists(hash, result);
    }

    bool   Remove(Uint32 hash) {
        auto it = std::find(Keys.begin(), Keys.end(), hash);
        if (it != Keys.end()) {
            Data.erase(hash);
            Keys.erase(it);
            return true;
        }
        return false;
    }
    bool   Remove(const char* key) {
        Uint32 hash = HashFunction(key, strlen(key));
        return Remove(hash);
    }

    void   Clear() {
        Data.clear();
        Keys.clear();
    }

    void   ForAll(void (*forFunc)(Uint32, T)) {
        ForAllOrdered(forFunc);
    }
    void   WithAll(std::function<void(Uint32, T)> withFunc) {
        WithAllOrdered(withFunc);
    }
    void   ForAllOrdered(void (*forFunc)(Uint32, T)) {
        for (size_t i = 0; i < Keys.size(); i++) {
            Uint32 key = Keys[i];
            forFunc(key, Data[key]);
        }
    }
    void   WithAllOrdered(std::function<void(Uint32, T)> withFunc) {
        for (size_t i = 0; i < Keys.size(); i++) {
            Uint32 key = Keys[i];
            withFunc(key, Data[key]);
        }
    }
    Uint32 GetFirstKey() {
        if (Keys.size() > 0)
            return Keys[0];
        return 0;
    }
    Uint32 GetNextKey(Uint32 key) {
        auto it = std::find(Keys.begin(), Keys.end(), key);
        if (it != Keys.end() && it + 1 != Keys.end()) {
            return *(it + 1);
        }
        return 0;
    }

    Uint8* GetBytes(bool exportHashes) {
        Uint32 stride = ((exportHashes ? 4 : 0) + sizeof(T));
        Uint8* bytes = (Uint8*)Memory::TrackedMalloc("HashMap::GetBytes", Count() * stride);
        if (exportHashes) {
            size_t index = 0;
            for (size_t i = 0; i < Keys.size(); i++) {
                Uint32 key = Keys[i];
                *(Uint32*)(bytes + index * stride) = key;
                *(T*)(bytes + index * stride + 4) = Data[key];
                index++;
            }
        }
        else {
            size_t index = 0;
            for (auto const& it : Data) {
                *(T*)(bytes + index * stride) = it.second;
                index++;
            }
        }
        return bytes;
    }
    void   FromBytes(Uint8* bytes, int count) {
        Uint32 stride = (4 + sizeof(T));
        for (int i = 0; i < count; i++) {
            Put(*(Uint32*)(bytes + i * stride),
                *(T*)(bytes + i * stride + 4));
        }
    }

    ~HashMap<T>() {
        Data.clear();
        Keys.clear();
    }
};

#endif
