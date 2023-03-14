// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimInstance.h"
#include "AnimNotifyState_TimedNiagaraEffect.generated.h"

class UNiagaraSystem;
class USkeletalMeshComponent;
class UFXSystemAsset;
class UFXSystemComponent;
class UAnimInstance;

// Timed Niagara Effect Notify
// Allows a looping Niagara effect to be played in an animation that will activate
// at the beginning of the notify and deactivate at the end.
UCLASS(Blueprintable, meta = (DisplayName = "Timed Niagara Effect"))
class NIAGARAANIMNOTIFIES_API UAnimNotifyState_TimedNiagaraEffect : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()

	// The niagara system template to use when spawning the niagara component
	UPROPERTY(EditAnywhere, Category = NiagaraSystem, meta = (ToolTip = "The niagara system to spawn for the notify state"))
	TObjectPtr<UNiagaraSystem> Template;

	// The socket within our mesh component to attach to when we spawn the Niagara component
	UPROPERTY(EditAnywhere, Category = NiagaraSystem, meta = (ToolTip = "The socket or bone to attach the system to", AnimNotifyBoneName = "true"))
	FName SocketName;

	// Offset from the socket / bone location
	UPROPERTY(EditAnywhere, Category = NiagaraSystem, meta = (ToolTip = "Offset from the socket or bone to place the Niagara system"))
	FVector LocationOffset;

	// Offset from the socket / bone rotation
	UPROPERTY(EditAnywhere, Category = NiagaraSystem, meta = (ToolTip = "Rotation offset from the socket or bone for the Niagara system"))
	FRotator RotationOffset;

	// Whether or not we destroy the component at the end of the notify or instead just stop
	// the emitters.
	UPROPERTY(EditAnywhere, Category = NiagaraSystem, meta = (DisplayName = "Destroy Immediately", ToolTip = "Whether the Niagara system should be immediately destroyed at the end of the notify state or be allowed to finish"))
	bool bDestroyAtEnd;

	UE_DEPRECATED(5.0, "Please use the other NotifyBegin function instead")
	virtual void NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration) override;

	UE_DEPRECATED(5.0, "Please use the other NotifyEnd function instead")
	virtual void NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation) override;
	
	virtual void NotifyBegin(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(class USkeletalMeshComponent * MeshComp, class UAnimSequenceBase * Animation, const FAnimNotifyEventReference& EventReference) override;

	// Overridden from UAnimNotifyState to provide custom notify name.
	FString GetNotifyName_Implementation() const override;

	// Return FXSystemComponent created from SpawnEffect
	UFUNCTION(BlueprintCallable, Category = "AnimNotify")
	UFXSystemComponent* GetSpawnedEffect(UMeshComponent* MeshComp);

protected:
	// Spawns the NiagaraSystemComponent. Called from Notify.
	virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const;

	bool ValidateParameters(USkeletalMeshComponent* MeshComp) const;

	FORCEINLINE FName GetSpawnedComponentTag()const { return GetFName(); }
};

USTRUCT(BlueprintType)
struct FCurveParameterPair
{
	GENERATED_BODY();
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AnimCurves, meta = (DisplayName = "Anim Curve Name", ToolTip = "Name of the curve in this montage."))
		FName AnimCurveName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AnimCurves, meta = (DisplayName = "Niagara User Float", ToolTip = "Name of the Niagara user float variable."))
		FName UserVariableName;
};

/**
Same as Timed Niagara Effect but also provides some more advanced abilities at an additional cost. 
*/
UCLASS(Blueprintable, meta = (DisplayName = "Advanced Timed Niagara Effect"))
class NIAGARAANIMNOTIFIES_API UAnimNotifyState_TimedNiagaraEffectAdvanced : public UAnimNotifyState_TimedNiagaraEffect
{
	GENERATED_UCLASS_BODY()

public:
	UE_DEPRECATED(5.0, "Please use the other NotifyBegin function instead")
	virtual void NotifyBegin(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation, float TotalDuration) override;

	UE_DEPRECATED(5.0, "Please use the other NotifyEnd function instead")
	virtual void NotifyEnd(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation) override;

	UE_DEPRECATED(5.0, "Please use the other NotifyTick function instead")
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime) override;
	
	virtual void NotifyBegin(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(class USkeletalMeshComponent* MeshComp, class UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent * MeshComp, UAnimSequenceBase * Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;

	// Returns a 0 to 1 value for the progress of this component along the notify.
	UFUNCTION(BlueprintCallable, Category = "AnimNotify")
	float GetNotifyProgress(UMeshComponent* MeshComp);

	UPROPERTY(EditAnywhere, Category = NotifyProgress, meta = (DisplayName = "Enable Normalized Notify Progress", ToolTip = "This send a 0-1 value of the normalized progress to the FX Component to the float User Parameter."))
	bool bEnableNormalizedNotifyProgress = true;

	UPROPERTY(EditAnywhere, Category = NotifyProgress, meta = (DisplayName = "User Parameter", ToolTip = "The name of your niagara user variable you would like to send the normalized notify progress to."))
	FName NotifyProgressUserParameter;

	UPROPERTY(EditAnywhere, Category = AnimCurves, meta = (DisplayName = "Anim Curve Parameters", ToolTip = "Array of fnames to map Anim Curve names to Niagara Parameters."))
	TArray<FCurveParameterPair> AnimCurves;

protected:

	struct FInstanceProgressInfo
	{
		float Duration = 1.0f;
		float Elapsed = 0.0f;
	};
	TMap<UMeshComponent*, FInstanceProgressInfo> ProgressInfoMap;
};