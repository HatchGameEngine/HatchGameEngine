#if INTERFACE
#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>

class VMThread {
public:
    VMValue   Stack[STACK_SIZE_MAX];
    VMValue*  StackTop = Stack;
    VMValue   RegisterValue;

    CallFrame Frames[FRAMES_MAX];
    Uint32    FrameCount;
    Uint32    ReturnFrame;

    VMValue   FunctionToInvoke;

    enum ThreadState {
        CREATED = 0,
        RUNNING = 1,
        WAITING = 2,
        CLOSED = 3,
    };

    char      Name[THREAD_NAME_MAX];
    Uint32    ID;
    Uint32    State;
    bool      DebugInfo;

    static    bool InstructionIgnoreMap[0x100];
    static    std::jmp_buf JumpBuffer;
};
#endif

#include <Engine/Bytecode/VMThread.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/Values.h>

#ifndef _MSC_VER
#define USING_VM_DISPATCH_TABLE
#endif

// Locks are only in 3 places:
// Heap, which contains object memory and globals
// Bytecode area, which contains function bytecode
// Tokens & Strings

#define __Tokens__ BytecodeObjectManager::Tokens

bool         VMThread::InstructionIgnoreMap[0x100];
std::jmp_buf VMThread::JumpBuffer;

// #region Error Handling & Debug Info
char GetTokenBuffer[16];

#define THROW_ERROR_START() va_list args; \
    char errorString[2048]; \
    va_start(args, errorMessage); \
    vsnprintf(errorString, sizeof(errorString), errorMessage, args); \
    va_end(args); \
    char* textBuffer = (char*)malloc(512); \
    PrintBuffer buffer; \
    buffer.Buffer = &textBuffer; \
    buffer.WriteIndex = 0; \
    buffer.BufferSize = 512
