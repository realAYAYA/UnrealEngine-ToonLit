// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SparseVolumeTexture.cpp: SparseVolumeTexture implementation.
=============================================================================*/

#include "SparseVolumeTexture/SparseVolumeTexture.h"

#include "Misc/SecureHash.h"
#include "EngineUtils.h"
#include "Shader/ShaderTypes.h"

#if WITH_EDITORONLY_DATA
#include "DerivedDataCacheInterface.h"
#endif

#include "Serialization/LargeMemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/BulkDataReader.h"
#include "Serialization/BulkDataWriter.h"
#include "Serialization/EditorBulkDataReader.h"

#define LOCTEXT_NAMESPACE "USparseVolumeTexture"

DEFINE_LOG_CATEGORY_STATIC(LogSparseVolumeTexture, Log, All);

FConvertOpenVDBToSparseVolumeTextureDelegate ConvertOpenVDBToSparseVolumeTextureDelegate;

FConvertOpenVDBToSparseVolumeTextureDelegate& OnConvertOpenVDBToSparseVolumeTexture()
{
	return ConvertOpenVDBToSparseVolumeTextureDelegate;
}

namespace UE
{
namespace SVT
{
namespace Private
{
	// SVT_TODO: This really should be a shared function.
	template <typename Y, typename T>
	void SerializeEnumAs(FArchive& Ar, T& Target)
	{
		Y Buffer = static_cast<Y>(Target);
		Ar << Buffer;
		if (Ar.IsLoading())
		{
			Target = static_cast<T>(Buffer);
		}
	}
} // Private
} // SVT
} // UE

