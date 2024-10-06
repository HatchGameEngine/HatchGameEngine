#ifndef ENGINE_BYTECODE_VMTHREAD_H
#define ENGINE_BYTECODE_VMTHREAD_H

#include <Engine/Includes/Standard.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/PrintBuffer.h>

class VMThread {
private:
    string GetFunctionName(ObjFunction* function);
    void PrintStackTrace(PrintBuffer* buffer, const char* errorString);
    bool CheckBranchLimit(CallFrame* frame);
    bool DoJump(CallFrame* frame, int offset);
    bool DoJumpBack(CallFrame* frame, int offset);
    bool GetProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields, ValueGetFn getter);
    bool GetProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields);
    bool GetProperty(Obj* object, ObjClass* klass, Uint32 hash);
    bool HasProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields, ValueGetFn getter);
    bool HasProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields);
    bool HasProperty(Obj* object, ObjClass* klass, Uint32 hash);
    bool SetProperty(Table* fields, Uint32 hash, VMValue field, VMValue value);
    bool BindMethod(VMValue receiver, VMValue method);
    bool CallBoundMethod(ObjBoundMethod* bound, int argCount);
    bool CallValue(VMValue callee, int argCount);
    bool CallForObject(VMValue callee, int argCount);
    bool InstantiateClass(VMValue callee, int argCount);
    bool DoClassExtension(VMValue value, VMValue originalValue, bool clearSrc);

public:
    VMValue Stack[STACK_SIZE_MAX];
    VMValue* StackTop = Stack;
    VMValue RegisterValue;
    CallFrame Frames[FRAMES_MAX];
    Uint32 FrameCount;
    Uint32 ReturnFrame;
    VMValue FunctionToInvoke;
    VMValue InterpretResult;
    enum ThreadState {
        CREATED = 0,
        RUNNING = 1,
        WAITING = 2,
        CLOSED = 3,
    }; 
    char Name[THREAD_NAME_MAX];
    Uint32 ID;
    bool DebugInfo;
    Uint32 BranchLimit;
    static bool InstructionIgnoreMap[0x100];
    static std::jmp_buf JumpBuffer;

    static char* GetToken(Uint32 hash);
    static char* GetVariableOrMethodName(Uint32 hash);
    void MakeErrorMessage(PrintBuffer* buffer, const char* errorString);
    int ThrowRuntimeError(bool fatal, const char* errorMessage, ...);
    void PrintStack();
    void ReturnFromNative() throw();
    void Push(VMValue value);
    VMValue Pop();
    void Pop(unsigned amount);
    VMValue Peek(int offset);
    Uint32 StackSize();
    void ResetStack();
    Uint8 ReadByte(CallFrame* frame);
    Uint16 ReadUInt16(CallFrame* frame);
    Uint32 ReadUInt32(CallFrame* frame);
    Sint16 ReadSInt16(CallFrame* frame);
    Sint32 ReadSInt32(CallFrame* frame);
    VMValue ReadConstant(CallFrame* frame);
    bool ShowBranchLimitMessage(const char* errorMessage, ...);
    int RunInstruction();
    void RunInstructionSet();
    void RunValue(VMValue value, int argCount);
    void RunFunction(ObjFunction* func, int argCount);
    void InvokeForEntity(VMValue value, int argCount);
    VMValue RunEntityFunction(ObjFunction* function, int argCount);
    void CallInitializer(VMValue value);
    bool Call(ObjFunction* function, int argCount);
    bool InvokeFromClass(ObjClass* klass, Uint32 hash, int argCount);
    bool InvokeForInstance(Uint32 hash, int argCount, bool isSuper);
    bool Import(VMValue value);
    bool ImportModule(VMValue value);
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
