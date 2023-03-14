// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/NaniteStreamingManager.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "RenderingThread.h"
#include "UnifiedBuffer.h"
#include "CommonRenderResources.h"
#include "FileCache/FileCache.h"
#include "DistanceFieldAtlas.h"
#include "ClearQuad.h"
#include "RenderGraphUtils.h"
#include "Logging/LogMacros.h"
#include "Async/ParallelFor.h"
#include "Misc/Compression.h"

#if WITH_EDITOR
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
using namespace UE::DerivedData;
#endif

#define MAX_LEGACY_REQUESTS_PER_UPDATE		32u		// Legacy IO requests are slow and cause lots of bubbles, so we NEED to limit them.

#define MAX_REQUESTS_HASH_TABLE_SIZE		(NANITE_MAX_STREAMING_REQUESTS << 1)
#define MAX_REQUESTS_HASH_TABLE_MASK		(MAX_REQUESTS_HASH_TABLE_SIZE - 1)
#define INVALID_HASH_ENTRY					0xFFFFFFFFu

#define INVALID_RUNTIME_RESOURCE_ID			0xFFFFFFFFu
#define INVALID_PAGE_INDEX					0xFFFFFFFFu

#define MAX_RUNTIME_RESOURCE_VERSIONS_BITS	8												// Just needs to be large enough to cover maximum number of in-flight versions
#define MAX_RUNTIME_RESOURCE_VERSIONS_MASK	((1 << MAX_RUNTIME_RESOURCE_VERSIONS_BITS) - 1)	

#define MAX_RESOURCE_PREFETCH_PAGES			16

static int32 GNaniteStreamingAsync = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingAsync(
	TEXT("r.Nanite.Streaming.Async"),
	GNaniteStreamingAsync,
	TEXT("Perform most of the Nanite streaming on an asynchronous worker thread instead of the rendering thread.")
);

static float GNaniteStreamingBandwidthLimit = -1.0f;
static FAutoConsoleVariableRef CVarNaniteStreamingBandwidthLimit(
	TEXT("r.Nanite.Streaming.BandwidthLimit" ),
	GNaniteStreamingBandwidthLimit,
	TEXT("Streaming bandwidth limit in megabytes per second. Negatives values are interpreted as unlimited. ")
);

