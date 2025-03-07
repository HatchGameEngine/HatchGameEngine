#ifndef CHAINEDHASHMAP_H
#define CHAINEDHASHMAP_H

#include <Engine/Includes/HashMap.h>

template<typename T>
struct ChainedHashMapElement : HashMapElement<T> {
	vector<ChainedHashMapElement<T>>* List = nullptr;
};
template<typename T>
class ChainedHashMap {
public:
	int Count = 0;
	int Capacity = 0;
	ChainedHashMapElement<T>* Data = NULL;

	Uint32 (*HashFunction)(const void*, size_t) = NULL;

	ChainedHashMap<T>(Uint32 (*hashFunc)(const void*, size_t) = NULL, int capacity = 16) {
		HashFunction = hashFunc;
		if (HashFunction == NULL) {
			HashFunction = Murmur::EncryptData;
		}

		Count = 0;
		Capacity = capacity;
		CapacityMask = Capacity - 1;

		Data = (ChainedHashMapElement<T>*)Memory::TrackedCalloc(
			"ChainedHashMap::Data", Capacity, sizeof(ChainedHashMapElement<T>));
		if (!Data) {
			Log::Print(Log::LOG_ERROR,
				"Could not allocate memory for ChainedHashMap data!");
			exit(-1);
		}
	}

	~ChainedHashMap<T>() {
		if (Data) {
			for (int i = 0; i < Capacity; i++) {
				if (Data[i].Used) {
					delete Data[i].List;
				}
			}
			Memory::Free(Data);
		}
	}

	void Put(Uint32 hash, T data) {
		ChainedHashMapElement<T>* element = nullptr;
		do {
			element = GetElement(hash);

			if (!element->Used) {
				Count++;
				if (Count >= Capacity / 2) {
					element = nullptr;
					Resize();
					continue;
				}
				break;
			}

			if (element->Used) {
				if (element->Key == hash) {
					break;
				}

				ChainedHashMapElement<T> newElem;
				element->List->push_back(newElem);
				element = &((*element->List)[element->List->size() - 1]);
				element->Used = true;
			}
		} while (element == nullptr);

		element->Key = hash;
		if (!element->Used) {
			AppendKey(element);
		}
		element->Used = true;
		element->Data = data;
		element->List = new vector<ChainedHashMapElement<T>>();
	}
	void Put(const char* key, T data) {
		Uint32 hash = HashFunction(key, strlen(key));
		Put(hash, data);
	}

	T Get(Uint32 hash) {
		ChainedHashMapElement<T>* element = GetElement(hash);
		if (element->Used) {
			if (element->Key == hash) {
				return element->Data;
			}

			for (size_t i = 0; i < element->List->size(); i++) {
				if ((*element->List)[i].Key == hash) {
					return (*element->List)[i].Data;
				}
			}
		}

#ifdef IOS
		return T();
#else
		return T{0};
#endif
	}
	T Get(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Get(hash);
	}
	bool Exists(Uint32 hash) {
		ChainedHashMapElement<T>* element = GetElement(hash);
		if (element->Used) {
			if (element->Key == hash) {
				return true;
			}

			for (size_t i = 0; i < element->List->size(); i++) {
				if ((*element->List)[i].Key == hash) {
					return true;
				}
			}
		}

		return false;
	}
	bool Exists(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Exists(hash);
	}

	bool GetIfExists(Uint32 hash, T* result) {
		ChainedHashMapElement<T>* element = GetElement(hash);
		if (element->Used) {
			if (element->Key == hash) {
				*result = element->Data;
				return true;
			}

			for (size_t i = 0; i < element->List->size(); i++) {
				if ((*element->List)[i].Key == hash) {
					*result = (*element->List)[i].Data;
					return true;
				}
			}
		}

		return false;
	}
	bool GetIfExists(const char* key, T* result) {
		Uint32 hash = HashFunction(key, strlen(key));
		return GetIfExists(hash, result);
	}

	bool Remove(Uint32 hash) {
		ChainedHashMapElement<T>* element = GetElement(hash);
		if (element->Used) {
			if (element->Key == hash) {
				Count--;
				RemoveKey(element);
				element->Used = false;
				return true;
			}

			for (size_t i = 0; i < element->List->size(); i++) {
				if ((*element->List)[i].Key == hash) {
					element->List->erase(element->List->begin() + i);
					return true;
				}
			}
		}

		return false;
	}
	bool Remove(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Remove(hash);
	}

	void Clear() {
		for (int i = 0; i < Capacity; i++) {
			if (Data[i].Used) {
				Data[i].Used = false;
				Data[i].List->clear();
			}
		}
	}

