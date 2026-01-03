#ifndef ENGINE_OPERATION_H
#define ENGINE_OPERATION_H

struct OperationResult {
	bool Success;
	void* Input;
	void* Output;
};

typedef void (*OperationCallback)(OperationResult result);

struct Operation {
	OperationCallback Callback;
	void* Data;
};

#endif
