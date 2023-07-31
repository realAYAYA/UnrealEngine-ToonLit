// Copyright Epic Games, Inc. All Rights Reserved.

#include "BakeToControlRigSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BakeToControlRigSettings)

UBakeToControlRigSettings::UBakeToControlRigSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{}

void UBakeToControlRigSettings::Reset()
{
	bReduceKeys = false;
	Tolerance = 0.001f;
}

