#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/SpriteImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Value.h>

/***
* \class Sprite
* \desc A sprite Asset.
*/

ObjClass* SpriteImpl::Class = nullptr;

#define CLASS_SPRITE "Sprite"

DECLARE_STRING_HASH(AnimationCount);
DECLARE_STRING_HASH(SheetCount);

void SpriteImpl::Init() {
	Class = NewClass(CLASS_SPRITE);

	GET_STRING_HASH(AnimationCount);
	GET_STRING_HASH(SheetCount);

	AddNatives();

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_SPRITE, Class);
	TypeImpl::DefinePrintableName(Class, "sprite");
}

bool SpriteImpl::IsValidField(Uint32 hash) {
	CHECK_VALID_FIELD(AnimationCount);
	CHECK_VALID_FIELD(SheetCount);

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

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	CHECK_EXISTS(asset);

	ISprite* sprite = (ISprite*)asset;

	/***
	 * \field AnimationCount
	 * \type integer
	 * \desc The amount of animations in the sprite.
	 * \ns Sprite
 	*/
	if (hash == Hash_AnimationCount) {
		*result = INTEGER_VAL((int)sprite->Animations.size());
	}
	/***
	 * \field SheetCount
	 * \type integer
	 * \desc The amount of spritesheets in the sprite.
	 * \ns Sprite
 	*/
	else if (hash == Hash_SheetCount) {
		*result = INTEGER_VAL((int)sprite->Spritesheets.size());
	}

	return true;
}

bool SpriteImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	CHECK_EXISTS(asset);

	if (hash == Hash_AnimationCount || hash == Hash_SheetCount) {
		VM_THROW_ERROR("Field cannot be written to!");
		return true;
	}

	return false;
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
 * \param animationIndex (integer): The animation index.
 * \return string Returns the name of the specified animation index.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param name (string): The animation name to search for.
 * \return integer Returns the first animation index with the specified name, or `null` if there was no match.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	char* name = GET_ARG(1, GetString);

	CHECK_EXISTS(sprite);

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
 * \param animationIndex (integer): The animation index.
 * \return integer Returns the speed of the specified animation index.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].Speed);
}
/***
 * \method GetAnimationLoopFrame
 * \desc Gets the loop frame of the specified animation index.
 * \param animationIndex (integer): The animation index.
 * \return integer Returns the loop frame of the specified animation index.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL(sprite->Animations[index].FrameToLoop);
}
/***
 * \method GetAnimationFrameCount
 * \desc Gets the amount of frames in the specified animation.
 * \param animation (integer): The animation index to check.
 * \return integer Returns the frame count of the specified animation.
 * \ns Sprite
 */
VMValue SpriteImpl_GetAnimationFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL((int)sprite->Animations[index].Frames.size());
}

#define GET_FRAME_PROPERTY(property) { \
	StandardLibrary::CheckArgCount(argCount, 3); \
	ISprite* sprite = GET_ARG(0, GetSprite); \
	int animation = GET_ARG(1, GetInteger); \
	int frame = GET_ARG(2, GetInteger); \
	CHECK_EXISTS(sprite); \
	CHECK_ANIMFRAME_INDEX(animation, frame); \
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].property); \
}

/***
 * \method GetFrameX
 * \desc Gets the X position in the spritesheet of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the X position (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameX(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(X);
}
/***
 * \method GetFrameY
 * \desc Gets the Y position in the spritesheet of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the Y position (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameY(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Y);
}
/***
 * \method GetFrameWidth
 * \desc Gets the width of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return Returns the width (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameWidth(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Width);
}
/***
 * \method GetFrameHeight
 * \desc Gets the height of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the height (in pixels) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameHeight(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Height);
}
/***
 * \method GetFrameOffsetX
 * \desc Gets the X offset of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the X offset of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetX);
}
/***
 * \method GetFrameOffsetY
 * \desc Gets the Y offset of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the Y offset of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(OffsetY);
}
/***
 * \method GetFrameDuration
 * \desc Gets the duration of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the duration (in game frames) of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(Duration);
}
/***
 * \method GetFrameID
 * \desc Gets the ID of the specified animation frame.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the ID of the specified animation frame.
 * \ns Sprite
 */
VMValue SpriteImpl_GetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	GET_FRAME_PROPERTY(ID);
}

#undef GET_FRAME_PROPERTY

/***
 * \method IsFrameValid
 * \desc Checks if an animation and frame is valid within a sprite.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The frame index to check.
 * \return boolean Returns whether the animation and frame indexes are valid.
 * \ns Sprite
 */
VMValue SpriteImpl_IsFrameValid(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);

	CHECK_EXISTS(sprite);

	return (INTEGER_VAL((animation >= 0 && animation < (int)sprite->Animations.size()) &&
		(frame >= 0 && frame < (int)sprite->Animations[animation].Frames.size())));
}

