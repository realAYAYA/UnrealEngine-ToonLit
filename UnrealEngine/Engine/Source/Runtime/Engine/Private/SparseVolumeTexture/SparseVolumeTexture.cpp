// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SparseVolumeTexture.cpp: SparseVolumeTexture implementation.
=============================================================================*/

#include "SparseVolumeTexture/SparseVolumeTexture.h"

#include "Materials/Material.h"
#include "MaterialShared.h"
#include "UObject/UObjectIterator.h"
#include "Misc/SecureHash.h"
#include "EngineUtils.h"
#include "Shader/ShaderTypes.h"
#include "RenderingThread.h"
#include "GlobalRenderResources.h"
#include "UObject/Package.h"
#include "SparseVolumeTexture/SparseVolumeTextureData.h"
#include "SparseVolumeTexture/SparseVolumeTextureUtility.h"
#include "SparseVolumeTexture/ISparseVolumeTextureStreamingManager.h"

#if WITH_EDITORONLY_DATA
#include "Misc/ScopedSlowTask.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#endif
#include "EditorFramework/AssetImportData.h"
#include "Async/ParallelFor.h"

#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"
#include "Serialization/EditorBulkDataReader.h"
#include "Serialization/EditorBulkDataWriter.h"

#define LOCTEXT_NAMESPACE "USparseVolumeTexture"

DEFINE_LOG_CATEGORY_STATIC(LogSparseVolumeTexture, Log, All);

static int32 GSVTRemoteDDCBehavior = 0;
static FAutoConsoleVariableRef CVarSVTRemoteDDCBehavior(
	TEXT("r.SparseVolumeTexture.RemoteDDCBehavior"),
	GSVTRemoteDDCBehavior,
	TEXT("Controls how SVTs use remote DDC. 0: The bLocalDDCOnly property controls per-SVT caching behavior, 1: Force local DDC only usage for all SVTs, 2: Force local + remote DDC usage for all SVTs"),
	ECVF_Default
);

////////////////////////////////////////////////////////////////////////////////////////////////

FArchive& operator<<(FArchive& Ar, UE::SVT::FMipLevelStreamingInfo& MipLevelStreamingInfo)
{
	Ar << MipLevelStreamingInfo.BulkOffset;
	Ar << MipLevelStreamingInfo.BulkSize;
	Ar << MipLevelStreamingInfo.PageTableOffset;
	Ar << MipLevelStreamingInfo.PageTableSize;
	Ar << MipLevelStreamingInfo.OccupancyBitsOffset;
	Ar << MipLevelStreamingInfo.OccupancyBitsSize;
	Ar << MipLevelStreamingInfo.TileDataOffsetsOffset;
	Ar << MipLevelStreamingInfo.TileDataOffsetsSize;
	Ar << MipLevelStreamingInfo.TileDataOffset;
	Ar << MipLevelStreamingInfo.TileDataSize;
	Ar << MipLevelStreamingInfo.NumPhysicalTiles;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, UE::SVT::FHeader& Header)
{
	Ar << Header.VirtualVolumeResolution;
	Ar << Header.VirtualVolumeAABBMin;
	Ar << Header.VirtualVolumeAABBMax;
	Ar << Header.PageTableVolumeResolution;
	Ar << Header.PageTableVolumeAABBMin;
	Ar << Header.PageTableVolumeAABBMax;
	UE::SVT::Private::SerializeEnumAs<uint8>(Ar, Header.AttributesFormats[0]);
	UE::SVT::Private::SerializeEnumAs<uint8>(Ar, Header.AttributesFormats[1]);
	Ar << Header.FallbackValues[0];
	Ar << Header.FallbackValues[1];
	return Ar;
}

////////////////////////////////////////////////////////////////////////////////////////////////

namespace UE
{
namespace SVT
{

static int32 ComputeNumMipLevels(const FIntVector3& InVirtualVolumeMin, const FIntVector3& InVirtualVolumeMax)
{
	FHeader DummyHeader(InVirtualVolumeMin, InVirtualVolumeMax, PF_Unknown, PF_Unknown, FVector4f(), FVector4f());
	int32 Levels = 1;
	FIntVector3 PageTableResolution = DummyHeader.PageTableVolumeResolution;
	while (PageTableResolution.X > 1 || PageTableResolution.Y > 1 || PageTableResolution.Z > 1)
	{
		PageTableResolution /= 2;
		++Levels;
	}
	return Levels;
};

static const FString& GetDerivedDataVersion()
{
	static FString CachedVersionString = TEXT("381AE2A9-A903-4C8F-8486-891E24D6FC72");	// Bump this if you want to ignore all cached data so far.
	return CachedVersionString;
}

FHeader::FHeader(const FIntVector3& AABBMin, const FIntVector3& AABBMax, EPixelFormat FormatA, EPixelFormat FormatB, const FVector4f& FallbackValueA, const FVector4f& FallbackValueB)
{
	VirtualVolumeAABBMin = AABBMin;
	VirtualVolumeAABBMax = AABBMax;
	VirtualVolumeResolution = VirtualVolumeAABBMax - VirtualVolumeAABBMin;

	PageTableVolumeAABBMin = VirtualVolumeAABBMin / SPARSE_VOLUME_TILE_RES;
	PageTableVolumeAABBMax = (VirtualVolumeAABBMax + FIntVector3(SPARSE_VOLUME_TILE_RES - 1)) / SPARSE_VOLUME_TILE_RES;
	PageTableVolumeResolution = PageTableVolumeAABBMax - PageTableVolumeAABBMin;

	// We need to ensure a power of two resolution for the page table in order to fit all mips of the page table into the physical mips of the texture resource.
	PageTableVolumeResolution.X = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.X);
	PageTableVolumeResolution.Y = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.Y);
	PageTableVolumeResolution.Z = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.Z);
	PageTableVolumeAABBMax = PageTableVolumeAABBMin + PageTableVolumeResolution;

	AttributesFormats[0] = FormatA;
	AttributesFormats[1] = FormatB;

	FallbackValues[0] = FallbackValueA;
	FallbackValues[1] = FallbackValueB;
}

void FHeader::UpdatePageTableFromGlobalNumMipLevels(int32 NumMipLevelsGlobal)
{
	check(NumMipLevelsGlobal < 32);
	const int32 Alignment = 1 << (NumMipLevelsGlobal - 1);

	PageTableVolumeAABBMin = ((VirtualVolumeAABBMin / SPARSE_VOLUME_TILE_RES) / Alignment) * Alignment;
	PageTableVolumeAABBMax = (VirtualVolumeAABBMax + FIntVector3(SPARSE_VOLUME_TILE_RES - 1)) / SPARSE_VOLUME_TILE_RES;
	PageTableVolumeResolution = PageTableVolumeAABBMax - PageTableVolumeAABBMin;

	// We need to ensure a power of two resolution for the page table in order to fit all mips of the page table into the physical mips of the texture resource.
	PageTableVolumeResolution.X = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.X);
	PageTableVolumeResolution.Y = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.Y);
	PageTableVolumeResolution.Z = FMath::RoundUpToPowerOfTwo(PageTableVolumeResolution.Z);
	PageTableVolumeAABBMax = PageTableVolumeAABBMin + PageTableVolumeResolution;
}

