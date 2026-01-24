#include "Engine/Diagnostics/Log.h"
#include <Engine/Bytecode/StandardLibrary.h>

#include <Engine/Audio/AudioManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Bytecode/ScriptEntity.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/TypeImpl/FontImpl.h>
#include <Engine/Bytecode/TypeImpl/ShaderImpl.h>
#include <Engine/Bytecode/TypeImpl/StreamImpl.h>
#include <Engine/Bytecode/Value.h>
#include <Engine/Bytecode/ValuePrinter.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Error.h>
#include <Engine/Extensions/Discord.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Graphics.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/IO/Serializer.h>
#include <Engine/Includes/DateTime.h>
#include <Engine/Input/Controller.h>
#include <Engine/Input/Input.h>
#include <Engine/Math/Ease.h>
#include <Engine/Math/Geometry.h>
#include <Engine/Math/Math.h>
#include <Engine/Math/Random.h>
#include <Engine/Network/HTTP.h>
#include <Engine/Network/WebSocketClient.h>
#include <Engine/Platforms/Capability.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>
#include <Engine/Rendering/ViewTexture.h>
#include <Engine/ResourceTypes/ImageFormats/GIF.h>
#include <Engine/ResourceTypes/ImageFormats/PNG.h>
#include <Engine/ResourceTypes/ResourceManager.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/Scene.h>
#include <Engine/Scene/SceneEnums.h>
#include <Engine/Scene/SceneInfo.h>
#include <Engine/Utilities/ColorUtils.h>
#include <Engine/Utilities/StringUtils.h>

#include <Libraries/jsmn.h>

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 1
#define NANOPRINTF_VISIBILITY_STATIC 1

#include <Libraries/nanoprintf.h>

#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

#define CHECK_ARGCOUNT(expects) \
	if (argCount != expects) { \
		if (THROW_ERROR("Expected %d arguments but got %d.", expects, argCount) == \
			ERROR_RES_CONTINUE) \
			return NULL_VAL; \
		return NULL_VAL; \
	}
#define CHECK_AT_LEAST_ARGCOUNT(expects) \
	if (argCount < expects) { \
		if (THROW_ERROR("Expected at least %d arguments but got %d.", \
			    expects, \
			    argCount) == ERROR_RES_CONTINUE) \
			return NULL_VAL; \
		return NULL_VAL; \
	}
#define GET_ARG(argIndex, argFunction) (argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, argFunction) : argDefault)

// Get([0-9A-Za-z]+)\(([0-9A-Za-z]+), ([0-9A-Za-z]+)\)
// Get$1($2, $3, threadID)

namespace LOCAL {
inline int GetInteger(VMValue* args, int index, Uint32 threadID) {
	int value = 0;
	switch (args[index].Type) {
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		value = AS_INTEGER(args[index]);
		break;
	default:
		if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_INTEGER),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}
	}
	return value;
}
inline float GetDecimal(VMValue* args, int index, Uint32 threadID) {
	float value = 0.0f;
	switch (args[index].Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL:
		value = AS_DECIMAL(args[index]);
		break;
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER:
		value = AS_DECIMAL(Value::CastAsDecimal(args[index]));
		break;
	default:
		if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_DECIMAL),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}
	}
	return value;
}
inline char* GetString(VMValue* args, int index, Uint32 threadID) {
	char* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_STRING(args[index])) {
			value = AS_CSTRING(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_STRING),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Unlock();
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjString* GetVMString(VMValue* args, int index, Uint32 threadID) {
	ObjString* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_STRING(args[index])) {
			value = AS_STRING(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_STRING),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Unlock();
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjArray* GetArray(VMValue* args, int index, Uint32 threadID) {
	ObjArray* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_ARRAY(args[index])) {
			value = (ObjArray*)(AS_OBJECT(args[index]));
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_ARRAY),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjMap* GetMap(VMValue* args, int index, Uint32 threadID) {
	ObjMap* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_MAP(args[index])) {
			value = (ObjMap*)(AS_OBJECT(args[index]));
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_MAP),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline CollisionBox GetHitbox(VMValue* args, int index, Uint32 threadID) {
	CollisionBox box;
	if (IS_HITBOX(args[index])) {
		Sint16* values = AS_HITBOX(args[index]);
		box.Left = values[HITBOX_LEFT];
		box.Top = values[HITBOX_TOP];
		box.Right = values[HITBOX_RIGHT];
		box.Bottom = values[HITBOX_BOTTOM];
	}
	else if (IS_ARRAY(args[index])) {
		if (ScriptManager::Lock()) {
			Sint16 values[NUM_HITBOX_SIDES];

			ObjArray* array = AS_ARRAY(args[index]);

			if (array->Values->size() != NUM_HITBOX_SIDES) {
				if (THROW_ERROR("Expected array to have %d elements instead of %d.",
					NUM_HITBOX_SIDES,
					array->Values->size()) == ERROR_RES_CONTINUE) {
					ScriptManager::Threads[threadID].ReturnFromNative();
				}
				ScriptManager::Unlock();
				return box;
			}

			for (int i = 0; i < NUM_HITBOX_SIDES; i++) {
				VMValue value = (*array->Values)[i];
				if (!IS_INTEGER(value)) {
					THROW_ERROR(
						"Expected value at index %d to be of type %s instead of %s.",
						i,
						GetTypeString(VAL_INTEGER),
						GetValueTypeString(value));
					ScriptManager::Unlock();
					return box;
				}
				values[i] = AS_INTEGER(value);
			}

			box.Left = values[HITBOX_LEFT];
			box.Top = values[HITBOX_TOP];
			box.Right = values[HITBOX_RIGHT];
			box.Bottom = values[HITBOX_BOTTOM];

			ScriptManager::Unlock();
		}
	}
	else {
		if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			    index + 1,
			    GetTypeString(VAL_HITBOX),
			    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}
	}
	return box;
}
inline ObjBoundMethod* GetBoundMethod(VMValue* args, int index, Uint32 threadID) {
	ObjBoundMethod* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_BOUND_METHOD(args[index])) {
			value = (ObjBoundMethod*)(AS_OBJECT(args[index]));
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_BOUND_METHOD),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjFunction* GetFunction(VMValue* args, int index, Uint32 threadID) {
	ObjFunction* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_FUNCTION(args[index])) {
			value = (ObjFunction*)(AS_OBJECT(args[index]));
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_FUNCTION),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline VMValue GetCallable(VMValue* args, int index, Uint32 threadID) {
	VMValue value = NULL_VAL;
	if (ScriptManager::Lock()) {
		if (IS_CALLABLE(args[index])) {
			value = args[index];
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type callable instead of %s.",
				    index + 1,
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjInstance* GetInstance(VMValue* args, int index, Uint32 threadID) {
	ObjInstance* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_INSTANCEABLE(args[index])) {
			value = AS_INSTANCE(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_INSTANCE),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjEntity* GetEntity(VMValue* args, int index, Uint32 threadID) {
	ObjEntity* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_ENTITY(args[index])) {
			value = AS_ENTITY(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    GetObjectTypeString(OBJ_ENTITY),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjStream* GetStream(VMValue* args, int index, Uint32 threadID) {
	ObjStream* value = NULL;
	if (ScriptManager::Lock()) {
		if (IS_STREAM(args[index])) {
			value = AS_STREAM(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetObjectTypeName(StreamImpl::Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjShader* GetShader(VMValue* args, int index, Uint32 threadID) {
	ObjShader* value = nullptr;
	if (ScriptManager::Lock()) {
		if (IS_SHADER(args[index])) {
			value = AS_SHADER(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetObjectTypeName(ShaderImpl::Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}
inline ObjFont* GetFont(VMValue* args, int index, Uint32 threadID) {
	ObjFont* value = nullptr;
	if (ScriptManager::Lock()) {
		if (IS_FONT(args[index])) {
			value = AS_FONT(args[index]);
		}
		else {
			if (THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
				    index + 1,
				    Value::GetObjectTypeName(FontImpl::Class),
				    GetValueTypeString(args[index])) == ERROR_RES_CONTINUE) {
				ScriptManager::Threads[threadID].ReturnFromNative();
			}
		}
		ScriptManager::Unlock();
	}
	return value;
}

inline ISprite* GetSpriteIndex(int where, Uint32 threadID) {
	if (where < 0 || where >= (int)Scene::SpriteList.size()) {
		if (THROW_ERROR("Sprite index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::SpriteList[where]) {
		return NULL;
	}

	return Scene::SpriteList[where]->AsSprite;
}
inline ISprite* GetSprite(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	return GetSpriteIndex(where, threadID);
}
inline Image* GetImage(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::ImageList.size()) {
		if (THROW_ERROR("Image index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::ImageList[where]) {
		return NULL;
	}

	return Scene::ImageList[where]->AsImage;
}
inline GameTexture* GetTexture(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::TextureList.size()) {
		if (THROW_ERROR("Texture index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::TextureList[where]) {
		return NULL;
	}

	return Scene::TextureList[where];
}
inline ISound* GetSound(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::SoundList.size()) {
		if (THROW_ERROR("Sound index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::SoundList[where]) {
		return NULL;
	}

	return Scene::SoundList[where]->AsSound;
}
inline ISound* GetMusic(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::MusicList.size()) {
		if (THROW_ERROR("Music index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::MusicList[where]) {
		return NULL;
	}

	return Scene::MusicList[where]->AsMusic;
}
inline IModel* GetModel(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::ModelList.size()) {
		if (THROW_ERROR("Model index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::ModelList[where]) {
		return NULL;
	}

	return Scene::ModelList[where]->AsModel;
}
inline MediaBag* GetVideo(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::MediaList.size()) {
		if (THROW_ERROR("Video index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::MediaList[where]) {
		return NULL;
	}

	return Scene::MediaList[where]->AsMedia;
}
inline Animator* GetAnimator(VMValue* args, int index, Uint32 threadID) {
	int where = GetInteger(args, index, threadID);
	if (where < 0 || where >= (int)Scene::AnimatorList.size()) {
		if (THROW_ERROR("Animator index \"%d\" outside bounds of list.", where) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}

		return NULL;
	}

	if (!Scene::AnimatorList[where]) {
		return NULL;
	}

	return Scene::AnimatorList[where];
}
} // namespace LOCAL

// NOTE:
// Integers specifically need to be whole integers.
// Floats can be just any countable real number.
int StandardLibrary::GetInteger(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetInteger(args, index, threadID);
}
float StandardLibrary::GetDecimal(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetDecimal(args, index, threadID);
}
char* StandardLibrary::GetString(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetString(args, index, threadID);
}
ObjString* StandardLibrary::GetVMString(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetVMString(args, index, threadID);
}
ObjArray* StandardLibrary::GetArray(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetArray(args, index, threadID);
}
ObjMap* StandardLibrary::GetMap(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetMap(args, index, threadID);
}
ISprite* StandardLibrary::GetSprite(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetSprite(args, index, threadID);
}
Image* StandardLibrary::GetImage(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetImage(args, index, threadID);
}
ISound* StandardLibrary::GetSound(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetSound(args, index, threadID);
}
ObjInstance* StandardLibrary::GetInstance(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetInstance(args, index, threadID);
}
ObjEntity* StandardLibrary::GetEntity(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetEntity(args, index, threadID);
}
ObjFunction* StandardLibrary::GetFunction(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetFunction(args, index, threadID);
}
ObjShader* StandardLibrary::GetShader(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetShader(args, index, threadID);
}
ObjFont* StandardLibrary::GetFont(VMValue* args, int index, Uint32 threadID) {
	return LOCAL::GetFont(args, index, threadID);
}

void StandardLibrary::CheckArgCount(int argCount, int expects) {
	Uint32 threadID = 0;
	if (argCount != expects) {
		if (THROW_ERROR("Expected %d arguments but got %d.", expects, argCount) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}
	}
}
void StandardLibrary::CheckAtLeastArgCount(int argCount, int expects) {
	Uint32 threadID = 0;
	if (argCount < expects) {
		if (THROW_ERROR("Expected at least %d arguments but got %d.", expects, argCount) ==
			ERROR_RES_CONTINUE) {
			ScriptManager::Threads[threadID].ReturnFromNative();
		}
	}
}

using namespace LOCAL;

Uint8 String_ToUpperCase_Map_ExtendedASCII[0x100];
Uint8 String_ToLowerCase_Map_ExtendedASCII[0x100];

typedef float MatrixHelper[4][4];

void MatrixHelper_CopyFrom(MatrixHelper* helper, ObjArray* array) {
	float* fArray = (float*)helper;
	for (int i = 0; i < 16; i++) {
		fArray[i] = AS_DECIMAL((*array->Values)[i]);
	}
}
void MatrixHelper_CopyTo(MatrixHelper* helper, ObjArray* array) {
	float* fArray = (float*)helper;
	for (int i = 0; i < 16; i++) {
		(*array->Values)[i] = DECIMAL_VAL(fArray[i]);
	}
}

VMValue ReturnString(const char* str) {
	if (str && ScriptManager::Lock()) {
		ObjString* string = CopyString(str);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}
	return NULL_VAL;
}

VMValue ReturnString(std::string str) {
	return ReturnString(str.c_str());
}

void AddToMap(ObjMap* map, const char* key, VMValue value) {
	char* keyString = StringUtils::Duplicate(key);
	Uint32 hash = map->Keys->HashFunction(keyString, strlen(keyString));
	map->Keys->Put(hash, keyString);
	map->Values->Put(hash, value);
}

float textAlign;
float textBaseline;
float textAscent;
float textAdvance;

#define CHECK_READ_STREAM \
	if (stream->Closed) { \
		THROW_ERROR("Cannot read closed stream!"); \
		return NULL_VAL; \
	}
#define CHECK_WRITE_STREAM \
	if (stream->Closed) { \
		THROW_ERROR("Cannot write to closed stream!"); \
		return NULL_VAL; \
	} \
	if (!stream->Writable) { \
		THROW_ERROR("Cannot write to read-only stream!"); \
		return NULL_VAL; \
	}

#define OUT_OF_RANGE_ERROR(eType, eIdx, eMin, eMax) \
	THROW_ERROR(eType " %d out of range. (%d - %d)", eIdx, eMin, eMax)

#define CHECK_PALETTE_INDEX(index) \
	if (index < 0 || index >= MAX_PALETTE_COUNT) { \
		OUT_OF_RANGE_ERROR("Palette index", index, 0, MAX_PALETTE_COUNT - 1); \
		return NULL_VAL; \
	}

#define CHECK_SCENE_LAYER_INDEX(layerIdx) \
	if (layerIdx < 0 || layerIdx >= (int)Scene::Layers.size()) { \
		OUT_OF_RANGE_ERROR("Layer index", layerIdx, 0, (int)Scene::Layers.size() - 1); \
		return NULL_VAL; \
	}

#define CHECK_INPUT_PLAYER_INDEX(playerNum) \
	if (playerNum < 0 || playerNum >= InputManager::GetPlayerCount()) { \
		OUT_OF_RANGE_ERROR( \
			"Player index", playerNum, 0, InputManager::GetPlayerCount() - 1); \
		return NULL_VAL; \
	}

#define CHECK_INPUT_DEVICE(deviceType) \
	if (deviceType < 0 || deviceType >= (int)InputDevice_MAX) { \
		OUT_OF_RANGE_ERROR("Input device", deviceType, 0, (int)InputDevice_MAX - 1); \
		return NULL_VAL; \
	}

#define CHECK_INPUT_BIND_TYPE(bindType) \
	if (bindType < 0 || bindType >= NUM_INPUT_BIND_TYPES) { \
		OUT_OF_RANGE_ERROR("Input bind type", bindType, 0, NUM_INPUT_BIND_TYPES - 1); \
		return NULL_VAL; \
	}

#define CHECK_KEYBOARD_KEY(kbKey) \
	if (kbKey < 0 || kbKey >= NUM_KEYBOARD_KEYS) { \
		OUT_OF_RANGE_ERROR("Keyboard key", kbKey, 0, NUM_KEYBOARD_KEYS - 1); \
		return NULL_VAL; \
	}

#define CHECK_CONTROLLER_BUTTON(controllerButton) \
	if (controllerButton < 0 || controllerButton >= (int)ControllerButton::Max) { \
		OUT_OF_RANGE_ERROR( \
			"Controller button", controllerButton, 0, (int)ControllerButton::Max - 1); \
		return NULL_VAL; \
	}

#define CHECK_CONTROLLER_AXIS(controllerAxis) \
	if (controllerAxis < 0 || controllerAxis >= (int)ControllerAxis::Max) { \
		OUT_OF_RANGE_ERROR( \
			"Controller axis", controllerAxis, 0, (int)ControllerAxis::Max - 1); \
		return NULL_VAL; \
	}

#define CHECK_CONTROLLER_INDEX(idx) \
	if (InputManager::NumControllers == 0) { \
		THROW_ERROR("No controllers are connected."); \
		return NULL_VAL; \
	} \
	else if (idx < 0 || idx >= InputManager::NumControllers) { \
		OUT_OF_RANGE_ERROR("Controller index", idx, 0, InputManager::NumControllers - 1); \
		return NULL_VAL; \
	}

// #region Animator
// return true if we found it in the list
bool GetAnimatorSpace(vector<Animator*>* list, size_t* index, bool* foundEmpty) {
	*foundEmpty = false;
	*index = list->size();
	for (size_t i = 0, listSz = list->size(); i < listSz; i++) {
		if (!(*list)[i]) {
			if (!(*foundEmpty)) {
				*foundEmpty = true;
				*index = i;
			}
			continue;
		}
	}
	return false;
}
/***
 * Animator.Create
 * \desc Creates a new animator.
 * \paramOpt sprite (integer): The index of the sprite.
 * \paramOpt animationID (integer): Which animation to use.
 * \paramOpt frameID (integer): Which frame to use.
 * \paramOpt unloadPolicy (integer): When to unload the animator.
 * \return integer Returns the index of the Animator.
 * \ns Animator
 */
VMValue Animator_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(0);

	Animator* animator = new Animator();
	animator->Sprite = argCount >= 1 ? GET_ARG(0, GetInteger) : -1;
	animator->CurrentAnimation = argCount >= 2 ? GET_ARG(1, GetInteger) : -1;
	animator->CurrentFrame = argCount >= 3 ? GET_ARG(2, GetInteger) : -1;
	animator->UnloadPolicy = argCount >= 4 ? GET_ARG(3, GetInteger) : SCOPE_SCENE;

	size_t index = 0;
	bool emptySlot = false;
	vector<Animator*>* list = &Scene::AnimatorList;
	if (GetAnimatorSpace(list, &index, &emptySlot)) {
		return INTEGER_VAL((int)index);
	}
	else if (emptySlot) {
		(*list)[index] = animator;
	}
	else {
		list->push_back(animator);
	}

	return INTEGER_VAL((int)index);
}
/***
 * Animator.Remove
 * \desc Removes an animator.
 * \param animator (integer): The index of the animator.
 * \ns Animator
 */
VMValue Animator_Remove(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int animator = GET_ARG(0, GetInteger);
	if (animator < 0 || animator >= (int)Scene::AnimatorList.size()) {
		return NULL_VAL;
	}
	if (!Scene::AnimatorList[animator]) {
		return NULL_VAL;
	}
	delete Scene::AnimatorList[animator];
	Scene::AnimatorList[animator] = NULL;
	return NULL_VAL;
}
/***
 * Animator.SetAnimation
 * \desc Sets the current animation and frame of an animator.
 * \param animator (integer): The index of the animator.
 * \param sprite (integer): The index of the sprite.
 * \param animationID (integer): The animator's changed animation ID.
 * \param frameID (integer): The animator's changed frame ID.
 * \param forceApply (boolean): Whether to force the animation to go back to the frame if the animation is the same as the current animation.
 * \ns Animator
 */
VMValue Animator_SetAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	Animator* animator = GET_ARG(0, GetAnimator);
	int spriteIndex = GET_ARG(1, GetInteger);
	int animationID = GET_ARG(2, GetInteger);
	int frameID = GET_ARG(3, GetInteger);
	int forceApply = GET_ARG(4, GetInteger);

	if (!animator) {
		return NULL_VAL;
	}

	if (spriteIndex < 0 || spriteIndex >= (int)Scene::SpriteList.size() || !animator) {
		if (animator) {
			animator->Frames.clear();
			animator->Sprite = -1;
			animator->CurrentAnimation = -1;
			animator->CurrentFrame = -1;
		}
		return NULL_VAL;
	}

	ISprite* sprite = Scene::SpriteList[spriteIndex]->AsSprite;
	if (!sprite || animationID < 0 || animationID >= (int)sprite->Animations.size()) {
		animator->CurrentAnimation = -1;
		return NULL_VAL;
	}

	if (frameID < 0 || frameID >= (int)sprite->Animations[animationID].Frames.size()) {
		animator->CurrentFrame = -1;
		return NULL_VAL;
	}

	Animation anim = sprite->Animations[animationID];
	vector<AnimFrame> frames = anim.Frames;

	if (animator->CurrentAnimation == animationID && !forceApply) {
		return NULL_VAL;
	}

	animator->Frames = frames;
	animator->AnimationTimer = 0;
	animator->Sprite = spriteIndex;
	animator->CurrentFrame = frameID;
	animator->FrameCount = anim.FrameCount;
	animator->Duration = animator->Frames[frameID].Duration;
	animator->AnimationSpeed = anim.AnimationSpeed;
	animator->RotationStyle = anim.Flags;
	animator->LoopIndex = anim.FrameToLoop;
	animator->PrevAnimation = animator->CurrentAnimation;
	animator->CurrentAnimation = animationID;

	if (animator->RotationStyle == ROTSTYLE_STATICFRAMES) {
		animator->FrameCount >>= 1;
	}

	return NULL_VAL;
}
/***
 * Animator.Animate
 * \desc Animates an animator.
 * \param animator (integer): The index of the animator.
 * \ns Animator
 */
VMValue Animator_Animate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);

	if (!animator || !animator->Frames.size()) {
		return NULL_VAL;
	}

	ResourceType* resource = Scene::GetSpriteResource(animator->Sprite);
	if (!resource) {
		return NULL_VAL;
	}

	ISprite* sprite = resource->AsSprite;
	if (!sprite) {
		return NULL_VAL;
	}

	if (animator->CurrentAnimation < 0 ||
		animator->CurrentAnimation >= (int)sprite->Animations.size() ||
		animator->CurrentFrame < 0 ||
		animator->CurrentFrame >=
			(int)sprite->Animations[animator->CurrentAnimation].Frames.size()) {
		return NULL_VAL;
	}

	animator->AnimationTimer += animator->AnimationSpeed;

	// TODO: Animate Retro Model if Frames = AnimFrame* 1 (no size?), else:
	while (animator->Duration && animator->AnimationTimer > animator->Duration) {
		++animator->CurrentFrame;

		animator->AnimationTimer -= animator->Duration;
		if (animator->CurrentFrame >= animator->FrameCount) {
			animator->CurrentFrame = animator->LoopIndex;
		}

		animator->Duration = animator->Frames[animator->CurrentFrame].Duration;
	}

	return NULL_VAL;
}
/***
 * Animator.GetSprite
 * \desc Gets the sprite index of an animator.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetSprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->Sprite);
}
/***
 * Animator.GetCurrentAnimation
 * \desc Gets the current animation value of an animator.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetCurrentAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->CurrentAnimation);
}
/***
 * Animator.GetCurrentFrame
 * \desc Gets the current animation value of an animator.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetCurrentFrame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->CurrentFrame);
}
/***
 * Animator.GetHitbox
 * \desc Gets the hitbox of an animation and frame of an animator.
 * \param animator (integer): The index of the animator.
 * \paramOpt hitboxID (integer): The index number of the hitbox. (default: `0`)
 * \return hitbox Returns a Hitbox value.
 * \ns Animator
 */
VMValue Animator_GetHitbox(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	int hitboxID = GET_ARG_OPT(1, GetInteger, 0);
	// Do not throw errors here because Animators are allowed to have negative sprite, animation, and frame indexes
	if (animator && animator->Sprite >= 0 && animator->CurrentAnimation >= 0 &&
		animator->CurrentFrame >= 0) {
		ISprite* sprite = GetSpriteIndex(animator->Sprite, threadID);
		if (!sprite) {
			return NULL_VAL;
		}

		if (animator->CurrentAnimation > sprite->Animations.size()) {
			return NULL_VAL;
		}

		if (animator->CurrentFrame >
			sprite->Animations[animator->CurrentFrame].Frames.size()) {
			return NULL_VAL;
		}

		AnimFrame frame = sprite->Animations[animator->CurrentAnimation]
					  .Frames[animator->CurrentFrame];

		if (frame.Boxes.size() == 0) {
			THROW_ERROR("Frame %d of animation %d contains no hitboxes.",
				animator->CurrentFrame,
				animator->CurrentAnimation);
			return NULL_VAL;
		}
		else if (!(hitboxID > -1 && hitboxID < frame.Boxes.size())) {
			THROW_ERROR("Hitbox %d is not in bounds of frame %d of animation %d.",
				hitboxID,
				animator->CurrentFrame,
				animator->CurrentAnimation);
			return NULL_VAL;
		}

		CollisionBox box = frame.Boxes[hitboxID];
		return HITBOX_VAL(box.Left, box.Top, box.Right, box.Bottom);
	}
	else {
		return NULL_VAL;
	}
}
/***
 * Animator.GetPrevAnimation
 * \desc Gets the previous animation value of an animator.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetPrevAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->PrevAnimation);
}
/***
 * Animator.GetAnimationSpeed
 * \desc Gets the animation speed of an animator's current animation.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->AnimationSpeed);
}
/***
 * Animator.GetAnimationTimer
 * \desc Gets the animation timer of an animator.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetAnimationTimer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->AnimationTimer);
}
/***
 * Animator.GetDuration
 * \desc Gets the frame duration of an animator's current frame.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetDuration(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->Duration);
}
/***
 * Animator.GetFrameCount
 * \desc Gets the frame count of an animator's current animation.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->FrameCount);
}
/***
 * Animator.GetLoopIndex
 * \desc Gets the loop index of an animator's current animation.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->LoopIndex);
}
/***
 * Animator.GetRotationStyle
 * \desc Gets the loop index of an animator's rotation style.
 * \param animator (integer): The index of the animator.
 * \return integer Returns an integer value.
 * \ns Animator
 */
VMValue Animator_GetRotationStyle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(animator->RotationStyle);
}
/***
 * Animator.SetSprite
 * \desc Sets the sprite index of an animator.
 * \param animator (integer): The animator index to change.
 * \param spriteID (integer): The sprite ID.
 * \ns Animator
 */
VMValue Animator_SetSprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->Sprite = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetCurrentAnimation
 * \desc Sets the current animation of an animator.
 * \param animator (integer): The animator index to change.
 * \param animationID (integer): The animation ID.
 * \ns Animator
 */
VMValue Animator_SetCurrentAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->CurrentAnimation = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetCurrentFrame
 * \desc Sets the current frame of an animator.
 * \param animator (integer): The animator index to change.
 * \param frameID (integer): The frame ID.
 * \ns Animator
 */
VMValue Animator_SetCurrentFrame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->CurrentFrame = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetPrevAnimation
 * \desc Sets the previous animation of an animator.
 * \param animator (integer): The animator index to change.
 * \param prevAnimationID (integer): The animation ID.
 * \ns Animator
 */
VMValue Animator_SetPrevAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->PrevAnimation = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetAnimationSpeed
 * \desc Sets the animation speed of an animator.
 * \param animator (integer): The animator index to change.
 * \param speed (integer): The animation speed.
 * \ns Animator
 */
VMValue Animator_SetAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->AnimationSpeed = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetAnimationTimer
 * \desc Sets the animation timer of an animator.
 * \param animator (integer): The animator index to change.
 * \param timer (integer): The animation timer.
 * \ns Animator
 */
VMValue Animator_SetAnimationTimer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->AnimationTimer = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetDuration
 * \desc Sets the frame duration of an animator.
 * \param animator (integer): The animator index to change.
 * \param duration (integer): The animator's changed duration.
 * \ns Animator
 */
VMValue Animator_SetDuration(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->Duration = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetFrameCount
 * \desc Sets the frame count of an animator.
 * \param animator (integer): The animator index to change.
 * \param frameCount (integer): The animator's changed frame count.
 * \ns Animator
 */
VMValue Animator_SetFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->FrameCount = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetLoopIndex
 * \desc Sets the loop index of an animator.
 * \param animator (integer): The animator index to change.
 * \param loopIndex (integer): The animator's changed loop index.
 * \ns Animator
 */
VMValue Animator_SetLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->LoopIndex = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.SetRotationStyle
 * \desc Sets the rotation style of an animator.
 * \param animator (integer): The animator index to change.
 * \param rotationStyle (integer): The animator's changed rotation style.
 * \ns Animator
 */
VMValue Animator_SetRotationStyle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->RotationStyle = GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustCurrentAnimation
 * \desc Adjusts the current animation of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's animation ID.
 * \ns Animator
 */
VMValue Animator_AdjustCurrentAnimation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->CurrentAnimation += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustCurrentFrame
 * \desc Adjusts the current frame of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's frame ID.
 * \ns Animator
 */
VMValue Animator_AdjustCurrentFrame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->CurrentFrame += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustAnimationSpeed
 * \desc Adjusts the animation speed of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's animation speed.
 * \ns Animator
 */
VMValue Animator_AdjustAnimationSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->AnimationSpeed += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustAnimationTimer
 * \desc Adjusts the animation timer of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's animation timer.
 * \ns Animator
 */
VMValue Animator_AdjustAnimationTimer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->AnimationTimer += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustDuration
 * \desc Adjusts the duration of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's duration.
 * \ns Animator
 */
VMValue Animator_AdjustDuration(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->Duration += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustFrameCount
 * \desc Adjusts the frame count of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's duration.
 * \ns Animator
 */
VMValue Animator_AdjustFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->FrameCount += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
/***
 * Animator.AdjustLoopIndex
 * \desc Adjusts the loop index of an animator by an amount.
 * \param animator (integer): The animator index to change.
 * \param amount (integer): The amount to adjust the animator's loop index.
 * \ns Animator
 */
VMValue Animator_AdjustLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Animator* animator = GET_ARG(0, GetAnimator);
	if (!animator) {
		return NULL_VAL;
	}
	animator->LoopIndex += GET_ARG(1, GetInteger);
	return NULL_VAL;
}
// #endregion

// #region API.Discord
/***
 * API.Discord.Init
 * \desc Initializes Discord integration.
 * \param applicationID (string): The Discord Application ID. This will have needed to be created via the Discord Developer Portal.
 * \return integer Returns an API response code.
 * \ns API.Discord
 */
VMValue Discord_Init(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* applicationID = GET_ARG(0, GetString);
	int result = Discord::Init(applicationID);
	return INTEGER_VAL(result);
}
/***
 * API.Discord.UpdateRichPresence
 * \desc Updates Discord Rich Presence.<br/>\
The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The first line of text in the Rich Presence.
 * \paramOpt state (string): The second line of text, appearing below details.
 * \paramOpt largeImageKey (string): The internal name of the large image asset to display, created via the Discord Developer Portal.
 * \paramOpt smallImageKey (string): The internal name of the small image asset to display, also created via the Discord Developer Portal.
 * \ns API.Discord
 */
/***
 * API.Discord.UpdateRichPresence
 * \desc Updates Discord Rich Presence.<br/>\
The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The first line of text in the Rich Presence.
 * \paramOpt state (string): The second line of text, appearing below details.
 * \paramOpt largeImageKey (string): The internal name of the large image asset to display, created via the Discord Developer Portal.
 * \paramOpt startTime (integer): A Unix timestamp of when the activity started. If `0`, the timer is disabled.
 * \ns API.Discord
 */
/***
 * API.Discord.UpdateRichPresence
 * \desc Updates Discord Rich Presence.<br/>\
The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The first line of text in the Rich Presence.
 * \paramOpt state (string): The second line of text, appearing below details.
 * \paramOpt largeImageKey (string): The internal name of the large image asset to display, created via the Discord Developer Portal.
 * \paramOpt smallImageKey (string): The internal name of the small image asset to display, also created via the Discord Developer Portal.
 * \paramOpt startTime (integer): A Unix timestamp of when the activity started. If `0`, the timer is disabled.
 * \ns API.Discord
 */
VMValue Discord_UpdateRichPresence(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	const char* details = GET_ARG(0, GetString);
	const char* state = GET_ARG_OPT(1, GetString, nullptr);
	const char* largeImageKey = GET_ARG_OPT(2, GetString, nullptr);
	const char* smallImageKey = nullptr;
	int startTime = 0;

	if (argCount == 4) {
		if (IS_INTEGER(args[3]))
			startTime = GET_ARG(3, GetInteger);
		else
			smallImageKey = GET_ARG(3, GetString);
	}
	else if (argCount >= 5) {
		smallImageKey = GET_ARG(3, GetString);
		startTime = GET_ARG(4, GetInteger);
	}

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	DiscordIntegrationActivity presence;
	presence.Details = details;
	presence.State = state;
	presence.LargeImageKey = largeImageKey;
	presence.SmallImageKey = smallImageKey;
	presence.StartTime = (time_t)startTime;

	Discord::UpdatePresence(presence);

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityDetails
 * \desc Sets the first line of text of the activity.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The first line of text in the Rich Presence.
 * \ns API.Discord
 */
VMValue Discord_SetActivityDetails(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* details = GET_ARG(0, GetString);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::SetDetails(details);

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityState
 * \desc Sets the second line of text of the activity.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The second line of text, appearing below details.
 * \ns API.Discord
 */
VMValue Discord_SetActivityState(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* state = GET_ARG(0, GetString);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::SetState(state);

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityLargeImage
 * \desc Sets the image (and optionally) the hover text of the large image asset.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param largeImageKey (string): The internal name of the large image asset to display, created via the Discord Developer Portal.
 * \paramOpt largeImageText (string): The hover text of the large image.
 * \ns API.Discord
 */
VMValue Discord_SetActivityLargeImage(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	const char* key = GET_ARG(0, GetString);
	const char* text = GET_ARG_OPT(1, GetString, nullptr);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	if (text) {
		Discord::Activity::SetLargeImage(key, text);
	}
	else {
		Discord::Activity::SetLargeImageKey(key);
	}

	return NULL_VAL;
}
/***
 * API.Discord.SetActivitySmallImage
 * \desc Sets the image (and optionally) the hover text of the small image asset.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param smallImageKey (string): The internal name of the small image asset to display, created via the Discord Developer Portal.
 * \paramOpt smallImageText (string): The hover text of the small image.
 * \ns API.Discord
 */
VMValue Discord_SetActivitySmallImage(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	const char* key = GET_ARG(0, GetString);
	const char* text = GET_ARG_OPT(1, GetString, nullptr);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	if (text) {
		Discord::Activity::SetSmallImage(key, text);
	}
	else {
		Discord::Activity::SetSmallImageKey(key);
	}

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityElapsedTimer
 * \desc Sets the elapsed timer of the activity.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param timestamp (integer): A Unix timestamp of when the timer started. If `0`, the timer is disabled.
 * \ns API.Discord
 */
VMValue Discord_SetActivityElapsedTimer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int timestamp = GET_ARG(0, GetInteger);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::SetElapsedTimer(timestamp);

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityRemainingTimer
 * \desc Sets the remaining timer of the activity.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param timestamp (integer): A Unix timestamp of when the timer will end. If `0`, the timer is disabled.
 * \ns API.Discord
 */
VMValue Discord_SetActivityRemainingTimer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int timestamp = GET_ARG(0, GetInteger);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::SetRemainingTimer(timestamp);

	return NULL_VAL;
}
/***
 * API.Discord.SetActivityPartySize
 * \desc Sets the current party size (and optionally) the max party size of the activity.<br/>\
This doesn't update the user's presence; you must call `API.Discord.UpdateActivity`. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param currentSize (integer): The current size of the party.
 * \paramOpt maxSize (integer): The max size of the party.
 * \ns API.Discord
 */
VMValue Discord_SetActivityPartySize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	int currentSize = GET_ARG(0, GetInteger);
	int maxSize = GET_ARG_OPT(1, GetInteger, 0);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::SetPartySize(currentSize);
	if (argCount >= 2) {
		Discord::Activity::SetPartyMaxSize(maxSize);
	}

	return NULL_VAL;
}
/***
 * API.Discord.UpdateActivity
 * \desc Updates the user's presence.<br/>\
The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param details (string): The first line of text in the Rich Presence.
 * \ns API.Discord
 */
VMValue Discord_UpdateActivity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	Discord::Activity::Update();

	return NULL_VAL;
}
/***
 * API.Discord.GetCurrentUsername
 * \desc Gets the current user's username.<br/>\
This returns `null` if the integration hasn't received the user's information yet. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \return string Returns a string value, or `null`.
 * \ns API.Discord
 */
VMValue Discord_GetCurrentUsername(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	DiscordIntegrationUserInfo* details = Discord::User::GetDetails();
	if (!details) {
		return NULL_VAL;
	}

	return ReturnString(details->Username);
}
/***
 * API.Discord.GetCurrentUserID
 * \desc Gets the current user's ID.<br/>\
This returns `null` if the integration hasn't received the user's information yet. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \return string Returns a string value, or `null`.
 * \ns API.Discord
 */
VMValue Discord_GetCurrentUserID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	DiscordIntegrationUserInfo* details = Discord::User::GetDetails();
	if (!details) {
		return NULL_VAL;
	}

	return ReturnString(details->ID);
}
/***
 * API.Discord.GetCurrentUserAvatar
 * \desc Gets the current user's avatar.<br/>\
The callback function must have two parameters: `responseCode` and `image`. `responseCode` receives the API response code, and `image` receives an Image if the operation succeeded, or `null` if it failed. The integration must have been initialized with `API.Discord.Init` before calling this.
 * \param size (integer): The size of the avatar to fetch. Must be one of: 16, 32, 64, 128, 256, 512, 1024
 * \param callback (function): The callback to execute.
 * \ns API.Discord
 */
VMValue Discord_GetCurrentUserAvatar(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	if (!Discord::Initialized) {
		return NULL_VAL;
	}

	int size = GET_ARG(0, GetInteger);
	if (size < 16 || size > 1024) {
		OUT_OF_RANGE_ERROR("Avatar size", size, 16, 1024);
		return NULL_VAL;
	}

	VMValue callable = GET_ARG(1, GetCallable);
	if (IS_NULL(callable)) {
		return NULL_VAL;
	}

	VMThreadCallback* callbackData = (VMThreadCallback*)Memory::Malloc(sizeof(VMThreadCallback));
	if (!callbackData) {
		return NULL_VAL;
	}
	callbackData->ThreadID = threadID;
	callbackData->Callable = callable;

	DiscordIntegrationCallback* callback = (DiscordIntegrationCallback*)Memory::Malloc(sizeof(DiscordIntegrationCallback));
	if (!callback) {
		Memory::Free(callbackData);
		return NULL_VAL;
	}
	callback->Type = DiscordIntegrationCallbackType_Script;
	callback->Function = callbackData;

	Discord::User::GetAvatar(size, callback);

	return NULL_VAL;
}
// #endregion

// #region Application
/***
 * Application.GetCommandLineArguments
 * \desc Gets a list of the command line arguments passed to the application.
 * \return array Returns an array of string values.
 * \ns Application
 */
VMValue Application_GetCommandLineArguments(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (ScriptManager::Lock()) {
		ObjArray* array = NewArray();
		for (size_t i = 0; i < Application::CmdLineArgs.size(); i++) {
			ObjString* argString = CopyString(Application::CmdLineArgs[i]);
			array->Values->push_back(OBJECT_VAL(argString));
		}
		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}
	return NULL_VAL;
}
/***
 * Application.GetEngineVersionString
 * \desc Gets the engine version string.
 * \return string Returns a string value.
 * \ns Application
 */
VMValue Application_GetEngineVersionString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Application::EngineVersion);
}
/***
 * Application.GetEngineVersionMajor
 * \desc Gets the major engine version.
 * \return integer Returns an integer value.
 * \ns Application
 */
VMValue Application_GetEngineVersionMajor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(VERSION_MAJOR);
}
/***
 * Application.GetEngineVersionMinor
 * \desc Gets the minor engine version.
 * \return integer Returns an integer value.
 * \ns Application
 */
VMValue Application_GetEngineVersionMinor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(VERSION_MINOR);
}
/***
 * Application.GetEngineVersionPatch
 * \desc Gets the minor engine version.
 * \return integer Returns an integer value.
 * \ns Application
 */
VMValue Application_GetEngineVersionPatch(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(VERSION_PATCH);
}
/***
 * Application.GetEngineVersionPrerelease
 * \desc Gets the prerelease engine version.
 * \return string Returns a string value, or `null`.
 * \ns Application
 */
VMValue Application_GetEngineVersionPrerelease(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
#ifdef VERSION_PRERELEASE
	return ReturnString(VERSION_PRERELEASE);
#else
	return NULL_VAL;
#endif
}
/***
 * Application.GetEngineVersionCodename
 * \desc Gets the engine version codename.
 * \return string Returns a string value, or `null`.
 * \ns Application
 */
VMValue Application_GetEngineVersionCodename(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
#ifdef VERSION_CODENAME
	return ReturnString(VERSION_CODENAME);
#else
	return NULL_VAL;
#endif
}
/***
 * Application.GetTargetFrameRate
 * \desc Gets the target frame rate of the fixed timestep.
 * \return integer Returns an integer value.
 * \ns Application
 */
VMValue Application_GetTargetFrameRate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::TargetFPS);
}
/***
 * Application.SetTargetFrameRate
 * \desc Sets the target frame rate of the fixed timestep.
 * \param framerate (integer): The target frame rate.
 * \ns Application
 */
VMValue Application_SetTargetFrameRate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int framerate = GET_ARG(0, GetInteger);
	if (framerate < 1 || framerate > MAX_TARGET_FRAMERATE) {
		OUT_OF_RANGE_ERROR("Framerate", framerate, 1, MAX_TARGET_FRAMERATE);
		return NULL_VAL;
	}
	Application::SetTargetFrameRate(framerate);
	return NULL_VAL;
}
/***
 * Application.UseFixedTimestep
 * \desc Enables or disables fixed timestep. This is enabled by default.
 * \param useFixedTimestep (boolean): Whether to use fixed timestep.
 * \ns Application
 */
VMValue Application_UseFixedTimestep(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	bool useFixedTimestep = GET_ARG(0, GetInteger);
	Application::ShouldUseFixedTimestep = useFixedTimestep;
	return NULL_VAL;
}

/***
 * Application.GetFPS
 * \desc Gets the current FPS (frames per second).
 * \return decimal Returns a decimal value.
 * \ns Application
 */
VMValue Application_GetFPS(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return DECIMAL_VAL(Application::CurrentFPS);
}
/***
 * Application.ShowFPSCounter
 * \desc Enables or disables the FPS (frames per second) counter.
 * \param show (boolean): Whether to show the FPS counter.
 * \ns Application
 */
VMValue Application_ShowFPSCounter(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::ShowFPS = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Application.GetKeyBind
 * \desc Gets a keybind.
 * \param keyBind (<ref KeyBind_*>): The keybind.
 * \return <ref KeyBind_*> Returns the key ID of the keybind.
 * \ns Application
 */
VMValue Application_GetKeyBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int bind = GET_ARG(0, GetInteger);
	return INTEGER_VAL(Application::GetKeyBind(bind));
}
/***
 * Application.SetKeyBind
 * \desc Sets a keybind.
 * \param keyBind (<ref KeyBind_*>): The keybind.
 * \param keyID (<ref Key_*>): The key ID.
 * \ns Application
 */
VMValue Application_SetKeyBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int bind = GET_ARG(0, GetInteger);
	int key = GET_ARG(1, GetInteger);
	Application::SetKeyBind(bind, key);
	return NULL_VAL;
}
/***
 * Application.GetGameTitle
 * \desc Gets the game title of the application.
 * \ns Application
 */
VMValue Application_GetGameTitle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Application::GameTitle);
}
/***
 * Application.GetGameTitleShort
 * \desc Gets the short game title of the application.
 * \ns Application
 */
VMValue Application_GetGameTitleShort(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Application::GameTitleShort);
}
/***
 * Application.GetGameVersion
 * \desc Gets the version of the application.
 * \ns Application
 */
VMValue Application_GetGameVersion(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Application::GameVersion);
}
/***
 * Application.GetGameDescription
 * \desc Gets the description of the game.
 * \ns Application
 */
VMValue Application_GetGameDescription(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Application::GameDescription);
}
/***
 * Application.SetGameTitle
 * \desc Sets the title of the game.
 * \param title (string): Game title.
 * \ns Application
 */
VMValue Application_SetGameTitle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* string = GET_ARG(0, GetString);
	StringUtils::Copy(Application::GameTitle, string, sizeof(Application::GameTitle));
	return NULL_VAL;
}
/***
 * Application.SetGameTitleShort
 * \desc Sets the short title of the game.
 * \param title (string): Short game title.
 * \ns Application
 */
VMValue Application_SetGameTitleShort(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* string = GET_ARG(0, GetString);
	StringUtils::Copy(Application::GameTitleShort, string, sizeof(Application::GameTitleShort));
	return NULL_VAL;
}
/***
 * Application.SetGameVersion
 * \desc Sets the version of the game.
 * \param title (string): Game version.
 * \ns Application
 */
VMValue Application_SetGameVersion(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* string = GET_ARG(0, GetString);
	StringUtils::Copy(Application::GameVersion, string, sizeof(Application::GameVersion));
	return NULL_VAL;
}
/***
 * Application.SetGameDescription
 * \desc Sets the description of the game.
 * \param title (string): Game description.
 * \ns Application
 */
VMValue Application_SetGameDescription(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	const char* string = GET_ARG(0, GetString);
	StringUtils::Copy(
		Application::GameDescription, string, sizeof(Application::GameDescription));
	return NULL_VAL;
}
/***
 * Application.SetCursorVisible
 * \desc Sets the visibility of the cursor.
 * \param cursorVisible (boolean): Whether the cursor is visible.
 * \ns Application
 */
VMValue Application_SetCursorVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	SDL_ShowCursor(!!GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Application.GetCursorVisible
 * \desc Gets the visibility of the cursor.
 * \return boolean Returns whether the cursor is visible.
 * \ns Application
 */
VMValue Application_GetCursorVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int state = SDL_ShowCursor(SDL_QUERY);
	if (state == SDL_ENABLE) {
		return INTEGER_VAL(1);
	}
	return INTEGER_VAL(0);
}
/***
 * Application.Error
 * \desc Shows an error message to the user through a dialog box, if possible. The error message is also logged.
 * \param message (string): The error message to show to the user.
 * \paramOpt detailed (boolean): Whether to show the stack trace alongside the error. (default: `true`)
 * \ns Application
 */
VMValue Application_Error(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* errorString = GET_ARG(0, GetString);
	bool detailed = GET_ARG_OPT(1, GetInteger, true);

	ScriptManager::Threads[threadID].ShowErrorFromScript(errorString, detailed);

	return NULL_VAL;
}
/***
 * Application.SetDefaultFont
 * \desc Sets the default font to the given Resource path, or array containing Resource paths.
 * \param font (string): The font. Passing `null` to this argument will change the default font to the one the application was built with, if one is present.
 * \ns Application
 */
/***
 * Application.SetDefaultFont
 * \desc Sets the default font to the given Resource path, or array containing Resource paths.
 * \param font (array): The list of fonts. Passing `null` to this argument will change the default font to the one the application was built with, if one is present.
 * \ns Application
 */
VMValue Application_SetDefaultFont(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	std::vector<std::string> fontList;

	// If null was passed, this clears the font list.
	if (IS_NULL(args[0])) {
		Application::DefaultFontList.clear();
		Application::LoadDefaultFont();
		return NULL_VAL;
	}
	else if (IS_ARRAY(args[0])) {
		ObjArray* array = AS_ARRAY(args[0]);

		// If an empty array was passed intentionally, this clears the font list.
		if (array->Values->size() == 0) {
			Application::DefaultFontList.clear();
			Application::LoadDefaultFont();
			return NULL_VAL;
		}

		for (size_t i = 0; i < array->Values->size(); i++) {
			if (ScriptManager::Lock()) {
				VMValue value = (*array->Values)[i];
				if (IS_STRING(value)) {
					char* filename = AS_CSTRING(value);
					fontList.push_back(std::string(filename));
				}
				else {
					ScriptManager::Threads[threadID].ThrowRuntimeError(false,
						"Expected argument to be of type %s instead of %s.",
						GetObjectTypeString(OBJ_STRING),
						GetValueTypeString(value));
				}
				ScriptManager::Unlock();
			}
		}
	}
	else {
		char* path = GET_ARG(0, GetString);
		if (ResourceManager::ResourceExists(path)) {
			fontList.push_back(std::string(path));
		}
		else {
			Log::Print(Log::LOG_ERROR, "Resource \"%s\" does not exist!", path);
		}
	}

	if (fontList.size() > 0) {
		Application::DefaultFontList = fontList;
		Application::LoadDefaultFont();
	}

	return NULL_VAL;
}
/***
 * Application.ChangeGame
 * \desc Changes the current game, by loading a data file containing the new game.<br/>If the path ends with a path separator (`/`), an entire directory will be loaded as the game instead. Only `game://` and `user://` URLs are supported (or an absolute path that resolves to those locations).<br/><br/>\
This is permanent for as long as the application is running, so restarting the application using the associated developer key (<ref KeyBind_DevRestartApp>) will reload the current game, and not the one the application started with. Script compiling is also disabled after the game changes.<br/><br/>\
The change only takes effect after a frame completes.<br/><br/>\
Note that certain game configurations will persist between games if not set by the new GameConfig:<ul>\
<li>Game-identifying configurations:</li><ul>\
<li>Game title (including the short game title)</li>\
<li>Game version</li>\
<li>Game description</li>\
<li>Game developer</li>\
<li>Game identifier</li>\
<li>Developer identifier</li>\
</ul>\
<li>Paths:</li><ul>\
<li>Saves directory</li>\
<li>Preferences directory</li>\
</ul>\
<li>Engine configurations:</li><ul>\
<li>Window size</li>\
<li>Audio volume</li>\
<li>Target frame rate</li>\
<li>Whether fixed timestep was enabled or disabled with `useFixedTimestep`</li>\
<li>Settings filename</li>\
<ul>If this is changed, the current settings are discarded (not saved) and the new settings file is loaded. If the file does not exist, however, default settings will be loaded.</ul>\
</ul></ul>\
Some of the game's current state also persists between games:<ul>\
<li>Command line arguments (unless <param cmdLineArgs> is passed to this function)</li>\
<li>Palette colors</li>\
<li>Whether palette rendering is enabled</li>\
<li>Whether software rendering was enabled or disabled with `useSoftwareRenderer`</li>\
</ul>\
The following <b>does not</b> persist between games:<ul>\
<li>Any loaded resources</li>\
<li>Any persistent entities</li>\
<li>Scripting state</li>\
</ul>\
The following <b>cannot</b> be changed between games:<ul>\
<li>Log file name</li>\
<li>Graphics rendering backend</li>\
</ul>
 * \param path (string): The path of the data file to load.
 * \paramOpt startingScene (string): The filename of the scene file to load upon changing the game. Note that restarting the game will load the starting scene defined in its GameConfig instead. Passing `null` to this argument is equivalent to not passing it at all.
 * \paramOpt cmdLineArgs (array): An array of strings containing the command line arguments to pass to the new game. If any of the values are not strings, they will be converted to a string representation. If this argument is not given, the current command line arguments will be passed to the new game.
 * \return boolean Returns whether the game will change.
 * \ns Application
 */
VMValue Application_ChangeGame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* path = GET_ARG(0, GetString);
	char* startingScene = nullptr;
	ObjArray* cmdLineArgsArray = GET_ARG_OPT(2, GetArray, nullptr);
	std::vector<std::string>* cmdLineArgs = nullptr;

	if (argCount >= 2 && !IS_NULL(args[1])) {
		startingScene = GET_ARG(1, GetString);
	}

	if (cmdLineArgsArray) {
		cmdLineArgs = new std::vector<std::string>();

		for (size_t i = 0; i < cmdLineArgsArray->Values->size(); i++) {
			std::string arg = Value::ToString((*cmdLineArgsArray->Values)[i]);
			cmdLineArgs->push_back(arg);
		}
	}

	bool exists = Application::SetNextGame(path, startingScene, cmdLineArgs);
	if (!exists) {
		delete cmdLineArgs;
		return INTEGER_VAL(false);
	}

	return INTEGER_VAL(true);
}
/***
 * Application.Quit
 * \desc Closes the application.
 * \ns Application
 */
VMValue Application_Quit(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Application::Running = false;
	return NULL_VAL;
}
// #endregion

// #region Array
/***
 * Array.Create
 * \desc Creates an array.
 * \param size (integer): Size of the array.
 * \paramOpt initialValue (value): Initial value to set the array elements to.
 * \return value A reference value to the array.
 * \ns Array
 */
VMValue Array_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	if (ScriptManager::Lock()) {
		ObjArray* array = NewArray();
		int length = GET_ARG(0, GetInteger);
		VMValue initialValue = NULL_VAL;
		if (argCount == 2) {
			initialValue = args[1];
		}

		for (int i = 0; i < length; i++) {
			array->Values->push_back(initialValue);
		}

		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}
	return NULL_VAL;
}
/***
 * Array.Length
 * \desc Gets the length of an array.
 * \param array (array): Array to get the length of.
 * \return integer Length of the array.
 * \ns Array
 */
VMValue Array_Length(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		int size = (int)array->Values->size();
		ScriptManager::Unlock();
		return INTEGER_VAL(size);
	}
	return INTEGER_VAL(0);
}
/***
 * Array.Push
 * \desc Adds a value to the end of an array.
 * \param array (array): Array to get the length of.
 * \param value (value): Value to add to the array.
 * \ns Array
 */
VMValue Array_Push(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		array->Values->push_back(args[1]);
		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Pop
 * \desc Gets the value at the end of an array, and removes it.
 * \param array (array): Array to get the length of.
 * \return value The value from the end of the array.
 * \ns Array
 */
VMValue Array_Pop(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		if (array->Values->size() == 0) {
			ScriptManager::Unlock();
			THROW_ERROR("Array is empty!");
			return NULL_VAL;
		}
		VMValue value = array->Values->back();
		array->Values->pop_back();
		ScriptManager::Unlock();
		return value;
	}
	return NULL_VAL;
}
/***
 * Array.Insert
 * \desc Inserts a value at an index of an array.
 * \param array (array): Array to insert value.
 * \param index (integer): Index to insert value.
 * \param value (value): Value to insert.
 * \ns Array
 */
VMValue Array_Insert(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		int index = GET_ARG(1, GetInteger);
		if (index < 0 || index > (int)array->Values->size()) { // Not a typo
			ScriptManager::Unlock();
			THROW_ERROR("Index %d is out of bounds of array of size %d.",
				index,
				(int)array->Values->size());
			return NULL_VAL;
		}
		array->Values->insert(array->Values->begin() + index, args[2]);
		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Erase
 * \desc Erases a value at an index of an array.
 * \param array (array): Array to erase value.
 * \param index (integer): Index to erase value.
 * \ns Array
 */
VMValue Array_Erase(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		int index = GET_ARG(1, GetInteger);
		if (index < 0 || index >= (int)array->Values->size()) {
			ScriptManager::Unlock();
			THROW_ERROR("Index %d is out of bounds of array of size %d.",
				index,
				(int)array->Values->size());
			return NULL_VAL;
		}
		array->Values->erase(array->Values->begin() + index);
		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Clear
 * \desc Clears an array.
 * \param array (array): Array to clear.
 * \ns Array
 */
VMValue Array_Clear(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		array->Values->clear();
		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Shift
 * \desc Rotates the array in the desired direction.
 * \param array (array): Array to shift.
 * \param toRight (boolean): Whether to rotate the array to the right or not.
 * \ns Array
 */
VMValue Array_Shift(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		int toright = GET_ARG(1, GetInteger);

		if (array->Values->size() > 1) {
			if (toright) {
				size_t lastIndex = array->Values->size() - 1;
				VMValue temp = (*array->Values)[lastIndex];
				array->Values->erase(array->Values->begin() + lastIndex);
				array->Values->insert(array->Values->begin(), temp);
			}
			else {
				VMValue temp = (*array->Values)[0];
				array->Values->erase(array->Values->begin() + 0);
				array->Values->push_back(temp);
			}
		}

		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.SetAll
 * \desc Sets values in the array from startIndex to endIndex (includes the value at endIndex.)
 * \param array (array): Array to set values to.
 * \param startIndex (integer): Index of value to start setting. (`-1` for first index)
 * \param endIndex (integer): Index of value to end setting. (`-1` for last index)
 * \param value (value): Value to set to.
 * \ns Array
 */
VMValue Array_SetAll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		size_t startIndex = GET_ARG(1, GetInteger);
		size_t endIndex = GET_ARG(2, GetInteger);
		VMValue value = args[3];

		size_t arraySize = array->Values->size();
		if (arraySize > 0) {
			if (startIndex < 0) {
				startIndex = 0;
			}
			else if (startIndex >= arraySize) {
				startIndex = arraySize - 1;
			}

			if (endIndex < 0) {
				endIndex = arraySize - 1;
			}
			else if (endIndex >= arraySize) {
				endIndex = arraySize - 1;
			}

			for (size_t i = startIndex; i <= endIndex; i++) {
				(*array->Values)[i] = value;
			}
		}

		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Reverse
 * \desc Reverses the elements of an array through the specified range, exclusive. The array is reversed from `startIndex` to, but not including, `endIndex`.
 * \param array (array): Array to reverse.
 * \paramOpt startIndex (integer): Start range. (default: `0`)
 * \paramOpt endIndex (integer): End range. (default: size of array)
 * \ns Array
 */
VMValue Array_Reverse(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		int startIndex = GET_ARG_OPT(1, GetInteger, 0);
		int endIndex = GET_ARG_OPT(2, GetInteger, array->Values->size());

		if (startIndex < 0 || startIndex >= (int)array->Values->size() ||
			startIndex >= endIndex) {
			THROW_ERROR("Start index out of range.");
			return NULL_VAL;
		}
		if (endIndex <= 0 || endIndex > (int)array->Values->size() ||
			endIndex <= startIndex) {
			THROW_ERROR("End index out of range.");
			return NULL_VAL;
		}

		std::reverse(
			array->Values->begin() + startIndex, array->Values->begin() + endIndex);

		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
/***
 * Array.Sort
 * \desc Sorts the entries of the given array.
 * \param array (array): Array to sort.
 * \paramOpt compFunction (function): Comparison function. If not given, a default comparison function is used; the entries of the array are sorted in ascending order, and non-numeric values do not participate in the comparison.
 * \ns Array
 */
VMValue Array_Sort(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	if (ScriptManager::Lock()) {
		ObjArray* array = GET_ARG(0, GetArray);
		ObjFunction* function = GET_ARG_OPT(1, GetFunction, nullptr);

		if (function) {
			VMThread* thread = &ScriptManager::Threads[threadID];

			std::stable_sort(array->Values->begin(),
				array->Values->end(),
				[array, thread, function](const VMValue& a, const VMValue& b) {
					thread->Push(a);
					thread->Push(b);

					VMValue result = thread->RunEntityFunction(function, 2);

					thread->Pop(2);

					if (IS_INTEGER(result)) {
						return AS_INTEGER(result) == 1;
					}

					return false;
				});
		}
		else {
			std::stable_sort(array->Values->begin(),
				array->Values->end(),
				[array](const VMValue& a, const VMValue& b) {
					if (IS_NOT_NUMBER(a) || IS_NOT_NUMBER(b)) {
						return false;
					}
					else if (IS_DECIMAL(a) || IS_DECIMAL(b)) {
						return AS_DECIMAL(Value::CastAsDecimal(a)) <
							AS_DECIMAL(Value::CastAsDecimal(b));
					}
					else {
						return AS_INTEGER(a) < AS_INTEGER(b);
					}
				});
		}

		ScriptManager::Unlock();
	}
	return NULL_VAL;
}
// #endregion

// #region Audio
/***
 * Audio.GetMasterVolume
 * \desc Gets the master volume of the audio mixer.
 * \return integer The master volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetMasterVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::MasterVolume);
}
/***
 * Audio.GetMusicVolume
 * \desc Gets the music volume of the audio mixer.
 * \return integer The music volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetMusicVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::MusicVolume);
}
/***
 * Audio.GetSoundVolume
 * \desc Gets the sound effect volume of the audio mixer.
 * \return integer The sound effect volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetSoundVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::SoundVolume);
}
/***
 * Audio.SetMasterVolume
 * \desc Sets the master volume of the audio mixer.
 * \param volume (integer): The master volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetMasterVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int volume = GET_ARG(0, GetInteger);
	if (volume < 0) {
		THROW_ERROR("Volume cannot be lower than 0.");
	}
	else if (volume > 100) {
		THROW_ERROR("Volume cannot be higher than 100.");
	}
	else {
		Application::SetMasterVolume(volume);
	}

	return NULL_VAL;
}
/***
 * Audio.SetMusicVolume
 * \desc Sets the music volume of the audio mixer.
 * \param volume (integer): The music volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetMusicVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int volume = GET_ARG(0, GetInteger);
	if (volume < 0) {
		THROW_ERROR("Volume cannot be lower than 0.");
	}
	else if (volume > 100) {
		THROW_ERROR("Volume cannot be higher than 100.");
	}
	else {
		Application::SetMusicVolume(volume);
	}

	return NULL_VAL;
}
/***
 * Audio.SetSoundVolume
 * \desc Sets the sound effect volume of the audio mixer.
 * \param volume (integer): The sound effect volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetSoundVolume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int volume = GET_ARG(0, GetInteger);
	if (volume < 0) {
		THROW_ERROR("Volume cannot be lower than 0.");
	}
	else if (volume > 100) {
		THROW_ERROR("Volume cannot be higher than 100.");
	}
	else {
		Application::SetSoundVolume(volume);
	}

	return NULL_VAL;
}
// #endregion

// #region Collision
/***
 * Collision.ProcessEntityMovement
 * \desc Processes movement of an instance with an outer hitbox and an inner hitboxes.
 * \param entity (Entity): The instance to move.
 * \param outer (hitbox): The outer hitbox.
 * \param inner (hitbox): The inner hitbox.
 * \ns Collision
 */
VMValue Collision_ProcessEntityMovement(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ObjEntity* entity = GET_ARG(0, GetEntity);
	CollisionBox outerBox = GET_ARG(1, GetHitbox);
	CollisionBox innerBox = GET_ARG(2, GetHitbox);

	if (entity) {
		Scene::ProcessEntityMovement((Entity*)entity->EntityPtr, &outerBox, &innerBox);
	}
	return NULL_VAL;
}
/***
 * Collision.CheckTileCollision
 * \desc Checks tile collision based on where an instance should check.
 * \param entity (Entity): The instance to base the values on.
 * \param cLayers (bitfield): Which layers the entity can collide with.
 * \param cMode (<ref CMODE_*>): Collision mode of the entity.
 * \param cPlane (integer): Collision plane of which to get the collision (A or B).
 * \param xOffset (integer): How far from the entity's X value to start.
 * \param yOffset (integer): How far from the entity's Y value to start.
 * \param setPos (boolean): Whether to set the entity's position if collision is found.
 * \return boolean Returns whether the instance has collided with a tile.
 * \ns Collision
 */
VMValue Collision_CheckTileCollision(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);
	ObjEntity* entity = GET_ARG(0, GetEntity);
	int cLayers = GET_ARG(1, GetInteger);
	int cMode = GET_ARG(2, GetInteger);
	int cPlane = GET_ARG(3, GetInteger);
	int xOffset = GET_ARG(4, GetInteger);
	int yOffset = GET_ARG(5, GetInteger);
	int setPos = GET_ARG(6, GetInteger);

	return INTEGER_VAL(Scene::CheckTileCollision((Entity*)entity->EntityPtr, cLayers, cMode, cPlane, xOffset, yOffset, setPos));
}
/***
 * Collision.CheckTileGrip
 * \desc Keeps an instance gripped to tile collision based on where an instance should check.
 * \param entity (Entity): The instance to move.
 * \param cLayers (bitfield): Which layers the entity can collide with.
 * \param cMode (<ref CMODE_*>): Collision mode of the entity.
 * \param cPlane (integer): Collision plane of which to get the collision (A or B).
 * \param xOffset (integer): How far from the entity's X value to start.
 * \param yOffset (integer): How far from the entity's Y value to start.
 * \param tolerance (decimal): How far of a tolerance the entity should check for.
 * \return boolean Returns whether to grip the instance.
 * \ns Collision
 */
VMValue Collision_CheckTileGrip(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);
	ObjEntity* entity = GET_ARG(0, GetEntity);
	int cLayers = GET_ARG(1, GetInteger);
	int cMode = GET_ARG(2, GetInteger);
	int cPlane = GET_ARG(3, GetInteger);
	int xOffset = GET_ARG(4, GetInteger);
	int yOffset = GET_ARG(5, GetInteger);
	float tolerance = GET_ARG(6, GetDecimal);

	return INTEGER_VAL(Scene::CheckTileGrip((Entity*)entity->EntityPtr, cLayers, cMode, cPlane, xOffset, yOffset, tolerance));
}
/***
 * Collision.CheckEntityTouch
 * \desc Checks if an instance is touching another instance with their respective hitboxes.
 * \param thisEntity (Entity): The first instance to check.
 * \param thisHitbox (hitbox): The first entity's hitbox.
 * \param otherEntity (Entity): The other instance to check.
 * \param otherHitbox (hitbox): The other entity's hitbox.
 * \return boolean Returns whether the entities are touching.
 * \ns Collision
 */
VMValue Collision_CheckEntityTouch(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjEntity* thisEntity = GET_ARG(0, GetEntity);
	CollisionBox thisBox = GET_ARG(1, GetHitbox);
	ObjEntity* otherEntity = GET_ARG(2, GetEntity);
	CollisionBox otherBox = GET_ARG(3, GetHitbox);

	return INTEGER_VAL(!!Scene::CheckEntityTouch((Entity*)thisEntity->EntityPtr, &thisBox, (Entity*)otherEntity->EntityPtr, &otherBox));
}
/***
 * Collision.CheckEntityCircle
 * \desc Checks if an instance is touching another instance with within their respective radii.
 * \param thisEntity (Entity): The first instance to check.
 * \param thisRadius (decimal): Radius of the first entity to check.
 * \param otherEntity (Entity): The other instance to check.
 * \param otherRadius (decimal): Radius of the other entity to check.
 * \return boolean Returns whether the entities have collided.
 * \ns Collision
 */
VMValue Collision_CheckEntityCircle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjEntity* thisEntity = GET_ARG(0, GetEntity);
	float thisRadius = GET_ARG(1, GetDecimal);
	ObjEntity* otherEntity = GET_ARG(2, GetEntity);
	float otherRadius = GET_ARG(3, GetDecimal);

	return INTEGER_VAL(!!Scene::CheckEntityCircle((Entity*)thisEntity->EntityPtr, thisRadius, (Entity*)otherEntity->EntityPtr, otherRadius));
}
/***
 * Collision.CheckEntityBox
 * \desc Checks if an instance is touching another instance with their respective hitboxes and sets the values of the other instance if specified.
 * \param thisEntity (Entity): The first instance to check.
 * \param thisHitbox (hitbox): The first entity's hitbox.
 * \param otherEntity (Entity): The other instance to check.
 * \param otherHitbox (hitbox): The other entity's hitbox.
 * \param setValues (boolean): Whether to set the values of the other entity.
 * \return <ref C_*> Returns the side the entities are colliding on.
 * \ns Collision
 */
VMValue Collision_CheckEntityBox(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	ObjEntity* thisEntity = GET_ARG(0, GetEntity);
	CollisionBox thisBox = GET_ARG(1, GetHitbox);
	ObjEntity* otherEntity = GET_ARG(2, GetEntity);
	CollisionBox otherBox = GET_ARG(3, GetHitbox);
	bool setValues = !!GET_ARG(4, GetInteger);

	return INTEGER_VAL(Scene::CheckEntityBox((Entity*)thisEntity->EntityPtr, &thisBox, (Entity*)otherEntity->EntityPtr, &otherBox, setValues));
}
/***
 * Collision.CheckEntityPlatform
 * \desc Checks if an instance is touching the top of another instance with their respective hitboxes and sets the values of the other instance if specified.
 * \param thisEntity (Entity): The first instance to check.
 * \param thisHitbox (hitbox): The first entity's hitbox.
 * \param otherEntity (Entity): The other instance to check whether it is on top of the first instance.
 * \param otherHitbox (hitbox): The other entity's hitbox.
 * \param setValues (boolean): Whether to set the values of the other entity.
 * \return boolean Returns whether the entities have collided.
 * \ns Collision
 */
VMValue Collision_CheckEntityPlatform(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	ObjEntity* thisEntity = GET_ARG(0, GetEntity);
	CollisionBox thisBox = GET_ARG(1, GetHitbox);
	ObjEntity* otherEntity = GET_ARG(2, GetEntity);
	CollisionBox otherBox = GET_ARG(3, GetHitbox);
	bool setValues = !!GET_ARG(4, GetInteger);

	return INTEGER_VAL(!!Scene::CheckEntityPlatform((Entity*)thisEntity->EntityPtr, &thisBox, (Entity*)otherEntity->EntityPtr, &otherBox, setValues));
}
// #endregion

// #region Controller
/***
 * Controller.GetCount
 * \desc Gets the amount of connected controllers in the device.
 * \return integer Returns the amount of connected controllers in the device.
 * \ns Controller
 */
VMValue Controller_GetCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(InputManager::NumControllers);
}
/***
 * Controller.IsConnected
 * \desc Gets whether the controller at the index is connected.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index is connected.
 * \ns Controller
 */
VMValue Controller_IsConnected(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	if (index < 0) {
		THROW_ERROR("Controller index %d out of range.", index);
		return NULL_VAL;
	}
	return INTEGER_VAL(InputManager::ControllerIsConnected(index));
}
#define CONTROLLER_GET_BOOL(str) \
	VMValue Controller_##str(int argCount, VMValue* args, Uint32 threadID) { \
		CHECK_ARGCOUNT(1); \
		int index = GET_ARG(0, GetInteger); \
		CHECK_CONTROLLER_INDEX(index); \
		return INTEGER_VAL(InputManager::Controller##str(index)); \
	}
#define CONTROLLER_GET_INT(str) \
	VMValue Controller_##str(int argCount, VMValue* args, Uint32 threadID) { \
		CHECK_ARGCOUNT(1); \
		int index = GET_ARG(0, GetInteger); \
		CHECK_CONTROLLER_INDEX(index); \
		return INTEGER_VAL((int)(InputManager::Controller##str(index))); \
	}
/***
 * Controller.IsXbox
 * \desc Gets whether the controller at the index is an Xbox controller.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index is an Xbox controller.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(IsXbox)
/***
 * Controller.IsPlayStation
 * \desc Gets whether the controller at the index is a PlayStation controller.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index is a PlayStation controller.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(IsPlayStation)
/***
 * Controller.IsJoyCon
 * \desc Gets whether the controller at the index is a Nintendo Switch Joy-Con L or R.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index is a Nintendo Switch Joy-Con L or R.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(IsJoyCon)
/***
 * Controller.HasShareButton
 * \desc Gets whether the controller at the index has a Share or Capture button.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index has a Share or Capture button.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(HasShareButton)
/***
 * Controller.HasMicrophoneButton
 * \desc Gets whether the controller at the index has a Microphone button.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index has a Microphone button.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(HasMicrophoneButton)
/***
 * Controller.HasPaddles
 * \desc Gets whether the controller at the index has paddles.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller at the index has paddles.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(HasPaddles)
/***
 * Controller.IsButtonHeld
 * \desc Checks if a button is held.
 * \param controllerIndex (integer): Index of the controller to check.
 * \param button (<ref Button_*>): Which button to check.
 * \return boolean Returns a boolean value.
 * \ns Controller
 */
VMValue Controller_IsButtonHeld(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int button = GET_ARG(1, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	if (button < 0 || button >= (int)ControllerButton::Max) {
		THROW_ERROR("Controller button %d out of range.", button);
		return NULL_VAL;
	}
	return INTEGER_VAL(InputManager::ControllerIsButtonHeld(index, button));
}
/***
 * Controller.IsButtonPressed
 * \desc Checks if a button is pressed.
 * \param controllerIndex (integer): Index of the controller to check.
 * \param button (<ref Button_*>): Which button to check.
 * \return boolean Returns a boolean value.
 * \ns Controller
 */
VMValue Controller_IsButtonPressed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int button = GET_ARG(1, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	if (button < 0 || button >= (int)ControllerButton::Max) {
		THROW_ERROR("Controller button %d out of range.", button);
		return NULL_VAL;
	}
	return INTEGER_VAL(InputManager::ControllerIsButtonPressed(index, button));
}
/***
 * Controller.GetButton
 * \desc Gets the button value from the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \param button (<ref Button_*>): Which button to check.
 * \return boolean Returns the button value from the controller at the index.
 * \deprecated Use <ref Controller.IsButtonHeld> instead.
 * \ns Controller
 */
VMValue Controller_GetButton(int argCount, VMValue* args, Uint32 threadID) {
	return Controller_IsButtonHeld(argCount, args, threadID);
}
/***
 * Controller.GetAxis
 * \desc Gets the axis value from the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \param axis (<ref Axis_*>): Which axis to check.
 * \return decimal Returns the axis value from the controller at the index.
 * \ns Controller
 */
VMValue Controller_GetAxis(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int axis = GET_ARG(1, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	if (axis < 0 || axis >= (int)ControllerAxis::Max) {
		THROW_ERROR("Controller axis %d out of range.", axis);
		return NULL_VAL;
	}
	return DECIMAL_VAL(InputManager::ControllerGetAxis(index, axis));
}
/***
 * Controller.GetType
 * \desc Gets the type of the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return <ref Controller_*> Returns the type of the controller at the index.
 * \ns Controller
 */
CONTROLLER_GET_INT(GetType)
/***
 * Controller.GetName
 * \desc Gets the name of the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return string Returns the name of the controller at the index.
 * \ns Controller
 */
VMValue Controller_GetName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);

	CHECK_CONTROLLER_INDEX(index);

	return ReturnString(InputManager::ControllerGetName(index));
}
/***
 * Controller.SetPlayerIndex
 * \desc Sets the player index of the controller at the index.
 * \param controllerIndex (integer): Index of the controller.
 * \param playerIndex (integer): The player index. Use `-1` to disable the controller's LEDs.
 * \ns Controller
 */
VMValue Controller_SetPlayerIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int player_index = GET_ARG(1, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	InputManager::ControllerSetPlayerIndex(index, player_index);
	return NULL_VAL;
}
/***
 * Controller.HasRumble
 * \desc Checks if the controller at the index supports rumble.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether the controller supports rumble.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(HasRumble)
/***
 * Controller.IsRumbleActive
 * \desc Checks if rumble is active for the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether rumble is active for the controller.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(IsRumbleActive)
/***
 * Controller.Rumble
 * \desc Rumbles a controller.
 * \param controllerIndex (integer): Index of the controller to rumble.
 * \param strength (number): Rumble strength. (0.0 - 1.0)
 * \param duration (integer): Duration in milliseconds. Use `0` for infinite duration.
 * \ns Controller
 */
VMValue Controller_Rumble(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount <= 3) {
		CHECK_ARGCOUNT(3);
		int index = GET_ARG(0, GetInteger);
		float strength = GET_ARG(1, GetDecimal);
		int duration = GET_ARG(2, GetInteger);
		CHECK_CONTROLLER_INDEX(index);
		if (strength < 0.0 || strength > 1.0) {
			THROW_ERROR("Rumble strength %f out of range. (0.0 - 1.0)", strength);
			return NULL_VAL;
		}
		if (duration < 0) {
			THROW_ERROR("Rumble duration %d out of range.", duration);
			return NULL_VAL;
		}
		InputManager::ControllerRumble(index, strength, duration);
	}
	else {
		CHECK_ARGCOUNT(4);
		int index = GET_ARG(0, GetInteger);
		float large_frequency = GET_ARG(1, GetDecimal);
		float small_frequency = GET_ARG(2, GetDecimal);
		int duration = GET_ARG(3, GetInteger);
		CHECK_CONTROLLER_INDEX(index);
		if (large_frequency < 0.0 || large_frequency > 1.0) {
			THROW_ERROR("Large motor frequency %f out of range. (0.0 - 1.0)",
				large_frequency);
			return NULL_VAL;
		}
		if (small_frequency < 0.0 || small_frequency > 1.0) {
			THROW_ERROR("Small motor frequency %f out of range. (0.0 - 1.0)",
				small_frequency);
			return NULL_VAL;
		}
		if (duration < 0) {
			THROW_ERROR("Rumble duration %d out of range.", duration);
			return NULL_VAL;
		}
		InputManager::ControllerRumble(index, large_frequency, small_frequency, duration);
	}
	return NULL_VAL;
}
/***
 * Controller.StopRumble
 * \desc Stops controller haptics.
 * \param controllerIndex (integer): Index of the controller to stop.
 * \ns Controller
 */
VMValue Controller_StopRumble(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	InputManager::ControllerStopRumble(index);
	return NULL_VAL;
}
/***
 * Controller.IsRumblePaused
 * \desc Checks if rumble is paused for the controller at the index.
 * \param controllerIndex (integer): Index of the controller to check.
 * \return boolean Returns whether rumble is paused for the controller.
 * \ns Controller
 */
CONTROLLER_GET_BOOL(IsRumblePaused)
/***
 * Controller.SetRumblePaused
 * \desc Pauses or unpauses rumble for the controller at the index.
 * \param controllerIndex (integer): Index of the controller.
 * \ns Controller
 */
VMValue Controller_SetRumblePaused(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	bool paused = !!GET_ARG(1, GetInteger);
	CHECK_CONTROLLER_INDEX(index);
	InputManager::ControllerSetRumblePaused(index, paused);
	return NULL_VAL;
}
/***
 * Controller.SetLargeMotorFrequency
 * \desc Sets the frequency of a controller's large motor.
 * \param controllerIndex (integer): Index of the controller.
 * \param frequency (number): Frequency of the large motor.
 * \deprecated Use <ref Controller.Rumble> instead.
 * \ns Controller
 */
VMValue Controller_SetLargeMotorFrequency(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	float frequency = GET_ARG(1, GetDecimal);
	CHECK_CONTROLLER_INDEX(index);
	if (frequency < 0.0 || frequency > 1.0) {
		THROW_ERROR("Large motor frequency %f out of range. (0.0 - 1.0)", frequency);
		return NULL_VAL;
	}
	InputManager::ControllerSetLargeMotorFrequency(index, frequency);
	return NULL_VAL;
}
/***
 * Controller.SetSmallMotorFrequency
 * \desc Sets the frequency of a controller's small motor.
 * \param controllerIndex (integer): Index of the controller.
 * \param frequency (number): Frequency of the small motor.
 * \deprecated Use <ref Controller.Rumble> instead.
 * \ns Controller
 */
VMValue Controller_SetSmallMotorFrequency(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	float frequency = GET_ARG(1, GetDecimal);
	CHECK_CONTROLLER_INDEX(index);
	if (frequency < 0.0 || frequency > 1.0) {
		THROW_ERROR("Small motor frequency %f out of range. (0.0 - 1.0)", frequency);
		return NULL_VAL;
	}
	InputManager::ControllerSetSmallMotorFrequency(index, frequency);
	return NULL_VAL;
}
#undef CONTROLLER_GET_BOOL
#undef CONTROLLER_GET_INT
// #endregion

// #region Date
/***
 * Date.GetEpoch
 * \desc Gets the amount of seconds from 1 January 1970, 0:00 UTC.
 * \return integer The amount of seconds from epoch.
 * \ns Date
 */
VMValue Date_GetEpoch(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)time(NULL));
}
/***
 * Date.GetWeekday
 * \desc Gets the current day of the week, starting from 1 January 1970, 0:00 UTC.
 * \return <ref WEEKDAY_*> The day of the week.
 * \ns Date
 */
VMValue Date_GetWeekday(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((((int)time(NULL) / 86400) + 3) % 7);
}
/***
 * Date.GetSecond
 * \desc Gets the the second of the current minute.
 * \return integer The second of the minute (from 0 to 59).
 * \ns Date
 */
VMValue Date_GetSecond(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)time(NULL) % 60);
}
/***
 * Date.GetMinute
 * \desc Gets the the minute of the current hour.
 * \return integer The minute of the hour (from 0 to 59).
 * \ns Date
 */
VMValue Date_GetMinute(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(((int)time(NULL) / 60) % 60);
}
/***
 * Date.GetHour
 * \desc Gets the the hour of the current day.
 * \return integer The hour of the day (from 0 to 23).
 * \ns Date
 */
VMValue Date_GetHour(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(((int)time(NULL) / 3600) % 24);
}
/***
 * Date.GetTimeOfDay
 * \desc Gets the the current time of the day (Morning, Midday, Evening, Night).
 * \return <ref TIMEOFDAY_*> The time of day based on the current hour.
 * \ns Date
 */
VMValue Date_GetTimeOfDay(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int hour = ((int)time(NULL) / 3600) % 24;

	if (hour >= 5 && hour <= 11) {
		return INTEGER_VAL((int)TimeOfDay::MORNING);
	}
	else if (hour >= 12 && hour <= 16) {
		return INTEGER_VAL((int)TimeOfDay::MIDDAY);
	}
	else if (hour >= 17 && hour <= 20) {
		return INTEGER_VAL((int)TimeOfDay::EVENING);
	}
	else {
		return INTEGER_VAL((int)TimeOfDay::NIGHT);
	}
}
/***
 * Date.GetTicks
 * \desc Gets the milliseconds since the application began running.
 * \return decimal Returns milliseconds since the application began running.
 * \ns Date
 */
VMValue Date_GetTicks(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return DECIMAL_VAL((float)Clock::GetTicks());
}
// #endregion

// #region Device
/***
 * Device.GetPlatform
 * \desc Gets the platform the application is currently running on.
 * \return <ref Platform_*> Returns the current platform.
 * \ns Device
 */
VMValue Device_GetPlatform(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)Application::Platform);
}
/***
 * Device.IsPC
 * \desc Determines whether the application is running on a personal computer OS (Windows, MacOS, Linux).
 * \return boolean Returns whether the device running on a computer OS.
 * \ns Device
 */
VMValue Device_IsPC(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	bool isPC = Application::IsPC();
	return INTEGER_VAL((int)isPC);
}
/***
 * Device.IsMobile
 * \desc Determines whether the application is running on a mobile device.
 * \return boolean Returns whether the device is running on a mobile OS.
 * \ns Device
 */
VMValue Device_IsMobile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	bool isMobile = Application::IsMobile();
	return INTEGER_VAL((int)isMobile);
}
/***
 * Device.GetCapability
 * \desc Checks a capability of the device the application is running on.
 * \param capability (string): The capability.
 * \return value The return value of this function depends on the capability being queried; however, if the capability is not present, this function returns `null`.
 * \ns Device
 */
VMValue Device_GetCapability(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* capType = GET_ARG(0, GetString);

	Capability capability = Application::GetCapability(capType);

	switch (capability.Type) {
	case Capability::TYPE_INTEGER:
		return INTEGER_VAL(capability.AsInteger);
	case Capability::TYPE_DECIMAL:
		return DECIMAL_VAL(capability.AsDecimal);
	case Capability::TYPE_BOOL:
		return INTEGER_VAL(capability.AsBoolean ? 1 : 0);
	case Capability::TYPE_STRING:
		if (ScriptManager::Lock()) {
			VMValue value = OBJECT_VAL(TakeString(capability.AsString));
			ScriptManager::Unlock();
			return value;
		}
		else {
			Memory::Free(capability.AsString); // Oh, okay...
		}
		break;
	}

	return NULL_VAL;
}
// #endregion

// #region Directory
/***
 * Directory.Create
 * \desc Creates a folder at the path.
 * \param path (string): The path of the folder to create.
 * \return boolean Returns whether folder creation was successful.
 * \ns Directory
 */
VMValue Directory_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* directory = GET_ARG(0, GetString);
	return INTEGER_VAL(Directory::Create(directory));
}
/***
 * Directory.Exists
 * \desc Determines if the folder at the path exists.
 * \param path (string): The path of the folder to check for existence.
 * \return boolean Returns whether the folder exists.
 * \ns Directory
 */
VMValue Directory_Exists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* directory = GET_ARG(0, GetString);
	return INTEGER_VAL(Directory::Exists(directory));
}
/***
 * Directory.GetFiles
 * \desc Gets the paths of all the files in the directory.
 * \param directory (string): The path of the folder to find files in.
 * \param pattern (string): The search pattern for the files. (ex: "*" for any file, "*.*" any file name with any file type, "*.png" any PNG file)
 * \param allDirs (boolean): Whether to search into all folders in the directory.
 * \return array Returns an array containing the filepaths as strings.
 * \ns Directory
 */
VMValue Directory_GetFiles(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ObjArray* array = NULL;
	char* directory = GET_ARG(0, GetString);
	char* pattern = GET_ARG(1, GetString);
	int allDirs = GET_ARG(2, GetInteger);

	std::vector<std::filesystem::path> fileList;
	Directory::GetFiles(&fileList, directory, pattern, allDirs);

	if (ScriptManager::Lock()) {
		array = NewArray();
		for (size_t i = 0; i < fileList.size(); i++) {
			ObjString* part = CopyString(fileList[i]);
			array->Values->push_back(OBJECT_VAL(part));
		}
		ScriptManager::Unlock();
	}

	return OBJECT_VAL(array);
}
/***
 * Directory.GetDirectories
 * \desc Gets the paths of all the folders in the directory.
 * \param directory (string): The path of the folder to find folders in.
 * \param pattern (string): The search pattern for the folders. (ex: "*" for any folder, "image*" any folder that starts with "image")
 * \param allDirs (boolean): Whether to search into all folders in the directory.
 * \return array Returns an array containing the filepaths as strings.
 * \ns Directory
 */
VMValue Directory_GetDirectories(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ObjArray* array = NULL;
	char* directory = GET_ARG(0, GetString);
	char* pattern = GET_ARG(1, GetString);
	int allDirs = GET_ARG(2, GetInteger);

	std::vector<std::filesystem::path> fileList;
	Directory::GetDirectories(&fileList, directory, pattern, allDirs);

	if (ScriptManager::Lock()) {
		array = NewArray();
		for (size_t i = 0; i < fileList.size(); i++) {
			ObjString* part = CopyString(fileList[i]);
			array->Values->push_back(OBJECT_VAL(part));
		}
		ScriptManager::Unlock();
	}

	return OBJECT_VAL(array);
}
// #endregion

// #region Display
/***
 * Display.GetWidth
 * \desc Gets the width of the current display.
 * \paramOpt index (integer): The display index to get the width of.
 * \return integer Returns the width of the current display.
 * \ns Display
 */
VMValue Display_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	SDL_DisplayMode dm;
	SDL_GetCurrentDisplayMode(0, &dm);
	return INTEGER_VAL(dm.w);
}
/***
 * Display.GetHeight
 * \desc Gets the height of the current display.
 * \paramOpt index (integer): The display index to get the width of.
 * \return integer Returns the height of the current display.
 * \ns Display
 */
VMValue Display_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	SDL_DisplayMode dm;
	SDL_GetCurrentDisplayMode(0, &dm);
	return INTEGER_VAL(dm.h);
}
// #endregion

// #region Draw
/***
 * Draw.Sprite
 * \desc Draws a sprite.
 * \param sprite (integer): Index of the loaded sprite.
 * \param animation (integer): Index of the animation entry.
 * \param frame (integer): Index of the frame in the animation entry.
 * \param x (number): X position of where to draw the sprite.
 * \param y (number): Y position of where to draw the sprite.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \paramOpt scaleX (number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (number): Scale multiplier of the sprite vertically.
 * \paramOpt rotation (number): Rotation of the drawn sprite in radians, or in integer if <param useInteger> is `true`.
 * \paramOpt useInteger (number): Whether the rotation argument is already in radians.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_Sprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(7);

	ISprite* sprite = (GET_ARG(0, GetInteger) > -1) ? GET_ARG(0, GetSprite) : NULL;
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int x = (int)GET_ARG(3, GetDecimal);
	int y = (int)GET_ARG(4, GetDecimal);
	int flipX = GET_ARG(5, GetInteger);
	int flipY = GET_ARG(6, GetInteger);
	float scaleX = GET_ARG_OPT(7, GetDecimal, 1.0f);
	float scaleY = GET_ARG_OPT(8, GetDecimal, 1.0f);
	float rotation = GET_ARG_OPT(9, GetDecimal, 0.0f);
	bool useInteger = GET_ARG_OPT(10, GetInteger, false);
	int paletteID = GET_ARG_OPT(11, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (sprite && animation >= 0 && frame >= 0) {
		if (useInteger) {
			int rot = (int)rotation;
			switch (int rotationStyle = sprite->Animations[animation].Flags) {
			case ROTSTYLE_NONE:
				rot = 0;
				break;
			case ROTSTYLE_FULL:
				rot = rot & 0x1FF;
				break;
			case ROTSTYLE_45DEG:
				rot = (rot + 0x20) & 0x1C0;
				break;
			case ROTSTYLE_90DEG:
				rot = (rot + 0x40) & 0x180;
				break;
			case ROTSTYLE_180DEG:
				rot = (rot + 0x80) & 0x100;
				break;
			case ROTSTYLE_STATICFRAMES:
				break; // Not implemented here because it requires extra fields from an entity
			default:
				break;
			}
			rotation = rot * M_PI / 256.0;
		}

		Graphics::DrawSprite(sprite,
			animation,
			frame,
			x,
			y,
			flipX,
			flipY,
			scaleX,
			scaleY,
			rotation,
			(unsigned)paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.SpriteBasic
 * \desc Draws a sprite based on an entity's current values (Sprite, CurrentAnimation, CurrentFrame, X, Y, Direction, ScaleX, ScaleY, Rotation).
 * \param entity (Entity): The entity to draw.
 * \paramOpt x (number): X position of where to draw the sprite, otherwise uses the entity's X value.
 * \paramOpt y (number): Y position of where to draw the sprite, otherwise uses the entity's Y value.
 * \ns Draw
 */
VMValue Draw_SpriteBasic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	ObjEntity* instance = GET_ARG(0, GetEntity);
	Entity* entity = (Entity*)instance->EntityPtr;
	int x = (int)GET_ARG_OPT(1, GetDecimal, entity->X);
	int y = (int)GET_ARG_OPT(2, GetDecimal, entity->Y);
	ISprite* sprite = GetSpriteIndex(entity->Sprite, threadID);
	float rotation = 0.0f;

	if (entity && sprite && entity->CurrentAnimation >= 0 && entity->CurrentFrame >= 0) {
		int rot = (int)entity->Rotation;
		int frame = entity->CurrentFrame;
		switch (entity->RotationStyle) {
		case ROTSTYLE_NONE:
			rot = 0;
			break;
		case ROTSTYLE_FULL:
			rot = rot & 0x1FF;
			break;
		case ROTSTYLE_45DEG:
			rot = (rot + 0x20) & 0x1C0;
			break;
		case ROTSTYLE_90DEG:
			rot = (rot + 0x40) & 0x180;
			break;
		case ROTSTYLE_180DEG:
			rot = (rot + 0x80) & 0x100;
			break;
		case ROTSTYLE_STATICFRAMES:
			if (rot >= 0x100) {
				rot = 0x08 - ((0x214 - rot) >> 6);
			}
			else {
				rot = (rot + 20) >> 6;
			}

			switch (rot) {
			case 0: // 0 degrees
			case 8: // 360 degrees
				rot = 0x00;
				break;

			case 1: // 45 degrees
				rot = 0x80;
				frame += entity->CurrentFrameCount;
				if (entity->Direction) {
					rot = 0x00;
				}
				break;

			case 2: // 90 degrees
				rot = 0x80;
				break;

			case 3: // 135 degrees
				rot = 0x100;
				frame += entity->CurrentFrameCount;
				if (entity->Direction) {
					rot = 0x80;
				}
				break;

			case 4: // 180 degrees
				rot = 0x100;
				break;

			case 5: // 225 degrees
				rot = 0x180;
				frame += entity->CurrentFrameCount;
				if (entity->Direction) {
					rot = 0x100;
				}
				break;

			case 6: // 270 degrees
				rot = 0x180;
				break;

			case 7: // 315 degrees
				rot = 0x180;
				frame += entity->CurrentFrameCount;
				if (!entity->Direction) {
					rot = 0x00;
				}
				break;

			default:
				break;
			}
			break;
		default:
			break;
		}
		rotation = rot * M_PI / 256.0;

		Graphics::DrawSprite(sprite,
			entity->CurrentAnimation,
			frame,
			x,
			y,
			entity->Direction & FLIP_X,
			entity->Direction & FLIP_Y,
			entity->ScaleX,
			entity->ScaleY,
			rotation);
	}
	return NULL_VAL;
}
/***
 * Draw.Animator
 * \desc Draws an animator based on its current values (Sprite, CurrentAnimation, CurrentFrame) and other provided values.
 * \param animator (integer): The animator to draw.
 * \param x (number): X position of where to draw the sprite.
 * \param y (number): Y position of where to draw the sprite.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \paramOpt scaleX (number): Scale multiplier of the sprite horizontally. (default: `1.0`)
 * \paramOpt scaleY (number): Scale multiplier of the sprite vertically. (default: `1.0`)
 * \paramOpt rotation (number): Rotation of the drawn sprite, from 0-511. (default: `0.0`)
 * \ns Draw
 */
VMValue Draw_Animator(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);

	Animator* animator = GET_ARG(0, GetAnimator);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);
	int flipX = GET_ARG(3, GetInteger);
	int flipY = GET_ARG(4, GetInteger);
	float scaleX = (argCount > 5) ? GET_ARG(5, GetDecimal) : 1.0f;
	float scaleY = (argCount > 6) ? GET_ARG(6, GetDecimal) : 1.0f;
	float rotation = (argCount > 7) ? GET_ARG(7, GetDecimal) : 0.0f;

	if (!animator || !animator->Frames.size()) {
		return NULL_VAL;
	}

	if (animator->Sprite >= 0 && animator->CurrentAnimation >= 0 &&
		animator->CurrentFrame >= 0) {
		ISprite* sprite = GetSpriteIndex(animator->Sprite, threadID);
		if (!sprite) {
			return NULL_VAL;
		}

		int rot = (int)rotation;
		switch (animator->RotationStyle) {
		case ROTSTYLE_NONE:
			rot = 0;
			break;
		case ROTSTYLE_FULL:
			rot = rot & 0x1FF;
			break;
		case ROTSTYLE_45DEG:
			rot = (rot + 0x20) & 0x1C0;
			break;
		case ROTSTYLE_90DEG:
			rot = (rot + 0x40) & 0x180;
			break;
		case ROTSTYLE_180DEG:
			rot = (rot + 0x80) & 0x100;
			break;
		case ROTSTYLE_STATICFRAMES:
			break; // Not implemented here because it requires extra fields from an entity
		default:
			break;
		}
		rotation = rot * M_PI / 256.0;

		Graphics::DrawSprite(sprite,
			animator->CurrentAnimation,
			animator->CurrentFrame,
			x,
			y,
			flipX,
			flipY,
			scaleX,
			scaleY,
			rotation);
	}
	return NULL_VAL;
}
/***
 * Draw.AnimatorBasic
 * \desc Draws an animator based on its current values (Sprite, CurrentAnimation, CurrentFrame) and an entity's other values (X, Y, Direction, ScaleX, ScaleY, Rotation).
 * \param animator (integer): The animator to draw.
 * \param entity (Entity): The entity to pull other values from.
 * \paramOpt x (number): X position of where to draw the sprite, otherwise uses the entity's X value.
 * \paramOpt y (number): Y position of where to draw the sprite, otherwise uses the entity's Y value.
 * \ns Draw
 */
VMValue Draw_AnimatorBasic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);

	Animator* animator = GET_ARG(0, GetAnimator);
	ObjEntity* instance = GET_ARG(1, GetEntity);
	Entity* entity = (Entity*)instance->EntityPtr;
	int x = (int)GET_ARG_OPT(2, GetDecimal, entity->X);
	int y = (int)GET_ARG_OPT(3, GetDecimal, entity->Y);
	float rotation = 0.0f;

	if (!animator || !animator->Frames.size()) {
		return NULL_VAL;
	}

	if (entity && animator->Sprite >= 0 && animator->CurrentAnimation >= 0 &&
		animator->CurrentFrame >= 0) {
		ISprite* sprite = GetSpriteIndex(animator->Sprite, threadID);
		if (!sprite) {
			return NULL_VAL;
		}

		int rot = (int)entity->Rotation;
		int frame = animator->CurrentFrame;
		switch (animator->RotationStyle) {
		case ROTSTYLE_NONE:
			rot = 0;
			break;
		case ROTSTYLE_FULL:
			rot = rot & 0x1FF;
			break;
		case ROTSTYLE_45DEG:
			rot = (rot + 0x20) & 0x1C0;
			break;
		case ROTSTYLE_90DEG:
			rot = (rot + 0x40) & 0x180;
			break;
		case ROTSTYLE_180DEG:
			rot = (rot + 0x80) & 0x100;
			break;
		case ROTSTYLE_STATICFRAMES:
			if (rot >= 0x100) {
				rot = 0x08 - ((0x214 - rot) >> 6);
			}
			else {
				rot = (rot + 20) >> 6;
			}

			switch (rot) {
			case 0: // 0 degrees
			case 8: // 360 degrees
				rot = 0x00;
				break;

			case 1: // 45 degrees
				rot = 0x80;
				frame += animator->FrameCount;
				if (entity->Direction) {
					rot = 0x00;
				}
				break;

			case 2: // 90 degrees
				rot = 0x80;
				break;

			case 3: // 135 degrees
				rot = 0x100;
				frame += animator->FrameCount;
				if (entity->Direction) {
					rot = 0x80;
				}
				break;

			case 4: // 180 degrees
				rot = 0x100;
				break;

			case 5: // 225 degrees
				rot = 0x180;
				frame += animator->FrameCount;
				if (entity->Direction) {
					rot = 0x100;
				}
				break;

			case 6: // 270 degrees
				rot = 0x180;
				break;

			case 7: // 315 degrees
				rot = 0x180;
				frame += animator->FrameCount;
				if (!entity->Direction) {
					rot = 0x00;
				}
				break;

			default:
				break;
			}
			break;
		default:
			break;
		}
		rotation = rot * M_PI / 256.0;

		Graphics::DrawSprite(sprite,
			animator->CurrentAnimation,
			frame,
			x,
			y,
			entity->Direction & FLIP_X,
			entity->Direction & FLIP_Y,
			entity->ScaleX,
			entity->ScaleY,
			rotation);
	}
	return NULL_VAL;
}
/***
 * Draw.SpritePart
 * \desc Draws part of a sprite.
 * \param sprite (integer): Index of the loaded sprite.
 * \param animation (integer): Index of the animation entry.
 * \param frame (integer): Index of the frame in the animation entry.
 * \param x (number): X position of where to draw the sprite.
 * \param y (number): Y position of where to draw the sprite.
 * \param partX (integer): X coordinate of part of frame to draw.
 * \param partY (integer): Y coordinate of part of frame to draw.
 * \param partW (integer): Width of part of frame to draw.
 * \param partH (integer): Height of part of frame to draw.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \paramOpt scaleX (number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (number): Scale multiplier of the sprite vertically.
 * \paramOpt rotation (number): Rotation of the drawn sprite in radians, or in integer if <param useInteger> is `true`.
 * \paramOpt useInteger (number): Whether the rotation argument is already in radians.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_SpritePart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(11);

	ISprite* sprite = (GET_ARG(0, GetInteger) > -1) ? GET_ARG(0, GetSprite) : NULL;
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int x = (int)GET_ARG(3, GetDecimal);
	int y = (int)GET_ARG(4, GetDecimal);
	int sx = (int)GET_ARG(5, GetDecimal);
	int sy = (int)GET_ARG(6, GetDecimal);
	int sw = (int)GET_ARG(7, GetDecimal);
	int sh = (int)GET_ARG(8, GetDecimal);
	int flipX = GET_ARG(9, GetInteger);
	int flipY = GET_ARG(10, GetInteger);
	float scaleX = GET_ARG_OPT(11, GetDecimal, 1.0f);
	float scaleY = GET_ARG_OPT(12, GetDecimal, 1.0f);
	float rotation = GET_ARG_OPT(13, GetDecimal, 0.0f);
	bool useInteger = GET_ARG_OPT(14, GetInteger, false);
	int paletteID = GET_ARG_OPT(15, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (sprite && animation >= 0 && frame >= 0) {
		if (useInteger) {
			int rot = (int)rotation;
			switch (int rotationStyle = sprite->Animations[animation].Flags) {
			case ROTSTYLE_NONE:
				rot = 0;
				break;
			case ROTSTYLE_FULL:
				rot = rot & 0x1FF;
				break;
			case ROTSTYLE_45DEG:
				rot = (rot + 0x20) & 0x1C0;
				break;
			case ROTSTYLE_90DEG:
				rot = (rot + 0x40) & 0x180;
				break;
			case ROTSTYLE_180DEG:
				rot = (rot + 0x80) & 0x100;
				break;
			case ROTSTYLE_STATICFRAMES:
				break;
			default:
				break;
			}
			rotation = rot * M_PI / 256.0;
		}

		Graphics::DrawSpritePart(sprite,
			animation,
			frame,
			sx,
			sy,
			sw,
			sh,
			x,
			y,
			flipX,
			flipY,
			scaleX,
			scaleY,
			rotation,
			(unsigned)paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.Image
 * \desc Draws an image.
 * \param image (integer): Index of the loaded image.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_Image(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);

	Image* image = GET_ARG(0, GetImage);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	int paletteID = GET_ARG_OPT(3, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (image) {
		Graphics::DrawTexture(image->TexturePtr,
			0,
			0,
			image->TexturePtr->Width,
			image->TexturePtr->Height,
			x,
			y,
			image->TexturePtr->Width,
			image->TexturePtr->Height,
			paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.ImagePart
 * \desc Draws part of an image.
 * \param image (integer): Index of the loaded image.
 * \param partX (integer): X coordinate of part of image to draw.
 * \param partY (integer): Y coordinate of part of image to draw.
 * \param partW (integer): Width of part of image to draw.
 * \param partH (integer): Height of part of image to draw.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_ImagePart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(7);

	Image* image = GET_ARG(0, GetImage);
	float sx = GET_ARG(1, GetDecimal);
	float sy = GET_ARG(2, GetDecimal);
	float sw = GET_ARG(3, GetDecimal);
	float sh = GET_ARG(4, GetDecimal);
	float x = GET_ARG(5, GetDecimal);
	float y = GET_ARG(6, GetDecimal);
	int paletteID = GET_ARG_OPT(7, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (image) {
		Graphics::DrawTexture(image->TexturePtr, sx, sy, sw, sh, x, y, sw, sh, paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.ImageSized
 * \desc Draws an image, but sized.
 * \param image (integer): Index of the loaded image.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \param width (number): Width to draw the image.
 * \param height (number): Height to draw the image.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_ImageSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);

	Image* image = GET_ARG(0, GetImage);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float w = GET_ARG(3, GetDecimal);
	float h = GET_ARG(4, GetDecimal);
	int paletteID = GET_ARG_OPT(5, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (image) {
		Graphics::DrawTexture(image->TexturePtr,
			0,
			0,
			image->TexturePtr->Width,
			image->TexturePtr->Height,
			x,
			y,
			w,
			h,
			paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.ImagePartSized
 * \desc Draws part of an image, but sized.
 * \param image (integer): Index of the loaded image.
 * \param partX (integer): X coordinate of part of image to draw.
 * \param partY (integer): Y coordinate of part of image to draw.
 * \param partW (integer): Width of part of image to draw.
 * \param partH (integer): Height of part of image to draw.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \param width (number): Width to draw the image.
 * \param height (number): Height to draw the image.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_ImagePartSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(9);

	Image* image = GET_ARG(0, GetImage);
	float sx = GET_ARG(1, GetDecimal);
	float sy = GET_ARG(2, GetDecimal);
	float sw = GET_ARG(3, GetDecimal);
	float sh = GET_ARG(4, GetDecimal);
	float x = GET_ARG(5, GetDecimal);
	float y = GET_ARG(6, GetDecimal);
	float w = GET_ARG(7, GetDecimal);
	float h = GET_ARG(8, GetDecimal);
	int paletteID = GET_ARG_OPT(9, GetInteger, 0);

	CHECK_PALETTE_INDEX(paletteID);

	if (image) {
		Graphics::DrawTexture(image->TexturePtr, sx, sy, sw, sh, x, y, w, h, paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.Layer
 * \desc Draws a layer.
 * \param layerIndex (integer): Index of layer.
 * \ns Draw
 */
VMValue Draw_Layer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (Graphics::CurrentView) {
		Graphics::DrawSceneLayer(
			&Scene::Layers[index], Graphics::CurrentView, index, false);
	}
	return NULL_VAL;
}
/***
 * Draw.View
 * \desc Draws a view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): X position of where to draw the view.
 * \param y (number): Y position of where to draw the view.
 * \ns Draw
 */
#define CHECK_VIEW_INDEX() \
	if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) { \
		OUT_OF_RANGE_ERROR("View index", view_index, 0, MAX_SCENE_VIEWS - 1); \
		return NULL_VAL; \
	}
#define CHECK_RENDER_VIEW() \
	if (view_index == Scene::ViewCurrent) { \
		THROW_ERROR("Cannot draw current view!"); \
		return NULL_VAL; \
	} \
	if (!Scene::Views[view_index].UseDrawTarget) { \
		THROW_ERROR("Cannot draw view %d if it lacks a draw target!", view_index); \
		return NULL_VAL; \
	}
#define DO_RENDER_VIEW() \
	Graphics::PushState(); \
	int current_view = Scene::ViewCurrent; \
	Scene::RenderView(view_index, false); \
	Scene::SetView(current_view); \
	Graphics::PopState()
VMValue Draw_View(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	int view_index = GET_ARG(0, GetInteger);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	CHECK_VIEW_INDEX();

	CHECK_RENDER_VIEW();
	DO_RENDER_VIEW();

	Texture* texture = Scene::Views[view_index].DrawTarget;
	Graphics::DrawTexture(texture,
		0,
		0,
		texture->Width,
		texture->Height,
		x,
		y,
		texture->Width,
		texture->Height);
	return NULL_VAL;
}
/***
 * Draw.ViewPart
 * \desc Draws part of a view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): X position of where to draw the view.
 * \param y (number): Y position of where to draw the view.
 * \param partX (integer): X coordinate of part of view to draw.
 * \param partY (integer): Y coordinate of part of view to draw.
 * \param partW (integer): Width of part of view to draw.
 * \param partH (integer): Height of part of view to draw.
 * \ns Draw
 */
VMValue Draw_ViewPart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);

	int view_index = GET_ARG(0, GetInteger);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float sx = GET_ARG(3, GetDecimal);
	float sy = GET_ARG(4, GetDecimal);
	float sw = GET_ARG(5, GetDecimal);
	float sh = GET_ARG(6, GetDecimal);
	CHECK_VIEW_INDEX();

	CHECK_RENDER_VIEW();
	DO_RENDER_VIEW();

	Graphics::DrawTexture(Scene::Views[view_index].DrawTarget, sx, sy, sw, sh, x, y, sw, sh);
	return NULL_VAL;
}
/***
 * Draw.ViewSized
 * \desc Draws a view, but sized.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): X position of where to draw the view.
 * \param y (number): Y position of where to draw the view.
 * \param width (number): Width to draw the view.
 * \param height (number): Height to draw the view.
 * \ns Draw
 */
VMValue Draw_ViewSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);

	int view_index = GET_ARG(0, GetInteger);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float w = GET_ARG(3, GetDecimal);
	float h = GET_ARG(4, GetDecimal);
	CHECK_VIEW_INDEX();

	CHECK_RENDER_VIEW();
	DO_RENDER_VIEW();

	Texture* texture = Scene::Views[view_index].DrawTarget;
	Graphics::DrawTexture(texture, 0, 0, texture->Width, texture->Height, x, y, w, h);
	return NULL_VAL;
}
/***
 * Draw.ViewPartSized
 * \desc Draws part of a view, but sized.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): X position of where to draw the view.
 * \param y (number): Y position of where to draw the view.
 * \param partX (integer): X coordinate of part of view to draw.
 * \param partY (integer): Y coordinate of part of view to draw.
 * \param partW (integer): Width of part of view to draw.
 * \param partH (integer): Height of part of view to draw.
 * \param width (number): Width to draw the view.
 * \param height (number): Height to draw the view.
 * \ns Draw
 */
VMValue Draw_ViewPartSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(9);

	int view_index = GET_ARG(0, GetInteger);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float sx = GET_ARG(3, GetDecimal);
	float sy = GET_ARG(4, GetDecimal);
	float sw = GET_ARG(5, GetDecimal);
	float sh = GET_ARG(6, GetDecimal);
	float w = GET_ARG(7, GetDecimal);
	float h = GET_ARG(8, GetDecimal);
	CHECK_VIEW_INDEX();

	CHECK_RENDER_VIEW();
	DO_RENDER_VIEW();

	Graphics::DrawTexture(Scene::Views[view_index].DrawTarget, sx, sy, sw, sh, x, y, w, h);
	return NULL_VAL;
}
/***
 * Draw.Video
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Video(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	MediaBag* video = GET_ARG(0, GetVideo);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);

	Graphics::DrawTexture(video->VideoTexture,
		0,
		0,
		video->VideoTexture->Width,
		video->VideoTexture->Height,
		x,
		y,
		video->VideoTexture->Width,
		video->VideoTexture->Height);
	return NULL_VAL;
}
/***
 * Draw.VideoPart
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_VideoPart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);

	MediaBag* video = GET_ARG(0, GetVideo);
	float sx = GET_ARG(1, GetDecimal);
	float sy = GET_ARG(2, GetDecimal);
	float sw = GET_ARG(3, GetDecimal);
	float sh = GET_ARG(4, GetDecimal);
	float x = GET_ARG(5, GetDecimal);
	float y = GET_ARG(6, GetDecimal);

	Graphics::DrawTexture(video->VideoTexture, sx, sy, sw, sh, x, y, sw, sh);
	return NULL_VAL;
}
/***
 * Draw.VideoSized
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_VideoSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);

	MediaBag* video = GET_ARG(0, GetVideo);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float w = GET_ARG(3, GetDecimal);
	float h = GET_ARG(4, GetDecimal);

#ifdef USING_FFMPEG
	video->Player->GetVideoData(video->VideoTexture);
#endif

	Graphics::DrawTexture(video->VideoTexture,
		0,
		0,
		video->VideoTexture->Width,
		video->VideoTexture->Height,
		x,
		y,
		w,
		h);
	return NULL_VAL;
}
/***
 * Draw.VideoPartSized
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_VideoPartSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(9);

	MediaBag* video = GET_ARG(0, GetVideo);
	float sx = GET_ARG(1, GetDecimal);
	float sy = GET_ARG(2, GetDecimal);
	float sw = GET_ARG(3, GetDecimal);
	float sh = GET_ARG(4, GetDecimal);
	float x = GET_ARG(5, GetDecimal);
	float y = GET_ARG(6, GetDecimal);
	float w = GET_ARG(7, GetDecimal);
	float h = GET_ARG(8, GetDecimal);

	Graphics::DrawTexture(video->VideoTexture, sx, sy, sw, sh, x, y, w, h);
	return NULL_VAL;
}
/***
 * Draw.Tile
 * \desc Draws a tile.
 * \param ID (integer): ID of the tile to draw.
 * \param x (number): X position of where to draw the tile.
 * \param y (number): Y position of where to draw the tile.
 * \paramOpt flipX (integer): Whether to flip the tile horizontally.
 * \paramOpt flipY (integer): Whether to flip the tile vertically.
 * \paramOpt scaleX (number): Horizontal scale multiplier of the tile.
 * \paramOpt scaleY (number): Vertical scale multiplier of the tile.
 * \paramOpt rotation (number): Rotation of the drawn tile in radians.
 * \paramOpt paletteID (integer): Which palette index to use.
 * \ns Draw
 */
VMValue Draw_Tile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);

	Uint32 id = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal) + 8;
	int y = (int)GET_ARG(2, GetDecimal) + 8;
	int flipX = GET_ARG_OPT(3, GetInteger, false);
	int flipY = GET_ARG_OPT(4, GetInteger, false);
	float scaleX = GET_ARG_OPT(5, GetDecimal, 1.0f);
	float scaleY = GET_ARG_OPT(6, GetDecimal, 1.0f);
	float rotation = GET_ARG_OPT(7, GetDecimal, 0.0f);
	int paletteID = -1;

	if (argCount >= 9) {
		paletteID = GET_ARG(8, GetInteger);

		CHECK_PALETTE_INDEX(paletteID);
	}

	TileSpriteInfo info;
	if (id < Scene::TileSpriteInfos.size() &&
		(info = Scene::TileSpriteInfos[id]).Sprite != NULL) {

		if (paletteID == -1) {
			paletteID = Scene::Tilesets[info.TilesetID].PaletteID;
		}

		Graphics::DrawSprite(info.Sprite,
			info.AnimationIndex,
			info.FrameIndex,
			x,
			y,
			flipX,
			flipY,
			scaleX,
			scaleY,
			rotation,
			paletteID);
	}
	return NULL_VAL;
}
/***
 * Draw.Texture
 * \desc Draws a texture.
 * \param texture (integer): Texture index.
 * \param x (number): X position of where to draw the texture.
 * \param y (number): Y position of where to draw the texture.
 * \ns Draw
 */
VMValue Draw_Texture(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	GameTexture* gameTexture = GET_ARG(0, GetTexture);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);

	if (!gameTexture) {
		return NULL_VAL;
	}

	Texture* texture = gameTexture->GetTexture();
	if (texture) {
		Graphics::DrawTexture(texture,
			0,
			0,
			texture->Width,
			texture->Height,
			x,
			y,
			texture->Width,
			texture->Height);
	}
	return NULL_VAL;
}
/***
 * Draw.TextureSized
 * \desc Draws a texture, but sized.
 * \param texture (integer): Texture index.
 * \param x (number): X position of where to draw the texture.
 * \param y (number): Y position of where to draw the texture.
 * \param width (number): Width to draw the texture.
 * \param height (number): Height to draw the texture.
 * \ns Draw
 */
VMValue Draw_TextureSized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);

	GameTexture* gameTexture = GET_ARG(0, GetTexture);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float w = GET_ARG(3, GetDecimal);
	float h = GET_ARG(4, GetDecimal);

	if (!gameTexture) {
		return NULL_VAL;
	}

	Texture* texture = gameTexture->GetTexture();
	if (texture) {
		Graphics::DrawTexture(texture, 0, 0, texture->Width, texture->Height, x, y, w, h);
	}
	return NULL_VAL;
}
/***
 * Draw.TexturePart
 * \desc Draws part of a texture.
 * \param texture (integer): Texture index.
 * \param partX (integer): X coordinate of part of texture to draw.
 * \param partY (integer): Y coordinate of part of texture to draw.
 * \param partW (integer): Width of part of texture to draw.
 * \param partH (integer): Height of part of texture to draw.
 * \param x (number): X position of where to draw the texture.
 * \param y (number): Y position of where to draw the texture.
 * \ns Draw
 */
VMValue Draw_TexturePart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);

	GameTexture* gameTexture = GET_ARG(0, GetTexture);
	float sx = GET_ARG(1, GetDecimal);
	float sy = GET_ARG(2, GetDecimal);
	float sw = GET_ARG(3, GetDecimal);
	float sh = GET_ARG(4, GetDecimal);
	float x = GET_ARG(5, GetDecimal);
	float y = GET_ARG(6, GetDecimal);

	if (!gameTexture) {
		return NULL_VAL;
	}

	Texture* texture = gameTexture->GetTexture();
	if (texture) {
		Graphics::DrawTexture(texture, sx, sy, sw, sh, x, y, sw, sh);
	}
	return NULL_VAL;
}
/***
 * Draw.SetTextAlign
 * \desc Sets the text drawing horizontal alignment. The initial value is left.<br/>\
This does not affect text drawn using a Font.
 * \param baseline (integer): 0 for left, 1 for center, 2 for right.
 * \ns Draw
 */
VMValue Draw_SetTextAlign(int argCount, VMValue* args, Uint32 threadID) {
	textAlign = GET_ARG(0, GetInteger) / 2.0f;
	return NULL_VAL;
}
/***
 * Draw.SetTextBaseline
 * \desc Sets the text drawing vertical alignment. The initial value is top.<br/>\
This does not affect text drawn using a Font.
 * \param baseline (integer): 0 for top, 1 for baseline, 2 for bottom.
 * \ns Draw
 */
VMValue Draw_SetTextBaseline(int argCount, VMValue* args, Uint32 threadID) {
	textBaseline = GET_ARG(0, GetInteger) / 2.0f;
	return NULL_VAL;
}
/***
 * Draw.SetTextAdvance
 * \desc Sets the character spacing multiplier. The initial value is 1.0.<br/>\
This does not affect text drawn using a Font.
 * \param ascent (number): Multiplier for character spacing.
 * \ns Draw
 */
VMValue Draw_SetTextAdvance(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	textAdvance = GET_ARG(0, GetDecimal);
	return NULL_VAL;
}
/***
 * Draw.SetTextLineAscent
 * \desc Sets the line height multiplier. The initial value is 1.25.<br/>\
This does not affect text drawn using a Font.
 * \param ascent (number): Multiplier for line height.
 * \ns Draw
 */
VMValue Draw_SetTextLineAscent(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	textAscent = GET_ARG(0, GetDecimal);
	return NULL_VAL;
}
/***
 * Draw.MeasureText
 * \desc Measures UTF-8 text using a font and stores max width and max height into the array.
 * \param outArray (array): Array to output size values to.
 * \param font (Font): The Font to be used as text.
 * \param text (string): Text to measure.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \return array Returns the array inputted into the function.
 * \ns Draw
 */
VMValue Draw_MeasureText(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);

	ObjArray* array = GET_ARG(0, GetArray);
	char* text = GET_ARG(2, GetString);
	float fontSize = GET_ARG_OPT(3, GetDecimal, 0.0f);

	float maxW = 0.0f, maxH = 0.0f;

	if (IS_FONT(args[1])) {
		ObjFont* objFont = GET_ARG(1, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 4) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;
		params.Descent = font->Descent;
		params.Leading = font->Leading;

		Graphics::MeasureText(font, text, &params, maxW, maxH);
	}
	else {
		ISprite* sprite = GET_ARG(1, GetSprite);
		if (sprite) {
			LegacyTextDrawParams params;
			params.Ascent = textAscent;
			params.Advance = textAdvance;
			Graphics::MeasureTextLegacy(sprite, text, &params, maxW, maxH);
		}
	}

	if (ScriptManager::Lock()) {
		array->Values->clear();
		array->Values->push_back(DECIMAL_VAL(maxW));
		array->Values->push_back(DECIMAL_VAL(maxH));
		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}
	return NULL_VAL;
}
/***
 * Draw.MeasureTextWrapped
 * \desc Measures wrapped UTF-8 text using a font and stores max width and max height into the array.
 * \param outArray (array): Array to output size values to.
 * \param font (Font): The Font to be used as text.
 * \param text (string): Text to measure.
 * \param maxWidth (number): Max width that a line can be.
 * \paramOpt maxLines (integer): Max number of lines to measure. Use `null` to measure all lines.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \return array Returns the array inputted into the function.
 * \ns Draw
 */
VMValue Draw_MeasureTextWrapped(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);

	ObjArray* array = GET_ARG(0, GetArray);
	char* text = GET_ARG(2, GetString);
	float maxWidth = GET_ARG(3, GetDecimal);
	int maxLines = 0;
	if (argCount > 4 && !IS_NULL(args[4])) {
		maxLines = GET_ARG(4, GetInteger);
	}
	float fontSize = GET_ARG_OPT(5, GetDecimal, 0.0f);

	float maxW = 0.0f, maxH = 0.0f;

	if (IS_FONT(args[1])) {
		ObjFont* objFont = GET_ARG(1, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 6) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;
		params.Descent = font->Descent;
		params.Leading = font->Leading;
		params.MaxWidth = maxWidth;
		params.MaxLines = maxLines;

		Graphics::MeasureTextWrapped(font, text, &params, maxW, maxH);
	}
	else {
		ISprite* sprite = GET_ARG(1, GetSprite);
		if (sprite) {
			LegacyTextDrawParams params;
			params.Ascent = textAscent;
			params.Advance = textAdvance;
			params.MaxWidth = maxWidth;
			params.MaxLines = maxLines;
			Graphics::MeasureTextLegacy(sprite, text, &params, maxW, maxH);
		}
	}

	if (ScriptManager::Lock()) {
		array->Values->clear();
		array->Values->push_back(DECIMAL_VAL(maxW));
		array->Values->push_back(DECIMAL_VAL(maxH));
		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}
	return NULL_VAL;
}
/***
 * Draw.Text
 * \desc Draws UTF-8 text using a font.
 * \param font (Font): The Font to be used as text.
 * \param text (string): Text to draw.
 * \param x (number): X position of where to draw the text.
 * \param y (number): Y position of where to draw the text.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \ns Draw
 */
VMValue Draw_Text(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);

	char* text = GET_ARG(1, GetString);
	float x = GET_ARG(2, GetDecimal);
	float y = GET_ARG(3, GetDecimal);
	float fontSize = GET_ARG_OPT(4, GetDecimal, 0.0f);

	if (IS_FONT(args[0])) {
		ObjFont* objFont = GET_ARG(0, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 5) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;
		params.Descent = font->Descent;
		params.Leading = font->Leading;

		Graphics::DrawText(font, text, x, y, &params);

		return NULL_VAL;
	}

	ISprite* sprite = GET_ARG(0, GetSprite);
	if (sprite) {
		LegacyTextDrawParams params;
		params.Align = textAlign;
		params.Baseline = textBaseline;
		params.Ascent = textAscent;
		params.Advance = textAdvance;
		Graphics::DrawTextLegacy(sprite, text, x, y, &params);
	}

	return NULL_VAL;
}
/***
 * Draw.TextWrapped
 * \desc Draws wrapped UTF-8 text using a font.
 * \param font (Font): The Font to be used as text.
 * \param text (string): Text to draw.
 * \param x (number): X position of where to draw the text.
 * \param y (number): Y position of where to draw the text.
 * \param maxWidth (number): Max width the text can draw in.
 * \paramOpt maxLines (integer): Max lines of text to draw. Use `null` to draw all lines.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \ns Draw
 */
VMValue Draw_TextWrapped(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);

	char* text = GET_ARG(1, GetString);
	float x = GET_ARG(2, GetDecimal);
	float y = GET_ARG(3, GetDecimal);
	float maxWidth = GET_ARG(4, GetDecimal);
	int maxLines = 0;
	if (argCount > 5 && !IS_NULL(args[5])) {
		maxLines = GET_ARG(5, GetInteger);
	}
	float fontSize = GET_ARG_OPT(6, GetDecimal, 0.0f);

	if (IS_FONT(args[0])) {
		ObjFont* objFont = GET_ARG(0, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 7) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;
		params.Descent = font->Descent;
		params.Leading = font->Leading;
		params.MaxWidth = maxWidth;
		params.MaxLines = maxLines;

		Graphics::DrawTextWrapped(font, text, x, y, &params);

		return NULL_VAL;
	}

	ISprite* sprite = GET_ARG(0, GetSprite);
	if (sprite) {
		LegacyTextDrawParams params;
		params.Align = textAlign;
		params.Baseline = textBaseline;
		params.Ascent = textAscent;
		params.Advance = textAdvance;
		params.MaxWidth = maxWidth;
		params.MaxLines = maxLines;
		Graphics::DrawTextWrappedLegacy(sprite, text, x, y, &params);
	}

	return NULL_VAL;
}
/***
 * Draw.TextEllipsis
 * \desc Draws UTF-8 text using a font, but adds ellipsis if the text doesn't fit in <param maxWidth>.
 * \param font (Font): The Font to be used as text.
 * \param text (string): Text to draw.
 * \param x (number): X position of where to draw the text.
 * \param y (number): Y position of where to draw the text.
 * \param maxWidth (number): Max width the text can draw in.
 * \paramOpt maxLines (integer): Max lines of text to draw. Use `null` to draw all lines.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \ns Draw
 */
VMValue Draw_TextEllipsis(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);

	char* text = GET_ARG(1, GetString);
	float x = GET_ARG(2, GetDecimal);
	float y = GET_ARG(3, GetDecimal);
	float maxWidth = GET_ARG(4, GetDecimal);
	int maxLines = 0;
	if (argCount > 5 && !IS_NULL(args[5])) {
		maxLines = GET_ARG(5, GetInteger);
	}
	float fontSize = GET_ARG_OPT(6, GetDecimal, 0.0f);

	if (IS_FONT(args[0])) {
		ObjFont* objFont = GET_ARG(0, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 7) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;
		params.Descent = font->Descent;
		params.Leading = font->Leading;
		params.MaxWidth = maxWidth;
		params.MaxLines = maxLines;

		Graphics::DrawTextEllipsis(font, text, x, y, &params);

		return NULL_VAL;
	}

	ISprite* sprite = GET_ARG(0, GetSprite);
	if (sprite) {
		LegacyTextDrawParams params;
		params.Align = textAlign;
		params.Baseline = textBaseline;
		params.Ascent = textAscent;
		params.Advance = textAdvance;
		params.MaxWidth = maxWidth;
		params.MaxLines = maxLines;
		Graphics::DrawTextEllipsisLegacy(sprite, text, x, y, &params);
	}

	return NULL_VAL;
}
/***
 * Draw.Glyph
 * \desc Draws a glyph for a given code point.
 * \param font (Font): The Font.
 * \param codepoint (integer): Code point to draw.
 * \param x (number): X position of where to draw the glyph.
 * \param y (number): Y position of where to draw the glyph.
 * \paramOpt fontSize (number): The size of the font. If this argument is not given, this uses the pixels per unit value that the font was configured with.
 * \ns Draw
 */
VMValue Draw_Glyph(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);

	Uint32 codepoint = GET_ARG(1, GetInteger);
	float x = GET_ARG(2, GetDecimal);
	float y = GET_ARG(3, GetDecimal);
	float fontSize = GET_ARG_OPT(4, GetDecimal, 0.0f);

	if (IS_FONT(args[0])) {
		ObjFont* objFont = GET_ARG(0, GetFont);
		Font* font = (Font*)objFont->FontPtr;

		if (argCount < 5) {
			fontSize = font->Size;
		}

		TextDrawParams params;
		params.FontSize = fontSize;
		params.Ascent = font->Ascent;

		Graphics::DrawGlyph(font, codepoint, x, y, &params);

		return NULL_VAL;
	}

	ISprite* sprite = GET_ARG(0, GetSprite);
	if (sprite) {
		LegacyTextDrawParams params;
		params.Align = textAlign;
		params.Baseline = textBaseline;
		params.Ascent = textAscent;
		params.Advance = textAdvance;
		Graphics::DrawGlyphLegacy(sprite, codepoint, x, y, &params);
	}

	return NULL_VAL;
}
/***
 * Draw.TextArray
 * \desc Draws a series of sprites based on a converted sprite string.
 * \param sprite (integer): The index of the loaded sprite to be used as text.
 * \param animation (integer): The animation index.
 * \param x (number): The X value to begin drawing.
 * \param y (number): The Y value to begin drawing.
 * \param string (array): The array containing frame indexes.
 * \param startFrame (integer): The index to begin drawing.
 * \param endFrame (integer): The index to end drawing.
 * \param align (integer): The text alignment.
 * \param spacing (integer): The space between drawn sprites.
 * \paramOpt charOffsetsX (array): The X offsets at which to draw per frame. Must also have <param charOffsetsY> to be used.
 * \paramOpt charOffsetsY (array): The Y offsets at which to draw per frame.
 * \ns Draw
 */
VMValue Draw_TextArray(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(9);
	if (ScriptManager::Lock()) {
		ISprite* sprite = GET_ARG(0, GetSprite);
		int animation = GET_ARG(1, GetInteger);
		float x = GET_ARG(2, GetDecimal);
		float y = GET_ARG(3, GetDecimal);
		ObjArray* string = GET_ARG(4, GetArray);
		int startFrame = GET_ARG(5, GetInteger);
		int endFrame = GET_ARG(6, GetInteger);
		int align = GET_ARG(7, GetInteger);
		int spacing = GET_ARG(8, GetInteger);
		ObjArray* charOffsetsX = GET_ARG_OPT(9, GetArray, nullptr);
		ObjArray* charOffsetsY = GET_ARG_OPT(10, GetArray, nullptr);

		if (sprite && string && animation >= 0 && animation < (int)sprite->Animations.size()) {
			startFrame = (int)Math::Clamp(startFrame, 0, (int)string->Values->size() - 1);

			if (endFrame <= 0 || endFrame > (int)string->Values->size())
				endFrame = (int)string->Values->size();

			int charOffsetIndex = 0;
			switch (align) {
			case ALIGN_LEFT:
				if (charOffsetsX && charOffsetsY
					&& charOffsetsX->Values->size() >= (endFrame - startFrame)
					&& charOffsetsY->Values->size() >= (endFrame - startFrame)) {
					for (; startFrame < endFrame; ++startFrame) {
						VMValue val = (*string->Values)[startFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							VMValue xVal = (*charOffsetsX->Values)[charOffsetIndex];
							float xOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(xVal, threadID)) {
								xOffset = AS_DECIMAL(xVal);
							}
							VMValue yVal = (*charOffsetsY->Values)[charOffsetIndex];
							float yOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(yVal, threadID)) {
								yOffset = AS_DECIMAL(yVal);
							}
							Graphics::DrawSprite(sprite, animation, curChar, x + xOffset, y + yOffset, false, false, 1.0f, 1.0f, 0.0f);
							x += spacing + frame.Width;
							++charOffsetIndex;
						}
					}
				}
				else {
					for (; startFrame < endFrame; ++startFrame) {
						VMValue val = (*string->Values)[startFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							Graphics::DrawSprite(sprite, animation, curChar, x, y, false, false, 1.0f, 1.0f, 0.0f);
							x += spacing + frame.Width;
						}
					}
				}
				break;

			case ALIGN_CENTER:
				--endFrame;
				if (charOffsetsX && charOffsetsY
					&& charOffsetsX->Values->size() >= (endFrame - startFrame)
					&& charOffsetsY->Values->size() >= (endFrame - startFrame)) {
					charOffsetIndex = endFrame;
					for (; endFrame >= startFrame; --endFrame) {
						VMValue val = (*string->Values)[endFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							VMValue xVal = (*charOffsetsX->Values)[charOffsetIndex];
							float xOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(xVal, threadID)) {
								xOffset = AS_DECIMAL(xVal);
							}
							VMValue yVal = (*charOffsetsY->Values)[charOffsetIndex];
							float yOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(yVal, threadID)) {
								yOffset = AS_DECIMAL(yVal);
							}
							Graphics::DrawSprite(sprite, animation, curChar, x - (frame.Width / 2) + xOffset, y + yOffset, false, false, 1.0f, 1.0f, 0.0f);
							x = (x - frame.Width) - spacing;
							--charOffsetIndex;
						}
					}
				}
				else {
					for (; endFrame >= startFrame; --endFrame) {
						VMValue val = (*string->Values)[endFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							Graphics::DrawSprite(sprite, animation, curChar, x - frame.Width / 2, y, false, false, 1.0f, 1.0f, 0.0f);
							x = (x - frame.Width) - spacing;
						}
					}
				}
				break;

			case ALIGN_RIGHT:
				int totalWidth = 0;
				for (int pos = startFrame; pos < endFrame; ++pos) {
					VMValue val = (*string->Values)[pos];
					int curChar = 0;
					if (ScriptManager::DoIntegerConversion(val, threadID)) {
						curChar = AS_INTEGER(val);
					}
					if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
						totalWidth += sprite->Animations[animation].Frames[curChar].Width + spacing;
					}
				}
				x -= totalWidth;

				if (charOffsetsX && charOffsetsY
					&& charOffsetsX->Values->size() >= (endFrame - startFrame)
					&& charOffsetsY->Values->size() >= (endFrame - startFrame)) {
					for (; startFrame < endFrame; ++startFrame) {
						VMValue val = (*string->Values)[startFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							VMValue xVal = (*charOffsetsX->Values)[charOffsetIndex];
							float xOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(xVal, threadID)) {
								xOffset = AS_DECIMAL(xVal);
							}
							VMValue yVal = (*charOffsetsY->Values)[charOffsetIndex];
							float yOffset = 0.0f;
							if (ScriptManager::DoDecimalConversion(yVal, threadID)) {
								yOffset = AS_DECIMAL(yVal);
							}
							Graphics::DrawSprite(sprite, animation, curChar, x + xOffset, y + yOffset, false, false, 1.0f, 1.0f, 0.0f);
							x += spacing + frame.Width;
							++charOffsetIndex;
						}
					}
				}
				else {
					for (; startFrame < endFrame; ++startFrame) {
						VMValue val = (*string->Values)[startFrame];
						int curChar = 0;
						if (ScriptManager::DoIntegerConversion(val, threadID)) {
							curChar = AS_INTEGER(val);
						}
						if (curChar >= 0 && curChar < sprite->Animations[animation].FrameCount) {
							AnimFrame frame = sprite->Animations[animation].Frames[curChar];
							Graphics::DrawSprite(sprite, animation, curChar, x, y, false, false, 1.0f, 1.0f, 0.0f);
							x += spacing + frame.Width;
						}
					}
				}
				break;
			}
		}

		ScriptManager::Unlock();
		return NULL_VAL;
	}
	return NULL_VAL;
}
/***
 * Draw.SetBlendColor
 * \desc Sets the color to be used for drawing and blending.
 * \param hex (integer): Hexadecimal format of desired color. (ex: Red = 0xFF0000, Green = 0x00FF00, Blue = 0x0000FF)
 * \param alpha (number): Opacity to use for drawing, 0.0 to 1.0.
 * \ns Draw
 */
VMValue Draw_SetBlendColor(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount <= 2) {
		CHECK_ARGCOUNT(2);
		int hex = GET_ARG(0, GetInteger);
		float alpha = GET_ARG(1, GetDecimal);
		Graphics::SetBlendColor((hex >> 16 & 0xFF) / 255.f,
			(hex >> 8 & 0xFF) / 255.f,
			(hex & 0xFF) / 255.f,
			alpha);
		return NULL_VAL;
	}
	CHECK_ARGCOUNT(4);
	Graphics::SetBlendColor(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.SetTextureBlend
 * \desc Sets whether to use color and alpha blending on sprites, images, and textures.
 * \param doBlend (boolean): Whether to use blending.
 * \ns Draw
 */
VMValue Draw_SetTextureBlend(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Graphics::TextureBlend = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Draw.SetBlendMode
 * \desc Sets the blend mode used for drawing.
 * \param blendMode (<ref BlendMode_*>): The desired blend mode.
 * \return
 * \ns Draw
 */
VMValue Draw_SetBlendMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int blendMode = GET_ARG(0, GetInteger);
	if (blendMode < 0 || blendMode > BlendMode_MATCH_NOT_EQUAL) {
		OUT_OF_RANGE_ERROR("Blend mode", blendMode, 0, BlendMode_MATCH_NOT_EQUAL);
		return NULL_VAL;
	}
	Graphics::SetBlendMode(blendMode);
	return NULL_VAL;
}
/***
 * Draw.SetBlendFactor
 * \desc Sets the blend factors used for drawing. (Only for hardware-rendering)
 * \param sourceFactor (<ref BlendFactor_*>): Source factor for blending.
 * \param destinationFactor (<ref BlendFactor_*>): Destination facto for blending.
 * \return
 * \ns Draw
 */
VMValue Draw_SetBlendFactor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int src = GET_ARG(0, GetInteger);
	int dst = GET_ARG(1, GetInteger);
	Graphics::SetBlendMode(src, dst, src, dst);
	return NULL_VAL;
}
/***
 * Draw.SetBlendFactorExtended
 * \desc Sets all the blend factors used for drawing. (Only for hardware-rendering)
 * \param sourceColorFactor (<ref BlendFactor_*>): Source factor for blending color.
 * \param destinationColorFactor (<ref BlendFactor_*>): Destination factor for blending color.
 * \param sourceAlphaFactor (<ref BlendFactor_*>): Source factor for blending alpha.
 * \param destinationAlphaFactor (<ref BlendFactor_*>): Destination factor for blending alpha.
 * \return
 * \ns Draw
 */
VMValue Draw_SetBlendFactorExtended(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int srcC = GET_ARG(0, GetInteger);
	int dstC = GET_ARG(1, GetInteger);
	int srcA = GET_ARG(2, GetInteger);
	int dstA = GET_ARG(3, GetInteger);
	Graphics::SetBlendMode(srcC, dstC, srcA, dstA);
	return NULL_VAL;
}
/***
 * Draw.SetCompareColor
 * \desc Sets the Comparison Color to draw over for Comparison Drawing.
 * \param hex (integer): Hexadecimal format of desired color. (ex: Red = 0xFF0000, Green = 0x00FF00, Blue = 0x0000FF)
 * \ns Draw
 */
VMValue Draw_SetCompareColor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int hex = GET_ARG(0, GetInteger);
	// SoftwareRenderer::CompareColor = 0xFF000000U | (hex & 0xF8F8F8);
	SoftwareRenderer::CompareColor = 0xFF000000U | (hex & 0xFFFFFF);
	return NULL_VAL;
}
/***
 * Draw.SetTintColor
 * \desc Sets the color to be used for tinting.
 * \param hex (integer): Hexadecimal format of desired color. (ex: Red = 0xFF0000, Green = 0x00FF00, Blue = 0x0000FF)
 * \param amount (number): Tint amount, from 0.0 to 1.0.
 * \ns Draw
 */
VMValue Draw_SetTintColor(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount <= 2) {
		CHECK_ARGCOUNT(2);
		int hex = GET_ARG(0, GetInteger);
		float alpha = GET_ARG(1, GetDecimal);
		Graphics::SetTintColor((hex >> 16 & 0xFF) / 255.f,
			(hex >> 8 & 0xFF) / 255.f,
			(hex & 0xFF) / 255.f,
			alpha);
		return NULL_VAL;
	}
	CHECK_ARGCOUNT(4);
	Graphics::SetTintColor(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.SetTintMode
 * \desc Sets the tint mode used for drawing.
 * \param tintMode (<ref TintMode_*>): The desired tint mode.
 * \return
 * \ns Draw
 */
VMValue Draw_SetTintMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tintMode = GET_ARG(0, GetInteger);
	if (tintMode < 0 || tintMode > TintMode_DST_BLEND) {
		OUT_OF_RANGE_ERROR("Tint mode", tintMode, 0, TintMode_DST_BLEND);
		return NULL_VAL;
	}
	Graphics::SetTintMode(tintMode);
	return NULL_VAL;
}
/***
 * Draw.UseTinting
 * \desc Sets whether to use color tinting when drawing.
 * \param useTinting (boolean): Whether to use color tinting when drawing.
 * \ns Draw
 */
VMValue Draw_UseTinting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int useTinting = GET_ARG(0, GetInteger);
	Graphics::SetTintEnabled(useTinting);
	return NULL_VAL;
}
/***
 * Draw.SetShader
 * \desc Sets the current shader.
 * \param shader (Shader): The shader, or `null` to unset the shader.
 * \ns Draw
 */
VMValue Draw_SetShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	if (IS_NULL(args[0])) {
		Graphics::SetUserShader(nullptr);
		return NULL_VAL;
	}

	ObjShader* objShader = GET_ARG(0, GetShader);
	Shader* shader = (Shader*)objShader->ShaderPtr;
	if (shader == nullptr) {
		THROW_ERROR("Shader has been deleted!");
		return NULL_VAL;
	}

	try {
		shader->Validate();

		Graphics::SetUserShader(shader);
	} catch (const std::runtime_error& error) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false, "%s", error.what());
	}

	return NULL_VAL;
}
/***
 * Draw.SetFilter
 * \desc Sets the current filter type.
 * \param filterType (<ref Filter_*>): The filter type.
 * \ns Draw
 */
VMValue Draw_SetFilter(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int filterType = GET_ARG(0, GetInteger);
	if (filterType < 0 || filterType > Filter_INVERT) {
		OUT_OF_RANGE_ERROR("Filter", filterType, 0, Filter_INVERT);
		return NULL_VAL;
	}
	Graphics::SetFilter(filterType);
	return NULL_VAL;
}
/***
 * Draw.UseStencil
 * \desc Enables or disables stencil operations.
 * \param enabled (boolean): Whether to enable or disable stencil operations.
 * \ns Draw
 */
VMValue Draw_UseStencil(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Graphics::SetStencilEnabled(!!GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.SetStencilTestFunction
 * \desc Sets the current stencil test function.
 * \param stencilTest (<ref StencilTest_*>): One of the stencil test functions.
 * \ns Draw
 */
VMValue Draw_SetStencilTestFunction(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int stencilTest = GET_ARG(0, GetInteger);
	if (stencilTest < StencilTest_Never || stencilTest > StencilTest_GEqual) {
		OUT_OF_RANGE_ERROR("Stencil test function",
			stencilTest,
			StencilTest_Never,
			StencilTest_GEqual);
		return NULL_VAL;
	}
	Graphics::SetStencilTestFunc(stencilTest);
	return NULL_VAL;
}
/***
 * Draw.SetStencilPassOperation
 * \desc Sets the stencil operation for when the stencil test passes.
 * \param stencilOp (<ref StencilTest_*>): One of the stencil operation.
 * \ns Draw
 */
VMValue Draw_SetStencilPassOperation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int stencilOp = GET_ARG(0, GetInteger);
	if (stencilOp < StencilOp_Keep || stencilOp > StencilOp_DecrWrap) {
		OUT_OF_RANGE_ERROR(
			"Stencil operation", stencilOp, StencilOp_Keep, StencilOp_DecrWrap);
		return NULL_VAL;
	}
	Graphics::SetStencilPassFunc(stencilOp);
	return NULL_VAL;
}
/***
 * Draw.SetStencilFailOperation
 * \desc Sets the stencil operation for when the stencil test fails.
 * \param stencilOp (<ref StencilTest_*>): One of the stencil operations.
 * \ns Draw
 */
VMValue Draw_SetStencilFailOperation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int stencilOp = GET_ARG(0, GetInteger);
	if (stencilOp < StencilOp_Keep || stencilOp > StencilOp_DecrWrap) {
		OUT_OF_RANGE_ERROR(
			"Stencil operation", stencilOp, StencilOp_Keep, StencilOp_DecrWrap);
		return NULL_VAL;
	}
	Graphics::SetStencilFailFunc(stencilOp);
	return NULL_VAL;
}
/***
 * Draw.SetStencilValue
 * \desc Sets the stencil value.
 * \param value (integer): The stencil value. This value is clamped by the stencil buffer's bit depth.
 * \ns Draw
 */
VMValue Draw_SetStencilValue(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int value = GET_ARG(0, GetInteger);
	Graphics::SetStencilValue(value);
	return NULL_VAL;
}
/***
 * Draw.SetStencilMask
 * \desc Sets the mask used for all stencil tests.
 * \param mask (integer): The stencil mask. This value is clamped by the stencil buffer's bit depth.
 * \ns Draw
 */
VMValue Draw_SetStencilMask(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int mask = GET_ARG(0, GetInteger);
	Graphics::SetStencilMask(mask);
	return NULL_VAL;
}
/***
 * Draw.ClearStencil
 * \desc Clears the stencil.
 * \ns Draw
 */
VMValue Draw_ClearStencil(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::ClearStencil();
	return NULL_VAL;
}
/***
 * Draw.SetDotMask
 * \desc Sets the dot mask.
 * \param mask (integer): The mask.
 * \ns Draw
 */
VMValue Draw_SetDotMask(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	SoftwareRenderer::SetDotMask(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.SetHorizontalDotMask
 * \desc Sets the horizontal dot mask.
 * \param mask (integer): The mask.
 * \ns Draw
 */
VMValue Draw_SetHorizontalDotMask(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	SoftwareRenderer::SetDotMaskH(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.SetVerticalDotMask
 * \desc Sets the vertical dot mask.
 * \param mask (integer): The mask.
 * \ns Draw
 */
VMValue Draw_SetVerticalDotMask(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	SoftwareRenderer::SetDotMaskV(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.SetHorizontalDotMaskOffset
 * \desc Sets the offset of the horizontal dot mask.
 * \param offsetH (integer): The offset.
 * \ns Draw
 */
VMValue Draw_SetHorizontalDotMaskOffset(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	SoftwareRenderer::SetDotMaskOffsetH(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.SetVerticalDotMaskOffset
 * \desc Sets the offset of the vertical dot mask.
 * \param offsetV (integer): The offset.
 * \ns Draw
 */
VMValue Draw_SetVerticalDotMaskOffset(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	SoftwareRenderer::SetDotMaskOffsetV(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.Line
 * \desc Draws a line.
 * \param x1 (number): X position of where to start drawing the line.
 * \param y1 (number): Y position of where to start drawing the line.
 * \param x2 (number): X position of where to end drawing the line.
 * \param y2 (number): Y position of where to end drawing the line.
 * \ns Draw
 */
VMValue Draw_Line(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Graphics::StrokeLine(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.Circle
 * \desc Draws a circle.
 * \param x (number): Center X position of where to draw the circle.
 * \param y (number): Center Y position of where to draw the circle.
 * \param radius (number): Radius of the circle.
 * \ns Draw
 */
VMValue Draw_Circle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	Graphics::FillCircle(
		GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.Ellipse
 * \desc Draws an ellipse.
 * \param x (number): X position of where to draw the ellipse.
 * \param y (number): Y position of where to draw the ellipse.
 * \param width (number): Width to draw the ellipse at.
 * \param height (number): Height to draw the ellipse at.
 * \ns Draw
 */
VMValue Draw_Ellipse(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Graphics::FillEllipse(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.Triangle
 * \desc Draws a triangle.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \ns Draw
 */
VMValue Draw_Triangle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(6);
	Graphics::FillTriangle(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal),
		GET_ARG(4, GetDecimal),
		GET_ARG(5, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.TriangleBlend
 * \desc Draws a triangle, blending the colors at the vertices. (Colors are multiplied by the global Draw Blend Color, do <ref Draw.SetBlendColor>`(0xFFFFFF, 1.0)` if you want the vertex colors unaffected.)
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param color1 (integer): Color of the first vertex.
 * \param color2 (integer): Color of the second vertex.
 * \param color3 (integer): Color of the third vertex.
 * \ns Draw
 */
VMValue Draw_TriangleBlend(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(9);
	Graphics::FillTriangleBlend(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal),
		GET_ARG(4, GetDecimal),
		GET_ARG(5, GetDecimal),
		GET_ARG(6, GetInteger),
		GET_ARG(7, GetInteger),
		GET_ARG(8, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.Quad
 * \desc Draws a quad.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \ns Draw
 */
VMValue Draw_Quad(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(8);
	Graphics::FillQuad(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal),
		GET_ARG(4, GetDecimal),
		GET_ARG(5, GetDecimal),
		GET_ARG(6, GetDecimal),
		GET_ARG(7, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.QuadBlend
 * \desc Draws a quad, blending the colors at the vertices. (Colors are multiplied by the global Draw Blend Color, do <ref Draw.SetBlendColor>`(0xFFFFFF, 1.0)` if you want the vertex colors unaffected.)
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \param color1 (integer): Color of the first vertex.
 * \param color2 (integer): Color of the second vertex.
 * \param color3 (integer): Color of the third vertex.
 * \param color4 (integer): Color of the fourth vertex.
 * \ns Draw
 */
VMValue Draw_QuadBlend(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(12);
	Graphics::FillQuadBlend(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal),
		GET_ARG(4, GetDecimal),
		GET_ARG(5, GetDecimal),
		GET_ARG(6, GetDecimal),
		GET_ARG(7, GetDecimal),
		GET_ARG(8, GetInteger),
		GET_ARG(9, GetInteger),
		GET_ARG(10, GetInteger),
		GET_ARG(11, GetInteger));
	return NULL_VAL;
}
/***
 * Draw.TriangleTextured
 * \desc Draws a textured triangle.
 * \param image (integer): Image to draw triangle with.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt u1 (number): Texture U of the first vertex.
 * \paramOpt v1 (number): Texture V of the first vertex.
 * \paramOpt u2 (number): Texture U of the second vertex.
 * \paramOpt v2 (number): Texture V of the second vertex.
 * \paramOpt u3 (number): Texture U of the third vertex.
 * \paramOpt v3 (number): Texture V of the third vertex.
 * \ns Draw
 */
VMValue Draw_TriangleTextured(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(7);

	Image* image = GET_ARG(0, GetImage);
	if (image) {
		Graphics::DrawTriangleTextured(image->TexturePtr,
			GET_ARG(1, GetDecimal),
			GET_ARG(2, GetDecimal),
			GET_ARG(3, GetDecimal),
			GET_ARG(4, GetDecimal),
			GET_ARG(5, GetDecimal),
			GET_ARG(6, GetDecimal),
			GET_ARG_OPT(7, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(8, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(9, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(10, GetDecimal, 0.0),
			GET_ARG_OPT(11, GetDecimal, 0.0),
			GET_ARG_OPT(12, GetDecimal, 1.0),
			GET_ARG_OPT(13, GetDecimal, 0.0),
			GET_ARG_OPT(14, GetDecimal, 1.0),
			GET_ARG_OPT(15, GetDecimal, 1.0));
	}

	return NULL_VAL;
}
/***
 * Draw.QuadTextured
 * \desc Draws a textured quad.
 * \param image (integer): Image to draw quad with.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt color4 (integer): Color of the fourth vertex.
 * \paramOpt u1 (number): Texture U of the first vertex.
 * \paramOpt v1 (number): Texture V of the first vertex.
 * \paramOpt u2 (number): Texture U of the second vertex.
 * \paramOpt v2 (number): Texture V of the second vertex.
 * \paramOpt u3 (number): Texture U of the third vertex.
 * \paramOpt v3 (number): Texture V of the third vertex.
 * \paramOpt u4 (number): Texture U of the fourth vertex.
 * \paramOpt v4 (number): Texture V of the fourth vertex.
 * \ns Draw
 */
VMValue Draw_QuadTextured(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(9);

	Image* image = GET_ARG(0, GetImage);
	if (image) {
		Graphics::DrawQuadTextured(image->TexturePtr,
			GET_ARG(1, GetDecimal),
			GET_ARG(2, GetDecimal),
			GET_ARG(3, GetDecimal),
			GET_ARG(4, GetDecimal),
			GET_ARG(5, GetDecimal),
			GET_ARG(6, GetDecimal),
			GET_ARG(7, GetDecimal),
			GET_ARG(8, GetDecimal),
			GET_ARG_OPT(9, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(10, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(11, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(12, GetInteger, 0xFFFFFF),
			GET_ARG_OPT(13, GetDecimal, 0.0),
			GET_ARG_OPT(14, GetDecimal, 0.0),
			GET_ARG_OPT(15, GetDecimal, 1.0),
			GET_ARG_OPT(16, GetDecimal, 0.0),
			GET_ARG_OPT(17, GetDecimal, 1.0),
			GET_ARG_OPT(18, GetDecimal, 1.0),
			GET_ARG_OPT(19, GetDecimal, 0.0),
			GET_ARG_OPT(20, GetDecimal, 1.0));
	}

	return NULL_VAL;
}
/***
 * Draw.Rectangle
 * \desc Draws a rectangle.
 * \param x (number): X position of where to draw the rectangle.
 * \param y (number): Y position of where to draw the rectangle.
 * \param width (number): Width to draw the rectangle at.
 * \param height (number): Height to draw the rectangle at.
 * \ns Draw
 */
VMValue Draw_Rectangle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Graphics::FillRectangle(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.CircleStroke
 * \desc Draws a circle outline.
 * \param x (number): Center X position of where to draw the circle.
 * \param y (number): Center Y position of where to draw the circle.
 * \param radius (number): Radius of the circle.
 * \paramOpt thickness (number): Thickness of the circle.
 * \ns Draw
 */
VMValue Draw_CircleStroke(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);
	Graphics::StrokeCircle(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG_OPT(3, GetDecimal, 1.0));
	return NULL_VAL;
}
/***
 * Draw.EllipseStroke
 * \desc Draws an ellipse outline.
 * \param x (number): X position of where to draw the ellipse.
 * \param y (number): Y position of where to draw the ellipse.
 * \param width (number): Width to draw the ellipse at.
 * \param height (number): Height to draw the ellipse at.
 * \ns Draw
 */
VMValue Draw_EllipseStroke(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Graphics::StrokeEllipse(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.TriangleStroke
 * \desc Draws a triangle outline.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \ns Draw
 */
VMValue Draw_TriangleStroke(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(6);
	Graphics::StrokeTriangle(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal),
		GET_ARG(4, GetDecimal),
		GET_ARG(5, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.RectangleStroke
 * \desc Draws a rectangle outline.
 * \param x (number): X position of where to draw the rectangle.
 * \param y (number): Y position of where to draw the rectangle.
 * \param width (number): Width to draw the rectangle at.
 * \param height (number): Height to draw the rectangle at.
 * \ns Draw
 */
VMValue Draw_RectangleStroke(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Graphics::StrokeRectangle(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal));
	return NULL_VAL;
}
/***
 * Draw.UseFillSmoothing
 * \desc Sets whether to use smoothing when drawing filled shapes. (hardware-renderer only)
 * \param smoothFill (boolean): Whether to use smoothing.
 * \return
 * \ns Draw
 */
VMValue Draw_UseFillSmoothing(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Graphics::SmoothFill = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Draw.UseStrokeSmoothing
 * \desc Sets whether to use smoothing when drawing un-filled shapes. (hardware-renderer only)
 * \param smoothStroke (boolean): Whether to use smoothing.
 * \ns Draw
 */
VMValue Draw_UseStrokeSmoothing(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Graphics::SmoothStroke = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}

/***
 * Draw.SetClip
 * \desc Sets the region in which drawing will occur.
 * \param x (number): X position of where to start the draw region.
 * \param y (number): Y position of where to start the draw region.
 * \param width (number): Width of the draw region.
 * \param height (number): Height of the draw region.
 * \ns Draw
 */
VMValue Draw_SetClip(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	if (GET_ARG(2, GetDecimal) >= 0.0 && GET_ARG(3, GetDecimal) >= 0.0) {
		Graphics::SetClip((int)GET_ARG(0, GetDecimal),
			(int)GET_ARG(1, GetDecimal),
			(int)GET_ARG(2, GetDecimal),
			(int)GET_ARG(3, GetDecimal));
	}
	return NULL_VAL;
}
/***
 * Draw.ClearClip
 * \desc Resets the drawing region.
 * \ns Draw
 */
VMValue Draw_ClearClip(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::ClearClip();
	return NULL_VAL;
}
/***
  * Draw.GetClipX
  * \desc Gets the X position in which drawing starts to occur.
  * \return integer The X position if clipping is enabled, else 0.
  * \ns Draw
  */
VMValue Draw_GetClipX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return Graphics::CurrentClip.Enabled ? INTEGER_VAL((int)Graphics::CurrentClip.X)
					     : INTEGER_VAL(0);
}
/***
 * Draw.GetClipY
 * \desc Gets the Y position in which drawing starts to occur.
 * \return integer The Y position if clipping is enabled, else 0.
 * \ns Draw
 */
VMValue Draw_GetClipY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return Graphics::CurrentClip.Enabled ? INTEGER_VAL((int)Graphics::CurrentClip.Y)
					     : INTEGER_VAL(0);
}
/***
 * Draw.GetClipWidth
 * \desc Gets the width in which drawing occurs.
 * \return integer The width if clipping is enabled, else 0.
 * \ns Draw
 */
VMValue Draw_GetClipWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return Graphics::CurrentClip.Enabled ? INTEGER_VAL((int)Graphics::CurrentClip.Width)
					     : INTEGER_VAL((int)Graphics::CurrentView->Width);
}
/***
 * Draw.GetClipHeight
 * \desc Gets the height in which drawing occurs.
 * \return integer The height if clipping is enabled, else 0.
 * \ns Draw
 */
VMValue Draw_GetClipHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return Graphics::CurrentClip.Enabled ? INTEGER_VAL((int)Graphics::CurrentClip.Height)
					     : INTEGER_VAL((int)Graphics::CurrentView->Height);
}
/***
 * Draw.Save
 * \desc Saves the current transform matrix to the stack.
 * \ns Draw
 */
VMValue Draw_Save(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::Save();
	return NULL_VAL;
}
/***
 * Draw.Scale
 * \desc Scales the view.
 * \param x (number): Desired X scale.
 * \param y (number): Desired Y scale.
 * \paramOpt z (number): Desired Z scale. (default: `1.0`)
 * \ns Draw
 */
VMValue Draw_Scale(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	float x = GET_ARG(0, GetDecimal);
	float y = GET_ARG(1, GetDecimal);
	float z = 1.0f;
	if (argCount == 3) {
		z = GET_ARG(2, GetDecimal);
	}
	Graphics::Scale(x, y, z);
	return NULL_VAL;
}
/***
 * Draw.Rotate
 * \desc Rotates the view.
 * \param x (number): Desired X angle.
 * \param y (number): Desired Y angle.
 * \param z (number): Desired Z angle.
 * \ns Draw
 */
/***
 * Draw.Rotate
 * \desc Rotates the view.
 * \param angle (number): Desired rotation angle.
 * \ns Draw
 */
VMValue Draw_Rotate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	if (argCount == 3) {
		float x = GET_ARG(0, GetDecimal);
		float y = GET_ARG(1, GetDecimal);
		float z = GET_ARG(2, GetDecimal);
		Graphics::Rotate(x, y, z);
	}
	else {
		float z = GET_ARG(0, GetDecimal);
		Graphics::Rotate(0.0f, 0.0f, z);
	}
	return NULL_VAL;
}
/***
 * Draw.Restore
 * \desc Restores the last saved transform matrix from the stack.
 * \ns Draw
 */
VMValue Draw_Restore(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::Restore();
	return NULL_VAL;
}
/***
 * Draw.Translate
 * \desc Translates the view.
 * \param x (number): Desired X translation.
 * \param y (number): Desired Y translation.
 * \paramOpt z (number): Desired Z translation. (default: `0.0`)
 * \ns Draw
 */
VMValue Draw_Translate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	float x = GET_ARG(0, GetDecimal);
	float y = GET_ARG(1, GetDecimal);
	float z = 0.0f;
	if (argCount == 3) {
		z = GET_ARG(2, GetDecimal);
	}
	Graphics::Translate(x, y, z);
	return NULL_VAL;
}
/***
 * Draw.SetTextureTarget
 * \desc Sets the current render target.
 * \param texture (integer): The texture target.
 * \ns Draw
 */
VMValue Draw_SetTextureTarget(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	GameTexture* gameTexture = GET_ARG(0, GetTexture);
	if (!gameTexture) {
		return NULL_VAL;
	}

	Texture* texture = gameTexture->GetTexture();
	if (texture) {
		Graphics::SetRenderTarget(texture);
	}
	return NULL_VAL;
}
/***
 * Draw.Clear
 * \desc Clears the screen.
 * \ns Draw
 */
VMValue Draw_Clear(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::Clear();
	return NULL_VAL;
}
/***
 * Draw.ResetTextureTarget
 * \desc Resets the current render target.
 * \ns Draw
 */
VMValue Draw_ResetTextureTarget(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (Graphics::CurrentView) {
		Graphics::SetRenderTarget(!Graphics::CurrentView->UseDrawTarget
				? NULL
				: Graphics::CurrentView->DrawTarget);
		Graphics::UpdateProjectionMatrix();
	}
	return NULL_VAL;
}
/***
 * Draw.UseSpriteDeform
 * \desc Sets whether to use sprite deform when drawing.
 * \param useDeform (boolean): Whether to use sprite deform when drawing.
 * \ns Draw
 */
VMValue Draw_UseSpriteDeform(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int useDeform = GET_ARG(0, GetInteger);
	SoftwareRenderer::UseSpriteDeform = useDeform;
	return NULL_VAL;
}
/***
 * Draw.SetSpriteDeformLine
 * \desc Sets the sprite deform line at the specified line index.
 * \param deformIndex (integer): Index of deform line. (0 = top of screen, 1 = the line below it, 2 = etc.)
 * \param deformValue (decimal): Deform value.
 * \ns Draw
 */
VMValue Draw_SetSpriteDeformLine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int lineIndex = GET_ARG(0, GetInteger);
	int deformValue = (int)GET_ARG(1, GetDecimal);

	SoftwareRenderer::SpriteDeformBuffer[lineIndex] = deformValue;
	return NULL_VAL;
}
/***
 * Draw.UseDepthTesting
 * \desc Sets whether to do depth tests when drawing.
 * \param useDepthTesting (boolean): Whether to do depth tests when drawing.
 * \ns Draw
 */
VMValue Draw_UseDepthTesting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int useDepthTesting = GET_ARG(0, GetInteger);
	Graphics::UseDepthTesting = useDepthTesting;
	Graphics::SetDepthTesting(useDepthTesting);
	return NULL_VAL;
}
/***
 * Draw.GetCurrentDrawGroup
 * \desc Gets the draw group currently being drawn.
 * \return integer Returns an integer value.
 * \ns Draw
 */
VMValue Draw_GetCurrentDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::CurrentDrawGroup);
}
/***
 * Draw.CopyScreen
 * \desc Copies the contents of the screen into a texture.
 * \param texture (integer): Texture index.
 * \ns Draw
 */
VMValue Draw_CopyScreen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	GameTexture* gameTexture = GET_ARG(0, GetTexture);
	if (!gameTexture) {
		return NULL_VAL;
	}

	Texture* texture = gameTexture->GetTexture();
	if (texture) {
		int width = Graphics::CurrentViewport.Width;
		int height = Graphics::CurrentViewport.Height;

		View* currentView = Graphics::CurrentView;
		if (currentView) {
			// If we are using a draw target, then we can't reliably use the viewport's dimensions,
			// because it might not match the view's size
			if (currentView->UseDrawTarget && currentView->DrawTarget) {
				width = Graphics::CurrentView->Width;
				height = Graphics::CurrentView->Height;
			}
		}

		Graphics::CopyScreen(
			// source
			0,
			0,
			width,
			height,
			// dest
			0,
			0,
			texture->Width,
			texture->Height,
			texture);
	}
	return NULL_VAL;
}
// #endregion

// #region Draw3D
/***
 * Draw3D.BindVertexBuffer
 * \desc Binds a vertex buffer.
 * \param vertexBufferIndex (integer): Sets the vertex buffer to bind.
 * \ns Draw3D
 */
VMValue Draw3D_BindVertexBuffer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 vertexBufferIndex = GET_ARG(0, GetInteger);
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		OUT_OF_RANGE_ERROR("Vertex index", vertexBufferIndex, 0, MAX_VERTEX_BUFFERS);
		return NULL_VAL;
	}

	Graphics::BindVertexBuffer(vertexBufferIndex);
	return NULL_VAL;
}
/***
 * Draw3D.UnbindVertexBuffer
 * \desc Unbinds the currently bound vertex buffer.
 * \ns Draw3D
 */
VMValue Draw3D_UnbindVertexBuffer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Graphics::UnbindVertexBuffer();
	return NULL_VAL;
}
#define GET_SCENE_3D() \
	if (scene3DIndex < 0 || scene3DIndex >= MAX_3D_SCENES) { \
		OUT_OF_RANGE_ERROR("Scene3D", scene3DIndex, 0, MAX_3D_SCENES - 1); \
		return NULL_VAL; \
	} \
	Scene3D* scene3D = &Graphics::Scene3Ds[scene3DIndex]
/***
 * Draw3D.BindScene
 * \desc Binds a 3D scene for drawing polygons in 3D space.
 * \param scene3DIndex (integer): Sets the 3D scene to bind.
 * \ns Draw3D
 */
VMValue Draw3D_BindScene(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	if (scene3DIndex < 0 || scene3DIndex >= MAX_3D_SCENES) {
		OUT_OF_RANGE_ERROR("Scene3D", scene3DIndex, 0, MAX_3D_SCENES - 1);
		return NULL_VAL;
	}

	Graphics::BindScene3D(scene3DIndex);
	return NULL_VAL;
}
static void PrepareMatrix(Matrix4x4* output, ObjArray* input) {
	MatrixHelper helper;
	MatrixHelper_CopyFrom(&helper, input);

	for (int i = 0; i < 16; i++) {
		int x = i >> 2;
		int y = i & 3;
		output->Values[i] = helper[x][y];
	}
}
/***
 * Draw3D.Model
 * \desc Draws a model.
 * \param modelIndex (integer): Index of loaded model.
 * \param animation (integer): Animation of model to draw.
 * \param frame (decimal): Frame of model to draw.
 * \paramOpt matrixModel (matrix): Matrix for transforming model coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming model normals.
 * \ns Draw3D
 */
VMValue Draw3D_Model(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);

	IModel* model = GET_ARG(0, GetModel);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetDecimal) * 0x100;

	ObjArray* matrixModelArr = NULL;
	Matrix4x4 matrixModel;
	if (argCount >= 4) {
		if (!IS_NULL(args[3])) {
			matrixModelArr = GET_ARG(3, GetArray);
			PrepareMatrix(&matrixModel, matrixModelArr);
		}
	}

	ObjArray* matrixNormalArr = NULL;
	Matrix4x4 matrixNormal;
	if (argCount >= 5) {
		if (!IS_NULL(args[4])) {
			matrixNormalArr = GET_ARG(4, GetArray);
			PrepareMatrix(&matrixNormal, matrixNormalArr);
		}
	}

	if (model) {
		Graphics::DrawModel(model,
			animation,
			frame,
			matrixModelArr ? &matrixModel : NULL,
			matrixNormalArr ? &matrixNormal : NULL);
	}

	return NULL_VAL;
}
/***
 * Draw3D.ModelSkinned
 * \desc Draws a skinned model.
 * \param modelIndex (integer): Index of loaded model.
 * \param armatureIndex (integer): Armature index to skin the model.
 * \paramOpt matrixModel (matrix): Matrix for transforming model coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming model normals.
 * \ns Draw3D
 */
VMValue Draw3D_ModelSkinned(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);

	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);

	ObjArray* matrixModelArr = NULL;
	Matrix4x4 matrixModel;
	if (argCount >= 3) {
		if (!IS_NULL(args[2])) {
			matrixModelArr = GET_ARG(2, GetArray);
			PrepareMatrix(&matrixModel, matrixModelArr);
		}
	}

	ObjArray* matrixNormalArr = NULL;
	Matrix4x4 matrixNormal;
	if (argCount >= 4) {
		if (!IS_NULL(args[3])) {
			matrixNormalArr = GET_ARG(3, GetArray);
			PrepareMatrix(&matrixNormal, matrixNormalArr);
		}
	}

	if (model) {
		Graphics::DrawModelSkinned(model,
			armature,
			matrixModelArr ? &matrixModel : NULL,
			matrixNormalArr ? &matrixNormal : NULL);
	}

	return NULL_VAL;
}
/***
 * Draw3D.ModelSimple
 * \desc Draws a model without using matrices.
 * \param modelIndex (integer): Index of loaded model.
 * \param animation (integer): Animation of model to draw.
 * \param frame (integer): Frame of model to draw.
 * \param x (number): X position
 * \param y (number): Y position
 * \param scale (number): Model scale
 * \param rx (number): X rotation in radians
 * \param ry (number): Y rotation in radians
 * \param rz (number): Z rotation in radians
 * \ns Draw3D
 */
VMValue Draw3D_ModelSimple(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(9);

	IModel* model = GET_ARG(0, GetModel);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetDecimal) * 0x100;
	float x = GET_ARG(3, GetDecimal);
	float y = GET_ARG(4, GetDecimal);
	float scale = GET_ARG(5, GetDecimal);
	float rx = GET_ARG(6, GetDecimal);
	float ry = GET_ARG(7, GetDecimal);
	float rz = GET_ARG(8, GetDecimal);

	Matrix4x4 matrixScaleTranslate;
	Matrix4x4::IdentityScale(&matrixScaleTranslate, scale, scale, scale);
	Matrix4x4::Translate(&matrixScaleTranslate, &matrixScaleTranslate, x, y, 0);

	Matrix4x4 matrixModel;
	Matrix4x4::IdentityRotationXYZ(&matrixModel, 0, ry, rz);
	Matrix4x4::Multiply(&matrixModel, &matrixModel, &matrixScaleTranslate);

	Matrix4x4 matrixRotationX;
	Matrix4x4::IdentityRotationX(&matrixRotationX, rx);
	Matrix4x4 matrixNormal;
	Matrix4x4::IdentityRotationXYZ(&matrixNormal, 0, ry, rz);
	Matrix4x4::Multiply(&matrixNormal, &matrixNormal, &matrixRotationX);

	Graphics::DrawModel(model, animation, frame, &matrixModel, &matrixNormal);
	return NULL_VAL;
}

#define PREPARE_MATRICES(matrixModelArr, matrixNormalArr) \
	Matrix4x4* matrixModel = NULL; \
	Matrix4x4* matrixNormal = NULL; \
	Matrix4x4 sMatrixModel, sMatrixNormal; \
	if (matrixModelArr) { \
		matrixModel = &sMatrixModel; \
		PrepareMatrix(matrixModel, matrixModelArr); \
	} \
	if (matrixNormalArr) { \
		matrixNormal = &sMatrixNormal; \
		PrepareMatrix(matrixNormal, matrixNormalArr); \
	}

static void DrawPolygon3D(VertexAttribute* data,
	int vertexCount,
	int vertexFlag,
	Texture* texture,
	ObjArray* matrixModelArr,
	ObjArray* matrixNormalArr) {
	PREPARE_MATRICES(matrixModelArr, matrixNormalArr);
	Graphics::DrawPolygon3D(data, vertexCount, vertexFlag, texture, matrixModel, matrixNormal);
}

#define VERTEX_ARGS(num, offset) \
	int argOffset = offset; \
	for (int i = 0; i < num; i++) { \
		data[i].Position.X = FP16_TO(GET_ARG(i * 3 + argOffset, GetDecimal)); \
		data[i].Position.Y = FP16_TO(GET_ARG(i * 3 + argOffset + 1, GetDecimal)); \
		data[i].Position.Z = FP16_TO(GET_ARG(i * 3 + argOffset + 2, GetDecimal)); \
		data[i].Normal.X = data[i].Normal.Y = data[i].Normal.Z = data[i].Normal.W = 0; \
		data[i].UV.X = data[i].UV.Y = 0; \
	} \
	argOffset += 3 * num

#define VERTEX_COLOR_ARGS(num) \
	for (int i = 0; i < num; i++) { \
		if (argCount <= i + argOffset) \
			break; \
		if (!IS_NULL(args[i + argOffset])) \
			data[i].Color = GET_ARG(i + argOffset, GetInteger); \
		else \
			data[i].Color = 0xFFFFFF; \
	} \
	argOffset += num

#define VERTEX_UV_ARGS(num) \
	for (int i = 0; i < num; i++) { \
		if (argCount <= (i * 2) + argOffset) \
			break; \
		if (!IS_NULL(args[(i * 2) + argOffset])) \
			data[i].UV.X = FP16_TO(GET_ARG((i * 2) + argOffset, GetDecimal)); \
		if (!IS_NULL(args[(i * 2) + 1 + argOffset])) \
			data[i].UV.Y = FP16_TO(GET_ARG((i * 2) + 1 + argOffset, GetDecimal)); \
	} \
	argOffset += num * 2

#define GET_MATRICES(offset) \
	ObjArray* matrixModelArr = NULL; \
	if (argCount > offset && !IS_NULL(args[offset])) \
		matrixModelArr = GET_ARG(offset, GetArray); \
	ObjArray* matrixNormalArr = NULL; \
	if (argCount > offset + 1 && !IS_NULL(args[offset + 1])) \
	matrixNormalArr = GET_ARG(offset + 1, GetArray)

/***
 * Draw3D.Triangle
 * \desc Draws a triangle in 3D space.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_Triangle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(9);

	VertexAttribute data[3];

	VERTEX_ARGS(3, 0);
	VERTEX_COLOR_ARGS(3);
	GET_MATRICES(argOffset);

	DrawPolygon3D(data,
		3,
		VertexType_Position | VertexType_Color,
		NULL,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.Quad
 * \desc Draws a quadrilateral in 3D space.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \param z4 (number): Z position of the fourth vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt color4 (integer): Color of the fourth vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_Quad(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(12);

	VertexAttribute data[4];

	VERTEX_ARGS(4, 0);
	VERTEX_COLOR_ARGS(4);
	GET_MATRICES(argOffset);

	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_Color,
		NULL,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}

/***
 * Draw3D.Sprite
 * \desc Draws a sprite in 3D space.
 * \param sprite (integer): Index of the loaded sprite.
 * \param animation (integer): Index of the animation entry.
 * \param frame (integer): Index of the frame in the animation entry.
 * \param x (number): X position of where to draw the sprite.
 * \param y (number): Y position of where to draw the sprite.
 * \param z (number): Z position of where to draw the sprite.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \paramOpt scaleX (number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (number): Scale multiplier of the sprite vertically.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_Sprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(8);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	float x = GET_ARG(3, GetDecimal);
	float y = GET_ARG(4, GetDecimal);
	float z = GET_ARG(5, GetDecimal);
	int flipX = GET_ARG(6, GetInteger);
	int flipY = GET_ARG(7, GetInteger);
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	if (argCount > 8) {
		scaleX = GET_ARG(8, GetDecimal);
	}
	if (argCount > 9) {
		scaleY = GET_ARG(9, GetDecimal);
	}

	GET_MATRICES(10);

	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return NULL_VAL;
	}

	AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	VertexAttribute data[4];

	x += frameStr.OffsetX * scaleX;
	y -= frameStr.OffsetY * scaleY;

	Graphics::MakeSpritePolygon(data,
		x,
		y,
		z,
		flipX,
		flipY,
		scaleX,
		scaleY,
		texture,
		frameStr.X,
		frameStr.Y,
		frameStr.Width,
		frameStr.Height);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_UV,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.SpritePart
 * \desc Draws part of a sprite in 3D space.
 * \param sprite (integer): Index of the loaded sprite.
 * \param animation (integer): Index of the animation entry.
 * \param frame (integer): Index of the frame in the animation entry.
 * \param x (number): X position of where to draw the sprite.
 * \param y (number): Y position of where to draw the sprite.
 * \param z (number): Z position of where to draw the sprite.
 * \param partX (integer): X coordinate of part of frame to draw.
 * \param partY (integer): Y coordinate of part of frame to draw.
 * \param partW (integer): Width of part of frame to draw.
 * \param partH (integer): Height of part of frame to draw.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \paramOpt scaleX (number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (number): Scale multiplier of the sprite vertically.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_SpritePart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(12);

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	float x = GET_ARG(3, GetDecimal);
	float y = GET_ARG(4, GetDecimal);
	float z = GET_ARG(5, GetDecimal);
	int sx = (int)GET_ARG(6, GetDecimal);
	int sy = (int)GET_ARG(7, GetDecimal);
	int sw = (int)GET_ARG(8, GetDecimal);
	int sh = (int)GET_ARG(9, GetDecimal);
	int flipX = GET_ARG(10, GetInteger);
	int flipY = GET_ARG(11, GetInteger);
	float scaleX = 1.0f;
	float scaleY = 1.0f;
	if (argCount > 12) {
		scaleX = GET_ARG(12, GetDecimal);
	}
	if (argCount > 13) {
		scaleY = GET_ARG(13, GetDecimal);
	}

	GET_MATRICES(14);

	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return NULL_VAL;
	}

	if (sw < 1 || sh < 1) {
		return NULL_VAL;
	}

	AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	VertexAttribute data[4];

	x += frameStr.OffsetX * scaleX;
	y -= frameStr.OffsetY * scaleY;

	Graphics::MakeSpritePolygon(
		data, x, y, z, flipX, flipY, scaleX, scaleY, texture, sx, sy, sw, sh);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_UV,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.Image
 * \desc Draws an image in 3D space.
 * \param image (integer): Index of the loaded image.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \param z (number): Z position of where to draw the image.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_Image(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);

	Image* image = GET_ARG(0, GetImage);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);

	GET_MATRICES(4);

	if (!image) {
		return NULL_VAL;
	}

	Texture* texture = image->TexturePtr;
	VertexAttribute data[4];

	Graphics::MakeSpritePolygon(
		data, x, y, z, 0, 0, 1.0f, 1.0f, texture, 0, 0, texture->Width, texture->Height);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_Normal | VertexType_UV,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.ImagePart
 * \desc Draws part of an image in 3D space.
 * \param image (integer): Index of the loaded image.
 * \param x (number): X position of where to draw the image.
 * \param y (number): Y position of where to draw the image.
 * \param z (number): Z position of where to draw the image.
 * \param partX (integer): X coordinate of part of image to draw.
 * \param partY (integer): Y coordinate of part of image to draw.
 * \param partW (integer): Width of part of image to draw.
 * \param partH (integer): Height of part of image to draw.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_ImagePart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(8);

	Image* image = GET_ARG(0, GetImage);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);
	int sx = (int)GET_ARG(4, GetDecimal);
	int sy = (int)GET_ARG(5, GetDecimal);
	int sw = (int)GET_ARG(6, GetDecimal);
	int sh = (int)GET_ARG(7, GetDecimal);

	GET_MATRICES(8);

	if (!image) {
		return NULL_VAL;
	}

	Texture* texture = image->TexturePtr;
	VertexAttribute data[4];

	Graphics::MakeSpritePolygon(data, x, y, z, 0, 0, 1.0f, 1.0f, texture, sx, sy, sw, sh);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_Normal | VertexType_UV,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.Tile
 * \desc Draws a tile in 3D space.
 * \param ID (integer): ID of the tile to draw.
 * \param x (number): X position of where to draw the tile.
 * \param y (number): Y position of where to draw the tile.
 * \param z (number): Z position of where to draw the tile.
 * \param flipX (integer): Whether to flip the tile horizontally.
 * \param flipY (integer): Whether to flip the tile vertically.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_Tile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(6);

	Uint32 id = GET_ARG(0, GetInteger);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);
	int flipX = GET_ARG(4, GetInteger);
	int flipY = GET_ARG(5, GetInteger);

	GET_MATRICES(6);

	TileSpriteInfo info;
	ISprite* sprite;
	if (id < Scene::TileSpriteInfos.size() &&
		(info = Scene::TileSpriteInfos[id]).Sprite != NULL) {
		sprite = info.Sprite;
	}
	else {
		return NULL_VAL;
	}

	AnimFrame frameStr = sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	VertexAttribute data[4];

	Graphics::MakeSpritePolygon(data,
		x,
		y,
		z,
		flipX,
		flipY,
		1.0f,
		1.0f,
		texture,
		frameStr.X,
		frameStr.Y,
		frameStr.Width,
		frameStr.Height);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_UV,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.TriangleTextured
 * \desc Draws a textured triangle in 3D space. The texture source should be an image.
 * \param image (integer): Index of the loaded image.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt u1 (number): Texture U of the first vertex.
 * \paramOpt v1 (number): Texture V of the first vertex.
 * \paramOpt u2 (number): Texture U of the second vertex.
 * \paramOpt v2 (number): Texture V of the second vertex.
 * \paramOpt u3 (number): Texture U of the third vertex.
 * \paramOpt v3 (number): Texture V of the third vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_TriangleTextured(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(10);

	VertexAttribute data[3];

	Image* image = GET_ARG(0, GetImage);

	VERTEX_ARGS(3, 1);
	VERTEX_COLOR_ARGS(3);

	// 0
	// | \
    // 1--2

	data[1].UV.X = FP16_TO(1.0f);

	data[2].UV.X = FP16_TO(1.0f);
	data[2].UV.Y = FP16_TO(1.0f);

	VERTEX_UV_ARGS(3);

	GET_MATRICES(argOffset);

	if (image) {
		DrawPolygon3D(data,
			3,
			VertexType_Position | VertexType_UV | VertexType_Color,
			image->TexturePtr,
			matrixModelArr,
			matrixNormalArr);
	}
	return NULL_VAL;
}
/***
 * Draw3D.QuadTextured
 * \desc Draws a textured quad in 3D space. The texture source should be an image.
 * \param image (integer): Index of the loaded image.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \param z4 (number): Z position of the fourth vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt color4 (integer): Color of the fourth vertex.
 * \paramOpt u1 (number): Texture U of the first vertex.
 * \paramOpt v1 (number): Texture V of the first vertex.
 * \paramOpt u2 (number): Texture U of the second vertex.
 * \paramOpt v2 (number): Texture V of the second vertex.
 * \paramOpt u3 (number): Texture U of the third vertex.
 * \paramOpt v3 (number): Texture V of the third vertex.
 * \paramOpt u4 (number): Texture U of the fourth vertex.
 * \paramOpt v4 (number): Texture V of the fourth vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_QuadTextured(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(13);

	VertexAttribute data[4];

	Image* image = GET_ARG(0, GetImage);

	VERTEX_ARGS(4, 1);
	VERTEX_COLOR_ARGS(4);

	// 0--1
	// |  |
	// 3--2

	data[1].UV.X = FP16_TO(1.0f);

	data[2].UV.X = FP16_TO(1.0f);
	data[2].UV.Y = FP16_TO(1.0f);

	data[3].UV.Y = FP16_TO(1.0f);

	VERTEX_UV_ARGS(4);

	GET_MATRICES(argOffset);

	if (image) {
		DrawPolygon3D(data,
			4,
			VertexType_Position | VertexType_UV | VertexType_Color,
			image->TexturePtr,
			matrixModelArr,
			matrixNormalArr);
	}
	return NULL_VAL;
}
/***
 * Draw3D.SpritePoints
 * \desc Draws a textured rectangle in 3D space. The texture source should be a sprite.
 * \param sprite (integer): Index of the loaded sprite.
 * \param animation (integer): Index of the animation entry.
 * \param frame (integer): Index of the frame in the animation entry.
 * \param flipX (integer): Whether to flip the sprite horizontally.
 * \param flipY (integer): Whether to flip the sprite vertically.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \param z4 (number): Z position of the fourth vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt color4 (integer): Color of the fourth vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_SpritePoints(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(17);

	VertexAttribute data[4];

	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	int flipX = GET_ARG(3, GetInteger);
	int flipY = GET_ARG(4, GetInteger);

	if (Graphics::SpriteRangeCheck(sprite, animation, frame)) {
		return NULL_VAL;
	}

	AnimFrame frameStr = sprite->Animations[animation].Frames[frame];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	VERTEX_ARGS(4, 5);
	VERTEX_COLOR_ARGS(4);
	GET_MATRICES(argOffset);

	Graphics::MakeSpritePolygonUVs(data,
		flipX,
		flipY,
		texture,
		frameStr.X,
		frameStr.Y,
		frameStr.Width,
		frameStr.Height);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_UV | VertexType_Color,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.TilePoints
 * \desc Draws a textured rectangle in 3D space.
 * \param ID (integer): ID of the tile to draw.
 * \param flipX (integer): Whether to flip the tile horizontally.
 * \param flipY (integer): Whether to flip the tile vertically.
 * \param x1 (number): X position of the first vertex.
 * \param y1 (number): Y position of the first vertex.
 * \param z1 (number): Z position of the first vertex.
 * \param x2 (number): X position of the second vertex.
 * \param y2 (number): Y position of the second vertex.
 * \param z2 (number): Z position of the second vertex.
 * \param x3 (number): X position of the third vertex.
 * \param y3 (number): Y position of the third vertex.
 * \param z3 (number): Z position of the third vertex.
 * \param x4 (number): X position of the fourth vertex.
 * \param y4 (number): Y position of the fourth vertex.
 * \param z4 (number): Z position of the fourth vertex.
 * \paramOpt color1 (integer): Color of the first vertex.
 * \paramOpt color2 (integer): Color of the second vertex.
 * \paramOpt color3 (integer): Color of the third vertex.
 * \paramOpt color4 (integer): Color of the fourth vertex.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_TilePoints(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(15);

	VertexAttribute data[4];
	TileSpriteInfo info;
	ISprite* sprite;

	Uint32 id = GET_ARG(0, GetInteger);
	int flipX = GET_ARG(1, GetInteger);
	int flipY = GET_ARG(2, GetInteger);
	if (id < Scene::TileSpriteInfos.size() &&
		(info = Scene::TileSpriteInfos[id]).Sprite != NULL) {
		sprite = info.Sprite;
	}
	else {
		return NULL_VAL;
	}

	AnimFrame frameStr = sprite->Animations[info.AnimationIndex].Frames[info.FrameIndex];
	Texture* texture = sprite->Spritesheets[frameStr.SheetNumber];

	VERTEX_ARGS(4, 3);
	VERTEX_COLOR_ARGS(4);
	GET_MATRICES(argOffset);

	Graphics::MakeSpritePolygonUVs(data,
		flipX,
		flipY,
		texture,
		frameStr.X,
		frameStr.Y,
		frameStr.Width,
		frameStr.Height);
	DrawPolygon3D(data,
		4,
		VertexType_Position | VertexType_UV | VertexType_Color,
		texture,
		matrixModelArr,
		matrixNormalArr);
	return NULL_VAL;
}
/***
 * Draw3D.SceneLayer
 * \desc Draws a scene layer in 3D space.
 * \param layer (integer): Index of the layer.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_SceneLayer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	int layerID = GET_ARG(0, GetInteger);

	GET_MATRICES(1);
	PREPARE_MATRICES(matrixModelArr, matrixNormalArr);

	CHECK_SCENE_LAYER_INDEX(layerID);

	SceneLayer* layer = &Scene::Layers[layerID];
	Graphics::DrawSceneLayer3D(
		layer, 0, 0, layer->Width, layer->Height, matrixModel, matrixNormal);
	return NULL_VAL;
}
/***
 * Draw3D.SceneLayerPart
 * \desc Draws part of a scene layer in 3D space.
 * \param layer (integer): Index of the layer.
 * \param partX (integer): X coordinate (in tiles) of part of layer to draw.
 * \param partY (integer): Y coordinate (in tiles) of part of layer to draw.
 * \param partW (integer): Width (in tiles) of part of layer to draw.
 * \param partH (integer): Height (in tiles) of part of layer to draw.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \ns Draw3D
 */
VMValue Draw3D_SceneLayerPart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);
	int layerID = GET_ARG(0, GetInteger);
	int sx = (int)GET_ARG(1, GetDecimal);
	int sy = (int)GET_ARG(2, GetDecimal);
	int sw = (int)GET_ARG(3, GetDecimal);
	int sh = (int)GET_ARG(4, GetDecimal);

	GET_MATRICES(5);
	PREPARE_MATRICES(matrixModelArr, matrixNormalArr);

	CHECK_SCENE_LAYER_INDEX(layerID);

	SceneLayer* layer = &Scene::Layers[layerID];
	if (sx < 0) {
		sx = 0;
	}
	if (sy < 0) {
		sy = 0;
	}
	if (sw <= 0 || sh <= 0) {
		return NULL_VAL;
	}
	if (sw > layer->Width) {
		sw = layer->Width;
	}
	if (sh > layer->Height) {
		sh = layer->Height;
	}
	if (sx >= sw || sy >= sh) {
		return NULL_VAL;
	}

	Graphics::DrawSceneLayer3D(layer, sx, sy, sw, sh, matrixModel, matrixNormal);
	return NULL_VAL;
}
/***
 * Draw3D.VertexBuffer
 * \desc Draws a vertex buffer.
 * \param vertexBufferIndex (integer): The vertex buffer to draw.
 * \paramOpt matrixModel (matrix): Matrix for transforming coordinates to world space.
 * \paramOpt matrixNormal (matrix): Matrix for transforming normals.
 * \return
 * \ns Draw3D
 */
VMValue Draw3D_VertexBuffer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	Uint32 vertexBufferIndex = GET_ARG(0, GetInteger);
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return NULL_VAL;
	}

	GET_MATRICES(1);
	PREPARE_MATRICES(matrixModelArr, matrixNormalArr);

	Graphics::DrawVertexBuffer(vertexBufferIndex, matrixModel, matrixNormal);
	return NULL_VAL;
}
#undef PREPARE_MATRICES
/***
 * Draw3D.RenderScene
 * \desc Draws everything in the 3D scene.
 * \param scene3DIndex (integer): The 3D scene at the index to draw.
 * \paramOpt drawMode (integer): The type of drawing to use for the vertices in the 3D scene.
 * \ns Draw3D
 */
VMValue Draw3D_RenderScene(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	Uint32 drawMode = GET_ARG_OPT(1, GetInteger, 0);
	GET_SCENE_3D();
	Graphics::DrawScene3D(scene3DIndex, drawMode);
	return NULL_VAL;
}
// #endregion

// #region Ease
/***
 * Ease.InSine
 * \desc Eases the value using the "InSine" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InSine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InSine(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutSine
 * \desc Eases the value using the "OutSine" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutSine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutSine(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutSine
 * \desc Eases the value using the "InOutSine" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutSine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutSine(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InQuad
 * \desc Eases the value using the "InQuad" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuad(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InQuad(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutQuad
 * \desc Eases the value using the "OutQuad" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuad(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutQuad(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutQuad
 * \desc Eases the value using the "InOutQuad" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuad(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutQuad(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InCubic
 * \desc Eases the value using the "InCubic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InCubic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InCubic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutCubic
 * \desc Eases the value using the "OutCubic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutCubic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutCubic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutCubic
 * \desc Eases the value using the "InOutCubic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutCubic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutCubic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InQuart
 * \desc Eases the value using the "InQuart" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InQuart(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutQuart
 * \desc Eases the value using the "OutQuart" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutQuart(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutQuart
 * \desc Eases the value using the "InOutQuart" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutQuart(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InQuint
 * \desc Eases the value using the "InQuint" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InQuint(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutQuint
 * \desc Eases the value using the "OutQuint" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutQuint(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutQuint
 * \desc Eases the value using the "InOutQuint" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutQuint(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InExpo
 * \desc Eases the value using the "InExpo" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InExpo(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InExpo(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutExpo
 * \desc Eases the value using the "OutExpo" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutExpo(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutExpo(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutExpo
 * \desc Eases the value using the "InOutExpo" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutExpo(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutExpo(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InCirc
 * \desc Eases the value using the "InCirc" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InCirc(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InCirc(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutCirc
 * \desc Eases the value using the "OutCirc" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutCirc(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutCirc(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutCirc
 * \desc Eases the value using the "InOutCirc" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutCirc(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutCirc(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InBack
 * \desc Eases the value using the "InBack" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InBack(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InBack(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutBack
 * \desc Eases the value using the "OutBack" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutBack(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutBack(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutBack
 * \desc Eases the value using the "InOutBack" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutBack(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutBack(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InElastic
 * \desc Eases the value using the "InElastic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InElastic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InElastic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutElastic
 * \desc Eases the value using the "OutElastic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutElastic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutElastic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutElastic
 * \desc Eases the value using the "InOutElastic" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutElastic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutElastic(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InBounce
 * \desc Eases the value using the "InBounce" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InBounce(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InBounce(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.OutBounce
 * \desc Eases the value using the "OutBounce" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutBounce(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::OutBounce(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.InOutBounce
 * \desc Eases the value using the "InOutBounce" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutBounce(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::InOutBounce(GET_ARG(0, GetDecimal)));
}
/***
 * Ease.Triangle
 * \desc Eases the value using the "Triangle" formula.
 * \param value (number): Percent value. (0.0 - 1.0)
 * \return decimal Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_Triangle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Ease::Triangle(GET_ARG(0, GetDecimal)));
}
// #endregion

// #region File
/***
 * File.Exists
 * \desc Determines if the file at the path exists.
 * \param path (string): The path of the file to check for existence.
 * \return boolean Returns whether the file exists.
 * \ns File
 */
VMValue File_Exists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* filePath = GET_ARG(0, GetString);
	return INTEGER_VAL(File::ProtectedExists(filePath, true));
}
/***
 * File.ReadAllText
 * \desc Reads all text from the given filename.
 * \param path (string): The path of the file to read.
 * \return string Returns all the text in the file as a string value if it can be read, otherwise it returns a `null` value if it cannot be read.
 * \ns File
 */
VMValue File_ReadAllText(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* filePath = GET_ARG(0, GetString);
	Stream* stream = FileStream::New(filePath, FileStream::READ_ACCESS, true);
	if (!stream) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		size_t size = stream->Length();
		ObjString* text = AllocString(size);
		stream->ReadBytes(text->Chars, size);
		stream->Close();
		ScriptManager::Unlock();
		return OBJECT_VAL(text);
	}
	return NULL_VAL;
}
/***
 * File.WriteAllText
 * \desc Writes all text to the given filename.
 * \param path (string): The path of the file to read.
 * \param text (string): The text to write to the file.
 * \return boolean Returns whether the file was written successfully.
 * \ns File
 */
VMValue File_WriteAllText(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filePath = GET_ARG(0, GetString);
	// To verify 2nd argument is string
	GET_ARG(1, GetString);
	if (ScriptManager::Lock()) {
		ObjString* text = AS_STRING(args[1]);

		Stream* stream = FileStream::New(filePath, FileStream::WRITE_ACCESS, true);
		if (!stream) {
			ScriptManager::Unlock();
			return INTEGER_VAL(false);
		}

		stream->WriteBytes(text->Chars, text->Length);
		stream->Close();

		ScriptManager::Unlock();
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
// #endregion

// #region Geometry
static vector<FVector2> GetPolygonPoints(ObjArray* array, const char* arrName, int threadID) {
	vector<FVector2> input;
	input.reserve(array->Values->size());

	for (unsigned i = 0; i < array->Values->size(); i++) {
		VMValue vtxVal = (*array->Values)[i];

		if (!IS_ARRAY(vtxVal)) {
			THROW_ERROR(
				"Expected value at index %d of %s to be of type %s instead of %s.",
				i,
				arrName,
				GetObjectTypeString(OBJ_ARRAY),
				GetValueTypeString(vtxVal));
			input.clear();
			break;
		}

		ObjArray* vtx = AS_ARRAY(vtxVal);
		VMValue xVal = (*vtx->Values)[0];
		VMValue yVal = (*vtx->Values)[1];

		float x, y;

		// Get X
		if (IS_DECIMAL(xVal)) {
			x = AS_DECIMAL(xVal);
		}
		else if (IS_INTEGER(xVal)) {
			x = (float)(AS_INTEGER(xVal));
		}
		else {
			THROW_ERROR(
				"Expected X value (index %d) at vertex index %d of %s to be of type %s instead of %s.",
				0,
				i,
				arrName,
				GetTypeString(VAL_DECIMAL),
				GetValueTypeString(xVal));
			input.clear();
			break;
		}

		// Get Y
		if (IS_DECIMAL(yVal)) {
			y = AS_DECIMAL(yVal);
		}
		else if (IS_INTEGER(yVal)) {
			y = (float)(AS_INTEGER(yVal));
		}
		else {
			THROW_ERROR(
				"Expected Y value (index %d) at vertex index %d of %s to be of type %s instead of %s.",
				1,
				i,
				arrName,
				GetTypeString(VAL_DECIMAL),
				GetValueTypeString(yVal));
			input.clear();
			break;
		}

		FVector2 vec(x, y);

		input.push_back(vec);
	}

	return input;
}
/***
 * Geometry.Triangulate
 * \desc Triangulates a 2D polygon.
 * \param polygon (array): Array of vertices that compromise the polygon to triangulate.
 * \paramOpt holes (array): Array of polygons that compromise the holes to be made in the resulting shape.
 * \return array Returns an array containing a list of triangles, or `null` if the polygon could not be triangulated.
 * \ns Geometry
 */
VMValue Geometry_Triangulate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	ObjArray* arrPoly = GET_ARG(0, GetArray);
	ObjArray* arrHoles = GET_ARG_OPT(1, GetArray, nullptr);

	vector<FVector2> points = GetPolygonPoints(arrPoly, "polygon array", threadID);
	if (!points.size()) {
		return NULL_VAL;
	}

	Polygon2D inputPoly(points);
	vector<Polygon2D> inputHoles;

	if (arrHoles) {
		for (unsigned i = 0; i < arrHoles->Values->size(); i++) {
			VMValue value = (*arrHoles->Values)[i];
			if (!IS_ARRAY(value)) {
				THROW_ERROR(
					"Expected value at index %d of holes array to be of type %s instead of %s.",
					i,
					GetObjectTypeString(OBJ_ARRAY),
					GetValueTypeString(value));
				return NULL_VAL;
			}

			Polygon2D hole(GetPolygonPoints(AS_ARRAY(value), "holes array", threadID));
			inputHoles.push_back(hole);
		}

		// Holes must not be touching each other or the bounds of the shape, so these two operations are needed
		vector<Polygon2D>* unionResult = Geometry::Intersect(
			GeoBooleanOp_Union, GeoFillRule_EvenOdd, inputHoles, {});
		inputHoles.clear();
		for (unsigned i = 0; i < unionResult->size(); i++) {
			inputHoles.push_back((*unionResult)[i]);
		}
		delete unionResult;

		vector<Polygon2D>* intersectResult = Geometry::Intersect(
			GeoBooleanOp_Intersection, GeoFillRule_EvenOdd, inputHoles, {inputPoly});
		inputHoles.clear();
		for (unsigned i = 0; i < intersectResult->size(); i++) {
			inputHoles.push_back((*intersectResult)[i]);
		}
		delete intersectResult;
	}

	vector<Polygon2D>* output = Geometry::Triangulate(inputPoly, inputHoles);
	if (!output) {
		return NULL_VAL;
	}

	ObjArray* result = NewArray();

	for (unsigned i = 0; i < output->size(); i++) {
		Polygon2D poly = (*output)[i];
		ObjArray* triArr = NewArray();

		for (unsigned j = 0; j < 3; j++) {
			ObjArray* vtx = NewArray();
			vtx->Values->push_back(DECIMAL_VAL(poly.Points[j].X));
			vtx->Values->push_back(DECIMAL_VAL(poly.Points[j].Y));
			triArr->Values->push_back(OBJECT_VAL(vtx));
		}

		result->Values->push_back(OBJECT_VAL(triArr));
	}

	delete output;

	return OBJECT_VAL(result);
}
/***
 * Geometry.Intersect
 * \desc Intersects a 2D polygon.
 * \param subjects (array): Array of subject polygons.
 * \param clips (array): Array of clip polygons.
 * \paramOpt booleanOp (<ref GeoBooleanOp_*>): The boolean operation. (default: `<ref GeoBooleanOp_Intersection>`)
 * \paramOpt fillRule (<ref GeoFillRule_*>): The fill rule. (default: `<ref GeoFillRule_EvenOdd>`)
 * \return array Returns an array containing a list of intersected polygons, or `null` if the polygon could not be intersected.
 * \ns Geometry
 */
VMValue Geometry_Intersect(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);

	ObjArray* subjects = GET_ARG(0, GetArray);
	ObjArray* clips = GET_ARG(1, GetArray);
	int booleanOp = GET_ARG_OPT(2, GetInteger, GeoBooleanOp_Intersection);
	int fillRule = GET_ARG_OPT(3, GetInteger, GeoFillRule_EvenOdd);

	if (booleanOp < GeoBooleanOp_Intersection || booleanOp > GeoBooleanOp_ExclusiveOr) {
		OUT_OF_RANGE_ERROR("Boolean operation",
			booleanOp,
			GeoBooleanOp_Intersection,
			GeoBooleanOp_ExclusiveOr);
		return NULL_VAL;
	}

	if (fillRule < GeoFillRule_EvenOdd || fillRule > GeoFillRule_Negative) {
		OUT_OF_RANGE_ERROR(
			"Fill rule", fillRule, GeoFillRule_EvenOdd, GeoFillRule_Negative);
		return NULL_VAL;
	}

	vector<Polygon2D> inputSubjects;
	vector<Polygon2D> inputClips;

	// Get subjects
	for (unsigned i = 0; i < subjects->Values->size(); i++) {
		VMValue value = (*subjects->Values)[i];
		if (!IS_ARRAY(value)) {
			THROW_ERROR(
				"Expected value at index %d of subjects array to be of type %s instead of %s.",
				i,
				GetObjectTypeString(OBJ_ARRAY),
				GetValueTypeString(value));
			return NULL_VAL;
		}

		Polygon2D subject(GetPolygonPoints(AS_ARRAY(value), "subject array", threadID));
		inputSubjects.push_back(subject);
	}

	// Get clips
	for (unsigned i = 0; i < clips->Values->size(); i++) {
		VMValue value = (*clips->Values)[i];
		if (!IS_ARRAY(value)) {
			THROW_ERROR(
				"Expected value at index %d of clips array to be of type %s instead of %s.",
				i,
				GetObjectTypeString(OBJ_ARRAY),
				GetValueTypeString(value));
			return NULL_VAL;
		}

		Polygon2D clip(GetPolygonPoints(AS_ARRAY(value), "clip array", threadID));
		inputClips.push_back(clip);
	}

	vector<Polygon2D>* output =
		Geometry::Intersect(booleanOp, fillRule, inputSubjects, inputClips);
	if (!output) {
		return NULL_VAL;
	}

	ObjArray* result = NewArray();

	for (unsigned i = 0; i < output->size(); i++) {
		Polygon2D& poly = (*output)[i];
		ObjArray* polyArr = NewArray();

		for (unsigned j = 0; j < poly.Points.size(); j++) {
			ObjArray* vtx = NewArray();
			vtx->Values->push_back(DECIMAL_VAL(poly.Points[j].X));
			vtx->Values->push_back(DECIMAL_VAL(poly.Points[j].Y));
			polyArr->Values->push_back(OBJECT_VAL(vtx));
		}

		result->Values->push_back(OBJECT_VAL(polyArr));
	}

	delete output;

	return OBJECT_VAL(result);
}
/***
 * Geometry.IsPointInsidePolygon
 * \desc Checks if a point is inside a polygon.
 * \param polygon (array): The polygon.
 * \param pointX (decimal): The X of the point.
 * \param pointY (decimal): The Y of the point.
 * \return boolean Returns whether the point is inside.
 * \ns Geometry
 */
VMValue Geometry_IsPointInsidePolygon(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	ObjArray* arr = GET_ARG(0, GetArray);
	float pointX = GET_ARG(1, GetDecimal);
	float pointY = GET_ARG(2, GetDecimal);

	Polygon2D polygon(GetPolygonPoints(arr, "polygon array", threadID));

	return INTEGER_VAL(polygon.IsPointInside(pointX, pointY));
}
/***
 * Geometry.IsLineIntersectingPolygon
 * \desc Checks if a line segment is intersecting a polygon.
 * \param polygon (array): The polygon to check.
 * \param x1 (decimal): The starting X of the segment.
 * \param y1 (decimal): The starting Y of the segment.
 * \param x2 (decimal): The ending X of the segment.
 * \param y2 (decimal): The ending Y of the segment.
 * \return boolean Returns whether the line segment is intersecting the polygon.
 * \ns Geometry
 */
VMValue Geometry_IsLineIntersectingPolygon(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);

	ObjArray* arr = GET_ARG(0, GetArray);
	float x1 = GET_ARG(1, GetDecimal);
	float y1 = GET_ARG(2, GetDecimal);
	float x2 = GET_ARG(3, GetDecimal);
	float y2 = GET_ARG(4, GetDecimal);

	Polygon2D polygon(GetPolygonPoints(arr, "polygon array", threadID));

	return INTEGER_VAL(polygon.IsLineSegmentIntersecting(x1, y1, x2, y2));
}
// #endregion

// #region HTTP
struct _HTTP_Bundle {
	char* url;
	char* filename;
	ObjBoundMethod callback;
};
int _HTTP_GetToFile(void* opaque) {
	_HTTP_Bundle* bundle = (_HTTP_Bundle*)opaque;

	size_t length;
	Uint8* data = NULL;
	if (HTTP::GET(bundle->url, &data, &length, NULL)) {
		Stream* stream = FileStream::New(bundle->filename, FileStream::WRITE_ACCESS);
		if (stream) {
			stream->WriteBytes(data, length);
			stream->Close();
		}
		Memory::Free(data);
	}
	free(bundle);
	return 0;
}

/***
 * HTTP.GetString
 * \desc
 * \return
 * \ns HTTP
 */
VMValue HTTP_GetString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* url = NULL;
	ObjBoundMethod* callback = NULL;

	url = GET_ARG(0, GetString);
	if (IS_BOUND_METHOD(args[1])) {
		callback = GET_ARG(1, GetBoundMethod);
	}

	size_t length;
	Uint8* data = NULL;
	if (!HTTP::GET(url, &data, &length, callback)) {
		return NULL_VAL;
	}

	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		obj = OBJECT_VAL(TakeString((char*)data, length));
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * HTTP.GetToFile
 * \desc
 * \return
 * \ns HTTP
 */
VMValue HTTP_GetToFile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	char* url = GET_ARG(0, GetString);
	char* filename = GET_ARG(1, GetString);
	bool blocking = GET_ARG(2, GetInteger);

	size_t url_sz = strlen(url) + 1;
	size_t filename_sz = strlen(filename) + 1;
	_HTTP_Bundle* bundle = (_HTTP_Bundle*)malloc(sizeof(_HTTP_Bundle) + url_sz + filename_sz);
	bundle->url = (char*)(bundle + 1);
	bundle->filename = bundle->url + url_sz;
	strcpy(bundle->url, url);
	strcpy(bundle->filename, filename);

	if (blocking) {
		_HTTP_GetToFile(bundle);
	}
	else {
		SDL_CreateThread(_HTTP_GetToFile, "HTTP.GetToFile", bundle);
	}
	return NULL_VAL;
}
// #endregion

// #region Image
/***
 * Image.GetWidth
 * \desc Gets the width of the specified image.
 * \param image (integer): The image index to check.
 * \return integer Returns an integer value.
 * \ns Image
 */
VMValue Image_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Image* image = GET_ARG(0, GetImage);
	if (image) {
		Texture* texture = image->TexturePtr;
		return INTEGER_VAL((int)texture->Width);
	}
	return INTEGER_VAL(0);
}
/***
 * Image.GetHeight
 * \desc Gets the height of the specified image.
 * \param image (integer): The image index to check.
 * \return integer Returns an integer value.
 * \ns Image
 */
VMValue Image_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Image* image = GET_ARG(0, GetImage);
	if (image) {
		Texture* texture = image->TexturePtr;
		return INTEGER_VAL((int)texture->Height);
	}
	return INTEGER_VAL(0);
}
// #endregion

// #region Input
/***
 * Input.GetMouseX
 * \desc Gets cursor's X position.
 * \return decimal Returns cursor's X position in relation to the window.
 * \ns Input
 */
VMValue Input_GetMouseX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int value;
	SDL_GetMouseState(&value, NULL);
	return DECIMAL_VAL((float)value);
}
/***
 * Input.GetMouseY
 * \desc Gets cursor's Y position.
 * \return decimal Returns cursor's Y position in relation to the window.
 * \ns Input
 */
VMValue Input_GetMouseY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int value;
	SDL_GetMouseState(NULL, &value);
	return DECIMAL_VAL((float)value);
}
/***
 * Input.IsMouseButtonDown
 * \desc Gets whether the mouse button is currently down.<br/>\
<br/>Mouse Button Indexes:<ul>\
<li>`0`: Left</li>\
<li>`1`: Middle</li>\
<li>`2`: Right</li>\
</ul>
 * \param mouseButtonID (integer): Index of the mouse button to check.
 * \return boolean Returns whether the mouse button is currently down.
 * \ns Input
 */
VMValue Input_IsMouseButtonDown(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int button = GET_ARG(0, GetInteger);
	return INTEGER_VAL((InputManager::MouseDown >> button) & 1);
}
/***
 * Input.IsMouseButtonPressed
 * \desc Gets whether the mouse button started pressing during the current frame.<br/>\
<br/>Mouse Button Indexes:<ul>\
<li>`0`: Left</li>\
<li>`1`: Middle</li>\
<li>`2`: Right</li>\
</ul>
 * \param mouseButtonID (integer): Index of the mouse button to check.
 * \return boolean Returns whether the mouse button started pressing during the current frame.
 * \ns Input
 */
VMValue Input_IsMouseButtonPressed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int button = GET_ARG(0, GetInteger);
	return INTEGER_VAL((InputManager::MousePressed >> button) & 1);
}
/***
 * Input.IsMouseButtonReleased
 * \desc Gets whether the mouse button released during the current frame.<br/>\
<br/>Mouse Button Indexes:<ul>\
<li>`0`: Left</li>\
<li>`1`: Middle</li>\
<li>`2`: Right</li>\
</ul>
 * \param mouseButtonID (integer): Index of the mouse button to check.
 * \return boolean Returns whether the mouse button released during the current frame.
 * \ns Input
 */
VMValue Input_IsMouseButtonReleased(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int button = GET_ARG(0, GetInteger);
	return INTEGER_VAL((InputManager::MouseReleased >> button) & 1);
}
/***
 * Input.GetMouseMode
 * \desc Gets the current mouse mode.
 * \return <ref MOUSEMODE_*> Returns the current mouse mode.
 * \ns Input
 */
VMValue Input_GetMouseMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(InputManager::MouseMode);
}
/***
 * Input.SetMouseMode
 * \desc Sets the current mouse mode.
 * \param mouseMode (<ref MOUSEMODE_*>): The mouse mode to set.
 * \ns Input
 */
VMValue Input_SetMouseMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int mouseMode = GET_ARG(0, GetInteger);
	if (mouseMode < 0 || mouseMode > MOUSEMODE_RELATIVE) {
		OUT_OF_RANGE_ERROR("Mouse mode", mouseMode, 0, MOUSEMODE_RELATIVE);
		return NULL_VAL;
	}
	InputManager::SetMouseMode(mouseMode);
	return NULL_VAL;
}
/***
 * Input.IsKeyDown
 * \desc Gets whether the key is currently down.
 * \param keyID (<ref Key_*>): The key ID to check.
 * \return boolean Returns whether the key is currently down.
 * \ns Input
 */
VMValue Input_IsKeyDown(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int key = GET_ARG(0, GetInteger);
	CHECK_KEYBOARD_KEY(key);
	return INTEGER_VAL(InputManager::IsKeyDown(key));
}
/***
 * Input.IsKeyPressed
 * \desc Gets whether the key started pressing during the current frame.
 * \param keyID (<ref Key_*>): The key ID to check.
 * \return boolean Returns whether the key started pressing during the current frame.
 * \ns Input
 */
VMValue Input_IsKeyPressed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int key = GET_ARG(0, GetInteger);
	CHECK_KEYBOARD_KEY(key);
	return INTEGER_VAL(InputManager::IsKeyPressed(key));
}
/***
 * Input.IsKeyReleased
 * \desc Gets whether the key released during the current frame.
 * \param keyID (<ref Key_*>): The key ID to check.
 * \return boolean Returns whether the key released during the current frame.
 * \ns Input
 */
VMValue Input_IsKeyReleased(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int key = GET_ARG(0, GetInteger);
	CHECK_KEYBOARD_KEY(key);
	return INTEGER_VAL(InputManager::IsKeyReleased(key));
}
/***
 * Input.GetKeyName
 * \desc Gets the name of the key.
 * \param keyID (<ref Key_*>): The key ID to check.
 * \return string Returns a string value.
 * \ns Input
 */
VMValue Input_GetKeyName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int key = GET_ARG(0, GetInteger);
	CHECK_KEYBOARD_KEY(key);
	return ReturnString(InputManager::GetKeyName(key));
}
/***
 * Input.GetButtonName
 * \desc Gets the name of the button.
 * \param button (<ref Button_*>): Which button to check.
 * \return string Returns a string value.
 * \ns Input
 */
VMValue Input_GetButtonName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int button = GET_ARG(0, GetInteger);
	CHECK_CONTROLLER_BUTTON(button);
	return ReturnString(InputManager::GetButtonName(button));
}
/***
 * Input.GetAxisName
 * \desc Gets the name of the axis.
 * \param axis (<ref Axis_*>): Which axis to check.
 * \return string Returns a string value.
 * \ns Input
 */
VMValue Input_GetAxisName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int axis = GET_ARG(0, GetInteger);
	CHECK_CONTROLLER_AXIS(axis);
	return ReturnString(InputManager::GetAxisName(axis));
}
/***
 * Input.ParseKeyName
 * \desc Parses a string into a <ref Key_*>.
 * \param keyName (string): The key name to parse.
 * \return <ref Key_*> Returns the parsed key, or `null` if it could not be parsed.
 * \ns Input
 */
VMValue Input_ParseKeyName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* key = GET_ARG(0, GetString);
	int parsed = InputManager::ParseKeyName(key);
	if (parsed == Key_UNKNOWN) {
		return NULL_VAL;
	}
	return INTEGER_VAL(parsed);
}
/***
 * Input.ParseButtonName
 * \desc Parses a string into a <ref Button_*>.
 * \param buttonName (string): The button name to parse.
 * \return <ref Button_*> Returns the parsed button, or `null` if it could not be parsed.
 * \ns Input
 */
VMValue Input_ParseButtonName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* button = GET_ARG(0, GetString);
	int parsed = InputManager::ParseButtonName(button);
	if (parsed < 0) {
		return NULL_VAL;
	}
	return INTEGER_VAL(parsed);
}
/***
 * Input.ParseAxisName
 * \desc Parses a string into an <ref Axis_*>.
 * \param axisName (string): The axis name to parse.
 * \return <ref Axis_*> Returns the parsed axis, or `null` if it could not be parsed.
 * \ns Input
 */
VMValue Input_ParseAxisName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* axis = GET_ARG(0, GetString);
	int parsed = InputManager::ParseAxisName(axis);
	if (parsed < 0) {
		return NULL_VAL;
	}
	return INTEGER_VAL(parsed);
}
/***
 * Input.GetActionList
 * \desc Gets a list of all input actions.
 * \return array Returns an array value, or `null` if no actions are registered.
 * \ns Input
 */
VMValue Input_GetActionList(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	size_t count = InputManager::Actions.size();
	if (count == 0) {
		return NULL_VAL;
	}

	ObjArray* array = NewArray();

	for (size_t i = 0; i < count; i++) {
		InputAction& action = InputManager::Actions[i];

		ObjString* actionName = CopyString(action.Name);

		array->Values->push_back(OBJECT_VAL(actionName));
	}

	return OBJECT_VAL(array);
}
/***
 * Input.ActionExists
 * \desc Gets whether the given input action exists.
 * \param actionName (string): Name of the action to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_ActionExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* actionName = GET_ARG(0, GetString);
	int actionID = InputManager::GetActionID(actionName);
	if (actionID != -1) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * Input.IsActionHeld
 * \desc Gets whether the input action is currently held for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionHeld(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int actionID = InputManager::GetActionID(actionName);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 3) {
		int inputDevice = GET_ARG(2, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsActionHeld(playerID, actionID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsActionHeld(playerID, actionID));
	}
}
/***
 * Input.IsActionPressed
 * \desc Gets whether the input action is currently pressed for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionPressed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int actionID = InputManager::GetActionID(actionName);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 3) {
		int inputDevice = GET_ARG(2, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(
			!!InputManager::IsActionPressed(playerID, actionID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsActionPressed(playerID, actionID));
	}
}
/***
 * Input.IsActionReleased
 * \desc Gets whether the input action was released for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionReleased(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int actionID = InputManager::GetActionID(actionName);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 3) {
		int inputDevice = GET_ARG(2, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(
			!!InputManager::IsActionReleased(playerID, actionID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsActionReleased(playerID, actionID));
	}
}
/***
 * Input.IsActionHeldByAny
 * \desc Gets whether the input action is currently held by any player.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionHeldByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* actionName = GET_ARG(0, GetString);
	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsActionHeldByAny(actionID, inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsActionHeldByAny(actionID));
}
/***
 * Input.IsActionPressedByAny
 * \desc Gets whether the input action is currently pressed by any player.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionPressedByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* actionName = GET_ARG(0, GetString);
	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsActionPressedByAny(actionID, inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsActionPressedByAny(actionID));
}
/***
 * Input.IsActionReleasedByAny
 * \desc Gets whether the input action was released by any player.
 * \param actionName (string): Name of the action to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsActionReleasedByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* actionName = GET_ARG(0, GetString);
	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsActionReleasedByAny(actionID, inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsActionReleasedByAny(actionID));
}
/***
 * Input.IsAnyActionHeld
 * \desc Gets whether any input action is currently held for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionHeld(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionHeld(playerID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsAnyActionHeld(playerID));
	}
}
/***
 * Input.IsAnyActionPressed
 * \desc Gets whether any input action is currently pressed for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionPressed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionPressed(playerID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsAnyActionPressed(playerID));
	}
}
/***
 * Input.IsAnyActionReleased
 * \desc Gets whether any input action was released for the specified player.
 * \param playerID (integer): Index of the player to check.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionReleased(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	if (argCount >= 2) {
		int inputDevice = GET_ARG(1, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionReleased(playerID, inputDevice));
	}
	else {
		return INTEGER_VAL(!!InputManager::IsAnyActionReleased(playerID));
	}
}
/***
 * Input.IsAnyActionHeldByAny
 * \desc Gets whether any input action is currently held by any player.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionHeldByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(0);
	if (argCount >= 1) {
		int inputDevice = GET_ARG(0, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionHeldByAny(inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsAnyActionHeldByAny());
}
/***
 * Input.IsAnyActionPressedByAny
 * \desc Gets whether any input action is currently pressed by any player.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionPressedByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(0);
	if (argCount >= 1) {
		int inputDevice = GET_ARG(0, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionPressedByAny(inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsAnyActionPressedByAny());
}
/***
 * Input.IsAnyActionReleasedByAny
 * \desc Gets whether any input action was released by any player.
 * \paramOpt inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsAnyActionReleasedByAny(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(0);
	if (argCount >= 1) {
		int inputDevice = GET_ARG(0, GetInteger);
		CHECK_INPUT_DEVICE(inputDevice);
		return INTEGER_VAL(!!InputManager::IsAnyActionReleasedByAny(inputDevice));
	}
	else
		return INTEGER_VAL(!!InputManager::IsAnyActionReleasedByAny());
}
/***
 * Input.GetAnalogActionInput
 * \desc Gets the analog value of a specific action.
 * \param playerID (integer): Index of the player to check.
 * \param actionName (string): Name of the action to check.
 * \return decimal Returns a decimal value.
 * \ns Input
 */
VMValue Input_GetAnalogActionInput(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}
	CHECK_INPUT_PLAYER_INDEX(playerID);
	return DECIMAL_VAL(InputManager::GetAnalogActionInput(playerID, actionID));
}
static KeyboardBind* GetKeyboardActionBind(ObjMap* map, Uint32 threadID) {
	KeyboardBind* bind = new KeyboardBind();

	map->Keys->WithAllOrdered([bind, map, threadID](Uint32 hash, char* key) -> void {
		VMValue value;

		// key: Integer
		if (strcmp("key", key) == 0) {
			value = map->Values->Get(hash);
			if (IS_INTEGER(value)) {
				int key = AS_INTEGER(value);
				if (key >= 0 && key < NUM_KEYBOARD_KEYS) {
					bind->Key = AS_INTEGER(value);
				}
				else {
					OUT_OF_RANGE_ERROR(
						"Keyboard key", key, 0, NUM_KEYBOARD_KEYS - 1);
				}
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR("Expected \"key\" to be of type %s instead of %s.",
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(value));
			}
		}
		// modifiers: Integer
		else if (strcmp("modifiers", key) == 0) {
			value = map->Values->Get("modifiers");
			if (IS_INTEGER(value)) {
				bind->Modifiers = AS_INTEGER(value);
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR(
					"Expected \"modifiers\" to be of type %s instead of %s.",
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(value));
			}
		}
	});

	return bind;
}
static ControllerButtonBind* GetControllerButtonActionBind(ObjMap* map, Uint32 threadID) {
	ControllerButtonBind* bind = new ControllerButtonBind();

	map->Keys->WithAllOrdered([&bind, map, threadID](Uint32 hash, char* key) -> void {
		VMValue value;

		// button: Integer
		if (strcmp("button", key) == 0) {
			value = map->Values->Get(hash);
			if (IS_INTEGER(value)) {
				int button = AS_INTEGER(value);
				if (button >= 0 && button < (int)ControllerButton::Max) {
					bind->Button = AS_INTEGER(value);
				}
				else {
					OUT_OF_RANGE_ERROR("Controller button",
						button,
						0,
						(int)ControllerButton::Max - 1);
				}
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR("Expected \"button\" to be of type %s instead of %s.",
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(value));
			}
		}
	});

	return bind;
}
static ControllerAxisBind* GetControllerAxisActionBind(ObjMap* map, Uint32 threadID) {
	ControllerAxisBind* bind = new ControllerAxisBind();

	map->Keys->WithAllOrdered([&bind, map, threadID](Uint32 hash, char* key) -> void {
		VMValue value;

		// axis: Integer
		if (strcmp("axis", key) == 0) {
			value = map->Values->Get("axis");
			if (IS_INTEGER(value)) {
				int axis = AS_INTEGER(value);
				if (axis >= 0 && axis < (int)ControllerAxis::Max) {
					bind->Axis = axis;
				}
				else {
					OUT_OF_RANGE_ERROR("Controller axis",
						axis,
						0,
						(int)ControllerAxis::Max - 1);
				}
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR("Expected \"axis\" to be of type %s instead of %s.",
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(value));
			}
		}
		// axis_deadzone: Decimal
		else if (strcmp("axis_deadzone", key) == 0) {
			value = map->Values->Get("axis_deadzone");
			if (IS_DECIMAL(value)) {
				bind->AxisDeadzone = AS_DECIMAL(value);
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR(
					"Expected \"axis_deadzone\" to be of type %s instead of %s.",
					GetTypeString(VAL_DECIMAL),
					GetValueTypeString(value));
			}
		}
		// axis_digital_threshold: Decimal
		else if (strcmp("axis_digital_threshold", key) == 0) {
			value = map->Values->Get("axis_digital_threshold");
			if (IS_DECIMAL(value)) {
				bind->AxisDigitalThreshold = AS_DECIMAL(value);
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR(
					"Expected \"axis_digital_threshold\" to be of type %s instead of %s.",
					GetTypeString(VAL_DECIMAL),
					GetValueTypeString(value));
			}
		}
		// axis_negative: Integer
		else if (strcmp("axis_negative", key) == 0) {
			value = map->Values->Get("axis_negative");
			if (IS_INTEGER(value)) {
				if (AS_INTEGER(value) != 0) {
					bind->IsAxisNegative = true;
				}
				else {
					bind->IsAxisNegative = false;
				}
			}
			else if (!IS_NULL(value)) {
				THROW_ERROR(
					"Expected \"axis_negative\" to be of type %s instead of %s.",
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(value));
			}
		}
	});

	return bind;
}
static ObjMap* CreateKeyboardActionMap(KeyboardBind* bind) {
	if (bind != nullptr && ScriptManager::Lock()) {
		ObjMap* map = NewMap();

		AddToMap(map, "key", (bind->Key != -1) ? INTEGER_VAL(bind->Key) : NULL_VAL);
		AddToMap(map, "modifiers", INTEGER_VAL(bind->Modifiers));

		ScriptManager::Unlock();

		return map;
	}

	return nullptr;
}
static ObjMap* CreateControllerButtonActionMap(ControllerButtonBind* bind) {
	if (bind != nullptr && ScriptManager::Lock()) {
		ObjMap* map = NewMap();

		AddToMap(
			map, "button", (bind->Button != -1) ? INTEGER_VAL(bind->Button) : NULL_VAL);

		ScriptManager::Unlock();

		return map;
	}

	return nullptr;
}
static ObjMap* CreateControllerAxisActionMap(ControllerAxisBind* bind) {
	if (bind != nullptr && ScriptManager::Lock()) {
		ObjMap* map = NewMap();

		AddToMap(map, "axis", (bind->Axis != -1) ? INTEGER_VAL(bind->Axis) : NULL_VAL);
		AddToMap(map, "axis_deadzone", DECIMAL_VAL((float)bind->AxisDeadzone));
		AddToMap(map,
			"axis_digital_threshold",
			DECIMAL_VAL((float)bind->AxisDigitalThreshold));
		AddToMap(map, "axis_negative", INTEGER_VAL(bind->IsAxisNegative));

		ScriptManager::Unlock();

		return map;
	}

	return nullptr;
}
static ObjMap* CreateInputActionMap(InputBind* bind) {
	if (bind == nullptr) {
		return nullptr;
	}

	ObjMap* map = nullptr;

	switch (bind->Type) {
	case INPUT_BIND_KEYBOARD:
		map = CreateKeyboardActionMap(static_cast<KeyboardBind*>(bind));
		if (map != nullptr) {
			AddToMap(map, "type", OBJECT_VAL(CopyString("key")));
		}
		break;
	case INPUT_BIND_CONTROLLER_BUTTON:
		map = CreateControllerButtonActionMap(static_cast<ControllerButtonBind*>(bind));
		if (map != nullptr) {
			AddToMap(map, "type", OBJECT_VAL(CopyString("controller_button")));
		}
		break;
	case INPUT_BIND_CONTROLLER_AXIS:
		map = CreateControllerAxisActionMap(static_cast<ControllerAxisBind*>(bind));
		if (map != nullptr) {
			AddToMap(map, "type", OBJECT_VAL(CopyString("controller_axis")));
		}
		break;
	}

	return map;
}
/***
 * Input.GetActionBind
 * \desc Gets the bound input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to get.
 * \paramOpt bindIndex (integer): Which bind index to get.
 * \return map Returns a map value, or `null` if the input action is not bound.
 * \ns Input
 */
VMValue Input_GetActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int bindIndex = GET_ARG_OPT(2, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	InputBind* bind = InputManager::GetPlayerInputBind(playerID, actionID, bindIndex, false);
	if (bind != nullptr) {
		ObjMap* map = CreateInputActionMap(bind);
		if (map != nullptr) {
			return OBJECT_VAL(map);
		}
	}

	return NULL_VAL;
}
static VMValue SetActionBindFromArg(int playerID,
	int actionID,
	int bindIndex,
	int inputBindType,
	int argIndex,
	bool setDefault,
	VMValue* args,
	Uint32 threadID) {
	InputBind* bind = nullptr;

	switch (inputBindType) {
	case INPUT_BIND_KEYBOARD: {
		if (IS_INTEGER(args[argIndex])) {
			int key = GET_ARG(argIndex, GetInteger);
			CHECK_KEYBOARD_KEY(key);
			bind = new KeyboardBind(key);
		}
		else {
			ObjMap* map = GET_ARG(argIndex, GetMap);
			bind = GetKeyboardActionBind(map, threadID);
		}
		break;
	}
	case INPUT_BIND_CONTROLLER_BUTTON: {
		if (IS_INTEGER(args[argIndex])) {
			int button = GET_ARG(argIndex, GetInteger);
			CHECK_CONTROLLER_BUTTON(button);
			bind = new ControllerButtonBind(button);
		}
		else {
			ObjMap* map = GET_ARG(argIndex, GetMap);
			bind = GetControllerButtonActionBind(map, threadID);
		}
		break;
	}
	case INPUT_BIND_CONTROLLER_AXIS: {
		if (IS_INTEGER(args[argIndex])) {
			int axis = GET_ARG(argIndex, GetInteger);
			CHECK_CONTROLLER_AXIS(axis);
			bind = new ControllerAxisBind(axis);
		}
		else {
			ObjMap* map = GET_ARG(argIndex, GetMap);
			bind = GetControllerAxisActionBind(map, threadID);
		}
		break;
	}
	default:
		break;
	}

	if (bind == nullptr) {
		return NULL_VAL;
	}

	if (bindIndex < 0) {
		int idx = InputManager::AddPlayerInputBind(playerID, actionID, bind, setDefault);
		if (idx == -1) {
			delete bind;
			return NULL_VAL;
		}

		return INTEGER_VAL(idx);
	}
	else if (!InputManager::SetPlayerInputBind(playerID, actionID, bind, bindIndex, setDefault)) {
		delete bind;
	}

	return NULL_VAL;
}
/***
 * Input.SetActionBind
 * \desc Binds an input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to set.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (enum): The bind definition.
 * \paramOpt bindIndex (integer): Which bind index to set.
 * \ns Input
 */
/***
 * Input.SetActionBind
 * \desc Binds an input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to set.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (map): The bind definition.
 * \paramOpt bindIndex (integer): Which bind index to set.
 * \ns Input
 */
VMValue Input_SetActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int inputBindType = GET_ARG(2, GetInteger);
	int bindIndex = GET_ARG_OPT(3, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	CHECK_INPUT_BIND_TYPE(inputBindType);

	return SetActionBindFromArg(
		playerID, actionID, bindIndex, inputBindType, 3, false, args, threadID);
}
/***
 * Input.AddActionBind
 * \desc Adds an input action bind for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (enum): The bind definition.
 * \return integer Returns the index of the added input action as an integer value.
 * \ns Input
 */
/***
 * Input.AddActionBind
 * \desc Adds an input action bind for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (map): The bind definition.
 * \return integer Returns the index of the added input action as an integer value.
 * \ns Input
 */
VMValue Input_AddActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int inputBindType = GET_ARG(2, GetInteger);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	CHECK_INPUT_BIND_TYPE(inputBindType);

	return SetActionBindFromArg(
		playerID, actionID, -1, inputBindType, 3, false, args, threadID);
}
/***
 * Input.RemoveActionBind
 * \desc Removes a bound input action from a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to unbind.
 * \paramOpt bindIndex (integer): Which bind index to remove. If not passed, this removes all binds from the given action.
 * \ns Input
 */
VMValue Input_RemoveActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int bindIndex = GET_ARG_OPT(2, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	if (argCount >= 3) {
		InputManager::RemovePlayerInputBind(playerID, actionID, bindIndex, false);
	}
	else {
		InputManager::ClearPlayerBinds(playerID, actionID, false);
	}

	return NULL_VAL;
}
static ObjArray* GetBoundActionList(int playerID, int actionID, bool isDefault) {
	ObjArray* array = NewArray();

	size_t count = InputManager::GetPlayerInputBindCount(playerID, actionID, isDefault);

	for (size_t i = 0; i < count; i++) {
		InputBind* bind =
			InputManager::GetPlayerInputBind(playerID, actionID, i, isDefault);
		ObjMap* map = CreateInputActionMap(bind);
		if (map != nullptr) {
			array->Values->push_back(OBJECT_VAL(map));
		}
	}

	return array;
}
/***
 * Input.GetBoundActionList
 * \desc Gets a list of the input actions currently bound to a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to get.
 * \return array Returns an array of map values.
 * \ns Input
 */
VMValue Input_GetBoundActionList(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	return OBJECT_VAL(GetBoundActionList(playerID, actionID, false));
}
/***
 * Input.GetBoundActionCount
 * \desc Gets the amount of bound input actions for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action.
 * \return integer Returns an integer value.
 * \ns Input
 */
VMValue Input_GetBoundActionCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	return INTEGER_VAL((int)InputManager::GetPlayerInputBindCount(playerID, actionID, false));
}
/***
 * Input.GetBoundActionMap
 * \desc Gets a map of the input actions currently bound to a specific player.
 * \param playerID (integer): Index of the player.
 * \return map Returns a map value, or `null` if no actions are registered.
 * \ns Input
 */
VMValue Input_GetBoundActionMap(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);

	size_t count = InputManager::Actions.size();
	if (count == 0) {
		return NULL_VAL;
	}

	ObjMap* map = NewMap();

	for (size_t i = 0; i < count; i++) {
		InputAction& action = InputManager::Actions[i];
		AddToMap(map,
			action.Name.c_str(),
			OBJECT_VAL(GetBoundActionList(playerID, i, false)));
	}

	return OBJECT_VAL(map);
}
/***
 * Input.GetDefaultActionBind
 * \desc Gets the default bound input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to get.
 * \paramOpt bindIndex (integer): Which bind index to get.
 * \return map Returns a map value, or `null` if the input action is not bound.
 * \ns Input
 */
VMValue Input_GetDefaultActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int bindIndex = GET_ARG_OPT(2, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	InputBind* bind = InputManager::GetPlayerInputBind(playerID, actionID, bindIndex, true);
	if (bind != nullptr) {
		ObjMap* map = CreateInputActionMap(bind);
		if (map != nullptr) {
			return OBJECT_VAL(map);
		}
	}

	return NULL_VAL;
}
/***
 * Input.SetDefaultActionBind
 * \desc Binds a default input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to set.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (enum): The bind definition.
 * \paramOpt bindIndex (integer): Which bind index to set.
 * \ns Input
 */
/***
 * Input.SetDefaultActionBind
 * \desc Binds a default input action for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to set.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (map): The bind definition.
 * \paramOpt bindIndex (integer): Which bind index to set.
 * \ns Input
 */
VMValue Input_SetDefaultActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int inputBindType = GET_ARG(2, GetInteger);
	int bindIndex = GET_ARG_OPT(3, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	CHECK_INPUT_BIND_TYPE(inputBindType);

	return SetActionBindFromArg(
		playerID, actionID, bindIndex, inputBindType, 3, true, args, threadID);
}
/***
 * Input.AddDefaultActionBind
 * \desc Adds a default input action bind for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action.
 * \param inputBindType (<ref InputBind_*>): The input bind type.
 * \param actionBind (enum): The bind definition.
 * \return integer Returns the index of the added input action as an integer value.
 * \ns Input
 */
VMValue Input_AddDefaultActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int inputBindType = GET_ARG(2, GetInteger);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	CHECK_INPUT_BIND_TYPE(inputBindType);

	return SetActionBindFromArg(playerID, actionID, -1, inputBindType, 3, true, args, threadID);
}
/***
 * Input.RemoveDefaultActionBind
 * \desc Removes a bound input action default from a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to unbind.
 * \paramOpt bindIndex (integer): Which bind index to remove. If not passed, this removes all binds from the given action.
 * \ns Input
 */
VMValue Input_RemoveDefaultActionBind(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);
	int bindIndex = GET_ARG_OPT(2, GetInteger, 0);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	if (argCount >= 3) {
		InputManager::RemovePlayerInputBind(playerID, actionID, bindIndex, true);
	}
	else {
		InputManager::ClearPlayerBinds(playerID, actionID, true);
	}

	return NULL_VAL;
}
/***
 * Input.GetDefaultBoundActionList
 * \desc Gets a list of the input actions bound by default to a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action to get.
 * \return array Returns an array of map values.
 * \ns Input
 */
VMValue Input_GetDefaultBoundActionList(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	return OBJECT_VAL(GetBoundActionList(playerID, actionID, true));
}
/***
 * Input.GetDefaultBoundActionCount
 * \desc Gets the amount of bound default input actions for a specific player.
 * \param playerID (integer): Index of the player.
 * \param actionName (string): Name of the action.
 * \return integer Returns an integer value.
 * \ns Input
 */
VMValue Input_GetDefaultBoundActionCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	char* actionName = GET_ARG(1, GetString);

	CHECK_INPUT_PLAYER_INDEX(playerID);

	int actionID = InputManager::GetActionID(actionName);
	if (actionID == -1) {
		THROW_ERROR("Invalid input action \"%s\"!", actionName);
		return NULL_VAL;
	}

	return INTEGER_VAL((int)InputManager::GetPlayerInputBindCount(playerID, actionID, true));
}
/***
 * Input.GetDefaultBoundActionMap
 * \desc Gets a map of the input actions bound by default to a specific player.
 * \param playerID (integer): Index of the player.
 * \return map Returns a map value, or `null` if no actions are registered.
 * \ns Input
 */
VMValue Input_GetDefaultBoundActionMap(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);

	size_t count = InputManager::Actions.size();
	if (count == 0) {
		return NULL_VAL;
	}

	ObjMap* map = NewMap();

	for (size_t i = 0; i < count; i++) {
		InputAction& action = InputManager::Actions[i];
		AddToMap(map,
			action.Name.c_str(),
			OBJECT_VAL(GetBoundActionList(playerID, i, true)));
	}

	return OBJECT_VAL(map);
}
/***
 * Input.ResetActionBindsToDefaults
 * \desc Sets all input actions for a specific player to its defaults.
 * \param playerID (integer): Index of the player.
 * \ns Input
 */
VMValue Input_ResetActionBindsToDefaults(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	InputManager::ResetPlayerBinds(playerID);
	return NULL_VAL;
}
/***
 * Input.IsPlayerUsingDevice
 * \desc Checks if a given input device is being used by the player.
 * \param playerID (integer): Index of the player to check.
 * \param inputDevice (<ref InputDevice_*>): Which input device to check.
 * \return boolean Returns a boolean value.
 * \ns Input
 */
VMValue Input_IsPlayerUsingDevice(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	int inputDevice = GET_ARG(1, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	CHECK_INPUT_DEVICE(inputDevice);
	return INTEGER_VAL(!!InputManager::IsPlayerUsingDevice(playerID, inputDevice));
}
/***
 * Input.GetPlayerControllerIndex
 * \desc Gets the controller index assigned to a specific player.
 * \param playerID (integer): Index of the player to check.
 * \return index Returns the index of the controller, or `-1` if there is no controller assigned.
 * \ns Input
 */
VMValue Input_GetPlayerControllerIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int playerID = GET_ARG(0, GetInteger);
	CHECK_INPUT_PLAYER_INDEX(playerID);
	return INTEGER_VAL(InputManager::GetPlayerControllerIndex(playerID));
}
/***
 * Input.SetPlayerControllerIndex
 * \desc Assigns a controller index to a specific player.
 * \param playerID (integer): Index of the player.
 * \param controllerID (integer): Index of the controller to assign, or `null` to unassign.
 * \ns Input
 */
VMValue Input_SetPlayerControllerIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int playerID = GET_ARG(0, GetInteger);
	int controllerID = -1;
	if (!IS_NULL(args[1])) {
		controllerID = GET_ARG(1, GetInteger);
	}
	CHECK_INPUT_PLAYER_INDEX(playerID);
	InputManager::SetPlayerControllerIndex(playerID, controllerID);
	return NULL_VAL;
}
#undef CHECK_INPUT_PLAYER_INDEX
#undef CHECK_INPUT_DEVICE
// #endregion

// #region Instance
/***
 * Instance.Create
 * \desc Creates a new instance of an object class, and calls its `Create` event with the flag.
 * \param className (string): Name of the object class.
 * \param x (number): X position of where to place the new instance.
 * \param y (number): Y position of where to place the new instance.
 * \paramOpt flag (value): Value to pass to the `Create` event. (default: `0`)
 * \return Entity Returns the new instance.
 * \ns Instance
 */
VMValue Instance_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);

	char* objectName = GET_ARG(0, GetString);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	VMValue flag = argCount == 4 ? args[3] : INTEGER_VAL(0);

	ObjectList* objectList = Scene::GetObjectList(objectName);
	if (!objectList || !objectList->SpawnFunction) {
		THROW_ERROR("Object class \"%s\" does not exist.", objectName);
		return NULL_VAL;
	}

	ScriptEntity* obj = (ScriptEntity*)objectList->Spawn();
	if (!obj) {
		THROW_ERROR("Could not spawn object of class \"%s\"!", objectName);
		return NULL_VAL;
	}

	obj->X = x;
	obj->Y = y;
	obj->InitialX = x;
	obj->InitialY = y;
	obj->List = objectList;
	obj->List->Add(obj);

	ObjEntity* instance = obj->Instance;

	// Call the initializer, if there is one.
	if (HasInitializer(instance->Object.Class)) {
		obj->Initialize();
	}

	// Add it to the scene
	Scene::AddDynamic(objectList, obj);

	obj->Create(flag);
	if (!Scene::Initializing) {
		obj->PostCreate();
	}

	return OBJECT_VAL(instance);
}
/***
 * Instance.GetNth
 * \desc Gets the n'th instance of an object class.
 * \param className (string): Name of the object class.
 * \param n (integer): n'th of object class' instances to get. `0` is first.
 * \return Entity Returns n'th of object class' instances, `null` if instance cannot be found or class does not exist.
 * \ns Instance
 */
VMValue Instance_GetNth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* objectName = GET_ARG(0, GetString);
	int n = GET_ARG(1, GetInteger);

	if (!Scene::ObjectLists->Exists(objectName)) {
		return NULL_VAL;
	}

	ObjectList* objectList = Scene::ObjectLists->Get(objectName);
	ScriptEntity* object = (ScriptEntity*)objectList->GetNth(n);

	if (object) {
		return OBJECT_VAL(object->Instance);
	}

	return NULL_VAL;
}
/***
 * Instance.IsClass
 * \desc Determines whether the instance is of a specified object class.
 * \param entity (Entity): The instance to check. If there is no instance, this automatically returns false.
 * \param className (string): Name of the object class.
 * \return boolean Returns whether the instance is of a specified object class.
 * \ns Instance
 */
VMValue Instance_IsClass(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	if (IS_NULL(args[0])) {
		return INTEGER_VAL(false);
	}

	ObjEntity* instance = GET_ARG(0, GetEntity);
	char* objectName = GET_ARG(1, GetString);

	Entity* self = (Entity*)instance->EntityPtr;
	if (!self) {
		return INTEGER_VAL(false);
	}

	if (!Scene::ObjectLists->Exists(objectName)) {
		return INTEGER_VAL(false);
	}

	ObjectList* objectList = Scene::ObjectLists->Get(objectName);
	if (self->List == objectList) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * Instance.GetClass
 * \desc Gets the object class of a instance.
 * \param entity (Entity): The instance to check.
 * \return string Returns a string value.
 * \ns Instance
 */
VMValue Instance_GetClass(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	ObjEntity* instance = GET_ARG(0, GetEntity);

	Entity* self = (Entity*)instance->EntityPtr;
	if (!self || !self->List) {
		return NULL_VAL;
	}

	return ReturnString(self->List->ObjectName);
}
/***
 * Instance.GetCount
 * \desc Gets amount of currently active instances in an object class.
 * \param className (string): Name of the object class.
 * \return integer Returns count of currently active instances in an object class.
 * \ns Instance
 */
VMValue Instance_GetCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	char* objectName = GET_ARG(0, GetString);

	if (!Scene::ObjectLists->Exists(objectName)) {
		return INTEGER_VAL(0);
	}

	ObjectList* objectList = Scene::ObjectLists->Get(objectName);
	return INTEGER_VAL(objectList->Count());
}
/***
 * Instance.GetNextInstance
 * \desc Gets the instance created after or before the specified instance. `0` is the next instance, `-1` is the previous instance.
 * \param entity (Entity): The instance to check.
 * \param n (integer): How many instances after or before the desired instance is to the checking instance.
 * \return Entity Returns the desired instance.
 * \ns Instance
 */
VMValue Instance_GetNextInstance(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	ObjEntity* instance = GET_ARG(0, GetEntity);
	Entity* self = (Entity*)instance->EntityPtr;
	int n = GET_ARG(1, GetInteger);

	if (!self) {
		return NULL_VAL;
	}

	Entity* object = self;
	if (n < 0) {
		for (int i = 0; i < -n; i++) {
			object = object->PrevSceneEntity;
			if (!object) {
				return NULL_VAL;
			}
		}
	}
	else {
		for (int i = 0; i <= n; i++) {
			object = object->NextSceneEntity;
			if (!object) {
				return NULL_VAL;
			}
		}
	}

	if (object) {
		return OBJECT_VAL(((ScriptEntity*)object)->Instance);
	}

	return NULL_VAL;
}
/***
 * Instance.GetBySlotID
 * \desc Gets an instance by its slot ID.
 * \param slotID (integer): The slot ID to search a corresponding instance for.
 * \return Entity Returns the instance corresponding to the specified slot ID, or `null` if no instance was found.
 * \ns Instance
 */
VMValue Instance_GetBySlotID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	int slotID = GET_ARG(0, GetInteger);
	if (slotID < 0) {
		return NULL_VAL;
	}

	// Search backwards
	for (Entity* ent = Scene::ObjectLast; ent; ent = ent->PrevSceneEntity) {
		if (ent->SlotID == slotID) {
			return OBJECT_VAL(((ScriptEntity*)ent)->Instance);
		}
	}

	return NULL_VAL;
}
/***
 * Instance.DisableAutoAnimate
 * \desc Disables the AutoAnimate function of entities.
 * \param disableAutoAnimate (boolean): Whether to turn off the engine automatically applying AutoAnimate when entities are initialized.
 * \ns Instance
 */
VMValue Instance_DisableAutoAnimate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ScriptEntity::DisableAutoAnimate = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Instance.SetUseRenderRegions
 * \desc Sets whether entities will use Render Regions when rendering. If false, entities will use their Update Regions instead.
 * \param useRenderRegions (boolean): Whether render regions will be used.
 * \ns Instance
 */
VMValue Instance_SetUseRenderRegions(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Scene::UseRenderRegions = !!GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Instance.Copy
 * \desc Copies an instance into another.
 * \param destInstance (Entity): The destination instance.
 * \param srcInstance (Entity): The source instance.
 * \paramOpt copyClass (boolean): Whether to copy the class of the source entity. (default: `true`)
 * \ns Instance
 */
VMValue Instance_Copy(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	ObjEntity* destInstance = GET_ARG(0, GetEntity);
	ObjEntity* srcInstance = GET_ARG(1, GetEntity);
	bool copyClass = !!GET_ARG_OPT(2, GetInteger, true);

	ScriptEntity* destEntity = (ScriptEntity*)destInstance->EntityPtr;
	ScriptEntity* srcEntity = (ScriptEntity*)srcInstance->EntityPtr;
	if (destEntity && srcEntity) {
		srcEntity->Copy(destEntity, copyClass);
	}

	return NULL_VAL;
}
/***
 * Instance.ChangeClass
 * \desc Changes an instance's class.
 * \param entity (Entity): The instance to swap.
 * \param className (string): Name of the object class.
 * \return boolean Returns whether the instance was swapped.
 * \ns Instance
 */
VMValue Instance_ChangeClass(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	ObjEntity* instance = GET_ARG(0, GetEntity);
	char* className = GET_ARG(1, GetString);

	ScriptEntity* self = (ScriptEntity*)instance->EntityPtr;
	if (!self) {
		return INTEGER_VAL(false);
	}

	if (self->ChangeClass(className)) {
		self->Instance->InstanceObj.Fields->Clear();
		self->LinkFields();
		self->Initialize();
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
// #endregion

// #region JSON
static int JSON_FillMap(ObjMap*, const char*, jsmntok_t*, size_t);
static int JSON_FillArray(ObjArray*, const char*, jsmntok_t*, size_t);

static int JSON_FillMap(ObjMap* map, const char* text, jsmntok_t* t, size_t count) {
	jsmntok_t* key;
	jsmntok_t* value;
	if (count == 0) {
		return 0;
	}

	Uint32 keyHash;
	int tokcount = 0;
	for (int i = 0; i < t->size; i++) {
		key = t + 1 + tokcount;
		keyHash = map->Keys->HashFunction(text + key->start, key->end - key->start);
		map->Keys->Put(
			keyHash, StringUtils::Duplicate(text + key->start, key->end - key->start));
		tokcount += 1;
		if (key->size > 0) {
			VMValue val = NULL_VAL;
			value = t + 1 + tokcount;
			switch (value->type) {
			case JSMN_PRIMITIVE:
				tokcount += 1;
				if (memcmp("true",
					    text + value->start,
					    value->end - value->start) == 0) {
					val = INTEGER_VAL(true);
				}
				else if (memcmp("false",
						 text + value->start,
						 value->end - value->start) == 0) {
					val = INTEGER_VAL(false);
				}
				else if (memcmp("null",
						 text + value->start,
						 value->end - value->start) == 0) {
					val = NULL_VAL;
				}
				else {
					bool isNumeric = true;
					bool hasDot = false;
					for (const char *cStart = text + value->start, *c = cStart;
						c < text + value->end;
						c++) {
						isNumeric &= (c == cStart && *cStart == '-') ||
							(*c >= '0' && *c <= '9') ||
							(isNumeric && *c == '.' &&
								c > text + value->start && !hasDot);
						hasDot |= (*c == '.');
					}
					if (isNumeric) {
						if (hasDot) {
							val = DECIMAL_VAL((float)strtod(
								text + value->start, NULL));
						}
						else {
							val = INTEGER_VAL((int)strtol(
								text + value->start, NULL, 10));
						}
					}
					else {
						val = OBJECT_VAL(CopyString(text + value->start,
							value->end - value->start));
					}
				}
				break;
			case JSMN_STRING: {
				tokcount += 1;
				val = OBJECT_VAL(
					CopyString(text + value->start, value->end - value->start));

				char* o = AS_CSTRING(val);
				for (const char* l = text + value->start; l < text + value->end;
					l++) {
					if (*l == '\\') {
						l++;
						switch (*l) {
						case '\"':
							*o++ = '\"';
							break;
						case '/':
							*o++ = '/';
							break;
						case '\\':
							*o++ = '\\';
							break;
						case 'b':
							*o++ = '\b';
							break;
						case 'f':
							*o++ = '\f';
							break;
						case 'r': // *o++ = '\r';
							break;
						case 'n':
							*o++ = '\n';
							break;
						case 't':
							*o++ = '\t';
							break;
						case 'u':
							l++;
							l++;
							l++;
							l++;
							*o++ = '\t';
							break;
						}
					}
					else {
						*o++ = *l;
					}
				}
				*o = 0;
				break;
			}
			// /*
			case JSMN_OBJECT: {
				ObjMap* subMap = NewMap();
				tokcount += JSON_FillMap(subMap, text, value, count - tokcount);
				val = OBJECT_VAL(subMap);
				break;
			}
			//*/
			case JSMN_ARRAY: {
				ObjArray* subArray = NewArray();
				tokcount += JSON_FillArray(subArray, text, value, count - tokcount);
				val = OBJECT_VAL(subArray);
				break;
			}
			default:
				break;
			}
			map->Values->Put(keyHash, val);
		}
	}
	return tokcount + 1;
}
static int JSON_FillArray(ObjArray* arr, const char* text, jsmntok_t* t, size_t count) {
	jsmntok_t* value;
	if (count == 0) {
		return 0;
	}

	int tokcount = 0;
	for (int i = 0; i < t->size; i++) {
		VMValue val = NULL_VAL;
		value = t + 1 + tokcount;
		switch (value->type) {
		// /*
		case JSMN_PRIMITIVE:
			tokcount += 1;
			if (memcmp("true", text + value->start, value->end - value->start) == 0) {
				val = INTEGER_VAL(true);
			}
			else if (memcmp("false", text + value->start, value->end - value->start) ==
				0) {
				val = INTEGER_VAL(false);
			}
			else if (memcmp("null", text + value->start, value->end - value->start) ==
				0) {
				val = NULL_VAL;
			}
			else {
				bool isNumeric = true;
				bool hasDot = false;
				for (const char *cStart = text + value->start, *c = cStart;
					c < text + value->end;
					c++) {
					isNumeric &= (c == cStart && *cStart == '-') ||
						(*c >= '0' && *c <= '9') ||
						(isNumeric && *c == '.' &&
							c > text + value->start && !hasDot);
					hasDot |= (*c == '.');
				}
				if (isNumeric) {
					if (hasDot) {
						val = DECIMAL_VAL(
							(float)strtod(text + value->start, NULL));
					}
					else {
						val = INTEGER_VAL(
							(int)strtol(text + value->start, NULL, 10));
					}
				}
				else {
					val = OBJECT_VAL(CopyString(
						text + value->start, value->end - value->start));
				}
			}
			break;
		case JSMN_STRING: {
			tokcount += 1;
			val = OBJECT_VAL(
				CopyString(text + value->start, value->end - value->start));

			char* o = AS_CSTRING(val);
			for (const char* l = text + value->start; l < text + value->end; l++) {
				if (*l == '\\') {
					l++;
					switch (*l) {
					case '\"':
						*o++ = '\"';
						break;
					case '/':
						*o++ = '/';
						break;
					case '\\':
						*o++ = '\\';
						break;
					case 'b':
						*o++ = '\b';
						break;
					case 'f':
						*o++ = '\f';
						break;
					case 'r': // *o++ = '\r';
						break;
					case 'n':
						*o++ = '\n';
						break;
					case 't':
						*o++ = '\t';
						break;
					case 'u':
						l++;
						l++;
						l++;
						l++;
						*o++ = '\t';
						break;
					}
				}
				else {
					*o++ = *l;
				}
			}
			*o = 0;
			break;
		}
		case JSMN_OBJECT: {
			ObjMap* subMap = NewMap();
			tokcount += JSON_FillMap(subMap, text, value, count - tokcount);
			val = OBJECT_VAL(subMap);
			break;
		}
		case JSMN_ARRAY: {
			ObjArray* subArray = NewArray();
			tokcount += JSON_FillArray(subArray, text, value, count - tokcount);
			val = OBJECT_VAL(subArray);
			break;
		}
		default:
			break;
		}
		arr->Values->push_back(val);
	}
	return tokcount + 1;
}

/***
 * JSON.Parse
 * \desc Decodes a string value into a map value.
 * \param jsonText (string): JSON-compliant text.
 * \return map Returns a map value if the text can be decoded, otherwise returns `null`.
 * \ns JSON
 */
VMValue JSON_Parse(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjString* string = GET_ARG(0, GetVMString);
	if (!string) return NULL_VAL;

	if (ScriptManager::Lock()) {
		ObjMap* map = NewMap();

		jsmn_parser p;
		jsmntok_t* tok;
		size_t tokcount = 16;
		tok = (jsmntok_t*)malloc(sizeof(*tok) * tokcount);
		if (tok == NULL) {
			return NULL_VAL;
		}

		jsmn_init(&p);
		while (true) {
			int r = jsmn_parse(&p, string->Chars, string->Length, tok, (Uint32)tokcount);
			if (r < 0) {
				if (r == JSMN_ERROR_NOMEM) {
					tokcount = tokcount * 2;
					jsmntok_t* tempTok = (jsmntok_t*)realloc(tok, sizeof(*tok) * tokcount);
					if (tempTok == NULL) {
						free(tok);
						ScriptManager::Unlock();
						return NULL_VAL;
					}
					tok = tempTok;
					continue;
				}
			}
			else {
				JSON_FillMap(map, string->Chars, tok, p.toknext);
			}
			break;
		}
		free(tok);

		ScriptManager::Unlock();
		return OBJECT_VAL(map);
	}
	return NULL_VAL;
}
/***
 * JSON.ToString
 * \desc Converts a value into a JSON string.
 * \param json (value): The value to convert.
 * \paramOpt prettyPrint (boolean): Whether to use spacing and newlines in the text.
 * \return string Returns a JSON string based on the value.
 * \ns JSON
 */
VMValue JSON_ToString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	VMValue value = args[0];
	bool prettyPrint = !!GET_ARG_OPT(1, GetInteger, false);

	char* buffer = (char*)malloc(512);
	PrintBuffer buffer_info;
	buffer_info.Buffer = &buffer;
	buffer_info.WriteIndex = 0;
	buffer_info.BufferSize = 512;
	ValuePrinter::Print(&buffer_info, value, prettyPrint, true);
	value = OBJECT_VAL(CopyString(buffer, buffer_info.WriteIndex));
	free(buffer);
	return value;
}
// #endregion

// #region Math
/***
 * Math.Cos
 * \desc Returns the cosine of an angle of x radians.
 * \param x (decimal): Angle (in radians) to get the cosine of.
 * \return decimal The cosine of x radians.
 * \ns Math
 */
VMValue Math_Cos(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Cos(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Sin
 * \desc Returns the sine of an angle of x radians.
 * \param x (decimal): Angle (in radians) to get the sine of.
 * \return decimal The sine of x radians.
 * \ns Math
 */
VMValue Math_Sin(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Sin(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Tan
 * \desc Returns the tangent of an angle of x radians.
 * \param x (decimal): Angle (in radians) to get the tangent of.
 * \return decimal The tangent of x radians.
 * \ns Math
 */
VMValue Math_Tan(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Tan(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Acos
 * \desc Returns the arccosine of x.
 * \param x (decimal): Number value to get the arccosine of.
 * \return decimal Returns the angle (in radians) as a decimal value.
 * \ns Math
 */
VMValue Math_Acos(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Acos(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Asin
 * \desc Returns the arcsine of x.
 * \param x (decimal): Number value to get the arcsine of.
 * \return decimal Returns the angle (in radians) as a decimal value.
 * \ns Math
 */
VMValue Math_Asin(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Asin(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Atan
 * \desc Returns the arctangent angle (in radians) from x and y.
 * \param x (decimal): x value.
 * \param y (decimal): y value.
 * \return decimal The angle from x and y.
 * \ns Math
 */
VMValue Math_Atan(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return DECIMAL_VAL(Math::Atan(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Distance
 * \desc Gets the distance from (x1,y1) to (x2,y2) in pixels.
 * \param x1 (number): X position of first point.
 * \param y1 (number): Y position of first point.
 * \param x2 (number): X position of second point.
 * \param y2 (number): Y position of second point.
 * \return decimal Returns the distance from (x1,y1) to (x2,y2) as a decimal value.
 * \ns Math
 */
VMValue Math_Distance(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	return DECIMAL_VAL(Math::Distance(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal)));
}
/***
 * Math.Direction
 * \desc Gets the angle from (x1,y1) to (x2,y2) in radians.
 * \param x1 (number): X position of first point.
 * \param y1 (number): Y position of first point.
 * \param x2 (number): X position of second point.
 * \param y2 (number): Y position of second point.
 * \return decimal Returns the angle from (x1,y1) to (x2,y2) as a decimal value.
 * \ns Math
 */
VMValue Math_Direction(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	return DECIMAL_VAL(Math::Atan(GET_ARG(2, GetDecimal) - GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal) - GET_ARG(3, GetDecimal)));
}
/***
 * Math.Abs
 * \desc Gets the absolute value of a Number.
 * \param n (number): Number value.
 * \return number Returns the absolute value of n.
 * \ns Math
 */
VMValue Math_Abs(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return IS_INTEGER(args[0]) ? INTEGER_VAL((int)Math::Abs(GET_ARG(0, GetDecimal)))
				   : DECIMAL_VAL(Math::Abs(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Min
 * \desc Gets the lesser value of two Number values.
 * \param a (number): Number value.
 * \param b (number): Number value.
 * \return number Returns the lesser value of a and b.
 * \ns Math
 */
VMValue Math_Min(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	if (IS_INTEGER(args[0]) && IS_INTEGER(args[1])) {
		return INTEGER_VAL((int)Math::Min(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
	}
	else {
		return DECIMAL_VAL(Math::Min(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
	}
}
/***
 * Math.Max
 * \desc Gets the greater value of two Number values.
 * \param a (number): Number value.
 * \param b (number): Number value.
 * \return number Returns the greater value of a and b.
 * \ns Math
 */
VMValue Math_Max(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	if (IS_INTEGER(args[0]) && IS_INTEGER(args[1])) {
		return INTEGER_VAL((int)Math::Max(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
	}
	else {
		return DECIMAL_VAL(Math::Max(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
	}
}
/***
 * Math.Clamp
 * \desc Gets the value clamped between a range.
 * \param n (number): Number value.
 * \param minValue (number): Minimum range value to clamp to.
 * \param maxValue (number): Maximum range value to clamp to.
 * \return number Returns the Number value if within the range, otherwise returns closest range value.
 * \ns Math
 */
VMValue Math_Clamp(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	if (IS_INTEGER(args[0]) && IS_INTEGER(args[1]) && IS_INTEGER(args[2])) {
		return INTEGER_VAL((int)Math::Clamp(
			GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal)));
	}
	else {
		return DECIMAL_VAL(Math::Clamp(
			GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal)));
	}
}
/***
 * Math.Sign
 * \desc Gets the sign associated with a decimal value.
 * \param n (number): Number value.
 * \return decimal Returns `-1` if <param n> is negative, `1` if positive, and `0` if otherwise.
 * \ns Math
 */
VMValue Math_Sign(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::Sign(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Uint8
 * \desc Converts an integer to an 8-bit unsigned value.
 * \param n (integer): Integer value to convert.
 * \return integer Returns the converted value.
 * \ns Math
 */
VMValue Math_Uint8(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)(Uint8)GET_ARG(0, GetInteger));
}
/***
 * Math.Uint16
 * \desc Converts an integer to a 16-bit unsigned value.
 * \param n (integer): Integer value to convert.
 * \return integer Returns the converted value.
 * \ns Math
 */
VMValue Math_Uint16(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)(Uint16)GET_ARG(0, GetInteger));
}
/***
 * Math.Uint32
 * \desc Converts an integer to a 32-bit unsigned value.
 * \param n (integer): Integer value to convert.
 * \return integer Returns the converted value.
 * \ns Math
 */
VMValue Math_Uint32(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)(Uint32)GET_ARG(0, GetInteger));
}
/***
 * Math.Uint64
 * \desc Converts an integer to a 64-bit unsigned value.
 * \param n (integer): Integer value to convert.
 * \return integer Returns the converted value.
 * \ns Math
 */
VMValue Math_Uint64(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)(Uint64)GET_ARG(0, GetInteger));
}
/***
 * Math.Random
 * \desc Get a random number between 0.0 and 1.0.
 * \return decimal Returns the random number.
 * \ns Math
 */
VMValue Math_Random(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return DECIMAL_VAL(Math::Random());
}
/***
 * Math.RandomMax
 * \desc Gets a random number between 0.0 and a specified maximum.
 * \param max (number): Maximum non-inclusive value.
 * \return decimal Returns the random number.
 * \ns Math
 */
VMValue Math_RandomMax(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Math::RandomMax(GET_ARG(0, GetDecimal)));
}
/***
 * Math.RandomRange
 * \desc Gets a random number between specified minimum and a specified maximum.
 * \param min (number): Minimum non-inclusive value.
 * \param max (number): Maximum non-inclusive value.
 * \return decimal Returns the random number.
 * \ns Math
 */
VMValue Math_RandomRange(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return DECIMAL_VAL(Math::RandomRange(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * RSDK.Math.GetRandSeed
 * \desc Gets the engine's random seed value.
 * \return integer Returns an integer of the engine's random seed value.
 * \ns RSDK.Math
 */
VMValue Math_GetRandSeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Math::RSDK_GetRandSeed());
}
/***
 * RSDK.Math.SetRandSeed
 * \desc Sets the engine's random seed value.
 * \param key (integer): Value to set the seed to.
 * \ns RSDK.Math
 */
VMValue Math_SetRandSeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Math::RSDK_SetRandSeed(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * RSDK.Math.RandomInteger
 * \desc Gets a random number between specified minimum integer and a specified maximum integer.
 * \param min (integer): Minimum non-inclusive integer value.
 * \param max (integer): Maximum non-inclusive integer value.
 * \return integer Returns the random number as an integer.
 * \ns RSDK.Math
 */
VMValue Math_RandomInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return INTEGER_VAL(
		Math::RSDK_RandomInteger(GET_ARG(0, GetInteger), GET_ARG(1, GetInteger)));
}
/***
 * RSDK.Math.RandomIntegerSeeded
 * \desc Gets a random number between specified minimum integer and a specified maximum integer based off of a given seed.
 * \param min (integer): Minimum non-inclusive integer value.
 * \param max (integer): Maximum non-inclusive integer value.
 * \paramOpt seed (integer): Seed of which to base the number.
 * \return integer Returns the random number.
 * \ns RSDK.Math
 */
VMValue Math_RandomIntegerSeeded(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	if (argCount < 3) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(Math::RSDK_RandomIntegerSeeded(
		GET_ARG(0, GetInteger), GET_ARG(1, GetInteger), GET_ARG(2, GetInteger)));
}
/***
 * Math.Floor
 * \desc Rounds the number n downward, returning the largest integral value that is not greater than n.
 * \param n (number): Number to be rounded.
 * \return integer Returns the floored number value.
 * \ns Math
 */
VMValue Math_Floor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(std::floor(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Ceil
 * \desc Rounds the number n upward, returning the smallest integral value that is not less than n.
 * \param n (number): Number to be rounded.
 * \return decimal Returns the ceiling-ed number value.
 * \ns Math
 */
VMValue Math_Ceil(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(std::ceil(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Round
 * \desc Rounds the number n.
 * \param n (number): Number to be rounded.
 * \return decimal Returns the rounded number value.
 * \ns Math
 */
VMValue Math_Round(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(std::round(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Sqrt
 * \desc Retrieves the square root of the number n.
 * \param n (number): Number to be square rooted.
 * \return decimal Returns the square root of the number n.
 * \ns Math
 */
VMValue Math_Sqrt(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(sqrt(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Pow
 * \desc Retrieves the number n to the power of p.
 * \param n (number): Number for the base of the exponent.
 * \param p (number): Exponent.
 * \return decimal Returns the number n to the power of p.
 * \ns Math
 */
VMValue Math_Pow(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return DECIMAL_VAL(pow(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Exp
 * \desc Retrieves the constant e (2.717) to the power of p.
 * \param p (number): Exponent.
 * \return decimal Returns the result number.
 * \ns Math
 */
VMValue Math_Exp(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(std::exp(GET_ARG(0, GetDecimal)));
}
// #endregion

// #region RSDK.Math
/***
 * RSDK.Math.ClearTrigLookupTables
 * \desc Clears the engine's angle lookup tables.
 * \ns RSDK.Math
 */
VMValue Math_ClearTrigLookupTables(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Math::ClearTrigLookupTables();
	return NULL_VAL;
}
/***
 * RSDK.Math.CalculateTrigAngles
 * \desc Sets the engine's angle lookup tables.
 * \ns RSDK.Math
 */
VMValue Math_CalculateTrigAngles(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Math::CalculateTrigAngles();
	return NULL_VAL;
}
/***
 * RSDK.Math.Sin1024
 * \desc Returns the sine of an angle of x based on a max of 1024.
 * \param angle (integer): Angle to get the sine of.
 * \return integer The sine 1024 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Sin1024(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Sin1024(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Cos1024
 * \desc Returns the cosine of an angle of x based on a max of 1024.
 * \param angle (integer): Angle to get the cosine of.
 * \return integer The cosine 1024 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Cos1024(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Cos1024(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Tan1024
 * \desc Returns the tangent of an angle of x based on a max of 1024.
 * \param angle (integer): Angle to get the tangent of.
 * \return integer The tangent 1024 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Tan1024(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Tan1024(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ASin1024
 * \desc Returns the arc sine of an angle of x based on a max of 1024.
 * \param angle (integer): Angle to get the arc sine of.
 * \return integer The arc sine 1024 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ASin1024(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ASin1024(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ACos1024
 * \desc Returns the arc cosine of an angle of x based on a max of 1024.
 * \param angle (integer): Angle to get the arc cosine of.
 * \return integer The arc cosine 1024 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ACos1024(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ACos1024(GET_ARG(0, GetInteger)));
}
/**
 * RSDK.Math.Sin512
 * \desc Returns the sine of an angle of x based on a max of 512.
 * \param angle (integer): Angle to get the sine of.
 * \return integer The sine 512 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Sin512(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Sin512(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Cos512
 * \desc Returns the cosine of an angle of x based on a max of 512.
 * \param angle (integer): Angle to get the cosine of.
 * \return integer The cosine 512 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Cos512(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Cos512(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Tan512
 * \desc Returns the tangent of an angle of x based on a max of 512.
 * \param angle (integer): Angle to get the tangent of.
 * \return integer The tangent 512 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Tan512(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Tan512(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ASin512
 * \desc Returns the arc sine of an angle of x based on a max of 512.
 * \param angle (integer): Angle to get the arc sine of.
 * \return integer The arc sine 512 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ASin512(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ASin512(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ACos512
 * \desc Returns the arc cosine of an angle of x based on a max of 512.
 * \param angle (integer): Angle to get the arc cosine of.
 * \return integer The arc cosine 512 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ACos512(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ACos512(GET_ARG(0, GetInteger)));
}
/**
 * RSDK.Math.Sin256
 * \desc Returns the sine of an angle of x based on a max of 256.
 * \param angle (integer): Angle to get the sine of.
 * \return integer The sine 256 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Sin256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Sin256(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Cos256
 * \desc Returns the cosine of an angle of x based on a max of 256.
 * \param angle (integer): Angle to get the cosine of.
 * \return integer The cosine 256 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Cos256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Cos256(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.Tan256
 * \desc Returns the tangent of an angle of x based on a max of 256.
 * \param angle (integer): Angle to get the tangent of.
 * \return integer The tangent 256 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_Tan256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::Tan256(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ASin256
 * \desc Returns the arc sine of an angle of x based on a max of 256.
 * \param angle (integer): Angle to get the arc sine of.
 * \return integer The arc sine 256 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ASin256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ASin256(GET_ARG(0, GetInteger)));
}
/***
 * RSDK.Math.ACos256
 * \desc Returns the arc cosine of an angle of x based on a max of 256.
 * \param angle (integer): Angle to get the arc cosine of.
 * \return integer The arc cosine 256 of the angle.
 * \ns RSDK.Math
 */
VMValue Math_ACos256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Math::ACos256(GET_ARG(0, GetInteger)));
}
/***
  * RSDK.Math.ATan2
  * \desc Returns the arc tangent of a position.
  * \param x (decimal): X value of the position.
  * \param y (decimal): Y value of the position.
  * \return integer The arc tangent of the position.
  * \ns RSDK.Math
  */
VMValue Math_ATan2(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return INTEGER_VAL((int)Math::ArcTanLookup((int)(GET_ARG(0, GetDecimal) * 65536.0f),
		(int)(GET_ARG(1, GetDecimal) * 65536.0f)));
}
/***
 * RSDK.Math.RadianToInteger
 * \desc Gets the integer conversion of a radian, based on 256.
 * \param radian (decimal): Radian value to convert.
 * \return integer An integer value of the converted radian.
 * \ns RSDK.Math
 */
VMValue Math_RadianToInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)((float)GET_ARG(0, GetDecimal) * 256.0 / M_PI));
}
/***
 * RSDK.Math.IntegerToRadian
 * \desc Gets the radian decimal conversion of an integer, based on 256.
 * \param integer (integer): Integer value to convert.
 * \return decimal A radian decimal value of the converted integer.
 * \ns RSDK.Math
 */
VMValue Math_IntegerToRadian(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL((float)(GET_ARG(0, GetInteger) * M_PI / 256.0));
}
/***
 * RSDK.Math.ToFixed
 * \desc Converts a decimal number to its fixed-point equivalent.
 * \param n (number): Number value.
 * \return integer Returns the converted fixed-point Number value.
 * \ns RSDK.Math
 */
VMValue Math_ToFixed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)(GET_ARG(0, GetDecimal) * 65536.0f));
}
/***
 * RSDK.Math.FromFixed
 * \desc Converts a fixed-point number to its decimal equivalent.
 * \param n (number): Number value.
 * \return decimal Returns the converted decimal Number value.
 * \ns RSDK.Math
 */
VMValue Math_FromFixed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL((float)GET_ARG(0, GetInteger) / 65536.0f);
}
// #endregion

// #region Matrix
/***
 * Matrix.Create
 * \desc Creates a 4x4 matrix and sets it to the identity. <br/>\
"The model, view and projection matrices are three separate matrices. <br/>\
Model maps from an object's local coordinate space into world space, <br/>\
view from world space to view space, projection from camera to screen.<br/>\
<br/>\
If you compose all three, you can use the one result to map all the way from <br/>\
object space to screen space, making you able to work out what you need to <br/>\
pass on to the next stage of a programmable pipeline from the incoming <br/>\
vertex positions." - Tommy (https://stackoverflow.com/questions/5550620/the-purpose-of-model-view-projection-matrix)
 * \return array Returns the matrix as an array.
 * \ns Matrix
 */
VMValue Matrix_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	ObjArray* array = NewArray();
	for (int i = 0; i < 16; i++) {
		array->Values->push_back(DECIMAL_VAL(0.0));
	}

	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	helper[0][0] = 1.0f;
	helper[1][1] = 1.0f;
	helper[2][2] = 1.0f;
	helper[3][3] = 1.0f;
	MatrixHelper_CopyTo(&helper, array);

	return OBJECT_VAL(array);
}
/***
 * Matrix.Identity
 * \desc Sets the matrix to the identity.
 * \param matrix (matrix): The matrix to set to the identity.
 * \ns Matrix
 */
VMValue Matrix_Identity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjArray* array = GET_ARG(0, GetArray);

	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	helper[0][0] = 1.0f;
	helper[1][1] = 1.0f;
	helper[2][2] = 1.0f;
	helper[3][3] = 1.0f;
	MatrixHelper_CopyTo(&helper, array);

	return NULL_VAL;
}
/***
 * Matrix.Perspective
 * \desc Creates a perspective projection matrix.
 * \param matrix (matrix): The matrix to generate the projection matrix into.
 * \param fov (number): The field of view, in degrees.
 * \param near (number): The near clipping plane value.
 * \param far (number): The far clipping plane value.
 * \param aspect (number): The aspect ratio.
 * \ns Matrix
 */
VMValue Matrix_Perspective(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	ObjArray* array = GET_ARG(0, GetArray);
	float fov = GET_ARG(1, GetDecimal);
	float nearClip = GET_ARG(2, GetDecimal);
	float farClip = GET_ARG(3, GetDecimal);
	float aspect = GET_ARG(4, GetDecimal);

	Matrix4x4 matrix4x4;
	Matrix4x4::Perspective(&matrix4x4, fov * M_PI / 180.0f, nearClip, farClip, aspect);

	for (int i = 0; i < 16; i++) {
		(*array->Values)[i] = DECIMAL_VAL(matrix4x4.Values[i]);
	}

	return NULL_VAL;
}
/***
 * Matrix.Copy
 * \desc Copies the matrix to the destination.
 * \param matrixDestination (matrix): Destination.
 * \param matrixSource (matrix): Source.
 * \ns Matrix
 */
VMValue Matrix_Copy(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjArray* matrixDestination = GET_ARG(0, GetArray);
	ObjArray* matrixSource = GET_ARG(1, GetArray);
	for (int i = 0; i < 16; i++) {
		(*matrixDestination->Values)[i] = (*matrixSource->Values)[i];
	}
	return NULL_VAL;
}
/***
 * Matrix.Multiply
 * \desc Multiplies two matrices.
 * \param matrix (matrix): The matrix to output the values to.
 * \param a (matrix): The first matrix to use for multiplying.
 * \param b (matrix): The second matrix to use for multiplying.
 * \ns Matrix
 */
VMValue Matrix_Multiply(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ObjArray* out = GET_ARG(0, GetArray);
	ObjArray* Oa = GET_ARG(1, GetArray);
	ObjArray* Ob = GET_ARG(2, GetArray);

	MatrixHelper hOut, a, b;
	MatrixHelper_CopyFrom(&a, Oa);
	MatrixHelper_CopyFrom(&b, Ob);

#define MULT_SET(x, y) a[3][y] * b[x][3] + a[2][y] * b[x][2] + a[1][y] * b[x][1] + a[0][y] * b[x][0]

	hOut[0][0] = MULT_SET(0, 0);
	hOut[0][1] = MULT_SET(0, 1);
	hOut[0][2] = MULT_SET(0, 2);
	hOut[0][3] = MULT_SET(0, 3);
	hOut[1][0] = MULT_SET(1, 0);
	hOut[1][1] = MULT_SET(1, 1);
	hOut[1][2] = MULT_SET(1, 2);
	hOut[1][3] = MULT_SET(1, 3);
	hOut[2][0] = MULT_SET(2, 0);
	hOut[2][1] = MULT_SET(2, 1);
	hOut[2][2] = MULT_SET(2, 2);
	hOut[2][3] = MULT_SET(2, 3);
	hOut[3][0] = MULT_SET(3, 0);
	hOut[3][1] = MULT_SET(3, 1);
	hOut[3][2] = MULT_SET(3, 2);
	hOut[3][3] = MULT_SET(3, 3);

#undef MULT_SET

	MatrixHelper_CopyTo(&hOut, out);
	return NULL_VAL;
}
/***
 * Matrix.Translate
 * \desc Translates the matrix.
 * \param matrix (matrix): The matrix to output the values to.
 * \param x (number): X position value.
 * \param y (number): Y position value.
 * \param z (number): Z position value.
 * \paramOpt resetToIdentity (boolean): Whether to calculate the translation values based on the matrix. (default: `false`)
 * \paramOpt actuallyTranslate (boolean): Adds the translation components to the matrix instead of overwriting them (Preserves older code functionality, please fix me!). (default: `false`)
 * \ns Matrix
 */
VMValue Matrix_Translate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);
	ObjArray* out = GET_ARG(0, GetArray);
	// ObjArray* a = out;
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);
	bool resetToIdentity = argCount >= 5 ? GET_ARG(4, GetInteger) : false;
	bool actuallyTranslate = argCount >= 6 ? GET_ARG(5, GetInteger) : false;

	MatrixHelper helper;
	MatrixHelper_CopyFrom(&helper, out);
	if (resetToIdentity) {
		memset(&helper, 0, sizeof(helper));

		helper[0][0] = 1.0f;
		helper[1][1] = 1.0f;
		helper[2][2] = 1.0f;
		helper[3][3] = 1.0f;
	}
	if (actuallyTranslate) {
		helper[0][3] += x;
		helper[1][3] += y;
		helper[2][3] += z;
	}
	else {
		helper[0][3] = x;
		helper[1][3] = y;
		helper[2][3] = z;
	}

	MatrixHelper_CopyTo(&helper, out);
	return NULL_VAL;
}
/***
 * Matrix.Scale
 * \desc Sets the matrix to a scale identity.
 * \param matrix (matrix): The matrix to output the values to.
 * \param x (number): X scale value.
 * \param y (number): Y scale value.
 * \param z (number): Z scale value.
 * \ns Matrix
 */
VMValue Matrix_Scale(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjArray* out = GET_ARG(0, GetArray);
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	helper[0][0] = x;
	helper[1][1] = y;
	helper[2][2] = z;
	helper[3][3] = 1.0f;
	MatrixHelper_CopyTo(&helper, out);
	return NULL_VAL;
}
/***
 * Matrix.Rotate
 * \desc Sets the matrix to a rotation identity.
 * \param matrix (matrix): The matrix to output the values to.
 * \param x (number): X rotation value.
 * \param y (number): Y rotation value.
 * \param z (number): Z rotation value.
 * \ns Matrix
 */
VMValue Matrix_Rotate(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjArray* out = GET_ARG(0, GetArray);
	// ObjArray* a = out;
	float x = GET_ARG(1, GetDecimal);
	float y = GET_ARG(2, GetDecimal);
	float z = GET_ARG(3, GetDecimal);

	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));

	float sinX = Math::Sin(x);
	float cosX = Math::Cos(x);
	float sinY = Math::Sin(y);
	float cosY = Math::Cos(y);
	float sinZ = Math::Sin(z);
	float cosZ = Math::Cos(z);
	float sinXY = sinX * sinY;
	helper[2][1] = sinX;
	helper[3][0] = 0.f;
	helper[3][1] = 0.f;
	helper[0][0] = (cosY * cosZ) + (sinZ * sinXY);
	helper[1][0] = (cosY * sinZ) - (cosZ * sinXY);
	helper[2][0] = cosX * sinY;
	helper[0][1] = -(cosX * sinZ);
	helper[1][1] = cosX * cosZ;

	float sincosXY = sinX * cosY;
	helper[0][2] = (sinZ * sincosXY) - (sinY * cosZ);
	helper[1][2] = (-(sinZ * sinY)) - (cosZ * sincosXY);
	helper[3][2] = 0.f;
	helper[0][3] = 0.f;
	helper[1][3] = 0.f;
	helper[2][2] = cosX * cosY;
	helper[2][3] = 0.f;
	helper[3][3] = 1.f;

	MatrixHelper_CopyTo(&helper, out);
	return NULL_VAL;
}
// #endregion

// #region RSDK.Matrix
/***
 * RSDK.Matrix.Create256
 * \desc Creates a 4x4 matrix based on the decimal 256.0.
 * \return array Returns the matrix as an array.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Create256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	ObjArray* array = NewArray();
	for (int i = 0; i < 16; i++) {
		array->Values->push_back(DECIMAL_VAL(0.0));
	}
	return OBJECT_VAL(array);
}
/***
 * RSDK.Matrix.Identity256
 * \desc Sets the matrix to the identity based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Identity256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjArray* array = GET_ARG(0, GetArray);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	helper[0][0] = 256.0;
	helper[1][0] = 0.0;
	helper[2][0] = 0.0;
	helper[3][0] = 0.0;
	helper[0][1] = 0.0;
	helper[1][1] = 256.0;
	helper[2][1] = 0.0;
	helper[3][1] = 0.0;
	helper[0][2] = 0.0;
	helper[1][2] = 0.0;
	helper[2][2] = 256.0;
	helper[3][2] = 0.0;
	helper[0][3] = 0.0;
	helper[1][3] = 0.0;
	helper[2][3] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, array);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.Multiply256
 * \desc Multiplies two matrices based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param a (matrix): The first matrix to use for multiplying.
 * \param b (matrix): The second matrix to use for multiplying.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Multiply256(int argCount, VMValue* args, Uint32 threadID) {
	ObjArray* dest = GET_ARG(0, GetArray);
	ObjArray* matrixA = GET_ARG(1, GetArray);
	ObjArray* matrixB = GET_ARG(2, GetArray);
	MatrixHelper result, a, b;
	MatrixHelper_CopyFrom(&a, matrixA);
	MatrixHelper_CopyFrom(&b, matrixB);
	for (int i = 0; i < 16; i++) {
		int rowA = i / 4;
		int rowB = i % 4;
		result[rowB][rowA] = (a[3][rowA] * b[rowB][3] / 256.0) +
			(a[2][rowA] * b[rowB][2] / 256.0) + (a[1][rowA] * b[rowB][1] / 256.0) +
			(a[0][rowA] * b[rowB][0] / 256.0);
	}
	MatrixHelper_CopyTo(&result, dest);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.Translate256
 * \desc Translates the matrix based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param x (number): X position value.
 * \param y (number): Y position value.
 * \param z (number): Z position value.
 * \param setIdentity (boolean): Whether to set the matrix as the identity.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Translate256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int x = GET_ARG(1, GetDecimal);
	int y = GET_ARG(2, GetDecimal);
	int z = GET_ARG(3, GetDecimal);
	bool setIdentity = GET_ARG(4, GetInteger);
	MatrixHelper helper;
	MatrixHelper_CopyFrom(&helper, matrix);
	if (setIdentity) {
		helper[0][0] = 256.0;
		helper[1][0] = 0.0;
		helper[2][0] = 0.0;
		helper[0][1] = 0.0;
		helper[1][1] = 256.0;
		helper[2][1] = 0.0;
		helper[0][2] = 0.0;
		helper[1][2] = 0.0;
		helper[2][2] = 256.0;
		helper[3][0] = 0.0;
		helper[3][1] = 0.0;
		helper[3][2] = 0.0;
		helper[3][3] = 256.0;
	}
	helper[0][3] = x / 256.0;
	helper[1][3] = y / 256.0;
	helper[2][3] = z / 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.Scale256
 * \desc Sets the matrix to a scale identity based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param scaleX (number): X scale value.
 * \param scaleY (number): Y scale value.
 * \param scaleZ (number): Z scale value.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Scale256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int scaleX = GET_ARG(1, GetDecimal);
	int scaleY = GET_ARG(2, GetDecimal);
	int scaleZ = GET_ARG(3, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	helper[0][0] = scaleZ;
	helper[1][0] = 0.0;
	helper[2][0] = 0.0;
	helper[3][0] = 0.0;
	helper[0][1] = 0.0;
	helper[1][1] = scaleY;
	helper[2][1] = 0.0;
	helper[3][1] = 0.0;
	helper[0][2] = 0.0;
	helper[1][2] = 0.0;
	helper[2][2] = scaleZ;
	helper[3][2] = 0.0;
	helper[0][3] = 0.0;
	helper[1][3] = 0.0;
	helper[2][3] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.RotateX256
 * \desc Sets the matrix to a rotation X identity based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param rotationY (number): X rotation value.
 * \ns RSDK.Matrix
 */
VMValue Matrix_RotateX256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int rotationX = GET_ARG(1, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	double sine = Math::Sin1024(rotationX) / 4.0;
	double cosine = Math::Cos1024(rotationX) / 4.0;
	helper[0][0] = 256.0;
	helper[1][0] = 0.0;
	helper[2][0] = 0.0;
	helper[3][0] = 0.0;
	helper[0][1] = 0.0;
	helper[1][1] = cosine;
	helper[2][1] = sine;
	helper[3][1] = 0.0;
	helper[0][2] = 0.0;
	helper[1][2] = -sine;
	helper[2][2] = cosine;
	helper[3][2] = 0.0;
	helper[0][3] = 0.0;
	helper[1][3] = 0.0;
	helper[2][3] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.RotateY256
 * \desc Sets the matrix to a rotation Y identity based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param rotationY (number): Y rotation value.
 * \ns RSDK.Matrix
 */
VMValue Matrix_RotateY256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int rotationY = GET_ARG(1, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	double sine = Math::Sin1024(rotationY) / 4.0;
	double cosine = Math::Cos1024(rotationY) / 4.0;
	helper[0][0] = cosine;
	helper[1][0] = 0.0;
	helper[2][0] = sine;
	helper[3][0] = 0.0;
	helper[0][1] = 0.0;
	helper[1][1] = 256.0;
	helper[2][1] = 0.0;
	helper[3][1] = 0.0;
	helper[0][2] = -sine;
	helper[1][2] = 0.0;
	helper[2][2] = cosine;
	helper[3][2] = 0.0;
	helper[0][3] = 0.0;
	helper[1][3] = 0.0;
	helper[2][3] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.RotateZ256
 * \desc Sets the matrix to a rotation Z identity based on the decimal 256.0.
 * \param matrix (matrix): The matrix to output the values to.
 * \param rotationZ (number): Z rotation value.
 * \ns RSDK.Matrix
 */
VMValue Matrix_RotateZ256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int rotationZ = GET_ARG(1, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	double sine = Math::Sin1024(rotationZ) / 4.0;
	double cosine = Math::Cos1024(rotationZ) / 4.0;
	helper[0][0] = cosine;
	helper[1][0] = -sine;
	helper[2][0] = 0.0;
	helper[3][0] = 0.0;
	helper[0][1] = sine;
	helper[1][1] = cosine;
	helper[2][1] = 0.0;
	helper[3][1] = 0.0;
	helper[0][2] = 0.0;
	helper[1][2] = 0.0;
	helper[2][2] = 256.0;
	helper[3][2] = 0.0;
	helper[0][3] = 0.0;
	helper[1][3] = 0.0;
	helper[2][3] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
/***
 * RSDK.Matrix.Rotate256
 * \desc Sets the matrix to a rotation identity based on 256.
 * \param matrix (matrix): The matrix to output the values to.
 * \param rotationX (number): X rotation value.
 * \param rotationY (number): Y rotation value.
 * \param rotationZ (number): Z rotation value.
 * \ns RSDK.Matrix
 */
VMValue Matrix_Rotate256(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ObjArray* matrix = GET_ARG(0, GetArray);
	int rotationX = GET_ARG(1, GetDecimal);
	int rotationY = GET_ARG(2, GetDecimal);
	int rotationZ = GET_ARG(3, GetDecimal);
	MatrixHelper helper;
	memset(&helper, 0, sizeof(helper));
	double sineX = Math::Sin1024(rotationX) / 4.0;
	double cosineX = Math::Cos1024(rotationX) / 4.0;
	double sineY = Math::Sin1024(rotationX) / 4.0;
	double cosineY = Math::Cos1024(rotationX) / 4.0;
	double sineZ = Math::Sin1024(rotationX) / 4.0;
	double cosineZ = Math::Cos1024(rotationX) / 4.0;

	helper[0][0] = (cosineZ * cosineY / 256.0) + (sineZ * (sineY * sineX / 256.0) / 256.0);
	helper[0][1] = -(sineZ * cosineX) / 256.0;
	helper[0][2] = (sineZ * (cosineY * cosineX / 256.0) / 256.0) - (cosineZ * cosineY / 256.0);
	helper[0][3] = 0.0;
	helper[1][0] = (sineZ * cosineY / 256.0) - (cosineZ * (sineY * sineX / 256.0) / 256.0);
	helper[1][1] = cosineZ * cosineX / 256.0;
	helper[1][2] = (-(sineZ * sineY) / 256.0) - (cosineZ * (cosineY * sineX / 256.0) / 256.0);
	helper[1][3] = 0.0;
	helper[2][0] = sineY * cosineX / 256.0;
	helper[2][1] = sineX;
	helper[2][2] = cosineY * cosineX / 256.0;
	helper[2][3] = 0.0;
	helper[3][0] = 0.0;
	helper[3][1] = 0.0;
	helper[3][2] = 0.0;
	helper[3][3] = 256.0;
	MatrixHelper_CopyTo(&helper, matrix);
	return NULL_VAL;
}
// #endregion

#define CHECK_MODEL_ANIMATION_INDEX(animation) \
	if (animation < 0 || animation >= (signed)model->Animations.size()) { \
		OUT_OF_RANGE_ERROR("Animation index", animation, 0, model->Animations.size() - 1); \
		return NULL_VAL; \
	}

#define CHECK_ARMATURE_INDEX(armature) \
	if (armature < 0 || armature >= (signed)model->Armatures.size()) { \
		OUT_OF_RANGE_ERROR("Armature index", armature, 0, model->Armatures.size() - 1); \
		return NULL_VAL; \
	}

// #region Model
/***
 * Model.GetVertexCount
 * \desc Returns how many vertices are in the model.
 * \param model (integer): The model index to check.
 * \return integer The vertex count.
 * \ns Model
 */
VMValue Model_GetVertexCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL((int)model->VertexCount);
}
/***
 * Model.GetAnimationCount
 * \desc Returns how many animations exist in the model.
 * \param model (integer): The model index to check.
 * \return integer Returns an integer value.
 * \ns Model
 */
VMValue Model_GetAnimationCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL((int)model->Animations.size());
}
/***
 * Model.GetAnimationName
 * \desc Gets the name of the model animation with the specified index.
 * \param model (integer): The model index to check.
 * \param animation (integer): Index of the animation.
 * \return string Returns the animation name, or `null` if the model contains no animations.
 * \ns Model
 */
VMValue Model_GetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	IModel* model = GET_ARG(0, GetModel);
	int animation = GET_ARG(1, GetInteger);

	if (!model || model->Animations.size() == 0) {
		return NULL_VAL;
	}

	CHECK_MODEL_ANIMATION_INDEX(animation);

	const char* animationName = model->Animations[animation]->Name;
	if (!animationName) {
		return NULL_VAL;
	}

	return ReturnString(animationName);
}
/***
 * Model.GetAnimationIndex
 * \desc Gets the index of the model animation with the specified name.
 * \param model (integer): The model index to check.
 * \param animationName (string): Name of the animation to find.
 * \return integer Returns the animation index, or `-1` if the animation could not be found. Will always return `-1` if the model contains no animations.
 * \ns Model
 */
VMValue Model_GetAnimationIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(-1);
	}
	char* animationName = GET_ARG(1, GetString);
	return INTEGER_VAL(model->GetAnimationIndex(animationName));
}
/***
 * Model.GetFrameCount
 * \desc Returns how many frames exist in the model.
 * \param model (integer): The model index to check.
 * \return integer Returns an integer value.
 * \deprecated Use <ref Model.GetAnimationLength> instead.
 * \ns Model
 */
VMValue Model_GetFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL((int)model->Meshes[0]->FrameCount);
}
/***
 * Model.GetAnimationLength
 * \desc Returns the length of the animation.
 * \param model (integer): The model index to check.
 * \param animation (integer): The animation index to check.
 * \return integer The number of keyframes in the animation.
 * \ns Model
 */
VMValue Model_GetAnimationLength(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	IModel* model = GET_ARG(0, GetModel);
	int animation = GET_ARG(1, GetInteger);
	if (!model) {
		return INTEGER_VAL(0);
	}
	CHECK_MODEL_ANIMATION_INDEX(animation);
	return INTEGER_VAL((int)model->Animations[animation]->Length);
}
/***
 * Model.HasMaterials
 * \desc Checks to see if the model has materials.
 * \param model (integer): The model index to check.
 * \return boolean Returns whether the model has materials.
 * \deprecated Use <ref Model.GetMaterialCount> instead.
 * \ns Model
 */
VMValue Model_HasMaterials(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(false);
	}
	return INTEGER_VAL((int)model->HasMaterials());
}
/***
 * Model.HasBones
 * \desc Checks to see if the model has bones.
 * \param model (integer): The model index to check.
 * \return boolean Returns whether the model has bones.
 * \ns Model
 */
VMValue Model_HasBones(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(false);
	}
	return INTEGER_VAL((int)model->HasBones());
}
/***
 * Model.GetMaterialCount
 * \desc Returns the amount of materials in the model.
 * \param model (integer): The model index to check.
 * \return integer Returns an integer value.
 * \ns Model
 */
VMValue Model_GetMaterialCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL((int)model->Materials.size());
}
/***
 * Model.GetMaterial
 * \desc Gets a material from a model.
 * \param model (integer): The model index to check.
 * \param material (string): The material to get, from its name.
 * \return Material Returns a <ref Material>, or `null` if the model has no materials.
 * \ns Model
 */
/***
 * Model.GetMaterial
 * \desc Gets a material from a model.
 * \param model (integer): The model index to check.
 * \param material (integer): The material to get, from its index.
 * \return Material Returns a <ref Material>, or `null` if the model has no materials.
 * \ns Model
 */
VMValue Model_GetMaterial(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	IModel* model = GET_ARG(0, GetModel);
	if (!model || model->Materials.size() == 0) {
		return INTEGER_VAL(0);
	}

	Material* material = nullptr;

	if (IS_INTEGER(args[1])) {
		int materialIndex = GET_ARG(1, GetInteger);
		if (materialIndex < 0 || (size_t)materialIndex >= model->Materials.size()) {
			OUT_OF_RANGE_ERROR(
				"Material index", materialIndex, 0, (int)model->Materials.size());
			return NULL_VAL;
		}
		material = model->Materials[materialIndex];
	}
	else {
		char* materialName = GET_ARG(1, GetString);
		size_t idx = model->FindMaterial(materialName);
		if (idx == -1) {
			THROW_ERROR("Model has no material named \"%s\".", materialName);
			return NULL_VAL;
		}
		material = model->Materials[idx];
	}

	return OBJECT_VAL(material->Object);
}
/***
 * Model.CreateArmature
 * \desc Creates an armature from the model.
 * \param model (integer): The model index.
 * \return integer Returns the index of the armature.
 * \ns Model
 */
VMValue Model_CreateArmature(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	IModel* model = GET_ARG(0, GetModel);
	if (!model) {
		return INTEGER_VAL(-1);
	}
	return INTEGER_VAL(model->NewArmature());
}
/***
 * Model.PoseArmature
 * \desc Poses an armature.
 * \param model (integer): The model index.
 * \param armature (integer): The armature index to pose.
 * \paramOpt animation (integer): Animation to pose the armature.
 * \paramOpt frame (decimal): Frame to pose the armature.
 * \ns Model
 */
VMValue Model_PoseArmature(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);

	if (!model) {
		return NULL_VAL;
	}

	CHECK_ARMATURE_INDEX(armature);

	if (argCount >= 3) {
		int animation = GET_ARG(2, GetInteger);
		int frame = GET_ARG(3, GetDecimal) * 0x100;
		if (frame < 0) {
			frame = 0;
		}

		CHECK_MODEL_ANIMATION_INDEX(animation);

		model->Animate(model->Armatures[armature], model->Animations[animation], frame);
	}
	else {
		// Just update the skeletons
		model->Armatures[armature]->UpdateSkeletons();
	}

	return NULL_VAL;
}
/***
 * Model.ResetArmature
 * \desc Resets an armature to its default pose.
 * \param model (integer): The model index.
 * \param armature (integer): The armature index to reset.
 * \return
 * \ns Model
 */
VMValue Model_ResetArmature(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);
	if (!model) {
		return NULL_VAL;
	}
	CHECK_ARMATURE_INDEX(armature);
	model->Armatures[armature]->Reset();
	return NULL_VAL;
}
/***
 * Model.DeleteArmature
 * \desc Deletes an armature from the model.
 * \param model (integer): The model index.
 * \param armature (integer): The armature index to delete.
 * \return
 * \ns Model
 */
VMValue Model_DeleteArmature(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);
	if (!model) {
		return NULL_VAL;
	}
	CHECK_ARMATURE_INDEX(armature);
	model->DeleteArmature((size_t)armature);
	return NULL_VAL;
}
// #endregion

#undef CHECK_MODEL_ANIMATION_INDEX
#undef CHECK_ARMATURE_INDEX

// #region Music
/***
 * Music.Play
 * \desc Places the music onto the music stack and plays it.
 * \param music (integer): The music index to play.
 * \paramOpt loopPoint (integer): Loop point in samples. Use <ref AUDIO_LOOP_NONE> to play the track once or <ref AUDIO_LOOP_DEFAULT> to use the audio file's metadata. (default: <ref AUDIO_LOOP_DEFAULT>)
 * \paramOpt panning (decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (default: `0.0`)
 * \paramOpt speed (decimal): Control the speed of the audio. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed. (default: `1.0`)
 * \paramOpt volume (decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (default: `1.0`)
 * \paramOpt startPoint (decimal): The time (in seconds) to start the music at. (default: `0.0`)
 * \paramOpt fadeInAfterFinished (decimal): The time period to fade in the previous music track after the currently playing track finishes playing, in seconds. (default: `0.0`)
 * \ns Music
 */
VMValue Music_Play(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetMusic);
	int loopPoint = IS_NULL(args[1]) ? AUDIO_LOOP_DEFAULT
					 : GET_ARG_OPT(1, GetInteger, AUDIO_LOOP_DEFAULT);
	float panning = GET_ARG_OPT(2, GetDecimal, 0.0f);
	float speed = GET_ARG_OPT(3, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(4, GetDecimal, 1.0f);
	double startPoint = GET_ARG_OPT(5, GetDecimal, 0.0);
	float fadeInAfterFinished = GET_ARG_OPT(6, GetDecimal, 0.0f);

	if (fadeInAfterFinished < 0.f) {
		fadeInAfterFinished = 0.f;
	}

	if (loopPoint < AUDIO_LOOP_NONE) {
		THROW_ERROR(
			"Audio loop point value should be AUDIO_LOOP_DEFAULT, AUDIO_LOOP_NONE, or a number higher than zero, received %d",
			loopPoint);
		return NULL_VAL;
	}

	if (loopPoint == AUDIO_LOOP_DEFAULT) {
		loopPoint = audio->LoopPoint;
	}

	if (audio) {
		AudioManager::PushMusicAt(audio,
			startPoint,
			loopPoint >= 0,
			loopPoint >= 0 ? loopPoint : 0,
			panning,
			speed,
			volume,
			fadeInAfterFinished);
	}

	return NULL_VAL;
}
/***
 * Music.Stop
 * \desc Removes the music from the music stack, stopping it if currently playing.
 * \param music (integer): The music index to stop.
 * \ns Music
 */
VMValue Music_Stop(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetMusic);
	if (audio) {
		AudioManager::RemoveMusic(audio);
	}
	return NULL_VAL;
}
/***
 * Music.StopWithFadeOut
 * \desc Removes the music at the top of the music stack, fading it out over a time period.
 * \param seconds (decimal): The time period to fade out the music, in seconds.
 * \ns Music
 */
VMValue Music_StopWithFadeOut(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	float seconds = GET_ARG(0, GetDecimal);
	if (seconds < 0.f) {
		seconds = 0.f;
	}
	AudioManager::FadeOutMusic(seconds);
	return NULL_VAL;
}
/***
 * Music.Pause
 * \desc Pauses the music at the top of the music stack.
 * \return
 * \ns Music
 */
VMValue Music_Pause(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::Lock();
	if (AudioManager::MusicStack.size() > 0) {
		AudioManager::MusicStack[0]->Paused = true;
	}
	AudioManager::Unlock();
	return NULL_VAL;
}
/***
 * Music.Resume
 * \desc Resumes the music at the top of the music stack.
 * \return
 * \ns Music
 */
VMValue Music_Resume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::Lock();
	if (AudioManager::MusicStack.size() > 0) {
		AudioManager::MusicStack[0]->Paused = false;
	}
	AudioManager::Unlock();
	return NULL_VAL;
}
/***
 * Music.Clear
 * \desc Completely clears the music stack, stopping all music.
 * \ns Music
 */
VMValue Music_Clear(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::ClearMusic();
	return NULL_VAL;
}
/***
 * Music.IsPlaying
 * \desc Checks to see if the specified music is currently playing.
 * \param music (integer): The music index to play.
 * \return boolean Returns a boolean value.
 * \ns Music
 */
VMValue Music_IsPlaying(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetMusic);
	if (!audio) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(AudioManager::IsPlayingMusic(audio));
}
/***
 * Music.GetPosition
 * \desc Gets the position of the current track playing.
 * \param music (integer): The music index to get the current position (in seconds) of.
 * \return decimal Returns a decimal value.
 * \ns Music
 */
VMValue Music_GetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetMusic);
	if (!audio) {
		return DECIMAL_VAL(0.0);
	}
	return DECIMAL_VAL((float)AudioManager::GetMusicPosition(audio));
}
/***
 * Music.Alter
 * \desc Alters the playback conditions of the current track playing.
 * \param panning (decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it.
 * \param speed (decimal): Control the speed of the audio. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed.
 * \param volume (decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume.
 * \ns Music
 */
VMValue Music_Alter(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	float panning = GET_ARG(0, GetDecimal);
	float speed = GET_ARG(1, GetDecimal);
	float volume = GET_ARG(2, GetDecimal);

	AudioManager::AlterMusic(panning, speed, volume);
	return NULL_VAL;
}
/***
 * Music.GetLoopPoint
 * \desc Gets the loop point of a music index, if it has one.
 * \param music (integer): The music index to get the loop point.
 * \return integer Returns the loop point in samples, as an integer value, or `null` if the audio does not have one.
 * \ns Music
 */
VMValue Music_GetLoopPoint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetMusic);
	if (!audio || audio->LoopPoint == -1) {
		return NULL_VAL;
	}
	return INTEGER_VAL(audio->LoopPoint);
}
/***
 * Music.SetLoopPoint
 * \desc Sets the loop point of a music index.
 * \param music (integer): The music index to set the loop point.
 * \param loopPoint (integer): The loop point in samples, or `null` to remove the audio's loop point.
 * \ns Music
 */
VMValue Music_SetLoopPoint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISound* audio = GET_ARG(0, GetMusic);
	int loopPoint = IS_NULL(args[1]) ? -1 : GET_ARG(1, GetInteger);
	if (!audio) {
		return NULL_VAL;
	}
	if (loopPoint >= -1) {
		audio->LoopPoint = loopPoint;
	}
	return NULL_VAL;
}
// #endregion

// #region Number
/***
 * Number.ToString
 * \desc Converts a number to a string.
 * \param n (number): Number value.
 * \paramOpt base (integer): The numerical base, or radix. (default: `10`)
 * \return string Returns a string value.
 * \ns Number
 */
VMValue Number_ToString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	int base = 10;
	if (argCount == 2) {
		base = GET_ARG(1, GetInteger);
	}

	switch (args[0].Type) {
	case VAL_DECIMAL:
	case VAL_LINKED_DECIMAL: {
		float n = GET_ARG(0, GetDecimal);
		char temp[16];
		snprintf(temp, sizeof temp, "%f", n);

		return ReturnString(temp);
	}
	case VAL_INTEGER:
	case VAL_LINKED_INTEGER: {
		int n = GET_ARG(0, GetInteger);
		char temp[16];
		if (base == 16) {
			snprintf(temp, sizeof temp, "0x%X", n);
		}
		else {
			snprintf(temp, sizeof temp, "%d", n);
		}

		return ReturnString(temp);
	}
	default:
		THROW_ERROR("Expected argument %d to be of type %s instead of %s.",
			0 + 1,
			"Number",
			GetValueTypeString(args[0]));
	}

	return NULL_VAL;
}
/***
 * Number.AsInteger
 * \desc Converts a decimal to an integer.
 * \param n (number): Number value.
 * \return integer Returns an integer value.
 * \ns Number
 */
VMValue Number_AsInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL((int)GET_ARG(0, GetDecimal));
}
/***
 * Number.AsDecimal
 * \desc Converts a integer to a decimal.
 * \param n (number): Number value.
 * \return decimal Returns a decimal value.
 * \ns Number
 */
VMValue Number_AsDecimal(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(GET_ARG(0, GetDecimal));
}
// #endregion

// #region Object
/***
 * Object.Loaded
 * \desc Checks if an object class is loaded.
 * \param className (string): Name of the object class.
 * \return boolean Returns whether the class is loaded.
 * \ns Object
 */
VMValue Object_Loaded(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	char* objectName = GET_ARG(0, GetString);

	return INTEGER_VAL(!!Scene::ObjectLists->Exists(
		Scene::ObjectLists->HashFunction(objectName, strlen(objectName))));
}
/***
 * Object.SetActivity
 * \desc Sets the active state of an object to determine if/when it runs its GlobalUpdate function.
 * \param className (string): Name of the object class.
 * \param Activity (integer): The active state to set the object to.
 * \ns Object
 */
VMValue Object_SetActivity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* objectName = GET_ARG(0, GetString);
	Uint32 objectNameHash = Scene::ObjectLists->HashFunction(objectName, strlen(objectName));

	if (Scene::ObjectLists->Exists(
		    Scene::ObjectLists->HashFunction(objectName, strlen(objectName)))) {
		Scene::GetObjectList(objectName)->Activity = GET_ARG(1, GetInteger);
	}

	return NULL_VAL;
}
/***
 * Object.GetActivity
 * \desc Gets the active state of an object that determines if/when it runs its GlobalUpdate function.
 * \param className (string): Name of the object class.
 * \return <ref ACTIVE_*> Returns the active state of the object if it is loaded, otherwise returns -1.
 * \ns Object
 */
VMValue Object_GetActivity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	char* objectName = GET_ARG(0, GetString);
	Uint32 objectNameHash = Scene::ObjectLists->HashFunction(objectName, strlen(objectName));

	if (Scene::ObjectLists->Exists(
		    Scene::ObjectLists->HashFunction(objectName, strlen(objectName)))) {
		return INTEGER_VAL(Scene::GetObjectList(objectName)->Activity);
	}

	return INTEGER_VAL(-1);
}
// #endregion

// #region Palette
/***
 * Palette.EnablePaletteUsage
 * \desc Enables or disables palette usage for the application.
 * \param usePalettes (boolean): Whether to use palettes.
 * \ns Palette
 */
VMValue Palette_EnablePaletteUsage(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int usePalettes = GET_ARG(0, GetInteger);
	Graphics::UsePalettes = usePalettes;
	return NULL_VAL;
}
#define CHECK_COLOR_INDEX(index) \
	if (index < 0 || index >= 0x100) { \
		OUT_OF_RANGE_ERROR("Palette color index", index, 0, 255); \
		return NULL_VAL; \
	}
/***
 * Palette.LoadFromResource
 * \desc Loads palette from an .act, .col, .gif, .png, or .hpal resource.
 * \param paletteIndex (integer): Index of palette to load to.
 * \param filename (string): Filepath of resource.
 * \paramOpt activeRows (bitfield): Which rows of 16 colors will not be loaded for .act, .col, and .gif files, from bottom to top.
 * \ns Palette
 */
VMValue Palette_LoadFromResource(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int palIndex = GET_ARG(0, GetInteger);
	char* filename = GET_ARG(1, GetString);
	int disabledRows = GET_ARG_OPT(2, GetInteger, 0);

	CHECK_PALETTE_INDEX(palIndex);

	// RSDK StageConfig
	if (StringUtils::StrCaseStr(filename, "StageConfig.bin")) {
		RSDKSceneReader::StageConfig_GetColors(filename);
	}
	// RSDK GameConfig
	else if (StringUtils::StrCaseStr(filename, "GameConfig.bin")) {
		RSDKSceneReader::GameConfig_GetColors(filename);
	}
	else {
		ResourceStream* reader;
		if ((reader = ResourceStream::New(filename))) {
			MemoryStream* memoryReader;
			if ((memoryReader = MemoryStream::New(reader))) {
				// ACT file
				if (StringUtils::StrCaseStr(filename, ".act") ||
					StringUtils::StrCaseStr(filename, ".ACT")) {
					do {
						Uint8 Color[3];
						for (int col = 0; col < 16; col++) {
							if (!(disabledRows & (1 << col))) {
								for (int d = 0; d < 16; d++) {
									memoryReader->ReadBytes(
										Color, 3);
									Graphics::PaletteColors
										[palIndex]
										[(col << 4) | d] =
											0xFF000000U |
										Color[0] << 16 |
										Color[1] << 8 |
										Color[2];
								}
								Graphics::ConvertFromARGBtoNative(
									&Graphics::PaletteColors
										[palIndex]
										[(col << 4)],
									16);
							}
							else {
								for (int d = 0; d < 16; d++) {
									memoryReader->ReadBytes(
										Color, 3);
								}
							}
						}
						Graphics::PaletteUpdated = true;
					} while (false);
				}
				// COL file
				else if (StringUtils::StrCaseStr(filename, ".col") ||
					StringUtils::StrCaseStr(filename, ".COL")) {
					// Skip COL header
					memoryReader->Skip(8);

					// Read colors
					Uint8 Color[4];
					for (int col = 0; col < 16; col++) {
						if (!(disabledRows & (1 << col))) {
							for (int d = 0; d < 16; d++) {
								memoryReader->ReadBytes(Color, 3);
								Graphics::PaletteColors
									[palIndex][(col << 4) | d] =
										0xFF000000U |
									Color[0] << 16 |
									Color[1] << 8 | Color[2];
							}
							Graphics::ConvertFromARGBtoNative(
								&Graphics::PaletteColors[palIndex][(
									col << 4)],
								16);
						}
						else {
							for (int d = 0; d < 16; d++) {
								memoryReader->ReadBytes(Color, 3);
							}
						}
					}
					Graphics::PaletteUpdated = true;
				}
				// HPAL file
				// .hpal defines color lines that it can load instead of full 256 color .act's
				else if (StringUtils::StrCaseStr(filename, ".hpal") ||
					StringUtils::StrCaseStr(filename, ".HPAL")) {
					do {
						Uint32 magic = memoryReader->ReadUInt32();
						if (magic != 0x4C415048) {
							break;
						}

						Uint8 Color[3];
						int paletteCount = memoryReader->ReadUInt32();

						if (paletteCount > MAX_PALETTE_COUNT - palIndex) {
							paletteCount = MAX_PALETTE_COUNT - palIndex;
						}

						for (int i = palIndex; i < palIndex + paletteCount;
							i++) {
							// Palette Set
							int bitmap = memoryReader->ReadUInt16();
							for (int col = 0; col < 16; col++) {
								int lineStart = col << 4;
								if ((bitmap & (1 << col)) != 0) {
									for (int d = 0; d < 16;
										d++) {
										memoryReader
											->ReadBytes(
												Color,
												3);
										Graphics::PaletteColors
											[i]
											[lineStart |
												d] =
												0xFF000000U |
											Color[0]
												<< 16 |
											Color[1]
												<< 8 |
											Color[2];
									}
									Graphics::ConvertFromARGBtoNative(
										&Graphics::PaletteColors
											[i]
											[lineStart],
										16);
								}
							}
						}
						Graphics::PaletteUpdated = true;
					} while (false);
				}
				// GIF file
				else if (StringUtils::StrCaseStr(filename, ".gif") ||
					StringUtils::StrCaseStr(filename, ".GIF")) {
					bool loadPalette = Graphics::UsePalettes;

					GIF* gif;

					Graphics::UsePalettes = false;
					gif = GIF::Load(memoryReader);
					Graphics::UsePalettes = loadPalette;

					if (gif) {
						if (gif->Colors) {
							for (int col = 0; col < 16; col++) {
								if (!(disabledRows & (1 << col))) {
									for (int d = 0; d < 16;
										d++) {
										Graphics::PaletteColors
											[palIndex]
											[(col << 4) |
												d] =
												gif->Colors[(col << 4) |
													d];
									}
								}
							}
							Graphics::PaletteUpdated = true;
						}
						Memory::Free(gif->Data);
						delete gif;
					}
				}
				// PNG file
				else if (StringUtils::StrCaseStr(filename, ".png") ||
					StringUtils::StrCaseStr(filename, ".PNG")) {
					bool loadPalette = Graphics::UsePalettes;

					PNG* png;

					Graphics::UsePalettes = true;
					png = PNG::Load(memoryReader);
					Graphics::UsePalettes = loadPalette;

					if (png) {
						if (png->Paletted) {
							for (int p = 0; p < png->NumPaletteColors;
								p++) {
								Graphics::PaletteColors
									[palIndex][p] =
										png->Colors[p];
							}
							Graphics::PaletteUpdated = true;
						}
						Memory::Free(png->Data);
						delete png;
					}
				}
				else {
					Log::Print(Log::LOG_ERROR,
						"Cannot read palette \"%s\"!",
						filename);
				}

				memoryReader->Close();
			}
			reader->Close();
		}
	}

	return NULL_VAL;
}
/***
 * Palette.LoadFromImage
 * \desc Loads palette from an image resource.
 * \param paletteIndex (integer): Index of palette to load to.
 * \param image (integer): Index of the loaded image.
 * \ns Palette
 */
VMValue Palette_LoadFromImage(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int palIndex = GET_ARG(0, GetInteger);
	Image* image = GET_ARG(1, GetImage);

	CHECK_PALETTE_INDEX(palIndex);

	if (!image) {
		return NULL_VAL;
	}

	Texture* texture = image->TexturePtr;

	size_t x = 0;

	for (size_t y = 0; y < texture->Height; y++) {
		Uint32* line = (Uint32*)texture->Pixels + (y * texture->Width);
		size_t length = texture->Width;
		if (length > 0x100) {
			length = 0x100;
		}

		for (size_t src = 0; src < length && x < 0x100;) {
			Graphics::PaletteColors[palIndex][x++] = 0xFF000000 | line[src++];
		}
		Graphics::PaletteUpdated = true;
		if (x >= 0x100) {
			break;
		}
	}

	return NULL_VAL;
}
/***
 * Palette.GetColor
 * \desc Gets a color from the specified palette.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndex (integer): Index of color.
 * \return integer Returns an integer value.
 * \ns Palette
 */
VMValue Palette_GetColor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndex = GET_ARG(1, GetInteger);
	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndex);
	Uint32 color = Graphics::PaletteColors[palIndex][colorIndex];
	Graphics::ConvertFromARGBtoNative(&color, 1);
	return INTEGER_VAL((int)(color & 0xFFFFFFU));
}
/***
 * Palette.SetColor
 * \desc Sets a color on the specified palette, format 0xRRGGBB.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndex (integer): Index of color.
 * \param hex (integer): Hexadecimal color value to set the color to. (format: 0xRRGGBB)
 * \ns Palette
 */
VMValue Palette_SetColor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndex = GET_ARG(1, GetInteger);
	Uint32 hex = (Uint32)GET_ARG(2, GetInteger);
	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndex);
	Uint32* color = &Graphics::PaletteColors[palIndex][colorIndex];
	*color = (hex & 0xFFFFFFU) | 0xFF000000U;
	Graphics::ConvertFromARGBtoNative(color, 1);
	Graphics::PaletteUpdated = true;
	return NULL_VAL;
}
/***
 * Palette.GetColorTransparent
 * \desc Gets if the color on the specified palette is transparent.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndex (integer): Index of color.
 * \return boolean Returns a boolean value.
 * \ns Palette
 */
VMValue Palette_GetColorTransparent(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndex = GET_ARG(1, GetInteger);
	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndex);
	if (Graphics::PaletteColors[palIndex][colorIndex] & 0xFF000000U) {
		return INTEGER_VAL(false);
	}
	return INTEGER_VAL(true);
}
/***
 * Palette.SetColorTransparent
 * \desc Sets a color on the specified palette transparent.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndex (integer): Index of color.
 * \param isTransparent (boolean): Whether to make the color transparent.
 * \ns Palette
 */
VMValue Palette_SetColorTransparent(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndex = GET_ARG(1, GetInteger);
	bool isTransparent = !!GET_ARG(2, GetInteger);
	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndex);
	Uint32* color = &Graphics::PaletteColors[palIndex][colorIndex];
	if (isTransparent) {
		*color &= ~0xFF000000U;
	}
	else {
		*color |= 0xFF000000U;
	}
	Graphics::PaletteUpdated = true;
	return NULL_VAL;
}
/***
 * Palette.MixPalettes
 * \desc Mixes colors between two palettes and outputs to another palette.
 * \param destinationPaletteIndex (integer): Index of palette to put colors to.
 * \param paletteIndexA (integer): First index of palette.
 * \param paletteIndexB (integer): Second index of palette.
 * \param mixRatio (number): Percentage to mix the colors between 0.0 - 1.0.
 * \param colorIndexStart (integer): First index of colors to mix.
 * \param colorCount (integer): Amount of colors to mix.
 * \ns Palette
 */
inline Uint32 PMP_ColorBlend(Uint32 color1, Uint32 color2, int percent) {
	Uint32 rb = color1 & 0xFF00FFU;
	Uint32 g = color1 & 0x00FF00U;
	rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
	g += (((color2 & 0x00FF00U) - g) * percent) >> 8;
	return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
VMValue Palette_MixPalettes(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(6);
	int palIndexDest = GET_ARG(0, GetInteger);
	int palIndex1 = GET_ARG(1, GetInteger);
	int palIndex2 = GET_ARG(2, GetInteger);
	float mixRatio = GET_ARG(3, GetDecimal);
	int colorIndexStart = GET_ARG(4, GetInteger);
	int colorCount = GET_ARG(5, GetInteger);

	CHECK_PALETTE_INDEX(palIndexDest);
	CHECK_PALETTE_INDEX(palIndex1);
	CHECK_PALETTE_INDEX(palIndex2);
	CHECK_COLOR_INDEX(colorIndexStart);

	if (colorCount > 0x100) {
		colorCount = 0x100;
	}

	mixRatio = Math::Clamp(mixRatio, 0.0f, 1.0f);

	int percent = mixRatio * 0x100;
	for (int c = colorIndexStart; c < colorIndexStart + colorCount; c++) {
		Graphics::PaletteColors[palIndexDest][c] = 0xFF000000U |
			PMP_ColorBlend(Graphics::PaletteColors[palIndex1][c],
				Graphics::PaletteColors[palIndex2][c],
				percent);
	}
	Graphics::PaletteUpdated = true;
	return NULL_VAL;
}
/***
 * Palette.RotateColorsLeft
 * \desc Shifts the colors on the palette to the left.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndexStart (integer): First index of colors to shift.
 * \param colorCount (integer): Amount of colors to shift.
 * \ns Palette
 */
VMValue Palette_RotateColorsLeft(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndexStart = GET_ARG(1, GetInteger);
	int count = GET_ARG(2, GetInteger);

	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndexStart);

	if (count > 0x100 - colorIndexStart) {
		count = 0x100 - colorIndexStart;
	}

	Uint32 temp = Graphics::PaletteColors[palIndex][colorIndexStart];
	for (int i = colorIndexStart + 1; i < colorIndexStart + count; i++) {
		Graphics::PaletteColors[palIndex][i - 1] = Graphics::PaletteColors[palIndex][i];
	}
	Graphics::PaletteColors[palIndex][colorIndexStart + count - 1] = temp;
	Graphics::PaletteUpdated = true;
	return NULL_VAL;
}
/***
 * Palette.RotateColorsRight
 * \desc Shifts the colors on the palette to the right.
 * \param paletteIndex (integer): Index of palette.
 * \param colorIndexStart (integer): First index of colors to shift.
 * \param colorCount (integer): Amount of colors to shift.
 * \ns Palette
 */
VMValue Palette_RotateColorsRight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int palIndex = GET_ARG(0, GetInteger);
	int colorIndexStart = GET_ARG(1, GetInteger);
	int count = GET_ARG(2, GetInteger);

	CHECK_PALETTE_INDEX(palIndex);
	CHECK_COLOR_INDEX(colorIndexStart);

	if (count > 0x100 - colorIndexStart) {
		count = 0x100 - colorIndexStart;
	}

	Uint32 temp = Graphics::PaletteColors[palIndex][colorIndexStart + count - 1];
	for (int i = colorIndexStart + count - 1; i >= colorIndexStart; i--) {
		Graphics::PaletteColors[palIndex][i] = Graphics::PaletteColors[palIndex][i - 1];
	}
	Graphics::PaletteColors[palIndex][colorIndexStart] = temp;
	Graphics::PaletteUpdated = true;
	return NULL_VAL;
}
/***
 * Palette.CopyColors
 * \desc Copies colors from Palette A to Palette B
 * \param paletteIndexA (integer): Index of palette to get colors from.
 * \param colorIndexStartA (integer): First index of colors to copy.
 * \param paletteIndexB (integer): Index of palette to put colors to.
 * \param colorIndexStartB (integer): First index of colors to be placed.
 * \param colorCount (integer): Amount of colors to be copied.
 * \ns Palette
 */
VMValue Palette_CopyColors(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	int palIndexFrom = GET_ARG(0, GetInteger);
	int colorIndexStartFrom = GET_ARG(1, GetInteger);
	int palIndexTo = GET_ARG(2, GetInteger);
	int colorIndexStartTo = GET_ARG(3, GetInteger);
	int count = GET_ARG(4, GetInteger);

	CHECK_PALETTE_INDEX(palIndexFrom);
	CHECK_COLOR_INDEX(colorIndexStartFrom);
	CHECK_PALETTE_INDEX(palIndexTo);
	CHECK_COLOR_INDEX(colorIndexStartTo);

	if (count > 0x100 - colorIndexStartTo) {
		count = 0x100 - colorIndexStartTo;
	}
	if (count > 0x100 - colorIndexStartFrom) {
		count = 0x100 - colorIndexStartFrom;
	}

	memcpy(&Graphics::PaletteColors[palIndexTo][colorIndexStartTo],
		&Graphics::PaletteColors[palIndexFrom][colorIndexStartFrom],
		count * sizeof(Uint32));
	Graphics::PaletteUpdated = true;

	return NULL_VAL;
}
/***
 * Palette.UsePaletteIndexLines
 * \desc Enables or disables the global palette index table.
 * \param usePaletteIndexLines (boolean): Whether to use the global palette index table.
 * \ns Palette
 */
VMValue Palette_UsePaletteIndexLines(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int usePaletteIndexLines = GET_ARG(0, GetInteger);
	Graphics::UsePaletteIndexLines = usePaletteIndexLines;
	return NULL_VAL;
}
/***
 * Palette.SetPaletteIndexLines
 * \desc Sets the palette to be used for drawing on certain Y-positions on the screen (between the start and end lines).
 * \param paletteIndex (integer): Index of palette.
 * \param lineStart (number): Start line to set to the palette.
 * \param lineEnd (number): Line where to stop setting the palette.
 * \ns Palette
 */
VMValue Palette_SetPaletteIndexLines(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int palIndex = GET_ARG(0, GetInteger);
	Sint32 lineStart = (int)GET_ARG(1, GetDecimal);
	Sint32 lineEnd = (int)GET_ARG(2, GetDecimal);

	CHECK_PALETTE_INDEX(palIndex);

	Sint32 lastLine = sizeof(Graphics::PaletteIndexLines) - 1;
	if (lineStart > lastLine) {
		lineStart = lastLine;
	}

	if (lineEnd > lastLine) {
		lineEnd = lastLine;
	}

	for (Sint32 i = lineStart; i < lineEnd; i++) {
		Graphics::PaletteIndexLines[i] = (Uint8)palIndex;
	}
	Graphics::PaletteIndexLinesUpdated = true;
	return NULL_VAL;
}
#undef CHECK_COLOR_INDEX
// #endregion

// #region Random
/***
 * Random.SetSeed
 * \desc Sets the PRNG seed.
 * \param seed (integer): The seed.
 * \ns Random
 */
VMValue Random_SetSeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Sint32 seed = GET_ARG(0, GetInteger);
	Random::SetSeed(seed);
	return NULL_VAL;
}
/***
 * Random.GetSeed
 * \desc Gets the PRNG seed.
 * \return integer Returns an integer value.
 * \ns Random
 */
VMValue Random_GetSeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Random::Seed);
}
/***
 * Random.Max
 * \desc Gets a random number between 0.0 and a specified maximum, and advances the PRNG state.
 * \param max (number): Maximum non-inclusive value.
 * \return decimal Returns a decimal value.
 * \ns Random
 */
VMValue Random_Max(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return DECIMAL_VAL(Random::Max(GET_ARG(0, GetDecimal)));
}
/***
 * Random.Range
 * \desc Gets a random number between a specified minimum and a specified maximum, and advances the PRNG state.
 * \param min (number): Minimum non-inclusive value.
 * \param max (number): Maximum non-inclusive value.
 * \return decimal Returns a decimal value.
 * \ns Random
 */
VMValue Random_Range(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	return DECIMAL_VAL(Random::Range(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
// #endregion

// #region Resources
/***
 * Resources.LoadSprite
 * \desc Loads a Sprite resource, returning its Sprite index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadSprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadSpriteResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadDynamicSprite
 * \desc Loads a Sprite resource via the current Scene's resource folder (else a fallback folder) if a scene list is loaded.
 * \param fallbackFolder (string): Folder to check if the sprite does not exist in the current Scene's resource folder.
 * \param name (string): Name of the animation file within the resource folder.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadDynamicSprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	char* fallbackFolder = GET_ARG(0, GetString);
	char* name = GET_ARG(1, GetString);
	int unloadPolicy = GET_ARG(2, GetInteger);

	char filename[4096];
	snprintf(filename,
		sizeof(filename),
		"Sprites/%s/%s.bin",
		Scene::CurrentResourceFolder,
		name);
	if (!ResourceManager::ResourceExists(filename)) {
		snprintf(filename, sizeof(filename), "Sprites/%s/%s.bin", fallbackFolder, name);
	}

	int result = Scene::LoadSpriteResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadImage
 * \desc Loads an Image resource, returning its Image index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadImage(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadImageResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadModel
 * \desc Loads Model resource, returning its Model index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadModel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadModelResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadMusic
 * \desc Loads a Music resource, returning its Music index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadMusic(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadMusicResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadSound
 * \desc Loads a Sound resource, returning its Sound index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadSound(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadSoundResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.LoadVideo
 * \desc Loads a Video resource, returning its Video index.
 * \param filename (string): Filename of the resource.
 * \param unloadPolicy (integer): Whether to unload the resource at the end of the current Scene, or the game end.
 * \return integer Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadVideo(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* filename = GET_ARG(0, GetString);
	int unloadPolicy = GET_ARG(1, GetInteger);

	int result = Scene::LoadVideoResource(filename, unloadPolicy);

	return INTEGER_VAL(result);
}
/***
 * Resources.FileExists
 * \desc Checks to see if a Resource exists with the given filename.
 * \param filename (string): The given filename.
 * \return boolean Returns whether the Resource exists.
 * \ns Resources
 */
VMValue Resources_FileExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* filename = GET_ARG(0, GetString);
	if (ResourceManager::ResourceExists(filename)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * Resources.ReadAllText
 * \desc Reads all text from the given filename.
 * \param path (string): The path of the resource to read.
 * \return string Returns all the text in the resource as a string value if it can be read, otherwise it returns a `null` value if it cannot be read.
 * \ns Resources
 */
VMValue Resources_ReadAllText(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* filePath = GET_ARG(0, GetString);
	Stream* stream = ResourceStream::New(filePath);
	if (!stream) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		size_t size = stream->Length();
		ObjString* text = AllocString(size);
		stream->ReadBytes(text->Chars, size);
		stream->Close();
		ScriptManager::Unlock();
		return OBJECT_VAL(text);
	}
	return NULL_VAL;
}
// #endregion

// #region Scene
#define CHECK_TILE_LAYER_POS_BOUNDS() \
	if (layer < 0 || layer >= (int)Scene::Layers.size() || x < 0 || y < 0 || \
		x >= Scene::Layers[layer].Width || y >= Scene::Layers[layer].Height) \
		return NULL_VAL;

/***
 * Scene.Load
 * \desc Changes the active scene. The active scene is changed to the one in the specified resource file.
 * \paramOpt filename (string): Filename of scene.
 * \paramOpt persistency (boolean): Whether the scene should load with persistency.
 * \ns Scene
 */
/***
 * Scene.Load
 * \desc Changes the active scene. The active scene is changed to the currently set entry in the scene list, if it exists (see <ref SceneList>.)
 * \paramOpt persistency (boolean): Whether the scene should load with persistency.
 * \ns Scene
 */
VMValue Scene_Load(int argCount, VMValue* args, Uint32 threadID) {
	bool loadFromResource = false;
	bool noPersistency = false;

	// If at least one argument is provided, and the first one isn't an integer
	if (argCount >= 1 && !IS_INTEGER(args[0])) {
		// Argument 1 is the resource path
		char* filename = GET_ARG(0, GetString);

		StringUtils::Copy(Scene::NextScene, filename, sizeof(Scene::NextScene));

		loadFromResource = true;

		// Argument 2 becomes noPersistency
		noPersistency = !!GET_ARG_OPT(1, GetInteger, false);
	}
	else {
		// Else, load from the scene list
		// Argument 1 becomes noPersistency
		noPersistency = !!GET_ARG_OPT(0, GetInteger, false);
	}

	// If this block is entered then Scene.Load must've been called like:
	// - Scene.Load()
	// - Scene.Load(false)
	// - Scene.Load(true)
	if (!loadFromResource) {
		if (!SceneInfo::IsEntryValid(Scene::ActiveCategory, Scene::CurrentSceneInList)) {
			return NULL_VAL;
		}

		std::string path =
			SceneInfo::GetFilename(Scene::ActiveCategory, Scene::CurrentSceneInList);

		StringUtils::Copy(Scene::NextScene, path.c_str(), sizeof(Scene::NextScene));
	}

	Scene::NoPersistency = noPersistency;

	return NULL_VAL;
}
/***
 * Scene.Change
 * \desc Changes the current scene if the category name and scene name are valid. If only one argument is provided, the global category is used. Note that this does not load the scene; you must use <ref Scene.Load>.
 * \param category (string): Category name.
 * \param scene (string): Scene name. If the scene name is not found but the category name is, the first scene in the category is used.
 * \ns Scene
 */
VMValue Scene_Change(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount == 1) {
		const char* sceneName = GET_ARG(0, GetString);
		Scene::SetCurrent(SCENEINFO_GLOBAL_CATEGORY_NAME, sceneName);
	}
	else {
		CHECK_ARGCOUNT(2);
		const char* categoryName = GET_ARG(0, GetString);
		const char* sceneName = GET_ARG(1, GetString);
		Scene::SetCurrent(categoryName, sceneName);
	}
	return NULL_VAL;
}
/***
 * Scene.LoadTileCollisions
 * \desc Load tile collisions from a resource file.
 * \param filename (string): Filename of tile collision file.
 * \paramOpt tilesetID (integer): Tileset to load tile collisions for.
 * \ns Scene
 */
VMValue Scene_LoadTileCollisions(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* filename = GET_ARG(0, GetString);
	int tilesetID = GET_ARG_OPT(1, GetInteger, 0);
	Scene::LoadTileCollisions(filename, (size_t)tilesetID);
	return NULL_VAL;
}
/***
 * Scene.AreTileCollisionsLoaded
 * \desc Returns or whether tile collisions are loaded.
 * \return boolean Returns whether tile collisions are loaded.
 * \ns Scene
 */
VMValue Scene_AreTileCollisionsLoaded(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)Scene::TileCfgLoaded);
}
/***
 * Scene.AddTileset
 * \desc Adds a new tileset into the scene.
 * \param tileset (string): Path of tileset to load.
 * \return boolean Returns whether the tileset was added to the scene.
 * \ns Scene
 */
VMValue Scene_AddTileset(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* tileset = GET_ARG(0, GetString);
	return INTEGER_VAL(Scene::AddTileset(tileset));
}
/***
 * Scene.Restart
 * \desc Restarts the active scene.
 * \ns Scene
 */
VMValue Scene_Restart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	Scene::DoRestart = true;
	return NULL_VAL;
}
/***
 * Scene.PropertyExists
 * \desc Checks if a property exists.
 * \param property (string): Name of property to check.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* property = GET_ARG(0, GetString);
	if (!Scene::Properties) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(Scene::Properties->Exists(property));
}
/***
 * Scene.GetProperty
 * \desc Gets a property.
 * \param property (string): Name of property.
 * \return value Returns the property.
 * \ns Scene
 */
VMValue Scene_GetProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* property = GET_ARG(0, GetString);
	if (!Scene::Properties || !Scene::Properties->Exists(property)) {
		return NULL_VAL;
	}
	return Scene::Properties->Get(property);
}
/***
 * Scene.GetLayerCount
 * \desc Gets the amount of layers in the active scene.
 * \return integer Returns the amount of layers in the active scene.
 * \ns Scene
 */
VMValue Scene_GetLayerCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)Scene::Layers.size());
}
/***
 * Scene.GetLayerIndex
 * \desc Gets the layer index of the layer with the specified name.
 * \param layerName (string): Name of the layer to find.
 * \return integer Returns the layer index, or `-1` if the layer could not be found.
 * \ns Scene
 */
VMValue Scene_GetLayerIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* layername = GET_ARG(0, GetString);
	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		if (strcmp(Scene::Layers[i].Name, layername) == 0) {
			return INTEGER_VAL((int)i);
		}
	}
	return INTEGER_VAL(-1);
}
/***
 * Scene.GetLayerName
 * \desc Gets the name of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetLayerName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return ReturnString(Scene::Layers[index].Name);
}
/***
 * Scene.GetLayerVisible
 * \desc Gets the visibility of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_GetLayerVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(!!Scene::Layers[index].Visible);
}
/***
 * Scene.GetLayerOpacity
 * \desc Gets the opacity of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns Scene
 */
VMValue Scene_GetLayerOpacity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return DECIMAL_VAL(Scene::Layers[index].Opacity);
}
/***
 * Scene.GetLayerShader
 * \desc Gets the shader of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return Shader Returns a Shader, or `null`.
 * \ns Scene
 */
VMValue Scene_GetLayerShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);

	Shader* shader = (Shader*)Scene::Layers[index].CurrentShader;
	if (shader != nullptr) {
		return OBJECT_VAL(shader->Object);
	}

	return NULL_VAL;
}
/***
 * Scene.GetLayerUsePaletteIndexLines
 * \desc Gets whether the layer is using the global palette index table.
 * \param layerIndex (integer): Index of layer.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_GetLayerUsePaletteIndexLines(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(Scene::Layers[index].UsePaletteIndexLines);
}
/***
 * Scene.GetLayerProperty
 * \desc Gets a property of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param property (string): Name of property.
 * \return value Returns the property.
 * \ns Scene
 */
VMValue Scene_GetLayerProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	char* property = GET_ARG(1, GetString);
	CHECK_SCENE_LAYER_INDEX(index);
	return Scene::Layers[index].PropertyGet(property);
}
/***
 * Scene.GetLayerExists
 * \desc Gets whether a layer exists.
 * \param layerIndex (integer): Index of layer.
 * \return boolean Returns a boolean value.
 * \deprecated
 * \ns Scene
 */
VMValue Scene_GetLayerExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(index < Scene::Layers.size());
}
/***
 * Scene.GetLayerDeformSplitLine
 * \desc Gets the Deform Split Line of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerDeformSplitLine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(Scene::Layers[index].DeformSplitLine);
}
/***
 * Scene.GetLayerDeformOffsetA
 * \desc Gets the tile deform offset above the Deform Split Line of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerDeformOffsetA(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(Scene::Layers[index].DeformOffsetA);
}
/***
 * Scene.GetLayerDeformOffsetB
 * \desc Gets the tile deform offset below the Deform Split Line of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerDeformOffsetB(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(Scene::Layers[index].DeformOffsetB);
}
/***
 * Scene.LayerPropertyExists
 * \desc Checks if a property exists in the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param property (string): Name of property to check.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_LayerPropertyExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	char* property = GET_ARG(1, GetString);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(!!Scene::Layers[index].PropertyExists(property));
}
/***
 * Scene.GetName
 * \desc Gets the name of the active scene.
 * \return string Returns the name of the active scene.
 * \ns Scene
 */
VMValue Scene_GetName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Scene::CurrentScene);
}
/***
 * Scene.GetType
 * \desc Gets the type of the active scene.
 * \return <ref SCENETYPE_*> Returns the type of the active scene.
 * \ns Scene
 */
VMValue Scene_GetType(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::SceneType);
}
/***
 * Scene.GetWidth
 * \desc Gets the width of the scene (in tiles).
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int v = 0;
	if (Scene::Layers.size() > 0) {
		v = Scene::Layers[0].Width;
	}

	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		if (strcmp(Scene::Layers[i].Name, "FG Low") == 0) {
			return INTEGER_VAL(Scene::Layers[i].Width);
		}
	}

	return INTEGER_VAL(v);
}
/***
 * Scene.GetHeight
 * \desc Gets the height of the scene (in tiles).
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int v = 0;
	if (Scene::Layers.size() > 0) {
		v = Scene::Layers[0].Height;
	}

	for (size_t i = 0; i < Scene::Layers.size(); i++) {
		if (strcmp(Scene::Layers[i].Name, "FG Low") == 0) {
			return INTEGER_VAL(Scene::Layers[i].Height);
		}
	}

	return INTEGER_VAL(v);
}
/***
 * Scene.GetLayerWidth
 * \desc Gets the width of a layer index (in tiles).
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int layer = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(layer);
	return INTEGER_VAL(Scene::Layers[layer].Width);
}
/***
 * Scene.GetLayerHeight
 * \desc Gets the height of a layer index (in tiles).
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int layer = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(layer);
	return INTEGER_VAL(Scene::Layers[layer].Height);
}
/***
 * Scene.GetLayerOffsetX
 * \desc Gets the X offset of a layer index.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns Scene
 */
VMValue Scene_GetLayerOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return DECIMAL_VAL(Scene::Layers[index].OffsetX);
}
/***
 * Scene.GetLayerOffsetY
 * \desc Gets the Y offset of a layer index.
 * \param layerIndex (integer): Index of layer.
 * \return decimal Returns a decimal value.
 * \ns Scene
 */
VMValue Scene_GetLayerOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return DECIMAL_VAL(Scene::Layers[index].OffsetY);
}
/***
 * Scene.GetLayerDrawGroup
 * \desc Gets the draw group of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetLayerDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	return INTEGER_VAL(Scene::Layers[index].DrawGroup);
}
/***
 * Scene.GetLayerHorizontalRepeat
 * \desc Gets whether the layer repeats horizontally.
 * \param layerIndex (integer): Index of layer.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_GetLayerHorizontalRepeat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);

	if (Scene::Layers[index].Flags & SceneLayer::FLAGS_REPEAT_X) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * Scene.GetLayerVerticalRepeat
 * \desc Gets whether the layer repeats vertically.
 * \param layerIndex (integer): Index of layer.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_GetLayerVerticalRepeat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);

	if (Scene::Layers[index].Flags & SceneLayer::FLAGS_REPEAT_Y) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * Scene.GetTilesetCount
 * \desc Gets the amount of tilesets in the current scene.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTilesetCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)Scene::Tilesets.size());
}
/***
 * Scene.GetTilesetIndex
 * \desc Gets the tileset index for the specified filename.
 * \param tilesetID (integer): The tileset index.
 * \return integer Returns the tileset index, or `-1` if there is no tileset with said filename.
 * \ns Scene
 */
VMValue Scene_GetTilesetIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* name = GET_ARG(0, GetString);
	for (size_t i = 0; i < Scene::Tilesets.size(); i++) {
		if (strcmp(Scene::Tilesets[i].Filename, name) == 0) {
			return INTEGER_VAL((int)i);
		}
	}
	return INTEGER_VAL(-1);
}
#define CHECK_TILESET_INDEX \
	if (index < 0 || index >= (int)Scene::Tilesets.size()) { \
		OUT_OF_RANGE_ERROR("Tileset index", index, 0, (int)Scene::Tilesets.size() - 1); \
		return NULL_VAL; \
	}
/***
 * Scene.GetTilesetName
 * \desc Gets the tileset name for the specified tileset index.
 * \param tilesetIndex (Index): The tileset index.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetTilesetName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_TILESET_INDEX
	return ReturnString(Scene::Tilesets[index].Filename);
}
/***
 * Scene.GetTilesetTileCount
 * \desc Gets the tile count for the specified tileset.
 * \param tilesetID (integer): The tileset index.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTilesetTileCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_TILESET_INDEX
	return INTEGER_VAL((int)Scene::Tilesets[index].TileCount);
}
/***
 * Scene.GetTilesetFirstTileID
 * \desc Gets the first tile index number for the specified tileset.
 * \param tilesetID (integer): The tileset index.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTilesetFirstTileID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_TILESET_INDEX
	return INTEGER_VAL((int)Scene::Tilesets[index].StartTile);
}
/***
 * Scene.GetTilesetPaletteIndex
 * \desc Gets the palette index for the specified tileset.
 * \param tilesetID (integer): The tileset index.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTilesetPaletteIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_TILESET_INDEX
	return INTEGER_VAL((int)Scene::Tilesets[index].PaletteID);
}
/***
 * Scene.GetTileWidth
 * \desc Gets the width of tiles.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTileWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::TileWidth);
}
/***
 * Scene.GetTileHeight
 * \desc Gets the height of tiles.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTileHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::TileHeight);
}
/***
 * Scene.GetTileID
 * \desc Gets the tile's index number at the tile coordinates.
 * \param layer (integer): Index of the layer
 * \param x (number): X position (in tiles) of the tile
 * \param y (number): Y position (in tiles) of the tile
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetTileID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int layer = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);

	CHECK_SCENE_LAYER_INDEX(layer);

	CHECK_TILE_LAYER_POS_BOUNDS();

	return INTEGER_VAL(
		(int)(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] &
			TILE_IDENT_MASK));
}
/***
 * Scene.GetTileFlipX
 * \desc Gets whether the tile at the tile coordinates is flipped horizontally.
 * \param layer (integer): Index of the layer
 * \param x (number): X position (in tiles) of the tile
 * \param y (number): Y position (in tiles) of the tile
 * \return boolean Returns whether the tile's horizontally flipped.
 * \ns Scene
 */
VMValue Scene_GetTileFlipX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int layer = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);

	CHECK_SCENE_LAYER_INDEX(layer);

	CHECK_TILE_LAYER_POS_BOUNDS();

	return INTEGER_VAL(
		!!(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] &
			TILE_FLIPX_MASK));
}
/***
 * Scene.GetTileFlipY
 * \desc Gets whether the tile at the tile coordinates is flipped vertically.
 * \param layer (integer): Index of the layer
 * \param x (number): X position (in tiles) of the tile
 * \param y (number): Y position (in tiles) of the tile
 * \return boolean Returns whether the tile's vertically flipped.
 * \ns Scene
 */
VMValue Scene_GetTileFlipY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int layer = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);

	CHECK_SCENE_LAYER_INDEX(layer);

	CHECK_TILE_LAYER_POS_BOUNDS();

	return INTEGER_VAL(
		!!(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] &
			TILE_FLIPY_MASK));
}
/***
 * Scene.GetDrawGroupCount
 * \desc Gets the amount of draw groups in the active scene.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetDrawGroupCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::PriorityPerLayer);
}
/***
 * Scene.GetDrawGroupEntityDepthSorting
 * \desc Gets if the specified draw group sorts objects by depth.
 * \param drawGroup (integer): Number between 0 to the result of <ref Scene.GetDrawGroupCount>.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_GetDrawGroupEntityDepthSorting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int drawGroup = GET_ARG(0, GetInteger);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		OUT_OF_RANGE_ERROR("Draw group", drawGroup, 0, MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	DrawGroupList* drawGroupList = Scene::GetDrawGroup(drawGroup);
	return INTEGER_VAL(!!drawGroupList->EntityDepthSortingEnabled);
}
/***
 * Scene.GetCurrentFolder
 * \desc Gets the current folder of the scene.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetCurrentFolder(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Scene::CurrentFolder);
}
/***
 * Scene.GetCurrentID
 * \desc Gets the current ID of the scene.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetCurrentID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Scene::CurrentID);
}
/***
 * Scene.GetCurrentResourceFolder
 * \desc Gets the current resource folder of the scene.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetCurrentResourceFolder(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Scene::CurrentResourceFolder);
}
/***
 * Scene.GetCurrentCategory
 * \desc Gets the current category name of the scene.
 * \return string Returns a string value.
 * \ns Scene
 */
VMValue Scene_GetCurrentCategory(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return ReturnString(Scene::CurrentCategory);
}
/***
 * Scene.GetDebugMode
 * \desc Gets whether Debug Mode has been turned on in the current scene.
 * \return integer Returns an integer value.
 * \ns Scene
 */
VMValue Scene_GetDebugMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::DebugMode);
}
/***
 * Scene.GetFirstInstance
 * \desc Gets the first active instance in the scene.
 * \return Entity Returns the first active instance in the scene, or `null` if there are no instances in the scene.
 * \ns Scene
 */
VMValue Scene_GetFirstInstance(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	ScriptEntity* object = (ScriptEntity*)Scene::ObjectFirst;
	if (object) {
		return OBJECT_VAL(object->Instance);
	}

	return NULL_VAL;
}
/***
 * Scene.GetLastInstance
 * \desc Gets the last active instance in the scene.
 * \return Entity Returns the last active instance in the scene, or `null` if there are no instances in the scene.
 * \ns Scene
 */
VMValue Scene_GetLastInstance(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);

	ScriptEntity* object = (ScriptEntity*)Scene::ObjectLast;
	if (object) {
		return OBJECT_VAL(object->Instance);
	}

	return NULL_VAL;
}
/***
 * Scene.GetReservedSlotIDs
 * \desc Gets the number of reserved slot IDs to account for before creating instances.
 * \return integer Returns how many reserved slot IDs are being used.
 * \ns Scene
 */
VMValue Scene_GetReservedSlotIDs(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::ReservedSlotIDs);
}
/***
 * Scene.GetInstanceCount
 * \desc Gets the count of instances currently in the scene.
 * \return integer Returns the amount of instances in the scene.
 * \ns Scene
 */
VMValue Scene_GetInstanceCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::ObjectCount);
}
/***
 * Scene.GetStaticInstanceCount
 * \desc Gets the count of instances currently in the scene.
 * \return integer Returns the amount of instances in the scene.
 * \ns Scene
 */
VMValue Scene_GetStaticInstanceCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::StaticObjectCount);
}
/***
 * Scene.GetDynamicInstanceCount
 * \desc Gets the count of instances currently in the scene.
 * \return integer Returns the amount of instances in the scene.
 * \ns Scene
 */
VMValue Scene_GetDynamicInstanceCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::DynamicObjectCount);
}
/***
 * Scene.GetTileAnimationEnabled
 * \desc Gets whether tile animation is enabled.
 * \return integer Returns 0 if tile animation is disabled, 1 if it's enabled, and 2 if tiles animate even if the scene is paused.
 * \ns Scene
 */
VMValue Scene_GetTileAnimationEnabled(int argCount, VMValue* args, Uint32 threadID) {
	return INTEGER_VAL((int)Scene::TileAnimationEnabled);
}
/***
 * Scene.GetTileAnimSequence
 * \desc Gets the tile IDs of the animation sequence for a tile ID.
 * \param tileID (integer): Which tile ID.
 * \return array Returns an array of tile IDs.
 * \ns Scene
 */
VMValue Scene_GetTileAnimSequence(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	Tileset* tileset = Scene::GetTileset(tileID);
	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (!tileset || !animator) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		ObjArray* array = NewArray();
		ISprite* tileSprite = animator->Sprite;
		Animation* animation = animator->GetCurrentAnimation();
		for (int i = 0; i < animation->Frames.size(); i++) {
			int x = animation->Frames[i].X / tileset->TileWidth;
			int y = animation->Frames[i].Y / tileset->TileHeight;
			int tileID = ((y * tileset->NumCols) + x) + tileset->StartTile;
			array->Values->push_back(INTEGER_VAL(tileID));
		}
		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}

	return NULL_VAL;
}
/***
 * Scene.GetTileAnimSequenceDurations
 * \desc Gets the frame durations of the animation sequence for a tile ID.
 * \param tileID (integer): Which tile ID.
 * \return array Returns an array of frame durations.
 * \ns Scene
 */
VMValue Scene_GetTileAnimSequenceDurations(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	Tileset* tileset = Scene::GetTileset(tileID);
	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (!tileset || !animator) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		ObjArray* array = NewArray();
		ISprite* tileSprite = animator->Sprite;
		Animation* animation = animator->GetCurrentAnimation();
		for (int i = 0; i < animation->Frames.size(); i++) {
			array->Values->push_back(INTEGER_VAL(animation->Frames[i].Duration));
		}
		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}

	return NULL_VAL;
}
/***
 * Scene.GetTileAnimSequencePaused
 * \desc Returns whether a tile ID animation sequence is paused.
 * \param tileID (integer): The tile ID.
 * \return boolean Whether the animation is paused.
 * \ns Scene
 */
VMValue Scene_GetTileAnimSequencePaused(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		return INTEGER_VAL(animator->Paused);
	}

	return NULL_VAL;
}
/***
 * Scene.GetTileAnimSequenceSpeed
 * \desc Gets the speed of a tile ID animation sequence.
 * \param tileID (integer): The tile ID.
 * \return decimal The frame speed.
 * \ns Scene
 */
VMValue Scene_GetTileAnimSequenceSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		return DECIMAL_VAL(animator->Speed);
	}

	return NULL_VAL;
}
/***
 * Scene.GetTileAnimSequenceFrame
 * \desc Gets the current frame of a tile ID animation sequence.
 * \param tileID (integer): The tile ID.
 * \return integer The frame index.
 * \ns Scene
 */
VMValue Scene_GetTileAnimSequenceFrame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		return INTEGER_VAL(animator->AnimationIndex);
	}

	return NULL_VAL;
}
/***
 * Scene.IsCurrentEntryValid
 * \desc Checks if the current entry in the scene list is valid.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_IsCurrentEntryValid(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (SceneInfo::IsEntryValid(Scene::ActiveCategory, Scene::CurrentSceneInList)) {
		return INTEGER_VAL(true);
	}
	return INTEGER_VAL(false);
}
/***
 * Scene.IsUsingFolder
 * \desc Checks whether the current scene's folder matches the string to check.
 * \param folder (string): Folder name to compare.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_IsUsingFolder(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	const char* checkFolder = GET_ARG(0, GetString);

	std::string folder = SceneInfo::GetFolder(Scene::ActiveCategory, Scene::CurrentSceneInList);
	if (folder == "") {
		return INTEGER_VAL(false);
	}

	if (strcmp(folder.c_str(), checkFolder) == 0) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * Scene.IsUsingID
 * \desc Checks whether the current scene's ID matches the string to check.
 * \param id (string): ID to compare.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_IsUsingID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	const char* checkID = GET_ARG(0, GetString);

	std::string id = SceneInfo::GetID(Scene::ActiveCategory, Scene::CurrentSceneInList);
	if (id == "") {
		return INTEGER_VAL(false);
	}

	if (strcmp(id.c_str(), checkID) == 0) {
		return INTEGER_VAL(true);
	}

	return INTEGER_VAL(false);
}
/***
 * Scene.IsPaused
 * \desc Gets whether the scene is paused.
 * \return boolean Returns a boolean value.
 * \ns Scene
 */
VMValue Scene_IsPaused(int argCount, VMValue* args, Uint32 threadID) {
	return INTEGER_VAL((int)Scene::Paused);
}
/***
 * Scene.SetReservedSlotIDs
 * \desc Sets the number of reserved slot IDs to account for before creating instances.
 * \param amount (integer): How many reserved slot IDs to use.
 * \ns Scene
 */
VMValue Scene_SetReservedSlotIDs(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Scene::ReservedSlotIDs = GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Scene.SetDebugMode
 * \desc Sets whether Debug Mode has been turned on in the current scene.
 * \ns Scene
 */
VMValue Scene_SetDebugMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Scene::DebugMode = GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Scene.SetTile
 * \desc Sets the tile at a position.
 * \param layer (integer): Layer index.
 * \param cellX (number): Tile cell X.
 * \param cellY (number): Tile cell Y.
 * \param tileID (integer): Tile ID.
 * \param flipX (boolean): Whether to flip the tile horizontally.
 * \param flipY (boolean): Whether to flip the tile vertically.
 * \paramOpt collisionMaskA (integer): Collision mask to use for the tile on Plane A. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \paramOpt collisionMaskB (integer): Collision mask to use for the tile on Plane B. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \ns Scene
 */
VMValue Scene_SetTile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(6);
	int layer = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);
	int tileID = GET_ARG(3, GetInteger);
	int flip_x = GET_ARG(4, GetInteger);
	int flip_y = GET_ARG(5, GetInteger);
	// Optionals
	int collA = TILE_COLLA_MASK, collB = TILE_COLLB_MASK;
	if (argCount == 7) {
		collA = collB = GET_ARG(6, GetInteger);
		// collA <<= 28;
		// collB <<= 26;
	}
	else if (argCount == 8) {
		collA = GET_ARG(6, GetInteger);
		collB = GET_ARG(7, GetInteger);
	}

	CHECK_SCENE_LAYER_INDEX(layer);

	CHECK_TILE_LAYER_POS_BOUNDS();

	Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

	*tile = tileID & TILE_IDENT_MASK;
	if (flip_x) {
		*tile |= TILE_FLIPX_MASK;
	}
	if (flip_y) {
		*tile |= TILE_FLIPY_MASK;
	}

	*tile |= collA;
	*tile |= collB;

	Scene::AnyLayerTileChange = true;

	return NULL_VAL;
}
/***
 * Scene.SetTileCollisionSides
 * \desc Sets the collision of a tile at a position.
 * \param layer (integer): Layer index.
 * \param cellX (number): Tile cell X.
 * \param cellY (number): Tile cell Y.
 * \param collisionMaskA (integer): Collision mask to use for the tile on Plane A. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \param collisionMaskB (integer): Collision mask to use for the tile on Plane B. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \return
 * \ns Scene
 */
VMValue Scene_SetTileCollisionSides(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	int layer = GET_ARG(0, GetInteger);
	int x = (int)GET_ARG(1, GetDecimal);
	int y = (int)GET_ARG(2, GetDecimal);
	int collA = GET_ARG(3, GetInteger) << 28;
	int collB = GET_ARG(4, GetInteger) << 26;

	CHECK_SCENE_LAYER_INDEX(layer);

	CHECK_TILE_LAYER_POS_BOUNDS();

	Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

	*tile &= TILE_FLIPX_MASK | TILE_FLIPY_MASK | TILE_IDENT_MASK;
	*tile |= collA;
	*tile |= collB;

	Scene::AnyLayerTileChange = true;

	return NULL_VAL;
}
/***
 * Scene.SetPaused
 * \desc Sets whether the game is paused. When paused, only objects with <ref Entity.Pauseable> set to `false` will continue to `Update`.
 * \param isPaused (boolean): Whether the scene is paused.
 * \ns Scene
 */
VMValue Scene_SetPaused(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Scene::Paused = GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Scene.SetTileAnimationEnabled
 * \desc Sets whether tile animation is enabled.
 * \param isEnabled (integer): 0 disables tile animation, 1 enables it, and 2 makes tiles animate even if the scene is paused.
 * \ns Scene
 */
VMValue Scene_SetTileAnimationEnabled(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Scene::TileAnimationEnabled = GET_ARG(0, GetInteger);
	return NULL_VAL;
}
/***
 * Scene.SetTileAnimSequence
 * \desc Sets an animation sequence for a tile ID.
 * \param tileID (integer): Which tile ID to add an animated sequence to.
 * \param tileIDs (array): Tile ID list.
 * \paramOpt frameDurations (array): Frame duration list.
 * \ns Scene
 */
VMValue Scene_SetTileAnimSequence(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);

	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	std::vector<int> tileIDs;
	std::vector<int> frameDurations;

	if (!IS_NULL(args[1])) {
		ObjArray* array = GET_ARG(1, GetArray);

		int otherTileID = 0;

		for (size_t i = 0; i < array->Values->size(); i++) {
			VMValue val = (*array->Values)[i];
			if (IS_INTEGER(val)) {
				otherTileID = AS_INTEGER(val);
			}
			else {
				THROW_ERROR(
					"Expected array index %d (argument 2) to be of type %s instead of %s.",
					i,
					GetTypeString(VAL_INTEGER),
					GetValueTypeString(val));
			}

			tileIDs.push_back(otherTileID);
			frameDurations.push_back(30);
		}

		if (argCount >= 3) {
			array = GET_ARG(2, GetArray);

			for (size_t i = 0; i < array->Values->size() && i < tileIDs.size(); i++) {
				VMValue val = (*array->Values)[i];
				if (!IS_INTEGER(val)) {
					THROW_ERROR(
						"Expected array index %d (argument 3) to be of type %s instead of %s.",
						i,
						GetTypeString(VAL_INTEGER),
						GetValueTypeString(val));
					continue;
				}

				frameDurations[i] = AS_INTEGER(val);
			}
		}
	}

	Tileset* tileset = Scene::GetTileset(tileID);
	if (tileset) {
		tileset->AddTileAnimSequence(
			tileID, &Scene::TileSpriteInfos[tileID], tileIDs, frameDurations);
	}

	return NULL_VAL;
}
/***
 * Scene.SetTileAnimSequenceFromSprite
 * \desc Sets an animation sequence for a tile ID.
 * \param tileID (integer): Which tile ID to add an animated sequence to.
 * \param spriteIndex (integer): Sprite index. (`null` to disable)
 * \param animationIndex (integer): Animation index in sprite.
 * \ns Scene
 */
VMValue Scene_SetTileAnimSequenceFromSprite(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	int tileID = GET_ARG(0, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	ISprite* sprite = nullptr;
	if (!IS_NULL(args[1])) {
		sprite = GET_ARG(1, GetSprite);
	}

	int animationIndex = GET_ARG(2, GetInteger);

	Tileset* tileset = Scene::GetTileset(tileID);
	if (tileset) {
		tileset->AddTileAnimSequence(
			tileID, &Scene::TileSpriteInfos[tileID], sprite, animationIndex);
	}

	return NULL_VAL;
}
/***
 * Scene.SetTileAnimSequencePaused
 * \desc Pauses a tile ID animation sequence.
 * \param tileID (integer): The tile ID.
 * \param isPaused (boolean): Whether the animation is paused.
 * \ns Scene
 */
VMValue Scene_SetTileAnimSequencePaused(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	bool isPaused = GET_ARG(1, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		animator->Paused = isPaused;
	}

	return NULL_VAL;
}
/***
 * Scene.SetTileAnimSequenceSpeed
 * \desc Changes the speed of a tile ID animation sequence.
 * \param tileID (integer): The tile ID.
 * \param speed (decimal): The frame speed.
 * \ns Scene
 */
VMValue Scene_SetTileAnimSequenceSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	float speed = GET_ARG(1, GetDecimal);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		if (speed < 0.0) {
			speed = 0.0;
		}
		animator->Speed = speed;
	}

	return NULL_VAL;
}
/***
 * Scene.SetTileAnimSequenceFrame
 * \desc Sets the current frame of a tile ID animation sequence.
 * \param tileID (integer): The tile ID.
 * \param index (integer): The frame index.
 * \ns Scene
 */
VMValue Scene_SetTileAnimSequenceFrame(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	int frameIndex = GET_ARG(1, GetInteger);
	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size()) {
		return NULL_VAL;
	}

	TileAnimator* animator = Scene::GetTileAnimator(tileID);
	if (animator) {
		animator->SetAnimation(animator->AnimationIndex, frameIndex);
	}

	return NULL_VAL;
}
/***
 * Scene.SetTilesetPaletteIndex
 * \desc Sets the palette index of the specified tileset.
 * \param tilesetID (integer): The tileset index.
 * \param paletteIndex (integer): The palette index.
 * \ns Scene
 */
VMValue Scene_SetTilesetPaletteIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int palIndex = GET_ARG(1, GetInteger);
	CHECK_TILESET_INDEX
	CHECK_PALETTE_INDEX(palIndex);
	Scene::Tilesets[index].PaletteID = (unsigned)palIndex;
	return NULL_VAL;
}
#undef CHECK_TILESET_INDEX
/***
 * Scene.SetLayerVisible
 * \desc Sets the visibility of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param isVisible (boolean): Whether the layer can be seen.
 * \ns Scene
 */
VMValue Scene_SetLayerVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int visible = GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].Visible = visible;
	return NULL_VAL;
}
/***
 * Scene.SetLayerCollidable
 * \desc Sets whether the specified layer's tiles can be collided with.
 * \param layerIndex (integer): Index of layer.
 * \param isVisible (boolean): Whether the layer can be collided with.
 * \ns Scene
 */
VMValue Scene_SetLayerCollidable(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int visible = GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (visible) {
		Scene::Layers[index].Flags |= SceneLayer::FLAGS_COLLIDEABLE;
	}
	else {
		Scene::Layers[index].Flags &= ~SceneLayer::FLAGS_COLLIDEABLE;
	}
	return NULL_VAL;
}
/***
 * Scene.SetLayerInternalSize
 * \desc Do not use this.
 * \ns Scene
 */
VMValue Scene_SetLayerInternalSize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int index = GET_ARG(0, GetInteger);
	int w = GET_ARG(1, GetInteger);
	int h = GET_ARG(2, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (w > 0) {
		Scene::Layers[index].Width = w;
	}
	if (h > 0) {
		Scene::Layers[index].Height = h;
	}
	return NULL_VAL;
}
/***
 * Scene.SetLayerOffsetPosition
 * \desc Sets the camera offset position of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param offsetX (number): Offset X position.
 * \param offsetY (number): Offset Y position.
 * \ns Scene
 */
VMValue Scene_SetLayerOffsetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int index = GET_ARG(0, GetInteger);
	float offsetX = GET_ARG(1, GetDecimal);
	float offsetY = GET_ARG(2, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].OffsetX = offsetX;
	Scene::Layers[index].OffsetY = offsetY;
	return NULL_VAL;
}
/***
 * Scene.SetLayerOffsetX
 * \desc Sets the camera offset X value of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param offsetX (number): Offset X position.
 * \ns Scene
 */
VMValue Scene_SetLayerOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	float offsetX = GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].OffsetX = offsetX;
	return NULL_VAL;
}
/***
 * Scene.SetLayerOffsetY
 * \desc Sets the camera offset Y value of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param offsetY (number): Offset Y position.
 * \ns Scene
 */
VMValue Scene_SetLayerOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	float offsetY = GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].OffsetY = offsetY;
	return NULL_VAL;
}
/***
 * Scene.SetLayerDrawGroup
 * \desc Sets the draw group of the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param drawGroup (integer): Number between 0 to the result of <ref Scene.GetDrawGroupCount>.
 * \ns Scene
 */
VMValue Scene_SetLayerDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int drawGroup = GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		OUT_OF_RANGE_ERROR("Draw group", drawGroup, 0, MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	Scene::GetDrawGroup(drawGroup); // In case it doesn't exist already
	Scene::Layers[index].DrawGroup = drawGroup;
	return NULL_VAL;
}
/***
 * Scene.SetLayerDrawBehavior
 * \desc Sets the parallax direction of the layer.
 * \param layerIndex (integer): Index of layer.
 * \param drawBehavior (<ref DrawBehavior_*>): The draw behavior.
 * \ns Scene
 */
VMValue Scene_SetLayerDrawBehavior(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int drawBehavior = GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].DrawBehavior = drawBehavior;
	return NULL_VAL;
}
/***
 * Scene.SetLayerRepeat
 * \desc Sets whether the specified layer repeats.
 * \param layerIndex (integer): Index of layer.
 * \param doesRepeat (boolean): Whether the layer repeats.
 * \ns Scene
 */
VMValue Scene_SetLayerRepeat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	bool doesRepeat = !!GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (doesRepeat) {
		Scene::Layers[index].Flags |=
			SceneLayer::FLAGS_REPEAT_X | SceneLayer::FLAGS_REPEAT_Y;
	}
	else {
		Scene::Layers[index].Flags &=
			~(SceneLayer::FLAGS_REPEAT_X | SceneLayer::FLAGS_REPEAT_Y);
	}
	return NULL_VAL;
}
/***
 * Scene.SetLayerHorizontalRepeat
 * \desc Sets whether the specified layer repeats horizontally.
 * \param layerIndex (integer): Index of layer.
 * \param doesRepeat (boolean): Whether the layer repeats horizontally.
 * \ns Scene
 */
VMValue Scene_SetLayerHorizontalRepeat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	bool doesRepeat = !!GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (doesRepeat) {
		Scene::Layers[index].Flags |= SceneLayer::FLAGS_REPEAT_X;
	}
	else {
		Scene::Layers[index].Flags &= ~SceneLayer::FLAGS_REPEAT_X;
	}
	return NULL_VAL;
}
/***
 * Scene.SetLayerVerticalRepeat
 * \desc Sets whether the specified layer repeats vertically.
 * \param layerIndex (integer): Index of layer.
 * \param doesRepeat (boolean): Whether the layer repeats vertically.
 * \ns Scene
 */
VMValue Scene_SetLayerVerticalRepeat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	bool doesRepeat = !!GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (doesRepeat) {
		Scene::Layers[index].Flags |= SceneLayer::FLAGS_REPEAT_Y;
	}
	else {
		Scene::Layers[index].Flags &= ~SceneLayer::FLAGS_REPEAT_Y;
	}
	return NULL_VAL;
}
/***
 * Scene.SetDrawGroupCount
 * \desc Sets the amount of draw groups in the active scene.
 * \param count (integer): Draw group count.
 * \deprecated
 * \ns Scene
 */
VMValue Scene_SetDrawGroupCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int count = GET_ARG(0, GetInteger);
	if (count < 1) {
		THROW_ERROR("Draw group count cannot be lower than 1.");
		return NULL_VAL;
	}
	else if (count >= MAX_PRIORITY_PER_LAYER) {
		THROW_ERROR(
			"Draw group count cannot be higher than %d.", MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	Scene::SetPriorityPerLayer(count);
	return NULL_VAL;
}
/***
 * Scene.SetDrawGroupEntityDepthSorting
 * \desc Sets the specified draw group to sort objects by depth.
 * \param drawGroup (integer): Number between 0 to the result of <ref Scene.GetDrawGroupCount>.
 * \param useEntityDepth (boolean): Whether to sort objects by depth.
 * \ns Scene
 */
VMValue Scene_SetDrawGroupEntityDepthSorting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int drawGroup = GET_ARG(0, GetInteger);
	bool useEntityDepth = !!GET_ARG(1, GetInteger);
	if (drawGroup < 0 || drawGroup >= MAX_PRIORITY_PER_LAYER) {
		OUT_OF_RANGE_ERROR("Draw group", drawGroup, 0, MAX_PRIORITY_PER_LAYER - 1);
		return NULL_VAL;
	}
	DrawGroupList* drawGroupList = Scene::GetDrawGroup(drawGroup);
	if (!drawGroupList->EntityDepthSortingEnabled && useEntityDepth) {
		drawGroupList->NeedsSorting = true;
	}
	drawGroupList->EntityDepthSortingEnabled = useEntityDepth;
	return NULL_VAL;
}
/***
 * Scene.SetLayerBlend
 * \desc Sets whether to use color and alpha blending on this layer.
 * \param layerIndex (integer): Index of layer.
 * \param doBlend (boolean): Whether to use blending.
 * \paramOpt blendMode (<ref BlendMode_*>): The desired blend mode.
 * \ns Scene
 */
VMValue Scene_SetLayerBlend(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].Blending = !!GET_ARG(1, GetInteger);
	Scene::Layers[index].BlendMode = argCount >= 3 ? GET_ARG(2, GetInteger) : BlendMode_NORMAL;
	return NULL_VAL;
}
/***
 * Scene.SetLayerOpacity
 * \desc Sets the opacity of the layer.
 * \param layerIndex (integer): Index of layer.
 * \param opacity (decimal): Opacity from 0.0 to 1.0.
 * \ns Scene
 */
VMValue Scene_SetLayerOpacity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	float opacity = GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	if (opacity < 0.0) {
		THROW_ERROR("Opacity cannot be lower than 0.0.");
	}
	else if (opacity > 1.0) {
		THROW_ERROR("Opacity cannot be higher than 1.0.");
	}
	Scene::Layers[index].Opacity = opacity;
	return NULL_VAL;
}
/***
 * Scene.SetLayerShader
 * \desc Sets a shader for the layer.
 * \param layerIndex (integer): Index of layer.
 * \param shader (Shader): The shader, or `null` to unset the shader.
 * \ns Scene
 */
VMValue Scene_SetLayerShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	ObjShader* objShader = GET_ARG(1, GetShader);

	CHECK_SCENE_LAYER_INDEX(index);

	if (IS_NULL(args[1])) {
		Scene::Layers[index].CurrentShader = nullptr;
		return NULL_VAL;
	}

	Shader* shader = (Shader*)objShader->ShaderPtr;
	if (shader == nullptr) {
		THROW_ERROR("Shader has been deleted!");
		return NULL_VAL;
	}

	try {
		shader->Validate();

		Scene::Layers[index].CurrentShader = shader;
	} catch (const std::runtime_error& error) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false, "%s", error.what());
		return NULL_VAL;
	}

	return NULL_VAL;
}
/***
 * Scene.SetLayerUsePaletteIndexLines
 * \desc Enables or disables the use of the global palette index table for the specified layer.
 * \param layerIndex (integer): Index of layer.
 * \param usePaletteIndexLines (boolean): Whether the layer is using the global palette index table.
 * \ns Scene
 */
VMValue Scene_SetLayerUsePaletteIndexLines(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int usePaletteIndexLines = !!GET_ARG(1, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].UsePaletteIndexLines = usePaletteIndexLines;
	return NULL_VAL;
}
/***
 * Scene.SetLayerScroll
 * \desc Sets the scroll values of the layer. (Horizontal Parallax = Up/Down values, Vertical Parallax = Left/Right values)
 * \param layerIndex (integer): Index of layer.
 * \param relative (decimal): How much to move the layer relative to the camera. (0.0 = no movement, 1.0 = moves opposite to speed of camera, 2.0 = moves twice the speed opposite to camera)
 * \param constant (decimal): How many pixels to move the layer per frame.
 * \ns Scene
 */
VMValue Scene_SetLayerScroll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int index = GET_ARG(0, GetInteger);
	float relative = GET_ARG(1, GetDecimal);
	float constant = GET_ARG(2, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].RelativeY = relative;
	Scene::Layers[index].ConstantY = constant;
	return NULL_VAL;
}
struct BufferedScrollInfo {
	float relative;
	float constant;
	bool canDeform;
};
Uint8* BufferedScrollLines = NULL;
int BufferedScrollLinesMax = 0;
int BufferedScrollSetupLayer = -1;
std::vector<BufferedScrollInfo> BufferedScrollInfos;
/***
 * Scene.SetLayerSetParallaxLinesBegin
 * \desc Begins setup for changing the parallax lines.
 * \param layerIndex (integer): Index of layer.
 * \ns Scene
 */
VMValue Scene_SetLayerSetParallaxLinesBegin(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (BufferedScrollLines) {
		THROW_ERROR("Did not end scroll line setup before beginning new one");
		Memory::Free(BufferedScrollLines);
	}
	BufferedScrollLinesMax = Scene::Layers[index].HeightData * Scene::TileWidth;
	BufferedScrollLines = (Uint8*)Memory::Malloc(BufferedScrollLinesMax);
	BufferedScrollSetupLayer = index;
	BufferedScrollInfos.clear();
	return NULL_VAL;
}
/***
 * Scene.SetLayerSetParallaxLines
 * \desc Set the parallax lines.
 * \param lineStart (integer): Start line.
 * \param lineEnd (integer): End line.
 * \param relative (number): How much to move the scroll line relative to the camera. (0.0 = no movement, 1.0 = moves opposite to speed of camera, 2.0 = moves twice the speed opposite to camera)
 * \param constant (number): How many pixels to move the layer per frame.
 * \param canDeform (boolean): Whether the parallax lines can be deformed.
 * \ns Scene
 */
VMValue Scene_SetLayerSetParallaxLines(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(5);
	int lineStart = GET_ARG(0, GetInteger);
	int lineEnd = GET_ARG(1, GetInteger);
	float relative = GET_ARG(2, GetDecimal);
	float constant = GET_ARG(3, GetDecimal);
	bool canDeform = !!GET_ARG(4, GetInteger);

	BufferedScrollInfo info;
	info.relative = relative;
	info.constant = constant;
	info.canDeform = canDeform;

	// Check to see if these scroll values are used, if not, add them.
	int scrollIndex = (int)BufferedScrollInfos.size();
	size_t setupCount = BufferedScrollInfos.size();
	if (setupCount) {
		scrollIndex = -1;
		for (size_t i = 0; i < setupCount; i++) {
			BufferedScrollInfo setup = BufferedScrollInfos[i];
			if (setup.relative == relative && setup.constant == constant &&
				setup.canDeform == canDeform) {
				scrollIndex = (int)i;
				break;
			}
		}
		if (scrollIndex < 0) {
			scrollIndex = (int)setupCount;
			BufferedScrollInfos.push_back(info);
		}
	}
	else {
		BufferedScrollInfos.push_back(info);
	}
	// Set line values.
	for (int i = lineStart > 0 ? lineStart : 0; i < lineEnd && i < BufferedScrollLinesMax;
		i++) {
		BufferedScrollLines[i] = (Uint8)scrollIndex;
	}
	return NULL_VAL;
}
/***
 * Scene.SetLayerSetParallaxLinesEnd
 * \desc Ends setup for changing the parallax lines and submits the changes.
 * \ns Scene
 */
VMValue Scene_SetLayerSetParallaxLinesEnd(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!BufferedScrollLines) {
		THROW_ERROR("Did not start scroll line setup before ending.");
		return NULL_VAL;
	}

	if (BufferedScrollSetupLayer < 0 || BufferedScrollSetupLayer >= (int)Scene::Layers.size()) {
		THROW_ERROR("Invalid layer set in scroll line setup.");
		return NULL_VAL;
	}

	SceneLayer* layer = &Scene::Layers[BufferedScrollSetupLayer];
	Memory::Free(layer->ScrollInfos);
	Memory::Free(layer->ScrollIndexes);

	layer->ScrollInfoCount = (int)BufferedScrollInfos.size();
	layer->ScrollInfos =
		(ScrollingInfo*)Memory::Malloc(layer->ScrollInfoCount * sizeof(ScrollingInfo));
	for (int g = 0; g < layer->ScrollInfoCount; g++) {
		layer->ScrollInfos[g].RelativeParallax = BufferedScrollInfos[g].relative;
		layer->ScrollInfos[g].ConstantParallax = BufferedScrollInfos[g].constant;
		layer->ScrollInfos[g].CanDeform = BufferedScrollInfos[g].canDeform;
	}

	int length16 = layer->HeightData * 16;
	if (layer->WidthData > layer->HeightData) {
		length16 = layer->WidthData * 16;
	}

	layer->UsingScrollIndexes = true;
	layer->ScrollIndexes = (Uint8*)Memory::Calloc(length16, sizeof(Uint8));
	memcpy(layer->ScrollIndexes, BufferedScrollLines, BufferedScrollLinesMax);

	// Cleanup
	BufferedScrollInfos.clear();
	Memory::Free(BufferedScrollLines);
	BufferedScrollLines = NULL;
	return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeforms
 * \desc Sets the tile deforms of the layer at the specified index.
 * \param layerIndex (integer): Index of layer.
 * \param deformIndex (integer): Index of deform value.
 * \param deformA (number): Deform value above the Deform Split Line.
 * \param deformB (number): Deform value below the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeforms(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int index = GET_ARG(0, GetInteger);
	int lineIndex = GET_ARG(1, GetInteger);
	int deformA = (int)(GET_ARG(2, GetDecimal));
	int deformB = (int)(GET_ARG(3, GetDecimal));
	const int maxDeformLineMask = MAX_DEFORM_LINES - 1;

	CHECK_SCENE_LAYER_INDEX(index);

	lineIndex &= maxDeformLineMask;
	Scene::Layers[index].DeformSetA[lineIndex] = deformA;
	Scene::Layers[index].DeformSetB[lineIndex] = deformB;
	Scene::Layers[index].UsingScrollIndexes = true;
	return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeformSplitLine
 * \desc Sets the position of the Deform Split Line.
 * \param layerIndex (integer): Index of layer.
 * \param deformPosition (number): The position on screen where the Deform Split Line should be. (Y when horizontal parallax, X when vertical.)
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeformSplitLine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int deformPosition = (int)GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].DeformSplitLine = deformPosition;
	Scene::Layers[index].UsingScrollIndexes = true;
	return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeformOffsets
 * \desc Sets the position of the Deform Split Line.
 * \param layerIndex (integer): Index of layer.
 * \param deformAOffset (number): Offset for the deforms above the Deform Split Line.
 * \param deformBOffset (number): Offset for the deforms below the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeformOffsets(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int index = GET_ARG(0, GetInteger);
	int deformAOffset = (int)GET_ARG(1, GetDecimal);
	int deformBOffset = (int)GET_ARG(2, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].DeformOffsetA = deformAOffset;
	Scene::Layers[index].DeformOffsetB = deformBOffset;
	Scene::Layers[index].UsingScrollIndexes = true;
	return NULL_VAL;
}
/***
 * Scene.SetLayerDeformOffsetA
 * \desc Sets the tile deform offset above the Deform Split Line of the layer.
 * \param layerIndex (integer): Index of layer.
 * \param deformA (number): Deform value above the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerDeformOffsetA(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int deformA = (int)GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].DeformOffsetA = deformA;
	Scene::Layers[index].UsingScrollIndexes = true;
	return NULL_VAL;
}
/***
 * Scene.SetLayerDeformOffsetB
 * \desc Sets the tile deform offset below the Deform Split Line of the layer.
 * \param layerIndex (integer): Index of layer.
 * \param deformA (number): Deform value below the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerDeformOffsetB(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	int deformB = (int)GET_ARG(1, GetDecimal);
	CHECK_SCENE_LAYER_INDEX(index);
	Scene::Layers[index].DeformOffsetB = deformB;
	Scene::Layers[index].UsingScrollIndexes = true;
	return NULL_VAL;
}
/***
 * Scene.SetLayerCustomScanlineFunction
 * \desc Sets the function to be used for generating custom tile scanlines.
 * \param layerIndex (integer): Index of layer.
 * \param function (function): Function to be used before tile drawing for generating scanlines. (Use `null` to reset functionality.)
 * \ns Scene
 */
VMValue Scene_SetLayerCustomScanlineFunction(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (args[0].Type == VAL_NULL) {
		Scene::Layers[index].UsingCustomScanlineFunction = false;
	}
	else {
		ObjFunction* function = GET_ARG(1, GetFunction);
		Scene::Layers[index].CustomScanlineFunction = *function;
		Scene::Layers[index].UsingCustomScanlineFunction = true;
	}
	return NULL_VAL;
}
/***
 * Scene.SetTileScanline
 * \desc Sets the tile scanline (for use only inside a Custom Scanline Function).
 * \param scanline (integer): Index of scanline to edit.
 * \param srcX (number):
 * \param srcY (number):
 * \param deltaX (number):
 * \param deltaY (number):
 * \paramOpt opacity (decimal):
 * \paramOpt maxHorzCells (number):
 * \paramOpt maxVertCells (number):
 * \ns Scene
 */
VMValue Scene_SetTileScanline(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(5);
	int scanlineIndex = GET_ARG(0, GetInteger);

	TileScanLine* scanLine = &Graphics::TileScanLineBuffer[scanlineIndex];
	scanLine->SrcX = (Sint64)(GET_ARG(1, GetDecimal) * 0x10000);
	scanLine->SrcY = (Sint64)(GET_ARG(2, GetDecimal) * 0x10000);
	scanLine->DeltaX = (Sint64)(GET_ARG(3, GetDecimal) * 0x10000);
	scanLine->DeltaY = (Sint64)(GET_ARG(4, GetDecimal) * 0x10000);

	int opacity = 0xFF;
	if (argCount >= 6) {
		opacity = (int)(GET_ARG(5, GetDecimal) * 0xFF);
		if (opacity < 0) {
			opacity = 0;
		}
		else if (opacity > 0xFF) {
			opacity = 0xFF;
		}
	}
	scanLine->Opacity = opacity;

	scanLine->MaxHorzCells = argCount >= 7 ? GET_ARG(6, GetInteger) : 0;
	scanLine->MaxVertCells = argCount >= 8 ? GET_ARG(7, GetInteger) : 0;

	return NULL_VAL;
}
/***
 * Scene.SetLayerCustomRenderFunction
 * \desc Sets the function to be used for rendering a specific layer.
 * \param layerIndex (integer): Index of layer.
 * \param function (function): Function to call to render the layer. (Use `null` to reset functionality.)
 * \ns Scene
 */
VMValue Scene_SetLayerCustomRenderFunction(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int index = GET_ARG(0, GetInteger);
	CHECK_SCENE_LAYER_INDEX(index);
	if (args[0].Type == VAL_NULL) {
		Scene::Layers[index].UsingCustomRenderFunction = false;
	}
	else {
		ObjFunction* function = GET_ARG(1, GetFunction);
		Scene::Layers[index].CustomRenderFunction = *function;
		Scene::Layers[index].UsingCustomRenderFunction = true;
	}
	return NULL_VAL;
}
/***
 * Scene.SetObjectViewRender
 * \desc Sets whether entities can render on the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param enableViewRender (boolean): Whether entities can render on the specified view.
 * \ns Scene
 */
VMValue Scene_SetObjectViewRender(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int enabled = !!GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();

	int viewRenderFlag = 1 << view_index;
	if (enabled) {
		Scene::ObjectViewRenderFlag |= viewRenderFlag;
	}
	else {
		Scene::ObjectViewRenderFlag &= ~viewRenderFlag;
	}

	return NULL_VAL;
}
/***
 * Scene.SetTileViewRender
 * \desc Sets whether tiles can render on the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param enableViewRender (boolean): Whether tiles can render on the specified view.
 * \ns Scene
 */
VMValue Scene_SetTileViewRender(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int enabled = !!GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();

	int viewRenderFlag = 1 << view_index;
	if (enabled) {
		Scene::TileViewRenderFlag |= viewRenderFlag;
	}
	else {
		Scene::TileViewRenderFlag &= ~viewRenderFlag;
	}

	return NULL_VAL;
}
// #endregion

// #region SceneList
/***
 * SceneList.Get
 * \desc Gets the scene path for the specified category and entry.
 * \param category (string): The category.
 * \param entry (string): The entry.
 * \return string Returns a string value.
 * \ns SceneList
 */
VMValue SceneList_Get(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	int categoryID = -1;
	int entryID = -1;

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		if (categoryID < 0) {
			return NULL_VAL;
		}
	}

	if (IS_INTEGER(args[1])) {
		entryID = GET_ARG(1, GetInteger);
	}
	else {
		entryID = SceneInfo::GetEntryID(categoryID, GET_ARG(1, GetString));
		if (entryID < 0) {
			return NULL_VAL;
		}
	}

	return ReturnString(SceneInfo::GetFilename(categoryID, entryID));
}
/***
 * SceneList.GetEntryID
 * \desc Gets the entry ID for the specified category and entry name.
 * \param categoryName (string): The category name.
 * \param entryName (string): The entry name.
 * \return integer Returns the entry ID, or `-1` if not found.
 * \ns SceneList
 */
VMValue SceneList_GetEntryID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* categoryName = GET_ARG(0, GetString);
	char* entryName = GET_ARG(1, GetString);
	int entryID = SceneInfo::GetEntryID(categoryName, entryName);
	if (entryID < 0) {
		return INTEGER_VAL(-1);
	}
	return INTEGER_VAL(entryID);
}
/***
 * SceneList.GetCategoryID
 * \desc Gets the category ID for the specified category name.
 * \param categoryName (string): The category name.
 * \return integer Returns the category ID, or `-1` if not found.
 * \ns SceneList
 */
VMValue SceneList_GetCategoryID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* categoryName = GET_ARG(0, GetString);
	int categoryID = SceneInfo::GetCategoryID(categoryName);
	return INTEGER_VAL(categoryID);
}
/***
 * SceneList.GetEntryName
 * \desc Gets the entry name for the specified category and entry.
 * \param category (string): The category.
 * \param entryID (integer): The entry ID.
 * \return string Returns the entry name.
 * \ns SceneList
 */
VMValue SceneList_GetEntryName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	int categoryID = -1;
	int entryID = GET_ARG(1, GetInteger);

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
	}

	if (!SceneInfo::IsEntryValid(categoryID, entryID)) {
		return NULL_VAL;
	}

	std::string name = SceneInfo::GetName(categoryID, entryID);
	return ReturnString(name);
}
/***
 * SceneList.GetCategoryName
 * \desc Gets the category name for the specified category ID.
 * \param categoryID (integer): The category ID.
 * \return string Returns the category name, or `-1` if not valid.
 * \ns SceneList
 */
VMValue SceneList_GetCategoryName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int categoryID = GET_ARG(0, GetInteger);
	if (!SceneInfo::IsCategoryValid(categoryID)) {
		return NULL_VAL;
	}
	return ReturnString(SceneInfo::Categories[categoryID].Name);
}
/***
 * SceneList.GetEntryProperty
 * \desc Gets a property for an entry.
 * \param category (string): The category.
 * \param entry (string): The entry.
 * \param property (string): The property.
 * \return string Returns a string value, or `null` if the entry has no such property.
 * \ns SceneList
 */
VMValue SceneList_GetEntryProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	int categoryID = -1;
	int entryID = -1;

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		if (categoryID < 0) {
			return NULL_VAL;
		}
	}

	if (IS_INTEGER(args[1])) {
		entryID = GET_ARG(1, GetInteger);
	}
	else {
		entryID = SceneInfo::GetEntryID(categoryID, GET_ARG(1, GetString));
		if (entryID < 0) {
			return NULL_VAL;
		}
	}

	char* propertyName = GET_ARG(2, GetString);

	char* property = SceneInfo::GetEntryProperty(categoryID, entryID, propertyName);
	if (property == nullptr) {
		return NULL_VAL;
	}

	return ReturnString(property);
}
/***
 * SceneList.GetCategoryProperty
 * \desc Gets a property for a category.
 * \param category (string): The category.
 * \param property (string): The property.
 * \return string Returns a string value, or `null` if the category has no such property.
 * \ns SceneList
 */
VMValue SceneList_GetCategoryProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	int categoryID = -1;

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		if (categoryID < 0) {
			return NULL_VAL;
		}
	}

	char* propertyName = GET_ARG(1, GetString);

	char* property = SceneInfo::GetCategoryProperty(categoryID, propertyName);
	if (property == nullptr) {
		return NULL_VAL;
	}

	return ReturnString(property);
}
/***
 * SceneList.HasEntryProperty
 * \desc Checks if a given property exists in the entry.
 * \param category (string): The category.
 * \param entry (string): The entry.
 * \param property (string): The property.
 * \return boolean Returns whether the property exists.
 * \ns SceneList
 */
VMValue SceneList_HasEntryProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	int categoryID = -1;
	int entryID = -1;

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		if (categoryID < 0) {
			return INTEGER_VAL(false);
		}
	}

	if (IS_INTEGER(args[1])) {
		entryID = GET_ARG(1, GetInteger);
	}
	else {
		entryID = SceneInfo::GetEntryID(categoryID, GET_ARG(1, GetString));
		if (entryID < 0) {
			return INTEGER_VAL(false);
		}
	}

	return INTEGER_VAL(
		!!SceneInfo::HasEntryProperty(categoryID, entryID, GET_ARG(2, GetString)));
}
/***
 * SceneList.HasCategoryProperty
 * \desc Checks if a given property exists in the category.
 * \param category (string): The category.
 * \param property (string): The property.
 * \return boolean Returns whether the property exists.
 * \ns SceneList
 */
VMValue SceneList_HasCategoryProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	int categoryID = -1;

	if (IS_INTEGER(args[0])) {
		categoryID = GET_ARG(0, GetInteger);
	}
	else {
		categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		if (categoryID < 0) {
			return INTEGER_VAL(false);
		}
	}

	return INTEGER_VAL(!!SceneInfo::HasCategoryProperty(categoryID, GET_ARG(1, GetString)));
}
/***
 * SceneList.GetCategoryCount
 * \desc Gets the amount of categories in the scene list.
 * \return integer Returns an integer value.
 * \ns SceneList
 */
VMValue SceneList_GetCategoryCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL((int)SceneInfo::Categories.size());
}
/***
 * SceneList.GetSceneCount
 * \desc Gets the amount of scenes.
 * \return integer Returns the total amount of scenes.
 * \ns SceneList
 */
/***
 * SceneList.GetSceneCount
 * \desc Gets the amount of scenes in a category.
 * \param categoryName (string): The category name.
 * \return integer Returns the number of scenes in the category.
 * \ns SceneList
 */
VMValue SceneList_GetSceneCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(0);
	if (argCount >= 1) {
		int categoryID = -1;
		if (IS_INTEGER(args[0])) {
			categoryID = GET_ARG(0, GetInteger);
		}
		else {
			categoryID = SceneInfo::GetCategoryID(GET_ARG(0, GetString));
		}
		if (!SceneInfo::IsCategoryValid(categoryID)) {
			return INTEGER_VAL(0);
		}
		return INTEGER_VAL((int)SceneInfo::Categories[categoryID].Entries.size());
	}
	else {
		return INTEGER_VAL(SceneInfo::NumTotalScenes);
	}
}
// #endregion

// #region Scene3D
/***
 * Scene3D.Create
 * \desc Creates a 3D scene.
 * \param unloadPolicy (integer): Whether to delete the 3D scene at the end of the current Scene, or the game end.
 * \return integer The index of the created 3D scene.
 * \ns Scene3D
 */
VMValue Scene3D_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 unloadPolicy = GET_ARG(0, GetInteger);
	Uint32 scene3DIndex = Graphics::CreateScene3D(unloadPolicy);
	if (scene3DIndex == 0xFFFFFFFF) {
		THROW_ERROR("No more 3D scenes available.");
		return NULL_VAL;
	}
	return INTEGER_VAL((int)scene3DIndex);
}
/***
 * Scene3D.Delete
 * \desc Deletes a 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene to delete.
 * \ns Scene3D
 */
VMValue Scene3D_Delete(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	if (scene3DIndex < 0 || scene3DIndex >= MAX_3D_SCENES) {
		OUT_OF_RANGE_ERROR("Scene3D", scene3DIndex, 0, MAX_3D_SCENES - 1);
		return NULL_VAL;
	}
	Graphics::DeleteScene3D(scene3DIndex);
	return NULL_VAL;
}
/***
 * Scene3D.SetDrawMode
 * \desc Sets the draw mode of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param drawMode (<ref DrawMode_*>): The type of drawing to use for the vertices in the 3D scene.
 * \ns Scene3D
 */
VMValue Scene3D_SetDrawMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	Uint32 drawMode = GET_ARG(1, GetInteger);
	GET_SCENE_3D();
	scene3D->DrawMode = drawMode;
	return NULL_VAL;
}
/***
 * Scene3D.SetFaceCullMode
 * \desc Sets the face culling mode of the 3D scene. (hardware-renderer only)
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param cullMode (<ref FaceCull_*>): The type of face culling to use for the vertices in the 3D scene.
 * \ns Scene3D
 */
VMValue Scene3D_SetFaceCullMode(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	int cullMode = GET_ARG(1, GetInteger);
	GET_SCENE_3D();
	if (cullMode < (int)FaceCull_None || cullMode > (int)FaceCull_Front) {
		THROW_ERROR("Invalid face cull mode %d.", cullMode);
		return NULL_VAL;
	}
	scene3D->FaceCullMode = cullMode;
	return NULL_VAL;
}
/***
 * Scene3D.SetFieldOfView
 * \desc Sets the field of view of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param fieldOfView (matrix): The field of view value.
 * \ns Scene3D
 */
VMValue Scene3D_SetFieldOfView(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float fieldOfView = GET_ARG(1, GetDecimal);
	GET_SCENE_3D();
	scene3D->FOV = fieldOfView;
	return NULL_VAL;
}
/***
 * Scene3D.SetFarClippingPlane
 * \desc Sets the far clipping plane of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param farClippingPlane (matrix): The far clipping plane value.
 * \ns Scene3D
 */
VMValue Scene3D_SetFarClippingPlane(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float farClippingPlane = GET_ARG(1, GetDecimal);
	GET_SCENE_3D();
	scene3D->FarClippingPlane = farClippingPlane;
	return NULL_VAL;
}
/***
 * Scene3D.SetNearClippingPlane
 * \desc Sets the near clipping plane of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param farClippingPlane (matrix): The near clipping plane value.
 * \ns Scene3D
 */
VMValue Scene3D_SetNearClippingPlane(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float nearClippingPlane = GET_ARG(1, GetDecimal);
	GET_SCENE_3D();
	scene3D->NearClippingPlane = nearClippingPlane;
	return NULL_VAL;
}
/***
 * Scene3D.SetViewMatrix
 * \desc Sets the view matrix of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param viewMatrix (matrix): The view matrix.
 * \ns Scene3D
 */
VMValue Scene3D_SetViewMatrix(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	ObjArray* viewMatrix = GET_ARG(1, GetArray);

	Matrix4x4 matrix4x4;

	for (int i = 0; i < 16; i++) {
		matrix4x4.Values[i] = AS_DECIMAL((*viewMatrix->Values)[i]);
	}

	GET_SCENE_3D();
	scene3D->SetViewMatrix(&matrix4x4);
	return NULL_VAL;
}
/***
 * Scene3D.SetCustomProjectionMatrix
 * \desc Sets a custom projection matrix.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param projMatrix (matrix): The projection matrix.
 * \ns Scene3D
 */
VMValue Scene3D_SetCustomProjectionMatrix(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	ObjArray* projMatrix;
	if (IS_NULL(args[1])) {
		GET_SCENE_3D();
		scene3D->SetCustomProjectionMatrix(nullptr);
		return NULL_VAL;
	}
	else {
		projMatrix = GET_ARG(1, GetArray);
	}

	Matrix4x4 matrix4x4;
	int arrSize = (int)projMatrix->Values->size();
	if (arrSize != 16) {
		THROW_ERROR(
			"Matrix has unexpected size (expected 16 elements, but has %d)", arrSize);
		return NULL_VAL;
	}

	// Yeah just copy it directly
	for (int i = 0; i < 16; i++) {
		matrix4x4.Values[i] = AS_DECIMAL((*projMatrix->Values)[i]);
	}

	GET_SCENE_3D();
	scene3D->SetCustomProjectionMatrix(&matrix4x4);
	return NULL_VAL;
}
/***
 * Scene3D.SetAmbientLighting
 * \desc Sets the ambient lighting of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param red (number): The red color value, bounded by 0.0 - 1.0.
 * \param green (number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (number): The blue color value, bounded by 0.0 - 1.0.
 * \ns Scene3D
 */
VMValue Scene3D_SetAmbientLighting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float r = Math::Clamp(GET_ARG(1, GetDecimal), 0.0f, 1.0f);
	float g = Math::Clamp(GET_ARG(2, GetDecimal), 0.0f, 1.0f);
	float b = Math::Clamp(GET_ARG(3, GetDecimal), 0.0f, 1.0f);
	GET_SCENE_3D();
	scene3D->SetAmbientLighting(r, g, b);
	return NULL_VAL;
}
/***
 * Scene3D.SetDiffuseLighting
 * \desc Sets the diffuse lighting of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param red (number): The red color value, bounded by 0.0 - 1.0.
 * \param green (number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (number): The blue color value, bounded by 0.0 - 1.0.
 * \ns Scene3D
 */
VMValue Scene3D_SetDiffuseLighting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float r = Math::Clamp(GET_ARG(1, GetDecimal), 0.0f, 1.0f);
	float g = Math::Clamp(GET_ARG(2, GetDecimal), 0.0f, 1.0f);
	float b = Math::Clamp(GET_ARG(3, GetDecimal), 0.0f, 1.0f);
	GET_SCENE_3D();
	scene3D->SetDiffuseLighting(r, g, b);
	return NULL_VAL;
}
/***
 * Scene3D.SetSpecularLighting
 * \desc Sets the specular lighting of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param red (number): The red color value, bounded by 0.0 - 1.0.
 * \param green (number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (number): The blue color value, bounded by 0.0 - 1.0.
 * \ns Scene3D
 */
VMValue Scene3D_SetSpecularLighting(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float r = Math::Clamp(GET_ARG(1, GetDecimal), 0.0f, 1.0f);
	float g = Math::Clamp(GET_ARG(2, GetDecimal), 0.0f, 1.0f);
	float b = Math::Clamp(GET_ARG(3, GetDecimal), 0.0f, 1.0f);
	GET_SCENE_3D();
	scene3D->SetSpecularLighting(r, g, b);
	return NULL_VAL;
}
/***
 * Scene3D.SetFogEquation
 * \desc Sets the fog equation of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param fogEquation (<ref FogEquation_*>): The fog equation to use.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogEquation(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	int fogEquation = GET_ARG(1, GetInteger);
	GET_SCENE_3D();
	if (fogEquation < (int)FogEquation_Linear || fogEquation > (int)FogEquation_Exp) {
		THROW_ERROR("Invalid fog equation %d.", fogEquation);
		return NULL_VAL;
	}
	scene3D->SetFogEquation((FogEquation)fogEquation);
	return NULL_VAL;
}
/***
 * Scene3D.SetFogStart
 * \desc Sets the near distance used in the linear equation of the 3D scene's fog.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param start (number): The start value.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogStart(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	GET_SCENE_3D();
	scene3D->SetFogStart(GET_ARG(1, GetDecimal));
	return NULL_VAL;
}
/***
 * Scene3D.SetFogEnd
 * \desc Sets the far distance used in the linear equation of the 3D scene's fog.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param end (number): The end value.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogEnd(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	GET_SCENE_3D();
	scene3D->SetFogEnd(GET_ARG(1, GetDecimal));
	return NULL_VAL;
}
/***
 * Scene3D.SetFogDensity
 * \desc Sets the density used in the exponential equation of the 3D scene's fog.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param density (number): The fog density.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogDensity(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	GET_SCENE_3D();
	scene3D->SetFogDensity(GET_ARG(1, GetDecimal));
	return NULL_VAL;
}
/***
 * Scene3D.SetFogColor
 * \desc Sets the fog color of the 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param red (number): The red color value, bounded by 0.0 - 1.0.
 * \param green (number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (number): The blue color value, bounded by 0.0 - 1.0.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogColor(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float r = Math::Clamp(GET_ARG(1, GetDecimal), 0.0f, 1.0f);
	float g = Math::Clamp(GET_ARG(2, GetDecimal), 0.0f, 1.0f);
	float b = Math::Clamp(GET_ARG(3, GetDecimal), 0.0f, 1.0f);
	GET_SCENE_3D();
	scene3D->SetFogColor(r, g, b);
	return NULL_VAL;
}
/***
 * Scene3D.SetFogSmoothness
 * \desc Sets the smoothness of the 3D scene's fog.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param smoothness (number): The smoothness, bounded by 0.0 - 1.0.
 * \ns Scene3D
 */
VMValue Scene3D_SetFogSmoothness(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float smoothness = Math::Clamp(GET_ARG(1, GetDecimal), 0.0f, 1.0f);
	GET_SCENE_3D();
	scene3D->SetFogSmoothness(smoothness);
	return NULL_VAL;
}
/***
 * Scene3D.SetPointSize
 * \desc Sets the point size of the 3D scene. (hardware-renderer only)
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \param pointSize (decimal): The point size.
 * \ns Scene3D
 */
VMValue Scene3D_SetPointSize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	float pointSize = !!GET_ARG(1, GetDecimal);
	GET_SCENE_3D();
	scene3D->PointSize = pointSize;
	return NULL_VAL;
}
/***
 * Scene3D.Clear
 * \desc Removes all previously drawn elements out of a 3D scene.
 * \param scene3DIndex (integer): The index of the 3D scene.
 * \ns Scene3D
 */
VMValue Scene3D_Clear(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 scene3DIndex = GET_ARG(0, GetInteger);
	if (scene3DIndex < 0 || scene3DIndex >= MAX_3D_SCENES) {
		OUT_OF_RANGE_ERROR("Scene3D", scene3DIndex, 0, MAX_3D_SCENES - 1);
		return NULL_VAL;
	}

	Graphics::ClearScene3D(scene3DIndex);
	return NULL_VAL;
}
#undef GET_SCENE_3D
// #endregion

// #region Serializer
/***
 * Serializer.WriteToStream
 * \desc Serializes a value into a stream.
 * \param stream (stream): The stream.
 * \param value (value): The value to serialize.
 * \ns Serializer
 */
VMValue Serializer_WriteToStream(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_WRITE_STREAM;

	Serializer serializer(stream->StreamPtr);
	serializer.Store(args[1]);

	return NULL_VAL;
}
/***
 * Serializer.ReadFromStream
 * \desc Deserializes a value from a stream.
 * \param stream (stream): The stream.
 * \return value The deserialized value.
 * \ns Serializer
 */
VMValue Serializer_ReadFromStream(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;

	Serializer serializer(stream->StreamPtr);
	return serializer.Retrieve();
}
// #endregion

// #region Settings
/***
 * Settings.Load
 * \desc Loads the config from the specified filename. Calling this does not save the current settings.
 * \param filename (string): Filepath of config.
 * \ns Settings
 */
VMValue Settings_Load(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::ReloadSettings(GET_ARG(0, GetString));
	return NULL_VAL;
}
/***
 * Settings.Save
 * \desc Saves the settings.
 * \ns Settings
 */
/***
 * Settings.Save
 * \desc Saves the settings with the specified filename.
 * \param filename (string): Filepath of config. This does not change the filepath of the current settings; use <ref Settings.SetFilename> to do that.
 * \ns Settings
 */
VMValue Settings_Save(int argCount, VMValue* args, Uint32 threadID) {
	if (argCount != 0) {
		CHECK_ARGCOUNT(1);
		Application::SaveSettings(GET_ARG(0, GetString));
	}
	else {
		Application::SaveSettings();
	}
	return NULL_VAL;
}
/***
 * Settings.SetFilename
 * \desc Sets the filepath of the settings.
 * \param filename (string): Filepath of config. This does not save the current settings.
 * \ns Settings
 */
VMValue Settings_SetFilename(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::SetSettingsFilename(GET_ARG(0, GetString));
	return NULL_VAL;
}
/***
 * Settings.GetString
 * \desc Looks for a property in a section, and returns its value, as a string.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property to look for.
 * \return string Returns the property as a string, or `null` if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}

	if (!Application::Settings->SectionExists(section)) {
		return NULL_VAL;
	}

	char const* result = Application::Settings->GetProperty(section, GET_ARG(1, GetString));
	if (!result) {
		return NULL_VAL;
	}

	return ReturnString(result);
}
/***
 * Settings.GetNumber
 * \desc Looks for a property in a section, and returns its value, as a number.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property to look for.
 * \return integer Returns the property as a number, or `null` if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetNumber(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		// THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	double result;
	if (!Application::Settings->GetDecimal(section, GET_ARG(1, GetString), &result)) {
		return NULL_VAL;
	}

	return DECIMAL_VAL((float)result);
}
/***
 * Settings.GetInteger
 * \desc Looks for a property in a section, and returns its value, as an integer.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property to look for.
 * \return integer Returns the property as an integer, or `null` if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		// THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	int result;
	if (!Application::Settings->GetInteger(section, GET_ARG(1, GetString), &result)) {
		return NULL_VAL;
	}

	return INTEGER_VAL(result);
}
/***
 * Settings.GetBool
 * \desc Looks for a property in a section, and returns its value, as a boolean.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property to look for.
 * \return boolean Returns the property as a boolean, or `null` if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetBool(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		// THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	bool result;
	if (!Application::Settings->GetBool(section, GET_ARG(1, GetString), &result)) {
		return NULL_VAL;
	}

	return INTEGER_VAL((int)result);
}
/***
 * Settings.SetString
 * \desc Sets a property in a section to a string value.
 * \param section (string): The section where the property resides. If the section doesn't exist, it will be created. If this is `null`, the global section is used instead.
 * \param property (string): The property to set.
 * \param value (string): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}

	Application::Settings->SetString(section, GET_ARG(1, GetString), GET_ARG(2, GetString));
	return NULL_VAL;
}
/***
 * Settings.SetNumber
 * \desc Sets a property in a section to a decimal value.
 * \param section (string): The section where the property resides. If the section doesn't exist, it will be created. If this is `null`, the global section is used instead.
 * \param property (string): The property to set.
 * \param value (number): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetNumber(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}

	Application::Settings->SetDecimal(section, GET_ARG(1, GetString), GET_ARG(2, GetDecimal));
	return NULL_VAL;
}
/***
 * Settings.SetInteger
 * \desc Sets a property in a section to an integer value.
 * \param section (string): The section where the property resides. If the section doesn't exist, it will be created. If this is `null`, the global section is used instead.
 * \param property (string): The property to set.
 * \param value (integer): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}

	Application::Settings->SetInteger(
		section, GET_ARG(1, GetString), (int)GET_ARG(2, GetDecimal));
	return NULL_VAL;
}
/***
 * Settings.SetBool
 * \desc Sets a property in a section to a boolean value.
 * \param section (string): The section where the property resides. If the section doesn't exist, it will be created. If this is `null`, the global section is used instead.
 * \param property (string): The property to set.
 * \param value (boolean): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetBool(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}

	Application::Settings->SetBool(section, GET_ARG(1, GetString), GET_ARG(2, GetInteger));
	return NULL_VAL;
}
/***
 * Settings.AddSection
 * \desc Creates a section.
 * \param section (string): The section name.
 * \ns Settings
 */
VMValue Settings_AddSection(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::Settings->AddSection(GET_ARG(0, GetString));
	return NULL_VAL;
}
/***
 * Settings.RemoveSection
 * \desc Removes a section.
 * \param section (string): The section name.
 * \ns Settings
 */
VMValue Settings_RemoveSection(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	Application::Settings->RemoveSection(section);
	return NULL_VAL;
}
/***
 * Settings.SectionExists
 * \desc Checks if a section exists.
 * \param section (string): The section name.
 * \return boolean Returns whether the section exists.
 * \ns Settings
 */
VMValue Settings_SectionExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	return INTEGER_VAL(Application::Settings->SectionExists(GET_ARG(0, GetString)));
}
/***
 * Settings.GetSectionCount
 * \desc Returns how many sections exist in the settings.
 * \return integer Returns the total section count.
 * \ns Settings
 */
VMValue Settings_GetSectionCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::Settings->GetSectionCount());
}
/***
 * Settings.PropertyExists
 * \desc Checks if a property exists.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property name.
 * \return boolean Returns whether the property exists.
 * \ns Settings
 */
VMValue Settings_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	return INTEGER_VAL(Application::Settings->PropertyExists(section, GET_ARG(1, GetString)));
}
/***
 * Settings.RemoveProperty
 * \desc Removes a property from a section.
 * \param section (string): The section where the property resides. If this is `null`, the global section is used instead.
 * \param property (string): The property to remove.
 * \ns Settings
 */
VMValue Settings_RemoveProperty(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	Application::Settings->RemoveProperty(section, GET_ARG(1, GetString));
	return NULL_VAL;
}
/***
 * Settings.GetPropertyCount
 * \desc Returns how many properties exist in the section.
 * \param section (string): The section. If this is `null`, the global section is used instead.
 * \return integer The total section count, as an integer.
 * \ns Settings
 */
VMValue Settings_GetPropertyCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	char* section = NULL;
	if (!IS_NULL(args[0])) {
		section = GET_ARG(0, GetString);
	}
	if (!Application::Settings->SectionExists(section)) {
		THROW_ERROR("Section \"%s\" does not exist.", section);
		return NULL_VAL;
	}

	return INTEGER_VAL(Application::Settings->GetPropertyCount(section));
}
// #endregion

// #region SocketClient
WebSocketClient* client = NULL;
/***
 * SocketClient.Open
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_Open(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* url = GET_ARG(0, GetString);

	client = WebSocketClient::New(url);
	if (!client) {
		return INTEGER_VAL(false);
	}

	return INTEGER_VAL(true);
}
/***
 * SocketClient.Close
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_Close(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client) {
		return NULL_VAL;
	}

	client->Close();
	client = NULL;

	return NULL_VAL;
}
/***
 * SocketClient.IsOpen
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_IsOpen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client || client->readyState != WebSocketClient::OPEN) {
		return INTEGER_VAL(false);
	}

	return INTEGER_VAL(true);
}
/***
 * SocketClient.Poll
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_Poll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int timeout = GET_ARG(0, GetInteger);

	if (!client) {
		return INTEGER_VAL(false);
	}

	client->Poll(timeout);
	return INTEGER_VAL(true);
}
/***
 * SocketClient.BytesToRead
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_BytesToRead(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client) {
		return INTEGER_VAL(0);
	}

	return INTEGER_VAL((int)client->BytesToRead());
}
/***
 * SocketClient.ReadDecimal
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_ReadDecimal(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client) {
		return NULL_VAL;
	}

	return DECIMAL_VAL(client->ReadFloat());
}
/***
 * SocketClient.ReadInteger
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_ReadInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client) {
		return NULL_VAL;
	}

	return INTEGER_VAL(client->ReadSint32());
}
/***
 * SocketClient.ReadString
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_ReadString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	if (!client) {
		return NULL_VAL;
	}

	if (ScriptManager::Lock()) {
		char* str = client->ReadString();
		ObjString* objStr = TakeString(str, strlen(str));

		ScriptManager::Unlock();
		return OBJECT_VAL(objStr);
	}
	return NULL_VAL;
}
/***
 * SocketClient.WriteDecimal
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_WriteDecimal(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	float value = GET_ARG(0, GetDecimal);
	if (!client) {
		return NULL_VAL;
	}

	client->SendBinary(&value, sizeof(value));
	return NULL_VAL;
}
/***
 * SocketClient.WriteInteger
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_WriteInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int value = GET_ARG(0, GetInteger);
	if (!client) {
		return NULL_VAL;
	}

	client->SendBinary(&value, sizeof(value));
	return NULL_VAL;
}
/***
 * SocketClient.WriteString
 * \desc
 * \return
 * \ns SocketClient
 */
VMValue SocketClient_WriteString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* value = GET_ARG(0, GetString);
	if (!client) {
		return NULL_VAL;
	}

	client->SendText(value);
	return NULL_VAL;
}
// #endregion

// #region Sound
/***
 * Sound.Play
 * \desc Plays a sound either once or in a loop.
 * \param sound (integer): The sound index to play.
 * \paramOpt loopPoint (integer): Loop point in samples. Use <ref AUDIO_LOOP_NONE> to play the sound once or <ref AUDIO_LOOP_DEFAULT> to use the audio file's metadata. (default: <ref AUDIO_LOOP_DEFAULT> )
 * \paramOpt panning (decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (default: `0.0`)
 * \paramOpt speed (decimal): Control the speed of the audio. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed. (default: `1.0`)
 * \paramOpt volume (decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (default: `1.0`)
 * \return integer Returns the channel index where the sound began to play, or `-1` if no channel was available.
 * \ns Sound
 */
VMValue Sound_Play(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	int loopPoint = IS_NULL(args[1]) ? AUDIO_LOOP_DEFAULT
					 : GET_ARG_OPT(1, GetInteger, AUDIO_LOOP_DEFAULT);
	float panning = GET_ARG_OPT(2, GetDecimal, 0.0f);
	float speed = GET_ARG_OPT(3, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(4, GetDecimal, 1.0f);
	int channel = -1;

	if (loopPoint < AUDIO_LOOP_NONE) {
		THROW_ERROR(
			"Audio loop point value should be AUDIO_LOOP_DEFAULT, AUDIO_LOOP_NONE, or a number higher than zero, received %d",
			loopPoint);
		return NULL_VAL;
	}

	if (loopPoint == AUDIO_LOOP_DEFAULT) {
		loopPoint = audio->LoopPoint;
	}

	if (audio) {
		AudioManager::AudioStop(audio);
		channel = AudioManager::PlaySound(audio,
			loopPoint >= 0,
			loopPoint >= 0 ? loopPoint : 0,
			panning,
			speed,
			volume,
			nullptr);
	}
	return INTEGER_VAL(channel);
}
/***
 * Sound.Stop
 * \desc Stops an actively playing sound.
 * \param sound (integer): The sound index to stop.
 * \ns Sound
 */
VMValue Sound_Stop(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	if (audio) {
		AudioManager::AudioStop(audio);
	}
	return NULL_VAL;
}
/***
 * Sound.Pause
 * \desc Pauses an actively playing sound.
 * \param sound (integer): The sound index to pause.
 * \ns Sound
 */
VMValue Sound_Pause(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	if (audio) {
		AudioManager::AudioPause(audio);
	}
	return NULL_VAL;
}
/***
 * Sound.Resume
 * \desc Unpauses a paused sound.
 * \param sound (integer): The sound index to resume.
 * \ns Sound
 */
VMValue Sound_Resume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	if (audio) {
		AudioManager::AudioUnpause(audio);
	}
	return NULL_VAL;
}
/***
 * Sound.StopAll
 * \desc Stops all actively playing sounds.
 * \ns Sound
 */
VMValue Sound_StopAll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::AudioStopAll();
	return NULL_VAL;
}
/***
 * Sound.PauseAll
 * \desc Pauses all actively playing sounds.
 * \ns Sound
 */
VMValue Sound_PauseAll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::AudioPauseAll();
	return NULL_VAL;
}
/***
 * Sound.ResumeAll
 * \desc Resumes all actively playing sounds.
 * \ns Sound
 */
VMValue Sound_ResumeAll(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	AudioManager::AudioUnpauseAll();
	return NULL_VAL;
}
/***
 * Sound.IsPlaying
 * \param sound (integer): The sound index.
 * \desc Checks whether a sound is currently playing
 * \return boolean Returns a boolean value.
 * \ns Sound
 */
VMValue Sound_IsPlaying(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	if (!audio) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL(AudioManager::AudioIsPlaying(audio));
}
/***
 * Sound.PlayMultiple
 * \desc Plays a sound once or loops it, without interrupting channels playing the same sound.
 * \param sound (integer): The sound index to play.
 * \paramOpt loopPoint (integer): Loop point in samples. Use <ref AUDIO_LOOP_NONE> to play the sound once or <ref AUDIO_LOOP_DEFAULT> to use the audio file's metadata. (default: <ref AUDIO_LOOP_DEFAULT>)
 * \paramOpt panning (decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (default: `0.0`)
 * \paramOpt speed (decimal): Control the speed of the audio. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed. (default: `1.0`)
 * \paramOpt volume (decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (default: `1.0`)
 * \return integer Returns the channel index where the sound began to play, or `-1` if no channel was available.
 * \ns Sound
 */
VMValue Sound_PlayMultiple(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	int loopPoint = IS_NULL(args[1]) ? AUDIO_LOOP_DEFAULT
					 : GET_ARG_OPT(1, GetInteger, AUDIO_LOOP_DEFAULT);
	float panning = GET_ARG_OPT(2, GetDecimal, 0.0f);
	float speed = GET_ARG_OPT(3, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(4, GetDecimal, 1.0f);
	int channel = -1;

	if (loopPoint < AUDIO_LOOP_NONE) {
		THROW_ERROR(
			"Audio loop point value should be AUDIO_LOOP_DEFAULT, AUDIO_LOOP_NONE, or a number higher than zero, received %d",
			loopPoint);
		return NULL_VAL;
	}

	if (loopPoint == AUDIO_LOOP_DEFAULT) {
		loopPoint = audio->LoopPoint;
	}

	if (audio) {
		channel = AudioManager::PlaySound(audio,
			loopPoint >= 0,
			loopPoint >= 0 ? loopPoint : 0,
			panning,
			speed,
			volume,
			nullptr);
	}
	return INTEGER_VAL(channel);
}
/***
 * Sound.PlayAtChannel
 * \desc Plays or loops a sound at the specified channel.
 * \param channel (integer): The channel index.
 * \param sound (integer): The sound index to play.
 * \paramOpt loopPoint (integer): Loop point in samples. Use <ref AUDIO_LOOP_NONE> to play the sound once or <ref AUDIO_LOOP_DEFAULT> to use the audio file's metadata. (default: <ref AUDIO_LOOP_DEFAULT>)
 * \paramOpt panning (decimal): Control the panning of the audio. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it. (default: `0.0`)
 * \paramOpt speed (decimal): Control the speed of the audio. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed. (default: `1.0`)
 * \paramOpt volume (decimal): Controls the volume of the audio. 0.0 is muted, 1.0 is normal volume. (default: `1.0`)
 * \return
 * \ns Sound
 */
VMValue Sound_PlayAtChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int channel = GET_ARG(0, GetInteger);
	ISound* audio = GET_ARG(1, GetSound);
	int loopPoint = IS_NULL(args[2]) ? AUDIO_LOOP_DEFAULT
					 : GET_ARG_OPT(2, GetInteger, AUDIO_LOOP_DEFAULT);
	float panning = GET_ARG_OPT(3, GetDecimal, 0.0);
	float speed = GET_ARG_OPT(4, GetDecimal, 1.0f);
	float volume = GET_ARG_OPT(5, GetDecimal, 1.0f);

	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}

	if (loopPoint < AUDIO_LOOP_NONE) {
		THROW_ERROR(
			"Audio loop point value should be AUDIO_LOOP_DEFAULT, AUDIO_LOOP_NONE, or a number higher than zero, received %d",
			loopPoint);
		return NULL_VAL;
	}

	if (loopPoint == AUDIO_LOOP_DEFAULT) {
		loopPoint = audio->LoopPoint;
	}

	if (audio) {
		AudioManager::SetSound(channel % AudioManager::SoundArrayLength,
			audio,
			loopPoint >= 0,
			loopPoint >= 0 ? loopPoint : 0,
			panning,
			speed,
			volume,
			nullptr);
	}
	return NULL_VAL;
}
/***
 * Sound.StopChannel
 * \desc Stops a channel.
 * \param channel (integer): The channel index to stop.
 * \return
 * \ns Sound
 */
VMValue Sound_StopChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int channel = GET_ARG(0, GetInteger);
	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}
	AudioManager::AudioStop(channel % AudioManager::SoundArrayLength);
	return NULL_VAL;
}
/***
 * Sound.PauseChannel
 * \desc Pauses a channel.
 * \param channel (integer): The channel index to pause.
 * \return
 * \ns Sound
 */
VMValue Sound_PauseChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int channel = GET_ARG(0, GetInteger);
	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}
	AudioManager::AudioPause(channel % AudioManager::SoundArrayLength);
	return NULL_VAL;
}
/***
 * Sound.ResumeChannel
 * \desc Unpauses a paused channel.
 * \param channel (integer): The channel index to resume.
 * \return
 * \ns Sound
 */
VMValue Sound_ResumeChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int channel = GET_ARG(0, GetInteger);
	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}
	AudioManager::AudioUnpause(channel % AudioManager::SoundArrayLength);
	return NULL_VAL;
}
/***
 * Sound.AlterChannel
 * \desc Alters the playback conditions of the specified channel.
 * \param channel (integer): The channel index to resume.
 * \param panning (decimal): Control the panning of the sound. -1.0 makes it sound in left ear only, 1.0 makes it sound in right ear, and closer to 0.0 centers it.
 * \param speed (decimal): Control the speed of the sound. Higher than 1.0 makes it faster, lesser than 1.0 is slower, 1.0 is normal speed.
 * \param volume (decimal): Controls the volume of the sound. 0.0 is muted, 1.0 is normal volume.
 * \return
 * \ns Sound
 */
VMValue Sound_AlterChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int channel = GET_ARG(0, GetInteger);
	float panning = GET_ARG(1, GetDecimal);
	float speed = GET_ARG(2, GetDecimal);
	float volume = GET_ARG(3, GetDecimal);
	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}
	AudioManager::AlterChannel(
		channel % AudioManager::SoundArrayLength, panning, speed, volume);
	return NULL_VAL;
}
/***
 * Sound.GetFreeChannel
 * \desc Gets the first channel index that is not currently playing any sound.
 * \return integer Returns the available channel index, or `-1` if no channel was available.
 * \ns Sound
 */
VMValue Sound_GetFreeChannel(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(AudioManager::GetFreeChannel());
}
/***
 * Sound.IsChannelFree
 * \desc Checks whether a channel is currently playing any sound.
 * \param sound (integer): The channel index.
 * \ns Sound
 */
VMValue Sound_IsChannelFree(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int channel = GET_ARG(0, GetInteger);
	if (channel < 0) {
		THROW_ERROR("Invalid channel index %d.", channel);
		return NULL_VAL;
	}
	return INTEGER_VAL(AudioManager::AudioIsPlaying(channel % AudioManager::SoundArrayLength));
}
/***
 * Sound.GetLoopPoint
 * \desc Gets the loop point of a sound index, if it has one.
 * \param sound (integer): The sound index to get the loop point.
 * \return integer Returns the loop point in samples, as an integer value, or `null` if the audio does not have one.
 * \ns Sound
 */
VMValue Sound_GetLoopPoint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISound* audio = GET_ARG(0, GetSound);
	if (!audio || audio->LoopPoint == -1) {
		return NULL_VAL;
	}
	return INTEGER_VAL(audio->LoopPoint);
}
/***
 * Sound.SetLoopPoint
 * \desc Sets the loop point of a sound index.
 * \param sound (integer): The sound index to set the loop point.
 * \param loopPoint (integer): The loop point in samples, or `null` to remove the audio's loop point.
 * \ns Sound
 */
VMValue Sound_SetLoopPoint(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISound* audio = GET_ARG(0, GetSound);
	int loopPoint = IS_NULL(args[1]) ? -1 : GET_ARG(1, GetInteger);
	if (!audio) {
		return NULL_VAL;
	}
	if (loopPoint >= -1) {
		audio->LoopPoint = loopPoint;
	}
	return NULL_VAL;
}
// #endregion

// #region Sprite
#define CHECK_ANIMATION_INDEX(idx) \
	if (idx < 0 || idx >= (int)sprite->Animations.size()) { \
		OUT_OF_RANGE_ERROR("Animation index", idx, 0, sprite->Animations.size() - 1); \
		return NULL_VAL; \
	}
#define CHECK_ANIMFRAME_INDEX(anim, idx) \
	CHECK_ANIMATION_INDEX(anim); \
	if (idx < 0 || idx >= (int)sprite->Animations[anim].Frames.size()) { \
		OUT_OF_RANGE_ERROR( \
			"Frame index", idx, 0, sprite->Animations[anim].Frames.size() - 1); \
		return NULL_VAL; \
	}
/***
 * Sprite.GetAnimationCount
 * \desc Gets the amount of animations in the sprite.
 * \param sprite (integer): The sprite index to check.
 * \return integer Returns the amount of animations in the sprite.
 * \ns Sprite
 */
VMValue Sprite_GetAnimationCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISprite* sprite = GET_ARG(0, GetSprite);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	return INTEGER_VAL((int)sprite->Animations.size());
}
/***
 * Sprite.GetAnimationName
 * \desc Gets the name of the specified animation index in the sprite.
 * \param sprite (integer): The sprite index to check.
 * \param animationIndex (integer): The animation index.
 * \return string Returns the name of the specified animation index.
 * \ns Sprite
 */
VMValue Sprite_GetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int index = GET_ARG(1, GetInteger);
	if (!sprite) {
		return NULL_VAL;
	}
	CHECK_ANIMATION_INDEX(index);
	return ReturnString(sprite->Animations[index].Name);
}
/***
 * Sprite.GetAnimationIndexByName
 * \desc Gets the first animation in the sprite which matches the specified name.
 * \param sprite (integer): The sprite index to check.
 * \param name (string): The animation name to search for.
 * \return integer Returns the first animation index with the specified name, or -1 if there was no match.
 * \ns Sprite
 */
VMValue Sprite_GetAnimationIndexByName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	char* name = GET_ARG(1, GetString);
	if (!sprite) {
		return INTEGER_VAL(-1);
	}
	for (size_t i = 0; i < sprite->Animations.size(); i++) {
		if (strcmp(name, sprite->Animations[i].Name) == 0) {
			return INTEGER_VAL((int)i);
		}
	}
	return INTEGER_VAL(-1);
}
/***
 * Sprite.GetFrameExists
 * \desc Checks if an animation and frame is valid within a sprite.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index to check.
 * \param frame (integer): The sprite index to check.
 * \return boolean Returns whether the frame is valid.
 * \ns Sprite
 */
VMValue Sprite_GetFrameExists(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(false);
	}
	return (INTEGER_VAL((animation >= 0 && animation < (int)sprite->Animations.size()) &&
		(frame >= 0 && frame < (int)sprite->Animations[animation].Frames.size())));
}
/***
 * Sprite.GetFrameLoopIndex
 * \desc Gets the index of the frame that the specified animation will loop back to when it finishes.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \return integer Returns the frame loop index.
 * \ns Sprite
 */
VMValue Sprite_GetFrameLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMATION_INDEX(animation);
	return INTEGER_VAL(sprite->Animations[animation].FrameToLoop);
}
/***
 * Sprite.GetFrameCount
 * \desc Gets the amount of frames in the specified animation.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \return integer Returns the frame count in the specified animation.
 * \ns Sprite
 */
VMValue Sprite_GetFrameCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMATION_INDEX(animation);
	return INTEGER_VAL((int)sprite->Animations[animation].Frames.size());
}
/***
 * Sprite.GetFrameDuration
 * \desc Gets the frame duration of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the frame duration (in game frames) of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].Duration);
}
/***
 * Sprite.GetFrameSpeed
 * \desc Gets the animation speed of the specified animation.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \return integer Returns an integer.
 * \ns Sprite
 */
VMValue Sprite_GetFrameSpeed(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMATION_INDEX(animation);
	return INTEGER_VAL(sprite->Animations[animation].AnimationSpeed);
}
/***
 * Sprite.GetFrameWidth
 * \desc Gets the frame width of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the frame width (in pixels) of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].Width);
}
/***
 * Sprite.GetFrameHeight
 * \desc Gets the frame height of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the frame height (in pixels) of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].Height);
}
/***
 * Sprite.GetFrameID
 * \desc Gets the frame ID of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the frame ID of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameID(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].Advance);
}
/***
 * Sprite.GetFrameOffsetX
 * \desc Gets the X offset of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the X offset of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameOffsetX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].OffsetX);
}
/***
 * Sprite.GetFrameOffsetY
 * \desc Gets the Y offset of the specified sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animation (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns the Y offset of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameOffsetY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	int frame = GET_ARG(2, GetInteger);
	if (!sprite) {
		return INTEGER_VAL(0);
	}
	CHECK_ANIMFRAME_INDEX(animation, frame);
	return INTEGER_VAL(sprite->Animations[animation].Frames[frame].OffsetY);
}
/***
 * Sprite.GetHitboxName
 * \desc Gets the name of a hitbox through its index.
 * \param sprite (integer): The sprite index to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \param hitboxID (integer): The hitbox index to check.
 * \return string Returns a string value.
 * \ns Sprite
 */
VMValue Sprite_GetHitboxName(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animationID = GET_ARG(1, GetInteger);
	int frameID = GET_ARG(2, GetInteger);
	int hitboxID = GET_ARG(3, GetInteger);

	CHECK_ANIMATION_INDEX(animationID);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];

	if (frame.Boxes.size() == 0) {
		THROW_ERROR("Frame %d of animation %d contains no hitboxes.", frameID, animationID);
		return NULL_VAL;
	}
	else if (!(hitboxID > -1 && hitboxID < frame.Boxes.size())) {
		THROW_ERROR("Hitbox %d is not in bounds of frame %d of animation %d.",
			hitboxID,
			frameID,
			animationID);
		return NULL_VAL;
	}

	return ReturnString(frame.Boxes[hitboxID].Name);
}
/***
 * Sprite.GetHitboxIndex
 * \desc Gets the index of a hitbox by its name.
 * \param sprite (integer): The sprite index to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \param name (string): The name of the hitbox to check.
 * \return integer Returns an integer value, or `null` if no such hitbox exists with that name.
 * \ns Sprite
 */
VMValue Sprite_GetHitboxIndex(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
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
 * Sprite.GetHitboxCount
 * \desc Gets the hitbox count in the given frame of an animation of a sprite.
 * \param sprite (integer): The sprite index to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frame (integer): The frame index of the animation to check.
 * \return integer Returns an integer value.
 * \ns Sprite
 */
VMValue Sprite_GetHitboxCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
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
 * Sprite.GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param entity (Entity): An entity with `Sprite`, `CurrentAnimation`, and `CurrentFrame` values.
 * \paramOpt hitbox (string): The hitbox name.
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
/***
 * Sprite.GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param entity (Entity): An entity with `Sprite`, `CurrentAnimation`, and `CurrentFrame` values.
 * \paramOpt hitbox (integer): The hitbox index. (default: `0`)
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
/***
 * Sprite.GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frameID (integer): The frame index of the animation to check.
 * \paramOpt hitbox (string): The hitbox name.
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
/***
 * Sprite.GetHitbox
 * \desc Gets the hitbox of a sprite frame.
 * \param sprite (integer): The sprite index to check.
 * \param animationID (integer): The animation index of the sprite to check.
 * \param frameID (integer): The frame index of the animation to check.
 * \paramOpt hitbox (integer): The hitbox index. (default: `0`)
 * \return hitbox Returns a Hitbox value.
 * \ns Sprite
 */
VMValue Sprite_GetHitbox(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	ISprite* sprite;
	int animationID, frameID, hitboxID = 0;
	int hitboxArgNum;

	if (argCount <= 2 && IS_ENTITY(args[0])) {
		ObjEntity* ent = GET_ARG(0, GetEntity);
		Entity* entity = (Entity*)ent->EntityPtr;
		hitboxArgNum = 1;

		sprite = GetSpriteIndex(entity->Sprite, threadID);
		if (!sprite) {
			return NULL_VAL;
		}

		animationID = entity->CurrentAnimation;
		frameID = entity->CurrentFrame;
	}
	else {
		CHECK_AT_LEAST_ARGCOUNT(3);
		sprite = GET_ARG(0, GetSprite);
		animationID = GET_ARG(1, GetInteger);
		frameID = GET_ARG(2, GetInteger);
		hitboxArgNum = 3;
	}

	if (!sprite) {
		return NULL_VAL;
	}

	CHECK_ANIMATION_INDEX(animationID);
	CHECK_ANIMFRAME_INDEX(animationID, frameID);

	AnimFrame frame = sprite->Animations[animationID].Frames[frameID];
	if (frame.Boxes.size() == 0) {
		THROW_ERROR("Frame %d of animation %d contains no hitboxes.", frameID, animationID);
		return NULL_VAL;
	}

	if (argCount > hitboxArgNum && IS_STRING(args[hitboxArgNum])) {
		char* name = GET_ARG(hitboxArgNum, GetString);
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
				THROW_ERROR("No hitbox named \"%s\" in frame %d of animation %d.",
					name,
					frameID,
					animationID);
			}
		}
	}
	else {
		hitboxID = GET_ARG_OPT(hitboxArgNum, GetInteger, 0);
	}

	if (hitboxID < 0 || hitboxID >= (int)frame.Boxes.size()) {
		THROW_ERROR("Hitbox %d is not in bounds of frame %d of animation %d.",
			hitboxID,
			frameID,
			animationID);
		return NULL_VAL;
	}

	CollisionBox box = frame.Boxes[hitboxID];
	return HITBOX_VAL(box.Left, box.Top, box.Right, box.Bottom);
}
/***
 * Sprite.GetTextArray
 * \desc Converts a string to an array of sprite indexes by comparing codepoints to a frame's ID.
 * \param sprite (integer): The sprite index.
 * \param animation (integer): The animation index containing frames with codepoint ID values.
 * \param text (string): The text to convert.
 * \return array Returns an array of sprite indexes per character in the text.
 * \ns Sprite
 */
VMValue Sprite_GetTextArray(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int animation = GET_ARG(1, GetInteger);
	char* string = GET_ARG(2, GetString);

	ObjArray* textArray = NewArray();

	if (!sprite || !string)
		return OBJECT_VAL(textArray);

	if (animation >= 0 && animation < (int)sprite->Animations.size()) {
		std::vector<Uint32> codepoints = StringUtils::GetCodepoints(string);

		for (Uint32 codepoint : codepoints) {
			if (codepoint == (Uint32)-1) {
				textArray->Values->push_back(INTEGER_VAL(-1));
				continue;
			}

			bool found = false;
			for (int f = 0; f < (int)sprite->Animations[animation].Frames.size(); f++) {
				if (sprite->Animations[animation].Frames[f].Advance == (int)codepoint) {
					textArray->Values->push_back(INTEGER_VAL(f));
					found = true;
					break;
				}
			}

			if (!found)
				textArray->Values->push_back(INTEGER_VAL(-1));
		}
	}

	return OBJECT_VAL(textArray);
}
/***
 * Sprite.GetTextWidth
 * \desc Gets the width (in pixels) of a converted sprite string.
 * \param sprite (integer): The sprite index.
 * \param animation (integer): The animation index.
 * \param text (array): The array containing frame indexes.
 * \param startIndex (integer): Where to start checking the width.
 * \param spacing (integer): The spacing (in pixels) between frames.
 * \ns Sprite
 */
VMValue Sprite_GetTextWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(6);

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

			if (length <= 0 || length > (int)text->Values->size())
				length = (int)text->Values->size();

			int w = 0;
			for (int c = startIndex; c < length; c++) {
				int charFrame = AS_INTEGER(Value::CastAsInteger((*text->Values)[c]));
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
 * Sprite.MakePalettized
 * \desc Converts a sprite's colors to the ones in the specified palette index.
 * \param sprite (integer): The sprite index.
 * \param paletteIndex (integer): The palette index.
 * \ns Sprite
 */
VMValue Sprite_MakePalettized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ISprite* sprite = GET_ARG(0, GetSprite);
	int palIndex = GET_ARG(1, GetInteger);
	if (!sprite) {
		return NULL_VAL;
	}
	CHECK_PALETTE_INDEX(palIndex);
	sprite->ConvertToPalette(palIndex);
	return NULL_VAL;
}
/***
 * Sprite.MakeNonPalettized
 * \desc Removes a sprite's palette.
 * \param sprite (integer): The sprite index.
 * \ns Sprite
 */
VMValue Sprite_MakeNonPalettized(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ISprite* sprite = GET_ARG(0, GetSprite);
	if (sprite) {
		sprite->ConvertToRGBA();
	}
	return NULL_VAL;
}
#undef CHECK_ANIMATION_INDEX
#undef CHECK_ANIMFRAME_INDEX
// #endregion

// #region Stream
/***
 * Stream.FromResource
 * \desc Opens a stream from a resource.
 * \param filename (string): Filename of the resource.
 * \return stream Returns the newly opened stream.
 * \ns Stream
 */
VMValue Stream_FromResource(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	if (ScriptManager::Lock()) {
		char* filename = GET_ARG(0, GetString);
		ResourceStream* streamPtr = ResourceStream::New(filename);
		if (!streamPtr) {
			ScriptManager::Unlock();
			THROW_ERROR("Could not open resource stream \"%s\"!", filename);
			return NULL_VAL;
		}
		ObjStream* stream = StreamImpl::New((void*)streamPtr, false);
		ScriptManager::Unlock();
		return OBJECT_VAL(stream);
	}
	return NULL_VAL;
}
/***
 * Stream.FromFile
 * \desc Opens a stream from a file.
 * \param filename (string): Path of the file.
 * \param mode (<ref FileStream_*>): The file access mode.
 * \return stream Returns the newly opened stream.
 * \ns Stream
 */
VMValue Stream_FromFile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	if (ScriptManager::Lock()) {
		char* filename = GET_ARG(0, GetString);
		int access = GET_ARG(1, GetInteger);
		FileStream* streamPtr = FileStream::New(filename, access, true);
		if (!streamPtr) {
			ScriptManager::Unlock();
			THROW_ERROR("Could not open file stream \"%s\"!", filename);
			return NULL_VAL;
		}
		ObjStream* stream =
			StreamImpl::New((void*)streamPtr, access == FileStream::WRITE_ACCESS);
		ScriptManager::Unlock();
		return OBJECT_VAL(stream);
	}
	return NULL_VAL;
}
/***
 * Stream.Close
 * \desc Closes a stream.
 * \param stream (stream): The stream to close.
 * \ns Stream
 */
VMValue Stream_Close(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	if (stream->Closed) {
		THROW_ERROR("Cannot close a stream that was already closed!");
		return NULL_VAL;
	}
	stream->StreamPtr->Close();
	stream->Closed = true;
	return NULL_VAL;
}
/***
 * Stream.Seek
 * \desc Seeks a stream, relative to the start of the stream.
 * \param stream (stream): The stream to seek.
 * \param offset (integer): Offset to seek to.
 * \ns Stream
 */
VMValue Stream_Seek(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint64 offset = GET_ARG(1, GetInteger);
	if (stream->Closed) {
		THROW_ERROR("Cannot seek closed stream!");
		return NULL_VAL;
	}
	stream->StreamPtr->Seek(offset);
	return NULL_VAL;
}
/***
 * Stream.SeekEnd
 * \desc Seeks a stream, relative to the end.
 * \param stream (stream): The stream to seek.
 * \param offset (integer): Offset to seek to.
 * \ns Stream
 */
VMValue Stream_SeekEnd(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint64 offset = GET_ARG(1, GetInteger);
	if (stream->Closed) {
		THROW_ERROR("Cannot seek closed stream!");
		return NULL_VAL;
	}
	stream->StreamPtr->SeekEnd(offset);
	return NULL_VAL;
}
/***
 * Stream.Skip
 * \desc Seeks a stream, relative to the current position.
 * \param stream (stream): The stream to skip.
 * \param offset (integer): How many bytes to skip.
 * \ns Stream
 */
VMValue Stream_Skip(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint64 offset = GET_ARG(1, GetInteger);
	if (stream->Closed) {
		THROW_ERROR("Cannot skip closed stream!");
		return NULL_VAL;
	}
	stream->StreamPtr->Skip(offset);
	return NULL_VAL;
}
/***
 * Stream.Position
 * \desc Returns the current position of the stream.
 * \param stream (stream): The stream.
 * \return integer The current position of the stream.
 * \ns Stream
 */
VMValue Stream_Position(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	if (stream->Closed) {
		THROW_ERROR("Cannot get position of closed stream!");
		return NULL_VAL;
	}
	return INTEGER_VAL((int)stream->StreamPtr->Position());
}
/***
 * Stream.Length
 * \desc Returns the length of the stream.
 * \param stream (stream): The stream.
 * \return integer The length of the stream.
 * \ns Stream
 */
VMValue Stream_Length(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	if (stream->Closed) {
		THROW_ERROR("Cannot get length of closed stream!");
		return NULL_VAL;
	}
	return INTEGER_VAL((int)stream->StreamPtr->Length());
}
/***
 * Stream.ReadByte
 * \desc Reads an unsigned 8-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned 8-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadByte(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadByte());
}
/***
 * Stream.ReadUInt16
 * \desc Reads an unsigned 16-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned 16-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadUInt16(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadUInt16());
}
/***
 * Stream.ReadUInt16BE
 * \desc Reads an unsigned big-endian 16-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned big-endian 16-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadUInt16BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadUInt16BE());
}
/***
 * Stream.ReadUInt32
 * \desc Reads an unsigned 32-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned 32-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadUInt32(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadUInt32());
}
/***
 * Stream.ReadUInt32BE
 * \desc Reads an unsigned big-endian 32-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned big-endian 32-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadUInt32BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadUInt32BE());
}
/***
 * Stream.ReadUInt64
 * \desc Reads an unsigned 64-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns an unsigned 64-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadUInt64(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadUInt64());
}
/***
 * Stream.ReadInt16
 * \desc Reads a signed 16-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns a signed 16-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadInt16(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadInt16());
}
/***
 * Stream.ReadInt16BE
 * \desc Reads a signed big-endian 16-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns a signed big-endian 16-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadInt16BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadInt16BE());
}
/***
 * Stream.ReadInt32
 * \desc Reads a signed 32-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns a signed 32-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadInt32(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadInt32());
}
/***
 * Stream.ReadInt32BE
 * \desc Reads a signed big-endian 32-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns a signed big-endian 32-bit number as an integer value.
 * \ns Stream
 */
VMValue Stream_ReadInt32BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadInt32BE());
}
/***
 * Stream.ReadInt64
 * \desc Reads a signed 64-bit number from the stream.
 * \param stream (stream): The stream.
 * \return integer Returns a signed 64-bit Integer value.
 * \ns Stream
 */
VMValue Stream_ReadInt64(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return INTEGER_VAL((int)stream->StreamPtr->ReadInt64());
}
/***
 * Stream.ReadFloat
 * \desc Reads a floating point number from the stream.
 * \param stream (stream): The stream.
 * \return decimal Returns a decimal value.
 * \ns Stream
 */
VMValue Stream_ReadFloat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	return DECIMAL_VAL((float)stream->StreamPtr->ReadFloat());
}
/***
 * Stream.ReadString
 * \desc Reads a null-terminated string from the stream.
 * \param stream (stream): The stream.
 * \return string Returns a string value.
 * \ns Stream
 */
VMValue Stream_ReadString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		char* line = stream->StreamPtr->ReadString();
		obj = OBJECT_VAL(TakeString(line));
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * Stream.ReadLine
 * \desc Reads a line from the stream.
 * \param stream (stream): The stream.
 * \return string Returns a string value.
 * \ns Stream
 */
VMValue Stream_ReadLine(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjStream* stream = GET_ARG(0, GetStream);
	CHECK_READ_STREAM;
	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		char* line = stream->StreamPtr->ReadLine();
		obj = OBJECT_VAL(TakeString(line));
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * Stream.WriteByte
 * \desc Writes an unsigned 8-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteByte(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint8 byte = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteByte(byte);
	return NULL_VAL;
}
/***
 * Stream.WriteUInt16
 * \desc Writes an unsigned 16-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteUInt16(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint16 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteUInt16(val);
	return NULL_VAL;
}
/***
 * Stream.WriteUInt16BE
 * \desc Writes an unsigned big-endian 16-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteUInt16BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint16 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteUInt16BE(val);
	return NULL_VAL;
}
/***
 * Stream.WriteUInt32
 * \desc Writes an unsigned 32-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteUInt32(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint32 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteUInt32(val);
	return NULL_VAL;
}
/***
 * Stream.WriteUInt32BE
 * \desc Writes an unsigned big-endian 32-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteUInt32BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint32 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteUInt32BE(val);
	return NULL_VAL;
}
/***
 * Stream.WriteUInt64
 * \desc Writes an unsigned 64-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteUInt64(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Uint64 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteUInt64(val);
	return NULL_VAL;
}
/***
 * Stream.WriteInt16
 * \desc Writes a signed 16-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteInt16(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint16 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteInt16(val);
	return NULL_VAL;
}
/***
 * Stream.WriteInt16BE
 * \desc Writes a signed big-endian 16-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteInt16BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint16 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteInt16BE(val);
	return NULL_VAL;
}
/***
 * Stream.WriteInt32
 * \desc Writes a signed 32-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteInt32(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint32 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteInt32(val);
	return NULL_VAL;
}
/***
 * Stream.WriteInt32BE
 * \desc Writes a signed big-endian 32-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteInt32BE(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint32 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteInt32BE(val);
	return NULL_VAL;
}
/***
 * Stream.WriteInt64
 * \desc Writes a signed 64-bit number to the stream.
 * \param stream (stream): The stream.
 * \param value (integer): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteInt64(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	Sint64 val = GET_ARG(1, GetInteger);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteInt64(val);
	return NULL_VAL;
}
/***
 * Stream.WriteFloat
 * \desc Writes a floating point number to the stream.
 * \param stream (stream): The stream.
 * \param value (decimal): The value to write.
 * \ns Stream
 */
VMValue Stream_WriteFloat(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	float val = GET_ARG(1, GetDecimal);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteFloat(val);
	return NULL_VAL;
}
/***
 * Stream.WriteString
 * \desc Writes a null-terminated string to the stream.
 * \param stream (stream): The stream.
 * \param string (string): The string to write.
 * \ns Stream
 */
VMValue Stream_WriteString(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	ObjStream* stream = GET_ARG(0, GetStream);
	char* string = GET_ARG(1, GetString);
	CHECK_WRITE_STREAM;
	stream->StreamPtr->WriteString(string);
	return NULL_VAL;
}
#undef CHECK_WRITE_STREAM
#undef CHECK_READ_STREAM
// #endregion

// #region String
/***
 * String.Format
 * \desc Formats a <b>format string</b> according to the given <b>format specifiers</b>. A format specifier is a string of the form <code>%[flags][width][.precision][conversion specifier]</code> where a <b>conversion specifier</b> must be one of the following:<br/>\
<ul><li>`d`: Integers</li>\
<li>`f` or `%F`: Decimals</li>\
<li>`s`: Strings</li>\
<li>`c`: Characters</li>\
<li>`x` or `%X`: Hexadecimal integers</li>\
<li>`b` or `%b`: Binary integers</li>\
<li>`o`: Octal integers</li>\
<li>`%`: A literal percent sign character</li>\
</ul>\
<b>Flags</b> are optional, and must be one of the following:<br/>\
<ul><li>`0`: Pads the value with leading zeroes. See the <b>width sub-specifier</b>.</li>\
<li>`-`: Left-justifies the result. See the <b>width sub-specifier</b>.</li>\
<li>`#`: Prefixes something to the value depending on the <b>conversion specifier</b>:\
<ul><li>`x` or `X`: Prefixes the value with `0x` or `0X` respectively.</li>\
<li>`b` or `B`: Prefixes the value with `0b` or `0B` respectively.</li>\
<li>`f`: Prefixes the value with a `.` character.</li>\
<li>`o`: Prefixes the value with a `0` character.</li>\
</ul>\
</li>\
<li>`+`: Prefixes positive numbers with a plus sign.</li>\
<li>A space character: If no sign character (`-` or `+`) was written, a space character is written instead.</li>\
</ul>\
A <b>width sub-specifier</b> is used in conjunction with the flags:<br/>\
<ul><li>A number: The amount of padding to add.</li>\
<li>`*`: This functions the same as the above, but the width is given in the next argument as an Integer value.</li>\
</ul>\
<b>Precision specifiers</b> are also supported:<br/>\
<ul><li>`.` followed by a number:<ul><li>For Integer values, this pads the value with leading zeroes.</li>\
<li>For Decimal values, this specifies the number of digits to be printed after the decimal point (which is 6 by default).</li>\
<li>For String values, this is the maximum amount of characters to be printed.</li>\
</ul>\
</li>\
<li>`.` followed by a `*`: This functions the same as the above, but the precision is given in the next argument as an Integer value.</li>\
</ul>
 * \param string (string): The format string.
 * \paramOpt values (value): Variable arguments.
 * \return string Returns a string value.
 * \ns String
 */
VMValue String_Format(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* fmtString = GET_ARG(0, GetString);

	if (fmtString && ScriptManager::Lock()) {
		ObjString* newString;
		if (argCount > 1) {
			newString = npf_vpprintf(fmtString, argCount - 1, args + 1, threadID);
		}
		else {
			newString = CopyString(fmtString);
		}
		ScriptManager::Unlock();
		return OBJECT_VAL(newString);
	}

	return NULL_VAL;
}
/***
 * String.Split
 * \desc Splits a string by a delimiter.
 * \param string (string): The string to split.
 * \param delimiter (integer): The delimiter string.
 * \return array Returns an array of string values.
 * \ns String
 */
VMValue String_Split(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	char* delimt = GET_ARG(1, GetString);

	if (ScriptManager::Lock()) {
		ObjArray* array = NewArray();

		char* input = StringUtils::Duplicate(string);
		char* tok = strtok(input, delimt);
		while (tok != NULL) {
			array->Values->push_back(OBJECT_VAL(CopyString(tok)));
			tok = strtok(NULL, delimt);
		}
		Memory::Free(input);

		ScriptManager::Unlock();
		return OBJECT_VAL(array);
	}
	return NULL_VAL;
}
/***
 * String.CharAt
 * \desc Gets the 8-bit value of the character at the specified index.
 * \param string (string): The string containing the character.
 * \param index (integer): The character index to check.
 * \return integer Returns the value as an integer.
 * \ns String
 */
VMValue String_CharAt(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	int n = GET_ARG(1, GetInteger);
	
	return INTEGER_VAL((Uint8)string[n]);
}
/***
 * String.CodepointAt
 * \desc Gets the codepoint value of the character at the specified index.
 * \param string (string): The string containing the character.
 * \param index (integer): The character index to check.
 * \return integer Returns the value as an integer.
 * \ns String
 */
VMValue String_CodepointAt(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	int n = GET_ARG(1, GetInteger);

	return INTEGER_VAL(StringUtils::DecodeUTF8Char(string, n));
}
/***
 * String.Length
 * \desc Gets the length of the string value.
 * \param string (string): The input string.
 * \return integer Returns the length of the string value as an integer.
 * \ns String
 */
VMValue String_Length(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);
	return INTEGER_VAL((int)strlen(string));
}
/***
 * String.Compare
 * \desc Compares two strings lexicographically.
 * \param stringA (string): The first string to compare.
 * \param stringB (string): The second string to compare.
 * \return integer Returns the comparison result as an integer. The return value is a negative integer if <param stringA> appears before `stringB` lexicographically, a positive integer if <param stringA> appears after <param stringB> lexicographically, and zero if <param stringA> and <param stringB> are equal.
 * \ns String
 */
VMValue String_Compare(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* stringA = GET_ARG(0, GetString);
	char* stringB = GET_ARG(1, GetString);
	return INTEGER_VAL((int)strcmp(stringA, stringB));
}
/***
 * String.IndexOf
 * \desc Get the first index at which the substring occurs in the string.
 * \param string (string): The string to compare.
 * \param substring (string): The substring to search for.
 * \return integer Returns the index as an integer.
 * \ns String
 */
VMValue String_IndexOf(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	char* substring = GET_ARG(1, GetString);
	char* find = strstr(string, substring);
	if (!find) {
		return INTEGER_VAL(-1);
	}
	return INTEGER_VAL((int)(find - string));
}
/***
 * String.Contains
 * \desc Searches for whether a substring is within a string value.
 * \param string (string): The string to compare.
 * \param substring (string): The substring to search for.
 * \return boolean Returns a boolean value.
 * \ns String
 */
VMValue String_Contains(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	char* substring = GET_ARG(1, GetString);
	return INTEGER_VAL((int)(!!strstr(string, substring)));
}
/***
 * String.Substring
 * \desc Get a string value from a portion of a larger string value.
 * \param string (string): The input string.
 * \param startIndex (integer): The starting index of the substring.
 * \param length (integer): The length of the substring.
 * \return string Returns a string value.
 * \ns String
 */
VMValue String_Substring(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	char* string = GET_ARG(0, GetString);
	size_t index = GET_ARG(1, GetInteger);
	size_t length = GET_ARG(2, GetInteger);

	size_t strln = strlen(string);
	if (length > strln - index) {
		length = strln - index;
	}

	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		obj = OBJECT_VAL(CopyString(string + index, length));
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * String.ToUpperCase
 * \desc Convert a string value to its uppercase representation.
 * \param string (string): The string to make uppercase.
 * \return string Returns a uppercase string value.
 * \ns String
 */
VMValue String_ToUpperCase(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);

	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		ObjString* objStr = CopyString(string);
		for (char* a = objStr->Chars; *a; a++) {
			if (*a >= 'a' && *a <= 'z') {
				*a += 'A' - 'a';
			}
			else if (*a >= 'a' && *a <= 'z') {
				*a += 'A' - 'a';
			}
		}
		obj = OBJECT_VAL(objStr);
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * String.ToLowerCase
 * \desc Convert a string value to its lowercase representation.
 * \param string (string): The string to make lowercase.
 * \return string Returns a lowercase string value.
 * \ns String
 */
VMValue String_ToLowerCase(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);

	VMValue obj = NULL_VAL;
	if (ScriptManager::Lock()) {
		ObjString* objStr = CopyString(string);
		for (char* a = objStr->Chars; *a; a++) {
			if (*a >= 'A' && *a <= 'Z') {
				*a += 'a' - 'A';
			}
		}
		obj = OBJECT_VAL(objStr);
		ScriptManager::Unlock();
	}
	return obj;
}
/***
 * String.LastIndexOf
 * \desc Get the last index at which the substring occurs in the string.
 * \param string (string): The string to compare.
 * \param substring (string): The substring to search for.
 * \return integer Returns the index as an integer.
 * \ns String
 */
VMValue String_LastIndexOf(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	char* string = GET_ARG(0, GetString);
	char* substring = GET_ARG(1, GetString);
	size_t string_len = strlen(string);
	size_t substring_len = strlen(substring);
	if (string_len < substring_len) {
		return INTEGER_VAL(-1);
	}

	char* find = NULL;
	for (char* start = string + string_len - substring_len; start >= string; start--) {
		if (memcmp(start, substring, substring_len) == 0) {
			find = start;
			break;
		}
	}
	if (!find) {
		return INTEGER_VAL(-1);
	}
	return INTEGER_VAL((int)(find - string));
}
/***
 * String.ParseInteger
 * \desc Converts a string value to an integer value, if possible.
 * \param string (string): The string to parse.
 * \paramOpt radix (integer): The numerical base, or radix. If `0`, the radix is detected by the value of `string`: <br/>\
If <param string> begins with `0x`, it is a hexadecimal number (base 16);<br/>\
Else, if <param string> begins with `0`, it is an octal number (base 8);<br/>\
Else, if <param string> begins with `0b`, it is a binary number (base 2);<br/>\
Else, the number is assumed to be in base 10.
 * \return integer Returns the value as an integer.
 * \ns String
 */
VMValue String_ParseInteger(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);
	int radix = GET_ARG_OPT(1, GetInteger, 10);
	if (radix < 0 || radix > 36) {
		THROW_ERROR("Invalid radix of %d. (0 - 36)", radix);
		return NULL_VAL;
	}
	return INTEGER_VAL((int)strtol(string, NULL, radix));
}
/***
 * String.ParseDecimal
 * \desc Convert a string value to a decimal value if possible.
 * \param string (string): The string to parse.
 * \return decimal Returns the value as a decimal.
 * \ns String
 */
VMValue String_ParseDecimal(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);
	return DECIMAL_VAL((float)strtod(string, NULL));
}
/***
 * String.GetCodepoints
 * \desc Gets a list of UCS codepoints from UTF-8 text.
 * \param string (string): The UTF-8 string.
 * \return array Returns an array of Integer values.
 * \ns String
 */
VMValue String_GetCodepoints(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);

	ObjArray* array = NewArray();

	if (string) {
		std::vector<Uint32> codepoints = StringUtils::GetCodepoints(string);

		for (size_t i = 0; i < codepoints.size(); i++) {
			array->Values->push_back(INTEGER_VAL((int)codepoints[i]));
		}
	}

	return OBJECT_VAL(array);
}
/***
 * String.FromCodepoints
 * \desc Creates UTF-8 text from a list of UCS codepoints.
 * \param codepoints (array): An array of integer values.
 * \return string Returns a string value.
 * \ns String
 */
VMValue String_FromCodepoints(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjArray* array = GET_ARG(0, GetArray);
	if (!array) {
		return NULL_VAL;
	}

	std::vector<Uint32> codepoints;
	std::string result;

	for (size_t i = 0; i < array->Values->size(); i++) {
		VMValue value = (*array->Values)[i];
		int codepoint = 0;

		if (IS_INTEGER(value)) {
			codepoint = AS_INTEGER(value);
		}
		else {
			THROW_ERROR("Expected array index %d to be of type %s instead of %s.",
				i,
				GetTypeString(VAL_INTEGER),
				GetValueTypeString(value));
		}

		codepoints.push_back(codepoint);
	}

	try {
		result = StringUtils::FromCodepoints(codepoints);
	} catch (const std::runtime_error& error) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false, "%s", error.what());
		return NULL_VAL;
	}

	return ReturnString(result);
}
// #endregion

// #region Texture
/***
 * Texture.Create
 * \desc
 * \return
 * \ns Texture
 */
VMValue Texture_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);

	int width = GET_ARG(0, GetInteger);
	int height = GET_ARG(1, GetInteger);
	int unloadPolicy = GET_ARG(2, GetInteger);

	GameTexture* texture = new GameTexture(width, height, unloadPolicy);
	size_t i = Scene::AddGameTexture(texture);

	return INTEGER_VAL((int)i);
}
/***
 * Texture.Copy
 * \desc
 * \return
 * \ns Texture
 */
VMValue Texture_Copy(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);

	GameTexture* textureA = GET_ARG(0, GetTexture);
	GameTexture* textureB = GET_ARG(1, GetTexture);

	if (!textureA || !textureB) {
		return NULL_VAL;
	}

	Texture* destTexture = textureA->GetTexture();
	Texture* srcTexture = textureB->GetTexture();

	if (destTexture && srcTexture) {
		destTexture->Copy(srcTexture);
	}

	return NULL_VAL;
}
// #endregion

// #region Touch
/***
 * Touch.GetX
 * \desc Gets the X position of a touch.
 * \param touchIndex (integer):
 * \return decimal Returns a decimal value.
 * \ns Touch
 */
VMValue Touch_GetX(int argCount, VMValue* args, Uint32 threadID) {
	int touch_index = GET_ARG(0, GetInteger);
	return DECIMAL_VAL(InputManager::TouchGetX(touch_index));
}
/***
 * Touch.GetY
 * \desc Gets the Y position of a touch.
 * \param touchIndex (integer):
 * \return decimal Returns a decimal value.
 * \ns Touch
 */
VMValue Touch_GetY(int argCount, VMValue* args, Uint32 threadID) {
	int touch_index = GET_ARG(0, GetInteger);
	return DECIMAL_VAL(InputManager::TouchGetY(touch_index));
}
/***
 * Touch.IsDown
 * \desc Gets whether a touch is currently down on the screen.
 * \param touchIndex (integer):
 * \return boolean Returns a boolean value.
 * \ns Touch
 */
VMValue Touch_IsDown(int argCount, VMValue* args, Uint32 threadID) {
	int touch_index = GET_ARG(0, GetInteger);
	return INTEGER_VAL(InputManager::TouchIsDown(touch_index));
}
/***
 * Touch.IsPressed
 * \desc Gets whether a touch just pressed down on the screen.
 * \param touchIndex (integer):
 * \return boolean Returns a boolean value.
 * \ns Touch
 */
VMValue Touch_IsPressed(int argCount, VMValue* args, Uint32 threadID) {
	int touch_index = GET_ARG(0, GetInteger);
	return INTEGER_VAL(InputManager::TouchIsPressed(touch_index));
}
/***
 * Touch.IsReleased
 * \desc Gets whether a touch just released from the screen.
 * \param touchIndex (integer):
 * \return boolean Returns a boolean value.
 * \ns Touch
 */
VMValue Touch_IsReleased(int argCount, VMValue* args, Uint32 threadID) {
	int touch_index = GET_ARG(0, GetInteger);
	return INTEGER_VAL(InputManager::TouchIsReleased(touch_index));
}
// #endregion

// #region TileCollision
/***
 * TileCollision.Point
 * \desc Checks for a tile collision at a specified point.
 * \param x (number): X position to check.
 * \param y (number): Y position to check.
 * \return boolean Returns a boolean value.
 * \ns TileCollision
 */
VMValue TileCollision_Point(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int x = (int)std::floor(GET_ARG(0, GetDecimal));
	int y = (int)std::floor(GET_ARG(1, GetDecimal));

	// 15, or 0b1111
	return INTEGER_VAL(Scene::CollisionAt(x, y, 0, 15, NULL) >= 0);
}
/***
 * TileCollision.PointExtended
 * \desc Checks for a tile collision at a specified point.
 * \param x (number): X position to check.
 * \param y (number): Y position to check.
 * \param collisionField (integer): Low (0) or high (1) field to check.
 * \param collisionSide (integer): Which side of the tile to check for collision. (TOP = 1, RIGHT = 2, BOTTOM = 4, LEFT = 8, ALL = 15)
 * \return integer Returns the angle of the ground if successful, `-1` if otherwise.
 * \ns TileCollision
 */
VMValue TileCollision_PointExtended(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int x = (int)std::floor(GET_ARG(0, GetDecimal));
	int y = (int)std::floor(GET_ARG(1, GetDecimal));
	int collisionField = GET_ARG(2, GetInteger);
	int collisionSide = GET_ARG(3, GetInteger);

	return INTEGER_VAL(Scene::CollisionAt(x, y, collisionField, collisionSide, NULL));
}
/***
 * TileCollision.Line
 * \desc Checks for a tile collision in a straight line.
 * \param x (number): X position to start checking from.
 * \param y (number): Y position to start checking from.
 * \param directionType (integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \param length (integer): How many pixels to check.
 * \param collisionField (integer): Low (0) or high (1) field to check.
 * \param compareAngle (integer): Only return a collision if the angle is within 0x20 this value, otherwise if angle comparison is not desired, set this value to -1.
 * \param entity (Entity): Instance to write the values to.
 * \return boolean Returns `false` if no tile collision, but if `true`: <br/>\
<pre class="code"><br/>\
    entity.SensorX: (number), // X Position where the sensor collided if it did. <br/>\
    entity.SensorY: (number), // Y Position where the sensor collided if it did. <br/>\
    entity.SensorCollided: (boolean), // Whether the sensor collided. <br/>\
    entity.SensorAngle: (integer) // Tile angle at the collision. <br/>\
\
</pre>
 * \ns TileCollision
 */
VMValue TileCollision_Line(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(7);
	int x = (int)std::floor(GET_ARG(0, GetDecimal));
	int y = (int)std::floor(GET_ARG(1, GetDecimal));
	int angleMode = GET_ARG(2, GetInteger);
	int length = (int)GET_ARG(3, GetDecimal);
	int collisionField = GET_ARG(4, GetInteger);
	int compareAngle = GET_ARG(5, GetInteger);
	ObjEntity* entity = GET_ARG(6, GetEntity);

	Sensor sensor;
	sensor.X = x;
	sensor.Y = y;
	sensor.Collided = false;
	sensor.Angle = 0;
	if (compareAngle > -1) {
		sensor.Angle = compareAngle & 0xFF;
	}

	Scene::CollisionInLine(x, y, angleMode, length, collisionField, compareAngle > -1, &sensor);

	if (ScriptManager::Lock()) {
		/*ObjMap*    mapSensor = NewMap();

        mapSensor->Values->Put("X", DECIMAL_VAL((float)sensor.X));
        mapSensor->Values->Put("Y", DECIMAL_VAL((float)sensor.Y));
        mapSensor->Values->Put("Collided", INTEGER_VAL(sensor.Collided));
        mapSensor->Values->Put("Angle", INTEGER_VAL(sensor.Angle));

        ScriptManager::Unlock();*/
		if (entity->EntityPtr) {
			auto ent = (Entity*)entity->EntityPtr;
			ent->SensorX = (float)sensor.X;
			ent->SensorY = (float)sensor.Y;
			ent->SensorCollided = sensor.Collided;
			ent->SensorAngle = sensor.Angle;
			return INTEGER_VAL(sensor.Collided);
		}
	}
	return INTEGER_VAL(false);
}
// #endregion

// #region TileInfo
/***
 * TileInfo.SetSpriteInfo
 * \desc Sets the sprite, animation, and frame to use for specified tile.
 * \param tileID (integer): ID of tile to check.
 * \param spriteIndex (integer): Sprite index. (`-1` for default tile sprite)
 * \param animationIndex (integer): Animation index.
 * \param frameIndex (integer): Frame index. (`-1` for default tile frame)
 * \ns TileInfo
 */
VMValue TileInfo_SetSpriteInfo(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int tileID = GET_ARG(0, GetInteger);
	int spriteIndex = GET_ARG(1, GetInteger);
	int animationIndex = GET_ARG(2, GetInteger);
	int frameIndex = GET_ARG(3, GetInteger);
	if (tileID >= 0 && tileID < (int)Scene::TileSpriteInfos.size()) {
		if (spriteIndex <= -1) {
			TileSpriteInfo& info = Scene::TileSpriteInfos[tileID];
			info.Sprite = Scene::Tilesets[0].Sprite;
			info.AnimationIndex = 0;
			if (frameIndex > -1) {
				info.FrameIndex = frameIndex;
			}
			else {
				info.FrameIndex = tileID;
			}
		}
		else {
			TileSpriteInfo& info = Scene::TileSpriteInfos[tileID];
			info.Sprite = GET_ARG(1, GetSprite);
			info.AnimationIndex = animationIndex;
			info.FrameIndex = frameIndex;
		}
	}
	return NULL_VAL;
}
/***
 * TileInfo.IsEmptySpace
 * \desc Checks to see if a tile at the ID is empty.
 * \param tileID (integer): ID of tile to check.
 * \param collisionPlane (integer): The collision plane of the tile to check for.
 * \return boolean Returns whether the tile is empty space.
 * \ns TileInfo
 */
VMValue TileInfo_IsEmptySpace(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	return INTEGER_VAL(tileID == Scene::EmptyTile);
}
/***
 * TileInfo.GetEmptyTile
 * \desc Gets the scene's empty tile ID.
 * \return integer Returns the ID of the scene's empty tile space.
 * \ns TileInfo
 */
VMValue TileInfo_GetEmptyTile(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::EmptyTile);
}
/***
 * TileInfo.GetCollision
 * \desc Gets the collision value at the pixel position of the desired tile, -1 if no collision.
 * \param tileID (integer): ID of the tile to get the value of.
 * \param collisionField (integer): The collision plane of the tile to get the collision from.
 * \param directionType (integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \param position (integer): Position on the tile to check, X position if the directionType is Up/Down, Y position if the directionType is Left/Right.
 * \paramOpt flipX (boolean): Whether to check the collision with the tile horizontally flipped.
 * \paramOpt flipY (boolean): Whether to check the collision with the tile vertically flipped.
 * \return integer Collision position on the tile, X position if the directionType is Left/Right, Y position if the directionType is Up/Down, -1 if there was no collision.
 * \ns TileInfo
 */
VMValue TileInfo_GetCollision(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(4);
	int tileID = GET_ARG(0, GetInteger);
	int collisionField = GET_ARG(1, GetInteger);
	int directionType = GET_ARG(2, GetInteger);
	int position = GET_ARG(3, GetInteger);
	int flipX = GET_ARG_OPT(4, GetInteger, 0);
	int flipY = GET_ARG_OPT(5, GetInteger, 0);

	if (!Scene::TileCfgLoaded) {
		THROW_ERROR("Tile collision data is not loaded.");
		return NULL_VAL;
	}

	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size() ||
		collisionField >= Scene::TileCfg.size()) {
		return INTEGER_VAL(-1);
	}

	TileConfig* tileCfgBase = Scene::TileCfg[collisionField];
	tileCfgBase = &tileCfgBase[tileID] + ((flipY << 1) | flipX) * Scene::TileCount;

	int cValue = -1;
	switch (directionType) {
	case 0:
		if (tileCfgBase->CollisionTop[position] < 0xF0) {
			cValue = tileCfgBase->CollisionTop[position];
		}
		break;
	case 1:
		if (tileCfgBase->CollisionLeft[position] < 0xF0) {
			cValue = tileCfgBase->CollisionLeft[position];
		}
		break;
	case 2:
		if (tileCfgBase->CollisionBottom[position] < 0xF0) {
			cValue = tileCfgBase->CollisionBottom[position];
		}
		break;
	case 3:
		if (tileCfgBase->CollisionRight[position] < 0xF0) {
			cValue = tileCfgBase->CollisionRight[position];
		}
		break;
	}

	return INTEGER_VAL(cValue);
}
/***
 * TileInfo.GetAngle
 * \desc Gets the angle value of the desired tile.
 * \param tileID (integer): ID of the tile to get the value of.
 * \param collisionField (integer): The collision plane of the tile to get the angle from.
 * \param directionType (integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \paramOpt flipX (boolean): Whether to check the angle with the tile horizontally flipped.
 * \paramOpt flipY (boolean): Whether to check the angle with the tile vertically flipped.
 * \return integer Angle value between 0x00 to 0xFF at the specified direction.
 * \ns TileInfo
 */
VMValue TileInfo_GetAngle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);
	int tileID = GET_ARG(0, GetInteger);
	int collisionField = GET_ARG(1, GetInteger);
	int directionType = GET_ARG(2, GetInteger);
	int flipX = GET_ARG_OPT(3, GetInteger, 0);
	int flipY = GET_ARG_OPT(4, GetInteger, 0);

	if (!Scene::TileCfgLoaded) {
		THROW_ERROR("Tile collision data is not loaded.");
		return NULL_VAL;
	}

	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size() ||
		collisionField >= Scene::TileCfg.size()) {
		return INTEGER_VAL(-1);
	}

	TileConfig* tileCfgBase = Scene::TileCfg[collisionField];
	tileCfgBase = &tileCfgBase[tileID] + ((flipY << 1) | flipX) * Scene::TileCount;

	int cValue = 0;
	switch (directionType) {
	case 0:
		cValue = tileCfgBase->AngleTop;
		break;
	case 1:
		cValue = tileCfgBase->AngleLeft;
		break;
	case 2:
		cValue = tileCfgBase->AngleBottom;
		break;
	case 3:
		cValue = tileCfgBase->AngleRight;
		break;
	}

	return INTEGER_VAL(cValue);
}
/***
 * TileInfo.GetBehaviorFlag
 * \desc Gets the behavior value of the desired tile.
 * \param tileID (integer): ID of the tile to get the value of.
 * \param collisionPlane (integer): The collision plane of the tile to get the behavior from.
 * \return integer Behavior flag of the tile.
 * \ns TileInfo
 */
VMValue TileInfo_GetBehaviorFlag(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	int collisionPlane = GET_ARG(1, GetInteger);

	if (!Scene::TileCfgLoaded) {
		THROW_ERROR("Tile Collision is not loaded.");
		return NULL_VAL;
	}

	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size() ||
		collisionPlane >= Scene::TileCfg.size()) {
		return INTEGER_VAL(0);
	}

	TileConfig* tileCfgBase = Scene::TileCfg[collisionPlane];

	return INTEGER_VAL(tileCfgBase[tileID].Behavior);
}
/***
 * TileInfo.IsCeiling
 * \desc Checks if the desired tile is a ceiling tile.
 * \param tileID (integer): ID of the tile to check.
 * \param collisionPlane (integer): The collision plane of the tile to check.
 * \return boolean Returns whether the tile is a ceiling tile.
 * \ns TileInfo
 */
VMValue TileInfo_IsCeiling(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int tileID = GET_ARG(0, GetInteger);
	int collisionPlane = GET_ARG(1, GetInteger);

	if (!Scene::TileCfgLoaded) {
		THROW_ERROR("Tile collision data is not loaded.");
		return NULL_VAL;
	}

	if (tileID < 0 || tileID >= (int)Scene::TileSpriteInfos.size() ||
		collisionPlane >= Scene::TileCfg.size()) {
		return INTEGER_VAL(0);
	}

	TileConfig* tileCfgBase = Scene::TileCfg[collisionPlane];

	return INTEGER_VAL(tileCfgBase[tileID].IsCeiling);
}
// #endregion

// #region Thread
struct _Thread_Bundle {
	ObjFunction Callback;
	int ArgCount;
	int ThreadIndex;
};

int _Thread_RunEvent(void* op) {
	_Thread_Bundle* bundle;
	bundle = (_Thread_Bundle*)op;

	VMValue* args = (VMValue*)(bundle + 1);
	VMThread* thread = ScriptManager::Threads + bundle->ThreadIndex;
	VMValue callbackVal = OBJECT_VAL(&bundle->Callback);

	// if (bundle->Callback.Method == NULL) {
	//     Log::Print(Log::LOG_ERROR, "No callback.");
	//     ScriptManager::ThreadCount--;
	//     free(bundle);
	//     return 0;
	// }

	thread->Push(callbackVal);
	for (int i = 0; i < bundle->ArgCount; i++) {
		thread->Push(args[i]);
	}
	thread->RunValue(callbackVal, bundle->ArgCount);

	free(bundle);

	ScriptManager::ThreadCount--;
	Log::Print(Log::LOG_IMPORTANT, "Thread %d closed.", ScriptManager::ThreadCount);
	return 0;
}
/***
 * Thread.RunEvent
 * \desc
 * \return
 * \ns Thread
 */
VMValue Thread_RunEvent(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	ObjFunction* callback = NULL;
	if (IS_BOUND_METHOD(args[0])) {
		callback = GET_ARG(0, GetBoundMethod)->Method;
	}
	else if (IS_FUNCTION(args[0])) {
		callback = AS_FUNCTION(args[0]);
	}

	if (callback == NULL) {
		ValuePrinter::Print(args[0]);
		printf("\n");
		Log::Print(Log::LOG_ERROR, "No callback.");
		return NULL_VAL;
	}

	int subArgCount = argCount - 1;

	_Thread_Bundle* bundle =
		(_Thread_Bundle*)malloc(sizeof(_Thread_Bundle) + subArgCount * sizeof(VMValue));
	bundle->Callback = *callback;
	bundle->Callback.Object.Next = NULL;
	bundle->ArgCount = subArgCount;
	bundle->ThreadIndex = ScriptManager::ThreadCount++;
	if (subArgCount > 0) {
		memcpy(bundle + 1, args + 1, subArgCount * sizeof(VMValue));
	}

	// SDL_DetachThread
	SDL_CreateThread(_Thread_RunEvent, "_Thread_RunEvent", bundle);

	return NULL_VAL;
}
/***
 * Thread.Sleep
 * \desc
 * \return
 * \ns Thread
 */
VMValue Thread_Sleep(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	SDL_Delay(GET_ARG(0, GetInteger));
	return NULL_VAL;
}
// #endregion

// #region VertexBuffer
/***
 * VertexBuffer.Create
 * \desc Create a vertex buffer.
 * \param numVertices (integer): The initial capacity of this vertex buffer.
 * \param unloadPolicy (integer): Whether to delete the vertex buffer at the end of the current Scene, or the game end.
 * \return integer The index of the created vertex buffer.
 * \ns VertexBuffer
 */
VMValue VertexBuffer_Create(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 numVertices = GET_ARG(0, GetInteger);
	Uint32 unloadPolicy = GET_ARG(1, GetInteger);
	Uint32 vertexBufferIndex = Graphics::CreateVertexBuffer(numVertices, unloadPolicy);
	if (vertexBufferIndex == 0xFFFFFFFF) {
		THROW_ERROR("No more vertex buffers available.");
		return NULL_VAL;
	}
	return INTEGER_VAL((int)vertexBufferIndex);
}
/***
 * VertexBuffer.Resize
 * \desc Resizes a vertex buffer.
 * \param vertexBufferIndex (integer): The vertex buffer to resize.
 * \param numVertices (integer): The amount of vertices that this vertex buffer will hold.
 * \return
 * \ns VertexBuffer
 */
VMValue VertexBuffer_Resize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	Uint32 vertexBufferIndex = GET_ARG(0, GetInteger);
	Uint32 numVertices = GET_ARG(1, GetInteger);
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return NULL_VAL;
	}

	VertexBuffer* buffer = Graphics::VertexBuffers[vertexBufferIndex];
	if (buffer) {
		buffer->Resize(numVertices);
	}
	return NULL_VAL;
}
/***
 * VertexBuffer.Clear
 * \desc Clears a vertex buffer.
 * \param vertexBufferIndex (integer): The vertex buffer to clear.
 * \return
 * \ns VertexBuffer
 */
VMValue VertexBuffer_Clear(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 vertexBufferIndex = GET_ARG(0, GetInteger);
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return NULL_VAL;
	}

	VertexBuffer* buffer = Graphics::VertexBuffers[vertexBufferIndex];
	if (buffer) {
		buffer->Clear();
	}
	return NULL_VAL;
}
/***
 * VertexBuffer.Delete
 * \desc Deletes a vertex buffer.
 * \param vertexBufferIndex (integer): The vertex buffer to delete.
 * \return
 * \ns VertexBuffer
 */
VMValue VertexBuffer_Delete(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Uint32 vertexBufferIndex = GET_ARG(0, GetInteger);
	if (vertexBufferIndex < 0 || vertexBufferIndex >= MAX_VERTEX_BUFFERS) {
		return NULL_VAL;
	}

	Graphics::DeleteVertexBuffer(vertexBufferIndex);
	return NULL_VAL;
}
// #endregion

// #region Video
/***
 * Video.Play
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_Play(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	video->Player->Play();
#endif
	return NULL_VAL;
}
/***
 * Video.Pause
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_Pause(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	video->Player->Pause();
#endif
	return NULL_VAL;
}
/***
 * Video.Resume
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_Resume(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	video->Player->Play();
#endif
	return NULL_VAL;
}
/***
 * Video.Stop
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_Stop(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	video->Player->Stop();
	video->Player->Seek(0);
#endif
	return NULL_VAL;
}
/***
 * Video.Close
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_Close(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);

	int index = GET_ARG(0, GetInteger);
	ResourceType* resource = Scene::MediaList[index];

	Scene::MediaList[index] = NULL;

#ifdef USING_FFMPEG
	video->Source->Close();
	video->Player->Close();
#endif

	if (!resource) {
		return NULL_VAL;
	}

	if (resource->AsMedia) {
		delete resource->AsMedia;
	}

	delete resource;

	return NULL_VAL;
}
/***
 * Video.IsBuffering
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_IsBuffering(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return INTEGER_VAL(video->Player->IsOutputEmpty());
#endif
	return NULL_VAL;
}
/***
 * Video.IsPlaying
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_IsPlaying(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return INTEGER_VAL(video->Player->GetPlayerState() == MediaPlayer::KIT_PLAYING);
#endif
	return NULL_VAL;
}
/***
 * Video.IsPaused
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_IsPaused(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return INTEGER_VAL(video->Player->GetPlayerState() == MediaPlayer::KIT_PAUSED);
#endif
	return NULL_VAL;
}
/***
 * Video.SetPosition
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_SetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	MediaBag* video = GET_ARG(0, GetVideo);
	float position = GET_ARG(1, GetDecimal);
#ifdef USING_FFMPEG
	video->Player->Seek(position);
#endif
	return NULL_VAL;
}
/***
 * Video.SetBufferDuration
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_SetBufferDuration(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.SetTrackEnabled
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_SetTrackEnabled(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetPosition
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return DECIMAL_VAL((float)video->Player->GetPosition());
#endif
	return NULL_VAL;
}
/***
 * Video.GetDuration
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetDuration(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return DECIMAL_VAL((float)video->Player->GetDuration());
#endif
	return NULL_VAL;
}
/***
 * Video.GetBufferDuration
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetBufferDuration(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetBufferEnd
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetBufferEnd(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
#ifdef USING_FFMPEG
	return DECIMAL_VAL((float)video->Player->GetBufferPosition());
#endif
	return NULL_VAL;
}
/***
 * Video.GetTrackCount
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetTrackCount(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetTrackEnabled
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetTrackEnabled(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetTrackName
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetTrackName(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetTrackLanguage
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetTrackLanguage(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetDefaultVideoTrack
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetDefaultVideoTrack(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetDefaultAudioTrack
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetDefaultAudioTrack(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetDefaultSubtitleTrack
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetDefaultSubtitleTrack(int argCount, VMValue* args, Uint32 threadID) {
	return NULL_VAL;
}
/***
 * Video.GetWidth
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
	if (video->VideoTexture) {
		return INTEGER_VAL((int)video->VideoTexture->Width);
	}

	return INTEGER_VAL(-1);
}
/***
 * Video.GetHeight
 * \desc
 * \return
 * \ns Video
 */
VMValue Video_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	MediaBag* video = GET_ARG(0, GetVideo);
	if (video->VideoTexture) {
		return INTEGER_VAL((int)video->VideoTexture->Height);
	}

	return INTEGER_VAL(-1);
}
// #endregion

// #region View
/***
 * View.SetX
 * \desc Sets the x-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X position.
 * \ns View
 */
VMValue View_SetX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float value = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].X = value;
	return NULL_VAL;
}
/***
 * View.SetY
 * \desc Sets the y-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param y (number): Desired Y position.
 * \ns View
 */
VMValue View_SetY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float value = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Y = value;
	return NULL_VAL;
}
/***
 * View.SetZ
 * \desc Sets the z-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param z (number): Desired Z position.
 * \ns View
 */
VMValue View_SetZ(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float value = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Z = value;
	return NULL_VAL;
}
/***
 * View.SetPosition
 * \desc Sets the position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X position.
 * \param y (number): Desired Y position.
 * \paramOpt z (number): Desired Z position.
 * \ns View
 */
VMValue View_SetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].X = GET_ARG(1, GetDecimal);
	Scene::Views[view_index].Y = GET_ARG(2, GetDecimal);
	if (argCount == 4) {
		Scene::Views[view_index].Z = GET_ARG(3, GetDecimal);
	}
	return NULL_VAL;
}
/***
 * View.AdjustX
 * \desc Adjusts the x-axis position of the camera for the specified view by an amount.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X adjust amount.
 * \ns View
 */
VMValue View_AdjustX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].X += GET_ARG(1, GetDecimal);
	return NULL_VAL;
}
/***
* View.AdjustY
* \desc Adjusts the y-axis position of the camera for the specified view by an amount.
* \param viewIndex (integer): Index of the view.
* \param y (number): Desired Y adjust amount.
* \ns View
*/
VMValue View_AdjustY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Y += GET_ARG(1, GetDecimal);
	return NULL_VAL;
}
/***
* View.AdjustZ
* \desc Adjusts the z-axis position of the camera for the specified view by an amount.
* \param viewIndex (integer): Index of the view.
* \param z (number): Desired Z adjust amount.
* \ns View
*/
VMValue View_AdjustZ(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Z += GET_ARG(1, GetDecimal);
	return NULL_VAL;
}
/***
 * View.SetScale
 * \desc Sets the scale of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X scale.
 * \param y (number): Desired Y scale.
 * \paramOpt z (number): Desired Z scale.
 * \ns View
 */
VMValue View_SetScale(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].ScaleX = GET_ARG(1, GetDecimal);
	Scene::Views[view_index].ScaleY = GET_ARG(2, GetDecimal);
	Scene::Views[view_index].ScaleZ = GET_ARG_OPT(3, GetDecimal, 1.0);
	return NULL_VAL;
}
/***
 * View.SetAngle
 * \desc Sets the angle of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X angle.
 * \param y (number): Desired Y angle.
 * \param z (number): Desired Z angle.
 * \ns View
 */
VMValue View_SetAngle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].RotateX = GET_ARG(1, GetDecimal);
	Scene::Views[view_index].RotateY = GET_ARG(2, GetDecimal);
	Scene::Views[view_index].RotateZ = GET_ARG(3, GetDecimal);
	return NULL_VAL;
}
/***
 * View.SetSize
 * \desc Sets the size of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param width (number): Desired width.
 * \param height (number): Desired height.
 * \return
 * \ns View
 */
VMValue View_SetSize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int view_index = GET_ARG(0, GetInteger);
	float view_w = GET_ARG(1, GetDecimal);
	float view_h = GET_ARG(2, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].SetSize(view_w, view_h);
	return NULL_VAL;
}
/***
 * View.SetOutputX
 * \desc Sets the x-axis output position of the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X position.
 * \ns View
 */
VMValue View_SetOutputX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float value = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].OutputX = value;
	return NULL_VAL;
}
/***
 * View.SetOutputY
 * \desc Sets the y-axis output position of the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param y (number): Desired Y position.
 * \ns View
 */
VMValue View_SetOutputY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float value = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].OutputY = value;
	return NULL_VAL;
}
/***
 * View.SetOutputPosition
 * \desc Sets the output position of the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param x (number): Desired X position.
 * \param y (number): Desired Y position.
 * \ns View
 */
VMValue View_SetOutputPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(3);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].OutputX = GET_ARG(1, GetDecimal);
	Scene::Views[view_index].OutputY = GET_ARG(2, GetDecimal);
	return NULL_VAL;
}
/***
 * View.SetOutputSize
 * \desc Sets the output size of the specified view.
 * \param viewIndex (integer): Index of the view.
 * \param width (number): Desired width.
 * \param height (number): Desired height.
 * \ns View
 */
VMValue View_SetOutputSize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(3);
	int view_index = GET_ARG(0, GetInteger);
	float view_w = GET_ARG(1, GetDecimal);
	float view_h = GET_ARG(2, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].OutputWidth = view_w;
	Scene::Views[view_index].OutputHeight = view_h;
	return NULL_VAL;
}
/***
 * View.GetX
 * \desc Gets the x-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(Scene::Views[view_index].X);
}
/***
 * View.GetY
 * \desc Gets the y-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(Scene::Views[GET_ARG(0, GetInteger)].Y);
}
/***
 * View.GetZ
 * \desc Gets the z-axis position of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetZ(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(Scene::Views[GET_ARG(0, GetInteger)].Z);
}
/***
 * View.GetWidth
 * \desc Gets the width of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(Scene::Views[view_index].Width);
}
/***
 * View.GetHeight
 * \desc Gets the height of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(Scene::Views[view_index].Height);
}
/***
 * View.GetCenterX
 * \desc Gets the x center of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetCenterX(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(((float)Scene::Views[view_index].Width) / 2.0f);
}
/***
 * View.GetCenterY
 * \desc Gets the y center of the camera for the specified view.
 * \param viewIndex (integer): Index of the view.
 * \return decimal Returns a decimal value.
 * \ns View
 */
VMValue View_GetCenterY(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return DECIMAL_VAL(((float)Scene::Views[view_index].Height) / 2.0f);
}
/***
 * View.IsUsingDrawTarget
 * \desc Gets whether the specified camera is using a draw target.
 * \param viewIndex (integer): Index of the view.
 * \return boolean Returns a boolean value.
 * \ns View
 */
VMValue View_IsUsingDrawTarget(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return INTEGER_VAL((int)Scene::Views[view_index].UseDrawTarget);
}
/***
 * View.SetUseDrawTarget
 * \desc Sets the specified camera to use a draw target.
 * \param viewIndex (integer): Index of the view.
 * \param useDrawTarget (boolean): Whether to use a draw target.
 * \ns View
 */
VMValue View_SetUseDrawTarget(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int usedrawtar = GET_ARG(1, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].UseDrawTarget = !!usedrawtar;
	return NULL_VAL;
}
/***
 * View.GetDrawTarget
 * \desc Gets the specified camera's draw target.
 * \param viewIndex (integer): Index of the view.
 * \return integer Returns an integer value.
 * \ns View
 */
VMValue View_GetDrawTarget(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	if (!Scene::Views[view_index].UseDrawTarget) {
		THROW_ERROR("View %d lacks a draw target!", view_index);
		return NULL_VAL;
	}

	size_t i = 0;

	if (!Scene::FindGameTextureByID(-(view_index + 1), i)) {
		GameTexture* texture = new ViewTexture(view_index);
		i = Scene::AddGameTexture(texture);
	}

	return INTEGER_VAL((int)i);
}
/***
 * View.SetShader
 * \desc Sets a shader for the specified camera.
 * \param viewIndex (integer): Index of the view.
 * \param shader (Shader): The shader, or `null` to unset the shader.
 * \ns View
 */
VMValue View_SetShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	Shader* shader = nullptr;

	CHECK_VIEW_INDEX();

	if (!IS_NULL(args[1])) {
		ObjShader* objShader = GET_ARG(1, GetShader);
		shader = (Shader*)objShader->ShaderPtr;

		if (shader == nullptr) {
			THROW_ERROR("Shader has been deleted!");
			return NULL_VAL;
		}
	}

	Scene::Views[view_index].CurrentShader = shader;
	return NULL_VAL;
}
/***
 * View.GetShader
 * \desc Gets the shader of the specified camera.
 * \param viewIndex (integer): Index of the view.
 * \return Shader Returns a Shader, or `null`.
 * \ns View
 */
VMValue View_GetShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);

	CHECK_VIEW_INDEX();

	Shader* shader = Scene::Views[view_index].CurrentShader;
	if (shader != nullptr) {
		return OBJECT_VAL(shader->Object);
	}

	return NULL_VAL;
}
/***
 * View.IsUsingSoftwareRenderer
 * \desc Gets whether the specified camera is using the software renderer.
 * \param viewIndex (integer): Index of the view.
 * \return boolean Returns a boolean value.
 * \ns View
 */
VMValue View_IsUsingSoftwareRenderer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return INTEGER_VAL((int)Scene::Views[view_index].Software);
}
/***
 * View.SetUseSoftwareRenderer
 * \desc Sets the specified camera to use the software renderer.
 * \param viewIndex (integer): Index of the view.
 * \param useSoftwareRenderer (boolean): Whether to use the software renderer.
 * \ns View
 */
VMValue View_SetUseSoftwareRenderer(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int usedswrend = GET_ARG(1, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Software = !!usedswrend;
	return NULL_VAL;
}
/***
 * View.SetUsePerspective
 * \desc
 * \return
 * \ns View
 */
VMValue View_SetUsePerspective(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);
	int view_index = GET_ARG(0, GetInteger);
	int useperspec = GET_ARG(1, GetInteger);
	float nearPlane = GET_ARG(2, GetDecimal);
	float farPlane = GET_ARG(3, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].UsePerspective = !!useperspec;
	Scene::Views[view_index].NearPlane = nearPlane;
	Scene::Views[view_index].FarPlane = farPlane;
	return NULL_VAL;
}
/***
 * View.IsEnabled
 * \desc Gets whether the specified camera is active.
 * \param viewIndex (integer): Index of the view.
 * \return boolean Returns a boolean value.
 * \ns View
 */
VMValue View_IsEnabled(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return INTEGER_VAL(Scene::Views[view_index].Active);
}
/***
 * View.SetEnabled
 * \desc Sets the specified camera to be active.
 * \param viewIndex (integer): Index of the view.
 * \param enabled (boolean): Whether the camera should be enabled.
 * \ns View
 */
VMValue View_SetEnabled(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int enabled = !!GET_ARG(1, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::SetViewActive(view_index, enabled);
	return NULL_VAL;
}
/***
 * View.IsVisible
 * \desc Gets whether the specified camera is visible.
 * \param viewIndex (integer): Index of the view.
 * \return boolean Returns a boolean value.
 * \ns View
 */
VMValue View_IsVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return INTEGER_VAL(Scene::Views[view_index].Visible);
}
/***
 * View.SetVisible
 * \desc Sets the specified camera to be visible.
 * \param viewIndex (integer): Index of the view.
 * \param visible (boolean): Whether the camera should be visible.
 * \ns View
 */
VMValue View_SetVisible(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int visible = GET_ARG(1, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].Visible = !!visible;
	return NULL_VAL;
}
/***
 * View.SetFieldOfView
 * \desc
 * \return
 * \ns View
 */
VMValue View_SetFieldOfView(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	float fovy = GET_ARG(1, GetDecimal);
	CHECK_VIEW_INDEX();
	Scene::Views[view_index].FOV = fovy;
	return NULL_VAL;
}
/***
 * View.SetPriority
 * \desc Gets the specified view's priority.
 * \param viewIndex (integer): Index of the view.
 * \return integer Returns an integer value.
 * \ns View
 */
VMValue View_SetPriority(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	int view_index = GET_ARG(0, GetInteger);
	int priority = GET_ARG(1, GetInteger);
	CHECK_VIEW_INDEX();
	Scene::SetViewPriority(view_index, priority);
	return NULL_VAL;
}
/***
 * View.GetPriority
 * \desc Gets the specified view's priority.
 * \param viewIndex (integer): Index of the view.
 * \param priority (integer):
 * \ns View
 */
VMValue View_GetPriority(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int view_index = GET_ARG(0, GetInteger);
	CHECK_VIEW_INDEX();
	return INTEGER_VAL(Scene::Views[view_index].Priority);
}
/***
 * View.GetCurrent
 * \desc Gets the current view index being drawn to.
 * \return integer Returns an integer value.
 * \ns View
 */
VMValue View_GetCurrent(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::ViewCurrent);
}
/***
 * View.GetCount
 * \desc Gets the total amount of views.
 * \return integer Returns an integer value.
 * \ns View
 */
VMValue View_GetCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(MAX_SCENE_VIEWS);
}
/***
 * View.GetActiveCount
 * \desc Gets the total amount of views currently activated.
 * \return integer Returns an integer value.
 * \ns View
 */
VMValue View_GetActiveCount(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Scene::ViewsActive);
}
/***
 * View.CheckOnScreen
 * \desc Determines whether an instance is on screen.
 * \param entity (Entity): The instance to check.
 * \paramOpt rangeX (decimal): The x range to check, or `null` if the entity's update region width should be used.
 * \paramOpt rangeY (decimal): The y range to check, or `null` if the entity's update region height should be used.
 * \return boolean Returns whether the instance is on screen in any view.
 * \ns View
 */
VMValue View_CheckOnScreen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_AT_LEAST_ARGCOUNT(1);

	ObjEntity* instance = GET_ARG(0, GetEntity);
	Entity* self = (Entity*)instance->EntityPtr;

	if (!self) {
		return INTEGER_VAL(false);
	}

	float rangeX = self->OnScreenHitboxW;
	float rangeY = self->OnScreenHitboxH;

	if (argCount >= 2 && !IS_NULL(args[1])) {
		rangeX = GET_ARG(1, GetDecimal);
	}
	if (argCount >= 3 && !IS_NULL(args[2])) {
		rangeY = GET_ARG(2, GetDecimal);
	}

	return INTEGER_VAL(Scene::CheckPosOnScreen(self->X, self->Y, rangeX, rangeY));
}
/***
 * View.CheckPosOnScreen
 * \desc Determines whether a position is on screen.
 * \param posX (decimal): The x position to check.
 * \param posY (decimal): The y position to check.
 * \param rangeX (decimal): The x range to check.
 * \param rangeY (decimal): The y range to check.
 * \return boolean Returns whether the position is on screen in any view.
 * \ns View
 */
VMValue View_CheckPosOnScreen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(4);

	return INTEGER_VAL(Scene::CheckPosOnScreen(GET_ARG(0, GetDecimal),
		GET_ARG(1, GetDecimal),
		GET_ARG(2, GetDecimal),
		GET_ARG(3, GetDecimal)));
}
// #endregion

// #region Window
/***
 * Window.SetSize
 * \desc Sets the size of the active window.
 * \param width (number): Window width
 * \param height (number): Window height
 * \ns Window
 */
VMValue Window_SetSize(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	if (!Application::IsWindowResizeable()) {
		return NULL_VAL;
	}

	int window_w = (int)GET_ARG(0, GetDecimal);
	int window_h = (int)GET_ARG(1, GetDecimal);
	Application::WindowWidth = window_w;
	Application::WindowHeight = window_h;
	Application::SetWindowSize(window_w * Application::WindowScale, window_h * Application::WindowScale);
	return NULL_VAL;
}
/***
 * Window.SetFullscreen
 * \desc Sets the fullscreen state of the active window.
 * \param isFullscreen (boolean): Whether the window should be fullscreen.
 * \ns Window
 */
VMValue Window_SetFullscreen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::SetWindowFullscreen(!!GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Window.SetScale
 * \desc Sets the scale of the active window.
 * \param scale (integer): Window scale.
 * \ns Window
 */
VMValue Window_SetScale(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	int scale = GET_ARG(0, GetInteger);
	Application::SetWindowScale(scale);
	return NULL_VAL;
}
/***
 * Window.SetBorderless
 * \desc Sets the bordered state of the active window.
 * \param isBorderless (boolean): Whether the window should be borderless.
 * \ns Window
 */
VMValue Window_SetBorderless(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Application::SetWindowBorderless(!!GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Window.SetVSync
 * \desc Enables or disables V-Sync for the active window.
 * \param enableVsync (boolean): Whether the window should use V-Sync.
 * \ns Window
 */
VMValue Window_SetVSync(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	Graphics::SetVSync((SDL_bool)!GET_ARG(0, GetInteger));
	return NULL_VAL;
}
/***
 * Window.SetPosition
 * \desc Sets the position of the active window.
 * \param x (number): Window x
 * \param y (number): Window y
 * \ns Window
 */
VMValue Window_SetPosition(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(2);
	SDL_SetWindowPosition(Application::Window, GET_ARG(0, GetInteger), GET_ARG(1, GetInteger));
	return NULL_VAL;
}
/***
 * Window.SetPositionCentered
 * \desc Sets the position of the active window to the center of the display.
 * \ns Window
 */
VMValue Window_SetPositionCentered(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	SDL_SetWindowPosition(Application::Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	return NULL_VAL;
}
/***
 * Window.SetTitle
 * \desc Sets the title of the active window.
 * \param title (string): Window title
 * \ns Window
 */
VMValue Window_SetTitle(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	char* string = GET_ARG(0, GetString);
	Application::SetWindowTitle(string);
	return NULL_VAL;
}
/***
 * Window.SetPostProcessingShader
 * \desc Sets a post-processing shader for the active window.
 * \param shader (Shader): The shader, or `null` to unset the shader.
 * \ns Window
 */
VMValue Window_SetPostProcessingShader(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);

	if (IS_NULL(args[0])) {
		Graphics::SetPostProcessShader(nullptr);
		return NULL_VAL;
	}

	ObjShader* objShader = GET_ARG(0, GetShader);
	Shader* shader = (Shader*)objShader->ShaderPtr;
	if (shader == nullptr) {
		THROW_ERROR("Shader has been deleted!");
		return NULL_VAL;
	}

	try {
		shader->Validate();

		Graphics::SetPostProcessShader(shader);
	} catch (const std::runtime_error& error) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(false, "%s", error.what());
	}

	return NULL_VAL;
}
/***
 * Window.GetWidth
 * \desc Gets the width of the active window.
 * \return integer Returns the width of the active window.
 * \ns Window
 */
VMValue Window_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int v;
	SDL_GetWindowSize(Application::Window, &v, NULL);
	return INTEGER_VAL(v);
}
/***
 * Window.GetHeight
 * \desc Gets the height of the active window.
 * \return integer Returns the height of the active window.
 * \ns Window
 */
VMValue Window_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	int v;
	SDL_GetWindowSize(Application::Window, NULL, &v);
	return INTEGER_VAL(v);
}
/***
 * Window.GetFullscreen
 * \desc Gets whether the active window is currently fullscreen.
 * \return boolean Returns whether the window is fullscreen.
 * \ns Window
 */
VMValue Window_GetFullscreen(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::GetWindowFullscreen());
}
/***
 * Window.GetScale
 * \desc Gets the scale of the active window.
 * \return integer Returns the scale of the window.
 * \ns Window
 */
VMValue Window_GetScale(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::WindowScale);
}
/***
 * Window.IsResizeable
 * \desc Gets whether the active window is resizeable.
 * \return boolean Returns whether the window is resizeable.
 * \ns Window
 */
VMValue Window_IsResizeable(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(0);
	return INTEGER_VAL(Application::IsWindowResizeable());
}
// #endregion

// #region XML
static VMValue XML_FillMap(XMLNode* parent) {
	ObjMap* map = NewMap();
	Uint32 keyHash;

	XMLAttributes* attributes = &parent->attributes;
	size_t numAttr = attributes->KeyVector.size();
	size_t numChildren = parent->children.size();

	if (numChildren == 1 && !parent->children[0]->children.size()) {
		Token text = parent->children[0]->name;

		if (numAttr) {
			char* textKey = StringUtils::Duplicate("#text");
			keyHash = map->Keys->HashFunction(textKey, 5);
			map->Keys->Put(keyHash, textKey);
			map->Values->Put(keyHash, OBJECT_VAL(CopyString(text.Start, text.Length)));
		}
		else {
			return OBJECT_VAL(CopyString(text.Start, text.Length));
		}
	}
	else {
		for (size_t i = 0; i < numChildren; i++) {
			XMLNode* node = parent->children[i];

			Token* nodeName = &node->name;
			keyHash = map->Keys->HashFunction(nodeName->Start, nodeName->Length);

			// If the key already exists, push into it
			if (map->Keys->Exists(keyHash)) {
				VMValue thisVal = map->Values->Get(keyHash);
				ObjArray* thisArray = NULL;

				// Turn the value into an array if it isn't one
				if (!IS_ARRAY(thisVal)) {
					thisArray = NewArray();
					thisArray->Values->push_back(thisVal);
					map->Values->Put(keyHash, OBJECT_VAL(thisArray));
				}
				else {
					thisArray = AS_ARRAY(thisVal);
				}

				thisArray->Values->push_back(XML_FillMap(node));
			}
			else {
				map->Keys->Put(keyHash,
					StringUtils::Duplicate(nodeName->Start, nodeName->Length));
				map->Values->Put(keyHash, XML_FillMap(node));
			}
		}
	}

	// Insert attributes
	if (!numAttr) {
		return OBJECT_VAL(map);
	}

	char* attrName = NULL;
	size_t attrNameSize = 0;

	for (size_t i = 0; i < numAttr; i++) {
		char* key = attributes->KeyVector[i];
		char* value = XMLParser::TokenToString(attributes->ValueMap.Get(key));

		size_t length = strlen(key) + 2;
		if (length > attrNameSize) {
			attrNameSize = length;
			attrName = (char*)realloc(attrName, attrNameSize);
			if (!attrName) {
				Error::Fatal("Out of memory parsing XML!");
			}
		}

		snprintf(attrName, attrNameSize, "#%s", key);

		keyHash = map->Keys->HashFunction(attrName, attrNameSize - 1);
		map->Keys->Put(keyHash, StringUtils::Duplicate(attrName));
		map->Values->Put(keyHash, OBJECT_VAL(CopyString(value)));

		Memory::Free(value);
	}

	free(attrName);

	return OBJECT_VAL(map);
}
/***
 * XML.Parse
 * \desc Decodes a string value into a map value.
 * \param xmlText (string): XML-compliant text.
 * \return map Returns a map value if the text can be decoded, otherwise returns `null`.
 * \ns XML
 */
VMValue XML_Parse(int argCount, VMValue* args, Uint32 threadID) {
	CHECK_ARGCOUNT(1);
	ObjString* string = GET_ARG(0, GetVMString);
	if (!string) return NULL_VAL;

	VMValue mapValue = NULL_VAL;
	if (ScriptManager::Lock()) {
		char* text = StringUtils::Duplicate(string->Chars);

		MemoryStream* stream = MemoryStream::New(text, strlen(text));
		if (stream) {
			XMLNode* xmlRoot = XMLParser::ParseFromStream(stream);
			if (xmlRoot) {
				mapValue = XML_FillMap(xmlRoot);

				// XMLParser will realloc text, so the stream needs to free it.
				stream->owns_memory = true;
				XMLParser::Free(xmlRoot);
			}
			else {
				stream->Close();
			}
		}
		else {
			Memory::Free(text);
		}

		ScriptManager::Unlock();
	}
	return mapValue;
}
// #endregion

#define String_CaseMapBind(lowerCase, upperCase) \
	String_ToUpperCase_Map_ExtendedASCII[(Uint8)lowerCase] = (Uint8)upperCase; \
	String_ToLowerCase_Map_ExtendedASCII[(Uint8)upperCase] = (Uint8)lowerCase;

#define DEF_CONST_INT(a, b) ScriptManager::GlobalConstInteger(NULL, a, b)
#define DEF_LINK_INT(a, b) ScriptManager::GlobalLinkInteger(NULL, a, b)
#define DEF_CONST_DECIMAL(a, b) ScriptManager::GlobalConstDecimal(NULL, a, b)
#define DEF_LINK_DECIMAL(a, b) ScriptManager::GlobalLinkDecimal(NULL, a, b)
#define DEF_ENUM(a) DEF_CONST_INT(#a, a)
#define DEF_ENUM_CLASS(a, b) DEF_CONST_INT(#a "_" #b, (int)a::b)
#define DEF_ENUM_NAMED(a, b, c) DEF_CONST_INT(a "_" #c, (int)b::c)
#define DEF_ENUM_IN_CLASS(a, b) ScriptManager::GlobalConstInteger(klass, a, b)

void StandardLibrary::Link() {
	ObjClass* klass;

	for (int i = 0; i < 0x100; i++) {
		String_ToUpperCase_Map_ExtendedASCII[i] = (Uint8)i;
		String_ToLowerCase_Map_ExtendedASCII[i] = (Uint8)i;
	}
	for (int i = 'a'; i <= 'z'; i++) {
		int upperCase = i + ('A' - 'a');
		String_ToUpperCase_Map_ExtendedASCII[i] = (Uint8)upperCase;
		String_ToLowerCase_Map_ExtendedASCII[upperCase] = (Uint8)i;
	}
	String_CaseMapBind(L'ç', L'Ç');
	String_CaseMapBind(L'ü', L'Ü');
	String_CaseMapBind(L'á', L'Ç');
	String_CaseMapBind(L'é', L'É');
	String_CaseMapBind(L'ç', L'Ç');
	String_CaseMapBind(L'ç', L'Ç');
	String_CaseMapBind(L'ç', L'Ç');
	String_CaseMapBind(L'ç', L'Ç');
	String_CaseMapBind(L'ç', L'Ç');

	textAlign = 0.0f;
	textBaseline = 0.0f;
	textAscent = 1.25f;
	textAdvance = 1.0f;

#define INIT_CLASS(className) \
	klass = NewClass(#className); \
	ScriptManager::Constants->Put(klass->Hash, OBJECT_VAL(klass));
#define DEF_NATIVE(className, funcName) \
	ScriptManager::DefineNative(klass, #funcName, className##_##funcName)
#define ALIAS_NATIVE(className, funcName, oldClassName, oldFuncName) \
	ScriptManager::DefineNative(klass, #funcName, oldClassName##_##oldFuncName)

#define INIT_NAMESPACE(nsName) \
	ObjNamespace* ns_##nsName = NewNamespace(#nsName); \
	ScriptManager::Constants->Put(ns_##nsName->Hash, OBJECT_VAL(ns_##nsName)); \
	ScriptManager::AllNamespaces.push_back(ns_##nsName)
#define INIT_NAMESPACED_CLASS(nsName, className) \
	klass = NewClass(#className); \
	ns_##nsName->Fields->Put(klass->Hash, OBJECT_VAL(klass))
#define DEF_NAMESPACED_NATIVE(className, funcName) \
	ScriptManager::DefineNative(klass, #funcName, className##_##funcName)

	/***
    * \namespace RSDK
    * \desc RSDK (Retro Engine) related classes and functions.
    */
	INIT_NAMESPACE(RSDK);
	/***
    * \namespace API
    * \desc Interfaces to external services.
    */
	INIT_NAMESPACE(API);

	// #region Animator
	/***
    * \class Animator
    * \desc A sprite animation player.
    */
	INIT_CLASS(Animator);
	DEF_NATIVE(Animator, Create);
	DEF_NATIVE(Animator, Remove);
	DEF_NATIVE(Animator, SetAnimation);
	DEF_NATIVE(Animator, Animate);
	DEF_NATIVE(Animator, GetSprite);
	DEF_NATIVE(Animator, GetCurrentAnimation);
	DEF_NATIVE(Animator, GetCurrentFrame);
	DEF_NATIVE(Animator, GetHitbox);
	DEF_NATIVE(Animator, GetPrevAnimation);
	DEF_NATIVE(Animator, GetAnimationSpeed);
	DEF_NATIVE(Animator, GetAnimationTimer);
	DEF_NATIVE(Animator, GetDuration);
	DEF_NATIVE(Animator, GetFrameCount);
	DEF_NATIVE(Animator, GetLoopIndex);
	DEF_NATIVE(Animator, SetSprite);
	DEF_NATIVE(Animator, SetCurrentAnimation);
	DEF_NATIVE(Animator, SetCurrentFrame);
	DEF_NATIVE(Animator, SetPrevAnimation);
	DEF_NATIVE(Animator, SetAnimationSpeed);
	DEF_NATIVE(Animator, SetAnimationTimer);
	DEF_NATIVE(Animator, SetDuration);
	DEF_NATIVE(Animator, SetFrameCount);
	DEF_NATIVE(Animator, SetLoopIndex);
	DEF_NATIVE(Animator, SetRotationStyle);
	DEF_NATIVE(Animator, AdjustCurrentAnimation);
	DEF_NATIVE(Animator, AdjustCurrentFrame);
	DEF_NATIVE(Animator, AdjustAnimationSpeed);
	DEF_NATIVE(Animator, AdjustAnimationTimer);
	DEF_NATIVE(Animator, AdjustDuration);
	DEF_NATIVE(Animator, AdjustFrameCount);
	DEF_NATIVE(Animator, AdjustLoopIndex);
	// #endregion

	// #region API.Discord
	/***
    * \class API.Discord
    * \desc Discord integration functions.<br/>\
They require the Game SDK library to be present.
    */
	INIT_NAMESPACED_CLASS(API, Discord);
	DEF_NAMESPACED_NATIVE(Discord, Init);
	DEF_NAMESPACED_NATIVE(Discord, UpdateRichPresence);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityDetails);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityState);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityLargeImage);
	DEF_NAMESPACED_NATIVE(Discord, SetActivitySmallImage);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityElapsedTimer);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityRemainingTimer);
	DEF_NAMESPACED_NATIVE(Discord, SetActivityPartySize);
	DEF_NAMESPACED_NATIVE(Discord, UpdateActivity);
	DEF_NAMESPACED_NATIVE(Discord, GetCurrentUsername);
	DEF_NAMESPACED_NATIVE(Discord, GetCurrentUserID);
	DEF_NAMESPACED_NATIVE(Discord, GetCurrentUserAvatar);

	/***
    * \enum OK
    * \desc Request was successful.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("OK", DISCORDRESULT_OK);
	/***
    * \enum ERROR
    * \desc An error happened.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("ERROR", DISCORDRESULT_ERROR);
	/***
    * \enum SERVICE_UNAVAILABLE
    * \desc Service isn't available.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("SERVICE_UNAVAILABLE", DISCORDRESULT_SERVICEUNAVAILABLE);
	/***
    * \enum INVALID_VERSION
    * \desc SDK version mismatch.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("INVALID_VERSION", DISCORDRESULT_INVALIDVERSION);
	/***
    * \enum INVALID_PAYLOAD
    * \desc The service didn't accept the data that it received.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("INVALID_PAYLOAD", DISCORDRESULT_INVALIDPAYLOAD);
	/***
    * \enum INVALID_PERMISSIONS
    * \desc Not authorized for this operation.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("INVALID_PERMISSIONS", DISCORDRESULT_INVALIDPERMISSIONS);
	/***
    * \enum NOT_FOUND
    * \desc The service couldn't find what was requested.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("NOT_FOUND", DISCORDRESULT_NOTFOUND);
	/***
    * \enum NOT_AUTHENTICATED
    * \desc User isn't authenticated.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("NOT_AUTHENTICATED", DISCORDRESULT_NOTAUTHENTICATED);
	/***
    * \enum NOT_INSTALLED
    * \desc Discord isn't installed.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("NOT_INSTALLED", DISCORDRESULT_NOTINSTALLED);
	/***
    * \enum NOT_RUNNING
    * \desc Discord isn't running.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("NOT_RUNNING", DISCORDRESULT_NOTRUNNING);
	/***
    * \enum RATE_LIMITED
    * \desc This request is being sent too quickly.
    * \ns API.Discord
    */
	DEF_ENUM_IN_CLASS("RATE_LIMITED", DISCORDRESULT_RATELIMITED);
	// #endregion

	// #region Application
	/***
    * \class Application
    * \desc Functions for manipulating and reading runtime information.
    */
	INIT_CLASS(Application);
	DEF_NATIVE(Application, GetCommandLineArguments);
	DEF_NATIVE(Application, GetEngineVersionString);
	DEF_NATIVE(Application, GetEngineVersionMajor);
	DEF_NATIVE(Application, GetEngineVersionMinor);
	DEF_NATIVE(Application, GetEngineVersionPatch);
	DEF_NATIVE(Application, GetEngineVersionPrerelease);
	DEF_NATIVE(Application, GetEngineVersionCodename);
	DEF_NATIVE(Application, GetTargetFrameRate);
	DEF_NATIVE(Application, SetTargetFrameRate);
	DEF_NATIVE(Application, UseFixedTimestep);
	DEF_NATIVE(Application, GetFPS);
	DEF_NATIVE(Application, ShowFPSCounter);
	DEF_NATIVE(Application, GetKeyBind);
	DEF_NATIVE(Application, SetKeyBind);
	DEF_NATIVE(Application, GetGameTitle);
	DEF_NATIVE(Application, GetGameTitleShort);
	DEF_NATIVE(Application, GetGameVersion);
	DEF_NATIVE(Application, GetGameDescription);
	DEF_NATIVE(Application, SetGameTitle);
	DEF_NATIVE(Application, SetGameTitleShort);
	DEF_NATIVE(Application, SetGameVersion);
	DEF_NATIVE(Application, SetGameDescription);
	DEF_NATIVE(Application, SetCursorVisible);
	DEF_NATIVE(Application, GetCursorVisible);
	DEF_NATIVE(Application, Error);
	DEF_NATIVE(Application, SetDefaultFont);
	DEF_NATIVE(Application, ChangeGame);
	DEF_NATIVE(Application, Quit);
	/***
    * \enum KeyBind_Fullscreen
    * \desc Fullscreen keybind.
    */
	DEF_ENUM_CLASS(KeyBind, Fullscreen);
	/***
    * \enum KeyBind_ToggleFPSCounter
    * \desc FPS counter toggle keybind.
    */
	DEF_ENUM_CLASS(KeyBind, ToggleFPSCounter);
	/***
    * \enum KeyBind_DevRestartApp
    * \desc App restart keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevRestartApp);
	/***
    * \enum KeyBind_DevRestartScene
    * \desc Scene restart keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevRestartScene);
	/***
    * \enum KeyBind_DevRecompile
    * \desc Script recompile keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevRecompile);
	/***
    * \enum KeyBind_DevPerfSnapshot
    * \desc Performance snapshot keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevPerfSnapshot);
	/***
    * \enum KeyBind_DevLayerInfo
    * \desc Scene layer info keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevLayerInfo);
	/***
    * \enum KeyBind_DevFastForward
    * \desc Fast forward keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevFastForward);
	/***
    * \enum KeyBind_DevFrameStepper
    * \desc Frame stepper keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevFrameStepper);
	/***
    * \enum KeyBind_DevStepFrame
    * \desc Step frame keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevStepFrame);
	/***
    * \enum KeyBind_DevTileCol
    * \desc Tile collision display keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevTileCol);
	/***
    * \enum KeyBind_DevObjectRegions
    * \desc Object regions display keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevObjectRegions);
	/***
	* \enum KeyBind_DevViewHitboxes
	* \desc Hitbox display keybind. (dev)
	*/
	DEF_ENUM_CLASS(KeyBind, DevViewHitboxes);
	/***
	* \enum KeyBind_DevMenuToggle
	* \desc Open/close dev menu keybind. (dev)
	*/
	DEF_ENUM_CLASS(KeyBind, DevMenuToggle);
	/***
    * \enum KeyBind_DevQuit
    * \desc App quit keybind. (dev)
    */
	DEF_ENUM_CLASS(KeyBind, DevQuit);
	// #endregion

	// #region Array
	/***
    * \class Array
    * \desc Array manipulation.
    */
	INIT_CLASS(Array);
	DEF_NATIVE(Array, Create);
	DEF_NATIVE(Array, Length);
	DEF_NATIVE(Array, Push);
	DEF_NATIVE(Array, Pop);
	DEF_NATIVE(Array, Insert);
	DEF_NATIVE(Array, Erase);
	DEF_NATIVE(Array, Clear);
	DEF_NATIVE(Array, Shift);
	DEF_NATIVE(Array, SetAll);
	DEF_NATIVE(Array, Reverse);
	DEF_NATIVE(Array, Sort);
	// #endregion

	// #region Audio
	/***
    * \class Audio
    * \desc Functions for manipulating the application's audio.
    */
	INIT_CLASS(Audio);
	DEF_NATIVE(Audio, GetMasterVolume);
	DEF_NATIVE(Audio, GetMusicVolume);
	DEF_NATIVE(Audio, GetSoundVolume);
	DEF_NATIVE(Audio, SetMasterVolume);
	DEF_NATIVE(Audio, SetMusicVolume);
	DEF_NATIVE(Audio, SetSoundVolume);
	// #endregion

	// #region Collision
	/***
    * \class Collision
    * \desc Entity collision functions.
    */
	INIT_CLASS(Collision);
	DEF_NATIVE(Collision, ProcessEntityMovement);
	DEF_NATIVE(Collision, CheckTileCollision);
	DEF_NATIVE(Collision, CheckTileGrip);
	DEF_NATIVE(Collision, CheckEntityTouch);
	DEF_NATIVE(Collision, CheckEntityCircle);
	DEF_NATIVE(Collision, CheckEntityBox);
	DEF_NATIVE(Collision, CheckEntityPlatform);
	// #endregion

	// #region Controller
	/***
    * \class Controller
    * \desc Functions for getting information about controllers, as well as functions to control rumble.
    */
	INIT_CLASS(Controller);
	DEF_NATIVE(Controller, GetCount);
	DEF_NATIVE(Controller, IsConnected);
	DEF_NATIVE(Controller, IsXbox);
	DEF_NATIVE(Controller, IsPlayStation);
	DEF_NATIVE(Controller, IsJoyCon);
	DEF_NATIVE(Controller, HasShareButton);
	DEF_NATIVE(Controller, HasMicrophoneButton);
	DEF_NATIVE(Controller, HasPaddles);
	DEF_NATIVE(Controller, IsButtonHeld);
	DEF_NATIVE(Controller, IsButtonPressed);
	DEF_NATIVE(Controller, GetButton); // deprecated
	DEF_NATIVE(Controller, GetAxis);
	DEF_NATIVE(Controller, GetType);
	DEF_NATIVE(Controller, GetName);
	DEF_NATIVE(Controller, SetPlayerIndex);
	DEF_NATIVE(Controller, HasRumble);
	DEF_NATIVE(Controller, IsRumbleActive);
	DEF_NATIVE(Controller, IsRumblePaused);
	DEF_NATIVE(Controller, Rumble);
	DEF_NATIVE(Controller, StopRumble);
	DEF_NATIVE(Controller, SetRumblePaused);
	DEF_NATIVE(Controller, SetLargeMotorFrequency); // deprecated
	DEF_NATIVE(Controller, SetSmallMotorFrequency); // deprecated
	/***
    * \constant NUM_CONTROLLER_BUTTONS
    * \type integer
    * \desc The amount of buttons in a controller.
    */
	DEF_CONST_INT("NUM_CONTROLLER_BUTTONS", (int)ControllerButton::Max);
	/***
    * \constant NUM_CONTROLLER_AXES
    * \type integer
    * \desc The amount of axes in a controller.
    */
	DEF_CONST_INT("NUM_CONTROLLER_AXES", (int)ControllerAxis::Max);
#define CONST_BUTTON(x, y) DEF_CONST_INT("Button_" #x, (int)ControllerButton::y)
	{
		/***
        * \enum Button_A
        * \desc Bottom controller face button.
        */
		CONST_BUTTON(A, A);
		/***
        * \enum Button_B
        * \desc Right controller face button.
        */
		CONST_BUTTON(B, B);
		/***
        * \enum Button_X
        * \desc Left controller face button.
        */
		CONST_BUTTON(X, X);
		/***
        * \enum Button_Y
        * \desc Top controller face button.
        */
		CONST_BUTTON(Y, Y);
		/***
        * \enum Button_BACK
        * \desc Controller Back button.
        */
		CONST_BUTTON(BACK, Back);
		/***
        * \enum Button_GUIDE
        * \desc Controller Guide button.
        */
		CONST_BUTTON(GUIDE, Guide);
		/***
        * \enum Button_START
        * \desc Controller Start button.
        */
		CONST_BUTTON(START, Start);
		/***
        * \enum Button_LEFTSTICK
        * \desc Controller left stick click.
        */
		CONST_BUTTON(LEFTSTICK, LeftStick);
		/***
        * \enum Button_RIGHTSTICK
        * \desc Controller right stick click.
        */
		CONST_BUTTON(RIGHTSTICK, RightStick);
		/***
        * \enum Button_LEFTSHOULDER
        * \desc Controller left shoulder.
        */
		CONST_BUTTON(LEFTSHOULDER, LeftShoulder);
		/***
        * \enum Button_RIGHTSHOULDER
        * \desc Controller right shoulder.
        */
		CONST_BUTTON(RIGHTSHOULDER, RightShoulder);
		/***
        * \enum Button_DPAD_UP
        * \desc Controller D-Pad Up.
        */
		CONST_BUTTON(DPAD_UP, DPadUp);
		/***
        * \enum Button_DPAD_DOWN
        * \desc Controller D-Pad Down.
        */
		CONST_BUTTON(DPAD_DOWN, DPadDown);
		/***
        * \enum Button_DPAD_LEFT
        * \desc Controller D-Pad Left.
        */
		CONST_BUTTON(DPAD_LEFT, DPadLeft);
		/***
        * \enum Button_DPAD_RIGHT
        * \desc Controller D-Pad Right.
        */
		CONST_BUTTON(DPAD_RIGHT, DPadRight);
		/***
        * \enum Button_SHARE
        * \desc Share/Capture controller button.
        */
		CONST_BUTTON(SHARE, Share);
		/***
        * \enum Button_MICROPHONE
        * \desc Microphone controller button.
        */
		CONST_BUTTON(MICROPHONE, Microphone);
		/***
        * \enum Button_TOUCHPAD
        * \desc Touchpad controller button.
        */
		CONST_BUTTON(TOUCHPAD, Touchpad);
		/***
        * \enum Button_PADDLE1
        * \desc P1 Paddle (Xbox Elite controllers.).
        */
		CONST_BUTTON(PADDLE1, Paddle1);
		/***
        * \enum Button_PADDLE2
        * \desc P2 Paddle (Xbox Elite controllers.).
        */
		CONST_BUTTON(PADDLE2, Paddle2);
		/***
        * \enum Button_PADDLE3
        * \desc P3 Paddle (Xbox Elite controllers.).
        */
		CONST_BUTTON(PADDLE3, Paddle3);
		/***
        * \enum Button_PADDLE4
        * \desc P4 Paddle (Xbox Elite controllers.).
        */
		CONST_BUTTON(PADDLE4, Paddle4);
		/***
        * \enum Button_MISC1
        * \desc Controller button for miscellaneous purposes.
        */
		CONST_BUTTON(MISC1, Misc1);
	}
#undef CONST_BUTTON
#define CONST_AXIS(x, y) DEF_CONST_INT("Axis_" #x, (int)ControllerAxis::y)
	{
		/***
        * \enum Axis_LEFTX
        * \desc Left controller stick X.
        */
		CONST_AXIS(LEFTX, LeftX);
		/***
        * \enum Axis_LEFTY
        * \desc Left controller stick Y.
        */
		CONST_AXIS(LEFTY, LeftY);
		/***
        * \enum Axis_RIGHTX
        * \desc Right controller stick X.
        */
		CONST_AXIS(RIGHTX, RightX);
		/***
        * \enum Axis_RIGHTY
        * \desc Right controller stick Y.
        */
		CONST_AXIS(RIGHTY, RightY);
		/***
        * \enum Axis_TRIGGERLEFT
        * \desc Left controller trigger.
        */
		CONST_AXIS(TRIGGERLEFT, TriggerLeft);
		/***
        * \enum Axis_TRIGGERRIGHT
        * \desc Right controller trigger.
        */
		CONST_AXIS(TRIGGERRIGHT, TriggerRight);
	}
#undef CONST_AXIS
#define CONST_CONTROLLER(type) DEF_CONST_INT("Axis_" #type, (int)ControllerType::type)
	{
		/***
        * \enum Controller_Xbox360
        * \desc Xbox 360 controller type.
        */
		CONST_CONTROLLER(Xbox360);
		/***
        * \enum Controller_XboxOne
        * \desc Xbox One controller type.
        */
		CONST_CONTROLLER(XboxOne);
		/***
        * \enum Controller_XboxSeriesXS
        * \desc Xbox Series XS controller type.
        */
		CONST_CONTROLLER(XboxSeriesXS);
		/***
        * \enum Controller_XboxElite
        * \desc Xbox Elite controller type.
        */
		CONST_CONTROLLER(XboxElite);
		/***
        * \enum Controller_PS3
        * \desc PlayStation 3 controller type.
        */
		CONST_CONTROLLER(PS3);
		/***
        * \enum Controller_PS4
        * \desc PlayStation 4 controller type.
        */
		CONST_CONTROLLER(PS4);
		/***
        * \enum Controller_PS5
        * \desc PlayStation 5 controller type.
        */
		CONST_CONTROLLER(PS5);
		/***
        * \enum Controller_SwitchJoyConPair
        * \desc Nintendo Switch Joy-Con pair controller type.
        */
		CONST_CONTROLLER(SwitchJoyConPair);
		/***
        * \enum Controller_SwitchJoyConLeft
        * \desc Nintendo Switch Joy-Con L controller type.
        */
		CONST_CONTROLLER(SwitchJoyConLeft);
		/***
        * \enum Controller_SwitchJoyConRight
        * \desc Nintendo Switch Joy-Con R controller type.
        */
		CONST_CONTROLLER(SwitchJoyConRight);
		/***
        * \enum Controller_SwitchPro
        * \desc Nintendo Switch Pro Controller controller type.
        */
		CONST_CONTROLLER(SwitchPro);
		/***
        * \enum Controller_Stadia
        * \desc Stadia Controller controller type.
        */
		CONST_CONTROLLER(Stadia);
		/***
        * \enum Controller_AmazonLuna
        * \desc Amazon Luna controller type.
        */
		CONST_CONTROLLER(AmazonLuna);
		/***
        * \enum Controller_NvidiaShield
        * \desc Nvidia Shield TV controller type.
        */
		CONST_CONTROLLER(NvidiaShield);
		/***
        * \enum Controller_Unknown
        * \desc Unknown or unrecognized controller type.
        */
		CONST_CONTROLLER(Unknown);
	}
#undef CONST_CONTROLLER
	// #endregion

	// #region Date
	/***
    * \class Date
    * \desc Date and time functions.
    */
	INIT_CLASS(Date);
	DEF_NATIVE(Date, GetEpoch);
	DEF_NATIVE(Date, GetWeekday);
	DEF_NATIVE(Date, GetSecond);
	DEF_NATIVE(Date, GetMinute);
	DEF_NATIVE(Date, GetHour);
	DEF_NATIVE(Date, GetTimeOfDay);
	DEF_NATIVE(Date, GetTicks);

	// #region Weekdays
	/***
	* \enum WEEKDAY_SUNDAY
	* \desc The first day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_SUNDAY", (int)Weekday::SUNDAY);
	/***
	* \enum WEEKDAY_MONDAY
	* \desc The second day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_MONDAY", (int)Weekday::MONDAY);
	/***
	* \enum WEEKDAY_TUESDAY
	* \desc The third day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_TUESDAY", (int)Weekday::TUESDAY);
	/***
	* \enum WEEKDAY_WEDNESDAY
	* \desc The fourth day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_WEDNESDAY", (int)Weekday::WEDNESDAY);
	/***
	* \enum WEEKDAY_THURSDAY
	* \desc The fifth day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_THURSDAY", (int)Weekday::THURSDAY);
	/***
	* \enum WEEKDAY_FRIDAY
	* \desc The sixth day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_FRIDAY", (int)Weekday::FRIDAY);
	/***
	* \enum WEEKDAY_SATURDAY
	* \desc The seventh day of the week.
	*/
	DEF_CONST_INT("WEEKDAY_SATURDAY", (int)Weekday::SATURDAY);
	// #endregion

	// #region TimesOfDay
	/***
	* \enum TIMEOFDAY_MORNING
	* \desc The early hours of the day (5AM to 11AM, or 05:00 to 11:00).
	*/
	DEF_CONST_INT("TIMEOFDAY_MORNING", (int)TimeOfDay::MORNING);
	/***
	* \enum TIMEOFDAY_MIDDAY
	* \desc The middle hours of the day (12PM to 4PM, or 12:00 to 16:00).
	*/
	DEF_CONST_INT("TIMEOFDAY_MIDDAY", (int)TimeOfDay::MIDDAY);
	/***
	* \enum TIMEOFDAY_EVENING
	* \desc The later hours of the day (5PM to 8PM, or 17:00 to 20:00).
	*/
	DEF_CONST_INT("TIMEOFDAY_EVENING", (int)TimeOfDay::EVENING);
	/***
	* \enum TIMEOFDAY_NIGHT
	* \desc The very late and very early hours of the day (9PM to 4AM, or 21:00 to 4:00).
	*/
	DEF_CONST_INT("TIMEOFDAY_NIGHT", (int)TimeOfDay::NIGHT);
	// #endregion
	// #endregion

	// #region Device
	/***
    * \class Device
    * \desc Functions to retrieve information about the device the application is running on.
    */
	INIT_CLASS(Device);
	DEF_NATIVE(Device, GetPlatform);
	DEF_NATIVE(Device, IsPC);
	DEF_NATIVE(Device, IsMobile);
	DEF_NATIVE(Device, GetCapability);
	/***
    * \enum Platform_Windows
    * \desc Windows platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Windows);
	/***
    * \enum Platform_MacOS
    * \desc MacOS platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, MacOS);
	/***
    * \enum Platform_Linux
    * \desc Linux platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Linux);
	/***
    * \enum Platform_Switch
    * \desc Nintendo Switch platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Switch);
	/***
    * \enum Platform_PlayStation
    * \desc PlayStation platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, PlayStation);
	/***
    * \enum Platform_Xbox
    * \desc Xbox platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Xbox);
	/***
    * \enum Platform_Android
    * \desc Android platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Android);
	/***
   * \enum Platform_iOS
   * \desc iOS platform.
   */
	DEF_ENUM_NAMED("Platform", Platforms, iOS);
	/***
    * \enum Platform_Unknown
    * \desc Unknown platform.
    */
	DEF_ENUM_NAMED("Platform", Platforms, Unknown);
	// #endregion

	// #region Directory
	/***
    * \class Directory
    * \desc Filesystem directory manipulation.
    */
	INIT_CLASS(Directory);
	DEF_NATIVE(Directory, Create);
	DEF_NATIVE(Directory, Exists);
	DEF_NATIVE(Directory, GetFiles);
	DEF_NATIVE(Directory, GetDirectories);
	// #endregion

	// #region Display
	/***
    * \class Display
    * \desc Functions to retrieve information about the displays in the device.
    */
	INIT_CLASS(Display);
	DEF_NATIVE(Display, GetWidth);
	DEF_NATIVE(Display, GetHeight);
	// #endregion

	// #region Draw
	/***
    * \class Draw
    * \desc General drawing functions.<br/>\
Draw provides functions to draw images, sprites, and textures; as well as primitives, such as lines, rectangles, ellipses and polygons. You may also apply basic transformations like translation, scaling, and rotation.
    */
	INIT_CLASS(Draw);
	DEF_NATIVE(Draw, Sprite);
	DEF_NATIVE(Draw, SpriteBasic);
	DEF_NATIVE(Draw, Animator);
	DEF_NATIVE(Draw, AnimatorBasic);
	DEF_NATIVE(Draw, SpritePart);
	DEF_NATIVE(Draw, Image);
	DEF_NATIVE(Draw, ImagePart);
	DEF_NATIVE(Draw, ImageSized);
	DEF_NATIVE(Draw, ImagePartSized);
	DEF_NATIVE(Draw, Layer);
	DEF_NATIVE(Draw, View);
	DEF_NATIVE(Draw, ViewPart);
	DEF_NATIVE(Draw, ViewSized);
	DEF_NATIVE(Draw, ViewPartSized);
	DEF_NATIVE(Draw, Video);
	DEF_NATIVE(Draw, VideoPart);
	DEF_NATIVE(Draw, VideoSized);
	DEF_NATIVE(Draw, VideoPartSized);
	DEF_NATIVE(Draw, Tile);
	DEF_NATIVE(Draw, Texture);
	DEF_NATIVE(Draw, TextureSized);
	DEF_NATIVE(Draw, TexturePart);
	DEF_NATIVE(Draw, SetTextAlign);
	DEF_NATIVE(Draw, SetTextBaseline);
	DEF_NATIVE(Draw, SetTextAdvance);
	DEF_NATIVE(Draw, SetTextLineAscent);
	DEF_NATIVE(Draw, MeasureText);
	DEF_NATIVE(Draw, MeasureTextWrapped);
	DEF_NATIVE(Draw, Text);
	DEF_NATIVE(Draw, TextWrapped);
	DEF_NATIVE(Draw, TextEllipsis);
	DEF_NATIVE(Draw, Glyph);
	DEF_NATIVE(Draw, TextArray);
	DEF_NATIVE(Draw, SetBlendColor);
	DEF_NATIVE(Draw, SetTextureBlend);
	DEF_NATIVE(Draw, SetBlendMode);
	DEF_NATIVE(Draw, SetBlendFactor);
	DEF_NATIVE(Draw, SetBlendFactorExtended);
	DEF_NATIVE(Draw, SetCompareColor);
	DEF_NATIVE(Draw, SetTintColor);
	DEF_NATIVE(Draw, SetTintMode);
	DEF_NATIVE(Draw, UseTinting);
	DEF_NATIVE(Draw, SetShader);
	DEF_NATIVE(Draw, SetFilter);
	DEF_NATIVE(Draw, UseStencil);
	DEF_NATIVE(Draw, SetStencilTestFunction);
	DEF_NATIVE(Draw, SetStencilPassOperation);
	DEF_NATIVE(Draw, SetStencilFailOperation);
	DEF_NATIVE(Draw, SetStencilValue);
	DEF_NATIVE(Draw, SetStencilMask);
	DEF_NATIVE(Draw, ClearStencil);
	DEF_NATIVE(Draw, SetDotMask);
	DEF_NATIVE(Draw, SetHorizontalDotMask);
	DEF_NATIVE(Draw, SetVerticalDotMask);
	DEF_NATIVE(Draw, SetHorizontalDotMaskOffset);
	DEF_NATIVE(Draw, SetVerticalDotMaskOffset);
	DEF_NATIVE(Draw, Line);
	DEF_NATIVE(Draw, Circle);
	DEF_NATIVE(Draw, Ellipse);
	DEF_NATIVE(Draw, Triangle);
	DEF_NATIVE(Draw, TriangleBlend);
	DEF_NATIVE(Draw, Quad);
	DEF_NATIVE(Draw, QuadBlend);
	DEF_NATIVE(Draw, TriangleTextured);
	DEF_NATIVE(Draw, QuadTextured);
	DEF_NATIVE(Draw, Rectangle);
	DEF_NATIVE(Draw, CircleStroke);
	DEF_NATIVE(Draw, EllipseStroke);
	DEF_NATIVE(Draw, TriangleStroke);
	DEF_NATIVE(Draw, RectangleStroke);
	DEF_NATIVE(Draw, UseFillSmoothing);
	DEF_NATIVE(Draw, UseStrokeSmoothing);
	DEF_NATIVE(Draw, SetClip);
	DEF_NATIVE(Draw, ClearClip);
	DEF_NATIVE(Draw, GetClipX);
	DEF_NATIVE(Draw, GetClipY);
	DEF_NATIVE(Draw, GetClipWidth);
	DEF_NATIVE(Draw, GetClipHeight);
	DEF_NATIVE(Draw, Save);
	DEF_NATIVE(Draw, Scale);
	DEF_NATIVE(Draw, Rotate);
	DEF_NATIVE(Draw, Restore);
	DEF_NATIVE(Draw, Translate);
	DEF_NATIVE(Draw, SetTextureTarget);
	DEF_NATIVE(Draw, Clear);
	DEF_NATIVE(Draw, ResetTextureTarget);
	DEF_NATIVE(Draw, UseSpriteDeform);
	DEF_NATIVE(Draw, SetSpriteDeformLine);
	DEF_NATIVE(Draw, UseDepthTesting);
	DEF_NATIVE(Draw, GetCurrentDrawGroup);
	DEF_NATIVE(Draw, CopyScreen);

	/***
    * \enum DrawMode_LINES
    * \desc Draws the faces with lines, using a solid color determined by the face's existing colors (and if not, the blend color.)
    */
	DEF_ENUM(DrawMode_LINES);
	/***
    * \enum DrawMode_POLYGONS
    * \desc Draws the faces with polygons, using a solid color determined by the face's existing colors (and if not, the blend color.)
    */
	DEF_ENUM(DrawMode_POLYGONS);
	/***
    * \enum DrawMode_POINTS
    * \desc (hardware-renderer only) Draws the faces with points, using a solid color determined by the face's existing colors (and if not, the blend color.)
    */
	DEF_ENUM(DrawMode_POINTS);
	/***
    * \enum DrawMode_FLAT_LIGHTING
    * \desc Enables lighting, using a color for the primitive calculated with the vertex normals, and the primitive's existing colors (and if not, the blend color.)
    */
	DEF_ENUM(DrawMode_FLAT_LIGHTING);
	/***
    * \enum DrawMode_SMOOTH_LIGHTING
    * \desc Enables lighting, using a color smoothly spread across the primitive calculated with the vertex normals, and the primitive's existing colors (and if not, the blend color.)
    */
	DEF_ENUM(DrawMode_SMOOTH_LIGHTING);
	/***
    * \enum DrawMode_TEXTURED
    * \desc Enables texturing.
    */
	DEF_ENUM(DrawMode_TEXTURED);
	/***
    * \enum DrawMode_AFFINE
    * \desc (software-renderer only) Uses affine texture mapping.
    */
	DEF_ENUM(DrawMode_AFFINE);
	/***
    * \enum DrawMode_DEPTH_TEST
    * \desc Enables depth testing.
    */
	DEF_ENUM(DrawMode_DEPTH_TEST);
	/***
    * \enum DrawMode_FOG
    * \desc Enables fog.
    */
	DEF_ENUM(DrawMode_FOG);
	/***
    * \enum DrawMode_ORTHOGRAPHIC
    * \desc (software-renderer only) Uses orthographic perspective projection.
    */
	DEF_ENUM(DrawMode_ORTHOGRAPHIC);
	/***
    * \enum DrawMode_LINES_FLAT
    * \desc Combination of <ref DrawMode_LINES> and <ref DrawMode_FLAT_LIGHTING>.
    */
	DEF_ENUM(DrawMode_LINES_FLAT);
	/***
    * \enum DrawMode_LINES_SMOOTH
    * \desc Combination of <ref DrawMode_LINES> and <ref DrawMode_SMOOTH_LIGHTING>.
    */
	DEF_ENUM(DrawMode_LINES_SMOOTH);
	/***
    * \enum DrawMode_POLYGONS_FLAT
    * \desc Combination of <ref DrawMode_POLYGONS> and <ref DrawMode_FLAT_LIGHTING>.
    */
	DEF_ENUM(DrawMode_POLYGONS_FLAT);
	/***
    * \enum DrawMode_POLYGONS_SMOOTH
    * \desc Combination of <ref DrawMode_POLYGONS> and <ref DrawMode_SMOOTH_LIGHTING>.
    */
	DEF_ENUM(DrawMode_POLYGONS_SMOOTH);
	/***
    * \enum DrawMode_PrimitiveMask
    * \desc Masks out <ref DrawMode_LINES> | <ref DrawMode_POLYGONS> | <ref DrawMode_POINTS> out of a draw mode.
    */
	DEF_ENUM(DrawMode_PrimitiveMask);
	/***
    * \enum DrawMode_LightingMask
    * \desc Masks out <ref DrawMode_FLAT_LIGHTING> | <ref DrawMode_SMOOTH_LIGHTING> out of a draw mode.
    */
	DEF_ENUM(DrawMode_LightingMask);
	/***
    * \enum DrawMode_FillTypeMask
    * \desc Masks out <ref DrawMode_PrimitiveMask> | <ref DrawMode_LightingMask> out of a draw mode.
    */
	DEF_ENUM(DrawMode_FillTypeMask);
	/***
    * \enum DrawMode_FlagsMask
    * \desc Masks out ~<ref DrawMode_FillTypeMask> out of a draw mode.
    */
	DEF_ENUM(DrawMode_FlagsMask);

	/***
    * \enum BlendMode_NORMAL
    * \desc Normal pixel blending.
    */
	DEF_ENUM(BlendMode_NORMAL);
	/***
    * \enum BlendMode_ADD
    * \desc Additive pixel blending.
    */
	DEF_ENUM(BlendMode_ADD);
	/***
    * \enum BlendMode_SUBTRACT
    * \desc Subtractive pixel blending.
    */
	DEF_ENUM(BlendMode_SUBTRACT);
	/***
    * \enum BlendMode_MAX
    * \desc (hardware-renderer only) Maximum pixel blending.
    */
	DEF_ENUM(BlendMode_MAX);
	/***
    * \enum BlendMode_MATCH_EQUAL
    * \desc (software-renderer only) Draw pixels only where it matches the Comparison Color.
    */
	DEF_ENUM(BlendMode_MATCH_EQUAL);
	/***
    * \enum BlendMode_MATCH_NOT_EQUAL
    * \desc (software-renderer only) Draw pixels only where it does not match the Comparison Color.
    */
	DEF_ENUM(BlendMode_MATCH_NOT_EQUAL);

	/***
    * \enum TintMode_SRC_NORMAL
    * \desc Tints the source pixel with the tint color.
    */
	DEF_ENUM(TintMode_SRC_NORMAL);
	/***
    * \enum TintMode_DST_NORMAL
    * \desc Tints the destination pixel with the tint color.
    */
	DEF_ENUM(TintMode_DST_NORMAL);
	/***
    * \enum TintMode_SRC_BLEND
    * \desc Blends the source pixel with the tint color.
    */
	DEF_ENUM(TintMode_SRC_BLEND);
	/***
    * \enum TintMode_DST_BLEND
    * \desc Blends the destination pixel with the tint color.
    */
	DEF_ENUM(TintMode_DST_BLEND);

	/***
    * \enum Filter_NONE
    * \desc Disables the current filter.
    */
	DEF_ENUM(Filter_NONE);
	/***
    * \enum Filter_BLACK_AND_WHITE
    * \desc Black and white filter.
    */
	DEF_ENUM(Filter_BLACK_AND_WHITE);
	/***
    * \enum Filter_INVERT
    * \desc Invert filter.
    */
	DEF_ENUM(Filter_INVERT);

	/***
    * \enum StencilTest_Never
    * \desc Always fails.
    */
	DEF_ENUM(StencilTest_Never);
	/***
    * \enum StencilTest_Always
    * \desc Always passes.
    */
	DEF_ENUM(StencilTest_Always);
	/***
    * \enum StencilTest_Equal
    * \desc Does an "equals" operation.
    */
	DEF_ENUM(StencilTest_Equal);
	/***
    * \enum StencilTest_NotEqual
    * \desc Does a "not equal" operation.
    */
	DEF_ENUM(StencilTest_NotEqual);
	/***
    * \enum StencilTest_Less
    * \desc Does a "less than" operation.
    */
	DEF_ENUM(StencilTest_Less);
	/***
    * \enum StencilTest_Greater
    * \desc Does a "greater than" operation.
    */
	DEF_ENUM(StencilTest_Greater);
	/***
    * \enum StencilTest_LEqual
    * \desc Does a "less than or equal to" operation.
    */
	DEF_ENUM(StencilTest_LEqual);
	/***
    * \enum StencilTest_GEqual
    * \desc Does a "greater than or equal to" operation.
    */
	DEF_ENUM(StencilTest_GEqual);

	/***
    * \enum StencilOp_Keep
    * \desc Doesn't modify the stencil buffer value (keeps it the same.)
    */
	DEF_ENUM(StencilOp_Keep);
	/***
    * \enum StencilOp_Zero
    * \desc Sets the stencil buffer value to zero.
    */
	DEF_ENUM(StencilOp_Zero);
	/***
    * \enum StencilOp_Incr
    * \desc Increases the stencil buffer value, saturating it if it would wrap around (the value is set to a specific maximum.)
    */
	DEF_ENUM(StencilOp_Incr);
	/***
    * \enum StencilOp_Decr
    * \desc Increases the stencil buffer value, setting it to zero if it would wrap around.
    */
	DEF_ENUM(StencilOp_Decr);
	/***
    * \enum StencilOp_Invert
    * \desc Inverts the bits of the stencil buffer value.
    */
	DEF_ENUM(StencilOp_Invert);
	/***
    * \enum StencilOp_Replace
    * \desc Replaces the bits of the stencil buffer value with the masked value.
    */
	DEF_ENUM(StencilOp_Replace);
	/***
    * \enum StencilOp_IncrWrap
    * \desc Increases the stencil buffer value, letting it wrap around.
    */
	DEF_ENUM(StencilOp_IncrWrap);
	/***
    * \enum StencilOp_DecrWrap
    * \desc Increases the stencil buffer value, letting it wrap around.
    */
	DEF_ENUM(StencilOp_DecrWrap);

	/***
	* \enum ALIGN_LEFT
	* \desc Left alignment for text drawing.
	*/
	DEF_ENUM(ALIGN_LEFT);
	/***
	* \enum ALIGN_CENTER
	* \desc Center alignment for text drawing.
	*/
	DEF_ENUM(ALIGN_CENTER);
	/***
	* \enum ALIGN_RIGHT
	* \desc Right alignment for text drawing.
	*/
	DEF_ENUM(ALIGN_RIGHT);

	/***
    * \enum BlendFactor_ZERO
    * \desc Blend factor: (0, 0, 0, 0)
    */
	DEF_ENUM(BlendFactor_ZERO);
	/***
    * \enum BlendFactor_ONE
    * \desc Blend factor: (1, 1, 1, 1)
    */
	DEF_ENUM(BlendFactor_ONE);
	/***
    * \enum BlendFactor_SRC_COLOR
    * \desc Blend factor: (Rs, Gs, Bs, As)
    */
	DEF_ENUM(BlendFactor_SRC_COLOR);
	/***
    * \enum BlendFactor_INV_SRC_COLOR
    * \desc Blend factor: (1-Rs, 1-Gs, 1-Bs, 1-As)
    */
	DEF_ENUM(BlendFactor_INV_SRC_COLOR);
	/***
    * \enum BlendFactor_SRC_ALPHA
    * \desc Blend factor: (As, As, As, As)
    */
	DEF_ENUM(BlendFactor_SRC_ALPHA);
	/***
    * \enum BlendFactor_INV_SRC_ALPHA
    * \desc Blend factor: (1-As, 1-As, 1-As, 1-As)
    */
	DEF_ENUM(BlendFactor_INV_SRC_ALPHA);
	/***
    * \enum BlendFactor_DST_COLOR
    * \desc Blend factor: (Rd, Gd, Bd, Ad)
    */
	DEF_ENUM(BlendFactor_DST_COLOR);
	/***
    * \enum BlendFactor_INV_DST_COLOR
    * \desc Blend factor: (1-Rd, 1-Gd, 1-Bd, 1-Ad)
    */
	DEF_ENUM(BlendFactor_INV_DST_COLOR);
	/***
    * \enum BlendFactor_DST_ALPHA
    * \desc Blend factor: (Ad, Ad, Ad, Ad)
    */
	DEF_ENUM(BlendFactor_DST_ALPHA);
	/***
    * \enum BlendFactor_INV_DST_ALPHA
    * \desc Blend factor: (1-Ad, 1-Ad, 1-Ad, 1-Ad)
    */
	DEF_ENUM(BlendFactor_INV_DST_ALPHA);

	/***
	* \enum ROTSTYLE_NONE
	* \desc Does not rotate the sprite in <ref Draw.SpriteBasic>, <ref Draw.Animator>, and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_NONE);
	/***
	* \enum ROTSTYLE_FULL
	* \desc Allows the sprite 256 angles of rotation in <ref Draw.SpriteBasic>, <ref Draw.Animator>, and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_FULL);
	/***
	* \enum ROTSTYLE_45DEG
	* \desc Allows the sprite to rotate in intervals of 45 degrees in <ref Draw.SpriteBasic>, <ref Draw.Animator>, and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_45DEG);
	/***
	* \enum ROTSTYLE_90DEG
	* \desc Allows the sprite to rotate in intervals of 90 degrees in <ref Draw.SpriteBasic>, <ref Draw.Animator>, and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_90DEG);
	/***
	* \enum ROTSTYLE_180DEG
	* \desc Allows the sprite to rotate in intervals of 180 degrees in <ref Draw.SpriteBasic>, <ref Draw.Animator>, and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_180DEG);
	/***
	* \enum ROTSTYLE_STATICFRAMES
	* \desc Uses extra frames to rotate the sprite in <ref Draw.SpriteBasic> and <ref Draw.AnimatorBasic>.
	*/
	DEF_ENUM(ROTSTYLE_STATICFRAMES);
	// #endregion

	// #region Draw3D
	/***
    * \class Draw3D
    * \desc Functions for drawing in 3D.
    */
	INIT_CLASS(Draw3D);
	DEF_NATIVE(Draw3D, BindVertexBuffer);
	DEF_NATIVE(Draw3D, UnbindVertexBuffer);
	DEF_NATIVE(Draw3D, BindScene);
	DEF_NATIVE(Draw3D, Model);
	DEF_NATIVE(Draw3D, ModelSkinned);
	DEF_NATIVE(Draw3D, ModelSimple);
	DEF_NATIVE(Draw3D, Triangle);
	DEF_NATIVE(Draw3D, Quad);
	DEF_NATIVE(Draw3D, Sprite);
	DEF_NATIVE(Draw3D, SpritePart);
	DEF_NATIVE(Draw3D, Image);
	DEF_NATIVE(Draw3D, ImagePart);
	DEF_NATIVE(Draw3D, Tile);
	DEF_NATIVE(Draw3D, TriangleTextured);
	DEF_NATIVE(Draw3D, QuadTextured);
	DEF_NATIVE(Draw3D, SpritePoints);
	DEF_NATIVE(Draw3D, TilePoints);
	DEF_NATIVE(Draw3D, SceneLayer);
	DEF_NATIVE(Draw3D, SceneLayerPart);
	DEF_NATIVE(Draw3D, VertexBuffer);
	DEF_NATIVE(Draw3D, RenderScene);
	// #endregion

	// #region Ease
	/***
    * \class Ease
    * \desc Easing functions.<br/>\
See https://easings.net/ for more details.
    */
	INIT_CLASS(Ease);
	DEF_NATIVE(Ease, InSine);
	DEF_NATIVE(Ease, OutSine);
	DEF_NATIVE(Ease, InOutSine);
	DEF_NATIVE(Ease, InQuad);
	DEF_NATIVE(Ease, OutQuad);
	DEF_NATIVE(Ease, InOutQuad);
	DEF_NATIVE(Ease, InCubic);
	DEF_NATIVE(Ease, OutCubic);
	DEF_NATIVE(Ease, InOutCubic);
	DEF_NATIVE(Ease, InQuart);
	DEF_NATIVE(Ease, OutQuart);
	DEF_NATIVE(Ease, InOutQuart);
	DEF_NATIVE(Ease, InQuint);
	DEF_NATIVE(Ease, OutQuint);
	DEF_NATIVE(Ease, InOutQuint);
	DEF_NATIVE(Ease, InExpo);
	DEF_NATIVE(Ease, OutExpo);
	DEF_NATIVE(Ease, InOutExpo);
	DEF_NATIVE(Ease, InCirc);
	DEF_NATIVE(Ease, OutCirc);
	DEF_NATIVE(Ease, InOutCirc);
	DEF_NATIVE(Ease, InBack);
	DEF_NATIVE(Ease, OutBack);
	DEF_NATIVE(Ease, InOutBack);
	DEF_NATIVE(Ease, InElastic);
	DEF_NATIVE(Ease, OutElastic);
	DEF_NATIVE(Ease, InOutElastic);
	DEF_NATIVE(Ease, InBounce);
	DEF_NATIVE(Ease, OutBounce);
	DEF_NATIVE(Ease, InOutBounce);
	DEF_NATIVE(Ease, Triangle);
	// #endregion

	// #region File
	/***
    * \class File
    * \desc Filesystem file manipulation.
    */
	INIT_CLASS(File);
	DEF_NATIVE(File, Exists);
	DEF_NATIVE(File, ReadAllText);
	DEF_NATIVE(File, WriteAllText);
	// #endregion

	// #region Geometry
	/***
    * \class Geometry
    * \desc Geometry intersection functions.
    */
	INIT_CLASS(Geometry);
	DEF_NATIVE(Geometry, Triangulate);
	DEF_NATIVE(Geometry, Intersect);
	DEF_NATIVE(Geometry, IsPointInsidePolygon);
	DEF_NATIVE(Geometry, IsLineIntersectingPolygon);

	/***
    * \enum GeoBooleanOp_Intersection
    * \desc AND operation.
    */
	DEF_ENUM(GeoBooleanOp_Intersection);
	/***
    * \enum GeoBooleanOp_Union
    * \desc OR operation.
    */
	DEF_ENUM(GeoBooleanOp_Union);
	/***
    * \enum GeoBooleanOp_Difference
    * \desc NOT operation.
    */
	DEF_ENUM(GeoBooleanOp_Difference);
	/***
    * \enum GeoBooleanOp_ExclusiveOr
    * \desc XOR operation.
    */
	DEF_ENUM(GeoBooleanOp_ExclusiveOr);

	/***
    * \enum GeoFillRule_EvenOdd
    * \desc Only odd numbered subregions are filled.
    */
	DEF_ENUM(GeoFillRule_EvenOdd);
	/***
    * \enum GeoFillRule_NonZero
    * \desc Only non-zero subregions are filled.
    */
	DEF_ENUM(GeoFillRule_NonZero);
	/***
    * \enum GeoFillRule_Positive
    * \desc Only subregions that have winding counts greater than zero are filled.
    */
	DEF_ENUM(GeoFillRule_Positive);
	/***
    * \enum GeoFillRule_Negative
    * \desc Only subregions that have winding counts lesser than zero are filled.
    */
	DEF_ENUM(GeoFillRule_Negative);
	// #endregion

	// #region HTTP
	INIT_CLASS(HTTP);
	DEF_NATIVE(HTTP, GetString);
	DEF_NATIVE(HTTP, GetToFile);
	// #endregion

	// #region Image
	/***
    * \class Image
    * \desc Functions to retrieve information about images.
    */
	INIT_CLASS(Image);
	DEF_NATIVE(Image, GetWidth);
	DEF_NATIVE(Image, GetHeight);
	// #endregion

	// #region Input
	/***
    * \class Input
    * \desc Input polling functions.<br/>\
This class also houses the input action system.
    */
	INIT_CLASS(Input);
	DEF_NATIVE(Input, GetMouseX);
	DEF_NATIVE(Input, GetMouseY);
	DEF_NATIVE(Input, IsMouseButtonDown);
	DEF_NATIVE(Input, IsMouseButtonPressed);
	DEF_NATIVE(Input, IsMouseButtonReleased);
	DEF_NATIVE(Input, GetMouseMode);
	DEF_NATIVE(Input, SetMouseMode);
	DEF_NATIVE(Input, IsKeyDown);
	DEF_NATIVE(Input, IsKeyPressed);
	DEF_NATIVE(Input, IsKeyReleased);
	DEF_NATIVE(Input, GetKeyName);
	DEF_NATIVE(Input, GetButtonName);
	DEF_NATIVE(Input, GetAxisName);
	DEF_NATIVE(Input, ParseKeyName);
	DEF_NATIVE(Input, ParseButtonName);
	DEF_NATIVE(Input, ParseAxisName);
	DEF_NATIVE(Input, GetActionList);
	DEF_NATIVE(Input, ActionExists);
	DEF_NATIVE(Input, IsActionHeld);
	DEF_NATIVE(Input, IsActionPressed);
	DEF_NATIVE(Input, IsActionReleased);
	DEF_NATIVE(Input, IsActionHeldByAny);
	DEF_NATIVE(Input, IsActionPressedByAny);
	DEF_NATIVE(Input, IsActionReleasedByAny);
	DEF_NATIVE(Input, IsAnyActionHeld);
	DEF_NATIVE(Input, IsAnyActionPressed);
	DEF_NATIVE(Input, IsAnyActionReleased);
	DEF_NATIVE(Input, IsAnyActionHeldByAny);
	DEF_NATIVE(Input, IsAnyActionPressedByAny);
	DEF_NATIVE(Input, IsAnyActionReleasedByAny);
	DEF_NATIVE(Input, GetAnalogActionInput);
	DEF_NATIVE(Input, GetActionBind);
	DEF_NATIVE(Input, SetActionBind);
	DEF_NATIVE(Input, AddActionBind);
	DEF_NATIVE(Input, RemoveActionBind);
	DEF_NATIVE(Input, GetBoundActionList);
	DEF_NATIVE(Input, GetBoundActionCount);
	DEF_NATIVE(Input, GetBoundActionMap);
	DEF_NATIVE(Input, GetDefaultActionBind);
	DEF_NATIVE(Input, SetDefaultActionBind);
	DEF_NATIVE(Input, AddDefaultActionBind);
	DEF_NATIVE(Input, RemoveDefaultActionBind);
	DEF_NATIVE(Input, GetDefaultBoundActionList);
	DEF_NATIVE(Input, GetDefaultBoundActionCount);
	DEF_NATIVE(Input, GetDefaultBoundActionMap);
	DEF_NATIVE(Input, ResetActionBindsToDefaults);
	DEF_NATIVE(Input, IsPlayerUsingDevice);
	DEF_NATIVE(Input, GetPlayerControllerIndex);
	DEF_NATIVE(Input, SetPlayerControllerIndex);

	/***
    * \enum InputDevice_Keyboard
    * \desc Keyboard input device.
    */
	DEF_ENUM(InputDevice_Keyboard);
	/***
    * \enum InputDevice_Controller
    * \desc Controller input device.
    */
	DEF_ENUM(InputDevice_Controller);
	/***
    * \constant NUM_INPUT_DEVICE_TYPES
    * \desc Number of input device types.
    */
	DEF_CONST_INT("NUM_INPUT_DEVICE_TYPES", (int)InputDevice_MAX);

	/***
    * \enum InputBind_Keyboard
    * \desc Keyboard key input bind.
    */
	DEF_CONST_INT("InputBind_Keyboard", INPUT_BIND_KEYBOARD);
	/***
    * \enum InputBind_ControllerButton
    * \desc Controller button input bind.
    */
	DEF_CONST_INT("InputBind_ControllerButton", INPUT_BIND_CONTROLLER_BUTTON);
	/***
    * \enum InputBind_ControllerAxis
    * \desc Controller axis input bind.
    */
	DEF_CONST_INT("InputBind_ControllerAxis", INPUT_BIND_CONTROLLER_AXIS);
	/***
    * \constant NUM_INPUT_BIND_TYPES
    * \desc Number of input bind types.
    */
	DEF_ENUM(NUM_INPUT_BIND_TYPES);
	// #endregion

	// #region Instance
	/***
    * \class Instance
    * \desc General <ref Entity> related functions.
    */
	INIT_CLASS(Instance);
	DEF_NATIVE(Instance, Create);
	DEF_NATIVE(Instance, GetNth);
	DEF_NATIVE(Instance, IsClass);
	DEF_NATIVE(Instance, GetClass);
	DEF_NATIVE(Instance, GetCount);
	DEF_NATIVE(Instance, GetNextInstance);
	DEF_NATIVE(Instance, GetBySlotID);
	DEF_NATIVE(Instance, DisableAutoAnimate);
	DEF_NATIVE(Instance, SetUseRenderRegions);
	DEF_NATIVE(Instance, Copy);
	DEF_NATIVE(Instance, ChangeClass);

	/***
    * \enum Persistence_NONE
    * \desc Doesn't persist between scenes.
    */
	DEF_ENUM(Persistence_NONE);
	/***
    * \enum Persistence_SCENE
    * \desc Persists between scenes.
    */
	DEF_ENUM(Persistence_SCENE);
	/***
    * \enum Persistence_GAME
    * \desc Always persists, unless the game is restarted.
    */
	DEF_ENUM(Persistence_GAME);
	// #endregion

	// #region JSON
	/***
    * \class JSON
    * \desc JSON parsing and writing.
    */
	INIT_CLASS(JSON);
	DEF_NATIVE(JSON, Parse);
	DEF_NATIVE(JSON, ToString);
	// #endregion

	// #region Math
	/***
    * \class Math
    * \desc General Math functions.
    */
	INIT_CLASS(Math);
	DEF_NATIVE(Math, Cos);
	DEF_NATIVE(Math, Sin);
	DEF_NATIVE(Math, Tan);
	DEF_NATIVE(Math, Acos);
	DEF_NATIVE(Math, Asin);
	DEF_NATIVE(Math, Atan);
	DEF_NATIVE(Math, Distance);
	DEF_NATIVE(Math, Direction);
	DEF_NATIVE(Math, Abs);
	DEF_NATIVE(Math, Min);
	DEF_NATIVE(Math, Max);
	DEF_NATIVE(Math, Clamp);
	DEF_NATIVE(Math, Sign);
	DEF_NATIVE(Math, Uint8);
	DEF_NATIVE(Math, Uint16);
	DEF_NATIVE(Math, Uint32);
	DEF_NATIVE(Math, Uint64);
	DEF_NATIVE(Math, Random);
	DEF_NATIVE(Math, RandomMax);
	DEF_NATIVE(Math, RandomRange);
	DEF_NATIVE(Math, Floor);
	DEF_NATIVE(Math, Ceil);
	DEF_NATIVE(Math, Round);
	DEF_NATIVE(Math, Sqrt);
	DEF_NATIVE(Math, Pow);
	DEF_NATIVE(Math, Exp);
	// #endregion

	// #region RSDK.Math
	/***
    * \class RSDK.Math
    * \desc RSDK math functions.
    */
	INIT_NAMESPACED_CLASS(RSDK, Math);
	DEF_NAMESPACED_NATIVE(Math, ClearTrigLookupTables);
	DEF_NAMESPACED_NATIVE(Math, CalculateTrigAngles);
	DEF_NAMESPACED_NATIVE(Math, Sin1024);
	DEF_NAMESPACED_NATIVE(Math, Cos1024);
	DEF_NAMESPACED_NATIVE(Math, Tan1024);
	DEF_NAMESPACED_NATIVE(Math, ASin1024);
	DEF_NAMESPACED_NATIVE(Math, ACos1024);
	DEF_NAMESPACED_NATIVE(Math, Sin512);
	DEF_NAMESPACED_NATIVE(Math, Cos512);
	DEF_NAMESPACED_NATIVE(Math, Tan512);
	DEF_NAMESPACED_NATIVE(Math, ASin512);
	DEF_NAMESPACED_NATIVE(Math, ACos512);
	DEF_NAMESPACED_NATIVE(Math, Sin256);
	DEF_NAMESPACED_NATIVE(Math, Cos256);
	DEF_NAMESPACED_NATIVE(Math, Tan256);
	DEF_NAMESPACED_NATIVE(Math, ASin256);
	DEF_NAMESPACED_NATIVE(Math, ACos256);
	DEF_NAMESPACED_NATIVE(Math, ATan2);
	DEF_NAMESPACED_NATIVE(Math, RadianToInteger);
	DEF_NAMESPACED_NATIVE(Math, IntegerToRadian);
	DEF_NAMESPACED_NATIVE(Math, ToFixed);
	DEF_NAMESPACED_NATIVE(Math, FromFixed);
	DEF_NAMESPACED_NATIVE(Math, GetRandSeed);
	DEF_NAMESPACED_NATIVE(Math, SetRandSeed);
	DEF_NAMESPACED_NATIVE(Math, RandomInteger);
	DEF_NAMESPACED_NATIVE(Math, RandomIntegerSeeded);
	// #endregion

	// #region Matrix
	/***
    * \class Matrix
    * \desc Matrix manipulation.
    */
	INIT_CLASS(Matrix);
	DEF_NATIVE(Matrix, Create);
	DEF_NATIVE(Matrix, Identity);
	DEF_NATIVE(Matrix, Perspective);
	DEF_NATIVE(Matrix, Copy);
	DEF_NATIVE(Matrix, Multiply);
	DEF_NATIVE(Matrix, Translate);
	DEF_NATIVE(Matrix, Scale);
	DEF_NATIVE(Matrix, Rotate);
	// #endregion

	// #region RSDK.Matrix
	/***
    * \class RSDK.Matrix
    * \desc RSDK matrix manipulation.
    */
	INIT_NAMESPACED_CLASS(RSDK, Matrix);
	DEF_NATIVE(Matrix, Create256);
	DEF_NATIVE(Matrix, Identity256);
	DEF_NATIVE(Matrix, Multiply256);
	DEF_NATIVE(Matrix, Translate256);
	DEF_NATIVE(Matrix, Scale256);
	DEF_NATIVE(Matrix, RotateX256);
	DEF_NATIVE(Matrix, RotateY256);
	DEF_NATIVE(Matrix, RotateZ256);
	DEF_NATIVE(Matrix, Rotate256);
	// #endregion

	// #region Model
	/***
    * \class Model
    * \desc Functions to manipulate and retrieve information about 3D models.
    */
	INIT_CLASS(Model);
	DEF_NATIVE(Model, GetVertexCount);
	DEF_NATIVE(Model, GetAnimationCount);
	DEF_NATIVE(Model, GetAnimationName);
	DEF_NATIVE(Model, GetAnimationIndex);
	DEF_NATIVE(Model, GetFrameCount);
	DEF_NATIVE(Model, GetAnimationLength);
	DEF_NATIVE(Model, HasMaterials);
	DEF_NATIVE(Model, HasBones);
	DEF_NATIVE(Model, GetMaterialCount);
	DEF_NATIVE(Model, GetMaterial);
	DEF_NATIVE(Model, CreateArmature);
	DEF_NATIVE(Model, PoseArmature);
	DEF_NATIVE(Model, ResetArmature);
	DEF_NATIVE(Model, DeleteArmature);
	// #endregion

	// #region Music
	/***
    * \class Music
    * \desc Music playback.
    */
	INIT_CLASS(Music);
	DEF_NATIVE(Music, Play);
	DEF_NATIVE(Music, Stop);
	DEF_NATIVE(Music, StopWithFadeOut);
	DEF_NATIVE(Music, Pause);
	DEF_NATIVE(Music, Resume);
	DEF_NATIVE(Music, Clear);
	DEF_NATIVE(Music, IsPlaying);
	DEF_NATIVE(Music, GetPosition);
	DEF_NATIVE(Music, Alter);
	DEF_NATIVE(Music, GetLoopPoint);
	DEF_NATIVE(Music, SetLoopPoint);
	/***
	* \enum AUDIO_LOOP_NONE
	* \desc When used as the `loopPoint` argument in <ref Music.Play> or <ref Sound.Play>, specifies that the audio should not loop, even if the audio file has a loop point.
	*/
	DEF_CONST_INT("AUDIO_LOOP_NONE", AUDIO_LOOP_NONE);
	/***
	* \enum AUDIO_LOOP_DEFAULT
	* \desc When used as the `loopPoint` argument in <ref Music.Play> or <ref Sound.Play>, specifies that the audio should use loop point defined in the metadata of the audio file.
	*/
	DEF_CONST_INT("AUDIO_LOOP_DEFAULT", AUDIO_LOOP_DEFAULT);
	// #endregion

	// #region Number
	/***
    * \class Number
    * \desc Number type related functions.
    */
	INIT_CLASS(Number);
	DEF_NATIVE(Number, ToString);
	DEF_NATIVE(Number, AsInteger);
	DEF_NATIVE(Number, AsDecimal);
	// #endregion

	// #region Object
	/***
    * \class Object
    * \desc Entity class related functions.
    */
	INIT_CLASS(Object);
	DEF_NATIVE(Object, Loaded);
	DEF_NATIVE(Object, SetActivity);
	DEF_NATIVE(Object, GetActivity);
	// #endRegion

	// #region Palette
	/***
    * \class Palette
    * \desc Palette manipulation.<br/>\
Use <ref Palette.EnablePaletteUsage> to enable palettes.
    */
	INIT_CLASS(Palette);
	DEF_NATIVE(Palette, EnablePaletteUsage);
	DEF_NATIVE(Palette, LoadFromResource);
	DEF_NATIVE(Palette, LoadFromImage);
	DEF_NATIVE(Palette, GetColor);
	DEF_NATIVE(Palette, SetColor);
	DEF_NATIVE(Palette, GetColorTransparent);
	DEF_NATIVE(Palette, SetColorTransparent);
	DEF_NATIVE(Palette, MixPalettes);
	DEF_NATIVE(Palette, RotateColorsLeft);
	DEF_NATIVE(Palette, RotateColorsRight);
	DEF_NATIVE(Palette, CopyColors);
	DEF_NATIVE(Palette, UsePaletteIndexLines);
	DEF_NATIVE(Palette, SetPaletteIndexLines);
	// #endregion

	// #region Random
	/***
    * \class Random
    * \desc The engine's pseudorandom number generator.<br/>\
This is preferred over <ref Math>'s random functions if you require consistency, since <ref Random> can be seeded and utilizes a defined algorithm, which functions such as <ref Math.Random> cannot guarantee.
    */
	INIT_CLASS(Random);
	DEF_NATIVE(Random, SetSeed);
	DEF_NATIVE(Random, GetSeed);
	DEF_NATIVE(Random, Max);
	DEF_NATIVE(Random, Range);
	// #endregion

	// #region Resources
	/***
    * \class Resources
    * \desc Resource loading.
    */
	INIT_CLASS(Resources);
	DEF_NATIVE(Resources, LoadSprite);
	DEF_NATIVE(Resources, LoadDynamicSprite);
	DEF_NATIVE(Resources, LoadImage);
	DEF_NATIVE(Resources, LoadModel);
	DEF_NATIVE(Resources, LoadMusic);
	DEF_NATIVE(Resources, LoadSound);
	DEF_NATIVE(Resources, LoadVideo);
	DEF_NATIVE(Resources, FileExists);
	DEF_NATIVE(Resources, ReadAllText);

	/***
    * \enum SCOPE_SCENE
    * \desc Scene scope.
    */
	DEF_ENUM(SCOPE_SCENE);
	/***
    * \enum SCOPE_GAME
    * \desc Game scope.
    */
	DEF_ENUM(SCOPE_GAME);
	// #endregion

	// #region Scene
	/***
    * \class Scene
    * \desc Functions for manipulating and retrieving information about scenes.
    */
	INIT_CLASS(Scene);
	DEF_NATIVE(Scene, Load);
	DEF_NATIVE(Scene, Change);
	DEF_NATIVE(Scene, LoadTileCollisions);
	DEF_NATIVE(Scene, AreTileCollisionsLoaded);
	DEF_NATIVE(Scene, AddTileset);
	DEF_NATIVE(Scene, Restart);
	DEF_NATIVE(Scene, PropertyExists);
	DEF_NATIVE(Scene, GetProperty);
	DEF_NATIVE(Scene, GetLayerCount);
	DEF_NATIVE(Scene, GetLayerIndex);
	DEF_NATIVE(Scene, GetLayerName);
	DEF_NATIVE(Scene, GetLayerVisible);
	DEF_NATIVE(Scene, GetLayerOpacity);
	DEF_NATIVE(Scene, GetLayerShader);
	DEF_NATIVE(Scene, GetLayerUsePaletteIndexLines);
	DEF_NATIVE(Scene, GetLayerProperty);
	DEF_NATIVE(Scene, GetLayerExists); // deprecated
	DEF_NATIVE(Scene, GetLayerDeformSplitLine);
	DEF_NATIVE(Scene, GetLayerDeformOffsetA);
	DEF_NATIVE(Scene, GetLayerDeformOffsetB);
	DEF_NATIVE(Scene, LayerPropertyExists);
	DEF_NATIVE(Scene, GetName);
	DEF_NATIVE(Scene, GetType);
	DEF_NATIVE(Scene, GetWidth);
	DEF_NATIVE(Scene, GetHeight);
	DEF_NATIVE(Scene, GetLayerWidth);
	DEF_NATIVE(Scene, GetLayerHeight);
	DEF_NATIVE(Scene, GetLayerOffsetX);
	DEF_NATIVE(Scene, GetLayerOffsetY);
	DEF_NATIVE(Scene, GetLayerDrawGroup);
	DEF_NATIVE(Scene, GetLayerHorizontalRepeat);
	DEF_NATIVE(Scene, GetLayerVerticalRepeat);
	DEF_NATIVE(Scene, GetTileWidth);
	DEF_NATIVE(Scene, GetTileHeight);
	DEF_NATIVE(Scene, GetTileID);
	DEF_NATIVE(Scene, GetTileFlipX);
	DEF_NATIVE(Scene, GetTileFlipY);
	DEF_NATIVE(Scene, GetTilesetCount);
	DEF_NATIVE(Scene, GetTilesetIndex);
	DEF_NATIVE(Scene, GetTilesetName);
	DEF_NATIVE(Scene, GetTilesetTileCount);
	DEF_NATIVE(Scene, GetTilesetFirstTileID);
	DEF_NATIVE(Scene, GetTilesetPaletteIndex);
	DEF_NATIVE(Scene, GetDrawGroupCount);
	DEF_NATIVE(Scene, GetDrawGroupEntityDepthSorting);
	DEF_NATIVE(Scene, GetCurrentFolder);
	DEF_NATIVE(Scene, GetCurrentID);
	DEF_NATIVE(Scene, GetCurrentResourceFolder);
	DEF_NATIVE(Scene, GetCurrentCategory);
	DEF_NATIVE(Scene, GetDebugMode);
	DEF_NATIVE(Scene, GetFirstInstance);
	DEF_NATIVE(Scene, GetLastInstance);
	DEF_NATIVE(Scene, GetReservedSlotIDs);
	DEF_NATIVE(Scene, GetInstanceCount);
	DEF_NATIVE(Scene, GetStaticInstanceCount);
	DEF_NATIVE(Scene, GetDynamicInstanceCount);
	DEF_NATIVE(Scene, GetTileAnimationEnabled);
	DEF_NATIVE(Scene, GetTileAnimSequence);
	DEF_NATIVE(Scene, GetTileAnimSequenceDurations);
	DEF_NATIVE(Scene, GetTileAnimSequencePaused);
	DEF_NATIVE(Scene, GetTileAnimSequenceSpeed);
	DEF_NATIVE(Scene, GetTileAnimSequenceFrame);
	DEF_NATIVE(Scene, IsCurrentEntryValid);
	DEF_NATIVE(Scene, IsUsingFolder);
	DEF_NATIVE(Scene, IsUsingID);
	DEF_NATIVE(Scene, IsPaused);
	DEF_NATIVE(Scene, SetReservedSlotIDs);
	DEF_NATIVE(Scene, SetDebugMode);
	DEF_NATIVE(Scene, SetTile);
	DEF_NATIVE(Scene, SetTileCollisionSides);
	DEF_NATIVE(Scene, SetPaused);
	DEF_NATIVE(Scene, SetTileAnimationEnabled);
	DEF_NATIVE(Scene, SetTileAnimSequence);
	DEF_NATIVE(Scene, SetTileAnimSequenceFromSprite);
	DEF_NATIVE(Scene, SetTileAnimSequencePaused);
	DEF_NATIVE(Scene, SetTileAnimSequenceSpeed);
	DEF_NATIVE(Scene, SetTileAnimSequenceFrame);
	DEF_NATIVE(Scene, SetTilesetPaletteIndex);
	DEF_NATIVE(Scene, SetLayerVisible);
	DEF_NATIVE(Scene, SetLayerCollidable);
	DEF_NATIVE(Scene, SetLayerInternalSize);
	DEF_NATIVE(Scene, SetLayerOffsetPosition);
	DEF_NATIVE(Scene, SetLayerOffsetX);
	DEF_NATIVE(Scene, SetLayerOffsetY);
	DEF_NATIVE(Scene, SetLayerDrawGroup);
	DEF_NATIVE(Scene, SetLayerDrawBehavior);
	DEF_NATIVE(Scene, SetLayerRepeat);
	DEF_NATIVE(Scene, SetLayerHorizontalRepeat);
	DEF_NATIVE(Scene, SetLayerVerticalRepeat);
	DEF_NATIVE(Scene, SetDrawGroupCount);
	DEF_NATIVE(Scene, SetDrawGroupEntityDepthSorting);
	DEF_NATIVE(Scene, SetLayerBlend);
	DEF_NATIVE(Scene, SetLayerOpacity);
	DEF_NATIVE(Scene, SetLayerShader);
	DEF_NATIVE(Scene, SetLayerUsePaletteIndexLines);
	DEF_NATIVE(Scene, SetLayerScroll);
	DEF_NATIVE(Scene, SetLayerSetParallaxLinesBegin);
	DEF_NATIVE(Scene, SetLayerSetParallaxLines);
	DEF_NATIVE(Scene, SetLayerSetParallaxLinesEnd);
	DEF_NATIVE(Scene, SetLayerTileDeforms);
	DEF_NATIVE(Scene, SetLayerTileDeformSplitLine);
	DEF_NATIVE(Scene, SetLayerTileDeformOffsets);
	DEF_NATIVE(Scene, SetLayerDeformOffsetA);
	DEF_NATIVE(Scene, SetLayerDeformOffsetB);
	DEF_NATIVE(Scene, SetLayerCustomScanlineFunction);
	DEF_NATIVE(Scene, SetTileScanline);
	DEF_NATIVE(Scene, SetLayerCustomRenderFunction);
	DEF_NATIVE(Scene, SetObjectViewRender);
	DEF_NATIVE(Scene, SetTileViewRender);

	/***
	* \enum SCENETYPE_NONE
	* \desc The current scene loaded in the game is not a scene.
	*/
	DEF_ENUM(SCENETYPE_NONE);
	/***
	* \enum SCENETYPE_HATCH
	* \desc The current scene loaded in the game is a Hatch scene.
	*/
	DEF_ENUM(SCENETYPE_HATCH);
	/***
	* \enum SCENETYPE_TILED
	* \desc The current scene loaded in the game is a Tiled map.
	*/
	DEF_ENUM(SCENETYPE_TILED);
	/***
	* \enum SCENETYPE_RSDK
	* \desc The current scene loaded in the game is an RSDK scene.
	*/
	DEF_ENUM(SCENETYPE_RSDK);
	/***
    * \enum DrawBehavior_HorizontalParallax
    * \desc Horizontal parallax.
    */
	DEF_ENUM(DrawBehavior_HorizontalParallax);
	/***
    * \enum DrawBehavior_VerticalParallax
    * \desc Do not use.
    */
	DEF_ENUM(DrawBehavior_VerticalParallax);
	/***
    * \enum DrawBehavior_CustomTileScanLines
    * \desc Custom scanline behavior.
    */
	DEF_ENUM(DrawBehavior_CustomTileScanLines);
	// #endregion

	// #region SceneList
	/***
    * \class SceneList
    * \desc Functions for manipulating and retrieving information about the scene list.
    */
	INIT_CLASS(SceneList);
	DEF_NATIVE(SceneList, Get);
	DEF_NATIVE(SceneList, GetEntryID);
	DEF_NATIVE(SceneList, GetCategoryID);
	DEF_NATIVE(SceneList, GetEntryName);
	DEF_NATIVE(SceneList, GetCategoryName);
	DEF_NATIVE(SceneList, GetEntryProperty);
	DEF_NATIVE(SceneList, GetCategoryProperty);
	DEF_NATIVE(SceneList, HasEntryProperty);
	DEF_NATIVE(SceneList, HasCategoryProperty);
	DEF_NATIVE(SceneList, GetCategoryCount);
	DEF_NATIVE(SceneList, GetSceneCount);
	// #endregion

	// #region Scene3D
	/***
    * \class Scene3D
    * \desc The main interface for drawing 3D elements.
    */
	INIT_CLASS(Scene3D);
	DEF_NATIVE(Scene3D, Create);
	DEF_NATIVE(Scene3D, Delete);
	DEF_NATIVE(Scene3D, SetDrawMode);
	DEF_NATIVE(Scene3D, SetFaceCullMode);
	DEF_NATIVE(Scene3D, SetFieldOfView);
	DEF_NATIVE(Scene3D, SetFarClippingPlane);
	DEF_NATIVE(Scene3D, SetNearClippingPlane);
	DEF_NATIVE(Scene3D, SetViewMatrix);
	DEF_NATIVE(Scene3D, SetCustomProjectionMatrix);
	DEF_NATIVE(Scene3D, SetAmbientLighting);
	DEF_NATIVE(Scene3D, SetDiffuseLighting);
	DEF_NATIVE(Scene3D, SetSpecularLighting);
	DEF_NATIVE(Scene3D, SetFogEquation);
	DEF_NATIVE(Scene3D, SetFogStart);
	DEF_NATIVE(Scene3D, SetFogEnd);
	DEF_NATIVE(Scene3D, SetFogDensity);
	DEF_NATIVE(Scene3D, SetFogColor);
	DEF_NATIVE(Scene3D, SetFogSmoothness);
	DEF_NATIVE(Scene3D, SetPointSize);
	DEF_NATIVE(Scene3D, Clear);

	/***
    * \enum FaceCull_None
    * \desc Disables face culling.
    */
	DEF_ENUM(FaceCull_None);
	/***
    * \enum FaceCull_Back
    * \desc Culls back faces.
    */
	DEF_ENUM(FaceCull_Back);
	/***
    * \enum FaceCull_Front
    * \desc Culls front faces.
    */
	DEF_ENUM(FaceCull_Front);

	/***
    * \enum FogEquation_Linear
    * \desc Linear fog equation.
    */
	DEF_ENUM(FogEquation_Linear);
	/***
    * \enum FogEquation_Exp
    * \desc Exponential fog equation.
    */
	DEF_ENUM(FogEquation_Exp);
	// #endregion

	// #region Serializer
	/***
    * \class Serializer
    * \desc Functions for serializing values.<br/>\
<br/>Supported types:<ul>\
<li>integer</li>\
<li>decimal</li>\
<li>null</li>\
<li>string</li>\
<li>array</li>\
<li>map</li>\
</ul>
    */
	INIT_CLASS(Serializer);
	DEF_NATIVE(Serializer, WriteToStream);
	DEF_NATIVE(Serializer, ReadFromStream);
	// #endregion

	// #region Settings
	/***
    * \class Settings
    * \desc Reading and writing the application's settings from and into INI files.
    */
	INIT_CLASS(Settings);
	DEF_NATIVE(Settings, Load);
	DEF_NATIVE(Settings, Save);
	DEF_NATIVE(Settings, SetFilename);
	DEF_NATIVE(Settings, GetString);
	DEF_NATIVE(Settings, GetNumber);
	DEF_NATIVE(Settings, GetInteger);
	DEF_NATIVE(Settings, GetBool);
	DEF_NATIVE(Settings, SetString);
	DEF_NATIVE(Settings, SetNumber);
	DEF_NATIVE(Settings, SetInteger);
	DEF_NATIVE(Settings, SetBool);
	DEF_NATIVE(Settings, AddSection);
	DEF_NATIVE(Settings, RemoveSection);
	DEF_NATIVE(Settings, SectionExists);
	DEF_NATIVE(Settings, GetSectionCount);
	DEF_NATIVE(Settings, PropertyExists);
	DEF_NATIVE(Settings, RemoveProperty);
	DEF_NATIVE(Settings, GetPropertyCount);
	// #endregion

	// #region Shader
	/***
    * \enum SHADERSTAGE_VERTEX
    * \desc Vertex shader stage.
    */
	DEF_CONST_INT("SHADERSTAGE_VERTEX", Shader::STAGE_VERTEX);
	/***
    * \enum SHADERSTAGE_FRAGMENT
    * \desc Fragment shader stage.
    */
	DEF_CONST_INT("SHADERSTAGE_FRAGMENT", Shader::STAGE_FRAGMENT);
	/***
    * \enum SHADERDATATYPE_FLOAT
    * \desc `float` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_FLOAT", Shader::DATATYPE_FLOAT);
	/***
    * \enum SHADERDATATYPE_VEC2
    * \desc `vec2` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_VEC2", Shader::DATATYPE_FLOAT_VEC2);
	/***
    * \enum SHADERDATATYPE_VEC3
    * \desc `vec3` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_VEC3", Shader::DATATYPE_FLOAT_VEC3);
	/***
    * \enum SHADERDATATYPE_VEC4
    * \desc `vec4` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_VEC4", Shader::DATATYPE_FLOAT_VEC4);
	/***
    * \enum SHADERDATATYPE_INT
    * \desc `int` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_INT", Shader::DATATYPE_INT);
	/***
    * \enum SHADERDATATYPE_IVEC2
    * \desc `ivec2` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_IVEC2", Shader::DATATYPE_INT_VEC2);
	/***
    * \enum SHADERDATATYPE_IVEC3
    * \desc `ivec3` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_IVEC3", Shader::DATATYPE_INT_VEC3);
	/***
    * \enum SHADERDATATYPE_IVEC4
    * \desc `ivec4` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_IVEC4", Shader::DATATYPE_INT_VEC4);
	/***
    * \enum SHADERDATATYPE_BOOL
    * \desc `bool` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_BOOL", Shader::DATATYPE_BOOL);
	/***
    * \enum SHADERDATATYPE_BVEC2
    * \desc `bvec2` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_BVEC2", Shader::DATATYPE_BOOL_VEC2);
	/***
    * \enum SHADERDATATYPE_BVEC3
    * \desc `bvec3` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_BVEC3", Shader::DATATYPE_BOOL_VEC3);
	/***
    * \enum SHADERDATATYPE_BVEC4
    * \desc `bvec4` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_BVEC4", Shader::DATATYPE_BOOL_VEC4);
	/***
    * \enum SHADERDATATYPE_MAT2
    * \desc `mat2` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_MAT2", Shader::DATATYPE_FLOAT_MAT2);
	/***
    * \enum SHADERDATATYPE_MAT3
    * \desc `mat3` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_MAT3", Shader::DATATYPE_FLOAT_MAT3);
	/***
    * \enum SHADERDATATYPE_MAT4
    * \desc `mat4` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_MAT4", Shader::DATATYPE_FLOAT_MAT4);
	/***
    * \enum SHADERDATATYPE_SAMPLER_2D
    * \desc `sampler2D` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_SAMPLER_2D", Shader::DATATYPE_SAMPLER_2D);
	/***
    * \enum SHADERDATATYPE_SAMPLER_CUBE
    * \desc `samplerCube` data type.
    */
	DEF_CONST_INT("SHADERDATATYPE_SAMPLER_CUBE", Shader::DATATYPE_SAMPLER_CUBE);
	// #endregion

	// #region SocketClient
	INIT_CLASS(SocketClient);
	DEF_NATIVE(SocketClient, Open);
	DEF_NATIVE(SocketClient, Close);
	DEF_NATIVE(SocketClient, IsOpen);
	DEF_NATIVE(SocketClient, Poll);
	DEF_NATIVE(SocketClient, BytesToRead);
	DEF_NATIVE(SocketClient, ReadDecimal);
	DEF_NATIVE(SocketClient, ReadInteger);
	DEF_NATIVE(SocketClient, ReadString);
	DEF_NATIVE(SocketClient, WriteDecimal);
	DEF_NATIVE(SocketClient, WriteInteger);
	DEF_NATIVE(SocketClient, WriteString);
	// #endregion

	// #region Sound
	/***
    * \class Sound
    * \desc Sound playback.
    */
	INIT_CLASS(Sound);
	DEF_NATIVE(Sound, Play);
	DEF_NATIVE(Sound, Stop);
	DEF_NATIVE(Sound, Pause);
	DEF_NATIVE(Sound, Resume);
	DEF_NATIVE(Sound, StopAll);
	DEF_NATIVE(Sound, PauseAll);
	DEF_NATIVE(Sound, ResumeAll);
	DEF_NATIVE(Sound, IsPlaying);
	DEF_NATIVE(Sound, PlayMultiple);
	DEF_NATIVE(Sound, PlayAtChannel);
	DEF_NATIVE(Sound, StopChannel);
	DEF_NATIVE(Sound, PauseChannel);
	DEF_NATIVE(Sound, ResumeChannel);
	DEF_NATIVE(Sound, AlterChannel);
	DEF_NATIVE(Sound, GetFreeChannel);
	DEF_NATIVE(Sound, IsChannelFree);
	DEF_NATIVE(Sound, GetLoopPoint);
	DEF_NATIVE(Sound, SetLoopPoint);
	// #endregion

	// #region Sprite
	/***
    * \class Sprite
    * \desc Functions to retrieve information about sprites.
    */
	INIT_CLASS(Sprite);
	DEF_NATIVE(Sprite, GetAnimationCount);
	DEF_NATIVE(Sprite, GetAnimationName);
	DEF_NATIVE(Sprite, GetAnimationIndexByName);
	DEF_NATIVE(Sprite, GetFrameExists);
	DEF_NATIVE(Sprite, GetFrameLoopIndex);
	DEF_NATIVE(Sprite, GetFrameCount);
	DEF_NATIVE(Sprite, GetFrameDuration);
	DEF_NATIVE(Sprite, GetFrameSpeed);
	DEF_NATIVE(Sprite, GetFrameWidth);
	DEF_NATIVE(Sprite, GetFrameHeight);
	DEF_NATIVE(Sprite, GetFrameID);
	DEF_NATIVE(Sprite, GetFrameOffsetX);
	DEF_NATIVE(Sprite, GetFrameOffsetY);
	DEF_NATIVE(Sprite, GetHitboxName);
	DEF_NATIVE(Sprite, GetHitboxIndex);
	DEF_NATIVE(Sprite, GetHitboxCount);
	DEF_NATIVE(Sprite, GetHitbox);
	DEF_NATIVE(Sprite, GetTextArray);
	DEF_NATIVE(Sprite, GetTextWidth);
	DEF_NATIVE(Sprite, MakePalettized);
	DEF_NATIVE(Sprite, MakeNonPalettized);
	// #endregion

	// #region Stream
	/***
    * \class Stream
    * \desc Functions for opening streams, as well as functions for reading and writing data.
    */
	INIT_CLASS(Stream);
	DEF_NATIVE(Stream, FromResource);
	DEF_NATIVE(Stream, FromFile);
	DEF_NATIVE(Stream, Close);
	DEF_NATIVE(Stream, Seek);
	DEF_NATIVE(Stream, SeekEnd);
	DEF_NATIVE(Stream, Skip);
	DEF_NATIVE(Stream, Position);
	DEF_NATIVE(Stream, Length);
	DEF_NATIVE(Stream, ReadByte);
	DEF_NATIVE(Stream, ReadUInt16);
	DEF_NATIVE(Stream, ReadUInt16BE);
	DEF_NATIVE(Stream, ReadUInt32);
	DEF_NATIVE(Stream, ReadUInt32BE);
	DEF_NATIVE(Stream, ReadUInt64);
	DEF_NATIVE(Stream, ReadInt16);
	DEF_NATIVE(Stream, ReadInt16BE);
	DEF_NATIVE(Stream, ReadInt32);
	DEF_NATIVE(Stream, ReadInt32BE);
	DEF_NATIVE(Stream, ReadInt64);
	DEF_NATIVE(Stream, ReadFloat);
	DEF_NATIVE(Stream, ReadString);
	DEF_NATIVE(Stream, ReadLine);
	DEF_NATIVE(Stream, WriteByte);
	DEF_NATIVE(Stream, WriteUInt16);
	DEF_NATIVE(Stream, WriteUInt16BE);
	DEF_NATIVE(Stream, WriteUInt32);
	DEF_NATIVE(Stream, WriteUInt32BE);
	DEF_NATIVE(Stream, WriteUInt64);
	DEF_NATIVE(Stream, WriteInt16);
	DEF_NATIVE(Stream, WriteInt16BE);
	DEF_NATIVE(Stream, WriteInt32);
	DEF_NATIVE(Stream, WriteInt32BE);
	DEF_NATIVE(Stream, WriteInt64);
	DEF_NATIVE(Stream, WriteFloat);
	DEF_NATIVE(Stream, WriteString);
	/***
    * \enum FileStream_READ_ACCESS
    * \desc Read file access mode. (`rb`)
    */
	DEF_ENUM_CLASS(FileStream, READ_ACCESS);
	/***
    * \enum FileStream_WRITE_ACCESS
    * \desc Write file access mode. (`wb`)
    */
	DEF_ENUM_CLASS(FileStream, WRITE_ACCESS);
	/***
    * \enum FileStream_APPEND_ACCESS
    * \desc Append file access mode. (`ab`)
    */
	DEF_ENUM_CLASS(FileStream, APPEND_ACCESS);
	// #endregion

	// #region String
	/***
    * \class String
    * \desc String manipulation functions.
    */
	INIT_CLASS(String);
	DEF_NATIVE(String, Format);
	DEF_NATIVE(String, Split);
	DEF_NATIVE(String, CharAt);
	DEF_NATIVE(String, CodepointAt);
	DEF_NATIVE(String, Length);
	DEF_NATIVE(String, Compare);
	DEF_NATIVE(String, IndexOf);
	DEF_NATIVE(String, Contains);
	DEF_NATIVE(String, Substring);
	DEF_NATIVE(String, ToUpperCase);
	DEF_NATIVE(String, ToLowerCase);
	DEF_NATIVE(String, LastIndexOf);
	DEF_NATIVE(String, ParseInteger);
	DEF_NATIVE(String, ParseDecimal);
	DEF_NATIVE(String, GetCodepoints);
	DEF_NATIVE(String, FromCodepoints);
	// #endregion

	// #region Texture
	/***
    * \class Texture
    * \desc Texture manipulation functions.
    */
	INIT_CLASS(Texture);
	DEF_NATIVE(Texture, Create);
	DEF_NATIVE(Texture, Copy);
	// #endregion

	// #region Touch
	/***
    * \class Touch
    * \desc Touch screen related functions.
    */
	INIT_CLASS(Touch);
	DEF_NATIVE(Touch, GetX);
	DEF_NATIVE(Touch, GetY);
	DEF_NATIVE(Touch, IsDown);
	DEF_NATIVE(Touch, IsPressed);
	DEF_NATIVE(Touch, IsReleased);
	// #endregion

	// #region TileCollision
	/***
    * \class TileCollision
    * \desc Basic functions for performing collision with scene tiles.
    */
	INIT_CLASS(TileCollision);
	DEF_NATIVE(TileCollision, Point);
	DEF_NATIVE(TileCollision, PointExtended);
	DEF_NATIVE(TileCollision, Line);
	/***
    * \enum SensorDirection_Down
    * \desc Down sensor direction.
    */
	DEF_CONST_INT("SensorDirection_Down", 0);
	/***
    * \enum SensorDirection_Right
    * \desc Right sensor direction.
    */
	DEF_CONST_INT("SensorDirection_Right", 1);
	/***
    * \enum SensorDirection_Up
    * \desc Up sensor direction.
    */
	DEF_CONST_INT("SensorDirection_Up", 2);
	/***
    * \enum SensorDirection_Left
    * \desc Left sensor direction.
    */
	DEF_CONST_INT("SensorDirection_Left", 3);
	// #endregion

	// #region TileInfo
	/***
    * \class TileInfo
    * \desc Functions for retrieving information about tiles.
    */
	INIT_CLASS(TileInfo);
	DEF_NATIVE(TileInfo, SetSpriteInfo);
	DEF_NATIVE(TileInfo, IsEmptySpace);
	DEF_NATIVE(TileInfo, GetEmptyTile);
	DEF_NATIVE(TileInfo, GetCollision);
	DEF_NATIVE(TileInfo, GetAngle);
	DEF_NATIVE(TileInfo, GetBehaviorFlag);
	DEF_NATIVE(TileInfo, IsCeiling);
	// #endregion

	// #region Thread
	INIT_CLASS(Thread);
	DEF_NATIVE(Thread, RunEvent);
	DEF_NATIVE(Thread, Sleep);
	// #endregion

	// #region VertexBuffer
	/***
    * \class VertexBuffer
    * \desc Representation of a vertex buffer.
    */
	INIT_CLASS(VertexBuffer);
	DEF_NATIVE(VertexBuffer, Create);
	DEF_NATIVE(VertexBuffer, Clear);
	DEF_NATIVE(VertexBuffer, Resize);
	DEF_NATIVE(VertexBuffer, Delete);
	// #endregion

	// #region Video
	/***
    * \class Video
    * \desc Video playback.
    */
	INIT_CLASS(Video);
	DEF_NATIVE(Video, Play);
	DEF_NATIVE(Video, Pause);
	DEF_NATIVE(Video, Resume);
	DEF_NATIVE(Video, Stop);
	DEF_NATIVE(Video, Close);
	DEF_NATIVE(Video, IsBuffering);
	DEF_NATIVE(Video, IsPlaying);
	DEF_NATIVE(Video, IsPaused);
	DEF_NATIVE(Video, SetPosition);
	DEF_NATIVE(Video, SetBufferDuration);
	DEF_NATIVE(Video, SetTrackEnabled);
	DEF_NATIVE(Video, GetPosition);
	DEF_NATIVE(Video, GetDuration);
	DEF_NATIVE(Video, GetBufferDuration);
	DEF_NATIVE(Video, GetBufferEnd);
	DEF_NATIVE(Video, GetTrackCount);
	DEF_NATIVE(Video, GetTrackEnabled);
	DEF_NATIVE(Video, GetTrackName);
	DEF_NATIVE(Video, GetTrackLanguage);
	DEF_NATIVE(Video, GetDefaultVideoTrack);
	DEF_NATIVE(Video, GetDefaultAudioTrack);
	DEF_NATIVE(Video, GetDefaultSubtitleTrack);
	DEF_NATIVE(Video, GetWidth);
	DEF_NATIVE(Video, GetHeight);
	// #endregion

	// #region View
	/***
    * \class View
    * \desc View manipulation functions.
    */
	INIT_CLASS(View);
	DEF_NATIVE(View, SetX);
	DEF_NATIVE(View, SetY);
	DEF_NATIVE(View, SetZ);
	DEF_NATIVE(View, SetPosition);
	DEF_NATIVE(View, AdjustX);
	DEF_NATIVE(View, AdjustY);
	DEF_NATIVE(View, AdjustZ);
	DEF_NATIVE(View, SetScale);
	DEF_NATIVE(View, SetAngle);
	DEF_NATIVE(View, SetSize);
	DEF_NATIVE(View, SetOutputX);
	DEF_NATIVE(View, SetOutputY);
	DEF_NATIVE(View, SetOutputPosition);
	DEF_NATIVE(View, SetOutputSize);
	DEF_NATIVE(View, GetX);
	DEF_NATIVE(View, GetY);
	DEF_NATIVE(View, GetZ);
	DEF_NATIVE(View, GetWidth);
	DEF_NATIVE(View, GetHeight);
	DEF_NATIVE(View, GetCenterX);
	DEF_NATIVE(View, GetCenterY);
	DEF_NATIVE(View, IsUsingDrawTarget);
	DEF_NATIVE(View, SetUseDrawTarget);
	DEF_NATIVE(View, GetDrawTarget);
	DEF_NATIVE(View, SetShader);
	DEF_NATIVE(View, GetShader);
	DEF_NATIVE(View, IsUsingSoftwareRenderer);
	DEF_NATIVE(View, SetUseSoftwareRenderer);
	DEF_NATIVE(View, SetUsePerspective);
	DEF_NATIVE(View, IsEnabled);
	DEF_NATIVE(View, SetEnabled);
	DEF_NATIVE(View, IsVisible);
	DEF_NATIVE(View, SetVisible);
	DEF_NATIVE(View, SetFieldOfView);
	DEF_NATIVE(View, SetPriority);
	DEF_NATIVE(View, GetPriority);
	DEF_NATIVE(View, GetCurrent);
	DEF_NATIVE(View, GetCount);
	DEF_NATIVE(View, GetActiveCount);
	DEF_NATIVE(View, CheckOnScreen);
	DEF_NATIVE(View, CheckPosOnScreen);
	// #endregion

	// #region Window
	/***
    * \class Window
    * \desc Window manipulation functions.
    */
	INIT_CLASS(Window);
	DEF_NATIVE(Window, SetSize);
	DEF_NATIVE(Window, SetFullscreen);
	DEF_NATIVE(Window, SetScale);
	DEF_NATIVE(Window, SetBorderless);
	DEF_NATIVE(Window, SetVSync);
	DEF_NATIVE(Window, SetPosition);
	DEF_NATIVE(Window, SetPositionCentered);
	DEF_NATIVE(Window, SetTitle);
	DEF_NATIVE(Window, SetPostProcessingShader);
	DEF_NATIVE(Window, GetWidth);
	DEF_NATIVE(Window, GetHeight);
	DEF_NATIVE(Window, GetFullscreen);
	DEF_NATIVE(Window, GetScale);
	DEF_NATIVE(Window, IsResizeable);
	// #endregion

	// #region XML
	/***
    * \class XML
    * \desc XML parsing.
    */
	INIT_CLASS(XML);
	DEF_NATIVE(XML, Parse);
	// #endregion

#undef DEF_NATIVE
#undef ALIAS_NATIVE
#undef INIT_CLASS

	// #region Tile Collision States
	/***
    * \enum TILECOLLISION_NONE
    * \desc Entity expects no tile collision.
    */
	DEF_ENUM(TILECOLLISION_NONE);
	/***
    * \enum TILECOLLISION_DOWN
    * \desc Entity expects downward gravity for tile collision.
    */
	DEF_ENUM(TILECOLLISION_DOWN);
	/***
    * \enum TILECOLLISION_UP
    * \desc Entity expects upward gravity for tile collision.
    */
	DEF_ENUM(TILECOLLISION_UP);
	// #endregion

	// #region Collision Sides
	/***
    * \enum C_NONE
    * \desc No collided side.
    */
	DEF_ENUM(C_NONE);
	/***
    * \enum C_TOP
    * \desc Top collided side.
    */
	DEF_ENUM(C_TOP);
	/***
    * \enum C_LEFT
    * \desc Left collided side.
    */
	DEF_ENUM(C_LEFT);
	/***
    * \enum C_RIGHT
    * \desc Right collided side.
    */
	DEF_ENUM(C_RIGHT);
	/***
    * \enum C_BOTTOM
    * \desc Bottom collided side.
    */
	DEF_ENUM(C_BOTTOM);
	// #endregion

	// #region Flip Flags
	/***
    * \enum FLIP_NONE
    * \desc No flip.
    */
	DEF_ENUM(FLIP_NONE);
	/***
    * \enum FLIP_X
    * \desc Horizontal flip.
    */
	DEF_ENUM(FLIP_X);
	/***
    * \enum FLIP_Y
    * \desc Vertical flip.
    */
	DEF_ENUM(FLIP_Y);
	/***
    * \enum FLIP_XY
    * \desc Horizontal and vertical flip.
    */
	DEF_ENUM(FLIP_XY);
	// #endregion

	// #region Collision Modes
	/***
    * \enum CMODE_FLOOR
    * \desc Entity expects to collide with a floor.
    */
	DEF_ENUM(CMODE_FLOOR);
	/***
    * \enum CMODE_LWALL
    * \desc Entity expects to collide with the left side of a wall.
    */
	DEF_ENUM(CMODE_LWALL);
	/***
    * \enum CMODE_ROOF
    * \desc Entity expects to collide with a roof.
    */
	DEF_ENUM(CMODE_ROOF);
	/***
    * \enum CMODE_RWALL
    * \desc Entity expects to collide with the right side of a wall.
    */
	DEF_ENUM(CMODE_RWALL);
	// #endregion

	// #region Active States
	/***
    * \enum ACTIVE_NEVER
    * \desc Entity never updates. Object never runs GlobalUpdate.
    */
	DEF_ENUM(ACTIVE_NEVER);
	/***
    * \enum ACTIVE_ALWAYS
    * \desc Entity always updates. Object always runs GlobalUpdate.
    */
	DEF_ENUM(ACTIVE_ALWAYS);
	/***
    * \enum ACTIVE_NORMAL
    * \desc Entity updates no matter where it is located on the scene, if the scene is not paused. GlobalUpdate is also called for the entity's class.
    */
	DEF_ENUM(ACTIVE_NORMAL);
	/***
    * \enum ACTIVE_PAUSED
    * \desc Entity only updates if the scene is paused. GlobalUpdate is also called for the entity's class.
    */
	DEF_ENUM(ACTIVE_PAUSED);
	/***
    * \enum ACTIVE_BOUNDS
    * \desc Entity only updates if it is within its bounds (uses UpdateRegionW and uses UpdateRegionH).
    */
	DEF_ENUM(ACTIVE_BOUNDS);
	/***
    * \enum ACTIVE_XBOUNDS
    * \desc Entity only updates within an X bound. (only uses UpdateRegionW)
    */
	DEF_ENUM(ACTIVE_XBOUNDS);
	/***
    * \enum ACTIVE_YBOUNDS
    * \desc Entity only updates within a Y bound. (only uses UpdateRegionH)
    */
	DEF_ENUM(ACTIVE_YBOUNDS);
	/***
    * \enum ACTIVE_RBOUNDS
    * \desc Entity updates within a radius. (uses UpdateRegionW)
    */
	DEF_ENUM(ACTIVE_RBOUNDS);
	/***
	* \enum ACTIVE_DISABLED
	* \desc Entity will not even reach a point where it would check for an update.
	*/
	DEF_ENUM(ACTIVE_DISABLED);
	// #endregion

	// #region Hitbox Sides
	/***
    * \enum HITBOX_LEFT
    * \desc Left side, slot 0 of a hitbox value.
    */
	DEF_ENUM(HITBOX_LEFT);
	/***
    * \enum HITBOX_TOP
    * \desc Top side, slot 1 of a hitbox value.
    */
	DEF_ENUM(HITBOX_TOP);
	/***
    * \enum HITBOX_RIGHT
    * \desc Right side, slot 2 of a hitbox value.
    */
	DEF_ENUM(HITBOX_RIGHT);
	/***
    * \enum HITBOX_BOTTOM
    * \desc Bottom side, slot 3 of a hitbox value.
    */
	DEF_ENUM(HITBOX_BOTTOM);
	// #endregion

	/***
    * \global CameraX
    * \type decimal
    * \desc The X position of the first camera.
    */
	DEF_LINK_DECIMAL("CameraX", &Scene::Views[0].X);
	/***
    * \global CameraY
    * \type decimal
    * \desc The Y position of the first camera.
    */
	DEF_LINK_DECIMAL("CameraY", &Scene::Views[0].Y);
	/***
    * \global LowPassFilter
    * \type decimal
    * \desc The low pass filter of the audio.
    */
	DEF_LINK_DECIMAL("LowPassFilter", &AudioManager::LowPassFilter);

	/***
    * \global CurrentView
    * \type integer
    * \desc The current camera index.
    */
	DEF_LINK_INT("CurrentView", &Scene::ViewCurrent);
	/***
    * \global Scene_Frame
    * \type integer
    * \desc The current scene frame.
    */
	DEF_LINK_INT("Scene_Frame", &Scene::Frame);
	/***
    * \global DeltaTime
    * \type decimal
    * \desc The delta time.
    */
	DEF_LINK_DECIMAL("DeltaTime", &Application::ActualDeltaTime);
	/***
	* \global Scene_Filter
	* \type integer
	* \desc The current scene filter.
	*/
	DEF_LINK_INT("Scene_Filter", &Scene::Filter);
	/***
    * \global Scene_TimeEnabled
    * \type integer
    * \desc Whether the scene timer is enabled.
    */
	DEF_LINK_INT("Scene_TimeEnabled", &Scene::TimeEnabled);
	/***
    * \global Scene_TimeCounter
    * \type integer
    * \desc The current scene time counter.
    */
	DEF_LINK_INT("Scene_TimeCounter", &Scene::TimeCounter);
	/***
    * \global Scene_Minutes
    * \type integer
    * \desc The minutes value of the scene timer.
    */
	DEF_LINK_INT("Scene_Minutes", &Scene::Minutes);
	/***
    * \global Scene_Seconds
    * \type integer
    * \desc The seconds value of the scene timer.
    */
	DEF_LINK_INT("Scene_Seconds", &Scene::Seconds);
	/***
    * \global Scene_Milliseconds
    * \type integer
    * \desc The milliseconds value of the scene timer.
    */
	DEF_LINK_INT("Scene_Milliseconds", &Scene::Milliseconds);
	/***
    * \global Scene_ListPos
    * \type integer
    * \desc The position of the current scene in the scene list.
    */
	DEF_LINK_INT("Scene_ListPos", &Scene::CurrentSceneInList);
	/***
    * \global Scene_ActiveCategory
    * \type integer
    * \desc The category number that contains the current scene.
    */
	DEF_LINK_INT("Scene_ActiveCategory", &Scene::ActiveCategory);
	/***
    * \global Scene_DebugMode
    * \type integer
    * \desc Whether Debug Mode has been turned on in the current scene.
    */
	DEF_LINK_INT("Scene_DebugMode", &Scene::DebugMode);
	/***
    * \constant MAX_SCENE_VIEWS
    * \type integer
    * \desc The max amount of scene views.
    */
	DEF_ENUM(MAX_SCENE_VIEWS);
	/***
    * \constant MAX_DRAW_GROUPS
    * \type integer
    * \desc The max amount of draw groups.
    */
	DEF_CONST_INT("MAX_DRAW_GROUPS", MAX_PRIORITY_PER_LAYER);

	/***
    * \constant Math_PI
    * \type decimal
    * \desc The value of pi.
    */
	DEF_CONST_DECIMAL("Math_PI", M_PI);
	/***
    * \constant Math_PI_DOUBLE
    * \type decimal
    * \desc Double of the value of pi.
    */
	DEF_CONST_DECIMAL("Math_PI_DOUBLE", M_PI * 2.0);
	/***
    * \constant Math_PI_HALF
    * \type decimal
    * \desc Half of the value of pi.
    */
	DEF_CONST_DECIMAL("Math_PI_HALF", M_PI / 2.0);
	/***
    * \constant Math_R_PI
    * \type decimal
    * \desc A less precise value of pi.
    */
	DEF_CONST_DECIMAL("Math_R_PI", RSDK_PI);

	/***
    * \constant NUM_KEYBOARD_KEYS
    * \type integer
    * \desc Count of keyboard keys.
    */
	DEF_ENUM(NUM_KEYBOARD_KEYS);

	/***
    * \constant MAX_PALETTE_COUNT
    * \type integer
    * \desc The max amount of palettes.
    */
	DEF_ENUM(MAX_PALETTE_COUNT);

	/***
    * \enum MOUSEMODE_DEFAULT
    * \desc "Absolute" mouse mode. The cursor is visible by default, and not constrained to the window.
    */
	DEF_ENUM(MOUSEMODE_DEFAULT);
	/***
    * \enum MOUSEMODE_RELATIVE
    * \desc "Relative" mouse mode. The cursor is invisible, and constrained to the window.
    */
	DEF_ENUM(MOUSEMODE_RELATIVE);

	/***
    * \constant KeyMod_SHIFT
    * \type integer
    * \desc Key modifier for either Shift key.
    */
	DEF_CONST_INT("KeyMod_SHIFT", KB_MODIFIER_SHIFT);

	/***
    * \constant KeyMod_CTRL
    * \type integer
    * \desc Key modifier for either Ctrl key.
    */
	DEF_CONST_INT("KeyMod_CTRL", KB_MODIFIER_CTRL);

	/***
    * \constant KeyMod_ALT
    * \type integer
    * \desc Key modifier for either Alt key.
    */
	DEF_CONST_INT("KeyMod_ALT", KB_MODIFIER_ALT);

	/***
    * \constant KeyMod_LSHIFT
    * \type integer
    * \desc Key modifier for the Left Shift key.
    */
	DEF_CONST_INT("KeyMod_LSHIFT", KB_MODIFIER_LSHIFT);

	/***
    * \constant KeyMod_RSHIFT
    * \type integer
    * \desc Key modifier for the Right Shift key.
    */
	DEF_CONST_INT("KeyMod_RSHIFT", KB_MODIFIER_RSHIFT);

	/***
    * \constant KeyMod_LCTRL
    * \type integer
    * \desc Key modifier for the Left Ctrl key.
    */
	DEF_CONST_INT("KeyMod_LCTRL", KB_MODIFIER_LCTRL);

	/***
    * \constant KeyMod_RCTRL
    * \type integer
    * \desc Key modifier for the Right Ctrl key.
    */
	DEF_CONST_INT("KeyMod_RCTRL", KB_MODIFIER_RCTRL);

	/***
    * \constant KeyMod_LALT
    * \type integer
    * \desc Key modifier for the Left Alt key.
    */
	DEF_CONST_INT("KeyMod_LALT", KB_MODIFIER_LALT);

	/***
    * \constant KeyMod_RALT
    * \type integer
    * \desc Key modifier for the Right Alt key.
    */
	DEF_CONST_INT("KeyMod_RALT", KB_MODIFIER_RALT);

	/***
    * \constant KeyMod_NUMLOCK
    * \type integer
    * \desc Key modifier for the Num Lock key.
    */
	DEF_CONST_INT("KeyMod_NUMLOCK", KB_MODIFIER_NUM);

	/***
    * \constant KeyMod_CAPSLOCK
    * \type integer
    * \desc Key modifier for the Caps Lock key.
    */
	DEF_CONST_INT("KeyMod_CAPSLOCK", KB_MODIFIER_CAPS);

#define CONST_KEY(key) DEF_CONST_INT("Key_" #key, Key_##key);
	{
		/***
        * \enum Key_UNKNOWN
        * \type integer
        * \desc Invalid key.
        */
		CONST_KEY(UNKNOWN);
		/***
        * \enum Key_A
        * \type integer
        * \desc A key.
        */
		CONST_KEY(A);
		/***
        * \enum Key_B
        * \type integer
        * \desc B key.
        */
		CONST_KEY(B);
		/***
        * \enum Key_C
        * \type integer
        * \desc C key.
        */
		CONST_KEY(C);
		/***
        * \enum Key_D
        * \type integer
        * \desc D key.
        */
		CONST_KEY(D);
		/***
        * \enum Key_E
        * \type integer
        * \desc E key.
        */
		CONST_KEY(E);
		/***
        * \enum Key_F
        * \type integer
        * \desc F key.
        */
		CONST_KEY(F);
		/***
        * \enum Key_G
        * \type integer
        * \desc G key.
        */
		CONST_KEY(G);
		/***
        * \enum Key_H
        * \type integer
        * \desc H key.
        */
		CONST_KEY(H);
		/***
        * \enum Key_I
        * \type integer
        * \desc I key.
        */
		CONST_KEY(I);
		/***
        * \enum Key_J
        * \type integer
        * \desc J key.
        */
		CONST_KEY(J);
		/***
        * \enum Key_K
        * \type integer
        * \desc K key.
        */
		CONST_KEY(K);
		/***
        * \enum Key_L
        * \type integer
        * \desc L key.
        */
		CONST_KEY(L);
		/***
        * \enum Key_M
        * \type integer
        * \desc M key.
        */
		CONST_KEY(M);
		/***
        * \enum Key_N
        * \type integer
        * \desc N key.
        */
		CONST_KEY(N);
		/***
        * \enum Key_O
        * \type integer
        * \desc O key.
        */
		CONST_KEY(O);
		/***
        * \enum Key_P
        * \type integer
        * \desc P key.
        */
		CONST_KEY(P);
		/***
        * \enum Key_Q
        * \type integer
        * \desc Q key.
        */
		CONST_KEY(Q);
		/***
        * \enum Key_R
        * \type integer
        * \desc R key.
        */
		CONST_KEY(R);
		/***
        * \enum Key_S
        * \type integer
        * \desc S key.
        */
		CONST_KEY(S);
		/***
        * \enum Key_T
        * \type integer
        * \desc T key.
        */
		CONST_KEY(T);
		/***
        * \enum Key_U
        * \type integer
        * \desc U key.
        */
		CONST_KEY(U);
		/***
        * \enum Key_V
        * \type integer
        * \desc V key.
        */
		CONST_KEY(V);
		/***
        * \enum Key_W
        * \type integer
        * \desc W key.
        */
		CONST_KEY(W);
		/***
        * \enum Key_X
        * \type integer
        * \desc X key.
        */
		CONST_KEY(X);
		/***
        * \enum Key_Y
        * \type integer
        * \desc Y key.
        */
		CONST_KEY(Y);
		/***
        * \enum Key_Z
        * \type integer
        * \desc Z key.
        */
		CONST_KEY(Z);

		/***
        * \enum Key_1
        * \type integer
        * \desc Number 1 key.
        */
		CONST_KEY(1);
		/***
        * \enum Key_2
        * \type integer
        * \desc Number 2 key.
        */
		CONST_KEY(2);
		/***
        * \enum Key_3
        * \type integer
        * \desc Number 3 key.
        */
		CONST_KEY(3);
		/***
        * \enum Key_4
        * \type integer
        * \desc Number 4 key.
        */
		CONST_KEY(4);
		/***
        * \enum Key_5
        * \type integer
        * \desc Number 5 key.
        */
		CONST_KEY(5);
		/***
        * \enum Key_6
        * \type integer
        * \desc Number 6 key.
        */
		CONST_KEY(6);
		/***
        * \enum Key_7
        * \type integer
        * \desc Number 7 key.
        */
		CONST_KEY(7);
		/***
        * \enum Key_8
        * \type integer
        * \desc Number 8 key.
        */
		CONST_KEY(8);
		/***
        * \enum Key_9
        * \type integer
        * \desc Number 9 key.
        */
		CONST_KEY(9);
		/***
        * \enum Key_0
        * \type integer
        * \desc Number 0 key.
        */
		CONST_KEY(0);

		/***
        * \enum Key_RETURN
        * \type integer
        * \desc Return key.
        */
		CONST_KEY(RETURN);
		/***
        * \enum Key_ESCAPE
        * \type integer
        * \desc Escape key.
        */
		CONST_KEY(ESCAPE);
		/***
        * \enum Key_BACKSPACE
        * \type integer
        * \desc Backspace key.
        */
		CONST_KEY(BACKSPACE);
		/***
        * \enum Key_TAB
        * \type integer
        * \desc Tab key.
        */
		CONST_KEY(TAB);
		/***
        * \enum Key_SPACE
        * \type integer
        * \desc Space Bar key.
        */
		CONST_KEY(SPACE);

		/***
        * \enum Key_MINUS
        * \type integer
        * \desc Minus key.
        */
		CONST_KEY(MINUS);
		/***
        * \enum Key_EQUALS
        * \type integer
        * \desc Equals key.
        */
		CONST_KEY(EQUALS);
		/***
        * \enum Key_LEFTBRACKET
        * \type integer
        * \desc Left Bracket key.
        */
		CONST_KEY(LEFTBRACKET);
		/***
        * \enum Key_RIGHTBRACKET
        * \type integer
        * \desc Right Bracket key.
        */
		CONST_KEY(RIGHTBRACKET);
		/***
        * \enum Key_BACKSLASH
        * \type integer
        * \desc Backslash key.
        */
		CONST_KEY(BACKSLASH);
		/***
        * \enum Key_SEMICOLON
        * \type integer
        * \desc Semicolon key.
        */
		CONST_KEY(SEMICOLON);
		/***
        * \enum Key_APOSTROPHE
        * \type integer
        * \desc Apostrophe key.
        */
		CONST_KEY(APOSTROPHE);
		/***
        * \enum Key_GRAVE
        * \type integer
        * \desc Grave key.
        */
		CONST_KEY(GRAVE);
		/***
        * \enum Key_COMMA
        * \type integer
        * \desc Comma key.
        */
		CONST_KEY(COMMA);
		/***
        * \enum Key_PERIOD
        * \type integer
        * \desc Period key.
        */
		CONST_KEY(PERIOD);
		/***
        * \enum Key_SLASH
        * \type integer
        * \desc SLASH key.
        */
		CONST_KEY(SLASH);

		/***
        * \enum Key_CAPSLOCK
        * \type integer
        * \desc Caps Lock key.
        */
		CONST_KEY(CAPSLOCK);

		/***
        * \enum Key_F1
        * \type integer
        * \desc F1 key.
        */
		CONST_KEY(F1);
		/***
        * \enum Key_F2
        * \type integer
        * \desc F2 key.
        */
		CONST_KEY(F2);
		/***
        * \enum Key_F3
        * \type integer
        * \desc F3 key.
        */
		CONST_KEY(F3);
		/***
        * \enum Key_F4
        * \type integer
        * \desc F4 key.
        */
		CONST_KEY(F4);
		/***
        * \enum Key_F5
        * \type integer
        * \desc F5 key.
        */
		CONST_KEY(F5);
		/***
        * \enum Key_F6
        * \type integer
        * \desc F6 key.
        */
		CONST_KEY(F6);
		/***
        * \enum Key_F7
        * \type integer
        * \desc F7 key.
        */
		CONST_KEY(F7);
		/***
        * \enum Key_F8
        * \type integer
        * \desc F8 key.
        */
		CONST_KEY(F8);
		/***
        * \enum Key_F9
        * \type integer
        * \desc F9 key.
        */
		CONST_KEY(F9);
		/***
        * \enum Key_F10
        * \type integer
        * \desc F10 key.
        */
		CONST_KEY(F10);
		/***
        * \enum Key_F11
        * \type integer
        * \desc F11 key.
        */
		CONST_KEY(F11);
		/***
        * \enum Key_F12
        * \type integer
        * \desc F12 key.
        */
		CONST_KEY(F12);

		/***
        * \enum Key_PRINTSCREEN
        * \type integer
        * \desc Print Screen key.
        */
		CONST_KEY(PRINTSCREEN);
		/***
        * \enum Key_SCROLLLOCK
        * \type integer
        * \desc Scroll Lock key.
        */
		CONST_KEY(SCROLLLOCK);
		/***
        * \enum Key_PAUSE
        * \type integer
        * \desc Pause/Break key.
        */
		CONST_KEY(PAUSE);
		/***
        * \enum Key_INSERT
        * \type integer
        * \desc Insert key.
        */
		CONST_KEY(INSERT);
		/***
        * \enum Key_HOME
        * \type integer
        * \desc Home key.
        */
		CONST_KEY(HOME);
		/***
        * \enum Key_PAGEUP
        * \type integer
        * \desc Page Up key.
        */
		CONST_KEY(PAGEUP);
		/***
        * \enum Key_DELETE
        * \type integer
        * \desc Delete key.
        */
		CONST_KEY(DELETE);
		/***
        * \enum Key_END
        * \type integer
        * \desc End key.
        */
		CONST_KEY(END);
		/***
        * \enum Key_PAGEDOWN
        * \type integer
        * \desc Page Down key.
        */
		CONST_KEY(PAGEDOWN);
		/***
        * \enum Key_RIGHT
        * \type integer
        * \desc Arrow Right key.
        */
		CONST_KEY(RIGHT);
		/***
        * \enum Key_LEFT
        * \type integer
        * \desc Arrow Left key.
        */
		CONST_KEY(LEFT);
		/***
        * \enum Key_DOWN
        * \type integer
        * \desc Arrow Down key.
        */
		CONST_KEY(DOWN);
		/***
        * \enum Key_UP
        * \type integer
        * \desc Arrow Up key.
        */
		CONST_KEY(UP);

		/***
        * \enum Key_NUMLOCKCLEAR
        * \type integer
        * \desc Num Clear key.
        */
		CONST_KEY(NUMLOCKCLEAR);
		/***
        * \enum Key_KP_DIVIDE
        * \type integer
        * \desc Keypad Divide key.
        */
		CONST_KEY(KP_DIVIDE);
		/***
        * \enum Key_KP_MULTIPLY
        * \type integer
        * \desc Keypad Multiply key.
        */
		CONST_KEY(KP_MULTIPLY);
		/***
        * \enum Key_KP_MINUS
        * \type integer
        * \desc Keypad Minus key.
        */
		CONST_KEY(KP_MINUS);
		/***
        * \enum Key_KP_PLUS
        * \type integer
        * \desc Keypad Plus key.
        */
		CONST_KEY(KP_PLUS);
		/***
        * \enum Key_KP_ENTER
        * \type integer
        * \desc Keypad Enter key.
        */
		CONST_KEY(KP_ENTER);
		/***
        * \enum Key_KP_1
        * \type integer
        * \desc Keypad 1 key.
        */
		CONST_KEY(KP_1);
		/***
        * \enum Key_KP_2
        * \type integer
        * \desc Keypad 2 key.
        */
		CONST_KEY(KP_2);
		/***
        * \enum Key_KP_3
        * \type integer
        * \desc Keypad 3 key.
        */
		CONST_KEY(KP_3);
		/***
        * \enum Key_KP_4
        * \type integer
        * \desc Keypad 4 key.
        */
		CONST_KEY(KP_4);
		/***
        * \enum Key_KP_5
        * \type integer
        * \desc Keypad 5 key.
        */
		CONST_KEY(KP_5);
		/***
        * \enum Key_KP_6
        * \type integer
        * \desc Keypad 6 key.
        */
		CONST_KEY(KP_6);
		/***
        * \enum Key_KP_7
        * \type integer
        * \desc Keypad 7 key.
        */
		CONST_KEY(KP_7);
		/***
        * \enum Key_KP_8
        * \type integer
        * \desc Keypad 8 key.
        */
		CONST_KEY(KP_8);
		/***
        * \enum Key_KP_9
        * \type integer
        * \desc Keypad 9 key.
        */
		CONST_KEY(KP_9);
		/***
        * \enum Key_KP_0
        * \type integer
        * \desc Keypad 0 key.
        */
		CONST_KEY(KP_0);
		/***
        * \enum Key_KP_PERIOD
        * \type integer
        * \desc Keypad Period key.
        */
		CONST_KEY(KP_PERIOD);

		/***
        * \enum Key_LCTRL
        * \type integer
        * \desc Left Ctrl key.
        */
		CONST_KEY(LCTRL);
		/***
        * \enum Key_LSHIFT
        * \type integer
        * \desc Left Shift key.
        */
		CONST_KEY(LSHIFT);
		/***
        * \enum Key_LALT
        * \type integer
        * \desc Left Alt key.
        */
		CONST_KEY(LALT);
		/***
        * \enum Key_LGUI
        * \type integer
        * \desc Left GUI key.
        */
		CONST_KEY(LGUI);
		/***
        * \enum Key_RCTRL
        * \type integer
        * \desc Right Ctrl key.
        */
		CONST_KEY(RCTRL);
		/***
        * \enum Key_RSHIFT
        * \type integer
        * \desc Right Shift key.
        */
		CONST_KEY(RSHIFT);
		/***
        * \enum Key_RALT
        * \type integer
        * \desc Right Alt key.
        */
		CONST_KEY(RALT);
		/***
        * \enum Key_RGUI
        * \type integer
        * \desc Right GUI key.
        */
		CONST_KEY(RGUI);
	}
#undef CONST_KEY
#undef DEF_CONST_INT
#undef DEF_LINK_INT
#undef DEF_CONST_DECIMAL
#undef DEF_LINK_DECIMAL
#undef DEF_ENUM
#undef DEF_ENUM_CLASS
#undef DEF_ENUM_NAMED
}
void StandardLibrary::Dispose() {}