FArchive& operator<<(FArchive& Ar, FSparseVolumeRawSourcePackedData& PackedData)
{
	UE::SVT::Private::SerializeEnumAs<uint8>(Ar, PackedData.Format);
	Ar << PackedData.SourceGridIndex;
	Ar << PackedData.SourceComponentIndex;
	Ar << PackedData.bRemapInputForUnorm;

	return Ar;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FSparseVolumeRawSource::Serialize(FArchive& Ar)
{
	Ar << Version;

	if (Version == 0)
	{
		Ar << PackedDataA;
		Ar << PackedDataB;
		Ar << SourceAssetFile;
	}
	else
	{
		// FSparseVolumeRawSource needs to account for new version
		check(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FSparseVolumeAssetHeader::Serialize(FArchive& Ar)
{
	Ar << Version;

	if (Version == 0)
	{
		Ar << PageTableVolumeResolution;
		Ar << TileDataVolumeResolution;
		Ar << SourceVolumeResolution;
		Ar << SourceVolumeAABBMin;
		UE::SVT::Private::SerializeEnumAs<uint8>(Ar, PackedDataAFormat);
		UE::SVT::Private::SerializeEnumAs<uint8>(Ar, PackedDataBFormat);
	}
	else
	{
		// FSparseVolumeAssetHeader needs to account for new version
		check(false);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void FSparseVolumeTextureRuntime::Serialize(FArchive& Ar)
{
	Header.Serialize(Ar);

	Ar << Version;

	if (Version == 0)
	{
		Ar << PageTable;
		Ar << PhysicalTileDataA;
		Ar << PhysicalTileDataB;
	}
	else
	{
		// FSparseVolumeRawSource needs to account for new version
		check(false);
	}
}

void FSparseVolumeTextureRuntime::SetAsDefaultTexture()
{
	const uint32 VolumeSize = 1;
	PageTable.SetNumZeroed(VolumeSize * VolumeSize * VolumeSize);
	PhysicalTileDataA.SetNumZeroed(VolumeSize * VolumeSize * VolumeSize);
	PhysicalTileDataB.SetNumZeroed(VolumeSize * VolumeSize * VolumeSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////

FSparseVolumeTextureSceneProxy::FSparseVolumeTextureSceneProxy()
	: FRenderResource()
	, SparseVolumeTextureRuntime(nullptr)
	, PageTableTextureRHI(nullptr)
	, PhysicalTileDataATextureRHI(nullptr)
	, PhysicalTileDataBTextureRHI(nullptr)
{
}

FSparseVolumeTextureSceneProxy::~FSparseVolumeTextureSceneProxy()
{
}

void FSparseVolumeTextureSceneProxy::InitialiseRuntimeData(FSparseVolumeTextureRuntime& SparseVolumeTextureRuntimeIn)
{
	SparseVolumeTextureRuntime = &SparseVolumeTextureRuntimeIn;
}

void FSparseVolumeTextureSceneProxy::InitRHI()
{
	// Page table
	{
		EPixelFormat PageEntryFormat = PF_R32_UINT;
		FIntVector3 PageTableVolumeResolution = SparseVolumeTextureRuntime->Header.PageTableVolumeResolution;
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PageTable.RHITexture"),
				PageTableVolumeResolution.X, PageTableVolumeResolution.Y, PageTableVolumeResolution.Z, PageEntryFormat)
			.SetFlags(ETextureCreateFlags::ShaderResource);

		PageTableTextureRHI = RHICreateTexture(Desc);

		const int32 FormatSize = GPixelFormats[PageEntryFormat].BlockBytes;
		const FUpdateTextureRegion3D UpdateRegion(0, 0, 0, 0, 0, 0, PageTableVolumeResolution.X, PageTableVolumeResolution.Y, PageTableVolumeResolution.Z);
		RHIUpdateTexture3D(PageTableTextureRHI, 0, UpdateRegion, UpdateRegion.Width * FormatSize, UpdateRegion.Width * UpdateRegion.Height * FormatSize, (const uint8*)SparseVolumeTextureRuntime->PageTable.GetData());
	}

	// Tile data
	{
		FIntVector3 TileDataVolumeResolution = SparseVolumeTextureRuntime->Header.TileDataVolumeResolution;
		EPixelFormat VoxelFormatA = SparseVolumeTextureRuntime->Header.PackedDataAFormat;
		EPixelFormat VoxelFormatB = SparseVolumeTextureRuntime->Header.PackedDataBFormat;
		const FUpdateTextureRegion3D UpdateRegion(0, 0, 0, 0, 0, 0, TileDataVolumeResolution.X, TileDataVolumeResolution.Y, TileDataVolumeResolution.Z);

		// A
		if (VoxelFormatA != PF_Unknown)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataA.RHITexture"),
					TileDataVolumeResolution.X, TileDataVolumeResolution.Y, TileDataVolumeResolution.Z, VoxelFormatA)
				.SetFlags(ETextureCreateFlags::ShaderResource);

			PhysicalTileDataATextureRHI = RHICreateTexture(Desc);

			const int32 FormatSize = GPixelFormats[VoxelFormatA].BlockBytes;
			RHIUpdateTexture3D(PhysicalTileDataATextureRHI, 0, UpdateRegion, UpdateRegion.Width * FormatSize, UpdateRegion.Width * UpdateRegion.Height * FormatSize, (const uint8*)SparseVolumeTextureRuntime->PhysicalTileDataA.GetData());
		}

		// B
		if (VoxelFormatB != PF_Unknown)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataB.RHITexture"),
					TileDataVolumeResolution.X, TileDataVolumeResolution.Y, TileDataVolumeResolution.Z, VoxelFormatB)
				.SetFlags(ETextureCreateFlags::ShaderResource);

			PhysicalTileDataBTextureRHI = RHICreateTexture(Desc);

			const int32 FormatSize = GPixelFormats[VoxelFormatB].BlockBytes;
			RHIUpdateTexture3D(PhysicalTileDataBTextureRHI, 0, UpdateRegion, UpdateRegion.Width * FormatSize, UpdateRegion.Width * UpdateRegion.Height * FormatSize, (const uint8*)SparseVolumeTextureRuntime->PhysicalTileDataB.GetData());
		}
	}
}

void FSparseVolumeTextureSceneProxy::ReleaseRHI()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

FSparseVolumeTextureFrame::FSparseVolumeTextureFrame()
	: RuntimeStreamedInData()
	, SparseVolumeTextureRuntime()
	, SparseVolumeTextureSceneProxy()
#if WITH_EDITORONLY_DATA
	, RawData()
#endif
{
}

FSparseVolumeTextureFrame::~FSparseVolumeTextureFrame()
{
}

bool FSparseVolumeTextureFrame::BuildRuntimeData()
{
#if WITH_EDITORONLY_DATA
	// Check if the virtualized bulk data payload is available now
	if (RawData.HasPayloadData())
	{
		// First, read the source data in from the raw data stored as bulk data
		UE::Serialization::FEditorBulkDataReader RawDataArchiveReader(RawData);
		FSparseVolumeRawSource SparseVolumeRawSource;
		SparseVolumeRawSource.Serialize(RawDataArchiveReader);

		// Then, convert the raw source data (OpenVDB) to SVT
		FOpenVDBToSVTConversionResult SVTResult;
		SVTResult.Header = &SparseVolumeTextureRuntime.Header;
		SVTResult.PageTable = &SparseVolumeTextureRuntime.PageTable;
		SVTResult.PhysicalTileDataA = &SparseVolumeTextureRuntime.PhysicalTileDataA;
		SVTResult.PhysicalTileDataB = &SparseVolumeTextureRuntime.PhysicalTileDataB;

		const bool bSuccess = ConvertOpenVDBToSparseVolumeTextureDelegate.IsBound() && ConvertOpenVDBToSparseVolumeTextureDelegate.Execute(
			SparseVolumeRawSource.SourceAssetFile,
			SparseVolumeRawSource.PackedDataA,
			SparseVolumeRawSource.PackedDataB,
			&SVTResult,
			false, FVector::Zero(), FVector::Zero());
		
		if (!bSuccess)
		{
			// Clear any writes that may have happened to this data during the cook attempt
			SparseVolumeTextureRuntime = {};
			return false;
		}

		// Now unload the raw data
		RawData.UnloadData();
		
		return true;
	}
#endif
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////

USparseVolumeTexture::USparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USparseVolumeTexture::GetFrameUVScaleBias(int32 FrameIndex, FVector* OutScale, FVector* OutBias) const
{
	*OutScale = FVector::One();
	*OutBias = FVector::Zero();
	const FSparseVolumeTextureSceneProxy* Proxy = GetSparseVolumeTextureSceneProxy(FrameIndex);
	if (Proxy)
	{
		const FSparseVolumeAssetHeader& Header = Proxy->GetHeader();
		const FBox VolumeBounds = GetVolumeBounds();
		check(VolumeBounds.GetVolume() > 0.0);
		const FBox FrameBounds = FBox(FVector(Header.SourceVolumeAABBMin), FVector(Header.SourceVolumeAABBMin + Header.PageTableVolumeResolution * SPARSE_VOLUME_TILE_RES)); // AABB of current frame

		*OutScale = VolumeBounds.GetExtent() / FrameBounds.GetExtent();
		*OutBias = -((FrameBounds.Min - VolumeBounds.Min) / (VolumeBounds.Max - VolumeBounds.Min) * *OutScale);
	}
}

UE::Shader::EValueType USparseVolumeTexture::GetUniformParameterType(int32 Index)
{
	switch (Index)
	{
	case ESparseVolumeTexture_PhysicalUVToPageUV:	return UE::Shader::EValueType::Float3;
	case ESparseVolumeTexture_TileSize:				return UE::Shader::EValueType::Float1;
	case ESparseVolumeTexture_PageTableSize:		return UE::Shader::EValueType::Float3;
	default:
		break;
	}
	check(0);
	return UE::Shader::EValueType::Float4;
}

////////////////////////////////////////////////////////////////////////////////////////////////

UStaticSparseVolumeTexture::UStaticSparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, StaticFrame()
{
}

void UStaticSparseVolumeTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	// CumulativeResourceSize.AddDedicatedSystemMemoryBytes but not the RawData size
}

void UStaticSparseVolumeTexture::PostLoad()
{
	GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy();

	Super::PostLoad();
}

void UStaticSparseVolumeTexture::BeginDestroy()
{
	Super::BeginDestroy();

	BeginReleaseResource(&StaticFrame.SparseVolumeTextureSceneProxy);
}

void UStaticSparseVolumeTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);

	if (StripFlags.IsEditorDataStripped() && Ar.IsLoadingFromCookedPackage())
	{
		// In this case we are loading in game with a cooked build so we only need to load the runtime data.

		// Read cooked bulk data from archive
		StaticFrame.RuntimeStreamedInData.Serialize(Ar, this);

		// Create runtime data from cooked bulk data
		{
			FBulkDataReader BulkDataReader(StaticFrame.RuntimeStreamedInData);
			StaticFrame.SparseVolumeTextureRuntime.Serialize(BulkDataReader);
		}

		// The bulk data is no longer needed
		StaticFrame.RuntimeStreamedInData.RemoveBulkData();

		// Runtime data is now valid, create the render thread proxy
		StaticFrame.SparseVolumeTextureSceneProxy.InitialiseRuntimeData(StaticFrame.SparseVolumeTextureRuntime);
		BeginInitResource(&StaticFrame.SparseVolumeTextureSceneProxy);
	}
	else if (Ar.IsCooking())
	{
		// We are cooking the game, serialize the asset out.
		
		// The runtime bulk data for static sparse volume texture is always loaded, not streamed in.
		StaticFrame.RuntimeStreamedInData.SetBulkDataFlags(BULKDATA_ForceInlinePayload);

		const bool bBuiltRuntimeData = StaticFrame.BuildRuntimeData();
		check(bBuiltRuntimeData); // SVT_TODO: actual error handling
		
		// Write runtime data into RuntimeStreamedInData
		{
			FBulkDataWriter BulkDataWriter(StaticFrame.RuntimeStreamedInData);
			StaticFrame.SparseVolumeTextureRuntime.Serialize(BulkDataWriter);
		}

		// And now write the cooked bulk data to the archive
		StaticFrame.RuntimeStreamedInData.Serialize(Ar, this);
	}
	else if (!Ar.IsObjectReferenceCollector())
	{
#if WITH_EDITORONLY_DATA
		// When in EDITOR:
		//  - We only serialize raw data 
		//  - The runtime data is fetched/put from/to DDC
		//  - This EditorBulk data do not load the full and huge OpenVDB data. That is only done explicitly later.
		StaticFrame.RawData.Serialize(Ar, this);
#endif
	}
}

void UStaticSparseVolumeTexture::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
}

