href = {}
descriptions = {}
lists = []

def init():
  from enums import DefType
  from doc_group import DocGroup

  for i in range(len(DefType)):
    lists.append(DocGroup())
