#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Rendering/Material.h>

ObjClass* MaterialImpl::Class = nullptr;

Uint32 MaterialImpl::Hash_DiffuseRed = 0;
Uint32 MaterialImpl::Hash_DiffuseGreen = 0;
Uint32 MaterialImpl::Hash_DiffuseBlue = 0;
Uint32 MaterialImpl::Hash_DiffuseAlpha = 0;
Uint32 MaterialImpl::Hash_DiffuseTexture = 0;

Uint32 MaterialImpl::Hash_SpecularRed = 0;
Uint32 MaterialImpl::Hash_SpecularGreen = 0;
Uint32 MaterialImpl::Hash_SpecularBlue = 0;
Uint32 MaterialImpl::Hash_SpecularAlpha = 0;
Uint32 MaterialImpl::Hash_SpecularTexture = 0;

Uint32 MaterialImpl::Hash_AmbientRed = 0;
Uint32 MaterialImpl::Hash_AmbientGreen = 0;
Uint32 MaterialImpl::Hash_AmbientBlue = 0;
Uint32 MaterialImpl::Hash_AmbientAlpha = 0;
Uint32 MaterialImpl::Hash_AmbientTexture = 0;

Uint32 MaterialImpl::Hash_EmissiveRed = 0;
Uint32 MaterialImpl::Hash_EmissiveGreen = 0;
Uint32 MaterialImpl::Hash_EmissiveBlue = 0;
Uint32 MaterialImpl::Hash_EmissiveAlpha = 0;
Uint32 MaterialImpl::Hash_EmissiveTexture = 0;

void MaterialImpl::Init() {
    const char *className = "Material";

    Class = NewClass(Murmur::EncryptString(className));
    Class->Name = CopyString(className);
    Class->NewFn = VM_New;
    Class->Initializer = OBJECT_VAL(NewNative(VM_Initializer));
    Class->PropertyGet = VM_PropertyGet;
    Class->PropertySet = VM_PropertySet;

    Hash_DiffuseRed = Murmur::EncryptString("DiffuseRed");
    Hash_DiffuseGreen = Murmur::EncryptString("DiffuseGreen");
    Hash_DiffuseBlue = Murmur::EncryptString("DiffuseBlue");
    Hash_DiffuseAlpha = Murmur::EncryptString("DiffuseAlpha");
    Hash_DiffuseTexture = Murmur::EncryptString("DiffuseTexture");

    Hash_SpecularRed = Murmur::EncryptString("SpecularRed");
    Hash_SpecularGreen = Murmur::EncryptString("SpecularGreen");
    Hash_SpecularBlue = Murmur::EncryptString("SpecularBlue");
    Hash_SpecularAlpha = Murmur::EncryptString("SpecularAlpha");
    Hash_SpecularTexture = Murmur::EncryptString("SpecularTexture");

    Hash_AmbientRed = Murmur::EncryptString("AmbientRed");
    Hash_AmbientGreen = Murmur::EncryptString("AmbientGreen");
    Hash_AmbientBlue = Murmur::EncryptString("AmbientBlue");
    Hash_AmbientAlpha = Murmur::EncryptString("AmbientAlpha");
    Hash_AmbientTexture = Murmur::EncryptString("AmbientTexture");

    Hash_EmissiveRed = Murmur::EncryptString("EmissiveRed");
    Hash_EmissiveGreen = Murmur::EncryptString("EmissiveGreen");
    Hash_EmissiveBlue = Murmur::EncryptString("EmissiveBlue");
    Hash_EmissiveAlpha = Murmur::EncryptString("EmissiveAlpha");
    Hash_EmissiveTexture = Murmur::EncryptString("EmissiveTexture");

    ScriptManager::ClassImplList.push_back(Class);

    ScriptManager::Globals->Put(className, OBJECT_VAL(Class));
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))

Obj* MaterialImpl::VM_New() {
    return (Obj*)NewMaterial(Material::Create(nullptr));
}

VMValue MaterialImpl::VM_Initializer(int argCount, VMValue* args, Uint32 threadID) {
    ObjMaterial* objMaterial = AS_MATERIAL(args[0]);
    Material* material = objMaterial->MaterialPtr;

    StandardLibrary::CheckArgCount(argCount, 2);

    char* name = GET_ARG(1, GetString);

    material->Name = StringUtils::Duplicate(name);

    return OBJECT_VAL(objMaterial);
}

#undef GET_ARG

#define GET_COLOR(type, col, idx) { \
    if (hash == Hash_ ## type ## col) { \
        if (result) \
            *result = DECIMAL_VAL(material->Color ## type[idx]); \
        return true; \
    } \
}

bool MaterialImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
    ObjMaterial* objMaterial = (ObjMaterial*)object;
    Material* material = objMaterial->MaterialPtr;
    if (material == nullptr)
        return false;

    GET_COLOR(Diffuse, Red, 0);
    GET_COLOR(Diffuse, Green, 1);
    GET_COLOR(Diffuse, Blue, 2);
    GET_COLOR(Diffuse, Alpha, 3);

    GET_COLOR(Specular, Red, 0);
    GET_COLOR(Specular, Green, 1);
    GET_COLOR(Specular, Blue, 2);
    GET_COLOR(Specular, Alpha, 3);

    GET_COLOR(Ambient, Red, 0);
    GET_COLOR(Ambient, Green, 1);
    GET_COLOR(Ambient, Blue, 2);
    GET_COLOR(Ambient, Alpha, 3);

    GET_COLOR(Emissive, Red, 0);
    GET_COLOR(Emissive, Green, 1);
    GET_COLOR(Emissive, Blue, 2);
    GET_COLOR(Emissive, Alpha, 3);

    return false;
}

#undef GET_COLOR

#define SET_COLOR(type, col, idx) { \
    if (hash == Hash_ ## type ## col) { \
        if (ScriptManager::DoDecimalConversion(value, threadID)) \
            material->Color ## type[idx] = AS_DECIMAL(value); \
        return true; \
    } \
}

bool MaterialImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
    ObjMaterial* objMaterial = (ObjMaterial*)object;
    Material* material = objMaterial->MaterialPtr;
    if (material == nullptr)
        return false;

    SET_COLOR(Diffuse, Red, 0);
    SET_COLOR(Diffuse, Green, 1);
    SET_COLOR(Diffuse, Blue, 2);
    SET_COLOR(Diffuse, Alpha, 3);

    SET_COLOR(Specular, Red, 0);
    SET_COLOR(Specular, Green, 1);
    SET_COLOR(Specular, Blue, 2);
    SET_COLOR(Specular, Alpha, 3);

    SET_COLOR(Ambient, Red, 0);
    SET_COLOR(Ambient, Green, 1);
    SET_COLOR(Ambient, Blue, 2);
    SET_COLOR(Ambient, Alpha, 3);

    SET_COLOR(Emissive, Red, 0);
    SET_COLOR(Emissive, Green, 1);
    SET_COLOR(Emissive, Blue, 2);
    SET_COLOR(Emissive, Alpha, 3);

    return false;
}

#undef SET_COLOR
