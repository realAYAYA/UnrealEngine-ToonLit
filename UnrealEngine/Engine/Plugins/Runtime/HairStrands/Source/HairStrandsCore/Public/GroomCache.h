// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GroomCacheData.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "Serialization/BulkData.h"
#include "GroomCache.generated.h"

struct FGroomCacheChunk;

/**
 * Implements an asset that is used to store an animated groom
 */
UCLASS(BlueprintType)
class HAIRSTRANDSCORE_API UGroomCache : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()

public:

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

	void Initialize(EGroomCacheType Type);
	int32 GetStartFrame() const;
	int32 GetEndFrame() const;
	float GetDuration() const;

	/** Get the frame number at the specified time within the animation range which might not start at 0 */
	int32 GetFrameNumberAtTime(const float Time, bool bLooping) const;

	/** Get the (floored) frame index at the specified time with the index 0 being the start of the animation */
	int32 GetFrameIndexAtTime(const float Time, bool bLooping) const;

	/** Get the frame indices and interpolation factor between them that correspond to the specified time */
	void GetFrameIndicesAtTime(float Time, bool bLooping, bool bIsPlayingBackwards, int32 &OutFrameIndex, int32 &OutNextFrameIndex, float &InterpolationFactor);

	/** Get the frame indices that correspond to the specified time range */
	void GetFrameIndicesForTimeRange(float StartTime, float EndTime, bool Looping, TArray<int32>& OutFrameIndices);

	bool GetGroomDataAtTime(float Time, bool bLooping, FGroomCacheAnimationData& AnimData);
	bool GetGroomDataAtFrameIndex(int32 FrameIndex, FGroomCacheAnimationData& AnimData);

	void SetGroomAnimationInfo(const FGroomAnimationInfo& AnimInfo);
	const FGroomAnimationInfo& GetGroomAnimationInfo() const { return GroomCacheInfo.AnimationInfo; }

	EGroomCacheType GetType() const;

	TArray<FGroomCacheChunk>& GetChunks() { return Chunks; }

	TOptional<FPackageFileVersion> ArchiveVersion;

#if WITH_EDITORONLY_DATA
	/** Import options used for this GroomCache */
	UPROPERTY(Category = ImportSettings, VisibleAnywhere, Instanced)
	TObjectPtr<class UAssetImportData> AssetImportData;	
#endif

protected:
	UPROPERTY(VisibleAnywhere, Category = GroomCache)
	FGroomCacheInfo GroomCacheInfo;

	TArray<FGroomCacheChunk> Chunks;

	friend class FGroomCacheProcessor;
};

/**
 * The smallest unit of streamed GroomCache data
 * The BulkData member is loaded on-demand so that loading the GroomCache itself is relatively lightweight
 */
struct FGroomCacheChunk
{
	/** Size of the chunk of data in bytes */
	int32 DataSize = 0;

	/** Frame index of the frame stored in this block */
	int32 FrameIndex = 0;

	/** Bulk data if stored in the package. */
	FByteBulkData BulkData;

	void Serialize(FArchive& Ar, UObject* Owner, int32 ChunkIndex);
};

/** Proxy that processes the HairGroupData into GroomCacheChunks that contain the groom animation data */
class HAIRSTRANDSCORE_API FGroomCacheProcessor
{
public:
	FGroomCacheProcessor(EGroomCacheType InType, EGroomCacheAttributes InAttributes);

	void AddGroomSample(TArray<FHairDescriptionGroup>&& GroomData);
	void TransferChunks(UGroomCache* GroomCache);
	EGroomCacheType GetType() const { return Type; }

private:
	TArray<FGroomCacheChunk> Chunks;
	EGroomCacheAttributes Attributes;
	EGroomCacheType Type;
};
