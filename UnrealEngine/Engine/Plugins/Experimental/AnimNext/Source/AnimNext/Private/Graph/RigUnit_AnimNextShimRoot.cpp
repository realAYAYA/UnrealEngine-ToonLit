// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/RigUnit_AnimNextShimRoot.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_AnimNextShimRoot)

FName FRigUnit_AnimNextShimRoot::EventName = TEXT("Execute");

FRigUnit_AnimNextShimRoot_Execute()
{
	// This RigUnit is a no-op entry point, it simply forwards execution to the real entry point
	// @see FRigUnit_AnimNextRuntimeRoot
}
