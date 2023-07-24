// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "SparseVolumeTextureOpenVDBUtility.h"
#include "SparseVolumeTextureOpenVDB.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "OpenVDBGridAdapter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSparseVolumeTextureOpenVDBUtility, Log, All);

#if OPENVDB_AVAILABLE

// We are using this instead of GMaxVolumeTextureDimensions to be independent of the platform that
// the asset is imported on. 2048 should be a safe value that should be supported by all our platforms.
static constexpr int32 SVTMaxVolumeTextureDim = 2048;

namespace
{
	// Utility class acting as adapter between TArray<uint8> and std::istream
	class FArrayUint8StreamBuf : public std::streambuf
	{
	public:
		explicit FArrayUint8StreamBuf(TArray<uint8>& Array)
		{
			char* Data = (char*)Array.GetData();
			setg(Data, Data, Data + Array.Num());
		}
	};
}

static FOpenVDBGridInfo GetOpenVDBGridInfo(openvdb::GridBase::Ptr Grid, uint32 GridIndex, bool bCreateStrings)
{
	openvdb::CoordBBox VolumeActiveAABB = Grid->evalActiveVoxelBoundingBox();
	openvdb::Coord VolumeActiveDim = Grid->evalActiveVoxelDim();
	openvdb::Vec3d VolumeVoxelSize = Grid->voxelSize();
	openvdb::math::MapBase::ConstPtr MapBase = Grid->constTransform().baseMap();
	openvdb::Vec3d VoxelSize = MapBase->voxelSize();
	openvdb::Mat4d GridTransformVDB = MapBase->getAffineMap()->getConstMat4();

	FOpenVDBGridInfo GridInfo;
	GridInfo.Index = GridIndex;
	GridInfo.NumComponents = 0;
	GridInfo.Type = EOpenVDBGridType::Unknown;
	GridInfo.VolumeActiveAABBMin = FVector(VolumeActiveAABB.min().x(), VolumeActiveAABB.min().y(), VolumeActiveAABB.min().z());
	GridInfo.VolumeActiveAABBMax = FVector(VolumeActiveAABB.max().x(), VolumeActiveAABB.max().y(), VolumeActiveAABB.max().z());
	GridInfo.VolumeActiveDim = FVector(VolumeActiveDim.x(), VolumeActiveDim.y(), VolumeActiveDim.z());
	GridInfo.VolumeVoxelSize = FVector(VolumeVoxelSize.x(), VolumeVoxelSize.y(), VolumeVoxelSize.z());
	GridInfo.bIsInWorldSpace = Grid->isInWorldSpace();
	GridInfo.bHasUniformVoxels = Grid->hasUniformVoxels();

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			GridInfo.Transform.M[i][j] = static_cast<float>(GridTransformVDB[j][i]);
		}
	}

	// Figure out the type/format of the grid
	if (Grid->isType<FOpenVDBFloat1Grid>())
	{
		GridInfo.NumComponents = 1;
		GridInfo.Type = EOpenVDBGridType::Float;
	}
	else if (Grid->isType<FOpenVDBFloat2Grid>())
	{
		GridInfo.NumComponents = 2;
		GridInfo.Type = EOpenVDBGridType::Float2;
	}
	else if (Grid->isType<FOpenVDBFloat3Grid>())
	{
		GridInfo.NumComponents = 3;
		GridInfo.Type = EOpenVDBGridType::Float3;
	}
	else if (Grid->isType<FOpenVDBFloat4Grid>())
	{
		GridInfo.NumComponents = 4;
		GridInfo.Type = EOpenVDBGridType::Float4;
	}
	else if (Grid->isType<FOpenVDBDouble1Grid>())
	{
		GridInfo.NumComponents = 1;
		GridInfo.Type = EOpenVDBGridType::Double;
	}
	else if (Grid->isType<FOpenVDBDouble2Grid>())
	{
		GridInfo.NumComponents = 2;
		GridInfo.Type = EOpenVDBGridType::Double2;
	}
	else if (Grid->isType<FOpenVDBDouble3Grid>())
	{
		GridInfo.NumComponents = 3;
		GridInfo.Type = EOpenVDBGridType::Double3;
	}
	else if (Grid->isType<FOpenVDBDouble4Grid>())
	{
		GridInfo.NumComponents = 4;
		GridInfo.Type = EOpenVDBGridType::Double4;
	}

	if (bCreateStrings)
	{
		GridInfo.Name = Grid->getName().c_str();

		FStringFormatOrderedArguments FormatArgs;
		FormatArgs.Add(GridInfo.Index);
		FormatArgs.Add(OpenVDBGridTypeToString(GridInfo.Type));
		FormatArgs.Add(GridInfo.Name);

		GridInfo.DisplayString = FString::Format(TEXT("{0}. Type: {1}, Name: \"{2}\""), FormatArgs);
	}

	return GridInfo;
}

