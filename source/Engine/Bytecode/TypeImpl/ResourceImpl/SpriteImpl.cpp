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
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

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
 * \desc Gets the name of the specified animation index.
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

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(sprite->Animations[index].Name);
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
		if (strcmp(name, sprite->Animations[i].Name.c_str()) == 0) {
			return INTEGER_VAL((int)i);
		}
	}

	return NULL_VAL;
}
/***
 * \method GetAnimationSpeed
 * \desc Gets the speed of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \return Returns the speed of the specified animation index.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].Speed);
}
/***
 * \method GetAnimationLoopFrame
 * \desc Gets the loop frame of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \return Returns the loop frame of the specified animation index.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].FrameToLoop);
}
/***
 * \method GetAnimationFrameCount
 * \desc Gets the amount of frames in the specified animation.
 * \param animation (Integer): The animation index to check.
 * \return Returns the frame count of the specified animation.
 * \ns Sprite
 */
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
 * \method GetFrameX
 * \desc Gets the X position in the spritesheet of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the X position (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameX(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(X);
}
/***
 * \method GetFrameY
 * \desc Gets the Y position in the spritesheet of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the Y position (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameY(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Y);
}
/***
 * \method GetFrameWidth
 * \desc Gets the width of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the width (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameWidth(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Width);
}
/***
 * \method GetFrameHeight
 * \desc Gets the height of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the height (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameHeight(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Height);
}
/***
 * \method GetFrameOffsetX
 * \desc Gets the X offset of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the X offset of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetX);
}
/***
 * \method GetFrameOffsetY
 * \desc Gets the Y offset of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the Y offset of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetY);
}
/***
 * \method GetFrameDuration
 * \desc Gets the duration of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the duration (in game frames) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Duration);
}
/***
 * \method GetFrameID
 * \desc Gets the ID of the specified animation frame.
 * \param animation (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the ID of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(ID);
}

#undef GET_FRAME_PROPERTY

/***
 * \method SetAnimationName
 * \desc Sets the name of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \param name (String): The name to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	char* name = GET_ARG(2, GetString);
	CHECK_ANIMATION_INDEX(index);

	if (name) {
		sprite->Animations[index].Name = std::string(name);
	}

	return NULL_VAL;
}
/***
 * \method SetAnimationSpeed
 * \desc Sets the speed of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \param speed (Integer): The speed to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	int speed = GET_ARG(2, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	if (speed < 0) {
		throw ScriptException("Animation speed cannot be lower than zero.");
	}

	sprite->Animations[index].Speed = speed;

	return NULL_VAL;
}
/***
 * \method SetAnimationLoopFrame
 * \desc Sets the loop frame of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \param loopFrame (Integer): The loop frame to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int index = GET_ARG(1, GetInteger);
	int loopFrame = GET_ARG(2, GetInteger);
	CHECK_ANIMATION_INDEX(index);

	size_t numFrames = sprite->Animations[index].Frames.size();
	if (numFrames == 0) {
		throw ScriptException("Cannot change loop frame of an animation with no frames!");
	}

	size_t maxFrame = numFrames - 1;
	if (loopFrame < 0 || loopFrame > (int)maxFrame) {
		VM_OUT_OF_RANGE_ERROR("Loop frame", loopFrame, 0, maxFrame);
		return NULL_VAL;
	}

	sprite->Animations[index].FrameToLoop = loopFrame;

	return NULL_VAL;
}

/***
 * \method SetFramePosition
 * \desc Changes the X and Y positions of the specified animation frame.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index of the animation.
 * \param x (Integer): The new X position of the frame.
 * \param y (Integer): The new Y position of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFramePosition(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int x = GET_ARG(3, GetInteger);
	int y = GET_ARG(4, GetInteger);

	CHECK_ANIMFRAME_INDEX(animation, frame);

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	Texture* sheet = sprite->Spritesheets[animFrame.SheetNumber];
	int maxFrameX = sheet->Width - animFrame.Width;
	int maxFrameY = sheet->Height - animFrame.Height;
	if (x < 0 || x >= maxFrameX) {
		VM_OUT_OF_RANGE_ERROR("Frame X", x, 0, maxFrameX - 1);
		return NULL_VAL;
	}
	if (y < 0 || y >= maxFrameY) {
		VM_OUT_OF_RANGE_ERROR("Frame Y", y, 0, maxFrameY - 1);
		return NULL_VAL;
	}

	animFrame.X = x;
	animFrame.Y = y;

	sprite->UpdateFrame(animation, frame);

	return NULL_VAL;
}
/***
 * \method SetFrameSize
 * \desc Changes the width and height of the specified animation frame.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index of the animation.
 * \param width (Integer): The new width of the frame.
 * \param height (Integer): The new height of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameSize(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int width = GET_ARG(3, GetInteger);
	int height = GET_ARG(4, GetInteger);

	CHECK_ANIMFRAME_INDEX(animation, frame);

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	Texture* sheet = sprite->Spritesheets[animFrame.SheetNumber];
	int maxFrameWidth = sheet->Width - animFrame.X;
	int maxFrameHeight = sheet->Height - animFrame.Y;
	if (width < 0 || width > maxFrameWidth) {
		VM_OUT_OF_RANGE_ERROR("Frame width", width, 0, maxFrameWidth);
		return NULL_VAL;
	}
	if (height < 0 || height > maxFrameHeight) {
		VM_OUT_OF_RANGE_ERROR("Frame height", height, 0, maxFrameHeight);
		return NULL_VAL;
	}

	animFrame.Width = width;
	animFrame.Height = height;

	sprite->UpdateFrame(animation, frame);

	return NULL_VAL;
}
/***
 * \method SetFrameOffset
 * \desc Changes the X and Y offsets of the specified animation frame.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index of the animation.
 * \param offsetX (Integer): The new X offset of the frame.
 * \param offsetY (Integer): The new Y offset of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameOffset(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int xOffset = GET_ARG(3, GetInteger);
	int yOffset = GET_ARG(4, GetInteger);

	CHECK_ANIMFRAME_INDEX(animation, frame);

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	animFrame.OffsetX = xOffset;
	animFrame.OffsetY = yOffset;

	sprite->UpdateFrame(animation, frame);

	return NULL_VAL;
}
/***
 * \method SetFrameDuration
 * \desc Changes the duration of the specified animation frame.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index of the animation.
 * \param duration (Integer): The new duration of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int duration = GET_ARG(3, GetInteger);

	CHECK_ANIMFRAME_INDEX(animation, frame);

	if (duration < 0) {
		throw ScriptException("Frame duration cannot be lower than zero.");
	}

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	animFrame.Duration = duration;

	return NULL_VAL;
}
/***
 * \method SetFrameID
 * \desc Changes the ID of the specified animation frame.
 * \param animation (Integer): The animation index.
 * \param frame (Integer): The frame index of the animation.
 * \param id (Integer): The new ID of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int id = GET_ARG(3, GetInteger);

	CHECK_ANIMFRAME_INDEX(animation, frame);

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	animFrame.ID = id;

	return NULL_VAL;
}

/***
 * \method AddAnimation
 * \desc Adds an animation.
 * \param name (String): The name to give to the animation.
 * \return Returns the index of the new animation.
 * \ns Sprite
 */
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
/***
 * \method RemoveAnimation
 * \desc Removes an animation.
 * \param animation (Integer): The index of the animation to remove.
 * \ns Sprite
 */
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

/***
 * \method AddFrame
 * \desc Adds a frame to an animation.
 * \param animation (Integer): The animation index to add the frame into.
 * \param x (Integer): X position of the frame in the spritesheet.
 * \param y (Integer): Y position of the frame in the spritesheet.
 * \param width (Integer): Width of the frame.
 * \param height (Integer): Height position of the frame.
 * \return Returns the index of the new frame.
 * \ns Sprite
 */
VMValue SpriteImpl_AddFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 6);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animation = GET_ARG(1, GetInteger);
	CHECK_ANIMATION_INDEX(animation);

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

	sprite->AddFrame(animation,
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

	return INTEGER_VAL((int)sprite->Animations[animation].Frames.size() - 1);
}
/***
 * \method RemoveFrame
 * \desc Removes a frame from an animation.
 * \param animation (Integer): The index of the animation.
 * \param frame (Integer): The frame index of the animation to remove.
 * \ns Sprite
 */
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

