// Copyright Epic Games, Inc. All Rights Reserved.

#include "SparseVolumeTexture/SparseVolumeTextureStreamingManager.h"
#include "SparseVolumeTexture/SparseVolumeTextureUtility.h"
#include "HAL/IConsoleManager.h"
#include "SparseVolumeTexture/SparseVolumeTexture.h"
#include "RenderCore.h"
#include "RenderGraph.h"
#include "GlobalShader.h"
#include "ShaderCompilerCore.h" // AllowGlobalShaderLoad()
#include "Async/ParallelFor.h"

#if WITH_EDITORONLY_DATA
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSparseVolumeTextureStreamingManager, Log, All);

#ifndef SVT_STREAMING_LOG_VERBOSE
#define SVT_STREAMING_LOG_VERBOSE 0
#endif

static int32 GSVTStreamingNumPrefetchFrames = 3;
static FAutoConsoleVariableRef CVarSVTStreamingNumPrefetchFrames(
	TEXT("r.SparseVolumeTexture.Streaming.NumPrefetchFrames"),
	GSVTStreamingNumPrefetchFrames,
	TEXT("Number of frames to prefetch when a frame is requested."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingPrefetchMipLevelBias = -1;
static FAutoConsoleVariableRef CVarSVTStreamingPrefetchMipLevelBias(
	TEXT("r.SparseVolumeTexture.Streaming.PrefetchMipLevelBias"),
	GSVTStreamingPrefetchMipLevelBias,
	TEXT("Bias to apply to the mip level of prefetched frames. Prefetching is done at increasingly higher mip levels (lower resolution), so setting a negative value here will increase the requested mip level resolution."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingForceBlockingRequests = 0;
static FAutoConsoleVariableRef CVarSVTStreamingForceBlockingRequests(
	TEXT("r.SparseVolumeTexture.Streaming.ForceBlockingRequests"),
	GSVTStreamingForceBlockingRequests,
	TEXT("If enabled, all SVT streaming requests will block on completion, guaranteeing that requested mip levels are available in the same frame they have been requested in (if there is enough memory available to stream them in)."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingAsyncThread = 1;
static FAutoConsoleVariableRef CVarSVTStreamingAsync(
	TEXT("r.SparseVolumeTexture.Streaming.AsyncThread"),
	GSVTStreamingAsyncThread,
	TEXT("Perform most of the SVT streaming on an asynchronous worker thread instead of the rendering thread."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingAsyncCompute = 1;
static FAutoConsoleVariableRef CVarSVTStreamingAsyncCompute(
	TEXT("r.SparseVolumeTexture.Streaming.AsyncCompute"),
	GSVTStreamingAsyncCompute,
	TEXT("Schedule GPU work in async compute queue."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingEmptyPhysicalTileTextures = 0;
static FAutoConsoleVariableRef CVarSVTStreamingEmptyPhysicalTileTextures(
	TEXT("r.SparseVolumeTexture.Streaming.EmptyPhysicalTileTextures"),
	GSVTStreamingEmptyPhysicalTileTextures,
	TEXT("Streams out all streamable tiles of all physical tile textures."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingPrintMemoryStats = 0;
static FAutoConsoleVariableRef CVarSVTStreamingPrintMemoryStats(
	TEXT("r.SparseVolumeTexture.Streaming.PrintMemoryStats"),
	GSVTStreamingPrintMemoryStats,
	TEXT("Prints memory sizes of all frames of all SVTs registered with the streaming system."),
	ECVF_RenderThreadSafe
);

static int32 GSVTStreamingMaxPendingMipLevels = 128;
static FAutoConsoleVariableRef CVarSVTStreamingMaxPendingMipLevels(
	TEXT("r.SparseVolumeTexture.Streaming.MaxPendingMipLevels"),
	GSVTStreamingMaxPendingMipLevels,
	TEXT("Maximum number of mip levels that can be pending for installation."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

namespace UE
{
namespace SVT
{

TGlobalResource<FStreamingManager> GStreamingManager;

IStreamingManager& GetStreamingManager()
{
	return GStreamingManager;
}

static bool DoesPlatformSupportSparseVolumeTexture(EShaderPlatform Platform)
{
	// SVT_TODO: This is a bit of a hack: FStreamingManager::Add_GameThread() issues a rendering thread lambda for creating the RHI resources and uploading root tile data.
	// Uploading root tile data involves access to the global shader map, which is empty under certain circumstances. By checking AllowGlobalShaderLoad(), we disallow streaming completely.
	return AllowGlobalShaderLoad();
}

static FIntVector3 ComputeTileDataVolumeResolution(int32 NumAllocatedPages)
{
	int32 TileVolumeResolutionCube = 1;
	while (TileVolumeResolutionCube * TileVolumeResolutionCube * TileVolumeResolutionCube < NumAllocatedPages)
	{
		TileVolumeResolutionCube++;				// We use a simple loop to compute the minimum resolution of a cube to store all the tile data
	}
	FIntVector3 TileDataVolumeResolution = FIntVector3(TileVolumeResolutionCube, TileVolumeResolutionCube, TileVolumeResolutionCube);
	
	// Trim volume to reclaim some space
	while ((TileDataVolumeResolution.X * TileDataVolumeResolution.Y * (TileDataVolumeResolution.Z - 1)) > NumAllocatedPages)
	{
		TileDataVolumeResolution.Z--;
	}
	while ((TileDataVolumeResolution.X * (TileDataVolumeResolution.Y - 1) * TileDataVolumeResolution.Z) > NumAllocatedPages)
	{
		TileDataVolumeResolution.Y--;
	}
	while (((TileDataVolumeResolution.X - 1) * TileDataVolumeResolution.Y * TileDataVolumeResolution.Z) > NumAllocatedPages)
	{
		TileDataVolumeResolution.X--;
	}

	return TileDataVolumeResolution * SPARSE_VOLUME_TILE_RES_PADDED;
}

static FIntVector3 ComputeLargestPossibleTileDataVolumeResolution(int32 VoxelMemSize)
{
	const int64 TileMemSize = SVT::NumVoxelsPerPaddedTile * VoxelMemSize;
	const int64 NumMaxTiles = SVT::MaxResourceSize / TileMemSize;
	int64 ResourceSize = NumMaxTiles * TileMemSize;

	// Find a cube with a volume as close to NumMaxTiles as possible
	int32 TileVolumeResolutionCube = 1;
	while (((TileVolumeResolutionCube + 1) * (TileVolumeResolutionCube + 1) * (TileVolumeResolutionCube + 1)) <= NumMaxTiles)
	{
		++TileVolumeResolutionCube;
	}

	// Try to add to the sides to get closer to NumMaxTiles
	FIntVector3 ResolutionInTiles = FIntVector3(TileVolumeResolutionCube, TileVolumeResolutionCube, TileVolumeResolutionCube);
	if (((ResolutionInTiles.X + 1) * ResolutionInTiles.Y * ResolutionInTiles.Z) <= NumMaxTiles)
	{
		++ResolutionInTiles.X;
	}
	if ((ResolutionInTiles.X * (ResolutionInTiles.Y + 1) * ResolutionInTiles.Z) <= NumMaxTiles)
	{
		++ResolutionInTiles.Y;
	}
	if ((ResolutionInTiles.X * ResolutionInTiles.Y * (ResolutionInTiles.Z + 1)) <= NumMaxTiles)
	{
		++ResolutionInTiles.Z;
	}

	const FIntVector3 Resolution = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	check(Resolution.X <= SVT::MaxVolumeTextureDim && Resolution.Y <= SVT::MaxVolumeTextureDim && Resolution.Z <= SVT::MaxVolumeTextureDim);
	check(((int64)Resolution.X * (int64)Resolution.Y * (int64)Resolution.Z) < SVT::MaxResourceSize);

	return Resolution;
}

class FSparseVolumeTextureUpdateFromBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSparseVolumeTextureUpdateFromBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FSparseVolumeTextureUpdateFromBufferCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, DstPhysicalTileTextureA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, DstPhysicalTileTextureB)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, DstTileCoords)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SrcPhysicalTileBufferA)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SrcPhysicalTileBufferB)
		SHADER_PARAMETER(uint32, TileCoordsBufferOffset)
		SHADER_PARAMETER(uint32, TileDataBufferOffsetInTiles)
		SHADER_PARAMETER(uint32, NumTilesToCopy)
		SHADER_PARAMETER(uint32, NumDispatchedGroups)
		SHADER_PARAMETER(uint32, PaddedTileSize)
		SHADER_PARAMETER(uint32, bCopyTexureAOnlyUI)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATE_TILE_TEXTURE_FROM_BUFFER"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSparseVolumeTextureUpdateFromBufferCS, "/Engine/Private/SparseVolumeTexture/UpdateSparseVolumeTexture.usf", "SparseVolumeTextureUpdateFromBufferCS", SF_Compute);

class FSparseVolumeTextureUpdateFromSparseBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSparseVolumeTextureUpdateFromSparseBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FSparseVolumeTextureUpdateFromSparseBufferCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, DstPhysicalTileTextureA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, DstPhysicalTileTextureB)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SrcPhysicalTileBufferA)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, SrcPhysicalTileBufferB)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, OccupancyBitsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, TileDataOffsetsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, DstTileCoordsBuffer)
		SHADER_PARAMETER(FVector4f, FallbackValueA)
		SHADER_PARAMETER(FVector4f, FallbackValueB)
		SHADER_PARAMETER(uint32, TileIndexOffset)
		SHADER_PARAMETER(uint32, SrcVoxelDataOffsetA)
		SHADER_PARAMETER(uint32, SrcVoxelDataOffsetB)
		SHADER_PARAMETER(uint32, NumTilesToCopy)
		SHADER_PARAMETER(uint32, BufferTileStep)
		SHADER_PARAMETER(uint32, NumDispatchedGroups)
		SHADER_PARAMETER(uint32, PaddedTileSize)
		SHADER_PARAMETER(uint32, CopyTexureMask)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATE_TILE_TEXTURE_FROM_SPARSE_BUFFER"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSparseVolumeTextureUpdateFromSparseBufferCS, "/Engine/Private/SparseVolumeTexture/UpdateSparseVolumeTexture.usf", "SparseVolumeTextureUpdateFromSparseBufferCS", SF_Compute);

class FSparseVolumeTextureUpdatePageTableCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSparseVolumeTextureUpdatePageTableCS);
	SHADER_USE_PARAMETER_STRUCT(FSparseVolumeTextureUpdatePageTableCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, PageTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PageTableUpdates)
		SHADER_PARAMETER(uint32, UpdateCoordOffset)
		SHADER_PARAMETER(uint32, UpdatePayloadOffset)
		SHADER_PARAMETER(uint32, NumUpdates)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATE_PAGE_TABLE"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSparseVolumeTextureUpdatePageTableCS, "/Engine/Private/SparseVolumeTexture/UpdateSparseVolumeTexture.usf", "SparseVolumeTextureUpdatePageTableCS", SF_Compute);

class FSparseVolumeTextureUpdateStreamingInfoBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSparseVolumeTextureUpdateStreamingInfoBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FSparseVolumeTextureUpdateStreamingInfoBufferCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, StreamingInfoBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, StreamingInfoBufferUpdates)
		SHADER_PARAMETER(uint32, UpdateOffset)
		SHADER_PARAMETER(uint32, NumUpdates)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("UPDATE_STREAMING_INFO_BUFFER"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FSparseVolumeTextureUpdateStreamingInfoBufferCS, "/Engine/Private/SparseVolumeTexture/UpdateSparseVolumeTexture.usf", "SparseVolumeTextureUpdateStreamingInfoBufferCS", SF_Compute);

// Utility class for uploading tiles to a physical tile data texture
class FTileUploader
{
public:

	struct FAddResult
	{
		TStaticArray<uint8*, 2> OccupancyBitsPtrs;
		TStaticArray<uint8*, 2> TileDataOffsetsPtrs;
		TStaticArray<uint8*, 2> TileDataPtrs;
		TStaticArray<uint32, 2> TileDataBaseOffsets; // Caller needs to add this value to all data written to TileDataOffsetsPtrs
		uint8* PackedPhysicalTileCoordsPtr;
	};

	FTileUploader()
	{
		ResetState();
	}

	void Init(FRDGBuilder& GraphBuilder, int32 InMaxNumTiles, int32 InMaxNumVoxelsA, int32 InMaxNumVoxelsB, EPixelFormat InFormatA, EPixelFormat InFormatB)
	{
		check(InFormatA != PF_Unknown || InFormatB != PF_Unknown);
		ResetState();
		MaxNumTiles = InMaxNumTiles;
		MaxNumVoxelsA = InMaxNumVoxelsA;
		MaxNumVoxelsB = InMaxNumVoxelsB;
		FormatA = InFormatA;
		FormatB = InFormatB;
		FormatSizeA = GPixelFormats[FormatA].BlockBytes;
		FormatSizeB = GPixelFormats[FormatB].BlockBytes;

		// Create a new set of buffers if the old set is already queued into RDG.
		if (IsRegistered(GraphBuilder, DstTileCoordsUploadBuffer))
		{
			OccupancyBitsUploadBuffer = nullptr;
			TileDataOffsetsUploadBuffer = nullptr;
			DstTileCoordsUploadBuffer = nullptr;
			TileDataAUploadBuffer = nullptr;
			TileDataBUploadBuffer = nullptr;
		}

		const int32 NumTextures = (FormatA == PF_Unknown || FormatB == PF_Unknown) ? 1 : 2;

		if (MaxNumTiles > 0)
		{
			FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

			// Occupancy bits
			{
				const uint32 BufferSize = NumTextures * MaxNumTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32);
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateByteAddressUploadDesc(BufferSize);
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic; // Skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				AllocatePooledBuffer(BufferDesc, OccupancyBitsUploadBuffer, TEXT("SparseVolumeTexture.OccupancyBitsUploadBuffer"));

				OccupancyBitsAPtr = (uint8*)RHICmdList.LockBuffer(OccupancyBitsUploadBuffer->GetRHI(), 0, BufferSize, RLM_WriteOnly);
				OccupancyBitsBPtr = OccupancyBitsAPtr + (FormatA != PF_Unknown ? (MaxNumTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32)) : 0);
			}
			// Tile data offsets
			{
				const uint32 BufferSize = NumTextures * MaxNumTiles * sizeof(uint32);
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateByteAddressUploadDesc(BufferSize);
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic; // Skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				AllocatePooledBuffer(BufferDesc, TileDataOffsetsUploadBuffer, TEXT("SparseVolumeTexture.TileDataOffsetsUploadBuffer"));

				// Due to a limit on the maximum number of texels in a buffer SRV, we need to upload the data in smaller chunks. In order to figure out the chunk offsets/sizes,
				// we need to read the TileDataOffset values the caller has written to the returned pointers. We want to avoid reading from a mapped upload buffer pointer,
				// which is why we use a temporary allocation to write the upload data to.
				TileDataOffsets.SetNumUninitialized(NumTextures * MaxNumTiles);
				TileDataOffsetsAPtr = (uint8*)TileDataOffsets.GetData();
				TileDataOffsetsBPtr = TileDataOffsetsAPtr + (FormatA != PF_Unknown ? (MaxNumTiles * sizeof(uint32)) : 0);
			}
			// TileCoords
			{
				const uint32 BufferSize = MaxNumTiles * sizeof(uint32);
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateByteAddressUploadDesc(BufferSize);
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic; // Skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				AllocatePooledBuffer(BufferDesc, DstTileCoordsUploadBuffer, TEXT("SparseVolumeTexture.TileCoordsUploadBuffer"));

				TileCoordsPtr = (uint8*)RHICmdList.LockBuffer(DstTileCoordsUploadBuffer->GetRHI(), 0, BufferSize, RLM_WriteOnly);
			}

			// TileData
			if (FormatSizeA > 0)
			{
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateUploadDesc(FormatSizeA, FMath::Max(MaxNumVoxelsA, 1));
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic; // Skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				AllocatePooledBuffer(BufferDesc, TileDataAUploadBuffer, TEXT("SparseVolumeTexture.TileDataAUploadBuffer"));

				TileDataAPtr = (uint8*)RHICmdList.LockBuffer(TileDataAUploadBuffer->GetRHI(), 0, FMath::Max(MaxNumVoxelsA, 1) * FormatSizeA, RLM_WriteOnly);
			}
			if (FormatSizeB > 0)
			{
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateUploadDesc(FormatSizeB, FMath::Max(MaxNumVoxelsB, 1));
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic; // Skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				AllocatePooledBuffer(BufferDesc, TileDataBUploadBuffer, TEXT("SparseVolumeTexture.TileDataBUploadBuffer"));

				TileDataBPtr = (uint8*)RHICmdList.LockBuffer(TileDataBUploadBuffer->GetRHI(), 0, FMath::Max(MaxNumVoxelsB, 1) * FormatSizeB, RLM_WriteOnly);
			}
		}
	}

	FAddResult Add_GetRef(int32 NumTiles, int32 NumVoxelsA, int32 NumVoxelsB)
	{
		check((NumWrittenTiles + NumTiles) <= MaxNumTiles);
		check((NumWrittenVoxelsA + NumVoxelsA) <= MaxNumVoxelsA);
		check((NumWrittenVoxelsB + NumVoxelsB) <= MaxNumVoxelsB);
		check(TileCoordsPtr);
		check(FormatSizeA <= 0 || TileDataAPtr);
		check(FormatSizeB <= 0 || TileDataBPtr);

		FAddResult Result = {};
		Result.OccupancyBitsPtrs[0] = TileDataAPtr ? OccupancyBitsAPtr + (NumWrittenTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32)) : nullptr;
		Result.OccupancyBitsPtrs[1] = TileDataBPtr ? OccupancyBitsBPtr + (NumWrittenTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32)) : nullptr;
		Result.TileDataOffsetsPtrs[0] = TileDataAPtr ? TileDataOffsetsAPtr + (NumWrittenTiles * sizeof(uint32)) : nullptr;
		Result.TileDataOffsetsPtrs[1] = TileDataBPtr ? TileDataOffsetsBPtr + (NumWrittenTiles * sizeof(uint32)) : nullptr;
		Result.TileDataPtrs[0] = TileDataAPtr ? TileDataAPtr + (NumWrittenVoxelsA * FormatSizeA) : nullptr;
		Result.TileDataPtrs[1] = TileDataBPtr ? TileDataBPtr + (NumWrittenVoxelsB * FormatSizeB) : nullptr;
		Result.TileDataBaseOffsets[0] = NumWrittenVoxelsA;
		Result.TileDataBaseOffsets[1] = NumWrittenVoxelsB;
		Result.PackedPhysicalTileCoordsPtr = TileCoordsPtr + NumWrittenTiles * sizeof(uint32);
		
		NumWrittenTiles += NumTiles;
		NumWrittenVoxelsA += NumVoxelsA;
		NumWrittenVoxelsB += NumVoxelsB;

		return Result;
	}

	void Release()
	{
		OccupancyBitsUploadBuffer.SafeRelease();
		TileDataOffsetsUploadBuffer.SafeRelease();
		DstTileCoordsUploadBuffer.SafeRelease();
		TileDataAUploadBuffer.SafeRelease();
		TileDataBUploadBuffer.SafeRelease();
		TileDataOffsets.Reset();
		ResetState();
	}

	void ResourceUploadTo(FRDGBuilder& GraphBuilder, FRHITexture* DstTextureA, FRHITexture* DstTextureB, const FVector4f& FallbackValueA, const FVector4f& FallbackValueB)
	{
		check(DstTextureA || FormatSizeA <= 0);
		check(DstTextureB || FormatSizeB <= 0);
		if (MaxNumTiles > 0)
		{
			FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

			RHICmdList.UnlockBuffer(OccupancyBitsUploadBuffer->GetRHI());
			RHICmdList.UnlockBuffer(DstTileCoordsUploadBuffer->GetRHI());

			// TileDataOffset values were written to a temporary allocation so that we can access them later in this function. Unlike the other buffers, we now need to copy that data over to the actual upload buffer.
			void* TileDataOffsetsUploadPtr = RHICmdList.LockBuffer(TileDataOffsetsUploadBuffer->GetRHI(), 0, TileDataOffsets.Num() * sizeof(TileDataOffsets[0]), RLM_WriteOnly);
			FMemory::Memcpy(TileDataOffsetsUploadPtr, TileDataOffsets.GetData(), TileDataOffsets.Num() * sizeof(TileDataOffsets[0]));
			RHICmdList.UnlockBuffer(TileDataOffsetsUploadBuffer->GetRHI());

			if (TileDataAPtr)
			{
				RHICmdList.UnlockBuffer(TileDataAUploadBuffer->GetRHI());
			}
			if (TileDataBPtr)
			{
				RHICmdList.UnlockBuffer(TileDataBUploadBuffer->GetRHI());
			}

			if (NumWrittenTiles > 0)
			{
				FRDGTexture* DummyTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create3D(FIntVector(1), PF_R8, FClearValueBinding::None, ETextureCreateFlags::UAV), TEXT("SparseVolumeTexture.DummyTexture"));
				FRDGTexture* DstTextureARDG = DstTextureA ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(DstTextureA, TEXT("SparseVolumeTexture.TileDataTextureA"))) : nullptr;
				FRDGTexture* DstTextureBRDG = DstTextureB ? GraphBuilder.RegisterExternalTexture(CreateRenderTarget(DstTextureB, TEXT("SparseVolumeTexture.TileDataTextureB"))) : nullptr;
				FRDGBuffer* SrcBufferARDG = FormatSizeA > 0 ? GraphBuilder.RegisterExternalBuffer(TileDataAUploadBuffer) : nullptr;
				FRDGBuffer* SrcBufferBRDG = FormatSizeB > 0 ? GraphBuilder.RegisterExternalBuffer(TileDataBUploadBuffer) : nullptr;
				check(SrcBufferARDG || SrcBufferBRDG);

				FRDGTextureUAV* DummyTextureUAV = GraphBuilder.CreateUAV(DummyTexture);
				FRDGTextureUAV* DstTextureAUAV = DstTextureARDG ? GraphBuilder.CreateUAV(DstTextureARDG) : nullptr;
				FRDGTextureUAV* DstTextureBUAV = DstTextureBRDG ? GraphBuilder.CreateUAV(DstTextureBRDG) : nullptr;

				FRDGBufferSRV* OccupancyBitsBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(OccupancyBitsUploadBuffer));
				FRDGBufferSRV* TileDataOffsetsBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(TileDataOffsetsUploadBuffer));
				FRDGBufferSRV* DstTileCoordsBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DstTileCoordsUploadBuffer));
				// Either SrcBufferARDG or SrcBufferBRDG must exist and will have at least 1 element.
				FRDGBufferSRV* DummySrcBufferSRV = SrcBufferARDG ? GraphBuilder.CreateSRV(SrcBufferARDG, FormatA) : GraphBuilder.CreateSRV(SrcBufferBRDG, FormatB);

				auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FSparseVolumeTextureUpdateFromSparseBufferCS>();

				int32 NumUploadedTiles = 0;
				int32 NumUploadedVoxelsA = 0;
				int32 NumUploadedVoxelsB = 0;

				// This is a limit on some platforms on the maximum number of texels in a texel/typed buffer. Unfortunately for R8 (1 byte) formats, this means that we can upload only 1/16 of all the texels of a 2GB texture.
				// In order to work around this issue, the data to be uploaded is split into chunks such that this limit is not violated. We can use the TileDataOffsets values to get the number of voxels per tile and then
				// just add as many tiles to the batch as we can.
				const int32 MaxNumTexelsPerResource = 1 << 27;

				while (NumUploadedTiles < NumWrittenTiles)
				{
					// Determine the number of tiles to upload in this iteration
					int32 NumTilesInThisBatch = 0;
					int32 NumVoxelsAInThisBatch = 0;
					int32 NumVoxelsBInThisBatch = 0;
					for (int32 TileIndex = NumUploadedTiles; TileIndex < NumWrittenTiles; ++TileIndex)
					{
						const int32 VoxelOffsetA = FormatSizeA > 0 ? reinterpret_cast<uint32*>(TileDataOffsetsAPtr)[TileIndex] : 0;
						const int32 VoxelOffsetB = FormatSizeB > 0 ? reinterpret_cast<uint32*>(TileDataOffsetsBPtr)[TileIndex] : 0;
						const int32 VoxelEndIndexA = FormatSizeA > 0 && (TileIndex + 1) < NumWrittenTiles ? reinterpret_cast<uint32*>(TileDataOffsetsAPtr)[TileIndex + 1] : NumWrittenVoxelsA;
						const int32 VoxelEndIndexB = FormatSizeB > 0 && (TileIndex + 1) < NumWrittenTiles ? reinterpret_cast<uint32*>(TileDataOffsetsBPtr)[TileIndex + 1] : NumWrittenVoxelsB;
						const int32 TileNumVoxelsA = VoxelEndIndexA - VoxelOffsetA;
						const int32 TileNumVoxelsB = VoxelEndIndexB - VoxelOffsetB;
						check(TileNumVoxelsA >= 0 && TileNumVoxelsA <= SVT::NumVoxelsPerPaddedTile);
						check(TileNumVoxelsB >= 0 && TileNumVoxelsB <= SVT::NumVoxelsPerPaddedTile);

						// Adding additional voxels to the batch would exceed the limit, so exit the loop and upload the data.
						if ((NumVoxelsAInThisBatch + TileNumVoxelsA) > MaxNumTexelsPerResource || (NumVoxelsBInThisBatch + TileNumVoxelsB) > MaxNumTexelsPerResource)
						{
							break;
						}

						NumTilesInThisBatch += 1;
						NumVoxelsAInThisBatch += TileNumVoxelsA;
						NumVoxelsBInThisBatch += TileNumVoxelsB;
					}

					FRDGBufferSRV* TileDataABufferSRV = nullptr;
					FRDGBufferSRV* TileDataBBufferSRV = nullptr;

					// This is the critical part: For every batch we create a SRV scoped to a range within the voxel data upload buffer still fitting within the typed buffer texel limit.
					if (FormatSizeA && NumVoxelsAInThisBatch)
					{
						FRDGBufferSRVDesc SRVDesc(SrcBufferARDG, FormatA);
						SRVDesc.StartOffsetBytes = NumUploadedVoxelsA * FormatSizeA;
						SRVDesc.NumElements = NumVoxelsAInThisBatch;
						TileDataABufferSRV = GraphBuilder.CreateSRV(SRVDesc);
					}
					if (FormatSizeB && NumVoxelsBInThisBatch)
					{
						FRDGBufferSRVDesc SRVDesc(SrcBufferBRDG, FormatB);
						SRVDesc.StartOffsetBytes = NumUploadedVoxelsB * FormatSizeB;
						SRVDesc.NumElements = NumVoxelsBInThisBatch;
						TileDataBBufferSRV = GraphBuilder.CreateSRV(SRVDesc);
					}

					FSparseVolumeTextureUpdateFromSparseBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSparseVolumeTextureUpdateFromSparseBufferCS::FParameters>();
					PassParameters->DstPhysicalTileTextureA = DstTextureAUAV ? DstTextureAUAV : DummyTextureUAV;
					PassParameters->DstPhysicalTileTextureB = DstTextureBUAV ? DstTextureBUAV : DummyTextureUAV;
					PassParameters->SrcPhysicalTileBufferA = TileDataABufferSRV ? TileDataABufferSRV : DummySrcBufferSRV;
					PassParameters->SrcPhysicalTileBufferB = TileDataBBufferSRV ? TileDataBBufferSRV : DummySrcBufferSRV;
					PassParameters->OccupancyBitsBuffer = OccupancyBitsBufferSRV;
					PassParameters->TileDataOffsetsBuffer = TileDataOffsetsBufferSRV;
					PassParameters->DstTileCoordsBuffer = DstTileCoordsBufferSRV;
					PassParameters->FallbackValueA = FallbackValueA;
					PassParameters->FallbackValueB = FallbackValueB;
					PassParameters->TileIndexOffset = NumUploadedTiles; // This lets the shader know how many tiles have already been processed in previous dispatches.
					PassParameters->SrcVoxelDataOffsetA = NumUploadedVoxelsA; // SrcVoxelDataOffsetA and SrcVoxelDataOffsetB are subtracted from the calculated voxel data buffer read indices
					PassParameters->SrcVoxelDataOffsetB = NumUploadedVoxelsB;
					PassParameters->NumTilesToCopy = NumTilesInThisBatch;
					PassParameters->BufferTileStep = MaxNumTiles;
					PassParameters->NumDispatchedGroups = FMath::Min(NumTilesInThisBatch, GRHIMaxDispatchThreadGroupsPerDimension.X);
					PassParameters->PaddedTileSize = SPARSE_VOLUME_TILE_RES_PADDED;
					PassParameters->CopyTexureMask = 0;
					PassParameters->CopyTexureMask |= FormatSizeA > 0 ? 0x1u : 0x0u;
					PassParameters->CopyTexureMask |= FormatSizeB > 0 ? 0x2u : 0x0u;

					// Disable async compute for streaming systems when MGPU is active, to work around GPU hangs
					const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GSVTStreamingAsyncCompute != 0) && (GNumExplicitGPUsForRendering == 1);

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("Upload SVT Tiles (TileCount: %u)", NumTilesInThisBatch),
						bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
						ComputeShader,
						PassParameters,
						FIntVector3(PassParameters->NumDispatchedGroups, 1, 1)
					);

					NumUploadedTiles += NumTilesInThisBatch;
					NumUploadedVoxelsA += NumVoxelsAInThisBatch;
					NumUploadedVoxelsB += NumVoxelsBInThisBatch;
				}

				check(NumUploadedTiles == NumWrittenTiles);
				check(NumUploadedVoxelsA == NumWrittenVoxelsA);
				check(NumUploadedVoxelsB == NumWrittenVoxelsB);
			}
		}
		Release();
	}