static int32 GNaniteStreamingPoolSize = 512;
static FAutoConsoleVariableRef CVarNaniteStreamingPoolSize(
	TEXT("r.Nanite.Streaming.StreamingPoolSize"),
	GNaniteStreamingPoolSize,
	TEXT("Size of streaming pool in MB. Does not include memory used for root pages."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingNumInitialRootPages = 2048;
static FAutoConsoleVariableRef CVarNaniteStreamingNumInitialRootPages(
	TEXT("r.Nanite.Streaming.NumInitialRootPages"),
	GNaniteStreamingNumInitialRootPages,
	TEXT("Number of root pages in initial allocation. Allowed to grow on demand if r.Nanite.Streaming.DynamicallyGrowAllocations is enabled."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingNumInitialImposters = 2048;
static FAutoConsoleVariableRef CVarNaniteStreamingNumInitialImposters(
	TEXT("r.Nanite.Streaming.NumInitialImposters"),
	GNaniteStreamingNumInitialImposters,
	TEXT("Number of imposters in initial allocation. Allowed to grow on demand if r.Nanite.Streaming.DynamicallyGrowAllocations is enabled."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingDynamicallyGrowAllocations = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingDynamicallyGrowAllocations(
	TEXT("r.Nanite.Streaming.DynamicallyGrowAllocations"),
	GNaniteStreamingDynamicallyGrowAllocations,
	TEXT("Determines if root page and imposter allocations are allowed to grow dynamically from initial allocation set by r.Nanite.Streaming.NumInitialRootPages and r.Nanite.Streaming.NumInitialImposters"),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingMaxPendingPages = 128;
static FAutoConsoleVariableRef CVarNaniteStreamingMaxPendingPages(
	TEXT("r.Nanite.Streaming.MaxPendingPages"),
	GNaniteStreamingMaxPendingPages,
	TEXT("Maximum number of pages that can be pending for installation."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingImposters = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingImposters(
	TEXT("r.Nanite.Streaming.Imposters"),
	GNaniteStreamingImposters,
	TEXT("Load imposters used for faster rendering of distant objects. Requires additional memory and might not be worthwhile for scenes with HLOD or no distant objects."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingMaxPageInstallsPerFrame = 128;
static FAutoConsoleVariableRef CVarNaniteStreamingMaxPageInstallsPerFrame(
	TEXT("r.Nanite.Streaming.MaxPageInstallsPerFrame"),
	GNaniteStreamingMaxPageInstallsPerFrame,
	TEXT("Maximum number of pages that can be installed per frame. Limiting this can limit the overhead of streaming."),
	ECVF_ReadOnly
);

static int32 GNaniteStreamingAsyncCompute = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingAsyncCompute(
	TEXT("r.Nanite.Streaming.AsyncCompute"),
	GNaniteStreamingAsyncCompute,
	TEXT("Schedule GPU work in async compute queue.")
);

static int32 GNaniteStreamingExplicitRequests = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingExplicitRequests(
	TEXT("r.Nanite.Streaming.Debug.ExplicitRequests"),
	GNaniteStreamingExplicitRequests,
	TEXT("Process requests coming from explicit calls to RequestNanitePages().")
);

static int32 GNaniteStreamingGPURequests = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingGPUFeedback(
	TEXT("r.Nanite.Streaming.Debug.GPURequests"),
	GNaniteStreamingGPURequests,
	TEXT("Process requests coming from GPU rendering feedback")
);

static int32 GNaniteStreamingPrefetch = 1;
static FAutoConsoleVariableRef CVarNaniteStreamingPrefetch(
	TEXT("r.Nanite.Streaming.Debug.Prefetch"),
	GNaniteStreamingPrefetch,
	TEXT("Process resource prefetch requests from calls to PrefetchResource().")
);

static_assert(NANITE_MAX_GPU_PAGES_BITS + MAX_RUNTIME_RESOURCE_VERSIONS_BITS + NANITE_STREAMING_REQUEST_MAGIC_BITS <= 32,	"Streaming request member RuntimeResourceID_Magic doesn't fit in 32 bits");
static_assert(NANITE_MAX_RESOURCE_PAGES_BITS + NANITE_MAX_GROUP_PARTS_BITS + NANITE_STREAMING_REQUEST_MAGIC_BITS <= 32,			"Streaming request member PageIndex_NumPages_Magic doesn't fit in 32 bits");

DECLARE_DWORD_COUNTER_STAT(		TEXT("Explicit Requests"),			STAT_NaniteExplicitRequests,				STATGROUP_Nanite );
DECLARE_DWORD_COUNTER_STAT(		TEXT("GPU Requests"),				STAT_NaniteGPURequests,						STATGROUP_Nanite );
DECLARE_DWORD_COUNTER_STAT(		TEXT("Unique Requests"),			STAT_NaniteUniqueRequests,					STATGROUP_Nanite );

DECLARE_DWORD_COUNTER_STAT(		TEXT("Unique New Requests"),			STAT_NaniteUniqueNewRequests,			STATGROUP_Nanite );
DECLARE_DWORD_COUNTER_STAT(		TEXT("Unique New Requests Resources"),	STAT_NaniteUniqueNewRequestsResources,	STATGROUP_Nanite );

DECLARE_DWORD_COUNTER_STAT(		TEXT("Page Installs"),				STAT_NanitePageInstalls,					STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Total Pages"),				STAT_NaniteTotalPages,						STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Registered Streaming Pages"),	STAT_NaniteRegisteredStreamingPages,		STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Installed Pages"),			STAT_NaniteInstalledPages,					STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Root Pages"),					STAT_NaniteRootPages,						STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Resources"),					STAT_NaniteResources,						STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT( TEXT("Imposters"),					STAT_NaniteImposters,						STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT(	TEXT("Peak Root Pages"),			STAT_NanitePeakRootPages,					STATGROUP_Nanite );
DECLARE_DWORD_ACCUMULATOR_STAT(	TEXT("Peak Allocated Root Pages"),	STAT_NanitePeakAllocatedRootPages,			STATGROUP_Nanite );

DECLARE_FLOAT_COUNTER_STAT(		TEXT("RootDataMB"),					STAT_NaniteRootDataMB,						STATGROUP_Nanite );
DECLARE_FLOAT_COUNTER_STAT(		TEXT("StreamingDiskIORequestsMB"),	STAT_NaniteStreamingDiskIORequestMB,		STATGROUP_Nanite );

DECLARE_LOG_CATEGORY_EXTERN(LogNaniteStreaming, Log, All);
DEFINE_LOG_CATEGORY(LogNaniteStreaming);

namespace Nanite
{
#if WITH_EDITOR
	const FValueId NaniteValueId = FValueId::FromName("NaniteStreamingData");
#endif

// Round up to smallest value greater than or equal to x of the form k*2^s where k < 2^NumSignificantBits.
// This is the same as RoundUpToPowerOfTwo when NumSignificantBits=1.
// For larger values of NumSignificantBits each po2 bucket is subdivided into 2^(NumSignificantBits-1) linear steps.
// This gives more steps while still maintaining an overall exponential structure and keeps numbers nice and round (in the po2 sense).

// Example:
// Representable values for different values of NumSignificantBits.
// 1: ..., 16, 32, 64, 128, 256, 512, ...
// 2: ..., 16, 24, 32,  48,  64,  96, ...
// 3: ..., 16, 20, 24,  28,  32,  40, ...
static uint32 RoundUpToSignificantBits(uint32 x, uint32 NumSignificantBits)
{
	check(NumSignificantBits <= 32);

	const int32_t Shift = FMath::Max((int32)FMath::CeilLogTwo(x) - (int32)NumSignificantBits, 0);
	const uint32 Mask = (1u << Shift) - 1u;
	return (x + Mask) & ~Mask;
}

class FTranscodePageToGPU_CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTranscodePageToGPU_CS);
	SHADER_USE_PARAMETER_STRUCT(FTranscodePageToGPU_CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32,								StartPageIndex)
		SHADER_PARAMETER(FIntVector4,							PageConstants)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPageInstallInfo>,InstallInfoBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>,			PageDependenciesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer,					SrcPageBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer,	DstPageBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FTranscodePageToGPU_CS, "/Engine/Private/Nanite/NaniteTranscode.usf", "TranscodePageToGPU", SF_Compute);

class FClearStreamingRequestCount_CS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClearStreamingRequestCount_CS);
	SHADER_USE_PARAMETER_STRUCT(FClearStreamingRequestCount_CS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FStreamingRequest>, OutStreamingRequests)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportNanite(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FClearStreamingRequestCount_CS, "/Engine/Private/Nanite/NaniteClusterCulling.usf", "ClearStreamingRequestCount", SF_Compute);


// Lean hash table for deduplicating requests.
// Linear probing hash table that only supports add and never grows.
// This is intended to be kept alive over the duration of the program, so allocation and clearing only has to happen once.
// TODO: Unify with VT?
class FRequestsHashTable
{
	FStreamingRequest*		HashTable;
	uint32*					ElementIndices;	// List of indices to unique elements of HashTable
	uint32					NumElements;	// Number of unique elements in HashTable
public:
	FRequestsHashTable()
	{
		check(FMath::IsPowerOfTwo(MAX_REQUESTS_HASH_TABLE_SIZE));
		HashTable = new FStreamingRequest[MAX_REQUESTS_HASH_TABLE_SIZE];
		ElementIndices = new uint32[MAX_REQUESTS_HASH_TABLE_SIZE];
		for(uint32 i = 0; i < MAX_REQUESTS_HASH_TABLE_SIZE; i++)
		{
			HashTable[i].Key.RuntimeResourceID = INVALID_RUNTIME_RESOURCE_ID;
		}
		NumElements = 0;
	}
	~FRequestsHashTable()
	{
		delete[] HashTable;
		delete[] ElementIndices;
		HashTable = nullptr;
		ElementIndices = nullptr;
	}

	FORCEINLINE void AddRequest(const FStreamingRequest& Request)
	{
		uint32 TableIndex = GetTypeHash(Request.Key) & MAX_REQUESTS_HASH_TABLE_MASK;

		while(true)
		{
			FStreamingRequest& TableEntry = HashTable[TableIndex];
			if(TableEntry.Key == Request.Key)
			{
				// Found it. Just update the key.
				TableEntry.Priority = FMath::Max( TableEntry.Priority, Request.Priority );
				return;
			}

			if(TableEntry.Key.RuntimeResourceID == INVALID_RUNTIME_RESOURCE_ID)
			{
				// Empty slot. Take it and add this to cell to the elements list.
				TableEntry = Request;
				ElementIndices[NumElements++] = TableIndex;
				return;
			}

			// Slot was taken by someone else. Move on to next slot.
			TableIndex = (TableIndex + 1) & MAX_REQUESTS_HASH_TABLE_MASK;
		}
	}

	uint32 GetNumElements() const
	{
		return NumElements;
	}

	const FStreamingRequest& GetElement(uint32 Index) const
	{
		check( Index < NumElements );
		return HashTable[ElementIndices[Index]];
	}

	// Clear by looping through unique elements. Cost is proportional to number of unique elements, not the whole table.
	void Clear()
	{
		for( uint32 i = 0; i < NumElements; i++ )
		{
			FStreamingRequest& Request		= HashTable[ ElementIndices[ i ] ];
			Request.Key.RuntimeResourceID	= INVALID_RUNTIME_RESOURCE_ID;
		}
		NumElements = 0;
	}
};

struct FPageInstallInfo
{
	uint32 SrcPageOffset;
	uint32 DstPageOffset;
	uint32 PageDependenciesStart;
	uint32 PageDependenciesNum;
};

class FStreamingPageUploader
{
	struct FAddedPageInfo
	{
		FPageInstallInfo	InstallInfo;
		FPageKey			GPUPageKey;
		uint32				InstallPassIndex;
	};
public:
	FStreamingPageUploader()
	{
		ResetState();
	}

	void Init(FRDGBuilder& GraphBuilder, uint32 InMaxPages, uint32 InMaxPageBytes, uint32 InMaxStreamingPages)
	{
		ResetState();
		MaxPages = InMaxPages;
		MaxPageBytes = InMaxPageBytes;
		MaxStreamingPages = InMaxStreamingPages;

		// Create a new set of buffers if the old set is already queued into RDG.
		if (IsRegistered(GraphBuilder, PageUploadBuffer))
		{
			PageUploadBuffer = nullptr;
			InstallInfoUploadBuffer = nullptr;
			PageDependenciesBuffer = nullptr;
		}

		uint32 PageAllocationSize = FMath::RoundUpToPowerOfTwo(MaxPageBytes);
		if (PageAllocationSize > TryGetSize(PageUploadBuffer))
		{
			AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressUploadDesc(PageAllocationSize), PageUploadBuffer, TEXT("Nanite.PageUploadBuffer"));
		}

		PageDataPtr = (uint8*)RHILockBuffer(PageUploadBuffer->GetRHI(), 0, PageAllocationSize, RLM_WriteOnly);
	}

	uint8* Add_GetRef(uint32 PageSize, uint32 DstPageOffset, const FPageKey& GPUPageKey, const TArray<uint32>& PageDependencies)
	{
		check(IsAligned(PageSize, 4));
		check(IsAligned(DstPageOffset, 4));

		const uint32 PageIndex = AddedPageInfos.Num();

		check(PageIndex < MaxPages);
		check(NextPageByteOffset + PageSize <= MaxPageBytes);

		FAddedPageInfo& Info = AddedPageInfos.AddDefaulted_GetRef();
		Info.GPUPageKey = GPUPageKey;
		Info.InstallInfo.SrcPageOffset = NextPageByteOffset;
		Info.InstallInfo.DstPageOffset = DstPageOffset;
		Info.InstallInfo.PageDependenciesStart = FlattenedPageDependencies.Num();
		Info.InstallInfo.PageDependenciesNum = PageDependencies.Num();
		Info.InstallPassIndex = 0xFFFFFFFFu;
		FlattenedPageDependencies.Append(PageDependencies);
		GPUPageKeyToAddedIndex.Add(GPUPageKey, PageIndex);
		
		uint8* ResultPtr = PageDataPtr + NextPageByteOffset;
		NextPageByteOffset += PageSize;
		
		return ResultPtr;
	}

	void Release()
	{
		InstallInfoUploadBuffer.SafeRelease();
		PageUploadBuffer.SafeRelease();
		PageDependenciesBuffer.SafeRelease();
		ResetState();
	}

	void ResourceUploadTo(FRDGBuilder& GraphBuilder, FRDGBuffer* DstBuffer)
	{
		RHIUnlockBuffer(PageUploadBuffer->GetRHI());

		const uint32 NumPages = AddedPageInfos.Num();
		if (NumPages == 0)	// This can end up getting called with NumPages = 0 when NumReadyPages > 0 and all pages early out.
		{
			ResetState();
			return;
		}

		uint32 InstallInfoAllocationSize = FMath::RoundUpToPowerOfTwo(NumPages * sizeof(FPageInstallInfo));
		if (InstallInfoAllocationSize > TryGetSize(InstallInfoUploadBuffer))
		{
			const uint32 BytesPerElement = sizeof(FPageInstallInfo);

			AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredUploadDesc(BytesPerElement, InstallInfoAllocationSize / BytesPerElement), InstallInfoUploadBuffer, TEXT("Nanite.InstallInfoUploadBuffer"));
		}

		FPageInstallInfo* InstallInfoPtr = (FPageInstallInfo*)RHILockBuffer(InstallInfoUploadBuffer->GetRHI(), 0, InstallInfoAllocationSize, RLM_WriteOnly);

		uint32 PageDependenciesAllocationSize = FMath::RoundUpToPowerOfTwo(FMath::Max(FlattenedPageDependencies.Num(), 4096) * sizeof(uint32));
		if (PageDependenciesAllocationSize > TryGetSize(PageDependenciesBuffer))
		{
			const uint32 BytesPerElement = sizeof(uint32);

			AllocatePooledBuffer(FRDGBufferDesc::CreateStructuredUploadDesc(BytesPerElement, PageDependenciesAllocationSize / BytesPerElement), PageDependenciesBuffer, TEXT("Nanite.PageDependenciesBuffer"));
		}

		uint32* PageDependenciesPtr = (uint32*)RHILockBuffer(PageDependenciesBuffer->GetRHI(), 0, PageDependenciesAllocationSize, RLM_WriteOnly);
		FMemory::Memcpy(PageDependenciesPtr, FlattenedPageDependencies.GetData(), FlattenedPageDependencies.Num() * sizeof(uint32));
		RHIUnlockBuffer(PageDependenciesBuffer->GetRHI());

		// Split page installs into passes.
		// Every pass adds the pages that no longer have any unresolved dependency.
		// Essentially a naive multi-pass topology sort, but with a low number of passes in practice.
		check(NumInstalledPagesPerPass.Num() == 0);
		uint32 NumRemainingPages = NumPages;
		while (NumRemainingPages > 0)
		{
			const uint32 CurrentPassIndex = NumInstalledPagesPerPass.Num();
			uint32 NumPassPages = 0;
			for (FAddedPageInfo& PageInfo : AddedPageInfos)
			{
				if (PageInfo.InstallPassIndex < CurrentPassIndex)
					continue;	// Page already installed in an earlier pass

				bool bMissingDependency = false;
				for (uint32 i = 0; i < PageInfo.InstallInfo.PageDependenciesNum; i++)
				{
					const uint32 GPUPageIndex = FlattenedPageDependencies[PageInfo.InstallInfo.PageDependenciesStart + i];
					const FPageKey DependencyGPUPageKey = { PageInfo.GPUPageKey.RuntimeResourceID, GPUPageIndex };
					const uint32* DependencyAddedIndexPtr = GPUPageKeyToAddedIndex.Find(DependencyGPUPageKey);

					// Check if a dependency has not yet been installed.
					// We only need to resolve dependencies in the current batch. Batches are already ordered.
					if (DependencyAddedIndexPtr && AddedPageInfos[*DependencyAddedIndexPtr].InstallPassIndex >= CurrentPassIndex)
					{
						bMissingDependency = true;
						break;
					}
				}

				if (!bMissingDependency)
				{
					*InstallInfoPtr++ = PageInfo.InstallInfo;
					PageInfo.InstallPassIndex = CurrentPassIndex;
					NumPassPages++;
				}
			}

			NumInstalledPagesPerPass.Add(NumPassPages);
			NumRemainingPages -= NumPassPages;
		}

		RHIUnlockBuffer(InstallInfoUploadBuffer->GetRHI());

		FRDGBufferSRV* PageUploadBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(PageUploadBuffer));
		FRDGBufferSRV* InstallInfoUploadBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(InstallInfoUploadBuffer));
		FRDGBufferSRV* PageDependenciesBufferSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(PageDependenciesBuffer));
		FRDGBufferUAV* DstBufferUAV = GraphBuilder.CreateUAV(DstBuffer);

		auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FTranscodePageToGPU_CS>();

		const bool bAsyncCompute = GSupportsEfficientAsyncCompute && (GNaniteStreamingAsyncCompute != 0);
		const uint32 NumPasses = NumInstalledPagesPerPass.Num();
		uint32 StartPageIndex = 0;
		for (uint32 PassIndex = 0; PassIndex < NumPasses; PassIndex++)
		{
			FTranscodePageToGPU_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTranscodePageToGPU_CS::FParameters>();
			PassParameters->InstallInfoBuffer      = InstallInfoUploadBufferSRV;
			PassParameters->PageDependenciesBuffer = PageDependenciesBufferSRV;
			PassParameters->SrcPageBuffer          = PageUploadBufferSRV;
			PassParameters->DstPageBuffer          = DstBufferUAV;
			PassParameters->StartPageIndex         = StartPageIndex;
			PassParameters->PageConstants          = FIntVector4(0, MaxStreamingPages, 0, 0);
			
			const uint32 NumPagesInPass = NumInstalledPagesPerPass[PassIndex];

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("TranscodePageToGPU (PageOffset: %u, PageCount: %u)", StartPageIndex, NumPagesInPass),
				bAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(NANITE_MAX_TRANSCODE_GROUPS_PER_PAGE, NumPagesInPass, 1));

			StartPageIndex += NumPagesInPass;
		}

		ResetState();
	}
private:
	TRefCountPtr<FRDGPooledBuffer> InstallInfoUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> PageUploadBuffer;
	TRefCountPtr<FRDGPooledBuffer> PageDependenciesBuffer;
	uint8*					PageDataPtr;
	uint32					MaxPages;
	uint32					MaxPageBytes;
	uint32					MaxStreamingPages;
	uint32					NextPageByteOffset;
	TArray<FAddedPageInfo>	AddedPageInfos;
	TMap<FPageKey, uint32>	GPUPageKeyToAddedIndex;
	TArray<uint32>			FlattenedPageDependencies;
	TArray<uint32>			NumInstalledPagesPerPass;
	
	void ResetState()
	{
		PageDataPtr = nullptr;
		MaxPages = 0;
		MaxPageBytes = 0;
		NextPageByteOffset = 0;
		AddedPageInfos.Reset();
		GPUPageKeyToAddedIndex.Reset();
		FlattenedPageDependencies.Reset();
		NumInstalledPagesPerPass.Reset();
	}
};

FStreamingManager::FStreamingManager() :
	StreamingRequestsBufferVersion(0),
	MaxStreamingPages(0),
	MaxPendingPages(0),
	MaxPageInstallsPerUpdate(0),
	MaxStreamingReadbackBuffers(4u),
	ReadbackBuffersWriteIndex(0),
	ReadbackBuffersNumPending(0),
	NextUpdateIndex(0),
	NumRegisteredStreamingPages(0),
	NumPendingPages(0),
	NextPendingPageIndex(0),
	StatNumRootPages(0),
	StatPeakRootPages(0),
	StatPeakAllocatedRootPages(0)
#if !UE_BUILD_SHIPPING
	,PrevUpdateTick(0)
#endif
#if WITH_EDITOR
	,RequestOwner(nullptr)
#endif
{
	NextRootPageVersion.SetNum(NANITE_MAX_GPU_PAGES);
}

void FStreamingManager::InitRHI()
{
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);

	MaxStreamingPages = (uint32)((uint64)GNaniteStreamingPoolSize * 1024 * 1024 / NANITE_STREAMING_PAGE_GPU_SIZE);
	check(MaxStreamingPages + GNaniteStreamingNumInitialRootPages <= NANITE_MAX_GPU_PAGES);

	MaxPendingPages = GNaniteStreamingMaxPendingPages;
	MaxPageInstallsPerUpdate = (uint32)FMath::Min(GNaniteStreamingMaxPageInstallsPerFrame, GNaniteStreamingMaxPendingPages);

	StreamingRequestReadbackBuffers.AddZeroed( MaxStreamingReadbackBuffers );

	// Initialize pages
	StreamingPageInfos.AddUninitialized( MaxStreamingPages );
	for( uint32 i = 0; i < MaxStreamingPages; i++ )
	{
		FStreamingPageInfo& Page = StreamingPageInfos[ i ];
		Page.RegisteredKey = { INVALID_RUNTIME_RESOURCE_ID, INVALID_PAGE_INDEX };
		Page.ResidentKey = { INVALID_RUNTIME_RESOURCE_ID, INVALID_PAGE_INDEX };
		Page.GPUPageIndex = i;
	}

	// Add pages to free list
	StreamingPageInfoFreeList = &StreamingPageInfos[0];
	for( uint32 i = 1; i < MaxStreamingPages; i++ )
	{
		StreamingPageInfos[ i - 1 ].Next = &StreamingPageInfos[ i ];
	}
	StreamingPageInfos[ MaxStreamingPages - 1 ].Next = nullptr;

	// Initialize LRU sentinels
	StreamingPageLRU.RegisteredKey		= { INVALID_RUNTIME_RESOURCE_ID, INVALID_PAGE_INDEX };
	StreamingPageLRU.ResidentKey		= { INVALID_RUNTIME_RESOURCE_ID, INVALID_PAGE_INDEX };
	StreamingPageLRU.GPUPageIndex		= INVALID_PAGE_INDEX;
	StreamingPageLRU.LatestUpdateIndex	= 0xFFFFFFFFu;
	StreamingPageLRU.RefCount			= 0xFFFFFFFFu;
	StreamingPageLRU.Next				= &StreamingPageLRU;
	StreamingPageLRU.Prev				= &StreamingPageLRU;

	StreamingPageFixupChunks.SetNum( MaxStreamingPages );

	PendingPages.SetNum( MaxPendingPages );

#if !WITH_EDITOR
	PendingPageStagingMemory.SetNumUninitialized(MaxPendingPages * NANITE_MAX_PAGE_DISK_SIZE);
#endif

	RequestsHashTable	= new FRequestsHashTable();
	PageUploader		= new FStreamingPageUploader();

	ImposterData.DataBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(4), TEXT("Nanite.StreamingManager.ImposterDataInitial"));
	ClusterPageData.DataBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(4), TEXT("Nanite.StreamingManager.ClusterPageDataInitial"));
	Hierarchy.DataBuffer = AllocatePooledBuffer(FRDGBufferDesc::CreateByteAddressDesc(4), TEXT("Nanite.StreamingManager.HierarchyDataInitial"));

