// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsControlData.h"
#include "PhysicsControlLimbData.h"

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

	// Data that have been compiled from a combination of inherited and "My" data.

	/**
	 * We can define controls in the form of limbs etc here
	 */
	UPROPERTY()
	FPhysicsControlCharacterSetupData CharacterSetupData;

	/**
	 * Additional controls and modifiers. If these have the same name as one that's already 
	 * created, they'll just override it.
	 */
	UPROPERTY()
	FPhysicsControlAndBodyModifierCreationDatas AdditionalControlsAndModifiers;

	/**
	 * Additional control and body modifier sets
	 */
	UPROPERTY()
	FPhysicsControlSetUpdates AdditionalSets;

	/**
	 * Initial updates to apply immediately after controls and modifiers are created
	 */
	UPROPERTY()
	TArray<FPhysicsControlControlAndModifierUpdates> InitialControlAndModifierUpdates;

	/**
	 * The named profiles, which are essentially control and modifier updates
	 */
	UPROPERTY()
	TMap<FName, FPhysicsControlControlAndModifierUpdates> Profiles;

public:
	// Data that will then be compiled down into the runtime data

#if WITH_EDITORONLY_DATA
	/** A profile asset to inherit from (can be null). If set, we will just add/modify data in that */
	UPROPERTY(EditAnywhere, Category = Inheritance)
	TSoftObjectPtr<UPhysicsControlProfileAsset> ParentAsset;

	/** 
	 * Additional profile assets from which profiles (not the setup data, extra sets etc) will be added 
	 * to this asset.
	 */
	UPROPERTY(EditAnywhere, Category = Inheritance)
	TArray<TSoftObjectPtr<UPhysicsControlProfileAsset>> AdditionalProfileAssets;

	/**
	* The skeletal mesh to use for generating controls and previewing. If it turns out this 
	* doesn't need to be stored in the asset, it will get moved out of here.
	*/
	UPROPERTY(AssetRegistrySearchable, EditAnywhere, Category = PreviewMesh)
	TSoftObjectPtr<USkeletalMesh> PreviewSkeletalMesh;

public:
	// "My" runtime data - i.e. the data that will be combined with what has been inherited
	// We should have custom UI that displays this combined with the inherited data

	/**
	 * We can define controls in the form of limbs etc here
	 */
	UPROPERTY(EditAnywhere, Category = ProfileData, meta=(DisplayName="Character Setup Data"))
	FPhysicsControlCharacterSetupData MyCharacterSetupData;

	/**
	 * Additional controls and modifiers. If these have the same name as one that's 
	 * already created, they'll just override it.
	 */
	UPROPERTY(EditAnywhere, Category = ProfileData, meta = (DisplayName = "Additional Controls and Modifiers"))
	FPhysicsControlAndBodyModifierCreationDatas MyAdditionalControlsAndModifiers;

	/**
	 * Additional control and body modifier sets
	 */
	UPROPERTY(EditAnywhere, Category = ProfileData, meta = (DisplayName = "Additional Sets"))
	FPhysicsControlSetUpdates MyAdditionalSets;

	/**
	 * Initial updates to apply immediately after controls and modifiers are created
	 */
	UPROPERTY(EditAnywhere, Category = ProfileData, meta = (DisplayName = "Initial Control and Modifier Updates"))
	TArray<FPhysicsControlControlAndModifierUpdates> MyInitialControlAndModifierUpdates;

	/**
	 * The named profiles, which are essentially control and modifier updates
	 */
	UPROPERTY(EditAnywhere, Category = ProfileData, meta = (DisplayName = "Profiles"))
	TMap<FName, FPhysicsControlControlAndModifierUpdates> MyProfiles;
#endif

public:
	// Buttons/actions in the editor

#if WITH_EDITOR
	/** Shows all the controls etc that would be made */
	UFUNCTION(CallInEditor, Category = Development)
	void ShowCompiledData() const;

	/** 
	 * Collapses inherited and authored profiles etc to make a profile asset that can be read without 
	 * need for subsequent processing.
	 */
	UFUNCTION(CallInEditor)
	void Compile();

	/** Combines and returns data from our parent and ourself */
	FPhysicsControlCharacterSetupData GetCharacterSetupData() const;

	/** Combines and returns data from our parent and ourself */
	FPhysicsControlAndBodyModifierCreationDatas GetAdditionalControlsAndModifiers() const;

	/** Combines and returns data from our parent and ourself */
	FPhysicsControlSetUpdates GetAdditionalSets() const;

	/** Combines and returns data from our parent and ourself */
	TArray<FPhysicsControlControlAndModifierUpdates> GetInitialControlAndModifierUpdates() const;

	/** Combines and returns data from our parent and ourself */
	TMap<FName, FPhysicsControlControlAndModifierUpdates> GetProfiles() const;

#endif

public:

	/** IInterface_PreviewMeshProvider interface */
	virtual void SetPreviewMesh(USkeletalMesh* PreviewMesh, bool bMarkAsDirty = true) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	/** END IInterface_PreviewMeshProvider interface */

#if WITH_EDITOR
	/* Get name of Preview Mesh property */
	static const FName GetPreviewMeshPropertyName();
#endif

};
