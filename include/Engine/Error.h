#ifndef ENGINE_ERROR_H
#define ENGINE_ERROR_H

class Error {
private:
	static void ShowFatal(const char* errorString, bool showMessageBox);

public:
	static void Fatal(const char* errorMessage, ...);
	static void FatalNoMessageBox(const char* errorMessage, ...);
};

#endif /* ENGINE_ERROR_H */