#if WITH_EDITOR
	RequestOwner = new FRequestOwner(EPriority::Normal);
#endif
}

void FStreamingManager::ReleaseRHI()
{
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

#if WITH_EDITOR
	delete RequestOwner;
	RequestOwner = nullptr;
#endif

	LLM_SCOPE_BYTAG(Nanite);
	for (FRHIGPUBufferReadback*& ReadbackBuffer : StreamingRequestReadbackBuffers)
	{
		if (ReadbackBuffer != nullptr)
		{
			delete ReadbackBuffer;
			ReadbackBuffer = nullptr;
		}
	}

	for (FFixupChunk* FixupChunk : StreamingPageFixupChunks)
	{
		FMemory::Free(FixupChunk);
	}

	ImposterData.Release();
	ClusterPageData.Release();
	Hierarchy.Release();
	ClusterFixupUploadBuffer = {};
	StreamingRequestsBuffer.SafeRelease();

	delete RequestsHashTable;
	delete PageUploader;
}

void FStreamingManager::Add( FResources* Resources )
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);

	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	if (Resources->RuntimeResourceID == INVALID_RUNTIME_RESOURCE_ID)
	{
		check(Resources->RootData.Num() > 0);
		Resources->HierarchyOffset = Hierarchy.Allocator.Allocate(Resources->HierarchyNodes.Num());
		Resources->NumHierarchyNodes = Resources->HierarchyNodes.Num();
		Hierarchy.TotalUpload += Resources->HierarchyNodes.Num();
		INC_DWORD_STAT_BY( STAT_NaniteTotalPages, Resources->PageStreamingStates.Num() );
		INC_DWORD_STAT_BY( STAT_NaniteRootPages, Resources->NumRootPages );
		INC_DWORD_STAT_BY( STAT_NaniteResources, 1 );

		Resources->RootPageIndex = ClusterPageData.Allocator.Allocate( Resources->NumRootPages );
		if (GNaniteStreamingDynamicallyGrowAllocations == 0 && ClusterPageData.Allocator.GetMaxSize() > GNaniteStreamingNumInitialRootPages)
		{
			UE_LOG(LogNaniteStreaming, Fatal, TEXT("Out of root pages. Increase the initial root page allocation (r.Nanite.Streaming.NumInitialRootPages) or allow it to grow dynamically (r.Nanite.Streaming.DynamicallyGrowAllocations)."));
		}
		StatNumRootPages += Resources->NumRootPages;

		StatPeakRootPages = FMath::Max(StatPeakRootPages, (uint32)ClusterPageData.Allocator.GetMaxSize());
		SET_DWORD_STAT(STAT_NanitePeakRootPages, StatPeakRootPages);

		if (GNaniteStreamingImposters && Resources->ImposterAtlas.Num())
		{
			Resources->ImposterIndex = ImposterData.Allocator.Allocate(1);
			if (GNaniteStreamingDynamicallyGrowAllocations == 0 && ImposterData.Allocator.GetMaxSize() > GNaniteStreamingNumInitialImposters)
			{
				UE_LOG(LogNaniteStreaming, Fatal, TEXT("Out of imposters. Increase the initial imposter allocation (r.Nanite.Streaming.NumInitialImposters) or allow it to grow dynamically (r.Nanite.Streaming.DynamicallyGrowAllocations)."));
			}
			ImposterData.TotalUpload++;
			INC_DWORD_STAT_BY( STAT_NaniteImposters, 1 );
		}

		// Version root pages so we can disregard invalid streaming requests.
		// TODO: We only need enough versions to cover the frame delay from the GPU, so most of the version bits can be reclaimed.
		check(Resources->RootPageIndex < NANITE_MAX_GPU_PAGES);
		Resources->RuntimeResourceID = (NextRootPageVersion[Resources->RootPageIndex] << NANITE_MAX_GPU_PAGES_BITS) | Resources->RootPageIndex;
		NextRootPageVersion[Resources->RootPageIndex] = (NextRootPageVersion[Resources->RootPageIndex] + 1) & MAX_RUNTIME_RESOURCE_VERSIONS_MASK;
		RuntimeResourceMap.Add( Resources->RuntimeResourceID, Resources );

		check(Resources->PersistentHash != NANITE_INVALID_PERSISTENT_HASH);
		PersistentHashResourceMap.Add(Resources->PersistentHash, Resources);
		
		PendingAdds.Add( Resources );
	}
}

void FStreamingManager::Remove( FResources* Resources )
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);

	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	if (Resources->RuntimeResourceID != INVALID_RUNTIME_RESOURCE_ID)
	{
		Hierarchy.Allocator.Free( Resources->HierarchyOffset, Resources->NumHierarchyNodes );
		Resources->HierarchyOffset = INDEX_NONE;

		ClusterPageData.Allocator.Free( Resources->RootPageIndex, Resources->NumRootPages );
		Resources->RootPageIndex = INDEX_NONE;

		if (Resources->ImposterIndex != INDEX_NONE)
		{
			ImposterData.Allocator.Free( Resources->ImposterIndex, 1 );
			Resources->ImposterIndex = INDEX_NONE;
			DEC_DWORD_STAT_BY( STAT_NaniteImposters, 1 );
		}

		const uint32 NumResourcePages = Resources->PageStreamingStates.Num();
		INC_DWORD_STAT_BY( STAT_NaniteTotalPages, NumResourcePages );
		DEC_DWORD_STAT_BY( STAT_NaniteRootPages, Resources->NumRootPages );
		DEC_DWORD_STAT_BY( STAT_NaniteResources, 1 );

		StatNumRootPages -= Resources->NumRootPages;

		// Move all registered pages to the free list. No need to properly uninstall them as they are no longer referenced from the hierarchy.
		for( uint32 PageIndex = 0; PageIndex < NumResourcePages; PageIndex++ )
		{
			FPageKey Key = { Resources->RuntimeResourceID, PageIndex };
			FStreamingPageInfo* Page;
			if( RegisteredStreamingPagesMap.RemoveAndCopyValue(Key, Page) )
			{
				Page->RegisteredKey.RuntimeResourceID = INVALID_RUNTIME_RESOURCE_ID;	// Mark as free, so we won't try to uninstall it later
				MovePageToFreeList( Page );
			}
		}

		RuntimeResourceMap.Remove( Resources->RuntimeResourceID );
		Resources->RuntimeResourceID = INVALID_RUNTIME_RESOURCE_ID;

		check(Resources->PersistentHash != NANITE_INVALID_PERSISTENT_HASH);
		int32 NumRemoved = PersistentHashResourceMap.Remove(Resources->PersistentHash, Resources);
		check(NumRemoved == 1);
		Resources->PersistentHash = NANITE_INVALID_PERSISTENT_HASH;
		
		PendingAdds.Remove( Resources );
	}
}

void FStreamingManager::CollectDependencyPages( FResources* Resources, TSet< FPageKey >& DependencyPages, const FPageKey& Key )
{
	LLM_SCOPE_BYTAG(Nanite);
	if( DependencyPages.Find( Key ) )
		return;

	DependencyPages.Add( Key );

	FPageStreamingState& PageStreamingState = Resources->PageStreamingStates[ Key.PageIndex ];
	for( uint32 i = 0; i < PageStreamingState.DependenciesNum; i++ )
	{
		uint32 DependencyPageIndex = Resources->PageDependencies[ PageStreamingState.DependenciesStart + i ];

		if( Resources->IsRootPage( DependencyPageIndex ) )
			continue;

		FPageKey ChildKey = { Key.RuntimeResourceID, DependencyPageIndex };
		if( DependencyPages.Find( ChildKey ) == nullptr )
		{
			CollectDependencyPages( Resources, DependencyPages, ChildKey );
		}
	}
}

void FStreamingManager::SelectStreamingPages( FResources* Resources, TArray< FPageKey >& SelectedPages, TSet<FPageKey>& SelectedPagesSet, uint32 RuntimeResourceID, uint32 PageIndex, uint32 MaxSelectedPages )
{
	LLM_SCOPE_BYTAG(Nanite);
	FPageKey Key = { RuntimeResourceID, PageIndex };
	if( SelectedPagesSet.Find( Key ) || (uint32)SelectedPages.Num() >= MaxSelectedPages )
		return;

	SelectedPagesSet.Add( Key );

	const FPageStreamingState& PageStreamingState = Resources->PageStreamingStates[ PageIndex ];
	
	for( uint32 i = 0; i < PageStreamingState.DependenciesNum; i++ )
	{
		uint32 DependencyPageIndex = Resources->PageDependencies[ PageStreamingState.DependenciesStart + i ];
		if( Resources->IsRootPage( DependencyPageIndex ) )
			continue;

		FPageKey DependencyKey = { RuntimeResourceID, DependencyPageIndex };
		if( RegisteredStreamingPagesMap.Find( DependencyKey ) == nullptr )
		{
			SelectStreamingPages( Resources, SelectedPages, SelectedPagesSet, RuntimeResourceID, DependencyPageIndex, MaxSelectedPages );
		}
	}

	if( (uint32)SelectedPages.Num() < MaxSelectedPages )
	{
		SelectedPages.Push( { RuntimeResourceID, PageIndex } );	// We need to write ourselves after our dependencies
	}
}

void FStreamingManager::RegisterStreamingPage( FStreamingPageInfo* Page, const FPageKey& Key )
{
	LLM_SCOPE_BYTAG(Nanite);

	FResources** Resources = RuntimeResourceMap.Find( Key.RuntimeResourceID );
	check( Resources != nullptr );
	check( !(*Resources)->IsRootPage(Key.PageIndex) );
	
	TArray< FPageStreamingState >& PageStreamingStates = (*Resources)->PageStreamingStates;
	FPageStreamingState& PageStreamingState = PageStreamingStates[ Key.PageIndex ];
	
	for( uint32 i = 0; i < PageStreamingState.DependenciesNum; i++ )
	{
		uint32 DependencyPageIndex = ( *Resources )->PageDependencies[ PageStreamingState.DependenciesStart + i ];
		if( (*Resources)->IsRootPage( DependencyPageIndex ) )
			continue;

		FPageKey DependencyKey = { Key.RuntimeResourceID, DependencyPageIndex };
		FStreamingPageInfo** DependencyPage = RegisteredStreamingPagesMap.Find( DependencyKey );
		check( DependencyPage != nullptr );
		(*DependencyPage)->RefCount++;
	}

	// Insert at the front of the LRU
	FStreamingPageInfo& LRUSentinel = StreamingPageLRU;

	Page->Prev = &LRUSentinel;
	Page->Next = LRUSentinel.Next;
	LRUSentinel.Next->Prev = Page;
	LRUSentinel.Next = Page;

	Page->RegisteredKey = Key;
	Page->LatestUpdateIndex = NextUpdateIndex;
	Page->RefCount = 0;

	// Register Page
	RegisteredStreamingPagesMap.Add(Key, Page);

	NumRegisteredStreamingPages++;
	INC_DWORD_STAT( STAT_NaniteRegisteredStreamingPages );
}

void FStreamingManager::UnregisterPage( const FPageKey& Key )
{
	LLM_SCOPE_BYTAG(Nanite);

	FResources** Resources = RuntimeResourceMap.Find( Key.RuntimeResourceID );
	check( Resources != nullptr );
	check( !(*Resources)->IsRootPage(Key.PageIndex) );

	FStreamingPageInfo** PagePtr = RegisteredStreamingPagesMap.Find( Key );
	check( PagePtr != nullptr );
	FStreamingPageInfo* Page = *PagePtr;
	
	// Decrement reference counts of dependencies.
	TArray< FPageStreamingState >& PageStreamingStates = ( *Resources )->PageStreamingStates;
	FPageStreamingState& PageStreamingState = PageStreamingStates[ Key.PageIndex ];
	for( uint32 i = 0; i < PageStreamingState.DependenciesNum; i++ )
	{
		uint32 DependencyPageIndex = ( *Resources )->PageDependencies[ PageStreamingState.DependenciesStart + i ];
		if( (*Resources)->IsRootPage( DependencyPageIndex ) )
			continue;

		FPageKey DependencyKey = { Key.RuntimeResourceID, DependencyPageIndex };
		FStreamingPageInfo** DependencyPage = RegisteredStreamingPagesMap.Find( DependencyKey );
		check( DependencyPage != nullptr );
		( *DependencyPage )->RefCount--;
	}

	RegisteredStreamingPagesMap.Remove( Key );
	MovePageToFreeList( Page );
}

