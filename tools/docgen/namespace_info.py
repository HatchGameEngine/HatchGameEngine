import doc_globals

from enums import DefType, defTypeNames

class NamespaceInfo:
  all = {}

  def __init__(self):
    self.is_enum_namespace = False
    self.docs_per_def = None

  def get_href(name):
    return "Reference_" + name

  def get_title(type):
    if type == DefType.FUNCTION:
      return "Namespaces"

    return defTypeNames[type][1]

  def get(name):
    if name in NamespaceInfo.all:
      return NamespaceInfo.all[name]

    ns_info = NamespaceInfo()
    ns_info.docs_per_def = {}

    for type in DefType:
      ns_info.docs_per_def[type.value] = []

    NamespaceInfo.all[name] = ns_info
    doc_globals.href[name] = NamespaceInfo.get_href(name)

    return ns_info

  def add_for_doc_def(group, doc_def):
    name = doc_def.namespace
    if name == None:
      if doc_def.type == DefType.ENUM:
        group.add_prefix(doc_def)
      return

    group.add_namespace(doc_def)

    ns_info = NamespaceInfo.get(name)
    ns_info.docs_per_def[doc_def.type.value].append(doc_def)

  def get_for_enum(name):
    if name in NamespaceInfo.all:
      return NamespaceInfo.all[name]

    ns_info = NamespaceInfo.get(name)
    ns_info.is_enum_namespace = True

    return ns_info
