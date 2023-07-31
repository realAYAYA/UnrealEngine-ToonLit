// Copyright Epic Games, Inc. All Rights Reserved.

#include "IAudioMotorSim.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IAudioMotorSim)

UAudioMotorSim::UAudioMotorSim(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAudioMotorSimComponent::UAudioMotorSimComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}