bool FHeader::Validate(bool bPrintToLog)
{
	for (int32 i = 0; i < 2; ++i)
	{
		if (!SVT::IsSupportedFormat(AttributesFormats[i]))
		{
			if (bPrintToLog)
			{
				UE_LOG(LogSparseVolumeTexture, Warning, TEXT("'%s' is not a supported SparseVolumeTexture format!"), GPixelFormats[AttributesFormats[i]].Name);
			}
			return false;
		}
	}

	if (PageTableVolumeResolution.X > SVT::MaxVolumeTextureDim || PageTableVolumeResolution.Y > SVT::MaxVolumeTextureDim || PageTableVolumeResolution.Z > SVT::MaxVolumeTextureDim)
	{
		if (bPrintToLog)
		{
			UE_LOG(LogSparseVolumeTexture, Warning, TEXT("SparseVolumeTexture page table texture dimensions exceed limit (%ix%ix%i): %ix%ix%i"),
				SVT::MaxVolumeTextureDim, SVT::MaxVolumeTextureDim, SVT::MaxVolumeTextureDim,
				PageTableVolumeResolution.X, PageTableVolumeResolution.Y, PageTableVolumeResolution.Z);
		}
		return false;
	}

	const int64 PageTableSizeBytes = (int64)PageTableVolumeResolution.X * (int64)PageTableVolumeResolution.Y * (int64)PageTableVolumeResolution.Z * sizeof(uint32);
	if (PageTableSizeBytes > SVT::MaxResourceSize)
	{
		if (bPrintToLog)
		{
			UE_LOG(LogSparseVolumeTexture, Warning, TEXT("SparseVolumeTexture page table texture memory size (%ll) exceeds the 2048MB GPU resource limit!"), (long long)PageTableSizeBytes);
		}
		return false;
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FResources::Serialize(FArchive& Ar, UObject* Owner, bool bCooked)
{
	// Note: this is all derived data, native versioning is not needed, but be sure to bump GetDerivedDataVersion() when modifying!
	FStripDataFlags StripFlags(Ar, 0);
	if (!StripFlags.IsDataStrippedForServer())
	{
		Ar << Header;

		uint32 StoredResourceFlags;
		if (Ar.IsSaving() && bCooked)
		{
			// Disable DDC store when saving out a cooked build
			StoredResourceFlags = ResourceFlags & ~EResourceFlag_StreamingDataInDDC;
			Ar << StoredResourceFlags;
		}
		else
		{
			Ar << ResourceFlags;
			StoredResourceFlags = ResourceFlags;
		}

		Ar << MipLevelStreamingInfo;
		Ar << RootData;

		// StreamableMipLevels is only serialized in cooked builds and when caching to DDC failed in editor builds.
		// If the data was successfully cached to DDC, we just query it from DDC on the next run or recreate it if that failed.
		if (StoredResourceFlags & EResourceFlag_StreamingDataInDDC)
		{
#if !WITH_EDITORONLY_DATA
			checkf(false, TEXT("UE::SVT::FResources was serialized with EResourceFlag_StreamingDataInDDC in a cooked build!"));
#endif
		}
		else
		{
			StreamableMipLevels.Serialize(Ar, Owner, 0);
		}

#if !WITH_EDITORONLY_DATA
		check(!HasStreamingData() || StreamableMipLevels.GetBulkDataSize() > 0);
#endif
	}
}

bool FResources::HasStreamingData() const
{
	if (MipLevelStreamingInfo.Num() == 1)
	{
		// Root mip level does not stream
		return false;
	}
	else
	{
		// It is possible for multiple mip levels to exist but all these levels are empty, so we can't just check the number of mip levels
		bool bHasStreamingData = false;
		for (int32 MipLevelIndex = 0; MipLevelIndex < MipLevelStreamingInfo.Num() - 1; ++MipLevelIndex)
		{
			bHasStreamingData = bHasStreamingData || (MipLevelStreamingInfo[MipLevelIndex].BulkSize > 0);
		}
		return bHasStreamingData;
	}
}

#if WITH_EDITORONLY_DATA

void FResources::DropBulkData()
{
	if (HasStreamingData() && (ResourceFlags & EResourceFlag_StreamingDataInDDC))
	{
		StreamableMipLevels.RemoveBulkData();
	}
}

bool FResources::RebuildBulkDataFromCacheAsync(const UObject* Owner, bool& bFailed)
{
	bFailed = false;
	if (!HasStreamingData() || (ResourceFlags & EResourceFlag_StreamingDataInDDC) == 0u)
	{
		return true;
	}
	if (DDCRebuildState.load() == EDDCRebuildState::Initial)
	{
		if (StreamableMipLevels.IsBulkDataLoaded())
		{
			return true;
		}
		// Handle Initial state first so we can transition directly to Succeeded/Failed if the data was immediately available from the cache.
		check(!(*DDCRequestOwner).IsValid());
		BeginRebuildBulkDataFromCache(Owner);
	}
	switch (DDCRebuildState.load())
	{
	case EDDCRebuildState::Pending:
		return false;
	case EDDCRebuildState::Succeeded:
		check(StreamableMipLevels.GetBulkDataSize() > 0);
		EndRebuildBulkDataFromCache();
		return true;
	case EDDCRebuildState::Failed:
		bFailed = true;
		EndRebuildBulkDataFromCache();
		return true;
	default:
		check(false);
		return true;
	}
}

bool FResources::Build(USparseVolumeTextureFrame* Owner, UE::Serialization::FEditorBulkData& SourceData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Build);

	// Check if the virtualized bulk data payload is available
	if (SourceData.HasPayloadData())
	{
		UE::Serialization::FEditorBulkDataReader SourceDataReader(SourceData);
		FTextureData SourceTextureData;
		SourceDataReader << SourceTextureData;

		FTextureDataAddressingInfo AddressingInfo{};
		AddressingInfo.VolumeResolution = Owner->GetVolumeResolution();
		AddressingInfo.AddressX = Owner->GetTextureAddressX();
		AddressingInfo.AddressY = Owner->GetTextureAddressY();
		AddressingInfo.AddressZ = Owner->GetTextureAddressZ();

		const int32 NumMipLevelsGlobal = Owner->GetNumMipLevels();
		const bool bMoveMip0FromSource = true; // we have no need to keep SourceTextureData around
		FTextureData DerivedTextureData;
		if (!SourceTextureData.BuildDerivedData(AddressingInfo, NumMipLevelsGlobal, bMoveMip0FromSource, DerivedTextureData))
		{
			return false;
		}

		// Now unload the source data
		SourceData.UnloadData();

		const int32 NumMipLevels = DerivedTextureData.MipMaps.Num();

		Header = DerivedTextureData.Header;
		RootData.Reset();
		MipLevelStreamingInfo.SetNumZeroed(NumMipLevels);
		ResourceFlags = 0;
		ResourceName.Reset();
		DDCKeyHash.Reset();
		DDCRawHash.Reset();
		DDCRebuildState.store(EDDCRebuildState::Initial);

		// Stores page table into BulkData as two consecutive arrays of packed page coordinates and linear indices into the physical tiles array.
		// Returns number of written/non-zero page table entries
		auto CompressPageTable = [](const TArray<uint32>& PageTable, const FIntVector3& Resolution, TArray<uint8>& BulkData) -> int32
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Build::CompressPageTable);

			int32 NumNonZeroEntries = 0;
			for (uint32 Entry : PageTable)
			{
				if (Entry)
				{
					++NumNonZeroEntries;
				}
			}

			const int32 BaseOffset = BulkData.Num();
			BulkData.SetNum(BulkData.Num() + NumNonZeroEntries * 2 * sizeof(uint32));
			uint32* PackedCoords = reinterpret_cast<uint32*>(BulkData.GetData() + BaseOffset);
			uint32* PageEntries = PackedCoords + NumNonZeroEntries;
			
			int32 NumWrittenEntries = 0;
			for (int32 Z = 0; Z < Resolution.Z; ++Z)
			{
				for (int32 Y = 0; Y < Resolution.Y; ++Y)
				{
					for (int32 X = 0; X < Resolution.X; ++X)
					{
						const int32 LinearCoord = (Z * Resolution.Y * Resolution.X) + (Y * Resolution.X) + X;
						const uint32 Entry = PageTable[LinearCoord];
						if (Entry)
						{
							const uint32 Packed = (X & 0x7FFu) | ((Y & 0x7FFu) << 11u) | ((Z & 0x3FFu) << 22u);
							PackedCoords[NumWrittenEntries] = Packed;
							PageEntries[NumWrittenEntries] = Entry - 1;
							++NumWrittenEntries;
						}
					}
				}
			}

			return NumNonZeroEntries;
		};

		auto CompressTiles = [](int32 NumTiles, const TArray64<uint8>& PhysicalTileDataA, const TArray64<uint8>& PhysicalTileDataB, const TArrayView<EPixelFormat>& Formats, const TArrayView<FVector4f>& FallbackValues, TArray<uint8>& BulkData, FMipLevelStreamingInfo& MipStreamingInfo)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Build::CompressTiles);

			const int64 FormatSize[] = { GPixelFormats[Formats[0]].BlockBytes, GPixelFormats[Formats[1]].BlockBytes };
			uint8 NullTileValuesU8[2][sizeof(float) * 4] = {};
			int32 NumTextures = 0;
			for (int32 i = 0; i < 2; ++i)
			{
				if (Formats[i] != PF_Unknown)
				{
					++NumTextures;
					SVT::WriteVoxel(0, NullTileValuesU8[i], Formats[i], FallbackValues[i]);
				}
			}

			const int32 OccupancySizePerTexture = NumTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32);
			const int32 OccupancySize = NumTextures * OccupancySizePerTexture;
			const int32 TileDataOffsetsSize = NumTextures * NumTiles * sizeof(uint32);
			BulkData.Reserve(BulkData.Num() + OccupancySize + TileDataOffsetsSize);

			MipStreamingInfo.OccupancyBitsOffset[0] = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.OccupancyBitsSize[0] = Formats[0] != PF_Unknown ? OccupancySizePerTexture : 0;
			BulkData.SetNum(BulkData.Num() + MipStreamingInfo.OccupancyBitsSize[0]);
			
			MipStreamingInfo.OccupancyBitsOffset[1] = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.OccupancyBitsSize[1] = Formats[1] != PF_Unknown ? OccupancySizePerTexture : 0;
			BulkData.SetNum(BulkData.Num() + MipStreamingInfo.OccupancyBitsSize[1]);

			MipStreamingInfo.TileDataOffsetsOffset[0] = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.TileDataOffsetsSize[0] = Formats[0] != PF_Unknown ? NumTiles * sizeof(uint32) : 0;
			BulkData.SetNum(BulkData.Num() + MipStreamingInfo.TileDataOffsetsSize[0]);

			MipStreamingInfo.TileDataOffsetsOffset[1] = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.TileDataOffsetsSize[1] = Formats[1] != PF_Unknown ? NumTiles * sizeof(uint32) : 0;
			BulkData.SetNum(BulkData.Num() + MipStreamingInfo.TileDataOffsetsSize[1]);

			// Zero the bitmasks
			FMemory::Memzero(BulkData.GetData() + MipStreamingInfo.BulkOffset + MipStreamingInfo.OccupancyBitsOffset[0], MipStreamingInfo.OccupancyBitsSize[0] + MipStreamingInfo.OccupancyBitsSize[1]);

			uint32* OccupancyBits[2];
			OccupancyBits[0] = reinterpret_cast<uint32*>(BulkData.GetData() + MipStreamingInfo.BulkOffset + MipStreamingInfo.OccupancyBitsOffset[0]);
			OccupancyBits[1] = reinterpret_cast<uint32*>(BulkData.GetData() + MipStreamingInfo.BulkOffset + MipStreamingInfo.OccupancyBitsOffset[1]);

			uint32* PrefixSums[2];
			PrefixSums[0] = reinterpret_cast<uint32*>(BulkData.GetData() + MipStreamingInfo.BulkOffset + MipStreamingInfo.TileDataOffsetsOffset[0]);
			PrefixSums[1] = reinterpret_cast<uint32*>(BulkData.GetData() + MipStreamingInfo.BulkOffset + MipStreamingInfo.TileDataOffsetsOffset[1]);

			// Compute occupancy bitmasks and count number of non-fallback voxels per tile
			ParallelFor(NumTiles, [&](int32 TileIndex)
				{
					for (int32 AttributesIdx = 0; AttributesIdx < 2; ++AttributesIdx)
					{
						if (Formats[AttributesIdx] != PF_Unknown)
						{
							PrefixSums[AttributesIdx][TileIndex] = 0;
							for (int64 VoxelIndex = 0; VoxelIndex < SVT::NumVoxelsPerPaddedTile; ++VoxelIndex)
							{
								const uint8* Src = (AttributesIdx == 0 ? PhysicalTileDataA.GetData() : PhysicalTileDataB.GetData()) + FormatSize[AttributesIdx] * (TileIndex * SVT::NumVoxelsPerPaddedTile + VoxelIndex);
								bool bIsFallbackValue = FMemory::Memcmp(Src, NullTileValuesU8[AttributesIdx], FormatSize[AttributesIdx]) == 0;
								if (!bIsFallbackValue)
								{
									const int64 WordIndex = TileIndex * SVT::NumOccupancyWordsPerPaddedTile + (VoxelIndex / 32);
									OccupancyBits[AttributesIdx][WordIndex] |= 1u << (static_cast<uint32>(VoxelIndex) % 32u);
									PrefixSums[AttributesIdx][TileIndex]++;
								}
							}
						}
					}
				});

			// Compute actual per-tile voxel data offsets (prefix sum)
			uint32 Sums[2] = {};
			for (int32 AttributesIdx = 0; AttributesIdx < 2; ++AttributesIdx)
			{
				if (Formats[AttributesIdx] != PF_Unknown)
				{
					for (int32 TileIndex = 0; TileIndex < NumTiles; ++TileIndex)
					{
						uint32 Tmp = PrefixSums[AttributesIdx][TileIndex];
						PrefixSums[AttributesIdx][TileIndex] = Sums[AttributesIdx];
						Sums[AttributesIdx] += Tmp;
					}
				}
			}

			// Allocate compacted tile data memory
			MipStreamingInfo.TileDataOffset[0] = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.TileDataSize[0] = Sums[0] * FormatSize[0];

			MipStreamingInfo.TileDataOffset[1] = MipStreamingInfo.TileDataOffset[0] + MipStreamingInfo.TileDataSize[0];
			MipStreamingInfo.TileDataSize[1] = Sums[1] * FormatSize[1];

			BulkData.SetNum(BulkData.Num() + MipStreamingInfo.TileDataSize[0] + MipStreamingInfo.TileDataSize[1]);

			// All above pointers into the BulkData are now invalid!

			uint8* RelativeBulkDataPtr = BulkData.GetData() + MipStreamingInfo.BulkOffset;

			uint8* DstTileData[2];
			DstTileData[0] = RelativeBulkDataPtr + MipStreamingInfo.TileDataOffset[0];
			DstTileData[1] = RelativeBulkDataPtr + MipStreamingInfo.TileDataOffset[1];

			// Copy voxels to compacted locations
			ParallelFor(NumTiles, [&](int32 TileIndex)
				{
					for (int32 AttributesIdx = 0; AttributesIdx < 2; ++AttributesIdx)
					{
						if (Formats[AttributesIdx] != PF_Unknown)
						{
							uint32 VoxelDataWriteOffset = reinterpret_cast<uint32*>(RelativeBulkDataPtr + MipStreamingInfo.TileDataOffsetsOffset[AttributesIdx])[TileIndex];
							const uint32* TileOccupancyBits = reinterpret_cast<uint32*>(RelativeBulkDataPtr + MipStreamingInfo.OccupancyBitsOffset[AttributesIdx] + TileIndex * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32));

							for (int64 VoxelIndex = 0; VoxelIndex < SVT::NumVoxelsPerPaddedTile; ++VoxelIndex)
							{
								const int64 WordIndex = (VoxelIndex / 32);

								if (TileOccupancyBits[WordIndex] & (1u << (static_cast<uint32>(VoxelIndex) % 32u)))
								{
									uint8* Dst = DstTileData[AttributesIdx] + VoxelDataWriteOffset * FormatSize[AttributesIdx];
									const uint8* Src = (AttributesIdx == 0 ? PhysicalTileDataA.GetData() : PhysicalTileDataB.GetData()) + FormatSize[AttributesIdx] * (TileIndex * SVT::NumVoxelsPerPaddedTile + VoxelIndex);
									FMemory::Memcpy(Dst, Src, FormatSize[AttributesIdx]);
									++VoxelDataWriteOffset;
								}
							}
						}
					}
				});
		};

		TArray<uint8> StreamableBulkData;
		for (int32 MipLevelIdx = 0; MipLevelIdx < DerivedTextureData.MipMaps.Num(); ++MipLevelIdx)
		{
			const FTextureData::FMipMap& Mip = DerivedTextureData.MipMaps[MipLevelIdx];
			const bool bIsRootMipLevel = (MipLevelIdx == (DerivedTextureData.MipMaps.Num() - 1));
			TArray<uint8>& BulkData = bIsRootMipLevel ? RootData : StreamableBulkData;

			FIntVector3 MipPageTableResolution = DerivedTextureData.Header.PageTableVolumeResolution >> MipLevelIdx;
			MipPageTableResolution = FIntVector3(FMath::Max(1, MipPageTableResolution.X), FMath::Max(1, MipPageTableResolution.Y), FMath::Max(1, MipPageTableResolution.Z));

			FMipLevelStreamingInfo& MipStreamingInfo = MipLevelStreamingInfo[MipLevelIdx];
			MipStreamingInfo.BulkOffset = BulkData.Num();

			MipStreamingInfo.PageTableOffset = BulkData.Num() - MipStreamingInfo.BulkOffset;
			const int32 NumNonZeroPageTableEntries = CompressPageTable(Mip.PageTable, MipPageTableResolution, BulkData);
			MipStreamingInfo.PageTableSize = NumNonZeroPageTableEntries * 2 * sizeof(uint32);
			
			CompressTiles(Mip.NumPhysicalTiles, Mip.PhysicalTileDataA, Mip.PhysicalTileDataB, Header.AttributesFormats, Header.FallbackValues, BulkData, MipStreamingInfo);

			MipStreamingInfo.BulkSize = BulkData.Num() - MipStreamingInfo.BulkOffset;
			MipStreamingInfo.NumPhysicalTiles = Mip.NumPhysicalTiles;
		}

		// Store StreamableMipLevels
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Build::StoreStreamableMipLevels);

			StreamableMipLevels.Lock(LOCK_READ_WRITE);
			uint8* Ptr = (uint8*)StreamableMipLevels.Realloc(StreamableBulkData.Num());
			FMemory::Memcpy(Ptr, StreamableBulkData.GetData(), StreamableBulkData.Num());
			StreamableMipLevels.Unlock();
			StreamableMipLevels.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
		}

		return true;
	}
	return false;
}

