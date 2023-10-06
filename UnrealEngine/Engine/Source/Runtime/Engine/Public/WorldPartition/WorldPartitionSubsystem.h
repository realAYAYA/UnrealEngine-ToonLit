// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartitionStreamingSource.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldPartition/Filter/WorldPartitionActorFilter.h"
#include "WorldPartition/WorldPartitionActorContainerID.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/Guid.h"
#include "WorldPartitionSubsystem.generated.h"

class ULevel;
class ULevelStreaming;
class UWorldPartition;
class UActorDescContainer;
class FWorldPartitionActorDesc;
class FWorldPartitionDraw2DContext;

enum class EWorldPartitionRuntimeCellState : uint8;
enum class ELevelStreamingState : uint8;
enum class ELevelStreamingTargetState : uint8;

/**
 * Helper to compute streaming source velocity based on position history.
 */
struct FStreamingSourceVelocity
{
	FStreamingSourceVelocity(const FName& InSourceName);
	void Invalidate() { bIsValid = false; }
	bool IsValid() { return bIsValid; }
	float GetAverageVelocity(const FVector& NewPosition, const float CurrentTime);

private:
	enum { VELOCITY_HISTORY_SAMPLE_COUNT = 16 };
	bool bIsValid;
	FName SourceName;
	int32 LastIndex;
	float LastUpdateTime;
	FVector LastPosition;
	float VelocityHistorySum;
	TArray<float, TInlineAllocator<VELOCITY_HISTORY_SAMPLE_COUNT>> VelocityHistory;
};

/**
 * UWorldPartitionSubsystem
 */

UCLASS(MinimalAPI)
class UWorldPartitionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	ENGINE_API UWorldPartitionSubsystem();

	//~ Begin UObject Interface
#if WITH_EDITOR
	static ENGINE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif
	//~ End UObject Interface

	//~ Begin USubsystem Interface.
	ENGINE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	ENGINE_API virtual void Deinitialize() override;
	//~ End USubsystem Interface.

	//~ Begin UWorldSubsystem Interface.
	ENGINE_API virtual void UpdateStreamingState() override;
	//~ End UWorldSubsystem Interface.

	//~ Begin FTickableGameObject
	ENGINE_API virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickableInEditor() const override { return true; }
	ENGINE_API virtual ETickableTickType GetTickableTickType() const override;
	ENGINE_API virtual TStatId GetStatId() const override;
	//~End FTickableGameObject

	UFUNCTION(BlueprintCallable, Category = Streaming)
	ENGINE_API bool IsStreamingCompleted(EWorldPartitionRuntimeCellState QueryState, const TArray<FWorldPartitionStreamingQuerySource>& QuerySources, bool bExactState) const;

	/** Returns true if world partition is done streaming levels, adding them to the world or removing them from the world. */
	UFUNCTION(BlueprintCallable, Category = Streaming)
	ENGINE_API bool IsAllStreamingCompleted();

	/*
	 * Returns true if world partition is done streaming levels, adding them to the world or removing them from the world. 
	 * When provided, the test is reduced to streaming levels affected by the optional streaming source provider.
	 */
	ENGINE_API bool IsStreamingCompleted(const IWorldPartitionStreamingSourceProvider* InStreamingSourceProvider = nullptr) const;

	ENGINE_API void DumpStreamingSources(FOutputDevice& OutputDevice) const;

	ENGINE_API TSet<IWorldPartitionStreamingSourceProvider*> GetStreamingSourceProviders() const;
	ENGINE_API void RegisterStreamingSourceProvider(IWorldPartitionStreamingSourceProvider* StreamingSource);
	ENGINE_API bool IsStreamingSourceProviderRegistered(IWorldPartitionStreamingSourceProvider* StreamingSource) const;
	ENGINE_API bool UnregisterStreamingSourceProvider(IWorldPartitionStreamingSourceProvider* StreamingSource);

	DECLARE_DELEGATE_RetVal_OneParam(bool, FWorldPartitionStreamingSourceProviderFilter, const IWorldPartitionStreamingSourceProvider*);
	FWorldPartitionStreamingSourceProviderFilter& OnIsStreamingSourceProviderFiltered() { return IsStreamingSourceProviderFiltered; }

	ENGINE_API void ForEachWorldPartition(TFunctionRef<bool(UWorldPartition*)> Func);

#if WITH_EDITOR
	ENGINE_API FWorldPartitionActorFilter GetWorldPartitionActorFilter(const FString& InWorldPackage, EWorldPartitionActorFilterType InFilterTypes = EWorldPartitionActorFilterType::Loading) const;
	ENGINE_API TMap<FActorContainerID, TSet<FGuid>> GetFilteredActorsPerContainer(const FActorContainerID& InContainerID, const FString& InWorldPackage, const FWorldPartitionActorFilter& InActorFilter, EWorldPartitionActorFilterType InFilterTypes = EWorldPartitionActorFilterType::Loading);

	static ENGINE_API bool IsRunningConvertWorldPartitionCommandlet();

	UActorDescContainer* RegisterContainer(FName PackageName) { return ActorDescContainerInstanceManager.RegisterContainer(PackageName, GetWorld()); }
	void UnregisterContainer(UActorDescContainer* Container) { ActorDescContainerInstanceManager.UnregisterContainer(Container); }
	FBox GetContainerBounds(FName PackageName) const { return ActorDescContainerInstanceManager.GetContainerBounds(PackageName); }
	void UpdateContainerBounds(FName PackageName) { ActorDescContainerInstanceManager.UpdateContainerBounds(PackageName); }

	TSet<FWorldPartitionActorDesc*> SelectedActorDescs;

	class FActorDescContainerInstanceManager
	{
		friend class UWorldPartitionSubsystem;

		struct FActorDescContainerInstance
		{
			FActorDescContainerInstance()
				: Container(nullptr)
				, RefCount(0)
				, Bounds(ForceInit)
			{}

			void AddReferencedObjects(FReferenceCollector& Collector);
			void UpdateBounds();

			TObjectPtr<UActorDescContainer> Container;
			uint32 RefCount;
			FBox Bounds;
		};

		void AddReferencedObjects(FReferenceCollector& Collector);

	public:
		UActorDescContainer* RegisterContainer(FName PackageName, UWorld* InWorld);
		void UnregisterContainer(UActorDescContainer* Container);

		FBox GetContainerBounds(FName PackageName) const;
		void UpdateContainerBounds(FName PackageName);

	private:
		TMap<FName, FActorDescContainerInstance> ActorDescContainers;
	};