void FStreamingManager::MovePageToFreeList( FStreamingPageInfo* Page )
{
	// Unlink
	FStreamingPageInfo* OldNext = Page->Next;
	FStreamingPageInfo* OldPrev = Page->Prev;
	OldNext->Prev = OldPrev;
	OldPrev->Next = OldNext;

	// Add to free list
	Page->Next = StreamingPageInfoFreeList;
	StreamingPageInfoFreeList = Page;

	NumRegisteredStreamingPages--;
	DEC_DWORD_STAT( STAT_NaniteRegisteredStreamingPages );
}

bool FStreamingManager::ArePageDependenciesCommitted(uint32 RuntimeResourceID, uint32 DependencyPageStart, uint32 DependencyPageNum)
{
	bool bResult = true;
	for (uint32 i = 0; i < DependencyPageNum; i++)
	{
		uint32 DependencyPage = DependencyPageStart + i;
		FPageKey DependencyKey = { RuntimeResourceID, DependencyPage };
		FStreamingPageInfo** DependencyPagePtr = CommittedStreamingPageMap.Find(DependencyKey);
		if (DependencyPagePtr == nullptr || (*DependencyPagePtr)->ResidentKey != DependencyKey)	// Is the page going to be committed after this batch and does it already have its fixupchunk loaded?
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

uint32 FStreamingManager::GPUPageIndexToGPUOffset(uint32 PageIndex) const
{
	return (FMath::Min(PageIndex, MaxStreamingPages) << NANITE_STREAMING_PAGE_GPU_SIZE_BITS) + ((uint32)FMath::Max((int32)PageIndex - (int32)MaxStreamingPages, 0) << NANITE_ROOT_PAGE_GPU_SIZE_BITS);
}

// Applies the fixups required to install/uninstall a page.
// Hierarchy references are patched up and leaf flags of parent clusters are set accordingly.
void FStreamingManager::ApplyFixups( const FFixupChunk& FixupChunk, const FResources& Resources, bool bUninstall )
{
	LLM_SCOPE_BYTAG(Nanite);

	const uint32 RuntimeResourceID = Resources.RuntimeResourceID;
	const uint32 HierarchyOffset = Resources.HierarchyOffset;
	uint32 Flags = bUninstall ? NANITE_CLUSTER_FLAG_LEAF : 0;

	// Fixup clusters
	for( uint32 i = 0; i < FixupChunk.Header.NumClusterFixups; i++ )
	{
		const FClusterFixup& Fixup = FixupChunk.GetClusterFixup( i );

		bool bPageDependenciesCommitted = bUninstall || ArePageDependenciesCommitted(RuntimeResourceID, Fixup.GetPageDependencyStart(), Fixup.GetPageDependencyNum());
		if (!bPageDependenciesCommitted)
			continue;
		
		uint32 TargetPageIndex = Fixup.GetPageIndex();
		uint32 TargetGPUPageIndex = INVALID_PAGE_INDEX;
		uint32 NumTargetPageClusters = 0;

		if( Resources.IsRootPage( TargetPageIndex ) )
		{
			TargetGPUPageIndex = MaxStreamingPages + Resources.RootPageIndex + TargetPageIndex;
			NumTargetPageClusters = RootPageInfos[ Resources.RootPageIndex + TargetPageIndex ].NumClusters;
		}
		else
		{
			FPageKey TargetKey = { RuntimeResourceID, TargetPageIndex };
			FStreamingPageInfo** TargetPagePtr = CommittedStreamingPageMap.Find( TargetKey );

			check( bUninstall || TargetPagePtr );
			if (TargetPagePtr)
			{
				FStreamingPageInfo* TargetPage = *TargetPagePtr;
				FFixupChunk& TargetFixupChunk = *StreamingPageFixupChunks[TargetPage->GPUPageIndex];
				check(StreamingPageInfos[TargetPage->GPUPageIndex].ResidentKey == TargetKey);

				NumTargetPageClusters = TargetFixupChunk.Header.NumClusters;
				check(Fixup.GetClusterIndex() < NumTargetPageClusters);

				TargetGPUPageIndex = TargetPage->GPUPageIndex;
			}
		}
		
		if(TargetGPUPageIndex != INVALID_PAGE_INDEX)
		{
			uint32 ClusterIndex = Fixup.GetClusterIndex();
			uint32 FlagsOffset = offsetof( FPackedCluster, Flags );
			uint32 Offset = GPUPageIndexToGPUOffset( TargetGPUPageIndex ) + NANITE_GPU_PAGE_HEADER_SIZE + ( ( FlagsOffset >> 4 ) * NumTargetPageClusters + ClusterIndex ) * 16 + ( FlagsOffset & 15 );
			ClusterFixupUploadBuffer.Add( Offset / sizeof( uint32 ), &Flags, 1 );
		}
	}

	// Fixup hierarchy
	for( uint32 i = 0; i < FixupChunk.Header.NumHierachyFixups; i++ )
	{
		const FHierarchyFixup& Fixup = FixupChunk.GetHierarchyFixup( i );

		bool bPageDependenciesCommitted = bUninstall || ArePageDependenciesCommitted(RuntimeResourceID, Fixup.GetPageDependencyStart(), Fixup.GetPageDependencyNum());
		if (!bPageDependenciesCommitted)
			continue;

		FPageKey TargetKey = { RuntimeResourceID, Fixup.GetPageIndex() };
		uint32 TargetGPUPageIndex = INVALID_PAGE_INDEX;
		if (!bUninstall)
		{
			if (Resources.IsRootPage(TargetKey.PageIndex))
			{
				TargetGPUPageIndex = MaxStreamingPages + Resources.RootPageIndex + TargetKey.PageIndex;
			}
			else
			{
				FStreamingPageInfo** TargetPagePtr = CommittedStreamingPageMap.Find(TargetKey);
				check(TargetPagePtr);
				check((*TargetPagePtr)->ResidentKey == TargetKey);
				TargetGPUPageIndex = (*TargetPagePtr)->GPUPageIndex;
			}
		}
		
		// Uninstalls are unconditional. The same uninstall might happen more than once.
		// If this page is getting uninstalled it also means it wont be reinstalled and any split groups can't be satisfied, so we can safely uninstall them.	
		
		uint32 HierarchyNodeIndex = Fixup.GetNodeIndex();
		check( HierarchyNodeIndex < Resources.NumHierarchyNodes );
		uint32 ChildIndex = Fixup.GetChildIndex();
		uint32 ChildStartReference = bUninstall ? 0xFFFFFFFFu : ( ( TargetGPUPageIndex << NANITE_MAX_CLUSTERS_PER_PAGE_BITS ) | Fixup.GetClusterGroupPartStartIndex() );
		uint32 Offset = ( size_t )&( ( (FPackedHierarchyNode*)0 )[ HierarchyOffset + HierarchyNodeIndex ].Misc1[ ChildIndex ].ChildStartReference );
		Hierarchy.UploadBuffer.Add( Offset / sizeof( uint32 ), &ChildStartReference );
	}
}

void FStreamingManager::InstallReadyPages( uint32 NumReadyPages )
{
	LLM_SCOPE_BYTAG(Nanite);
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::CopyReadyPages);

	if (NumReadyPages == 0)
		return;

	const uint32 StartPendingPageIndex = ( NextPendingPageIndex + MaxPendingPages - NumPendingPages ) % MaxPendingPages;

	struct FUploadTask
	{
		FPendingPage* PendingPage = nullptr;
		uint8* Dst = nullptr;
		const uint8* Src = nullptr;
		uint32 SrcSize = 0;
	};

#if WITH_EDITOR
	TMap<FResources*, const uint8*> ResourceToBulkPointer;
#endif

	TArray<FUploadTask> UploadTasks;
	UploadTasks.AddDefaulted(NumReadyPages);

	// Install ready pages
	{
		// Batched page install:
		// GPU uploads are unordered, so we need to make sure we have no overlapping writes.
		// For actual page uploads, we only upload the last page that ends up on a given GPU page.

		// Fixups are handled with set of UploadBuffers that are executed AFTER page upload.
		// To ensure we don't end up fixing up the same addresses more than once, we only perform the fixup associated with the first uninstall and the last install on a given GPU page.
		// If a page ends up being both installed and uninstalled in the same frame, we only install it to prevent a race.
		// Uninstall fixup depends on StreamingPageFixupChunks that is also updated by installs. To prevent races we perform all uninstalls before installs.
		
		// Calculate first and last Pending Page Index update for each GPU page.
		TMap<uint32, uint32> GPUPageToLastPendingPageIndex;
		for (uint32 i = 0; i < NumReadyPages; i++)
		{
			uint32 PendingPageIndex = (StartPendingPageIndex + i) % MaxPendingPages;
			FPendingPage& PendingPage = PendingPages[PendingPageIndex];
			
			// Update when the GPU page was touched for the last time.
			// This also includes pages from deleted resources. This is intentional as the corresponding uninstall still needs to happen.
			GPUPageToLastPendingPageIndex.Add(PendingPage.GPUPageIndex, PendingPageIndex);
		}

		TSet<FPageKey> BatchNewPageKeys;
		for (auto& Elem : GPUPageToLastPendingPageIndex)
		{
			uint32 GPUPageIndex = Elem.Key;

			// Remove uninstalled pages from streaming map, so we won't try to do uninstall fixup on them.
			FStreamingPageInfo& StreamingPageInfo = StreamingPageInfos[GPUPageIndex];
			if (StreamingPageInfo.ResidentKey.RuntimeResourceID != INVALID_RUNTIME_RESOURCE_ID)
			{
				CommittedStreamingPageMap.Remove(StreamingPageInfo.ResidentKey);
			}

			// Mark newly installed page
			FPendingPage& PendingPage = PendingPages[Elem.Value];
			BatchNewPageKeys.Add(PendingPage.InstallKey);
		}

		// Uninstall pages
		// We are uninstalling pages in a separate pass as installs will also overwrite the GPU page fixup information we need for uninstalls.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UninstallFixup);
			for (auto& Elem : GPUPageToLastPendingPageIndex)
			{
				uint32 GPUPageIndex = Elem.Key;
				FStreamingPageInfo& StreamingPageInfo = StreamingPageInfos[GPUPageIndex];

				// Uninstall GPU page
				if (StreamingPageInfo.ResidentKey.RuntimeResourceID != INVALID_RUNTIME_RESOURCE_ID)
				{
					// Apply fixups to uninstall page. No need to fix up anything if resource is gone.
					FResources** Resources = RuntimeResourceMap.Find(StreamingPageInfo.ResidentKey.RuntimeResourceID);
					if (Resources)
					{
						// Prevent race between installs and uninstalls of the same page. Only uninstall if the page is not going to be installed again.
						if (!BatchNewPageKeys.Contains(StreamingPageInfo.ResidentKey))
						{
							ApplyFixups(*StreamingPageFixupChunks[GPUPageIndex], **Resources, true);
						}

						ModifiedResources.Add(StreamingPageInfo.ResidentKey.RuntimeResourceID);
					}
				}

				StreamingPageInfo.ResidentKey.RuntimeResourceID = INVALID_RUNTIME_RESOURCE_ID;	// Only uninstall it the first time.
				DEC_DWORD_STAT(STAT_NaniteInstalledPages);
			}
		}

		// Commit to streaming map, so install fixups will happen on all pages
		for (auto& Elem : GPUPageToLastPendingPageIndex)
		{
			uint32 GPUPageIndex = Elem.Key;
			uint32 LastPendingPageIndex = Elem.Value;
			FPendingPage& PendingPage = PendingPages[LastPendingPageIndex];

			FResources** Resources = RuntimeResourceMap.Find(PendingPage.InstallKey.RuntimeResourceID);
			if (Resources)
			{
				CommittedStreamingPageMap.Add(PendingPage.InstallKey, &StreamingPageInfos[GPUPageIndex]);
			}
		}

		// Install pages
		// Must be processed in PendingPages order so FFixupChunks are loaded when we need them.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(InstallReadyPages);
			uint32 NumInstalledPages = 0;
			for (uint32 TaskIndex = 0; TaskIndex < NumReadyPages; TaskIndex++)
			{
				uint32 PendingPageIndex = (StartPendingPageIndex + TaskIndex) % MaxPendingPages;
				FPendingPage& PendingPage = PendingPages[PendingPageIndex];

				FUploadTask& UploadTask = UploadTasks[TaskIndex];
				UploadTask.PendingPage = &PendingPage;

				FResources** Resources = RuntimeResourceMap.Find(PendingPage.InstallKey.RuntimeResourceID);
				uint32 LastPendingPageIndex = GPUPageToLastPendingPageIndex.FindChecked(PendingPages[PendingPageIndex].GPUPageIndex);
				if (PendingPageIndex != LastPendingPageIndex || !Resources)
				{
					continue;	// Skip resource install. Resource no longer exists or page has already been overwritten.
				}

				TArray< FPageStreamingState >& PageStreamingStates = ( *Resources )->PageStreamingStates;
				const FPageStreamingState& PageStreamingState = PageStreamingStates[ PendingPage.InstallKey.PageIndex ];
				FStreamingPageInfo* StreamingPage = &StreamingPageInfos[ PendingPage.GPUPageIndex ];

				CommittedStreamingPageMap.Add(PendingPage.InstallKey, StreamingPage);

				ModifiedResources.Add(PendingPage.InstallKey.RuntimeResourceID);

#if WITH_EDITOR
				const uint8* SrcPtr;
				if ((*Resources)->ResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC)
				{
					SrcPtr = (const uint8*)PendingPage.SharedBuffer.GetData();
				}
				else
				{
					// Make sure we only lock each resource BulkData once.
					const uint8** BulkDataPtrPtr = ResourceToBulkPointer.Find(*Resources);
					if (BulkDataPtrPtr)
					{
						SrcPtr = *BulkDataPtrPtr + PageStreamingState.BulkOffset;
					}
					else
					{
						FByteBulkData& BulkData = (*Resources)->StreamablePages;
						check(BulkData.IsBulkDataLoaded() && BulkData.GetBulkDataSize() > 0);
						const uint8* BulkDataPtr = (const uint8*)BulkData.LockReadOnly();
						ResourceToBulkPointer.Add(*Resources, BulkDataPtr);
						SrcPtr = BulkDataPtr + PageStreamingState.BulkOffset;
					}
				}
#else
				const uint8* SrcPtr = PendingPage.RequestBuffer.GetData();
#endif

				const uint32 FixupChunkSize = ((const FFixupChunk*)SrcPtr)->GetSize();
				FFixupChunk* FixupChunk = (FFixupChunk*)FMemory::Realloc(StreamingPageFixupChunks[PendingPage.GPUPageIndex], FixupChunkSize, sizeof(uint16));
				StreamingPageFixupChunks[PendingPage.GPUPageIndex] = FixupChunk;
				FMemory::Memcpy(FixupChunk, SrcPtr, FixupChunkSize);

				// Build list of GPU page dependencies
				GPUPageDependencies.Reset();
				if(PageStreamingState.Flags & NANITE_PAGE_FLAG_RELATIVE_ENCODING)
				{
					for (uint32 i = 0; i < PageStreamingState.DependenciesNum; i++)
					{
						const uint32 DependencyPageIndex = (*Resources)->PageDependencies[PageStreamingState.DependenciesStart + i];
						if ((*Resources)->IsRootPage(DependencyPageIndex))
						{
							GPUPageDependencies.Add(MaxStreamingPages + (*Resources)->RootPageIndex + DependencyPageIndex);
						}
						else
						{
							FPageKey DependencyKey = { PendingPage.InstallKey.RuntimeResourceID, DependencyPageIndex };
							FStreamingPageInfo** DependencyPagePtr = CommittedStreamingPageMap.Find(DependencyKey);
							check(DependencyPagePtr != nullptr);
							GPUPageDependencies.Add((*DependencyPagePtr)->GPUPageIndex);
						}
					}
				}
			
				uint32 PageOffset = GPUPageIndexToGPUOffset( PendingPage.GPUPageIndex );
				uint32 DataSize = PageStreamingState.BulkSize - FixupChunkSize;
				check(NumInstalledPages < MaxPageInstallsPerUpdate);

				const FPageKey GPUPageKey = FPageKey{ PendingPage.InstallKey.RuntimeResourceID, PendingPage.GPUPageIndex };

				UploadTask.PendingPage = &PendingPage;
				UploadTask.Dst = PageUploader->Add_GetRef(DataSize, PageOffset, GPUPageKey, GPUPageDependencies);
				UploadTask.Src = SrcPtr + FixupChunkSize;
				UploadTask.SrcSize = DataSize;
				NumInstalledPages++;

				// Apply fixups to install page
				StreamingPage->ResidentKey = PendingPage.InstallKey;
				ApplyFixups( *FixupChunk, **Resources, false );

				INC_DWORD_STAT( STAT_NaniteInstalledPages );
				INC_DWORD_STAT(STAT_NanitePageInstalls);
			}
		}
	}

	// Upload pages
	ParallelFor(UploadTasks.Num(), [&UploadTasks](int32 i)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CopyPageTask);
		const FUploadTask& Task = UploadTasks[i];
		
		if(Task.Dst)	// Dst can be 0 if we skipped install in InstallReadyPages.
		{
			FMemory::Memcpy(Task.Dst, Task.Src, Task.SrcSize);
		}

