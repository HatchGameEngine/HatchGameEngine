#ifndef COMPTIMETABLE_H
#define COMPTIMETABLE_H

typedef void (*CompTimeCalc)(int *table, int table_size);

template<int size, CompTimeCalc comp_func>
struct CompTimeTable {
	int table[size];

	constexpr CompTimeTable<size, comp_func>(): table() {
		comp_func(table, size);
	}

    constexpr int operator[](int i) const {
        if (i > 0 && i < sizeof table){
            return table[i];
        }
        return 0;
    }
};

#endif
