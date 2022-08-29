#if INTERFACE
#include <Engine/Bytecode/Types.h>
#include <Engine/ResourceTypes/ISprite.h>

class StandardLibrary {
public:

};
#endif

#include <Engine/Bytecode/StandardLibrary.h>

#include <Engine/FontFace.h>
#include <Engine/Graphics.h>
#include <Engine/Scene.h>
#include <Engine/Audio/AudioManager.h>
#include <Engine/ResourceTypes/ISound.h>
#include <Engine/Bytecode/BytecodeObject.h>
#include <Engine/Bytecode/BytecodeObjectManager.h>
#include <Engine/Bytecode/Compiler.h>
#include <Engine/Diagnostics/Clock.h>
#include <Engine/Filesystem/File.h>
#include <Engine/Filesystem/Directory.h>
#include <Engine/Hashing/CombinedHash.h>
#include <Engine/Hashing/CRC32.h>
#include <Engine/Hashing/FNV1A.h>
#include <Engine/IO/FileStream.h>
#include <Engine/IO/MemoryStream.h>
#include <Engine/IO/ResourceStream.h>
#include <Engine/Math/Ease.h>
#include <Engine/Math/Math.h>
#include <Engine/Network/HTTP.h>
#include <Engine/Network/WebSocketClient.h>
#include <Engine/Rendering/Software/SoftwareRenderer.h>
#include <Engine/ResourceTypes/ImageFormats/GIF.h>
#include <Engine/ResourceTypes/SceneFormats/RSDKSceneReader.h>
#include <Engine/ResourceTypes/ResourceType.h>
#include <Engine/TextFormats/JSON/jsmn.h>
#include <Engine/Utilities/StringUtils.h>

#ifdef USING_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#define CHECK_ARGCOUNT(expects) \
    if (argCount != expects) { \
        if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Expected %d arguments but got %d.", expects, argCount) == ERROR_RES_CONTINUE) \
            return NULL_VAL; \
        return NULL_VAL; \
    }
#define CHECK_AT_LEAST_ARGCOUNT(expects) \
    if (argCount < expects) { \
        if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Expected at least %d arguments but got %d.", expects, argCount) == ERROR_RES_CONTINUE) \
            return NULL_VAL; \
        return NULL_VAL; \
    }
#define GET_ARG(argIndex, argFunction) (argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) (argIndex < argCount ? GET_ARG(argIndex, argFunction) : argDefault)

// Get([0-9A-Za-z]+)\(([0-9A-Za-z]+), ([0-9A-Za-z]+)\)
// Get$1($2, $3, threadID)

namespace LOCAL {
    inline int             GetInteger(VMValue* args, int index, Uint32 threadID) {
        int value = 0;
        switch (args[index].Type) {
            case VAL_INTEGER:
            case VAL_LINKED_INTEGER:
                value = AS_INTEGER(args[index]);
                break;
            default:
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Integer", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline float           GetDecimal(VMValue* args, int index, Uint32 threadID) {
        float value = 0.0f;
        switch (args[index].Type) {
            case VAL_DECIMAL:
            case VAL_LINKED_DECIMAL:
                value = AS_DECIMAL(args[index]);
                break;
            case VAL_INTEGER:
            case VAL_LINKED_INTEGER:
                value = AS_DECIMAL(BytecodeObjectManager::CastValueAsDecimal(args[index]));
                break;
            default:
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Decimal", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline char*           GetString(VMValue* args, int index, Uint32 threadID) {
        char* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_STRING(args[index])) {
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "String", GetTypeString(args[index])) == ERROR_RES_CONTINUE) {
                    BytecodeObjectManager::Unlock();
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();
                }
            }

            value = AS_CSTRING(args[index]);
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "String"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline ObjArray*       GetArray(VMValue* args, int index, Uint32 threadID) {
        ObjArray* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_ARRAY(args[index])) {
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Array", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();
            }

            value = (ObjArray*)(AS_OBJECT(args[index]));
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "Array"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline ObjMap*         GetMap(VMValue* args, int index, Uint32 threadID) {
        ObjMap* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_MAP(args[index]))
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Map", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();

            value = (ObjMap*)(AS_OBJECT(args[index]));
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "Map"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline ObjBoundMethod* GetBoundMethod(VMValue* args, int index, Uint32 threadID) {
        ObjBoundMethod* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_BOUND_METHOD(args[index]))
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Event", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();

            value = (ObjBoundMethod*)(AS_OBJECT(args[index]));
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "Event"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline ObjFunction*    GetFunction(VMValue* args, int index, Uint32 threadID) {
        ObjFunction* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_FUNCTION(args[index]))
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Event", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();

            value = (ObjFunction*)(AS_OBJECT(args[index]));
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "Event"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }
    inline ObjInstance*    GetInstance(VMValue* args, int index, Uint32 threadID) {
        ObjInstance* value = NULL;
        if (BytecodeObjectManager::Lock()) {
            if (!IS_INSTANCE(args[index]))
                if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                    "Expected argument %d to be of type %s instead of %s.", index + 1, "Instance", GetTypeString(args[index])) == ERROR_RES_CONTINUE)
                    BytecodeObjectManager::Threads[threadID].ReturnFromNative();

            value = (ObjInstance*)(AS_OBJECT(args[index]));
            BytecodeObjectManager::Unlock();
        }
        if (!value) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Argument %d could not be read as type %s.", index + 1,
                "Instance"))
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }
        return value;
    }

    inline ISprite*        GetSprite(VMValue* args, int index, Uint32 threadID) {
        int where = GetInteger(args, index, threadID);
        if (where < 0 || where > (int)Scene::SpriteList.size()) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                "Sprite index \"%d\" outside bounds of list.", where) == ERROR_RES_CONTINUE)
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }

        if (!Scene::SpriteList[where]) return NULL;

        return Scene::SpriteList[where]->AsSprite;
    }
    inline IModel*         GetModel(VMValue* args, int index, Uint32 threadID) {
        int where = GetInteger(args, index, threadID);
        if (where < 0 || where > (int)Scene::ModelList.size()) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                "Model index \"%d\" outside bounds of list.", where) == ERROR_RES_CONTINUE)
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }

        if (!Scene::ModelList[where]) return NULL;

        return Scene::ModelList[where]->AsModel;
    }
    inline MediaBag*       GetVideo(VMValue* args, int index, Uint32 threadID) {
        int where = GetInteger(args, index, threadID);
        if (where < 0 || where > (int)Scene::MediaList.size()) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                "Video index \"%d\" outside bounds of list.", where) == ERROR_RES_CONTINUE)
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }

        if (!Scene::MediaList[where]) return NULL;

        return Scene::MediaList[where]->AsMedia;
    }
}

// NOTE:
// Integers specifically need to be whole integers.
// Floats can be just any countable real number.
PUBLIC STATIC int       StandardLibrary::GetInteger(VMValue* args, int index) {
    Uint32 threadID = 0;
    return LOCAL::GetInteger(args, index, threadID);
}
PUBLIC STATIC float     StandardLibrary::GetDecimal(VMValue* args, int index) {
    Uint32 threadID = 0;
    return LOCAL::GetDecimal(args, index, threadID);
}
PUBLIC STATIC char*     StandardLibrary::GetString(VMValue* args, int index) {
    Uint32 threadID = 0;
    return LOCAL::GetString(args, index, threadID);
}
PUBLIC STATIC ObjArray* StandardLibrary::GetArray(VMValue* args, int index) {
    Uint32 threadID = 0;
    return LOCAL::GetArray(args, index, threadID);
}
PUBLIC STATIC ISprite*  StandardLibrary::GetSprite(VMValue* args, int index) {
    Uint32 threadID = 0;
    return LOCAL::GetSprite(args, index, threadID);
}

PUBLIC STATIC void      StandardLibrary::CheckArgCount(int argCount, int expects) {
    Uint32 threadID = 0;
    if (argCount != expects) {
        if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Expected %d arguments but got %d.", expects, argCount) == ERROR_RES_CONTINUE)
            BytecodeObjectManager::Threads[threadID].ReturnFromNative();
    }
}
PUBLIC STATIC void      StandardLibrary::CheckAtLeastArgCount(int argCount, int expects) {
    Uint32 threadID = 0;
    if (argCount < expects) {
        if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Expected at least %d arguments but got %d.", expects, argCount) == ERROR_RES_CONTINUE)
            BytecodeObjectManager::Threads[threadID].ReturnFromNative();
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

// #region Audio
/***
 * Audio.GetMasterVolume
 * \desc Gets the master volume of the audio mixer.
 * \return The master volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetMasterVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Application::MasterVolume);
}
/***
 * Audio.GetMusicVolume
 * \desc Gets the music volume of the audio mixer.
 * \return The music volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetMusicVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Application::MusicVolume);
}
/***
 * Audio.GetSoundVolume
 * \desc Gets the sound effect volume of the audio mixer.
 * \return The sound effect volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_GetSoundVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Application::SoundVolume);
}
/***
 * Audio.SetMasterVolume
 * \desc Sets the master volume of the audio mixer.
 * \param volume (Integer): The master volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetMasterVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int volume = GET_ARG(0, GetInteger);
    if (volume < 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be lower than zero.");
    } else if (volume > 100) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be higher than 100.");
    } else
        Application::SetMasterVolume(volume);

    return NULL_VAL;
}
/***
 * Audio.SetMusicVolume
 * \desc Sets the music volume of the audio mixer.
 * \param volume (Integer): The music volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetMusicVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int volume = GET_ARG(0, GetInteger);
    if (volume < 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be lower than zero.");
    } else if (volume > 100) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be higher than 100.");
    } else
        Application::SetMusicVolume(volume);

    return NULL_VAL;
}
/***
 * Audio.SetSoundVolume
 * \desc Sets the sound effect volume of the audio mixer.
 * \param volume (Integer): The sound effect volume, from 0 to 100.
 * \ns Audio
 */
VMValue Audio_SetSoundVolume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int volume = GET_ARG(0, GetInteger);
    if (volume < 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be lower than zero.");
    } else if (volume > 100) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Volume cannot be higher than 100.");
    } else
        Application::SetSoundVolume(volume);

    return NULL_VAL;
}
// #endregion

// #region Array
/***
 * Array.Create
 * \desc Creates an array.
 * \param size (Integer): Size of the array.
 * \paramOpt initialValue (Value): Initial value to set the array elements to.
 * \return A reference value to the array.
 * \ns Array
 */
VMValue Array_Create(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(1);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = NewArray();
        int       length = GET_ARG(0, GetInteger);
        VMValue   initialValue = NULL_VAL;
        if (argCount == 2) {
            initialValue = args[1];
        }

        for (int i = 0; i < length; i++) {
            array->Values->push_back(initialValue);
        }

        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(array);
    }
    return NULL_VAL;
}
/***
 * Array.Length
 * \desc Gets the length of an array.
 * \param array (Array): Array to get the length of.
 * \return Length of the array.
 * \ns Array
 */
VMValue Array_Length(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        int size = (int)array->Values->size();
        BytecodeObjectManager::Unlock();
        return INTEGER_VAL(size);
    }
    return INTEGER_VAL(0);
}
/***
 * Array.Push
 * \desc Adds a value to the end of an array.
 * \param array (Array): Array to get the length of.
 * \param value (Value): Value to add to the array.
 * \ns Array
 */
VMValue Array_Push(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        array->Values->push_back(args[1]);
        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
/***
 * Array.Pop
 * \desc Gets the value at the end of an array, and removes it.
 * \param array (Array): Array to get the length of.
 * \return The value from the end of the array.
 * \ns Array
 */
VMValue Array_Pop(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        VMValue   value = array->Values->back();
        array->Values->pop_back();
        BytecodeObjectManager::Unlock();
        return value;
    }
    return NULL_VAL;
}
/***
 * Array.Insert
 * \desc Inserts a value at an index of an array.
 * \param array (Array): Array to insert value.
 * \param index (Integer): Index to insert value.
 * \param value (Value): Value to insert.
 * \ns Array
 */