void FResources::Cache(USparseVolumeTextureFrame* Owner, UE::Serialization::FEditorBulkData& SourceData, bool bLocalCachingOnly)
{
	if (Owner->GetPackage()->bIsCookedForEditor)
	{
		// Don't cache for cooked packages
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Cache);

	using namespace UE::DerivedData;

	static const FValueId SVTDataId = FValueId::FromName("SparseVolumeTextureData");
	static const FValueId SVTStreamingDataId = FValueId::FromName("SparseVolumeTextureStreamingData");
	const FString KeySuffix = SourceData.GetIdentifier().ToString() + FString::Format(TEXT("{0}_{1}_{2}_{3}"), { Owner->GetNumMipLevels(), Owner->GetTextureAddressX(), Owner->GetTextureAddressY(), Owner->GetTextureAddressZ() });
	FString DerivedDataKey = FDerivedDataCacheInterface::BuildCacheKey(TEXT("SPARSEVOLUMETEXTURE"), *GetDerivedDataVersion(), *KeySuffix);

	FCacheKey CacheKey;
	CacheKey.Bucket = FCacheBucket(TEXT("SparseVolumeTexture"));
	CacheKey.Hash = FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(DerivedDataKey)));

	// Set the cache policy (local only vs local+remote)
	ECachePolicy DefaultCachePolicy = ECachePolicy::Default;
	switch (GSVTRemoteDDCBehavior)
	{
	case 1:	DefaultCachePolicy = ECachePolicy::Local; break;
	case 2:	DefaultCachePolicy = ECachePolicy::Default; break;
	default: DefaultCachePolicy = bLocalCachingOnly ? ECachePolicy::Local : ECachePolicy::Default; break;
	}

	// Check if the data already exists in DDC
	FSharedBuffer ResourcesDataBuffer;
	FIoHash SVTStreamingDataHash;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Cache::CheckDDC);

		FCacheRecordPolicyBuilder PolicyBuilder(DefaultCachePolicy | ECachePolicy::KeepAlive);
		PolicyBuilder.AddValuePolicy(SVTStreamingDataId, DefaultCachePolicy | ECachePolicy::SkipData);

		FCacheGetRequest Request;
		Request.Name = Owner->GetPathName();
		Request.Key = CacheKey;
		Request.Policy = PolicyBuilder.Build();

		FRequestOwner RequestOwner(EPriority::Blocking);
		GetCache().Get(MakeArrayView(&Request, 1), RequestOwner,
			[&ResourcesDataBuffer, &SVTStreamingDataHash](FCacheGetResponse&& Response)
			{
				if (Response.Status == EStatus::Ok)
				{
					const FCompressedBuffer& CompressedBuffer = Response.Record.GetValue(SVTDataId).GetData();
					ResourcesDataBuffer = CompressedBuffer.Decompress();

					SVTStreamingDataHash = Response.Record.GetValue(SVTStreamingDataId).GetRawHash();
				}
			});
		RequestOwner.Wait();
	}

	if (!ResourcesDataBuffer.IsNull())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Cache::SerializeFromDDC);

		// Found it!
		// We can serialize the data from the DDC buffer and are done.
		FMemoryReaderView Ar(ResourcesDataBuffer.GetView(), /*bIsPersistent=*/ true);
		Serialize(Ar, Owner, /*bCooked=*/ false);

		check(StreamableMipLevels.GetBulkDataSize() == 0);
		if (ResourceFlags & EResourceFlag_StreamingDataInDDC)
		{
			DDCKeyHash = CacheKey.Hash;
			DDCRawHash = SVTStreamingDataHash;
		}
	}
	else
	{
		// DDC lookup failed! Build the data again.
		const bool bBuiltSuccessfully = Build(Owner, SourceData);
		check(bBuiltSuccessfully);

		FCacheRecordBuilder RecordBuilder(CacheKey);
		if (!MipLevelStreamingInfo.IsEmpty())
		{
			if (HasStreamingData())
			{
				FByteBulkData& BulkData = StreamableMipLevels;

				FValue Value = FValue::Compress(FSharedBuffer::MakeView(BulkData.LockReadOnly(), BulkData.GetBulkDataSize()));
				RecordBuilder.AddValue(SVTStreamingDataId, Value);
				BulkData.Unlock();
				ResourceFlags |= EResourceFlag_StreamingDataInDDC;
				DDCKeyHash = CacheKey.Hash;
				DDCRawHash = Value.GetRawHash();
			}
		}

		// Serialize to a buffer and store into DDC.
		FLargeMemoryWriter Ar(0, /*bIsPersistent=*/ true);
		Serialize(Ar, Owner, /*bCooked=*/ false);

		bool bSavedToDDC = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FResources::Cache::SaveToDDC);

			FValue Value = FValue::Compress(FSharedBuffer::MakeView(Ar.GetData(), Ar.TotalSize()));
			RecordBuilder.AddValue(SVTDataId, Value);

			FRequestOwner RequestOwner(UE::DerivedData::EPriority::Blocking);
			const FCachePutRequest PutRequest = { FSharedString(Owner->GetPathName()), RecordBuilder.Build(), DefaultCachePolicy | ECachePolicy::KeepAlive };
			GetCache().Put(MakeArrayView(&PutRequest, 1), RequestOwner,
				[&bSavedToDDC](FCachePutResponse&& Response)
				{
					if (Response.Status == EStatus::Ok)
					{
						bSavedToDDC = true;
					}
				});

			RequestOwner.Wait();

			if (bSavedToDDC && HasStreamingData())
			{
				// Drop streaming data from memory when it has been successfully committed to DDC
				DropBulkData();
			}
		}

		if (HasStreamingData() && !bSavedToDDC)
		{
			// Streaming data was not pushed to DDC. Disable DDC streaming flag.
			check(StreamableMipLevels.GetBulkDataSize() > 0);
			ResourceFlags &= ~EResourceFlag_StreamingDataInDDC;
		}
	}
}