#if WITH_EDITOR
		Task.PendingPage->SharedBuffer.Reset();
#else
		check(Task.PendingPage->Request.IsCompleted());
		Task.PendingPage->Request.Reset();
#endif
	});

#if WITH_EDITOR
	// Unlock BulkData
	for (auto it : ResourceToBulkPointer)
	{
		FResources* Resources = it.Key;
		FByteBulkData& BulkData = Resources->StreamablePages;
		BulkData.Unlock();
	}
#endif
}

#if DO_CHECK
void FStreamingManager::VerifyPageLRU( FStreamingPageInfo& List, uint32 TargetListLength, bool bCheckUpdateIndex )
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::VerifyPageLRU);

	uint32 ListLength = 0u;
	uint32 PrevUpdateIndex = 0u;
	FStreamingPageInfo* Ptr = List.Prev;
	while( Ptr != &List )
	{
		if( bCheckUpdateIndex )
		{
			check( Ptr->LatestUpdateIndex >= PrevUpdateIndex );
			PrevUpdateIndex = Ptr->LatestUpdateIndex;
		}

		ListLength++;
		Ptr = Ptr->Prev;
	}

	check( ListLength == TargetListLength );
}
#endif

void FStreamingManager::ProcessNewResources( FRDGBuilder& GraphBuilder)
{
	LLM_SCOPE_BYTAG(Nanite);

	if (PendingAdds.Num() == 0)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::ProcessNewResources);

	// Upload hierarchy for pending resources
	FRDGBuffer* HierarchyDataBuffer = ResizeByteAddressBufferIfNeeded(GraphBuilder, Hierarchy.DataBuffer, FMath::RoundUpToPowerOfTwo(Hierarchy.Allocator.GetMaxSize()) * sizeof(FPackedHierarchyNode), TEXT("Nanite.StreamingManager.Hierarchy"));

	check(MaxStreamingPages <= NANITE_MAX_GPU_PAGES);
	uint32 MaxRootPages = NANITE_MAX_GPU_PAGES - MaxStreamingPages;
	
	uint32 NumAllocatedRootPages;	
	if(GNaniteStreamingDynamicallyGrowAllocations)
	{
		if(ClusterPageData.Allocator.GetMaxSize() <= GNaniteStreamingNumInitialRootPages)
		{
			NumAllocatedRootPages = GNaniteStreamingNumInitialRootPages;	// Don't round up initial allocation
		}
		else
		{
			NumAllocatedRootPages = FMath::Clamp( RoundUpToSignificantBits( ClusterPageData.Allocator.GetMaxSize(), 2 ), (uint32)GNaniteStreamingNumInitialRootPages, MaxRootPages);
		}
	}
	else
	{
		NumAllocatedRootPages = GNaniteStreamingNumInitialRootPages;
	}

	check( NumAllocatedRootPages >= (uint32)ClusterPageData.Allocator.GetMaxSize() );	// Root pages just don't fit!

	StatPeakAllocatedRootPages = FMath::Max(StatPeakAllocatedRootPages, NumAllocatedRootPages);
	SET_DWORD_STAT(STAT_NanitePeakAllocatedRootPages, StatPeakAllocatedRootPages);
	
	const uint32 NumAllocatedPages = MaxStreamingPages + NumAllocatedRootPages;
	const uint32 AllocatedPagesSize = GPUPageIndexToGPUOffset( NumAllocatedPages );
	check(NumAllocatedPages <= NANITE_MAX_GPU_PAGES);

	FRDGBuffer* ClusterPageDataBuffer = ResizeByteAddressBufferIfNeeded(GraphBuilder, ClusterPageData.DataBuffer, AllocatedPagesSize, TEXT("Nanite.StreamingManager.ClusterPageData"));
	RootPageInfos.SetNum( NumAllocatedRootPages );

	check( AllocatedPagesSize <= ( 1u << 31 ) );	// 2GB seems to be some sort of limit.
													// TODO: Is it a GPU/API limit or is it a signed integer bug on our end?

	FRDGBuffer* ImposterDataBuffer = nullptr;

	const bool bUploadImposters = GNaniteStreamingImposters && ImposterData.TotalUpload > 0;
	if(bUploadImposters)
	{
		uint32 WidthInTiles = 12;
		uint32 TileSize = 12;
		uint32 AtlasBytes = FMath::Square( WidthInTiles * TileSize ) * sizeof( uint16 );
		const uint32 NumAllocatedImposters = FMath::Max( RoundUpToSignificantBits(ImposterData.Allocator.GetMaxSize(), 2), (uint32)GNaniteStreamingNumInitialImposters );
		ImposterDataBuffer = ResizeByteAddressBufferIfNeeded(GraphBuilder, ImposterData.DataBuffer, NumAllocatedImposters * AtlasBytes, TEXT("Nanite.StreamingManager.ImposterData"));
		ImposterData.UploadBuffer.Init(GraphBuilder, ImposterData.TotalUpload, AtlasBytes, false, TEXT("Nanite.StreamingManager.ImposterDataUpload"));
	}

	// TODO: These uploads can end up being quite large.
	// We should try to change the high level logic so the proxy is not considered loaded until the root page has been loaded, so we can split this over multiple frames.
	
	Hierarchy.UploadBuffer.Init(GraphBuilder, Hierarchy.TotalUpload, sizeof(FPackedHierarchyNode), false, TEXT("Nanite.StreamingManager.HierarchyUpload"));

	// Calculate total required size
	uint32 TotalPageSize = 0;
	uint32 TotalRootPages = 0;
	for (FResources* Resources : PendingAdds)
	{
		for (uint32 i = 0; i < Resources->NumRootPages; i++)
		{
			TotalPageSize += Resources->PageStreamingStates[i].PageSize;
		}

		TotalRootPages += Resources->NumRootPages;
	}

	FStreamingPageUploader RootPageUploader;
	RootPageUploader.Init(GraphBuilder, TotalRootPages, TotalPageSize, MaxStreamingPages);

	GPUPageDependencies.Reset();

	for (FResources* Resources : PendingAdds)
	{
		for (uint32 LocalPageIndex = 0; LocalPageIndex < Resources->NumRootPages; LocalPageIndex++)
		{
			const FPageStreamingState& PageStreamingState = Resources->PageStreamingStates[LocalPageIndex];

			const uint32 RootPageIndex = Resources->RootPageIndex + LocalPageIndex;
			const uint32 GPUPageIndex = MaxStreamingPages + RootPageIndex;

			const uint8* Ptr = Resources->RootData.GetData() + PageStreamingState.BulkOffset;
			const FFixupChunk& FixupChunk = *(FFixupChunk*)Ptr;
			const uint32 FixupChunkSize = FixupChunk.GetSize();
			const uint32 NumClusters = FixupChunk.Header.NumClusters;

			const FPageKey GPUPageKey = { Resources->RuntimeResourceID, GPUPageIndex };

			const uint32 PageDiskSize = PageStreamingState.PageSize;
			check(PageDiskSize == PageStreamingState.BulkSize - FixupChunkSize);
			const uint32 PageOffset = GPUPageIndexToGPUOffset(GPUPageIndex);
			uint8* Dst = RootPageUploader.Add_GetRef(PageDiskSize, PageOffset, GPUPageKey, GPUPageDependencies);
			FMemory::Memcpy(Dst, Ptr + FixupChunkSize, PageDiskSize);

			// Root node should only have fixups that depend on other non-root pages and cannot be satisfied yet.

			// Fixup hierarchy
			for (uint32 i = 0; i < FixupChunk.Header.NumHierachyFixups; i++)
			{
				const FHierarchyFixup& Fixup = FixupChunk.GetHierarchyFixup(i);
				const uint32 HierarchyNodeIndex = Fixup.GetNodeIndex();
				check(HierarchyNodeIndex < (uint32)Resources->HierarchyNodes.Num());
				const uint32 ChildIndex = Fixup.GetChildIndex();
				const uint32 GroupStartIndex = Fixup.GetClusterGroupPartStartIndex();
				const uint32 TargetGPUPageIndex = MaxStreamingPages + Resources->RootPageIndex + Fixup.GetPageIndex();
				const uint32 ChildStartReference = (TargetGPUPageIndex << NANITE_MAX_CLUSTERS_PER_PAGE_BITS) | Fixup.GetClusterGroupPartStartIndex();

				if (Fixup.GetPageDependencyNum() == 0)	// Only install part if it has no other dependencies
				{
					Resources->HierarchyNodes[HierarchyNodeIndex].Misc1[ChildIndex].ChildStartReference = ChildStartReference;
				}
			}

			FRootPageInfo& RootPageInfo = RootPageInfos[RootPageIndex];
			RootPageInfo.RuntimeResourceID = Resources->RuntimeResourceID;
			RootPageInfo.NumClusters = NumClusters;

			const float RootSizeMB = (PageStreamingState.BulkSize + Resources->HierarchyNodes.Num() * Resources->HierarchyNodes.GetTypeSize() + Resources->ImposterAtlas.Num() * Resources->ImposterAtlas.GetTypeSize()) * (1.0f / 1048576.0f);
			INC_FLOAT_STAT_BY(STAT_NaniteRootDataMB, RootSizeMB);
		}

		Hierarchy.UploadBuffer.Add(Resources->HierarchyOffset, Resources->HierarchyNodes.GetData(), Resources->HierarchyNodes.Num());
		if (bUploadImposters && Resources->ImposterAtlas.Num() > 0)
		{
			ImposterData.UploadBuffer.Add(Resources->ImposterIndex, Resources->ImposterAtlas.GetData());
		}

#if !WITH_EDITOR
		// We can't free the CPU data in editor builds because the resource might be kept around and used for cooking later.
		Resources->RootData.Empty();
		Resources->HierarchyNodes.Empty();
		Resources->ImposterAtlas.Empty();
#endif
	}

	{
		Hierarchy.TotalUpload = 0;
		Hierarchy.UploadBuffer.ResourceUploadTo(GraphBuilder, HierarchyDataBuffer);

		RootPageUploader.ResourceUploadTo(GraphBuilder, ClusterPageDataBuffer);

		if (bUploadImposters)
		{
			ImposterData.TotalUpload = 0;
			ImposterData.UploadBuffer.ResourceUploadTo(GraphBuilder, ImposterDataBuffer);
		}
	}

	PendingAdds.Reset();
}

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
		Parameters.StreamingManager->AsyncUpdate();
	}

	static ESubsequentsMode::Type	GetSubsequentsMode()	{ return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type				GetDesiredThread()		{ return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId				GetStatId() const		{ return TStatId(); }
};

uint32 FStreamingManager::DetermineReadyPages()
{
	LLM_SCOPE_BYTAG(Nanite);
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::DetermineReadyPages);

	const uint32 StartPendingPageIndex = (NextPendingPageIndex + MaxPendingPages - NumPendingPages) % MaxPendingPages;
	uint32 NumReadyPages = 0;
	
#if !UE_BUILD_SHIPPING
	uint64 UpdateTick = FPlatformTime::Cycles64();
	uint64 DeltaTick = PrevUpdateTick ? UpdateTick - PrevUpdateTick : 0;
	PrevUpdateTick = UpdateTick;
#endif

	// Check how many pages are ready
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CheckReadyPages);

		for( uint32 i = 0; i < NumPendingPages && NumReadyPages < MaxPageInstallsPerUpdate; i++ )
		{
			uint32 PendingPageIndex = ( StartPendingPageIndex + i ) % MaxPendingPages;
			FPendingPage& PendingPage = PendingPages[ PendingPageIndex ];
			
#if WITH_EDITOR
			if (PendingPage.State == FPendingPage::EState::Ready)
			{
				if (PendingPage.RetryCount > 0)
				{
					FResources** Resources = RuntimeResourceMap.Find(PendingPage.InstallKey.RuntimeResourceID);
					if (Resources)
					{
						UE_LOG(LogNaniteStreaming, Error, TEXT("Nanite DDC retry succeeded for '%s' (Page %d) on %d attempt."), *(*Resources)->ResourceName, PendingPage.InstallKey.PageIndex, PendingPage.RetryCount);
					}
				}
			}
			else if (PendingPage.State == FPendingPage::EState::Pending)
			{
				break;
			}
			else if (PendingPage.State == FPendingPage::EState::Failed)
			{
				FResources** Resources = RuntimeResourceMap.Find(PendingPage.InstallKey.RuntimeResourceID);
				if (Resources)
				{
					// Resource is still there. Retry the request.
					PendingPage.State = FPendingPage::EState::Pending;
					PendingPage.RetryCount++;
					
					if(PendingPage.RetryCount == 0)	// Only warn on first retry to prevent spam
					{
						UE_LOG(LogNaniteStreaming, Error, TEXT("Nanite DDC request failed for '%s' (Page %d). Retrying..."), *(*Resources)->ResourceName, PendingPage.InstallKey.PageIndex);
					}

					const FPageStreamingState& PageStreamingState = (*Resources)->PageStreamingStates[PendingPage.InstallKey.PageIndex];
					FCacheGetChunkRequest Request = BuildDDCRequest(**Resources, PageStreamingState, PendingPageIndex);
					RequestDDCData(MakeArrayView(&Request, 1));
				}
				else
				{
					// Resource is no longer there. Just mark as ready so it will be skipped in InstallReadyPages
					PendingPage.State = FPendingPage::EState::Ready;
				}
				break;
			}
#else
			if (PendingPage.Request.IsCompleted() == false)
			{
				break;
			}
#endif

#if !UE_BUILD_SHIPPING
			if( GNaniteStreamingBandwidthLimit >= 0.0 )
			{
				uint32 SimulatedBytesRemaining = FPlatformTime::ToSeconds64(DeltaTick) * GNaniteStreamingBandwidthLimit * 1048576.0;
				uint32 SimulatedBytesRead = FMath::Min( PendingPage.BytesLeftToStream, SimulatedBytesRemaining );
				PendingPage.BytesLeftToStream -= SimulatedBytesRead;
				SimulatedBytesRemaining -= SimulatedBytesRead;
				if( PendingPage.BytesLeftToStream > 0 )
					break;
			}
#endif

			NumReadyPages++;
		}
	}
	
	return NumReadyPages;
}

