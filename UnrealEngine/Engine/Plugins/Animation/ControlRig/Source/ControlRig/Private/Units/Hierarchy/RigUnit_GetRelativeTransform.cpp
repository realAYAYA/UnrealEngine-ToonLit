// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GetRelativeTransform.h"
#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/Hierarchy/RigUnit_GetTransform.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_GetRelativeTransform)

FRigUnit_GetRelativeTransformForItem_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	FTransform ChildTransform = FTransform::Identity;
	FTransform ParentTransform = FTransform::Identity;

	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Child, EBoneGetterSetterMode::GlobalSpace, bChildInitial, ChildTransform, CachedChild, Context);
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Parent, EBoneGetterSetterMode::GlobalSpace, bParentInitial, ParentTransform, CachedParent, Context);
	FRigUnit_MathTransformMakeRelative::StaticExecute(RigVMExecuteContext, ChildTransform, ParentTransform, RelativeTransform, Context);
}
