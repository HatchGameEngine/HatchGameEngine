#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/IO/Stream.h>

/***
* \class Stream
* \desc An abstraction of a continuous sequence of data.
Use <ref Stream.FromResource> or <ref Stream.FromFile> to open a stream.
*/

ObjClass* StreamImpl::Class = nullptr;

void StreamImpl::Init() {
	Class = NewClass(CLASS_STREAM);
	Class->NewFn = Constructor;

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(Class);
}

Obj* StreamImpl::Constructor() {
	throw ScriptException("Cannot directly construct Stream! Use Stream.FromResource or Stream.FromFile.");
	return nullptr;
}

ObjStream* StreamImpl::New(void* streamPtr, bool writable) {
	ObjStream* stream = (ObjStream*)NewNativeInstance(sizeof(ObjStream));
	Memory::Track(stream, "NewStream");
	stream->Object.Class = Class;
	stream->InstanceObj.Destructor = Dispose;
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
