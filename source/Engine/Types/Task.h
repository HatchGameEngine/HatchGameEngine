#ifndef ENGINE_TYPES_TASK_H
#define ENGINE_TYPES_TASK_H

typedef int (*TaskCallback)(class Task*, void*);

class Task {
public:
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

	Task* Next = nullptr;
	Task* Prev = nullptr;

	Task(TaskCallback callback, void* userdata);

	void Start();
	int Run();
	void Stop();
};

#endif /* ENGINE_TYPES_TASK_H */