VMValue Array_Insert(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        int       index = GET_ARG(1, GetInteger);
        array->Values->insert(array->Values->begin() + index, args[2]);
        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
/***
 * Array.Erase
 * \desc Erases a value at an index of an array.
 * \param array (Array): Array to erase value.
 * \param index (Integer): Index to erase value.
 * \ns Array
 */
VMValue Array_Erase(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        int       index = GET_ARG(1, GetInteger);
        array->Values->erase(array->Values->begin() + index);
        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
/***
 * Array.Clear
 * \desc Clears an array.
 * \param array (Array): Array to clear.
 * \ns Array
 */
VMValue Array_Clear(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        array->Values->clear();
        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
/***
 * Array.Shift
 * \desc Rotates the array in the desired direction.
 * \param array (Array): Array to shift.
 * \param toRight (Boolean): Whether to rotate the array to the right or not (left.)
 * \ns Array
 */
VMValue Array_Shift(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
        int       toright = GET_ARG(1, GetInteger);

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

        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
/***
 * Array.SetAll
 * \desc Sets values in the array from startIndex to endIndex (includes the value at endIndex.)
 * \param array (Array): Array to set values to.
 * \param startIndex (Integer): Index of value to start setting. (-1 for first index)
 * \param endIndex (Integer): Index of value to end setting. (-1 for last index)
 * \param value (Value): Value to set to.
 * \ns Array
 */
VMValue Array_SetAll(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = GET_ARG(0, GetArray);
		size_t    startIndex = GET_ARG(1, GetInteger);
		size_t    endIndex = GET_ARG(2, GetInteger);
        VMValue   value = args[3];

		size_t arraySize = array->Values->size();
        if (arraySize > 0) {
            if (startIndex < 0)
                startIndex = 0;
            else if (startIndex >= arraySize)
                startIndex = arraySize - 1;

            if (endIndex < 0)
                endIndex = arraySize - 1;
            else if (endIndex >= arraySize)
                endIndex = arraySize - 1;

            for (size_t i = startIndex; i <= endIndex; i++) {
                (*array->Values)[i] = value;
            }
        }

        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
// #endregion

// #region Date
/***
 * Date.GetEpoch
 * \desc Gets the amount of seconds from 1 January 1970, 0:00 UTC.
 * \return The amount of seconds from epoch.
 * \ns Date
 */
VMValue Date_GetEpoch(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL((int)time(NULL));
}
/***
 * Date.GetTicks
 * \desc Gets the milliseconds since the application began running.
 * \return Returns milliseconds since the application began running.
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
 * \desc Gets the id of the platform the application is currently running on. <br/></br>Platform IDs:<ul><li>Unknown = 0</li><li>Windows = 1</li><li>MacOS = 2</li><li>Linux = 3</li><li>Switch = 5</li><li>Android = 8</li><li>iOS = 9</li></ul>
 * \return Returns ID of the current platform.
 * \ns Device
 */
VMValue Device_GetPlatform(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL((int)Application::Platform);
}
/***
 * Device.IsMobile
 * \desc Determines whether or not the application is running on a mobile device.
 * \return 1 if the device is on a mobile device, 0 if otherwise.
 * \ns Device
 */
VMValue Device_IsMobile(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    bool isMobile =
        Application::Platform == Platforms::iOS ||
        Application::Platform == Platforms::Android;
    return INTEGER_VAL((int)isMobile);
}
// #endregion

// #region Directory
/***
 * Directory.Create
 * \desc Creates a folder at the path.
 * \param path (String): The path of the folder to create.
 * \return Returns 1 if the folder creation was successful, 0 if otherwise
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
 * \param path (String): The path of the folder to check for existence.
 * \return Returns 1 if the folder exists, 0 if otherwise
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
 * \param directory (String): The path of the folder to find files in.
 * \param pattern (String): The search pattern for the files. (ex: "*" for any file, "*.*" any file name with any file type, "*.png" any PNG file)
 * \param allDirs (Boolean): Whether or not to search into all folders in the directory.
 * \return Returns an Array containing the filepaths (as Strings.)
 * \ns Directory
 */
VMValue Directory_GetFiles(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    ObjArray* array     = NULL;
    char*     directory = GET_ARG(0, GetString);
    char*     pattern   = GET_ARG(1, GetString);
    int       allDirs   = GET_ARG(2, GetInteger);

    vector<char*> fileList;
    Directory::GetFiles(&fileList, directory, pattern, allDirs);

    if (BytecodeObjectManager::Lock()) {
        array = NewArray();
        for (size_t i = 0; i < fileList.size(); i++) {
            ObjString* part = CopyString(fileList[i], strlen(fileList[i]));
            array->Values->push_back(OBJECT_VAL(part));
            free(fileList[i]);
        }
        BytecodeObjectManager::Unlock();
    }

    return OBJECT_VAL(array);
}
/***
 * Directory.GetDirectories
 * \desc Gets the paths of all the folders in the directory.
 * \param directory (String): The path of the folder to find folders in.
 * \param pattern (String): The search pattern for the folders. (ex: "*" for any folder, "image*" any folder that starts with "image")
 * \param allDirs (Boolean): Whether or not to search into all folders in the directory.
 * \return Returns an Array containing the filepaths (as Strings.)
 * \ns Directory
 */
VMValue Directory_GetDirectories(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    ObjArray* array     = NULL;
    char*     directory = GET_ARG(0, GetString);
    char*     pattern   = GET_ARG(1, GetString);
    int       allDirs   = GET_ARG(2, GetInteger);

    vector<char*> fileList;
    Directory::GetDirectories(&fileList, directory, pattern, allDirs);

    if (BytecodeObjectManager::Lock()) {
        array = NewArray();
        for (size_t i = 0; i < fileList.size(); i++) {
            ObjString* part = CopyString(fileList[i], strlen(fileList[i]));
            array->Values->push_back(OBJECT_VAL(part));
            free(fileList[i]);
        }
        BytecodeObjectManager::Unlock();
    }

    return OBJECT_VAL(array);
}
// #endregion

// #region Display
/***
 * Display.GetWidth
 * \desc Gets the width of the current display.
 * \paramOpt index (Integer): The display index to get the width of.
 * \return Returns the width of the current display.
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
 * \paramOpt index (Integer): The display index to get the width of.
 * \return Returns the height of the current display.
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
 * \param sprite (Integer): Index of the loaded sprite.
 * \param animation (Integer): Index of the animation entry.
 * \param frame (Integer): Index of the frame in the animation entry.
 * \param x (Number): X position of where to draw the sprite.
 * \param y (Number): Y position of where to draw the sprite.
 * \param flipX (Integer): Whether or not to flip the sprite horizontally.
 * \param flipY (Integer): Whether or not to flip the sprite vertically.
 * \paramOpt scaleX (Number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (Number): Scale multiplier of the sprite vertically.
 * \paramOpt rotation (Number): Rotation of the drawn sprite in radians.
 * \ns Draw
 */
VMValue Draw_Sprite(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(7);

    ISprite* sprite = GET_ARG(0, GetSprite);
    int animation = GET_ARG(1, GetInteger);
    int frame = GET_ARG(2, GetInteger);
    int x = (int)GET_ARG(3, GetDecimal);
    int y = (int)GET_ARG(4, GetDecimal);
    int flipX = GET_ARG(5, GetInteger);
    int flipY = GET_ARG(6, GetInteger);
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    if (argCount > 7)
        scaleX = GET_ARG(7, GetDecimal);
    if (argCount > 8)
        scaleY = GET_ARG(8, GetDecimal);
    if (argCount > 9)
        rotation = GET_ARG(9, GetDecimal);

    // Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
    Graphics::DrawSprite(sprite, animation, frame, x, y, flipX, flipY, scaleX, scaleY, rotation);
    return NULL_VAL;
}
/***
 * Draw.SpritePart
 * \desc Draws part of a sprite.
 * \param sprite (Integer): Index of the loaded sprite.
 * \param animation (Integer): Index of the animation entry.
 * \param frame (Integer): Index of the frame in the animation entry.
 * \param x (Number): X position of where to draw the sprite.
 * \param y (Number): Y position of where to draw the sprite.
 * \param partX (Integer): X coordinate of part of frame to draw.
 * \param partY (Integer): Y coordinate of part of frame to draw.
 * \param partW (Integer): Width of part of frame to draw.
 * \param partH (Integer): Height of part of frame to draw.
 * \param flipX (Integer): Whether or not to flip the sprite horizontally.
 * \param flipY (Integer): Whether or not to flip the sprite vertically.
 * \paramOpt scaleX (Number): Scale multiplier of the sprite horizontally.
 * \paramOpt scaleY (Number): Scale multiplier of the sprite vertically.
 * \paramOpt rotation (Number): Rotation of the drawn sprite in radians.
 * \ns Draw
 */
VMValue Draw_SpritePart(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(11);

    ISprite* sprite = GET_ARG(0, GetSprite);
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
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    if (argCount > 11)
        scaleX = GET_ARG(11, GetDecimal);
    if (argCount > 12)
        scaleY = GET_ARG(12, GetDecimal);
    if (argCount > 13)
        rotation = GET_ARG(13, GetDecimal);

    // Graphics::SetBlendColor(1.0, 1.0, 1.0, 1.0);
    Graphics::DrawSpritePart(sprite, animation, frame, sx, sy, sw, sh, x, y, flipX, flipY, scaleX, scaleY, rotation);
    return NULL_VAL;
}
/***
 * Draw.Image
 * \desc Draws an image.
 * \param image (Integer): Index of the loaded image.
 * \param x (Number): X position of where to draw the image.
 * \param y (Number): Y position of where to draw the image.
 * \ns Draw
 */
VMValue Draw_Image(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

	int index = GET_ARG(0, GetInteger);
	if (index < 0)
		return NULL_VAL;

    Image* image = Scene::ImageList[index]->AsImage;
    float x = GET_ARG(1, GetDecimal);
    float y = GET_ARG(2, GetDecimal);

    Graphics::DrawTexture(image->TexturePtr, 0, 0, image->TexturePtr->Width, image->TexturePtr->Height, x, y, image->TexturePtr->Width, image->TexturePtr->Height);
    return NULL_VAL;
}
/***
 * Draw.ImagePart
 * \desc Draws part of an image.
 * \param image (Integer): Index of the loaded image.
 * \param partX (Integer): X coordinate of part of image to draw.
 * \param partY (Integer): Y coordinate of part of image to draw.
 * \param partW (Integer): Width of part of image to draw.
 * \param partH (Integer): Height of part of image to draw.
 * \param x (Number): X position of where to draw the image.
 * \param y (Number): Y position of where to draw the image.
 * \ns Draw
 */
VMValue Draw_ImagePart(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(7);

	int index = GET_ARG(0, GetInteger);
	if (index < 0)
		return NULL_VAL;

	Image* image = Scene::ImageList[index]->AsImage;
    float sx = GET_ARG(1, GetDecimal);
    float sy = GET_ARG(2, GetDecimal);
    float sw = GET_ARG(3, GetDecimal);
    float sh = GET_ARG(4, GetDecimal);
    float x = GET_ARG(5, GetDecimal);
    float y = GET_ARG(6, GetDecimal);

    Graphics::DrawTexture(image->TexturePtr, sx, sy, sw, sh, x, y, sw, sh);
    return NULL_VAL;
}
/***
 * Draw.ImageSized
 * \desc Draws an image, but sized.
 * \param x (Number): X position of where to draw the image.
 * \param y (Number): Y position of where to draw the image.
 * \param width (Number): Width to draw the image.
 * \param height (Number): Height to draw the image.
 * \ns Draw
 */
VMValue Draw_ImageSized(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);

	int index = GET_ARG(0, GetInteger);
	if (index < 0)
		return NULL_VAL;

	Image* image = Scene::ImageList[index]->AsImage;
    float x = GET_ARG(1, GetDecimal);
    float y = GET_ARG(2, GetDecimal);
    float w = GET_ARG(3, GetDecimal);
    float h = GET_ARG(4, GetDecimal);

    Graphics::DrawTexture(image->TexturePtr, 0, 0, image->TexturePtr->Width, image->TexturePtr->Height, x, y, w, h);
    return NULL_VAL;
}
/***
 * Draw.ImagePartSized
 * \desc Draws part of an image, but sized.
 * \param image (Integer): Index of the loaded image.
 * \param partX (Integer): X coordinate of part of image to draw.
 * \param partY (Integer): Y coordinate of part of image to draw.
 * \param partW (Integer): Width of part of image to draw.
 * \param partH (Integer): Height of part of image to draw.
 * \param x (Number): X position of where to draw the image.
 * \param y (Number): Y position of where to draw the image.
 * \param width (Number): Width to draw the image.
 * \param height (Number): Height to draw the image.
 * \ns Draw
 */
VMValue Draw_ImagePartSized(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(9);

	int index = GET_ARG(0, GetInteger);
	if (index < 0)
		return NULL_VAL;

	Image* image = Scene::ImageList[index]->AsImage;
    float sx = GET_ARG(1, GetDecimal);
    float sy = GET_ARG(2, GetDecimal);
    float sw = GET_ARG(3, GetDecimal);
    float sh = GET_ARG(4, GetDecimal);
    float x = GET_ARG(5, GetDecimal);
    float y = GET_ARG(6, GetDecimal);
    float w = GET_ARG(7, GetDecimal);
    float h = GET_ARG(8, GetDecimal);

    Graphics::DrawTexture(image->TexturePtr, sx, sy, sw, sh, x, y, w, h);
    return NULL_VAL;
}
/***
 * Draw.InitArrayBuffer
 * \desc Initializes an array buffer. There are 32 array buffers.
 * \param arrayBufferIndex (Integer): The array buffer at the index to use. (Maximum index: 31)
 * \param maxVertices (Integer): The maximum vertices that this array buffer will hold.
 * \return
 * \ns Draw
 */
VMValue Draw_InitArrayBuffer(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    Uint32 maxVertices = GET_ARG(1, GetInteger);
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return NULL_VAL;

    SoftwareRenderer::CurrentArrayBuffer = arrayBufferIndex;
    SoftwareRenderer::ArrayBuffer_Init(SoftwareRenderer::CurrentArrayBuffer, maxVertices);
    return NULL_VAL;
}
/***
 * Draw.SetAmbientLighting
 * \desc Sets the ambient lighting of the array buffer.
 * \param arrayBufferIndex (Integer): The index of the array buffer.
 * \param red (Number): The red color value, bounded by 0.0 - 1.0.
 * \param green (Number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (Number): The blue color value, bounded by 0.0 - 1.0.
 * \return
 * \ns Draw
 */
VMValue Draw_SetAmbientLighting(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    Uint32 r = (Uint32)(GET_ARG(1, GetDecimal) * 0x100);
    Uint32 g = (Uint32)(GET_ARG(2, GetDecimal) * 0x100);
    Uint32 b = (Uint32)(GET_ARG(3, GetDecimal) * 0x100);
    SoftwareRenderer::ArrayBuffer_SetAmbientLighting(arrayBufferIndex, r, g, b);
    return NULL_VAL;
}
/***
 * Draw.SetDiffuseLighting
 * \desc Sets the diffuse lighting of the array buffer.
 * \param arrayBufferIndex (Integer): The index of the array buffer.
 * \param red (Number): The red color value, bounded by 0.0 - 1.0.
 * \param green (Number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (Number): The blue color value, bounded by 0.0 - 1.0.
 * \return
 * \ns Draw
 */
VMValue Draw_SetDiffuseLighting(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    int r = Math::CeilPOT((int)(GET_ARG(1, GetDecimal) * 0x100));
    int g = Math::CeilPOT((int)(GET_ARG(2, GetDecimal) * 0x100));
    int b = Math::CeilPOT((int)(GET_ARG(3, GetDecimal) * 0x100));

    int v;

    v = 0;
    while (r) { r >>= 1; v++; }
    r = --v;

    v = 0;
    while (g) { g >>= 1; v++; }
    g = --v;

    v = 0;
    while (b) { b >>= 1; v++; }
    b = --v;

    SoftwareRenderer::ArrayBuffer_SetDiffuseLighting(arrayBufferIndex, (Uint32)r, (Uint32)g, (Uint32)b);
    return NULL_VAL;
}
/***
 * Draw.SetSpecularLighting
 * \desc Sets the specular lighting of the array buffer.
 * \param arrayBufferIndex (Integer): The index of the array buffer.
 * \param red (Number): The red color value, bounded by 0.0 - 1.0.
 * \param green (Number): The green color value, bounded by 0.0 - 1.0.
 * \param blue (Number): The blue color value, bounded by 0.0 - 1.0.
 * \return
 * \ns Draw
 */
VMValue Draw_SetSpecularLighting(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    int r = Math::CeilPOT((int)(GET_ARG(1, GetDecimal) * 0x10000));
    int g = Math::CeilPOT((int)(GET_ARG(2, GetDecimal) * 0x10000));
    int b = Math::CeilPOT((int)(GET_ARG(3, GetDecimal) * 0x10000));

    int v;

    v = 0;
    while (r) { r >>= 1; v++; }
    r = --v;

    v = 0;
    while (g) { g >>= 1; v++; }
    g = --v;

    v = 0;
    while (b) { b >>= 1; v++; }
    b = --v;

    SoftwareRenderer::ArrayBuffer_SetSpecularLighting(arrayBufferIndex, r, g, b);
    return NULL_VAL;
}
/***
 * Draw.BindArrayBuffer
 * \desc Binds an array buffer for drawing models.
 * \param arrayBufferIndex (Integer): Sets the array buffer to bind.
 * \return
 * \ns Draw
 */
VMValue Draw_BindArrayBuffer(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    if (arrayBufferIndex >= MAX_ARRAY_BUFFERS)
        return NULL_VAL;

    SoftwareRenderer::CurrentArrayBuffer = arrayBufferIndex;
    SoftwareRenderer::ArrayBuffer_DrawBegin(arrayBufferIndex);
    return NULL_VAL;
}
/***
 * Draw.Model
 * \desc Draws a model.
 * \param modelIndex (Integer): Index of loaded model.
 * \param frame (Integer): Frame of model to draw.
 * \param matrixView (Matrix): Matrix for transforming model coordinates to screen space.
 * \param matrixNormal (Matrix): Matrix for transforming model normals.
 * \return
 * \ns Draw
 */
VMValue Draw_Model(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    IModel* model = GET_ARG(0, GetModel);
    int frame = GET_ARG(1, GetInteger);

    ObjArray* matrixViewArr = NULL;
    if (!IS_NULL(args[2]))
        matrixViewArr = GET_ARG(2, GetArray);

    if (!matrixViewArr)
        return NULL_VAL;

    ObjArray* matrixNormalArr = NULL;
    if (!IS_NULL(args[3]))
        matrixNormalArr = GET_ARG(3, GetArray);

    MatrixHelper helperV;
    MatrixHelper_CopyFrom(&helperV, matrixViewArr);

    if (matrixNormalArr) {
        MatrixHelper helperN;
        MatrixHelper_CopyFrom(&helperN, matrixNormalArr);

        Matrix4x4i matrixView;
        Matrix4x4i matrixNormal;
        for (int i = 0; i < 16; i++) {
            int x = i  & 3;
            int y = i >> 2;
            matrixView.Column[x][y] = (int)(helperV[x][y] * 0x100);
            matrixNormal.Column[x][y] = (int)(helperN[x][y] * 0x100);
        }

        SoftwareRenderer::DrawModel(model, frame, &matrixView, &matrixNormal);
    }
    else {
        Matrix4x4i matrixView;
        for (int i = 0; i < 16; i++) {
            int x = i  & 3;
            int y = i >> 2;
            matrixView.Column[x][y] = (int)(helperV[x][y] * 0x100);
        }

        SoftwareRenderer::DrawModel(model, frame, &matrixView, NULL);
    }
    return NULL_VAL;
}
/***
 * Draw.ModelSimple
 * \desc Draws a model without using matrices.
 * \param modelIndex (Integer): Index of loaded model.
 * \param frame (Integer): Frame of model to draw.
 * \param x (Number): X position
 * \param y (Number): Y position
 * \param scale (Number): Model scale
 * \param rx (Number): X rotation in radians
 * \param ry (Number): Y rotation in radians
 * \param rz (Number): Z rotation in radians
 * \return
 * \ns Draw
 */
int  _Matrix4x4_CosBuffer[0x400];
int  _Matrix4x4_SinBuffer[0x400];
bool _Matrix4x4_TrigBufferMade = false;
void _Matrix4x4_EnsureTrigBuffer() {
    if (_Matrix4x4_TrigBufferMade) return;
    for (int i = 0; i < 0x400; i++) {
        _Matrix4x4_CosBuffer[i] = (int)(Math::Cos(i * M_PI / 0x200) * 0x100);
        _Matrix4x4_SinBuffer[i] = (int)(Math::Sin(i * M_PI / 0x200) * 0x100);
    }
    _Matrix4x4_TrigBufferMade = true;
}
void _Matrix4x4_Identity(Matrix4x4i* matrix) {
    matrix->Column[0][0] = 0x100;
    matrix->Column[1][0] = 0;
    matrix->Column[2][0] = 0;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = 0;
    matrix->Column[1][1] = 0x100;
    matrix->Column[2][1] = 0;
    matrix->Column[3][1] = 0;
    matrix->Column[0][2] = 0;
    matrix->Column[1][2] = 0;
    matrix->Column[2][2] = 0x100;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
void _Matrix4x4_Multiply(Matrix4x4i* out, Matrix4x4i* a, Matrix4x4i* b) {
    int b0, b1, b2, b3;
    int a00 = a->Column[0][0], a01 = a->Column[0][1], a02 = a->Column[0][2], a03 = a->Column[0][3];
    int a10 = a->Column[1][0], a11 = a->Column[1][1], a12 = a->Column[1][2], a13 = a->Column[1][3];
    int a20 = a->Column[2][0], a21 = a->Column[2][1], a22 = a->Column[2][2], a23 = a->Column[2][3];
    int a30 = a->Column[3][0], a31 = a->Column[3][1], a32 = a->Column[3][2], a33 = a->Column[3][3];

    // Cache only the current line of the second matrix
    b0 = b->Column[0][0]; b1 = b->Column[0][1]; b2 = b->Column[0][2]; b3 = b->Column[0][3];
    out->Column[0][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
    out->Column[0][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
    out->Column[0][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
    out->Column[0][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

    b0 = b->Column[1][0]; b1 = b->Column[1][1]; b2 = b->Column[1][2]; b3 = b->Column[1][3];
    out->Column[1][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
    out->Column[1][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
    out->Column[1][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
    out->Column[1][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

    b0 = b->Column[2][0]; b1 = b->Column[2][1]; b2 = b->Column[2][2]; b3 = b->Column[2][3];
    out->Column[2][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
    out->Column[2][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
    out->Column[2][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
    out->Column[2][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;

    b0 = b->Column[3][0]; b1 = b->Column[3][1]; b2 = b->Column[3][2]; b3 = b->Column[3][3];
    out->Column[3][0] = (b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30) >> 8;
    out->Column[3][1] = (b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31) >> 8;
    out->Column[3][2] = (b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32) >> 8;
    out->Column[3][3] = (b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33) >> 8;
}
void _Matrix4x4_Translate(Matrix4x4i* matrix, int x, int y, int z, bool resetToIdentity) {
    if (resetToIdentity) {
        matrix->Column[0][0] = 0x100;
        matrix->Column[1][0] = 0;
        matrix->Column[2][0] = 0;
        matrix->Column[3][0] = 0;

        matrix->Column[0][1] = 0;
        matrix->Column[1][1] = 0x100;
        matrix->Column[2][1] = 0;
        matrix->Column[3][1] = 0;

        matrix->Column[0][2] = 0;
        matrix->Column[1][2] = 0;
        matrix->Column[2][2] = 0x100;
        matrix->Column[3][2] = 0;

        matrix->Column[3][3] = 0x100;
    }

    matrix->Column[0][3] = x >> 8;
    matrix->Column[1][3] = y >> 8;
    matrix->Column[2][3] = z >> 8;
}
void _Matrix4x4_IdentityScale(Matrix4x4i* matrix, int x, int y, int z) {
    matrix->Column[0][0] = x;
    matrix->Column[1][0] = 0;
    matrix->Column[2][0] = 0;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = 0;
    matrix->Column[1][1] = y;
    matrix->Column[2][1] = 0;
    matrix->Column[3][1] = 0;
    matrix->Column[0][2] = 0;
    matrix->Column[1][2] = 0;
    matrix->Column[2][2] = z;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
void _Matrix4x4_IdentityRotationX(Matrix4x4i* matrix, int x) {
    x &= 0x3FF;
    int sin = _Matrix4x4_SinBuffer[x];
    int cos = _Matrix4x4_CosBuffer[x];
    matrix->Column[0][0] = 0x100;
    matrix->Column[1][0] = 0;
    matrix->Column[2][0] = 0;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = 0;
    matrix->Column[1][1] = cos;
    matrix->Column[2][1] = sin;
    matrix->Column[3][1] = 0;
    matrix->Column[0][2] = 0;
    matrix->Column[1][2] = -sin;
    matrix->Column[2][2] = cos;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
void _Matrix4x4_IdentityRotationY(Matrix4x4i* matrix, int y) {
    y &= 0x3FF;
    int sin = _Matrix4x4_SinBuffer[y];
    int cos = _Matrix4x4_CosBuffer[y];
    matrix->Column[0][0] = cos;
    matrix->Column[1][0] = 0;
    matrix->Column[2][0] = sin;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = 0;
    matrix->Column[1][1] = 0x100;
    matrix->Column[2][1] = 0;
    matrix->Column[3][1] = 0;
    matrix->Column[0][2] = -sin;
    matrix->Column[1][2] = 0;
    matrix->Column[2][2] = cos;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
void _Matrix4x4_IdentityRotationZ(Matrix4x4i* matrix, int z) {
    z &= 0x3FF;
    int sin = _Matrix4x4_SinBuffer[z];
    int cos = _Matrix4x4_CosBuffer[z];
    matrix->Column[0][0] = cos;
    matrix->Column[1][0] = -sin;
    matrix->Column[2][0] = 0;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = sin;
    matrix->Column[1][1] = cos;
    matrix->Column[2][1] = 0;
    matrix->Column[3][1] = 0;
    matrix->Column[0][2] = 0;
    matrix->Column[1][2] = 0;
    matrix->Column[2][2] = 0x100;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
void _Matrix4x4_IdentityRotationXYZ(Matrix4x4i* matrix, int x, int y, int z) {
    x &= 0x3FF;
    y &= 0x3FF;
    z &= 0x3FF;
    int sinX = _Matrix4x4_SinBuffer[x];
    int cosX = _Matrix4x4_CosBuffer[x];
    int sinY = _Matrix4x4_SinBuffer[y];
    int cosY = _Matrix4x4_CosBuffer[y];
    int sinZ = _Matrix4x4_SinBuffer[z];
    int cosZ = _Matrix4x4_CosBuffer[z];
    int sinXY = sinX * sinY >> 8;
    matrix->Column[0][0] = (cosY * cosZ >> 8) + (sinZ * sinXY >> 8);
    matrix->Column[1][0] = (cosY * sinZ >> 8) - (cosZ * sinXY >> 8);
    matrix->Column[2][0] = cosX * sinY >> 8;
    matrix->Column[3][0] = 0;
    matrix->Column[0][1] = -(cosX * sinZ) >> 8;
    matrix->Column[1][1] = cosX * cosZ >> 8;
    matrix->Column[2][1] = 0;
    matrix->Column[3][1] = 0;

    int sincosXY = sinX * cosY >> 8;
    matrix->Column[0][2] = (sinZ * sincosXY >> 8) - (sinY * cosZ >> 8);
    matrix->Column[1][2] = (-sinZ * sinY >> 8) - (cosZ * sincosXY >> 8);
    matrix->Column[2][2] = cosX * cosY >> 8;
    matrix->Column[3][2] = 0;
    matrix->Column[0][3] = 0;
    matrix->Column[1][3] = 0;
    matrix->Column[2][3] = 0;
    matrix->Column[3][3] = 0x100;
}
VMValue Draw_ModelSimple(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(8);

    IModel* model = GET_ARG(0, GetModel);
    int frame = GET_ARG(1, GetInteger);
    int x = (int)(GET_ARG(2, GetDecimal) * 0x10000);
    int y = (int)(GET_ARG(3, GetDecimal) * 0x10000);
    int scale = (int)(GET_ARG(4, GetDecimal) * 0x100);
    int rx = (int)(GET_ARG(5, GetDecimal) * 0x200 / M_PI);
    int ry = (int)(GET_ARG(6, GetDecimal) * 0x200 / M_PI);
    int rz = (int)(GET_ARG(7, GetDecimal) * 0x200 / M_PI);

    _Matrix4x4_EnsureTrigBuffer();

    Matrix4x4i matrixScaleTranslate;
    _Matrix4x4_IdentityScale(&matrixScaleTranslate, scale, scale, scale);
    _Matrix4x4_Translate(&matrixScaleTranslate, x, y, 0, false);

    Matrix4x4i matrixView;
    _Matrix4x4_IdentityRotationXYZ(&matrixView, 0, ry, rz);
    _Matrix4x4_Multiply(&matrixView, &matrixView, &matrixScaleTranslate);

    Matrix4x4i matrixRotationX;
    _Matrix4x4_IdentityRotationX(&matrixRotationX, rx);
    Matrix4x4i matrixNormal;
    _Matrix4x4_IdentityRotationXYZ(&matrixNormal, 0, ry, rz);
    _Matrix4x4_Multiply(&matrixNormal, &matrixNormal, &matrixRotationX);

    SoftwareRenderer::DrawModel(model, frame, &matrixView, &matrixNormal);
    return NULL_VAL;
}
/***
 * Draw.RenderArrayBuffer
 * \desc Draws everything in the array buffer with the specified draw mode. <br/>\
</br>Draw Modes:<ul>\
<li><code>DrawMode_LINES</code>: Draws the model faces with lines, using a solid color determined by the model's existing colors (and if not, the blend color.)</li>\
<li><code>DrawMode_LINES_FLAT</code>: Draws the model faces with lines, using a color for the face calculated with the vertex normals, the model's existing colors (and if not, the blend color.)</li>\
<li><code>DrawMode_LINES_SMOOTH</code>: Draws the model faces with lines, using a color smoothly spread across the face calculated with the vertex normals, the model's existing colors (and if not, the blend color.)</li>\
<li><code>DrawMode_POLYGONS</code>: Draws the model faces with polygons, using a solid color determined by the model's existing colors (and if not, the blend color.)</li>\
<li><code>DrawMode_POLYGONS_FLAT</code>: Draws the model faces with polygons, using a color for the face calculated with the vertex normals, the model's existing colors (and if not, the blend color.)</li>\
<li><code>DrawMode_POLYGONS_SMOOTH</code>: Draws the model faces with polygons, using a color smoothly spread across the face calculated with the vertex normals, the model's existing colors (and if not, the blend color.)</li>\
</ul>
 * \param arrayBufferIndex (Integer): The array buffer at the index to draw.
 * \param drawMode (Integer): The type of drawing to use for the vertices in the array buffer.
 * \return
 * \ns Draw
 */
VMValue Draw_RenderArrayBuffer(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    Uint32 arrayBufferIndex = GET_ARG(0, GetInteger);
    Uint32 drawMode = GET_ARG(1, GetInteger);
    SoftwareRenderer::ArrayBuffer_DrawFinish(arrayBufferIndex, drawMode);
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

    Graphics::DrawTexture(video->VideoTexture, 0, 0, video->VideoTexture->Width, video->VideoTexture->Height, x, y, video->VideoTexture->Width, video->VideoTexture->Height);
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

    Graphics::DrawTexture(video->VideoTexture, 0, 0, video->VideoTexture->Width, video->VideoTexture->Height, x, y, w, h);
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
 * \param ID (Integer): ID of the tile to draw.
 * \param x (Number): X position of where to draw the tile.
 * \param y (Number): Y position of where to draw the tile.
 * \param flipX (Integer): Whether or not to flip the tile horizontally.
 * \param flipY (Integer): Whether or not to flip the tile vertically.
 * \ns Draw
 */
VMValue Draw_Tile(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);

    Uint32 id = GET_ARG(0, GetInteger);
    int x = (int)GET_ARG(1, GetDecimal) + 8;
    int y = (int)GET_ARG(2, GetDecimal) + 8;
    int flipX = GET_ARG(3, GetInteger);
    int flipY = GET_ARG(4, GetInteger);

    TileSpriteInfo info;
    if (id < Scene::TileSpriteInfos.size() && (info = Scene::TileSpriteInfos[id]).Sprite != NULL) {
        Graphics::DrawSprite(info.Sprite, info.AnimationIndex, info.FrameIndex, x, y, flipX, flipY, 1.0f, 1.0f, 0.0f);
    }
    return NULL_VAL;
}
/***
 * Draw.Texture
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Texture(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    Texture* texture = Graphics::TextureMap->Get((Uint32)GET_ARG(0, GetInteger));
    float x = GET_ARG(1, GetDecimal);
    float y = GET_ARG(2, GetDecimal);

    Graphics::DrawTexture(texture, 0, 0, texture->Width, texture->Height, x, y, texture->Width, texture->Height);
    return NULL_VAL;
}
/***
 * Draw.TextureSized
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_TextureSized(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);

    Texture* texture = Graphics::TextureMap->Get((Uint32)GET_ARG(0, GetInteger));
    float x = GET_ARG(1, GetDecimal);
    float y = GET_ARG(2, GetDecimal);
    float w = GET_ARG(3, GetDecimal);
    float h = GET_ARG(4, GetDecimal);

    Graphics::DrawTexture(texture, 0, 0, texture->Width, texture->Height, x, y, w, h);
    return NULL_VAL;
}
/***
 * Draw.TexturePart
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_TexturePart(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(7);

    Texture* texture = Graphics::TextureMap->Get((Uint32)GET_ARG(0, GetInteger));
    float sx = GET_ARG(1, GetDecimal);
    float sy = GET_ARG(2, GetDecimal);
    float sw = GET_ARG(3, GetDecimal);
    float sh = GET_ARG(4, GetDecimal);
    float x = GET_ARG(5, GetDecimal);
    float y = GET_ARG(6, GetDecimal);

    Graphics::DrawTexture(texture, sx, sy, sw, sh, x, y, sw, sh);
    return NULL_VAL;
}

float textAlign = 0.0f;
float textBaseline = 0.0f;
float textAscent = 1.25f;
float textAdvance = 1.0f;
int _Text_GetLetter(int l) {
    if (l < 0)
        return ' ';
    return l;
}
/***
 * Draw.SetFont
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_SetFont(int argCount, VMValue* args, Uint32 threadID) {
    return NULL_VAL;
}
/***
 * Draw.SetTextAlign
 * \desc Sets the text drawing horizontal alignment. (default: left)
 * \param baseline (Integer): 0 for left, 1 for center, 2 for right.
 * \ns Draw
 */
VMValue Draw_SetTextAlign(int argCount, VMValue* args, Uint32 threadID) {
    textAlign = GET_ARG(0, GetInteger) / 2.0f;
    return NULL_VAL;
}
/***
 * Draw.SetTextBaseline
 * \desc Sets the text drawing vertical alignment. (default: top)
 * \param baseline (Integer): 0 for top, 1 for baseline, 2 for bottom.
 * \ns Draw
 */
VMValue Draw_SetTextBaseline(int argCount, VMValue* args, Uint32 threadID) {
    textBaseline = GET_ARG(0, GetInteger) / 2.0f;
    return NULL_VAL;
}
/***
 * Draw.SetTextAdvance
 * \desc Sets the character spacing multiplier. (default: 1.0)
 * \param ascent (Number): Multiplier for character spacing.
 * \ns Draw
 */
VMValue Draw_SetTextAdvance(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    textAdvance = GET_ARG(0, GetDecimal);
    return NULL_VAL;
}
/***
 * Draw.SetTextLineAscent
 * \desc Sets the line height multiplier. (default: 1.25)
 * \param ascent (Number): Multiplier for line height.
 * \ns Draw
 */
VMValue Draw_SetTextLineAscent(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    textAscent = GET_ARG(0, GetDecimal);
    return NULL_VAL;
}
/***
 * Draw.MeasureText
 * \desc Measures Extended UTF8 text using a sprite or font and stores max width and max height into the array.
 * \param outArray (Array): Array to output size values to.
 * \param sprite (Integer): Index of the loaded sprite to be used as text.
 * \param text (String): Text to measure.
 * \return Returns the array inputted into the function.
 * \ns Draw
 */
VMValue Draw_MeasureText(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    ObjArray* array = GET_ARG(0, GetArray);
    ISprite* sprite = GET_ARG(1, GetSprite);
    char*    text   = GET_ARG(2, GetString);

    float x = 0.0, y = 0.0;
    float maxW = 0.0, maxH = 0.0;
    float lineHeight = sprite->Animations[0].FrameToLoop;
    for (char* i = text; *i; i++) {
        if (*i == '\n') {
            x = 0.0;
            y += lineHeight * textAscent;
            goto __MEASURE_Y;
        }

        x += sprite->Animations[0].Frames[*i].Advance * textAdvance;

        if (maxW < x)
            maxW = x;

        __MEASURE_Y:
        if (maxH < y + (sprite->Animations[0].Frames[*i].Height - sprite->Animations[0].Frames[*i].OffsetY))
            maxH = y + (sprite->Animations[0].Frames[*i].Height - sprite->Animations[0].Frames[*i].OffsetY);
    }

    if (BytecodeObjectManager::Lock()) {
        array->Values->clear();
        array->Values->push_back(DECIMAL_VAL(maxW));
        array->Values->push_back(DECIMAL_VAL(maxH));
        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(array);
    }
    return NULL_VAL;
}
/***
 * Draw.MeasureTextWrapped
 * \desc Measures wrapped Extended UTF8 text using a sprite or font and stores max width and max height into the array.
 * \param outArray (Array): Array to output size values to.
 * \param sprite (Integer): Index of the loaded sprite to be used as text.
 * \param text (String): Text to measure.
 * \param maxWidth (Number): Max width that a line can be.
 * \paramOpt maxLines (Integer): Max number of lines to measure.
 * \return Returns the array inputted into the function.
 * \ns Draw
 */
VMValue Draw_MeasureTextWrapped(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(4);

    ObjArray* array = GET_ARG(0, GetArray);
    ISprite* sprite = GET_ARG(1, GetSprite);
    char*    text   = GET_ARG(2, GetString);
    float    max_w  = GET_ARG(3, GetDecimal);
    int      maxLines = 0x7FFFFFFF;
    if (argCount > 4)
        maxLines = GET_ARG(4, GetInteger);

    int word = 0;
    char* linestart = text;
    char* wordstart = text;

    float x = 0.0, y = 0.0;
    float maxW = 0.0, maxH = 0.0;
    float lineHeight = sprite->Animations[0].FrameToLoop;

    int lineNo = 1;
    for (char* i = text; ; i++) {
        if (((*i == ' ' || *i == 0) && i != wordstart) || *i == '\n') {
            float testWidth = 0.0f;
            for (char* o = linestart; o < i; o++) {
                testWidth += sprite->Animations[0].Frames[*o].Advance * textAdvance;
            }
            if ((testWidth > max_w && word > 0) || *i == '\n') {
                x = 0.0f;
                for (char* o = linestart; o < wordstart - 1; o++) {
                    x += sprite->Animations[0].Frames[*o].Advance * textAdvance;

                    if (maxW < x)
                        maxW = x;
                    if (maxH < y + (sprite->Animations[0].Frames[*o].Height + sprite->Animations[0].Frames[*o].OffsetY))
                        maxH = y + (sprite->Animations[0].Frames[*o].Height + sprite->Animations[0].Frames[*o].OffsetY);
                }

                if (lineNo == maxLines)
                    goto FINISH;
                lineNo++;

                linestart = wordstart;
                y += lineHeight * textAscent;
            }

            wordstart = i + 1;
            word++;
        }

        if (!*i)
            break;
    }

    x = 0.0f;
    for (char* o = linestart; *o; o++) {
        x += sprite->Animations[0].Frames[*o].Advance * textAdvance;
        if (maxW < x)
            maxW = x;
        if (maxH < y + (sprite->Animations[0].Frames[*o].Height + sprite->Animations[0].Frames[*o].OffsetY))
            maxH = y + (sprite->Animations[0].Frames[*o].Height + sprite->Animations[0].Frames[*o].OffsetY);
    }

    FINISH:
    if (BytecodeObjectManager::Lock()) {
        array->Values->clear();
        array->Values->push_back(DECIMAL_VAL(maxW));
        array->Values->push_back(DECIMAL_VAL(maxH));
        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(array);
    }
    return NULL_VAL;
}
/***
 * Draw.Text
 * \desc Draws Extended UTF8 text using a sprite or font.
 * \param sprite (Integer): Index of the loaded sprite to be used as text.
 * \param text (String): Text to draw.
 * \param x (Number): X position of where to draw the text.
 * \param y (Number): Y position of where to draw the text.
 * \ns Draw
 */
VMValue Draw_Text(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);

    ISprite* sprite = GET_ARG(0, GetSprite);
    char*    text   = GET_ARG(1, GetString);
    float    basex = GET_ARG(2, GetDecimal);
    float    basey = GET_ARG(3, GetDecimal);

    float    x = basex;
    float    y = basey;
    float*   lineWidths;
    int      line = 0;

    // Count lines
    for (char* i = text; *i; i++) {
        if (*i == '\n') {
            line++;
            continue;
        }
    }
    line++;
    lineWidths = (float*)malloc(line * sizeof(float));
    if (!lineWidths)
        return NULL_VAL;

    // Get line widths
    line = 0;
    x = 0.0f;
    for (char* i = text, l; *i; i++) {
        l = _Text_GetLetter((Uint8)*i);
        if (l == '\n') {
            lineWidths[line++] = x;
            x = 0.0f;
            continue;
        }
        x += sprite->Animations[0].Frames[l].Advance * textAdvance;
    }
    lineWidths[line++] = x;

    // Draw text
    line = 0;
    x = basex;
    bool lineBack = true;
    for (char* i = text, l; *i; i++) {
        l = _Text_GetLetter((Uint8)*i);
        if (lineBack) {
            x -= sprite->Animations[0].Frames[l].OffsetX;
            lineBack = false;
        }

        if (l == '\n') {
            x = basex;
            y += sprite->Animations[0].FrameToLoop * textAscent;
            lineBack = true;
            line++;
            continue;
        }

        Graphics::DrawSprite(sprite, 0, l, x - lineWidths[line] * textAlign, y - sprite->Animations[0].AnimationSpeed * textBaseline, false, false, 1.0f, 1.0f, 0.0f);
        x += sprite->Animations[0].Frames[l].Advance * textAdvance;
    }

    free(lineWidths);
    return NULL_VAL;
}
/***
 * Draw.TextWrapped
 * \desc Draws wrapped Extended UTF8 text using a sprite or font.
 * \param sprite (Integer): Index of the loaded sprite to be used as text.
 * \param text (String): Text to draw.
 * \param x (Number): X position of where to draw the tile.
 * \param y (Number): Y position of where to draw the tile.
 * \param maxWidth (Number): Max width the text can draw in.
 * \param maxLines (Integer): Max lines the text can draw.
 * \ns Draw
 */
VMValue Draw_TextWrapped(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(5);

    ISprite* sprite = GET_ARG(0, GetSprite);
    char*    text   = GET_ARG(1, GetString);
    float    basex = GET_ARG(2, GetDecimal);
    float    basey = GET_ARG(3, GetDecimal);
    float    max_w = GET_ARG(4, GetDecimal);
    int      maxLines = 0x7FFFFFFF;
    if (argCount > 5)
        maxLines = GET_ARG(5, GetInteger);

    float    x = basex;
    float    y = basey;

    // Draw text
    int   word = 0;
    char* linestart = text;
    char* wordstart = text;
    bool  lineBack = true;
    int   lineNo = 1;
    for (char* i = text, l; ; i++) {
        l = _Text_GetLetter((Uint8)*i);
        if (((l == ' ' || l == 0) && i != wordstart) || l == '\n') {
            float testWidth = 0.0f;
            for (char* o = linestart, lm; o < i; o++) {
                lm = _Text_GetLetter((Uint8)*o);
                testWidth += sprite->Animations[0].Frames[lm].Advance * textAdvance;
            }

            if ((testWidth > max_w && word > 0) || l == '\n') {
                float lineWidth = 0.0f;
                for (char* o = linestart, lm; o < wordstart - 1; o++) {
                    lm = _Text_GetLetter((Uint8)*o);
                    if (lineBack) {
                        lineWidth -= sprite->Animations[0].Frames[lm].OffsetX;
                        lineBack = false;
                    }
                    lineWidth += sprite->Animations[0].Frames[lm].Advance * textAdvance;
                }
                lineBack = true;

                x = basex - lineWidth * textAlign;
                for (char* o = linestart, lm; o < wordstart - 1; o++) {
                    lm = _Text_GetLetter((Uint8)*o);
                    if (lineBack) {
                        x -= sprite->Animations[0].Frames[lm].OffsetX;
                        lineBack = false;
                    }
                    Graphics::DrawSprite(sprite, 0, lm, x, y - sprite->Animations[0].AnimationSpeed * textBaseline, false, false, 1.0f, 1.0f, 0.0f);
                    x += sprite->Animations[0].Frames[lm].Advance * textAdvance;
                }

                if (lineNo == maxLines)
                    return NULL_VAL;

                lineNo++;

                linestart = wordstart;
                y += sprite->Animations[0].FrameToLoop * textAscent;
                lineBack = true;
            }

            wordstart = i + 1;
            word++;
        }
        if (!l)
            break;
    }

    float lineWidth = 0.0f;
    for (char* o = linestart, l; *o; o++) {
        l = _Text_GetLetter((Uint8)*o);
        if (lineBack) {
            lineWidth -= sprite->Animations[0].Frames[l].OffsetX;
            lineBack = false;
        }
        lineWidth += sprite->Animations[0].Frames[l].Advance * textAdvance;
    }
    lineBack = true;

    x = basex - lineWidth * textAlign;
    for (char* o = linestart, l; *o; o++) {
        l = _Text_GetLetter((Uint8)*o);
        if (lineBack) {
            x -= sprite->Animations[0].Frames[l].OffsetX;
            lineBack = false;
        }
        Graphics::DrawSprite(sprite, 0, l, x, y - sprite->Animations[0].AnimationSpeed * textBaseline, false, false, 1.0f, 1.0f, 0.0f);
        x += sprite->Animations[0].Frames[l].Advance * textAdvance;
    }

    // FINISH:

    return NULL_VAL;
}
/***
 * Draw.TextEllipsis
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_TextEllipsis(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);

    ISprite* sprite = GET_ARG(0, GetSprite);
    char*    text = GET_ARG(1, GetString);
    float    x = GET_ARG(2, GetDecimal);
    float    y = GET_ARG(3, GetDecimal);
    float    maxwidth = GET_ARG(4, GetDecimal);

    float    elpisswidth = sprite->Animations[0].Frames['.'].Advance * 3;

    int t;
    size_t textlen = strlen(text);
    float textwidth = 0.0f;
    for (size_t i = 0; i < textlen; i++) {
        t = (int)text[i];
        textwidth += sprite->Animations[0].Frames[t].Advance;
    }
    // If smaller than or equal to maxwidth, just draw normally.
    if (textwidth <= maxwidth) {
        for (size_t i = 0; i < textlen; i++) {
            t = (int)text[i];
            Graphics::DrawSprite(sprite, 0, t, x, y, false, false, 1.0f, 1.0f, 0.0f);
            x += sprite->Animations[0].Frames[t].Advance;
        }
    }
    else {
        for (size_t i = 0; i < textlen; i++) {
            t = (int)text[i];
            if (x + sprite->Animations[0].Frames[t].Advance + elpisswidth > maxwidth) {
                Graphics::DrawSprite(sprite, 0, '.', x, y, false, false, 1.0f, 1.0f, 0.0f);
                x += sprite->Animations[0].Frames['.'].Advance;
                Graphics::DrawSprite(sprite, 0, '.', x, y, false, false, 1.0f, 1.0f, 0.0f);
                x += sprite->Animations[0].Frames['.'].Advance;
                Graphics::DrawSprite(sprite, 0, '.', x, y, false, false, 1.0f, 1.0f, 0.0f);
                x += sprite->Animations[0].Frames['.'].Advance;
                break;
            }
            Graphics::DrawSprite(sprite, 0, t, x, y, false, false, 1.0f, 1.0f, 0.0f);
            x += sprite->Animations[0].Frames[t].Advance;
        }
    }
    // Graphics::DrawSprite(sprite, 0, t, x, y, false, false, 1.0f, 1.0f, 0.0f);
    return NULL_VAL;
}

/***
 * Draw.SetBlendColor
 * \desc Sets the color to be used for drawing and blending.
 * \param hex (Integer): Hexadecimal format of desired color. (ex: Red = 0xFF0000, Green = 0x00FF00, Blue = 0x0000FF)
 * \param alpha (Number): Opacity to use for drawing, 0.0 to 1.0.
 * \ns Draw
 */
VMValue Draw_SetBlendColor(int argCount, VMValue* args, Uint32 threadID) {
    if (argCount <= 2) {
        CHECK_ARGCOUNT(2);
        int hex = GET_ARG(0, GetInteger);
        float alpha = GET_ARG(1, GetDecimal);
        Graphics::SetBlendColor(
            (hex >> 16 & 0xFF) / 255.f,
            (hex >> 8 & 0xFF) / 255.f,
            (hex & 0xFF) / 255.f, alpha);
        return NULL_VAL;
    }
    CHECK_ARGCOUNT(4);
    Graphics::SetBlendColor(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.SetTextureBlend
 * \desc Sets whether or not to use color and alpha blending on sprites, images, and textures.
 * \param doBlend (Boolean): Whether or not to use blending.
 * \ns Draw
 */
VMValue Draw_SetTextureBlend(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    Graphics::TextureBlend = !!GET_ARG(0, GetInteger);
    return NULL_VAL;
}
/***
 * Draw.SetBlendMode
 * \desc Sets the blend mode used for drawing. <br/>\
</br>Blend Modes:<ul>\
<li><code>BlendMode_NORMAL</code>: Normal pixel blending.</li>\
<li><code>BlendMode_ADD</code>: Additive pixel blending.</li>\
<li><code>BlendMode_MAX</code>: Maximum pixel blending.</li>\
<li><code>BlendMode_SUBTRACT</code>: Subtractive pixel blending.</li>\
<li><code>BlendMode_MATCH_EQUAL</code>: (software-renderer only) Draw pixels only where it matches the Comparison Color.</li>\
<li><code>BlendMode_MATCH_NOT_EQUAL</code>: (software-renderer only) Draw pixels only where it does not match the Comparison Color.</li>\
</ul>
 * \param blendMode (Integer): The desired blend mode.
 * \return
 * \ns Draw
 */
VMValue Draw_SetBlendMode(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    Graphics::SetBlendMode(GET_ARG(0, GetInteger));
    return NULL_VAL;
}
/***
 * Draw.SetBlendFactor
 * \desc Sets the blend factors used for drawing. (Only for hardware-rendering)<br/>\
</br>Blend Factors:<ul>\
<li><code>BlendFactor_ZERO</code>: (0, 0, 0, 0)</li>\
<li><code>BlendFactor_ONE</code>: (1, 1, 1, 1)</li>\
<li><code>BlendFactor_SRC_COLOR</code>: (Rs, Gs, Bs, As)</li>\
<li><code>BlendFactor_INV_SRC_COLOR</code>: (1-Rs, 1-Gs, 1-Bs, 1-As)</li>\
<li><code>BlendFactor_SRC_ALPHA</code>: (As, As, As, As)</li>\
<li><code>BlendFactor_INV_SRC_ALPHA</code>: (1-As, 1-As, 1-As, 1-As)</li>\
<li><code>BlendFactor_DST_COLOR</code>: (Rd, Gd, Bd, Ad)</li>\
<li><code>BlendFactor_INV_DST_COLOR</code>: (1-Rd, 1-Gd, 1-Bd, 1-Ad)</li>\
<li><code>BlendFactor_DST_ALPHA</code>: (Ad, Ad, Ad, Ad)</li>\
<li><code>BlendFactor_INV_DST_ALPHA</code>: (1-Ad, 1-Ad, 1-Ad, 1-Ad)</li>\
</ul>
 * \param sourceFactor (Integer): Source factor for blending.
 * \param destinationFactor (Integer): Destination factor for blending.
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
 * \desc Sets all the blend factors used for drawing. (Only for hardware-rendering)<br/>\
</br>Blend Factors:<ul>\
<li><code>BlendFactor_ZERO</code>: (0, 0, 0, 0)</li>\
<li><code>BlendFactor_ONE</code>: (1, 1, 1, 1)</li>\
<li><code>BlendFactor_SRC_COLOR</code>: (Rs, Gs, Bs, As)</li>\
<li><code>BlendFactor_INV_SRC_COLOR</code>: (1-Rs, 1-Gs, 1-Bs, 1-As)</li>\
<li><code>BlendFactor_SRC_ALPHA</code>: (As, As, As, As)</li>\
<li><code>BlendFactor_INV_SRC_ALPHA</code>: (1-As, 1-As, 1-As, 1-As)</li>\
<li><code>BlendFactor_DST_COLOR</code>: (Rd, Gd, Bd, Ad)</li>\
<li><code>BlendFactor_INV_DST_COLOR</code>: (1-Rd, 1-Gd, 1-Bd, 1-Ad)</li>\
<li><code>BlendFactor_DST_ALPHA</code>: (Ad, Ad, Ad, Ad)</li>\
<li><code>BlendFactor_INV_DST_ALPHA</code>: (1-Ad, 1-Ad, 1-Ad, 1-Ad)</li>\
</ul>
 * \param sourceColorFactor (Integer): Source factor for blending color.
 * \param destinationColorFactor (Integer): Destination factor for blending color.
 * \param sourceAlphaFactor (Integer): Source factor for blending alpha.
 * \param destinationAlphaFactor (Integer): Destination factor for blending alpha.
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
 * \param hex (Integer): Hexadecimal format of desired color. (ex: Red = 0xFF0000, Green = 0x00FF00, Blue = 0x0000FF)
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
 * Draw.Line
 * \desc Draws a line.
 * \param x1 (Number): X position of where to start drawing the line.
 * \param y1 (Number): Y position of where to start drawing the line.
 * \param x2 (Number): X position of where to end drawing the line.
 * \param y2 (Number): Y position of where to end drawing the line.
 * \ns Draw
 */
VMValue Draw_Line(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::StrokeLine(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.Circle
 * \desc Draws a circle.
 * \param x (Number): Center X position of where to draw the circle.
 * \param y (Number): Center Y position of where to draw the circle.
 * \param radius (Number): Radius of the circle.
 * \ns Draw
 */
VMValue Draw_Circle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    Graphics::FillCircle(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.Ellipse
 * \desc Draws an ellipse.
 * \param x (Number): X position of where to draw the ellipse.
 * \param y (Number): Y position of where to draw the ellipse.
 * \param width (Number): Width to draw the ellipse at.
 * \param height (Number): Height to draw the ellipse at.
 * \ns Draw
 */
VMValue Draw_Ellipse(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::FillEllipse(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.Triangle
 * \desc Draws a triangle.
 * \param x1 (Number): X position of the first vertex.
 * \param y1 (Number): Y position of the first vertex.
 * \param x2 (Number): X position of the second vertex.
 * \param y2 (Number): Y position of the second vertex.
 * \param x3 (Number): X position of the third vertex.
 * \param y3 (Number): Y position of the third vertex.
 * \ns Draw
 */
VMValue Draw_Triangle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(6);
    Graphics::FillTriangle(
        GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal),
        GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal),
        GET_ARG(4, GetDecimal), GET_ARG(5, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.TriangleBlend
 * \desc Draws a triangle, blending the colors at the vertices. (Colors are multipled by the global Draw Blend Color, do <code>Draw.SetBlendColor(0xFFFFFF, 1.0)</code> if you want the vertex colors unaffected.)
 * \param x1 (Number): X position of the first vertex.
 * \param y1 (Number): Y position of the first vertex.
 * \param x2 (Number): X position of the second vertex.
 * \param y2 (Number): Y position of the second vertex.
 * \param x3 (Number): X position of the third vertex.
 * \param y3 (Number): Y position of the third vertex.
 * \param color1 (Integer): Color of the first vertex.
 * \param color2 (Integer): Color of the second vertex.
 * \param color3 (Integer): Color of the third vertex.
 * \ns Draw
 */
VMValue Draw_TriangleBlend(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(9);
    SoftwareRenderer::FillTriangleBlend(
        GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal),
        GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal),
        GET_ARG(4, GetDecimal), GET_ARG(5, GetDecimal),
        GET_ARG(6, GetInteger),
        GET_ARG(7, GetInteger),
        GET_ARG(8, GetInteger));
    return NULL_VAL;
}
/***
 * Draw.QuadBlend
 * \desc Draws a triangle, blending the colors at the vertices. (Colors are multipled by the global Draw Blend Color, do <code>Draw.SetBlendColor(0xFFFFFF, 1.0)</code> if you want the vertex colors unaffected.)
 * \param x1 (Number): X position of the first vertex.
 * \param y1 (Number): Y position of the first vertex.
 * \param x2 (Number): X position of the second vertex.
 * \param y2 (Number): Y position of the second vertex.
 * \param x3 (Number): X position of the third vertex.
 * \param y3 (Number): Y position of the third vertex.
 * \param x4 (Number): X position of the fourth vertex.
 * \param y4 (Number): Y position of the fourth vertex.
 * \param color1 (Integer): Color of the first vertex.
 * \param color2 (Integer): Color of the second vertex.
 * \param color3 (Integer): Color of the third vertex.
 * \param color4 (Integer): Color of the fourth vertex.
 * \ns Draw
 */
VMValue Draw_QuadBlend(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(12);
    SoftwareRenderer::FillQuadBlend(
        GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal),
        GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal),
        GET_ARG(4, GetDecimal), GET_ARG(5, GetDecimal),
        GET_ARG(6, GetDecimal), GET_ARG(7, GetDecimal),
        GET_ARG(8, GetInteger),
        GET_ARG(9, GetInteger),
        GET_ARG(10, GetInteger),
        GET_ARG(11, GetInteger));
    return NULL_VAL;
}
/***
 * Draw.Rectangle
 * \desc Draws a rectangle.
 * \param x (Number): X position of where to draw the rectangle.
 * \param y (Number): Y position of where to draw the rectangle.
 * \param width (Number): Width to draw the rectangle at.
 * \param height (Number): Height to draw the rectangle at.
 * \ns Draw
 */
VMValue Draw_Rectangle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::FillRectangle(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.CircleStroke
 * \desc Draws a circle outline.
 * \param x (Number): Center X position of where to draw the circle.
 * \param y (Number): Center Y position of where to draw the circle.
 * \param radius (Number): Radius of the circle.
 * \ns Draw
 */
VMValue Draw_CircleStroke(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    Graphics::StrokeCircle(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.EllipseStroke
 * \desc Draws an ellipse outline.
 * \param x (Number): X position of where to draw the ellipse.
 * \param y (Number): Y position of where to draw the ellipse.
 * \param width (Number): Width to draw the ellipse at.
 * \param height (Number): Height to draw the ellipse at.
 * \ns Draw
 */
VMValue Draw_EllipseStroke(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::StrokeEllipse(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.TriangleStroke
 * \desc Draws a triangle outline.
 * \param x1 (Number): X position of the first vertex.
 * \param y1 (Number): Y position of the first vertex.
 * \param x2 (Number): X position of the second vertex.
 * \param y2 (Number): Y position of the second vertex.
 * \param x3 (Number): X position of the third vertex.
 * \param y3 (Number): Y position of the third vertex.
 * \ns Draw
 */
VMValue Draw_TriangleStroke(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(6);
    Graphics::StrokeTriangle(
        GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal),
        GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal),
        GET_ARG(4, GetDecimal), GET_ARG(5, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.RectangleStroke
 * \desc Draws a rectangle outline.
 * \param x (Number): X position of where to draw the rectangle.
 * \param y (Number): Y position of where to draw the rectangle.
 * \param width (Number): Width to draw the rectangle at.
 * \param height (Number): Height to draw the rectangle at.
 * \ns Draw
 */
VMValue Draw_RectangleStroke(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::StrokeRectangle(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal));
    return NULL_VAL;
}
/***
 * Draw.UseFillSmoothing
 * \desc Sets whether or not to use smoothing when drawing filled shapes. (hardware-renderer only)
 * \param smoothFill (Boolean): Whether or not to use smoothing.
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
 * \desc Sets whether or not to use smoothing when drawing un-filled shapes. (hardware-renderer only)
 * \param smoothFill (Boolean): Whether or not to use smoothing.
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
 * \param x (Number): X position of where to start the draw region.
 * \param y (Number): Y position of where to start the draw region.
 * \param width (Number): Width of the draw region.
 * \param height (Number): Height of the draw region.
 * \ns Draw
 */
VMValue Draw_SetClip(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    Graphics::SetClip((int)GET_ARG(0, GetDecimal), (int)GET_ARG(1, GetDecimal), (int)GET_ARG(2, GetDecimal), (int)GET_ARG(3, GetDecimal));
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
 * Draw.Save
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Save(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Graphics::Save();
    return NULL_VAL;
}
/***
 * Draw.Scale
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Scale(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(2);
    float x = GET_ARG(0, GetDecimal);
    float y = GET_ARG(1, GetDecimal);
    float z = 1.0f; if (argCount == 3) z = GET_ARG(2, GetDecimal);
    Graphics::Scale(x, y, z);
    return NULL_VAL;
}
/***
 * Draw.Rotate
 * \desc
 * \return
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
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Restore(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Graphics::Restore();
    return NULL_VAL;
}
/***
 * Draw.Translate
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Translate(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(2);
    float x = GET_ARG(0, GetDecimal);
    float y = GET_ARG(1, GetDecimal);
    float z = 0.0f; if (argCount == 3) z = GET_ARG(2, GetDecimal);
    Graphics::Translate(x, y, z);
    return NULL_VAL;
}

/***
 * Draw.SetTextureTarget
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_SetTextureTarget(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    Texture* texture = Graphics::TextureMap->Get((Uint32)GET_ARG(0, GetInteger));
    Graphics::SetRenderTarget(texture);
    return NULL_VAL;
}
/***
 * Draw.Clear
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_Clear(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Graphics::Clear();
    return NULL_VAL;
}
/***
 * Draw.ResetTextureTarget
 * \desc
 * \return
 * \ns Draw
 */
VMValue Draw_ResetTextureTarget(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Graphics::SetRenderTarget(!Scene::Views[Scene::ViewCurrent].UseDrawTarget ? NULL : Scene::Views[Scene::ViewCurrent].DrawTarget);
    // Graphics::UpdateOrtho(Scene::Views[Scene::ViewCurrent].Width, Scene::Views[Scene::ViewCurrent].Height);

    // View* currentView = &Scene::Views[Scene::ViewCurrent];
    // Graphics::UpdatePerspective(45.0, currentView->Width / currentView->Height, 0.1, 1000.0);
    // currentView->Z = 500.0;
    // Matrix4x4::Scale(currentView->ProjectionMatrix, currentView->ProjectionMatrix, 1.0, -1.0, 1.0);
    // Matrix4x4::Translate(currentView->ProjectionMatrix, currentView->ProjectionMatrix, -currentView->X - currentView->Width / 2, -currentView->Y - currentView->Height / 2, -currentView->Z);
	Graphics::UpdateProjectionMatrix();
    return NULL_VAL;
}
// #endregion

// #region Ease
/***
 * Ease.InSine
 * \desc Eases the value using the "InSine" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InSine(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InSine(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutSine
 * \desc Eases the value using the "OutSine" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutSine(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutSine(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutSine
 * \desc Eases the value using the "InOutSine" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutSine(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutSine(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InQuad
 * \desc Eases the value using the "InQuad" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuad(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InQuad(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutQuad
 * \desc Eases the value using the "OutQuad" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuad(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutQuad(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutQuad
 * \desc Eases the value using the "InOutQuad" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuad(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutQuad(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InCubic
 * \desc Eases the value using the "InCubic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InCubic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InCubic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutCubic
 * \desc Eases the value using the "OutCubic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutCubic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutCubic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutCubic
 * \desc Eases the value using the "InOutCubic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutCubic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutCubic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InQuart
 * \desc Eases the value using the "InQuart" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuart(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InQuart(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutQuart
 * \desc Eases the value using the "OutQuart" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuart(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutQuart(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutQuart
 * \desc Eases the value using the "InOutQuart" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuart(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutQuart(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InQuint
 * \desc Eases the value using the "InQuint" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InQuint(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InQuint(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutQuint
 * \desc Eases the value using the "OutQuint" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutQuint(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutQuint(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutQuint
 * \desc Eases the value using the "InOutQuint" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutQuint(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutQuint(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InExpo
 * \desc Eases the value using the "InExpo" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InExpo(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InExpo(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutExpo
 * \desc Eases the value using the "OutExpo" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutExpo(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutExpo(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutExpo
 * \desc Eases the value using the "InOutExpo" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutExpo(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutExpo(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InCirc
 * \desc Eases the value using the "InCirc" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InCirc(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InCirc(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutCirc
 * \desc Eases the value using the "OutCirc" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutCirc(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutCirc(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutCirc
 * \desc Eases the value using the "InOutCirc" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutCirc(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutCirc(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InBack
 * \desc Eases the value using the "InBack" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InBack(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InBack(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutBack
 * \desc Eases the value using the "OutBack" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutBack(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutBack(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutBack
 * \desc Eases the value using the "InOutBack" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutBack(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutBack(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InElastic
 * \desc Eases the value using the "InElastic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InElastic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InElastic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutElastic
 * \desc Eases the value using the "OutElastic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutElastic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutElastic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutElastic
 * \desc Eases the value using the "InOutElastic" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutElastic(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutElastic(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InBounce
 * \desc Eases the value using the "InBounce" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InBounce(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InBounce(GET_ARG(0, GetDecimal))); }
/***
 * Ease.OutBounce
 * \desc Eases the value using the "OutBounce" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_OutBounce(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::OutBounce(GET_ARG(0, GetDecimal))); }
/***
 * Ease.InOutBounce
 * \desc Eases the value using the "InOutBounce" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_InOutBounce(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::InOutBounce(GET_ARG(0, GetDecimal))); }
/***
 * Ease.Triangle
 * \desc Eases the value using the "Triangle" formula.
 * \param value (Number): Percent value. (0.0 - 1.0)
 * \return Eased Number value between 0.0 and 1.0.
 * \ns Ease
 */
VMValue Ease_Triangle(int argCount, VMValue* args, Uint32 threadID) { CHECK_ARGCOUNT(1); return NUMBER_VAL(Ease::Triangle(GET_ARG(0, GetDecimal))); }
// #endregion

// #region File
/***
 * File.Exists
 * \desc Determines if the file at the path exists.
 * \param path (String): The path of the file to check for existence.
 * \return Returns 1 if the file exists, 0 if otherwise
 * \ns File
 */
VMValue File_Exists(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* filePath = GET_ARG(0, GetString);
    return INTEGER_VAL(File::Exists(filePath));
}
/***
 * File.ReadAllText
 * \desc Reads all text from the given filename.
 * \param path (String): The path of the file to read.
 * \return Returns all the text in the file as a String value if it can be read, otherwise it returns a <code>null</code> value if it cannot be read.
 * \ns File
 */
VMValue File_ReadAllText(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* filePath = GET_ARG(0, GetString);
    Stream* stream = NULL;
    if (strncmp(filePath, "save://", 7) == 0)
        stream = FileStream::New(filePath + 7, FileStream::SAVEGAME_ACCESS | FileStream::READ_ACCESS);
    else
        stream = FileStream::New(filePath, FileStream::READ_ACCESS);
    if (!stream)
        return NULL_VAL;

    if (BytecodeObjectManager::Lock()) {
        size_t size = stream->Length();
        ObjString* text = AllocString(size);
        stream->ReadBytes(text->Chars, size);
        stream->Close();
        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(text);
    }
    return NULL_VAL;
}
/***
 * File.WriteAllText
 * \desc Writes all text to the given filename.
 * \param path (String): The path of the file to read.
 * \param text (String): The text to write to the file.
 * \return Returns <code>true</code> if successful, <code>false</code> if otherwise.
 * \ns File
 */
VMValue File_WriteAllText(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char* filePath = GET_ARG(0, GetString);
    // To verify 2nd argument is string
    GET_ARG(1, GetString);
    if (BytecodeObjectManager::Lock()) {
        ObjString* text = AS_STRING(args[1]);

        Stream* stream = NULL;
        if (strncmp(filePath, "save://", 7) == 0)
            stream = FileStream::New(filePath + 7, FileStream::SAVEGAME_ACCESS | FileStream::WRITE_ACCESS);
        else
            stream = FileStream::New(filePath, FileStream::WRITE_ACCESS);
        if (!stream) {
            BytecodeObjectManager::Unlock();
            return INTEGER_VAL(false);
        }

        stream->WriteBytes(text->Chars, text->Length);
        stream->Close();

        BytecodeObjectManager::Unlock();
        return INTEGER_VAL(true);
    }
    return INTEGER_VAL(false);
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
        Stream* stream = NULL;
        if (strncmp(bundle->filename, "save://", 7) == 0)
            stream = FileStream::New(bundle->filename + 7, FileStream::SAVEGAME_ACCESS | FileStream::WRITE_ACCESS);
        else
            stream = FileStream::New(bundle->filename, FileStream::WRITE_ACCESS);
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
    char*           url = NULL;
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
    if (BytecodeObjectManager::Lock()) {
        obj = OBJECT_VAL(TakeString((char*)data, length));
        BytecodeObjectManager::Unlock();
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
    bool  blocking = GET_ARG(2, GetInteger);

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

// #region Input
/***
 * Input.GetMouseX
 * \desc Gets cursor's X position.
 * \return Returns cursor's X position in relation to the window.
 * \ns Input
 */
VMValue Input_GetMouseX(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int value; SDL_GetMouseState(&value, NULL);
    return NUMBER_VAL((float)value);
}
/***
 * Input.GetMouseY
 * \desc Gets cursor's Y position.
 * \return Returns cursor's Y position in relation to the window.
 * \ns Input
 */
VMValue Input_GetMouseY(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int value; SDL_GetMouseState(NULL, &value);
    return NUMBER_VAL((float)value);
}
/***
 * Input.IsMouseButtonDown
 * \desc Gets whether the mouse button is currently down.<br/>\
</br>Mouse Button Indexes:<ul>\
<li><code>0</code>: Left</li>\
<li><code>1</code>: Middle</li>\
<li><code>2</code>: Right</li>\
</ul>
 * \param mouseButtonID (Integer): Index of the mouse button to check.
 * \return Returns whether the mouse button is currently down.
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
</br>Mouse Button Indexes:<ul>\
<li><code>0</code>: Left</li>\
<li><code>1</code>: Middle</li>\
<li><code>2</code>: Right</li>\
</ul>
 * \param mouseButtonID (Integer): Index of the mouse button to check.
 * \return Returns whether the mouse button started pressing during the current frame.
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
</br>Mouse Button Indexes:<ul>\
<li><code>0</code>: Left</li>\
<li><code>1</code>: Middle</li>\
<li><code>2</code>: Right</li>\
</ul>
 * \param mouseButtonID (Integer): Index of the mouse button to check.
 * \return Returns whether the mouse button released during the current frame.
 * \ns Input
 */
VMValue Input_IsMouseButtonReleased(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int button = GET_ARG(0, GetInteger);
    return INTEGER_VAL((InputManager::MouseReleased >> button) & 1);
}
/***
 * Input.IsKeyDown
 * \desc Gets whether the key is currently down.
 * \param keyID (Integer): Index of the key to check.
 * \return Returns whether the key is currently down.
 * \ns Input
 */
VMValue Input_IsKeyDown(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int key = GET_ARG(0, GetInteger);
    return INTEGER_VAL(InputManager::KeyboardState[key]);
}
/***
 * Input.IsKeyPressed
 * \desc Gets whether the key started pressing during the current frame.
 * \param mouseButtonID (Integer): Index of the key to check.
 * \return Returns whether the key started pressing during the current frame.
 * \ns Input
 */
VMValue Input_IsKeyPressed(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int key = GET_ARG(0, GetInteger);
    int down = InputManager::KeyboardState[key] && !InputManager::KeyboardStateLast[key];
    return INTEGER_VAL(down);
}
/***
 * Input.IsKeyReleased
 * \desc Gets whether the key released during the current frame.
 * \param mouseButtonID (Integer): Index of the key to check.
 * \return Returns whether the key released during the current frame.
 * \ns Input
 */
VMValue Input_IsKeyReleased(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int key = GET_ARG(0, GetInteger);
    int down = !InputManager::KeyboardState[key] && InputManager::KeyboardStateLast[key];
    return INTEGER_VAL(down);
}
/***
 * Input.GetControllerAttached
 * \desc Gets whether the controller at the index is attached.
 * \param controllerIndex (Integer): Index of the controller to check.
 * \return Returns whether the controller at the index is attached.
 * \ns Input
 */
VMValue Input_GetControllerAttached(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int controller_index = GET_ARG(0, GetInteger);
    return INTEGER_VAL(InputManager::GetControllerAttached(controller_index));
}
/***
 * Input.GetControllerHat
 * \desc Gets the hat value from the controller at the index.
 * \param controllerIndex (Integer): Index of the controller to check.
 * \param hatIndex (Integer): Index of the hat to check.
 * \return Returns the hat value from the controller at the index.
 * \ns Input
 */
VMValue Input_GetControllerHat(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int controller_index = GET_ARG(0, GetInteger);
    int index = GET_ARG(1, GetInteger);
    return INTEGER_VAL(InputManager::GetControllerHat(controller_index, index));
}
/***
 * Input.GetControllerAxis
 * \desc Gets the axis value from the controller at the index.
 * \param controllerIndex (Integer): Index of the controller to check.
 * \param axisIndex (Integer): Index of the axis to check.
 * \return Returns the axis value from the controller at the index.
 * \ns Input
 */
VMValue Input_GetControllerAxis(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int controller_index = GET_ARG(0, GetInteger);
    int index = GET_ARG(1, GetInteger);
    return DECIMAL_VAL(InputManager::GetControllerAxis(controller_index, index));
}
/***
 * Input.GetControllerButton
 * \desc Gets the button value from the controller at the index.
 * \param controllerIndex (Integer): Index of the controller to check.
 * \param buttonIndex (Integer): Index of the button to check.
 * \return Returns the button value from the controller at the index.
 * \ns Input
 */
VMValue Input_GetControllerButton(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int controller_index = GET_ARG(0, GetInteger);
    int index = GET_ARG(1, GetInteger);
    return INTEGER_VAL(InputManager::GetControllerButton(controller_index, index));
}
/***
 * Input.GetControllerName
 * \desc Gets the name of the controller at the index.
 * \param controllerIndex (Integer): Index of the controller to check.
 * \return Returns the name of the controller at the index.
 * \ns Input
 */
VMValue Input_GetControllerName(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int controller_index = GET_ARG(0, GetInteger);

    char* name = InputManager::GetControllerName(controller_index);

    if (BytecodeObjectManager::Lock()) {
        ObjString* string = CopyString(name, strlen(name));
        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(string);
    }
    return NULL_VAL;
}
// #endregion

// #region IO
// #endregion

// #region Instance
/***
 * Instance.Create
 * \desc Creates a new instance of an object class and calls it's <code>Create</code> event with the flag.
 * \param className (String): Name of the object class.
 * \param x (Number): X position of where to place the new instance.
 * \param y (Number): Y position of where to place the new instance.
 * \paramOpt flag (Integer): Integer value to pass to the <code>Create</code> event. (Default: 0)
 * \return Returns the new instance.
 * \ns Instance
 */
VMValue Instance_Create(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(3);

    char* objectName = GET_ARG(0, GetString);
    float x = GET_ARG(1, GetDecimal);
    float y = GET_ARG(2, GetDecimal);
    int flag = argCount == 4 ? GET_ARG(3, GetInteger) : 0;

    ObjectList* objectList = NULL;
    if (!Scene::ObjectLists->Exists(objectName)) {
        objectList = new (nothrow) ObjectList();
        strcpy(objectList->ObjectName, objectName);
        objectList->SpawnFunction = (Entity* (*)())BytecodeObjectManager::GetSpawnFunction(CombinedHash::EncryptString(objectName), objectName);
        Scene::ObjectLists->Put(objectName, objectList);
    }
    else {
        objectList = Scene::ObjectLists->Get(objectName);
        objectList->SpawnFunction = (Entity* (*)())BytecodeObjectManager::GetSpawnFunction(CombinedHash::EncryptString(objectName), objectName);
    }

    if (!objectList->SpawnFunction) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Object \"%s\" does not exist.", objectName);
        return NULL_VAL;
    }

    BytecodeObject* obj = (BytecodeObject*)objectList->SpawnFunction();
    obj->X = x;
    obj->Y = y;
    obj->InitialX = x;
    obj->InitialY = y;
    obj->List = objectList;
    Scene::AddDynamic(objectList, obj);

    ObjInstance* instance = obj->Instance;

    obj->Create(flag);


    return OBJECT_VAL(instance);
}
/***
 * Instance.GetNth
 * \desc Gets the n'th instance of an object class.
 * \param className (String): Name of the object class.
 * \param n (Integer): n'th of object class' instances to get. <code>0</code> is first.
 * \return Returns n'th of object class' instances, <code>null</code> if instance cannot be found or class does not exist.
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
    BytecodeObject* object = (BytecodeObject*)objectList->GetNth(n);

    if (object) {
        return OBJECT_VAL(object->Instance);
    }

    return NULL_VAL;
}
/***
 * Instance.IsClass
 * \desc Determines whether or not the instance is of a specified object class.
 * \param instance (Instance): The instance to check.
 * \param className (String): Name of the object class.
 * \return Returns whether or not the instance is of a specified object class.
 * \ns Instance
 */
VMValue Instance_IsClass(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    char* objectName = GET_ARG(1, GetString);

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
 * Instance.GetCount
 * \desc Gets amount of currently active instances in an object class.
 * \param className (String): Name of the object class.
 * \return Returns count of currently active instances in an object class.
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
 * \desc Gets the instance created after or before the specified instance. <code>0</code> is the next instance, <code>-1</code> is the previous instance.
 * \param instance (Instance): The instance to check.
 * \param n (Integer): How many instances after or before the desired instance is to the checking instance.
 * \return Returns the desired instance.
 * \ns Instance
 */
VMValue Instance_GetNextInstance(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    Entity* self = (Entity*)AS_INSTANCE(args[0])->EntityPtr;
    int     n    = GET_ARG(1, GetInteger);

    Entity* object = self;
    if (n < 0) {
        for (int i = 0; i < -n; i++) {
            object = object->PrevEntity;
            if (!object)
                return NULL_VAL;
        }
    }
    else {
        for (int i = 0; i <= n; i++) {
            object = object->NextEntity;
            if (!object)
                return NULL_VAL;
        }
    }

    if (object)
        return OBJECT_VAL(((BytecodeObject*)object)->Instance);

    return NULL_VAL;
}
// #endregion

// #region JSON
static int _JSON_FillMap(ObjMap*, const char*, jsmntok_t*, size_t);
static int _JSON_FillArray(ObjArray*, const char*, jsmntok_t*, size_t);

static int _JSON_FillMap(ObjMap* map, const char* text, jsmntok_t* t, size_t count) {
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
        map->Keys->Put(keyHash, HeapCopyString(text + key->start, key->end - key->start));
        tokcount += 1;
        if (key->size > 0) {
            VMValue val = NULL_VAL;
            value = t + 1 + tokcount;
            switch (value->type) {
                case JSMN_PRIMITIVE:
                    tokcount += 1;
                    if (memcmp("true", text + value->start, value->end - value->start) == 0)
                        val = INTEGER_VAL(true);
                    else if (memcmp("false", text + value->start, value->end - value->start) == 0)
                        val = INTEGER_VAL(false);
                    else if (memcmp("null", text + value->start, value->end - value->start) == 0)
                        val = NULL_VAL;
                    else {
                        bool isNumeric = true;
                        bool hasDot = false;
                        for (const char* cStart = text + value->start, *c = cStart; c < text + value->end; c++) {
                            isNumeric &= (c == cStart && *cStart == '-') || (*c >= '0' && *c <= '9') || (isNumeric && *c == '.' && c > text + value->start && !hasDot);
                            hasDot |= (*c == '.');
                        }
                        if (isNumeric) {
                            if (hasDot) {
                                val = DECIMAL_VAL((float)strtod(text + value->start, NULL));
                            }
                            else {
                                val = INTEGER_VAL((int)strtol(text + value->start, NULL, 10));
                            }
                        }
                        else
                            val = OBJECT_VAL(CopyString(text + value->start, value->end - value->start));
                    }
                    break;
                case JSMN_STRING: {
                    tokcount += 1;
                    val = OBJECT_VAL(CopyString(text + value->start, value->end - value->start));

                    char* o = AS_CSTRING(val);
                    for (const char* l = text + value->start; l < text + value->end; l++) {
                        if (*l == '\\') {
                            l++;
                            switch (*l) {
                                case '\"':
                                    *o++ = '\"'; break;
                                case '/':
                                    *o++ = '/'; break;
                                case '\\':
                                    *o++ = '\\'; break;
                                case 'b':
                                    *o++ = '\b'; break;
                                case 'f':
                                    *o++ = '\f'; break;
                                case 'r': // *o++ = '\r';
                                    break;
                                case 'n':
                                    *o++ = '\n'; break;
                                case 't':
                                    *o++ = '\t'; break;
                                case 'u':
                                    l++;
                                    l++;
                                    l++;
                                    l++;
                                    *o++ = '\t'; break;
                            }
                        }
                        else
                            *o++ = *l;
                    }
                    *o = 0;
                    break;
                }
                // /*
                case JSMN_OBJECT: {
                    ObjMap* subMap = NewMap();
                    tokcount += _JSON_FillMap(subMap, text, value, count - tokcount);
                    val = OBJECT_VAL(subMap);
                    break;
                }
                //*/
                case JSMN_ARRAY: {
                    ObjArray* subArray = NewArray();
                    tokcount += _JSON_FillArray(subArray, text, value, count - tokcount);
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
static int _JSON_FillArray(ObjArray* arr, const char* text, jsmntok_t* t, size_t count) {
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
                if (memcmp("true", text + value->start, value->end - value->start) == 0)
                    val = INTEGER_VAL(true);
                else if (memcmp("false", text + value->start, value->end - value->start) == 0)
                    val = INTEGER_VAL(false);
                else if (memcmp("null", text + value->start, value->end - value->start) == 0)
                    val = NULL_VAL;
                else {
                    bool isNumeric = true;
                    bool hasDot = false;
                    for (const char* cStart = text + value->start, *c = cStart; c < text + value->end; c++) {
                        isNumeric &= (c == cStart && *cStart == '-') || (*c >= '0' && *c <= '9') || (isNumeric && *c == '.' && c > text + value->start && !hasDot);
                        hasDot |= (*c == '.');
                    }
                    if (isNumeric) {
                        if (hasDot) {
                            val = DECIMAL_VAL((float)strtod(text + value->start, NULL));
                        }
                        else {
                            val = INTEGER_VAL((int)strtol(text + value->start, NULL, 10));
                        }
                    }
                    else
                        val = OBJECT_VAL(CopyString(text + value->start, value->end - value->start));
                }
                break;
            case JSMN_STRING: {
                tokcount += 1;
                val = OBJECT_VAL(CopyString(text + value->start, value->end - value->start));

                char* o = AS_CSTRING(val);
                for (const char* l = text + value->start; l < text + value->end; l++) {
                    if (*l == '\\') {
                        l++;
                        switch (*l) {
                            case '\"':
                                *o++ = '\"'; break;
                            case '/':
                                *o++ = '/'; break;
                            case '\\':
                                *o++ = '\\'; break;
                            case 'b':
                                *o++ = '\b'; break;
                            case 'f':
                                *o++ = '\f'; break;
                            case 'r': // *o++ = '\r';
                                break;
                            case 'n':
                                *o++ = '\n'; break;
                            case 't':
                                *o++ = '\t'; break;
                            case 'u':
                                l++;
                                l++;
                                l++;
                                l++;
                                *o++ = '\t'; break;
                        }
                    }
                    else
                        *o++ = *l;
                }
                *o = 0;
                break;
            }
            case JSMN_OBJECT: {
                ObjMap* subMap = NewMap();
                tokcount += _JSON_FillMap(subMap, text, value, count - tokcount);
                val = OBJECT_VAL(subMap);
                break;
            }
            case JSMN_ARRAY: {
                ObjArray* subArray = NewArray();
                tokcount += _JSON_FillArray(subArray, text, value, count - tokcount);
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
 * \desc Decodes a String value into a Map value.
 * \param jsonText (String): JSON-compliant text.
 * \return Returns a Map value if the text can be decoded, otherwise returns <code>null</code>.
 * \ns JSON
 */
VMValue JSON_Parse(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    if (BytecodeObjectManager::Lock()) {
        ObjString* string = AS_STRING(args[0]);
        ObjMap*    map = NewMap();

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
                    tok = (jsmntok_t*)realloc(tok, sizeof(*tok) * tokcount);
                    if (tok == NULL) {
                        BytecodeObjectManager::Unlock();
                        return NULL_VAL;
                    }
                    continue;
                }
            }
            else {
                _JSON_FillMap(map, string->Chars, tok, p.toknext);
            }
            break;
        }
        free(tok);

        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(map);
    }
    return NULL_VAL;
}
/***
 * JSON.ToString
 * \desc Converts a Map value into a String value.
 * \param json (Map): Map value.
 * \param prettyPrint (Boolean): Whether or not to use spacing and newlines in the text.
 * \return Returns a JSON string based on the Map value.
 * \ns JSON
 */
VMValue JSON_ToString(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    Compiler::PrettyPrint = !!GET_ARG(1, GetInteger);
    VMValue value = BytecodeObjectManager::CastValueAsString(args[0]);
    Compiler::PrettyPrint = true;
    return value;
}
// #endregion

// #region Math
/***
 * Math.Cos
 * \desc Returns the cosine of an angle of x radians.
 * \param x (Decimal): Angle (in radians) to get the cosine of.
 * \return The cosine of x radians.
 * \ns Math
 */
VMValue Math_Cos(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Cos(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Sin
 * \desc Returns the sine of an angle of x radians.
 * \param x (Decimal): Angle (in radians) to get the sine of.
 * \return The sine of x radians.
 * \ns Math
 */
VMValue Math_Sin(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Sin(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Tan
 * \desc Returns the tangent of an angle of x radians.
 * \param x (Decimal): Angle (in radians) to get the tangent of.
 * \return The tangent of x radians.
 * \ns Math
 */
VMValue Math_Tan(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Tan(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Acos
 * \desc Returns the arccosine of x.
 * \param x (Decimal): Number value to get the arccosine of.
 * \return Returns the angle (in radians) as a Decimal value.
 * \ns Math
 */
VMValue Math_Acos(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Acos(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Asin
 * \desc Returns the arcsine of x.
 * \param x (Decimal): Number value to get the arcsine of.
 * \return Returns the angle (in radians) as a Decimal value.
 * \ns Math
 */
VMValue Math_Asin(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Asin(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Atan
 * \desc Returns the arctangent angle (in radians) from x and y.
 * \param x (Decimal): x value.
 * \param y (Decimal): y value.
 * \return The angle from x and y.
 * \ns Math
 */
VMValue Math_Atan(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    return DECIMAL_VAL(Math::Atan(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Distance
 * \desc Gets the distance from (x1,y1) to (x2,y2) in pixels.
 * \param x1 (Number): X position of first point.
 * \param y1 (Number): Y position of first point.
 * \param x2 (Number): X position of second point.
 * \param y2 (Number): Y position of second point.
 * \return Returns the distance from (x1,y1) to (x2,y2) as a Number value.
 * \ns Math
 */
VMValue Math_Distance(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    return DECIMAL_VAL(Math::Distance(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal), GET_ARG(3, GetDecimal)));
}
/***
 * Math.Direction
 * \desc Gets the angle from (x1,y1) to (x2,y2) in radians.
 * \param x1 (Number): X position of first point.
 * \param y1 (Number): Y position of first point.
 * \param x2 (Number): X position of second point.
 * \param y2 (Number): Y position of second point.
 * \return Returns the angle from (x1,y1) to (x2,y2) as a Number value.
 * \ns Math
 */
VMValue Math_Direction(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    return DECIMAL_VAL(Math::Atan(GET_ARG(2, GetDecimal) - GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal) - GET_ARG(3, GetDecimal)));
}
/***
 * Math.Abs
 * \desc Gets the absolute value of a Number.
 * \param n (Number): Number value.
 * \return Returns the absolute value of n.
 * \ns Math
 */
VMValue Math_Abs(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Abs(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Min
 * \desc Gets the lesser value of two Number values.
 * \param a (Number): Number value.
 * \param b (Number): Number value.
 * \return Returns the lesser value of a and b.
 * \ns Math
 */
VMValue Math_Min(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    // respect the type of number
    return DECIMAL_VAL(Math::Min(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Max
 * \desc Gets the greater value of two Number values.
 * \param a (Number): Number value.
 * \param b (Number): Number value.
 * \return Returns the greater value of a and b.
 * \ns Math
 */
VMValue Math_Max(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    return DECIMAL_VAL(Math::Max(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Clamp
 * \desc Gets the value clamped between a range.
 * \param n (Number): Number value.
 * \param minValue (Number): Minimum range value to clamp to.
 * \param maxValue (Number): Maximum range value to clamp to.
 * \return Returns the Number value if within the range, otherwise returns closest range value.
 * \ns Math
 */
VMValue Math_Clamp(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    return DECIMAL_VAL(Math::Clamp(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal), GET_ARG(2, GetDecimal)));
}
/***
 * Math.Sign
 * \desc Gets the sign associated with a Number value.
 * \param n (Number): Number value.
 * \return Returns <code>-1</code> if <code>n</code> is negative, <code>1</code> if positive, and <code>0</code> if otherwise.
 * \ns Math
 */
VMValue Math_Sign(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::Sign(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Random
 * \desc Get a random number between 0.0 and 1.0.
 * \return Returns the random number.
 * \ns Math
 */
VMValue Math_Random(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return DECIMAL_VAL(Math::Random());
}
/***
 * Math.RandomMax
 * \desc Gets a random number between 0.0 and a specified maximum.
 * \param max (Number): Maximum non-inclusive value.
 * \return Returns the random number.
 * \ns Math
 */
VMValue Math_RandomMax(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(Math::RandomMax(GET_ARG(0, GetDecimal)));
}
/***
 * Math.RandomRange
 * \desc Gets a random number between specified minimum and a specified maximum.
 * \param min (Number): Minimum non-inclusive value.
 * \param max (Number): Maximum non-inclusive value.
 * \return Returns the random number.
 * \ns Math
 */
VMValue Math_RandomRange(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    return DECIMAL_VAL(Math::RandomRange(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Floor
 * \desc Rounds the number n downward, returning the largest integral value that is not greater than n.
 * \param n (Number): Number to be rounded.
 * \return Returns the floored number value.
 * \ns Math
 */
VMValue Math_Floor(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(std::floor(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Ceil
 * \desc Rounds the number n upward, returning the smallest integral value that is not less than n.
 * \param n (Number): Number to be rounded.
 * \return Returns the ceiling-ed number value.
 * \ns Math
 */
VMValue Math_Ceil(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(std::ceil(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Round
 * \desc Rounds the number n.
 * \param n (Number): Number to be rounded.
 * \return Returns the rounded number value.
 * \ns Math
 */
VMValue Math_Round(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(std::round(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Sqrt
 * \desc Retrieves the square root of the number n.
 * \param n (Number): Number to be square rooted.
 * \return Returns the square root of the number n.
 * \ns Math
 */
VMValue Math_Sqrt(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(sqrt(GET_ARG(0, GetDecimal)));
}
/***
 * Math.Pow
 * \desc Retrieves the number n to the power of p.
 * \param n (Number): Number for the base of the exponent.
 * \param p (Number): Exponent.
 * \return Returns the number n to the power of p.
 * \ns Math
 */
VMValue Math_Pow(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    return DECIMAL_VAL(pow(GET_ARG(0, GetDecimal), GET_ARG(1, GetDecimal)));
}
/***
 * Math.Exp
 * \desc Retrieves the constant e (2.717) to the power of p.
 * \param p (Number): Exponent.
 * \return Returns the result number.
 * \ns Math
 */
VMValue Math_Exp(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(std::exp(GET_ARG(0, GetDecimal)));
}
// #endregion

// #region Matrix
/***
 * Matrix.Create
 * \desc Creates a 4x4 matrix and sets it to the identity. <br/>\
"The model, view and projection matrices are three separate matrices. <br/>\
Model maps from an object's local coordinate space into world space, <br/>\
view from world space to camera space, projection from camera to screen.<br/>\
<br/>\
If you compose all three, you can use the one result to map all the way from <br/>\
object space to screen space, making you able to work out what you need to <br/>\
pass on to the next stage of a programmable pipeline from the incoming <br/>\
vertex positions." - Tommy (https://stackoverflow.com/questions/5550620/the-purpose-of-model-view-projection-matrix)
 * \return Returns the Matrix as an Array.
 * \ns Matrix
 */
VMValue Matrix_Create(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    ObjArray* array = NewArray();
    for (int i = 0; i < 16; i++) {
        array->Values->push_back(DECIMAL_VAL(0.0));
    }

    // (*array->Values)[0] = DECIMAL_VAL(1.0);
    // (*array->Values)[5] = DECIMAL_VAL(1.0);
    // (*array->Values)[10] = DECIMAL_VAL(1.0);
    // (*array->Values)[15] = DECIMAL_VAL(1.0);

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
 * \param matrix (Matrix): The matrix to set to the identity.
 * \ns Matrix
 */
VMValue Matrix_Identity(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ObjArray* array = GET_ARG(0, GetArray);
    // for (int i = 0; i < 16; i++) {
    //     (*array->Values)[i] = DECIMAL_VAL(0.0);
    // }
    // (*array->Values)[0] = DECIMAL_VAL(1.0);
    // (*array->Values)[5] = DECIMAL_VAL(1.0);
    // (*array->Values)[10] = DECIMAL_VAL(1.0);
    // (*array->Values)[15] = DECIMAL_VAL(1.0);

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
 * Matrix.Copy
 * \desc Copies the matrix to the destination.
 * \param matrixDestination (Matrix): Destination.
 * \param matrixSource (Matrix): Source.
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
 * \param matrix (Matrix): The matrix to output the values to.
 * \param a (Matrix): The first matrix to use for multiplying.
 * \param b (Matrix): The second matrix to use for multiplying.
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

    #define MULT_SET(x, y) a[3][y] * b[x][3] + \
        a[2][y] * b[x][2] + \
        a[1][y] * b[x][1] + \
        a[0][y] * b[x][0]

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
 * \param matrix (Matrix): The matrix to output the values to.
 * \param x (Number): X position value.
 * \param y (Number): Y position value.
 * \param z (Number): Z position value.
 * \paramOpt resetToIdentity (Boolean): Whether or not to calculate the translation values based on the matrix. (Default: <code>false</code>)
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

    MatrixHelper helper;
    MatrixHelper_CopyFrom(&helper, out);
    if (resetToIdentity) {
        memset(&helper, 0, sizeof(helper));

        helper[0][0] = 1.0f;
        helper[1][1] = 1.0f;
        helper[2][2] = 1.0f;
        helper[3][3] = 1.0f;
    }
    helper[0][3] = x;
    helper[1][3] = y;
    helper[2][3] = z;

    MatrixHelper_CopyTo(&helper, out);
    return NULL_VAL;
}
/***
 * Matrix.Scale
 * \desc Sets the matrix to a scale identity.
 * \param matrix (Matrix): The matrix to output the values to.
 * \param x (Number): X scale value.
 * \param y (Number): Y scale value.
 * \param z (Number): Z scale value.
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
 * \param matrix (Matrix): The matrix to output the values to.
 * \param x (Number): X rotation value.
 * \param y (Number): Y rotation value.
 * \param z (Number): Z rotation value.
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

// #region Music
/***
 * Music.Play
 * \desc Places the music onto the music stack and plays it.
 * \param music (Integer): The music index to play.
 * \ns Music
 */
VMValue Music_Play(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    ISound* audio = Scene::MusicList[GET_ARG(0, GetInteger)]->AsMusic;

    AudioManager::PushMusic(audio, false, 0);
    return NULL_VAL;
}
/***
 * Music.Stop
 * \desc Removes the music from the music stack, stopping it if currently playing.
 * \param music (Integer): The music index to play.
 * \ns Music
 */
VMValue Music_Stop(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ISound* audio = Scene::MusicList[GET_ARG(0, GetInteger)]->AsMusic;
    AudioManager::RemoveMusic(audio);
    return NULL_VAL;
}
/***
 * Music.StopWithFadeOut
 * \desc Removes the music at the top of the music stack, fading it out over a time period.
 * \ns Music
 */
VMValue Music_StopWithFadeOut(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    // ISound* audio = Scene::MusicList[GET_ARG(0, GetInteger)]->AsMusic;
    float seconds = GET_ARG(0, GetDecimal);
    if (seconds < 0.f)
        seconds = 0.f;
    AudioManager::FadeMusic(seconds);
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
    if (AudioManager::MusicStack.size() > 0)
        AudioManager::MusicStack[0]->Paused = true;
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
    if (AudioManager::MusicStack.size() > 0)
        AudioManager::MusicStack[0]->Paused = false;
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
 * Music.Loop
 * \desc Places the music onto the music stack and plays it, looping back to the specified sample index if it reaches the end of playback.
 * \param music (Integer): The music index to play.
 * \param loop (Boolean): Unused.
 * \param loopPoint (Integer): The sample index to loop back to.
 * \ns Music
 */
VMValue Music_Loop(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    ISound* audio = Scene::MusicList[GET_ARG(0, GetInteger)]->AsMusic;
    // int loop = GET_ARG(1, GetInteger);
    int loop_point = GET_ARG(2, GetInteger);

    AudioManager::PushMusic(audio, true, loop_point);
    return NULL_VAL;
}
/***
 * Music.IsPlaying
 * \desc Checks to see if the specified music is currently playing.
 * \param music (Integer): The music index to play.
 * \return Returns whether or not the music is playing.
 * \ns Music
 */
VMValue Music_IsPlaying(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ISound* audio = Scene::MusicList[GET_ARG(0, GetInteger)]->AsMusic;
    return INTEGER_VAL(AudioManager::IsPlayingMusic(audio));
}
// #endregion

// #region Number
/***
 * Number.ToString
 * \desc Converts a Number to a String.
 * \param n (Number): Number value.
 * \paramOpt base (Integer): radix
 * \return
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
            sprintf(temp, "%f", n);

            VMValue strng = NULL_VAL;
            if (BytecodeObjectManager::Lock()) {
                strng = OBJECT_VAL(CopyString(temp, strlen(temp)));
                BytecodeObjectManager::Unlock();
            }
            return strng;
        }
        case VAL_INTEGER:
        case VAL_LINKED_INTEGER: {
            int n = GET_ARG(0, GetInteger);
            char temp[16];
            if (base == 16)
                sprintf(temp, "0x%X", n);
            else
                sprintf(temp, "%d", n);

            VMValue strng = NULL_VAL;
            if (BytecodeObjectManager::Lock()) {
                strng = OBJECT_VAL(CopyString(temp, strlen(temp)));
                BytecodeObjectManager::Unlock();
            }
            return strng;
        }
        default:
            BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false,
                "Expected argument %d to be of type %s instead of %s.", 0 + 1, "Number", GetTypeString(args[0]));
    }

    return NULL_VAL;
}
/***
 * Number.AsInteger
 * \desc Converts a Decimal to an Integer.
 * \param n (Number): Number value.
 * \return Returns an Integer value.
 * \ns Number
 */
VMValue Number_AsInteger(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return INTEGER_VAL((int)GET_ARG(0, GetDecimal));
}
/***
 * Number.AsDecimal
 * \desc Converts a Integer to an Decimal.
 * \param n (Number): Number value.
 * \return Returns an Decimal value.
 * \ns Number
 */
VMValue Number_AsDecimal(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return DECIMAL_VAL(GET_ARG(0, GetDecimal));
}
// #endregion

// #region Palette
/***
 * Palette.EnablePaletteUsage
 * \desc Enables or disables palette usage for the application.
 * \param usePalettes (Boolean): Whether or not to use palettes.
 * \ns Palette
 */
VMValue Palette_EnablePaletteUsage(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int usePalettes = GET_ARG(0, GetInteger);
    Graphics::UsePalettes = usePalettes;
    return NULL_VAL;
}
// .hpal defines color lines that it can load instead of full 256 color .act's
/***
 * Palette.LoadFromFile
 * \desc Loads palette from an .act, .col, .gif, or .hpal file.
 * \param paletteIndex (Integer): Index of palette to load to.
 * \param filename (String): Filepath of file.
 * \ns Palette
 */
VMValue Palette_LoadFromFile(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    // int palIndex = GET_ARG(0, GetInteger);
    // char* filename = GET_ARG(1, GetString);
    return NULL_VAL;
}
/***
 * Palette.LoadFromResource
 * \desc Loads palette from an .act, .col, .gif, or .hpal resource.
 * \param paletteIndex (Integer): Index of palette to load to.
 * \param filename (String): Filepath of resource.
 * \ns Palette
 */
VMValue Palette_LoadFromResource(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int palIndex = GET_ARG(0, GetInteger);
    char* filename = GET_ARG(1, GetString);

    // RSDK StageConfig
    if (StringUtils::StrCaseStr(filename, "StageConfig.bin"))
        RSDKSceneReader::StageConfig_GetColors(filename);
    // RSDK GameConfig
    else if (StringUtils::StrCaseStr(filename, "GameConfig.bin"))
        RSDKSceneReader::GameConfig_GetColors(filename);
    else {
        ResourceStream* reader;
        if ((reader = ResourceStream::New(filename))) {
            MemoryStream* memoryReader;
            if ((memoryReader = MemoryStream::New(reader))) {
                // ACT file
                if (StringUtils::StrCaseStr(filename, ".act") || StringUtils::StrCaseStr(filename, ".ACT")) {
                    do {
                        Uint8 Color[3];
                        for (int d = 0; d < 256; d++) {
                            memoryReader->ReadBytes(Color, 3);
                            SoftwareRenderer::PaletteColors[palIndex][d] = 0xFF000000U | Color[0] << 16 | Color[1] << 8 | Color[2];
                        }
                        SoftwareRenderer::ConvertFromARGBtoNative(&SoftwareRenderer::PaletteColors[palIndex][0], 256);
                    } while (false);
                }
                // COL file
                else if (StringUtils::StrCaseStr(filename, ".col") || StringUtils::StrCaseStr(filename, ".COL")) {
                    // Skip COL header
                    memoryReader->Skip(8);

                    // Read colors
                    Uint8 Color[4];
                    for (int d = 0; d < 256; d++) {
                        memoryReader->ReadBytes(Color, 3);
                        SoftwareRenderer::PaletteColors[palIndex][d] = 0xFF000000U | Color[0] << 16 | Color[1] << 8 | Color[2];
                    }
                    SoftwareRenderer::ConvertFromARGBtoNative(&SoftwareRenderer::PaletteColors[palIndex][0], 256);
                }
                // HPAL file
                else if (StringUtils::StrCaseStr(filename, ".hpal") || StringUtils::StrCaseStr(filename, ".HPAL")) {
                    do {
                        Uint32 magic = memoryReader->ReadUInt32();
                        if (magic != 0x4C415048)
                            break;

                        Uint8 Color[3];
                        int paletteCount = memoryReader->ReadUInt32();

                        if (paletteCount > MAX_PALETTE_COUNT - palIndex)
                            paletteCount = MAX_PALETTE_COUNT - palIndex;

                        for (int i = palIndex; i < palIndex + paletteCount; i++) {
                            // Palette Set
                            int bitmap = memoryReader->ReadUInt16();
                            for (int col = 0; col < 16; col++) {
                                int lineStart = col << 4;
                                if ((bitmap & (1 << col)) != 0) {
                                    for (int d = 0; d < 16; d++) {
                                        memoryReader->ReadBytes(Color, 3);
                                        SoftwareRenderer::PaletteColors[i][lineStart | d] = 0xFF000000U | Color[0] << 16 | Color[1] << 8 | Color[2];
                                    }
                                    SoftwareRenderer::ConvertFromARGBtoNative(&SoftwareRenderer::PaletteColors[i][lineStart], 16);
                                }
                            }
                        }
                    } while (false);
                }
                // GIF file
                else if (StringUtils::StrCaseStr(filename, ".gif") || StringUtils::StrCaseStr(filename, ".GIF")) {
                    bool loadPalette = Graphics::UsePalettes;

                    GIF* gif;

                    Graphics::UsePalettes = false;
                    gif = GIF::Load(filename);
                    Graphics::UsePalettes = loadPalette;

                    if (gif) {
                        if (gif->Colors) {
                            for (int p = 0; p < 256; p++)
                                SoftwareRenderer::PaletteColors[palIndex][p] = gif->Colors[p];
                            Memory::Free(gif->Colors);
                        }
                        delete gif;
                    }
                }
                else {
                    Log::Print(Log::LOG_ERROR, "Cannot read palette \"%s\"!", filename);
                }

                memoryReader->Close();
            }
            reader->Close();
        }
    }

    return NULL_VAL;
}
/***
 * Palette.GetColor
 * \desc Gets a color from the specified palette.
 * \param paletteIndex (Integer): Index of palette.
 * \param colorIndex (Integer): Index of color.
 * \ns Palette
 */
VMValue Palette_GetColor(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int palIndex = GET_ARG(0, GetInteger);
    int colorIndex = GET_ARG(1, GetInteger);
    return INTEGER_VAL((int)(SoftwareRenderer::PaletteColors[palIndex][colorIndex] & 0xFFFFFFU));
}
/***
 * Palette.SetColor
 * \desc Sets a color on the specified palette, format 0xRRGGBB.
 * \param paletteIndex (Integer): Index of palette.
 * \param colorIndex (Integer): Index of color.
 * \param hex (Integer): Hexadecimal color value to set the color to. (format: 0xRRGGBB)
 * \ns Palette
 */
VMValue Palette_SetColor(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int palIndex = GET_ARG(0, GetInteger);
    int colorIndex = GET_ARG(1, GetInteger);
    Uint32 hex = (Uint32)GET_ARG(2, GetInteger);
    SoftwareRenderer::PaletteColors[palIndex][colorIndex] = (hex & 0xFFFFFFU) | 0xFF000000U;
    SoftwareRenderer::ConvertFromARGBtoNative(&SoftwareRenderer::PaletteColors[palIndex][colorIndex], 1);
    return NULL_VAL;
}
/***
 * Palette.MixPalettes
 * \desc Mixes colors between two palettes and outputs to another palette.
 * \param destinationPaletteIndex (Integer): Index of palette to put colors to.
 * \param paletteIndexA (Integer): First index of palette.
 * \param paletteIndexB (Integer): Second index of palette.
 * \param mixRatio (Number): Percentage to mix the colors between 0.0 - 1.0.
 * \param colorIndexStart (Integer): First index of colors to mix.
 * \param colorCount (Integer): Amount of colors to mix.
 * \ns Palette
 */
inline Uint32 PMP_ColorBlend(Uint32 color1, Uint32 color2, int percent) {
    Uint32 rb = color1 & 0xFF00FFU;
    Uint32 g  = color1 & 0x00FF00U;
    rb += (((color2 & 0xFF00FFU) - rb) * percent) >> 8;
    g  += (((color2 & 0x00FF00U) - g) * percent) >> 8;
    return (rb & 0xFF00FFU) | (g & 0x00FF00U);
}
VMValue Palette_MixPalettes(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(6);
    int palIndexDest = GET_ARG(0, GetInteger);
    int palIndex1 = GET_ARG(1, GetInteger);
    int palIndex2 = GET_ARG(2, GetInteger);
    float mix = GET_ARG(3, GetDecimal);
    int colorIndexStart = GET_ARG(4, GetInteger);
    int colorCount = GET_ARG(5, GetInteger);

    int percent = mix * 0x100;
    for (int c = colorIndexStart; c < colorIndexStart + colorCount; c++) {
        SoftwareRenderer::PaletteColors[palIndexDest][c] = 0xFF000000U | PMP_ColorBlend(SoftwareRenderer::PaletteColors[palIndex1][c], SoftwareRenderer::PaletteColors[palIndex2][c], percent);
    }
    return NULL_VAL;
}
/***
 * Palette.RotateColorsLeft
 * \desc Shifts the colors on the palette to the left.
 * \param paletteIndex (Integer): Index of palette.
 * \param colorIndexStart (Integer): First index of colors to shift.
 * \param colorCount (Integer): Amount of colors to shift.
 * \ns Palette
 */
VMValue Palette_RotateColorsLeft(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int palIndex = GET_ARG(0, GetInteger);
    int colorIndexStart = GET_ARG(1, GetInteger);
    int count = GET_ARG(2, GetInteger);

    if (count > 0x100 - colorIndexStart)
        count = 0x100 - colorIndexStart;

    Uint32 temp = SoftwareRenderer::PaletteColors[palIndex][colorIndexStart];
    for (int i = colorIndexStart + 1; i < colorIndexStart + count; i++) {
        SoftwareRenderer::PaletteColors[palIndex][i - 1] = SoftwareRenderer::PaletteColors[palIndex][i];
    }
    SoftwareRenderer::PaletteColors[palIndex][colorIndexStart + count - 1] = temp;
    return NULL_VAL;
}
/***
 * Palette.RotateColorsRight
 * \desc Shifts the colors on the palette to the right.
 * \param paletteIndex (Integer): Index of palette.
 * \param colorIndexStart (Integer): First index of colors to shift.
 * \param colorCount (Integer): Amount of colors to shift.
 * \ns Palette
 */
VMValue Palette_RotateColorsRight(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int palIndex = GET_ARG(0, GetInteger);
    int colorIndexStart = GET_ARG(1, GetInteger);
    int count = GET_ARG(2, GetInteger);

    if (count > 0x100 - colorIndexStart)
        count = 0x100 - colorIndexStart;

    Uint32 temp = SoftwareRenderer::PaletteColors[palIndex][colorIndexStart + count - 1];
    for (int i = colorIndexStart + count - 1; i >= colorIndexStart; i--) {
        SoftwareRenderer::PaletteColors[palIndex][i] = SoftwareRenderer::PaletteColors[palIndex][i - 1];
    }
    SoftwareRenderer::PaletteColors[palIndex][colorIndexStart] = temp;
    return NULL_VAL;
}
/***
 * Palette.CopyColors
 * \desc Copies colors from Palette A to Palette B
 * \param paletteIndexA (Integer): Index of palette to get colors from.
 * \param colorIndexStartA (Integer): First index of colors to copy.
 * \param paletteIndexB (Integer): Index of palette to put colors to.
 * \param colorIndexStartB (Integer): First index of colors to be placed.
 * \param colorCount (Integer): Amount of colors to be copied.
 * \ns Palette
 */
VMValue Palette_CopyColors(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);
    int palIndexFrom = GET_ARG(0, GetInteger);
    int colorIndexStartFrom = GET_ARG(1, GetInteger);
    int palIndexTo = GET_ARG(2, GetInteger);
    int colorIndexStartTo = GET_ARG(3, GetInteger);
    int count = GET_ARG(4, GetInteger);

    if (count > 0x100 - colorIndexStartTo)
        count = 0x100 - colorIndexStartTo;
    if (count > 0x100 - colorIndexStartFrom)
        count = 0x100 - colorIndexStartFrom;

    memcpy(&SoftwareRenderer::PaletteColors[palIndexTo][colorIndexStartTo], &SoftwareRenderer::PaletteColors[palIndexFrom][colorIndexStartFrom], count * sizeof(Uint32));
    return NULL_VAL;
}
/***
 * Palette.SetPaletteIndexLines
 * \desc Sets the palette to be used for drawing, on certain Y-positions on the screen (between the start and end lines).
 * \param paletteIndex (Integer): Index of palette.
 * \param lineStart (Integer): Start line to set to the palette.
 * \param lineEnd (Integer): Line where to stop setting the palette.
 * \ns Palette
 */
VMValue Palette_SetPaletteIndexLines(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int palIndex = GET_ARG(0, GetInteger);
    Sint32 lineStart = GET_ARG(1, GetInteger);
    Sint32 lineEnd = GET_ARG(2, GetInteger);

    Sint32 lastLine = sizeof(SoftwareRenderer::PaletteIndexLines) - 1;
    if (lineStart > lastLine)
        lineStart = lastLine;

    if (lineEnd > lastLine)
        lineEnd = lastLine;

    for (Sint32 i = lineStart; i < lineEnd; i++) {
        SoftwareRenderer::PaletteIndexLines[i] = (Uint8)palIndex;
    }
    return NULL_VAL;
}
// #endregion

// #region Resources
// return true if we found it in the list
bool    GetResourceListSpace(vector<ResourceType*>* list, ResourceType* resource, size_t* index, bool* foundEmpty) {
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
        if ((*list)[i]->FilenameHash == resource->FilenameHash) {
            *index = i;
            delete resource;
            return true;
        }
    }
    return false;
}
/***
 * Resources.LoadSprite
 * \desc Loads a Sprite resource, returning its Sprite index.
 * \param filename (String): Filename of the resource.
 * \param unloadPolicy (Integer): Whether or not to unload the resource at the end of the current Scene, or the game end.
 * \return Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadSprite(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    // size_t listSz = Scene::SpriteList.size();
    // for (size_t i = 0; i < listSz; i++)
    //     if (Scene::SpriteList[i]->FilenameHash == resource->FilenameHash)
    //         return INTEGER_VAL((int)i);
    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::SpriteList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    resource->AsSprite = new (nothrow) ISprite(filename);
    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadImage
 * \desc Loads an Image resource, returning its Image index.
 * \param filename (String): Filename of the resource.
 * \param unloadPolicy (Integer): Whether or not to unload the resource at the end of the current Scene, or the game end.
 * \return Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadImage(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::ImageList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    resource->AsImage = new (nothrow) Image(filename);
    if (!resource->AsImage->TexturePtr) {
        delete resource->AsImage;
        delete resource;
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }
    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadFont
 * \desc Loads a Font resource, returning its Font index.
 * \param filename (String):
 * \param pixelSize (Number):
 * \param unloadPolicy (Integer):
 * \return
 * \ns Resources
 */
VMValue Resources_LoadFont(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    char*  filename = GET_ARG(0, GetString);
    int    pixel_sz = (int)GET_ARG(1, GetDecimal);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->FilenameHash = CRC32::EncryptData(&pixel_sz, sizeof(int), resource->FilenameHash);
    resource->UnloadPolicy = GET_ARG(2, GetInteger);

    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::SpriteList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    ResourceStream* stream = ResourceStream::New(filename);
    if (!stream) {
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }
    resource->AsSprite = FontFace::SpriteFromFont(stream, pixel_sz, filename);
    stream->Close();

    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadShader
 * \desc Please do not use this
 * \param vertexShaderFilename (String):
 * \param fragmentShaderFilename (String):
 * \param unloadPolicy (Integer):
 * \return
 * \ns Resources
 */
VMValue Resources_LoadShader(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    // char* filename_v = GET_ARG(0, GetString);
    // char* filename_f = GET_ARG(1, GetString);

    /*

    ResourceType* shader;
    size_t found = Scene::ShaderList.size();
    for (size_t i = 0; i < found; i++) {
        shader = Scene::ShaderList[i];
        if (!strcmp(shader->FilenameV, filename_v) &&
            !strcmp(shader->FilenameF, filename_f)) {
            found = i;
            break;
        }
    }
    if (found == Scene::ShaderList.size()) {
        ResourceStream* streamV = ResourceStream::New(filename_v);
        if (!streamV) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Resource File \"%s\" does not exist.", filename_v) == ERROR_RES_CONTINUE)
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }

        size_t lenV = streamV->Length();
        char*  sourceV = (char*)malloc(lenV);
        streamV->ReadBytes(sourceV, lenV);
        sourceV[lenV] = 0;
        streamV->Close();

        ResourceStream* streamF = ResourceStream::New(filename_f);
        if (!streamF) {
            if (BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Resource File \"%s\" does not exist.", filename_f) == ERROR_RES_CONTINUE)
                BytecodeObjectManager::Threads[threadID].ReturnFromNative();
        }

        size_t lenF = streamF->Length();
        char*  sourceF = (char*)malloc(lenF);
        streamF->ReadBytes(sourceF, lenF);
        sourceF[lenF] = 0;
        streamF->Close();

        MemoryStream* streamMV = MemoryStream::New(sourceV, lenV);
        MemoryStream* streamMF = MemoryStream::New(sourceF, lenF);

        // if (Graphics::Internal.Init == GLRenderer::Init) {
        //     // shader = new GLShader(streamMV, streamMF);
        // }
        // else {
            free(sourceV);
            free(sourceF);
            streamMV->Close();
            streamMF->Close();
            return INTEGER_VAL((int)-1);
        // }

        free(sourceV);
        free(sourceF);
        streamMV->Close();
        streamMF->Close();

        strcpy(shader->FilenameV, filename_v);
        strcpy(shader->FilenameF, filename_f);

        Scene::ShaderList.push_back(shader);
    }

    return INTEGER_VAL((int)found);
    // */
    return INTEGER_VAL(0);
}
/***
 * Resources.LoadModel
 * \desc Doesn't work yet
 * \param filename (String):
 * \param unloadPolicy (Integer):
 * \return
 * \ns Resources
 */
VMValue Resources_LoadModel(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::ModelList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    ResourceStream* stream = ResourceStream::New(filename);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Could not read resource \"%s\"!", filename);
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }

    resource->AsModel = new (nothrow) IModel();
    if (!resource->AsModel->Load(stream, filename)) {
        delete resource->AsModel;
        resource->AsModel = NULL;
        list->pop_back();
        stream->Close();
        return INTEGER_VAL(-1);
    }
    stream->Close();

    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadMusic
 * \desc Loads a Music resource, returning its Music index.
 * \param filename (String): Filename of the resource.
 * \param unloadPolicy (Integer): Whether or not to unload the resource at the end of the current Scene, or the game end.
 * \return Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadMusic(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::MusicList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    resource->AsMusic = new (nothrow) ISound(filename);
    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadSound
 * \desc Loads a Sound resource, returning its Sound index.
 * \param filename (String): Filename of the resource.
 * \param unloadPolicy (Integer): Whether or not to unload the resource at the end of the current Scene, or the game end.
 * \return Returns the index of the Resource.
 * \ns Resources
 */
VMValue Resources_LoadSound(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::SoundList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    resource->AsSound = new (nothrow) ISound(filename);
    return INTEGER_VAL((int)index);
}
/***
 * Resources.LoadVideo
 * \desc Loads a Video resource, returning its Video index.
 * \param filename (String):
 * \param unloadPolicy (Integer):
 * \return
 * \ns Resources
 */
VMValue Resources_LoadVideo(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char*  filename = GET_ARG(0, GetString);

    ResourceType* resource = new (nothrow) ResourceType();
    resource->FilenameHash = CRC32::EncryptString(filename);
    resource->UnloadPolicy = GET_ARG(1, GetInteger);

    // size_t listSz = Scene::MediaList.size();
    // for (size_t i = 0; i < listSz; i++)
    //     if (Scene::MediaList[i]->FilenameHash == resource->FilenameHash)
    //         return INTEGER_VAL((int)i);
    size_t index = 0;
    bool emptySlot = false;
    vector<ResourceType*>* list = &Scene::MediaList;
    if (GetResourceListSpace(list, resource, &index, &emptySlot))
        return INTEGER_VAL((int)index);
    else if (emptySlot) (*list)[index] = resource; else list->push_back(resource);

    #ifdef USING_FFMPEG
    Texture* VideoTexture = NULL;
    MediaSource* Source = NULL;
    MediaPlayer* Player = NULL;

    Stream* stream = NULL;
    if (strncmp(filename, "file://", 7) == 0)
        stream = FileStream::New(filename + 7, FileStream::READ_ACCESS);
    else
        stream = ResourceStream::New(filename);
    if (!stream) {
        Log::Print(Log::LOG_ERROR, "Couldn't open file '%s'!", filename);
        delete resource;
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }

    Source = MediaSource::CreateSourceFromStream(stream);
    if (!Source) {
        delete resource;
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }

    Player = MediaPlayer::Create(Source,
        Source->GetBestStream(MediaSource::STREAMTYPE_VIDEO),
        Source->GetBestStream(MediaSource::STREAMTYPE_AUDIO),
        Source->GetBestStream(MediaSource::STREAMTYPE_SUBTITLE),
        Scene::Views[0].Width, Scene::Views[0].Height);
    if (!Player) {
        Source->Close();
        delete resource;
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }

    PlayerInfo playerInfo;
    Player->GetInfo(&playerInfo);
    VideoTexture = Graphics::CreateTexture(playerInfo.Video.Output.Format, SDL_TEXTUREACCESS_STATIC, playerInfo.Video.Output.Width, playerInfo.Video.Output.Height);
    if (!VideoTexture) {
        Player->Close();
        Source->Close();
        delete resource;
        (*list)[index] = NULL;
        return INTEGER_VAL(-1);
    }

    if (Player->GetVideoStream() > -1) {
        Log::Print(Log::LOG_WARN, "VIDEO STREAM:");
        Log::Print(Log::LOG_INFO, "    Resolution:  %d x %d", playerInfo.Video.Output.Width, playerInfo.Video.Output.Height);
    }
    if (Player->GetAudioStream() > -1) {
        Log::Print(Log::LOG_WARN, "AUDIO STREAM:");
        Log::Print(Log::LOG_INFO, "    Sample Rate: %d", playerInfo.Audio.Output.SampleRate);
        Log::Print(Log::LOG_INFO, "    Bit Depth:   %d-bit", playerInfo.Audio.Output.Format & 0xFF);
        Log::Print(Log::LOG_INFO, "    Channels:    %d", playerInfo.Audio.Output.Channels);
    }

    MediaBag* newMediaBag = new (nothrow) MediaBag;
    newMediaBag->Source = Source;
    newMediaBag->Player = Player;
    newMediaBag->VideoTexture = VideoTexture;

    resource->AsMedia = newMediaBag;
    return INTEGER_VAL((int)index);
    #endif
    return INTEGER_VAL(-1);
}
/***
 * Resources.FileExists
 * \desc Checks to see if a Resource exists with the given filename.
 * \param filename (String): The given filename.
 * \return Returns <code>true</code> if the Resource exists, <code>false</code> if otherwise.
 * \ns Resources
 */
VMValue Resources_FileExists(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char*  filename = GET_ARG(0, GetString);
    Stream* reader = ResourceStream::New(filename);
    if (reader) {
        reader->Close();
        return INTEGER_VAL(true);
    }
    return INTEGER_VAL(false);
}
/***
 * Resources.ReadAllText
 * \desc Reads all text from the given filename.
 * \param path (String): The path of the resource to read.
 * \return Returns all the text in the resource as a String value if it can be read, otherwise it returns a <code>null</code> value if it cannot be read.
 * \ns Resources
 */
VMValue Resources_ReadAllText(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* filePath = GET_ARG(0, GetString);
    Stream* stream = ResourceStream::New(filePath);
    if (!stream)
        return NULL_VAL;

    if (BytecodeObjectManager::Lock()) {
        size_t size = stream->Length();
        ObjString* text = AllocString(size);
        stream->ReadBytes(text->Chars, size);
        stream->Close();
        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(text);
    }
    return NULL_VAL;
}
/***
 * Resources.UnloadImage
 * \desc
 * \return
 * \ns Resources
 */
VMValue Resources_UnloadImage(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int index = GET_ARG(0, GetInteger);

    ResourceType* resource = Scene::ImageList[index];

    if (!resource)
        return INTEGER_VAL(false);
    delete resource;

    if (!resource->AsImage)
        return INTEGER_VAL(false);
    delete resource->AsImage;

    Scene::ImageList[index] = NULL;

    return INTEGER_VAL(true);
}
// #endregion

// #region Scene
#define CHECK_TILE_LAYER_POS_BOUNDS() if (layer < 0 || layer >= (int)Scene::Layers.size() || x < 0 || y < 0 || x >= Scene::Layers[layer].Width || y >= Scene::Layers[layer].Height) return NULL_VAL;

/***
 * Scene.Load
 * \desc Changes active scene to the one in the specified resource file.
 * \param filename (String): Filename of scene.
 * \ns Scene
 */
VMValue Scene_Load(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* filename = GET_ARG(0, GetString);

    strcpy(Scene::NextScene, filename);
    Scene::NextScene[strlen(filename)] = 0;

    return NULL_VAL;
}
/***
 * Scene.LoadTileCollisions
 * \desc Load tile collisions from a resource file.
 * \param filename (String): Filename of tile collision file.
 * \ns Scene
 */
VMValue Scene_LoadTileCollisions(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* filename = GET_ARG(0, GetString);
    Scene::LoadTileCollisions(filename);
    return NULL_VAL;
}
/***
 * Scene.AreTileCollisionsLoaded
 * \desc Returns or whether tile collisions are loaded.
 * \return Returns <code>true</code> if tile collisions are loaded, or <code>false</code> if not.
 * \ns Scene
 */
VMValue Scene_AreTileCollisionsLoaded(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(!!(Scene::TileCfgA != NULL && Scene::TileCfgB != NULL));
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
 * Scene.GetLayerCount
 * \desc Gets the amount of layers in the active scene.
 * \return Returns the amount of layers in the active scene.
 * \ns Scene
 */
VMValue Scene_GetLayerCount(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL((int)Scene::Layers.size());
}
/***
 * Scene.GetLayerIndex
 * \desc Gets the layer index of the layer with the specified name.
 * \param layerName (String): Name of the layer to find.
 * \return Returns the layer index, or <code>-1</code> if the layer could not be found.
 * \ns Scene
 */
VMValue Scene_GetLayerIndex(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* layername = GET_ARG(0, GetString);
    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        if (strcmp(Scene::Layers[i].Name, layername) == 0)
            return INTEGER_VAL((int)i);
    }
    return INTEGER_VAL(-1);
}
/***
 * Scene.GetName
 * \desc Gets the name of the active scene.
 * \return Returns the name of the active scene.
 * \ns Scene
 */
VMValue Scene_GetName(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return OBJECT_VAL(CopyString(Scene::CurrentScene, strlen(Scene::CurrentScene)));
}
/***
 * Scene.GetWidth
 * \desc Gets the width of the scene in tiles.
 * \return Returns the width of the scene in tiles.
 * \ns Scene
 */
VMValue Scene_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int v = 0;
    if (Scene::Layers.size() > 0)
        v = Scene::Layers[0].Width;

    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        if (strcmp(Scene::Layers[i].Name, "FG Low") == 0)
            return INTEGER_VAL(Scene::Layers[i].Width);
    }

    return INTEGER_VAL(v);
}
/***
 * Scene.GetHeight
 * \desc Gets the height of the scene in tiles.
 * \return Returns the height of the scene in tiles.
 * \ns Scene
 */
VMValue Scene_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int v = 0;
    if (Scene::Layers.size() > 0)
        v = Scene::Layers[0].Height;

    for (size_t i = 0; i < Scene::Layers.size(); i++) {
        if (strcmp(Scene::Layers[i].Name, "FG Low") == 0)
            return INTEGER_VAL(Scene::Layers[i].Height);
    }

    return INTEGER_VAL(v);
}
/***
 * Scene.GetTileSize
 * \desc Gets the size of tiles.
 * \return Returns the size of tiles.
 * \ns Scene
 */
VMValue Scene_GetTileSize(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Scene::TileSize);
}
/***
 * Scene.GetTileID
 * \desc Gets the tile's index number at the tile coordinates.
 * \param layer (Integer): Index of the layer
 * \param x (Number): X position (in tiles) of the tile
 * \param y (Number): Y position (in tiles) of the tile
 * \return Returns the tile's index number.
 * \ns Scene
 */
VMValue Scene_GetTileID(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int layer  = GET_ARG(0, GetInteger);
    int x = (int)GET_ARG(1, GetDecimal);
    int y = (int)GET_ARG(2, GetDecimal);

    CHECK_TILE_LAYER_POS_BOUNDS();

    return INTEGER_VAL((int)(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] & TILE_IDENT_MASK));
}
/***
 * Scene.GetTileFlipX
 * \desc Gets whether the tile at the tile coordinates is flipped horizontally or not.
 * \param layer (Integer): Index of the layer
 * \param x (Number): X position (in tiles) of the tile
 * \param y (Number): Y position (in tiles) of the tile
 * \return Returns whether the tile's horizontally flipped.
 * \ns Scene
 */
VMValue Scene_GetTileFlipX(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int layer  = GET_ARG(0, GetInteger);
    int x = (int)GET_ARG(1, GetDecimal);
    int y = (int)GET_ARG(2, GetDecimal);

    CHECK_TILE_LAYER_POS_BOUNDS();

    return INTEGER_VAL(!!(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] & TILE_FLIPX_MASK));
}
/***
 * Scene.GetTileFlipY
 * \desc Gets whether the tile at the tile coordinates is flipped vertically or not.
 * \param layer (Integer): Index of the layer
 * \param x (Number): X position (in tiles) of the tile
 * \param y (Number): Y position (in tiles) of the tile
 * \return Returns whether the tile's vertically flipped.
 * \ns Scene
 */
VMValue Scene_GetTileFlipY(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int layer  = GET_ARG(0, GetInteger);
    int x = (int)GET_ARG(1, GetDecimal);
    int y = (int)GET_ARG(2, GetDecimal);

    CHECK_TILE_LAYER_POS_BOUNDS();

    return INTEGER_VAL(!!(Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)] & TILE_FLIPY_MASK));
}
/***
 * Scene.SetTile
 * \desc Sets the tile at a position.
 * \param layer (Integer): Layer index.
 * \param cellX (Number): Tile cell X.
 * \param cellY (Number): Tile cell Y.
 * \param tileID (Integer): Tile ID.
 * \param flipX (Boolean): Whether to flip the tile horizontally or not.
 * \param flipY (Boolean): Whether to flip the tile vertically or not.
 * \paramOpt collisionMaskA (Integer): Collision mask to use for the tile on Plane A. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \paramOpt collisionMaskB (Integer): Collision mask to use for the tile on Plane B. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \ns Scene
 */
VMValue Scene_SetTile(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(6);
    int layer  = GET_ARG(0, GetInteger);
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

    CHECK_TILE_LAYER_POS_BOUNDS();

    Uint32* tile = &Scene::Layers[layer].Tiles[x + (y << Scene::Layers[layer].WidthInBits)];

    *tile = tileID & TILE_IDENT_MASK;
    if (flip_x)
        *tile |= TILE_FLIPX_MASK;
    if (flip_y)
        *tile |= TILE_FLIPY_MASK;

    *tile |= collA;
    *tile |= collB;

    // Scene::UpdateTileBatch(layer, x / 8, y / 8);

    Scene::AnyLayerTileChange = true;

    return NULL_VAL;
}
/***
 * Scene.SetTileCollisionSides
 * \desc Sets the collision of a tile at a position.
 * \param layer (Integer): Layer index.
 * \param cellX (Number): Tile cell X.
 * \param cellY (Number): Tile cell Y.
 * \param collisionMaskA (Integer): Collision mask to use for the tile on Plane A. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \param collisionMaskB (Integer): Collision mask to use for the tile on Plane B. (0: No collision, 1: Top-side collision only, 2: Left-right-bottom-side collision only, 3: All-side collision)
 * \return
 * \ns Scene
 */
VMValue Scene_SetTileCollisionSides(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);
    int layer  = GET_ARG(0, GetInteger);
    int x = (int)GET_ARG(1, GetDecimal);
    int y = (int)GET_ARG(2, GetDecimal);
    int collA = GET_ARG(3, GetInteger) << 28;
    int collB = GET_ARG(4, GetInteger) << 26;

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
 * \desc Sets whether the game is paused or not. When paused, only objects with <code>Pauseable</code> set to <code>false</code> will continue to <code>Update</code>.
 * \param isPaused (Boolean): Whether or not the scene is paused.
 * \ns Scene
 */
VMValue Scene_SetPaused(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    Scene::Paused = GET_ARG(0, GetInteger);
    return NULL_VAL;
}
/***
 * Scene.SetLayerVisible
 * \desc Sets the visibility of the specified layer.
 * \param layerIndex (Integer): Index of layer.
 * \param isVisible (Boolean): Whether or not the layer can be seen.
 * \ns Scene
 */
VMValue Scene_SetLayerVisible(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    int visible = GET_ARG(1, GetInteger);
    Scene::Layers[index].Visible = visible;
    return NULL_VAL;
}
/***
 * Scene.SetLayerCollidable
 * \desc Sets whether or not the specified layer's tiles can be collided with.
 * \param layerIndex (Integer): Index of layer.
 * \param isVisible (Boolean): Whether or not the layer can be collided with.
 * \ns Scene
 */
VMValue Scene_SetLayerCollidable(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    int visible = GET_ARG(1, GetInteger);
    if (visible)
        Scene::Layers[index].Flags |=  SceneLayer::FLAGS_COLLIDEABLE;
    else
        Scene::Layers[index].Flags &= ~SceneLayer::FLAGS_COLLIDEABLE;
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
 * \param layerIndex (Integer): Index of layer.
 * \param offsetX (Number): Offset X position.
 * \param offsetY (Number): Offset Y position.
 * \ns Scene
 */
VMValue Scene_SetLayerOffsetPosition(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int index = GET_ARG(0, GetInteger);
    int offsetX = (int)GET_ARG(1, GetDecimal);
    int offsetY = (int)GET_ARG(2, GetDecimal);
    Scene::Layers[index].OffsetX = offsetX;
    Scene::Layers[index].OffsetY = offsetY;
    return NULL_VAL;
}
/***
 * Scene.SetLayerDrawGroup
 * \desc Sets the draw group of the specified layer.
 * \param layerIndex (Integer): Index of layer.
 * \param drawGroup (Integer): Number from 0 to 15. (0 = Back, 15 = Front)
 * \ns Scene
 */
VMValue Scene_SetLayerDrawGroup(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    int drawg = GET_ARG(1, GetInteger);
    Scene::Layers[index].DrawGroup = drawg % Scene::PriorityPerLayer;
    return NULL_VAL;
}
/***
 * Scene.SetLayerDrawBehavior
 * \desc Sets the parallax direction of the layer.
 * \param layerIndex (Integer): Index of layer.
 * \param drawBehavior (Integer): Draw behavior. <br/>\
</br>Behaviors:<ul>\
<li><code>DrawBehavior_HorizontalParallax</code>: Horizontal parallax.</li>\
<li><code>DrawBehavior_VerticalParallax</code>: Do not use.</li>\
<li><code>DrawBehavior_CustomTileScanLines</code>: Custom scanline behavior.</li>\
</ul>
 * \ns Scene
 */
VMValue Scene_SetLayerDrawBehavior(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    int drawBehavior = GET_ARG(1, GetInteger);
    Scene::Layers[index].DrawBehavior = drawBehavior;
    return NULL_VAL;
}
/***
 * Scene.SetLayerBlend
 * \desc Sets whether or not to use color and alpha blending on this layer.
 * \param layerIndex (Integer): Index of layer.
 * \param doBlend (Boolean): Whether or not to use blending.
 * \paramOpt blendMode (Integer): The desired blend mode. <br/>\
</br>Blend Modes:<ul>\
<li><code>BlendMode_NORMAL</code>: Normal pixel blending.</li>\
<li><code>BlendMode_ADD</code>: Additive pixel blending.</li>\
<li><code>BlendMode_MAX</code>: Maximum pixel blending.</li>\
<li><code>BlendMode_SUBTRACT</code>: Subtractive pixel blending.</li>\
<li><code>BlendMode_MATCH_EQUAL</code>: (software-renderer only) Draw pixels only where it matches the Comparison Color.</li>\
<li><code>BlendMode_MATCH_NOT_EQUAL</code>: (software-renderer only) Draw pixels only where it does not match the Comparison Color.</li>\
</ul>
 * \ns Scene
 */
VMValue Scene_SetLayerBlend(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    Scene::Layers[index].Blending = !!GET_ARG(1, GetInteger);
    Scene::Layers[index].BlendMode = argCount >= 3 ? GET_ARG(2, GetInteger) : BlendMode_NORMAL;
    return NULL_VAL;
}
/***
 * Scene.SetLayerOpacity
 * \desc Sets the opacity of the layer.
 * \param layerIndex (Integer): Index of layer.
 * \param opacity (Decimal): Opacity from 0.0 to 1.0.
 * \ns Scene
 */
VMValue Scene_SetLayerOpacity(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    float opacity = GET_ARG(1, GetDecimal);
    if (opacity < 0.0)
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Opacity cannot be lower than 0.0.");
    else if (opacity > 1.0)
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Opacity cannot be higher than 1.0.");
    Scene::Layers[index].Opacity = opacity;
    return NULL_VAL;
}
/***
 * Scene.SetLayerScroll
 * \desc Sets the scroll values of the layer. (Horizontal Parallax = Up/Down values, Vertical Parallax = Left/Right values)
 * \param layerIndex (Integer): Index of layer.
 * \param relative (Decimal): How much to move the layer relative to the camera. (0.0 = no movement, 1.0 = moves opposite to speed of camera, 2.0 = moves twice the speed opposite to camera)
 * \param constant (Decimal): How many pixels to move the layer per frame.
 * \ns Scene
 */
VMValue Scene_SetLayerScroll(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int index = GET_ARG(0, GetInteger);
    float relative = GET_ARG(1, GetDecimal);
    float constant = GET_ARG(2, GetDecimal);
    Scene::Layers[index].RelativeY = (short)(relative * 0x100);
    Scene::Layers[index].ConstantY = (short)(constant * 0x100);
    return NULL_VAL;
}
struct BufferedScrollInfo {
    short relative;
    short constant;
    int canDeform;
};
Uint8* BufferedScrollLines = NULL;
int    BufferedScrollLinesMax = 0;
int    BufferedScrollSetupLayer = -1;
std::vector<BufferedScrollInfo> BufferedScrollInfos;
/***
 * Scene.SetLayerSetParallaxLinesBegin
 * \desc Begins setup for changing the parallax lines.
 * \param layerIndex (Integer): Index of layer.
 * \ns Scene
 */
VMValue Scene_SetLayerSetParallaxLinesBegin(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int index = GET_ARG(0, GetInteger);
    if (BufferedScrollLines) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Did not end scroll line setup before beginning new one");
        Memory::Free(BufferedScrollLines);
    }
    BufferedScrollLinesMax = Scene::Layers[index].HeightData * Scene::TileSize;
    BufferedScrollLines = (Uint8*)Memory::Malloc(BufferedScrollLinesMax);
    BufferedScrollSetupLayer = index;
    BufferedScrollInfos.clear();
    return NULL_VAL;
}
/***
 * Scene.SetLayerSetParallaxLines
 * \desc Set the parallax lines.
 * \param lineStart (Integer): Start line.
 * \param lineEnd (Integer): End line.
 * \param relative (Number): How much to move the scroll line relative to the camera. (0.0 = no movement, 1.0 = moves opposite to speed of camera, 2.0 = moves twice the speed opposite to camera)
 * \param constant (Number): How many pixels to move the layer per frame.
 * \param canDeform (Boolean): Whether the parallax lines can be deformed.
 * \ns Scene
 */
VMValue Scene_SetLayerSetParallaxLines(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(5);
    int lineStart = GET_ARG(0, GetInteger);
    int lineEnd = GET_ARG(1, GetInteger);
    float relative = GET_ARG(2, GetDecimal);
    float constant = GET_ARG(3, GetDecimal);
    int canDeform = GET_ARG(4, GetInteger);

    short relVal = (short)(relative * 0x100);
    short constVal = (short)(constant * 0x100);

    BufferedScrollInfo info;
    info.relative = relVal;
    info.constant = constVal;
    info.canDeform = canDeform;

    // Check to see if these scroll values are used, if not, add them.
    int scrollIndex = (int)BufferedScrollInfos.size();
    size_t setupCount = BufferedScrollInfos.size();
    if (setupCount) {
        scrollIndex = -1;
        for (size_t i = 0; i < setupCount; i++) {
            BufferedScrollInfo setup = BufferedScrollInfos[i];
            if (setup.relative == relVal && setup.constant == constVal && setup.canDeform == canDeform) {
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
    for (int i = lineStart > 0 ? lineStart : 0; i < lineEnd && i < BufferedScrollLinesMax; i++) {
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
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Did not start scroll line setup before ending.");
        return NULL_VAL;
    }

    SceneLayer* layer = &Scene::Layers[BufferedScrollSetupLayer];
    Memory::Free(layer->ScrollInfos);
    Memory::Free(layer->ScrollIndexes);
    Memory::Free(layer->ScrollInfosSplitIndexes);

    layer->ScrollInfoCount = (int)BufferedScrollInfos.size();
    layer->ScrollInfos = (ScrollingInfo*)Memory::Malloc(layer->ScrollInfoCount * sizeof(ScrollingInfo));
    for (int g = 0; g < layer->ScrollInfoCount; g++) {
        layer->ScrollInfos[g].RelativeParallax = BufferedScrollInfos[g].relative;
        layer->ScrollInfos[g].ConstantParallax = BufferedScrollInfos[g].constant;
        layer->ScrollInfos[g].CanDeform = BufferedScrollInfos[g].canDeform;
    }

    int length16 = layer->HeightData * 16;
    if (layer->WidthData > layer->HeightData)
        length16 = layer->WidthData * 16;
    // int splitCount, lastValue, lastY, sliceLen;

    layer->ScrollIndexes = (Uint8*)Memory::Calloc(length16, sizeof(Uint8));
    memcpy(layer->ScrollIndexes, BufferedScrollLines, BufferedScrollLinesMax);

	/*

    // Count first
    splitCount = 0, lastValue = layer->ScrollIndexes[0], lastY = 0;
    for (int y = 0; y < length16; y++) {
        if ((y > 0 && ((y & 0xF) == 0)) || lastValue != layer->ScrollIndexes[y]) {
            splitCount++;
            lastValue = layer->ScrollIndexes[y];
        }
    }
    splitCount++;

    layer->ScrollInfosSplitIndexes = (Uint16*)Memory::Malloc(splitCount * sizeof(Uint16));

    splitCount = 0, lastValue = layer->ScrollIndexes[0], lastY = 0;
    for (int y = 0; y < length16; y++) {
        if ((y > 0 && ((y & 0xF) == 0)) || lastValue != layer->ScrollIndexes[y]) {
            // Do slice
            sliceLen = y - lastY;
            layer->ScrollInfosSplitIndexes[splitCount] = (sliceLen << 8) | lastValue;
            splitCount++;

            // Iterate
            lastValue = layer->ScrollIndexes[y];
            lastY = y;
        }
    }

    // Do slice
    sliceLen = length16 - lastY;
    layer->ScrollInfosSplitIndexes[splitCount] = (sliceLen << 8) | lastValue;
    splitCount++;

    layer->ScrollInfosSplitIndexesCount = splitCount;

	//*/

    // Cleanup
    BufferedScrollInfos.clear();
    Memory::Free(BufferedScrollLines);
    BufferedScrollLines = NULL;
    return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeforms
 * \desc Sets the tile deforms of the layer at the specified index.
 * \param layerIndex (Integer): Index of layer.
 * \param deformIndex (Integer): Index of deform value.
 * \param deformA (Number): Deform value above the Deform Split Line.
 * \param deformB (Number): Deform value below the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeforms(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    int index = GET_ARG(0, GetInteger);
    int lineIndex = GET_ARG(1, GetInteger);
    int deformA = (int)(GET_ARG(2, GetDecimal));
    int deformB = (int)(GET_ARG(3, GetDecimal));
    const int maxDeformLineMask = MAX_DEFORM_LINES - 1;

    lineIndex &= maxDeformLineMask;
    Scene::Layers[index].DeformSetA[lineIndex] = deformA;
    Scene::Layers[index].DeformSetB[lineIndex] = deformB;
    return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeformSplitLine
 * \desc Sets the position of the Deform Split Line.
 * \param layerIndex (Integer): Index of layer.
 * \param deformPosition (Number): The position on screen where the Deform Split Line should be. (Y when horizontal parallax, X when vertical.)
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeformSplitLine(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
    int deformPosition = (int)GET_ARG(1, GetDecimal);
    Scene::Layers[index].DeformSplitLine = deformPosition;
    return NULL_VAL;
}
/***
 * Scene.SetLayerTileDeformOffsets
 * \desc Sets the position of the Deform Split Line.
 * \param layerIndex (Integer): Index of layer.
 * \param deformAOffset (Number): Offset for the deforms above the Deform Split Line.
 * \param deformBOffset (Number): Offset for the deforms below the Deform Split Line.
 * \ns Scene
 */
VMValue Scene_SetLayerTileDeformOffsets(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int index = GET_ARG(0, GetInteger);
    int deformAOffset = (int)GET_ARG(1, GetDecimal);
    int deformBOffset = (int)GET_ARG(2, GetDecimal);
    Scene::Layers[index].DeformOffsetA = deformAOffset;
    Scene::Layers[index].DeformOffsetB = deformBOffset;
    return NULL_VAL;
}
/***
 * Scene.SetLayerCustomScanlineFunction
 * \desc Sets the function to be used for generating custom tile scanlines.
 * \param layerIndex (Integer): Index of layer.
 * \param function (Function): Function to be used before tile drawing for generating scanlines. (Use <code>null</code> to reset functionality.)
 * \ns Scene
 */
VMValue Scene_SetLayerCustomScanlineFunction(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int index = GET_ARG(0, GetInteger);
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
 * \param scanline (Integer): Index of scanline to edit.
 * \param srcX (Number):
 * \param srcY (Number):
 * \param deltaX (Number):
 * \param deltaY (Number):
 * \paramOpt opacity (Decimal):
 * \paramOpt maxHorzCells (Number):
 * \paramOpt maxVertCells (Number):
 * \ns Scene
 */
VMValue Scene_SetTileScanline(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(5);
    int scanlineIndex = GET_ARG(0, GetInteger);

    TileScanLine* scanLine = &SoftwareRenderer::TileScanLineBuffer[scanlineIndex];
    scanLine->SrcX = (Sint64)(GET_ARG(1, GetDecimal) * 0x10000);
    scanLine->SrcY = (Sint64)(GET_ARG(2, GetDecimal) * 0x10000);
    scanLine->DeltaX = (Sint64)(GET_ARG(3, GetDecimal) * 0x10000);
    scanLine->DeltaY = (Sint64)(GET_ARG(4, GetDecimal) * 0x10000);

    int opacity = 0xFF;
    if (argCount >= 6) {
        opacity = (int)(GET_ARG(5, GetDecimal) * 0xFF);
        if (opacity < 0)
            opacity = 0;
        else if (opacity > 0xFF)
            opacity = 0xFF;
    }
    scanLine->Opacity = opacity;

    scanLine->MaxHorzCells = argCount >= 7 ? GET_ARG(6, GetInteger) : 0;
    scanLine->MaxVertCells = argCount >= 8 ? GET_ARG(7, GetInteger) : 0;

    return NULL_VAL;
}

/***
 * Scene.SetObjectViewRender
 * \desc Sets whether or not objects can render on the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param enableViewRender (Boolean):
 * \ns Scene
 */
VMValue Scene_SetObjectViewRender(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int enabled = !!GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }

    int viewRenderFlag = (1 << view_index);
    if (enabled) {
        Scene::ObjectViewRenderFlag |= viewRenderFlag;
        for (Entity* ent = Scene::StaticObjectFirst; ent; ent = ent->NextEntity) {
            ent->ViewRenderFlag |= viewRenderFlag;
        }
        for (Entity* ent = Scene::DynamicObjectFirst; ent; ent = ent->NextEntity) {
            ent->ViewRenderFlag |= viewRenderFlag;
        }
    }
    else {
        Scene::ObjectViewRenderFlag &= ~viewRenderFlag;
        for (Entity* ent = Scene::StaticObjectFirst; ent; ent = ent->NextEntity) {
            ent->ViewRenderFlag &= ~viewRenderFlag;
        }
        for (Entity* ent = Scene::DynamicObjectFirst; ent; ent = ent->NextEntity) {
            ent->ViewRenderFlag &= ~viewRenderFlag;
        }
    }

    return NULL_VAL;
}
/***
 * Scene.SetTileViewRender
 * \desc Sets whether or not tiles can render on the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param enableViewRender (Boolean):
 * \ns Scene
 */
VMValue Scene_SetTileViewRender(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int enabled = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }

    int viewRenderFlag = (1 << view_index);
    if (!!enabled) {
        Scene::TileViewRenderFlag |= viewRenderFlag;
    }
    else {
        Scene::TileViewRenderFlag &= ~viewRenderFlag;
    }

    return NULL_VAL;
}

/***
 * Scene.IsPaused
 * \desc Gets whether or not the scene is in Paused mode.
 * \return Returns whether or not the scene is in Paused mode.
 * \ns Scene
 */
VMValue Scene_IsPaused(int argCount, VMValue* args, Uint32 threadID) {
    return INTEGER_VAL((int)Scene::Paused);
}
// #endregion

// #region Settings
/***
 * Settings.GetString
 * \desc Looks for a property in a section, and returns its value, as a string.
 * \param section (String): The section where the property resides. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to look for.
 * \return Returns the property as a string, or <code>null</code> if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetString(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        // BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    char const* result = Application::Settings->GetProperty(section, GET_ARG(1, GetString));
    if (!result)
        return NULL_VAL;

    return OBJECT_VAL(CopyString(result, strlen(result)));
}
/***
 * Settings.GetInteger
 * \desc Looks for a property in a section, and returns its value, as an integer.
 * \param section (String): The section where the property resides. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to look for.
 * \return Returns the property as an integer, or <code>null</code> if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetInteger(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        // BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    int result;
    if (!Application::Settings->GetInteger(section, GET_ARG(1, GetString), &result))
        return NULL_VAL;

    return INTEGER_VAL(result);
}
/***
 * Settings.GetBool
 * \desc Looks for a property in a section, and returns its value, as a boolean.
 * \param section (String): The section where the property resides. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to look for.
 * \return Returns the property as a boolean, or <code>null</code> if the section or property aren't valid.
 * \ns Settings
 */
VMValue Settings_GetBool(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        // BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    bool result;
    if (!Application::Settings->GetBool(section, GET_ARG(1, GetString), &result))
        return NULL_VAL;

    return INTEGER_VAL((int)result);
}
/***
 * Settings.SetString
 * \desc Sets a property in a section to a string value.
 * \param section (String): The section where the property resides. If the section doesn't exist, it will be created. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to set.
 * \param value (String): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetString(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);

    Application::Settings->SetString(section, GET_ARG(1, GetString), GET_ARG(2, GetString));
    return NULL_VAL;
}
/***
 * Settings.SetInteger
 * \desc Sets a property in a section to an integer value.
 * \param section (String): The section where the property resides. If the section doesn't exist, it will be created. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to set.
 * \param value (Integer): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetInteger(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);

    Application::Settings->SetInteger(section, GET_ARG(1, GetString), GET_ARG(2, GetInteger));
    return NULL_VAL;
}
/***
 * Settings.SetBool
 * \desc Sets a property in a section to a boolean value.
 * \param section (String): The section where the property resides. If the section doesn't exist, it will be created. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to set.
 * \param value (Boolean): The value of the property.
 * \ns Settings
 */
VMValue Settings_SetBool(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);

    Application::Settings->SetBool(section, GET_ARG(1, GetString), GET_ARG(2, GetInteger));
    return NULL_VAL;
}
/***
 * Settings.AddSection
 * \desc Creates a section.
 * \param section (String): The section name.
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
 * \param section (String): The section name.
 * \ns Settings
 */
VMValue Settings_RemoveSection(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    Application::Settings->RemoveSection(section);
    return NULL_VAL;
}
/***
 * Settings.SectionExists
 * \desc Checks if a section exists.
 * \param section (String): The section name.
 * \return Returns <code>true</code> if the section exists, <code>false</code> if not.
 * \ns Settings
 */
VMValue Settings_SectionExists(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    return INTEGER_VAL(Application::Settings->SectionExists(GET_ARG(0, GetString)));
}
/***
 * Settings.GetSectionCount
 * \desc Returns how many sections exist in the settings.
 * \return The total section count, as an integer.
 * \ns Settings
 */
VMValue Settings_GetSectionCount(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Application::Settings->GetSectionCount());
}
/***
 * Settings.PropertyExists
 * \desc Checks if a property exists.
 * \param section (String): The section where the property resides. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property name.
 * \return Returns <code>true</code> if the property exists, <code>false</code> if not.
 * \ns Settings
 */
VMValue Settings_PropertyExists(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    return INTEGER_VAL(Application::Settings->PropertyExists(section, GET_ARG(1, GetString)));
}
/***
 * Settings.RemoveProperty
 * \desc Removes a property from a section.
 * \param section (String): The section where the property resides. If this is <code>null</code>, the global section is used instead.
 * \param property (String): The property to remove.
 * \ns Settings
 */
VMValue Settings_RemoveProperty(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    Application::Settings->RemoveProperty(section, GET_ARG(1, GetString));
    return NULL_VAL;
}
/***
 * Settings.GetPropertyCount
 * \desc Returns how many properties exist in the section.
 * \param section (String): The section. If this is <code>null</code>, the global section is used instead.
 * \return The total section count, as an integer.
 * \ns Settings
 */
VMValue Settings_GetPropertyCount(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    char* section = NULL;
    if (!IS_NULL(args[0]))
        section = GET_ARG(0, GetString);
    if (!Application::Settings->SectionExists(section)) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Section \"%s\" does not exist.", section);
        return NULL_VAL;
    }

    return INTEGER_VAL(Application::Settings->GetPropertyCount(section));
}
/***
 * Settings.Save
 * \desc Saves the settings.
 * \ns Settings
 */
VMValue Settings_Save(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Application::SaveSettings();
    return NULL_VAL;
}
// #endregion

// #region Shader
/***
 * Shader.Set
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_Set(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    if (IS_ARRAY(args[0])) {
        ObjArray* array = GET_ARG(0, GetArray);
        Graphics::UseShader(array);
        return NULL_VAL;
    }

    int   arg1 = GET_ARG(0, GetInteger);

	if (!(arg1 >= 0 && arg1 < (int)Scene::ShaderList.size())) return NULL_VAL;

    Graphics::UseShader(Scene::ShaderList[arg1]);
    return NULL_VAL;
}
/***
 * Shader.GetUniform
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_GetUniform(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   arg1 = GET_ARG(0, GetInteger);
    // char* arg2 = GET_ARG(1, GetString);

    if (!(arg1 >= 0 && arg1 < (int)Scene::ShaderList.size())) return INTEGER_VAL(-1);

    // return INTEGER_VAL(Scene::ShaderList[arg1]->GetUniformLocation(arg2));
    return INTEGER_VAL(0);
}
/***
 * Shader.SetUniformI
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_SetUniformI(int argCount, VMValue* args, Uint32 threadID) {
    // CHECK_AT_LEAST_ARGCOUNT(1);
    // int   arg1 = GET_ARG(0, GetInteger);
    // for (int i = 1; i < argCount; i++) {
    //
    // }

    return NULL_VAL;
}
/***
 * Shader.SetUniformF
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_SetUniformF(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(1);
    int   arg1 = GET_ARG(0, GetInteger);

    float* values = (float*)malloc((argCount - 1) * sizeof(float));
    for (int i = 1; i < argCount; i++) {
        values[i - 1] = GetDecimal(args, i, threadID);
    }
    Graphics::Internal.SetUniformF(arg1, argCount - 1, values);
    free(values);
    return NULL_VAL;
}
/***
 * Shader.SetUniform3x3
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_SetUniform3x3(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(9);
    for (int i = 0; i < 9; i++) {
        // GetDecimal(args, i, threadID);
    }
    return NULL_VAL;
}
/***
 * Shader.SetUniform4x4
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_SetUniform4x4(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(16);
    for (int i = 0; i < 16; i++) {
        // GetDecimal(args, i, threadID);
    }
    return NULL_VAL;
}
/***
 * Shader.SetUniformTexture
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_SetUniformTexture(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int   uniform_index = GET_ARG(0, GetInteger);
    int   texture_index = GET_ARG(1, GetInteger);
    int   slot = GET_ARG(2, GetInteger);
    Texture* texture = Graphics::TextureMap->Get(texture_index);

    Graphics::Internal.SetUniformTexture(texture, uniform_index, slot);
    return NULL_VAL;
}
/***
 * Shader.Unset
 * \desc
 * \return
 * \ns Shader
 */
VMValue Shader_Unset(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    Graphics::UseShader(NULL);
    return NULL_VAL;
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
    if (!client)
        return INTEGER_VAL(false);

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
    if (!client)
        return NULL_VAL;

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
    if (!client || client->readyState != WebSocketClient::OPEN)
        return INTEGER_VAL(false);

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

    if (!client)
        return INTEGER_VAL(false);

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
    if (!client)
        return INTEGER_VAL(0);

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
    if (!client)
        return NULL_VAL;

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
    if (!client)
        return NULL_VAL;

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
    if (!client)
        return NULL_VAL;

    if (BytecodeObjectManager::Lock()) {
        char* str = client->ReadString();
        ObjString* objStr = TakeString(str, strlen(str));

        BytecodeObjectManager::Unlock();
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
    if (!client)
        return NULL_VAL;

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
    if (!client)
        return NULL_VAL;

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
    if (!client)
        return NULL_VAL;

    client->SendText(value);
    return NULL_VAL;
}
// #endregion

// #region Sound
/***
 * Sound.Play
 * \desc Plays a sound once.
 * \param sound (Integer): The sound index to play.
 * \return
 * \ns Sound
 */
VMValue Sound_Play(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ISound* audio = Scene::SoundList[GET_ARG(0, GetInteger)]->AsSound;
    int channel = GET_ARG(0, GetInteger);
    AudioManager::SetSound(channel % AudioManager::SoundArrayLength, audio);
    return NULL_VAL;
}
/***
 * Sound.Loop
 * \desc Plays a sound, looping back when it ends.
 * \param sound (Integer): The sound index to play.
 * \paramOpt loopPoint (Integer): Loop point in samples.
 * \return
 * \ns Sound
 */
VMValue Sound_Loop(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(1);
    ISound* audio = Scene::SoundList[GET_ARG(0, GetInteger)]->AsSound;
    int channel = GET_ARG(0, GetInteger);
    int loopPoint = argCount >= 2 ? GET_ARG(1, GetInteger) : 0;
    AudioManager::SetSound(channel % AudioManager::SoundArrayLength, audio, true, loopPoint);
    return NULL_VAL;
}
/***
 * Sound.Stop
 * \desc Stops an actively playing sound.
 * \param sound (Integer): The sound index to stop.
 * \return
 * \ns Sound
 */
VMValue Sound_Stop(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    int channel = GET_ARG(0, GetInteger);

    AudioManager::AudioStop(channel % AudioManager::SoundArrayLength);
    return NULL_VAL;
}
/***
 * Sound.Pause
 * \desc Pauses an actively playing sound.
 * \param sound (Integer): The sound index to pause.
 * \return
 * \ns Sound
 */
VMValue Sound_Pause(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int channel = GET_ARG(0, GetInteger);
    AudioManager::AudioPause(channel % AudioManager::SoundArrayLength);
    return NULL_VAL;
}
/***
 * Sound.Resume
 * \desc Unpauses an paused sound.
 * \param sound (Integer): The sound index to resume.
 * \return
 * \ns Sound
 */
VMValue Sound_Resume(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int channel = GET_ARG(0, GetInteger);
    AudioManager::AudioUnpause(channel % AudioManager::SoundArrayLength);
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
 * \param sound (Integer): The sound index to play.
 * \desc Checks whether a sound is currently playing or not.
 * \ns Sound
 */
VMValue Sound_IsPlaying(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int channel = GET_ARG(0, GetInteger);
    return INTEGER_VAL(AudioManager::AudioIsPlaying(channel));
}
// #endregion

// #region Sprite
/***
 * Sprite.GetAnimationCount
 * \desc Gets the amount of animations in the sprite.
 * \param sprite (Integer): The sprite index to check.
 * \return Returns the amount of animations in the sprite.
 * \ns Sprite
 */
VMValue Sprite_GetAnimationCount(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ISprite* sprite = GET_ARG(0, GetSprite);
    return INTEGER_VAL((int)sprite->Animations.size());
}
/***
 * Sprite.GetFrameLoopIndex
 * \desc Gets the index of the frame that the specified animation will loop back to when it finishes.
 * \param sprite (Integer): The sprite index to check.
 * \param animation (Integer): The animation index of the sprite to check.
 * \return Returns the frame loop index.
 * \ns Sprite
 */
VMValue Sprite_GetFrameLoopIndex(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    ISprite* sprite = GET_ARG(0, GetSprite);
    int animation = GET_ARG(1, GetInteger);
    return INTEGER_VAL(sprite->Animations[animation].FrameToLoop);
}
/***
 * Sprite.GetFrameCount
 * \desc Gets the amount of frames in the specified animation.
 * \param sprite (Integer): The sprite index to check.
 * \param animation (Integer): The animation index of the sprite to check.
 * \return Returns the frame count in the specified animation.
 * \ns Sprite
 */
VMValue Sprite_GetFrameCount(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    ISprite* sprite = GET_ARG(0, GetSprite);
    int animation = GET_ARG(1, GetInteger);
    return INTEGER_VAL((int)sprite->Animations[animation].Frames.size());
}
/***
 * Sprite.GetFrameDuration
 * \desc Gets the frame duration of the specified sprite frame.
 * \param sprite (Integer): The sprite index to check.
 * \param animation (Integer): The animation index of the sprite to check.
 * \param frame (Integer): The frame index of the animation to check.
 * \return Returns the frame duration (in game frames) of the specified sprite frame.
 * \ns Sprite
 */
VMValue Sprite_GetFrameDuration(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    ISprite* sprite = GET_ARG(0, GetSprite);
    int animation = GET_ARG(1, GetInteger);
    int frame = GET_ARG(2, GetInteger);
    return INTEGER_VAL(sprite->Animations[animation].Frames[frame].Duration);
}
/***
 * Sprite.GetFrameSpeed
 * \desc Gets the animation speed of the specified animation.
 * \param sprite (Integer): The sprite index to check.
 * \param animation (Integer): The animation index of the sprite to check.
 * \return Returns an Integer.
 * \ns Sprite
 */
VMValue Sprite_GetFrameSpeed(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    ISprite* sprite = GET_ARG(0, GetSprite);
    int animation = GET_ARG(1, GetInteger);
    return INTEGER_VAL(sprite->Animations[animation].AnimationSpeed);
}
// #endregion

// #region String
/***
 * String.Split
 * \desc
 * \return
 * \ns String
 */
VMValue String_Split(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    // char* string = GET_ARG(0, GetString);
    // char* delimt = GET_ARG(1, GetString);

    if (BytecodeObjectManager::Lock()) {
        ObjArray* array = NewArray();
        int       length = 1;
        for (int i = 0; i < length; i++) {
            ObjString* part = AllocString(16);
            array->Values->push_back(OBJECT_VAL(part));
        }

        BytecodeObjectManager::Unlock();
        return OBJECT_VAL(array);
    }
    return NULL_VAL;
}
/***
 * String.CharAt
 * \desc Gets the UTF8 value of the character at the specified index.
 * \param string (String):
 * \param index (Integer):
 * \return Returns the UTF8 value as an Integer.
 * \ns String
 */
VMValue String_CharAt(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char* string = GET_ARG(0, GetString);
    int   n = GET_ARG(1, GetInteger);
    return INTEGER_VAL((Uint8)string[n]);
}
/***
 * String.Length
 * \desc Gets the length of the String value.
 * \param string (String):
 * \return Returns the length of the String value as an Integer.
 * \ns String
 */
VMValue String_Length(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);
    return INTEGER_VAL((int)strlen(string));
}
/***
 * String.Compare
 * \desc Compare two Strings and retrieve a numerical difference.
 * \param stringA (String):
 * \param stringB (String):
 * \return Returns the comparison result as an Integer.
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
 * \param string (String):
 * \param substring (String):
 * \return Returns the index as an Integer.
 * \ns String
 */
VMValue String_IndexOf(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char* string = GET_ARG(0, GetString);
    char* substring = GET_ARG(1, GetString);
    char* find = strstr(string, substring);
    if (!find)
        return INTEGER_VAL(-1);
    return INTEGER_VAL((int)(find - string));
}
/***
 * String.Contains
 * \desc Searches for whether or not a substring is within a String value.
 * \param string (String):
 * \param substring (String):
 * \return Returns a Boolean value.
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
 * \desc Get a String value from a portion of a larger String value.
 * \param string (String):
 * \param startIndex (Integer):
 * \param length (Integer):
 * \return Returns a String value.
 * \ns String
 */
VMValue String_Substring(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    char* string = GET_ARG(0, GetString);
	size_t index = GET_ARG(1, GetInteger);
	size_t length = GET_ARG(2, GetInteger);

	size_t strln = strlen(string);
    if (length > strln - index)
        length = strln - index;

    VMValue obj = NULL_VAL;
    if (BytecodeObjectManager::Lock()) {
        obj = OBJECT_VAL(CopyString(string + index, length));
        BytecodeObjectManager::Unlock();
    }
    return obj;
}
/***
 * String.ToUpperCase
 * \desc Convert a String value to its uppercase representation.
 * \param string (String):
 * \return Returns a uppercase String value.
 * \ns String
 */
VMValue String_ToUpperCase(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);

    VMValue obj = NULL_VAL;
    if (BytecodeObjectManager::Lock()) {
        ObjString* objStr = CopyString(string, strlen(string));
        for (char* a = objStr->Chars; *a; a++) {
            if (*a >= 'a' && *a <= 'z')
                *a += 'A' - 'a';
            else if (*a >= 'a' && *a <= 'z')
                *a += 'A' - 'a';
        }
        obj = OBJECT_VAL(objStr);
        BytecodeObjectManager::Unlock();
    }
    return obj;
}
/***
 * String.ToLowerCase
 * \desc Convert a String value to its lowercase representation.
 * \param string (String):
 * \return Returns a lowercase String value.
 * \ns String
 */
VMValue String_ToLowerCase(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);

    VMValue obj = NULL_VAL;
    if (BytecodeObjectManager::Lock()) {
        ObjString* objStr = CopyString(string, strlen(string));
        for (char* a = objStr->Chars; *a; a++) {
            if (*a >= 'A' && *a <= 'Z')
                *a += 'a' - 'A';
        }
        obj = OBJECT_VAL(objStr);
        BytecodeObjectManager::Unlock();
    }
    return obj;
}
/***
 * String.LastIndexOf
 * \desc Get the last index at which the substring occurs in the string.
 * \param string (String):
 * \param substring (String):
 * \return Returns the index as an Integer.
 * \ns String
 */
VMValue String_LastIndexOf(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    char* string = GET_ARG(0, GetString);
    char* substring = GET_ARG(1, GetString);
    size_t string_len = strlen(string);
    size_t substring_len = strlen(substring);
    if (string_len < substring_len)
        return INTEGER_VAL(-1);

    char* find = NULL;
    for (char* start = string + string_len - substring_len; start >= string; start--) {
        if (memcmp(start, substring, substring_len) == 0) {
            find = start;
            break;
        }
    }
    if (!find)
        return INTEGER_VAL(-1);
    return INTEGER_VAL((int)(find - string));
}
/***
 * String.ParseInteger
 * \desc Convert a String value to an Integer value if possible.
 * \param string (String):
 * \return Returns the value as an Integer.
 * \ns String
 */
VMValue String_ParseInteger(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);
    return INTEGER_VAL((int)strtol(string, NULL, 10));
}
/***
 * String.ParseDecimal
 * \desc Convert a String value to an Decimal value if possible.
 * \param string (String):
 * \return Returns the value as an Decimal.
 * \ns String
 */
VMValue String_ParseDecimal(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);
    return DECIMAL_VAL((float)strtod(string, NULL));
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
    CHECK_ARGCOUNT(2);
    int width = GET_ARG(0, GetInteger);
    int height = GET_ARG(1, GetInteger);
    Texture* texture = Graphics::CreateTexture(SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width, height);
    return INTEGER_VAL((int)texture->ID);
}
/***
 * Texture.FromSprite
 * \desc
 * \return
 * \ns Texture
 */
VMValue Texture_FromSprite(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    ISprite* arg1 = GET_ARG(0, GetSprite);
    return INTEGER_VAL((int)arg1->Spritesheets[0]->ID);
}
/***
 * Texture.FromImage
 * \desc
 * \return
 * \ns Texture
 */
VMValue Texture_FromImage(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    Image* arg1 = Scene::ImageList[GET_ARG(0, GetInteger)]->AsImage;
    return INTEGER_VAL((int)arg1->TexturePtr->ID);
}
/***
 * Texture.SetInterpolation
 * \desc
 * \return
 * \ns Texture
 */
VMValue Texture_SetInterpolation(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int interpolate = GET_ARG(0, GetInteger);
    Graphics::SetTextureInterpolation(interpolate);
    return NULL_VAL;
}
// #endregion

// #region Touch
/***
 * Touch.GetX
 * \desc Gets the X position of a touch.
 * \param touchIndex (Integer):
 * \return Returns a Number value.
 * \ns Touch
 */
VMValue Touch_GetX(int argCount, VMValue* args, Uint32 threadID) {
    int touch_index = GET_ARG(0, GetInteger);
    return DECIMAL_VAL(InputManager::TouchGetX(touch_index));
}
/***
 * Touch.GetY
 * \desc Gets the Y position of a touch.
 * \param touchIndex (Integer):
 * \return Returns a Number value.
 * \ns Touch
 */
VMValue Touch_GetY(int argCount, VMValue* args, Uint32 threadID) {
    int touch_index = GET_ARG(0, GetInteger);
    return DECIMAL_VAL(InputManager::TouchGetY(touch_index));
}
/***
 * Touch.IsDown
 * \desc Gets whether a touch is currently down on the screen.
 * \param touchIndex (Integer):
 * \return Returns a Boolean value.
 * \ns Touch
 */
VMValue Touch_IsDown(int argCount, VMValue* args, Uint32 threadID) {
    int touch_index = GET_ARG(0, GetInteger);
    return INTEGER_VAL(InputManager::TouchIsDown(touch_index));
}
/***
 * Touch.IsPressed
 * \desc Gets whether a touch just pressed down on the screen.
 * \param touchIndex (Integer):
 * \return Returns a Boolean value.
 * \ns Touch
 */
VMValue Touch_IsPressed(int argCount, VMValue* args, Uint32 threadID) {
    int touch_index = GET_ARG(0, GetInteger);
    return INTEGER_VAL(InputManager::TouchIsPressed(touch_index));
}
/***
 * Touch.IsReleased
 * \desc Gets whether a touch just released from the screen.
 * \param touchIndex (Integer):
 * \return Returns a Boolean value.
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
 * \desc Checks for a tile collision at a specified point, returning <code>true</code> if successful, <code>false</code> if otherwise.
 * \param x (Number): X position to check.
 * \param y (Number): Y position to check.
 * \return Returns a Boolean value.
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
 * \desc Checks for a tile collision at a specified point, returning the angle value if successful, <code>-1</code> if otherwise.
 * \param x (Number): X position to check.
 * \param y (Number): Y position to check.
 * \param collisionField (Integer): Low (0) or high (1) field to check.
 * \param collisionSide (Integer): Which side of the tile to check for collision. (TOP = 1, RIGHT = 2, BOTTOM = 4, LEFT = 8, ALL = 15)
 * \return Returns the angle of the ground as an Integer value.
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
 * \desc Checks for a tile collision in a straight line, returning the angle value if successful, <code>-1</code> if otherwise.
 * \param x (Number): X position to start checking from.
 * \param y (Number): Y position to start checking from.
 * \param directionType (Integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \param length (Integer): How many pixels to check.
 * \param collisionField (Integer): Low (0) or high (1) field to check.
 * \param compareAngle (Integer): Only return a collision if the angle is within 0x20 this value, otherwise if angle comparison is not desired, set this value to -1.
 * \param instance (Instance): Instance to write the values to.
 * \return Returns <code>false</code> if no tile collision, but if <code>true</code>: <br/>\
<pre class="code"><br/>\
    instance.SensorX: (Number), // X Position where the sensor collided if it did. <br/>\
    instance.SensorY: (Number), // Y Position where the sensor collided if it did. <br/>\
    instance.SensorCollided: (Boolean), // Whether or not the sensor collided. <br/>\
    instance.SensorAngle: (Integer) // Tile angle at the collision. <br/>\
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
    ObjInstance* entity = GET_ARG(6, GetInstance);

    Sensor sensor;
    sensor.X = x;
    sensor.Y = y;
    sensor.Collided = false;
    sensor.Angle = 0;
    if (compareAngle > -1)
        sensor.Angle = compareAngle & 0xFF;

    Scene::CollisionInLine(x, y, angleMode, length, collisionField, compareAngle > -1, &sensor);

    if (BytecodeObjectManager::Lock()) {
        /*ObjMap*    mapSensor = NewMap();

        mapSensor->Values->Put("X", DECIMAL_VAL((float)sensor.X));
        mapSensor->Values->Put("Y", DECIMAL_VAL((float)sensor.Y));
        mapSensor->Values->Put("Collided", INTEGER_VAL(sensor.Collided));
        mapSensor->Values->Put("Angle", INTEGER_VAL(sensor.Angle));

        BytecodeObjectManager::Unlock();*/
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
 * \param tileID (Integer): ID of tile to check.
 * \param spriteIndex (Integer): Sprite index. (-1 for default tile sprite)
 * \param animationIndex (Integer): Animation index.
 * \param frameIndex (Integer): Frame index. (-1 for default tile frame)
 * \ns TileInfo
 */
VMValue TileInfo_SetSpriteInfo(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    int tileID = GET_ARG(0, GetInteger);
    int spriteIndex = GET_ARG(1, GetInteger);
    int animationIndex = GET_ARG(2, GetInteger);
    int frameIndex = GET_ARG(3, GetInteger);
    if (spriteIndex <= -1) {
        if (tileID < (int)Scene::TileSpriteInfos.size()) {
            TileSpriteInfo info = Scene::TileSpriteInfos[tileID];
            info.Sprite = Scene::TileSprites[0];
            info.AnimationIndex = 0;
            if (frameIndex > -1)
                info.FrameIndex = frameIndex;
            else
                info.FrameIndex = tileID;
            Scene::TileSpriteInfos[tileID] = info;
        }
    }
    else {
        if (tileID < (int)Scene::TileSpriteInfos.size()) {
            TileSpriteInfo info = Scene::TileSpriteInfos[tileID];
            info.Sprite = GET_ARG(1, GetSprite);
            info.AnimationIndex = animationIndex;
            info.FrameIndex = frameIndex;
            Scene::TileSpriteInfos[tileID] = info;
        }
    }
    return NULL_VAL;
}
/***
 * TileInfo.IsEmptySpace
 * \desc Checks to see if a tile at the ID is empty.
 * \param tileID (Integer): ID of tile to check.
 * \param collisionPlane (Integer): The collision plane of the tile to check for.
 * \return 1 if the tile is empty space, 0 if otherwise.
 * \ns TileInfo
 */
VMValue TileInfo_IsEmptySpace(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int tileID = GET_ARG(0, GetInteger);
    // int collisionPlane = GET_ARG(1, GetInteger);

    return INTEGER_VAL(tileID == Scene::EmptyTile);

    // if (collisionPlane == 0)
    //     return INTEGER_VAL(Scene::TileCfgA[tileID].CollisionTop[0] >= 0xF0);
    //
    // return INTEGER_VAL(Scene::TileCfgB[tileID].CollisionTop[0]);
}
/***
 * TileInfo.GetCollision
 * \desc Gets the collision value at the pixel position of the desired tile, -1 if no collision.
 * \param tileID (Integer): ID of the tile to get the value of.
 * \param collisionField (Integer): The collision plane of the tile to get the collision from.
 * \param directionType (Integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \param position (Integer): Position on the tile to check, X position if the directionType is Up/Down, Y position if the directionType is Left/Right.
 * \paramOpt flipX (Boolean): Whether or not to check the collision with the tile horizontally flipped.
 * \paramOpt flipY (Boolean): Whether or not to check the collision with the tile vertically flipped.
 * \return Collision position (Integer) on the tile, X position if the directionType is Left/Right, Y position if the directionType is Up/Down, -1 if there was no collision.
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

    if (!Scene::TileCfgA && collisionField == 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision A is not loaded.");
        return NULL_VAL;
    }
    if (!Scene::TileCfgB && collisionField == 1) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision B is not loaded.");
        return NULL_VAL;
    }

    TileConfig* tileCfgBase = collisionField ? Scene::TileCfgB : Scene::TileCfgA;
    tileCfgBase = &tileCfgBase[tileID] + ((flipY << 1) | flipX) * Scene::TileCount;

    int cValue = -1;
    switch (directionType) {
        case 0:
            if (tileCfgBase->CollisionTop[position] < 0xF0)
                cValue = tileCfgBase->CollisionTop[position];
            break;
        case 1:
            if (tileCfgBase->CollisionLeft[position] < 0xF0)
                cValue = tileCfgBase->CollisionLeft[position];
            break;
        case 2:
            if (tileCfgBase->CollisionBottom[position] < 0xF0)
                cValue = tileCfgBase->CollisionBottom[position];
            break;
        case 3:
            if (tileCfgBase->CollisionRight[position] < 0xF0)
                cValue = tileCfgBase->CollisionRight[position];
            break;
    }

    return INTEGER_VAL(cValue);
}
/***
 * TileInfo.GetAngle
 * \desc Gets the angle value of the desired tile.
 * \param tileID (Integer): ID of the tile to get the value of.
 * \param collisionField (Integer): The collision plane of the tile to get the angle from.
 * \param directionType (Integer): Ordinal direction to check in. (0: Down, 1: Right, 2: Up, 3: Left, or one of the enums: SensorDirection_Up, SensorDirection_Left, SensorDirection_Down, SensorDirection_Right)
 * \paramOpt flipX (Boolean): Whether or not to check the angle with the tile horizontally flipped.
 * \paramOpt flipY (Boolean): Whether or not to check the angle with the tile vertically flipped.
 * \return Angle value between 0x00 to 0xFF at the specified direction.
 * \ns TileInfo
 */
VMValue TileInfo_GetAngle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(3);
    int tileID = GET_ARG(0, GetInteger);
    int collisionField = GET_ARG(1, GetInteger);
    int directionType = GET_ARG(2, GetInteger);
    int flipX = GET_ARG_OPT(3, GetInteger, 0);
    int flipY = GET_ARG_OPT(4, GetInteger, 0);

    if (!Scene::TileCfgA && collisionField == 0) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision A is not loaded.");
        return NULL_VAL;
    }
    if (!Scene::TileCfgB && collisionField == 1) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision B is not loaded.");
        return NULL_VAL;
    }

    TileConfig* tileCfgBase = collisionField ? Scene::TileCfgB : Scene::TileCfgA;
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
 * \param tileID (Integer): ID of the tile to get the value of.
 * \param collisionPlane (Integer): The collision plane of the tile to get the behavior from.
 * \return Behavior flag (Integer) of the tile.
 * \ns TileInfo
 */
VMValue TileInfo_GetBehaviorFlag(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int tileID = GET_ARG(0, GetInteger);
    int collisionPlane = GET_ARG(1, GetInteger);

    if (collisionPlane == 0) {
        if (!Scene::TileCfgA) {
            BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision A is not loaded.");
            return NULL_VAL;
        }
        return INTEGER_VAL(Scene::TileCfgA[tileID].Behavior);
    }

    if (!Scene::TileCfgB) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "Tile Collision B is not loaded.");
        return NULL_VAL;
    }
    return INTEGER_VAL(Scene::TileCfgB[tileID].Behavior);
}
// #endregion

// #region Thread
struct _Thread_Bundle {
    ObjFunction    Callback;
    int            ArgCount;
    int            ThreadIndex;
};

int     _Thread_RunEvent(void* op) {
    _Thread_Bundle* bundle;
    bundle = (_Thread_Bundle*)op;

    VMValue*  args = (VMValue*)(bundle + 1);
    VMThread* thread = BytecodeObjectManager::Threads + bundle->ThreadIndex;
    VMValue   callbackVal = OBJECT_VAL(&bundle->Callback);

    // if (bundle->Callback.Method == NULL) {
    //     Log::Print(Log::LOG_ERROR, "No callback.");
    //     BytecodeObjectManager::ThreadCount--;
    //     free(bundle);
    //     return 0;
    // }

    thread->Push(callbackVal);
    for (int i = 0; i < bundle->ArgCount; i++) {
        thread->Push(args[i]);
    }
    thread->RunValue(callbackVal, bundle->ArgCount);

    free(bundle);

    BytecodeObjectManager::ThreadCount--;
    Log::Print(Log::LOG_IMPORTANT, "Thread %d closed.", BytecodeObjectManager::ThreadCount);
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
        Compiler::PrintValue(args[0]);
        printf("\n");
        Log::Print(Log::LOG_ERROR, "No callback.");
        return NULL_VAL;
    }

    int subArgCount = argCount - 1;

    _Thread_Bundle* bundle = (_Thread_Bundle*)malloc(sizeof(_Thread_Bundle) + subArgCount * sizeof(VMValue));
    bundle->Callback = *callback;
    bundle->Callback.Object.Next = NULL;
    bundle->ArgCount = subArgCount;
    bundle->ThreadIndex = BytecodeObjectManager::ThreadCount++;
    if (subArgCount > 0)
        memcpy(bundle + 1, args + 1, subArgCount * sizeof(VMValue));

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

    if (!resource)
        return NULL_VAL;
    delete resource;

    if (!resource->AsMedia)
        return NULL_VAL;
    delete resource->AsMedia;

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
    if (video->VideoTexture)
        return INTEGER_VAL((int)video->VideoTexture->Width);

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
    if (video->VideoTexture)
        return INTEGER_VAL((int)video->VideoTexture->Height);

    return INTEGER_VAL(-1);
}
// #endregion

// #region View
/***
 * View.SetX
 * \desc Sets the x-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param x (Number): Desired X position
 * \ns View
 */
VMValue View_SetX(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   view_index = GET_ARG(0, GetInteger);
    float value = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].X = value;
    return NULL_VAL;
}
/***
 * View.SetY
 * \desc Sets the y-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param y (Number): Desired Y position
 * \ns View
 */
VMValue View_SetY(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   view_index = GET_ARG(0, GetInteger);
    float value = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].Y = value;
    return NULL_VAL;
}
/***
 * View.SetZ
 * \desc Sets the z-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param z (Number): Desired Z position
 * \ns View
 */
VMValue View_SetZ(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   view_index = GET_ARG(0, GetInteger);
    float value = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].Z = value;
    return NULL_VAL;
}
/***
 * View.SetPosition
 * \desc Sets the position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param x (Number): Desired X position
 * \param y (Number): Desired Y position
 * \paramOpt z (Number): Desired Z position
 * \ns View
 */
VMValue View_SetPosition(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(3);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].X = GET_ARG(1, GetDecimal);
    Scene::Views[view_index].Y = GET_ARG(2, GetDecimal);
    if (argCount == 4)
        Scene::Views[view_index].Z = GET_ARG(3, GetDecimal);
    return NULL_VAL;
}
/***
 * View.SetAngle
 * \desc Sets the angle of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param x (Number): Desired X angle
 * \param y (Number): Desired Y angle
 * \param z (Number): Desired Z angle
 * \ns View
 */
VMValue View_SetAngle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(4);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].RotateX = GET_ARG(1, GetDecimal);
    Scene::Views[view_index].RotateY = GET_ARG(2, GetDecimal);
    Scene::Views[view_index].RotateZ = GET_ARG(3, GetDecimal);
    return NULL_VAL;
}
/***
 * View.GetX
 * \desc Gets the x-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Number value.
 * \ns View
 */
VMValue View_GetX(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return DECIMAL_VAL(Scene::Views[view_index].X);
}
/***
 * View.GetY
 * \desc Gets the y-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Number value.
 * \ns View
 */
VMValue View_GetY(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return DECIMAL_VAL(Scene::Views[GET_ARG(0, GetInteger)].Y);
}
/***
 * View.GetZ
 * \desc Gets the z-axis position of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Number value.
 * \ns View
 */
VMValue View_GetZ(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return DECIMAL_VAL(Scene::Views[GET_ARG(0, GetInteger)].Z);
}
/***
 * View.GetWidth
 * \desc Gets the width of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Number value.
 * \ns View
 */
VMValue View_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return DECIMAL_VAL(Scene::Views[view_index].Width);
}
/***
 * View.GetHeight
 * \desc Gets the height of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Number value.
 * \ns View
 */
VMValue View_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return DECIMAL_VAL(Scene::Views[view_index].Height);
}
/***
 * View.SetSize
 * \desc Sets the size of the camera for the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param width (Number): Desired width.
 * \param height (Number): Desired height.
 * \return
 * \ns View
 */
inline int _CEILPOW_(int n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 16;
    n++;
    return n;
}
VMValue View_SetSize(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int view_index = GET_ARG(0, GetInteger);
    float view_w = GET_ARG(1, GetDecimal);
    float view_h = GET_ARG(2, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].Width = view_w;
    Scene::Views[view_index].Height = view_h;
    Scene::Views[view_index].Stride = _CEILPOW_(view_w);
    return NULL_VAL;
}
/***
 * View.SetOutputX
 * \desc Sets the x-axis output position of the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param x (Number): Desired X position
 * \ns View
 */
VMValue View_SetOutputX(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   view_index = GET_ARG(0, GetInteger);
    float value = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].OutputX = value;
    return NULL_VAL;
}
/***
 * View.SetOutputY
 * \desc Sets the y-axis output position of the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param y (Number): Desired Y position
 * \ns View
 */
VMValue View_SetOutputY(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int   view_index = GET_ARG(0, GetInteger);
    float value = GET_ARG(1, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].OutputY = value;
    return NULL_VAL;
}
/***
 * View.SetOutputPosition
 * \desc Sets the output position of the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param x (Number): Desired X position
 * \param y (Number): Desired Y position
 * \ns View
 */
VMValue View_SetOutputPosition(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_AT_LEAST_ARGCOUNT(3);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].OutputX = GET_ARG(1, GetDecimal);
    Scene::Views[view_index].OutputY = GET_ARG(2, GetDecimal);
    return NULL_VAL;
}
/***
 * View.SetOutputSize
 * \desc Sets the output size of the specified view.
 * \param viewIndex (Integer): Index of the view.
 * \param width (Number): Desired width.
 * \param height (Number): Desired height.
 * \ns View
 */
 VMValue View_SetOutputSize(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(3);
    int view_index = GET_ARG(0, GetInteger);
    float view_w = GET_ARG(1, GetDecimal);
    float view_h = GET_ARG(2, GetDecimal);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].OutputWidth = view_w;
    Scene::Views[view_index].OutputHeight = view_h;
    return NULL_VAL;
}
/***
 * View.IsUsingDrawTarget
 * \desc Gets whether the specified camera is using a draw target or not.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Boolean value.
 * \ns View
 */
VMValue View_IsUsingDrawTarget(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return INTEGER_VAL((int)Scene::Views[view_index].UseDrawTarget);
}
/***
 * View.SetUseDrawTarget
 * \desc Sets the specified camera to use a draw target.
 * \param viewIndex (Integer): Index of the view.
 * \param useDrawTarget (Boolean):
 * \ns View
 */
VMValue View_SetUseDrawTarget(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int usedrawtar = GET_ARG(1, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].UseDrawTarget = !!usedrawtar;
    return NULL_VAL;
}
/***
 * View.IsUsingSoftwareRenderer
 * \desc Gets whether the specified camera is using the software renderer or not.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Boolean value.
 * \ns View
 */
VMValue View_IsUsingSoftwareRenderer(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return INTEGER_VAL((int)Scene::Views[view_index].Software);
}
/***
 * View.SetUseSoftwareRenderer
 * \desc Sets the specified camera to use the software renderer.
 * \param viewIndex (Integer): Index of the view.
 * \param useSoftwareRenderer (Boolean):
 * \ns View
 */
VMValue View_SetUseSoftwareRenderer(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int usedswrend = GET_ARG(1, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
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
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].UsePerspective = !!useperspec;
    Scene::Views[view_index].NearPlane = nearPlane;
    Scene::Views[view_index].FarPlane = farPlane;
    return NULL_VAL;
}
/***
 * View.IsEnabled
 * \desc Gets whether the specified camera is active or not.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns a Boolean value.
 * \ns View
 */
VMValue View_IsEnabled(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return INTEGER_VAL(Scene::Views[view_index].Active);
}
/***
 * View.SetEnabled
 * \desc Sets the specified camera to be active.
 * \param viewIndex (Integer): Index of the view.
 * \param enabled (Boolean): Whether or not the camera should be enabled.
 * \ns View
 */
VMValue View_SetEnabled(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int enabledddd = GET_ARG(1, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    enabledddd = !!enabledddd;
    if (Scene::Views[view_index].Active != enabledddd) {
        Scene::Views[view_index].Active = enabledddd;
        if (enabledddd)
            Scene::ViewsActive++;
        else
            Scene::ViewsActive--;
        Scene::SortViews();
    }
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
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].FOV = fovy;
    return NULL_VAL;
}
/***
 * View.SetPriority
 * \desc Gets the specified view's priority.
 * \param viewIndex (Integer): Index of the view.
 * \return Returns an Integer value.
 * \ns View
 */
VMValue View_SetPriority(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    int view_index = GET_ARG(0, GetInteger);
    int priority = GET_ARG(1, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    Scene::Views[view_index].Priority = priority;
    if (Scene::Views[view_index].Active)
        Scene::SortViews();
    return NULL_VAL;
}
/***
 * View.GetPriority
 * \desc Gets the specified view's priority.
 * \param viewIndex (Integer): Index of the view.
 * \param priority (Integer):
 * \ns View
 */
VMValue View_GetPriority(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    int view_index = GET_ARG(0, GetInteger);
    if (view_index < 0 || view_index >= MAX_SCENE_VIEWS) {
        BytecodeObjectManager::Threads[threadID].ThrowRuntimeError(false, "View Index out of range 0 through %d", MAX_SCENE_VIEWS - 1);
        return NULL_VAL;
    }
    return INTEGER_VAL(Scene::Views[view_index].Priority);
}
/***
 * View.GetCurrent
 * \desc Gets the current view index being drawn to.
 * \return Returns an Integer value.
 * \ns View
 */
VMValue View_GetCurrent(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    return INTEGER_VAL(Scene::ViewCurrent);
}
// #endregion

// #region Window
/***
 * Window.SetSize
 * \desc Sets the size of the active window.
 * \param width (Number): Window width
 * \param height (Number): Window height
 * \ns Window
 */
VMValue Window_SetSize(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(2);
    // HACK: Make variable "IsWindowResizeable"
    if (Application::Platform == Platforms::iOS ||
        Application::Platform == Platforms::Android)
        return NULL_VAL;

    int window_w = (int)GET_ARG(0, GetDecimal);
    int window_h = (int)GET_ARG(1, GetDecimal);
    Application::WindowWidth = window_w;
    Application::WindowHeight = window_h;
    SDL_SetWindowSize(Application::Window, window_w, window_h);

    int defaultMonitor = 0;
    Application::Settings->GetInteger("display", "defaultMonitor", &defaultMonitor);
    SDL_SetWindowPosition(Application::Window, SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor), SDL_WINDOWPOS_CENTERED_DISPLAY(defaultMonitor));

    // Incase the window just doesn't resize (Android)
    SDL_GetWindowSize(Application::Window, &window_w, &window_h);

    Graphics::Resize(window_w, window_h);
    return NULL_VAL;
}
/***
 * Window.SetFullscreen
 * \desc Sets the fullscreen state of the active window.
 * \param isFullscreen (Boolean): Whether or not the window should be fullscreen.
 * \ns Window
 */
VMValue Window_SetFullscreen(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);

    SDL_SetWindowFullscreen(Application::Window, GET_ARG(0, GetInteger) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

    int window_w, window_h;
    SDL_GetWindowSize(Application::Window, &window_w, &window_h);

    Graphics::Resize(window_w, window_h);
    return NULL_VAL;
}
/***
 * Window.SetBorderless
 * \desc Sets the bordered state of the active window.
 * \param isBorderless (Boolean): Whether or not the window should be borderless.
 * \ns Window
 */
VMValue Window_SetBorderless(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    SDL_SetWindowBordered(Application::Window, (SDL_bool)!GET_ARG(0, GetInteger));
    return NULL_VAL;
}
/***
 * Window.SetPosition
 * \desc Sets the position of the active window.
 * \param x (Number): Window x
 * \param y (Number): Window y
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
 * \param title (String): Window title
 * \ns Window
 */
VMValue Window_SetTitle(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    char* string = GET_ARG(0, GetString);
    memset(Application::WindowTitle, 0, sizeof(Application::WindowTitle));
    sprintf(Application::WindowTitle, "%s", string);
    SDL_SetWindowTitle(Application::Window, Application::WindowTitle);
    return NULL_VAL;
}
/***
 * Window.GetWidth
 * \desc Gets the width of the active window.
 * \return Returns the width of the active window.
 * \ns Window
 */
VMValue Window_GetWidth(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int v; SDL_GetWindowSize(Application::Window, &v, NULL);
    return INTEGER_VAL(v);
}
/***
 * Window.GetHeight
 * \desc Gets the height of the active window.
 * \return Returns the height of the active window.
 * \ns Window
 */
VMValue Window_GetHeight(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int v; SDL_GetWindowSize(Application::Window, NULL, &v);
    return INTEGER_VAL(v);
}
/***
 * Window.GetFullscreen
 * \desc Gets whether or not the active window is currently fullscreen.
 * \return Returns <code>true</code> if the window is fullscreen, <code>false</code> if otherwise.
 * \ns Window
 */
VMValue Window_GetFullscreen(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(0);
    int v = !!(SDL_GetWindowFlags(Application::Window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
    return INTEGER_VAL(v);
}
// #endregion

// #region XML
static VMValue _XML_FillMap(XMLNode* parent) {
    ObjMap* map = NewMap();
    Uint32 keyHash;

    XMLAttributes* attributes = &parent->attributes;
    size_t numAttr = attributes->KeyVector.size();
    size_t numChildren = parent->children.size();

    if (numChildren == 1 && !parent->children[0]->children.size()) {
        Token text = parent->children[0]->name;

        if (numAttr) {
            const char* textKey = "#text";
            keyHash = map->Keys->HashFunction(textKey, strlen(textKey));

            map->Keys->Put(keyHash, HeapCopyString(textKey, strlen(textKey)));
            map->Values->Put(keyHash, OBJECT_VAL(CopyString(text.Start, text.Length)));
        }
        else
            return OBJECT_VAL(CopyString(text.Start, text.Length));
    }
    else {
        for (size_t i = 0; i < numChildren; i++) {
            XMLNode* node = parent->children[i];

            Token *nodeName = &node->name;
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
                } else {
                    thisArray = AS_ARRAY(thisVal);
                }

                thisArray->Values->push_back(_XML_FillMap(node));
            }
            else {
                map->Keys->Put(keyHash, HeapCopyString(nodeName->Start, nodeName->Length));
                map->Values->Put(keyHash, _XML_FillMap(node));
            }
        }
    }

    // Insert attributes
    if (!numAttr)
        return OBJECT_VAL(map);

    char* attrName = NULL;

    for (size_t i = 0; i < numAttr; i++) {
        char *key = attributes->KeyVector[i];
        char *value = XMLParser::TokenToString(attributes->ValueMap.Get(key));

        attrName = (char*)realloc(attrName, strlen(key) + 2);
        if (!attrName) {
            Log::Print(Log::LOG_ERROR, "Out of memory in XML.FillMap!");
            abort();
        }

        sprintf(attrName, "#%s", key);

        keyHash = map->Keys->HashFunction(attrName, strlen(attrName));
        map->Keys->Put(keyHash, HeapCopyString(attrName, strlen(attrName)));
        map->Values->Put(keyHash, OBJECT_VAL(CopyString(value, strlen(value))));
    }

    free(attrName);

    return OBJECT_VAL(map);
}

/***
 * XML.Parse
 * \desc Decodes a String value into a Map value.
 * \param xmlText (String): XML-compliant text.
 * \return Returns a Map value if the text can be decoded, otherwise returns <code>null</code>.
 * \ns XML
 */
VMValue XML_Parse(int argCount, VMValue* args, Uint32 threadID) {
    CHECK_ARGCOUNT(1);
    if (BytecodeObjectManager::Lock()) {
        ObjString* string = AS_STRING(args[0]);
        MemoryStream* stream = MemoryStream::New(string->Chars, string->Length);

        if (stream) {
            XMLNode* xmlRoot = XMLParser::ParseFromStream(stream);

            if (xmlRoot) {
                VMValue mapValue = _XML_FillMap(xmlRoot);
                BytecodeObjectManager::Unlock();
                return mapValue;
            }
        }

        BytecodeObjectManager::Unlock();
    }
    return NULL_VAL;
}
// #endregion

#define String_CaseMapBind(lowerCase, upperCase) \
    String_ToUpperCase_Map_ExtendedASCII[(Uint8)lowerCase] = (Uint8)upperCase; \
    String_ToLowerCase_Map_ExtendedASCII[(Uint8)upperCase] = (Uint8)lowerCase;

PUBLIC STATIC void StandardLibrary::Link() {
    VMValue val;
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

    #define INIT_CLASS(className) \
        klass = NewClass(Murmur::EncryptString(#className)); \
        klass->Name = CopyString(#className, strlen(#className)); \
        val = OBJECT_VAL(klass); \
        BytecodeObjectManager::Globals->Put(klass->Hash, OBJECT_VAL(klass));
    #define DEF_NATIVE(className, funcName) \
        BytecodeObjectManager::DefineNative(klass, #funcName, className##_##funcName)

    // #region Audio
    INIT_CLASS(Audio);
    DEF_NATIVE(Audio, GetMasterVolume);
    DEF_NATIVE(Audio, GetMusicVolume);
    DEF_NATIVE(Audio, GetSoundVolume);
    DEF_NATIVE(Audio, SetMasterVolume);
    DEF_NATIVE(Audio, SetMusicVolume);
    DEF_NATIVE(Audio, SetSoundVolume);
    // #endregion

    // #region Array
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
    // #endregion

    // #region Date
    INIT_CLASS(Date);
    DEF_NATIVE(Date, GetEpoch);
    DEF_NATIVE(Date, GetTicks);
    // #endregion

    // #region Device
    INIT_CLASS(Device);
    DEF_NATIVE(Device, GetPlatform);
    DEF_NATIVE(Device, IsMobile);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Windows", (int)Platforms::Windows);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_MacOSX", (int)Platforms::MacOSX);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Linux", (int)Platforms::Linux);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Ubuntu", (int)Platforms::Ubuntu);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Switch", (int)Platforms::Switch);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_iOS", (int)Platforms::iOS);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Android", (int)Platforms::Android);
    BytecodeObjectManager::GlobalConstInteger(NULL, "Platform_Default", (int)Platforms::Default);
    // #endregion

    // #region Directory
    INIT_CLASS(Directory);
    DEF_NATIVE(Directory, Create);
    DEF_NATIVE(Directory, Exists);
    DEF_NATIVE(Directory, GetFiles);
    DEF_NATIVE(Directory, GetDirectories);
    // #endregion

    // #region Display
    INIT_CLASS(Display);
    DEF_NATIVE(Display, GetWidth);
    DEF_NATIVE(Display, GetHeight);
    // #endregion

    // #region Draw
    INIT_CLASS(Draw);
    DEF_NATIVE(Draw, Sprite);
    DEF_NATIVE(Draw, SpritePart);
    DEF_NATIVE(Draw, Image);
    DEF_NATIVE(Draw, ImagePart);
    DEF_NATIVE(Draw, ImageSized);
    DEF_NATIVE(Draw, ImagePartSized);
    DEF_NATIVE(Draw, InitArrayBuffer);
    DEF_NATIVE(Draw, BindArrayBuffer);
    DEF_NATIVE(Draw, SetAmbientLighting);
    DEF_NATIVE(Draw, SetDiffuseLighting);
    DEF_NATIVE(Draw, SetSpecularLighting);
    DEF_NATIVE(Draw, Model);
    DEF_NATIVE(Draw, ModelSimple);
    DEF_NATIVE(Draw, RenderArrayBuffer);
    DEF_NATIVE(Draw, Video);
    DEF_NATIVE(Draw, VideoPart);
    DEF_NATIVE(Draw, VideoSized);
    DEF_NATIVE(Draw, VideoPartSized);
    DEF_NATIVE(Draw, Tile);
    DEF_NATIVE(Draw, Texture);
    DEF_NATIVE(Draw, TextureSized);
    DEF_NATIVE(Draw, TexturePart);
    DEF_NATIVE(Draw, SetFont);
    DEF_NATIVE(Draw, SetTextAlign);
    DEF_NATIVE(Draw, SetTextBaseline);
    DEF_NATIVE(Draw, SetTextAdvance);
    DEF_NATIVE(Draw, SetTextLineAscent);
    DEF_NATIVE(Draw, MeasureText);
    DEF_NATIVE(Draw, MeasureTextWrapped);
    DEF_NATIVE(Draw, Text);
    DEF_NATIVE(Draw, TextWrapped);
    DEF_NATIVE(Draw, TextEllipsis);
    DEF_NATIVE(Draw, SetBlendColor);
    DEF_NATIVE(Draw, SetTextureBlend);
    DEF_NATIVE(Draw, SetBlendMode);
    DEF_NATIVE(Draw, SetBlendFactor);
    DEF_NATIVE(Draw, SetBlendFactorExtended);
    DEF_NATIVE(Draw, SetCompareColor);
    DEF_NATIVE(Draw, Line);
    DEF_NATIVE(Draw, Circle);
    DEF_NATIVE(Draw, Ellipse);
    DEF_NATIVE(Draw, Triangle);
    DEF_NATIVE(Draw, TriangleBlend);
    DEF_NATIVE(Draw, QuadBlend);
    DEF_NATIVE(Draw, Rectangle);
    DEF_NATIVE(Draw, CircleStroke);
    DEF_NATIVE(Draw, EllipseStroke);
    DEF_NATIVE(Draw, TriangleStroke);
    DEF_NATIVE(Draw, RectangleStroke);
    DEF_NATIVE(Draw, UseFillSmoothing);
    DEF_NATIVE(Draw, UseStrokeSmoothing);
    DEF_NATIVE(Draw, SetClip);
    DEF_NATIVE(Draw, ClearClip);
    DEF_NATIVE(Draw, Save);
    DEF_NATIVE(Draw, Scale);
    DEF_NATIVE(Draw, Rotate);
    DEF_NATIVE(Draw, Restore);
    DEF_NATIVE(Draw, Translate);
    DEF_NATIVE(Draw, SetTextureTarget);
    DEF_NATIVE(Draw, Clear);
    DEF_NATIVE(Draw, ResetTextureTarget);

    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_LINES", 0);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_POLYGONS", 1);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_FLAT_LIGHTING", 2);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_SMOOTH_LIGHTING", 4);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_PERSPECTIVE", 8);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_LINES_FLAT", 2);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_LINES_SMOOTH", 4);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_POLYGONS_FLAT", 3);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawMode_POLYGONS_SMOOTH", 5);

    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_ADD", BlendMode_ADD);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_MAX", BlendMode_MAX);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_NORMAL", BlendMode_NORMAL);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_SUBTRACT", BlendMode_SUBTRACT);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_MATCH_EQUAL", BlendMode_MATCH_EQUAL);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendMode_MATCH_NOT_EQUAL", BlendMode_MATCH_NOT_EQUAL);

    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_ZERO", BlendFactor_ZERO);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_ONE", BlendFactor_ONE);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_SRC_COLOR", BlendFactor_SRC_COLOR);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_INV_SRC_COLOR", BlendFactor_INV_SRC_COLOR);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_SRC_ALPHA", BlendFactor_SRC_ALPHA);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_INV_SRC_ALPHA", BlendFactor_INV_SRC_ALPHA);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_DST_COLOR", BlendFactor_DST_COLOR);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_INV_DST_COLOR", BlendFactor_INV_DST_COLOR);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_DST_ALPHA", BlendFactor_DST_ALPHA);
    BytecodeObjectManager::GlobalConstInteger(NULL, "BlendFactor_INV_DST_ALPHA", BlendFactor_INV_DST_ALPHA);
    // #endregion

    // #region Ease
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
    INIT_CLASS(File);
    DEF_NATIVE(File, Exists);
    DEF_NATIVE(File, ReadAllText);
    DEF_NATIVE(File, WriteAllText);
    // #endregion

    // #region HTTP
    INIT_CLASS(HTTP);
    DEF_NATIVE(HTTP, GetString);
    DEF_NATIVE(HTTP, GetToFile);
    // #endregion

    // #region Input
    INIT_CLASS(Input);
    DEF_NATIVE(Input, GetMouseX);
    DEF_NATIVE(Input, GetMouseY);
    DEF_NATIVE(Input, IsMouseButtonDown);
    DEF_NATIVE(Input, IsMouseButtonPressed);
    DEF_NATIVE(Input, IsMouseButtonReleased);
    DEF_NATIVE(Input, IsKeyDown);
    DEF_NATIVE(Input, IsKeyPressed);
    DEF_NATIVE(Input, IsKeyReleased);
    DEF_NATIVE(Input, GetControllerAttached);
    DEF_NATIVE(Input, GetControllerHat);
    DEF_NATIVE(Input, GetControllerAxis);
    DEF_NATIVE(Input, GetControllerButton);
    DEF_NATIVE(Input, GetControllerName);
    // DEF_NATIVE(Key, IsDown);
    // DEF_NATIVE(Key, IsPressed);
    // DEF_NATIVE(Key, IsReleased);
    // DEF_NATIVE(Controller, IsAttached);
    // DEF_NATIVE(Controller, GetHat);
    // DEF_NATIVE(Controller, GetAxis);
    // DEF_NATIVE(Controller, GetButton);
    // DEF_NATIVE(Controller, GetName);
    // DEF_NATIVE(Mouse, GetX);
    // DEF_NATIVE(Mouse, GetY);
    // DEF_NATIVE(Mouse, IsButtonDown);
    // DEF_NATIVE(Mouse, IsButtonPressed);
    // DEF_NATIVE(Mouse, IsButtonReleased);
    // #endregion

    // #region Instance
    INIT_CLASS(Instance);
    DEF_NATIVE(Instance, Create);
    DEF_NATIVE(Instance, GetNth);
    DEF_NATIVE(Instance, IsClass);
    DEF_NATIVE(Instance, GetCount);
    DEF_NATIVE(Instance, GetNextInstance);
    // #endregion

    // #region JSON
    INIT_CLASS(JSON);
    DEF_NATIVE(JSON, Parse);
    DEF_NATIVE(JSON, ToString);
    // #endregion

    // #region Math
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

    // #region Matrix
    INIT_CLASS(Matrix);
    DEF_NATIVE(Matrix, Create);
    DEF_NATIVE(Matrix, Identity);
    DEF_NATIVE(Matrix, Copy);
    DEF_NATIVE(Matrix, Multiply);
    DEF_NATIVE(Matrix, Translate);
    DEF_NATIVE(Matrix, Scale);
    DEF_NATIVE(Matrix, Rotate);
    // #endregion

    // #region Music
    INIT_CLASS(Music);
    DEF_NATIVE(Music, Play);
    DEF_NATIVE(Music, Stop);
    DEF_NATIVE(Music, StopWithFadeOut);
    DEF_NATIVE(Music, Pause);
    DEF_NATIVE(Music, Resume);
    DEF_NATIVE(Music, Clear);
    DEF_NATIVE(Music, Loop);
    DEF_NATIVE(Music, IsPlaying);
    // #endregion

    // #region Number
    INIT_CLASS(Number);
    DEF_NATIVE(Number, ToString);
    DEF_NATIVE(Number, AsInteger);
    DEF_NATIVE(Number, AsDecimal);
    // #endregion

    // #region Palette
    INIT_CLASS(Palette);
    DEF_NATIVE(Palette, EnablePaletteUsage);
    DEF_NATIVE(Palette, LoadFromFile);
    DEF_NATIVE(Palette, LoadFromResource);
    DEF_NATIVE(Palette, GetColor);
    DEF_NATIVE(Palette, SetColor);
    DEF_NATIVE(Palette, MixPalettes);
    DEF_NATIVE(Palette, RotateColorsLeft);
    DEF_NATIVE(Palette, RotateColorsRight);
    DEF_NATIVE(Palette, CopyColors);
    DEF_NATIVE(Palette, SetPaletteIndexLines);
    // #endregion

    // #region Resources
    INIT_CLASS(Resources);
    DEF_NATIVE(Resources, LoadSprite);
    DEF_NATIVE(Resources, LoadImage);
    DEF_NATIVE(Resources, LoadFont);
    DEF_NATIVE(Resources, LoadShader);
    DEF_NATIVE(Resources, LoadModel);
    DEF_NATIVE(Resources, LoadMusic);
    DEF_NATIVE(Resources, LoadSound);
    DEF_NATIVE(Resources, LoadVideo);
    DEF_NATIVE(Resources, FileExists);
    DEF_NATIVE(Resources, ReadAllText);
    DEF_NATIVE(Resources, UnloadImage);

    BytecodeObjectManager::GlobalConstInteger(NULL, "SCOPE_SCENE", 0);
    BytecodeObjectManager::GlobalConstInteger(NULL, "SCOPE_GAME", 1);
    // #endregion

    // #region Scene
    INIT_CLASS(Scene);
    DEF_NATIVE(Scene, Load);
    DEF_NATIVE(Scene, LoadTileCollisions);
    DEF_NATIVE(Scene, AreTileCollisionsLoaded);
    DEF_NATIVE(Scene, Restart);
    DEF_NATIVE(Scene, GetLayerCount);
    DEF_NATIVE(Scene, GetLayerIndex);
    DEF_NATIVE(Scene, GetName);
    DEF_NATIVE(Scene, GetWidth);
    DEF_NATIVE(Scene, GetHeight);
    DEF_NATIVE(Scene, GetTileSize);
    DEF_NATIVE(Scene, GetTileID);
    DEF_NATIVE(Scene, GetTileFlipX);
    DEF_NATIVE(Scene, GetTileFlipY);
    DEF_NATIVE(Scene, SetTile);
    DEF_NATIVE(Scene, SetTileCollisionSides);
    DEF_NATIVE(Scene, SetPaused);
    DEF_NATIVE(Scene, SetLayerVisible);
    DEF_NATIVE(Scene, SetLayerCollidable);
    DEF_NATIVE(Scene, SetLayerInternalSize);
    DEF_NATIVE(Scene, SetLayerOffsetPosition);
    DEF_NATIVE(Scene, SetLayerDrawGroup);
    DEF_NATIVE(Scene, SetLayerDrawBehavior);
    DEF_NATIVE(Scene, SetLayerBlend);
    DEF_NATIVE(Scene, SetLayerOpacity);
    DEF_NATIVE(Scene, SetLayerScroll);
    DEF_NATIVE(Scene, SetLayerSetParallaxLinesBegin);
    DEF_NATIVE(Scene, SetLayerSetParallaxLines);
    DEF_NATIVE(Scene, SetLayerSetParallaxLinesEnd);
    DEF_NATIVE(Scene, SetLayerTileDeforms);
    DEF_NATIVE(Scene, SetLayerTileDeformSplitLine);
    DEF_NATIVE(Scene, SetLayerTileDeformOffsets);
    DEF_NATIVE(Scene, SetLayerCustomScanlineFunction);
    DEF_NATIVE(Scene, SetTileScanline);
    DEF_NATIVE(Scene, SetObjectViewRender);
    DEF_NATIVE(Scene, SetTileViewRender);
    DEF_NATIVE(Scene, IsPaused);

    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawBehavior_HorizontalParallax", DrawBehavior_HorizontalParallax);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawBehavior_VerticalParallax", DrawBehavior_VerticalParallax);
    BytecodeObjectManager::GlobalConstInteger(NULL, "DrawBehavior_CustomTileScanLines", DrawBehavior_CustomTileScanLines);
    // #endregion

    // #region Settings
    INIT_CLASS(Settings);
    DEF_NATIVE(Settings, GetString);
    DEF_NATIVE(Settings, GetInteger);
    DEF_NATIVE(Settings, GetBool);
    DEF_NATIVE(Settings, SetString);
    DEF_NATIVE(Settings, SetInteger);
    DEF_NATIVE(Settings, SetBool);
    DEF_NATIVE(Settings, AddSection);
    DEF_NATIVE(Settings, RemoveSection);
    DEF_NATIVE(Settings, SectionExists);
    DEF_NATIVE(Settings, GetSectionCount);
    DEF_NATIVE(Settings, PropertyExists);
    DEF_NATIVE(Settings, RemoveProperty);
    DEF_NATIVE(Settings, GetPropertyCount);
    DEF_NATIVE(Settings, Save);
    // #endregion

    // #region Shader
    INIT_CLASS(Shader);
    DEF_NATIVE(Shader, Set);
    DEF_NATIVE(Shader, GetUniform);
    DEF_NATIVE(Shader, SetUniformI);
    DEF_NATIVE(Shader, SetUniformF);
    DEF_NATIVE(Shader, SetUniform3x3);
    DEF_NATIVE(Shader, SetUniform4x4);
    DEF_NATIVE(Shader, SetUniformTexture);
    DEF_NATIVE(Shader, Unset);
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
    INIT_CLASS(Sound);
    DEF_NATIVE(Sound, Play);
    DEF_NATIVE(Sound, Loop);
    DEF_NATIVE(Sound, Stop);
    DEF_NATIVE(Sound, Pause);
    DEF_NATIVE(Sound, Resume);
    DEF_NATIVE(Sound, StopAll);
    DEF_NATIVE(Sound, PauseAll);
    DEF_NATIVE(Sound, ResumeAll);
    DEF_NATIVE(Sound, IsPlaying);
    // #endregion

    // #region Sprite
    INIT_CLASS(Sprite);
    DEF_NATIVE(Sprite, GetAnimationCount);
    DEF_NATIVE(Sprite, GetFrameLoopIndex);
    DEF_NATIVE(Sprite, GetFrameCount);
    DEF_NATIVE(Sprite, GetFrameDuration);
    DEF_NATIVE(Sprite, GetFrameSpeed);
    // #endregion

    // #region String
    INIT_CLASS(String);
    DEF_NATIVE(String, Split);
    DEF_NATIVE(String, CharAt);
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
    // #endregion

    // #region Texture
    INIT_CLASS(Texture);
    DEF_NATIVE(Texture, FromSprite);
    DEF_NATIVE(Texture, FromImage);
    DEF_NATIVE(Texture, Create);
    DEF_NATIVE(Texture, SetInterpolation);
    // #endregion

    // #region Touch
    INIT_CLASS(Touch);
    DEF_NATIVE(Touch, GetX);
    DEF_NATIVE(Touch, GetY);
    DEF_NATIVE(Touch, IsDown);
    DEF_NATIVE(Touch, IsPressed);
    DEF_NATIVE(Touch, IsReleased);
    // #endregion

    // #region TileCollision
    INIT_CLASS(TileCollision);
    DEF_NATIVE(TileCollision, Point);
    DEF_NATIVE(TileCollision, PointExtended);
    DEF_NATIVE(TileCollision, Line);
    BytecodeObjectManager::GlobalConstInteger(NULL, "SensorDirection_Down", 0);
    BytecodeObjectManager::GlobalConstInteger(NULL, "SensorDirection_Right", 1);
    BytecodeObjectManager::GlobalConstInteger(NULL, "SensorDirection_Up", 2);
    BytecodeObjectManager::GlobalConstInteger(NULL, "SensorDirection_Left", 3);
    // #endregion

    // #region TileInfo
    INIT_CLASS(TileInfo);
    DEF_NATIVE(TileInfo, SetSpriteInfo);
    DEF_NATIVE(TileInfo, IsEmptySpace);
    DEF_NATIVE(TileInfo, GetCollision);
    DEF_NATIVE(TileInfo, GetAngle);
    DEF_NATIVE(TileInfo, GetBehaviorFlag);
    // #endregion

    // #region Thread
    INIT_CLASS(Thread);
    DEF_NATIVE(Thread, RunEvent);
    DEF_NATIVE(Thread, Sleep);
    // #endregion

    // #region Video
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
    INIT_CLASS(View);
    DEF_NATIVE(View, SetX);
    DEF_NATIVE(View, SetY);
    DEF_NATIVE(View, SetZ);
    DEF_NATIVE(View, SetPosition);
    DEF_NATIVE(View, SetAngle);
    DEF_NATIVE(View, SetSize);
    DEF_NATIVE(View, GetX);
    DEF_NATIVE(View, GetY);
    DEF_NATIVE(View, GetZ);
    DEF_NATIVE(View, GetWidth);
    DEF_NATIVE(View, GetHeight);
    DEF_NATIVE(View, SetOutputX);
    DEF_NATIVE(View, SetOutputY);
    DEF_NATIVE(View, SetOutputPosition);
    DEF_NATIVE(View, SetOutputSize);
    DEF_NATIVE(View, IsUsingDrawTarget);
    DEF_NATIVE(View, SetUseDrawTarget);
    DEF_NATIVE(View, SetUsePerspective);
    DEF_NATIVE(View, IsUsingSoftwareRenderer);
    DEF_NATIVE(View, SetUseSoftwareRenderer);
    DEF_NATIVE(View, IsEnabled);
    DEF_NATIVE(View, SetEnabled);
    DEF_NATIVE(View, SetFieldOfView);
    DEF_NATIVE(View, SetPriority);
    DEF_NATIVE(View, GetPriority);
    DEF_NATIVE(View, GetCurrent);
    // #endregion

    // #region Window
    INIT_CLASS(Window);
    DEF_NATIVE(Window, SetSize);
    DEF_NATIVE(Window, SetFullscreen);
    DEF_NATIVE(Window, SetBorderless);
    DEF_NATIVE(Window, SetPosition);
    DEF_NATIVE(Window, SetPositionCentered);
    DEF_NATIVE(Window, SetTitle);
    DEF_NATIVE(Window, GetWidth);
    DEF_NATIVE(Window, GetHeight);
    DEF_NATIVE(Window, GetFullscreen);
    // #endregion

    // #region XML
    INIT_CLASS(XML);
    DEF_NATIVE(XML, Parse);
    // #endregion

    #undef DEF_NATIVE
    #undef INIT_CLASS

    BytecodeObjectManager::Globals->Put("other", NULL_VAL);

    BytecodeObjectManager::GlobalLinkDecimal(NULL, "CameraX", &Scene::Views[0].X);
    BytecodeObjectManager::GlobalLinkDecimal(NULL, "CameraY", &Scene::Views[0].Y);
    BytecodeObjectManager::GlobalLinkDecimal(NULL, "LowPassFilter", &AudioManager::LowPassFilter);

    BytecodeObjectManager::GlobalLinkInteger(NULL, "Scene_Frame", &Scene::Frame);
    BytecodeObjectManager::GlobalConstDecimal(NULL, "Scene_MaxViews", MAX_SCENE_VIEWS);

    BytecodeObjectManager::GlobalConstDecimal(NULL, "Math_PI", M_PI);
    BytecodeObjectManager::GlobalConstDecimal(NULL, "Math_PI_DOUBLE", M_PI * 2.0);
    BytecodeObjectManager::GlobalConstDecimal(NULL, "Math_PI_HALF", M_PI / 2.0);

    #define CONST_KEY(key) BytecodeObjectManager::GlobalConstInteger(NULL, "Key_"#key, SDL_SCANCODE_##key);
    {
        CONST_KEY(A);
        CONST_KEY(B);
        CONST_KEY(C);
        CONST_KEY(D);
        CONST_KEY(E);
        CONST_KEY(F);
        CONST_KEY(G);
        CONST_KEY(H);
        CONST_KEY(I);
        CONST_KEY(J);
        CONST_KEY(K);
        CONST_KEY(L);
        CONST_KEY(M);
        CONST_KEY(N);
        CONST_KEY(O);
        CONST_KEY(P);
        CONST_KEY(Q);
        CONST_KEY(R);
        CONST_KEY(S);
        CONST_KEY(T);
        CONST_KEY(U);
        CONST_KEY(V);
        CONST_KEY(W);
        CONST_KEY(X);
        CONST_KEY(Y);
        CONST_KEY(Z);

        CONST_KEY(1);
        CONST_KEY(2);
        CONST_KEY(3);
        CONST_KEY(4);
        CONST_KEY(5);
        CONST_KEY(6);
        CONST_KEY(7);
        CONST_KEY(8);
        CONST_KEY(9);
        CONST_KEY(0);

        CONST_KEY(RETURN);
        CONST_KEY(ESCAPE);
        CONST_KEY(BACKSPACE);
        CONST_KEY(TAB);
        CONST_KEY(SPACE);

        CONST_KEY(MINUS);
        CONST_KEY(EQUALS);
        CONST_KEY(LEFTBRACKET);
        CONST_KEY(RIGHTBRACKET);
        CONST_KEY(BACKSLASH);
        CONST_KEY(SEMICOLON);
        CONST_KEY(APOSTROPHE);
        CONST_KEY(GRAVE);
        CONST_KEY(COMMA);
        CONST_KEY(PERIOD);
        CONST_KEY(SLASH);

        CONST_KEY(CAPSLOCK);

        CONST_KEY(SEMICOLON);

        CONST_KEY(F1);
        CONST_KEY(F2);
        CONST_KEY(F3);
        CONST_KEY(F4);
        CONST_KEY(F5);
        CONST_KEY(F6);
        CONST_KEY(F7);
        CONST_KEY(F8);
        CONST_KEY(F9);
        CONST_KEY(F10);
        CONST_KEY(F11);
        CONST_KEY(F12);

        CONST_KEY(PRINTSCREEN);
        CONST_KEY(SCROLLLOCK);
        CONST_KEY(PAUSE);
        CONST_KEY(INSERT);
        CONST_KEY(HOME);
        CONST_KEY(PAGEUP);
        CONST_KEY(DELETE);
        CONST_KEY(END);
        CONST_KEY(PAGEDOWN);
        CONST_KEY(RIGHT);
        CONST_KEY(LEFT);
        CONST_KEY(DOWN);
        CONST_KEY(UP);

        CONST_KEY(NUMLOCKCLEAR);
        CONST_KEY(KP_DIVIDE);
        CONST_KEY(KP_MULTIPLY);
        CONST_KEY(KP_MINUS);
        CONST_KEY(KP_PLUS);
        CONST_KEY(KP_ENTER);
        CONST_KEY(KP_1);
        CONST_KEY(KP_2);
        CONST_KEY(KP_3);
        CONST_KEY(KP_4);
        CONST_KEY(KP_5);
        CONST_KEY(KP_6);
        CONST_KEY(KP_7);
        CONST_KEY(KP_8);
        CONST_KEY(KP_9);
        CONST_KEY(KP_0);
        CONST_KEY(KP_PERIOD);

        CONST_KEY(LCTRL);
        CONST_KEY(LSHIFT);
        CONST_KEY(LALT);
        CONST_KEY(LGUI);
        CONST_KEY(RCTRL);
        CONST_KEY(RSHIFT);
        CONST_KEY(RALT);
        CONST_KEY(RGUI);
    }
    #undef  CONST_KEY
}
PUBLIC STATIC void StandardLibrary::Dispose() {

}