private:
	TRefCountPtr<FRDGPooledBuffer> OccupancyBitsUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> TileDataOffsetsUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> DstTileCoordsUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> TileDataAUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> TileDataBUploadBuffer;
	TArray<uint32> TileDataOffsets; // CPU-readable per-tile offsets into tile data
	uint8* OccupancyBitsAPtr = nullptr;
	uint8* OccupancyBitsBPtr = nullptr;
	uint8* TileDataOffsetsAPtr = nullptr;
	uint8* TileDataOffsetsBPtr = nullptr;
	uint8* TileCoordsPtr = nullptr;
	uint8* TileDataAPtr = nullptr;
	uint8* TileDataBPtr = nullptr;
	int32 MaxNumTiles = 0;
	int32 MaxNumVoxelsA = 0;
	int32 MaxNumVoxelsB = 0;
	EPixelFormat FormatA = PF_Unknown;
	EPixelFormat FormatB = PF_Unknown;
	int32 FormatSizeA = 0;
	int32 FormatSizeB = 0;
	int32 NumWrittenTiles = 0;
	int32 NumWrittenVoxelsA = 0;
	int32 NumWrittenVoxelsB = 0;

	void ResetState()
	{
		OccupancyBitsAPtr = nullptr;
		OccupancyBitsBPtr = nullptr;
		TileDataOffsetsAPtr = nullptr;
		TileDataOffsetsBPtr = nullptr;
		TileCoordsPtr = nullptr;
		TileDataAPtr = nullptr;
		TileDataBPtr = nullptr;
		MaxNumTiles = 0;
		MaxNumVoxelsA = 0;
		MaxNumVoxelsB = 0;
		FormatA = PF_Unknown;
		FormatB = PF_Unknown;
		FormatSizeA = 0;
		FormatSizeB = 0;
		NumWrittenTiles = 0;
		NumWrittenVoxelsA = 0;
		NumWrittenVoxelsB = 0;
	}
};

