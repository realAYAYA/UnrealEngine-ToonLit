// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/BulkData.h"

#include "PCGLandscapeCache.generated.h"

class ALandscapeProxy;
class FLandscapeProxyComponentDataChangedParams;
class ULandscapeComponent;
class ULandscapeInfo;
class UPCGPointData;
class UPCGMetadata;
struct FPCGPoint;

USTRUCT(BlueprintType)
struct FPCGLandscapeLayerWeight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Attribute")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landscape Attribute")
	float Weight = 0.0f;
};

namespace PCGLandscapeCache
{
	struct FSafeIndices
	{
		int32 X0Y0 = 0;
		int32 X1Y0 = 0;
		int32 X0Y1 = 0;
		int32 X1Y1 = 0;

		float XFraction = 0;
		float YFraction = 0;
	};

	// this will ensure all indicies are valid in a Stride*Stride sized array
	FSafeIndices CalcSafeIndices(FVector2D LocalPosition, int32 Stride);
}

struct FPCGLandscapeCacheEntry
{
	friend class UPCGLandscapeCache;

public:
	void GetPoint(int32 PointIndex, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetPointHeightOnly(int32 PointIndex, FPCGPoint& OutPoint) const;
	void GetInterpolatedPoint(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetInterpolatedPointMetadataOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetInterpolatedPointHeightOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetInterpolatedLayerWeights(const FVector2D& LocalPoint, TArray<FPCGLandscapeLayerWeight>& OutLayerWeights) const;

private:
	// Private API to remove boilerplate
	void GetInterpolatedPointInternal(const PCGLandscapeCache::FSafeIndices& Indices, FPCGPoint& OutPoint, bool bHeightOnly = false) const;
	void GetInterpolatedPointMetadataInternal(const PCGLandscapeCache::FSafeIndices& Indices, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;

	// Private API for UPCGLandscapeCache usage
	bool TouchAndLoad(int32 Touch) const;
	void Unload();
	int32 GetMemorySize() const;

#if WITH_EDITOR
	static FPCGLandscapeCacheEntry* CreateCacheEntry(ULandscapeInfo* LandscapeInfo, ULandscapeComponent* InComponent);
#endif

	// Serialize called from the landscape cache
	void Serialize(FArchive& Ar, UObject* Owner, int32 BulkIndex);

	// Internal usage methods
	void SerializeToBulkData();
	void SerializeFromBulkData() const;

	// Serialized data
	TArray<FName> LayerDataNames;
	FVector PointHalfSize = FVector::One();
	int32 Stride = 0;

	// Data built in editor or loaded from the bulk data
	mutable FByteBulkData BulkData;

	// Data stored in the BulkData
	mutable TArray<FVector> PositionsAndNormals;
	mutable TArray<TArray<uint8>> LayerData;

	// Transient data
	mutable FCriticalSection DataLock;
	mutable int32 Touch = 0;
	mutable bool bDataLoaded = false;
};

UCLASS()
class UPCGLandscapeCache : public UObject
{
	GENERATED_BODY()
public:
	//~Begin UObject interface
	virtual void BeginDestroy() override;
	virtual void Serialize(FArchive& Archive) override;
	//~End UObject interface

	/** Initialize cache. Can be safely called multiple times. */
	void Initialize();

	void PrimeCache();
	void ClearCache();
	void Tick(float DeltaSeconds);

#if WITH_EDITOR
	/** Gets (and creates if needed) the cache entry - available only in Editor */
	const FPCGLandscapeCacheEntry* GetCacheEntry(ULandscapeComponent* LandscapeComponent, const FIntPoint& ComponentKey);
#endif

	/** Gets landscape cache entry, works both in editor (but does not create) but works in game mode too. */
	const FPCGLandscapeCacheEntry* GetCacheEntry(const FGuid& LandscapeGuid, const FIntPoint& ComponentKey);

	TArray<FName> GetLayerNames(ALandscapeProxy* Landscape);

	/** Convenience method to get metadata from the landscape for a given pair of landscape and position */
	void SampleMetadataOnPoint(ALandscapeProxy* Landscape, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata);

private:
#if WITH_EDITOR
	void SetupLandscapeCallbacks();
	void TeardownLandscapeCallbacks();
	void OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams);
	void OnLandscapeMoved(AActor* InActor);
	void OnLandscapeAdded(AActor* Actor);
	void OnLandscapeDeleted(AActor* Actor);
	void CacheLayerNames(ALandscapeProxy* InLandscape);
	void CacheLayerNames();
	void RemoveComponentFromCache(const ALandscapeProxy* LandscapeProxy);
#endif

	TMap<TPair<FGuid, FIntPoint>, FPCGLandscapeCacheEntry*> CachedData;

	//TODO: separate by landscape
	UPROPERTY(VisibleAnywhere, Category = "Cache")
	TSet<FName> CachedLayerNames;

	std::atomic<int32> CacheMemorySize = 0;
	std::atomic<int32> CacheTouch = 0;
	float TimeSinceLastCleanupInSeconds = 0;
	bool bInitialized = false;

#if WITH_EDITOR
	TSet<TWeakObjectPtr<ALandscapeProxy>> Landscapes;
#endif

#if WITH_EDITOR
	FRWLock CacheLock;
#endif
};