#if WITH_EDITOR
void UStaticSparseVolumeTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy();
}
#endif // WITH_EDITOR

const FSparseVolumeAssetHeader* UStaticSparseVolumeTexture::GetSparseVolumeTextureHeader(int32 FrameIndex) const
{
	return &StaticFrame.SparseVolumeTextureRuntime.Header;
}
FSparseVolumeTextureSceneProxy* UStaticSparseVolumeTexture::GetSparseVolumeTextureSceneProxy(int32 FrameIndex)
{ 
	return &StaticFrame.SparseVolumeTextureSceneProxy; 
}
const FSparseVolumeTextureSceneProxy* UStaticSparseVolumeTexture::GetSparseVolumeTextureSceneProxy(int32 FrameIndex) const
{ 
	return &StaticFrame.SparseVolumeTextureSceneProxy; 
}

FVector4 UStaticSparseVolumeTexture::GetUniformParameter(int32 Index, int32 FrameIndex) const
{
	const FSparseVolumeTextureSceneProxy* Proxy = GetSparseVolumeTextureSceneProxy(0);
	if (Proxy)
	{
		const FSparseVolumeAssetHeader& Header = Proxy->GetHeader();
		switch (Index)
		{
		case ESparseVolumeTexture_PhysicalUVToPageUV:
		{
			// 3d uv coordinates are specified in [0, 1]. Before addressing the page tables which might have padding,
			// since source volume resolution might not align to tile resolution, we have to rescale the uv so that [0,1] maps to the source texture boundaries.
			FVector3f PhysicalUVToPageUV = FVector3f(Header.SourceVolumeResolution)/ FVector3f(Header.PageTableVolumeResolution * SPARSE_VOLUME_TILE_RES);
			return FVector4(PhysicalUVToPageUV.X, PhysicalUVToPageUV.Y, PhysicalUVToPageUV.Z, 0.0f);
		}
		case ESparseVolumeTexture_PageTableSize:
		{
			return FVector4(Header.PageTableVolumeResolution.X, Header.PageTableVolumeResolution.Y, Header.PageTableVolumeResolution.Z, 0.0f);
		}
		case ESparseVolumeTexture_TileSize:				return FVector4(float(SPARSE_VOLUME_TILE_RES), 0.0f, 0.0f, 0.0f);
		default:
			break;
		}
		check(0);
		return FVector4(ForceInitToZero);
	}
	// 0 while waiting for the proxy
	return FVector4(ForceInitToZero);
}