#define THROW_ERROR_END() Log::Print(Log::LOG_ERROR, textBuffer); \
    PrintStack(); \
    const SDL_MessageBoxButtonData buttonsError[] = { \
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Exit Game" }, \
        { 0                                      , 2, "Ignore All" }, \
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Continue" }, \
    }; \
    const SDL_MessageBoxButtonData buttonsFatal[] = { \
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Exit Game" }, \
    }; \
    const SDL_MessageBoxData messageBoxData = { \
        SDL_MESSAGEBOX_ERROR, NULL, \
        "Script Error", \
        textBuffer, \
        (int)(fatal ? SDL_arraysize(buttonsFatal) : SDL_arraysize(buttonsError)), \
        fatal ? buttonsFatal : buttonsError, \
        NULL, \
    }; \
    int buttonClicked; \
    if (SDL_ShowMessageBox(&messageBoxData, &buttonClicked) < 0) { \
        buttonClicked = 2; \
    } \
    free(textBuffer); \
    switch (buttonClicked) { \
        /* Exit Game */ \
        case 1: \
            Application::Cleanup(); \
            exit(-1); \
            /* NOTE: This is for later, this doesn't actually execute. */ \
            return ERROR_RES_EXIT; \
        /* Ignore All */ \
        case 2: \
            VMThread::InstructionIgnoreMap[000000000] = true; \
            return ERROR_RES_CONTINUE; \
    }
#define GET_FUNCTION_NAME(hash) \
    std::string functionName(GetToken(hash)); \
    if (functionName == "main") \
        functionName = "top-level function"; \
    else if (functionName != "<anonymous-fn>") \
        functionName = "event " + functionName

PUBLIC char*   VMThread::GetToken(Uint32 hash) {
    if (__Tokens__ && __Tokens__->Exists(hash))
        return __Tokens__->Get(hash);

    snprintf(GetTokenBuffer, 15, "%X", hash);
    return &GetTokenBuffer[0];
}
PRIVATE void    VMThread::PrintStackTrace(PrintBuffer* buffer, const char* errorString) {
    int line;
    char* source;

    CallFrame* frame = &Frames[FrameCount - 1];
    ObjFunction* function = frame->Function;

    if (function) {
        if (function->Chunk.Lines) {
            size_t bpos = (frame->IPLast - frame->IPStart);
            line = function->Chunk.Lines[bpos] & 0xFFFF;

            GET_FUNCTION_NAME(function->NameHash);
            buffer_printf(buffer, "In %s of %s, line %d:\n\n    %s\n", functionName.c_str(), function->SourceFilename, line, errorString);
        }
        else {
            buffer_printf(buffer, "In %d:\n    %s\n", (int)(frame->IP - frame->IPStart), errorString);
        }
    }
    else {
        buffer_printf(buffer, "In %d:\n    %s\n", (int)(frame->IP - frame->IPStart), errorString);
    }

    buffer_printf(buffer, "\nCall Trace (Thread %d):\n", ID);
    for (Uint32 i = 0; i < FrameCount; i++) {
        CallFrame* fr = &Frames[i];
        function = fr->Function;
        source = function->SourceFilename;
        line = -1;
        if (i > 0) {
            CallFrame* fr2 = &Frames[i - 1];
            line = fr2->Function->Chunk.Lines[fr2->IPLast - fr2->IPStart] & 0xFFFF;
        }
        GET_FUNCTION_NAME(function->NameHash);
        buffer_printf(buffer, "    called \"%s\" of \"%s\"", functionName.c_str(), source);

        if (line > 0) {
            buffer_printf(buffer, " on Line %d", line);
        }
        if (i < FrameCount - 1)
            buffer_printf(buffer, ", then");
        buffer_printf(buffer, "\n");
    }
}
PUBLIC int     VMThread::ThrowRuntimeError(bool fatal, const char* errorMessage, ...) {
    if (VMThread::InstructionIgnoreMap[000000000])
        return ERROR_RES_CONTINUE;

    THROW_ERROR_START();

    if (FrameCount > 0)
        PrintStackTrace(&buffer, errorString);
    else if (IS_OBJECT(FunctionToInvoke)) {
        if (OBJECT_TYPE(FunctionToInvoke) == OBJ_NATIVE) {
            buffer_printf(&buffer, "While calling native function:\n\n    %s\n", errorString);
        } else {
            ObjFunction* function = NULL;

            if (OBJECT_TYPE(FunctionToInvoke) == OBJ_BOUND_METHOD) {
                ObjBoundMethod* bound = AS_BOUND_METHOD(FunctionToInvoke);
                function = bound->Method;
            }
            else if (OBJECT_TYPE(FunctionToInvoke) == OBJ_FUNCTION) {
                function = AS_FUNCTION(FunctionToInvoke);
            }

            if (function) {
                GET_FUNCTION_NAME(function->NameHash);
                buffer_printf(&buffer, "While calling %s of %s:\n\n    %s\n", functionName.c_str(), function->SourceFilename, errorString);
            }
            else {
                buffer_printf(&buffer, "While calling value: %s\n", errorString);
            }
        }
    }
    else {
        buffer_printf(&buffer, "%s\n", errorString);
    }

    THROW_ERROR_END();

    return ERROR_RES_CONTINUE;
}
PUBLIC void    VMThread::PrintStack() {
    int i = 0;
    printf("Stack:\n");
    for (VMValue* v = StackTop - 1; v >= Stack; v--) {
        printf("%4d '", i);
        Compiler::PrintValue(*v);
        printf("'\n");
        i--;
    }
}
PUBLIC void    VMThread::ReturnFromNative() throw() {
    // std::longjmp(VMThread::JumpBuffer, 1);
    // throw "should be ignored";
}
// #endregion

// #region Stack stuff
PUBLIC void    VMThread::Push(VMValue value) {
    if (StackSize() == STACK_SIZE_MAX) {
        if (ThrowRuntimeError(true, "Stack overflow! \nStack Top: %p \nStack: %p\nCount: %d", StackTop, Stack, StackSize()) == ERROR_RES_CONTINUE)
            return;
    }

    // bool debugInstruction = ID == 1;
    // if (debugInstruction) printf("push\n");

    // if (IS_OBJECT(value)) {
    //     if (AS_OBJECT(value) == NULL) {
    //         ThrowRuntimeError(true, "hol up");
    //     }
    // }
    *(StackTop++) = value;
}
PUBLIC VMValue VMThread::Pop() {
    if (StackTop == Stack) {
        if (ThrowRuntimeError(true, "Stack underflow!") == ERROR_RES_CONTINUE)
            return *StackTop;
    }

    // bool debugInstruction = ID == 1;
    // if (debugInstruction) printf("pop\n");

    StackTop--;
    return *StackTop;
}
PUBLIC VMValue VMThread::Peek(int offset) {
    return *(StackTop - offset - 1);
}
PUBLIC Uint32  VMThread::StackSize() {
    return (Uint32)(StackTop - Stack);
}
PUBLIC void    VMThread::ResetStack() {
    // bool debugInstruction = ID == 1;
    // if (debugInstruction) printf("reset stack\n");

    StackTop = Stack;
}
// #endregion

// #region Instruction stuff
enum   ThreadReturnCodes {
    INTERPRET_RUNTIME_ERROR = -100,
    INTERPRET_GLOBAL_DOES_NOT_EXIST,
    INTERPRET_GLOBAL_ALREADY_EXIST,
    INTERPRET_FINISHED = -1,
    INTERPRET_OK = 0,
};

// NOTE: These should be inlined
PUBLIC Uint8   VMThread::ReadByte(CallFrame* frame) {
    frame->IP += sizeof(Uint8);
    return *(Uint8*)(frame->IP - sizeof(Uint8));
}
PUBLIC Uint16  VMThread::ReadUInt16(CallFrame* frame) {
    frame->IP += sizeof(Uint16);
    return *(Uint16*)(frame->IP - sizeof(Uint16));
}
PUBLIC Uint32  VMThread::ReadUInt32(CallFrame* frame) {
    frame->IP += sizeof(Uint32);
    return *(Uint32*)(frame->IP - sizeof(Uint32));
}
PUBLIC Sint16  VMThread::ReadSInt16(CallFrame* frame) {
    return (Sint16)ReadUInt16(frame);
}
PUBLIC Sint32  VMThread::ReadSInt32(CallFrame* frame) {
    return (Sint32)ReadUInt32(frame);
}
PUBLIC VMValue VMThread::ReadConstant(CallFrame* frame) {
    return (*frame->Function->Chunk.Constants)[ReadUInt32(frame)];
}

PUBLIC int     VMThread::RunInstruction() {
    // #define VM_DEBUG_INSTRUCTIONS 1

    // NOTE: MSVC cannot take advantage of the dispatch table.
    #ifdef USING_VM_DISPATCH_TABLE
        #define VM_ADD_DISPATCH(op) &&START_ ## op
        #define VM_ADD_DISPATCH_NULL(op) NULL
        static const void* dispatch_table[] = {
            VM_ADD_DISPATCH_NULL(OP_ERROR),
            VM_ADD_DISPATCH(OP_CONSTANT),
            VM_ADD_DISPATCH(OP_DEFINE_GLOBAL),
            VM_ADD_DISPATCH(OP_GET_PROPERTY),
            VM_ADD_DISPATCH(OP_SET_PROPERTY),
            VM_ADD_DISPATCH(OP_GET_GLOBAL),
            VM_ADD_DISPATCH(OP_SET_GLOBAL),
            VM_ADD_DISPATCH(OP_GET_LOCAL),
            VM_ADD_DISPATCH(OP_SET_LOCAL),
            VM_ADD_DISPATCH(OP_PRINT_STACK),
            VM_ADD_DISPATCH(OP_INHERIT),
            VM_ADD_DISPATCH(OP_RETURN),
            VM_ADD_DISPATCH(OP_METHOD),
            VM_ADD_DISPATCH(OP_CLASS),
            VM_ADD_DISPATCH(OP_CALL),
            VM_ADD_DISPATCH_NULL(OP_SUPER),
            VM_ADD_DISPATCH(OP_INVOKE),
            VM_ADD_DISPATCH(OP_JUMP),
            VM_ADD_DISPATCH(OP_JUMP_IF_FALSE),
            VM_ADD_DISPATCH(OP_JUMP_BACK),
            VM_ADD_DISPATCH(OP_POP),
            VM_ADD_DISPATCH(OP_COPY),
            VM_ADD_DISPATCH(OP_ADD),
            VM_ADD_DISPATCH(OP_SUBTRACT),
            VM_ADD_DISPATCH(OP_MULTIPLY),
            VM_ADD_DISPATCH(OP_DIVIDE),
            VM_ADD_DISPATCH(OP_MODULO),
            VM_ADD_DISPATCH(OP_NEGATE),
            VM_ADD_DISPATCH(OP_INCREMENT),
            VM_ADD_DISPATCH(OP_DECREMENT),
            VM_ADD_DISPATCH(OP_BITSHIFT_LEFT),
            VM_ADD_DISPATCH(OP_BITSHIFT_RIGHT),
            VM_ADD_DISPATCH(OP_NULL),
            VM_ADD_DISPATCH(OP_TRUE),
            VM_ADD_DISPATCH(OP_FALSE),
            VM_ADD_DISPATCH(OP_BW_NOT),
            VM_ADD_DISPATCH(OP_BW_AND),
            VM_ADD_DISPATCH(OP_BW_OR),
            VM_ADD_DISPATCH(OP_BW_XOR),
            VM_ADD_DISPATCH(OP_LG_NOT),
            VM_ADD_DISPATCH(OP_LG_AND),
            VM_ADD_DISPATCH(OP_LG_OR),
            VM_ADD_DISPATCH(OP_EQUAL),
            VM_ADD_DISPATCH(OP_EQUAL_NOT),
            VM_ADD_DISPATCH(OP_GREATER),
            VM_ADD_DISPATCH(OP_GREATER_EQUAL),
            VM_ADD_DISPATCH(OP_LESS),
            VM_ADD_DISPATCH(OP_LESS_EQUAL),
            VM_ADD_DISPATCH(OP_PRINT),
            VM_ADD_DISPATCH_NULL(OP_ENUM),
            VM_ADD_DISPATCH(OP_SAVE_VALUE),
            VM_ADD_DISPATCH(OP_LOAD_VALUE),
            VM_ADD_DISPATCH(OP_WITH),
            VM_ADD_DISPATCH(OP_GET_ELEMENT),
            VM_ADD_DISPATCH(OP_SET_ELEMENT),
            VM_ADD_DISPATCH(OP_NEW_ARRAY),
            VM_ADD_DISPATCH(OP_NEW_MAP),
            VM_ADD_DISPATCH(OP_SWITCH_TABLE),
            VM_ADD_DISPATCH(OP_FAILSAFE),
            VM_ADD_DISPATCH(OP_EVENT),
            VM_ADD_DISPATCH(OP_TYPEOF),
            VM_ADD_DISPATCH(OP_NEW),
            VM_ADD_DISPATCH(OP_IMPORT),
            VM_ADD_DISPATCH_NULL(OP_SYNC),
        };
        #define VM_START(ins) goto *dispatch_table[(ins)];
        #define VM_END() dispatch_end:
        #define VM_CASE(n) START_ ## n
        #define VM_BREAK goto dispatch_end;
    #else
        #define VM_START(ins) switch ((ins))
        #define VM_END() ;
        #define VM_CASE(n) case n
        #define VM_BREAK break
    #endif

    CallFrame* frame;
    Uint8 instruction;

    frame = &Frames[FrameCount - 1];
    frame->IPLast = frame->IP;

    #ifdef VM_DEBUG_INSTRUCTIONS
        DebugInfo = false;
        if (DebugInfo) {
            #define PRINT_CASE(n) case n: Log::Print(Log::LOG_VERBOSE, #n); break;

            switch (*frame->IP) {
                PRINT_CASE(OP_ERROR)
                PRINT_CASE(OP_CONSTANT)
                PRINT_CASE(OP_DEFINE_GLOBAL)
                PRINT_CASE(OP_GET_PROPERTY)
                PRINT_CASE(OP_SET_PROPERTY)
                PRINT_CASE(OP_GET_GLOBAL)
                PRINT_CASE(OP_SET_GLOBAL)
                PRINT_CASE(OP_GET_LOCAL)
                PRINT_CASE(OP_SET_LOCAL)
                PRINT_CASE(OP_PRINT_STACK)
                PRINT_CASE(OP_INHERIT)
                PRINT_CASE(OP_RETURN)
                PRINT_CASE(OP_METHOD)
                PRINT_CASE(OP_CLASS)
                PRINT_CASE(OP_CALL)
                PRINT_CASE(OP_SUPER)
                PRINT_CASE(OP_INVOKE)
                PRINT_CASE(OP_JUMP)
                PRINT_CASE(OP_JUMP_IF_FALSE)
                PRINT_CASE(OP_JUMP_BACK)
                PRINT_CASE(OP_POP)
                PRINT_CASE(OP_COPY)
                PRINT_CASE(OP_ADD)
                PRINT_CASE(OP_SUBTRACT)
                PRINT_CASE(OP_MULTIPLY)
                PRINT_CASE(OP_DIVIDE)
                PRINT_CASE(OP_MODULO)
                PRINT_CASE(OP_NEGATE)
                PRINT_CASE(OP_INCREMENT)
                PRINT_CASE(OP_DECREMENT)
                PRINT_CASE(OP_BITSHIFT_LEFT)
                PRINT_CASE(OP_BITSHIFT_RIGHT)
                PRINT_CASE(OP_NULL)
                PRINT_CASE(OP_TRUE)
                PRINT_CASE(OP_FALSE)
                PRINT_CASE(OP_BW_NOT)
                PRINT_CASE(OP_BW_AND)
                PRINT_CASE(OP_BW_OR)
                PRINT_CASE(OP_BW_XOR)
                PRINT_CASE(OP_LG_NOT)
                PRINT_CASE(OP_LG_AND)
                PRINT_CASE(OP_LG_OR)
                PRINT_CASE(OP_EQUAL)
                PRINT_CASE(OP_EQUAL_NOT)
                PRINT_CASE(OP_GREATER)
                PRINT_CASE(OP_GREATER_EQUAL)
                PRINT_CASE(OP_LESS)
                PRINT_CASE(OP_LESS_EQUAL)
                PRINT_CASE(OP_PRINT)
                PRINT_CASE(OP_ENUM)
                PRINT_CASE(OP_SAVE_VALUE)
                PRINT_CASE(OP_LOAD_VALUE)
                PRINT_CASE(OP_WITH)
                PRINT_CASE(OP_GET_ELEMENT)
                PRINT_CASE(OP_SET_ELEMENT)
                PRINT_CASE(OP_NEW_ARRAY)
                PRINT_CASE(OP_NEW_MAP)
                PRINT_CASE(OP_SWITCH_TABLE)
                PRINT_CASE(OP_TYPEOF)
                PRINT_CASE(OP_NEW)
                PRINT_CASE(OP_IMPORT)

                default:
                    Log::Print(Log::LOG_ERROR, "Unknown opcode %d\n", frame->IP); break;
            }

            #undef  PRINT_CASE
        }
    #endif

    VM_START(instruction = ReadByte(frame)) {
        // Globals (heap)
        VM_CASE(OP_GET_GLOBAL): {
            Uint32 hash = ReadUInt32(frame);
            if (BytecodeObjectManager::Lock()) {
                if (!BytecodeObjectManager::Globals->Exists(hash)) {
                    if (__Tokens__ && __Tokens__->Exists(hash)) {
                        if (ThrowRuntimeError(false, "Variable \"%s\" does not exist.", __Tokens__->Get(hash)) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_GLOBAL;
                    }
                    else {
                        if (ThrowRuntimeError(false, "Variable $[%08X] does not exist.", hash) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_GLOBAL;
                    }
                    Push(NULL_VAL);
                    return INTERPRET_GLOBAL_DOES_NOT_EXIST;
                }

                Push(BytecodeObjectManager::DelinkValue(BytecodeObjectManager::Globals->Get(hash)));
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;

            FAIL_OP_GET_GLOBAL:
            BytecodeObjectManager::Unlock();
            Push(NULL_VAL);
            VM_BREAK;
        }
        VM_CASE(OP_SET_GLOBAL): {
            Uint32 hash = ReadUInt32(frame);
            if (BytecodeObjectManager::Lock()) {
                if (!BytecodeObjectManager::Globals->Exists(hash)) {
                    if (__Tokens__ && __Tokens__->Exists(hash)) {
                        if (ThrowRuntimeError(false, "Global variable \"%s\" does not exist.", __Tokens__->Get(hash)) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_SET_GLOBAL;
                    }
                    else {
                        if (ThrowRuntimeError(false, "Global variable $[%08X] does not exist.", hash) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_SET_GLOBAL;
                    }
                    return INTERPRET_GLOBAL_DOES_NOT_EXIST;
                }

                VMValue LHS = BytecodeObjectManager::Globals->Get(hash);
                VMValue value = Peek(0);
                switch (LHS.Type) {
                    case VAL_LINKED_INTEGER: {
                        VMValue result = BytecodeObjectManager::CastValueAsInteger(value);
                        if (IS_NULL(result)) {
                            // Conversion failed
                            if (ThrowRuntimeError(false, "Expected value to be of type %s instead of %s.", "Integer", GetTypeString(value)) == ERROR_RES_CONTINUE)
                                goto FAIL_OP_SET_PROPERTY;
                        }
                        AS_LINKED_INTEGER(LHS) = AS_INTEGER(result);
                        break;
                    }
                    case VAL_LINKED_DECIMAL: {
                        VMValue result = BytecodeObjectManager::CastValueAsDecimal(value);
                        if (IS_NULL(result)) {
                            // Conversion failed
                            if (ThrowRuntimeError(false, "Expected value to be of type %s instead of %s.", "Decimal", GetTypeString(value)) == ERROR_RES_CONTINUE)
                                goto FAIL_OP_SET_PROPERTY;
                        }
                        AS_LINKED_DECIMAL(LHS) = AS_DECIMAL(result);
                        break;
                    }
                    default:
                        BytecodeObjectManager::Globals->Put(hash, value);
                }
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;

            FAIL_OP_SET_GLOBAL:
            BytecodeObjectManager::Unlock();
            VM_BREAK;
        }
        VM_CASE(OP_DEFINE_GLOBAL): {
            Uint32 hash = ReadUInt32(frame);
            if (BytecodeObjectManager::Lock()) {
                VMValue value = Peek(0);
                // If it already exists,
                if (BytecodeObjectManager::Globals->Exists(hash)) {
                    VMValue originalValue = BytecodeObjectManager::Globals->Get(hash);

                    // If the value is a class and original is a class,
                    if (IS_CLASS(value) && IS_CLASS(originalValue)) {
                        ObjClass* src = AS_CLASS(value);
                        ObjClass* dst = AS_CLASS(originalValue);

                        src->Methods->WithAll([dst](Uint32 hash, VMValue value) -> void {
                            dst->Methods->Put(hash, value);
                        });
                        src->Methods->Clear();
                    }
                    // Otherwise,
                    else {
                        BytecodeObjectManager::Globals->Put(hash, value);
                    }
                }
                // Otherwise,
                else {
                    BytecodeObjectManager::Globals->Put(hash, value);
                }
                Pop();
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;
        }

        // Object Properties (heap)
        VM_CASE(OP_GET_PROPERTY): {
            Uint32 hash = ReadUInt32(frame);

            VMValue object = Peek(0);

            // If it's an instance,
            if (IS_INSTANCE(object)) {
                ObjInstance* instance = AS_INSTANCE(object);

                if (BytecodeObjectManager::Lock()) {
                    // Fields have priority over methods
                    if (instance->Fields->Exists(hash)) {
                        Pop();
                        Push(BytecodeObjectManager::DelinkValue(instance->Fields->Get(hash)));
                        BytecodeObjectManager::Unlock();
                        VM_BREAK;
                    }

                    ObjClass* klass = instance->Class;
                    if (klass->ParentHash && !klass->Parent) {
                        BytecodeObjectManager::SetClassParent(klass);
                    }

                    if (GetMethod(instance->Class, hash)) {
                        BytecodeObjectManager::Unlock();
                        VM_BREAK;
                    }

                    if (__Tokens__ && __Tokens__->Exists(hash)) {
                        if (ThrowRuntimeError(false, "Could not find %s in instance!", __Tokens__->Get(hash)) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_PROPERTY;
                    }
                    else {
                        if (ThrowRuntimeError(false, "Could not find %X in instance!", hash) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_PROPERTY;
                    }
                    goto FAIL_OP_GET_PROPERTY;
                }
            }
            // Otherwise, if it's a class,
            else if (IS_CLASS(object)) {
                ObjClass* klass = AS_CLASS(object);

                if (BytecodeObjectManager::Lock()) {
                    // Fields have priority over methods
                    if (klass->Fields->Exists(hash)) {
                        Pop();
                        Push(BytecodeObjectManager::DelinkValue(klass->Fields->Get(hash)));
                        BytecodeObjectManager::Unlock();
                        VM_BREAK;
                    }

                    if (klass->ParentHash && !klass->Parent) {
                        BytecodeObjectManager::SetClassParent(klass);
                    }

                    if (GetMethod(klass, hash)) {
                        BytecodeObjectManager::Unlock();
                        VM_BREAK;
                    }

                    if (__Tokens__ && __Tokens__->Exists(hash)) {
                        if (ThrowRuntimeError(false, "Could not find %s in class!", __Tokens__->Get(hash)) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_PROPERTY;
                    }
                    else {
                        if (ThrowRuntimeError(false, "Could not find %X in class!", hash) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_PROPERTY;
                    }
                    goto FAIL_OP_GET_PROPERTY;
                }
            }
            else {
                if (ThrowRuntimeError(false, "Only instances and classes have properties.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_GET_PROPERTY;
            }
            VM_BREAK;

            FAIL_OP_GET_PROPERTY:
            Pop();
            Push(NULL_VAL);
            BytecodeObjectManager::Unlock();
            VM_BREAK;
        }
        VM_CASE(OP_SET_PROPERTY): {
            Uint32 hash;
            VMValue field;
            VMValue value;
            VMValue result;
            VMValue object;
            Table* fields;

            object = Peek(1);

            if (IS_INSTANCE(object)) {
                ObjInstance* instance = AS_INSTANCE(object);
                fields = instance->Fields;
            }
            else if (IS_CLASS(object)) {
                ObjClass* klass = AS_CLASS(object);
                fields = klass->Fields;
            }
            else {
                if (ThrowRuntimeError(false, "Only instances and classes have properties.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_SET_PROPERTY;
            }

            if (BytecodeObjectManager::Lock()) {
                hash = ReadUInt32(frame);

                value = Pop();
                if (fields->Exists(hash)) {
                    field = fields->Get(hash);
                    switch (field.Type) {
                        case VAL_LINKED_INTEGER:
                            result = BytecodeObjectManager::CastValueAsInteger(value);
                            if (IS_NULL(result)) {
                                // Conversion failed
                                if (ThrowRuntimeError(false, "Expected value to be of type %s instead of %s.", "Integer", GetTypeString(value)) == ERROR_RES_CONTINUE)
                                    goto FAIL_OP_SET_PROPERTY;
                            }
                            AS_LINKED_INTEGER(field) = AS_INTEGER(result);
                            break;
                        case VAL_LINKED_DECIMAL:
                            result = BytecodeObjectManager::CastValueAsDecimal(value);
                            if (IS_NULL(result)) {
                                // Conversion failed
                                if (ThrowRuntimeError(false, "Expected value to be of type %s instead of %s.", "Decimal", GetTypeString(value)) == ERROR_RES_CONTINUE)
                                    goto FAIL_OP_SET_PROPERTY;
                            }
                            AS_LINKED_DECIMAL(field) = AS_DECIMAL(result);
                            break;
                        default:
                            fields->Put(hash, value);
                    }
                }
                else {
                    fields->Put(hash, value);
                }

                Pop(); // Instance / Class
                Push(value);
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;

            FAIL_OP_SET_PROPERTY:
            Pop();
            Pop(); // Instance / Class
            Push(NULL_VAL);
            BytecodeObjectManager::Unlock();
            VM_BREAK;
        }
        VM_CASE(OP_GET_ELEMENT): {
            VMValue at = Pop();
            VMValue obj = Pop();
            if (!IS_OBJECT(obj)) {
                if (ThrowRuntimeError(false, "Cannot get value from non-Array or non-Map.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_GET_ELEMENT;
            }
            if (IS_ARRAY(obj)) {
                if (!IS_INTEGER(at)) {
                    if (ThrowRuntimeError(false, "Cannot get value from array using non-Integer value as an index.") == ERROR_RES_CONTINUE)
                        goto FAIL_OP_GET_ELEMENT;
                }

                if (BytecodeObjectManager::Lock()) {
                    ObjArray* array = AS_ARRAY(obj);
                    int index = AS_INTEGER(at);
                    if (index < 0 || (Uint32)index >= array->Values->size()) {
                        if (ThrowRuntimeError(false, "Index %d is out of bounds of array of size %d.", index, (int)array->Values->size()) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_ELEMENT;
                    }
                    Push((*array->Values)[index]);
                    BytecodeObjectManager::Unlock();
                }
            }
            else if (IS_MAP(obj)) {
                if (!IS_STRING(at)) {
                    if (ThrowRuntimeError(false, "Cannot get value from map using non-String value as an index.") == ERROR_RES_CONTINUE)
                        goto FAIL_OP_GET_ELEMENT;
                }

                if (BytecodeObjectManager::Lock()) {
                    ObjMap* map = AS_MAP(obj);
                    char* index = AS_CSTRING(at);
                    if (!*index) {
                        if (ThrowRuntimeError(false, "Cannot find value at empty key.") == ERROR_RES_CONTINUE)
                            goto FAIL_OP_GET_ELEMENT;
                    }
                    if (!map->Values->Exists(index)) {
                        goto FAIL_OP_GET_ELEMENT;
                    }

                    Push(map->Values->Get(index));
                    BytecodeObjectManager::Unlock();
                }
            }
            else {
                if (ThrowRuntimeError(false, "Cannot get value from object that's non-Array or non-Map.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_GET_ELEMENT;
            }
            VM_BREAK;

            FAIL_OP_GET_ELEMENT:
            Push(NULL_VAL);
            BytecodeObjectManager::Unlock();
            VM_BREAK;
        }
        VM_CASE(OP_SET_ELEMENT): {
            VMValue value = Peek(0);
            VMValue at = Peek(1);
            VMValue obj = Peek(2);
            if (!IS_OBJECT(obj)) {
                if (ThrowRuntimeError(false, "Cannot set value in non-Array or non-Map.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_SET_ELEMENT;
            }

            if (IS_ARRAY(obj)) {
                if (!IS_INTEGER(at)) {
                    if (ThrowRuntimeError(false, "Cannot get value from array using non-Integer value as an index.") == ERROR_RES_CONTINUE)
                        goto FAIL_OP_SET_ELEMENT;
                }

                if (BytecodeObjectManager::Lock()) {
                    ObjArray* array = AS_ARRAY(obj);
                    int index = AS_INTEGER(at);
                    if (index < 0 || (Uint32)index >= array->Values->size()) {
                        if (ThrowRuntimeError(false, "Index %d is out of bounds of array of size %d.", index, (int)array->Values->size()) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_SET_ELEMENT;
                    }
                    (*array->Values)[index] = value;
                    BytecodeObjectManager::Unlock();
                }
            }
            else if (IS_MAP(obj)) {
                if (!IS_STRING(at)) {
                    if (ThrowRuntimeError(false, "Cannot get value from map using non-String value as an index.") == ERROR_RES_CONTINUE)
                        goto FAIL_OP_SET_ELEMENT;
                }

                if (BytecodeObjectManager::Lock()) {
                    ObjMap* map = AS_MAP(obj);
                    char* index = AS_CSTRING(at);
                    if (!*index) {
                        if (ThrowRuntimeError(false, "Cannot find value at empty key.") == ERROR_RES_CONTINUE)
                            goto FAIL_OP_SET_ELEMENT;
                    }

                    map->Values->Put(index, value);
                    map->Keys->Put(index, HeapCopyString(index, strlen(index)));
                    BytecodeObjectManager::Unlock();
                }
            }
            else {
                if (ThrowRuntimeError(false, "Cannot set value in object that's non-Array or non-Map.") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_SET_ELEMENT;
            }

            Pop(); // value
            Pop(); // at
            Pop(); // Array
            Push(value);
            VM_BREAK;

            FAIL_OP_SET_ELEMENT:
            Pop(); // value
            Pop(); // at
            Pop(); // Array
            Push(NULL_VAL);
            BytecodeObjectManager::Unlock();
            VM_BREAK;
        }

        // Locals
        VM_CASE(OP_GET_LOCAL): {
            Uint8 slot = ReadByte(frame);
            Push(frame->Slots[slot]);
            VM_BREAK;
        }
        VM_CASE(OP_SET_LOCAL): {
            Uint8 slot = ReadByte(frame);
            frame->Slots[slot] = Peek(0);
            VM_BREAK;
        }

        // Object Allocations (heap)
        VM_CASE(OP_NEW_ARRAY): {
            Uint32 count = ReadUInt32(frame);
            if (BytecodeObjectManager::Lock()) {
                ObjArray* array = NewArray();
                for (int i = count - 1; i >= 0; i--)
                    array->Values->push_back(Peek(i));
                for (int i = count - 1; i >= 0; i--)
                    Pop();
                Push(OBJECT_VAL(array));
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;
        }
        VM_CASE(OP_NEW_MAP): {
            Uint32 count = ReadUInt32(frame);
            if (BytecodeObjectManager::Lock()) {
                ObjMap* map = NewMap();
                for (int i = count - 1; i >= 0; i--) {
                    char* keystr = AS_CSTRING(Peek(i * 2 + 1));
                    map->Values->Put(keystr, Peek(i * 2));
                    map->Keys->Put(keystr, HeapCopyString(keystr, strlen(keystr)));
                }
                for (int i = count - 1; i >= 0; i--) {
                    Pop();
                    Pop();
                }
                Push(OBJECT_VAL(map));
                BytecodeObjectManager::Unlock();
            }
            VM_BREAK;
        }

        // Stack constants
        VM_CASE(OP_NULL): {
            Push(NULL_VAL);
            VM_BREAK;
        }
        VM_CASE(OP_TRUE): {
            Push(INTEGER_VAL(1));
            VM_BREAK;
        }
        VM_CASE(OP_FALSE): {
            Push(INTEGER_VAL(0));
            VM_BREAK;
        }
        VM_CASE(OP_CONSTANT): {
            Push(ReadConstant(frame));
            VM_BREAK;
        }

        // Switch statements
        VM_CASE(OP_SWITCH_TABLE): {
            Uint16 count = ReadUInt16(frame);
            VMValue switch_value = Pop();

            Uint8* end = frame->IP + ((1 + 2) * count) + 3;

            int default_offset = -1;
            for (int i = 0; i < count; i++) {
                int constant_index = ReadByte(frame);
                int offset = ReadUInt16(frame);
                if (constant_index == 0xFF) {
                    default_offset = offset;
                }
                else {
                    VMValue constant = (*frame->Function->Chunk.Constants)[constant_index];
                    if (BytecodeObjectManager::ValuesSortaEqual(switch_value, constant)) {
                        frame->IP = end + offset;
                        goto JUMPED;
                    }
                }
            }

            if (default_offset > -1) {
                frame->IP = end + default_offset;
            }

            JUMPED:
            VM_BREAK;
        }

        // Stack Operations
        VM_CASE(OP_POP): {
            Pop();
            VM_BREAK;
        }
        VM_CASE(OP_COPY): {
            Uint8 count = ReadByte(frame);
            for (int i = 0; i < count; i++)
                Push(Peek(count - 1));
            VM_BREAK;
        }
        VM_CASE(OP_SAVE_VALUE): {
            RegisterValue = Pop();
            VM_BREAK;
        }
        VM_CASE(OP_LOAD_VALUE): {
            Push(RegisterValue);
            VM_BREAK;
        }
        VM_CASE(OP_PRINT): {
            VMValue v = Peek(0);

            char* textBuffer = (char*)malloc(64);

            PrintBuffer buffer;
            buffer.Buffer = &textBuffer;
            buffer.WriteIndex = 0;
            buffer.BufferSize = 64;
            Values::PrintValue(&buffer, v);

            Log::Print(Log::LOG_INFO, textBuffer);

            free(textBuffer);

            Pop();
            VM_BREAK;
        }
        VM_CASE(OP_PRINT_STACK): {
            PrintStack();
            VM_BREAK;
        }

        // Frame stuffs & Returning
        VM_CASE(OP_RETURN): {
            VMValue result = Pop();

            FrameCount--;
            if (FrameCount == ReturnFrame) {
                return INTERPRET_FINISHED;
            }

            StackTop = frame->Slots;
            Push(result);

            frame = &Frames[FrameCount - 1];
            VM_BREAK;
        }

        // Jumping
        VM_CASE(OP_JUMP): {
            Sint32 offset = ReadSInt16(frame);
            frame->IP += offset;
            VM_BREAK;
        }
        VM_CASE(OP_JUMP_BACK): {
            Sint32 offset = ReadSInt16(frame);
            frame->IP -= offset;
            VM_BREAK;
        }
        VM_CASE(OP_JUMP_IF_FALSE): {
            Sint32 offset = ReadSInt16(frame);
            if (BytecodeObjectManager::ValueFalsey(Peek(0))) {
                frame->IP += offset;
            }
            VM_BREAK;
        }

        // Numeric Operations
        VM_CASE(OP_ADD):            Push(Values_Plus());  VM_BREAK;
        VM_CASE(OP_SUBTRACT):       Push(Values_Minus());  VM_BREAK;
        VM_CASE(OP_MULTIPLY):       Push(Values_Multiply());  VM_BREAK;
        VM_CASE(OP_DIVIDE):         Push(Values_Division());  VM_BREAK;
        VM_CASE(OP_MODULO):         Push(Values_Modulo());  VM_BREAK;
        VM_CASE(OP_NEGATE):         Push(Values_Negate());  VM_BREAK;
        VM_CASE(OP_INCREMENT):      Push(Values_Increment()); VM_BREAK;
        VM_CASE(OP_DECREMENT):      Push(Values_Decrement()); VM_BREAK;
        // Bit Operations
        VM_CASE(OP_BITSHIFT_LEFT):  Push(Values_BitwiseLeft());  VM_BREAK;
        VM_CASE(OP_BITSHIFT_RIGHT): Push(Values_BitwiseRight());  VM_BREAK;
        // Bitwise Operations
        VM_CASE(OP_BW_NOT):         Push(Values_BitwiseNOT());  VM_BREAK;
        VM_CASE(OP_BW_AND):         Push(Values_BitwiseAnd());  VM_BREAK;
        VM_CASE(OP_BW_OR):          Push(Values_BitwiseOr());  VM_BREAK;
        VM_CASE(OP_BW_XOR):         Push(Values_BitwiseXor());  VM_BREAK;
        // Logical Operations
        VM_CASE(OP_LG_NOT):         Push(Values_LogicalNOT());  VM_BREAK;
        VM_CASE(OP_LG_AND):         Push(Values_LogicalAND()); VM_BREAK;
        VM_CASE(OP_LG_OR):          Push(Values_LogicalOR()); VM_BREAK;
        // Equality and Comparison Operators
        VM_CASE(OP_EQUAL):          Push(INTEGER_VAL(BytecodeObjectManager::ValuesSortaEqual(Pop(), Pop()))); VM_BREAK;
        VM_CASE(OP_EQUAL_NOT):      Push(INTEGER_VAL(!BytecodeObjectManager::ValuesSortaEqual(Pop(), Pop()))); VM_BREAK;
        VM_CASE(OP_LESS):           Push(Values_LessThan()); VM_BREAK;
        VM_CASE(OP_GREATER):        Push(Values_GreaterThan()); VM_BREAK;
        VM_CASE(OP_LESS_EQUAL):     Push(Values_LessThanOrEqual()); VM_BREAK;
        VM_CASE(OP_GREATER_EQUAL):  Push(Values_GreaterThanOrEqual()); VM_BREAK;
        // typeof Operator
        VM_CASE(OP_TYPEOF):         Push(Value_TypeOf()); VM_BREAK;

        // Functions
        VM_CASE(OP_WITH): {
            enum {
                WITH_STATE_INIT,
                WITH_STATE_ITERATE,
                WITH_STATE_FINISH,
            };

            int state = ReadByte(frame);
            int offset = ReadSInt16(frame);

            switch (state) {
                case WITH_STATE_INIT: {
                    VMValue receiver = Peek(0);
                    if (receiver.Type == VAL_NULL) {
                        frame->IP += offset;
                        Pop(); // pop receiver
                        break;
                    }
                    else if (IS_STRING(receiver)) {
                        // iterate through objectlist
                        char* objectNameChar = AS_CSTRING(receiver);
                        ObjectList* objectList = NULL;
                        ObjectRegistry* registry = NULL;
                        if (Scene::ObjectRegistries->Exists(objectNameChar))
                            registry = Scene::ObjectRegistries->Get(objectNameChar);
                        else if (Scene::ObjectLists->Exists(objectNameChar))
                            objectList = Scene::ObjectLists->Get(objectNameChar);

                        Pop(); // pop receiver

                        if (!objectList && !registry) {
                            frame->IP += offset;
                            break;
                        }

                        BytecodeObject* objectStart = NULL;
                        int startIndex = 0;

                        // If in list,
                        if (objectList) {
                            if (objectList->Count() == 0) {
                                frame->IP += offset;
                                break;
                            }

                            for (Entity* ent = objectList->EntityFirst; ent; ent = ent->NextEntityInList) {
                                if (ent->Active && ent->Interactable) {
                                    objectStart = (BytecodeObject*)ent;
                                    break;
                                }
                            }
                        }
                        // Otherwise in registry,
                        else {
                            int count = registry->Count();
                            if (count == 0) {
                                frame->IP += offset;
                                break;
                            }

                            for (int o = 0; o < count; o++) {
                                Entity* ent = registry->GetNth(o);
                                if (ent && ent->Active && ent->Interactable) {
                                    objectStart = (BytecodeObject*)ent;
                                    startIndex = o;
                                    break;
                                }
                            }
                        }

                        if (!objectStart) {
                            frame->IP += offset;
                            break;
                        }

                        // Add iterator
                        if (registry)
                            *frame->WithIteratorStackTop = NEW_STRUCT_MACRO(WithIter) { NULL, NULL, startIndex, registry };
                        else
                            *frame->WithIteratorStackTop = NEW_STRUCT_MACRO(WithIter) { objectStart, objectStart->NextEntityInList, 0, NULL };
                        frame->WithIteratorStackTop++;

                        // Backup original receiver
                        BytecodeObjectManager::Globals->Put("other", frame->Slots[0]);
                        *frame->WithReceiverStackTop = frame->Slots[0];
                        frame->WithReceiverStackTop++;
                        // Replace receiver
                        frame->Slots[0] = OBJECT_VAL(objectStart->Instance);
                        BytecodeObjectManager::Globals->Put("this", frame->Slots[0]);
                        break;
                    }
                    else if (IS_INSTANCE(receiver)) {
                        // Backup original receiver
                        BytecodeObjectManager::Globals->Put("other", frame->Slots[0]);
                        *frame->WithReceiverStackTop = frame->Slots[0];
                        frame->WithReceiverStackTop++;
                        // Replace receiver
                        frame->Slots[0] = receiver;
                        BytecodeObjectManager::Globals->Put("this", frame->Slots[0]);

                        Pop(); // pop receiver

                        // Add dummy iterator
                        *frame->WithIteratorStackTop = NEW_STRUCT_MACRO(WithIter) { NULL, NULL, 0, NULL };
                        frame->WithIteratorStackTop++;
                        break;
                    }
                    break;
                }
                case WITH_STATE_ITERATE: {
                    VMValue originalReceiver = frame->WithReceiverStackTop[-1];
                    // Restore original receiver
                    frame->Slots[0] = originalReceiver;

                    WithIter it = frame->WithIteratorStackTop[-1];

                    // If in list,
                    if (it.entity) {
                        Entity* objectNext = NULL;
                        for (Entity* ent = (Entity*)it.entityNext; ent; ent = ent->NextEntityInList) {
                            if (ent->Active && ent->Interactable) {
                                objectNext = (BytecodeObject*)ent;
                                break;
                            }
                        }

                        if (objectNext) {
                            it.entity = objectNext;
                            it.entityNext = objectNext->NextEntityInList;

                            frame->IP -= offset;

                            // Put iterator back onto stack
                            frame->WithIteratorStackTop[-1] = it;

                            // Backup original receiver
                            frame->WithReceiverStackTop[-1] = originalReceiver;
                            // Replace receiver
                            BytecodeObject* object = (BytecodeObject*)it.entity;
                            frame->Slots[0] = OBJECT_VAL(object->Instance);
                            BytecodeObjectManager::Globals->Put("this", frame->Slots[0]);
                        }
                    }
                    // Otherwise in registry,
                    else if (it.registry) {
                        ObjectRegistry* registry = (ObjectRegistry*)it.registry;
                        if (++it.index < registry->Count()) {
                            frame->IP -= offset;

                            // Put iterator back onto stack
                            frame->WithIteratorStackTop[-1] = it;

                            // Backup original receiver
                            frame->WithReceiverStackTop[-1] = originalReceiver;
                            // Replace receiver
                            BytecodeObject* object = (BytecodeObject*)registry->GetNth(it.index);
                            frame->Slots[0] = OBJECT_VAL(object->Instance);
                            BytecodeObjectManager::Globals->Put("this", frame->Slots[0]);
                        }
                    }
                    else {
                        Log::Print(Log::LOG_ERROR, "hey you might need to handle the stack here (receiverStack: %d, iterator: %d)", frame->WithReceiverStackTop - frame->WithReceiverStack, frame->WithIteratorStackTop - frame->WithIteratorStack);
                        PrintStack();
                        assert(false);
                    }
                    break;
                }
                case WITH_STATE_FINISH: {
                    frame->WithReceiverStackTop--;

                    VMValue originalReceiver = *frame->WithReceiverStackTop;
                    frame->Slots[0] = originalReceiver;

                    frame->WithIteratorStackTop--;

                    BytecodeObjectManager::Globals->Remove("this");
                    break;
                }
            }
            VM_BREAK;
        }
        VM_CASE(OP_CALL): {
            int argCount = ReadByte(frame);
            if (!CallValue(Peek(argCount), argCount)) {
                if (ThrowRuntimeError(false, "Could not call value!") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_CALL;
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &Frames[FrameCount - 1];
            VM_BREAK;

            FAIL_OP_CALL:
            VM_BREAK;
        }
        VM_CASE(OP_INVOKE): {
            Uint32 argCount = ReadByte(frame);
            Uint32 hash = ReadUInt32(frame);
            Uint32 isSuper = ReadByte(frame);

            VMValue receiver = Peek(argCount);
            if (IS_CLASS(receiver)) {
                ObjClass* klass = AS_CLASS(receiver);
                if (klass->Methods->Exists(hash)) {
                    VMValue nat = klass->Methods->Get(hash);
                    if (IS_NATIVE(nat)) {
                        if (!CallValue(nat, argCount)) {
                            if (ThrowRuntimeError(false, "Could not call value!", hash) == ERROR_RES_CONTINUE)
                                goto FAIL_OP_INVOKE;

                            return INTERPRET_RUNTIME_ERROR;
                        }

                        // // Get returned value
                        // VMValue returned = Pop();
                        // // Pop class value off the stack.
                        // Pop();
                        // // Put returned value on top of stack.
                        // Push(returned);
                    }
                    else {
                        // ThrowRuntimeError(true, "Cannot get non-native function value from class.");
                        // return INTERPRET_RUNTIME_ERROR;
                        // StackTop[-argCount - 1] = receiver;
                        if (!CallValue(nat, argCount)) {
                            if (ThrowRuntimeError(false, "Could not call value!", hash) == ERROR_RES_CONTINUE)
                                goto FAIL_OP_INVOKE;

                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                }
                else {
                    if (__Tokens__ && __Tokens__->Exists(hash))
                        if (ThrowRuntimeError(false, "Event \"%s\" does not exist in class %s.", __Tokens__->Get(hash), __Tokens__->Get(klass->Hash)) == ERROR_RES_CONTINUE)
                            goto FAIL_OP_INVOKE;

                    return INTERPRET_RUNTIME_ERROR;
                }
                VM_BREAK;
            }

            if (!Invoke(hash, argCount, isSuper)) {
                if (__Tokens__ && __Tokens__->Exists(hash))
                    if (ThrowRuntimeError(false, "Could not invoke %s value!", __Tokens__->Get(hash)) == ERROR_RES_CONTINUE)
                        goto FAIL_OP_INVOKE;

                return INTERPRET_RUNTIME_ERROR;
            }

            frame = &Frames[FrameCount - 1];
            VM_BREAK;

            FAIL_OP_INVOKE:
            VM_BREAK;
        }
        VM_CASE(OP_CLASS): {
            Uint32 hash = ReadUInt32(frame);
            ObjClass* klass = NewClass(hash);
            klass->Extended = ReadByte(frame);

            if (!__Tokens__ || !__Tokens__->Exists(hash)) {
                char name[9];
                snprintf(name, sizeof(name), "%8X", hash);
                klass->Name = CopyString(name);
            }
            else {
                char* t = __Tokens__->Get(hash);
                klass->Name = CopyString(t);
            }

            Push(OBJECT_VAL(klass));
            VM_BREAK;
        }
        VM_CASE(OP_INHERIT): {
            ObjClass* klass = AS_CLASS(Peek(0));
            Uint32 hashSuper = ReadUInt32(frame);
            klass->ParentHash = hashSuper;
            VM_BREAK;
        }
        VM_CASE(OP_NEW): {
            int argCount = ReadByte(frame);
            if (!InstantiateClass(Peek(argCount), argCount)) {
                if (ThrowRuntimeError(false, "Could not instantiate class!") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_NEW;
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &Frames[FrameCount - 1];
            VM_BREAK;

            FAIL_OP_NEW:
            VM_BREAK;
        }
        VM_CASE(OP_EVENT): {
            int index = ReadByte(frame);
            VMValue method = OBJECT_VAL(BytecodeObjectManager::AllFunctionList[frame->FunctionListOffset + index]);
            Push(method);
            VM_BREAK;
        }
        VM_CASE(OP_METHOD): {
            int index = ReadByte(frame);
            Uint32 hash = ReadUInt32(frame);
            BytecodeObjectManager::DefineMethod(frame->FunctionListOffset + index, hash);
            VM_BREAK;
        }

        VM_CASE(OP_IMPORT): {
            VMValue value = ReadConstant(frame);
            if (!Import(value)) {
                if (ThrowRuntimeError(false, "Could not import class!") == ERROR_RES_CONTINUE)
                    goto FAIL_OP_IMPORT;
                return INTERPRET_RUNTIME_ERROR;
            }

            FAIL_OP_IMPORT:
            VM_BREAK;
        }

        VM_CASE(OP_FAILSAFE): {
            int offset = ReadUInt16(frame);
            frame->Function->Chunk.Failsafe = frame->IPStart + offset;
            // frame->IP = frame->IPStart + offset;
            VM_BREAK;
        }
    }
    VM_END();

    #ifdef VM_DEBUG_INSTRUCTIONS
        if (DebugInfo) {
            Log::Print(Log::LOG_WARN, "START");
            PrintStack();
        }
    #endif

    return INTERPRET_OK;
}
PUBLIC void    VMThread::RunInstructionSet() {
    while (true) {
        // if (!BytecodeObjectManager::Lock()) break;

        int ret;
        if ((ret = RunInstruction()) < INTERPRET_OK) {
            if (ret < INTERPRET_FINISHED)
                Log::Print(Log::LOG_ERROR, "ret: %d!!!!!!!!!!!!\n", ret);
            // BytecodeObjectManager::Unlock();
            break;
        }
        // BytecodeObjectManager::Unlock();
    }
}
// #endregion

PUBLIC void    VMThread::RunValue(VMValue value, int argCount) {
    int lastReturnFrame = ReturnFrame;

    ReturnFrame = FrameCount;
    CallValue(value, argCount);
    RunInstructionSet();
    ReturnFrame = lastReturnFrame;
}
PUBLIC void    VMThread::RunFunction(ObjFunction* func, int argCount) {
    VMValue* lastStackTop = StackTop;
    int lastReturnFrame = ReturnFrame;

    ReturnFrame = FrameCount;

    Call(func, argCount);
    RunInstructionSet();

    ReturnFrame = lastReturnFrame;
    StackTop = lastStackTop;
}
PUBLIC void    VMThread::InvokeForEntity(VMValue value, int argCount) {
    VMValue* lastStackTop = StackTop;
    int      lastReturnFrame = ReturnFrame;

    FunctionToInvoke = value;
    ReturnFrame = FrameCount;

    if (CallValue(value, argCount)) {
        switch (OBJECT_TYPE(value)) {
            case OBJ_BOUND_METHOD:
            case OBJ_FUNCTION:
                RunInstructionSet();
            default:
                // Do nothing for native functions
                break;
        }
    }

    FunctionToInvoke = NULL_VAL;
    ReturnFrame = lastReturnFrame;
    StackTop = lastStackTop;
}
PUBLIC void    VMThread::RunEntityFunction(ObjFunction* function, int argCount) {
    VMValue* lastStackTop = StackTop;
    int      lastReturnFrame = ReturnFrame;

    FunctionToInvoke = OBJECT_VAL(function);
    ReturnFrame = FrameCount;

    Call(function, argCount);
    RunInstructionSet();

    FunctionToInvoke = NULL_VAL;
    ReturnFrame = lastReturnFrame;
    StackTop = lastStackTop;
}
PUBLIC void    VMThread::CallInitializer(VMValue value) {
    FunctionToInvoke = value;

    ObjFunction* initializer = AS_FUNCTION(value);
    if (initializer->Arity != 0) {
        ThrowRuntimeError(false, "Initializer must have no parameters.");
    }
    else {
        VMValue* lastStackTop = StackTop;
        int      lastReturnFrame = ReturnFrame;

        ReturnFrame = FrameCount;

        RunFunction(initializer, 0);

        ReturnFrame = lastReturnFrame;
        StackTop = lastStackTop;
    }

    FunctionToInvoke = NULL_VAL;
}

PUBLIC bool    VMThread::GetMethod(ObjClass* klass, Uint32 hash) {
    if (BytecodeObjectManager::Lock()) {
        VMValue method;
        if (klass->Methods->Exists(hash)) {
            method = klass->Methods->Get(hash);
        }
        else if (klass->Parent && klass->Parent->Methods->Exists(hash)) {
            method = klass->Parent->Methods->Get(hash);
        }
        else {
            if (!__Tokens__ || !__Tokens__->Exists(hash))
                ThrowRuntimeError(false, "Undefined property $[%08X].", hash);
            else
                ThrowRuntimeError(false, "Undefined property \"%s\".", __Tokens__->Get(hash));
            Pop(); // Instance.
            Push(NULL_VAL);
            return false;
        }

        Pop(); // Instance.
        Push(method);
        BytecodeObjectManager::Unlock();
        return true;
    }
    return false;
}
PUBLIC bool    VMThread::BindMethod(VMValue receiver, VMValue method) {
    ObjBoundMethod* bound = NewBoundMethod(receiver, AS_FUNCTION(method));
    Push(OBJECT_VAL(bound));
    return true;
}
PUBLIC bool    VMThread::CallValue(VMValue callee, int argCount) {
    if (BytecodeObjectManager::Lock()) {
        bool result;
        if (IS_OBJECT(callee)) {
            switch (OBJECT_TYPE(callee)) {
                case OBJ_BOUND_METHOD: {
                    ObjBoundMethod* bound = AS_BOUND_METHOD(callee);

                    // if (__Tokens__ && __Tokens__->Exists(bound->Method->NameHash))
                    //     printf("Calling Bound Method: %s, argCount: %d\n", __Tokens__->Get(bound->Method->NameHash), argCount);

                    // Replace the bound method with the receiver so it's in the
                    // right slot when the method is called.
                    StackTop[-argCount - 1] = bound->Receiver;
                    result = Call(bound->Method, argCount);
                    BytecodeObjectManager::Unlock();
                    return result;
                }
                case OBJ_FUNCTION:
                    result = Call(AS_FUNCTION(callee), argCount);
                    BytecodeObjectManager::Unlock();
                    return result;
                case OBJ_NATIVE: {
                    NativeFn native = AS_NATIVE(callee);

                    VMValue result = NULL_VAL;

                    try {
                        result = native(argCount, StackTop - argCount, ID);
                    }
                    catch (const char* err) { }

                    // Pop arguments
                    StackTop -= argCount;
                    // Pop receiver / class
                    StackTop -= 1;
                    // Push result
                    Push(result);
                    BytecodeObjectManager::Unlock();
                    return true;
                }
                default:
                    // Do nothing.
                    break;
            }
        }
        BytecodeObjectManager::Unlock();
    }

    return false;
}
PUBLIC bool    VMThread::InstantiateClass(VMValue callee, int argCount) {
    if (BytecodeObjectManager::Lock()) {
        bool result = false;
        if (!IS_OBJECT(callee) || OBJECT_TYPE(callee) != OBJ_CLASS) {
            ThrowRuntimeError(false, "Cannot instantiate non-class.");
        }
        else {
            ObjClass* klass = AS_CLASS(callee);

            // Create the instance.
            StackTop[-argCount - 1] = OBJECT_VAL(NewInstance(klass));

            result = true;

            // Call the initializer, if there is one.
            if (HasInitializer(klass))
                result = Call(AS_FUNCTION(klass->Initializer), argCount);
            else if (argCount != 0) {
                ThrowRuntimeError(false, "Expected no arguments to initializer, got %d.", argCount);
                result = false;
            }
        }

        BytecodeObjectManager::Unlock();
        return result;
    }

    return false;
}
PUBLIC bool    VMThread::Call(ObjFunction* function, int argCount) {
    if (argCount != function->Arity) {
        if (ThrowRuntimeError(false, "Expected %d arguments to function call, got %d.", function->Arity, argCount) == ERROR_RES_CONTINUE)
            return false;
    }

    // if (__Tokens__ && __Tokens__->Exists(function->NameHash))
    //     printf("new frame '%s' at %d#%d\n", __Tokens__->Get(function->NameHash), ID, FrameCount);

    if (FrameCount == FRAMES_MAX) {
        ThrowRuntimeError(true, "Frame overflow. (Count %d / %d)", FrameCount, FRAMES_MAX);
        return false;
    }

    CallFrame* frame = &Frames[FrameCount++];
    frame->IP = function->Chunk.Code;
    frame->IPStart = frame->IP;
    frame->Function = function;
    frame->Slots = StackTop - (function->Arity + 1);
    frame->WithReceiverStackTop = frame->WithReceiverStack;
    frame->WithIteratorStackTop = frame->WithIteratorStack;
    frame->FunctionListOffset = function->FunctionListOffset;
    BytecodeObjectManager::Globals->Remove("this");

    return true;
}
PUBLIC bool    VMThread::InvokeFromClass(ObjClass* klass, Uint32 hash, int argCount) {
    if (BytecodeObjectManager::Lock()) {
        VMValue method;

        if (klass->ParentHash && !klass->Parent) {
            BytecodeObjectManager::SetClassParent(klass);
        }

        if (klass->Methods->Exists(hash)) {
            method = klass->Methods->Get(hash);
        }
        else if (klass->Parent && klass->Parent->Methods->Exists(hash)) {
            method = klass->Parent->Methods->Get(hash);
        }
        else {
            BytecodeObjectManager::Unlock();
            return false;
        }

        if (IS_NATIVE(method)) {
            NativeFn native = AS_NATIVE(method);
            VMValue result = native(argCount + 1, StackTop - argCount - 1, ID);
            StackTop -= argCount + 1;
            Push(result);
            BytecodeObjectManager::Unlock();
            return true;
        }

        BytecodeObjectManager::Unlock();
        return Call(AS_FUNCTION(method), argCount);
    }
    return false;
}
PUBLIC bool    VMThread::Invoke(Uint32 hash, int argCount, bool isSuper) {
    VMValue receiver = Peek(argCount);

    if (!IS_INSTANCE(receiver)) {
        ThrowRuntimeError(false, "Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    if (!isSuper) {
        // First look for a field which may shadow a method.
        VMValue value;
        bool exists = false;
        if (BytecodeObjectManager::Lock()) {
            if ((exists = instance->Fields->Exists(hash)))
                value = instance->Fields->Get(hash);

            BytecodeObjectManager::Unlock();
        }
        if (exists) {
            return CallValue(value, argCount);
        }
    }
    else {
        ObjClass* klass = instance->Class;
        if (klass->ParentHash && !klass->Parent) {
            BytecodeObjectManager::SetClassParent(klass);
        }

        if (instance->Class->Parent) {
            return InvokeFromClass(instance->Class->Parent, hash, argCount);
        }

        ThrowRuntimeError(false, "Instance's class does not have a parent to call method from.");
        return false;
    }
    return InvokeFromClass(instance->Class, hash, argCount);
}
PUBLIC bool    VMThread::Import(VMValue value) {
    bool result = false;
    if (BytecodeObjectManager::Lock()) {
        if (!IS_OBJECT(value) || OBJECT_TYPE(value) != OBJ_STRING) {
            ThrowRuntimeError(false, "Cannot import from a %s.", GetTypeString(value));
        }
        else {
            char* className = AS_CSTRING(value);
            if (BytecodeObjectManager::ClassExists(className)) {
                if (!BytecodeObjectManager::Classes->Exists(className))
                    BytecodeObjectManager::LoadClass(className);
                result = true;
            }
            else {
                ThrowRuntimeError(false, "Could not import %s!", className);
            }
        }
    }
    BytecodeObjectManager::Unlock();
    return result;
}

// #region Value Operations
#define IS_NOT_NUMBER(a) (a.Type != VAL_DECIMAL && a.Type != VAL_INTEGER && a.Type != VAL_LINKED_DECIMAL && a.Type != VAL_LINKED_INTEGER)
#define CHECK_IS_NUM(a, b, def) \
    if (IS_NOT_NUMBER(a)) { \
        ThrowRuntimeError(false, "Cannot perform %s operation on non-number value of type %s.", #b, GetTypeString(a)); \
        a = def; \
    }


enum {
    ASSIGNMENT_MULTIPLY,
    ASSIGNMENT_DIVISION,
    ASSIGNMENT_MODULO,
    ASSIGNMENT_PLUS,
    ASSIGNMENT_MINUS,
    ASSIGNMENT_BITWISE_LEFT,
    ASSIGNMENT_BITWISE_RIGHT,
    ASSIGNMENT_BITWISE_AND,
    ASSIGNMENT_BITWISE_XOR,
    ASSIGNMENT_BITWISE_OR,
};
// If one of the operators is a decimal, they both become one

PUBLIC VMValue VMThread::Values_Multiply() {
    VMValue b = Peek(0);
    VMValue a = Peek(1);

    CHECK_IS_NUM(a, "multiply", DECIMAL_VAL(0.0f));
    CHECK_IS_NUM(b, "multiply", DECIMAL_VAL(0.0f));

    Pop();
    Pop();

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL(a_d * b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d * b_d);
}
PUBLIC VMValue VMThread::Values_Division() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "division", DECIMAL_VAL(1.0f));
    CHECK_IS_NUM(b, "division", DECIMAL_VAL(1.0f));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        if (b_d == 0.0) {
            if (ThrowRuntimeError(false, "Cannot divide decimal by zero.") == ERROR_RES_CONTINUE)
                return DECIMAL_VAL(0.f);
        }
        return DECIMAL_VAL(a_d / b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    if (b_d == 0) {
        if (ThrowRuntimeError(false, "Cannot divide integer by zero.") == ERROR_RES_CONTINUE)
            return INTEGER_VAL(0);
    }
    return INTEGER_VAL(a_d / b_d);
}
PUBLIC VMValue VMThread::Values_Modulo() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "modulo", DECIMAL_VAL(1.0f));
    CHECK_IS_NUM(b, "modulo", DECIMAL_VAL(1.0f));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL(fmod(a_d, b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d % b_d);
}
PUBLIC VMValue VMThread::Values_Plus() {
    VMValue b = Peek(0);
    VMValue a = Peek(1);
    if (IS_STRING(a) || IS_STRING(b)) {
        if (BytecodeObjectManager::Lock()) {
            VMValue str_b = BytecodeObjectManager::CastValueAsString(b);
            VMValue str_a = BytecodeObjectManager::CastValueAsString(a);
            VMValue out = BytecodeObjectManager::Concatenate(str_a, str_b);
            Pop();
            Pop();
            BytecodeObjectManager::Unlock();
            return out;
        }
    }

    CHECK_IS_NUM(a, "plus", DECIMAL_VAL(0.0f));
    CHECK_IS_NUM(b, "plus", DECIMAL_VAL(0.0f));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        Pop();
        Pop();
        return DECIMAL_VAL(a_d + b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    Pop();
    Pop();
    return INTEGER_VAL(a_d + b_d);
}
PUBLIC VMValue VMThread::Values_Minus() {
    VMValue b = Peek(0);
    VMValue a = Peek(1);

    CHECK_IS_NUM(a, "minus", DECIMAL_VAL(0.0f));
    CHECK_IS_NUM(b, "minus", DECIMAL_VAL(0.0f));

    Pop();
    Pop();

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL(a_d - b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d - b_d);
}
PUBLIC VMValue VMThread::Values_BitwiseLeft() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "bitwise left", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "bitwise left", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL((float)((int)a_d << (int)b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d << b_d);
}
PUBLIC VMValue VMThread::Values_BitwiseRight() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "bitwise right", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "bitwise right", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL((float)((int)a_d >> (int)b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d >> b_d);
}
PUBLIC VMValue VMThread::Values_BitwiseAnd() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "bitwise and", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "bitwise and", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL((float)((int)a_d & (int)b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d & b_d);
}
PUBLIC VMValue VMThread::Values_BitwiseXor() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "xor", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "xor", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL((float)((int)a_d ^ (int)b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d ^ b_d);
}
PUBLIC VMValue VMThread::Values_BitwiseOr() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "bitwise or", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "bitwise or", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return DECIMAL_VAL((float)((int)a_d | (int)b_d));
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d | b_d);
}
PUBLIC VMValue VMThread::Values_LogicalAND() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "logical and", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "logical and", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        // float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        // float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        // return DECIMAL_VAL((float)((int)a_d & (int)b_d));
        return INTEGER_VAL(0);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d && b_d);
}
PUBLIC VMValue VMThread::Values_LogicalOR() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "logical or", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "logical or", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        // float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        // float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        // return DECIMAL_VAL((float)((int)a_d & (int)b_d));
        return INTEGER_VAL(0);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d || b_d);
}
PUBLIC VMValue VMThread::Values_LessThan() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "less than", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "less than", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return INTEGER_VAL(a_d < b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d < b_d);
}
PUBLIC VMValue VMThread::Values_GreaterThan() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "greater than", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "greater than", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return INTEGER_VAL(a_d > b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d > b_d);
}
PUBLIC VMValue VMThread::Values_LessThanOrEqual() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "less than or equal", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "less than or equal", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return INTEGER_VAL(a_d <= b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d <= b_d);
}
PUBLIC VMValue VMThread::Values_GreaterThanOrEqual() {
    VMValue b = Pop();
    VMValue a = Pop();

    CHECK_IS_NUM(a, "greater than or equal", INTEGER_VAL(0));
    CHECK_IS_NUM(b, "greater than or equal", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL || b.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(a));
        float b_d = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(b));
        return INTEGER_VAL(a_d >= b_d);
    }
    int a_d = AS_INTEGER(a);
    int b_d = AS_INTEGER(b);
    return INTEGER_VAL(a_d >= b_d);
}
PUBLIC VMValue VMThread::Values_Increment() {
    VMValue a = Pop();

    CHECK_IS_NUM(a, "increment", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(a);
        return DECIMAL_VAL(++a_d);
    }
    int a_d = AS_INTEGER(a);
    return INTEGER_VAL(++a_d);
}
PUBLIC VMValue VMThread::Values_Decrement() {
    VMValue a = Pop();

    CHECK_IS_NUM(a, "decrement", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL) {
        float a_d = AS_DECIMAL(a);
        return DECIMAL_VAL(--a_d);
    }
    int a_d = AS_INTEGER(a);
    return INTEGER_VAL(--a_d);
}
PUBLIC VMValue VMThread::Values_Negate() {
    VMValue a = Pop();

    CHECK_IS_NUM(a, "negate", INTEGER_VAL(0));

    if (a.Type == VAL_DECIMAL) {
        return DECIMAL_VAL(-AS_DECIMAL(a));
    }
    return INTEGER_VAL(-AS_INTEGER(a));
}
PUBLIC VMValue VMThread::Values_LogicalNOT() {
    VMValue a = Pop();

    // HACK: Yikes.
    switch (a.Type) {
        case VAL_NULL:
            return INTEGER_VAL(true);
        case VAL_OBJECT:
            return INTEGER_VAL(false);
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            return DECIMAL_VAL((float)(AS_DECIMAL(a) == 0.0));
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            return INTEGER_VAL(!AS_INTEGER(a));
    }

    return INTEGER_VAL(false);
}
PUBLIC VMValue VMThread::Values_BitwiseNOT() {
    VMValue a = Pop();
    if (a.Type == VAL_DECIMAL) {
        return DECIMAL_VAL((float)(~(int)AS_DECIMAL(a)));
    }
    return INTEGER_VAL(~AS_INTEGER(a));
}
PUBLIC VMValue VMThread::Value_TypeOf() {
    const char *valueType = "unknown";

    VMValue value = Pop();

    switch (value.Type) {
        case VAL_NULL:
            valueType = "null";
            break;
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER:
            valueType = "integer";
            break;
        case VAL_DECIMAL:
        case VAL_LINKED_DECIMAL:
            valueType = "decimal";
            break;
        case VAL_OBJECT: {
            switch (OBJECT_TYPE(value)) {
                case OBJ_BOUND_METHOD:
                case OBJ_FUNCTION:
                    valueType = "event";
                    break;
                case OBJ_CLASS:
                    valueType = "class";
                    break;
                case OBJ_CLOSURE:
                    valueType = "closure";
                    break;
                case OBJ_INSTANCE:
                    valueType = "instance";
                    break;
                case OBJ_NATIVE:
                    valueType = "native";
                    break;
                case OBJ_STRING:
                    valueType = "string";
                    break;
                case OBJ_UPVALUE:
                    valueType = "upvalue";
                    break;
                case OBJ_ARRAY:
                    valueType = "array";
                    break;
                case OBJ_MAP:
                    valueType = "map";
                    break;
                case OBJ_STREAM:
                    valueType = "stream";
                    break;
            }
        }
    }

    return OBJECT_VAL(CopyString(valueType));
}
// #endregion
