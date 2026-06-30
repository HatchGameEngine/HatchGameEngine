#include <Engine/Application.h>
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

Uint32 Hash_State = 0;
Uint32 Hash_Timer = 0;
Uint32 Hash_DelayTime = 0;
Uint32 Hash_Priority = 0;

void TaskImpl::Init() {
	Class = NewClass(CLASS_TASK);
	Class->NewFn = Constructor;

	/***
    * \field State
    * \type <ref TASKSTATE_*>
    * \ns Task
    * \desc The state of the task.
    */
	Hash_State = Murmur::EncryptString("State");
	/***
    * \field Timer
    * \type decimal
    * \ns Task
    * \desc How long the task has been active for, in seconds. It does not increment if the task is not running. This value is read-only.
    */
	Hash_Timer = Murmur::EncryptString("Timer");
	/***
    * \field DelayTime
    * \type decimal
    * \ns Task
    * \desc The amount of seconds to delay the execution of the task. Changes to this value do not take any effect until the task returns <ref TASK_CONTINUE>, or until <ref Task.Restart> is called for the task.
    */
	Hash_DelayTime = Murmur::EncryptString("DelayTime");
	/***
    * \field Priority
    * \type integer
    * \ns Task
    * \desc The priority. Higher numbers cause tasks to be executed sooner, and lower numbers cause tasks to be executed later. Changes to this value do not take any effect until the next frame.
    */
	Hash_Priority = Murmur::EncryptString("Priority");

	ScriptManager::DefineNative(Class, "Create", VM_Create);
	ScriptManager::DefineNative(Class, "Restart", VM_Restart);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

Obj* TaskImpl::Constructor() {
	throw ScriptException("Cannot directly construct Task! Use Task.Create.");
	return nullptr;
}

void TaskImpl::Dispose(Obj* object) {
	Task* task = (Task*)ScriptManager::RegistryGet(object);
	if (task != nullptr) {
		task->Dispose();
	}

	InstanceImpl::Dispose(object);
}

bool TaskImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	Task* task = (Task*)ScriptManager::RegistryGet(object);
	if (task == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Task is no longer valid!");
		return false;
	}

	if (hash == Hash_State) {
		if (result) {
			switch (task->State) {
			case Task::STATE_WAITING:
				*result = INTEGER_VAL(TASK_STATE_WAITING);
				break;
			case Task::STATE_RUNNING:
				*result = INTEGER_VAL(TASK_STATE_RUNNING);
				break;
			case Task::STATE_STOPPED:
				*result = INTEGER_VAL(TASK_STATE_STOPPED);
				break;
			}
		}
		return true;
	}
	else if (hash == Hash_Timer) {
		if (result) {
			*result = DECIMAL_VAL((float)task->TotalExecutionTime / 1000.0f);
		}
		return true;
	}
	else if (hash == Hash_DelayTime) {
		if (result) {
			*result = DECIMAL_VAL((float)task->ExecutionDelay / 1000.0f);
		}
		return true;
	}
	else if (hash == Hash_Priority) {
		if (result) {
			*result = INTEGER_VAL(task->Priority);
		}
		return true;
	}

	return false;
}

bool TaskImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	Task* task = (Task*)ScriptManager::RegistryGet(object);
	if (task == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Task is no longer valid!");
		return false;
	}

#define CHECK_CANNOT_MODIFY(name) \
	{ \
		if (hash == Hash_##name) { \
			ScriptManager::Threads[threadID].ThrowRuntimeError( \
				false, "Field \"" #name "\" cannot be written to!"); \
			return true; \
		} \
	}

	CHECK_CANNOT_MODIFY(State);
	CHECK_CANNOT_MODIFY(Timer);

#undef CHECK_CANNOT_MODIFY

	if (hash == Hash_DelayTime) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			float delay = AS_DECIMAL(value);
			if (delay < 0.0) {
				ScriptManager::Threads[threadID].ThrowRuntimeError(
					false, "Delay time cannot be lower than 0.0.");
			}
			else {
				task->ExecutionDelay = delay * 1000.0f;
			}
		}
		return true;
	}
	else if (hash == Hash_Priority) {
		if (ScriptManager::DoIntegerConversion(value, threadID)) {
			int priority = AS_INTEGER(value);
			if (priority != task->Priority) {
				task->Priority = priority;
				Application::TaskPriorityChanged = true;
			}
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
 * \paramOpt delayTime (decimal): The amount of seconds to delay the execution of the task.
 * \paramOpt priority (integer): The priority of the task. Higher numbers cause tasks to be executed sooner, and lower numbers cause tasks to be executed later.
 * \return Task Returns the newly created task.
 * \ns Task
 */
VMValue TaskImpl::VM_Create(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	VMValue callable = GET_ARG(0, GetCallable);
	float delay = GET_ARG_OPT(1, GetDecimal, 0.0f);
	int priority = GET_ARG_OPT(2, GetInteger, 0);

	if (IS_NULL(callable)) {
		return NULL_VAL;
	}
	if (delay < 0.0) {
		throw ScriptException("Delay time cannot be lower than 0.0.");
	}

	ObjTask* objTask = (ObjTask*)NewNativeInstance(sizeof(ObjTask));
	Memory::Track(objTask, "TaskImpl::New");
	objTask->Object.Class = Class;
	objTask->InstanceObj.PropertyGet = VM_PropertyGet;
	objTask->InstanceObj.PropertySet = VM_PropertySet;
	objTask->InstanceObj.Destructor = Dispose;

	Task* task = new Task(NativeCallback, AS_OBJECT(callable));
	task->ExecutionDelay = delay * 1000.0f;
	task->Priority = priority;
	task->Start();

	ScriptManager::RegistryAdd(task, (Obj*)objTask);

	Application::AddTask(task);

	return OBJECT_VAL(objTask);
}
/***
 * Task.Restart
 * \desc Stops and starts a task.
 * \param task (Task): The task to restart.
 * \ns Task
 */
VMValue TaskImpl::VM_Restart(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ObjTask* objTask = GET_ARG(0, GetTask);
	Task* task = (Task*)ScriptManager::RegistryGet((Obj*)objTask);
	if (task == nullptr) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Task is no longer valid!");
		return NULL_VAL;
	}

	Application::RemoveTask(task);
	Application::AddTask(task);

	task->Start();

	return NULL_VAL;
}

#undef GET_ARG
#undef GET_ARG_OPT
