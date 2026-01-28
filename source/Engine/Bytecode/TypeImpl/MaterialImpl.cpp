#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Rendering/Material.h>
#include <Engine/ResourceTypes/Resource.h>

/***
* \class Material
* \desc Representation of a 3D model material.
*/

ObjClass* MaterialImpl::Class = nullptr;

static Uint32 Hash_Name = 0;

static Uint32 Hash_DiffuseRed = 0;
static Uint32 Hash_DiffuseGreen = 0;
static Uint32 Hash_DiffuseBlue = 0;
static Uint32 Hash_DiffuseAlpha = 0;
static Uint32 Hash_DiffuseTexture = 0;

static Uint32 Hash_SpecularRed = 0;
static Uint32 Hash_SpecularGreen = 0;
static Uint32 Hash_SpecularBlue = 0;
static Uint32 Hash_SpecularAlpha = 0;
static Uint32 Hash_SpecularTexture = 0;

static Uint32 Hash_AmbientRed = 0;
static Uint32 Hash_AmbientGreen = 0;
static Uint32 Hash_AmbientBlue = 0;
static Uint32 Hash_AmbientAlpha = 0;
static Uint32 Hash_AmbientTexture = 0;

#ifdef MATERIAL_EXPOSE_EMISSIVE
static Uint32 Hash_EmissiveRed = 0;
static Uint32 Hash_EmissiveGreen = 0;
static Uint32 Hash_EmissiveBlue = 0;
static Uint32 Hash_EmissiveAlpha = 0;
static Uint32 Hash_EmissiveTexture = 0;
#endif

void MaterialImpl::Init() {
	Class = NewClass(CLASS_MATERIAL);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));

	Hash_Name = Murmur::EncryptString("Name");

	/***
    * \field DiffuseRed
    * \type decimal
    * \ns Material
    * \desc The red component of the diffuse color.
    */
	Hash_DiffuseRed = Murmur::EncryptString("DiffuseRed");
	/***
    * \field DiffuseGreen
    * \type decimal
    * \ns Material
    * \desc The green component of the diffuse color.
    */
	Hash_DiffuseGreen = Murmur::EncryptString("DiffuseGreen");
	/***
    * \field DiffuseBlue
    * \type decimal
    * \ns Material
    * \desc The blue component of the diffuse color.
    */
	Hash_DiffuseBlue = Murmur::EncryptString("DiffuseBlue");
	/***
    * \field DiffuseAlpha
    * \type decimal
    * \ns Material
    * \desc The alpha component of the diffuse color.
    */
	Hash_DiffuseAlpha = Murmur::EncryptString("DiffuseAlpha");
	/***
    * \field DiffuseTexture
    * \type integer
    * \ns Material
    * \desc The diffuse texture.
    */
	Hash_DiffuseTexture = Murmur::EncryptString("DiffuseTexture");

	/***
    * \field SpecularRed
    * \type decimal
    * \ns Material
    * \desc The red component of the specular color.
    */
	Hash_SpecularRed = Murmur::EncryptString("SpecularRed");
	/***
    * \field SpecularGreen
    * \type decimal
    * \ns Material
    * \desc The green component of the specular color.
    */
	Hash_SpecularGreen = Murmur::EncryptString("SpecularGreen");
	/***
    * \field SpecularBlue
    * \type decimal
    * \ns Material
    * \desc The blue component of the specular color.
    */
	Hash_SpecularBlue = Murmur::EncryptString("SpecularBlue");
	/***
    * \field SpecularAlpha
    * \type decimal
    * \ns Material
    * \desc The alpha component of the specular color.
    */
	Hash_SpecularAlpha = Murmur::EncryptString("SpecularAlpha");
	/***
    * \field SpecularTexture
    * \type integer
    * \ns Material
    * \desc The specular texture.
    */
	Hash_SpecularTexture = Murmur::EncryptString("SpecularTexture");

	/***
    * \field AmbientRed
    * \type decimal
    * \ns Material
    * \desc The red component of the ambient color.
    */
	Hash_AmbientRed = Murmur::EncryptString("AmbientRed");
	/***
    * \field AmbientGreen
    * \type decimal
    * \ns Material
    * \desc The green component of the ambient color.
    */
	Hash_AmbientGreen = Murmur::EncryptString("AmbientGreen");
	/***
    * \field AmbientBlue
    * \type decimal
    * \ns Material
    * \desc The blue component of the ambient color.
    */
	Hash_AmbientBlue = Murmur::EncryptString("AmbientBlue");
	/***
    * \field AmbientAlpha
    * \type decimal
    * \ns Material
    * \desc The alpha component of the ambient color.
    */
	Hash_AmbientAlpha = Murmur::EncryptString("AmbientAlpha");
	/***
    * \field AmbientTexture
    * \type integer
    * \ns Material
    * \desc The ambient texture.
    */
	Hash_AmbientTexture = Murmur::EncryptString("AmbientTexture");

#ifdef MATERIAL_EXPOSE_EMISSIVE
	Hash_EmissiveRed = Murmur::EncryptString("EmissiveRed");
	Hash_EmissiveGreen = Murmur::EncryptString("EmissiveGreen");
	Hash_EmissiveBlue = Murmur::EncryptString("EmissiveBlue");
	Hash_EmissiveAlpha = Murmur::EncryptString("EmissiveAlpha");
	Hash_EmissiveTexture = Murmur::EncryptString("EmissiveTexture");
#endif

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_MATERIAL, Class);
	TypeImpl::DefinePrintableName(Class, "material");
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

/***
 * \constructor
 * \desc Creates a material.
 * \ns Material
 */
