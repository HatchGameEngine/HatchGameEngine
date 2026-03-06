#ifndef ENGINE_BYTECODE_API_API_H
#define ENGINE_BYTECODE_API_API_H

#include <Engine/Bytecode/API/libhsl.h>
#include <Engine/Bytecode/Types.h>

class Compiler;
class MemoryStream;

hsl_ValueType ValueTypeToAPIValueType(ValueType type);
hsl_ObjType ObjTypeToAPIObjType(ObjType type);

Uint32 hsl_get_hash_internal(const char* name);

hsl_Result hsl_compile_internal(Compiler* compiler, const char* code, MemoryStream** stream, const char* input_filename);

#endif /* ENGINE_BYTECODE_API_API_H */
