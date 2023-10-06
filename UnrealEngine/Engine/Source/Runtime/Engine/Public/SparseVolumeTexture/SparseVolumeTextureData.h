// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"

namespace UE
{
namespace SVT
{

struct FTextureDataCreateInfo
{
	FIntVector3 VirtualVolumeAABBMin = FIntVector3(INT32_MAX, INT32_MAX, INT32_MAX);
	FIntVector3 VirtualVolumeAABBMax = FIntVector3(INT32_MIN, INT32_MIN, INT32_MIN);
	TStaticArray<EPixelFormat, 2> AttributesFormats = TStaticArray<EPixelFormat, 2>(InPlace, PF_Unknown); // See UE::SVT::IsSupportedFormat(EPixelFormat Format) for list of supported formats
	TStaticArray<FVector4f, 2> FallbackValues = TStaticArray<FVector4f, 2>(InPlace, FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
};

class ITextureDataProvider
{
public:
	virtual FTextureDataCreateInfo GetCreateInfo() const = 0;
	virtual void IteratePhysicalSource(TFunctionRef<void(const FIntVector3& Coord, int32 AttributesIdx, int32 ComponentIdx, float VoxelValue)> OnVisit) const = 0;
	virtual ~ITextureDataProvider() = default;
};

struct FTextureDataAddressingInfo
{
	FIntVector3 VolumeResolution;
	TextureAddress AddressX;
	TextureAddress AddressY;
	TextureAddress AddressZ;
};

// Holds the data for a SparseVolumeTexture that is stored on disk. It only has a single mip after importing a source asset. The mip chain is built during cooking.
// Tiles are addressed by a flat index; unlike the runtime representation, this one stores all tiles in a 1D array (per mip level) and doesn't have the concept of a 3D physical tile texture.
// The page table itself is 3D though.
struct FTextureData
{
	struct FMipMap
	{
		TArray<uint32> PageTable;
		TArray64<uint8> PhysicalTileDataA;
		TArray64<uint8> PhysicalTileDataB;
		int32 NumPhysicalTiles;
	};

	FHeader Header = {};
	TStaticArray<FVector4f, 2> FallbackValuesQuantized = TStaticArray<FVector4f, 2>(InPlace, FVector4f(0.0f, 0.0f, 0.0f, 0.0f));
	TArray<FMipMap> MipMaps;

	ENGINE_API bool Create(const ITextureDataProvider& DataProvider);
	ENGINE_API bool CreateFromDense(const FTextureDataCreateInfo& CreateInfo, const TArrayView64<uint8>& VoxelDataA, const TArrayView64<uint8>& VoxelDataB);
	ENGINE_API void CreateDefault();
	ENGINE_API FVector4f Load(const FIntVector3& VolumeCoord, int32 MipLevel, int32 AttributesIdx, const FTextureDataAddressingInfo& AddressingInfo) const;
	ENGINE_API bool BuildDerivedData(const FTextureDataAddressingInfo& AddressingInfo, int32 NumMipLevelsGlobal, bool bMoveMip0FromThis, FTextureData& OutDerivedData);

private:

	bool GenerateMipMaps(const FTextureDataAddressingInfo& AddressingInfo, int32 NumMipLevels = -1);
	bool GenerateBorderVoxels(const FTextureDataAddressingInfo& AddressingInfo, int32 MipLevel, const TArray<FIntVector3>& PageCoords);
	bool DeduplicateTiles();

	uint32 ReadPageTable(const FIntVector3& PageTableCoord, int32 MipLevel) const;
	FVector4f ReadTileDataVoxel(int32 TileIndex, const FIntVector3& TileDataCoord, int32 MipLevel, int32 AttributesIdx) const;
	void WriteTileDataVoxel(int32 TileIndex, const FIntVector3& TileDataCoord, int32 MipLevel, int32 AttributesIdx, const FVector4f& Value, int32 DstComponent = -1);
};

}
}

FArchive& operator<<(FArchive& Ar, UE::SVT::FTextureData& TextureData);
