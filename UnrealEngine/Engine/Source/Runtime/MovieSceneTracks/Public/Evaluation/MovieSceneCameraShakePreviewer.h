// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Camera/CameraShakeBase.h"
#include "Camera/CameraTypes.h"
#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class FLevelEditorViewportClient;
class UCameraShakeSourceComponent;
struct FActiveCameraShakeInfo;
struct FEditorViewportViewModifierParams;

struct FCameraShakePreviewerAddParams
{
	// The class of the shake.
	TSubclassOf<UCameraShakeBase> ShakeClass;

	// Optional shake source.
	TObjectPtr<const UCameraShakeSourceComponent> SourceComponent;

	// Start time of the shake, for scrubbing.
	float GlobalStartTime;

	// Parameters to be passed to the shake's start method.
	float Scale = 1.f;
	ECameraShakePlaySpace PlaySpace = ECameraShakePlaySpace::CameraLocal;
	FRotator UserPlaySpaceRot = FRotator::ZeroRotator;
	TOptional<float> DurationOverride;
};

/**
 * A class that owns a gameplay camera shake manager, so that we can us it to preview shakes in editor.
 */
class MOVIESCENETRACKS_API FCameraShakePreviewer : public FGCObject
{
public:
	using FViewportFilter = TFunctionRef<bool(FLevelEditorViewportClient*)>;

	FCameraShakePreviewer(UWorld* InWorld);
	~FCameraShakePreviewer();

	UWorld* GetWorld() const { return World; }

	void ModifyView(FEditorViewportViewModifierParams& Params);

	void RegisterViewModifiers(bool bIgnoreDuplicateRegistration = false);
	void RegisterViewModifiers(FViewportFilter InViewportFilter, bool bIgnoreDuplicateRegistration = false);
	void UnRegisterViewModifiers();

	void RegisterViewModifier(FLevelEditorViewportClient* ViewportClient, bool bIgnoreDuplicateRegistration = false);
	void UnRegisterViewModifier(FLevelEditorViewportClient* ViewportClient);

	void Update(float DeltaTime, bool bIsPlaying);
	void Scrub(float ScrubTime);

	UCameraShakeBase* AddCameraShake(const FCameraShakePreviewerAddParams& Params);
	void RemoveCameraShake(UCameraShakeBase* ShakeInstance);
	void RemoveAllCameraShakesFromSource(const UCameraShakeSourceComponent* SourceComponent);
	void RemoveAllCameraShakes();

	int32 NumActiveCameraShakes() const { return ActiveShakes.Num(); }
	void GetActiveCameraShakes(TArray<FActiveCameraShakeInfo>& ActiveCameraShakes) const;

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

private:
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FCameraShakePreviewer"); }

private:
	void OnModifyView(FEditorViewportViewModifierParams& Params);
	void OnLevelViewportClientListChanged();

	void ResetModifiers();

private:
	UWorld* World;

	TArray<FLevelEditorViewportClient*> RegisteredViewportClients;

	struct FPreviewCameraShakeInfo
	{
		FCameraShakeBaseStartParams StartParams;
		TObjectPtr<UCameraShakeBase> ShakeInstance;
		TWeakObjectPtr<const UCameraShakeSourceComponent> SourceComponent;
		float StartTime;
	};
	TArray<FPreviewCameraShakeInfo> ActiveShakes;

	TOptional<float> LastDeltaTime;
	TOptional<float> LastScrubTime;

	FVector LastLocationModifier;
	FRotator LastRotationModifier;
	float LastFOVModifier;

	TArray<FPostProcessSettings> LastPostProcessSettings;
	TArray<float> LastPostProcessBlendWeights;
};

#endif