// Utility class for writing page table entries
class FPageTableUpdater
{
public:
	FPageTableUpdater()
	{
		ResetState();
	}

	void Init(FRDGBuilder& GraphBuilder, int32 InMaxNumUpdates, int32 InEstimatedNumBatches)
	{
		ResetState();
		MaxNumUpdates = InMaxNumUpdates;
		Batches.Reserve(InEstimatedNumBatches);

		// Create a new buffer if the old one is already queued into RDG.
		if (IsRegistered(GraphBuilder, UpdatesUploadBuffer))
		{
			UpdatesUploadBuffer = nullptr;
		}

		if (MaxNumUpdates > 0)
		{
			// Add EBufferUsageFlags::Dynamic to skip the unneeded copy from upload to VRAM resource on d3d12 RHI
			FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateByteAddressUploadDesc(MaxNumUpdates * 2 * sizeof(uint32));
			BufferDesc.Usage |= EBufferUsageFlags::Dynamic;
			AllocatePooledBuffer(BufferDesc, UpdatesUploadBuffer, TEXT("SparseVolumeTexture.PageTableUpdatesUploadBuffer"));

			DataPtr = (uint8*)GraphBuilder.RHICmdList.LockBuffer(UpdatesUploadBuffer->GetRHI(), 0, MaxNumUpdates * 2 * sizeof(uint32), RLM_WriteOnly);
		}
	}

	void Add_GetRef(FRHITexture* PageTable, int32 MipLevel, int32 NumUpdates, uint8*& OutCoordsPtr, uint8*& OutPayloadPtr)
	{
		check((NumWrittenUpdates + NumUpdates) <= MaxNumUpdates);
		check(DataPtr);
		FBatch* Batch = Batches.IsEmpty() ? nullptr : &Batches.Last();
		if (!Batch || Batch->PageTable != PageTable || Batch->MipLevel != MipLevel)
		{
			Batch = &Batches.Add_GetRef(FBatch(PageTable, MipLevel));
		}

		OutCoordsPtr = DataPtr + NumWrittenUpdates * sizeof(uint32);
		OutPayloadPtr = DataPtr + (MaxNumUpdates + NumWrittenUpdates) * sizeof(uint32);

		Batch->NumUpdates += NumUpdates;
		NumWrittenUpdates += NumUpdates;
	}

	void Release()
	{
		UpdatesUploadBuffer.SafeRelease();
		ResetState();
	}

	void Apply(FRDGBuilder& GraphBuilder)
	{
		if (MaxNumUpdates > 0)
		{
			GraphBuilder.RHICmdList.UnlockBuffer(UpdatesUploadBuffer->GetRHI());

			if (NumWrittenUpdates > 0)
			{
				// Disable async compute for streaming systems when MGPU is active, to work around GPU hangs
				const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GSVTStreamingAsyncCompute != 0) && (GNumExplicitGPUsForRendering == 1);
				auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FSparseVolumeTextureUpdatePageTableCS>();

				uint32 UpdatesOffset = 0;
				for (const FBatch& Batch : Batches)
				{
					FRDGTexture* PageTableRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Batch.PageTable, TEXT("SparseVolumeTexture.PageTableTexture")));
					FRDGTextureUAV* PageTableUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(PageTableRDG, Batch.MipLevel, PF_R32_UINT));
					FRDGBufferSRV* UpdatesBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(UpdatesUploadBuffer));

					FSparseVolumeTextureUpdatePageTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSparseVolumeTextureUpdatePageTableCS::FParameters>();
					PassParameters->PageTable = PageTableUAV;
					PassParameters->PageTableUpdates = UpdatesBufferSRV;
					PassParameters->UpdateCoordOffset = UpdatesOffset;
					PassParameters->UpdatePayloadOffset = MaxNumUpdates + UpdatesOffset;
					PassParameters->NumUpdates = Batch.NumUpdates;

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("Update SVT PageTable (UpdateCount: %u)", Batch.NumUpdates),
						bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(Batch.NumUpdates, 64)
					);

					UpdatesOffset += Batch.NumUpdates;
				}
			}
		}
		
		Release();
	}

private:
	struct FBatch
	{
		FRHITexture* PageTable;
		int32 MipLevel;
		int32 NumUpdates;

		FBatch() = default;
		FBatch(FRHITexture* InPageTable, int32 InMipLevel) : PageTable(InPageTable), MipLevel(InMipLevel), NumUpdates(0) {}
	};

	TRefCountPtr<FRDGPooledBuffer> UpdatesUploadBuffer;
	TArray<FBatch> Batches;
	uint8* DataPtr = nullptr;
	int32 NumWrittenUpdates = 0;
	int32 MaxNumUpdates = 0;


	void ResetState()
	{
		Batches.Reset();
		DataPtr = nullptr;
		NumWrittenUpdates = 0;
		MaxNumUpdates = 0;
	}
};

// Updates entries in the streaming info buffers of multiple SVTs
class FStreamingInfoBufferUpdater
{
public:
	FStreamingInfoBufferUpdater()
	{
		ResetState();
	}

	void Add(TRefCountPtr<FRDGPooledBuffer> StreamingInfoBuffer, int32 FrameIndex, int32 LowestResidentMipLevel)
	{
		FBatch* Batch = Batches.IsEmpty() ? nullptr : &Batches.Last();
		if (!Batch || Batch->StreamingInfoBuffer != StreamingInfoBuffer)
		{
			Batch = &Batches.Add_GetRef(FBatch(StreamingInfoBuffer, Updates.Num()));
		}

		Updates.Add(FrameIndex);
		Updates.Add(LowestResidentMipLevel);

		++Batch->NumUpdates;
	}

	void Apply(FRDGBuilder& GraphBuilder)
	{
		if (!Updates.IsEmpty())
		{
			TRefCountPtr<FRDGPooledBuffer> UpdatesUploadBuffer;
			{
				// Add EBufferUsageFlags::Dynamic to skip the unneeded copy from upload to VRAM resource on d3d12 RHI
				FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateByteAddressUploadDesc(Updates.Num() * sizeof(uint32));
				BufferDesc.Usage |= EBufferUsageFlags::Dynamic;
				AllocatePooledBuffer(BufferDesc, UpdatesUploadBuffer, TEXT("SparseVolumeTexture.StreamingInfoUploadBuffer"));

				void* DataPtr = GraphBuilder.RHICmdList.LockBuffer(UpdatesUploadBuffer->GetRHI(), 0, Updates.Num() * sizeof(uint32), RLM_WriteOnly);
				FMemory::Memcpy(DataPtr, Updates.GetData(), Updates.Num() * sizeof(uint32));
				GraphBuilder.RHICmdList.UnlockBuffer(UpdatesUploadBuffer->GetRHI());
			}

			// Disable async compute for streaming systems when MGPU is active, to work around GPU hangs
			const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GSVTStreamingAsyncCompute != 0) && (GNumExplicitGPUsForRendering == 1);
			auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FSparseVolumeTextureUpdateStreamingInfoBufferCS>();

			uint32 UpdatesOffset = 0;
			for (const FBatch& Batch : Batches)
			{
				FRDGBufferUAV* StreamingInfoBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(Batch.StreamingInfoBuffer), PF_R32_UINT);
				FRDGBufferSRV* UpdatesBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(UpdatesUploadBuffer));

				FSparseVolumeTextureUpdateStreamingInfoBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSparseVolumeTextureUpdateStreamingInfoBufferCS::FParameters>();
				PassParameters->StreamingInfoBuffer = StreamingInfoBufferUAV;
				PassParameters->StreamingInfoBufferUpdates = UpdatesBufferSRV;
				PassParameters->UpdateOffset = UpdatesOffset;
				PassParameters->NumUpdates = Batch.NumUpdates;

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Update SVT StreamingInfoBuffer (UpdateCount: %u)", Batch.NumUpdates),
					bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount(Batch.NumUpdates, 64)
				);

				UpdatesOffset += Batch.NumUpdates;
			}
		}
		
		ResetState();
	}

private:
	struct FBatch
	{
		TRefCountPtr<FRDGPooledBuffer> StreamingInfoBuffer;
		int32 UpdatesOffset;
		int32 NumUpdates;

		FBatch() = default;
		FBatch(TRefCountPtr<FRDGPooledBuffer> InStreamingInfoBuffer, int32 InUpdatesOffset) : StreamingInfoBuffer(InStreamingInfoBuffer), UpdatesOffset(InUpdatesOffset), NumUpdates(0) {}
	};

	TArray<FBatch> Batches;
	TArray<uint32> Updates;

	void ResetState()
	{
		Batches.Reset();
		Updates.Reset();
	}
};

struct FStreamingUpdateParameters
{
	FStreamingManager* StreamingManager = nullptr;
};

class FStreamingUpdateTask
{
public:
	explicit FStreamingUpdateTask(const FStreamingUpdateParameters& InParams) : Parameters(InParams) {}

	FStreamingUpdateParameters Parameters;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Parameters.StreamingManager->InstallReadyMipLevels();
	}

	static ESubsequentsMode::Type	GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type				GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId				GetStatId() const { return TStatId(); }
};

FStreamingManager::FStreamingManager()
{

}

void FStreamingManager::InitRHI(FRHICommandListBase& RHICmdList)
{
	using namespace UE::DerivedData;

	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform))
	{
		return;
	}

	MaxPendingMipLevels = GSVTStreamingMaxPendingMipLevels;
	PendingMipLevels.SetNum(MaxPendingMipLevels);
	PageTableUpdater = MakeUnique<FPageTableUpdater>();
	StreamingInfoBufferUpdater = MakeUnique<FStreamingInfoBufferUpdater>();

#if WITH_EDITORONLY_DATA
	RequestOwner = MakeUnique<FRequestOwner>(EPriority::Normal);
	RequestOwnerBlocking = MakeUnique<FRequestOwner>(EPriority::Blocking);
#endif
}

void FStreamingManager::ReleaseRHI()
{
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform))
	{
		return;
	}
}

void FStreamingManager::Add_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture)
{
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || !SparseVolumeTexture)
	{
		return;
	}

	FNewSparseVolumeTextureInfo NewSVTInfo{};
	const int32 NumFrames = SparseVolumeTexture->GetNumFrames();
	NewSVTInfo.SVT = SparseVolumeTexture;
	NewSVTInfo.FormatA = SparseVolumeTexture->GetFormat(0);
	NewSVTInfo.FormatB = SparseVolumeTexture->GetFormat(1);
	NewSVTInfo.FallbackValueA = SparseVolumeTexture->GetFallbackValue(0);
	NewSVTInfo.FallbackValueB = SparseVolumeTexture->GetFallbackValue(1);
	NewSVTInfo.NumMipLevelsGlobal = SparseVolumeTexture->GetNumMipLevels();
	NewSVTInfo.FrameInfo.SetNum(NumFrames);

	for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
	{
		USparseVolumeTextureFrame* SVTFrame = SparseVolumeTexture->GetFrame(FrameIdx);
		FFrameInfo& FrameInfo = NewSVTInfo.FrameInfo[FrameIdx];
		FrameInfo.Resources = SVTFrame->GetResources();
		FrameInfo.TextureRenderResources = SVTFrame->TextureRenderResources;
		check(FrameInfo.TextureRenderResources);
	}


	ENQUEUE_RENDER_COMMAND(SVTAdd)(
		[this, NewSVTInfoCaptured = MoveTemp(NewSVTInfo), SVTName = SparseVolumeTexture->GetName()](FRHICommandListImmediate& RHICmdList) mutable /* Required to be able to move from NewSVTInfoCaptured inside the lambda */
		{
			// We need to fully initialize the SVT streaming state (including resource creation) to ensure that valid resources exist before FillUniformBuffers() is called.
			// This is why we can't defer resource creation until BeginAsyncUpdate() is called.
			FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("SVT::FStreamingManager::Add(%s)", *SVTName));
			AddInternal(GraphBuilder, MoveTemp(NewSVTInfoCaptured));
			GraphBuilder.Execute();
		});
}

void FStreamingManager::Remove_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture)
{
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || !SparseVolumeTexture)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(SVTRemove)(
		[this, SparseVolumeTexture](FRHICommandListImmediate& RHICmdList)
		{
			RemoveInternal(SparseVolumeTexture);
		});
}

void FStreamingManager::Request_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture, float FrameIndex, int32 MipLevel, bool bBlocking)
{
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || !SparseVolumeTexture)
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(SVTRequest)(
		[this, SparseVolumeTexture, FrameIndex, MipLevel, bBlocking](FRHICommandListImmediate& RHICmdList)
		{
			Request(SparseVolumeTexture, FrameIndex, MipLevel, bBlocking);
		});
}

void FStreamingManager::Update_GameThread()
{
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform))
	{
		return;
	}
	ENQUEUE_RENDER_COMMAND(SVTUpdate)(
		[](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			const bool bForceNonAsync = true; // No need to spin up a thread if we immediately wait on it anyways.
			GStreamingManager.BeginAsyncUpdate(GraphBuilder, bForceNonAsync);
			GStreamingManager.EndAsyncUpdate(GraphBuilder);
			GraphBuilder.Execute();
		});
}

