#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/SpriteImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* SpriteImpl::Class = nullptr;

Uint32 Hash_NumAnimations = 0;

void SpriteImpl::Init() {
	Class = NewClass("SpriteResource");

	GET_STRING_HASH(NumAnimations);

	AddNatives();

	TypeImpl::RegisterClass(Class);
	TypeImpl::DefinePrintableName(Class, "sprite");
}

bool SpriteImpl::IsValidField(Uint32 hash) {
	return hash == Hash_NumAnimations;
}

#define CHECK_EXISTS(ptr) \
	if (!ptr || !ptr->IsLoaded()) { \
		VM_THROW_ERROR("Sprite is no longer loaded!"); \
		return true; \
	}

bool SpriteImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
	CHECK_EXISTS(resourceable);

	ISprite* sprite = (ISprite*)resourceable;

	/***
	 * \field NumAnimations
	 * \desc The amount of animations in the sprite.
	 * \ns SpriteResource
 	*/
	if (hash == Hash_NumAnimations) {
		*result = INTEGER_VAL((int)sprite->Animations.size());
	}

	return true;
}

bool SpriteImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Resourceable* resourceable = object ? GET_RESOURCEABLE(object) : nullptr;
	CHECK_EXISTS(resourceable);

	if (hash == Hash_NumAnimations) {
		VM_THROW_ERROR("Field cannot be written to!");
		return true;
	}
	else {
		return false;
	}

	return true;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

#define CHECK_ANIMATION_INDEX(idx) \
	if (idx < 0 || idx >= (int)sprite->Animations.size()) { \
		VM_OUT_OF_RANGE_ERROR("Animation index", idx, 0, sprite->Animations.size() - 1); \
		return NULL_VAL; \
	}

#define CHECK_ANIMFRAME_INDEX(anim, idx) \
	CHECK_ANIMATION_INDEX(anim); \
	if (idx < 0 || idx >= (int)sprite->Animations[anim].Frames.size()) { \
		VM_OUT_OF_RANGE_ERROR( \
			"Frame index", idx, 0, sprite->Animations[anim].Frames.size() - 1); \
		return NULL_VAL; \
	}

#undef CHECK_EXISTS
#define CHECK_EXISTS(ptr) \
	if (!ptr) { \
		return NULL_VAL; \
	} \
	if (!ptr->IsLoaded()) { \
		VM_THROW_ERROR("Sprite is no longer loaded!"); \
		return NULL_VAL; \
	}

VMValue SpriteImpl_GetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	const char* name = sprite->Animations[index].Name;
	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(name);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
VMValue SpriteImpl_GetAnimationIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	char* name = GET_ARG(1, GetString);
	for (size_t i = 0; i < sprite->Animations.size(); i++) {
		if (strcmp(name, sprite->Animations[i].Name) == 0) {
			return INTEGER_VAL((int)i);
		}
	}

	return NULL_VAL;
}
VMValue SpriteImpl_GetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].AnimationSpeed);
}
VMValue SpriteImpl_GetAnimationLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].FrameToLoop);
}
VMValue SpriteImpl_GetAnimationFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL((int)sprite->Animations[index].Frames.size());
}

void SpriteImpl::AddNatives() {
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationIndex);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationLoopIndex);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationFrameCount);
}
