// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraTypes.h"
#include "Components/SceneComponent.h"
#include "UObject/ObjectMacros.h"
#include "CameraShakeSourceComponent.generated.h"

class UCameraShakeBase;
class UTexture2D;

UENUM(BlueprintType)
enum class ECameraShakeAttenuation : uint8
{
	Linear,
	Quadratic
};

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class ENGINE_API UCameraShakeSourceComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCameraShakeSourceComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void OnRegister() override;

#if WITH_EDITOR
    virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	void UpdateEditorSpriteTexture();

#if WITH_EDITOR
	TSubclassOf<UCameraShakeBase> PreviousCameraShake;
#endif

public:
	/** The attenuation profile for how camera shakes' intensity falls off with distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation)
	ECameraShakeAttenuation Attenuation;

	/** Under this distance from the source, the camera shakes are at full intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation)
	float InnerAttenuationRadius;

	/** Outside of this distance from the source, the camera shakes don't apply at all */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attenuation)
	float OuterAttenuationRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraShake)
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraShake)
	bool bAutoStart;

#if WITH_EDITORONLY_DATA
	/** Sprite to display in the editor. */
	UPROPERTY(transient)
	TObjectPtr<UTexture2D> EditorSpriteTexture;

	/** Sprite scaling for display in the editor. */
	UPROPERTY(transient)
	float EditorSpriteTextureScale;
#endif

public:
	UFUNCTION(BlueprintCallable, Category = CameraShake)
	void Start();

	/** Starts a new camera shake originating from this source, and apply it on all player controllers */
	UFUNCTION(BlueprintCallable, Category = CameraShake)
	void StartCameraShake(TSubclassOf<UCameraShakeBase> InCameraShake, float Scale=1.f, ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal, FRotator UserPlaySpaceRot = FRotator::ZeroRotator);

	/** Stops a camera shake originating from this source */
	UFUNCTION(BlueprintCallable, Category = CameraShake)
	void StopAllCameraShakesOfType(TSubclassOf<UCameraShakeBase> InCameraShake, bool bImmediately = true);

	/** Stops all currently active camera shakes that are originating from this source from all player controllers */
	UFUNCTION(BlueprintCallable, Category = CameraShake)
	void StopAllCameraShakes(bool bImmediately = true);

	/** Computes an attenuation factor from this source */
	UFUNCTION(BlueprintCallable, Category = CameraShake)
	float GetAttenuationFactor(const FVector& Location) const;
};
