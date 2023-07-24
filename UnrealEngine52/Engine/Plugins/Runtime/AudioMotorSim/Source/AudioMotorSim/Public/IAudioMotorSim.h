// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "IAudioMotorSim.generated.h"

struct FAudioMotorSimInputContext;
struct FAudioMotorSimRuntimeContext;

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

UCLASS(Abstract, Blueprintable, Category = "AudioMotorSim", meta=(BlueprintSpawnableComponent))
class AUDIOMOTORSIM_API UAudioMotorSimComponent : public UActorComponent, public IAudioMotorSim
{
	GENERATED_BODY()

public:
	UAudioMotorSimComponent(const FObjectInitializer& ObjectInitializer);
	
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;
	
	virtual void Reset() override;

	/* Called every tick that this component is being updated. Use "Set Members in Struct" to update values for future components in the chain. The return value does nothing.
	* @param Input			Holds values which are not saved between update frames which represent input to the simulation
	* @param RuntimeInfo	Holds values which are saved between update frames to represent the output or state of the simulation
	* @return				Vestigial, does nothing.
	*/
	UFUNCTION(BlueprintImplementableEvent, Category = "AudioMotorSim", DisplayName = "Update")
	bool BP_Update(UPARAM(ref) FAudioMotorSimInputContext& Input, UPARAM(ref) FAudioMotorSimRuntimeContext& RuntimeInfo);
	
	// Use to reset any state that might be desired. Will be called automatically if the entire MotorSim is Reset, or call it manually
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="AudioMotorSim", DisplayName = "Reset")
	void BP_Reset();
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AudioMotorSimTypes.h"
#endif