#define CHECK_SHEET_INDEX(idx) \
	if (idx < 0 || idx >= (int)sprite->Spritesheets.size()) { \
		VM_OUT_OF_RANGE_ERROR( \
			"Sheet index", idx, 0, sprite->Spritesheets.size() - 1); \
		return NULL_VAL; \
	}

/***
 * \method GetSheetFilename
 * \desc Gets the filename of the specified spritesheet, if it has a filename.
 * \param sheetID (integer): The spritesheet index.
 * \return string Returns the filename of the specified spritesheet, or `null` if the given spritesheet is not named.
 * \ns Sprite
 */
VMValue SpriteImpl_GetSheetFilename(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int sheetID = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_SHEET_INDEX(sheetID);

	std::string sheetName = sprite->SpritesheetFilenames[sheetID];
	if (sheetName.size() > 0 && ScriptManager::Lock()) {
		ObjString* string = CopyString(sheetName);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
/***
 * \method GetSheetImage
 * \desc Gets the specified spritesheet as an image. This returns a copy of the spritesheet texture.
 * \param sheetID (integer): The spritesheet index.
 * \return <ref Image> Returns an Image.
 * \ns Sprite
 */
VMValue SpriteImpl_GetSheetImage(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int sheetID = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_SHEET_INDEX(sheetID);

	Texture* texture = sprite->Spritesheets[sheetID];
	Texture* newTexture = Graphics::CopyTexture(texture, texture->Access);

	Image* image = new Image(newTexture);

	return OBJECT_VAL(image->GetVMObject());
}

#undef CHECK_SHEET_INDEX

/***
 * \method SetAnimationName
 * \desc Sets the name of the specified animation index.
 * \param animationIndex (integer): The animation index.
 * \param name (string): The name to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);
	char* name = GET_ARG(2, GetString);

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(index);

	if (name) {
		sprite->Animations[index].Name = std::string(name);
	}

	return NULL_VAL;
}
/***
 * \method SetAnimationSpeed
 * \desc Sets the speed of the specified animation index.
 * \param animationIndex (integer): The animation index.
 * \param speed (integer): The speed to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);
	int speed = GET_ARG(2, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animationIndex (integer): The animation index.
 * \param loopFrame (integer): The loop frame to assign to the animation.
 * \ns Sprite
 */
VMValue SpriteImpl_SetAnimationLoopFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);
	int loopFrame = GET_ARG(2, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animation (integer): The animation index.
 * \param frame (integer): The frame index of the animation.
 * \param x (integer): The new X position of the frame.
 * \param y (integer): The new Y position of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFramePosition(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int x = GET_ARG(3, GetInteger);
	int y = GET_ARG(4, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animation (integer): The animation index.
 * \param frame (integer): The frame index of the animation.
 * \param width (integer): The new width of the frame.
 * \param height (integer): The new height of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameSize(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int width = GET_ARG(3, GetInteger);
	int height = GET_ARG(4, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animation (integer): The animation index.
 * \param frame (integer): The frame index of the animation.
 * \param offsetX (integer): The new X offset of the frame.
 * \param offsetY (integer): The new Y offset of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameOffset(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 5);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int xOffset = GET_ARG(3, GetInteger);
	int yOffset = GET_ARG(4, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animation (integer): The animation index.
 * \param frame (integer): The frame index of the animation.
 * \param duration (integer): The new duration of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int duration = GET_ARG(3, GetInteger);

	CHECK_EXISTS(sprite);
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
 * \param animation (integer): The animation index.
 * \param frame (integer): The frame index of the animation.
 * \param id (integer): The new ID of the frame.
 * \ns Sprite
 */
VMValue SpriteImpl_SetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int id = GET_ARG(3, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMFRAME_INDEX(animation, frame);

	AnimFrame& animFrame = sprite->Animations[animation].Frames[frame];

	animFrame.ID = id;

	return NULL_VAL;
}

/***
 * \method AddAnimation
 * \desc Adds an animation.
 * \param name (string): The name to give to the animation.
 * \return integer Returns the index of the new animation.
 * \ns Sprite
 */
VMValue SpriteImpl_AddAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	char* name = GET_ARG(1, GetString);

	CHECK_EXISTS(sprite);

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
 * \param animation (integer): The index of the animation to remove.
 * \ns Sprite
 */
VMValue SpriteImpl_RemoveAnimation(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(index);

	sprite->Animations.erase(sprite->Animations.begin() + index);
	sprite->RefreshGraphicsID();

	return NULL_VAL;
}

/***
 * \method AddFrame
 * \desc Adds a frame to an animation.
 * \param animation (integer): The animation index to add the frame into.
 * \param x (integer): X position of the frame in the spritesheet.
 * \param y (integer): Y position of the frame in the spritesheet.
 * \param width (integer): Width of the frame.
 * \param height (integer): Height position of the frame.
 * \return integer Returns the index of the new frame.
 * \ns Sprite
 */
VMValue SpriteImpl_AddFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 6);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
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

	CHECK_EXISTS(sprite);
	CHECK_ANIMATION_INDEX(animation);

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
 * \param animation (integer): The index of the animation.
 * \param frame (integer): The frame index of the animation to remove.
 * \ns Sprite
 */
VMValue SpriteImpl_RemoveFrame(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);

	CHECK_EXISTS(sprite);
	CHECK_ANIMFRAME_INDEX(animation, frame);

	sprite->RemoveFrame(animation, frame);
	sprite->RefreshGraphicsID();

	return NULL_VAL;
}

/***
 * \method GetHitboxName
 * \desc Gets the name of a hitbox through its index.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \param hitboxID (integer): The hitbox index to check.
 * \return string Returns a string value.
 * \ns Sprite
 */
VMValue SpriteImpl_GetHitboxName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);
	int hitboxID = GET_ARG(3, GetInteger);

	CHECK_ANIMATION_INDEX(animationID);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	if (frame.Boxes.size() == 0) {
		VM_THROW_ERROR("Frame %d of animation %d contains no hitboxes.", frameID, animationID);
		return NULL_VAL;
	}
	else if (!(hitboxID > -1 && hitboxID < frame.Boxes.size())) {
		VM_THROW_ERROR("Hitbox %d is not in bounds of frame %d of animation %d.",
			hitboxID,
			frameID,
			animationID);
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		ObjString* string = CopyString(frame.Boxes[hitboxID].Name);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
/***
 * \method GetHitboxIndex
 * \desc Gets the index of a hitbox by its name.
 * \param sprite (Sprite): The sprite to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \param name (string): The name of the hitbox to check.
 * \return integer Returns an integer value, or `null` if no such hitbox exists with that name.
 * \ns Sprite
 */
VMValue SpriteImpl_GetHitboxIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 4);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);
	char* name = GET_ARG(3, GetString);

	CHECK_ANIMATION_INDEX(animationID);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	if (name != nullptr) {
		AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

		for (size_t i = 0; i < frame.Boxes.size(); i++) {
			if (strcmp(frame.Boxes[i].Name.c_str(), name) == 0) {
				return INTEGER_VAL((int)i);
			}
		}
	}

	return NULL_VAL;
}
/***
 * \method GetHitboxCount
 * \desc Gets the hitbox count in the given frame of an animation of a sprite.
 * \param sprite (Sprite): The sprite to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns an integer value.
 * \ns Sprite
 */
VMValue SpriteImpl_GetHitboxCount(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);

	CHECK_ANIMATION_INDEX(animationID);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	size_t numHitboxes = frame.Boxes.size();

	return INTEGER_VAL((int)numHitboxes);
}
/***
 * \method GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frameID (integer): The frame index of the animation to check.
 * \paramOpt hitbox (string): The hitbox name.
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
/***
 * \method GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frameID (integer): The frame index of the animation to check.
 * \paramOpt hitbox (integer): The hitbox index. (default: `0`)
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
VMValue SpriteImpl_GetHitbox(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);
	int hitboxID = 0;

	CHECK_EXISTS(sprite);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	if (argCount > 1 && IS_STRING(args[1])) {
		char* name = GET_ARG(1, GetString);
		if (name) {
			int boxIndex = -1;

			for (size_t i = 0; i < frame.Boxes.size(); i++) {
				if (strcmp(frame.Boxes[i].Name.c_str(), name) == 0) {
					boxIndex = (int)i;
					break;
				}
			}

			if (boxIndex != -1) {
				hitboxID = boxIndex;
			}
			else {
				VM_THROW_ERROR("No hitbox named \"%s\" in frame %d of animation %d.",
					name,
					frameID,
					animationID);
			}
		}
	}
	else {
		hitboxID = GET_ARG_OPT(1, GetInteger, 0);
	}

	if (hitboxID < 0 || hitboxID >= (int)frame.Boxes.size()) {
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
 * \method GetTextArray
 * \desc Converts a string to an array of frame indexes by comparing codepoints to a frame's ID.
 * \param animation (integer): The animation index containing frames with codepoint ID values.
 * \param text (string): The text to convert.
 * \return array Returns an array of frame indexes per character in the text.
 * \ns Sprite
 */
VMValue SpriteImpl_GetTextArray(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	char* string = GET_ARG(2, GetString);

	ObjArray* textArray = NewArray();

	if (!sprite || !string) {
		return OBJECT_VAL(textArray);
	}

	if (animation >= 0 && animation < (int)sprite->Animations.size()) {
		std::vector<Uint32> codepoints = StringUtils::GetCodepoints(string);

		for (Uint32 codepoint : codepoints) {
			if (codepoint == (Uint32)-1) {
				textArray->Values->push_back(INTEGER_VAL(-1));
				continue;
			}

			bool found = false;
			for (int f = 0; f < (int)sprite->Animations[animation].Frames.size(); f++) {
				if (sprite->Animations[animation].Frames[f].ID ==
					(int)codepoint) {
					textArray->Values->push_back(INTEGER_VAL(f));
					found = true;
					break;
				}
			}

			if (!found) {
				textArray->Values->push_back(INTEGER_VAL(-1));
			}
		}
	}

	return OBJECT_VAL(textArray);
}
/***
 * \method GetTextWidth
 * \desc Gets the width (in pixels) of a converted sprite string.
 * \param animation (integer): The animation index.
 * \param text (array): The array containing frame indexes.
 * \param startIndex (integer): Where to start checking the width.
 * \param spacing (integer): The spacing (in pixels) between frames.
 * \ns Sprite
 */
VMValue SpriteImpl_GetTextWidth(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 6);

	if (ScriptManager::Lock()) {
		ISprite* sprite = GET_ARG(0, GetSprite);
		int animation = GET_ARG(1, GetInteger);
		ObjArray* text = GET_ARG(2, GetArray);
		int startIndex = GET_ARG(3, GetInteger);
		int length = GET_ARG(4, GetInteger);
		int spacing = GET_ARG(5, GetInteger);

		if (!sprite || !text->Values) {
			ScriptManager::Unlock();
			return INTEGER_VAL(0);
		}

		if (animation >= 0 && animation <= (int)sprite->Animations.size()) {
			Animation anim = sprite->Animations[animation];

			startIndex = (int)Math::Clamp(startIndex, 0, (int)text->Values->size() - 1);

			if (length <= 0 || length > (int)text->Values->size()) {
				length = (int)text->Values->size();
			}

			int w = 0;
			for (int c = startIndex; c < length; c++) {
				int charFrame =
					AS_INTEGER(Value::CastAsInteger((*text->Values)[c]));
				if (charFrame < anim.Frames.size()) {
					w += anim.Frames[charFrame].Width;
					if (c + 1 >= length) {
						ScriptManager::Unlock();
						return INTEGER_VAL(w);
					}

					w += spacing;
				}
			}

			ScriptManager::Unlock();
			return INTEGER_VAL(w);
		}
	}
	return INTEGER_VAL(0);
}

