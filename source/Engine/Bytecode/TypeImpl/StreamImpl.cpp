#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/IO/Stream.h>

ObjClass* StreamImpl::Class = nullptr;

void StreamImpl::Init() {
	Class = NewClass(CLASS_STREAM);

	TypeImpl::RegisterClass(Class);
	TypeImpl::DefinePrintableName(Class, "stream");
}

ObjStream* StreamImpl::New(void* streamPtr, bool writable) {
	ObjStream* stream = (ObjStream*)NewNativeInstance(sizeof(ObjStream));
	Memory::Track(stream, "NewStream");
	stream->Object.Class = Class;
	stream->Object.Destructor = Dispose;
	stream->StreamPtr = (Stream*)streamPtr;
	stream->Writable = writable;
	stream->Closed = false;
	return stream;
}

void StreamImpl::Dispose(Obj* object) {
	ObjStream* stream = (ObjStream*)object;

	if (!stream->Closed) {
		stream->StreamPtr->Close();
	}

	InstanceImpl::Dispose(object);
}
