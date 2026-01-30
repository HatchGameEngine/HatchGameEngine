#ifndef ORDEREDHASHMAP_H
#define ORDEREDHASHMAP_H

#include <Engine/Includes/HashMap.h>
#include <functional>

template<typename T>
class OrderedHashMap : public HashMap<T> {
public:
	std::vector<Uint32> Keys;

	OrderedHashMap<T>(Uint32 (*hashFunc)(const void*, size_t) = nullptr, int capacity = 16) : HashMap<T>(hashFunc, capacity) {}

	void Put(Uint32 hash, T data) {
		if (!HashMap<T>::Exists(hash)) {
			Keys.push_back(hash);
		}
		HashMap<T>::Data[hash] = data;
	}
	void Put(const char* key, T data) {
		Uint32 hash = HashMap<T>::HashFunction(key, strlen(key));
		Put(hash, data);
	}

	bool Exists(Uint32 hash) {
		return std::find(Keys.begin(), Keys.end(), hash) != Keys.end();
	}
	bool Exists(const char* key) {
		Uint32 hash = HashMap<T>::HashFunction(key, strlen(key));
		return Exists(hash);
	}

	bool Remove(Uint32 hash) {
		auto it = std::find(Keys.begin(), Keys.end(), hash);
		if (it != Keys.end()) {
			HashMap<T>::Data.erase(hash);
			Keys.erase(it);
			return true;
		}
		return false;
	}
	bool Remove(const char* key) {
		Uint32 hash = HashMap<T>::HashFunction(key, strlen(key));
		return Remove(hash);
	}

	void Clear() {
		HashMap<T>::Data.clear();
		Keys.clear();
	}

	void ForAllOrdered(void (*forFunc)(Uint32, T)) {
		for (size_t i = 0; i < Keys.size(); i++) {
			Uint32 key = Keys[i];
			forFunc(key, HashMap<T>::Data[key]);
		}
	}
	void WithAllOrdered(std::function<void(Uint32, T)> withFunc) {
		for (size_t i = 0; i < Keys.size(); i++) {
			Uint32 key = Keys[i];
			withFunc(key, HashMap<T>::Data[key]);
		}
	}

	void EraseIf(bool (*eraseFunc)(Uint32, T)) {
		for (size_t i = 0; i < Keys.size(); i++) {
			Uint32 key = Keys[i];
			if (eraseFunc(key, HashMap<T>::Data[key])) {
				Remove(key);
			}
			else {
				i++;
			}
		}
	}

	Uint32 GetFirstKey() {
		if (Keys.size() > 0) {
			return Keys[0];
		}
		return 0;
	}
	Uint32 GetNextKey(Uint32 key) {
		auto it = std::find(Keys.begin(), Keys.end(), key);
		if (it != Keys.end() && it + 1 != Keys.end()) {
			return *(it + 1);
		}
		return 0;
	}

	~OrderedHashMap<T>() {
		Clear();
	}
};

#endif
