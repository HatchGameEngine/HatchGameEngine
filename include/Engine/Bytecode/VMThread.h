#ifndef ENGINE_BYTECODE_VMTHREAD_H
#define ENGINE_BYTECODE_VMTHREAD_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/PrintBuffer.h>
#include <Engine/Includes/Standard.h>

class VMThread {
private:
	string GetFunctionName(const ObjFunction *function) const;
	void PrintStackTrace(PrintBuffer* buffer, const char* errorString);
	bool CheckBranchLimit(CallFrame* frame);
	bool DoJump(CallFrame* frame, int offset);
	bool DoJumpBack(CallFrame* frame, int offset);
	bool
	GetProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields, ValueGetFn getter);
	bool GetProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields);
	bool GetProperty(Obj* object, ObjClass* klass, Uint32 hash);
	bool
	HasProperty(Obj* object, ObjClass* klass, Uint32 hash, bool checkFields, ValueGetFn getter);
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
	float ReadFloat(CallFrame* frame);
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

#if USING_VM_FUNCPTRS
	int RunOpcodeFunc(CallFrame* frame);

#define VM_ADD_OPFUNC(op) int OPFUNC_##op(CallFrame* frame)
	VM_ADD_OPFUNC(OP_ERROR);
	VM_ADD_OPFUNC(OP_CONSTANT);
	VM_ADD_OPFUNC(OP_DEFINE_GLOBAL);
	VM_ADD_OPFUNC(OP_GET_PROPERTY);
	VM_ADD_OPFUNC(OP_SET_PROPERTY);
	VM_ADD_OPFUNC(OP_GET_GLOBAL);
	VM_ADD_OPFUNC(OP_SET_GLOBAL);
	VM_ADD_OPFUNC(OP_GET_LOCAL);
	VM_ADD_OPFUNC(OP_SET_LOCAL);
	VM_ADD_OPFUNC(OP_PRINT_STACK);
	VM_ADD_OPFUNC(OP_INHERIT);
	VM_ADD_OPFUNC(OP_RETURN);
	VM_ADD_OPFUNC(OP_METHOD);
	VM_ADD_OPFUNC(OP_CLASS);
	VM_ADD_OPFUNC(OP_CALL);
	VM_ADD_OPFUNC(OP_SUPER);
	VM_ADD_OPFUNC(OP_INVOKE);
	VM_ADD_OPFUNC(OP_JUMP);
	VM_ADD_OPFUNC(OP_JUMP_IF_FALSE);
	VM_ADD_OPFUNC(OP_JUMP_BACK);
	VM_ADD_OPFUNC(OP_POP);
	VM_ADD_OPFUNC(OP_COPY);
	VM_ADD_OPFUNC(OP_ADD);
	VM_ADD_OPFUNC(OP_SUBTRACT);
	VM_ADD_OPFUNC(OP_MULTIPLY);
	VM_ADD_OPFUNC(OP_DIVIDE);
	VM_ADD_OPFUNC(OP_MODULO);
	VM_ADD_OPFUNC(OP_NEGATE);
	VM_ADD_OPFUNC(OP_INCREMENT);
	VM_ADD_OPFUNC(OP_DECREMENT);
	VM_ADD_OPFUNC(OP_BITSHIFT_LEFT);
	VM_ADD_OPFUNC(OP_BITSHIFT_RIGHT);
	VM_ADD_OPFUNC(OP_NULL);
	VM_ADD_OPFUNC(OP_TRUE);
	VM_ADD_OPFUNC(OP_FALSE);
	VM_ADD_OPFUNC(OP_BW_NOT);
	VM_ADD_OPFUNC(OP_BW_AND);
	VM_ADD_OPFUNC(OP_BW_OR);
	VM_ADD_OPFUNC(OP_BW_XOR);
	VM_ADD_OPFUNC(OP_LG_NOT);
	VM_ADD_OPFUNC(OP_LG_AND);
	VM_ADD_OPFUNC(OP_LG_OR);
	VM_ADD_OPFUNC(OP_EQUAL);
	VM_ADD_OPFUNC(OP_EQUAL_NOT);
	VM_ADD_OPFUNC(OP_GREATER);
	VM_ADD_OPFUNC(OP_GREATER_EQUAL);
	VM_ADD_OPFUNC(OP_LESS);
	VM_ADD_OPFUNC(OP_LESS_EQUAL);
	VM_ADD_OPFUNC(OP_PRINT);
	VM_ADD_OPFUNC(OP_ENUM_NEXT);
	VM_ADD_OPFUNC(OP_SAVE_VALUE);
	VM_ADD_OPFUNC(OP_LOAD_VALUE);
	VM_ADD_OPFUNC(OP_WITH);
	VM_ADD_OPFUNC(OP_GET_ELEMENT);
	VM_ADD_OPFUNC(OP_SET_ELEMENT);
	VM_ADD_OPFUNC(OP_NEW_ARRAY);
	VM_ADD_OPFUNC(OP_NEW_MAP);
	VM_ADD_OPFUNC(OP_SWITCH_TABLE);
	VM_ADD_OPFUNC(OP_FAILSAFE);
	VM_ADD_OPFUNC(OP_EVENT);
	VM_ADD_OPFUNC(OP_TYPEOF);
	VM_ADD_OPFUNC(OP_NEW);
	VM_ADD_OPFUNC(OP_IMPORT);
	VM_ADD_OPFUNC(OP_SWITCH);
	VM_ADD_OPFUNC(OP_POPN);
	VM_ADD_OPFUNC(OP_HAS_PROPERTY);
	VM_ADD_OPFUNC(OP_IMPORT_MODULE);
	VM_ADD_OPFUNC(OP_ADD_ENUM);
	VM_ADD_OPFUNC(OP_NEW_ENUM);
	VM_ADD_OPFUNC(OP_GET_SUPERCLASS);
	VM_ADD_OPFUNC(OP_GET_MODULE_LOCAL);
	VM_ADD_OPFUNC(OP_SET_MODULE_LOCAL);
	VM_ADD_OPFUNC(OP_DEFINE_MODULE_LOCAL);
	VM_ADD_OPFUNC(OP_USE_NAMESPACE);
	VM_ADD_OPFUNC(OP_DEFINE_CONSTANT);
	VM_ADD_OPFUNC(OP_INTEGER);
	VM_ADD_OPFUNC(OP_DECIMAL);
#endif
};

#endif /* ENGINE_BYTECODE_VMTHREAD_H */