void FResources::SetDefault(EPixelFormat FormatA, EPixelFormat FormatB, const FVector4f& FallbackValueA, const FVector4f& FallbackValueB)
{
	Header = FHeader(FIntVector(0, 0, 0), FIntVector(1, 1, 1), FormatA, FormatB, FallbackValueA, FallbackValueB);
	ResourceFlags = 0;
	MipLevelStreamingInfo.SetNumZeroed(1);
	RootData.Reset();
	StreamableMipLevels.RemoveBulkData();
	ResourceName.Reset();
	DDCKeyHash.Reset();
	DDCRawHash.Reset();
	DDCRebuildState.store(EDDCRebuildState::Initial);
}

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA

void FResources::BeginRebuildBulkDataFromCache(const UObject* Owner)
{
	check(DDCRebuildState.load() == EDDCRebuildState::Initial);
	if (!HasStreamingData() || (ResourceFlags & EResourceFlag_StreamingDataInDDC) == 0u)
	{
		return;
	}
	using namespace UE::DerivedData;
	FCacheKey Key;
	Key.Bucket = FCacheBucket(TEXT("SparseVolumeTexture"));
	Key.Hash = DDCKeyHash;
	check(!DDCKeyHash.IsZero());
	FCacheGetChunkRequest Request;
	Request.Name = Owner->GetPathName();
	Request.Id = FValueId::FromName("SparseVolumeTextureStreamingData");
	Request.Key = Key;
	Request.RawHash = DDCRawHash;
	check(!DDCRawHash.IsZero());
	FSharedBuffer SharedBuffer;
	*DDCRequestOwner = MakePimpl<FRequestOwner>(EPriority::Normal);
	DDCRebuildState.store(EDDCRebuildState::Pending);
	GetCache().GetChunks(MakeArrayView(&Request, 1), **DDCRequestOwner,
		[this](FCacheGetChunkResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				StreamableMipLevels.Lock(LOCK_READ_WRITE);
				uint8* Ptr = (uint8*)StreamableMipLevels.Realloc(Response.RawData.GetSize());
				FMemory::Memcpy(Ptr, Response.RawData.GetData(), Response.RawData.GetSize());
				StreamableMipLevels.Unlock();
				StreamableMipLevels.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
				DDCRebuildState.store(EDDCRebuildState::Succeeded);
			}
			else
			{
				DDCRebuildState.store(EDDCRebuildState::Failed);
			}
		});
}