/***
 * \method GetHitbox
 * \desc Gets the hitbox of an animation and frame.
 * \param animationID (Integer): The animation index to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \paramOpt hitboxID (Integer): The hitbox index of the animation to check. Defaults to <code>0</code>.
 * \ns Sprite
 */
VMValue SpriteImpl_GetHitbox(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);
	int hitboxID = GET_ARG_OPT(3, GetInteger, 0);

	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];
	if (hitboxID < 0 || hitboxID >= frame.BoxCount) {
		VM_THROW_ERROR("Hitbox %d is not in bounds of frame %d.", hitboxID, frameID);
		return NULL_VAL;
	}

	CollisionBox box = frame.Boxes[hitboxID];
	ObjArray* hitbox = NewArray();
	hitbox->Values->push_back(INTEGER_VAL(box.Left));
	hitbox->Values->push_back(INTEGER_VAL(box.Top));
	hitbox->Values->push_back(INTEGER_VAL(box.Right));
	hitbox->Values->push_back(INTEGER_VAL(box.Bottom));
	return OBJECT_VAL(hitbox);
}
/***
 * \method MakePalettized
 * \desc Converts the sprite's colors to the ones in the specified palette index.
 * \param paletteIndex (Integer): The palette index.
 * \ns Sprite
 */
