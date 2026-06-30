#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Task.h>

Task::Task(TaskCallback callback, void* userdata) {
	Callback = callback;
	Userdata = userdata;
}

// Start the task.
void Task::Start() {
	TimeRemainingUntilExecution = ExecutionDelay;
	TotalExecutionTime = 0.0f;
}

// Run the callback.
int Task::Run() {
	return Callback(this, Userdata);
}

// Stop the task.
void Task::Stop() {}

// Stop and delete the task.
void Task::Dispose() {
	Stop();

	ScriptManager::RegistryRemove(this);

	delete this;
}