void FResources::EndRebuildBulkDataFromCache()
{
	if (*DDCRequestOwner)
	{
		(*DDCRequestOwner)->Wait();
		(*DDCRequestOwner).Reset();
	}
	DDCRebuildState.store(EDDCRebuildState::Initial);
}

#endif // WITH_EDITORONLY_DATA

////////////////////////////////////////////////////////////////////////////////////////////////

void FTextureRenderResources::GetPackedUniforms(FUintVector4& OutPacked0, FUintVector4& OutPacked1) const
{
	check(IsInParallelRenderingThread());

	auto AsUint = [](float X)
	{
		union { float F; uint32 U; } FU = { X };
		return FU.U;
	};

	const FIntVector3 PageTableOffset = Header.PageTableVolumeAABBMin;
	const FVector3f TileDataTexelSize = FVector3f(
		1.0f / TileDataTextureResolution.X,
		1.0f / TileDataTextureResolution.Y,
		1.0f / TileDataTextureResolution.Z);
	const FVector3f VolumePageResolution = FVector3f(GlobalVolumeResolution) / SPARSE_VOLUME_TILE_RES;

	OutPacked0.X = AsUint(VolumePageResolution.X);
	OutPacked0.Y = AsUint(VolumePageResolution.Y);
	OutPacked0.Z = AsUint(VolumePageResolution.Z);
	OutPacked0.W = (PageTableOffset.X & 0x7FFu) | ((PageTableOffset.Y & 0x7FFu) << 11u) | ((PageTableOffset.Z & 0x3FFu) << 22u);
	OutPacked1.X = AsUint(TileDataTexelSize.X);
	OutPacked1.Y = AsUint(TileDataTexelSize.Y);
	OutPacked1.Z = AsUint(TileDataTexelSize.Z);
	OutPacked1.W = 0;
	OutPacked1.W |= (uint32)((FrameIndex & 0xFFFF) << 0);
	OutPacked1.W |= (uint32)(((NumLogicalMipLevels - 1) & 0xFFFF) << 16);
}

void FTextureRenderResources::SetGlobalVolumeResolution_GameThread(const FIntVector3& InGlobalVolumeResolution)
{
	ENQUEUE_RENDER_COMMAND(FTextureRenderResources_UpdateGlobalVolumeResolution)(
		[this, InGlobalVolumeResolution](FRHICommandListImmediate& RHICmdList)
		{
			GlobalVolumeResolution = InGlobalVolumeResolution;
		});
}

