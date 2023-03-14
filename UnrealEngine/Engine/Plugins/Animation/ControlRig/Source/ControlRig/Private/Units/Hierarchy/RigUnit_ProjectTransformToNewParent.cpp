// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_ProjectTransformToNewParent.h"
#include "Units/Math/RigUnit_MathTransform.h"
#include "Units/Hierarchy/RigUnit_GetTransform.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_ProjectTransformToNewParent)

FRigUnit_ProjectTransformToNewParent_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	FTransform ChildTransform = FTransform::Identity;
	FTransform OldParentTransform = FTransform::Identity;
	FTransform NewParentTransform = FTransform::Identity;
	FTransform RelativeTransform = FTransform::Identity;

	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Child, EBoneGetterSetterMode::GlobalSpace, bChildInitial, ChildTransform, CachedChild, Context);
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, OldParent, EBoneGetterSetterMode::GlobalSpace, bOldParentInitial, OldParentTransform, CachedOldParent, Context);
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, NewParent, EBoneGetterSetterMode::GlobalSpace, bNewParentInitial, NewParentTransform, CachedNewParent, Context);
	FRigUnit_MathTransformMakeRelative::StaticExecute(RigVMExecuteContext, ChildTransform, OldParentTransform, RelativeTransform, Context);
	FRigUnit_MathTransformMakeAbsolute::StaticExecute(RigVMExecuteContext, RelativeTransform, NewParentTransform, Transform, Context);
}

