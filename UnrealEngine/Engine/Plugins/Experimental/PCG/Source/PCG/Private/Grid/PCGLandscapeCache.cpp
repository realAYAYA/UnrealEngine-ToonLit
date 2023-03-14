// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGLandscapeCache.h"

#include "PCGSubsystem.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

#include "UObject/WeakObjectPtr.h"

#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeComponent.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "LandscapeProxy.h"
#include "LandscapeDataAccess.h"
#include "Landscape.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/BufferReader.h"
#include "Serialization/BufferWriter.h"

static TAutoConsoleVariable<float> CVarLandscapeCacheGCFrequency(
	TEXT("pcg.LandscapeCacheGCFrequency"),
	10,
	TEXT("Rate at which to release landscape cache data, in seconds"));

static TAutoConsoleVariable<int32> CVarLandscapeCacheSizeThreshold(
	TEXT("pcg.LandscapeCacheSizeThreshold"),
	64,
	TEXT("Memory Threhold at which we start cleaning up the landscape cache"));

#if WITH_EDITOR
void FPCGLandscapeCacheEntry::BuildCacheData(ULandscapeInfo* LandscapeInfo, ULandscapeComponent* InComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLandscapeCacheEntry::BuildCacheData);
	check(!Component.Get() && InComponent && PositionsAndNormals.Num() == 0 && !bDataLoaded);
	Component = InComponent;

	ALandscape* Landscape = Component->GetLandscapeActor();
	if (!Landscape)
	{
		return;
	}

	// Get landscape default layer (heightmap/tangents/normal)
	{
		FLandscapeComponentDataInterface CDI(InComponent, 0, /*bWorkInEditingLayer=*/false);
		FVector WorldPos;
		FVector WorldTangentX;
		FVector WorldTangentY;
		FVector WorldTangentZ;

		PointHalfSize = InComponent->GetComponentTransform().GetScale3D() * 0.5;

		// The component has an extra vertex on the edge, for interpolation purposes
		const int32 ComponentSizeQuads = InComponent->ComponentSizeQuads + 1;
		const int32 NumVertices = FMath::Square(ComponentSizeQuads);

		Stride = ComponentSizeQuads;

		PositionsAndNormals.Reserve(2 * NumVertices);
		for (int32 Index = 0; Index < NumVertices; ++Index)
		{
			CDI.GetWorldPositionTangents(Index, WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ);
			PositionsAndNormals.Add(WorldPos);
			PositionsAndNormals.Add(WorldTangentZ);
		}
	}
	
	// Get other layers, push data into metadata attributes
	TArray<uint8> LayerCache;
	for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
	{
		ULandscapeLayerInfoObject* LayerInfo = Layer.LayerInfoObj;
		if (!LayerInfo)
		{
			continue;
		}

		FLandscapeComponentDataInterface CDI(InComponent, 0, /*bWorkInEditingLayer=*/false);
		if (CDI.GetWeightmapTextureData(LayerInfo, LayerCache, /*bUseEditingLayer=*/false))
		{
			LayerDataNames.Add(Layer.LayerName);
			LayerData.Add(LayerCache);
		}

		LayerCache.Reset();
	}
	
	bDataLoaded = true;
}
#endif // WITH_EDITOR

void FPCGLandscapeCacheEntry::GetPoint(int32 PointIndex, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLandscapeCacheEntry::GetPoint);
	check(bDataLoaded && PointIndex >= 0 && 2 * PointIndex < PositionsAndNormals.Num());	

	const FVector& Position = PositionsAndNormals[2 * PointIndex];
	const FVector& Normal = PositionsAndNormals[2 * PointIndex + 1];
	
	FVector TangentX;
	FVector TangentY;
	TangentX = FVector(Normal.Z, 0.f, -Normal.X);
	TangentY = Normal ^ TangentX;

	OutPoint.Transform = FTransform(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), Normal.GetSafeNormal(), Position);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
	OutPoint.Seed = 1 + PointIndex;
	
	if (OutMetadata && !LayerData.IsEmpty())
	{
		check(LayerData.Num() == LayerDataNames.Num());
		OutPoint.MetadataEntry = OutMetadata->AddEntry();

		for(int32 LayerIndex = 0; LayerIndex < LayerData.Num(); ++LayerIndex)
		{
			const FName& CurrentLayerName = LayerDataNames[LayerIndex];
			const TArray<uint8>& CurrentLayerData = LayerData[LayerIndex];

			if (FPCGMetadataAttributeBase* Attribute = OutMetadata->GetMutableAttribute(CurrentLayerName))
			{
				check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id);
				static_cast<FPCGMetadataAttribute<float>*>(Attribute)->SetValue(OutPoint.MetadataEntry, (float)CurrentLayerData[PointIndex] / 255.0f);
			}
		}
	}
}

