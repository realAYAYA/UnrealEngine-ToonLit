// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/CameraSystemEvaluator.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "Components/SceneComponent.h"

#include "GameplayCameraSystemComponent.generated.h"

class UCameraSystemEvaluator;
class UCameraMode;
struct FMinimalViewInfo;

/**
 * A component that hosts a camera system.
 */
UCLASS(BlueprintType, MinimalAPI, ClassGroup=Camera, HideCategories=(Mobility, Rendering, LOD))
class UGameplayCameraSystemComponent : public USceneComponent
{
	GENERATED_BODY()

public:

	UGameplayCameraSystemComponent(const FObjectInitializer& ObjectInit);

	UFUNCTION(BlueprintGetter)
	UCameraSystemEvaluator* GetCameraSystemEvaluator() { return Evaluator; }

	GAMEPLAYCAMERAS_API void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);

public:

	// UActorComponent interface
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// USceneComponent interface
#if WITH_EDITOR
	virtual bool GetEditorPreviewInfo(float DeltaTime, FMinimalViewInfo& ViewOut) override;
#endif  // WITH_EDITOR

public:

	// Internal API
	void OnBecomeViewTarget();
	void OnEndViewTarget();

private:
	
	UPROPERTY(Instanced, Transient)
	TObjectPtr<UCameraSystemEvaluator> Evaluator;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

#endif	// WITH_EDITORONLY_DATA
};

