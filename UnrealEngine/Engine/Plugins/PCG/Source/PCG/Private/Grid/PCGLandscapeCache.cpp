// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGLandscapeCache.h"

#include "PCGCustomVersion.h"
#include "PCGPoint.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeInfoMap.h"
#include "LandscapeProxy.h"

#include "Async/ParallelFor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/BufferWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGLandscapeCache)

static TAutoConsoleVariable<float> CVarLandscapeCacheGCFrequency(
	TEXT("pcg.LandscapeCacheGCFrequency"),
	10,
	TEXT("Rate at which to release landscape cache data, in seconds"));

static TAutoConsoleVariable<int32> CVarLandscapeCacheSizeThreshold(
	TEXT("pcg.LandscapeCacheSizeThreshold"),
	64,
	TEXT("Memory Threhold at which we start cleaning up the landscape cache"));

namespace PCGLandscapeCache
{
	FSafeIndices CalcSafeIndices(FVector2D LocalPoint, int32 Stride)
	{
		check(Stride != 0);

		const FVector2D ClampedLocalPoint = LocalPoint.ClampAxes(0.0, FVector2D::FReal(Stride-1));

		FSafeIndices Result;
		const int32 CellX0 = FMath::FloorToInt(ClampedLocalPoint.X);
		const int32 CellY0 = FMath::FloorToInt(ClampedLocalPoint.Y);
		const int32 CellX1 = FMath::Min(CellX0+1, Stride-1);
		const int32 CellY1 = FMath::Min(CellY0+1, Stride-1);

		Result.X0Y0 = CellX0 + CellY0 * Stride;
		Result.X1Y0 = CellX1 + CellY0 * Stride;
		Result.X0Y1 = CellX0 + CellY1 * Stride;
		Result.X1Y1 = CellX1 + CellY1 * Stride;

		Result.XFraction = FMath::Fractional(ClampedLocalPoint.X);
		Result.YFraction = FMath::Fractional(ClampedLocalPoint.Y);

		return Result;
	}

	FIntPoint GetCoordinates(const ULandscapeComponent* LandscapeComponent)
	{
		check(LandscapeComponent && LandscapeComponent->ComponentSizeQuads != 0);
		return FIntPoint(LandscapeComponent->SectionBaseX / LandscapeComponent->ComponentSizeQuads, LandscapeComponent->SectionBaseY / LandscapeComponent->ComponentSizeQuads);
	}
}

#if WITH_EDITOR
FPCGLandscapeCacheEntry* FPCGLandscapeCacheEntry::CreateCacheEntry(ULandscapeInfo* LandscapeInfo, ULandscapeComponent* InComponent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLandscapeCacheEntry::CreateCacheEntry);

	FLandscapeComponentDataInterface CDI(InComponent, 0, /*bWorkInEditingLayer=*/false);

	// CDI.GetRawHeightData() may be nullptr if InComponent has no texture data.
	if (CDI.GetRawHeightData() == nullptr)
	{
		return nullptr;
	}

	// The component has an extra vertex on the edge, for interpolation purposes
	const int32 Stride = InComponent->ComponentSizeQuads + 1;

	if (Stride <= 0)
	{
		return nullptr;
	}

	FPCGLandscapeCacheEntry *Result = new FPCGLandscapeCacheEntry();

	Result->PointHalfSize = InComponent->GetComponentTransform().GetScale3D() * 0.5;
	Result->Stride = Stride;

	// Get landscape default layer (heightmap/tangents/normal)
	{
		FVector WorldPos;
		FVector WorldTangentX;
		FVector WorldTangentY;
		FVector WorldTangentZ;

		const int32 NumVertices = FMath::Square(Stride);

		Result->PositionsAndNormals.Reserve(2 * NumVertices);

		for (int32 Index = 0; Index < NumVertices; ++Index)
		{
			CDI.GetWorldPositionTangents(Index, WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ);
			Result->PositionsAndNormals.Add(WorldPos);
			Result->PositionsAndNormals.Add(WorldTangentZ);
		}
	}

	// Get other layers, push data into metadata attributes
	{
		TArray<uint8> LayerCache;
		for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
		{
			ULandscapeLayerInfoObject* LayerInfo = Layer.LayerInfoObj;
			if (!LayerInfo)
			{
				continue;
			}

			if (CDI.GetWeightmapTextureData(LayerInfo, LayerCache, /*bUseEditingLayer=*/false, /*bRemoveSubsectionDuplicates=*/true))
			{
				Result->LayerDataNames.Add(Layer.LayerName);
				Result->LayerData.Emplace(std::move(LayerCache));
			}

			LayerCache.Reset();
		}
	}
	
	Result->bDataLoaded = true;

	return Result;
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

	const float Density = 1;
	const int32 Seed = 1 + PointIndex;

	new(&OutPoint) FPCGPoint(FTransform(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), Normal.GetSafeNormal(), Position), Density, Seed);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
	
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
	
	const float Density = 1;
	const int32 Seed = 1 + PointIndex;

	new(&OutPoint) FPCGPoint(FTransform(Position), Density, Seed);
	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
}

