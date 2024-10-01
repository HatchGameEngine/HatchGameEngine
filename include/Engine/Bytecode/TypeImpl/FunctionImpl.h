#ifndef ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

class FunctionImpl {
public:
    static ObjClass *Class;

    static void Init();
    static VMValue VM_Bind(int argCount, VMValue* args, Uint32 threadID);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_FUNCTIONIMPL_H */
