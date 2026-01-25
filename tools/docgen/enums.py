from enum import Enum

class DefType(Enum):
  FUNCTION = 0
  METHOD = 1
  CONSTRUCTOR = 2
  FIELD = 3
  CLASS_FIELD = 4
  ENUM = 5
  CONSTANT = 6
  GLOBAL_VAR = 7
  CLASS = 8
  NAMESPACE = 9

  def is_descriptive(type):
    return type == DefType.CLASS or type == DefType.NAMESPACE

  def is_field(type):
    return type == DefType.FIELD or type == DefType.CLASS_FIELD

  def is_method(type):
    return type == DefType.METHOD or type == DefType.CONSTRUCTOR

defTypeNames = {
  DefType.FUNCTION: ("functions", "Class methods"),
  DefType.METHOD: ("methods", "Instance methods"),
  DefType.CONSTRUCTOR: ("constructors", "Instance constructors"),
  DefType.FIELD: ("fields", "Instance fields"),
  DefType.CLASS_FIELD: ("class fields", "Class fields"),
  DefType.ENUM: ("enums", "Enums"),
  DefType.CONSTANT: ("constants", "Constants"),
  DefType.GLOBAL_VAR: ("globals", "Globals"),
  DefType.CLASS: ("classes", "Classes"),
  DefType.NAMESPACE: ("namespaces", "Namespaces")
}
