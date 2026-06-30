#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Task.h>

Task::Task(TaskCallback callback, void* userdata) {
	Callback = callback;
	Userdata = userdata;
}

// Start the task.
void Task::Start() {
	State = STATE_WAITING;
	TimeRemainingUntilExecution = ExecutionDelay;
	TotalExecutionTime = 0.0f;
}

// Tick down the execution delay.
bool Task::DoWait(float deltaTime) {
	State = STATE_WAITING;

	TimeRemainingUntilExecution -= deltaTime;
	if (TimeRemainingUntilExecution < 0.0) {
		TimeRemainingUntilExecution = 0.0;
		return true;
	}

	return false;
}

// Run the callback.
int Task::Run() {
	State = STATE_RUNNING;

	return Callback(this, Userdata);
}

// Stop the task.
void Task::Stop() {
	if (State == STATE_STOPPED) {
		return;
	}

	State = STATE_STOPPED;
}

// Stop and delete the task.
void Task::Dispose() {
	Stop();

	ScriptManager::RegistryRemove(this);

	delete this;
}
