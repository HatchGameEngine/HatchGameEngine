#ifndef ENGINE_TYPES_TASK_H
#define ENGINE_TYPES_TASK_H

typedef int (*TaskRunCallback)(class Task*, void*);
typedef void (*TaskStopCallback)(class Task*, void*);

class Task {
public:
	enum {
		STATE_WAITING,
		STATE_RUNNING,
		STATE_STOPPED
	};

	enum {
		CONTINUE = 0,
		REPEAT = 1,
		DONE = -1
	};

	double TotalExecutionTime = 0.0f;
	double ExecutionDelay = 0.0f;
	double TimeRemainingUntilExecution = 0.0f;

	TaskRunCallback Callback = nullptr;
	TaskStopCallback StopCallback = nullptr;
	void* Userdata = nullptr;

	void* ScriptRunCallback = nullptr;
	void* ScriptStopCallback = nullptr;

	int State = STATE_WAITING;
	int Priority = 0;

	Task(TaskRunCallback callback);
	Task(TaskRunCallback runCallback, TaskStopCallback stopCallback);

	void Start();
	bool Wait(float deltaTime);
	void Repeat();
	int Run();
	void Stop();
	void Dispose();
};

#endif /* ENGINE_TYPES_TASK_H */