void FPCGLandscapeCacheEntry::GetPointHeightOnly(int32 PointIndex, FPCGPoint& OutPoint) const
{
	//TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLandscapeCacheEntry::GetPointHeightOnly);
	check(bDataLoaded && PointIndex >= 0 && 2 * PointIndex < PositionsAndNormals.Num());

	const FVector& Position = PositionsAndNormals[2 * PointIndex];
	
	OutPoint.Transform = FTransform(Position);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
	OutPoint.Seed = 1 + PointIndex;
}

void FPCGLandscapeCacheEntry::GetInterpolatedPoint(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	check(bDataLoaded);

	const int32 X0Y0 = FMath::FloorToInt(LocalPoint.X) + FMath::FloorToInt(LocalPoint.Y) * Stride;
	const int32 X1Y0 = X0Y0 + 1;
	const int32 X0Y1 = X0Y0 + Stride;
	const int32 X1Y1 = X0Y1 + 1;

	const float XFactor = FMath::Fractional(LocalPoint.X);
	const float YFactor = FMath::Fractional(LocalPoint.Y);

	check(X0Y0 >= 0 && 2 * X1Y1 < PositionsAndNormals.Num());

	const FVector& PositionX0Y0 = PositionsAndNormals[2 * X0Y0];
	const FVector& NormalX0Y0 = PositionsAndNormals[2 * X0Y0 + 1];
	const FVector& PositionX1Y0 = PositionsAndNormals[2 * X1Y0];
	const FVector& NormalX1Y0 = PositionsAndNormals[2 * X1Y0 + 1];
	const FVector& PositionX0Y1 = PositionsAndNormals[2 * X0Y1];
	const FVector& NormalX0Y1 = PositionsAndNormals[2 * X0Y1 + 1];
	const FVector& PositionX1Y1 = PositionsAndNormals[2 * X1Y1];
	const FVector& NormalX1Y1 = PositionsAndNormals[2 * X1Y1 + 1];

	const FVector LerpPositionY0 = FMath::Lerp(PositionX0Y0, PositionX1Y0, XFactor);
	const FVector LerpPositionY1 = FMath::Lerp(PositionX0Y1, PositionX1Y1, XFactor);
	const FVector Position = FMath::Lerp(LerpPositionY0, LerpPositionY1, YFactor);

	const FVector LerpNormalY0 = FMath::Lerp(NormalX0Y0.GetSafeNormal(), NormalX1Y0.GetSafeNormal(), XFactor).GetSafeNormal();
	const FVector LerpNormalY1 = FMath::Lerp(NormalX0Y1.GetSafeNormal(), NormalX1Y1.GetSafeNormal(), XFactor).GetSafeNormal();
	const FVector Normal = FMath::Lerp(LerpNormalY0, LerpNormalY1, YFactor);

	FVector TangentX;
	FVector TangentY;
	TangentX = FVector(Normal.Z, 0.f, -Normal.X);
	TangentY = Normal ^ TangentX;

	OutPoint.Transform = FTransform(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), Normal.GetSafeNormal(), Position);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
	OutPoint.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Position);

	if (OutMetadata && !LayerData.IsEmpty())
	{
		OutPoint.MetadataEntry = OutMetadata->AddEntry();

		check(LayerData.Num() == LayerDataNames.Num());

		for(int32 LayerIndex = 0; LayerIndex < LayerData.Num(); ++LayerIndex)
		{
			const FName& CurrentLayerName = LayerDataNames[LayerIndex];
			const TArray<uint8>& CurrentLayerData = LayerData[LayerIndex];

			if (FPCGMetadataAttributeBase* Attribute = OutMetadata->GetMutableAttribute(CurrentLayerName))
			{
				check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id);
				float Y0Data = FMath::Lerp((float)CurrentLayerData[X0Y0] / 255.0f, (float)CurrentLayerData[X1Y0] / 255.0f, XFactor);
				float Y1Data = FMath::Lerp((float)CurrentLayerData[X0Y1] / 255.0f, (float)CurrentLayerData[X1Y1] / 255.0f, XFactor);
				float Data = FMath::Lerp(Y0Data, Y1Data, YFactor);

				static_cast<FPCGMetadataAttribute<float>*>(Attribute)->SetValue(OutPoint.MetadataEntry, Data);
			}
		}
	}
}