FBox UStaticSparseVolumeTexture::GetVolumeBounds() const
{
	return VolumeBounds;
}

void UStaticSparseVolumeTexture::ConvertRawSourceDataToSparseVolumeTextureRuntime()
{
#if WITH_EDITOR
	// Check if the virtualized bulk data payload is available now
	if (StaticFrame.RawData.HasPayloadData())
	{
		const bool bSuccess = StaticFrame.BuildRuntimeData();
		ensure(bSuccess);
	}
	else
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("SparseVolumeTexture - Raw source data is not available for %s. Using default data."), *GetName());
		StaticFrame.SparseVolumeTextureRuntime.SetAsDefaultTexture();
	}
#endif // WITH_EDITOR
}

void UStaticSparseVolumeTexture::GenerateOrLoadDDCRuntimeData()
{
#if WITH_EDITORONLY_DATA
	static const FString SparseVolumeTextureDDCVersion = TEXT("381AE2A9-A903-4C8F-8486-891E24D6EC70");	// Bump this if you want to ignore all cached data so far.
	const FString DerivedDataKey = StaticFrame.RawData.GetIdentifier().ToString() + SparseVolumeTextureDDCVersion;

	bool bSuccess = true;
	TArray<uint8> DerivedData;
	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_LOG(LogSparseVolumeTexture, Display, TEXT("SparseVolumeTexture - Caching %s"), *GetName());

		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);

		int64 UncompressedSize = 0;
		Ar << UncompressedSize;

		uint8* DecompressionBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(UncompressedSize));
		Ar.SerializeCompressedNew(DecompressionBuffer, UncompressedSize);

		FLargeMemoryReader LargeMemReader(DecompressionBuffer, UncompressedSize, ELargeMemoryReaderFlags::Persistent | ELargeMemoryReaderFlags::TakeOwnership);

		StaticFrame.SparseVolumeTextureRuntime.Serialize(LargeMemReader);
	}
	else
	{
		// Using a LargeMemoryWriter for serialization since the data can be bigger than 2 GB
		FLargeMemoryWriter LargeMemWriter(0, /*bIsPersistent=*/ true);

		ConvertRawSourceDataToSparseVolumeTextureRuntime();
		StaticFrame.SparseVolumeTextureRuntime.Serialize(LargeMemWriter);

		int64 UncompressedSize = LargeMemWriter.TotalSize();

		// Since the DDC doesn't support data bigger than 2 GB, we only cache for such uncompressed size.
		constexpr int64 SizeThreshold = 2147483648;	// 2GB
		const bool bIsCacheable = UncompressedSize < SizeThreshold;
		if (bIsCacheable)
		{
			FMemoryWriter CompressedArchive(DerivedData, true);

			CompressedArchive << UncompressedSize; // needed for allocating decompression buffer
			CompressedArchive.SerializeCompressedNew(LargeMemWriter.GetData(), UncompressedSize);

			GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
		}
		else
		{
			UE_LOG(LogSparseVolumeTexture, Error, TEXT("SparseVolumeTexture - the asset is too large to fit in Derived Data Cache %s"), *GetName());
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UStaticSparseVolumeTexture::GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy()
{
	// Release any previously allocated render thread proxy
	BeginReleaseResource(&StaticFrame.SparseVolumeTextureSceneProxy);

#if WITH_EDITORONLY_DATA
	// We only fetch/put DDC when in editor. Otherwise, StaticFrame.SparseVolumeTextureRuntime is serialize in.
	GenerateOrLoadDDCRuntimeData();
#endif

	// Runtime data is now valid, create the render thread proxy
	StaticFrame.SparseVolumeTextureSceneProxy.InitialiseRuntimeData(StaticFrame.SparseVolumeTextureRuntime);
	BeginInitResource(&StaticFrame.SparseVolumeTextureSceneProxy);
}

////////////////////////////////////////////////////////////////////////////////////////////////

UAnimatedSparseVolumeTexture::UAnimatedSparseVolumeTexture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FrameCount(0)
	, AnimationFrames()
{
}

void UAnimatedSparseVolumeTexture::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	// CumulativeResourceSize.AddDedicatedSystemMemoryBytes but not the RawData size
}

void UAnimatedSparseVolumeTexture::PostLoad()
{
	const int32 FrameCountToLoad = GetFrameCountToLoad();
	for (int32 FrameIndex = 0; FrameIndex < FrameCountToLoad; FrameIndex++)
	{
		GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy(FrameIndex);
	}

	Super::PostLoad();
}

void UAnimatedSparseVolumeTexture::BeginDestroy()
{
	Super::BeginDestroy();

	const int32 FrameCountToLoad = GetFrameCountToLoad();
	for(int32 FrameIndex = 0; FrameIndex < FrameCountToLoad; FrameIndex++)
	{
		FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];
		BeginReleaseResource(&Frame.SparseVolumeTextureSceneProxy);
	}
}

void UAnimatedSparseVolumeTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FStripDataFlags StripFlags(Ar);

	if (StripFlags.IsEditorDataStripped() && Ar.IsLoadingFromCookedPackage())
	{
		// In this case we are loading in game with a cooked build so we only need to load the runtime data.

		AnimationFrames.SetNum(FrameCount);
		for (int32 i = 0; i < FrameCount; ++i)
		{
			FSparseVolumeTextureFrame& Frame = AnimationFrames[i];
			
			// Read cooked bulk data from archive
			Frame.RuntimeStreamedInData.Serialize(Ar, this);

			// Create runtime data from cooked bulk data
			{
				FBulkDataReader RuntimeStreamedInData(Frame.RuntimeStreamedInData);
				Frame.SparseVolumeTextureRuntime.Serialize(RuntimeStreamedInData);
			}

			// The bulk data is no longer needed
			Frame.RuntimeStreamedInData.RemoveBulkData();

			// Runtime data is now valid, create the render thread proxy
			Frame.SparseVolumeTextureSceneProxy.InitialiseRuntimeData(Frame.SparseVolumeTextureRuntime);
			BeginInitResource(&Frame.SparseVolumeTextureSceneProxy);
		}
	}
	else if (Ar.IsCooking())
	{
		// We are cooking the game, serialize the asset out.

		check(AnimationFrames.Num() == FrameCount);
		for (int32 i = 0; i < FrameCount; ++i)
		{
			FSparseVolumeTextureFrame& Frame = AnimationFrames[i];

			// SVT_TODO: Until we have streaming, the runtime bulk data for all sparse volume texture frames is always loaded.
			AnimationFrames[i].RuntimeStreamedInData.SetBulkDataFlags(BULKDATA_ForceInlinePayload);

			const bool bBuiltRuntimeData = Frame.BuildRuntimeData();
			check(bBuiltRuntimeData); // SVT_TODO

			// Write runtime data into RuntimeStreamedInData
			{
				FBulkDataWriter RuntimeStreamedInData(Frame.RuntimeStreamedInData);
				Frame.SparseVolumeTextureRuntime.Serialize(RuntimeStreamedInData);
			}

			// And now write the cooked bulk data to the archive
			Frame.RuntimeStreamedInData.Serialize(Ar, this);
		}
	}
	else if (!Ar.IsObjectReferenceCollector())
	{
#if WITH_EDITORONLY_DATA
		// When in EDITOR:
		//  - We only serialize raw data 
		//  - The runtime data is fetched/put from/to DDC
		//  - This EditorBulk data do not load the full and huge OpenVDB data. That is only done explicitly later.
		if (Ar.IsSaving())
		{
			check(AnimationFrames.Num() == FrameCount);
		}
		else if (Ar.IsLoading())
		{
			AnimationFrames.SetNum(FrameCount);
		}

		for (int32 i = 0; i < FrameCount; ++i)
		{
			AnimationFrames[i].RawData.Serialize(Ar, this);
		}
#endif
	}
}

