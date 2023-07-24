// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AudioMotorSimTypes.h"
#endif
