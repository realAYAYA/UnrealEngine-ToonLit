// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "NiagaraActor.h"
#include "Camera/CameraLensEffectInterface.h"

#include "NiagaraLensEffectBase.generated.h"

class APlayerCameraManager;

/**
 * Niagara equivalent of AEmitterCameraLensEffectBase.
 * This class is supported by APlayerCameraManager (via ICameraLensEffectInterface) as a camera lens effect.
 */
UCLASS(abstract, Blueprintable, hideCategories=(NiagaraActorActivation, "Components|Activation", Input, Collision, "Game|Damage"))
class NIAGARA_API ANiagaraLensEffectBase : public ANiagaraActor, public ICameraLensEffectInterface
{
	GENERATED_BODY()


protected:
	ANiagaraLensEffectBase(const FObjectInitializer& ObjectInitializer);

	/**
	 * Relative offset from the camera (where X is out from the camera)
	 * Might be changed slightly when the FOV is different than expected.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Positioning And Scale")
	FTransform DesiredRelativeTransform;

	/**
	 * FOVs that differ from this may cause adjustments in positioning.
	 * This is the FOV which the effect was tested with.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Positioning And Scale")
	float BaseAuthoredFOV;

	/**
	 * Are multiple instances of the effect allowed? 
	 * When disabled, a single instance of the effect may be retriggered!
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Effect Activation")
	uint8 bAllowMultipleInstances:1;

	/**
	 * When an instance of this effect is retriggered (because more than one instance is not allowed)
	 * should the particle system be explicitly reset? Activate(bReset = true);
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Effect Activation", meta = (EditCondition = "!bAllowMultipleInstances"))
	uint8 bResetWhenRetriggered:1;
	
	/** 
	 *  If an effect class in this array is currently playing, do not play this effect.
	 *  Useful for preventing multiple similar or expensive camera effects from playing simultaneously.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Effect Activation", meta = (MustImplement = "/Script/Engine.CameraLensEffectInterface"))
	TArray<TSubclassOf<AActor>> EmittersToTreatAsSame;

	UPROPERTY()
	TObjectPtr<APlayerCameraManager> OwningCameraManager;

protected:

	//~ Begin ICameraLensEffectInterface interface
	virtual const FTransform& GetRelativeTransform() const override;
	virtual float GetBaseFOV() const override;
	virtual bool ShouldAllowMultipleInstances() const override;
	virtual bool ResetWhenTriggered() const override;
	virtual bool ShouldTreatEmitterAsSame(TSubclassOf<AActor> OtherEmitter) const override;
	virtual void NotifyWillBePooled() override;
	virtual void AdjustBaseFOV(float NewFOV);
	virtual void RegisterCamera(APlayerCameraManager* CameraManager) override;
	virtual void NotifyRetriggered() override;
	virtual void ActivateLensEffect() override;
	virtual void DeactivateLensEffect() override;
	virtual bool IsLooping() const override;
	//~ End ICameraLensEffectInterface interface

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};