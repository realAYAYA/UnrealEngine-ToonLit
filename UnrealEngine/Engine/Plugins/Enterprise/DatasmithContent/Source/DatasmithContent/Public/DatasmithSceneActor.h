// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "DatasmithSceneActor.generated.h"

class UFactory;

UCLASS()
class DATASMITHCONTENT_API ADatasmithSceneActor : public AActor
{
	GENERATED_BODY()

public:

	ADatasmithSceneActor();

	virtual ~ADatasmithSceneActor();

	UPROPERTY(VisibleAnywhere, Category="Datasmith")
	TObjectPtr<class UDatasmithScene> Scene;

	/** Map of all the actors related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< AActor > > RelatedActors;

private:
#if WITH_EDITOR
	// For backward compatibility with UE <= 4.21
	void EnsureDatasmithIdsForRelatedActors();

	// Backward compatibility fix will happen on PostLoad.
	void PostLoad() override;

	// We need this to monitor when a sub-level is loaded
	void OnMapChange(uint32 MapChangeFlags);

	// Cleans the invalid Soft Object Ptr
	void OnActorDeleted(AActor* ActorDeleted);

	// Reattach newly imported actors if they've been moved / cut and pasted
	void OnAssetPostImport(UFactory* InFactory, UObject* ActorAdded);

	FDelegateHandle OnActorDeletedDelegateHandle;
#endif // WITH_EDITOR
};
