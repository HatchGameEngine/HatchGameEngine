from namespace_info import NamespaceInfo

class DocGroup:
  def __init__(self):
    self.doc_list = []
    self.namespaces = {}
    self.namespace_list = []
    self.count = 0
    self.has_desc = 0

  def add_namespace(self, doc_def):
    namespace_name = doc_def.namespace
    if namespace_name == None:
      return

    ns = None

    # Check if this namespace exists
    if not namespace_name in self.namespaces:
      ns = []
      self.namespaces[namespace_name] = ns
      self.namespace_list.append(namespace_name)

    ns = self.namespaces[namespace_name]
    ns.append(doc_def)

  def add_prefix(self, enum_def):
    prefix = enum_def.prefix
    if prefix == None:
      return

    ns = None

    # Check if this namespace exists
    if not prefix in self.namespaces:
      ns = []
      self.namespaces[prefix] = ns
      self.namespace_list.append(prefix)

    ns = self.namespaces[prefix]
    ns.append(enum_def)

    ns_info = NamespaceInfo.get_for_enum(prefix)
    ns_info.docs_per_def[enum_def.type.value].append(enum_def)
