// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"


#include "PCGWorldActor.generated.h"

class UPCGLandscapeCache;
namespace EEndPlayReason { enum Type : int; }

UCLASS(MinimalAPI, NotBlueprintable, NotPlaceable)
class APCGWorldActor : public AActor
{
	GENERATED_BODY()

public:
	APCGWorldActor(const FObjectInitializer& ObjectInitializer);

	//~Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR	
	virtual bool CanChangeIsSpatiallyLoadedFlag() const { return false; }
	virtual bool IsUserManaged() const override { return false; }
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform);
	//~End AActor Interface

	static APCGWorldActor* CreatePCGWorldActor(UWorld* InWorld);
#endif

	static inline constexpr uint32 DefaultPartitionGridSize = 25600; // 256m

	//~ Begin UObject Interface.
#if WITH_EDITOR	
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.

	/** Size of the grid for PCG partition actors */
	UPROPERTY(config, EditAnywhere, Category = WorldPartition)
	uint32 PartitionGridSize;

	/** Contains all the PCG data required to query the landscape complete. Serialized in cooked builds only */
	UPROPERTY(VisibleAnywhere, Category = CachedData, meta = (DisplayName="Landscape Cache"))
	TObjectPtr<UPCGLandscapeCache> LandscapeCacheObject = nullptr;

	/** Disable creation of Partition Actors on the Z axis. Can improve performances if 3D partitioning is not needed. */
	UPROPERTY(config, EditAnywhere, Category = WorldPartition)
	bool bUse2DGrid = true;

private:
	void RegisterToSubsystem();
	void UnregisterFromSubsystem();

#if WITH_EDITOR	
	void OnPartitionGridSizeChanged();
#endif
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Grid/PCGLandscapeCache.h"
#endif