void FStreamingManager::AddPendingExplicitRequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AddPendingExplicitRequests);
	
	const int32 NumPendingExplicitRequests = PendingExplicitRequests.Num();
	if (NumPendingExplicitRequests > 0)
	{
		int32 Index = 0;
		while (Index < NumPendingExplicitRequests)
		{
			const uint32 ResourcePersistentHash = PendingExplicitRequests[Index++];
			
			// Resolve resource
			TArray<FResources*, TInlineAllocator<16>> MultiMapResult;
			PersistentHashResourceMap.MultiFind(ResourcePersistentHash, MultiMapResult);

			// Keep processing requests from this resource as long as they have the repeat bit set
			bool bRepeat = true;
			while (bRepeat && Index < NumPendingExplicitRequests)
			{
				const uint32 Packed = PendingExplicitRequests[Index++];

				bRepeat = (Packed & 1u) != 0u;
				
				// Add requests to table
				// In the rare event of a collision all resources with the same hash will be requested
				for (const FResources* Resources : MultiMapResult)
				{
					const uint32 PageIndex = (Packed >> 1) & NANITE_MAX_RESOURCE_PAGES_MASK;
					const uint32 Priority = Packed | ((1 << (NANITE_MAX_RESOURCE_PAGES_BITS + 1)) - 1);	// Round quantized priority up
					if (PageIndex >= Resources->NumRootPages && PageIndex < (uint32)Resources->PageStreamingStates.Num())
					{
						FStreamingRequest Request;
						Request.Key.RuntimeResourceID = Resources->RuntimeResourceID;
						Request.Key.PageIndex = PageIndex;
						Request.Priority = *(const float*)&Priority;
						RequestsHashTable->AddRequest(Request);
						INC_DWORD_STAT(STAT_NaniteExplicitRequests);
					}
				}
			}
		}
		PendingExplicitRequests.Empty();
	}
}

void FStreamingManager::AddPendingResourcePrefetchRequests()
{
	for (FResourcePrefetch& Prefetch : PendingResourcePrefetches)
	{
		FResources** Resources = RuntimeResourceMap.Find(Prefetch.RuntimeResourceID);
		if (Resources)
		{
			// Request first MAX_RESOURCE_PREFETCH_PAGES streaming pages of resource
			const uint32 NumRootPages = (*Resources)->NumRootPages;
			const uint32 NumPages = (*Resources)->PageStreamingStates.Num();
			const uint32 EndPage = FMath::Min(NumPages, NumRootPages + MAX_RESOURCE_PREFETCH_PAGES);

			for (uint32 PageIndex = NumRootPages; PageIndex < EndPage; PageIndex++)
			{
				FStreamingRequest Request;
				Request.Key.RuntimeResourceID	= Prefetch.RuntimeResourceID;
				Request.Key.PageIndex			= PageIndex;
				Request.Priority				= 0xFFFFFFFFu - Prefetch.NumFramesUntilRender;	// Prefetching has highest priority. Prioritize requests closer to the deadline higher.
																								// TODO: Calculate appropriate priority based on bounds
				RequestsHashTable->AddRequest(Request);
			}
		}
		Prefetch.NumFramesUntilRender--;	// Keep the request alive until projected first render
	}

	// Remove requests that are past the rendering deadline
	PendingResourcePrefetches.RemoveAll([](const FResourcePrefetch& Prefetch) { return Prefetch.NumFramesUntilRender == 0; });
}

void FStreamingManager::BeginAsyncUpdate(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::BeginAsyncUpdate);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::Streaming");
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteStreaming);
	
	check(!AsyncState.bUpdateActive);
	AsyncState = FAsyncState {};
	AsyncState.bUpdateActive = true;

	if (!StreamingRequestsBuffer.IsValid())
	{
		// Init and clear StreamingRequestsBuffer.
		// Can't do this in InitRHI as RHICmdList doesn't have a valid context yet.
		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FStreamingRequest), NANITE_MAX_STREAMING_REQUESTS);
		Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_SourceCopy);
		FRDGBufferRef StreamingRequestsBufferRef = GraphBuilder.CreateBuffer(Desc, TEXT("Nanite.StreamingRequests"));
		
		ClearStreamingRequestCount(GraphBuilder, GraphBuilder.CreateUAV(StreamingRequestsBufferRef));

		StreamingRequestsBuffer = GraphBuilder.ConvertToExternalBuffer(StreamingRequestsBufferRef);
	}

	ProcessNewResources(GraphBuilder);

	AsyncState.NumReadyPages = DetermineReadyPages();
	if (AsyncState.NumReadyPages > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AllocBuffers);
		// Prepare buffers for upload
		PageUploader->Init(GraphBuilder, MaxPageInstallsPerUpdate, MaxPageInstallsPerUpdate * NANITE_MAX_PAGE_DISK_SIZE, MaxStreamingPages);
		ClusterFixupUploadBuffer.Init(GraphBuilder, MaxPageInstallsPerUpdate * NANITE_MAX_CLUSTERS_PER_PAGE, sizeof(uint32), false, TEXT("Nanite.ClusterFixupUploadBuffer"));	// No more parents than children, so no more than MAX_CLUSTER_PER_PAGE parents need to be fixed
		Hierarchy.UploadBuffer.Init(GraphBuilder, 2 * MaxPageInstallsPerUpdate * NANITE_MAX_CLUSTERS_PER_PAGE, sizeof(uint32), false, TEXT("Nanite.HierarchyUploadBuffer"));	// Allocate enough to load all selected pages and evict old pages
	}

	// Find latest most recent ready readback buffer
	{
		// Find latest buffer that is ready
		while (ReadbackBuffersNumPending > 0)
		{
			uint32 Index = (ReadbackBuffersWriteIndex + MaxStreamingReadbackBuffers - ReadbackBuffersNumPending) % MaxStreamingReadbackBuffers;
			if (StreamingRequestReadbackBuffers[Index]->IsReady())	//TODO: process all buffers or just the latest?
			{
				ReadbackBuffersNumPending--;
				AsyncState.LatestReadbackBuffer = StreamingRequestReadbackBuffers[Index];
			}
			else
			{
				break;
			}
		}
	}

	// Lock buffer
	if (AsyncState.LatestReadbackBuffer)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(LockBuffer);
		AsyncState.LatestReadbackBufferPtr = (const uint32*)AsyncState.LatestReadbackBuffer->Lock(NANITE_MAX_STREAMING_REQUESTS * sizeof(uint32) * 3);
	}

	// Start async processing
	FStreamingUpdateParameters Parameters;
	Parameters.StreamingManager = this;

	check(AsyncTaskEvents.IsEmpty());
	if (GNaniteStreamingAsync)
	{
		AsyncTaskEvents.Add(TGraphTask<FStreamingUpdateTask>::CreateTask().ConstructAndDispatchWhenReady(Parameters));
	}
	else
	{
		AsyncUpdate();
	}
}

