#include <Engine/Bytecode/ScriptManager.h>
#include <Engine/Bytecode/StandardLibrary.h>
#include <Engine/Bytecode/TypeImpl/AssetImpl.h>
#include <Engine/Bytecode/TypeImpl/MaterialImpl.h>
#include <Engine/Bytecode/TypeImpl/ResourceImpl/ModelImpl.h>
#include <Engine/Bytecode/TypeImpl/TypeImpl.h>

ObjClass* ModelImpl::Class = nullptr;

#define CLASS_MODEL "Model"

DECLARE_STRING_HASH(VertexCount);
DECLARE_STRING_HASH(AnimationCount);
DECLARE_STRING_HASH(MaterialCount);
DECLARE_STRING_HASH(BoneCount);

void ModelImpl::Init() {
	Class = NewClass(CLASS_MODEL);

	GET_STRING_HASH(VertexCount);
	GET_STRING_HASH(AnimationCount);
	GET_STRING_HASH(MaterialCount);
	GET_STRING_HASH(BoneCount);

	AddNatives();

	TypeImpl::RegisterClass(Class);
	TypeImpl::ExposeClass(CLASS_MODEL, Class);
	TypeImpl::DefinePrintableName(Class, "model");
}

bool ModelImpl::IsValidField(Uint32 hash) {
	CHECK_VALID_FIELD(VertexCount);
	CHECK_VALID_FIELD(AnimationCount);
	CHECK_VALID_FIELD(MaterialCount);
	CHECK_VALID_FIELD(BoneCount);

	return false;
}

#define CHECK_EXISTS(ptr) \
	if (!ptr || !ptr->IsLoaded()) { \
		VM_THROW_ERROR("Model is no longer loaded!"); \
		return true; \
	}

bool ModelImpl::VM_PropertyGet(Obj* object, Uint32 hash, VMValue* result, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	CHECK_EXISTS(asset);

	IModel* model = (IModel*)asset;

	/***
	 * \field VertexCount
	 * \type Integer
	 * \desc The amount of vertices in the model.
	 * \ns Model
 	*/
	if (hash == Hash_VertexCount) {
		*result = INTEGER_VAL((int)model->VertexCount);
	}
	/***
	 * \field AnimationCount
	 * \type Integer
	 * \desc The amount of animations in the model.
	 * \ns Model
 	*/
	else if (hash == Hash_AnimationCount) {
		*result = INTEGER_VAL((int)model->Animations.size());
	}
	/***
	 * \field MaterialCount
	 * \type Integer
	 * \desc The amount of materials in the model.
	 * \ns Model
 	*/
	else if (hash == Hash_MaterialCount) {
		*result = INTEGER_VAL((int)model->Materials.size());
	}
	/***
	 * \field BoneCount
	 * \type Integer
	 * \desc The amount of bones in the model.
	 * \ns Model
 	*/
	else if (hash == Hash_BoneCount) {
		*result = INTEGER_VAL((int)model->BoneCount);
	}

	return true;
}

bool ModelImpl::VM_PropertySet(Obj* object, Uint32 hash, VMValue value, Uint32 threadID) {
	if (!IsValidField(hash)) {
		return false;
	}

	Asset* asset = object ? GET_ASSET(object) : nullptr;
	CHECK_EXISTS(asset);

	if (hash == Hash_VertexCount || hash == Hash_AnimationCount || hash == Hash_MaterialCount ||
		hash == Hash_BoneCount) {
		VM_THROW_ERROR("Field cannot be written to!");
		return true;
	}

	return false;
}

#define GET_ARG(argIndex, argFunction) (StandardLibrary::argFunction(args, argIndex, threadID))
#define GET_ARG_OPT(argIndex, argFunction, argDefault) \
	(argIndex < argCount ? GET_ARG(argIndex, StandardLibrary::argFunction) : argDefault)

#define CHECK_ANIMATION_INDEX(animation) \
	if (animation < 0 || animation >= (signed)model->Animations.size()) { \
		VM_OUT_OF_RANGE_ERROR( \
			"Animation index", animation, 0, model->Animations.size() - 1); \
		return NULL_VAL; \
	}

#define CHECK_ARMATURE_INDEX(armature) \
	if (armature < 0 || armature >= (signed)model->Armatures.size()) { \
		VM_OUT_OF_RANGE_ERROR("Armature index", armature, 0, model->Armatures.size() - 1); \
		return NULL_VAL; \
	}

#undef CHECK_EXISTS
#define CHECK_EXISTS(ptr) \
	if (!ptr) { \
		return NULL_VAL; \
	} \
	if (!ptr->IsLoaded()) { \
		throw ScriptException("Model is no longer loaded!"); \
	}

/***
 * \method GetAnimationName
 * \desc Gets the name of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \return Returns the animation name, or <code>null</code> if the model contains no animations.
 * \ns Model
 */
VMValue ModelImpl_GetAnimationName(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(model);

	if (model->Animations.size() == 0) {
		return NULL_VAL;
	}

	CHECK_ANIMATION_INDEX(index);

	const char* animationName = model->Animations[index]->Name;
	if (animationName && ScriptManager::Lock()) {
		ObjString* string = CopyString(animationName);
		ScriptManager::Unlock();
		return OBJECT_VAL(string);
	}

	return NULL_VAL;
}
/***
 * \method GetAnimationIndex
 * \desc Gets the first animation in the model which matches the specified name.
 * \param name (String): The animation name to search for.
 * \return Returns the first animation index with the specified name, or <code>null</code> if there was no match.
 * \ns Model
 */
