// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavAreas/NavArea_LowHeight.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavArea_LowHeight)

UNavArea_LowHeight::UNavArea_LowHeight(const FObjectInitializer& ObjectInitializer) 
	: Super(ObjectInitializer)
{
	DefaultCost = BIG_NUMBER;
	DrawColor = FColor(0, 0, 128);

	// can't traverse
	AreaFlags = 0;
}

