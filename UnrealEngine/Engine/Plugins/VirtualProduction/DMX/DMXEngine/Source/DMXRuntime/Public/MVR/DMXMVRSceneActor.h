// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "DMXMVRSceneActor.generated.h"

class FDMXMVRFixtureActorLibrary;
class UDMXEntityFixturePatch;
class UDMXImportGDTF;
class UDMXLibrary;
class UDMXMVRSceneComponent;

class UFactory;


USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXMVRSceneGDTFToActorClassPair
{
	GENERATED_BODY()

	/** The GDTF of the Actor (may not correlate with its Patch after the Patch changed) */
	UPROPERTY(VisibleAnywhere, Category = "MVR")
	TSoftObjectPtr<UDMXImportGDTF> GDTF;
	
	/** The Actor Class that should be or was spawned */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = MVR, NoClear, Meta = (MustImplement = "/Script/DMXFixtureActorInterface.DMXMVRFixtureActorInterface"))
	TSoftClassPtr<AActor> ActorClass;
};

UCLASS(NotBlueprintable)
class DMXRUNTIME_API ADMXMVRSceneActor
	: public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ADMXMVRSceneActor();

	/** Destructor */
	~ADMXMVRSceneActor();

	//~ Begin AActor interface
	virtual void PostLoad() override;
	virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor interface

#if WITH_EDITOR
	/** Sets the dmx library for this MVR actor. Should only be called once, further calls will have no effect and hit an ensure condition */
	void SetDMXLibrary(UDMXLibrary* NewDMXLibrary);

	/** Refreshes the scene from the DMX Library */
	void RefreshFromDMXLibrary();

	/** Returns an array of actors that are currently spawned in the scene for specified GDTF */
	TArray<AActor*> GetActorsSpawnedForGDTF(const UDMXImportGDTF* GDTF) const;
#endif
	/** Returns the DMX Library of this MVR Scene Actor */
	FORCEINLINE UDMXLibrary* GetDMXLibrary() const { return DMXLibrary; }

	/** Returns the MVR UUID To Related Actor Map */
	FORCEINLINE const TArray<TSoftObjectPtr<AActor>>& GetRelatedActors() const { return RelatedActors; };

#if WITH_EDITOR
	// Property name getters
	static FName GetDMXLibraryPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, DMXLibrary); }
	static FName GetRelatedAcctorsPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, RelatedActors); }
	static FName GetGDTFToDefaultActorClassesPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, GDTFToDefaultActorClasses); }
	static FName GetMVRSceneRootPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXMVRSceneActor, MVRSceneRoot); }
#endif

#if WITH_EDITORONLY_DATA
	/** If checked, respawns Fixture Actors deleted from the MVR Scene */
	UPROPERTY(EditAnywhere, Category = "MVR", Meta = (DisplayName = "Respawn Deleted Actors"))
	bool bRespawnDeletedActorsOnRefresh = false;

	/** If checked, updates transforms from the DMX Library */
	UPROPERTY(EditAnywhere, Category = "MVR", Meta = (DisplayName = "Update Transforms"))
	bool bUpdateTransformsOnRefresh = false;
#endif

private:
	/** Set MVR UUIDs for related Actors */
	void EnsureMVRUUIDsForRelatedActors();

	/** Updates the GDTFToDefaultActorClasses map so it only contains those entries present both in the DMX Libraryand the MVR Scene */
	void UpdateGDTFToDefaultActorClasses(const TSharedRef<FDMXMVRFixtureActorLibrary>& MVRFixtureActorLibrary);

	/** Gets the Fixture Patch of an Actor, or nullptr if the Fixture Patch cannot be retrieved anymore */
	UDMXEntityFixturePatch* GetFixturePatchFromActor(AActor* Actor) const;

	/** Sets the Fixture Patch on an Actor if possible or fails silently */
	void SetFixturePatchOnActor(AActor* Actor, UDMXEntityFixturePatch* FixturePatch);

#if WITH_EDITOR
	/** Called when a sub-level is loaded */
	void OnMapChange(uint32 MapChangeFlags);

	/** Called when an actor got deleted in editor */
	void OnActorDeleted(AActor* DeletedActor);

	/** Called when an asset was imported */
	void OnAssetPostImport(UFactory* InFactory, UObject* ActorAdded);

	/** Replaces related Actors that use the Default Actor Class for the GDTF with an instance of the new Default Actor Class */
	void HandleDefaultActorClassForGDTFChanged();

	/** Spawns an MVR Actor in this Scene. Returns the newly spawned Actor, or nullptr if no Actor could be spawned. */
	AActor* SpawnMVRActor(const TSubclassOf<AActor>&ActorClass, UDMXEntityFixturePatch * FixturePatch, const FTransform & Transform, AActor * Template = nullptr);

	/** Replaces an MVR Actor in this Scene with another. Returns the newly spawned Actor, or nullptr if no Actor could be spawned. */
	AActor* ReplaceMVRActor(AActor* ActorToReplace, const TSubclassOf<AActor>& ClassOfNewActor);
#endif // WITH_EDITOR

	/** The DMX Library this Scene Actor uses */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "MVR", Meta = (AllowPrivateAccess = true))
	TObjectPtr<UDMXLibrary> DMXLibrary;

	/** The actors that created along with this scene */
	UPROPERTY(VisibleAnywhere, Category = "Actor", AdvancedDisplay, Meta = (AllowPrivateAccess = true))
	TArray<TSoftObjectPtr<AActor>> RelatedActors;
	
	/** The actor class that is spawned for a specific GDTF by default (can be overriden per MVR UUID, see below) */
	UPROPERTY(EditAnywhere, Category = "MVR", Meta = (DispayName = "Default Actor used for GDTF"))
	TArray<FDMXMVRSceneGDTFToActorClassPair> GDTFToDefaultActorClasses;

	/** Array holding MVR Fixture UUIDs that were explicitly removed from the scene */
	UPROPERTY()
	TArray<FGuid> DeletedMVRFixtureUUIDs;

#if WITH_EDITORONLY_DATA
	/** The GDTFToDefaultActorClassMap cached of PreEditChange, to find changes. */
	TArray<FDMXMVRSceneGDTFToActorClassPair> GDTFToDefaultActorClasses_PreEditChange;
#endif

	/** The root component to which all actors are attached initially */
	UPROPERTY(VisibleAnywhere, Category = "Actor", AdvancedDisplay, Meta = (AllowPrivateAccess = true))
	TObjectPtr<USceneComponent> MVRSceneRoot;
};