void FPCGLandscapeCacheEntry::GetInterpolatedPointHeightOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint) const
{
	check(bDataLoaded);

	const int32 X0Y0 = FMath::FloorToInt(LocalPoint.X) + FMath::FloorToInt(LocalPoint.Y) * Stride;
	const int32 X1Y0 = X0Y0 + 1;
	const int32 X0Y1 = X0Y0 + Stride;
	const int32 X1Y1 = X0Y1 + 1;

	const float XFactor = FMath::Fractional(LocalPoint.X);
	const float YFactor = FMath::Fractional(LocalPoint.Y);

	check(X0Y0 >= 0 && 2 * X1Y1 < PositionsAndNormals.Num());

	const FVector& PositionX0Y0 = PositionsAndNormals[2 * X0Y0];
	const FVector& PositionX1Y0 = PositionsAndNormals[2 * X1Y0];
	const FVector& PositionX0Y1 = PositionsAndNormals[2 * X0Y1];
	const FVector& PositionX1Y1 = PositionsAndNormals[2 * X1Y1];

	const FVector LerpPositionY0 = FMath::Lerp(PositionX0Y0, PositionX1Y0, XFactor);
	const FVector LerpPositionY1 = FMath::Lerp(PositionX0Y1, PositionX1Y1, XFactor);
	const FVector Position = FMath::Lerp(LerpPositionY0, LerpPositionY1, YFactor);

	OutPoint.Transform = FTransform(Position);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
	OutPoint.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Position);
}

bool FPCGLandscapeCacheEntry::TouchAndLoad(int32 InTouch) const
{
	Touch = InTouch; // technically, this could be an atomic, but we don't need this to be very precise

#if WITH_EDITOR
	check(bDataLoaded);
	return false; // In editor, we're "always" loaded since it's created on the fly
#else
	if (!bDataLoaded)
	{
		FScopeLock ScopeDataLock(&DataLock);
		if (!bDataLoaded)
		{
			SerializeFromBulkData();
			return true;
		}
	}

	return false;
#endif
}

void FPCGLandscapeCacheEntry::Unload()
{
	check(bDataLoaded);
	PositionsAndNormals.Reset();
	PositionsAndNormals.Shrink();
	LayerData.Reset();
	LayerData.Shrink();
	bDataLoaded = false;
	Touch = 0;
}

int32 FPCGLandscapeCacheEntry::GetMemorySize() const
{
	int32 MemSize = PositionsAndNormals.GetAllocatedSize();
	for (const TArray<uint8>& Layer : LayerData)
	{
		MemSize += Layer.GetAllocatedSize();
	}

	return MemSize;
}

void FPCGLandscapeCacheEntry::SerializeToBulkData()
{
	// Move data from local arrays to the bulk data
	BulkData.Lock(LOCK_READ_WRITE);

	int32 NumBytes = 0;
	// Number of entries in the array + size of the array
	NumBytes += sizeof(int32) + PositionsAndNormals.Num() * PositionsAndNormals.GetTypeSize();

	// Number of layers
	NumBytes += sizeof(int32); 
	for (TArray<uint8>& CurrentLayerData : LayerData)
	{
		// Number of entries in the layer data array + size of the array
		NumBytes += sizeof(int32) + CurrentLayerData.Num() * CurrentLayerData.GetTypeSize();
	}

	uint8* Dest = (uint8*)BulkData.Realloc(NumBytes);
	FBufferWriter Ar(Dest, NumBytes);
	Ar.SetIsPersistent(true);

	Ar << PositionsAndNormals;
	
	int32 LayerDataCount = LayerData.Num();
	Ar << LayerDataCount;

	for (TArray<uint8>& CurrentLayerData : LayerData)
	{
		Ar << CurrentLayerData;
	}

	BulkData.Unlock();
}

