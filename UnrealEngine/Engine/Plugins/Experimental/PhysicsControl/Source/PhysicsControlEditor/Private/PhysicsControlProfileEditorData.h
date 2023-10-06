// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"

class IPersonaPreviewScene;
class UPhysicsControlProfileAsset;
class UPhysicsControlProfileAssetEditorSkeletalMeshComponent;

/**
 * Helper/container for data used by the Physics Control Profile Editor 
 */
class FPhysicsControlProfileEditorData
{
public:
	FPhysicsControlProfileEditorData();

	/** Initializes members */
	void Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene);

	/** Caches a preview mesh. Sets us to a default mesh if none is set yet (or if an older one got deleted) */
	void CachePreviewMesh();

public:
	/** The asset being inspected */
	TObjectPtr<UPhysicsControlProfileAsset> PhysicsControlProfileAsset;

	/** Skeletal mesh component specialized for this asset editor */
	UPhysicsControlProfileAssetEditorSkeletalMeshComponent* EditorSkelComp;

	/** The physics control component used for testing/simulating on the character */
	class UPhysicsControlComponent* PhysicsControlComponent;

	/** Preview scene */
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

};