void FStreamingManager::Request(UStreamableSparseVolumeTexture* SparseVolumeTexture, float FrameIndex, int32 MipLevel, bool bBlocking)
{
	check(IsInRenderingThread());
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || !SparseVolumeTexture)
	{
		return;
	}

	FStreamingInfo* SVTInfo = FindStreamingInfo(SparseVolumeTexture);
	if (SVTInfo)
	{
		const int32 NumFrames = SVTInfo->PerFrameInfo.Num();
		const int32 FrameIndexI32 = static_cast<int32>(FrameIndex);
		if (FrameIndexI32 < 0 || FrameIndexI32 >= NumFrames)
		{
			return;
		}

		// Try to find a FStreamingWindow around the requested frame index. This will inform us about which direction we need to prefetch into.
		FStreamingWindow* StreamingWindow = nullptr;
		for (FStreamingWindow& Window : SVTInfo->StreamingWindows)
		{
			if (FMath::Abs(FrameIndex - Window.CenterFrame) <= FStreamingWindow::WindowSize)
			{
				StreamingWindow = &Window;
				break;
			}
		}
		// Found an existing window!
		if (StreamingWindow)
		{
			const bool bForward = StreamingWindow->LastCenterFrame <= FrameIndex;
			if (StreamingWindow->LastRequested < NextUpdateIndex)
			{
				StreamingWindow->LastCenterFrame = StreamingWindow->CenterFrame;
				StreamingWindow->CenterFrame = FrameIndex;
				StreamingWindow->NumRequestsThisUpdate = 1;
				StreamingWindow->LastRequested = NextUpdateIndex;
				StreamingWindow->bPlayForward = bForward;
				StreamingWindow->bPlayBackward = !bForward;
			}
			else
			{
				// Update the average center frame
				StreamingWindow->CenterFrame = (StreamingWindow->CenterFrame * StreamingWindow->NumRequestsThisUpdate + FrameIndex) / (StreamingWindow->NumRequestsThisUpdate + 1.0f);
				++StreamingWindow->NumRequestsThisUpdate;
				StreamingWindow->bPlayForward |= bForward;
				StreamingWindow->bPlayBackward |= !bForward;
			}
		}
		// No existing window. Create a new one.
		else
		{
			StreamingWindow = &SVTInfo->StreamingWindows.AddDefaulted_GetRef();
			StreamingWindow->CenterFrame = FrameIndex;
			StreamingWindow->LastCenterFrame = FrameIndex;
			StreamingWindow->NumRequestsThisUpdate = 1;
			StreamingWindow->LastRequested = NextUpdateIndex;
			StreamingWindow->bPlayForward = true; // No prior data, so just take a guess that playback is forwards
			StreamingWindow->bPlayBackward = false;
		}

		check(StreamingWindow);

		// Make sure the number of prefetched frames doesn't exceed the total number of frames.
		// Not only does this make no sense, it also breaks the wrap around logic in the loop below if there is reverse playback.
		const int32 NumPrefetchFrames = FMath::Clamp(GSVTStreamingNumPrefetchFrames, 0, NumFrames);

		// No prefetching for blocking requests. Making the prefetches blocking would increase latency even more and making them non-blocking could lead
		// to situations where lower mips are already streamed in in subsequent frames but can't be used because dependent higher mips haven't finished streaming
		// due to non-blocking requests.
		// SVT_TODO: This can still break if blocking and non-blocking requests of the same frames/mips are made to the same SVT. We would need to cancel already scheduled non-blocking requests and reissue them as blocking.
		// Or alternatively we could just block on all requests if we detect this case. If we had a single DDC request owner per request, we could just selectively wait on already scheduled non-blocking requests.
		const int32 OffsetMagnitude = !bBlocking ? NumPrefetchFrames : 0;
		const int32 LowerFrameOffset = StreamingWindow->bPlayBackward ? -OffsetMagnitude : 0;
		const int32 UpperFrameOffset = StreamingWindow->bPlayForward ? OffsetMagnitude : 0;

		for (int32 i = LowerFrameOffset; i <= UpperFrameOffset; ++i)
		{
			// Wrap around on both positive and negative numbers, assuming (i + NumFrames) >= 0. See the comment on NumPrefetchFrames.
			const int32 RequestFrameIndex = (static_cast<int32>(FrameIndex) + i + NumFrames) % NumFrames;
			const int32 RequestMipLevelOffset = FMath::Abs(i) + GSVTStreamingPrefetchMipLevelBias;
			FStreamingRequest Request;
			Request.Key.SVT = SparseVolumeTexture;
			Request.Key.FrameIndex = RequestFrameIndex;
			Request.Key.MipLevelIndex = FMath::Clamp(MipLevel + RequestMipLevelOffset, 0, SVTInfo->PerFrameInfo[RequestFrameIndex].NumMipLevels);
			Request.Priority = FMath::Max(0, OffsetMagnitude - FMath::Abs(i));
			if (bBlocking)
			{
				Request.Priority = FStreamingRequest::BlockingPriority;
			}
			AddRequest(Request);
		}

		// Clean up unused streaming windows
		SVTInfo->StreamingWindows.RemoveAll([&](const FStreamingWindow& Window) { return (NextUpdateIndex - Window.LastRequested) > 5; });
	}
}

void FStreamingManager::BeginAsyncUpdate(FRDGBuilder& GraphBuilder, bool bBlocking)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || StreamingInfo.IsEmpty())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FStreamingManager::BeginAsyncUpdate);

#if SVT_STREAMING_LOG_VERBOSE
	UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("SVT Streaming Update %i"), NextUpdateIndex);
#endif

	AsyncState = {};
	AsyncState.bUpdateActive = true;

	// For debugging, we can stream out ALL tiles
	if (GSVTStreamingEmptyPhysicalTileTextures != 0)
	{
		TArray<FLRUNode*> MipLevelsToFree;
		for (auto& Pair : StreamingInfo)
		{
			MipLevelsToFree.Reset();
			FStreamingInfo* SVTInfo = Pair.Value.Get();
			const int32 NumFrames = SVTInfo->PerFrameInfo.Num();
			const int32 NumMipLevelsGlobal = SVTInfo->NumMipLevelsGlobal;
			
			for (int32 MipLevel = 0; MipLevel < NumMipLevelsGlobal; ++MipLevel)
			{
				for (auto& Node : SVTInfo->PerMipLRULists[MipLevel])
				{
					MipLevelsToFree.Add(&Node);
				}
			}
			for (FLRUNode* Node : MipLevelsToFree)
			{
				StreamOutMipLevel(SVTInfo, Node);
			}
		}

		GSVTStreamingEmptyPhysicalTileTextures = 0;
	}

	if (GSVTStreamingPrintMemoryStats != 0)
	{
		for (auto& Pair : StreamingInfo)
		{
			double MinFrameMiB = FLT_MAX;
			double MaxFrameMiB = -FLT_MAX;
			double SumFrameMiB = 0.0;

			const int32 NumFrames = Pair.Value->PerFrameInfo.Num();
			UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("Memory stats for SVT '%p': Each mip level is printed as a tuple of [PageTable Size | VoxelData Size | Total]"), Pair.Key);

			for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
			{
				const FFrameInfo& FrameInfo = Pair.Value->PerFrameInfo[FrameIdx];
				FString Str;
				int32 TotalSize = 0;
				for (const auto& SInfo : FrameInfo.Resources->MipLevelStreamingInfo)
				{
					Str += FString::Printf(TEXT("[%5.2f KiB|%5.2f KiB|%5.2f KiB] "), SInfo.PageTableSize / 1024.0f, (SInfo.TileDataSize[0] + SInfo.TileDataSize[1]) / 1024.0f, (SInfo.PageTableSize + SInfo.TileDataSize[0] + SInfo.TileDataSize[1]) / 1024.0f);
					TotalSize += SInfo.BulkSize;
				}

				MinFrameMiB = FMath::Min(MinFrameMiB, TotalSize / 1024.0 / 1024.0);
				MaxFrameMiB = FMath::Max(MaxFrameMiB, TotalSize / 1024.0 / 1024.0);
				SumFrameMiB += TotalSize / 1024.0 / 1024.0;

				UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("SVT Frame %3i: TotalSize: %3.2f MiB %s"), FrameIdx, TotalSize / 1024.0f / 1024.0f, *Str);
			}

			UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("SVT Frame Stats: Min: %3.2f MiB, Max: %3.2f MiB, Avg: %3.2f, Total All: %3.2f MiB"), (float)MinFrameMiB, (float)MaxFrameMiB, (float)(SumFrameMiB / NumFrames), SumFrameMiB);
		}

		GSVTStreamingPrintMemoryStats = 0;
	}

	AddParentRequests();
	const int32 MaxSelectedRequests = MaxPendingMipLevels - NumPendingMipLevels;
	SelectHighestPriorityRequestsAndUpdateLRU(MaxSelectedRequests);
	IssueRequests(MaxSelectedRequests);
	AsyncState.NumReadyMipLevels = DetermineReadyMipLevels();

	// Do a first pass over all the mips to be uploaded to compute the upload buffer size requirements.
	int32 NumPageTableUpdatesTotal = 0;
	TileDataTexturesToUpdate.Reset();
	{
		const int32 StartPendingMipLevelIndex = (NextPendingMipLevelIndex + MaxPendingMipLevels - NumPendingMipLevels) % MaxPendingMipLevels;
		for (int32 i = 0; i < AsyncState.NumReadyMipLevels; ++i)
		{
			const int32 PendingMipLevelIndex = (StartPendingMipLevelIndex + i) % MaxPendingMipLevels;
			FPendingMipLevel& PendingMipLevel = PendingMipLevels[PendingMipLevelIndex];

			FStreamingInfo* SVTInfo = FindStreamingInfo(PendingMipLevel.SparseVolumeTexture);
			if (!SVTInfo || (SVTInfo->PerFrameInfo[PendingMipLevel.FrameIndex].LowestRequestedMipLevel > PendingMipLevel.MipLevelIndex))
			{
				continue; // Skip mip level install. SVT no longer exists or mip level was "streamed out" before it was even installed in the first place.
			}

			const int32 FormatSizeA = GPixelFormats[SVTInfo->FormatA].BlockBytes;
			const int32 FormatSizeB = GPixelFormats[SVTInfo->FormatB].BlockBytes;
			const FResources* Resources = SVTInfo->PerFrameInfo[PendingMipLevel.FrameIndex].Resources;
			SVTInfo->TileDataTexture->NumTilesToUpload += Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex].NumPhysicalTiles;
			SVTInfo->TileDataTexture->NumVoxelsToUploadA += FormatSizeA > 0 ? Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex].TileDataSize[0] / FormatSizeA : 0;
			SVTInfo->TileDataTexture->NumVoxelsToUploadB += FormatSizeB > 0 ? Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex].TileDataSize[1] / FormatSizeB : 0;
			TileDataTexturesToUpdate.Add(SVTInfo->TileDataTexture.Get());
			NumPageTableUpdatesTotal += Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex].PageTableSize / (2 * sizeof(uint32));
		}

		PageTableUpdater->Init(GraphBuilder, NumPageTableUpdatesTotal, 1);

		for (FTileDataTexture* TileDataTexture : TileDataTexturesToUpdate)
		{
			TileDataTexture->TileUploader->Init(GraphBuilder, TileDataTexture->NumTilesToUpload, TileDataTexture->NumVoxelsToUploadA, TileDataTexture->NumVoxelsToUploadB, TileDataTexture->FormatA, TileDataTexture->FormatB);
		}
	}

	// Start async processing
	FStreamingUpdateParameters Parameters;
	Parameters.StreamingManager = this;
	
	check(AsyncTaskEvents.IsEmpty());
	if (GSVTStreamingAsyncThread && !bBlocking)
	{
		AsyncState.bUpdateIsAsync = true;
		AsyncTaskEvents.Add(TGraphTask<FStreamingUpdateTask>::CreateTask().ConstructAndDispatchWhenReady(Parameters));
	}
	else
	{
		InstallReadyMipLevels();
	}
}

void FStreamingManager::EndAsyncUpdate(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!DoesPlatformSupportSparseVolumeTexture(GMaxRHIShaderPlatform) || StreamingInfo.IsEmpty())
	{
		return;
	}
	check(AsyncState.bUpdateActive);

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FStreamingManager::EndAsyncUpdate);

	// Wait for async processing to finish
	if (AsyncState.bUpdateIsAsync)
	{
		check(!AsyncTaskEvents.IsEmpty());
		FTaskGraphInterface::Get().WaitUntilTasksComplete(AsyncTaskEvents, ENamedThreads::GetRenderThread_Local());
	}
	AsyncTaskEvents.Empty();

	// Clear unused mip levels to 0. SVT_TODO: We can probably skip this because the page table lookup in the shader is clamped
	if (!PageTableClears.IsEmpty())
	{
		for (auto& Clear : PageTableClears)
		{
			FRDGTexture* PageTableTextureRDG = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Clear.PageTableTexture, TEXT("SparseVolumeTexture.PageTableTexture")));
			FRDGTextureUAVDesc UAVDesc(PageTableTextureRDG, static_cast<uint8>(Clear.MipLevel), PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(UAVDesc), FUintVector4(ForceInitToZero));
		}
		PageTableClears.Reset();
	}

	// Issue the actual data uploads
	for (FTileDataTexture* TileDataTexture : TileDataTexturesToUpdate)
	{
		TileDataTexture->TileUploader->ResourceUploadTo(GraphBuilder, TileDataTexture->TileDataTextureARHIRef, TileDataTexture->TileDataTextureBRHIRef, TileDataTexture->FallbackValueA, TileDataTexture->FallbackValueB);
		TileDataTexture->NumTilesToUpload = 0;
		TileDataTexture->NumVoxelsToUploadA = 0;
		TileDataTexture->NumVoxelsToUploadB = 0;
	}

	// Update streaming info buffers
	for (FStreamingInfo* SVTInfo : SVTsWithInvalidatedStreamingInfoBuffer)
	{
#if DO_CHECK
		bool bSVTInfoExists = false;
		for (const auto& Pair : StreamingInfo)
		{
			if (Pair.Value.Get() == SVTInfo)
			{
				bSVTInfoExists = true;
				break;
			}
		}
		check(bSVTInfoExists);
#endif
		
		for (TConstSetBitIterator It(SVTInfo->DirtyStreamingInfoData); It; ++It)
		{
			const int32 FrameIndex = It.GetIndex();
			StreamingInfoBufferUpdater->Add(SVTInfo->StreamingInfoBuffer, FrameIndex, SVTInfo->PerFrameInfo[FrameIndex].LowestResidentMipLevel);
		}
	}
	SVTsWithInvalidatedStreamingInfoBuffer.Reset();
	StreamingInfoBufferUpdater->Apply(GraphBuilder);

	PageTableUpdater->Apply(GraphBuilder);

	check(AsyncState.NumReadyMipLevels <= NumPendingMipLevels);
	NumPendingMipLevels -= AsyncState.NumReadyMipLevels;
	++NextUpdateIndex;
	AsyncState.bUpdateActive = false;
	AsyncState.bUpdateIsAsync = false;

