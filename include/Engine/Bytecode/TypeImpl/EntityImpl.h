#ifndef ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H
#define ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H

#include <Engine/Bytecode/Types.h>
#include <Engine/Includes/Standard.h>

#define ENTITY_NATIVE_FN_LIST \
	ENTITY_NATIVE_FN(InView)\
	ENTITY_NATIVE_FN(Animate)\
	ENTITY_NATIVE_FN(ApplyPhysics)\
	ENTITY_NATIVE_FN(SetAnimation)\
	ENTITY_NATIVE_FN(ResetAnimation)\
	ENTITY_NATIVE_FN(GetHitboxFromSprite)\
	ENTITY_NATIVE_FN(ReturnHitbox)\
	ENTITY_NATIVE_FN(AddToRegistry)\
	ENTITY_NATIVE_FN(IsInRegistry)\
	ENTITY_NATIVE_FN(RemoveFromRegistry)\
	ENTITY_NATIVE_FN(CollidedWithObject)\
	ENTITY_NATIVE_FN(CollideWithObject)\
	ENTITY_NATIVE_FN(SolidCollideWithObject)\
	ENTITY_NATIVE_FN(TopSolidCollideWithObject)\
	ENTITY_NATIVE_FN(PropertyGet)\
	ENTITY_NATIVE_FN(PropertyExists)\
	ENTITY_NATIVE_FN(SetViewVisibility)\
	ENTITY_NATIVE_FN(SetViewOverride)\
	ENTITY_NATIVE_FN(AddToDrawGroup)\
	ENTITY_NATIVE_FN(IsInDrawGroup)\
	ENTITY_NATIVE_FN(RemoveFromDrawGroup)\
	ENTITY_NATIVE_FN(GetIDWithinClass)\
	ENTITY_NATIVE_FN(PlaySound)\
	ENTITY_NATIVE_FN(LoopSound)\
	ENTITY_NATIVE_FN(StopSound)\
	ENTITY_NATIVE_FN(StopAllSounds)

class EntityImpl {
public:
	static ObjClass* Class;
	static ObjClass* ParentClass;

	static void Init();
};

#endif /* ENGINE_BYTECODE_TYPEIMPL_ENTITYIMPL_H */
