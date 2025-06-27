#ifndef ENGINE_ERROR_H
#define ENGINE_ERROR_H

class Error {
public:
	static void Fatal(const char* errorMessage, ...);
};

#endif /* ENGINE_ERROR_H */
