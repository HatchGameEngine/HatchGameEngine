#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/ColorImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>

/***
* \class Color
* \desc Representation of a color in RGBA format.
*/

ObjClass* ColorImpl::Class = nullptr;

Uint32 Hash_Red = 0;
Uint32 Hash_Green = 0;
Uint32 Hash_Blue = 0;
Uint32 Hash_Alpha = 0;

void ColorImpl::Init() {
	Class = NewClass(CLASS_COLOR);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	/***
    * \field Red
    * \type decimal
    * \ns Color
    * \desc The red channel.
    */
	Hash_Red = Murmur::EncryptString("Red");
	/***
    * \field Green
    * \type decimal
    * \ns Color
    * \desc The green channel.
    */
	Hash_Green = Murmur::EncryptString("Green");
	/***
    * \field Blue
    * \type decimal
    * \ns Color
    * \desc The blue channel.
    */
	Hash_Blue = Murmur::EncryptString("Blue");
	/***
    * \field Alpha
    * \type decimal
    * \ns Color
    * \desc The alpha channel.
    */
	Hash_Alpha = Murmur::EncryptString("Alpha");

	ScriptManager::DefineNative(Class, "Inverted", VM_Inverted);

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_COLOR, Class);
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)
#define THROW_ERROR(...) ScriptManager::Threads[threadID].ThrowRuntimeError(false, __VA_ARGS__)

/***
 * \constructor
 * \desc Creates a color.
 * \ns Color
 */
VMValue ColorImpl::New() {
	VMValue val;
	val.Type = VAL_COLOR;
	AS_COLOR(val).Red = 0.0;
	AS_COLOR(val).Green = 0.0;
	AS_COLOR(val).Blue = 0.0;
	AS_COLOR(val).Alpha = 0.0;
	return val;
}

VMValue ColorImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 1);

	VMColor& color = AS_COLOR(args[0]);
	color.Red = GET_ARG_OPT(1, GetDecimal, 0.0);
	color.Green = GET_ARG_OPT(2, GetDecimal, 0.0);
	color.Blue = GET_ARG_OPT(3, GetDecimal, 0.0);
	color.Alpha = GET_ARG_OPT(4, GetDecimal, 0.0);

	return args[0];
}

#undef GET_ARG
#undef GET_ARG_OPT

bool ColorImpl::VM_PropertyGet(VMValue instance, Uint32 hash, VMValue* result, Uint32 threadID) {
	VMColor color = AS_COLOR(instance);

	if (hash == Hash_Red) {
		if (result) {
			*result = DECIMAL_VAL(color.Red);
		}
		return true;
	}
	else if (hash == Hash_Green) {
		if (result) {
			*result = DECIMAL_VAL(color.Green);
		}
		return true;
	}
	else if (hash == Hash_Blue) {
		if (result) {
			*result = DECIMAL_VAL(color.Blue);
		}
		return true;
	}
	else if (hash == Hash_Alpha) {
		if (result) {
			*result = DECIMAL_VAL(color.Alpha);
		}
		return true;
	}

	return false;
}

bool ColorImpl::VM_PropertySet(VMValue& instance, Uint32 hash, VMValue value, Uint32 threadID) {
	VMColor& color = AS_COLOR(instance);

	if (hash == Hash_Red) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			color.Red = AS_DECIMAL(value);
		}
		return true;
	}
	else if (hash == Hash_Green) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			color.Green = AS_DECIMAL(value);
		}
		return true;
	}
	else if (hash == Hash_Blue) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			color.Blue = AS_DECIMAL(value);
		}
		return true;
	}
	else if (hash == Hash_Alpha) {
		if (ScriptManager::DoDecimalConversion(value, threadID)) {
			color.Alpha = AS_DECIMAL(value);
		}
		return true;
	}

	return false;
}

bool ColorImpl::VM_ElementGet(VMValue instance, VMValue at, VMValue* result, Uint32 threadID) {
	VMColor color = AS_COLOR(instance);

	if (!IS_INTEGER(at)) {
		THROW_ERROR("Cannot get value from color using non-Integer value as an index.");
		if (result) {
			*result = NULL_VAL;
		}
		return true;
	}

	float value;

	int index = AS_INTEGER(at);
	switch (index) {
	case 0:
		value = color.Red;
		break;
	case 1:
		value = color.Green;
		break;
	case 2:
		value = color.Blue;
		break;
	case 3:
		value = color.Alpha;
		break;
	default:
		THROW_ERROR("Index %d is out of bounds for color value.", index);
		if (result) {
			*result = NULL_VAL;
		}
		return true;
	}

	if (result) {
		*result = DECIMAL_VAL(value);
	}
	return true;
}

bool ColorImpl::VM_ElementSet(VMValue& instance, VMValue at, VMValue value, Uint32 threadID) {
	VMColor& color = AS_COLOR(instance);

	if (!IS_INTEGER(at)) {
		THROW_ERROR("Cannot get value from color using non-Integer value as an index.");
		return true;
	}

	if (!ScriptManager::DoDecimalConversion(value, threadID)) {
		return true;
	}

	int index = AS_INTEGER(at);
	switch (index) {
	case 0:
		color.Red = AS_DECIMAL(value);
		break;
	case 1:
		color.Green = AS_DECIMAL(value);
		break;
	case 2:
		color.Blue = AS_DECIMAL(value);
		break;
	case 3:
		color.Alpha = AS_DECIMAL(value);
		break;
	default:
		THROW_ERROR("Index %d is out of bounds for color value.", index);
		break;
	}

	return true;
}

/***
 * \method Inverted
 * \desc Returns the color with its red, green, and blue components inverted.
 * \return Color Returns a Color value.
 * \ns Color
 */
VMValue ColorImpl::VM_Inverted(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	VMColor color = StandardLibrary::GetColor(args, 0, threadID);
	color.Red = 1.0f - color.Red;
	color.Green = 1.0f - color.Green;
	color.Blue = 1.0f - color.Blue;
	return COLOR_VAL(color);
}