	void ForAll(void (*forFunc)(Uint32, T)) {
		for (int i = 0; i < Capacity; i++) {
			if (Data[i].Used) {
				forFunc(Data[i].Key, Data[i].Data);
				for (size_t i = 0; i < Data[i].List->size(); i++) {
					forFunc(Data[i].List[i].Key, Data[i].List[i].Data);
				}
			}
		}
	}
	void WithAll(std::function<void(Uint32, T)> forFunc) {
		for (int i = 0; i < Capacity; i++) {
			if (Data[i].Used) {
				forFunc(Data[i].Key, Data[i].Data);
				for (size_t i = 0; i < Data[i].List->size(); i++) {
					forFunc(Data[i].List[i].Key, Data[i].List[i].Data);
				}
			}
		}
	}
	void ForAllOrdered(void (*forFunc)(Uint32, T)) {
		ChainedHashMapElement<T>* element;
		if (FirstKey == 0) {
			return;
		}

		Uint32 nextKey = FirstKey;
		do {
			element = FindKey(nextKey);
			nextKey = 0;
			if (element != nullptr) {
				forFunc(element->Key, element->Data);
				for (size_t i = 0; i < element->List->size(); i++) {
					forFunc((*element->List)[i].Key, (*element->List)[i].Data);
				}
				nextKey = element->NextKey;
			}
		} while (nextKey);
	}
	void WithAllOrdered(std::function<void(Uint32, T)> forFunc) {
		ChainedHashMapElement<T>* element;
		if (FirstKey == 0) {
			return;
		}

		Uint32 nextKey = FirstKey;
		do {
			element = FindKey(nextKey);
			nextKey = 0;
			if (element != nullptr) {
				forFunc(element->Key, element->Data);
				for (size_t i = 0; i < element->List->size(); i++) {
					forFunc((*element->List)[i].Key, (*element->List)[i].Data);
				}
				nextKey = element->NextKey;
			}
		} while (nextKey);
	}
	Uint32 GetFirstKey() { return FirstKey; }
	Uint32 GetNextKey(Uint32 key) {
		ChainedHashMapElement<T>* element = FindKey(key);
		if (element != nullptr) {
			return element->NextKey;
		}
		return 0;
	}

private:
	int CapacityMask = 0;
	Uint32 FirstKey = 0;
	Uint32 LastKey = 0;

	Uint32 TranslateIndex(Uint32 index) { return TranslateHashMapIndex(index) & CapacityMask; }

	ChainedHashMapElement<T>* GetElement(Uint32 key) {
		Uint32 index = TranslateIndex(key);
		return &Data[index];
	}

	ChainedHashMapElement<T>* FindKey(Uint32 key) {
		ChainedHashMapElement<T>* element = GetElement(key);

		if (element->Used) {
			if (element->Key == key) {
				return element;
			}

			for (size_t i = 0; i < element->List->size(); i++) {
				if ((*element->List)[i].Used && (*element->List)[i].Key == key) {
					return &((*element->List)[i]);
				}
			}
		}

		return nullptr;
	}
	void RemoveKey(ChainedHashMapElement<T>* data) {
		ChainedHashMapElement<T>* element;
		if (data->PrevKey == 0 && data->NextKey != 0) {
			FirstKey = data->NextKey;
			element = FindKey(FirstKey);
			if (element != nullptr) {
				element->PrevKey = 0;
			}
		}
		else if (data->PrevKey != 0 && data->NextKey == 0) {
			LastKey = data->PrevKey;
			element = FindKey(LastKey);
			if (element != nullptr) {
				element->NextKey = 0;
			}
		}
		else {
			element = FindKey(data->PrevKey);
			if (element != nullptr) {
				element->NextKey = data->NextKey;
			}

			element = FindKey(data->NextKey);
			if (element != nullptr) {
				element->PrevKey = data->PrevKey;
			}
		}
	}
	void AppendKey(ChainedHashMapElement<T>* data) {
		data->PrevKey = LastKey;
		data->NextKey = 0;
		if (LastKey != 0) {
			ChainedHashMapElement<T>* lastElement = FindKey(LastKey);
			if (lastElement != nullptr) {
				lastElement->NextKey = data->Key;
			}
		}
		else {
			FirstKey = data->Key;
		}
		LastKey = data->Key;
	}

	int Resize() {
		int oldCapacity = Capacity;

		Capacity <<= 1;
		CapacityMask = Capacity - 1;

		ChainedHashMapElement<T>* oldData = Data;
		ChainedHashMapElement<T>* newData = NULL;
		const char* oldTrack = Memory::GetName(oldData);

		newData = (ChainedHashMapElement<T>*)Memory::TrackedCalloc(
			oldTrack, Capacity, sizeof(ChainedHashMapElement<T>));
		if (!newData) {
			Log::Print(Log::LOG_ERROR,
				"Could not allocate memory for ChainedHashMap data!");
			exit(-1);
		}

		Data = newData;
		Count = 0;

		for (int i = 0; i < oldCapacity; i++) {
			if (oldData[i].Used) {
				ChainedHashMapElement<T>* element = GetElement(oldData[i].Key);
				if (!element->Used) {
					Count++;
				}
				else if (element->Used) {
					element->List->push_back(oldData[i]);
					continue;
				}
				element->Key = oldData[i].Key;
				element->PrevKey = oldData[i].PrevKey;
				element->NextKey = oldData[i].NextKey;
				element->Used = true;
				element->Data = oldData[i].Data;
				element->List = new vector<ChainedHashMapElement<T>>();
			}
		}

		for (int i = 0; i < oldCapacity; i++) {
			if (oldData[i].Used) {
				delete oldData[i].List;
			}
		}
		Memory::Free(oldData);

		return Capacity;
	}
};

#endif
