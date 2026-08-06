#ifndef HASHMAP_H
#define HASHMAP_H

#include <Engine/Hashing/Murmur.h>
#include <Engine/Includes/Standard.h>
#include <functional>

#ifndef USE_STD_UNORDERED_MAP
#include <Libraries/ankerl/unordered_dense.h>
#endif

template<typename T>
class HashMap {
public:
#ifdef USE_STD_UNORDERED_MAP
	std::unordered_map<Uint32, T> Data;
#else
	ankerl::unordered_dense::map<Uint32, T> Data;
#endif

	Uint32 (*HashFunction)(const void*, size_t) = nullptr;

	HashMap<T>(Uint32 (*hashFunc)(const void*, size_t) = nullptr, int capacity = 16) {
		HashFunction = hashFunc;
		if (HashFunction == nullptr) {
			HashFunction = Murmur::EncryptData;
		}
	}

	size_t Count() { return Data.size(); }

	void Put(Uint32 hash, T data) {
		Data[hash] = data;
	}
	void Put(const char* key, T data) {
		Uint32 hash = HashFunction(key, strlen(key));
		Put(hash, data);
	}

	T Get(Uint32 hash) {
		if (Data.find(hash) != Data.end()) {
			return Data[hash];
		}
		return T{0};
	}
	T Get(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Get(hash);
	}

	bool Exists(Uint32 hash) {
		return Data.count(hash) > 0;
	}
	bool Exists(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Exists(hash);
	}

	bool GetIfExists(Uint32 hash, T* result) {
		if (Data.find(hash) != Data.end()) {
			*result = Data[hash];
			return true;
		}
		return false;
	}
	bool GetIfExists(const char* key, T* result) {
		Uint32 hash = HashFunction(key, strlen(key));
		return GetIfExists(hash, result);
	}

	bool Remove(Uint32 hash) {
		auto it = Data.find(hash);
		if (it != Data.end()) {
			Data.erase(it);
			return true;
		}
		return false;
	}
	bool Remove(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Remove(hash);
	}

	void Clear() {
		Data.clear();
	}

	void ForAll(void (*forFunc)(Uint32, T)) {
		for (auto it = Data.begin(); it != Data.end(); it++) {
			forFunc(it->first, it->second);
		}
	}
	void WithAll(std::function<void(Uint32, T)> withFunc) {
		for (auto it = Data.begin(); it != Data.end(); it++) {
			withFunc(it->first, it->second);
		}
	}

	void EraseIf(bool (*eraseFunc)(Uint32, T)) {
		for (auto it = Data.begin(); it != Data.end();) {
			if (eraseFunc(it->first, it->second)) {
				it = Data.erase(it);
			}
			else {
				it++;
			}
		}
	}

	void GetBytes(Uint8* bytes) {
		size_t stride = sizeof(Uint32) + sizeof(T);
		size_t index = 0;
		for (auto const& it : Data) {
			*(Uint32*)(bytes + index * stride) = it.first;
			*(T*)(bytes + index * stride + sizeof(Uint32)) = it.second;
			index++;
		}
	}
	void FromBytes(Uint8* bytes, int count) {
		size_t stride = sizeof(Uint32) + sizeof(T);
		for (int i = 0; i < count; i++) {
			Put(*(Uint32*)(bytes + i * stride), *(T*)(bytes + i * stride + sizeof(Uint32)));
		}
	}

	~HashMap<T>() {
		Clear();
	}
};

#endif