void FPCGLandscapeCacheEntry::SerializeFromBulkData() const
{
	check(!bDataLoaded);

	// Note: this call is not threadsafe by itself, it is meant to be called from a locked region
	uint8* Data = nullptr;
	BulkData.GetCopy((void**)&Data);
	int32 DataSize = BulkData.GetBulkDataSize();

	FBufferReader Ar(Data, DataSize, /*bInFreeOnClose=*/true, /*bIsPersistent=*/true);

	Ar << PositionsAndNormals;

	int32 LayerCount = 0;
	Ar << LayerCount;

	LayerData.SetNum(LayerCount);

	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		Ar << LayerData[LayerIndex];
	}

	bDataLoaded = true;
}

void FPCGLandscapeCacheEntry::Serialize(FArchive& Archive, UObject* Owner, int32 Index)
{
	check(bDataLoaded || Archive.IsLoading());

	if (bDataLoaded && Archive.IsSaving() && Archive.IsCooking())
	{
		SerializeToBulkData();
	}

	Archive << Component;
	Archive << PointHalfSize;
	Archive << Stride;
	Archive << LayerDataNames;

	// We force it not inline that means bulk data won't automatically be loaded when we deserialize
	// later but only when we explicitly take action to load it
	BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
	BulkData.Serialize(Archive, Owner, Index);
}

void UPCGLandscapeCache::BeginDestroy()
{
	ClearCache();
#if WITH_EDITOR
	TeardownLandscapeCallbacks();
#endif

	Super::BeginDestroy();
}

