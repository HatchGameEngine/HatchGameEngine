#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/FontImpl.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/ResourceTypes/Font.h>

ObjClass* FontImpl::Class = nullptr;

void FontImpl::Init() {
	Class = NewClass(CLASS_FONT);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	ScriptManager::DefineNative(Class, "SetPixelsPerUnit", VM_SetPixelsPerUnit);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_FONT, Class);
	TypeImpl::DefinePrintableName(Class, "font");
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

Stream* GetFontStream(VMValue value, bool& closeStream, Uint32 threadID) {
	closeStream = false;

	if (IS_STREAM(value)) {
		ObjStream* objStream = AS_STREAM(value);
		if (!objStream || !objStream->StreamPtr) {
			return nullptr;
		}

		if (objStream->Closed) {
			throw ScriptException("Cannot read closed stream!");
		}
		if (objStream->Writable) {
			throw ScriptException("Stream is not readable!");
		}

		return objStream->StreamPtr;
	}
	else {
		char* filename = nullptr;

		if (ScriptManager::Lock()) {
			if (IS_STRING(value)) {
				filename = AS_CSTRING(value);
			}
			else {
				ScriptManager::Threads[threadID].ThrowRuntimeError(false, "Expected argument to be of type %s instead of %s.", GetObjectTypeString(OBJ_STRING), GetValueTypeString(value));
			}
			ScriptManager::Unlock();
		}

		if (!filename) {
			return nullptr;
		}

		Stream* stream = ResourceStream::New(filename);
		if (!stream) {
			throw ScriptException("Resource \"" + std::string(filename) + "\" does not exist!");
		}

		closeStream = true;

		return stream;
	}
}

/***
 * \constructor
 * \param font (String, Stream, or Array): The font or list of fonts.
 * \desc Loads a font from the given Resource path, Stream, or Array containing Resource paths or Streams.
 * \ns Font
 */
Obj* FontImpl::New() {
	Font* font = new Font();
	ObjFont* obj = New((void*)font);
	return (Obj*)obj;
}
ObjFont* FontImpl::New(void* fontPtr) {
	ObjFont* font = (ObjFont*)NewNativeInstance(sizeof(ObjFont));
	Memory::Track(font, "NewFont");
	font->Object.Class = Class;
	font->Object.Destructor = Dispose;
	font->FontPtr = (Font*)fontPtr;
	return font;
}
VMValue FontImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	ObjFont* objFont = AS_FONT(args[0]);
	Font* font = objFont->FontPtr;

	StandardLibrary::CheckArgCount(argCount, 2);

	std::vector<Stream*> streamList;
	std::vector<bool> closeStream;

#define FREE_STREAM_LIST \
	for (size_t i = 0; i < streamList.size(); i++) { \
		if (closeStream[i]) { \
			streamList[i]->Close(); \
		} \
	}

#define GET_STREAM(value) \
	try { \
		bool close; \
		Stream* stream = GetFontStream(value, close, threadID); \
		streamList.push_back(stream); \
		closeStream.push_back(close); \
	} \
	catch (const std::runtime_error& error) { \
		FREE_STREAM_LIST \
		throw; \
	}

	if (IS_ARRAY(args[1])) {
		ObjArray* array = AS_ARRAY(args[1]);
		for (size_t i = 0; i < array->Values->size(); i++) {
			GET_STREAM((*array->Values)[i]);
		}
	}
	else {
		GET_STREAM(args[1]);
	}

#undef GET_STREAM

	if (streamList.size() == 0) {
		throw ScriptException("No fonts to load!");
	}

	if (font->Load(streamList) && font->LoadSize(DEFAULT_FONT_SIZE)) {
		font->LoadFailed = false;
	}

	FREE_STREAM_LIST

	if (font->LoadFailed) {
		throw ScriptException("Could not load font!");
	}

	return OBJECT_VAL(objFont);
}
void FontImpl::Dispose(Obj* object) {
	ObjFont* objFont = (ObjFont*)object;

	Font* font = (Font*)objFont->FontPtr;
	if (font != nullptr) {
		delete font;
	}

	InstanceImpl::Dispose(object);
}
/***
 * \method SetPixelsPerUnit
 * \desc Sets the pixels per unit value of the font. Higher values improve the clarity, at the cost of increased memory usage.
 * \param pixelsPerUnit (Number): The pixels per unit value.
 * \ns Font
 */
VMValue FontImpl::VM_SetPixelsPerUnit(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ObjFont* objFont = GET_ARG(0, GetFont);
	if (objFont == nullptr) {
		return NULL_VAL;
	}

	Font* font = (Font*)objFont->FontPtr;
	float pixelsPerUnit = GET_ARG(1, GetDecimal);

	font->Size = Math::Clamp(pixelsPerUnit, 1.0f, 2048.0f);
	font->Reload();

	return NULL_VAL;
}

#undef GET_ARG
