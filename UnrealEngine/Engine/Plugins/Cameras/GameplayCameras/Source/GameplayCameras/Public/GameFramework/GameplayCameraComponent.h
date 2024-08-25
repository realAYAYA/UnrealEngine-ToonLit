// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "Core/CameraEvaluationContext.h"
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"

#include "GameplayCameraComponent.generated.h"

class UCameraAsset;
class UGameplayCameraComponentEvaluationContext;

/**
 * A component that can run a camera asset inside its own camera evaluation context.
 */
UCLASS(BlueprintType, MinimalAPI, ClassGroup=Camera, HideCategories=(Mobility, Rendering, LOD))
class UGameplayCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UGameplayCameraComponent(const FObjectInitializer& ObjectInit);

	UFUNCTION(BlueprintCallable, Category=Camera)
	GAMEPLAYCAMERAS_API void ActivateCamera(int32 PlayerIndex = 0);

public:

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:

	void ActivateCamera(APlayerController* PlayerController);

#if WITH_EDITORONLY_DATA

	void UpdatePreviewMeshTransform();

#endif

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	TObjectPtr<UCameraAsset> Camera;

protected:

	UPROPERTY(Transient)
	TObjectPtr<UGameplayCameraComponentEvaluationContext> EvaluationContext;
	
#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

#endif	// WITH_EDITORONLY_DATA
};

UCLASS(MinimalAPI)
class UGameplayCameraComponentEvaluationContext : public UCameraEvaluationContext
{
	GENERATED_BODY()

public:

	UGameplayCameraComponentEvaluationContext(const FObjectInitializer& ObjectInit);

	void Initialize(UGameplayCameraComponent* Owner);
	void Update(UGameplayCameraComponent* Owner);
};

