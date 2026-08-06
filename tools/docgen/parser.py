import re
import xml.etree.ElementTree as ET

from enums import DefType
from doc_def import DocDef, FunctionDef, ParamDef, EnumDef, ConstantDef, FieldDef
from marker import Marker

import doc_globals

class Parser:
  REF_PATTERN = r'<ref (.*?)>'

  HTML_BREAK_STR = '<br/>'

  def parse_base_def_marker(doc_def, line, lines, line_num):
    if line.startswith(Marker.DESC):
      description, num_lines = Marker.get_multiline(Marker.DESC, lines, line_num)
      description = description.strip()
      if len(description) == 0:
        description = None
      doc_def.description = description
      return num_lines
    elif line.startswith(Marker.DEPRECATED):
      deprecated, num_lines = Marker.get_multiline(Marker.DEPRECATED, lines, line_num)
      doc_def.deprecated = deprecated.strip()
      return num_lines
    elif line.startswith(Marker.NS):
      doc_def.namespace = Marker.get(Marker.NS, line)
      return 0

    return None

  def parse_function_def(title, type, lines):
    doc_def = FunctionDef()
    doc_def.title = title
    doc_def.type = type

    for line_num, line in enumerate(lines):
      line = line.strip()
      if line.startswith(Marker.DEF_END):
        break

      num_lines = Parser.parse_base_def_marker(doc_def, line, lines, line_num)
      if num_lines is None:
        # paramOpt
        if line.startswith(Marker.PARAM_OPT):
          result, num_lines = Marker.get_multiline(Marker.PARAM_OPT, lines, line_num)
          param_opt = ParamDef(result, True)
          doc_def.params.append(param_opt)
        # param
        elif line.startswith(Marker.PARAM):
          result, num_lines = Marker.get_multiline(Marker.PARAM, lines, line_num)
          param = ParamDef(result, False)
          doc_def.params.append(param)
        # return
        elif line.startswith(Marker.RETURN):
          result, num_lines = Marker.get_multiline(Marker.RETURN, lines, line_num)
          match = re.search(Parser.REF_PATTERN, result.strip())
          if match:
            start, end = match.span()
            if start == 0:
              result = result[:start] + result[end:]
              doc_def.return_type = Parser.parse_ref(match.group(0))
              doc_def.returns = result.strip()
            else:
              match = None
          if not match:
            split_result = result.split(maxsplit = 1)
            if len(split_result):
              doc_def.return_type = split_result[0]
              doc_def.returns = split_result[1]
            else:
              doc_def.returns = result

      line_num += num_lines or 0

    if type == DefType.CONSTRUCTOR:
      doc_def.title = doc_def.namespace

    return doc_def

  def parse_generic_def(title, type, lines):
    doc_def = DocDef()
    doc_def.title = title
    doc_def.type = type

    for line_num, line in enumerate(lines):
      line = line.strip()
      if line.startswith(Marker.DEF_END):
        break

      line_num += Parser.parse_base_def_marker(doc_def, line, lines, line_num) or 0

    return doc_def

  def parse_enum_def(title, lines):
    doc_def = EnumDef()
    doc_def.title = title

    for line_num, line in enumerate(lines):
      line = line.strip()
      if line.startswith(Marker.DEF_END):
        break

      line_num += Parser.parse_base_def_marker(doc_def, line, lines, line_num) or 0

    pos = title.find('_')
    if pos != -1:
      doc_def.prefix = title[0:(pos + 1)] + '*'

    return doc_def

  def parse_constant_def(title, lines):
    doc_def = ConstantDef()
    doc_def.title = title

    for line_num, line in enumerate(lines):
      line = line.strip()
      if line.startswith(Marker.DEF_END):
        break

      num_lines = Parser.parse_base_def_marker(doc_def, line, lines, line_num)
      if num_lines is not None:
        line_num += num_lines
      # type
      elif line.startswith(Marker.TYPE):
        doc_def.value_type = Marker.get(Marker.TYPE, line)

    return doc_def

  def parse_field_def(title, type, lines):
    doc_def = FieldDef()
    doc_def.title = title
    doc_def.type = type

    for line_num, line in enumerate(lines):
      line = line.strip()
      if line.startswith(Marker.DEF_END):
        break

      num_lines = Parser.parse_base_def_marker(doc_def, line, lines, line_num)
      if num_lines is not None:
        line_num += num_lines
      # type
      elif line.startswith(Marker.TYPE):
        doc_def.value_type = Parser.parse_ref(Marker.get(Marker.TYPE, line))
      # default
      elif line.startswith(Marker.DEFAULT):
        doc_def.default_value = Marker.get(Marker.DEFAULT, line)

    return doc_def

  def parse_def(title, type, lines):
    if type == DefType.FUNCTION or type == DefType.METHOD or type == DefType.CONSTRUCTOR:
      return Parser.parse_function_def(title, type, lines)
    elif type == DefType.ENUM:
        return Parser.parse_enum_def(title, lines)
    elif type == DefType.CONSTANT:
      return Parser.parse_constant_def(title, lines)
    elif type == DefType.FIELD or type == DefType.CLASS_FIELD:
      return Parser.parse_field_def(title, type, lines)

    return Parser.parse_generic_def(title, type, lines)

  def parse_doc_lines(lines):
    if len(lines) == 0:
      return None

    first_line = lines[0]

    for key, value in Marker.to_def_type:
      if first_line.startswith(key):
        title = first_line[len(key):].strip()
        return Parser.parse_def(title, value, lines)

    # Assume it's a function
    title = first_line[1:].strip()
    if len(title) == 0:
      return None

    return Parser.parse_function_def(title, DefType.FUNCTION, lines[1:])

  def parse_ref(input, use_doxygen_refs = False, use_html_links = False):
    if input is None:
      return None

    def convert_fn(match):
      match = match.group(1)
      if use_html_links:
        if match in doc_globals.href:
          return f"<a href=\"#{doc_globals.href[match]}\">{match}</a>"
        else:
          return match
      else:
        replaced = match.replace('_*', '')
        if use_doxygen_refs:
          return f"\\ref {replaced}"
        else:
          return replaced

    output = re.sub(Parser.REF_PATTERN, convert_fn, input)

    return output

  def parse_param_ref(input, is_doxygen, use_html_code):
    if input is None:
      return None

    def convert_fn(match):
      match = match.group(1)
      if is_doxygen:
        return f"\\a {match}"
      elif use_html_code:
        return f"<code>{match}</code>"
      else:
        return match

    output = re.sub(r'<param (.*?)>', convert_fn, input)

    return output
