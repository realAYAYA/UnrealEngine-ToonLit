// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorViewportClient.h"
#include "Engine/EngineBaseTypes.h"
#include "HAL/Platform.h"
#include "InputCoreTypes.h"
#include "Templates/SharedPointer.h"

class FAdvancedPreviewScene;
class FReferenceCollector;
class FSceneView;
class HHitProxy;
class USkeletalMeshComponent;


class FCustomizableObjectPopulationEditorViewportClient : public FEditorViewportClient
{
public:

	FCustomizableObjectPopulationEditorViewportClient(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene);
	~FCustomizableObjectPopulationEditorViewportClient();

	virtual void Tick(float DeltaSeconds) override;

	void SetPreviewComponent(TArray<USkeletalMeshComponent*> InSkeletalMeshComponent, TArray<class UCapsuleComponent*> InColliderComponents);

	// FSerializableObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	// End of FSerializableObject interface

	// Select an instance of the viewport
	void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;

	int32 GetSelectedInstance() { return SelectedInstance; }

private:

	// All the Skeletal Mesh components of the scene
	TArray<USkeletalMeshComponent*> SkeletalMeshComponents;

	// All the Collider components of the scene
	TArray<class UCapsuleComponent*> ColliderComponents;

	// index of the selected instance
	int32 SelectedInstance;

};