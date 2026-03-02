#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/InstanceImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>
#include <Engine/Bytecode/Types.h>
#include <Engine/Error.h>
#include <Engine/Rendering/Material.h>

/***
* \class Material
* \desc Representation of a 3D model material.
*/

MaterialImpl::MaterialImpl(ScriptManager* manager) {
	Manager = manager;
	Class = Manager->NewClass(CLASS_MATERIAL);
	Class->NewFn = New;
	Class->Initializer = OBJECT_VAL(Manager->NewNative(VM_Initializer));

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

	TypeImpl::RegisterClass(manager, Class);
	TypeImpl::ExposeClass(manager, CLASS_MATERIAL, Class);

	TypeImpl::DefinePrintableName(Class, "material");
}

#define GET_ARG(argIndex, argFunction) (thread->Manager->argFunction(args, argIndex, thread))

/***
 * \constructor
 * \desc Creates a material.
 * \ns Material
 */
Obj* MaterialImpl::New(VMThread* thread) {
	Material* materialPtr = Material::Create(nullptr);
	return (Obj*)thread->Manager->ImplMaterial->New((void*)materialPtr);
}
ObjMaterial* MaterialImpl::New(void* materialPtr) {
	ObjMaterial* material = (ObjMaterial*)Manager->NewNativeInstance(sizeof(ObjMaterial));
	Memory::Track(material, "NewMaterial");
	material->Object.Class = Class;
	material->InstanceObj.PropertyGet = VM_PropertyGet;
	material->InstanceObj.PropertySet = VM_PropertySet;
	material->InstanceObj.Destructor = Dispose;
	material->MaterialPtr = (Material*)materialPtr;
	return material;
}
void MaterialImpl::Dispose(Obj* object) {
	ObjMaterial* material = (ObjMaterial*)object;

	Material::Remove(material->MaterialPtr);

	InstanceImpl::Dispose(object);
}

VMValue MaterialImpl::VM_Initializer(int argCount, VMValue* args, VMThread* thread) {
	ObjMaterial* objMaterial = AS_MATERIAL(args[0]);
	Material* material = objMaterial->MaterialPtr;

	ScriptManager::CheckArgCount(argCount, 2, thread);

	char* name = GET_ARG(1, GetString);

	material->Name = StringUtils::Duplicate(name);

	return OBJECT_VAL(objMaterial);
}

#undef GET_ARG

#define IS_FIELD(fieldName) (hash == thread->Manager->ImplMaterial->Hash_##fieldName)

#define GET_COLOR(type, col, idx) \
	{ \
		if (hash == thread->Manager->ImplMaterial->Hash_##type##col) { \
			if (result) \
				*result = DECIMAL_VAL(material->Color##type[idx]); \
			return true; \
		} \
	}

#define GET_TEXTURE(type) \
	{ \
		if (hash == thread->Manager->ImplMaterial->Hash_##type##Texture) { \
			Image* image = material->Texture##type; \
			if (image != nullptr) { \
				*result = INTEGER_VAL(image->ID); \
			} \
			else { \
				*result = NULL_VAL; \
			} \
			return true; \
		} \
	}

bool MaterialImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, VMThread* thread) {
	ObjMaterial* objMaterial = (ObjMaterial*)object;
	Material* material = objMaterial->MaterialPtr;
	if (material == nullptr) {
		thread->ThrowRuntimeError(
			false, "Material is no longer valid!");
		return false;
	}

	if (IS_FIELD(Name)) {
		if (thread->Manager->Lock()) {
			ObjString* string = thread->Manager->CopyString(material->Name);
			thread->Manager->Unlock();
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
		if (hash == thread->Manager->ImplMaterial->Hash_##type##col) { \
			if (thread->DoDecimalConversion(value)) \
				material->Color##type[idx] = AS_DECIMAL(value); \
			return true; \
		} \
	}

static void DoTextureRemoval(Image** image) {
	if (*image) {
		if ((*image)->TakeRef()) {
			Error::Fatal("Unexpected reference count for Image!");
		}

		(*image) = nullptr;
	}
}

static void DoTextureReplacement(int imageID, Image** image, VMThread* thread) {
	ResourceType* resource = Scene::GetImageResource(imageID);
	if (!resource) {
		thread->ThrowRuntimeError(
			false, "Image index \"%d\" is not valid!", imageID);
		return;
	}

	DoTextureRemoval(image);

	Image* newImage = resource->AsImage;
	newImage->AddRef();
	(*image) = newImage;
}

#define SET_TEXTURE(type) \
	{ \
		if (hash == thread->Manager->ImplMaterial->Hash_##type##Texture) { \
			if (IS_NULL(value)) { \
				DoTextureRemoval(&material->Texture##type); \
			} \
			else if (thread->DoIntegerConversion(value)) { \
				DoTextureReplacement( \
					AS_INTEGER(value), &material->Texture##type, thread); \
			} \
			return true; \
		} \
	}

bool MaterialImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, VMThread* thread) {
	ObjMaterial* objMaterial = (ObjMaterial*)object;
	Material* material = objMaterial->MaterialPtr;
	if (material == nullptr) {
		thread->ThrowRuntimeError(
			false, "Material is no longer valid!");
		return false;
	}

	if (IS_FIELD(Name)) {
		thread->ThrowRuntimeError(
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

#undef IS_FIELD

#undef SET_TEXTURE
