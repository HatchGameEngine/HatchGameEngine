#ifndef ENGINE_EXCEPTIONS_COMPILERERROREXCEPTION_H
#define ENGINE_EXCEPTIONS_COMPILERERROREXCEPTION_H

#include <stdexcept>
#include <string>

class CompilerErrorException : public std::runtime_error {
public:
	CompilerErrorException(const std::string& message) : std::runtime_error(message) {}
};

#endif /* ENGINE_EXCEPTIONS_COMPILERERROREXCEPTION_H */
