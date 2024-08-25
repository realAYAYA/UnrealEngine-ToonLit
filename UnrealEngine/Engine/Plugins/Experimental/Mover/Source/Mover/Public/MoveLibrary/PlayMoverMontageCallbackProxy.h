// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Animation/AnimInstance.h"
#include "PlayMontageCallbackProxy.h"
#include "PlayMoverMontageCallbackProxy.generated.h"

class UMoverComponent;
class UAnimMontage;
class USkeletalMeshComponent;

// Runtime object used as a proxy to an async blueprint task node that runs animation montages on Mover actors. This leverages 
// parent UPlayMontageCallbackProxy to perform animation, while adding an accompanying layered move to handle any root motion 
// from the montage.
UCLASS()
class MOVER_API UPlayMoverMontageCallbackProxy : public UPlayMontageCallbackProxy
{
	GENERATED_UCLASS_BODY()

	// Called to perform the query internally
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UPlayMoverMontageCallbackProxy* CreateProxyObjectForPlayMoverMontage(
		UMoverComponent* InMoverComponent,
		UAnimMontage* MontageToPlay,
		float PlayRate = 1.f,
		float StartingPosition = 0.f,
		FName StartingSection = NAME_None);

public:
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	//~ End UObject Interface

protected:
	bool PlayMoverMontage(
		UMoverComponent* InMoverComponent,
		USkeletalMeshComponent* InSkeletalMeshComponent,
		UAnimMontage* MontageToPlay,
		float PlayRate,
		float StartingPosition,
		FName StartingSection);

	UFUNCTION()
	void OnMoverMontageEnded(FName IgnoredNotifyName);

	void UnbindMontageDelegates();
};