void FPCGLandscapeCacheEntry::GetInterpolatedLayerWeights(const FVector2D& LocalPoint, TArray<FPCGLandscapeLayerWeight>& OutLayerWeights) const
{
	OutLayerWeights.SetNum(LayerData.Num());
	
	if (!bDataLoaded)
	{
		return;
	}

	const PCGLandscapeCache::FSafeIndices Indices = PCGLandscapeCache::CalcSafeIndices(LocalPoint, Stride);

	if (!LayerData.IsEmpty())
	{
		check(LayerData.Num() == LayerDataNames.Num());

		for(int32 LayerIndex = 0; LayerIndex < LayerData.Num(); ++LayerIndex)
		{
			FPCGLandscapeLayerWeight& OutLayer = OutLayerWeights[LayerIndex];
			OutLayer.Name = LayerDataNames[LayerIndex];

			const TArray<uint8>& CurrentLayerData = LayerData[LayerIndex];

			const float Y0Data = FMath::Lerp((float)CurrentLayerData[Indices.X0Y0] / 255.0f, (float)CurrentLayerData[Indices.X1Y0] / 255.0f, Indices.XFraction);
			const float Y1Data = FMath::Lerp((float)CurrentLayerData[Indices.X0Y1] / 255.0f, (float)CurrentLayerData[Indices.X1Y1] / 255.0f, Indices.XFraction);

			OutLayer.Weight = FMath::Lerp(Y0Data, Y1Data, Indices.YFraction);
		}
	}
}

void FPCGLandscapeCacheEntry::GetInterpolatedPointInternal(const PCGLandscapeCache::FSafeIndices& Indices, FPCGPoint& OutPoint, bool bHeightOnly) const
{
	check(bDataLoaded);
	check(2 * Indices.X1Y1 < PositionsAndNormals.Num());

	const FVector& PositionX0Y0 = PositionsAndNormals[2 * Indices.X0Y0];
	const FVector& PositionX1Y0 = PositionsAndNormals[2 * Indices.X1Y0];
	const FVector& PositionX0Y1 = PositionsAndNormals[2 * Indices.X0Y1];
	const FVector& PositionX1Y1 = PositionsAndNormals[2 * Indices.X1Y1];

	const FVector LerpPositionY0 = FMath::Lerp(PositionX0Y0, PositionX1Y0, Indices.XFraction);
	const FVector LerpPositionY1 = FMath::Lerp(PositionX0Y1, PositionX1Y1, Indices.XFraction);
	const FVector Position = FMath::Lerp(LerpPositionY0, LerpPositionY1, Indices.YFraction);

	const int32 Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Position);
	const float Density = 1;

	if (bHeightOnly)
	{
		new(&OutPoint) FPCGPoint(FTransform(Position), Density, Seed);
	}
	else
	{
		const FVector& NormalX0Y0 = PositionsAndNormals[2 * Indices.X0Y0 + 1];
		const FVector& NormalX1Y0 = PositionsAndNormals[2 * Indices.X1Y0 + 1];
		const FVector& NormalX0Y1 = PositionsAndNormals[2 * Indices.X0Y1 + 1];
		const FVector& NormalX1Y1 = PositionsAndNormals[2 * Indices.X1Y1 + 1];

		const FVector LerpNormalY0 = FMath::Lerp(NormalX0Y0.GetSafeNormal(), NormalX1Y0.GetSafeNormal(), Indices.XFraction).GetSafeNormal();
		const FVector LerpNormalY1 = FMath::Lerp(NormalX0Y1.GetSafeNormal(), NormalX1Y1.GetSafeNormal(), Indices.XFraction).GetSafeNormal();
		const FVector Normal = FMath::Lerp(LerpNormalY0, LerpNormalY1, Indices.YFraction);

		FVector TangentX;
		FVector TangentY;
		TangentX = FVector(Normal.Z, 0.f, -Normal.X);
		TangentY = Normal ^ TangentX;

		new(&OutPoint) FPCGPoint(FTransform(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), Normal.GetSafeNormal(), Position), Density, Seed);
	}

	OutPoint.BoundsMin = -PointHalfSize;
	OutPoint.BoundsMax = PointHalfSize;
}