#if DO_CHECK
	for (const auto& Pair : StreamingInfo)
	{
#if SVT_STREAMING_LOG_VERBOSE
		FString ResidentMipLevelsStr = TEXT("");
#endif
		const int32 NumFrames = Pair.Value->PerFrameInfo.Num();
		for (int32 FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
		{
			const auto& FrameInfo = Pair.Value->PerFrameInfo[FrameIndex];
			check(FrameInfo.LowestResidentMipLevel <= (FrameInfo.NumMipLevels - 1));
			check(FrameInfo.LowestRequestedMipLevel <= FrameInfo.LowestResidentMipLevel);
			check(FrameInfo.TextureRenderResources->GetNumLogicalMipLevels() == FrameInfo.NumMipLevels);

#if SVT_STREAMING_LOG_VERBOSE
			ResidentMipLevelsStr += FString::Printf(TEXT("%i"), FrameInfo.LowestResidentMipLevel);
#endif
		}
#if SVT_STREAMING_LOG_VERBOSE
		UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("%s"), *ResidentMipLevelsStr);
#endif
	}
#endif // DO_CHECK
}

void FStreamingManager::AddInternal(FRDGBuilder& GraphBuilder, FNewSparseVolumeTextureInfo&& NewSVTInfo)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	if (!ensure(!StreamingInfo.Contains(NewSVTInfo.SVT)))
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FStreamingManager::AddInternal);

	const int32 NumFrames = NewSVTInfo.FrameInfo.Num();

	FStreamingInfo& SVTInfo = *StreamingInfo.Emplace(NewSVTInfo.SVT, MakeUnique<FStreamingInfo>());
	SVTInfo.FormatA = NewSVTInfo.FormatA;
	SVTInfo.FormatB = NewSVTInfo.FormatB;
	SVTInfo.FallbackValueA = NewSVTInfo.FallbackValueA;
	SVTInfo.FallbackValueB = NewSVTInfo.FallbackValueB;
	SVTInfo.NumMipLevelsGlobal = NewSVTInfo.NumMipLevelsGlobal;
	SVTInfo.LastRequested = 0;
	SVTInfo.PerFrameInfo = MoveTemp(NewSVTInfo.FrameInfo);
	SVTInfo.LRUNodes.SetNum(NumFrames * SVTInfo.NumMipLevelsGlobal);
	SVTInfo.PerMipLRULists.SetNum(SVTInfo.NumMipLevelsGlobal);

	const int32 FormatSizes[] = { GPixelFormats[SVTInfo.FormatA].BlockBytes, GPixelFormats[SVTInfo.FormatB].BlockBytes };

	int32 NumRootPhysicalTiles = 0;
	int32 NumRootVoxelsA = 0;
	int32 NumRootVoxelsB = 0;
	int32 MaxNumPhysicalTiles = 0;
	for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
	{
		FFrameInfo& FrameInfo = SVTInfo.PerFrameInfo[FrameIdx];
		check(FrameInfo.TextureRenderResources && FrameInfo.TextureRenderResources->IsInitialized());
		const FResources* Resources = FrameInfo.Resources;

		FrameInfo.NumMipLevels = Resources->MipLevelStreamingInfo.Num();
		FrameInfo.LowestRequestedMipLevel = FrameInfo.NumMipLevels - 1;
		FrameInfo.LowestResidentMipLevel = FrameInfo.NumMipLevels - 1;
		FrameInfo.TileAllocations.SetNum(FrameInfo.NumMipLevels);
		for (int32 MipLevel = 0; MipLevel < FrameInfo.NumMipLevels; ++MipLevel)
		{
			FrameInfo.TileAllocations[MipLevel].SetNumZeroed(Resources->MipLevelStreamingInfo[MipLevel].NumPhysicalTiles);
		}
		
		int32 NumPhysicalTiles = 0;
		for (const FMipLevelStreamingInfo& MipLevelStreamingInfo : Resources->MipLevelStreamingInfo)
		{
			NumPhysicalTiles += MipLevelStreamingInfo.NumPhysicalTiles;
		}

		MaxNumPhysicalTiles = FMath::Max(NumPhysicalTiles, MaxNumPhysicalTiles);
		if (NumPhysicalTiles > 0)
		{
			++NumRootPhysicalTiles;
			check(FormatSizes[0] == 0 || (Resources->MipLevelStreamingInfo.Last().TileDataSize[0] % FormatSizes[0]) == 0);
			check(FormatSizes[1] == 0 || (Resources->MipLevelStreamingInfo.Last().TileDataSize[1] % FormatSizes[1]) == 0);
			NumRootVoxelsA += FormatSizes[0] > 0 ? (Resources->MipLevelStreamingInfo.Last().TileDataSize[0] / FormatSizes[0]) : 0;
			NumRootVoxelsB += FormatSizes[1] > 0 ? (Resources->MipLevelStreamingInfo.Last().TileDataSize[1] / FormatSizes[1]) : 0;
		}

		for (int32 MipIdx = 0; MipIdx < SVTInfo.NumMipLevelsGlobal; ++MipIdx)
		{
			FLRUNode& LRUNode = SVTInfo.LRUNodes[FrameIdx * SVTInfo.NumMipLevelsGlobal + MipIdx];
			LRUNode.Reset();
			LRUNode.FrameIndex = FrameIdx;
			LRUNode.MipLevelIndex = MipIdx < FrameInfo.NumMipLevels ? MipIdx : INDEX_NONE;

			if ((MipIdx + 1) < FrameInfo.NumMipLevels)
			{
				LRUNode.NextHigherMipLevel = &SVTInfo.LRUNodes[FrameIdx * SVTInfo.NumMipLevelsGlobal + (MipIdx + 1)];
			}
		}
	}

	// Create RHI resources and upload root tile data
	{
		const int32 TileFactor = NumFrames <= 1 ? 1 : 3;
		const int32 NumPhysicalTilesCapacity = FMath::Max(1, NumRootPhysicalTiles + (TileFactor * MaxNumPhysicalTiles)); // Ensure a minimum size of 1
		const FIntVector3 TileDataVolumeResolution = ComputeTileDataVolumeResolution(NumPhysicalTilesCapacity);
		const FIntVector3 TileDataVolumeResolutionInTiles = TileDataVolumeResolution / SPARSE_VOLUME_TILE_RES_PADDED;

		SVTInfo.TileDataTexture = MakeUnique<FTileDataTexture>(TileDataVolumeResolutionInTiles, SVTInfo.FormatA, SVTInfo.FormatB, SVTInfo.FallbackValueA, SVTInfo.FallbackValueB);
		SVTInfo.TileDataTexture->InitResource(GraphBuilder.RHICmdList);

		// Create streaming info buffer
		{
			FRDGBufferRef StreamingInfoBufferRDG = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(sizeof(uint32) * NumFrames), TEXT("SparseVolumeTexture.StreamingInfo.RHIBuffer"));
			SVTInfo.StreamingInfoBuffer = GraphBuilder.ConvertToExternalBuffer(StreamingInfoBufferRDG);
			SVTInfo.StreamingInfoBufferSRVRHIRef = SVTInfo.StreamingInfoBuffer->GetSRV();
			SVTInfo.DirtyStreamingInfoData.Init(true, NumFrames);
		}

		FTileUploader RootTileUploader;
		RootTileUploader.Init(GraphBuilder, NumRootPhysicalTiles + 1 /*null tile*/, NumRootVoxelsA, NumRootVoxelsB, SVTInfo.FormatA, SVTInfo.FormatB);

		// Allocate null tile
		{
			const uint32 NullTileCoord = SVTInfo.TileDataTexture->Allocate();
			check(NullTileCoord == 0);
			FTileUploader::FAddResult AddResult = RootTileUploader.Add_GetRef(1 /*NumTiles*/, 0 /*NumVoxelsA*/, 0 /*NumVoxelsB*/);
			FMemory::Memcpy(AddResult.PackedPhysicalTileCoordsPtr, &NullTileCoord, sizeof(NullTileCoord));
			if (SVTInfo.FormatA != PF_Unknown)
			{
				FMemory::Memzero(AddResult.OccupancyBitsPtrs[0], SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32));
				FMemory::Memzero(AddResult.TileDataOffsetsPtrs[0], sizeof(uint32));
			}
			if (SVTInfo.FormatB != PF_Unknown)
			{
				FMemory::Memzero(AddResult.OccupancyBitsPtrs[1], SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32));
				FMemory::Memzero(AddResult.TileDataOffsetsPtrs[1], sizeof(uint32));
			}
			// No need to write to TileDataPtrA and TileDataPtrB because we zeroed out all the occupancy bits.
		}

		// Process frames
		for (int32 FrameIdx = 0; FrameIdx < NumFrames; ++FrameIdx)
		{
			FFrameInfo& FrameInfo = SVTInfo.PerFrameInfo[FrameIdx];
			const FResources* Resources = FrameInfo.Resources;
			const int32 NumMipLevels = Resources->MipLevelStreamingInfo.Num();

			FrameInfo.LowestRequestedMipLevel = NumMipLevels - 1;
			FrameInfo.LowestResidentMipLevel = NumMipLevels - 1;

			// Create page table
			{
				// SVT_TODO: Currently we keep all mips of the page table resident. It would be better to stream in/out page table mips.
				const int32 NumResidentMipLevels = NumMipLevels;
				FIntVector3 PageTableResolution = Resources->Header.PageTableVolumeResolution;
				PageTableResolution = FIntVector3(FMath::Max(1, PageTableResolution.X), FMath::Max(1, PageTableResolution.Y), FMath::Max(1, PageTableResolution.Z));

				const EPixelFormat PageEntryFormat = PF_R32_UINT;
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PageTable.RHITexture"), PageTableResolution.X, PageTableResolution.Y, PageTableResolution.Z, PageEntryFormat)
					.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV)
					.SetNumMips((uint8)NumResidentMipLevels);

				FrameInfo.PageTableTextureRHIRef = RHICreateTexture(Desc);
			}

			// Initialize TextureRenderResources
			RHIUpdateTextureReference(FrameInfo.TextureRenderResources->PageTableTextureReferenceRHI, FrameInfo.PageTableTextureRHIRef);
			RHIUpdateTextureReference(FrameInfo.TextureRenderResources->PhysicalTileDataATextureReferenceRHI, SVTInfo.TileDataTexture->TileDataTextureARHIRef);
			RHIUpdateTextureReference(FrameInfo.TextureRenderResources->PhysicalTileDataBTextureReferenceRHI, SVTInfo.TileDataTexture->TileDataTextureBRHIRef);
			FrameInfo.TextureRenderResources->StreamingInfoBufferSRVRHI = SVTInfo.StreamingInfoBufferSRVRHIRef;
			FrameInfo.TextureRenderResources->Header = Resources->Header;
			FrameInfo.TextureRenderResources->TileDataTextureResolution = SVTInfo.TileDataTexture->ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
			FrameInfo.TextureRenderResources->FrameIndex = FrameIdx;
			FrameInfo.TextureRenderResources->NumLogicalMipLevels = NumMipLevels;

			// Upload root mip data and update page tables
			const FMipLevelStreamingInfo* RootStreamingInfo = !Resources->MipLevelStreamingInfo.IsEmpty() ? &Resources->MipLevelStreamingInfo.Last() : nullptr;
			if (!Resources->RootData.IsEmpty() && RootStreamingInfo)
			{
				const uint32 TileCoord = SVTInfo.TileDataTexture->Allocate();
				check(TileCoord != INDEX_NONE);
				FrameInfo.TileAllocations.Last()[0] = TileCoord;

				const int32 NumVoxelsA = FormatSizes[0] > 0 ? RootStreamingInfo->TileDataSize[0] / FormatSizes[0] : 0;
				const int32 NumVoxelsB = FormatSizes[1] > 0 ? RootStreamingInfo->TileDataSize[1] / FormatSizes[1] : 0;
				FTileUploader::FAddResult AddResult = RootTileUploader.Add_GetRef(1, NumVoxelsA, NumVoxelsB);

				FMemory::Memcpy(AddResult.PackedPhysicalTileCoordsPtr, &TileCoord, sizeof(TileCoord));
				for (int32 AttributesIdx = 0; AttributesIdx < 2; ++AttributesIdx)
				{
					if (FormatSizes[AttributesIdx] > 0)
					{
						// Occupancy bits
						const uint8* SrcOccupancyBits = Resources->RootData.GetData() + RootStreamingInfo->OccupancyBitsOffset[AttributesIdx];
						check(AddResult.OccupancyBitsPtrs[AttributesIdx]);
						FMemory::Memcpy(AddResult.OccupancyBitsPtrs[AttributesIdx], SrcOccupancyBits, RootStreamingInfo->OccupancyBitsSize[AttributesIdx]);

						// Per-tile offsets into tile data
						const uint32* SrcTileDataOffsets = reinterpret_cast<const uint32*>(Resources->RootData.GetData() + RootStreamingInfo->TileDataOffsetsOffset[AttributesIdx]);
						check(AddResult.TileDataOffsetsPtrs[AttributesIdx]);
						check(RootStreamingInfo->TileDataOffsetsSize[AttributesIdx] == sizeof(uint32));
						reinterpret_cast<uint32*>(AddResult.TileDataOffsetsPtrs[AttributesIdx])[0] = AddResult.TileDataBaseOffsets[AttributesIdx] + SrcTileDataOffsets[0];

						// Tile data
						const uint8* SrcTileData = Resources->RootData.GetData() + RootStreamingInfo->TileDataOffset[AttributesIdx];
						check(AddResult.TileDataPtrs[AttributesIdx]);
						FMemory::Memcpy(AddResult.TileDataPtrs[AttributesIdx], SrcTileData, RootStreamingInfo->TileDataSize[AttributesIdx]);
					}
				}

				// Update highest mip (1x1x1) in page table
				const FUpdateTextureRegion3D UpdateRegion(0, 0, 0, 0, 0, 0, 1, 1, 1);
				RHIUpdateTexture3D(FrameInfo.PageTableTextureRHIRef, FrameInfo.PageTableTextureRHIRef->GetDesc().NumMips - 1, UpdateRegion, sizeof(uint32), sizeof(uint32), (uint8*)&TileCoord);
			}
		}

		RootTileUploader.ResourceUploadTo(GraphBuilder, SVTInfo.TileDataTexture->TileDataTextureARHIRef, SVTInfo.TileDataTexture->TileDataTextureBRHIRef, SVTInfo.FallbackValueA, SVTInfo.FallbackValueB);
	}

	SVTsWithInvalidatedStreamingInfoBuffer.Add(&SVTInfo);

	// Add requests for all mips the first frame. This is necessary for cases where UAnimatedSparseVolumeTexture or UStaticSparseVolumeTexture
	// are directly bound to the material without getting a specific frame through USparseVolumeTextureFrame::GetFrameAndIssueStreamingRequest().
	const int32 NumMipLevelsFrame0 = SVTInfo.PerFrameInfo[0].NumMipLevels;
	for (int32 MipLevel = 0; (MipLevel + 1) < NumMipLevelsFrame0; ++MipLevel)
	{
		FStreamingRequest Request;
		Request.Key.SVT = NewSVTInfo.SVT;
		Request.Key.FrameIndex = 0;
		Request.Key.MipLevelIndex = MipLevel;
		Request.Priority = MipLevel;
		AddRequest(Request);
	}
}

