// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Engine/CollisionProfile.h"

#include <type_traits>

#include "PCGActorHelpers.generated.h"

class AActor;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UPCGComponent;
class UMaterialInterface;
class UActorComponent;
class ULevel;
class UWorld;

struct FPCGISMCBuilderParameters
{
	UStaticMesh* Mesh = nullptr;
	TArray<UMaterialInterface*> MaterialOverrides;
	EComponentMobility::Type Mobility = EComponentMobility::Static;
	FName CollisionProfile = TEXT("Default");
	int32 NumCustomDataFloats = 0;
	float CullStartDistance = 0;
	float CullEndDistance = 0;
};

UCLASS(BlueprintType)
class PCG_API UPCGActorHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UInstancedStaticMeshComponent* GetOrCreateISMC(AActor* InActor, UPCGComponent* SourceComponent, const FPCGISMCBuilderParameters& Params);
	static bool DeleteActors(UWorld* World, const TArray<TSoftObjectPtr<AActor>>& ActorsToDelete);

	/**
	* Fetches all the components of ActorClass's CDO, including the ones added via the BP editor (which AActor.GetComponents fails to do)
	* @param ActorClass class of AActor for which we will retrieve all components
	* @param OutComponents this is where the found components will end up. Note that the preexisting contents of OutComponents will get overridden.
	* @param InComponentClass if supplied will be used to filter the results
	*/
	static void GetActorClassDefaultComponents(const TSubclassOf<AActor>& ActorClass, TArray<UActorComponent*>& OutComponents, const TSubclassOf<UActorComponent>& InComponentClass = TSubclassOf<UActorComponent>());

	template <typename T, typename = typename std::enable_if_t<std::is_base_of_v<AActor, T>>>
	inline static void ForEachActorInLevel(ULevel* Level, TFunctionRef<bool(AActor*)> Callback)
	{
		return ForEachActorInLevel(Level, T::StaticClass(), Callback);
	}

	/**
	* Iterate over all actors in the level, from the given class, and pass them to a callback
	* @param Level The level
	* @param ActorClass class of AActor to pass to the callback
	* @param Callback Function to call with the found actor. Needs to return a bool, to indicate if it needs to continue (true = yes)
	*/
	static void ForEachActorInLevel(ULevel* Level, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(AActor*)> Callback);

	template <typename T, typename = typename std::enable_if_t<std::is_base_of_v<AActor, T>>>
	inline static void ForEachActorInWorld(UWorld* World, TFunctionRef<bool(AActor*)> Callback)
	{
		return ForEachActorInWorld(World, T::StaticClass(), Callback);
	}

	/**
	* Iterate over all actors in the world, from the given class, and pass them to a callback
	* @param World The world
	* @param ActorClass class of AActor to pass to the callback
	* @param Callback Function to call with the found actor. Needs to return a bool, to indicate if it needs to continue (true = yes)
	*/
	static void ForEachActorInWorld(UWorld* World, TSubclassOf<AActor> ActorClass, TFunctionRef<bool(AActor*)> Callback);

	/**
	* Spawn a new actor of type T and attach it to the parent (if not null)
	* @param World The world
	* @param BaseName Base name for the actor, will have a unique name
	* @param Transform The transform for the new actor
	* @param Parent Optional parent to attach to.
	*/
	template <typename T = AActor, typename = typename std::enable_if_t<std::is_base_of_v<AActor, T>>>
	inline static AActor* SpawnDefaultActor(UWorld* World, FName BaseName, const FTransform& Transform, AActor* Parent = nullptr)
	{
		return SpawnDefaultActor(World, T::StaticClass(), BaseName, Transform, Parent);
	}

	/**
	* Spawn a new actor and attach it to the parent (if not null)
	* @param World The world
	* @param ActorClass Class of the actor to spawn
	* @param BaseName Base name for the actor, will have a unique name
	* @param Transform The transform for the new actor
	* @param Parent Optional parent to attach to.
	*/
	static AActor* SpawnDefaultActor(UWorld* World, TSubclassOf<AActor> ActorClass, FName BaseName, const FTransform& Transform, AActor* Parent = nullptr);

	/**
	 * Return the grid cell coordinates on the PCG partition grid given a position and the grid size.
	 */
	static FIntVector GetCellCoord(FVector InPosition, int InGridSize, bool bUse2DGrid);
};