void UAnimatedSparseVolumeTexture::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);
}

#if WITH_EDITOR
void UAnimatedSparseVolumeTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const int32 FrameCountToLoad = GetFrameCountToLoad();
	for (int32 FrameIndex = 0; FrameIndex < FrameCountToLoad; FrameIndex++)
	{
		GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy(FrameIndex);
	}
}
#endif // WITH_EDITOR

const FSparseVolumeAssetHeader* UAnimatedSparseVolumeTexture::GetSparseVolumeTextureHeader(int32 FrameIndex) const
{
	// When an AnimatedSparseVolumeTexture is used as SparseVolumeTexture, it can only be previewed using its first frame.
	check(AnimationFrames.Num() >= 1);
	FrameIndex = FrameIndex % GetFrameCountToLoad();
	const FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];
	return &Frame.SparseVolumeTextureRuntime.Header;
}

FSparseVolumeTextureSceneProxy* UAnimatedSparseVolumeTexture::GetSparseVolumeTextureSceneProxy(int32 FrameIndex)
{
	// When an AnimatedSparseVolumeTexture is used as SparseVolumeTexture, it can only be previewed using its first frame.
	check(AnimationFrames.Num() >= 1);
	FrameIndex = FrameIndex % GetFrameCountToLoad();
	FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];
	return &Frame.SparseVolumeTextureSceneProxy;
}