void FStreamingManager::RemoveInternal(UStreamableSparseVolumeTexture* SparseVolumeTexture)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	FStreamingInfo* SVTInfo = FindStreamingInfo(SparseVolumeTexture);
	if (SVTInfo)
	{
		// Remove any requests for this SVT
		TArray<FMipLevelKey> RequestsToRemove;
		for (auto& Pair : RequestsHashTable)
		{
			if (Pair.Key.SVT == SparseVolumeTexture)
			{
				RequestsToRemove.Add(Pair.Key);
			}
		}
		for (const FMipLevelKey& Key : RequestsToRemove)
		{
			RequestsHashTable.Remove(Key);
		}

		// Cancel any pending mip levels
		for (FPendingMipLevel& PendingMipLevel : PendingMipLevels)
		{
			if (PendingMipLevel.SparseVolumeTexture == SparseVolumeTexture)
			{
				PendingMipLevel.Reset();
			}
		}

		// Release resources
		for (FFrameInfo& FrameInfo : SVTInfo->PerFrameInfo)
		{
			FrameInfo.PageTableTextureRHIRef.SafeRelease();
		}
		if (SVTInfo->TileDataTexture)
		{
			SVTInfo->TileDataTexture->ReleaseResource();
			SVTInfo->TileDataTexture.Reset();
		}

		SVTsWithInvalidatedStreamingInfoBuffer.Remove(SVTInfo);

		StreamingInfo.Remove(SparseVolumeTexture);
	}
}

bool FStreamingManager::AddRequest(const FStreamingRequest& Request)
{
	uint32* ExistingRequestPriority = RequestsHashTable.Find(Request.Key);
	if (ExistingRequestPriority)
	{
		if (Request.Priority > *ExistingRequestPriority)
		{
			*ExistingRequestPriority = Request.Priority;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		RequestsHashTable.Add(Request.Key, Request.Priority);
		return true;
	}
}

void FStreamingManager::AddParentRequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::AddParentRequests);

	ParentRequestsToAdd.Reset();
	for (const auto& Request : RequestsHashTable)
	{
		FStreamingInfo* SVTInfo = FindStreamingInfo(Request.Key.SVT);
		check(SVTInfo);
		const int32 NumStreamableMipLevels = SVTInfo->PerFrameInfo[Request.Key.FrameIndex].NumMipLevels - 1;
		uint32 Priority = Request.Value == FStreamingRequest::BlockingPriority ? FStreamingRequest::BlockingPriority : (Request.Value + 1);
		for (int32 MipLevelIndex = Request.Key.MipLevelIndex + 1; MipLevelIndex < NumStreamableMipLevels; ++MipLevelIndex)
		{
			FMipLevelKey ParentKey = Request.Key;
			ParentKey.MipLevelIndex = MipLevelIndex;
			
			uint32* ExistingParentRequestPriority = RequestsHashTable.Find(ParentKey);
			if (ExistingParentRequestPriority && Priority > *ExistingParentRequestPriority)
			{
				*ExistingParentRequestPriority = Priority;
			}
			else
			{
				ParentRequestsToAdd.Add(FStreamingRequest{ ParentKey, Priority });
			}

			if (Priority != FStreamingRequest::BlockingPriority)
			{
				++Priority;
			}
		}
	}

	for (const FStreamingRequest& Request : ParentRequestsToAdd)
	{
		AddRequest(Request);
	}
}

void FStreamingManager::SelectHighestPriorityRequestsAndUpdateLRU(int32 MaxSelectedRequests)
{
	PrioritizedRequestsHeap.Reset();
	SelectedRequests.Reset();

	if (!RequestsHashTable.IsEmpty())
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SVT::SelectHighestPriorityRequestsAndUpdateLRU);

		for (const auto& Request : RequestsHashTable)
		{
			FStreamingInfo* SVTInfo = FindStreamingInfo(Request.Key.SVT);
			check(SVTInfo);

			// Discard invalid requests: frame index out of bounds, mip level index out of bounds (or root mip level) and mip levels without any data.
			// There can never be lower mip levels with data depending on higher mip levels without any data, so discarding such requests is ok.
			if (Request.Key.FrameIndex < 0
				|| Request.Key.FrameIndex >= SVTInfo->PerFrameInfo.Num()
				|| Request.Key.MipLevelIndex < 0
				|| Request.Key.MipLevelIndex >= (SVTInfo->PerFrameInfo[Request.Key.FrameIndex].NumMipLevels - 1)
				|| SVTInfo->PerFrameInfo[Request.Key.FrameIndex].Resources->MipLevelStreamingInfo[Request.Key.MipLevelIndex].BulkSize == 0)
			{
				continue;
			}

			const int32 LRUNodeIndex = Request.Key.FrameIndex * SVTInfo->NumMipLevelsGlobal + Request.Key.MipLevelIndex;
			FLRUNode* LRUNode = &SVTInfo->LRUNodes[LRUNodeIndex];
#if DO_CHECK
			bool bFoundNodeInList = false;
			for (auto& Node : SVTInfo->PerMipLRULists[Request.Key.MipLevelIndex])
			{
				if (&Node == LRUNode)
				{
					bFoundNodeInList = true;
					break;
				}
			}
#endif

			const bool bIsAlreadyStreaming = Request.Key.MipLevelIndex >= SVTInfo->PerFrameInfo[Request.Key.FrameIndex].LowestRequestedMipLevel;
			if (bIsAlreadyStreaming)
			{
				check(bFoundNodeInList);
				// Update LastRequested and move to front of LRU
				LRUNode->LastRequested = NextUpdateIndex;

				// Unlink
				LRUNode->Remove();

				// Insert at the end of the LRU list
				SVTInfo->PerMipLRULists[Request.Key.MipLevelIndex].AddTail(LRUNode);
			}
			else
			{
				check(!bFoundNodeInList);
				PrioritizedRequestsHeap.Add(FStreamingRequest{ Request.Key, Request.Value });
			}
		}

		// Sort by priority but make sure to load higher mip levels with the same priority first. This can happen when a blocking priority is used.
		auto PriorityPredicate = [](const auto& A, const auto& B) { return A.Priority != B.Priority ? (A.Priority > B.Priority) : A.Key.MipLevelIndex > B.Key.MipLevelIndex; };
		PrioritizedRequestsHeap.Heapify(PriorityPredicate);

		while (SelectedRequests.Num() < MaxSelectedRequests && PrioritizedRequestsHeap.Num() > 0)
		{
			FStreamingRequest SelectedRequest;
			PrioritizedRequestsHeap.HeapPop(SelectedRequest, PriorityPredicate, false /*bAllowShrinking*/);

			FStreamingInfo* SVTInfo = FindStreamingInfo(SelectedRequest.Key.SVT);
			if (SVTInfo)
			{
				check(SelectedRequest.Key.FrameIndex < SVTInfo->PerFrameInfo.Num());
				check(SelectedRequest.Key.MipLevelIndex < SVTInfo->PerFrameInfo[SelectedRequest.Key.FrameIndex].NumMipLevels);
				SelectedRequests.Push(SelectedRequest);
			}
		}

		RequestsHashTable.Reset();
	}
}

void FStreamingManager::IssueRequests(int32 MaxSelectedRequests)
{
	using namespace UE::DerivedData;

	if (SelectedRequests.IsEmpty())
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::IssueRequests);

#if WITH_EDITORONLY_DATA
	TArray<FCacheGetChunkRequest> DDCRequests;
	DDCRequests.Reserve(MaxSelectedRequests);
	TArray<FCacheGetChunkRequest> DDCRequestsBlocking;
	DDCRequestsBlocking.Reserve(MaxSelectedRequests);
#endif

	FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(SelectedRequests.Num());
	bool bIssueIOBatch = false;

	for (const FStreamingRequest& SelectedRequest : SelectedRequests)
	{
		const FMipLevelKey& SelectedKey = SelectedRequest.Key;
		FStreamingInfo* SVTInfo = FindStreamingInfo(SelectedKey.SVT);
		check(SVTInfo);
		check(SVTInfo->PerFrameInfo.Num() > SelectedKey.FrameIndex && SelectedKey.FrameIndex >= 0);
		check(SVTInfo->PerFrameInfo[SelectedKey.FrameIndex].LowestRequestedMipLevel > SelectedKey.MipLevelIndex);
		const FResources* Resources = SVTInfo->PerFrameInfo[SelectedKey.FrameIndex].Resources;
		check((SelectedKey.MipLevelIndex + 1) < Resources->MipLevelStreamingInfo.Num()); // The lowest/last mip level is always resident and does not stream.
		const FMipLevelStreamingInfo& MipLevelStreamingInfo = Resources->MipLevelStreamingInfo[SelectedKey.MipLevelIndex];

		TUniquePtr<FTileDataTexture>& TileDataTexture = SVTInfo->TileDataTexture;
		check(TileDataTexture);

		// Ensure that enough tiles are available in the tile texture
		const int32 TileDataTextureCapacity = TileDataTexture->PhysicalTilesCapacity;
		const int32 NumAvailableTiles = TileDataTexture->GetNumAvailableTiles();
		const int32 NumRequiredTiles = MipLevelStreamingInfo.NumPhysicalTiles;
		if (NumAvailableTiles < NumRequiredTiles)
		{
#if SVT_STREAMING_LOG_VERBOSE
			UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("(%i)%i IssueRequests() Frame %i Mip %i: Not enough tiles available (%i) to fit mip level (%i)"), 
				NextUpdateIndex, NextPendingMipLevelIndex, SelectedKey.FrameIndex, SelectedKey.MipLevelIndex, NumAvailableTiles, NumRequiredTiles);
#endif

			// Try to free old mip levels, starting at higher resolution mips and going up the mip chain
			TArray<FLRUNode*, TInlineAllocator<16>> MipLevelsToFree;
			int32 NumNewlyAvailableTiles = 0;
			const int32 NumMipLevelsGlobal = SVTInfo->NumMipLevelsGlobal;
			for (int32 MipLevel = 0; MipLevel < NumMipLevelsGlobal && (NumAvailableTiles + NumNewlyAvailableTiles) < NumRequiredTiles; ++MipLevel)
			{
				for (auto& Node : SVTInfo->PerMipLRULists[MipLevel])
				{
					// Only free "leaf" mip levels with no higher resolution mip levels resident. Don't free mip levels requested this frame.
					if (Node.RefCount == 0 && Node.LastRequested < NextUpdateIndex)
					{
						MipLevelsToFree.Add(&Node);
						NumNewlyAvailableTiles += SVTInfo->PerFrameInfo[Node.FrameIndex].Resources->MipLevelStreamingInfo[Node.MipLevelIndex].NumPhysicalTiles;

						// Decrement ref count of mip levels higher up the chain
						FLRUNode* Dependency = Node.NextHigherMipLevel;
						while (Dependency)
						{
							check(Dependency->RefCount > 0);
							--Dependency->RefCount;
							Dependency = Dependency->NextHigherMipLevel;
						}
					}

					// Exit once we freed enough tiles
					if ((NumAvailableTiles + NumNewlyAvailableTiles) >= NumRequiredTiles)
					{
						break;
					}
				}
			}

			// Free mip levels
			for (FLRUNode* MipLevelToFree : MipLevelsToFree)
			{
				StreamOutMipLevel(SVTInfo, MipLevelToFree);
			}

			// Couldn't free enough tiles, so skip this mip level
			if ((NumAvailableTiles + NumNewlyAvailableTiles) < NumRequiredTiles)
			{
				UE_LOG(LogSparseVolumeTextureStreamingManager, Warning, TEXT("IssueRequests() SVT %p Frame %i Mip %i: Not enough tiles available (%i) to fit mip level (%i) even after freeing"),
					SelectedKey.SVT, SelectedKey.FrameIndex, SelectedKey.MipLevelIndex, (NumAvailableTiles + NumNewlyAvailableTiles), NumRequiredTiles);
				continue;
			}
		}

#if DO_CHECK
		for (auto& Pending : PendingMipLevels)
		{
			check(Pending.SparseVolumeTexture != SelectedKey.SVT || Pending.FrameIndex != SelectedKey.FrameIndex || Pending.MipLevelIndex != SelectedKey.MipLevelIndex);
		}
#endif

		const int32 PendingMipLevelIndex = NextPendingMipLevelIndex;
		FPendingMipLevel& PendingMipLevel = PendingMipLevels[PendingMipLevelIndex];
		PendingMipLevel.Reset();
		PendingMipLevel.SparseVolumeTexture = SelectedKey.SVT;
		PendingMipLevel.FrameIndex = SelectedKey.FrameIndex;
		PendingMipLevel.MipLevelIndex = SelectedKey.MipLevelIndex;
		PendingMipLevel.IssuedInFrame = NextUpdateIndex;
		PendingMipLevel.bBlocking = GSVTStreamingForceBlockingRequests || (SelectedRequest.Priority == FStreamingRequest::BlockingPriority);

		const FByteBulkData& BulkData = Resources->StreamableMipLevels;
#if WITH_EDITORONLY_DATA
		const bool bDiskRequest = (!(Resources->ResourceFlags & EResourceFlag_StreamingDataInDDC) && !BulkData.IsBulkDataLoaded());
#else
		const bool bDiskRequest = true;
#endif

#if WITH_EDITORONLY_DATA
		if (!bDiskRequest)
		{
			if (Resources->ResourceFlags & EResourceFlag_StreamingDataInDDC)
			{
				UE::DerivedData::FCacheGetChunkRequest DDCRequest = BuildDDCRequest(*Resources, MipLevelStreamingInfo, NextPendingMipLevelIndex);
				if (PendingMipLevel.bBlocking)
				{
					DDCRequestsBlocking.Add(DDCRequest);
				}
				else
				{
					DDCRequests.Add(DDCRequest);
				}
				PendingMipLevel.State = FPendingMipLevel::EState::DDC_Pending;
			}
			else
			{
				PendingMipLevel.State = FPendingMipLevel::EState::Memory;
			}
		}
		else
#endif
		{
			PendingMipLevel.RequestBuffer = FIoBuffer(MipLevelStreamingInfo.BulkSize); // SVT_TODO: Use FIoBuffer::Wrap with preallocated memory
			const EAsyncIOPriorityAndFlags Priority = PendingMipLevel.bBlocking ? AIOP_CriticalPath : AIOP_Low;
			Batch.Read(BulkData, MipLevelStreamingInfo.BulkOffset, MipLevelStreamingInfo.BulkSize, Priority, PendingMipLevel.RequestBuffer, PendingMipLevel.Request);
			bIssueIOBatch = true;

#if WITH_EDITORONLY_DATA
			PendingMipLevel.State = FPendingMipLevel::EState::Disk;
#endif
		}

		NextPendingMipLevelIndex = (NextPendingMipLevelIndex + 1) % MaxPendingMipLevels;
		check(NumPendingMipLevels < MaxPendingMipLevels);
		++NumPendingMipLevels;

		FFrameInfo& FrameInfo = SVTInfo->PerFrameInfo[SelectedKey.FrameIndex];

		// Allocate tiles in the tile data texture
		{
			TArray<uint32>& TileAllocations = FrameInfo.TileAllocations[SelectedKey.MipLevelIndex];
			check(TileAllocations.Num() == NumRequiredTiles);
			for (int32 TileIdx = 0; TileIdx < NumRequiredTiles; ++TileIdx)
			{
				const int32 TileCoord = TileDataTexture->Allocate();
				check(TileCoord != INDEX_NONE);
				TileAllocations[TileIdx] = TileCoord;
			}
		}

		// Add to tail of LRU list
		{
			const int32 LRUNodeIndex = SelectedKey.FrameIndex * SVTInfo->NumMipLevelsGlobal + SelectedKey.MipLevelIndex;
			FLRUNode* LRUNode = &SVTInfo->LRUNodes[LRUNodeIndex];
			check(!LRUNode->IsInList());
			LRUNode->LastRequested = NextUpdateIndex;
			LRUNode->PendingMipLevelIndex = PendingMipLevelIndex;

			FLRUNode* Dependency = LRUNode->NextHigherMipLevel;
			while (Dependency)
			{
				++Dependency->RefCount;
				Dependency = Dependency->NextHigherMipLevel;
			}

			SVTInfo->PerMipLRULists[SelectedKey.MipLevelIndex].AddTail(LRUNode);
		}

