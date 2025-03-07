#include <Engine/Bytecode/Values.h>

#include <Engine/Diagnostics/Log.h>

#include <Engine/Includes/PrintBuffer.h>

// NOTE: This is for printing, not string conversion
void Values::PrintValue(VMValue value) {
	Values::PrintValue(NULL, value);
}
void Values::PrintValue(PrintBuffer* buffer, VMValue value) {
	Values::PrintValue(buffer, value, 0, false);
}
void Values::PrintValue(PrintBuffer* buffer, VMValue value, bool prettyPrint) {
	Values::PrintValue(buffer, value, 0, prettyPrint);
}
void Values::PrintValue(PrintBuffer* buffer, VMValue value, int indent, bool prettyPrint) {
	switch (value.Type) {
	case VAL_NULL:
		buffer_printf(buffer, "null");
		break;
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		buffer_printf(buffer, "%d", AS_INTEGER(value));
		break;
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		buffer_printf(buffer, "%f", AS_DECIMAL(value));
		break;
	case VAL_OBJECT:
		PrintObject(buffer, value, indent, prettyPrint);
		break;
	default:
		buffer_printf(buffer, "<unknown value type 0x%02X>", value.Type);
	}
}
void Values::PrintObject(PrintBuffer* buffer, VMValue value, int indent, bool prettyPrint) {
	switch (OBJECT_TYPE(value)) {
	case OBJ_CLASS:
		buffer_printf(buffer,
			"<class %s>",
			AS_CLASS(value)->Name ? AS_CLASS(value)->Name->Chars : "(null)");
		break;
	case OBJ_BOUND_METHOD:
		buffer_printf(buffer,
			"<bound method %s>",
			AS_BOUND_METHOD(value)->Method->Name
				? AS_BOUND_METHOD(value)->Method->Name->Chars
				: "(null)");
		break;
	case OBJ_CLOSURE:
		buffer_printf(buffer,
			"<closure %s>",
			AS_CLOSURE(value)->Function->Name ? AS_CLOSURE(value)->Function->Name->Chars
							  : "(null)");
		break;
	case OBJ_FUNCTION:
		buffer_printf(buffer,
			"<fn %s>",
			AS_FUNCTION(value)->Name ? AS_FUNCTION(value)->Name->Chars : "(null)");
		break;
	case OBJ_MODULE:
		buffer_printf(buffer,
			"<module %s>",
			AS_MODULE(value)->SourceFilename ? AS_MODULE(value)->SourceFilename->Chars
							 : "(null)");
		break;
	case OBJ_INSTANCE:
		buffer_printf(buffer,
			"<class %s> instance",
			AS_INSTANCE(value)->Object.Class->Name
				? AS_INSTANCE(value)->Object.Class->Name->Chars
				: "(null)");
		break;
	case OBJ_NATIVE:
		buffer_printf(buffer, "<native fn>");
		break;
	case OBJ_STREAM:
		buffer_printf(buffer, "<stream>");
		break;
	case OBJ_MATERIAL:
		buffer_printf(buffer,
			"<material %s>",
			(AS_MATERIAL(value)->MaterialPtr != nullptr &&
				AS_MATERIAL(value)->MaterialPtr->Name != nullptr)
				? AS_MATERIAL(value)->MaterialPtr->Name
				: "(null)");
		break;
	case OBJ_NAMESPACE:
		buffer_printf(buffer,
			"<namespace %s>",
			AS_NAMESPACE(value)->Name ? AS_NAMESPACE(value)->Name->Chars : "(null)");
		break;
	case OBJ_STRING:
		buffer_printf(buffer, "\"%s\"", AS_CSTRING(value));
		break;
	case OBJ_UPVALUE:
		buffer_printf(buffer, "<upvalue>");
		break;
	case OBJ_ARRAY: {
		ObjArray* array = (ObjArray*)AS_OBJECT(value);

		buffer_printf(buffer, "[");
		if (prettyPrint) {
			buffer_printf(buffer, "\n");
		}

		for (size_t i = 0; i < array->Values->size(); i++) {
			if (i > 0) {
				buffer_printf(buffer, ",");
				if (prettyPrint) {
					buffer_printf(buffer, "\n");
				}
			}

			if (prettyPrint) {
				for (int k = 0; k < indent + 1; k++) {
					buffer_printf(buffer, "    ");
				}
			}

			PrintValue(buffer, (*array->Values)[i], indent + 1);
		}

		if (prettyPrint) {
			buffer_printf(buffer, "\n");
			for (int i = 0; i < indent; i++) {
				buffer_printf(buffer, "    ");
			}
		}

		buffer_printf(buffer, "]");
		break;
	}
	case OBJ_MAP: {
		ObjMap* map = (ObjMap*)AS_OBJECT(value);

		Uint32 hash;
		VMValue value;
		buffer_printf(buffer, "{");
		if (prettyPrint) {
			buffer_printf(buffer, "\n");
		}

		bool first = false;
		for (int i = 0; i < map->Values->Capacity; i++) {
			if (map->Values->Data[i].Used) {
				if (!first) {
					first = true;
				}
				else {
					buffer_printf(buffer, ",");
					if (prettyPrint) {
						buffer_printf(buffer, "\n");
					}
				}

				for (int k = 0; k < indent + 1 && prettyPrint; k++) {
					buffer_printf(buffer, "    ");
				}

				hash = map->Values->Data[i].Key;
				value = map->Values->Data[i].Data;
				if (map->Keys && map->Keys->Exists(hash)) {
					buffer_printf(buffer, "\"%s\": ", map->Keys->Get(hash));
				}
				else {
					buffer_printf(buffer, "0x%08X: ", hash);
				}
				PrintValue(buffer, value, indent + 1);
			}
		}
		if (prettyPrint) {
			buffer_printf(buffer, "\n");
		}
		for (int k = 0; k < indent && prettyPrint; k++) {
			buffer_printf(buffer, "    ");
		}

		buffer_printf(buffer, "}");
		break;
	}
	default:
		buffer_printf(buffer, "<unknown object type 0x%02X>", OBJECT_TYPE(value));
	}
}
