#ifndef ENGINE_TYPES_TASK_H
#define ENGINE_TYPES_TASK_H

typedef int (*TaskCallback)(class Task*, void*);

class Task {
public:
	enum {
		STATE_WAITING,
		STATE_RUNNING,
		STATE_STOPPED
	};

	enum {
		CONTINUE = 0,
		RESTART = 1,
		DONE = -1
	};

	double TotalExecutionTime = 0.0f;
	double ExecutionDelay = 0.0f;
	double TimeRemainingUntilExecution = 0.0f;

	TaskCallback Callback = nullptr;
	void* Userdata = nullptr;

	int State = STATE_WAITING;
	int Priority = 0;

	Task(TaskCallback callback, void* userdata);

	void Start();
	bool DoWait(float deltaTime);
	int Run();
	void Stop();
	void Dispose();
};

#endif /* ENGINE_TYPES_TASK_H */