#if SVT_STREAMING_LOG_VERBOSE
		UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("(%i)%i StreamIn Frame %i OldReqMip %i, NewReqMip %i, ResMip %i"),
			PendingMipLevel.IssuedInFrame, PendingMipLevelIndex,
			SelectedKey.FrameIndex, 
			FrameInfo.LowestRequestedMipLevel, SelectedKey.MipLevelIndex, 
			FrameInfo.LowestResidentMipLevel);
#endif

		check(FrameInfo.LowestRequestedMipLevel == (SelectedKey.MipLevelIndex + 1));
		FrameInfo.LowestRequestedMipLevel = SelectedKey.MipLevelIndex;
	}

#if WITH_EDITORONLY_DATA
	if (!DDCRequests.IsEmpty())
	{
		RequestDDCData(DDCRequests, false /*bBlocking*/);
		DDCRequests.Empty();
	}
	if (!DDCRequestsBlocking.IsEmpty())
	{
		RequestDDCData(DDCRequestsBlocking, true /*bBlocking*/);
		DDCRequestsBlocking.Empty();
	}
#endif

	if (bIssueIOBatch)
	{
		(void)Batch.Issue();
	}
}

void FStreamingManager::StreamOutMipLevel(FStreamingInfo* SVTInfo, FLRUNode* LRUNode)
{
	const int32 FrameIndex = LRUNode->FrameIndex;
	const int32 MipLevelIndex = LRUNode->MipLevelIndex;

	FFrameInfo& FrameInfo = SVTInfo->PerFrameInfo[FrameIndex];

	check(FrameInfo.LowestResidentMipLevel >= MipLevelIndex); // mip might not have streamed in yet, so use >= instead of ==
	check(FrameInfo.LowestRequestedMipLevel == MipLevelIndex);

	// Cancel potential IO request
	check((MipLevelIndex < FrameInfo.LowestResidentMipLevel) == (LRUNode->PendingMipLevelIndex != INDEX_NONE));
	if (LRUNode->PendingMipLevelIndex != INDEX_NONE)
	{
		PendingMipLevels[LRUNode->PendingMipLevelIndex].Reset();
		LRUNode->PendingMipLevelIndex = INDEX_NONE;
	}

	const int32 NewLowestRequestedMipLevel = MipLevelIndex + 1;
	const int32 NewLowestResidentMipLevel = FMath::Max(MipLevelIndex + 1, FrameInfo.LowestResidentMipLevel);
#if SVT_STREAMING_LOG_VERBOSE
	UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("(%i)%i StreamOut Frame %i OldReqMip %i, NewReqMip %i, OldResMip %i, NewResMip %i"),
		NextUpdateIndex, NextPendingMipLevelIndex,
		FrameIndex,
		FrameInfo.LowestRequestedMipLevel, NewLowestRequestedMipLevel,
		FrameInfo.LowestResidentMipLevel, NewLowestResidentMipLevel);
#endif

	// Only clear page table mip if the mip to be freed is actually resident.
	const bool bNeedsPageTableClear = MipLevelIndex >= FrameInfo.LowestResidentMipLevel;
	FrameInfo.LowestRequestedMipLevel = NewLowestRequestedMipLevel;
	FrameInfo.LowestResidentMipLevel = NewLowestResidentMipLevel;
	
	// Update the streaming info buffer data
	SVTInfo->DirtyStreamingInfoData[FrameIndex] = true;
	SVTsWithInvalidatedStreamingInfoBuffer.Add(SVTInfo);

	// Unlink
	LRUNode->Remove();
	LRUNode->LastRequested = INDEX_NONE;

	if (bNeedsPageTableClear)
	{
		PageTableClears.Push({ FrameInfo.PageTableTextureRHIRef, MipLevelIndex });
	}

	// Free allocated tiles
	for (uint32& TileCoord : FrameInfo.TileAllocations[MipLevelIndex])
	{
		SVTInfo->TileDataTexture->Free(TileCoord);
		TileCoord = 0;
	}
}

int32 FStreamingManager::DetermineReadyMipLevels()
{
	using namespace UE::DerivedData;

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::DetermineReadyMipLevels);

	const int32 StartPendingMipLevelIndex = (NextPendingMipLevelIndex + MaxPendingMipLevels - NumPendingMipLevels) % MaxPendingMipLevels;
	int32 NumReadyMipLevels = 0;

	for (int32 i = 0; i < NumPendingMipLevels; ++i)
	{
		const int32 PendingMipLevelIndex = (StartPendingMipLevelIndex + i) % MaxPendingMipLevels;
		FPendingMipLevel& PendingMipLevel = PendingMipLevels[PendingMipLevelIndex];

		FStreamingInfo* SVTInfo = FindStreamingInfo(PendingMipLevel.SparseVolumeTexture);
		if (!SVTInfo)
		{
#if WITH_EDITORONLY_DATA
			// Resource is no longer there. Just mark as ready so it will be skipped later
			PendingMipLevel.State = FPendingMipLevel::EState::DDC_Ready;
#endif
			continue; 
		}

		const FResources* Resources = SVTInfo->PerFrameInfo[PendingMipLevel.FrameIndex].Resources;

#if WITH_EDITORONLY_DATA
		if (PendingMipLevel.State == FPendingMipLevel::EState::DDC_Ready)
		{
			if (PendingMipLevel.RetryCount > 0)
			{
				check(SVTInfo);
				UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("SVT DDC retry succeeded for '%s' (frame %i, mip %i) on %i attempt."), 
					*Resources->ResourceName, PendingMipLevel.FrameIndex, PendingMipLevel.MipLevelIndex, PendingMipLevel.RetryCount);
			}
		}
		else if (PendingMipLevel.State == FPendingMipLevel::EState::DDC_Pending)
		{
			break;
		}
		else if (PendingMipLevel.State == FPendingMipLevel::EState::DDC_Failed)
		{
			PendingMipLevel.State = FPendingMipLevel::EState::DDC_Pending;

			if (PendingMipLevel.RetryCount == 0) // Only warn on first retry to prevent spam
			{
				UE_LOG(LogSparseVolumeTextureStreamingManager, Warning, TEXT("SVT DDC request failed for '%s' (frame %i, mip %i). Retrying..."),
					*Resources->ResourceName, PendingMipLevel.FrameIndex, PendingMipLevel.MipLevelIndex);
			}

			const FMipLevelStreamingInfo& MipLevelStreamingInfo = Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex];
			FCacheGetChunkRequest Request = BuildDDCRequest(*Resources, MipLevelStreamingInfo, PendingMipLevelIndex);
			const bool bBlocking = GSVTStreamingForceBlockingRequests || PendingMipLevel.bBlocking;
			RequestDDCData(MakeArrayView(&Request, 1), bBlocking);

			++PendingMipLevel.RetryCount;
			break;
		}
		else if (PendingMipLevel.State == FPendingMipLevel::EState::Memory)
		{
			// Memory is always ready
		}
		else
#endif // WITH_EDITORONLY_DATA
		{
#if WITH_EDITORONLY_DATA
			check(PendingMipLevel.State == FPendingMipLevel::EState::Disk);
#endif
			if (PendingMipLevel.Request.IsCompleted())
			{
				if (!PendingMipLevel.Request.IsOk())
				{
					// Retry if IO request failed for some reason
					const FMipLevelStreamingInfo& MipLevelStreamingInfo = Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex];
					UE_LOG(LogSparseVolumeTextureStreamingManager, Warning, TEXT("SVT IO request failed for %p (frame %i, mip %i, offset %i, size %i). Retrying..."),
						PendingMipLevel.SparseVolumeTexture, PendingMipLevel.FrameIndex, PendingMipLevel.MipLevelIndex, MipLevelStreamingInfo.BulkOffset, MipLevelStreamingInfo.BulkSize);
					
					FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(1);
					Batch.Read(Resources->StreamableMipLevels, MipLevelStreamingInfo.BulkOffset, MipLevelStreamingInfo.BulkSize, AIOP_Low, PendingMipLevel.RequestBuffer, PendingMipLevel.Request);
					(void)Batch.Issue();
					break;
				}
			}
			else
			{
				break;
			}
		}

		++NumReadyMipLevels;
	}

	return NumReadyMipLevels;
}