void FPCGLandscapeCacheEntry::GetInterpolatedPointMetadataInternal(const PCGLandscapeCache::FSafeIndices& Indices, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	check(bDataLoaded);
	check(2 * Indices.X1Y1 < PositionsAndNormals.Num());

	if (OutMetadata && !LayerData.IsEmpty())
	{
		OutPoint.MetadataEntry = OutMetadata->AddEntry();

		check(LayerData.Num() == LayerDataNames.Num());

		for (int32 LayerIndex = 0; LayerIndex < LayerData.Num(); ++LayerIndex)
		{
			const FName& CurrentLayerName = LayerDataNames[LayerIndex];
			const TArray<uint8>& CurrentLayerData = LayerData[LayerIndex];

			if (FPCGMetadataAttributeBase* Attribute = OutMetadata->GetMutableAttribute(CurrentLayerName))
			{
				check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<float>::Id);
				const float Y0Data = FMath::Lerp((float)CurrentLayerData[Indices.X0Y0] / 255.0f, (float)CurrentLayerData[Indices.X1Y0] / 255.0f, Indices.XFraction);
				const float Y1Data = FMath::Lerp((float)CurrentLayerData[Indices.X0Y1] / 255.0f, (float)CurrentLayerData[Indices.X1Y1] / 255.0f, Indices.XFraction);
				const float Data = FMath::Lerp(Y0Data, Y1Data, Indices.YFraction);

				static_cast<FPCGMetadataAttribute<float>*>(Attribute)->SetValue(OutPoint.MetadataEntry, Data);
			}
		}
	}
}

void FPCGLandscapeCacheEntry::GetInterpolatedPoint(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	check (bDataLoaded);
	const PCGLandscapeCache::FSafeIndices Indices = PCGLandscapeCache::CalcSafeIndices(LocalPoint, Stride);
	GetInterpolatedPointInternal(Indices, OutPoint);
	GetInterpolatedPointMetadataInternal(Indices, OutPoint, OutMetadata);
}

void FPCGLandscapeCacheEntry::GetInterpolatedPointMetadataOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	check(bDataLoaded);
	const PCGLandscapeCache::FSafeIndices Indices = PCGLandscapeCache::CalcSafeIndices(LocalPoint, Stride);
	GetInterpolatedPointMetadataInternal(Indices, OutPoint, OutMetadata);
}

