// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_WorldSpace.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_WorldSpace)

FRigUnit_ToWorldSpace_Transform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    World = Context.ToWorldSpace(Value);
}

FRigUnit_ToRigSpace_Transform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    Global = Context.ToRigSpace(Value);
}

FRigUnit_ToWorldSpace_Location_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    World = Context.ToWorldSpace(Value);
}

FRigUnit_ToRigSpace_Location_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    Global = Context.ToRigSpace(Value);
}

FRigUnit_ToWorldSpace_Rotation_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    World = Context.ToWorldSpace(Value);
}

FRigUnit_ToRigSpace_Rotation_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
    Global = Context.ToRigSpace(Value);
}
