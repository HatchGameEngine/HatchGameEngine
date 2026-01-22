from enums import DefType

class Marker:
  DEF_START = "/***"
  DEF_END = "*/"

  def make(str):
    return "* \\" + str

  METHOD = make("method")
  CONSTRUCTOR = make("constructor")
  CONSTANT = make("constant")
  ENUM = make("enum")
  GLOBAL = make("global")
  FIELD = make("field")
  CLASS_FIELD = make("classfield")
  CLASS = make("class")
  NAMESPACE = make("namespace")

  DESC = make("desc")
  PARAM = make("param")
  RETURN = make("return")
  PARAM_OPT = make("paramOpt")
  TYPE = make("type")
  DEFAULT = make("default")
  DEPRECATED = make("deprecated")
  NS = make("ns")

  to_def_type = [
    (METHOD, DefType.METHOD),
    (CONSTRUCTOR, DefType.CONSTRUCTOR),
    (FIELD, DefType.FIELD),
    (CLASS_FIELD, DefType.CLASS_FIELD),
    (ENUM, DefType.ENUM),
    (CONSTANT, DefType.CONSTANT),
    (GLOBAL, DefType.GLOBAL_VAR),
    (CLASS, DefType.CLASS),
    (NAMESPACE, DefType.NAMESPACE)
  ]

  def get(marker, line):
    return line[len(marker):].strip()

  def get_multiline(marker, lines, line_num = 0):
    if line_num > 0:
      lines = lines[line_num:]

    result = Marker.get(marker, lines[0])
    cur_line = 1

    while result.endswith("\\"):
      if cur_line == len(lines):
        break
      result = result[0:(len(result)) - 1] + lines[cur_line].strip()
      cur_line += 1

    return result, cur_line
