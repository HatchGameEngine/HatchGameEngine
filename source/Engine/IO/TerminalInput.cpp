#include <Engine/IO/TerminalInput.h>

#ifdef USING_LINENOISE
#include <Libraries/linenoise-ng/linenoise.h>
#endif

bool TerminalInput::ReadLine(std::string& line, const char* prompt) {
#ifdef USING_LINENOISE
	char* read = linenoise(prompt);
	if (!read) {
		return false;
	}

	line = std::string(read);

	free(read);
#else
	printf(prompt);

	std::getline(std::cin, line);
#endif

	return true;
}

std::string TerminalInput::GetLine(const char* prompt) {
	// Read the line
	std::string read;

	if (!ReadLine(read, prompt)) {
		return "";
	}

	// Trim the line
	size_t pos = read.find_first_not_of(REPL_TRIM_CHARS);
	if (pos != std::string::npos) {
		read.erase(0, pos);
		pos = read.find_last_not_of(REPL_TRIM_CHARS);
		if (pos != std::string::npos) {
			read.erase(pos + 1);
		}
		else {
			read.clear();
		}
	}

	// Return nothing if the entire string is empty
	if (read.size() == 0) {
		return read;
	}

	// Return nothing if the entire string is whitespace
	bool isWhitespace = std::all_of(read.cbegin(), read.cend(), [](char c) {
		return std::isspace(c);
	});
	if (isWhitespace) {
		return "";
	}

#ifdef USING_LINENOISE
	linenoiseHistoryAdd(read.c_str());
#endif

	return read;
}
