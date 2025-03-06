#ifndef ENGINE_TOKEN_H
#define ENGINE_TOKEN_H

struct Token {
	int Type;
	char* Start;
	size_t Length;
	int Line;
	size_t Pos;

	std::string ToString() { return std::string(Start, Length); }
};

#endif