#endif

bool IsOpenVDBGridValid(const FOpenVDBGridInfo& GridInfo, const FString& Filename)
{
	if (GridInfo.VolumeActiveDim.X * GridInfo.VolumeActiveDim.Y * GridInfo.VolumeActiveDim.Z == 0)
	{
		// SVT_TODO we should gently handle that case
		UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("OpenVDB grid is empty due to volume size being 0: %s"), *Filename);
		return false;
	}

	if (!GridInfo.bHasUniformVoxels)
	{
		UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("OpenVDB importer cannot handle non uniform voxels: %s"), *Filename);
		return false;
	}
	return true;
}

bool GetOpenVDBGridInfo(TArray<uint8>& SourceFile, bool bCreateStrings, TArray<FOpenVDBGridInfo>* OutGridInfo)
{
#if OPENVDB_AVAILABLE
	FArrayUint8StreamBuf StreamBuf(SourceFile);
	std::istream IStream(&StreamBuf);
	openvdb::io::Stream Stream(IStream, false /*delayLoad*/);

	openvdb::GridPtrVecPtr Grids = Stream.getGrids();

	OutGridInfo->Empty(Grids->size());

	uint32 GridIndex = 0;
	for (openvdb::GridBase::Ptr& Grid : *Grids)
	{
		OutGridInfo->Add(GetOpenVDBGridInfo(Grid, GridIndex, bCreateStrings));
		++GridIndex;
	}

	return true;

#endif // OPENVDB_AVAILABLE
	return false;
}

static EPixelFormat GetMultiComponentFormat(ESparseVolumePackedDataFormat Format, uint32 NumComponents)
{
	switch (Format)
	{
	case ESparseVolumePackedDataFormat::Unorm8:
	{
		switch (NumComponents)
		{
		case 1: return PF_R8;
		case 2: return PF_R8G8;
		case 3:
		case 4: return PF_R8G8B8A8;
		}
		break;
	}
	case ESparseVolumePackedDataFormat::Float16:
	{
		switch (NumComponents)
		{
		case 1: return PF_R16F;
		case 2: return PF_G16R16F;
		case 3:
		case 4: return PF_FloatRGBA;
		}
		break;
	}
	case ESparseVolumePackedDataFormat::Float32:
	{
		switch (NumComponents)
		{
		case 1: return PF_R32_FLOAT;
		case 2: return PF_G32R32F;
		case 3:
		case 4: return PF_A32B32G32R32F;
		}
		break;
	}
	}
	return PF_Unknown;
}

static uint32 PackPageTableEntry(const FIntVector3& Coord)
{
	// A page encodes the physical tile coord as unsigned int of 11 11 10 bits
	// This means a page coord cannot be larger than 2047 for x and y and 1023 for z
	// which mean we cannot have more than 2048*2048*1024 = 4 Giga tiles of 16^3 tiles.
	uint32 Result = (Coord.X & 0x7FF) | ((Coord.Y & 0x7FF) << 11) | ((Coord.Z & 0x3FF) << 22);
	return Result;
}

