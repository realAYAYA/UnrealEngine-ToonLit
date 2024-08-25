// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "AudioMotorSimTypes.h"
#include "AudioMotorModelComponent.generated.h"

class IAudioMotorSim;
class IAudioMotorSimOutput;

USTRUCT(BlueprintType)
struct FMotorSimEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MotorSim")
	TScriptInterface<IAudioMotorSim> Sim;
	
	UPROPERTY(BlueprintReadOnly, Category = "MotorSim")
	int32 SortOrder = 0;
};
	
UCLASS(ClassGroup = AudioMotorSim, Blueprintable, meta = (BlueprintSpawnableComponent))
class AUDIOMOTORSIM_API UAudioMotorModelComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Model")
	TArray<FMotorSimEntry> SimComponents;

	UPROPERTY(Transient)
	TArray<TScriptInterface<IAudioMotorSimOutput>> AudioComponents;

	UFUNCTION(BlueprintCallable, Category = MotorModel)
	virtual void Update(const FAudioMotorSimInputContext& Input);
	
	UFUNCTION(BlueprintCallable, Category = MotorModel)
	virtual void Reset();

	UFUNCTION(BlueprintCallable, Category = MotorModel)
	virtual void StartOutput();
	
	UFUNCTION(BlueprintCallable, Category = MotorModel)
	virtual void StopOutput();

	UFUNCTION(BlueprintCallable, Category = MotorModel)
	void AddMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent);
	
	UFUNCTION(BlueprintCallable, Category = MotorModel)
	void RemoveMotorAudioComponent(TScriptInterface<IAudioMotorSimOutput> InComponent);
	
	UFUNCTION(BlueprintCallable, Category = MotorModel)
	void AddMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent, const int32 SortOrder = 0);
	
	UFUNCTION(BlueprintCallable, Category = MotorModel)
	void RemoveMotorSimComponent(TScriptInterface<IAudioMotorSim> InComponent);
	
	UFUNCTION(BlueprintPure, Category = State)
	float GetRpm() const { return CachedRuntimeContext.Rpm; }

	UFUNCTION(BlueprintPure, Category = State)
	int32 GetGear() const { return CachedRuntimeContext.Gear; }
	
	UFUNCTION(BlueprintPure, Category = State)
	FAudioMotorSimRuntimeContext GetRuntimeInfo() const { return CachedRuntimeContext; }

	UFUNCTION(BlueprintPure, Category = State)
	const FAudioMotorSimInputContext& GetCachedInputData() const { return CachedInputContext; }

private:
	FAudioMotorSimRuntimeContext CachedRuntimeContext;
	
	FAudioMotorSimInputContext CachedInputContext;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "IAudioMotorSim.h"
#include "IAudioMotorSimOutput.h"
#endif