void FPCGLandscapeCacheEntry::GetInterpolatedPointHeightOnly(const FVector2D& LocalPoint, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const
{
	check(bDataLoaded);
	const PCGLandscapeCache::FSafeIndices Indices = PCGLandscapeCache::CalcSafeIndices(LocalPoint, Stride);
	GetInterpolatedPointInternal(Indices, OutPoint, /*bHeightOnly=*/true);
	GetInterpolatedPointMetadataInternal(Indices, OutPoint, OutMetadata);
}

bool FPCGLandscapeCacheEntry::TouchAndLoad(int32 InTouch) const
{
	Touch = InTouch; // technically, this could be an atomic, but we don't need this to be very precise

	if (!bDataLoaded)
	{
		FScopeLock ScopeDataLock(&DataLock);
		if (!bDataLoaded)
		{
			return SerializeFromBulkData();
		}
	}

	return false;
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

void FPCGLandscapeCacheEntry::SerializeToBulkData(EPCGLandscapeCacheSerializationContents SerializationContents)
{
	const bool bSerializeEverything = SerializationContents == EPCGLandscapeCacheSerializationContents::SerializeAll;
	const bool bShouldSerializePositionAndNormals = (bSerializeEverything || SerializationContents == EPCGLandscapeCacheSerializationContents::SerializeOnlyPositionsAndNormals);
	const bool bShouldSerializeLayerData = (bSerializeEverything || SerializationContents == EPCGLandscapeCacheSerializationContents::SerializeOnlyLayerData);

	// Use empty buffers if the contents should not be serialized
	TArray<FVector> PositionsAndNormalsTemp;
	TArray<TArray<uint8>> LayerDataTemp;
	TArray<FVector>& SerializePositionsAndNormals = bShouldSerializePositionAndNormals ? PositionsAndNormals : PositionsAndNormalsTemp;
	TArray<TArray<uint8>>& SerializeLayerData = bShouldSerializeLayerData ? LayerData : LayerDataTemp;


	// Move data from local arrays to the bulk data
	BulkData.Lock(LOCK_READ_WRITE);

	int32 NumBytes = 0;
	// Number of entries in the array + size of the array
	NumBytes += sizeof(int32) + SerializePositionsAndNormals.Num() * SerializePositionsAndNormals.GetTypeSize();

	// Number of layers
	NumBytes += sizeof(int32); 
	for (TArray<uint8>& CurrentLayerData : SerializeLayerData)
	{
		// Number of entries in the layer data array + size of the array
		NumBytes += sizeof(int32) + CurrentLayerData.Num() * CurrentLayerData.GetTypeSize();
	}

	uint8* Dest = (uint8*)BulkData.Realloc(NumBytes);
	FBufferWriter Ar(Dest, NumBytes);
	Ar.SetIsPersistent(true);

	Ar << SerializePositionsAndNormals;
	
	int32 LayerDataCount = SerializeLayerData.Num();
	Ar << LayerDataCount;

	for (TArray<uint8>& CurrentLayerData : SerializeLayerData)
	{
		Ar << CurrentLayerData;
	}

	BulkData.Unlock();
}

bool FPCGLandscapeCacheEntry::SerializeFromBulkData() const
{
	check(!bDataLoaded);

	// If owner object has been unloaded, we can't serialize the bulk data.
	if (!OwningCache.IsValid())
	{
		return false;
	}

	LLM_SCOPE_BYNAME(TEXT("PCGLandscape"));

	// Note: this call is not threadsafe by itself, it is meant to be called from a locked region
	uint8* Data = nullptr;
	BulkData.GetCopy((void**)&Data);
	int32 DataSize = BulkData.GetBulkDataSize();

	if (!Data)
	{
		UE_LOG(LogPCG, Error, TEXT("Unable to load Landscape Cache Entry bulk data"));
		return false;
	}

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
	return true;
}

void FPCGLandscapeCacheEntry::Serialize(FArchive& Archive, UObject* Owner, int32 Index, EPCGLandscapeCacheSerializationContents SerializeContents)
{
	// If the current bulk output data does not match the desired save data, make sure to load from the bulk data and update the bulked save data
	if(Archive.IsCooking() && SerializeContents != EPCGLandscapeCacheSerializationContents::SerializeAll && !bDataLoaded)
	{
		SerializeFromBulkData();
	}
	
	// Important implementation note:
	// If the serialization here or in the cache entries change, we still need to load data from previous versions.
	// While that's not really needed in non-editor builds, it is very much important when loading from the editor,
	// at least in the "AlwaysSerialize" case.
	if (bDataLoaded && Archive.IsSaving())
	{
		SerializeToBulkData(SerializeContents);
	}
	
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

	const bool bShouldSerializeEntries = (Archive.IsSaving() &&
		(SerializationMode == EPCGLandscapeCacheSerializationMode::AlwaysSerialize || 
			(SerializationMode == EPCGLandscapeCacheSerializationMode::SerializeOnlyAtCook && Archive.IsCooking())));
	EPCGLandscapeCacheSerializationContents SerializedContents = Archive.IsCooking() ? CookedSerializedContents : EPCGLandscapeCacheSerializationContents::SerializeAll;
	// Important implementation note:
	// If the serialization here or in the cache entries change, we still need to load data from previous versions.
	// While that's not really needed in non-editor builds, it is very much important when loading from the editor,
	// at least in the "AlwaysSerialize" case.
	Archive.UsingCustomVersion(FPCGCustomVersion::GUID);

	int32 DataVersion = FPCGCustomVersion::LatestVersion;
	if (Archive.IsLoading())
	{
		DataVersion = Archive.CustomVer(FPCGCustomVersion::GUID);
	}

	// Serialize cache entries
	int32 NumEntries = ((Archive.IsLoading() || !bShouldSerializeEntries) ? 0 : CachedData.Num());
	Archive << NumEntries;

	if (Archive.IsLoading())
	{
		CachedData.Reserve(NumEntries);

		for (int32 EntryIndex = 0; EntryIndex < NumEntries; ++EntryIndex)
		{
			TPair<FGuid, FIntPoint> Key;
			Archive << Key;

			FPCGLandscapeCacheEntry* Entry = new FPCGLandscapeCacheEntry();
			Entry->OwningCache = this;
			Entry->Serialize(Archive, this, EntryIndex, SerializedContents);

			CacheMapKey MapKey(Key.Key, Key.Value, nullptr);
			CachedData.Add(MapKey, Entry);
		}

#if WITH_EDITOR
		CacheEntryCount = CachedData.Num();
#endif
	}
	else if(bShouldSerializeEntries)
	{
		int32 EntryIndex = 0;
		for (auto& CacheEntry : CachedData)
		{
			TPair<FGuid, FIntPoint> Key(CacheEntry.Key.LandscapeGuid, CacheEntry.Key.Coordinate);
			Archive << Key;
			CacheEntry.Value->Serialize(Archive, this, EntryIndex++, SerializedContents);
		}
	}
}

void UPCGLandscapeCache::Initialize()
{
	if (!bInitialized && GetWorld())
	{
#if WITH_EDITOR
		SetupLandscapeCallbacks();
		CacheLayerNames();
#endif
		UpdateCacheWorldKeys();
		bInitialized = true;
	}

}

void UPCGLandscapeCache::UpdateCacheWorldKeys()
{
	check(IsInGameThread());

	AActor* HintActor = Cast<AActor>(GetOuter());
	if (!HintActor)
	{
		return;
	}

	TMap<CacheMapKey, FPCGLandscapeCacheEntry*> UpdatedCachedData;
	UpdatedCachedData.Reserve(CachedData.Num());

	for (const TPair<CacheMapKey, FPCGLandscapeCacheEntry*>& CacheEntry : CachedData)
	{
		CacheMapKey Key = CacheEntry.Key;
		if (Key.WorldKey == FObjectKey())
		{
			Key = CacheMapKey(Key.LandscapeGuid, Key.Coordinate, HintActor);
		}

		UpdatedCachedData.Add(Key, CacheEntry.Value);
	}

	CachedData = MoveTemp(UpdatedCachedData);
}

void UPCGLandscapeCache::Tick(float DeltaSeconds)
{
	Initialize();

#if !WITH_EDITOR
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
			for(TPair<CacheMapKey, FPCGLandscapeCacheEntry*>& CacheEntry : CachedData)
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

	// First, gather all potential cached data to create, emplace a nullptr at these locations
	TArray<TTuple<ULandscapeInfo*, ULandscapeComponent*, CacheMapKey>> CacheEntriesToBuild;

	for (auto It = ULandscapeInfoMap::GetLandscapeInfoMap(World).Map.CreateIterator(); It; ++It)
	{
		ULandscapeInfo* LandscapeInfo = It.Value();
		if (IsValid(LandscapeInfo))
		{
			// Build per-component information
			LandscapeInfo->ForEachLandscapeProxy([this, LandscapeInfo, &CacheEntriesToBuild](ALandscapeProxy* LandscapeProxy)
			{
				check(LandscapeProxy);
				Landscapes.Add(LandscapeProxy);
				const FGuid LandscapeGuid = LandscapeProxy->GetOriginalLandscapeGuid();

				for (ULandscapeComponent* LandscapeComponent : LandscapeProxy->LandscapeComponents)
				{
					if (!LandscapeComponent)
					{
						continue;
					}

					CacheMapKey ComponentKey(LandscapeGuid, PCGLandscapeCache::GetCoordinates(LandscapeComponent), Cast<AActor>(GetOuter()));
					if (!CachedData.Contains(ComponentKey))
					{
						CacheEntriesToBuild.Emplace(LandscapeInfo, LandscapeComponent, ComponentKey);
					}
				}
				return true;
			});
		}
	}

	// Then create the landscape cache entries
	TArray<FPCGLandscapeCacheEntry*> NewEntries;
	NewEntries.SetNum(CacheEntriesToBuild.Num());

	ParallelFor(CacheEntriesToBuild.Num(), [this, &CacheEntriesToBuild, &NewEntries](int32 EntryIndex)
	{
		const TTuple<ULandscapeInfo*, ULandscapeComponent*, CacheMapKey>& CacheEntryInfo = CacheEntriesToBuild[EntryIndex];
		NewEntries[EntryIndex] = FPCGLandscapeCacheEntry::CreateCacheEntry(CacheEntryInfo.Get<0>(), CacheEntryInfo.Get<1>());
		NewEntries[EntryIndex]->OwningCache = this;
	});

	if (SerializationMode != EPCGLandscapeCacheSerializationMode::NeverSerialize)
	{
		Modify();
	}

	// Finally, write them back to the cache
	for (int32 EntryIndex = 0; EntryIndex < CacheEntriesToBuild.Num(); ++EntryIndex)
	{
		if (NewEntries[EntryIndex])
		{
			CachedData.Add(CacheEntriesToBuild[EntryIndex].Get<2>(), NewEntries[EntryIndex]);
		}
	}

	CacheEntryCount = CachedData.Num();
	CacheLayerNames();
#endif
}

void UPCGLandscapeCache::ClearCache()
{
	if (SerializationMode != EPCGLandscapeCacheSerializationMode::NeverSerialize)
	{
		Modify();
	}

	for(TPair<CacheMapKey, FPCGLandscapeCacheEntry*>& CacheEntry : CachedData)
	{
		delete CacheEntry.Value;
		CacheEntry.Value = nullptr;
	}

	CachedData.Reset();
	CachedLayerNames.Reset();
#if WITH_EDITOR
	CacheEntryCount = 0;
#endif
}

void UPCGLandscapeCache::TakeOwnership(UPCGLandscapeCache* InLandscapeCache)
{
	if (SerializationMode != EPCGLandscapeCacheSerializationMode::NeverSerialize)
	{
		Modify(/*bAlwaysMarkDirty=*/false);
	}

	InLandscapeCache->UpdateCacheWorldKeys();
	bool bShouldDirty = false;

	for(TPair<CacheMapKey, FPCGLandscapeCacheEntry*>& CacheEntryPair : InLandscapeCache->CachedData)
	{
		FPCGLandscapeCacheEntry* CacheEntry = CachedData.FindOrAdd(CacheEntryPair.Key, CacheEntryPair.Value);
		if (CacheEntry == CacheEntryPair.Value)
		{
			CacheEntryPair.Value = nullptr;
			bShouldDirty = true;
		}
	}

	for (FName LayerName : InLandscapeCache->CachedLayerNames)
	{
		bool bLayerWasAlreadyPresent = false;
		CachedLayerNames.Add(LayerName, &bLayerWasAlreadyPresent);
		bShouldDirty |= !bLayerWasAlreadyPresent;
	}

	InLandscapeCache->ClearCache();

#if WITH_EDITOR
	CacheEntryCount = CachedData.Num();
#endif

	if (bShouldDirty && SerializationMode != EPCGLandscapeCacheSerializationMode::NeverSerialize)
	{
		MarkPackageDirty();
	}
}

#if WITH_EDITOR
const FPCGLandscapeCacheEntry* UPCGLandscapeCache::GetCacheEntry(ULandscapeComponent* LandscapeComponent, const FIntPoint& ComponentCoordinate)
{
	if (!LandscapeComponent)
	{
		return nullptr;
	}

	const FGuid LandscapeGuid = (LandscapeComponent->GetLandscapeProxy() ? LandscapeComponent->GetLandscapeProxy()->GetOriginalLandscapeGuid() : FGuid());
	const FPCGLandscapeCacheEntry* CacheEntry = GetCacheEntry(LandscapeComponent->GetLandscapeProxy(), LandscapeGuid, ComponentCoordinate);

	if (!CacheEntry && LandscapeComponent && LandscapeComponent->GetLandscapeInfo())
	{
		FWriteScopeLock ScopeLock(CacheLock);
		CacheMapKey ComponentKey(LandscapeGuid, ComponentCoordinate, LandscapeComponent->GetOwner());
		if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
		{
			CacheEntry = *FoundEntry;
		}
		else
		{
			check(LandscapeComponent->SectionBaseX / LandscapeComponent->ComponentSizeQuads == ComponentKey.Coordinate.X && LandscapeComponent->SectionBaseY / LandscapeComponent->ComponentSizeQuads == ComponentKey.Coordinate.Y);
			if (FPCGLandscapeCacheEntry* NewEntry = FPCGLandscapeCacheEntry::CreateCacheEntry(LandscapeComponent->GetLandscapeInfo(), LandscapeComponent))
			{
				NewEntry->OwningCache = this;
				CacheEntry = NewEntry;
				CachedData.Add(ComponentKey, NewEntry);
				++CacheEntryCount;
			}
		}

		if (CacheEntry)
		{
			if (CacheEntry->TouchAndLoad(CacheTouch++))
			{
				CacheMemorySize += CacheEntry->GetMemorySize();
			}
			else if(!CacheEntry->bDataLoaded)
			{
				CacheEntry = nullptr;
			}
		}
	}

	return CacheEntry;
}
#endif

const FPCGLandscapeCacheEntry* UPCGLandscapeCache::GetCacheEntry(AActor* HintActor, const FGuid& LandscapeGuid, const FIntPoint& ComponentCoordinate)
{
	const FPCGLandscapeCacheEntry* CacheEntry = nullptr;
	CacheMapKey ComponentKey(LandscapeGuid, ComponentCoordinate, HintActor);

	{
#if WITH_EDITOR
		FReadScopeLock ScopeLock(CacheLock);
#endif
		if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
		{
			CacheEntry = *FoundEntry;
		}
	}

	if (CacheEntry)
	{
		if (CacheEntry->TouchAndLoad(CacheTouch++))
		{
			CacheMemorySize += CacheEntry->GetMemorySize();
		}
		else if(!CacheEntry->bDataLoaded)
		{
			CacheEntry = nullptr;
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

void UPCGLandscapeCache::SampleMetadataOnPoint(ALandscapeProxy* Landscape, FPCGPoint& InOutPoint, UPCGMetadata* OutMetadata)
{
	if (!Landscape || !Landscape->GetWorld() || !OutMetadata || CachedLayerNames.IsEmpty())
	{
		return;
	}

	ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
	if (!LandscapeInfo)
	{
		UE_LOG(LogPCG, Warning, TEXT("Unable to get landscape layer weights because the landscape info is not available (landscape not registered yet?"));
		return;
	}

	const FTransform LandscapeTransform = Landscape->LandscapeActorToWorld();
	const FVector LocalPoint = LandscapeTransform.InverseTransformPosition(InOutPoint.Transform.GetLocation());
	const FIntPoint ComponentMapKey(FMath::FloorToInt(LocalPoint.X / LandscapeInfo->ComponentSizeQuads), FMath::FloorToInt(LocalPoint.Y / LandscapeInfo->ComponentSizeQuads));

#if WITH_EDITOR
	ULandscapeComponent* LandscapeComponent = LandscapeInfo->XYtoComponentMap.FindRef(ComponentMapKey);
	const FPCGLandscapeCacheEntry* CacheEntry = GetCacheEntry(LandscapeComponent, ComponentMapKey);
#else
	const FPCGLandscapeCacheEntry* CacheEntry = GetCacheEntry(Landscape, Landscape->GetOriginalLandscapeGuid(), ComponentMapKey);
#endif

	if (!CacheEntry || CacheEntry->LayerData.IsEmpty())
	{
		return;
	}

	const FVector2D ComponentLocalPoint(LocalPoint.X - ComponentMapKey.X * LandscapeInfo->ComponentSizeQuads, LocalPoint.Y - ComponentMapKey.Y * LandscapeInfo->ComponentSizeQuads);
	CacheEntry->GetInterpolatedPointMetadataOnly(ComponentLocalPoint, InOutPoint, OutMetadata);
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

	// Also track when the landscape is moved, added, or deleted
	if (GEngine)
	{
		GEngine->OnActorMoved().AddUObject(this, &UPCGLandscapeCache::OnLandscapeMoved);
		GEngine->OnLevelActorAdded().AddUObject(this, &UPCGLandscapeCache::OnLandscapeAdded);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UPCGLandscapeCache::OnLandscapeDeleted);
	}

	// In editor, loading from the persistent level should add it to the cache. Note that we don't need to track unloaded landscapes
	if (!World->IsPlayInEditor() && World->IsPartitionedWorld() && World->PersistentLevel)
	{
		World->PersistentLevel->OnLoadedActorAddedToLevelEvent.AddUObject(this, &UPCGLandscapeCache::OnLandscapeLoaded);
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

	if (GEngine)
	{
		GEngine->OnActorMoved().RemoveAll(this);
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	if (UWorld* World = GetWorld())
	{
		if (!World->IsPlayInEditor() && World->IsPartitionedWorld() && World->PersistentLevel)
		{
			World->PersistentLevel->OnLoadedActorAddedToLevelEvent.RemoveAll(this);
		}
	}
}

void UPCGLandscapeCache::OnLandscapeChanged(ALandscapeProxy* InLandscape, const FLandscapeProxyComponentDataChangedParams& InChangeParams)
{
	if (!Landscapes.Contains(InLandscape))
	{
		return;
	}

	CacheLock.WriteLock();

	// Just remove these from the cache, they'll be added back on demand
	InChangeParams.ForEachComponent([this, InLandscape](const ULandscapeComponent* LandscapeComponent)
	{
		if (LandscapeComponent)
		{
			const CacheMapKey ComponentKey(InLandscape->GetOriginalLandscapeGuid(), PCGLandscapeCache::GetCoordinates(LandscapeComponent), InLandscape);
			FPCGLandscapeCacheEntry* EntryToDelete = nullptr;

			if (CachedData.RemoveAndCopyValue(ComponentKey, EntryToDelete))
			{
				--CacheEntryCount;
				delete EntryToDelete;
			}
		}
	});

	CacheLayerNames(InLandscape);

	CacheLock.WriteUnlock();
}

void UPCGLandscapeCache::OnLandscapeMoved(AActor* InActor)
{
	ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(InActor);
	if (!Landscape || !Landscapes.Contains(Landscape))
	{
		return;
	}

	CacheLock.WriteLock();

	// Just remove these from the cache, they'll be added back on demand
	RemoveComponentFromCache(Landscape);

	CacheLayerNames(Landscape);

	CacheLock.WriteUnlock();
}

void UPCGLandscapeCache::OnLandscapeDeleted(AActor* Actor)
{
	ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor);
	if (!LandscapeProxy || !Landscapes.Contains(LandscapeProxy))
	{
		return;
	}

	CacheLock.WriteLock();

	RemoveComponentFromCache(LandscapeProxy);

	Landscapes.Remove(LandscapeProxy);
	LandscapeProxy->OnComponentDataChanged.RemoveAll(this);

	CacheLock.WriteUnlock();
}

void UPCGLandscapeCache::OnLandscapeLoaded(AActor& Actor)
{
	OnLandscapeAdded(&Actor);
}

void UPCGLandscapeCache::OnLandscapeAdded(AActor* Actor)
{
	if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor))
	{
		Landscapes.Add(LandscapeProxy);
		if (!LandscapeProxy->OnComponentDataChanged.IsBoundToObject(this))
		{
			LandscapeProxy->OnComponentDataChanged.AddUObject(this, &UPCGLandscapeCache::OnLandscapeChanged);
		}

		// Note: Landscape Proxies have no components at this stage, so they will need to be added on demand
	}
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

void UPCGLandscapeCache::RemoveComponentFromCache(const ALandscapeProxy* LandscapeProxy)
{
	check(LandscapeProxy);

	LandscapeProxy->ForEachComponent<ULandscapeComponent>(/*bIncludeFromChildActors=*/false, [this, LandscapeProxy](const ULandscapeComponent* LandscapeComponent)
	{
		if (LandscapeComponent)
		{
			const CacheMapKey ComponentKey(LandscapeProxy->GetOriginalLandscapeGuid(), PCGLandscapeCache::GetCoordinates(LandscapeComponent), LandscapeProxy);
			if (FPCGLandscapeCacheEntry** FoundEntry = CachedData.Find(ComponentKey))
			{
				CachedData.Remove(ComponentKey);
				--CacheEntryCount;
				delete* FoundEntry;
			}
		}
	});
}

void UPCGLandscapeCache::CacheLayerNames(ALandscapeProxy* InLandscape)
{
	check(InLandscape);

	if (ULandscapeInfo* LandscapeInfo = InLandscape->GetLandscapeInfo())
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
