#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ResourceableImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/SpriteImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* SpriteImpl::Class = nullptr;

Uint32 Hash_AnimationCount = 0;

#define CLASS_SPRITE "Sprite"

void SpriteImpl::Init() {
	Class = NewClass(CLASS_SPRITE);

	GET_STRING_HASH(AnimationCount);

	AddNatives();

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_SPRITE, Class);
	TypeImpl::DefinePrintableName(Class, "sprite");
}

bool SpriteImpl::IsValidField(Uint32 hash) {
	CHECK_VALID_FIELD(AnimationCount);

	return false;
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
	 * \field AnimationCount
	 * \desc The amount of animations in the sprite.
	 * \ns Sprite
 	*/
	if (hash == Hash_AnimationCount) {
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

	if (hash == Hash_AnimationCount) {
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
	if (sprite->Animations.size() == 0) { \
		throw ScriptException("Sprite has no animations!"); \
	} \
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
		throw ScriptException("Sprite is no longer loaded!"); \
	}

/***
 * \method GetAnimationName
 * \desc Gets the name of the specified animation index in the sprite.
 * \param animationIndex (Integer): The animation index.
 * \return Returns the name of the specified animation index.
 * \ns Sprite
 */
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
/***
 * \method GetAnimationIndex
 * \desc Gets the first animation in the sprite which matches the specified name.
 * \param name (String): The animation name to search for.
 * \return Returns the first animation index with the specified name, or <code>null</code> if there was no match.
 * \ns Sprite
 */
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
VMValue SpriteImpl_GetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID) {
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

#define GET_FRAME_PROPERTY(property) { \
	StandardLibrary::CheckArgCount(argCount, 3); \
	ISprite* sprite = GET_ARG(0, GetSprite); \
	CHECK_EXISTS(sprite); \
	int animation = GET_ARG(1, GetInteger); \
	int frame = GET_ARG(2, GetInteger); \
	CHECK_ANIMFRAME_INDEX(animation, frame); \
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].property); \
}

/***
 * \method GetFrameWidth
 * \desc Gets the frame width of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the frame width (in pixels) of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameWidth(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Width);
}
/***
 * \method GetFrameHeight
 * \desc Gets the frame height of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the frame height (in pixels) of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameHeight(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Height);
}
/***
 * \method GetFrameOffsetX
 * \desc Gets the X offset of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the X offset of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetX);
}
/***
 * \method GetFrameOffsetY
 * \desc Gets the Y offset of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the Y offset of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetY);
}
/***
 * \method GetFrameDuration
 * \desc Gets the frame duration of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the frame duration (in game frames) of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Duration);
}
/***
 * \method GetFrameID
 * \desc Gets the frame ID of the specified sprite frame.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the frame ID of the specified sprite frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Advance);
}

VMValue SpriteImpl_AddAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	char* name = GET_ARG(1, GetString);
	if (name) {
		sprite->AddAnimation(name, 1, 0);
		sprite->RefreshGraphicsID();

		return INTEGER_VAL((int)sprite->Animations.size() - 1);
	}

	return NULL_VAL;
}
VMValue SpriteImpl_RemoveAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	sprite->Animations.erase(sprite->Animations.begin() + index);
	sprite->RefreshGraphicsID();

	return NULL_VAL;
}

VMValue SpriteImpl_AddFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 6);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	int frameX = GET_ARG(2, GetInteger);
	int frameY = GET_ARG(3, GetInteger);
	int frameWidth = GET_ARG(4, GetInteger);
	int frameHeight = GET_ARG(5, GetInteger);

	// TODO: Add optional arguments
	int frameOffsetX = 0;
	int frameOffsetY = 0;
	int frameDuration = 1;
	int frameSheetNumber = 0;
	int frameID = 0;

	size_t numSpritesheets = sprite->Spritesheets.size();
	if (numSpritesheets == 0) {
		throw ScriptException("Cannot add a frame to a sprite without any spritesheets!");
	}
	else if (frameSheetNumber < 0 || frameSheetNumber >= (int)numSpritesheets) {
		VM_OUT_OF_RANGE_ERROR("Frame spritesheet", frameSheetNumber, 0, numSpritesheets - 1);
		return NULL_VAL;
	}

	Texture* sheet = sprite->Spritesheets[frameSheetNumber];
	if (frameX < 0 || frameX >= sheet->Width) {
		VM_OUT_OF_RANGE_ERROR("Frame X", frameX, 0, sheet->Width - 1);
		return NULL_VAL;
	}
	if (frameY < 0 || frameY >= sheet->Height) {
		VM_OUT_OF_RANGE_ERROR("Frame Y", frameY, 0, sheet->Height - 1);
		return NULL_VAL;
	}

	int maxFrameWidth = sheet->Width - frameX;
	int maxFrameHeight = sheet->Height - frameY;
	if (frameWidth < 0 || frameWidth > maxFrameWidth) {
		VM_OUT_OF_RANGE_ERROR("Frame width", frameWidth, 0, maxFrameWidth);
		return NULL_VAL;
	}
	if (frameHeight < 0 || frameHeight > maxFrameHeight) {
		VM_OUT_OF_RANGE_ERROR("Frame height", frameHeight, 0, maxFrameHeight);
		return NULL_VAL;
	}

	if (frameDuration < 0) {
		throw ScriptException("Frame duration cannot be lower than zero.");
	}

	sprite->AddFrame(index,
		frameDuration,
		frameX,
		frameY,
		frameWidth,
		frameHeight,
		frameOffsetX,
		frameOffsetY,
		frameID,
		frameSheetNumber);
	sprite->RefreshGraphicsID();

	return INTEGER_VAL((int)sprite->Animations[index].Frames.size() - 1);
}
VMValue SpriteImpl_RemoveFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	CHECK_ANIMFRAME_INDEX(animation, frame);

	sprite->RemoveFrame(animation, frame);
	sprite->RefreshGraphicsID();

	return NULL_VAL;
}

#undef GET_FRAME_PROPERTY

void SpriteImpl::AddNatives() {
	// Getting animation/frame properties
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationIndex);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationLoopFrame);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationFrameCount);

	DEF_CLASS_NATIVE(SpriteImpl, GetFrameWidth);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameHeight);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameOffsetX);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameOffsetY);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameDuration);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameID);

#if 0
	// Setting animation/frame properties
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationLoopFrame);

	DEF_CLASS_NATIVE(SpriteImpl, SetFrameWidth);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameHeight);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameOffsetX);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameOffsetY);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameDuration);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameID);
#endif

	// Adding or removing animations/frames
	DEF_CLASS_NATIVE(SpriteImpl, AddAnimation);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveAnimation);

	DEF_CLASS_NATIVE(SpriteImpl, AddFrame);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveFrame);
}
