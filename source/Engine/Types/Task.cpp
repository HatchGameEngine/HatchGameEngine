#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Types/Task.h>

Task::Task(TaskRunCallback callback) {
	Callback = callback;
}

Task::Task(TaskRunCallback runCallback, TaskStopCallback stopCallback) {
	Callback = runCallback;
	StopCallback = stopCallback;
}

// Start the task.
void Task::Start() {
	State = STATE_WAITING;
	TimeRemainingUntilExecution = ExecutionDelay;
	TotalExecutionTime = 0.0f;
}

// Tick down the execution delay.
bool Task::Wait(float deltaTime) {
	State = STATE_WAITING;

	TimeRemainingUntilExecution -= deltaTime;
	if (TimeRemainingUntilExecution < 0.0) {
		TimeRemainingUntilExecution = 0.0;
		return true;
	}

	return false;
}

// Repeat the task.
void Task::Repeat() {
	TimeRemainingUntilExecution = ExecutionDelay;
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

	if (StopCallback) {
		StopCallback(this, Userdata);
	}

	State = STATE_STOPPED;
}

// Stop and delete the task.
void Task::Dispose() {
	Stop();

	if (DeleteCallback) {
		DeleteCallback(this, Userdata);
	}

	ScriptManager::RegistryRemove(this);

	delete this;
}
