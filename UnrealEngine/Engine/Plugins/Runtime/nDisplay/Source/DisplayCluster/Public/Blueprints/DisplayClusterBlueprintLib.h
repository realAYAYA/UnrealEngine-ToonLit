// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Blueprints/IDisplayClusterBlueprintAPI.h"
#include "DisplayClusterBlueprintLib.generated.h"

class ADisplayClusterLightCardActor;
class ADisplayClusterRootActor;

/**
 * Blueprint API function library
 */
UCLASS()
class UDisplayClusterBlueprintLib
	: public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/** Return Display Cluster API interface. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DisplayCluster Module API"), Category = "NDisplay")
	static void GetAPI(TScriptInterface<IDisplayClusterBlueprintAPI>& OutAPI);

	/** Create a new light card parented to the given nDisplay root actor. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DisplayCluster Module API"), Category = "NDisplay")
	static ADisplayClusterLightCardActor* CreateLightCard(ADisplayClusterRootActor* RootActor);

	/** Create duplicates of a list of existing light cards. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DisplayCluster Module API"), Category = "NDisplay")
	static void DuplicateLightCards(TArray<ADisplayClusterLightCardActor*> OriginalLightcards, TArray<ADisplayClusterLightCardActor*>& OutNewLightCards);

	/** Gets a list of all light card actors on the level linked to the specified root actor. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "DisplayCluster Module API"), Category = "NDisplay")
	static DISPLAYCLUSTER_API void FindLightCardsForRootActor(ADisplayClusterRootActor* RootActor, TSet<ADisplayClusterLightCardActor*>& OutLightCards);
};
