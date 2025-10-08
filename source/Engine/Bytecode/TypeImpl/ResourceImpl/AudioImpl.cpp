#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/AudioImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* AudioImpl::Class = nullptr;

Uint32 Hash_LoopPoint = 0;

#define CLASS_AUDIO "Audio"

void AudioImpl::Init() {
	Class = NewClass(CLASS_AUDIO);

	GET_STRING_HASH(LoopPoint);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_AUDIO, Class);
	TypeImpl::DefinePrintableName(Class, "audio");
}

bool AudioImpl::IsValidField(Uint32 hash) {
	CHECK_VALID_FIELD(LoopPoint);

	return false;
}

bool AudioImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
	if (!resourceable || !resourceable->IsLoaded()) {
		VM_THROW_ERROR("Audio is no longer loaded!");
		return true;
	}

	ISound* audio = (ISound*)resourceable;

	/***
	 * \field LoopPoint
	 * \desc The loop point in samples, or <code>null</code> if it doesn't have one.
	 * \ns Audio
 	*/
	if (hash == Hash_LoopPoint) {
		if (audio->LoopPoint >= 0) {
			*result = INTEGER_VAL(audio->LoopPoint);
		}
		else {
			*result = NULL_VAL;
		}
	}

	return true;
}

bool AudioImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
	if (!resourceable || !resourceable->IsLoaded()) {
		VM_THROW_ERROR("Audio is no longer loaded!");
		return true;
	}

	ISound* audio = (ISound*)resourceable;

	if (hash == Hash_LoopPoint) {
		if (IS_NULL(value)) {
			audio->LoopPoint = -1;
		}
		else if (ScriptManager::DoIntegerConversion(value, threadID)) {
			int loopPoint = AS_INTEGER(value);
			if (loopPoint >= 0) {
				audio->LoopPoint = loopPoint;
			}
		}
		return true;
	}

	return false;
}
