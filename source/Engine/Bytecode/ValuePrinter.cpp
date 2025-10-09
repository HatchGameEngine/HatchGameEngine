#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>

void ValuePrinter::Print(VMValue value) {
	Print(nullptr, value);
}
void ValuePrinter::Print(PrintBuffer* buffer, VMValue value) {
	Print(buffer, value, false);
}
void ValuePrinter::Print(PrintBuffer* buffer, VMValue value, bool prettyPrint) {
	Print(buffer, value, false, false);
}
void ValuePrinter::Print(PrintBuffer* buffer, VMValue value, bool prettyPrint, bool isJSON) {
	ValuePrinter printer;
	printer.Buffer = buffer;
	printer.PrettyPrint = prettyPrint;
	printer.IsJSON = isJSON;
	printer.PrintRootValue(value);
}

void ValuePrinter::PrintRootValue(VMValue value) {
	if (IS_STRING(value) && !IsJSON) {
		buffer_printf(Buffer, "%s", AS_CSTRING(value));
		return;
	}

	PrintValue(value, 0);
}
void ValuePrinter::PrintValue(VMValue value, int indent) {
	switch (value.Type) {
	case VAL_NULL:
		buffer_printf(Buffer, "null");
		break;
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		buffer_printf(Buffer, "%d", AS_INTEGER(value));
		break;
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		buffer_printf(Buffer, "%f", AS_DECIMAL(value));
		break;
	case VAL_OBJECT:
		PrintObject(value, indent);
		break;
	default:
		buffer_printf(Buffer, "<unknown value type 0x%02X>", value.Type);
	}
}
void ValuePrinter::PrintObject(VMValue value, int indent) {
	switch (OBJECT_TYPE(value)) {
	case OBJ_CLASS:
	case OBJ_BOUND_METHOD:
	case OBJ_CLOSURE:
	case OBJ_FUNCTION:
	case OBJ_MODULE:
	case OBJ_NAMESPACE:
	case OBJ_RESOURCE:
		if (IsJSON) {
			buffer_printf(Buffer,
				"\"%s %s\"",
				Value::GetObjectTypeName(value),
				Value::GetPrintableObjectName(value));
		}
		else {
			buffer_printf(Buffer,
				"<%s %s>",
				Value::GetObjectTypeName(value),
				Value::GetPrintableObjectName(value));
		}
		break;
	case OBJ_NATIVE_FUNCTION:
	case OBJ_UPVALUE:
	case OBJ_ASSET:
		if (IsJSON) {
			buffer_printf(Buffer, "\"%s\"", Value::GetObjectTypeName(value));
		}
		else {
			buffer_printf(Buffer, "<%s>", Value::GetObjectTypeName(value));
		}
		break;
	case OBJ_INSTANCE:
	case OBJ_NATIVE_INSTANCE:
	case OBJ_ENTITY:
		if (IsJSON) {
			buffer_printf(
				Buffer, "\"%s instance\"", Value::GetPrintableObjectName(value));
		}
		else {
			buffer_printf(
				Buffer, "<%s instance>", Value::GetPrintableObjectName(value));
		}
		break;
	case OBJ_STRING:
		buffer_printf(Buffer, "\"%s\"", AS_CSTRING(value));
		break;
	case OBJ_ARRAY:
		PrintArray((ObjArray*)AS_OBJECT(value), indent);
		break;
	case OBJ_MAP:
		PrintMap((ObjMap*)AS_OBJECT(value), indent);
		break;
	default:
		if (IsJSON) {
			buffer_printf(Buffer, "\"unknown object type 0x%02X\"", OBJECT_TYPE(value));
		}
		else {
			buffer_printf(Buffer, "<unknown object type 0x%02X>", OBJECT_TYPE(value));
		}
	}
}
void ValuePrinter::PrintArray(ObjArray* array, int indent) {
	buffer_printf(Buffer, "[");
	if (PrettyPrint) {
		buffer_printf(Buffer, "\n");
	}

	for (size_t i = 0; i < array->Values->size(); i++) {
		if (i > 0) {
			buffer_printf(Buffer, ",");
			if (PrettyPrint) {
				buffer_printf(Buffer, "\n");
			}
		}

		if (PrettyPrint) {
			for (int k = 0; k < indent + 1; k++) {
				buffer_printf(Buffer, "    ");
			}
		}

		PrintValue((*array->Values)[i], indent + 1);
	}

	if (PrettyPrint) {
		buffer_printf(Buffer, "\n");
		for (int i = 0; i < indent; i++) {
			buffer_printf(Buffer, "    ");
		}
	}

	buffer_printf(Buffer, "]");
}
void ValuePrinter::PrintMap(ObjMap* map, int indent) {
	Uint32 hash;
	VMValue value;
	buffer_printf(Buffer, "{");
	if (PrettyPrint) {
		buffer_printf(Buffer, "\n");
	}

	bool first = false;
	for (int i = 0; i < map->Values->Capacity; i++) {
		if (map->Values->Data[i].Used) {
			if (!first) {
				first = true;
			}
			else {
				buffer_printf(Buffer, ",");
				if (PrettyPrint) {
					buffer_printf(Buffer, "\n");
				}
			}

			for (int k = 0; k < indent + 1 && PrettyPrint; k++) {
				buffer_printf(Buffer, "    ");
			}

			hash = map->Values->Data[i].Key;
			value = map->Values->Data[i].Data;
			if (map->Keys && map->Keys->Exists(hash)) {
				buffer_printf(Buffer, "\"%s\": ", map->Keys->Get(hash));
			}
			else {
				buffer_printf(Buffer, "0x%08X: ", hash);
			}
			PrintValue(value, indent + 1);
		}
	}
	if (PrettyPrint) {
		buffer_printf(Buffer, "\n");
	}
	for (int k = 0; k < indent && PrettyPrint; k++) {
		buffer_printf(Buffer, "    ");
	}

	buffer_printf(Buffer, "}");
}