VMValue ModelImpl_GetAnimationIndex(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	char* name = GET_ARG(1, GetString);

	CHECK_EXISTS(model);

	int index = model->GetAnimationIndex(name);
	if (index == -1) {
		return NULL_VAL;
	}

	return INTEGER_VAL(index);
}
/***
 * \method GetAnimationLength
 * \desc Gets the length of the specified animation index.
 * \param animationIndex (Integer): The animation index.
 * \return Returns the number of keyframes in the animation.
 * \ns Model
 */
VMValue ModelImpl_GetAnimationLength(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	int index = GET_ARG(1, GetInteger);

	CHECK_EXISTS(model);
	CHECK_ANIMATION_INDEX(index);

	return INTEGER_VAL((int)model->Animations[index]->Length);
}
/***
 * \method GetMaterial
 * \desc Gets a material from a model.
 * \param material (String or Integer): The material name or ID to get.
 * \return Returns a Material.
 * \ns Model
 */
VMValue ModelImpl_GetMaterial(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	Material* material = nullptr;

	if (IS_INTEGER(args[1])) {
		int index = GET_ARG(1, GetInteger);
		CHECK_EXISTS(model);
		if (index < 0 || (size_t)index >= model->Materials.size()) {
			VM_OUT_OF_RANGE_ERROR(
				"Material index", index, 0, (int)model->Materials.size());
			return NULL_VAL;
		}
		material = model->Materials[index];
	}
	else {
		char* name = GET_ARG(1, GetString);
		CHECK_EXISTS(model);
		size_t idx = model->FindMaterial(name);
		if (idx == -1) {
			VM_THROW_ERROR("Model has no material named \"%s\".", name);
			return NULL_VAL;
		}
		material = model->Materials[idx];
	}

	if (material->VMObject == nullptr) {
		material->VMObject = (void*)MaterialImpl::New((void*)material);
	}

	return OBJECT_VAL(material->VMObject);
}
/***
 * \method CreateArmature
 * \desc Creates an armature from the model.
 * \param model (Asset): A model asset.
 * \return Returns the index of the armature.
 * \ns Model
 */
VMValue ModelImpl_CreateArmature(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 1);

	IModel* model = GET_ARG(0, GetModel);
	CHECK_EXISTS(model);

	return INTEGER_VAL(model->NewArmature());
}
/***
 * \method PoseArmature
 * \desc Poses an armature.
 * \param armature (Integer): The armature index to pose.
 * \paramOpt animation (Integer): Animation to pose the armature.
 * \paramOpt frame (Decimal): Frame to pose the armature.
 * \ns Model
 */
VMValue ModelImpl_PoseArmature(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckAtLeastArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);

	CHECK_EXISTS(model);
	CHECK_ARMATURE_INDEX(armature);

	if (argCount >= 3) {
		int animation = GET_ARG(2, GetInteger);
		int frame = GET_ARG(3, GetDecimal) * 0x100;
		if (frame < 0) {
			frame = 0;
		}

		CHECK_ANIMATION_INDEX(animation);

		model->Animate(model->Armatures[armature], model->Animations[animation], frame);
	}
	else {
		// Just update the skeletons
		model->Armatures[armature]->UpdateSkeletons();
	}

	return NULL_VAL;
}
/***
 * \method ResetArmature
 * \desc Resets an armature to its default pose.
 * \param armature (Integer): The armature index to reset.
 * \ns Model
 */
VMValue ModelImpl_ResetArmature(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);

	CHECK_EXISTS(model);
	CHECK_ARMATURE_INDEX(armature);

	model->Armatures[armature]->Reset();

	return NULL_VAL;
}
/***
 * \method DeleteArmature
 * \desc Deletes an armature from the model.
 * \param armature (Integer): The armature index to delete.
 * \ns Model
 */
VMValue ModelImpl_DeleteArmature(int argCount, VMValue* args, Uint32 threadID) {
	StandardLibrary::CheckArgCount(argCount, 2);

	IModel* model = GET_ARG(0, GetModel);
	int armature = GET_ARG(1, GetInteger);

	CHECK_EXISTS(model);
	CHECK_ARMATURE_INDEX(armature);

	model->DeleteArmature((size_t)armature);

	return NULL_VAL;
}

#undef GET_ARG
#undef GET_ARG_OPT
#undef CHECK_ANIMATION_INDEX
#undef CHECK_ARMATURE_INDEX
#undef CHECK_EXISTS

void ModelImpl::AddNatives() {
	DEF_CLASS_NATIVE(ModelImpl, GetAnimationName);
	DEF_CLASS_NATIVE(ModelImpl, GetAnimationIndex);
	DEF_CLASS_NATIVE(ModelImpl, GetAnimationLength);
	DEF_CLASS_NATIVE(ModelImpl, GetMaterial);
	DEF_CLASS_NATIVE(ModelImpl, CreateArmature);
	DEF_CLASS_NATIVE(ModelImpl, PoseArmature);
	DEF_CLASS_NATIVE(ModelImpl, ResetArmature);
	DEF_CLASS_NATIVE(ModelImpl, DeleteArmature);
}
