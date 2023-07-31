// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavArea_Null.h"
#include "NavMesh/RecastNavMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavArea_Null)

UNavArea_Null::UNavArea_Null(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DefaultCost = FLT_MAX;
	AreaFlags = 0;
}

