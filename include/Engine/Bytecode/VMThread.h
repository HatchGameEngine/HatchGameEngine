#ifndef ENGINE_BYTECODE_VMTHREAD_H
#define ENGINE_BYTECODE_VMTHREAD_H

#define PUBLIC
#define PRIVATE
#define PROTECTED
#define STATIC
#define VIRTUAL
#define EXPOSED


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

    char*   GetToken(Uint32 hash);
    int     ThrowRuntimeError(bool fatal, const char* errorMessage, ...);
    void    PrintStack();
    void    ReturnFromNative() throw();
    void    Push(VMValue value);
    VMValue Pop();
    VMValue Peek(int offset);
    Uint32  StackSize();
    void    ResetStack();
    Uint8   ReadByte(CallFrame* frame);
    Uint16  ReadUInt16(CallFrame* frame);
    Uint32  ReadUInt32(CallFrame* frame);
    Sint16  ReadSInt16(CallFrame* frame);
    Sint32  ReadSInt32(CallFrame* frame);
    VMValue ReadConstant(CallFrame* frame);
    int     RunInstruction();
    void    RunInstructionSet();
    void    RunValue(VMValue value, int argCount);
    void    RunFunction(ObjFunction* func, int argCount);
    void    RunInvoke(Uint32 hash, int argCount);
    bool    BindMethod(ObjClass* klass, Uint32 hash);
    bool    CallValue(VMValue callee, int argCount);
    bool    Call(ObjFunction* function, int argCount);
    bool    InvokeFromClass(ObjClass* klass, Uint32 hash, int argCount);
    bool    Invoke(Uint32 hash, int argCount, bool isSuper);
    VMValue Values_Multiply();
    VMValue Values_Division();
    VMValue Values_Modulo();
    VMValue Values_Plus();
    VMValue Values_Minus();
    VMValue Values_BitwiseLeft();
    VMValue Values_BitwiseRight();
    VMValue Values_BitwiseAnd();
    VMValue Values_BitwiseXor();
    VMValue Values_BitwiseOr();
    VMValue Values_LogicalAND();
    VMValue Values_LogicalOR();
    VMValue Values_LessThan();
    VMValue Values_GreaterThan();
    VMValue Values_LessThanOrEqual();
    VMValue Values_GreaterThanOrEqual();
    VMValue Values_Increment();
    VMValue Values_Decrement();
    VMValue Values_Negate();
    VMValue Values_LogicalNOT();
    VMValue Values_BitwiseNOT();
    VMValue Value_TypeOf();
};

#endif /* ENGINE_BYTECODE_VMTHREAD_H */
