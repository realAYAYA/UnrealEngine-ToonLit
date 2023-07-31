// Copyright Epic Games, Inc. All Rights Reserved.

#include "GauntletTestControllerBootTest.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GauntletTestControllerBootTest)



void UGauntletTestControllerBootTest::OnTick(float TimeDelta)
{
	if (IsBootProcessComplete())
	{
		EndTest(0);
	}
}