void UPCGLandscapeCache::Serialize(FArchive& Archive)
{
	Super::Serialize(Archive);

	if (Archive.IsSaving() && !Archive.IsCooking())
	{
		ClearCache();
	}

	// Serialize cache entries
	int32 NumEntries = (Archive.IsLoading() ? 0 : CachedData.Num());
	Archive << NumEntries;

	if (Archive.IsLoading())
	{
		CachedData.Reserve(NumEntries);

		for (int32 EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
		{
			TPair<FGuid, FIntPoint> Key;
			Archive << Key;

			FPCGLandscapeCacheEntry* Entry = new FPCGLandscapeCacheEntry();
			Entry->Serialize(Archive, this, EntryIndex);

			CachedData.Add(Key, Entry);
		}
	}
	else
	{
		int32 EntryIndex = 0;
		for (auto& CacheEntry : CachedData)
		{
			Archive << CacheEntry.Key;
			CacheEntry.Value->Serialize(Archive, this, EntryIndex++);
		}
	}
}

void UPCGLandscapeCache::Tick(float DeltaSeconds)
{
#if WITH_EDITOR
	if (!bInitialized && GetWorld())
	{
		SetupLandscapeCallbacks();
		CacheLayerNames();
		bInitialized = true;
	}
#else
	// Important implementation note:
	// If the threshold is too low, it could lead to some issues - namely the check(bDataLoaded) in the cache entries
	// as we currently do not have a state in the landscape cache to know whether something is still under use.
	// It is possible to do so however, by adding a scoped variable + a counter, so we could safely remove these from
	// the cleanup process done below, but at this point in time it doesn't seem to be needed or likely,
	// since we do not keep (wrt time-slicing) the landscape cache entry pointers at any point in the PCG landscape data implementation
	TimeSinceLastCleanupInSeconds += DeltaSeconds;

	if (TimeSinceLastCleanupInSeconds >= CVarLandscapeCacheGCFrequency.GetValueOnAnyThread())
	{
		TimeSinceLastCleanupInSeconds = 0;

		const int32 MemoryThresholdInBytes = CVarLandscapeCacheSizeThreshold.GetValueOnAnyThread() * 1024 * 1024;

		if (CacheMemorySize > MemoryThresholdInBytes)
		{
			TArray<FPCGLandscapeCacheEntry*> LoadedEntries;
			for (TPair<TPair<FGuid, FIntPoint>, FPCGLandscapeCacheEntry*>& CacheEntry : CachedData)
			{
				if (CacheEntry.Value->bDataLoaded)
				{
					LoadedEntries.Add(CacheEntry.Value);
				}
			}

			Algo::Sort(LoadedEntries, [](const FPCGLandscapeCacheEntry* A, const FPCGLandscapeCacheEntry* B)
			{
				return A->Touch < B->Touch;
			});

			// Go through the oldest data and free them until we're under the threshold
			int32 EntryIndex = 0;
			int32 HighestTouchUnloaded = 0;
			while(EntryIndex < LoadedEntries.Num() && CacheMemorySize > MemoryThresholdInBytes)
			{
				CacheMemorySize -= LoadedEntries[EntryIndex]->GetMemorySize();
				HighestTouchUnloaded = LoadedEntries[EntryIndex]->Touch;
				LoadedEntries[EntryIndex]->Unload();
				check(LoadedEntries[EntryIndex]->GetMemorySize()==0);
				++EntryIndex;
			}

			// Adjust touch on the other loaded entries so we can keep Touch bounded
			while (EntryIndex < LoadedEntries.Num())
			{
				LoadedEntries[EntryIndex++]->Touch -= HighestTouchUnloaded;
			}

			CacheTouch -= HighestTouchUnloaded;
		}
	}
#endif // !WITH_EDITOR
}

void UPCGLandscapeCache::PrimeCache()
{
#if WITH_EDITOR
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	for (auto It = ULandscapeInfoMap::GetLandscapeInfoMap(World).Map.CreateIterator(); It; ++It)
	{
		ULandscapeInfo* LandscapeInfo = It.Value();
		if (IsValid(LandscapeInfo))
		{
			// Build per-component information
			LandscapeInfo->ForAllLandscapeProxies([this, LandscapeInfo](const ALandscapeProxy* LandscapeProxy)
			{
				check(LandscapeProxy);
				const FGuid LandscapeGuid = LandscapeProxy->GetLandscapeGuid();

				for (ULandscapeComponent* LandscapeComponent : LandscapeProxy->LandscapeComponents)
				{
					if (!LandscapeComponent)
					{
						continue;
					}

					FIntPoint Coordinate(LandscapeComponent->SectionBaseX / LandscapeComponent->ComponentSizeQuads, LandscapeComponent->SectionBaseY / LandscapeComponent->ComponentSizeQuads);
					TPair<FGuid, FIntPoint> ComponentKey(LandscapeGuid, Coordinate);

					if (!CachedData.Contains(ComponentKey))
					{
						FPCGLandscapeCacheEntry* NewEntry = new FPCGLandscapeCacheEntry();
						NewEntry->BuildCacheData(LandscapeInfo, LandscapeComponent);
						CachedData.Add(ComponentKey, NewEntry);
					}
				}
			});
		}
	}

	CacheLayerNames();
#endif
}

void UPCGLandscapeCache::ClearCache()
{
	for (TPair<TPair<FGuid, FIntPoint>, FPCGLandscapeCacheEntry*>& CacheEntry : CachedData)
	{
		delete CacheEntry.Value;
		CacheEntry.Value = nullptr;
	}

	CachedData.Reset();
}

const FPCGLandscapeCacheEntry* UPCGLandscapeCache::GetCacheEntry(ULandscapeComponent* LandscapeComponent, const FIntPoint& ComponentCoordinate)
{
	const FPCGLandscapeCacheEntry* CacheEntry = nullptr;
	TPair<FGuid, FIntPoint> ComponentKey(LandscapeComponent && LandscapeComponent->GetLandscapeProxy() ? LandscapeComponent->GetLandscapeProxy()->GetLandscapeGuid() : FGuid(), ComponentCoordinate);

	{
#if WITH_EDITOR
		FReadScopeLock ScopeLock(CacheLock);
#endif
		if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
		{
			CacheEntry = *FoundEntry;
		}
	}

#if WITH_EDITOR
	if (!CacheEntry && LandscapeComponent && LandscapeComponent->GetLandscapeInfo())
	{
		FWriteScopeLock ScopeLock(CacheLock);
		if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
		{
			CacheEntry = *FoundEntry;
		}
		else
		{
			check(LandscapeComponent->SectionBaseX / LandscapeComponent->ComponentSizeQuads == ComponentKey.Value.X && LandscapeComponent->SectionBaseY / LandscapeComponent->ComponentSizeQuads == ComponentKey.Value.Y);
			FPCGLandscapeCacheEntry* NewEntry = new FPCGLandscapeCacheEntry();
			NewEntry->BuildCacheData(LandscapeComponent->GetLandscapeInfo(), LandscapeComponent);

			CacheEntry = NewEntry;
			CachedData.Add(ComponentKey, NewEntry);
		}
	}
#endif

	if (CacheEntry)
	{
		if (CacheEntry->TouchAndLoad(CacheTouch++))
		{
			CacheMemorySize += CacheEntry->GetMemorySize();
		}
	}

	return CacheEntry;
}

TArray<FName> UPCGLandscapeCache::GetLayerNames(ALandscapeProxy* Landscape)
{
#if WITH_EDITOR
	FReadScopeLock ScopeLock(CacheLock);
#endif
	return CachedLayerNames.Array();
}

#if WITH_EDITOR
void UPCGLandscapeCache::SetupLandscapeCallbacks()
{
	// Remove previous callbacks, if any
	TeardownLandscapeCallbacks();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Gather landspace actors
	TArray<AActor*> FoundLandscapes;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeProxy::StaticClass(), FoundLandscapes);

	for (AActor* FoundLandscape : FoundLandscapes)
	{
		ALandscapeProxy* Landscape = CastChecked<ALandscapeProxy>(FoundLandscape);

		Landscapes.Add(Landscape);
		Landscape->OnComponentDataChanged.AddUObject(this, &UPCGLandscapeCache::OnLandscapeChanged);
	}
}

