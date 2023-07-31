// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class FLevelEditorViewportClient;
class UCameraModifier_CameraShake;
struct FEditorViewportViewModifierParams;

/**
 * A class that owns a gameplay camera shake manager, so that we can us it to preview shakes in editor.
 */
class FCameraShakePreviewer : public FGCObject
{
public:
	FCameraShakePreviewer();
	~FCameraShakePreviewer();

	void RegisterViewModifier();
	void UnRegisterViewModifier();
	void Update(float DeltaTime, bool bIsPlaying);

	UCameraModifier_CameraShake* GetCameraShake() { return PreviewCameraShake; }

private:
	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Collector.AddReferencedObject(PreviewCameraShake); }
	virtual FString GetReferencerName() const override { return TEXT("SCameraShakePreviewer"); }

private:
	void OnModifyView(FEditorViewportViewModifierParams& Params);
	void OnLevelViewportClientListChanged();

private:
	UCameraModifier_CameraShake* PreviewCameraShake;
	TArray<FLevelEditorViewportClient*> RegisteredViewportClients;

	TOptional<float> LastDeltaTime;
	FVector LastLocationModifier;
	FRotator LastRotationModifier;
	float LastFOVModifier;
};

#endif

