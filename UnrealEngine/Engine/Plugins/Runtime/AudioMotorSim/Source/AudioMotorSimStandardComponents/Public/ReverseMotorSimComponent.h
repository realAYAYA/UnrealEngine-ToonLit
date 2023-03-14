// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "IAudioMotorSim.h"
#include "ReverseMotorSimComponent.generated.h"

// Scales engine friction when reversing
UCLASS(ClassGroup = "AudioMotorSim", meta = (BlueprintSpawnableComponent))
class UReverseMotorSimComponent : public UAudioMotorSimComponent
{
	GENERATED_BODY()
	
public:
	// How much to scale engine friction by when reversing
	UPROPERTY(EditAnywhere, Category = "Reverse")
	float ReverseEngineResistanceModifier = 1.f;
	
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;
};