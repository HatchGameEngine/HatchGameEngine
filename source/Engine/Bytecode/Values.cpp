#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>
class Values {
public:
    static bool PrettyPrint;
};
#endif

#include <Engine/Bytecode/Values.h>

#include <Engine/Diagnostics/Log.h>

int buffer_printf(PrintBuffer* printBuffer, const char *format, ...) {
    va_list args;
    va_list argsCopy;
    va_start(args, format);
    va_copy(argsCopy, args);

    // Do a simple print if printBuffer is missing
    if (!printBuffer || !printBuffer->Buffer) {
        vprintf(format, args);
        va_end(argsCopy);
        va_end(args);
        return 0;
    }

    // Get the character count we would write if we did
    int count = vsnprintf(NULL, 0, format, argsCopy);

    while (printBuffer->WriteIndex + count >= printBuffer->BufferSize) {
        // Increase the buffer size
        printBuffer->BufferSize <<= 1;

        // Reallocate buffer
        *printBuffer->Buffer = (char*)realloc(*printBuffer->Buffer, printBuffer->BufferSize);
        if (!*printBuffer->Buffer) {
            Log::Print(Log::LOG_ERROR, "Could not reallocate print buffer of size %d!", printBuffer->BufferSize);
            va_end(argsCopy);
            va_end(args);
            return -1;
        }
    }

    // Write the characters
    printBuffer->WriteIndex += vsnprintf(*printBuffer->Buffer + printBuffer->WriteIndex, printBuffer->BufferSize - printBuffer->WriteIndex, format, args);
    va_end(argsCopy);
    va_end(args);
    return 0;
}

bool Values::PrettyPrint = false;

// NOTE: This is for printing, not string conversion
PUBLIC STATIC void Values::PrintValue(VMValue value) {
    Values::PrintValue(NULL, value);
}
PUBLIC STATIC void Values::PrintValue(PrintBuffer* buffer, VMValue value) {
    Values::PrintValue(buffer, value, 0);
}
PUBLIC STATIC void Values::PrintValue(PrintBuffer* buffer, VMValue value, int indent) {
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
            PrintObject(buffer, value, indent);
            break;
        default:
            buffer_printf(buffer, "<unknown value type 0x%02X>", value.Type);
    }
}
PUBLIC STATIC void Values::PrintObject(PrintBuffer* buffer, VMValue value, int indent) {
    switch (OBJECT_TYPE(value)) {
        case OBJ_CLASS:
            buffer_printf(buffer, "<class %s>", AS_CLASS(value)->Name ? AS_CLASS(value)->Name->Chars : "(null)");
            break;
        case OBJ_BOUND_METHOD:
            buffer_printf(buffer, "<bound method %s>", AS_BOUND_METHOD(value)->Method->Name ? AS_BOUND_METHOD(value)->Method->Name->Chars : "(null)");
            break;
        case OBJ_CLOSURE:
            buffer_printf(buffer, "<clsr %s>", AS_CLOSURE(value)->Function->Name ? AS_CLOSURE(value)->Function->Name->Chars : "(null)");
            break;
        case OBJ_FUNCTION:
            buffer_printf(buffer, "<fn %s>", AS_FUNCTION(value)->Name ? AS_FUNCTION(value)->Name->Chars : "(null)");
            break;
        case OBJ_INSTANCE:
            buffer_printf(buffer, "<class %s> instance", AS_INSTANCE(value)->Object.Class->Name ? AS_INSTANCE(value)->Object.Class->Name->Chars : "(null)");
            break;
        case OBJ_NATIVE:
            buffer_printf(buffer, "<native fn>");
            break;
        case OBJ_STREAM:
            buffer_printf(buffer, "<stream>");
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
            if (PrettyPrint)
                buffer_printf(buffer, "\n");

            for (size_t i = 0; i < array->Values->size(); i++) {
                if (i > 0) {
                    buffer_printf(buffer, ",");
                    if (PrettyPrint)
                        buffer_printf(buffer, "\n");
                }

                if (PrettyPrint) {
                    for (int k = 0; k < indent + 1; k++)
                        buffer_printf(buffer, "    ");
                }

                PrintValue(buffer, (*array->Values)[i], indent + 1);
            }

            if (PrettyPrint) {
                buffer_printf(buffer, "\n");
                for (int i = 0; i < indent; i++)
                    buffer_printf(buffer, "    ");
            }

            buffer_printf(buffer, "]");
            break;
        }
        case OBJ_MAP: {
            ObjMap* map = (ObjMap*)AS_OBJECT(value);

            Uint32 hash;
            VMValue value;
            buffer_printf(buffer, "{");
            if (PrettyPrint)
                buffer_printf(buffer, "\n");

            bool first = false;
            for (int i = 0; i < map->Values->Capacity; i++) {
                if (map->Values->Data[i].Used) {
                    if (!first) {
                        first = true;
                    }
                    else {
                        buffer_printf(buffer, ",");
                        if (PrettyPrint)
                            buffer_printf(buffer, "\n");
                    }

                    for (int k = 0; k < indent + 1 && PrettyPrint; k++)
                        buffer_printf(buffer, "    ");

                    hash = map->Values->Data[i].Key;
                    value = map->Values->Data[i].Data;
                    if (map->Keys && map->Keys->Exists(hash))
                        buffer_printf(buffer, "\"%s\": ", map->Keys->Get(hash));
                    else
                        buffer_printf(buffer, "0x%08X: ", hash);
                    PrintValue(buffer, value, indent + 1);
                }
            }
            if (PrettyPrint)
                buffer_printf(buffer, "\n");
            for (int k = 0; k < indent && PrettyPrint; k++)
                buffer_printf(buffer, "    ");

            buffer_printf(buffer, "}");
            break;
        }
        default:
            buffer_printf(buffer, "UNKNOWN OBJECT TYPE %d", OBJECT_TYPE(value));
    }
}

PUBLIC STATIC bool Values::GetMethodFromClass(VMValue* value) {
    return false;
}
