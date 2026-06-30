#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/TaskImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Types/Task.h>

/***
* \class Task
* \desc A scheduled function that runs every frame.
All tasks run concurrently at the end of the frame, in the main thread.
*/

ObjClass* TaskImpl::Class = nullptr;

Uint32 Hash_Timer = 0;

void TaskImpl::Init() {
	Class = NewClass(CLASS_TASK);
	Class->NewFn = Constructor;

	/***
    * \field Timer
    * \type decimal
    * \ns Task
    * \desc How long the task has been active for, in seconds. This does not increment if the task is not running.
    */
	Hash_Timer = Murmur::EncryptString("Timer");

	ScriptManager::DefineNative(Class, "Create", VM_Create);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

Obj* TaskImpl::Constructor() {
	throw ScriptException("Cannot directly construct Task! Use Task.Create.");
	return nullptr;
}

void TaskImpl::Dispose(Obj* object) {
	InstanceImpl::Dispose(object);
}

bool TaskImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	Task* task = (Task*)ScriptManager::RegistryGet(object);
	if (task == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Task is no longer valid!");
		return false;
	}

	if (hash == Hash_Timer) {
		if (result) {
			*result = DECIMAL_VAL((float)task->TotalExecutionTime / 1000.0f);
		}
		return true;
	}

	return false;
}

int TaskImpl::NativeCallback(Task* task, void* userdata) {
	ObjTask* objTask = (ObjTask*)ScriptManager::RegistryGet(task);
	if (!objTask || !userdata) {
		return Task::DONE;
	}

	VMValue callable = OBJECT_VAL(userdata);
	if (!IS_CALLABLE(callable)) {
		return Task::DONE;
	}

	VMThread* thread = &ScriptManager::Threads[0];
	thread->Push(callable);
	thread->Push(OBJECT_VAL(objTask));
	thread->InvokeForEntity(callable, 1);

	if (!IS_INTEGER(thread->InterpretResult)) {
		return Task::DONE;
	}

	switch (AS_INTEGER(thread->InterpretResult)) {
	case TASK_RESULT_CONTINUE:
		return Task::CONTINUE;
	case TASK_RESULT_RESTART:
		return Task::RESTART;
	case TASK_RESULT_DONE:
		return Task::DONE;
	default:
		break;
	}

	return Task::DONE;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

/***
 * Task.Create
 * \desc Creates a task.
 * \param callback (callable): The callback to call for the task. The task itself is passed as the first argument.
 * \paramOpt delay (decimal): How many seconds to delay execution of the task.
 * \return Task Returns the newly created task.
 * \ns Task
 */
VMValue TaskImpl::VM_Create(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	VMValue callable = GET_ARG(0, StandardLibrary::GetCallable);
	float delay = GET_ARG_OPT(1, GetDecimal, 0.0f);

	if (IS_NULL(callable)) {
		return NULL_VAL;
	}
	if (delay < 0.0) {
		throw ScriptException("Delay cannot be lower than 0.0.");
	}

	ObjTask* objTask = (ObjTask*)NewNativeInstance(sizeof(ObjTask));
	Memory::Track(objTask, "TaskImpl::New");
	objTask->Object.Class = Class;
	objTask->InstanceObj.PropertyGet = VM_PropertyGet;
	objTask->InstanceObj.Destructor = Dispose;

	Task* task = new Task(NativeCallback, AS_OBJECT(callable));
	task->ExecutionDelay = delay * 1000.0f;
	task->Start();

	ScriptManager::RegistryAdd(task, (Obj*)objTask);

	Application::AddTask(task);

	return OBJECT_VAL(objTask);
}

#undef GET_ARG
#undef GET_ARG_OPT