void UPCGLandscapeCache::TeardownLandscapeCallbacks()
{
	for (TWeakObjectPtr<ALandscapeProxy> LandscapeWeakPtr : Landscapes)
	{
		if (ALandscapeProxy* Landscape = LandscapeWeakPtr.Get())
		{
			Landscape->OnComponentDataChanged.RemoveAll(this);
		}
	}
}

void UPCGLandscapeCache::OnLandscapeChanged(ALandscapeProxy* Landscape, const FLandscapeProxyComponentDataChangedParams& ChangeParams)
{
	if (!Landscapes.Contains(Landscape))
	{
		return;
	}

	CacheLock.WriteLock();

	// Just remove these from the cache, they'll be added back on demand
	ChangeParams.ForEachComponent([this, Landscape](const ULandscapeComponent* LandscapeComponent)
	{
		if (LandscapeComponent)
		{
			FIntPoint Coordinate(LandscapeComponent->SectionBaseX / LandscapeComponent->ComponentSizeQuads, LandscapeComponent->SectionBaseY / LandscapeComponent->ComponentSizeQuads);
			TPair<FGuid, FIntPoint> ComponentKey(Landscape->GetLandscapeGuid(), Coordinate);

			if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
			{
				delete *FoundEntry;
				CachedData.Remove(ComponentKey);
			}
		}
	});

	CacheLayerNames(Landscape);

	CacheLock.WriteUnlock();
}

void UPCGLandscapeCache::CacheLayerNames()
{
	CachedLayerNames.Reset();

	for (TWeakObjectPtr<ALandscapeProxy> Landscape : Landscapes)
	{
		if (Landscape.Get())
		{
			CacheLayerNames(Landscape.Get());
		}
	}
}

void UPCGLandscapeCache::CacheLayerNames(ALandscapeProxy* Landscape)
{
	check(Landscape);

	if (ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo())
	{
		for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
		{
			ULandscapeLayerInfoObject* LayerInfo = Layer.LayerInfoObj;
			if (!LayerInfo)
			{
				continue;
			}

			CachedLayerNames.Add(Layer.LayerName);
		}
	}
}

#endif // WITH_EDITOR