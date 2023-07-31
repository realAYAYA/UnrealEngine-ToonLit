// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ActorPartition/PartitionActor.h"

#include "PCGPartitionActor.generated.h"

class UPCGComponent;
class UPCGSubsystem;
class UBoxComponent;

/** 
* The APCGPartitionActor actor is used to store grid cell data
* and its size will be a multiple of the grid size.
*/
UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable)
class APCGPartitionActor : public APartitionActor
{
	GENERATED_BODY()

public:
	APCGPartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~Begin UObject Interface
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	//~End UObject Interface

	//~Begin AActor Interface
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Destroyed() override;
	virtual void GetActorBounds(bool bOnlyCollidingComponents, FVector& Origin, FVector& BoxExtent, bool bIncludeFromChildActors) const override;
	virtual void PostRegisterAllComponents() override;
#if WITH_EDITOR
	virtual FBox GetStreamingBounds() const override;
	virtual AActor* GetSceneOutlinerParent() const override;
#endif
	//~End AActor Interface

#if WITH_EDITOR
	//~Begin APartitionActor Interface
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const override;
	virtual FGuid GetGridGuid() const override { return PCGGuid; }
	//~End APartitionActor Interface
#endif

	FBox GetFixedBounds() const;
	FIntVector GetGridCoord() const;

	uint32 GetGridSize() const { return PCGGridSize; }
	bool IsUsing2DGrid() const { return bUse2DGrid; }

	void AddGraphInstance(UPCGComponent* OriginalComponent);
	void RemapGraphInstance(const UPCGComponent* OldOriginalComponent, UPCGComponent* NewOriginalComponent);
	bool RemoveGraphInstance(UPCGComponent* OriginalComponent);
	void CleanupDeadGraphInstances();

#if WITH_EDITOR
	/** To be called after the creation of a new actor to copy the GridSize property (Editor only) into the PCGGridSize property */
	void PostCreation();

	/** [Game thread only] Return if the actor is safe for deletion, meaning no generation is currently running on all original components. */
	bool IsSafeForDeletion() const;

	/** Return an array of all the PCGComponents on this actor */
	TSet<TObjectPtr<UPCGComponent>> GetAllLocalPCGComponents() const;

	/** Return a set of all the PCGComponents linked to this actor */
	TSet<TObjectPtr<UPCGComponent>> GetAllOriginalPCGComponents() const;
#endif

	// TODO: Make this in-editor only; during runtime, we should keep a map of component to bounds/volume only
	// and preferably precompute the intersection, so this would make it easier/possible to not have the original actor in game version.
	UFUNCTION(BlueprintCallable, Category = "PCG|PartitionActor")
	UPCGComponent* GetLocalComponent(const UPCGComponent* OriginalComponent) const;

	UFUNCTION(BlueprintCallable, Category = "PCG|PartitionActor")
	UPCGComponent* GetOriginalComponent(const UPCGComponent* LocalComponent) const;

	UPROPERTY()
	FGuid PCGGuid;

private:
	UPCGSubsystem* GetSubsystem() const;

	// TODO: Make these properties editor only (see comment before).
	UPROPERTY()
	TMap<TObjectPtr<UPCGComponent>, TObjectPtr<UPCGComponent>> OriginalToLocalMap;

	UPROPERTY()
	TMap<TObjectPtr<UPCGComponent>, TObjectPtr<UPCGComponent>> LocalToOriginalMap;

	UPROPERTY(VisibleAnywhere, Category = WorldPartition)
	uint32 PCGGridSize;

	UPROPERTY(VisibleAnywhere, Category = WorldPartition)
	bool bUse2DGrid;

#if WITH_EDITORONLY_DATA
	/** Box component to draw the Partition actor bounds in the Editor viewport */
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> BoundsComponent;

	/** Utility bool to check if PostCreation/PostLoad was called. */
	bool bWasPostCreatedLoaded = false;
#endif // WITH_EDITORONLY_DATA
};
