#ifndef ENGINE_IO_TERMINALINPUT_H
#define ENGINE_IO_TERMINALINPUT_H

#include <Engine/Includes/Standard.h>
#include <iostream>

#define REPL_TRIM_CHARS " \t\r\v\f"

class TerminalInput {
private:
	static bool ReadLine(std::string& line, const char* prompt);

public:
	static std::string GetLine(const char* prompt);
};

#endif /* ENGINE_IO_TERMINALINPUT_H */
