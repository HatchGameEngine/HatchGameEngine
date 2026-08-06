import re

import doc_globals

from enums import DefType
from parser import Parser

class Writer:
  def can_write_docs(type):
    if is_descriptive(type):
      return False

    return doc_globals.lists[type.value].count > 0

  def can_write_namespace_link_list(type):
    if DefType.is_field(type) or type == DefType.CONSTRUCTOR or is_descriptive(type):
      return False

    if type == DefType.FUNCTION or type == DefType.METHOD or type == DefType.ENUM:
      if len(doc_globals.lists[type.value].namespace_list) == 0:
        return False

    if len(doc_globals.lists[type.value].doc_list) == 0:
      return False

    return True

  def can_write_namespace_contents_list(type):
    if type == DefType.CONSTANT or type == DefType.GLOBAL_VAR or is_descriptive(type):
      return False

    return Writer.can_write_namespace_link_list(type)

  def write_function_parameters(doc):
    parameter_index = 0
    parameter_text = "("

    end_optional_parameters = False

    for parameter in doc.params:
      label = parameter.label

      if parameter.optional and not end_optional_parameters:
        parameter_text += "["
        end_optional_parameters = True

      if parameter_index == 0:
        parameter_text += label
      else:
        parameter_text += f", {label}"

      parameter_index += 1

    if end_optional_parameters:
      parameter_text += "]"
    parameter_text += ")"

    return parameter_text

  def process_description(input, is_doxygen = False, use_html_code = True, use_html_links = True):
    if input is None:
      return None

    if is_doxygen:
      use_html_links = False
      use_html_code = False

    output = input

    # Replace backticks with <code>...</code> if using HTML
    if use_html_code:
      def convert_fn(match):
        match = match.group(1)
        return f"<code>{match}</code>"

      output = re.sub(r'`([^`]*)`', convert_fn, output)

    output = Parser.parse_ref(output, is_doxygen, use_html_links)
    output = Parser.parse_param_ref(output, is_doxygen, use_html_links)

    return output
