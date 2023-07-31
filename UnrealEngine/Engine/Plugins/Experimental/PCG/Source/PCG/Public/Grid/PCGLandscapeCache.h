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

struct FPCGLandscapeCacheEntry
{
	friend class UPCGLandscapeCache;

public:
	void GetPoint(int32 PointIndex, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetPointHeightOnly(int32 PointIndex, FPCGPoint& OutPoint) const;
	void GetInterpolatedPoint(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const;
	void GetInterpolatedPointHeightOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint) const;

private:
	// Private API for UPCGLandscapeCache usage
	bool TouchAndLoad(int32 Touch) const;
	void Unload();
	int32 GetMemorySize() const;

#if WITH_EDITOR
	void BuildCacheData(ULandscapeInfo* LandscapeInfo, ULandscapeComponent* InComponent);
#endif

	// Serialize called from the landscape cache
	void Serialize(FArchive& Ar, UObject* Owner, int32 BulkIndex);

	// Internal usage methods
	void SerializeToBulkData();
	void SerializeFromBulkData() const;

	// Serialized data
	TWeakObjectPtr<const ULandscapeComponent> Component = nullptr;
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

	void PrimeCache();
	void ClearCache();
	void Tick(float DeltaSeconds);

	const FPCGLandscapeCacheEntry* GetCacheEntry(ULandscapeComponent* LandscapeComponent, const FIntPoint& ComponentKey);
	TArray<FName> GetLayerNames(ALandscapeProxy* Landscape);

private:
#if WITH_EDITOR
	void SetupLandscapeCallbacks();
	void TeardownLandscapeCallbacks();
	void OnLandscapeChanged(ALandscapeProxy* Landscape, const FLandscapeProxyComponentDataChangedParams& ChangeParams);
	void CacheLayerNames(ALandscapeProxy* Landscape);
	void CacheLayerNames();
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