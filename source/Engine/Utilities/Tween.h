#ifndef ENGINE_UTILITIES_TWEEN_H
#define ENGINE_UTILITIES_TWEEN_H

#include <Engine/Types/Task.h>

class Tween {
private:
	Task* StartTask(TaskRunCallback callback);
	static double Do(float from, float to, float lerpValue);
	static int RunCallback(class Task* task, void* userdata);
	static int RunCallbackScript(class Task* task, void* userdata);
	static void StopCallback(class Task* task, void* userdata);
	static void DeleteCallback(class Task* task, void* userdata);

public:
	static Task* Perform(double* field, double from, double to, double duration, int easing);
	static Task* PerformForScript(void* tweenable, const char* field, double from, double to, double duration, int easing);
	static Task* PerformForScript(void* tweenable, void* callback, double from, double to, double duration, int easing);

	double GetValue();
	bool Step();

	void* Tweenable = nullptr;
	double ValueFrom = 0.0f;
	double ValueTo = 0.0f;
	double Duration = 1.0f;
	int Easing = 0;

	double Timer = 0.0f;
	int Frame = 0;

	Uint32 FieldHash = 0x00000000; // Hash of the field to tween, for scripting.
	void* Callback = nullptr; // The callback of the tween, for scripting.
};

#endif /* ENGINE_UTILITIES_TWEEN_H */
