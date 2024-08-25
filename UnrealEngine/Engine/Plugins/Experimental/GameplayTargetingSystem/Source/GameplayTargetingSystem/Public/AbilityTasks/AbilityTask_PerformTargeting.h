// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Types/TargetingSystemTypes.h"
#include "AbilityTask_PerformTargeting.generated.h"

class UTargetingPreset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTargetReadyDelegate, FTargetingRequestHandle, TargetingRequestHandle);

UCLASS()
class TARGETINGSYSTEM_API UAbilityTask_PerformTargeting : public UAbilityTask
{
	GENERATED_BODY()

public:
	// Called when the targeting request has been completed and results are ready
	UPROPERTY(BlueprintAssignable)
	FTargetReadyDelegate OnTargetReady;

	virtual void Activate() override;

	// Performs a targeting request based on a Targeting Preset from a GameplayAbility
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_PerformTargeting* PerformTargetingRequest(UGameplayAbility* OwningAbility, UTargetingPreset* InTargetingPreset, bool bAllowAsync);
	
	// Performs a target filtering request based on a Targeting Preset from a GameplayAbility
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_PerformTargeting* PerformFilteringRequest(UGameplayAbility* OwningAbility, UTargetingPreset* TargetingPreset, const TArray<AActor*> InTargets, bool bAllowAsync);

private:
	// Helper function to push the InitialTargets array to the DataStore for Filtering requests
	void SetupInitialTargetsForRequest(FTargetingRequestHandle RequestHandle) const;

protected:
	// Targeting Preset to request
	TObjectPtr<UTargetingPreset> TargetingPreset;

	// Initial Targets to pass into the request (only populated for Filtering Requests)
	TArray<AActor*> InitialTargets;

	// Whether to perform this request immediately or async
	bool bPerformAsync;
};