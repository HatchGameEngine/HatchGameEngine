#ifndef ENGINE_BYTECODE_TYPEIMPL_TASKIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_TASKIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define CLASS_TASK "Task"

#define IS_TASK(value) IsNativeInstance(value, CLASS_TASK)
#define AS_TASK(value) ((ObjTask*)AS_OBJECT(value))

#define TASK_RESULT_CONTINUE 0
#define TASK_RESULT_RESTART 1
#define TASK_RESULT_DONE -1

class TaskImpl {
private:
	static int NativeCallback(Task* task, void* userdata);
	static bool VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID);
	static bool VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID);
	static VMValue VM_Create(int argCount, VMValue* args, Uint32 threadID);
	static VMValue VM_Restart(int argCount, VMValue* args, Uint32 threadID);

public:
	static ObjClass* Class;

	static void Init();

	static Obj* Constructor();
	static void Dispose(Obj* object);
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_TASKIMPL_H */