Obj* MaterialImpl::New() {
	Material* materialPtr = Material::Create(nullptr);
	return (Obj*)New((void*)materialPtr);
}
ObjMaterial* MaterialImpl::New(void* materialPtr) {
	ObjMaterial* material = (ObjMaterial*)NewNativeInstance(sizeof(ObjMaterial));
	Memory::Track(material, "NewMaterial");
	material->Object.Class = Class;
	material->Object.PropertyGet = VM_PropertyGet;
	material->Object.PropertySet = VM_PropertySet;
	material->Object.Destructor = Dispose;
	material->MaterialPtr = (Material*)materialPtr;
	return material;
}
void MaterialImpl::Dispose(Obj* object) {
	ObjMaterial* material = (ObjMaterial*)object;

	Material::Remove((Material*)material->MaterialPtr);

	InstanceImpl::Dispose(object);
}

VMValue MaterialImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
	ObjMaterial* objMaterial = AS_MATERIAL(args[0]);
	Material* material = (Material*)objMaterial->MaterialPtr;

	StandardLibrary::CheckArgCount(argCount, 2);

	char* name = GET_ARG(1, GetString);

	material->Name = StringUtils::Duplicate(name);

	return OBJECT_VAL(objMaterial);
}

#undef GET_ARG

#define GET_COLOR(type, col, idx) \
	{ \
		if (hash == Hash_##type##col) { \
			if (result) \
				*result = DECIMAL_VAL(material->Color##type[idx]); \
			return true; \
		} \
	}

#define GET_TEXTURE(type) \
	{ \
		if (hash == Hash_##type##Texture) { \
			Image* resource = (Image*)material->Texture##type; \
			if (resource != nullptr) { \
				*result = OBJECT_VAL(resource->GetVMObject()); \
			} \
			else { \
				*result = NULL_VAL; \
			} \
			return true; \
		} \
	}

bool MaterialImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	ObjMaterial* objMaterial = (ObjMaterial*)object;
	Material* material = (Material*)objMaterial->MaterialPtr;

	if (hash == Hash_Name) {
		if (ScriptManager::Lock()) {
			ObjString* string = CopyString(material->Name);
			ScriptManager::Unlock();
			*result = OBJECT_VAL(string);
		}
		else {
			*result = NULL_VAL;
		}
		return true;
	}

	GET_COLOR(Diffuse, Red, 0);
	GET_COLOR(Diffuse, Green, 1);
	GET_COLOR(Diffuse, Blue, 2);
	GET_COLOR(Diffuse, Alpha, 3);
	GET_TEXTURE(Diffuse);

	GET_COLOR(Specular, Red, 0);
	GET_COLOR(Specular, Green, 1);
	GET_COLOR(Specular, Blue, 2);
	GET_COLOR(Specular, Alpha, 3);
	GET_TEXTURE(Specular);

	GET_COLOR(Ambient, Red, 0);
	GET_COLOR(Ambient, Green, 1);
	GET_COLOR(Ambient, Blue, 2);
	GET_COLOR(Ambient, Alpha, 3);
	GET_TEXTURE(Ambient);

#ifdef MATERIAL_EXPOSE_EMISSIVE
	GET_COLOR(Emissive, Red, 0);
	GET_COLOR(Emissive, Green, 1);
	GET_COLOR(Emissive, Blue, 2);
	GET_COLOR(Emissive, Alpha, 3);
	GET_TEXTURE(Emissive);
#endif

	return false;
}

#undef GET_COLOR

#define SET_COLOR(type, col, idx) \
	{ \
		if (hash == Hash_##type##col) { \
			if (ScriptManager::DoDecimalConversion(value, threadID)) \
				material->Color##type[idx] = AS_DECIMAL(value); \
			return true; \
		} \
	}

#define SET_TEXTURE(type) \
	{ \
		if (hash == Hash_##type##Texture) { \
			if (IS_NULL(value)) { \
				material->SetTexture(&material->Texture##type, nullptr); \
			} \
			else { \
				Image* image = (Image*)StandardLibrary::GetAsset(ASSET_IMAGE, value, threadID); \
				material->SetTexture(&material->Texture##type, (void*)image); \
			} \
			return true; \
		} \
	}

bool MaterialImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	ObjMaterial* objMaterial = (ObjMaterial*)object;
	Material* material = (Material*)objMaterial->MaterialPtr;

	if (hash == Hash_Name) {
		ScriptManager::Threads[threadID].ThrowRuntimeError(
			false, "Field cannot be written to!");
		return true;
	}

	SET_COLOR(Diffuse, Red, 0);
	SET_COLOR(Diffuse, Green, 1);
	SET_COLOR(Diffuse, Blue, 2);
	SET_COLOR(Diffuse, Alpha, 3);
	SET_TEXTURE(Diffuse);

	SET_COLOR(Specular, Red, 0);
	SET_COLOR(Specular, Green, 1);
	SET_COLOR(Specular, Blue, 2);
	SET_COLOR(Specular, Alpha, 3);
	SET_TEXTURE(Specular);

	SET_COLOR(Ambient, Red, 0);
	SET_COLOR(Ambient, Green, 1);
	SET_COLOR(Ambient, Blue, 2);
	SET_COLOR(Ambient, Alpha, 3);
	SET_TEXTURE(Ambient);

#ifdef MATERIAL_EXPOSE_EMISSIVE
	SET_COLOR(Emissive, Red, 0);
	SET_COLOR(Emissive, Green, 1);
	SET_COLOR(Emissive, Blue, 2);
	SET_COLOR(Emissive, Alpha, 3);
	SET_TEXTURE(Emissive);
#endif

	return false;
}

#undef SET_TEXTURE
