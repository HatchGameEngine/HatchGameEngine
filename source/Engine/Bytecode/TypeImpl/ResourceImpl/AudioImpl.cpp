#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/AudioImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* AudioImpl::Class = nullptr;

Uint32 Hash_LoopPoint = 0;

void AudioImpl::Init() {
	Class = NewClass("AudioResource");

	Hash_LoopPoint = Murmur::EncryptString("LoopPoint");

	TypeImpl::RegisterClass(Class);
	TypeImpl::DefinePrintableName(Class, "audio");
}

bool AudioImpl::IsValidField(Uint32 hash) {
	return hash == Hash_LoopPoint;
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
	 * \desc The loop point of the audio in samples, or <code>null</code> if it doesn't have one.
	 * \ns AudioResource
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
	else {
		return false;
	}

	return true;
}