void FTextureRenderResources::InitRHI(FRHICommandListBase&)
{
	PageTableTextureReferenceRHI = RHICreateTextureReference(GBlackUintVolumeTexture->TextureRHI);
	PhysicalTileDataATextureReferenceRHI = RHICreateTextureReference(GBlackVolumeTexture->TextureRHI);
	PhysicalTileDataBTextureReferenceRHI = RHICreateTextureReference(GBlackVolumeTexture->TextureRHI);
}

void FTextureRenderResources::ReleaseRHI()
{
	PageTableTextureReferenceRHI.SafeRelease();
	PhysicalTileDataATextureReferenceRHI.SafeRelease();
	PhysicalTileDataBTextureReferenceRHI.SafeRelease();
	StreamingInfoBufferSRVRHI.SafeRelease();
}

}
}

////////////////////////////////////////////////////////////////////////////////////////////////

USparseVolumeTexture::USparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UE::Shader::EValueType USparseVolumeTexture::GetUniformParameterType(int32 Index)
{
	switch (Index)
	{
	case ESparseVolumeTexture_TileSize:				return UE::Shader::EValueType::Float1;
	case ESparseVolumeTexture_PageTableSize:		return UE::Shader::EValueType::Float3;
	case ESparseVolumeTexture_UVScale:				return UE::Shader::EValueType::Float3;
	case ESparseVolumeTexture_UVBias:				return UE::Shader::EValueType::Float3;
	default:
		break;
	}
	checkNoEntry();
	return UE::Shader::EValueType::Float4;
}

#if WITH_EDITOR
void USparseVolumeTexture::NotifyMaterials(const ENotifyMaterialsEffectOnShaders EffectOnShaders)
{
	// Create a material update context to safely update materials.
	{
		FMaterialUpdateContext UpdateContext;

		// Notify any material that uses this texture
		TSet<UMaterial*> BaseMaterialsThatUseThisTexture;
		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* MaterialInterface = *It;
			if (!FPlatformProperties::IsServerOnly() && MaterialInterface->GetReferencedTextures().Contains(this))
			{
				UpdateContext.AddMaterialInterface(MaterialInterface);
				// This is a bit tricky. We want to make sure all materials using this texture are
				// updated. Materials are always updated. Material instances may also have to be
				// updated and if they have static permutations their children must be updated
				// whether they use the texture or not! The safe thing to do is to add the instance's
				// base material to the update context causing all materials in the tree to update.
				BaseMaterialsThatUseThisTexture.Add(MaterialInterface->GetMaterial());
			}
		}

		// Go ahead and update any base materials that need to be.
		if (EffectOnShaders == ENotifyMaterialsEffectOnShaders::Default)
		{
			for (TSet<UMaterial*>::TConstIterator It(BaseMaterialsThatUseThisTexture); It; ++It)
			{
				(*It)->PostEditChange();
			}
		}
		else
		{
			FPropertyChangedEvent EmptyPropertyUpdateStruct(nullptr);
			for (TSet<UMaterial*>::TConstIterator It(BaseMaterialsThatUseThisTexture); It; ++It)
			{
				(*It)->PostEditChangePropertyInternal(EmptyPropertyUpdateStruct, UMaterial::EPostEditChangeEffectOnShaders::DoesNotInvalidate);
			}
		}
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

USparseVolumeTextureFrame::USparseVolumeTextureFrame(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

USparseVolumeTextureFrame* USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(USparseVolumeTexture* SparseVolumeTexture, float FrameIndex, int32 MipLevel, bool bBlocking)
{
	if (UStreamableSparseVolumeTexture* StreamableSVT = Cast<UStreamableSparseVolumeTexture>(SparseVolumeTexture))
	{
		UE::SVT::GetStreamingManager().Request_GameThread(StreamableSVT, FrameIndex, MipLevel, bBlocking);
		return StreamableSVT->GetFrame(static_cast<int32>(FrameIndex));
	}
	return nullptr;
}

bool USparseVolumeTextureFrame::Initialize(USparseVolumeTexture* InOwner, int32 InFrameIndex, UE::SVT::FTextureData& UncookedFrame)
{
#if WITH_EDITORONLY_DATA
	Owner = InOwner;
	FrameIndex = InFrameIndex;
	{
		UE::Serialization::FEditorBulkDataWriter SourceDataArchiveWriter(SourceData);
		SourceDataArchiveWriter << UncookedFrame;
	}

	return true;
#else
	return false;
#endif
}

bool USparseVolumeTextureFrame::CreateTextureRenderResources()
{
	if (!TextureRenderResources)
	{
		TextureRenderResources = new UE::SVT::FTextureRenderResources();
		TextureRenderResources->SetGlobalVolumeResolution_GameThread(Owner->GetVolumeResolution());
		BeginInitResource(TextureRenderResources);

		return true;
	}
	return false;
}

void USparseVolumeTextureFrame::PostLoad()
{
	Super::PostLoad();

	CreateTextureRenderResources();
}

void USparseVolumeTextureFrame::FinishDestroy()
{
	Super::FinishDestroy();
}

void USparseVolumeTextureFrame::BeginDestroy()
{
	// Ensure that the streamable SVT has been removed from the streaming manager
	if (IsValid(Owner))
	{
		UE::SVT::GetStreamingManager().Remove_GameThread(CastChecked<UStreamableSparseVolumeTexture>(Owner));
	}
	
	if (TextureRenderResources)
	{
		ENQUEUE_RENDER_COMMAND(USparseVolumeTextureFrame_DeleteTextureRenderResources)(
			[Resources = TextureRenderResources](FRHICommandListImmediate& RHICmdList)
			{
				Resources->ReleaseResource();
				delete Resources;
			});
		TextureRenderResources = nullptr;
	}

	Super::BeginDestroy();
}

void USparseVolumeTextureFrame::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsCooking())
	{
		SourceData.Serialize(Ar, this);
	}
#endif

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	// Inline the derived data for cooked builds.
	if (bCooked && !IsTemplate() && !Ar.IsCountingMemory())
	{
		Resources.Serialize(Ar, this, bCooked);
	}
}

void USparseVolumeTextureFrame::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
}

#if WITH_EDITOR

void USparseVolumeTextureFrame::BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
}

bool USparseVolumeTextureFrame::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	bool bFailed = false;
	if (!Resources.RebuildBulkDataFromCacheAsync(this, bFailed))
	{
		return false;
	}

	if (bFailed)
	{
		UE_LOG(LogSparseVolumeTexture, Log, TEXT("Failed to recover SparseVolumeTexture streaming from DDC for '%s' Frame %i. Rebuilding and retrying."), *Owner->GetPathName(), FrameIndex);

		Resources.Cache(this, SourceData, CastChecked<UStreamableSparseVolumeTexture>(Owner)->bLocalDDCOnly);
		return false;
	}

	return true;
}

void USparseVolumeTextureFrame::WillNeverCacheCookedPlatformDataAgain()
{
}

void USparseVolumeTextureFrame::ClearCachedCookedPlatformData(const ITargetPlatform* TargetPlatform)
{
	Resources.DropBulkData();
}

