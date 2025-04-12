#ifndef HASHMAP_H
#define HASHMAP_H

#include <Engine/Diagnostics/Log.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/Hashing/Murmur.h>
#include <Engine/Includes/Standard.h>
#include <functional>

inline Uint32 TranslateHashMapIndex(Uint32 index) {
	// Find index that works to put new data in.
	index += (index << 12);
	index ^= (index >> 22);
	index += (index << 4);
	index ^= (index >> 9);
	index += (index << 10);
	index ^= (index >> 2);
	index += (index << 7);
	index ^= (index >> 12);
	index = (index >> 3) * 0x9E3779B1U;
	return index;
}

using HashFuncPtr =  Uint32 (*)(const void*, size_t);

template<typename T>
struct HashMapElement {
	Uint32 Key;
	Uint32 PrevKey;
	Uint32 NextKey;
	bool Used;
	T Data;
};
template<typename T>
class HashMap {
public:
	int Count = 0;
	int Capacity = 0;
	HashMapElement<T>* Data = NULL;

	HashFuncPtr HashFunction = NULL;

	HashMap<T>(HashFuncPtr hashFunc = NULL, int capacity = 16) {
		HashFunction = hashFunc;
		if (HashFunction == NULL) {
			HashFunction = Murmur::EncryptData;
		}

		Count = 0;
		Capacity = capacity;
		CapacityMask = capacity - 1;

		Data = (HashMapElement<T>*)Memory::TrackedCalloc(
			"HashMap::Data", Capacity, sizeof(HashMapElement<T>));
		if (!Data) {
			Log::Print(Log::LOG_ERROR, "Could not allocate memory for HashMap data!");
			exit(-1);
		}
	}

	void Put(Uint32 hash, T data) {
		Uint32 index;
		do {
			index = TranslateIndex(hash);

			for (int i = 0; i < ChainLength; i++) {
				if (!Data[index].Used) {
					Count++;
					if (Count >= Capacity / 2) {
						index = 0xFFFFFFFFU;
						Resize();
						break;
					}
					break;
				}

				if (Data[index].Used && Data[index].Key == hash) {
					break;
				}

				index = (index + 1) & CapacityMask;
			}
		} while (index == 0xFFFFFFFFU);

		Data[index].Key = hash;
		if (!Data[index].Used) {
			AppendKey(&Data[index]);
		}
		Data[index].Used = true;
		Data[index].Data = data;
	}
	void Put(const char* key, T data) {
		Uint32 hash = HashFunction(key, strlen(key));
		Put(hash, data);
	}

	T Get(Uint32 hash) {
		Uint32 index = TranslateIndex(hash);

		for (int i = 0; i < ChainLength; i++) {
			if (Data[index].Used && Data[index].Key == hash) {
				return Data[index].Data;
			}

			index = (index + 1) & CapacityMask;
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
		Uint32 index = TranslateIndex(hash);

		for (int i = 0; i < ChainLength; i++) {
			if (Data[index].Used && Data[index].Key == hash) {
				return true;
			}

			index = (index + 1) & CapacityMask;
		}

		return false;
	}
	bool Exists(const char* key) {
		Uint32 hash = HashFunction(key, strlen(key));
		return Exists(hash);
	}

	bool GetIfExists(Uint32 hash, T* result) {
		Uint32 index = TranslateIndex(hash);

		for (int i = 0; i < ChainLength; i++) {
			if (Data[index].Used && Data[index].Key == hash) {
				*result = Data[index].Data;
				return true;
			}

			index = (index + 1) & CapacityMask;
		}

		return false;
	}
	bool GetIfExists(const char* key, T* result) {
		Uint32 hash = HashFunction(key, strlen(key));
		return GetIfExists(hash, result);
	}

	bool Remove(Uint32 hash) {
		Uint32 index = TranslateIndex(hash);

		for (int i = 0; i < ChainLength; i++) {
			if (Data[index].Used && Data[index].Key == hash) {
				Count--;
				RemoveKey(&Data[index]);
				Data[index].Used = false;
				return true;
			}

			index = (index + 1) & CapacityMask;
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
			}
		}
	}

	void ForAll(void (*forFunc)(Uint32, T)) {
		for (int i = 0; i < Capacity; i++) {
			if (Data[i].Used) {
				forFunc(Data[i].Key, Data[i].Data);
			}
		}
	}
	void WithAll(std::function<void(Uint32, T)> forFunc) {
		for (int i = 0; i < Capacity; i++) {
			if (Data[i].Used) {
				forFunc(Data[i].Key, Data[i].Data);
			}
		}
	}
	void ForAllOrdered(void (*forFunc)(Uint32, T)) {
		Uint32 index;
		if (FirstKey == 0) {
			return;
		}

		Uint32 nextKey = FirstKey;
		do {
			index = FindKey(nextKey);
			nextKey = 0;
			if (index != 0xFFFFFFFFU) {
				forFunc(Data[index].Key, Data[index].Data);
				nextKey = Data[index].NextKey;
			}
		} while (nextKey);
	}
	void WithAllOrdered(std::function<void(Uint32, T)> forFunc) {
		Uint32 index;
		if (FirstKey == 0) {
			return;
		}

		Uint32 nextKey = FirstKey;
		do {
			index = FindKey(nextKey);
			nextKey = 0;
			if (index != 0xFFFFFFFFU) {
				forFunc(Data[index].Key, Data[index].Data);
				nextKey = Data[index].NextKey;
			}
		} while (nextKey);
	}
	Uint32 GetFirstKey() { return FirstKey; }
	Uint32 GetNextKey(Uint32 key) {
		Uint32 index = FindKey(key);
		if (index != 0xFFFFFFFFU) {
			return Data[index].NextKey;
		}
		return 0;
	}

