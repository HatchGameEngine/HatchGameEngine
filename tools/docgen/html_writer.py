import doc_globals

from enums import DefType, defTypeNames
from namespace_info import NamespaceInfo
from parser import Parser
from writer import Writer

class HTMLWriter:
  def write_namespace_link_list(type):
    group = doc_globals.lists[type.value]

    text = f"        <h3>{NamespaceInfo.get_title(type)}</h3>\n"
    text += "        <ul>\n"

    if type == DefType.FUNCTION or type == DefType.METHOD or type == DefType.ENUM:
      for namespace_name in group.namespace_list:
        if type == DefType.ENUM and not NamespaceInfo.all[namespace_name].is_enum_namespace:
          continue

        href = NamespaceInfo.get_href(namespace_name)
        text += f"            <li><a href=\"#{href}\">{namespace_name}</a></li>\n"
    else:
      for doc in group.doc_list:
        href = doc.get_href()
        title = doc.get_title()
        text += f"                    <li><a href=\"#{href}\">{title}</a></li>\n"

    return text + "        </ul>\n"

  def write_enum_namespace_contents_list():
    def_type = DefType.ENUM

    text = f"        <h3>{NamespaceInfo.get_title(def_type)}</h3>\n"

    group = doc_globals.lists[def_type.value]

    for namespace_name in group.namespace_list:
      namespace_info = NamespaceInfo.all[namespace_name]
      if not namespace_info.is_enum_namespace:
        continue

      text += f"            <p id=\"{NamespaceInfo.get_href(namespace_name)}\">\n"
      text += f"                <h2><code>{namespace_name}</code></h2>\n"

      if len(namespace_info.docs_per_def[def_type.value]) == 0:
        break

      text += "                <ul>\n"

      for doc in namespace_info.docs_per_def[def_type.value]:
        text += f"                    <li><a href=\"#{doc.get_href()}\">{doc.get_title()}</a></li>\n"

      text += "                </ul>\n"
      text += "            </p>\n"

    return text

  def write_namespace_contents_list(type):
    if type == DefType.ENUM:
      return HTMLWriter.write_enum_namespace_contents_list()

    text = f"        <h3>{defTypeNames[type][1]}</h3>\n"

    group = doc_globals.lists[type.value]

    for namespace_name in group.namespace_list:
      text += f"            <p id=\"{NamespaceInfo.get_href(namespace_name)}\">\n"
      text += "                <h2>" + namespace_name + "</h2>\n"

      namespace_info = NamespaceInfo.all[namespace_name]

      for def_type in DefType:
        if len(namespace_info.docs_per_def[def_type.value]) == 0:
          continue

        text += f"                <i>{defTypeNames[def_type][1]}:</i>\n"
        text += "                <ul>\n"

        for doc in namespace_info.docs_per_def[def_type.value]:
          text += f"                    <li><a href=\"#{doc.get_href()}\">{doc.get_title()}</a></li>\n"

        text += "                </ul>\n"

      text += "            </p>\n"

    return text

  def write_docdef_title(doc):
    return f"        <h3 style=\"margin-bottom: 8px;\"><code>{doc.get_title()}</code></h2>\n"

  def write_docdef_description(doc):
    description = Writer.process_description(doc.description) or ""
    return f"        <div style=\"margin-top: 8px; font-size: 14px;\">{description}</div>\n"

  def write_docdef_type(doc):
    return f"        <div style=\"font-size: 14px;\"><b>Type: </b>{doc.value_type}</div>\n"

  def write_generic_docs(doc):
    text = HTMLWriter.write_docdef_title(doc)

    if doc.description is not None:
      text += HTMLWriter.write_docdef_description(doc)

    return text

  def write_function_docs(doc):
    title = doc.get_title()
    parameters = Writer.write_function_parameters(doc)
    description = doc.description
    returns = doc.returns

    text = None

    if description is not None:
      text = f"        <h2 style=\"margin-bottom: 8px;\">{title}</h2>\n"
    else:
      text = f"        <h2 style=\"margin-bottom: 8px; color: red;\">{title}</h2>\n"

    text += f"        <code>{title}{parameters}</code>\n"

    if description is not None:
      text += HTMLWriter.write_docdef_description(doc)

    if len(doc.params) > 0:
      text += "        <div style=\"font-weight: bold; margin-top: 8px;\">Parameters:</div>\n"
      text += "        <ul style=\"margin-top: 0px; font-size: 14px;\">\n"

      for param in doc.params:
        type = Parser.parse_ref(param.type, False, True)
        description = Writer.process_description(param.description) or ""
        if description:
          description = f": {description}"
        text += f"        <li><b>{param.label} ({type})</b>{description}</li>\n"

      text += "        </ul>\n"

    returns_description = Writer.process_description(returns)
    if returns_description:
      text += "        <div style=\"font-weight: bold; margin-top: 8px;\">Returns:</div>\n"
      text += f"        <div style=\"font-size: 14px;\">{returns_description}</div>\n"

    return text

  def write_constant_docs(doc):
    text = HTMLWriter.write_docdef_title(doc)

    if doc.value_type is not None:
      text += HTMLWriter.write_docdef_type(doc)

    if doc.description is not None:
      text += HTMLWriter.write_docdef_description(doc)

    return text

  def write_field_docs(doc):
    text = HTMLWriter.write_docdef_title(doc)

    if doc.value_type is not None:
      text += HTMLWriter.write_docdef_type(doc)

    default_value = doc.default_value
    if default_value is not None:
      text += f"        <div style=\"font-size: 14px;\"><b>Default: </b><code>{default_value}</code></div>\n"

    if doc.description is not None:
      text += HTMLWriter.write_docdef_description(doc)

    return text

  def write_docdef(group, doc, type):
    text = f"        <p id=\"{doc.get_href()}\">\n"

    if type == DefType.FUNCTION or type == DefType.METHOD or type == DefType.CONSTRUCTOR:
      text += HTMLWriter.write_function_docs(doc)
    elif type == DefType.CONSTANT:
      text += HTMLWriter.write_constant_docs(doc)
    elif type == DefType.FIELD or type == DefType.CLASS_FIELD:
      text += HTMLWriter.write_field_docs(doc)
    else:
      text += HTMLWriter.write_generic_docs(doc)

    if doc.description is not None:
      group.has_desc += 1

    text += "        </p>\n"

    return text

  def write_docs(type):
    text = f"        <h3>{defTypeNames[type][1]}</h3>\n"

    group = doc_globals.lists[type.value]

    if type == DefType.CONSTANT or type == DefType.GLOBAL_VAR:
      for doc in group.doc_list:
        text += HTMLWriter.write_docdef(group, doc, type)
    else:
      for namespace_name in group.namespace_list:
        namespace_info = NamespaceInfo.all[namespace_name]

        for doc in namespace_info.docs_per_def[type.value]:
          text += HTMLWriter.write_docdef(group, doc, type)

    with_descriptions = str(group.has_desc)
    without_descriptions = str(group.count)

    text += f"        <p>{with_descriptions} out of {without_descriptions} {defTypeNames[type][0]} have descriptions. </p>\n"
    text += "        <hr/>\n"

    return text

  def read_stylesheet(path):
    try:
      with open(path, 'r', encoding = 'utf-8') as file:
        return file.read()
    except FileNotFoundError:
      return ""

  def generate_doc_file(file):
    namespace_link_list = ""
    namespace_contents_list = ""
    docs_text = ""

    for type in DefType:
      # Write out all namespaces
      if Writer.can_write_namespace_link_list(type):
        namespace_link_list += HTMLWriter.write_namespace_link_list(type)

      # Write out what's in those namespaces
      if Writer.can_write_namespace_contents_list(type):
        namespace_contents_list += HTMLWriter.write_namespace_contents_list(type)

      # Write out docs
      if Writer.can_write_docs(type):
        docs_text += HTMLWriter.write_docs(type)

    # Read stylesheet
    stylesheet_data = HTMLWriter.read_stylesheet("style.css")

    # Write to file
    file.write(f"""<html>
  <head>
    <title>Hatch Game Engine Documentation</title>
    <style>
    {stylesheet_data}
    </style>
  </head>
  <body>
    <div style="position: fixed; margin-top: -32px; margin-left: -96px; width: 100%; text-align: right;">
        <a href="#Reference_top">Back to top</a>
    </div>
    <h1 id="Reference_top">Hatch Game Engine Reference</h1>
    {namespace_link_list}
    <hr/>
    {namespace_contents_list}
    <hr/>
    {docs_text}
  </body>
</html>""")