void USparseVolumeTextureFrame::ClearAllCachedCookedPlatformData()
{
	Resources.DropBulkData();
}

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
void USparseVolumeTextureFrame::Cache(bool bSkipDDCAndSetResourcesToDefault)
{
	if (bSkipDDCAndSetResourcesToDefault)
	{
		Resources.SetDefault(GetFormat(0), GetFormat(1), GetFallbackValue(0), GetFallbackValue(1));
	}
	else
	{
		Resources.Cache(this, SourceData, CastChecked<UStreamableSparseVolumeTexture>(Owner)->bLocalDDCOnly);
	}
	CreateTextureRenderResources();
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

UStreamableSparseVolumeTexture::UStreamableSparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStreamableSparseVolumeTexture::BeginInitialize(int32 NumExpectedFrames)
{
#if WITH_EDITORONLY_DATA
	if (InitState != EInitState_Uninitialized)
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to call UStreamableSparseVolumeTexture::BeginInitialize() while not in the Uninitialized init state."));
		return false;
	}

	check(Frames.IsEmpty());
	Frames.Empty(NumExpectedFrames);
	VolumeBoundsMin = FIntVector(INT32_MAX, INT32_MAX, INT32_MAX);
	VolumeBoundsMax = FIntVector(INT32_MIN, INT32_MIN, INT32_MIN);
	check(FormatA == PF_Unknown);
	check(FormatB == PF_Unknown);

	InitState = EInitState_Pending;

	return true;
#else
	return false;
#endif
}

bool UStreamableSparseVolumeTexture::AppendFrame(UE::SVT::FTextureData& UncookedFrame)
{
#if WITH_EDITORONLY_DATA
	if (InitState != EInitState_Pending)
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to call UStreamableSparseVolumeTexture::AppendFrame() while not in the Pending init state."));
		return false;
	}

	// The the minimum of the union of all frame AABBs should ideally be at (0, 0, 0), but it should also be fine if it is greater than that.
	// A mimimum of less than (0, 0, 0) is not permitted.
	if (UncookedFrame.Header.VirtualVolumeAABBMin.X < 0 || UncookedFrame.Header.VirtualVolumeAABBMin.Y < 0 || UncookedFrame.Header.VirtualVolumeAABBMin.Z < 0)
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to add a frame to a SparseVolumeTexture with a VirtualVolumeAABBMin < 0 (%i, %i, %i)"),
			VolumeBoundsMin.X, VolumeBoundsMin.Y, VolumeBoundsMin.Z);
		return false;
	}

	// SVT_TODO: Valide formats against list of supported formats
	if (Frames.IsEmpty())
	{
		FormatA = UncookedFrame.Header.AttributesFormats[0];
		FormatB = UncookedFrame.Header.AttributesFormats[1];
		FallbackValueA = UncookedFrame.Header.FallbackValues[0];
		FallbackValueB = UncookedFrame.Header.FallbackValues[1];
	}
	else
	{
		if (UncookedFrame.Header.AttributesFormats[0] != FormatA || UncookedFrame.Header.AttributesFormats[1] != FormatB)
		{
			UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to add a frame to a SparseVolumeTexture without matching formats! Expected: (%i, %i), Actual: (%i %i)"),
				(int)FormatA.GetIntValue(), (int)FormatB.GetIntValue(), (int)UncookedFrame.Header.AttributesFormats[0], (int)UncookedFrame.Header.AttributesFormats[1]);
			return false;
		}
		if (UncookedFrame.Header.FallbackValues[0] != FallbackValueA || UncookedFrame.Header.FallbackValues[1] != FallbackValueB)
		{
			UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to add a frame to a SparseVolumeTexture without matching fallback/null tile values!"));
			return false;
		}
	}

	// Compute union of all frame AABBs
	VolumeBoundsMin.X = FMath::Min(VolumeBoundsMin.X, UncookedFrame.Header.VirtualVolumeAABBMin.X);
	VolumeBoundsMin.Y = FMath::Min(VolumeBoundsMin.Y, UncookedFrame.Header.VirtualVolumeAABBMin.Y);
	VolumeBoundsMin.Z = FMath::Min(VolumeBoundsMin.Z, UncookedFrame.Header.VirtualVolumeAABBMin.Z);
	VolumeBoundsMax.X = FMath::Max(VolumeBoundsMax.X, UncookedFrame.Header.VirtualVolumeAABBMax.X);
	VolumeBoundsMax.Y = FMath::Max(VolumeBoundsMax.Y, UncookedFrame.Header.VirtualVolumeAABBMax.Y);
	VolumeBoundsMax.Z = FMath::Max(VolumeBoundsMax.Z, UncookedFrame.Header.VirtualVolumeAABBMax.Z);

	if (VolumeBoundsMin.X >= VolumeBoundsMax.X || VolumeBoundsMin.Y >= VolumeBoundsMax.Y || VolumeBoundsMin.Z >= VolumeBoundsMax.Z)
	{
		// Force a minimum resolution of (1, 1, 1) if the frames were empty so far
		VolumeResolution = FIntVector3(1, 1, 1);
	}
	else
	{
		VolumeResolution = VolumeBoundsMax;
	}
	

	USparseVolumeTextureFrame* Frame = NewObject<USparseVolumeTextureFrame>(this);
	if (Frame->Initialize(this, Frames.Num(), UncookedFrame))
	{
		Frames.Add(Frame);
		return true;
	}
	return false;
	
#else
	return false;
#endif
}

bool UStreamableSparseVolumeTexture::EndInitialize()
{
#if WITH_EDITORONLY_DATA
	if (InitState != EInitState_Pending)
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to call UStreamableSparseVolumeTexture::EndInitialize() while not in the Pending init state."));
		return false;
	}

	// Ensure that at least one frame of data exists
	if (Frames.IsEmpty())
	{
		UE_LOG(LogSparseVolumeTexture, Warning, TEXT("SVT has zero frames! Adding a dummy frame. SVT: %s"), *GetName());
		UE::SVT::FTextureData DummyFrame;
		DummyFrame.CreateDefault();
		AppendFrame(DummyFrame);
	}

	check(VolumeResolution.X > 0 && VolumeResolution.Y > 0 && VolumeResolution.Z > 0);
	check(VolumeBoundsMin.X >= 0 && VolumeBoundsMin.Y >= 0 && VolumeBoundsMin.Z >= 0);
	check(FormatA != PF_Unknown || FormatB != PF_Unknown);

	if (VolumeBoundsMin.X > 0 || VolumeBoundsMin.Y > 0 || VolumeBoundsMin.Z > 0)
	{
		UE_LOG(LogSparseVolumeTexture, Warning, TEXT("Initialized a SparseVolumeTexture with a VirtualVolumeAABBMin > 0 (%i, %i, %i). This wastes memory"),
			VolumeBoundsMin.X, VolumeBoundsMin.Y, VolumeBoundsMin.Z);
	}

	const int32 NumMipLevelsFullMipChain = UE::SVT::ComputeNumMipLevels(VolumeBoundsMin, VolumeBoundsMax);
	check(NumMipLevelsFullMipChain > 0);

	NumMipLevels = NumMipLevelsFullMipChain;
	NumFrames = Frames.Num();

	for (USparseVolumeTextureFrame* Frame : Frames)
	{
		Frame->PostLoad();
	}

	InitState = EInitState_Done;

	return true;
#else
	return false;
#endif
}

bool UStreamableSparseVolumeTexture::Initialize(const TArrayView<UE::SVT::FTextureData>& InUncookedData)
{
	if (InUncookedData.IsEmpty())
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to initialize a SparseVolumeTexture with no frames"));
		return false;
	}

	if (!BeginInitialize(InUncookedData.Num()))
	{
		return false;
	}
	for (UE::SVT::FTextureData& UncookedFrame : InUncookedData)
	{
		if (!AppendFrame(UncookedFrame))
		{
			return false;
		}
	}
	if (!EndInitialize())
	{
		return false;
	}

	return true;
}

void UStreamableSparseVolumeTexture::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITORONLY_DATA
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		AssetImportData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	}
#endif
}