	Uint8* GetBytes(bool exportHashes) {
		Uint32 stride = ((exportHashes ? 4 : 0) + sizeof(T));
		Uint8* bytes = (Uint8*)Memory::TrackedMalloc("HashMap::GetBytes", Count * stride);
		if (exportHashes) {
			for (int i = 0, index = 0; i < Capacity; i++) {
				if (Data[i].Used) {
					*(Uint32*)(bytes + index * stride) = Data[i].Key;
					*(T*)(bytes + index * stride + 4) = Data[i].Data;
					index++;
				}
			}
		}
		else {
			for (int i = 0, index = 0; i < Capacity; i++) {
				if (Data[i].Used) {
					*(T*)(bytes + index * stride) = Data[i].Data;
					index++;
				}
			}
		}
		return bytes;
	}
	void FromBytes(Uint8* bytes, int count) {
		Uint32 stride = (4 + sizeof(T));
		for (int i = 0; i < count; i++) {
			Put(*(Uint32*)(bytes + i * stride), *(T*)(bytes + i * stride + 4));
		}
	}

	~HashMap<T>() { Memory::Free(Data); }

private:
	int CapacityMask = 0;
	int ChainLength = 64;
	Uint32 FirstKey = 0;
	Uint32 LastKey = 0;

	Uint32 TranslateIndex(Uint32 index) { return TranslateHashMapIndex(index) & CapacityMask; }

	Uint32 FindKey(Uint32 key) {
		Uint32 index = TranslateIndex(key);
		for (int i = 0; i < ChainLength; i++) {
			if (Data[index].Used && Data[index].Key == key) {
				return index;
			}

			index = (index + 1) & CapacityMask;
		}
		return 0xFFFFFFFFU;
	}
	void RemoveKey(HashMapElement<T>* data) {
		Uint32 index;
		if (data->PrevKey == 0 && data->NextKey != 0) {
			FirstKey = data->NextKey;
			index = FindKey(FirstKey);
			if (index != 0xFFFFFFFFU) {
				Data[index].PrevKey = 0;
			}
		}
		else if (data->PrevKey != 0 && data->NextKey == 0) {
			LastKey = data->PrevKey;
			index = FindKey(LastKey);
			if (index != 0xFFFFFFFFU) {
				Data[index].NextKey = 0;
			}
		}
		else {
			index = FindKey(data->PrevKey);
			if (index != 0xFFFFFFFFU) {
				Data[index].NextKey = data->NextKey;
			}

			index = FindKey(data->NextKey);
			if (index != 0xFFFFFFFFU) {
				Data[index].PrevKey = data->PrevKey;
			}
		}
	}
	void AppendKey(HashMapElement<T>* data) {
		data->PrevKey = LastKey;
		data->NextKey = 0;
		if (LastKey != 0) {
			Uint32 lastIndex = FindKey(LastKey);
			if (lastIndex != 0xFFFFFFFFU) {
				Data[lastIndex].NextKey = data->Key;
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

		HashMapElement<T>* oldData = Data;
		HashMapElement<T>* newData = NULL;
		const char* oldTrack = Memory::GetName(oldData);

		newData = (HashMapElement<T>*)Memory::TrackedCalloc(
			oldTrack, Capacity, sizeof(HashMapElement<T>));
		if (!newData) {
			Log::Print(Log::LOG_ERROR, "Could not allocate memory for HashMap data!");
			exit(-1);
		}

		Data = newData;
		Count = 0;

		for (int i = 0; i < oldCapacity; i++) {
			if (oldData[i].Used) {
				Uint32 index = TranslateIndex(oldData[i].Key);

				for (int c = 0; c < ChainLength; c++) {
					if (!newData[index].Used) {
						Count++;
						break;
					}
					if (newData[index].Used &&
						newData[index].Key == oldData[i].Key) {
						break;
					}
					index = (index + 1) & CapacityMask;
				}

				newData[index].Key = oldData[i].Key;
				newData[index].PrevKey = oldData[i].PrevKey;
				newData[index].NextKey = oldData[i].NextKey;
				newData[index].Used = true;
				newData[index].Data = oldData[i].Data;
			}
		}

		Memory::Free(oldData);

		return Capacity;
	}
};

#endif