#if NANITE_SANITY_CHECK_STREAMING_REQUESTS
void FStreamingManager::SanityCheckStreamingRequests(const FGPUStreamingRequest* StreamingRequestsPtr, const uint32 NumStreamingRequests)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SanityCheckRequests);
	uint32 PrevFrameNibble = ~0u;
	for (uint32 Index = 0; Index < NumStreamingRequests; Index++)
	{
		const FGPUStreamingRequest& GPURequest = StreamingRequestsPtr[Index];

		// Validate request magics
		if ((GPURequest.RuntimeResourceID_Magic & 0xF0) != 0xA0 ||
			(GPURequest.PageIndex_NumPages_Magic & 0xF0) != 0xB0 ||
			(GPURequest.Priority_Magic & 0xF0) != 0xC0)
		{
			UE_LOG(LogNaniteStreaming, Fatal, TEXT("Validation of Nanite streaming request failed! The magic doesn't match. This likely indicates an issue with the GPU readback."));
		}

		// Validate that requests are from the same frame
		const uint32 FrameNibble0 = GPURequest.RuntimeResourceID_Magic & 0xF;
		const uint32 FrameNibble1 = GPURequest.PageIndex_NumPages_Magic & 0xF;
		const uint32 FrameNibble2 = GPURequest.Priority_Magic & 0xF;
		if (FrameNibble0 != FrameNibble1 || FrameNibble0 != FrameNibble2 || FrameNibble1 != FrameNibble2 || (PrevFrameNibble != ~0u && FrameNibble0 != PrevFrameNibble))
		{
			UE_LOG(LogNaniteStreaming, Fatal, TEXT("Validation of Nanite streaming request failed! Single readback has data from multiple frames. Is there a race condition on the readback, a missing streaming update or is GPUScene being updated mid-frame?"));
		}
		PrevFrameNibble = FrameNibble0;

		const uint32 NumPages = (GPURequest.PageIndex_NumPages_Magic >> NANITE_STREAMING_REQUEST_MAGIC_BITS) & NANITE_MAX_GROUP_PARTS_MASK;
		const uint32 PageStartIndex = GPURequest.PageIndex_NumPages_Magic >> (NANITE_STREAMING_REQUEST_MAGIC_BITS + NANITE_MAX_GROUP_PARTS_BITS);

		if (NumPages == 0)
		{
			UE_LOG(LogNaniteStreaming, Fatal, TEXT("Validation of Nanite streaming request failed! Request range is empty."));
		}

		FResources** Resources = RuntimeResourceMap.Find(GPURequest.RuntimeResourceID_Magic >> NANITE_STREAMING_REQUEST_MAGIC_BITS);
		if (Resources)
		{
			// Check that request page range is within the resource limits
			// Resource could have been uninstalled in the meantime, which is ok. The request is ignored.
			// We don't have to worry about RuntimeResourceIDs being reused because MAX_RUNTIME_RESOURCE_VERSIONS is high enough to never have two resources with the same ID in flight.
			const uint32 MaxPageIndex = PageStartIndex + NumPages - 1;
			if (MaxPageIndex >= (uint32)(*Resources)->PageStreamingStates.Num())
			{
				UE_LOG(LogNaniteStreaming, Fatal, TEXT("Validation of Nanite streaming request failed! Page range out of bounds. Start: %d Num: %d Total: %d"), PageStartIndex, NumPages, (*Resources)->PageStreamingStates.Num());
			}
		}
	}
}
#endif

void FStreamingManager::AsyncUpdate()
{
	LLM_SCOPE_BYTAG(Nanite);
	SCOPED_NAMED_EVENT(FStreamingManager_AsyncUpdate, FColor::Cyan);
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::AsyncUpdate);

	check(AsyncState.bUpdateActive);
	InstallReadyPages(AsyncState.NumReadyPages);

	if (!AsyncState.LatestReadbackBuffer)
		return;

	auto StreamingPriorityPredicate = []( const FStreamingRequest& A, const FStreamingRequest& B ) { return A.Priority > B.Priority; };

	PrioritizedRequestsHeap.Empty(NANITE_MAX_STREAMING_REQUESTS);

	uint32 NumLegacyRequestsIssued = 0;

	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessReadback);
	const uint32* BufferPtr = AsyncState.LatestReadbackBufferPtr;
	const uint32 NumGPUStreamingRequests = FMath::Min(BufferPtr[0], NANITE_MAX_STREAMING_REQUESTS - 1u);	// First request is reserved for counter

	RequestsHashTable->Clear();

	if(GNaniteStreamingGPURequests && NumGPUStreamingRequests > 0)
	{
		// Update priorities
		const FGPUStreamingRequest* StreamingRequestsPtr = ((const FGPUStreamingRequest*)BufferPtr + 1);
#if NANITE_SANITY_CHECK_STREAMING_REQUESTS
		SanityCheckStreamingRequests(StreamingRequestsPtr, NumGPUStreamingRequests);
#endif

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DeduplicateGPURequests);
			for( uint32 Index = 0; Index < NumGPUStreamingRequests; Index++ )
			{
				const FGPUStreamingRequest& GPURequest = StreamingRequestsPtr[ Index ];
				const uint32 NumPages		= (GPURequest.PageIndex_NumPages_Magic >> NANITE_STREAMING_REQUEST_MAGIC_BITS) & NANITE_MAX_GROUP_PARTS_MASK;
				const uint32 PageStartIndex	= GPURequest.PageIndex_NumPages_Magic >> (NANITE_STREAMING_REQUEST_MAGIC_BITS + NANITE_MAX_GROUP_PARTS_BITS);
					
				FStreamingRequest Request;
				Request.Key.RuntimeResourceID	= (GPURequest.RuntimeResourceID_Magic >> NANITE_STREAMING_REQUEST_MAGIC_BITS);
				Request.Priority				= GPURequest.Priority_Magic & ~NANITE_STREAMING_REQUEST_MAGIC_MASK;
				for (uint32 i = 0; i < NumPages; i++)
				{
					Request.Key.PageIndex = PageStartIndex + i;
					RequestsHashTable->AddRequest(Request);
				}
			}
		}

		INC_DWORD_STAT_BY( STAT_NaniteGPURequests, NumGPUStreamingRequests );

#if WITH_EDITOR
		RecordGPURequests();
#endif
	}

	// Add any pending explicit requests
	AddPendingExplicitRequests();

	// Add any requests coming from pending resource prefetch hints
	AddPendingResourcePrefetchRequests();
	
	const uint32 NumUniqueRequests = RequestsHashTable->GetNumElements();
	INC_DWORD_STAT_BY(STAT_NaniteUniqueRequests, NumUniqueRequests);

	if (NumUniqueRequests > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UpdatePriorities);

		TSet<uint32> ResourcesWithRequests;
		uint32 NumNewPageRequests = 0;

		struct FPrioritizedStreamingPage
		{
			FStreamingPageInfo* Page;
			uint32 Priority;
		};

		TArray< FPrioritizedStreamingPage > UpdatedPages;
		for(uint32 UniqueRequestIndex = 0; UniqueRequestIndex < NumUniqueRequests; UniqueRequestIndex++)
		{
			const FStreamingRequest& Request = RequestsHashTable->GetElement(UniqueRequestIndex);
			FStreamingPageInfo** StreamingPage = RegisteredStreamingPagesMap.Find( Request.Key );
			if( StreamingPage )
			{
				// Update index and move to front of LRU.
				(*StreamingPage)->LatestUpdateIndex = NextUpdateIndex;
				UpdatedPages.Push( { *StreamingPage, Request.Priority } );
			}
			else
			{
				// Page isn't there. Is the resource still here?
				FResources** Resources = RuntimeResourceMap.Find( Request.Key.RuntimeResourceID );
				if( Resources )
				{
					// ResourcesID is valid, so add request to the queue
					PrioritizedRequestsHeap.Push( Request );

					++NumNewPageRequests;
					ResourcesWithRequests.Add(Request.Key.RuntimeResourceID);
				}
			}
		}

		INC_DWORD_STAT_BY(STAT_NaniteUniqueNewRequests, NumNewPageRequests);
		INC_DWORD_STAT_BY(STAT_NaniteUniqueNewRequestsResources, ResourcesWithRequests.Num());

		PrioritizedRequestsHeap.Heapify( StreamingPriorityPredicate );

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(PrioritySort);
			UpdatedPages.Sort( []( const FPrioritizedStreamingPage& A, const FPrioritizedStreamingPage& B ) { return A.Priority < B.Priority; } );
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UpdateLRU);

			for( const FPrioritizedStreamingPage& PrioritizedPage : UpdatedPages )
			{
				FStreamingPageInfo* Page = PrioritizedPage.Page;

				// Unlink
				FStreamingPageInfo* OldNext = Page->Next;
				FStreamingPageInfo* OldPrev = Page->Prev;
				OldNext->Prev = OldPrev;
				OldPrev->Next = OldNext;

				// Insert at the front of the LRU
				Page->Prev = &StreamingPageLRU;
				Page->Next = StreamingPageLRU.Next;
				StreamingPageLRU.Next->Prev = Page;
				StreamingPageLRU.Next = Page;
			}
		}
	}

#if DO_CHECK
	VerifyPageLRU( StreamingPageLRU, NumRegisteredStreamingPages, true );
#endif
			
	uint32 MaxSelectedPages = MaxPendingPages - NumPendingPages;
	if( PrioritizedRequestsHeap.Num() > 0 )
	{
#if WITH_EDITOR
		TArray<FCacheGetChunkRequest> DDCRequests;
		DDCRequests.Reserve(MaxSelectedPages);
#endif

		TArray< FPageKey > SelectedPages;
		TSet< FPageKey > SelectedPagesSet;
			
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SelectStreamingPages);

			if(MaxSelectedPages > 0)
			{
				// Add low priority pages based on prioritized requests
				while( (uint32)SelectedPages.Num() < MaxSelectedPages && PrioritizedRequestsHeap.Num() > 0 )
				{
					FStreamingRequest SelectedRequest;
					PrioritizedRequestsHeap.HeapPop( SelectedRequest, StreamingPriorityPredicate, false );
					FResources** Resources = RuntimeResourceMap.Find( SelectedRequest.Key.RuntimeResourceID );
					check( Resources != nullptr );

					const uint32 NumResourcePages = (uint32)(*Resources)->PageStreamingStates.Num();
					if (SelectedRequest.Key.PageIndex < NumResourcePages)
					{
						SelectStreamingPages(*Resources, SelectedPages, SelectedPagesSet, SelectedRequest.Key.RuntimeResourceID, SelectedRequest.Key.PageIndex, MaxSelectedPages);
					}
					else
					{
						checkf(false, TEXT(	"Reference to page index that is out of bounds: %d / %d. "
											"This could be caused by GPUScene corruption or issues with the GPU readback."),
											SelectedRequest.Key.PageIndex, NumResourcePages);
					}
				}
				check( (uint32)SelectedPages.Num() <= MaxSelectedPages );
			}
		}

		if( SelectedPages.Num() > 0 )
		{
			// Collect all pending registration dependencies so we are not going to remove them.
			TSet< FPageKey > RegistrationDependencyPages;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CollectDependencyPages);
				for (const FPageKey& SelectedKey : SelectedPages)
				{
					FResources** Resources = RuntimeResourceMap.Find(SelectedKey.RuntimeResourceID);
					check(Resources != nullptr);

					CollectDependencyPages(*Resources, RegistrationDependencyPages, SelectedKey);	// Mark all dependencies as unremovable.
				}
			}

			FBulkDataBatchRequest::FBatchBuilder Batch = FBulkDataBatchRequest::NewBatch(SelectedPages.Num());
			FPendingPage* LastPendingPage = nullptr;
				
			// Register Pages
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(RegisterPages);

				for( const FPageKey& SelectedKey : SelectedPages )
				{
					FPendingPage& PendingPage = PendingPages[ NextPendingPageIndex ];
					PendingPage = FPendingPage();

					FStreamingPageInfo** FreePage = nullptr;
						
					check(NumRegisteredStreamingPages <= MaxStreamingPages);
					if( NumRegisteredStreamingPages == MaxStreamingPages )
					{
						// No space. Free a page!
						FStreamingPageInfo* StreamingPage = StreamingPageLRU.Prev;
						while( StreamingPage != &StreamingPageLRU )
						{
							FStreamingPageInfo* PrevStreamingPage = StreamingPage->Prev;

							// Only remove leaf nodes. Make sure to never delete a node that was added this frame or is a dependency for a pending page registration.
							FPageKey FreeKey = PrevStreamingPage->RegisteredKey;
							if( PrevStreamingPage->RefCount == 0 && ( PrevStreamingPage->LatestUpdateIndex < NextUpdateIndex ) && RegistrationDependencyPages.Find( FreeKey ) == nullptr )
							{
								FreePage = RegisteredStreamingPagesMap.Find( FreeKey );
								check( FreePage != nullptr );
								check( (*FreePage)->RegisteredKey == FreeKey );
								break;
							}
							StreamingPage = PrevStreamingPage;
						}

						if (!FreePage)	// Couldn't free a page. Abort.
							break;
					}

					FResources** Resources = RuntimeResourceMap.Find(SelectedKey.RuntimeResourceID);
					check(Resources);
					FByteBulkData& BulkData = (*Resources)->StreamablePages;

#if WITH_EDITOR
					bool bLegacyRequest = false;
#else
					bool bLegacyRequest = !BulkData.IsUsingIODispatcher();
					if (bLegacyRequest)
					{
						if (NumLegacyRequestsIssued == MAX_LEGACY_REQUESTS_PER_UPDATE)
							break;
					}
#endif

					if (FreePage)
					{
						UnregisterPage((*FreePage)->RegisteredKey);
					}

					const FPageStreamingState& PageStreamingState = ( *Resources )->PageStreamingStates[ SelectedKey.PageIndex ];
					check( !(*Resources)->IsRootPage( SelectedKey.PageIndex ) );

#if WITH_EDITOR
					if((*Resources)->ResourceFlags & NANITE_RESOURCE_FLAG_STREAMING_DATA_IN_DDC)
					{
						DDCRequests.Add(BuildDDCRequest(**Resources, PageStreamingState, NextPendingPageIndex));
					}
					else
					{
						PendingPage.State = FPendingPage::EState::Ready;
					}
#else
					LastPendingPage = &PendingPage;
					uint8* Dst = PendingPageStagingMemory.GetData() + NextPendingPageIndex * NANITE_MAX_PAGE_DISK_SIZE;
					PendingPage.RequestBuffer = FIoBuffer(FIoBuffer::Wrap, Dst, PageStreamingState.BulkSize);
					Batch.Read(BulkData, PageStreamingState.BulkOffset, PageStreamingState.BulkSize, AIOP_Low, PendingPage.RequestBuffer, PendingPage.Request);
#endif
					const float RequestSizeMB = PageStreamingState.BulkSize * (1.0f / 1048576.0f);
					INC_FLOAT_STAT_BY(STAT_NaniteStreamingDiskIORequestMB, RequestSizeMB);

					// Grab a free page
					check(StreamingPageInfoFreeList != nullptr);
					FStreamingPageInfo* Page = StreamingPageInfoFreeList;
					StreamingPageInfoFreeList = StreamingPageInfoFreeList->Next;

					PendingPage.InstallKey = SelectedKey;
					PendingPage.GPUPageIndex = Page->GPUPageIndex;

					NextPendingPageIndex = ( NextPendingPageIndex + 1 ) % MaxPendingPages;
					NumPendingPages++;

#if !UE_BUILD_SHIPPING
					PendingPage.BytesLeftToStream = PageStreamingState.BulkSize;
#endif

					RegisterStreamingPage( Page, SelectedKey );
				}
			}

