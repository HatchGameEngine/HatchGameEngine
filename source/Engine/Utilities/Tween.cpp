#include <Engine/Application.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Bytecode/TypeImpl/TaskImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Math/Ease.h>
#include <Engine/Diagnostics/Memory.h>
#include <Engine/Utilities/Tween.h>

#define TASK_TWEENTARGET_FIELD "TweenTarget"

Task* Tween::Perform(float* field, float from, float to, float duration, int easing) {
	Tween* tween = new Tween;
	tween->Tweenable = (void*)field;
	tween->ValueFrom = from;
	tween->ValueTo = to;
	tween->Duration = duration;
	tween->Easing = easing;

	return tween->StartTask(RunCallback);
}

Task* Tween::PerformForScript(void* tweenable, const char* field, float from, float to, float duration, int easing) {
	ObjInstance* objTask = (ObjInstance*)TaskImpl::CreateObject();

	Tween* tween = new Tween;
	tween->Tweenable = nullptr;
	tween->ValueFrom = from;
	tween->ValueTo = to;
	tween->Duration = duration;
	tween->Easing = easing;
	tween->FieldHash = Murmur::EncryptString(field);
	tween->ExposeFieldsForScript(objTask, tweenable);

	Task* task = tween->StartTask(RunCallbackScript);
	ScriptManager::RegistryAdd(task, (Obj*)objTask);

	return task;
}

Task* Tween::PerformForScript(void* tweenable, void* callback, float from, float to, float duration, int easing) {
	ObjInstance* objTask = (ObjInstance*)TaskImpl::CreateObject();

	Tween* tween = new Tween;
	tween->Tweenable = nullptr;
	tween->ValueFrom = from;
	tween->ValueTo = to;
	tween->Duration = duration;
	tween->Easing = easing;
	tween->Callback = callback;
	tween->ExposeFieldsForScript(objTask, tweenable);

	Task* task = tween->StartTask(RunCallbackScript);
	ScriptManager::RegistryAdd(task, (Obj*)objTask);

	return task;
}

void Tween::ExposeFieldsForScript(void* obj, void* tweenable) {
	ObjInstance* objTask = (ObjInstance*)obj;

	// The Tween exists for as long as the Task does, so it's okay to link the fields.
	objTask->Fields->Put(TASK_TWEENTARGET_FIELD, OBJECT_VAL(tweenable));
	objTask->Fields->Put("TweenFrom", DECIMAL_LINK_VAL(&ValueFrom));
	objTask->Fields->Put("TweenTo", DECIMAL_LINK_VAL(&ValueTo));
	objTask->Fields->Put("TweenDuration", DECIMAL_LINK_VAL(&Duration));
	objTask->Fields->Put("TweenEasing", INTEGER_LINK_VAL(&Easing));
}

Task* Tween::StartTask(TaskRunCallback callback) {
	Task* task = new Task(callback);
	task->Userdata = (void*)this;
	task->StopCallback = StopCallback;
	task->DeleteCallback = DeleteCallback;

	Application::AddTask(task);

	return task;
}

float Tween::Do(float from, float to, float lerpValue) {
	return ((1.0f - lerpValue) * from) + (lerpValue * to);
}

float Tween::GetValue() {
	float value;

	if (Application::UseFixedTimestep) {
		value = Frame / (Duration * Application::TargetFPS);
	}
	else {
		value = Timer / (Duration * 1000.0f);
	}

	if (value < 0.0f) {
		value = 0.0f;
	}
	else if (value > 1.0f) {
		value = 1.0f;
	}

	if (Easing != Ease::NONE) {
		value = Ease::Do((Ease::Mode)Easing, value);
	}

	return value;
}

bool Tween::Step() {
	Timer += Application::DeltaTime;
	Frame++;

	if (Application::UseFixedTimestep) {
		return Frame >= Duration * Application::TargetFPS;
	}
	else {
		return Timer >= Duration * 1000.0f;
	}
}

int Tween::RunCallback(Task* task, void* userdata) {
	Tween* tween = (Tween*)userdata;
	if (!tween || tween->Duration <= 0.0f) {
		return Task::DONE;
	}

	bool done = tween->Step();

	float value = Do(tween->ValueFrom, tween->ValueTo, tween->GetValue());

	float* field = (float*)tween->Tweenable;
	*field = value;

	if (done) {
		return Task::DONE;
	}

	return Task::CONTINUE;
}

int Tween::RunCallbackScript(Task* task, void* userdata) {
	ObjInstance* objTask = (ObjInstance*)ScriptManager::RegistryGet(task);
	Tween* tween = (Tween*)userdata;
	if (!objTask || !tween || tween->Duration <= 0.0f) {
		return Task::DONE;
	}

	VMValue tweenable = objTask->Fields->Get(TASK_TWEENTARGET_FIELD);
	if (IS_NULL(tweenable)) {
		return Task::DONE;
	}

	bool done = tween->Step();

	float value = Do(tween->ValueFrom, tween->ValueTo, tween->GetValue());

	VMThread* thread = &ScriptManager::Threads[0];
	if (tween->Callback) {
		thread->Push(tweenable);
		thread->Push(DECIMAL_VAL(value));
		thread->InvokeForEntity(OBJECT_VAL(tween->Callback), 1);
	}
	else {
		thread->SetProperty(tweenable, tween->FieldHash, DECIMAL_VAL(value));
	}

	if (done) {
		return Task::DONE;
	}

	return Task::CONTINUE;
}

void Tween::StopCallback(Task* task, void* userdata) {
	Tween* tween = (Tween*)userdata;
	tween->Timer = 0.0f;
	tween->Frame = 0;
}

void Tween::DeleteCallback(Task* task, void* userdata) {
	Tween* tween = (Tween*)userdata;
	delete tween;
}