private:
	ENGINE_API FWorldPartitionActorFilter GetWorldPartitionActorFilterInternal(const FString& InWorldPackage, EWorldPartitionActorFilterType InFilterTypes, TSet<FString>& InOutVisitedPackages) const;
#endif

protected:
	//~ Begin USubsystem Interface.
	ENGINE_API virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End USubsystem Interface.

private:

	// Streaming Sources
	void UpdateStreamingSources();
	ENGINE_API void GetStreamingSources(const UWorldPartition* InWorldPartition, TArray<FWorldPartitionStreamingSource>& OutStreamingSources) const;
	uint32 GetStreamingSourcesHash() const { return StreamingSourcesHash; }

	void OnWorldPartitionInitialized(UWorldPartition* InWorldPartition);
	void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);
	void OnLevelStreamingStateChanged(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* LevelIfLoaded, ELevelStreamingState PreviousState, ELevelStreamingState NewState);
	void OnLevelStreamingTargetStateChanged(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* InLevelIfLoaded, ELevelStreamingState InCurrentState, ELevelStreamingTargetState InPrevTarget, ELevelStreamingTargetState InNewTarget);
	void OnLevelBeginMakingVisible(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* InLoadedLevel);
	void OnLevelBeginMakingInvisible(UWorld* InWorld, const ULevelStreaming* InStreamingLevel, ULevel* InLoadedLevel);
	void UpdateLoadingAndPendingLoadStreamingLevels(const ULevelStreaming* InStreamingLevel);
	bool IncrementalUpdateStreamingState();
	
	// Server information
	uint32 GetServerClientsVisibleLevelsHash() const { return ServerClientsVisibleLevelsHash; }
	void UpdateServerClientsVisibleLevelNames();

	static ENGINE_API void UpdateStreamingStateInternal(const UWorld* InWorld, UWorldPartition* InWorldPartition = nullptr);
	static int32 GetMaxCellsToLoad(const UWorld* InWorld);
	static bool IsServer(const UWorld* InWorld);

	ENGINE_API bool HasAnyWorldPartitionServerStreamingEnabled() const;
	ENGINE_API bool HasUninitializationPendingStreamingLevels(const UWorldPartition* InWorldPartition) const;

	ENGINE_API UWorldPartition* GetWorldPartition();
	ENGINE_API const UWorldPartition* GetWorldPartition() const;
	ENGINE_API bool CanDebugDraw() const;
	ENGINE_API void Draw(class UCanvas* Canvas, class APlayerController* PC);
	ENGINE_API void DrawStreamingStatusLegend(class UCanvas* Canvas, FVector2D& Offset, const UWorldPartition* InWorldPartition);
	friend class UWorldPartition;
	friend class UWorldPartitionStreamingPolicy;

	// Registered world partitions & incremental update
	TArray<TObjectPtr<UWorldPartition>> RegisteredWorldPartitions;
	TSet<TObjectPtr<UWorldPartition>> IncrementalUpdateWorldPartitions;
	TSet<TObjectPtr<UWorldPartition>> IncrementalUpdateWorldPartitionsPendingAdd;

	// Streaming Sources
	TSet<IWorldPartitionStreamingSourceProvider*> StreamingSourceProviders;
	FWorldPartitionStreamingSourceProviderFilter IsStreamingSourceProviderFiltered;
	TArray<FWorldPartitionStreamingSource> StreamingSources;
	TMap<FName, FStreamingSourceVelocity> StreamingSourcesVelocity;
	uint32 StreamingSourcesHash;

	// Server information
	int32 NumWorldPartitionServerStreamingEnabled;
	TSet<FName> ServerClientsVisibleLevelNames;
	uint32 ServerClientsVisibleLevelsHash;

	// Debug draw
	TArray<FWorldPartitionDraw2DContext> WorldPartitionsDraw2DContext;
	FDelegateHandle	DrawHandle;

	// GC backup values
	int32 LevelStreamingContinuouslyIncrementalGCWhileLevelsPendingPurge;
	int32 LevelStreamingForceGCAfterLevelStreamedOut;

	// Tracks streaming levels of uninitialized world partition used to delay next initialization until they're done being removed
	TMap<FSoftObjectPath, TSet<TWeakObjectPtr<ULevelStreaming>>> WorldPartitionUninitializationPendingStreamingLevels;

	// Tracks world partition loading and pending loads
	TSet<TWeakObjectPtr<const ULevelStreaming>> WorldPartitionLoadingAndPendingLoadStreamingLevels;

#if WITH_EDITOR
	bool bIsRunningConvertWorldPartitionCommandlet;
	mutable FActorDescContainerInstanceManager ActorDescContainerInstanceManager;
#endif
};
