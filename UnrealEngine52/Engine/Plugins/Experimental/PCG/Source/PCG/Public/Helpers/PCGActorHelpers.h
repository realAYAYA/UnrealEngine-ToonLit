// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"

#include "PCGActorHelpers.generated.h"

class AActor;
class UActorComponent;
class UInstancedStaticMeshComponent;
class ULevel;
class UMaterialInterface;
class UPCGComponent;
class UPCGManagedISMComponent;
class UStaticMesh;
class UWorld;

struct FPCGISMCBuilderParameters
{
	FISMComponentDescriptor Descriptor;
	int32 NumCustomDataFloats = 0;

	friend inline uint32 GetTypeHash(const FPCGISMCBuilderParameters& Key)
	{
		return HashCombine(GetTypeHash(Key.Descriptor), 1 + Key.NumCustomDataFloats);
	}

	inline bool operator==(const FPCGISMCBuilderParameters& Other) const { return Descriptor == Other.Descriptor && NumCustomDataFloats == Other.NumCustomDataFloats; }
};

UCLASS(BlueprintType)
class PCG_API UPCGActorHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static UInstancedStaticMeshComponent* GetOrCreateISMC(AActor* InTargetActor, UPCGComponent* SourceComponent, uint64 SettingsUID, const FPCGISMCBuilderParameters& Params);
	static UPCGManagedISMComponent* GetOrCreateManagedISMC(AActor* InTargetActor, UPCGComponent* SourceComponent, uint64 SettingsUID, const FPCGISMCBuilderParameters& Params);
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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/CollisionProfile.h"
#endif
