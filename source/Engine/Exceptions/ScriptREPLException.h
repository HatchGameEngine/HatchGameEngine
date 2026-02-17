#ifndef ENGINE_EXCEPTIONS_SCRIPTREPLEXCEPTION_H
#define ENGINE_EXCEPTIONS_SCRIPTREPLEXCEPTION_H

#include <stdexcept>
#include <string>

class ScriptREPLException : public std::runtime_error {
public:
	ScriptREPLException(const std::string& message) : std::runtime_error(message) {}
};

#endif /* ENGINE_EXCEPTIONS_SCRIPTREPLEXCEPTION_H */