VMValue SpriteImpl_MakePalettized(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	int palIndex = GET_ARG(1, GetInteger);
	if (palIndex < 0 || palIndex >= MAX_PALETTE_COUNT) {
		VM_OUT_OF_RANGE_ERROR("Palette index", palIndex, 0, MAX_PALETTE_COUNT - 1);
		return NULL_VAL;
	}

	sprite->ConvertToPalette(palIndex);

	return NULL_VAL;
}
/***
 * \method MakeNonPalettized
 * \desc Removes the sprite's palette.
 * \ns Sprite
 */
VMValue SpriteImpl_MakeNonPalettized(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	ISprite* sprite = GET_ARG(0, GetSprite);
	CHECK_EXISTS(sprite);

	sprite->ConvertToRGBA();

	return NULL_VAL;
}

#undef GET_ARG
#undef GET_ARG_OPT
#undef CHECK_EXISTS

void SpriteImpl::AddNatives() {
	// Getting animation/frame properties
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationIndex);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationLoopFrame);
	DEF_CLASS_NATIVE(SpriteImpl, GetAnimationFrameCount);

	DEF_CLASS_NATIVE(SpriteImpl, GetFrameX);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameY);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameWidth);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameHeight);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameOffsetX);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameOffsetY);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameDuration);
	DEF_CLASS_NATIVE(SpriteImpl, GetFrameID);

	// Setting animation/frame properties
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationLoopFrame);

	DEF_CLASS_NATIVE(SpriteImpl, SetFramePosition);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameSize);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameOffset);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameDuration);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameID);

	// Adding or removing animations/frames
	DEF_CLASS_NATIVE(SpriteImpl, AddAnimation);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveAnimation);

	DEF_CLASS_NATIVE(SpriteImpl, AddFrame);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveFrame);

	// Miscellaneous methods
	DEF_CLASS_NATIVE(SpriteImpl, GetHitbox);
	DEF_CLASS_NATIVE(SpriteImpl, MakePalettized);
	DEF_CLASS_NATIVE(SpriteImpl, MakeNonPalettized);
}
