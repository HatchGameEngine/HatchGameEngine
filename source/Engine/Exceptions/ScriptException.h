#ifndef ENGINE_EXCEPTIONS_SCRIPTEXCEPTION_H
#define ENGINE_EXCEPTIONS_SCRIPTEXCEPTION_H

#include <stdexcept>
#include <string>

class ScriptException : public std::runtime_error {
public:
	ScriptException(const std::string& message) : std::runtime_error(message) {}
};

#endif /* ENGINE_EXCEPTIONS_SCRIPTEXCEPTION_H */
