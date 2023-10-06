// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/Interface_PreviewMeshProvider.h"

#include "PhysicsControlProfileAsset.generated.h"

class USkeletalMesh;

/**
 * Asset for storing Physics Control Profiles. These will contain data that define:
 * - Controls and body modifiers to be created on a mesh
 * - Sets referencing those controls and body modifiers
 * - Full profiles containing settings for all the controls/modifiers
 * - Sparse profiles containing partial sets of settings for specific controls/modifiers
 * 
 * It will also be desirable to support "inheritance" - so a generic profile can be made, and then 
 * customized for certain characters or scenarios.
 */
UCLASS(BlueprintType)
class PHYSICSCONTROL_API UPhysicsControlProfileAsset : public UObject, public IInterface_PreviewMeshProvider
{
	GENERATED_BODY()
public:
	UPhysicsControlProfileAsset();

	/** Placeholder for testing/developing */
	UFUNCTION(CallInEditor)
	void Log();

	/** IInterface_PreviewMeshProvider interface */
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	/** END IInterface_PreviewMeshProvider interface */

#if WITH_EDITOR
/* Get name of Preview Mesh property */
	static const FName GetPreviewMeshPropertyName();
#endif

public:
	/** 
	* The skeletal mesh to use for generating controls and previewing. If it turns out this 
	* doesn't need to be stored in the asset, it will get moved out of here.
	*/
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = PreviewMesh)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;

	/** Placeholder for testing/developing */
	UPROPERTY(EditAnywhere, Category=PhysicsControl)
	float TestValue;
};