static FIntVector3 UnpackPageTableEntry(uint32 Packed)
{
	FIntVector3 Result;
	Result.X = Packed & 0x7FF;
	Result.Y = (Packed >> 11) & 0x7FF;
	Result.Z = (Packed >> 22) & 0x3FF;
	return Result;
}

bool ConvertOpenVDBToSparseVolumeTexture(
	TArray<uint8>& SourceFile,
	FSparseVolumeRawSourcePackedData& PackedDataA,
	FSparseVolumeRawSourcePackedData& PackedDataB,
	FOpenVDBToSVTConversionResult* OutResult,
	bool bOverrideActiveMinMax,
	FVector ActiveMin,
	FVector ActiveMax)
{
#if OPENVDB_AVAILABLE

	constexpr uint32 NumPackedData = 2; // PackedDataA and PackedDataB, representing the two textures with voxel data
	FSparseVolumeRawSourcePackedData* PackedData[NumPackedData] = { &PackedDataA, &PackedDataB };

	// Compute some basic info about the number of components and which format to use
	uint32 NumActualComponents[NumPackedData] = {};
	EPixelFormat MultiCompFormat[NumPackedData] = {};
	uint32 FormatSize[NumPackedData] = {};
	uint32 SingleComponentFormatSize[NumPackedData] = {};
	bool bNormalizedFormat[NumPackedData] = {};
	bool bHasValidSourceGrids[NumPackedData] = {};
	bool bAnySourceGridIndicesValid = false;

	for (uint32 PackedDataIdx = 0; PackedDataIdx < NumPackedData; ++PackedDataIdx)
	{
		uint32 NumRequiredComponents = 0;
		for (uint32 ComponentIdx = 0; ComponentIdx < 4; ++ComponentIdx)
		{
			if (PackedData[PackedDataIdx]->SourceGridIndex[ComponentIdx] != INDEX_NONE)
			{
				check(PackedData[PackedDataIdx]->SourceComponentIndex[ComponentIdx] != INDEX_NONE);
				NumRequiredComponents = FMath::Max(ComponentIdx + 1, NumRequiredComponents);
				bHasValidSourceGrids[PackedDataIdx] = true;
				bAnySourceGridIndicesValid = true;
			}
		}

		if (bHasValidSourceGrids[PackedDataIdx])
		{
			NumActualComponents[PackedDataIdx] = NumRequiredComponents == 3 ? 4 : NumRequiredComponents; // We don't support formats with only 3 components
			bNormalizedFormat[PackedDataIdx] = PackedData[PackedDataIdx]->Format == ESparseVolumePackedDataFormat::Unorm8;
			MultiCompFormat[PackedDataIdx] = GetMultiComponentFormat(PackedData[PackedDataIdx]->Format, NumActualComponents[PackedDataIdx]);

			if (MultiCompFormat[PackedDataIdx] == PF_Unknown)
			{
				UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("SparseVolumeTexture is set to use an unsupported format: %i"), (int32)PackedData[PackedDataIdx]->Format);
				return false;
			}

			FormatSize[PackedDataIdx] = (uint32)GPixelFormats[(SIZE_T)MultiCompFormat[PackedDataIdx]].BlockBytes;
			SingleComponentFormatSize[PackedDataIdx] = FormatSize[PackedDataIdx] / NumActualComponents[PackedDataIdx];
		}
	}

	// All source grid indices are INDEX_NONE, so nothing was selected for import
	if (!bAnySourceGridIndicesValid)
	{
		UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("SparseVolumeTexture has all components set to <None>, so there is nothing to import."));
		return false;
	}

	// Load file
	FArrayUint8StreamBuf StreamBuf(SourceFile);
	std::istream IStream(&StreamBuf);
	openvdb::io::Stream Stream(IStream, false /*delayLoad*/);

	// Check that the source grid indices are valid
	openvdb::GridPtrVecPtr Grids = Stream.getGrids();
	const size_t NumSourceGrids = Grids ? Grids->size() : 0;
	for (uint32 PackedDataIdx = 0; PackedDataIdx < NumPackedData; ++PackedDataIdx)
	{
		for (uint32 CompIdx = 0; CompIdx < 4; ++CompIdx)
		{
			const uint32 SourceGridIndex = PackedData[PackedDataIdx]->SourceGridIndex[CompIdx];
			if (SourceGridIndex != INDEX_NONE && SourceGridIndex >= NumSourceGrids)
			{
				UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("SparseVolumeTexture has invalid index into the array of grids in the source file: %i"), (int32)SourceGridIndex);
				return false;
			}
		}
	}

	FSparseVolumeAssetHeader& Header = *OutResult->Header;
	Header.PackedDataAFormat = MultiCompFormat[0];
	Header.PackedDataBFormat = MultiCompFormat[1];
	Header.SourceVolumeResolution = FIntVector::ZeroValue;
	
	FIntVector SmallestAABBMin = FIntVector(INT32_MAX);

	// Compute per source grid data of up to 4 different grids (one per component)
	TArray<TSharedPtr<IOpenVDBGridAdapterBase>> UniqueGridAdapters;
	UniqueGridAdapters.SetNum((int32)NumSourceGrids);
	TSharedPtr<IOpenVDBGridAdapterBase> GridAdapters[NumPackedData][4]{};
	float GridBackgroundValues[NumPackedData][4]{};
	float NormalizeScale[NumPackedData][4]{};
	float NormalizeBias[NumPackedData][4]{};
	for (uint32 PackedDataIdx = 0; PackedDataIdx < NumPackedData; ++PackedDataIdx)
	{
		for (uint32 CompIdx = 0; CompIdx < 4; ++CompIdx)
		{
			NormalizeScale[PackedDataIdx][CompIdx] = 1.0f;
			const uint32 SourceGridIndex = PackedData[PackedDataIdx]->SourceGridIndex[CompIdx];
			const uint32 SourceComponentIndex = PackedData[PackedDataIdx]->SourceComponentIndex[CompIdx];
			if (SourceGridIndex == INDEX_NONE)
			{
				continue;
			}

			openvdb::GridBase::Ptr GridBase = (*Grids)[SourceGridIndex];

			// Try to reuse adapters. Internally they use caching to accelerate read accesses, 
			// so using three different adapters to access the three components of a single grid would be wasteful.
			if (UniqueGridAdapters[SourceGridIndex] == nullptr)
			{
				UniqueGridAdapters[SourceGridIndex] = CreateOpenVDBGridAdapter(GridBase);
				if (!UniqueGridAdapters[SourceGridIndex])
				{
					return false;
				}
			}

			GridAdapters[PackedDataIdx][CompIdx] = UniqueGridAdapters[SourceGridIndex];

			FOpenVDBGridInfo GridInfo = GetOpenVDBGridInfo(GridBase, 0, false);
			if (!IsOpenVDBGridValid(GridInfo, TEXT("")))
			{
				return false;
			}

			if (bOverrideActiveMinMax)
			{
				GridInfo.VolumeActiveAABBMin = ActiveMin;
				GridInfo.VolumeActiveAABBMax = ActiveMax;
				GridInfo.VolumeActiveDim = ActiveMax + 1 - ActiveMin;
			}

			Header.SourceVolumeResolution.X = FMath::Max(Header.SourceVolumeResolution.X, GridInfo.VolumeActiveDim.X);
			Header.SourceVolumeResolution.Y = FMath::Max(Header.SourceVolumeResolution.Y, GridInfo.VolumeActiveDim.Y);
			Header.SourceVolumeResolution.Z = FMath::Max(Header.SourceVolumeResolution.Z, GridInfo.VolumeActiveDim.Z);
			SmallestAABBMin.X = FMath::Min(SmallestAABBMin.X, GridInfo.VolumeActiveAABBMin.X);
			SmallestAABBMin.Y = FMath::Min(SmallestAABBMin.Y, GridInfo.VolumeActiveAABBMin.Y);
			SmallestAABBMin.Z = FMath::Min(SmallestAABBMin.Z, GridInfo.VolumeActiveAABBMin.Z);

			GridBackgroundValues[PackedDataIdx][CompIdx] = GridAdapters[PackedDataIdx][CompIdx]->GetBackgroundValue(SourceComponentIndex);
			if (bNormalizedFormat[PackedDataIdx] && PackedData[PackedDataIdx]->bRemapInputForUnorm)
			{
				float MinVal = 0.0f;
				float MaxVal = 0.0f;
				GridAdapters[PackedDataIdx][CompIdx]->GetMinMaxValue(SourceComponentIndex, &MinVal, &MaxVal);
				const float Diff = MaxVal - MinVal;
				NormalizeScale[PackedDataIdx][CompIdx] = MaxVal > SMALL_NUMBER ? (1.0f / Diff) : 1.0f;
				NormalizeBias[PackedDataIdx][CompIdx] = -MinVal * NormalizeScale[PackedDataIdx][CompIdx];
			}
		}
	}
	OutResult->Header->SourceVolumeAABBMin = SmallestAABBMin;
	
	FIntVector3 PageTableVolumeResolution = FIntVector3(
		FMath::DivideAndRoundUp(Header.SourceVolumeResolution.X, SPARSE_VOLUME_TILE_RES),
		FMath::DivideAndRoundUp(Header.SourceVolumeResolution.Y, SPARSE_VOLUME_TILE_RES),
		FMath::DivideAndRoundUp(Header.SourceVolumeResolution.Z, SPARSE_VOLUME_TILE_RES));
	if (PageTableVolumeResolution.X > SVTMaxVolumeTextureDim
		|| PageTableVolumeResolution.Y > SVTMaxVolumeTextureDim
		|| PageTableVolumeResolution.Z > SVTMaxVolumeTextureDim)
	{
		UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("SparseVolumeTexture page table texture dimensions exceed limit (%ix%ix%i): %ix%ix%i"), SVTMaxVolumeTextureDim, SVTMaxVolumeTextureDim, SVTMaxVolumeTextureDim, PageTableVolumeResolution.X, PageTableVolumeResolution.Y, PageTableVolumeResolution.Z);
		return false;
	}

	Header.PageTableVolumeResolution = PageTableVolumeResolution;
	Header.TileDataVolumeResolution = FIntVector::ZeroValue;	// unknown for now

	// Tag all pages with valid data
	TBitArray PagesWithData(false, Header.PageTableVolumeResolution.Z * Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.X);
	for (uint32 GridIdx = 0; GridIdx < NumSourceGrids; ++GridIdx)
	{
		if (!UniqueGridAdapters[GridIdx])
		{
			continue;
		}

		UniqueGridAdapters[GridIdx]->IteratePhysical(
			[&](const FIntVector3& Coord, uint32 NumComponents, float* VoxelValues)
			{
				// Check if the source grid component is used at all
				bool bValueIsUsed = false;
				for (uint32 PackedDataIdx = 0; PackedDataIdx < NumPackedData && !bValueIsUsed; ++PackedDataIdx)
				{
					for (uint32 DstCompIdx = 0; DstCompIdx < NumActualComponents[PackedDataIdx] && !bValueIsUsed; ++DstCompIdx)
					{
						for (uint32 SrcCompIdx = 0; SrcCompIdx < NumComponents && !bValueIsUsed; ++SrcCompIdx)
						{
							const bool bIsBackgroundValue = (VoxelValues[SrcCompIdx] == GridBackgroundValues[PackedDataIdx][DstCompIdx]);
							bValueIsUsed |= !bIsBackgroundValue && (PackedData[PackedDataIdx]->SourceGridIndex[DstCompIdx] == GridIdx) && (PackedData[PackedDataIdx]->SourceComponentIndex[DstCompIdx] == SrcCompIdx);
						}
					}
				}
				if (!bValueIsUsed)
				{
					return;
				}

				const FIntVector3 GridCoord = Coord - SmallestAABBMin;
				check(GridCoord.X >= 0 && GridCoord.Y >= 0 && GridCoord.Z >= 0);
				check(GridCoord.X < Header.SourceVolumeResolution.X && GridCoord.Y < Header.SourceVolumeResolution.Y && GridCoord.Z < Header.SourceVolumeResolution.Z);
				const FIntVector3 PageCoord = GridCoord / SPARSE_VOLUME_TILE_RES;
				check(PageCoord.X >= 0 && PageCoord.Y >= 0 && PageCoord.Z >= 0);
				check(PageCoord.X < Header.PageTableVolumeResolution.X && PageCoord.Y < Header.PageTableVolumeResolution.Y && PageCoord.Z < Header.PageTableVolumeResolution.Z);

				const int32 PageIndex = PageCoord.Z * (Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.X) + PageCoord.Y * Header.PageTableVolumeResolution.X + PageCoord.X;
				PagesWithData[PageIndex] = true;
			});
	}

	// Allocate some memory for temp data (worst case)
	TArray<FIntVector3> LinearAllocatedPages;
	LinearAllocatedPages.SetNum(Header.PageTableVolumeResolution.X * Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.Z);

	// Go over each potential page from the source data and push allocate it if it has any data.
	// Otherwise point to the default empty page.
	bool bAnyEmptyPageExists = false;
	uint32 NumAllocatedPages = 0;
	for (int32_t PageZ = 0; PageZ < Header.PageTableVolumeResolution.Z; ++PageZ)
	{
		for (int32_t PageY = 0; PageY < Header.PageTableVolumeResolution.Y; ++PageY)
		{
			for (int32_t PageX = 0; PageX < Header.PageTableVolumeResolution.X; ++PageX)
			{
				const int32 PageIndex = PageZ * (Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.X) + PageY * Header.PageTableVolumeResolution.X + PageX;
				bool bHasAnyData = PagesWithData[PageIndex];
				if (bHasAnyData)
				{
					LinearAllocatedPages[NumAllocatedPages] = FIntVector3(PageX, PageY, PageZ);
					NumAllocatedPages++;
				}
				bAnyEmptyPageExists |= !bHasAnyData;
			}
		}
	}

	// Compute Page and Tile VolumeResolution from allocated pages
	const uint32 EffectivelyAllocatedPageEntries = NumAllocatedPages + (bAnyEmptyPageExists ? 1 : 0);
	uint32 TileVolumeResolutionCube = 1;
	while (TileVolumeResolutionCube * TileVolumeResolutionCube * TileVolumeResolutionCube < EffectivelyAllocatedPageEntries)
	{
		TileVolumeResolutionCube++;				// We use a simple loop to compute the minimum resolution of a cube to store all the tile data
	}
	Header.TileDataVolumeResolution = FIntVector3(TileVolumeResolutionCube, TileVolumeResolutionCube, TileVolumeResolutionCube);
	while (Header.TileDataVolumeResolution.X * Header.TileDataVolumeResolution.Y * (Header.TileDataVolumeResolution.Z - 1) > int32(EffectivelyAllocatedPageEntries))
	{
		Header.TileDataVolumeResolution.Z--;	// We then trim an edge to get back space.
	}
	const FIntVector3 TileCoordResolution = Header.TileDataVolumeResolution;
	Header.TileDataVolumeResolution = Header.TileDataVolumeResolution * SPARSE_VOLUME_TILE_RES;
	if (Header.TileDataVolumeResolution.X > SVTMaxVolumeTextureDim
		|| Header.TileDataVolumeResolution.Y > SVTMaxVolumeTextureDim
		|| Header.TileDataVolumeResolution.Z > SVTMaxVolumeTextureDim)
	{
		UE_LOG(LogSparseVolumeTextureOpenVDBUtility, Warning, TEXT("SparseVolumeTexture tile data texture dimensions exceed limit (%ix%ix%i): %ix%ix%i"), SVTMaxVolumeTextureDim, SVTMaxVolumeTextureDim, SVTMaxVolumeTextureDim, Header.TileDataVolumeResolution.X, Header.TileDataVolumeResolution.Y, Header.TileDataVolumeResolution.Z);
		return false;
	}

	// Initialise the SparseVolumeTexture page and tile.
	OutResult->PageTable->SetNumZeroed(Header.PageTableVolumeResolution.X * Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.Z);
	OutResult->PhysicalTileDataA->SetNumZeroed(Header.TileDataVolumeResolution.X * Header.TileDataVolumeResolution.Y * Header.TileDataVolumeResolution.Z * FormatSize[0] * (bHasValidSourceGrids[0] ? 1 : 0));
	OutResult->PhysicalTileDataB->SetNumZeroed(Header.TileDataVolumeResolution.X * Header.TileDataVolumeResolution.Y * Header.TileDataVolumeResolution.Z * FormatSize[1] * (bHasValidSourceGrids[1] ? 1 : 0));
	uint32* PageTablePtr = OutResult->PageTable->GetData();
	uint8* PhysicalTileDataPtrs[] = { OutResult->PhysicalTileDataA->GetData(), OutResult->PhysicalTileDataB->GetData() };

	// Generate page table and tile volume data by splatting the data
	{
		FIntVector3 DstTileCoord = FIntVector3::ZeroValue;
		auto GoToNextTileCoord = [&]()
		{
			DstTileCoord.X++;
			if (DstTileCoord.X >= TileCoordResolution.X)
			{
				DstTileCoord.X = 0;
				DstTileCoord.Y++;
			}
			if (DstTileCoord.Y >= TileCoordResolution.Y)
			{
				DstTileCoord.Y = 0;
				DstTileCoord.Z++;
			}
		};

		// Add an empty tile is needed, reserve slot at coord 0
		if (bAnyEmptyPageExists)
		{
			// PageTable is all cleared to zero, simply skip a tile
			GoToNextTileCoord();
		}

		for (uint32 i = 0; i < NumAllocatedPages; ++i)
		{
			FIntVector3 PageCoordToSplat = LinearAllocatedPages[i];
			uint32 DestinationTileCoord32bit = PackPageTableEntry(DstTileCoord);

			// Setup the page table entry
			PageTablePtr
				[
					PageCoordToSplat.Z * Header.PageTableVolumeResolution.X * Header.PageTableVolumeResolution.Y +
					PageCoordToSplat.Y * Header.PageTableVolumeResolution.X +
				PageCoordToSplat.X
				] = DestinationTileCoord32bit;

			// Set the next tile to be written to
			GoToNextTileCoord();
		}
	}

	for (uint32 GridIdx = 0; GridIdx < NumSourceGrids; ++GridIdx)
	{
		if (!UniqueGridAdapters[GridIdx])
		{
			continue;
		}

		UniqueGridAdapters[GridIdx]->IteratePhysical(
			[&](const FIntVector3& Coord, uint32 NumComponents, float* VoxelValues)
			{
				const FIntVector3 GridCoord = Coord - SmallestAABBMin;
				check(GridCoord.X >= 0 && GridCoord.Y >= 0 && GridCoord.Z >= 0);
				check(GridCoord.X < Header.SourceVolumeResolution.X&& GridCoord.Y < Header.SourceVolumeResolution.Y&& GridCoord.Z < Header.SourceVolumeResolution.Z);
				const FIntVector3 PageCoord = GridCoord / SPARSE_VOLUME_TILE_RES;
				check(PageCoord.X >= 0 && PageCoord.Y >= 0 && PageCoord.Z >= 0);
				check(PageCoord.X < Header.PageTableVolumeResolution.X&& PageCoord.Y < Header.PageTableVolumeResolution.Y&& PageCoord.Z < Header.PageTableVolumeResolution.Z);

				const int32 PageIndex = PageCoord.Z * (Header.PageTableVolumeResolution.Y * Header.PageTableVolumeResolution.X) + PageCoord.Y * Header.PageTableVolumeResolution.X + PageCoord.X;

				if (!PagesWithData[PageIndex])
				{
					return;
				}

				const FIntVector3 DstTileCoord = UnpackPageTableEntry(PageTablePtr[PageIndex]);
				const FIntVector3 TileLocalCoord = GridCoord % SPARSE_VOLUME_TILE_RES;

				// Check all output components and splat VoxelValue if they map to this source grid/component
				for (uint32 PackedDataIdx = 0; PackedDataIdx < NumPackedData; ++PackedDataIdx)
				{
					for (uint32 DstCompIdx = 0; DstCompIdx < NumActualComponents[PackedDataIdx]; ++DstCompIdx)
					{
						for (uint32 SrcCompIdx = 0; SrcCompIdx < NumComponents; ++SrcCompIdx)
						{
							if ((PackedData[PackedDataIdx]->SourceGridIndex[DstCompIdx] != GridIdx) || (PackedData[PackedDataIdx]->SourceComponentIndex[DstCompIdx] != SrcCompIdx))
							{
								continue;
							}

							const float VoxelValueNormalized = FMath::Clamp(VoxelValues[SrcCompIdx] * NormalizeScale[PackedDataIdx][DstCompIdx] + NormalizeBias[PackedDataIdx][DstCompIdx], 0.0f, 1.0f);

							const size_t DstVoxelIndex =
								(DstTileCoord.Z * SPARSE_VOLUME_TILE_RES + TileLocalCoord.Z) * Header.TileDataVolumeResolution.X * Header.TileDataVolumeResolution.Y +
								(DstTileCoord.Y * SPARSE_VOLUME_TILE_RES + TileLocalCoord.Y) * Header.TileDataVolumeResolution.X +
								(DstTileCoord.X * SPARSE_VOLUME_TILE_RES + TileLocalCoord.X);
							const size_t DstCoord = DstVoxelIndex * FormatSize[PackedDataIdx] + DstCompIdx * SingleComponentFormatSize[PackedDataIdx];

							switch (PackedData[PackedDataIdx]->Format)
							{
							case ESparseVolumePackedDataFormat::Unorm8:
							{
								PhysicalTileDataPtrs[PackedDataIdx][DstCoord] = uint8(VoxelValueNormalized * 255.0f);
								break;
							}
							case ESparseVolumePackedDataFormat::Float16:
							{
								const uint16 VoxelValue16FEncoded = FFloat16(VoxelValues[SrcCompIdx]).Encoded;
								*((uint16*)(&PhysicalTileDataPtrs[PackedDataIdx][DstCoord])) = VoxelValue16FEncoded;
								break;
							}
							case ESparseVolumePackedDataFormat::Float32:
							{
								*((float*)(&PhysicalTileDataPtrs[PackedDataIdx][DstCoord])) = VoxelValues[SrcCompIdx];
								break;
							}
							default:
								checkNoEntry();
								break;
							}
						}
					}
				}
			});
	}

	return true;
#else
	return false;
#endif // OPENVDB_AVAILABLE
}

const TCHAR* OpenVDBGridTypeToString(EOpenVDBGridType Type)
{
	switch (Type)
	{
	case EOpenVDBGridType::Float:
		return TEXT("Float");
	case EOpenVDBGridType::Float2:
		return TEXT("Float2");
	case EOpenVDBGridType::Float3:
		return TEXT("Float3");
	case EOpenVDBGridType::Float4:
		return TEXT("Float4");
	case EOpenVDBGridType::Double:
		return TEXT("Double");
	case EOpenVDBGridType::Double2:
		return TEXT("Double2");
	case EOpenVDBGridType::Double3:
		return TEXT("Double3");
	case EOpenVDBGridType::Double4:
		return TEXT("Double4");
	default:
		return TEXT("Unknown");
	}
}

#endif // WITH_EDITOR