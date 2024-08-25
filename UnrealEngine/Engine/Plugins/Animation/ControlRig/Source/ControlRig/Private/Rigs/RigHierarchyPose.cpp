// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rigs/RigHierarchyPose.h"
#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigHierarchyPose)


FRigPoseElement::FRigPoseElement(): Index()
                                  , GlobalTransform(FTransform::Identity)
                                  , LocalTransform(FTransform::Identity)
                                  , PreferredEulerAngle(FVector::ZeroVector)
                                  , ActiveParent(URigHierarchy::GetDefaultParentKey())
                                  , CurveValue(0.f)
{
}
