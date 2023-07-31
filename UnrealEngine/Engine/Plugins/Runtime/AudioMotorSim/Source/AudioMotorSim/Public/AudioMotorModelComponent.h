// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "AudioMotorSimTypes.h"
#include "IAudioMotorSim.h"
#include "IAudioMotorSimOutput.h"
#include "AudioMotorModelComponent.generated.h"

USTRUCT(BlueprintType)
struct FMotorSimEntry
{
	GENERATED_BODY()
	
	TScriptInterface<IAudioMotorSim> Sim;
	int32 SortOrder = 0;
};
	
UCLASS(ClassGroup = AudioMotorSim, Blueprintable, meta = (BlueprintSpawnableComponent))
class AUDIOMOTORSIM_API UAudioMotorModelComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	TArray<FMotorSimEntry> SimComponents;

	UPROPERTY(Transient)
	TArray<TScriptInterface<IAudioMotorSimOutput>> AudioComponents;

	UFUNCTION(BlueprintCallable, Category = MotorModel)
	virtual void Update(FAudioMotorSimInputContext Input);
	
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
	FAudioMotorSimInputContext GetCachedInputData() const { return CachedInputContext; }

private:
	FAudioMotorSimRuntimeContext CachedRuntimeContext;
	
	FAudioMotorSimInputContext CachedInputContext;
};