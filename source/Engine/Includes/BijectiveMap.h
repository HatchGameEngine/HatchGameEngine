#ifndef BIJECTIVEMAP_H
#define BIJECTIVEMAP_H

#include <functional>
#include <unordered_map>

template<typename T1, typename T2>
class BijectiveMap {
private:
	std::unordered_map<T1, T2> KeyToValueMap;
	std::unordered_map<T2, T1> ValueToKeyMap;

public:
	void Put(T1 key, T2 value) {
		KeyToValueMap.insert(std::make_pair(key, value));
		ValueToKeyMap.insert(std::make_pair(value, key));
	}

	T2 Get(T1 key) { return KeyToValueMap[key]; }
	T1 Get(T2 value) { return ValueToKeyMap[value]; }

	bool Exists(T1 key) { return KeyToValueMap.find(key) != KeyToValueMap.end(); }
	bool Exists(T2 value) { return ValueToKeyMap.find(value) != ValueToKeyMap.end(); }

	void WithAllKeys(std::function<bool(T1, T2)> withFunc) {
		for (auto const& it : KeyToValueMap) {
			if (withFunc(it.first, it.second)) {
				return;
			}
		}
	}
	void WithAllValues(std::function<bool(T1, T2)> withFunc) {
		for (auto const& it : ValueToKeyMap) {
			if (withFunc(it.first, it.second)) {
				return;
			}
		}
	}
};

#endif