void FStreamingManager::InstallReadyMipLevels()
{
	check(AsyncState.bUpdateActive);
	check(AsyncState.NumReadyMipLevels <= PendingMipLevels.Num())
	if (AsyncState.NumReadyMipLevels <= 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(SVT::InstallReadyMipLevels);

	UploadTasks.Reset();
	UploadTasks.Reserve(AsyncState.NumReadyMipLevels * 2 /*slack for splitting large uploads*/);
	UploadCleanupTasks.Reset();

#if WITH_EDITORONLY_DATA
	TMap<const FResources*, const uint8*> ResourceToBulkPointer;
#endif

	// Do a second pass over all ready mip levels, claiming memory in the upload buffers and creating FUploadTasks
	const int32 StartPendingMipLevelIndex = (NextPendingMipLevelIndex + MaxPendingMipLevels - NumPendingMipLevels) % MaxPendingMipLevels;
	for (int32 i = 0; i < AsyncState.NumReadyMipLevels; ++i)
	{
		const int32 PendingMipLevelIndex = (StartPendingMipLevelIndex + i) % MaxPendingMipLevels;
		FPendingMipLevel& PendingMipLevel = PendingMipLevels[PendingMipLevelIndex];

		FStreamingInfo* SVTInfo = FindStreamingInfo(PendingMipLevel.SparseVolumeTexture);
		if (!SVTInfo || (SVTInfo->PerFrameInfo[PendingMipLevel.FrameIndex].LowestRequestedMipLevel > PendingMipLevel.MipLevelIndex))
		{
			PendingMipLevel.Reset();
			continue; // Skip mip level install. SVT no longer exists or mip level was "streamed out" before it was even installed in the first place.
		}

		FFrameInfo& FrameInfo = SVTInfo->PerFrameInfo[PendingMipLevel.FrameIndex];
		const FResources* Resources = FrameInfo.Resources;
		const FMipLevelStreamingInfo& MipLevelStreamingInfo = Resources->MipLevelStreamingInfo[PendingMipLevel.MipLevelIndex];

		const uint8* SrcPtr = nullptr;

#if WITH_EDITORONLY_DATA
		if (PendingMipLevel.State == FPendingMipLevel::EState::DDC_Ready)
		{
			check(Resources->ResourceFlags & EResourceFlag_StreamingDataInDDC);
			SrcPtr = (const uint8*)PendingMipLevel.SharedBuffer.GetData();
		}
		else if (PendingMipLevel.State == FPendingMipLevel::EState::Memory)
		{
			const uint8** BulkDataPtrPtr = ResourceToBulkPointer.Find(Resources);
			if (BulkDataPtrPtr)
			{
				SrcPtr = *BulkDataPtrPtr + MipLevelStreamingInfo.BulkOffset;
			}
			else
			{
				const FByteBulkData& BulkData = Resources->StreamableMipLevels;
				check(BulkData.IsBulkDataLoaded() && BulkData.GetBulkDataSize() > 0);
				const uint8* BulkDataPtr = (const uint8*)BulkData.LockReadOnly();
				ResourceToBulkPointer.Add(Resources, BulkDataPtr);
				SrcPtr = BulkDataPtr + MipLevelStreamingInfo.BulkOffset;
			}
		}
		else
#endif
		{
#if WITH_EDITORONLY_DATA
			check(PendingMipLevel.State == FPendingMipLevel::EState::Disk);
#endif
			SrcPtr = PendingMipLevel.RequestBuffer.GetData();
		}

		check(SrcPtr);

		const int32 FormatSizeA = GPixelFormats[SVTInfo->FormatA].BlockBytes;
		const int32 FormatSizeB = GPixelFormats[SVTInfo->FormatB].BlockBytes;

		const int32 NumPhysicalTiles = MipLevelStreamingInfo.NumPhysicalTiles;
		const int32 NumVoxelsA = FormatSizeA > 0 ? MipLevelStreamingInfo.TileDataSize[0] / FormatSizeA : 0;
		const int32 NumVoxelsB = FormatSizeB > 0 ? MipLevelStreamingInfo.TileDataSize[1] / FormatSizeB : 0;
		TArray<uint32>& TileAllocations = FrameInfo.TileAllocations[PendingMipLevel.MipLevelIndex];
		check(TileAllocations.Num() == NumPhysicalTiles);
		check((MipLevelStreamingInfo.PageTableSize % (sizeof(uint32) * 2)) == 0);
		const int32 NumPageTableUpdates = MipLevelStreamingInfo.PageTableSize / (sizeof(uint32) * 2);

		FTileUploader::FAddResult TileDataAddResult = SVTInfo->TileDataTexture->TileUploader->Add_GetRef(NumPhysicalTiles, NumVoxelsA, NumVoxelsB);

		uint8* DstPageCoords = nullptr;
		uint8* DstPageEntries = nullptr;
		PageTableUpdater->Add_GetRef(FrameInfo.PageTableTextureRHIRef, PendingMipLevel.MipLevelIndex, NumPageTableUpdates, DstPageCoords, DstPageEntries);

		// Tile data
		{
			FUploadTask::FTileDataTask TileDataTask = {};
			TileDataTask.DstOccupancyBitsPtrs = TileDataAddResult.OccupancyBitsPtrs;
			TileDataTask.DstTileDataOffsetsPtrs = TileDataAddResult.TileDataOffsetsPtrs;
			TileDataTask.DstTileDataPtrs = TileDataAddResult.TileDataPtrs;
			TileDataTask.DstPhysicalTileCoords = TileDataAddResult.PackedPhysicalTileCoordsPtr;
			TileDataTask.SrcOccupancyBitsPtrs[0] = SrcPtr + MipLevelStreamingInfo.OccupancyBitsOffset[0];
			TileDataTask.SrcOccupancyBitsPtrs[1] = SrcPtr + MipLevelStreamingInfo.OccupancyBitsOffset[1];
			TileDataTask.SrcTileDataOffsetsPtrs[0] = SrcPtr + MipLevelStreamingInfo.TileDataOffsetsOffset[0];
			TileDataTask.SrcTileDataOffsetsPtrs[1] = SrcPtr + MipLevelStreamingInfo.TileDataOffsetsOffset[1];
			TileDataTask.SrcTileDataPtrs[0] = SrcPtr + MipLevelStreamingInfo.TileDataOffset[0];
			TileDataTask.SrcTileDataPtrs[1] = SrcPtr + MipLevelStreamingInfo.TileDataOffset[1];
			TileDataTask.SrcPhysicalTileCoords = reinterpret_cast<const uint8*>(TileAllocations.GetData());
			TileDataTask.TileDataBaseOffsets = TileDataAddResult.TileDataBaseOffsets;
			TileDataTask.TileDataSizes = MipLevelStreamingInfo.TileDataSize;
			TileDataTask.NumPhysicalTiles = NumPhysicalTiles;

			FUploadTask& Task = UploadTasks.AddDefaulted_GetRef();
			Task.Union.SetSubtype<FUploadTask::FTileDataTask>(TileDataTask);
		}

		// Page table
		{
			FUploadTask::FPageTableTask PageTableTask = {};
			PageTableTask.PendingMipLevel = &PendingMipLevel;
			PageTableTask.DstPageCoords = DstPageCoords;
			PageTableTask.DstPageEntries = DstPageEntries;
			PageTableTask.SrcPageCoords = SrcPtr + MipLevelStreamingInfo.PageTableOffset;
			PageTableTask.SrcPageEntries = SrcPtr + MipLevelStreamingInfo.PageTableOffset + NumPageTableUpdates * sizeof(uint32);
			PageTableTask.NumPageTableUpdates = NumPageTableUpdates;

			FUploadTask& Task = UploadTasks.AddDefaulted_GetRef();
			Task.Union.SetSubtype<FUploadTask::FPageTableTask>(PageTableTask);
		}

		// Cleanup
		{
			UploadCleanupTasks.Add(&PendingMipLevel);
		}
	
#if SVT_STREAMING_LOG_VERBOSE
		UE_LOG(LogSparseVolumeTextureStreamingManager, Display, TEXT("(%i)%i Install Frame %i OldResMip %i, NewResMip %i, ReqMip %i"),
			PendingMipLevel.IssuedInFrame, PendingMipLevelIndex,
			PendingMipLevel.FrameIndex, 
			FrameInfo.LowestResidentMipLevel, PendingMipLevel.MipLevelIndex,
			FrameInfo.LowestRequestedMipLevel);
#endif

		check(FrameInfo.LowestResidentMipLevel == (PendingMipLevel.MipLevelIndex + 1));
		FrameInfo.LowestResidentMipLevel = PendingMipLevel.MipLevelIndex;

		// Update the streaming info buffer data
		SVTInfo->DirtyStreamingInfoData[PendingMipLevel.FrameIndex] = true;
		SVTsWithInvalidatedStreamingInfoBuffer.Add(SVTInfo);

		const int32 LRUNodeIndex = PendingMipLevel.FrameIndex * SVTInfo->NumMipLevelsGlobal + PendingMipLevel.MipLevelIndex;
		SVTInfo->LRUNodes[LRUNodeIndex].PendingMipLevelIndex = INDEX_NONE;
	}

	// Do all the memcpy's in parallel
	ParallelFor(UploadTasks.Num(), [&](int32 TaskIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FUploadTask);

			FUploadTask& Task = UploadTasks[TaskIndex];

			if (Task.Union.HasSubtype<FUploadTask::FPageTableTask>())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SVT::PageTableUpload);
				FUploadTask::FPageTableTask& PageTableTask = Task.Union.GetSubtype<FUploadTask::FPageTableTask>();
				if (PageTableTask.NumPageTableUpdates > 0)
				{
					FMemory::Memcpy(PageTableTask.DstPageCoords, PageTableTask.SrcPageCoords, PageTableTask.NumPageTableUpdates * sizeof(uint32));

					FStreamingInfo* SVTInfo = FindStreamingInfo(PageTableTask.PendingMipLevel->SparseVolumeTexture);
					TArray<uint32>& TileAllocations = SVTInfo->PerFrameInfo[PageTableTask.PendingMipLevel->FrameIndex].TileAllocations[PageTableTask.PendingMipLevel->MipLevelIndex];
					const uint32* SrcEntries = reinterpret_cast<const uint32*>(PageTableTask.SrcPageEntries);
					uint32* DstEntries = reinterpret_cast<uint32*>(PageTableTask.DstPageEntries);
					for (int32 i = 0; i < PageTableTask.NumPageTableUpdates; ++i)
					{
						DstEntries[i] = TileAllocations[SrcEntries[i]];
					}
				}
			}
			else if(Task.Union.HasSubtype<FUploadTask::FTileDataTask>())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SVT::TileDataUpload);
				FUploadTask::FTileDataTask& TileDataTask = Task.Union.GetSubtype<FUploadTask::FTileDataTask>();
				for (int32 i = 0; i < 2; ++i)
				{
					if (TileDataTask.DstOccupancyBitsPtrs[i])
					{
						FMemory::Memcpy(TileDataTask.DstOccupancyBitsPtrs[i], TileDataTask.SrcOccupancyBitsPtrs[i], TileDataTask.NumPhysicalTiles * SVT::NumOccupancyWordsPerPaddedTile * sizeof(uint32));
					}
					if (TileDataTask.TileDataSizes[i] > 0)
					{
						FMemory::Memcpy(TileDataTask.DstTileDataPtrs[i], TileDataTask.SrcTileDataPtrs[i], TileDataTask.TileDataSizes[i]);
					}
					if (TileDataTask.DstTileDataOffsetsPtrs[i])
					{
						uint32* Dst = reinterpret_cast<uint32*>(TileDataTask.DstTileDataOffsetsPtrs[i]);
						const uint32* Src = reinterpret_cast<const uint32*>(TileDataTask.SrcTileDataOffsetsPtrs[i]);
						for (int32 TileIndex = 0; TileIndex < TileDataTask.NumPhysicalTiles; ++TileIndex)
						{
							Dst[TileIndex] = Src[TileIndex] + TileDataTask.TileDataBaseOffsets[i];
						}
					}
				}
				FMemory::Memcpy(TileDataTask.DstPhysicalTileCoords, TileDataTask.SrcPhysicalTileCoords, TileDataTask.NumPhysicalTiles * sizeof(uint32));
			}
			else
			{
				checkNoEntry();
			}
		});

	ParallelFor(UploadCleanupTasks.Num(), [&](int32 TaskIndex)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SVT::FUploadCleanupTask);

			FPendingMipLevel* PendingMipLevel = UploadCleanupTasks[TaskIndex];
#if WITH_EDITORONLY_DATA
			PendingMipLevel->SharedBuffer.Reset();
#endif
			if (!PendingMipLevel->Request.IsNone())
			{
				check(PendingMipLevel->Request.IsCompleted());
				PendingMipLevel->Request.Reset();
			}
		});

#if DO_CHECK // Clear processed pending mip levels for better debugging
	for (int32 i = 0; i < AsyncState.NumReadyMipLevels; ++i)
	{
		const int32 PendingMipLevelIndex = (StartPendingMipLevelIndex + i) % MaxPendingMipLevels;
		PendingMipLevels[PendingMipLevelIndex].Reset();
	}
#endif

#if WITH_EDITORONLY_DATA
	// Unlock BulkData
	for (auto& Pair : ResourceToBulkPointer)
	{
		Pair.Key->StreamableMipLevels.Unlock();
	}
#endif
}

FStreamingManager::FStreamingInfo* FStreamingManager::FindStreamingInfo(UStreamableSparseVolumeTexture* Key)
{
	TUniquePtr<FStreamingInfo>* SVTInfoPtr = StreamingInfo.Find(Key);
	check(!SVTInfoPtr || SVTInfoPtr->Get());
	return SVTInfoPtr ? SVTInfoPtr->Get() : nullptr;
}

#if WITH_EDITORONLY_DATA

UE::DerivedData::FCacheGetChunkRequest FStreamingManager::BuildDDCRequest(const FResources& Resources, const FMipLevelStreamingInfo& MipLevelStreamingInfo, const uint32 PendingMipLevelIndex)
{
	using namespace UE::DerivedData;

	FCacheKey Key;
	Key.Bucket = FCacheBucket(TEXT("SparseVolumeTexture"));
	Key.Hash = Resources.DDCKeyHash;
	check(!Resources.DDCRawHash.IsZero());

	FCacheGetChunkRequest Request;
	Request.Id = FValueId::FromName("SparseVolumeTextureStreamingData");
	Request.Key = Key;
	Request.RawOffset = MipLevelStreamingInfo.BulkOffset;
	Request.RawSize = MipLevelStreamingInfo.BulkSize;
	Request.RawHash = Resources.DDCRawHash;
	Request.UserData = (((uint64)PendingMipLevelIndex) << uint64(32)) | (uint64)PendingMipLevels[PendingMipLevelIndex].RequestVersion;
	return Request;
}

void FStreamingManager::RequestDDCData(TConstArrayView<UE::DerivedData::FCacheGetChunkRequest> DDCRequests, bool bBlocking)
{
	using namespace UE::DerivedData;

	{
		FRequestOwner* RequestOwnerPtr = bBlocking ? RequestOwnerBlocking.Get() : RequestOwner.Get();
		FRequestBarrier Barrier(*RequestOwnerPtr);	// This is a critical section on the owner. It does not constrain ordering
		GetCache().GetChunks(DDCRequests, *RequestOwnerPtr,
			[this](FCacheGetChunkResponse&& Response)
			{
				const uint32 PendingMipLevelIndex = (uint32)(Response.UserData >> uint64(32));
				const uint32 RequestVersion = (uint32)Response.UserData;

				// In case the request returned after the mip level was already streamed out again we need to abort so that we do not overwrite data in the FPendingMipLevel slot.
				if (RequestVersion < PendingMipLevels[PendingMipLevelIndex].RequestVersion)
				{
					return;
				}

				FPendingMipLevel& PendingMipLevel = PendingMipLevels[PendingMipLevelIndex];
				check(PendingMipLevel.SparseVolumeTexture); // A valid PendingMipLevel should have a non-nullptr here

				if (Response.Status == EStatus::Ok)
				{
					PendingMipLevel.SharedBuffer = MoveTemp(Response.RawData);
					PendingMipLevel.State = FPendingMipLevel::EState::DDC_Ready;
				}
				else
				{
					PendingMipLevel.State = FPendingMipLevel::EState::DDC_Failed;
				}
			});
	}
	
	if (bBlocking)
	{
		RequestOwnerBlocking->Wait();
	}
}

#endif // WITH_EDITORONLY_DATA

FStreamingManager::FTileDataTexture::FTileDataTexture(const FIntVector3& InResolutionInTiles, EPixelFormat InFormatA, EPixelFormat InFormatB, const FVector4f& InFallbackValueA, const FVector4f& InFallbackValueB)
	: ResolutionInTiles(InResolutionInTiles), 
	PhysicalTilesCapacity(InResolutionInTiles.X * InResolutionInTiles.Y * InResolutionInTiles.Z), 
	FormatA(InFormatA),
	FormatB(InFormatB),
	FallbackValueA(InFallbackValueA),
	FallbackValueB(InFallbackValueB),
	TileUploader(MakeUnique<FTileUploader>()),
	NumTilesToUpload(0),
	NumVoxelsToUploadA(0),
	NumVoxelsToUploadB(0)
{
	const int64 MaxFormatSize = FMath::Max(GPixelFormats[FormatA].BlockBytes, GPixelFormats[FormatB].BlockBytes);
	const FIntVector3 LargestPossibleResolution = ComputeLargestPossibleTileDataVolumeResolution(MaxFormatSize);
	const int32 LargestPossiblePhysicalTilesCapacity = LargestPossibleResolution.X * LargestPossibleResolution.Y * LargestPossibleResolution.Z;
	
	// Ensure that the tile data texture(s) do not exceed the memory size and resolution limits.
	if (PhysicalTilesCapacity > LargestPossiblePhysicalTilesCapacity
		|| (ResolutionInTiles.X * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim
		|| (ResolutionInTiles.Y * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim
		|| (ResolutionInTiles.Z * SPARSE_VOLUME_TILE_RES_PADDED) > SVT::MaxVolumeTextureDim)
	{
		ResolutionInTiles = LargestPossibleResolution;
		PhysicalTilesCapacity = LargestPossiblePhysicalTilesCapacity;

		UE_LOG(LogSparseVolumeTextureStreamingManager, Warning, TEXT("Requested SparseVolumeTexture tile data texture resolution (in tiles) (%i, %i, %i) exceeds the resource size limit. Using the maximum value of (%i, %i. %i) instead."),
			InResolutionInTiles.X, InResolutionInTiles.Y, InResolutionInTiles.Z,
			LargestPossibleResolution.X, LargestPossibleResolution.Y, LargestPossibleResolution.Z);
	}

	const FIntVector3 Resolution = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	check(Resolution.X <= SVT::MaxVolumeTextureDim && Resolution.Y <= SVT::MaxVolumeTextureDim && Resolution.Z <= SVT::MaxVolumeTextureDim);
	check(((int64)Resolution.X * (int64)Resolution.Y * (int64)Resolution.Z * (int64)GPixelFormats[FormatA].BlockBytes) <= SVT::MaxResourceSize);
	check(((int64)Resolution.X * (int64)Resolution.Y * (int64)Resolution.Z * (int64)GPixelFormats[FormatB].BlockBytes) <= SVT::MaxResourceSize);
	
	TileCoords.SetNum(PhysicalTilesCapacity);

	int32 TileCoordsIndex = 0;
	for (int32 Z = 0; Z < ResolutionInTiles.Z; ++Z)
	{
		for (int32 Y = 0; Y < ResolutionInTiles.Y; ++Y)
		{
			for (int32 X = 0; X < ResolutionInTiles.X; ++X)
			{
				uint32 PackedCoord = 0;
				PackedCoord |= (X & 0xFFu);
				PackedCoord |= (Y & 0xFFu) << 8u;
				PackedCoord |= (Z & 0xFFu) << 16u;
				TileCoords[TileCoordsIndex++] = PackedCoord;
			}
		}
	}
	check(TileCoordsIndex == PhysicalTilesCapacity);
}

void FStreamingManager::FTileDataTexture::InitRHI(FRHICommandListBase&)
{
	const FIntVector3 Resolution = ResolutionInTiles * SPARSE_VOLUME_TILE_RES_PADDED;
	if (FormatA != PF_Unknown)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataA.RHITexture"), Resolution.X, Resolution.Y, Resolution.Z, FormatA)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		TileDataTextureARHIRef = RHICreateTexture(Desc);
	}
	if (FormatB != PF_Unknown)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create3D(TEXT("SparseVolumeTexture.PhysicalTileDataB.RHITexture"), Resolution.X, Resolution.Y, Resolution.Z, FormatB)
			.SetFlags(ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV);
		TileDataTextureBRHIRef = RHICreateTexture(Desc);
	}
}

void FStreamingManager::FTileDataTexture::ReleaseRHI()
{
}

}
}