void UStreamableSparseVolumeTexture::PostLoad()
{
	Super::PostLoad();

	// Ensure that NumFrames always corresponds to the actual number of frames
	NumFrames = GetNumFrames();

#if WITH_EDITORONLY_DATA
	RecacheFrames();
#else
	for (USparseVolumeTextureFrame* Frame : Frames)
	{
		Frame->CreateTextureRenderResources();
	}
	UE::SVT::GetStreamingManager().Add_GameThread(this); // RecacheFrames() handles this in editor builds
#endif
}

void UStreamableSparseVolumeTexture::FinishDestroy()
{
	Super::FinishDestroy();
}

void UStreamableSparseVolumeTexture::BeginDestroy()
{
	Super::BeginDestroy();
	UE::SVT::GetStreamingManager().Remove_GameThread(this);
}

void UStreamableSparseVolumeTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	// Check that we are not trying to cook unitialized data!
	check(!Ar.IsCooking() || InitState == EInitState_Done);
#endif
}

#if WITH_EDITOR
void UStreamableSparseVolumeTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UStreamableSparseVolumeTexture, AddressX)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UStreamableSparseVolumeTexture, AddressY)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UStreamableSparseVolumeTexture, AddressZ))
	{
		// SVT need to recompile shaders when address mode changes
		NotifyMaterials();
		for (USparseVolumeTextureFrame* Frame : Frames)
		{
			Frame->NotifyMaterials();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);

	RecacheFrames();
}
#endif // WITH_EDITOR

void UStreamableSparseVolumeTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	SIZE_T SizeCPU = sizeof(*this) - sizeof(Super);
	SIZE_T SizeGPU = 0;
	SizeCPU += Frames.GetAllocatedSize();
	for (USparseVolumeTextureFrame* Frame : Frames)
	{
		Frame->GetResourceSizeEx(CumulativeResourceSize);
	}
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(SizeCPU);
	CumulativeResourceSize.AddDedicatedVideoMemoryBytes(SizeGPU);
}

void UStreamableSparseVolumeTexture::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

#if WITH_EDITORONLY_DATA
	if (AssetImportData)
	{
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden));
	}
#endif
}

#if WITH_EDITORONLY_DATA
void UStreamableSparseVolumeTexture::RecacheFrames()
{
	if (InitState != EInitState_Done)
	{
		UE_LOG(LogSparseVolumeTexture, Warning, TEXT("Tried to cache derived data of an uninitialized SVT: %s"), *GetName());
		return;
	}

	FScopedSlowTask RecacheTask(static_cast<float>(Frames.Num() + 2), LOCTEXT("SparseVolumeTextureCacheFrames", "Caching SparseVolumeTexture frames in Derived Data Cache"));
	RecacheTask.MakeDialog(true);

	UE::SVT::GetStreamingManager().Remove_GameThread(this);
	RecacheTask.EnterProgressFrame(1.0f);

	bool bCanceled = false;
	for (USparseVolumeTextureFrame* Frame : Frames)
	{
		if (!bCanceled && RecacheTask.ShouldCancel())
		{
			bCanceled = true;
			UE_LOG(LogSparseVolumeTexture, Warning, TEXT("Canceled '%s' SparseVolumeTexture caching. All frames not yet cached will be empty!"), *GetName());
		}
		if (!bCanceled)
		{
			RecacheTask.EnterProgressFrame(1.0f);
		}
		
		// Even when the user cancels the caching, we still need valid data, so skip any DDC interactions and any actual building and instead just set the FResources
		// of the frame to (valid) empty/default data.
		const bool bSkipDDCAndSetResourcesToDefault = bCanceled;
		Frame->Cache(bSkipDDCAndSetResourcesToDefault);
	}

	if (!bCanceled)
	{
		RecacheTask.EnterProgressFrame(1.0f);
	}
	
	UE::SVT::GetStreamingManager().Add_GameThread(this);
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////

UStaticSparseVolumeTexture::UStaticSparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UStaticSparseVolumeTexture::AppendFrame(UE::SVT::FTextureData& UncookedFrame)
{
	if (!Frames.IsEmpty())
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("Tried to initialize a UStaticSparseVolumeTexture with more than 1 frame"));
		return false;
	}
	return Super::AppendFrame(UncookedFrame);
}

////////////////////////////////////////////////////////////////////////////////////////////////

UAnimatedSparseVolumeTexture::UAnimatedSparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

UAnimatedSparseVolumeTextureController::UAnimatedSparseVolumeTextureController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimatedSparseVolumeTextureController::Play()
{
	bIsPlaying = true;
}

void UAnimatedSparseVolumeTextureController::Pause()
{
	bIsPlaying = false;
}

void UAnimatedSparseVolumeTextureController::Stop()
{
	if (bIsPlaying)
	{
		bIsPlaying = false;
		Time = 0.0f;
	}
}

void UAnimatedSparseVolumeTextureController::Update(float DeltaTime)
{
	if (!IsValid(SparseVolumeTexture) || !bIsPlaying)
	{
		return;
	}

	// Update animation time
	const float AnimationDuration = GetDuration();
	Time = FMath::Fmod(Time + DeltaTime, AnimationDuration + UE_SMALL_NUMBER);
}

float UAnimatedSparseVolumeTextureController::GetFractionalFrameIndex()
{
	if (!IsValid(SparseVolumeTexture))
	{
		return 0.0f;
	}

	const int32 FrameCount = SparseVolumeTexture->GetNumFrames();
	const float FrameIndexF = FMath::Fmod(Time * FrameRate, (float)FrameCount);
	return FrameIndexF;
}

USparseVolumeTextureFrame* UAnimatedSparseVolumeTextureController::GetFrameByIndex(int32 FrameIndex)
{
	if (!IsValid(SparseVolumeTexture))
	{
		return nullptr;
	}

	return USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(SparseVolumeTexture, FrameIndex, MipLevel, bBlockingStreamingRequests);
}

USparseVolumeTextureFrame* UAnimatedSparseVolumeTextureController::GetCurrentFrame()
{
	if (!IsValid(SparseVolumeTexture))
	{
		return nullptr;
	}

	// Compute (fractional) index of frame to sample
	const float FrameIndexF = GetFractionalFrameIndex();

	return USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(SparseVolumeTexture, FrameIndexF, MipLevel, bBlockingStreamingRequests);
}

void UAnimatedSparseVolumeTextureController::GetCurrentFramesForInterpolation(USparseVolumeTextureFrame*& Frame0, USparseVolumeTextureFrame*& Frame1, float& LerpAlpha)
{
	if (!IsValid(SparseVolumeTexture))
	{
		return;
	}

	// Compute (fractional) index of frame to sample
	const float FrameIndexF = GetFractionalFrameIndex();
	const int32 FrameIndex = (int32)FrameIndexF;
	LerpAlpha = FMath::Frac(FrameIndexF);

	Frame0 = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(SparseVolumeTexture, FrameIndexF, MipLevel, bBlockingStreamingRequests);
	Frame1 = USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest(SparseVolumeTexture, (FrameIndex + 1) % SparseVolumeTexture->GetNumFrames(), MipLevel, bBlockingStreamingRequests);
}

float UAnimatedSparseVolumeTextureController::GetDuration()
{
	if (!IsValid(SparseVolumeTexture))
	{
		return 0.0f;
	}

	const int32 FrameCount = SparseVolumeTexture->GetNumFrames();
	const float AnimationDuration = FrameCount / (FrameRate + UE_SMALL_NUMBER);
	return AnimationDuration;
}

////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