/***
 * \method MakePalettized
 * \desc Converts the sprite's colors to the ones in the specified palette index.
 * \param paletteIndex (integer): The palette index.
 * \ns Sprite
 */
VMValue SpriteImpl_MakePalettized(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int palIndex = GET_ARG(1, GetInteger);

	CHECK_EXISTS(sprite);

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
#undef CHECK_ANIMATION_INDEX
#undef CHECK_ANIMFRAME_INDEX
#undef CHECK_EXISTS

void SpriteImpl::AddNatives() {
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
	DEF_CLASS_NATIVE(SpriteImpl, IsFrameValid);
	DEF_CLASS_NATIVE(SpriteImpl, GetSheetFilename);
	DEF_CLASS_NATIVE(SpriteImpl, GetSheetImage);

	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationName);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationSpeed);
	DEF_CLASS_NATIVE(SpriteImpl, SetAnimationLoopFrame);
	DEF_CLASS_NATIVE(SpriteImpl, SetFramePosition);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameSize);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameOffset);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameDuration);
	DEF_CLASS_NATIVE(SpriteImpl, SetFrameID);

	DEF_CLASS_NATIVE(SpriteImpl, AddAnimation);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveAnimation);
	DEF_CLASS_NATIVE(SpriteImpl, AddFrame);
	DEF_CLASS_NATIVE(SpriteImpl, RemoveFrame);

	DEF_CLASS_NATIVE(SpriteImpl, GetHitboxName);
	DEF_CLASS_NATIVE(SpriteImpl, GetHitboxIndex);
	DEF_CLASS_NATIVE(SpriteImpl, GetHitboxCount);
	DEF_CLASS_NATIVE(SpriteImpl, GetHitbox);

	DEF_CLASS_NATIVE(SpriteImpl, GetTextArray);
	DEF_CLASS_NATIVE(SpriteImpl, GetTextWidth);

	DEF_CLASS_NATIVE(SpriteImpl, MakePalettized);
	DEF_CLASS_NATIVE(SpriteImpl, MakeNonPalettized);
}
