// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/Guid.h"
#include "PartitionActor.generated.h"

#if WITH_EDITOR
struct FActorPartitionIdentifier;
#endif

// Actor base class for instance containers placed on a grid.
// See UActorPartitionSubsystem.
UCLASS(Abstract)
class ENGINE_API APartitionActor: public AActor
{
	GENERATED_BODY()

public:
	APartitionActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin AActor Interface
#if WITH_EDITOR	
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	virtual TUniquePtr<class FWorldPartitionActorDesc> CreateClassActorDesc() const override;
	virtual uint32 GetDefaultGridSize(UWorld* InWorld) const PURE_VIRTUAL(APartitionActor, return 0;)
	virtual bool ShouldIncludeGridSizeInName(UWorld * InWorld, const FActorPartitionIdentifier& InIdentifier) const;
	virtual FGuid GetGridGuid() const { return FGuid(); }
	virtual bool IsUserManaged() const override;
#endif
	//~ End AActor Interface	

#if WITH_EDITORONLY_DATA
	/** The grid size this actors was generated for */
	UPROPERTY()
	uint32 GridSize;
#endif
};

DEFINE_ACTORDESC_TYPE(APartitionActor, FPartitionActorDesc);
