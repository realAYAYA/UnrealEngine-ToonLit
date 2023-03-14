// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Factories/AssetFactoryInterface.h"
#include "Engine/World.h"

#include "ActorFactory.generated.h"

UNREALED_API DECLARE_LOG_CATEGORY_EXTERN(LogActorFactory, Log, All);

class AActor;
struct FActorSpawnParameters;
struct FAssetData;
class UBlueprint;
class ULevel;
class UInstancedPlacemenClientSettings;
class AVolume;
class UBrushBuilder;
UCLASS(collapsecategories, hidecategories=Object, editinlinenew, config=Editor, abstract, transient)
class UNREALED_API UActorFactory : public UObject, public IAssetFactoryInterface
{
	GENERATED_UCLASS_BODY()

	/** Name used as basis for 'New Actor' menu. */
	UPROPERTY()
	FText DisplayName;

	/** Indicates how far up the menu item should be. The higher the number, the higher up the list.*/
	UPROPERTY(config)
	int32 MenuPriority;

	/** name of actor subclass this actorfactory creates - dynamically loaded.  Overrides NewActorClass. */
	UPROPERTY(config)
	FString NewActorClassName;

	/**  AActor  subclass this ActorFactory creates. */
	UPROPERTY()
	TSubclassOf<AActor>  NewActorClass;

	/** Whether to appear in the editor add actor quick menu */
	UPROPERTY()
	uint32 bShowInEditorQuickMenu:1;

	UPROPERTY()
	uint32 bUseSurfaceOrientation:1;

	UPROPERTY()
	uint32 bUsePlacementExtent:1;

	/** Translation applied to the spawn position. */
	UPROPERTY()
	FVector SpawnPositionOffset;

	/** Called to actual create an actor with the supplied transform (scale is ignored), using the properties in the ActorFactory */
	UE_DEPRECATED(5.0, "This function has been deprecated in favor of the other version that takes a FActorSpawnParameters in parameter")
	AActor* CreateActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags InObjectFlags, const FName InName = NAME_None);
	AActor* CreateActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams = FActorSpawnParameters());

	/** Called to create a blueprint class that can be used to spawn an actor from this factory */
	UE_DEPRECATED(5.0, "This function is no longer used. See FKismetEditorUtilities::CreateBlueprint.")
	UBlueprint* CreateBlueprint( UObject* Instance, UObject* Outer, const FName Name, const FName CallingContext = NAME_None );

	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg );

	/** Name to put on context menu. */
	FText GetDisplayName() const { return DisplayName; }

	/** Initialize NewActorClass if necessary, and return default actor for that class. */
	virtual AActor* GetDefaultActor( const FAssetData& AssetData );

	/** Initialize NewActorClass if necessary, and return that class. */
	virtual UClass* GetDefaultActorClass( const FAssetData& AssetData );

	/** Given an instance of an actor, find the wrapped asset object which can be used to create a valid FAssetData.
	 *  Returns nullptr if the given ActorInstance is not valid for this factory.
	 *  Override this function if the factory actor is a different class than the asset data's class which this factory operates on.
	 *  For example, if this is the static mesh actor factory, the class of the asset data is UStaticMesh, but the actor factory's class is AStaticMeshActor
	 */
	virtual UObject* GetAssetFromActorInstance(AActor* ActorInstance);

	/** Return a quaternion which aligns this actor type to the specified surface normal */
	virtual FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation = FQuat::Identity) const;

	// Begin IAssetFactoryInterface Interface
	virtual bool CanPlaceElementsFromAssetData(const FAssetData& InAssetData) override;
	virtual bool PrePlaceAsset(FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual TArray<FTypedElementHandle> PlaceAsset(const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual void PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	virtual FAssetData GetAssetDataFromElementHandle(const FTypedElementHandle& InHandle) override;
	virtual void BeginPlacement(const FPlacementOptions& InPlacementOptions) override;
	virtual void EndPlacement(TArrayView<const FTypedElementHandle> InPlacedElements, const FPlacementOptions& InPlacementOptions) override;
	virtual UInstancedPlacemenClientSettings* FactorySettingsObjectForPlacement(const FAssetData& InAssetData, const FPlacementOptions& InPlacementOptions) override;
	// End IAssetFactoryInterface Interface

	static void CreateBrushForVolumeActor(AVolume* NewActor, UBrushBuilder* BrushBuilder);

protected:

	/** Validates the input params for SpawnActor and returns the appropriate level to use depending on whether InLevel and/or InSpawnParams.OverrideLevel is passed : */
	ULevel* ValidateSpawnActorLevel(ULevel* InLevel, const FActorSpawnParameters& InSpawnParams) const;

	virtual bool PreSpawnActor( UObject* Asset, FTransform& InOutLocation);

	UE_DEPRECATED(5.0, "This function has been deprecated in favor of the other version that takes a FActorSpawnParameters in parameter")
	virtual AActor* SpawnActor(UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name ) final;
	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams);

	/** Subclasses may implement this to modify the actor after it has been spawned 
	    IMPORTANT: If you override this, you should usually also override PostCreateBlueprint()! */
	virtual void PostSpawnActor( UObject* Asset, AActor* NewActor );

	/** Override this in derived factory classes if needed.  This is called after a blueprint is created by this factory to
	    update the blueprint's CDO properties with state from the asset for this factory.
		IMPORTANT: If you override this, you should usually also override PostSpawnActor()! */
	virtual void PostCreateBlueprint( UObject* Asset, AActor* CDO );

	/**
	 * Get the default label that should be used for the actor spawned by the given asset (does not include any numeric suffix).
	 * For classes or BPs that inherit from AActor this will defer to AActor::GetDefaultActorLabel, and for everything else it will use the asset name.
	 */
	virtual FString GetDefaultActorLabel(UObject* Asset) const;
};

extern UNREALED_API FQuat FindActorAlignmentRotation(const FQuat& InActorRotation, const FVector& InModelAxis, const FVector& InWorldNormal, FQuat* OutDeltaRotation = nullptr);