const FSparseVolumeTextureSceneProxy* UAnimatedSparseVolumeTexture::GetSparseVolumeTextureSceneProxy(int32 FrameIndex) const
{
	// When an AnimatedSparseVolumeTexture is used as SparseVolumeTexture, it can only be previewed using its first frame.
	check(AnimationFrames.Num() >= 1);
	FrameIndex = FrameIndex % GetFrameCountToLoad();
	const FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];
	return &Frame.SparseVolumeTextureSceneProxy;
}

FVector4 UAnimatedSparseVolumeTexture::GetUniformParameter(int32 Index, int32 FrameIndex) const
{
	// When an AnimatedSparseVolumeTexture is used as SparseVolumeTexture, it can only be previewed using its first frame.
	const FSparseVolumeTextureSceneProxy* Proxy = GetSparseVolumeTextureSceneProxy(FrameIndex);
	if (Proxy)
	{
		const FSparseVolumeAssetHeader& Header = Proxy->GetHeader();
		switch (Index)
		{
		case ESparseVolumeTexture_PhysicalUVToPageUV:
		{
			// 3d uv coordinates are specified in [0, 1]. Before addressing the page tables which might have padding,
			// since source volume resolution might not align to tile resolution, we have to rescale the uv so that [0,1] maps to the source texture boundaries.
			FVector3f PhysicalUVToPageUV = FVector3f(Header.SourceVolumeResolution) / FVector3f(Header.PageTableVolumeResolution * SPARSE_VOLUME_TILE_RES);
			return FVector4(PhysicalUVToPageUV.X, PhysicalUVToPageUV.Y, PhysicalUVToPageUV.Z, 0.0f);
		}
		case ESparseVolumeTexture_PageTableSize:
		{
			return FVector4(Header.PageTableVolumeResolution.X, Header.PageTableVolumeResolution.Y, Header.PageTableVolumeResolution.Z, 0.0f);
		}
		case ESparseVolumeTexture_TileSize:				return FVector4(float(SPARSE_VOLUME_TILE_RES), 0.0f, 0.0f, 0.0f);
		default:
			break;
		}
		check(0);
		return FVector4(ForceInitToZero);
	}

	// 0 while waiting for the proxy
	return FVector4(ForceInitToZero);
}

FBox UAnimatedSparseVolumeTexture::GetVolumeBounds() const
{
	return VolumeBounds;
}

const FSparseVolumeTextureSceneProxy* UAnimatedSparseVolumeTexture::GetSparseVolumeTextureFrameSceneProxy(int32 FrameIndex) const
{
	check(AnimationFrames.Num() >= 1);
	FrameIndex = FrameIndex % GetFrameCountToLoad();
	// SVT_TODO when streaming is enabled, this will likely change.
	return &AnimationFrames[FrameIndex].SparseVolumeTextureSceneProxy;
}

int32 UAnimatedSparseVolumeTexture::GetFrameCountToLoad() const
{
	if (FrameCount > 0)
	{
		return bLoadAllFramesToProxies ? FrameCount : 1;
	}
	return 0;
}