#if WITH_EDITOR
			if (DDCRequests.Num() > 0)
			{
				RequestDDCData(DDCRequests);
				DDCRequests.Empty();
			}
#else
			if (LastPendingPage)
			{
				// Issue batch
				TRACE_CPUPROFILER_EVENT_SCOPE(FIoBatch::Issue);
				(void)Batch.Issue();
			}
#endif
		}
	}

	// Issue warning if we end up taking the legacy path
	if (NumLegacyRequestsIssued > 0)
	{
		static bool bHasWarned = false;
		if(!bHasWarned)
		{
			UE_LOG(LogNaniteStreaming, Warning, TEXT(	"PERFORMANCE WARNING: Nanite is issuing IO requests using the legacy IO path. Expect slower streaming and higher CPU overhead. "
														"To avoid this penalty make sure iostore is enabled, it is supported by the platform, and that resources are built with -iostore."));
			bHasWarned = true;
		}
	}
}

void FStreamingManager::EndAsyncUpdate(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	TRACE_CPUPROFILER_EVENT_SCOPE(FStreamingManager::EndAsyncUpdate);
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteStreaming);
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	check(AsyncState.bUpdateActive);

	// Wait for async processing to finish
	if (GNaniteStreamingAsync)
	{
		check(!AsyncTaskEvents.IsEmpty());
		FTaskGraphInterface::Get().WaitUntilTasksComplete(AsyncTaskEvents, ENamedThreads::GetRenderThread_Local());
	}

	AsyncTaskEvents.Empty();

	// Unlock readback buffer
	if (AsyncState.LatestReadbackBuffer)
	{
		AsyncState.LatestReadbackBuffer->Unlock();
	}

	// Issue GPU copy operations
	if (AsyncState.NumReadyPages > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UploadPages);


		PageUploader->ResourceUploadTo(GraphBuilder, GraphBuilder.RegisterExternalBuffer(ClusterPageData.DataBuffer));
		Hierarchy.UploadBuffer.ResourceUploadTo(GraphBuilder, GraphBuilder.RegisterExternalBuffer(Hierarchy.DataBuffer));
		ClusterFixupUploadBuffer.ResourceUploadTo(GraphBuilder, GraphBuilder.RegisterExternalBuffer(ClusterPageData.DataBuffer));

		NumPendingPages -= AsyncState.NumReadyPages;
	}

	NextUpdateIndex++;
	AsyncState.bUpdateActive = false;
}

void FStreamingManager::SubmitFrameStreamingRequests(FRDGBuilder& GraphBuilder)
{
	check(IsInRenderingThread());
	if (!DoesPlatformSupportNanite(GMaxRHIShaderPlatform))
	{
		return;
	}

	LLM_SCOPE_BYTAG(Nanite);
	RDG_GPU_STAT_SCOPE(GraphBuilder, NaniteReadback);
	RDG_EVENT_SCOPE(GraphBuilder, "Nanite::Readback");

	if (ReadbackBuffersNumPending == MaxStreamingReadbackBuffers)
	{
		// Return when queue is full. It is NOT safe to EnqueueCopy on a buffer that already has a pending copy.
		return;
	}

	if (StreamingRequestReadbackBuffers[ReadbackBuffersWriteIndex] == nullptr)
	{
		FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("Nanite.StreamingRequestReadBack"));
		StreamingRequestReadbackBuffers[ReadbackBuffersWriteIndex] = GPUBufferReadback;
	}

	FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(StreamingRequestsBuffer);
	FRHIGPUBufferReadback* ReadbackBuffer = StreamingRequestReadbackBuffers[ReadbackBuffersWriteIndex];

	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("Readback"), Buffer,
		[ReadbackBuffer, Buffer](FRHICommandList& RHICmdList)
	{
		ReadbackBuffer->EnqueueCopy(RHICmdList, Buffer->GetRHI(), 0u);
	});

	ClearStreamingRequestCount(GraphBuilder, GraphBuilder.CreateUAV(Buffer));

	ReadbackBuffersWriteIndex = ( ReadbackBuffersWriteIndex + 1u ) % MaxStreamingReadbackBuffers;
	ReadbackBuffersNumPending = FMath::Min( ReadbackBuffersNumPending + 1u, MaxStreamingReadbackBuffers );
	StreamingRequestsBufferVersion++;
}

void FStreamingManager::ClearStreamingRequestCount(FRDGBuilder& GraphBuilder, FRDGBufferUAVRef BufferUAVRef)
{
	// Need to always clear streaming requests on all GPUs.  We sometimes write to streaming request buffers on a mix of
	// GPU masks (shadow rendering on all GPUs, other passes on a single GPU), and we need to make sure all are clear
	// when they get used again.
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	FClearStreamingRequestCount_CS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearStreamingRequestCount_CS::FParameters>();
	PassParameters->OutStreamingRequests = BufferUAVRef;

	auto ComputeShader = GetGlobalShaderMap(GMaxRHIFeatureLevel)->GetShader<FClearStreamingRequestCount_CS>();
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("ClearStreamingRequestCount"),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1)
	);
}


bool FStreamingManager::IsAsyncUpdateInProgress()
{
	return AsyncState.bUpdateActive;
}

void FStreamingManager::PrefetchResource(const FResources* Resources, uint32 NumFramesUntilRender)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	check(Resources);
	if (GNaniteStreamingPrefetch)
	{
		FResourcePrefetch Prefetch;
		Prefetch.RuntimeResourceID		= Resources->RuntimeResourceID;
		Prefetch.NumFramesUntilRender	= FMath::Min(NumFramesUntilRender, 30u);		// Make sure invalid values doesn't cause the request to stick around forever
		PendingResourcePrefetches.Add(Prefetch);
	}
}

void FStreamingManager::RequestNanitePages(TArrayView<uint32> RequestData)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	if (GNaniteStreamingExplicitRequests)
	{
		PendingExplicitRequests.Append(RequestData.GetData(), RequestData.Num());
	}
}

#if WITH_EDITOR
uint64 FStreamingManager::GetRequestRecordBuffer(TArray<uint32>& OutRequestData)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	if (PageRequestRecordHandle == (uint64)-1)
	{
		return (uint64)-1;
	}

	const uint64 Ret = PageRequestRecordHandle;
	PageRequestRecordHandle = (uint64)-1;
	if (PageRequestRecordMap.Num() == 0)
	{
		OutRequestData.Empty();
		return Ret;
	}

	// Resolve requests and convert to persistent resource IDs
	TArray<FStreamingRequest> Requests;
	Requests.Reserve(PageRequestRecordMap.Num());
	for (const TPair<FPageKey, uint32>& MapEntry : PageRequestRecordMap)
	{
		FResources** Resources = RuntimeResourceMap.Find(MapEntry.Key.RuntimeResourceID);
		if (Resources)
		{	
			Requests.Add(FStreamingRequest { FPageKey { (*Resources)->PersistentHash, MapEntry.Key.PageIndex }, MapEntry.Value } );
		}
	}
	PageRequestRecordMap.Reset();

	Requests.Sort();

	// Count unique resources
	uint32 NumUniqueResources = 0;
	{
		uint64 PrevPersistentHash = NANITE_INVALID_PERSISTENT_HASH;
		for (const FStreamingRequest& Request : Requests)
		{
			if (Request.Key.RuntimeResourceID != PrevPersistentHash)
			{
				NumUniqueResources++;
			}
			PrevPersistentHash = Request.Key.RuntimeResourceID;
		}
	}
	
	// Write packed requests
	// A request consists of two DWORDs. A resource DWORD and a pageindex/priority/repeat DWORD.
	// The repeat bit indicates if the next request is to the same resource, so the resource DWORD can be omitted.
	// As there are often many requests per resource, this encoding can safe upwards of half of the total DWORDs.
	{
		const uint32 NumOutputDwords = NumUniqueResources + Requests.Num();
		OutRequestData.SetNum(NumOutputDwords);
		uint32 WriteIndex = 0;
		uint64 PrevResourceID = ~0ull;
		for (const FStreamingRequest& Request : Requests)
		{
			check(Request.Key.PageIndex < NANITE_MAX_RESOURCE_PAGES);
			if (Request.Key.RuntimeResourceID != PrevResourceID)
			{
				OutRequestData[WriteIndex++] = Request.Key.RuntimeResourceID;
			}
			else
			{
				OutRequestData[WriteIndex - 1] |= 1;	// Mark resource repeat bit in previous packed dword
 			}
			PrevResourceID = Request.Key.RuntimeResourceID;

			const uint32 QuantizedPriority = Request.Priority >> (NANITE_MAX_RESOURCE_PAGES_BITS + 1);	// Exact priority doesn't matter, so just quantize it to fit
			const uint32 Packed = (QuantizedPriority << (NANITE_MAX_RESOURCE_PAGES_BITS + 1)) | (Request.Key.PageIndex << 1);	// Lowest bit is resource repeat bit
			OutRequestData[WriteIndex++] = Packed;
		}

		check(WriteIndex == NumOutputDwords);
	}
	
	return Ret;
}

void FStreamingManager::SetRequestRecordBuffer(uint64 Handle)
{
	check(IsInRenderingThread());
	check(!AsyncState.bUpdateActive);
	PageRequestRecordHandle = Handle;
	PageRequestRecordMap.Empty();
}

void FStreamingManager::RecordGPURequests()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RecordGPURequests);
	if (PageRequestRecordHandle != (uint64)-1)
	{
		const uint32 NumUniqueRequests = RequestsHashTable->GetNumElements();
		for (uint32 Index = 0; Index < NumUniqueRequests; Index++)
		{
			const FStreamingRequest& Request = RequestsHashTable->GetElement(Index);
			uint32* Priority = PageRequestRecordMap.Find(Request.Key);
			if (Priority)
				*Priority = FMath::Max(*Priority, Request.Priority);
			else
				PageRequestRecordMap.Add(Request.Key, Request.Priority);
		}
	}
}

FCacheGetChunkRequest FStreamingManager::BuildDDCRequest(const FResources& Resources, const FPageStreamingState& PageStreamingState, const uint32 PendingPageIndex)
{
	FCacheKey Key;
	Key.Bucket = FCacheBucket(TEXT("StaticMesh"));
	Key.Hash = Resources.DDCKeyHash;
	check(!Resources.DDCRawHash.IsZero());

	FCacheGetChunkRequest Request;
	Request.Id			= NaniteValueId;
	Request.Key			= Key;
	Request.RawOffset	= PageStreamingState.BulkOffset;
	Request.RawSize		= PageStreamingState.BulkSize;
	Request.RawHash		= Resources.DDCRawHash;
	Request.UserData	= PendingPageIndex;
	return Request;
}

void FStreamingManager::RequestDDCData(TConstArrayView<FCacheGetChunkRequest> DDCRequests)
{
	FRequestBarrier Barrier(*RequestOwner);	// This is a critical section on the owner. It does not constrain ordering
	GetCache().GetChunks(DDCRequests, *RequestOwner,
		[this](FCacheGetChunkResponse&& Response)
		{
			const uint32 PendingPageIndex = (uint32)Response.UserData;
			FPendingPage& PendingPage = PendingPages[PendingPageIndex];

			//const bool bRandomFalure = FMath::RandHelper(16) == 0;
			const bool bRandomFalure = false;
			if (Response.Status == EStatus::Ok && !bRandomFalure)
			{
				PendingPage.SharedBuffer = MoveTemp(Response.RawData);
				PendingPage.State = FPendingPage::EState::Ready;
			}
			else
			{
				PendingPage.State = FPendingPage::EState::Failed;
			}
		});
}

#endif // WITH_EDITOR

TGlobalResource< FStreamingManager > GStreamingManager;

} // namespace Nanite
