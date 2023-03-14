// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "Components/ActorComponent.h"
#include "UObject/Interface.h"
#include "IAudioMotorSim.generated.h"

UINTERFACE(BlueprintType)
class UAudioMotorSim : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AUDIOMOTORSIM_API IAudioMotorSim
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) = 0;

	virtual void Reset() {};
};

UCLASS(Blueprintable, Category = "AudioMotorSim", meta=(BlueprintSpawnableComponent))
class AUDIOMOTORSIM_API UAudioMotorSimComponent : public UActorComponent, public IAudioMotorSim
{
	GENERATED_BODY()

public:
	UAudioMotorSimComponent(const FObjectInitializer& ObjectInitializer);
	
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override {};
};