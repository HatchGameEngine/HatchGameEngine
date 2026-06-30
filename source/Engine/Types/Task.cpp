#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Task.h>

Task::Task(TaskCallback callback, void* userdata) {
	Callback = callback;
	Userdata = userdata;
}

void Task::Start() {
	TimeRemainingUntilExecution = ExecutionDelay;
	TotalExecutionTime = 0.0f;
}

int Task::Run() {
	return Callback(this, Userdata);
}

void Task::Stop() {
	ScriptManager::RegistryRemove(this);

	delete this;
}
