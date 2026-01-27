#!/usr/bin/env python3

import os, argparse, pathlib, glob
import doc_globals

from sys import stdout, stderr
from enums import DefType
from marker import Marker
from doc_def import DocDef
from parser import Parser
from html_writer import HTMLWriter
from doxygen_writer import DoxygenWriter

arg_parser = argparse.ArgumentParser(prog = 'docgen')
arg_parser.add_argument(
  '-i', '--input',
  nargs = '*',
  help = 'The input files, or path to a directory containing the files'
)
arg_parser.add_argument(
  '-o', '--output',
  help='The output path, or stdout if omitted',
  nargs = '?',
  type = pathlib.Path,
  default = stdout
)
arg_parser.add_argument(
  '--dox', '--doxygen',
  help='Generate documentation for Doxygen',
  action='store_true'
)

def main(args, arg_count):
  if arg_count < 2 or '-h' in args or '--help' in args:
    arg_parser.print_help()
    return

  parsed_args = arg_parser.parse_args(args[1:])

  input_paths = parsed_args.input
  output_file = parsed_args.output

  doc_globals.init()

  read_docs(input_paths)
  process_docs(doc_globals.lists)
  write_docs(output_file, parsed_args)

def read_file(file):
  is_parsing_doc = False
  doc_def = None
  doc_lines = []

  for line_in_file in file:
    line = line_in_file.strip()

    if line.startswith(Marker.DEF_START):
      is_parsing_doc = True
      continue
    elif line.startswith(Marker.DEF_END):
      doc_def = Parser.parse_doc_lines(doc_lines)
      doc_lines.clear()
      is_parsing_doc = False

    if doc_def:
      DocDef.add(doc_def)
      doc_def = None
    elif is_parsing_doc:
      doc_lines.append(line)

def read_docs(input_paths):
  for path in input_paths:
    if os.path.isdir(path):
      open_and_read_files_in_folder(path)
    else:
      open_and_read_file(path)

def open_and_read_file(path):
  with open(path) as file:
    read_file(file)

def open_and_read_files_in_folder(path):
  for filename in glob.glob(path + "/**/*.cpp", recursive=True):
    open_and_read_file(filename)

def process_docs(lists):
  # Sort namespace and enum lists alphabetically
  for type in DefType:
      if type == DefType.FUNCTION or type == DefType.METHOD or type == DefType.ENUM:
        lists[type.value].namespace_list.sort()

def write_docs(output_file, parsed_args):
  if parsed_args.dox == True:
    if output_file == stdout or output_file.is_file():
      raise ValueError("Must specify path (not file) when exporting Doxygen documentation")
    DoxygenWriter.generate_files(output_file)
  elif output_file == stdout:
    HTMLWriter.generate_doc_file(output_file)
  else:
    with output_file.open(mode='w') as file:
      HTMLWriter.generate_doc_file(file)

if __name__ == '__main__':
  from sys import argv, exit
  main(argv, len(argv))