void UAnimatedSparseVolumeTexture::ConvertRawSourceDataToSparseVolumeTextureRuntime(int32 FrameIndex)
{
#if WITH_EDITOR
	FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];

	// Check if the virtualized bulk data payload is available now
	if (Frame.RawData.HasPayloadData())
	{
		const bool bSuccess = Frame.BuildRuntimeData();
		ensure(bSuccess);
	}
	else
	{
		UE_LOG(LogSparseVolumeTexture, Error, TEXT("AnimatedSparseVolumeTexture - Raw source data is not available for %s - Frame %i. Using default data."), *GetName(), FrameIndex);
		Frame.SparseVolumeTextureRuntime.SetAsDefaultTexture();
	}
#endif // WITH_EDITOR
}

void UAnimatedSparseVolumeTexture::GenerateOrLoadDDCRuntimeData(int32 FrameIndex)
{
#if WITH_EDITORONLY_DATA
	static const FString SparseVolumeTextureDDCVersion = TEXT("381AE2A9-A903-4C8F-8486-891E24D6EC71");	// Bump this if you want to ignore all cached data so far.

	FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];

	const FString DerivedDataKey = Frame.RawData.GetIdentifier().ToString() + SparseVolumeTextureDDCVersion;

	bool bSuccess = true;
	TArray<uint8> DerivedData;
	if (GetDerivedDataCacheRef().GetSynchronous(*DerivedDataKey, DerivedData, GetPathName()))
	{
		UE_LOG(LogSparseVolumeTexture, Display, TEXT("SparseVolumeTexture - Caching %s"), *GetName());

		FMemoryReader Ar(DerivedData, /*bIsPersistent=*/ true);

		int64 UncompressedSize = 0;
		Ar << UncompressedSize;

		uint8* DecompressionBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(UncompressedSize));
		Ar.SerializeCompressedNew(DecompressionBuffer, UncompressedSize);

		FLargeMemoryReader LargeMemReader(DecompressionBuffer, UncompressedSize, ELargeMemoryReaderFlags::Persistent | ELargeMemoryReaderFlags::TakeOwnership);

		Frame.SparseVolumeTextureRuntime.Serialize(LargeMemReader);
	}
	else
	{
		// Using a LargeMemoryWriter for serialization since the data can be bigger than 2 GB
		FLargeMemoryWriter LargeMemWriter(0, /*bIsPersistent=*/ true);

		ConvertRawSourceDataToSparseVolumeTextureRuntime(FrameIndex);
		Frame.SparseVolumeTextureRuntime.Serialize(LargeMemWriter);

		int64 UncompressedSize = LargeMemWriter.TotalSize();

		// Since the DDC doesn't support data bigger than 2 GB, we only cache for such uncompressed size.
		constexpr int64 SizeThreshold = 2147483648;	// 2GB
		const bool bIsCacheable = UncompressedSize < SizeThreshold;
		if (bIsCacheable)
		{
			FMemoryWriter CompressedArchive(DerivedData, true);

			CompressedArchive << UncompressedSize; // needed for allocating decompression buffer
			CompressedArchive.SerializeCompressedNew(LargeMemWriter.GetData(), UncompressedSize);

			GetDerivedDataCacheRef().Put(*DerivedDataKey, DerivedData, GetPathName());
		}
		else
		{
			UE_LOG(LogSparseVolumeTexture, Error, TEXT("SparseVolumeTexture - the asset is too large to fit in Derived Data Cache %s - Frame %i"), *GetName(), FrameIndex);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UAnimatedSparseVolumeTexture::GenerateOrLoadDDCRuntimeDataAndCreateSceneProxy(int32 FrameIndex)
{
	FSparseVolumeTextureFrame& Frame = AnimationFrames[FrameIndex];

	// Release any previously allocated render thread proxy
	BeginReleaseResource(&Frame.SparseVolumeTextureSceneProxy);

#if WITH_EDITORONLY_DATA
	// We only fetch/put DDC when in editor. Otherwise, StaticFrame.SparseVolumeTextureRuntime is serialize in.
	GenerateOrLoadDDCRuntimeData(FrameIndex);
#endif

	// Runtime data is now valid, create the render thread proxy
	Frame.SparseVolumeTextureSceneProxy.InitialiseRuntimeData(Frame.SparseVolumeTextureRuntime);
	BeginInitResource(&Frame.SparseVolumeTextureSceneProxy);
}

////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
