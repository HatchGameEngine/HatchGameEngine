#!/usr/bin/env python3
#

HELP_TEXT = '''
Alphabetiser and ASCIIbetiser program
Written by Alexander Nicholi
Copyright (C) 2025 Aquefir Consulting LLC
Released under BSD-2-Clause

Usage:
\tcat mylist.txt | python3 abc.py [<-h|--help>]
\tcat mylist.txt | python3 abc.py <-7|--ascii>
\tcat mylist.txt | python3 abc.py <-u|--unicode>

This program operates on textual input in a line-oriented fashion. It
detects the line ending style by remembering the first one it encounters
and recognises Unix style, Windows style and Classic Mac OS style.

Pass the -u option to do full Unicode-aware alphabetisation. This
requires IBM\'s ICU library; install it with pip3 install pyicu.

Pass the -7 option to do ASCIIbetisation, ergo sort by ASCII character
number. The program will error out if a non-ASCII octet is encountered
and nothing will be output; check stderr and return code then.
'''

from typing import Literal

def lnstyle(input: str) -> Literal['\n', '\r\n', '\r']:
	i: int = 0
	maybemac: bool = False
	input_sz: int = len(input)
	while i < input_sz:
		if input[i] == '\n':
			return '\n' if not maybemac else '\r\n'
		elif maybemac: # previous was \r and current isn't \n so...
			return '\r'
		elif input[i] == '\r': # wasn't already maybemac so note that
			maybemac = True
		else: # all other characters reset the check
			maybemac = False
		i += 1
	raise Exception('no line endings found! this cannot be sorted')

def asciibetise(input: str, ln: Literal['\n', '\r\n', '\r']) -> str:
	lines: list[str] = input.split(ln)
	return ln.join(sorted(lines))

def main(args: list[str]) -> int:
	if '-h' in args or '--help' in args:
		from sys import stderr
		print(HELP_TEXT, file=stderr)
		return 0
	asciimode: bool | None = None
	if '-7' in args or '--ascii' in args:
		asciimode = True
	if '-u' in args or '--unicode' in args:
		if asciimode != None:
			raise Exception('multiple conflicting modes specified')
		asciimode = False
	if asciimode == None:
		from sys import stderr
		print(HELP_TEXT, file=stderr)
		return 0
	from sys import stdin
	text = stdin.read()[:-1]
	ln = lnstyle(text)
	from sys import stdout
	output = asciibetise(text, ln)
	stdout.write(output)
	stdout.write('\n')
	return 0

if __name__ == '__main__':
	from sys import argv, exit
	exit(main(argv))
