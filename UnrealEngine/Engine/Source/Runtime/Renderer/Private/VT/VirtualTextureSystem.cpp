// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSystem.h"

#include "AllocatedVirtualTexture.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Debug/DebugDrawService.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialRenderProxy.h"
#include "MaterialShared.h"
#include "PostProcess/SceneRenderTargets.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RendererOnScreenNotification.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "Stats/Stats.h"
#include "VirtualTexturing.h"
#include "VT/AdaptiveVirtualTexture.h"
#include "VT/TexturePagePool.h"
#include "VT/UniquePageList.h"
#include "VT/UniqueRequestList.h"
#include "VT/VirtualTextureFeedback.h"
#include "VT/VirtualTexturePhysicalSpace.h"
#include "VT/VirtualTexturePoolConfig.h"
#include "VT/VirtualTextureScalability.h"
#include "VT/VirtualTextureSpace.h"

#define LOCTEXT_NAMESPACE "VirtualTexture"

CSV_DEFINE_CATEGORY(VirtualTexturing, true);

DECLARE_CYCLE_STAT(TEXT("VirtualTextureSystem Update"), STAT_VirtualTextureSystem_Update, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Gather Requests"), STAT_ProcessRequests_Gather, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Sort Requests"), STAT_ProcessRequests_Sort, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Requests"), STAT_ProcessRequests_Submit, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Map Requests"), STAT_ProcessRequests_Map, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Map New VTs"), STAT_ProcessRequests_MapNew, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Finalize Requests"), STAT_ProcessRequests_Finalize, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Merge Unique Pages"), STAT_ProcessRequests_MergePages, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Merge Requests"), STAT_ProcessRequests_MergeRequests, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Submit Tasks"), STAT_ProcessRequests_SubmitTasks, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Wait Tasks"), STAT_ProcessRequests_WaitTasks, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Queue Adaptive Requests"), STAT_ProcessRequests_QueueAdaptiveRequests, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Finalize Adaptive Requests"), STAT_ProcessRequests_UpdateAdaptiveAllocations, STATGROUP_VirtualTexturing);

DECLARE_CYCLE_STAT(TEXT("Feedback Map"), STAT_FeedbackMap, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Feedback Analysis"), STAT_FeedbackAnalysis, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Page Table Updates"), STAT_PageTableUpdates, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Flush Cache"), STAT_FlushCache, STATGROUP_VirtualTexturing);
DECLARE_CYCLE_STAT(TEXT("Update Residency Tracking"), STAT_ResidencyTracking, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible"), STAT_NumPageVisible, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible resident"), STAT_NumPageVisibleResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page visible not resident"), STAT_NumPageVisibleNotResident, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page prefetch"), STAT_NumPagePrefetch, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page update"), STAT_NumPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num mapped page update"), STAT_NumMappedPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num continuous page update"), STAT_NumContinuousPageUpdate, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num page allocation fails"), STAT_NumPageAllocateFails, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks requested"), STAT_NumStacksRequested, STATGROUP_VirtualTexturing);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num stacks produced"), STAT_NumStacksProduced, STATGROUP_VirtualTexturing);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num flush caches"), STAT_NumFlushCache, STATGROUP_VirtualTexturing);

DECLARE_MEMORY_STAT_POOL(TEXT("Total Pagetable Memory"), STAT_TotalPagetableMemory, STATGROUP_VirtualTextureMemory, FPlatformMemory::MCR_GPU);

DECLARE_GPU_STAT(VirtualTexture);
DECLARE_GPU_STAT(VirtualTextureAllocate);

static TAutoConsoleVariable<int32> CVarVTVerbose(
	TEXT("r.VT.Verbose"),
	0,
	TEXT("Be pedantic about certain things that shouln't occur unless something is wrong. This may cause a lot of logspam 100's of lines per frame."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVTEnableFeedback(
	TEXT("r.VT.EnableFeedback"),
	1,
	TEXT("Enable processing of the GPU generated feedback buffer."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTParallelFeedbackTasks(
	TEXT("r.VT.ParallelFeedbackTasks"),
	0,
	TEXT("Use worker threads for virtual texture feedback tasks."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTNumFeedbackTasks(
	TEXT("r.VT.NumFeedbackTasks"),
	1,
	TEXT("Number of tasks to create to read virtual texture feedback."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTEnablePlayback(
	TEXT("r.VT.EnablePlayback"),
	1,
	TEXT("Enable playback of recorded feedback requests."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<float> CVarVTPlaybackMipBias(
	TEXT("r.VT.PlaybackMipBias"),
	0,
	TEXT("Mip bias to apply during playback of recorded feedback requests."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTNumGatherTasks(
	TEXT("r.VT.NumGatherTasks"),
	1,
	TEXT("Number of tasks to create to combine virtual texture feedback."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTPageUpdateFlushCount(
	TEXT("r.VT.PageUpdateFlushCount"),
	8,
	TEXT("Number of page updates to buffer before attempting to flush by taking a lock."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTForceContinuousUpdate(
	TEXT("r.VT.ForceContinuousUpdate"),
	0,
	TEXT("Force continuous update on all virtual textures."),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTSyncProduceLockedTiles(
	TEXT("r.VT.SyncProduceLockedTiles"),
	1,
	TEXT("Should we sync loading when producing locked tiles"),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTProduceLockedTilesOnFlush(
	TEXT("r.VT.ProduceLockedTilesOnFlush"),
	1,
	TEXT("Should locked tiles be (re)produced when flushing the cache"),
	ECVF_RenderThreadSafe
);
static TAutoConsoleVariable<int32> CVarVTResidencyShow(
	TEXT("r.VT.Residency.Show"),
	0,
	TEXT("Show on screen HUD for virtual texture physical pool residency"),
	ECVF_Default
);
static TAutoConsoleVariable<int32> CVarVTResidencyNotify(
	TEXT("r.VT.Residency.Notify"),
	0,
	TEXT("Show on screen notifications for virtual texture physical pool residency"),
	ECVF_Default
);
static TAutoConsoleVariable<int32> CVarVTCsvStats(
	TEXT("r.VT.CsvStats"),
	1,
	TEXT("Send virtual texturing stats to CSV profiler\n")
	TEXT("0=off, 1=on, 2=verbose"),
	ECVF_Default
);
static TAutoConsoleVariable<int32> CVarVTAsyncPageRequestTask(
	TEXT("r.VT.AsyncPageRequestTask"),
	1,
	TEXT("Performs VT page requests on an async task."),
	ECVF_RenderThreadSafe
);

FVirtualTextureUpdateSettings::FVirtualTextureUpdateSettings()
{
	bEnableFeedback = CVarVTEnableFeedback.GetValueOnRenderThread() != 0;
	bEnablePlayback = CVarVTEnablePlayback.GetValueOnRenderThread() != 0;
	bForceContinuousUpdate = CVarVTForceContinuousUpdate.GetValueOnRenderThread() != 0;
	bParallelFeedbackTasks = CVarVTParallelFeedbackTasks.GetValueOnRenderThread() != 0;
	NumFeedbackTasks = CVarVTNumFeedbackTasks.GetValueOnRenderThread();
	NumGatherTasks = CVarVTNumGatherTasks.GetValueOnRenderThread();
	MaxGatherPagesBeforeFlush = CVarVTPageUpdateFlushCount.GetValueOnRenderThread();
	MaxRVTPageUploads = VirtualTextureScalability::GetMaxUploadsPerFrame();
	MaxSVTPageUploads = VirtualTextureScalability::GetMaxUploadsPerFrameForStreamingVT();
	MaxPagesProduced = VirtualTextureScalability::GetMaxPagesProducedPerFrame();
	MaxContinuousUpdates = VirtualTextureScalability::GetMaxContinuousUpdatesPerFrame();
}


static FORCEINLINE uint32 EncodePage(uint32 ID, uint32 vLevel, uint32 vTileX, uint32 vTileY)
{
	const uint32 vLevelPlus1 = vLevel + 1u;

	uint32 Page;
	Page = vTileX << 0;
	Page |= vTileY << 12;
	Page |= vLevelPlus1 << 24;
	Page |= ID << 28;
	return Page;
}

struct FPageUpdateBuffer
{
	static const uint32 PageCapacity = 128u;
	uint16 PhysicalAddresses[PageCapacity];
	uint32 PrevPhysicalAddress = ~0u;
	uint32 NumPages = 0u;
	uint32 NumPageUpdates = 0u;
	uint32 WorkingSetSize = 0u;
};

struct FFeedbackAnalysisParameters
{
	FVirtualTextureSystem* System = nullptr;
	const uint32* FeedbackBuffer = nullptr;
	FUniquePageList* UniquePageList = nullptr;
	uint32 FeedbackSize = 0u;
};

class FFeedbackAnalysisTask
{
public:
	explicit FFeedbackAnalysisTask(const FFeedbackAnalysisParameters& InParams) : Parameters(InParams) {}

	FFeedbackAnalysisParameters Parameters;

	static void DoTask(FFeedbackAnalysisParameters& InParams)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::FeedbackAnalysisTask);
		InParams.UniquePageList->Initialize();
		InParams.System->FeedbackAnalysisTask(InParams);
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
		DoTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyNormalThreadNormalTask; }
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FFeedbackAnalysisTask, STATGROUP_VirtualTexturing); }
};

struct FAddRequestedTilesParameters
{
	FVirtualTextureSystem* System = nullptr;
	uint32 LevelBias = 0u;
	const uint64* RequestBuffer = nullptr;
	uint32 NumRequests = 0u;
	FUniquePageList* UniquePageList = nullptr;
};

class FAddRequestedTilesTask
{
public:
	explicit FAddRequestedTilesTask(const FAddRequestedTilesParameters& InParams) : Parameters(InParams) {}

	FAddRequestedTilesParameters Parameters;

	static void DoTask(FAddRequestedTilesParameters& InParams)
	{
		InParams.UniquePageList->Initialize();
		InParams.System->AddRequestedTilesTask(InParams);
	}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
		DoTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyHiPriThreadHiPriTask; }
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FAddRequestedTilesTask, STATGROUP_VirtualTexturing); }
};

struct FGatherRequestsParameters
{
	FVirtualTextureSystem* System = nullptr;
	const FUniquePageList* UniquePageList = nullptr;
	FPageUpdateBuffer* PageUpdateBuffers = nullptr;
	FUniqueRequestList* RequestList = nullptr;
	uint32 PageUpdateFlushCount = 0u;
	uint32 PageStartIndex = 0u;
	uint32 NumPages = 0u;
	uint32 FrameRequested;
	bool bForceContinuousUpdate = false;
};

class FGatherRequestsTask
{
public:
	explicit FGatherRequestsTask(const FGatherRequestsParameters& InParams) : Parameters(InParams) {}

	FGatherRequestsParameters Parameters;

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::GatherRequestsTask);
		Parameters.RequestList->Initialize();
		Parameters.System->GatherRequestsTask(Parameters);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }
	ENamedThreads::Type GetDesiredThread() { return ENamedThreads::AnyHiPriThreadHiPriTask; }
	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FGatherRequestsTask, STATGROUP_VirtualTexturing); }
};

static FVirtualTextureSystem* GVirtualTextureSystem = nullptr;

void FVirtualTextureSystem::Initialize()
{
	if (!GVirtualTextureSystem)
	{
		GVirtualTextureSystem = new FVirtualTextureSystem();
	}
}

void FVirtualTextureSystem::Shutdown()
{
	if (GVirtualTextureSystem)
	{
		delete GVirtualTextureSystem;
		GVirtualTextureSystem = nullptr;
	}
}

FVirtualTextureSystem& FVirtualTextureSystem::Get()
{
	check(GVirtualTextureSystem);
	return *GVirtualTextureSystem;
}

FVirtualTextureSystem::FVirtualTextureSystem()
	: Frame(1024u) // Need to start on a high enough value that we'll be able to allocate pages
	, bFlushCaches(false)
	, FlushCachesCommand(TEXT("r.VT.Flush"), TEXT("Flush all the physical caches in the VT system."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::FlushCachesFromConsole))
	, DumpCommand(TEXT("r.VT.Dump"), TEXT("Dump a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::DumpFromConsole))
	, ListPhysicalPools(TEXT("r.VT.ListPhysicalPools"), TEXT("Dump a whole lot of info on the VT system state."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::ListPhysicalPoolsFromConsole))
	, DumpPoolUsageCommand(TEXT("r.VT.DumpPoolUsage"), TEXT("Dump detailed info about VT pool usage."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::DumpPoolUsageFromConsole))
#if WITH_EDITOR
	, SaveAllocatorImages(TEXT("r.VT.SaveAllocatorImages"), TEXT("Save images showing allocator usage."),
		FConsoleCommandDelegate::CreateRaw(this, &FVirtualTextureSystem::SaveAllocatorImagesFromConsole))
	, PageRequestRecordHandle(~0ull)
#endif
{
#if !UE_BUILD_SHIPPING
	OnScreenMessageDelegateHandle = FRendererOnScreenNotification::Get().AddLambda([this](FCoreDelegates::FSeverityMessageMap& OutMessages) { GetOnScreenMessages(OutMessages); });
	DrawResidencyHudDelegateHandle = UDebugDrawService::Register(TEXT("VirtualTextureResidency"), FDebugDrawDelegate::CreateRaw(this, &FVirtualTextureSystem::DrawResidencyHud));
#endif
}

FVirtualTextureSystem::~FVirtualTextureSystem()
{
#if !UE_BUILD_SHIPPING
	FRendererOnScreenNotification::Get().Remove(OnScreenMessageDelegateHandle);
	UDebugDrawService::Unregister(DrawResidencyHudDelegateHandle);
#endif

	DestroyPendingVirtualTextures(true);

	check(AllocatedVTs.Num() == 0);
	check(PersistentVTMap.Num() == 0);

	for (uint32 SpaceID = 0u; SpaceID < MaxSpaces; ++SpaceID)
	{
		FVirtualTextureSpace* Space = Spaces[SpaceID].Get();
		if (Space)
		{
			check(Space->GetRefCount() == 0u);
			DEC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
			BeginReleaseResource(Space);
		}
	}
	for(int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace)
		{
			check(PhysicalSpace->GetRefCount() == 0u);
			BeginReleaseResource(PhysicalSpace);
		}
	}
}

void FVirtualTextureSystem::FlushCachesFromConsole()
{
	FlushCache();
}

void FVirtualTextureSystem::FlushCache()
{
	// We defer the actual flush to the render thread in the Update function
	bFlushCaches = true;
}

void FVirtualTextureSystem::FlushCache(FVirtualTextureProducerHandle const& ProducerHandle, int32 SpaceID, FIntRect const& TextureRegion, uint32 MaxLevelToEvict, uint32 MaxAgeToKeepMapped)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);

	SCOPE_CYCLE_COUNTER(STAT_FlushCache);
	INC_DWORD_STAT_BY(STAT_NumFlushCache, 1);

	FVirtualTextureProducer const* Producer = Producers.FindProducer(ProducerHandle);
	if (Producer != nullptr)
	{
		FVTProducerDescription const& ProducerDescription = Producer->GetDescription();

		TArray<FVirtualTexturePhysicalSpace*> PhysicalSpacesForProducer;
		for (uint32 i = 0; i < Producer->GetNumPhysicalGroups(); ++i)
		{
			PhysicalSpacesForProducer.AddUnique(Producer->GetPhysicalSpaceForPhysicalGroup(i));
		}

		check(TransientCollectedPages.Num() == 0);

		const uint32 MinFrameToKeepMapped = Frame > MaxAgeToKeepMapped ? Frame - MaxAgeToKeepMapped : 0;

		// If this is an Adaptive VT we need to collect all of the associated Producers to flush.
		TArray<FAdaptiveVirtualTexture::FProducerInfo> ProducerInfos;
		if (AdaptiveVTs[SpaceID] != nullptr)
		{
			AdaptiveVTs[SpaceID]->GetProducers(TextureRegion, MaxLevelToEvict, ProducerInfos);
		}

		for (int32 i = 0; i < PhysicalSpacesForProducer.Num(); ++i)
		{
			FTexturePagePool& Pool = PhysicalSpacesForProducer[i]->GetPagePool();
		
			if (ProducerInfos.Num())
			{
				// Adaptive VT flushes.
				for (FAdaptiveVirtualTexture::FProducerInfo& Info : ProducerInfos)
				{
					Pool.EvictPages(this, Info.ProducerHandle, ProducerDescription, Info.RemappedTextureRegion, Info.RemappedMaxLevel, MinFrameToKeepMapped, TransientCollectedPages);
				}
			}
			else
			{
				// Regular flush.
				Pool.EvictPages(this, ProducerHandle, ProducerDescription, TextureRegion, MaxLevelToEvict, MinFrameToKeepMapped, TransientCollectedPages);
			}
		}

		for (auto& Page : TransientCollectedPages)
		{
			MappedTilesToProduce.Add(Page);
		}

		// Don't resize to allow this container to grow as needed (avoid allocations when collecting)
		TransientCollectedPages.Reset();
	}
}

void FVirtualTextureSystem::DumpFromConsole()
{
	bool verbose = false;
	for (int ID = 0; ID < 16; ID++)
	{
		FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space)
		{
			Space->DumpToConsole(verbose);
		}
	}
}

void FVirtualTextureSystem::ListPhysicalPoolsFromConsole()
{
	uint64 TotalPhysicalMemory = 0u;
	for(int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		if (PhysicalSpaces[i] && PhysicalSpaces[i]->IsInitialized())
		{
			const FVirtualTexturePhysicalSpace& PhysicalSpace = *PhysicalSpaces[i];
			const FVTPhysicalSpaceDescription& Desc = PhysicalSpace.GetDescription();
			const FTexturePagePool& PagePool = PhysicalSpace.GetPagePool();
			const uint32 TotalSizeInBytes = PhysicalSpace.GetSizeInBytes();

			UE_LOG(LogConsoleResponse, Display, TEXT("PhysicalPool: [%i] %s (%ix%i):"), i, *PhysicalSpace.GetFormatString(), Desc.TileSize, Desc.TileSize);
			
			const int32 AllocatedTiles = PagePool.GetNumAllocatedPages();
			const float AllocatedLoad = (float)AllocatedTiles / (float)PhysicalSpace.GetNumTiles();
			const float AllocatedMemory = AllocatedLoad * TotalSizeInBytes / 1024.0f / 1024.0f;

			const int32 LockedTiles = PagePool.GetNumLockedPages();
			const float LockedLoad = (float)LockedTiles / (float)PhysicalSpace.GetNumTiles();
			const float LockedMemory = LockedLoad * TotalSizeInBytes / 1024.0f / 1024.0f;

			UE_LOG(LogConsoleResponse, Display, TEXT("  SizeInMegabyte= %f"), (float)TotalSizeInBytes / 1024.0f / 1024.0f);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Dimensions= %ix%i"), PhysicalSpace.GetTextureSize(), PhysicalSpace.GetTextureSize());
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles= %i"), PhysicalSpace.GetNumTiles());
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Allocated= %i (%fMB)"), AllocatedTiles, AllocatedMemory);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Locked= %i (%fMB)"), LockedTiles, LockedMemory);
			UE_LOG(LogConsoleResponse, Display, TEXT("  Tiles Mapped= %i"), PagePool.GetNumMappedPages());

			TotalPhysicalMemory += TotalSizeInBytes;
		}
	}

	uint64 TotalPageTableMemory = 0u;
	for (int ID = 0; ID < 16; ID++)
	{
		const FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space == nullptr)
		{
			continue;
		}

		const FVTSpaceDescription& Desc = Space->GetDescription();
		const FVirtualTextureAllocator& Allocator = Space->GetAllocator();
		const uint32 PageTableWidth = Space->GetPageTableWidth();
		const uint32 PageTableHeight = Space->GetPageTableHeight();
		const uint32 TotalSizeInBytes = Space->GetSizeInBytes();
		const uint32 NumAllocatedPages = Allocator.GetNumAllocatedPages();
		const uint32 NumTotalPages = PageTableWidth * PageTableHeight;
		const double AllocatedRatio = (double)NumAllocatedPages / NumTotalPages;

		const uint32 PhysicalTileSize = Desc.TileSize + Desc.TileBorderSize * 2u;
		const TCHAR* FormatName = nullptr;
		switch (Desc.PageTableFormat)
		{
		case EVTPageTableFormat::UInt16: FormatName = TEXT("UInt16"); break;
		case EVTPageTableFormat::UInt32: FormatName = TEXT("UInt32"); break;
		default: checkNoEntry(); break;
		}

		UE_LOG(LogConsoleResponse, Display, TEXT("Pool: [%i] %s (%ix%i) x %i:"), ID, FormatName, PhysicalTileSize, PhysicalTileSize, Desc.NumPageTableLayers);
		UE_LOG(LogConsoleResponse, Display, TEXT("  PageTableSize= %ix%i"), PageTableWidth, PageTableHeight);
		UE_LOG(LogConsoleResponse, Display, TEXT("  Allocations= %i, %i%% (%fMB)"),
			Allocator.GetNumAllocations(),
			(int)(AllocatedRatio * 100.0),
			(float)(AllocatedRatio * TotalSizeInBytes / 1024.0 / 1024.0));

		TotalPageTableMemory += TotalSizeInBytes;
	}

	UE_LOG(LogConsoleResponse, Display, TEXT("TotalPageTableMemory: %fMB"), (double)TotalPageTableMemory / 1024.0 / 1024.0);
	UE_LOG(LogConsoleResponse, Display, TEXT("TotalPhysicalMemory: %fMB"), (double)TotalPhysicalMemory / 1024.0 / 1024.0);
}

void FVirtualTextureSystem::DumpPoolUsageFromConsole()
{
	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		if (PhysicalSpaces[i])
		{
			const FVirtualTexturePhysicalSpace& PhysicalSpace = *PhysicalSpaces[i];
			const FVTPhysicalSpaceDescription& Desc = PhysicalSpace.GetDescription();
			const FTexturePagePool& PagePool = PhysicalSpace.GetPagePool();

			UE_LOG(LogConsoleResponse, Display, TEXT("PhysicalPool: [%i] %s (%ix%i):"), i, *PhysicalSpace.GetFormatString(), Desc.TileSize, Desc.TileSize);

			TMap<uint32, uint32> ProducerCountMap;
			PagePool.CollectProducerCounts(ProducerCountMap);

			TSet< TPair<uint32, uint32> > SortedProducerCounts;
			for (TPair<uint32, uint32> ProducerCount : ProducerCountMap)
			{
				// Filter out producers that only have locked page mapped.
				// Keep all producers for non compressed formats (assuming that we want to avoid these as much as possible).
				// In future we can add other filters here (count, age etc), or we can dump more info and process off line.
				if (ProducerCount.Value > 1 || GPixelFormats[PhysicalSpace.GetFormat(0)].BlockSizeX == 1)
				{
					SortedProducerCounts.Add(ProducerCount);
				}
			}
			SortedProducerCounts.Sort([](TPair<uint32, uint32> const& LHS, TPair<uint32, uint32> const& RHS) { return LHS.Value > RHS.Value; });

			for (TPair<uint32, uint32> ProducerCount : SortedProducerCounts)
			{
				const uint32 PackedProducerHandle = ProducerCount.Key;
				const uint32 Count = ProducerCount.Value;

				FVirtualTextureProducer* Producer = Producers.FindProducer(FVirtualTextureProducerHandle(PackedProducerHandle));
				if (Producer != nullptr)
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("   %s %d"), *Producer->GetName().ToString(), Count);
				}
			}
		}
	}
}

#if WITH_EDITOR
void FVirtualTextureSystem::SaveAllocatorImagesFromConsole()
{
	for (int ID = 0; ID < 16; ID++)
	{
		const FVirtualTextureSpace* Space = Spaces[ID].Get();
		if (Space)
		{
			Space->SaveAllocatorDebugImage();
		}
	}
}
#endif // WITH_EDITOR

IAllocatedVirtualTexture* FVirtualTextureSystem::AllocateVirtualTexture(FRHICommandListBase& RHICmdList, const FAllocatedVTDescription& Desc)
{
	check(Desc.NumTextureLayers <= VIRTUALTEXTURE_SPACE_MAXLAYERS);

	UE::TScopeLock Lock(Mutex);

	// Check to see if we already have an allocated VT that matches this description
	// This can happen often as multiple material instances will share the same textures
	FAllocatedVirtualTexture*& AllocatedVT = AllocatedVTs.FindOrAdd(Desc);
	if (AllocatedVT)
	{
		const int32 PrevNumRefs = AllocatedVT->NumRefs++;
		check(PrevNumRefs >= 0);
		if (PrevNumRefs == 0)
		{
			// Bringing a VT 'back to life', remove it from the pending delete list
			verify(PendingDeleteAllocatedVTs.RemoveSwap(AllocatedVT, EAllowShrinking::No) == 1);
		}

		return AllocatedVT;
	}

	uint32 BlockWidthInTiles = 0u;
	uint32 BlockHeightInTiles = 0u;
	uint32 WidthInBlocks = 0u;
	uint32 HeightInBlocks = 0u;
	uint32 DepthInTiles = 0u;
	FVirtualTextureProducer* ProducerForLayer[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { nullptr };
	bool bAnyLayerProducerWantsPersistentHighestMip = false;
	for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
	{
		FVirtualTextureProducer* Producer = Producers.FindProducer(Desc.ProducerHandle[LayerIndex]);
		ProducerForLayer[LayerIndex] = Producer;
		if (Producer)
		{
			const FVTProducerDescription& ProducerDesc = Producer->GetDescription();
			BlockWidthInTiles = FMath::Max(BlockWidthInTiles, ProducerDesc.BlockWidthInTiles);
			BlockHeightInTiles = FMath::Max(BlockHeightInTiles, ProducerDesc.BlockHeightInTiles);
			WidthInBlocks = FMath::Max<uint32>(WidthInBlocks, ProducerDesc.WidthInBlocks);
			HeightInBlocks = FMath::Max<uint32>(HeightInBlocks, ProducerDesc.HeightInBlocks);
			DepthInTiles = FMath::Max(DepthInTiles, ProducerDesc.DepthInTiles);

			uint32 ProducerLayerIndex = Desc.ProducerLayerIndex[LayerIndex];
			uint32 ProducerPhysicalGroup = Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex);
			FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroup);
			bAnyLayerProducerWantsPersistentHighestMip |= Producer->GetDescription().bPersistentHighestMip;
		}
	}

	check(BlockWidthInTiles > 0u);
	check(BlockHeightInTiles > 0u);
	check(DepthInTiles > 0u);
	check(WidthInBlocks > 0u);
	check(HeightInBlocks > 0u);

	// Sum the total number of physical groups from all producers
	uint32 NumPhysicalGroups = 0;
	if (Desc.bShareDuplicateLayers)
	{
		TArray<FVirtualTextureProducer*> UniqueProducers;
		for (uint32 LayerIndex = 0u; LayerIndex < Desc.NumTextureLayers; ++LayerIndex)
		{
			if (ProducerForLayer[LayerIndex] != nullptr)
			{
				UniqueProducers.AddUnique(ProducerForLayer[LayerIndex]);
			}
		}
		for (int32 ProducerIndex = 0u; ProducerIndex < UniqueProducers.Num(); ++ProducerIndex)
		{
			NumPhysicalGroups += UniqueProducers[ProducerIndex]->GetNumPhysicalGroups();
		}
	}
	else
	{
		NumPhysicalGroups = Desc.NumTextureLayers;
	}

	if (NumAllocatedSpaces == MaxSpaces)
	{
		// We have already allocated the maximum number of spaces. If this allocation needs a new space then it will fail.
		// To give us the best chance of success, flush all pending destroys now. This may free up a space.
		// This is potentially an expensive operation, which is why we only do it if necessary.
		DestroyPendingVirtualTextures(true);
	}

	AllocatedVT = new FAllocatedVirtualTexture(RHICmdList, this, Frame, Desc, ProducerForLayer, BlockWidthInTiles, BlockHeightInTiles, WidthInBlocks, HeightInBlocks, DepthInTiles);
	AllocatedVT->NumRefs = 1;
	if (bAnyLayerProducerWantsPersistentHighestMip)
	{
		AllocatedVTsToMap.Add(AllocatedVT);
	}

	// Add to deterministic map that should apply across runs.
	// Note that this may overwrite a duplicate old mapping whenever a new AllocatedVT is recreated after its producers have been recreated.
	// In contrast AllocatedVTs maintains the duplicate entries temporarily until the older entry is deleted.
	PersistentVTMap.Add(TPair<uint32, IAllocatedVirtualTexture*>(AllocatedVT->GetPersistentHash(), AllocatedVT));
	
	return AllocatedVT;
}

void FVirtualTextureSystem::DestroyVirtualTexture(IAllocatedVirtualTexture* AllocatedVT)
{
	UE::TScopeLock Lock(Mutex);
	const int32 NewNumRefs = --AllocatedVT->NumRefs;
	check(NewNumRefs >= 0);
	if (NewNumRefs == 0)
	{
		AllocatedVT->FrameDeleted = Frame;
		PendingDeleteAllocatedVTs.Add(AllocatedVT);
	}
}

void FVirtualTextureSystem::DestroyPendingVirtualTextures(bool bForceDestroyAll)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::DestroyPendingVirtualTextures);

	TArray<IAllocatedVirtualTexture*> AllocatedVTsToDelete;
	{
		if (bForceDestroyAll)
		{
			AllocatedVTsToDelete = MoveTemp(PendingDeleteAllocatedVTs);
			PendingDeleteAllocatedVTs.Reset();
		}
		else
		{
			const int32 MaxDeleteBudget = VirtualTextureScalability::GetMaxAllocatedVTReleasedPerFrame();
			const uint32 CurrentFrame = Frame;
			int32 Index = 0;
			while (Index < PendingDeleteAllocatedVTs.Num())
			{
				IAllocatedVirtualTexture* AllocatedVT = PendingDeleteAllocatedVTs[Index];
				const FAllocatedVTDescription& Desc = AllocatedVT->GetDescription();
				check(AllocatedVT->NumRefs == 0);

				// If the AllocatedVT is using a private space release it immediately, we don't want to hold references to these private spaces any longer then needed.
				const bool bForceDelete = Desc.bPrivateSpace;
				// Keep deleted VTs around for a few frames, in case they are reused.
				const bool bCanDeleteForAge = CurrentFrame >= AllocatedVT->FrameDeleted + 60u;
				// Time slice deletion unless we can make it cheaper.
				const bool bCanDeleteForBudget = MaxDeleteBudget <= 0 || AllocatedVTsToDelete.Num() < MaxDeleteBudget;
				if (bForceDelete || (bCanDeleteForAge && bCanDeleteForBudget))
				{
					AllocatedVTsToDelete.Add(AllocatedVT);
					PendingDeleteAllocatedVTs.RemoveAtSwap(Index, 1, EAllowShrinking::No);
				}
				else
				{
					Index++;
				}
			}
		}
	}

	for (IAllocatedVirtualTexture* AllocatedVT : AllocatedVTsToDelete)
	{
		// shouldn't be more than 1 instance of this in the list
		verify(AllocatedVTsToMap.Remove(AllocatedVT) <= 1);
		verify(AllocatedVTs.Remove(AllocatedVT->GetDescription()) == 1);
		
		// persistent entry might have already been reallocated
		IAllocatedVirtualTexture** Found = PersistentVTMap.Find(AllocatedVT->GetPersistentHash());
		if (Found != nullptr && *Found == AllocatedVT)
		{
			PersistentVTMap.Remove(AllocatedVT->GetPersistentHash());
		}
		
		AllocatedVT->Destroy(this);
		delete AllocatedVT;
	}
}

IAdaptiveVirtualTexture* FVirtualTextureSystem::AllocateAdaptiveVirtualTexture(FRHICommandListBase& RHICmdList, const FAdaptiveVTDescription& AdaptiveVTDesc, const FAllocatedVTDescription& AllocatedVTDesc)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	FAdaptiveVirtualTexture* AdaptiveVT = new FAdaptiveVirtualTexture(AdaptiveVTDesc, AllocatedVTDesc);
	AdaptiveVT->Init(RHICmdList, this);
	check(AdaptiveVTs[AdaptiveVT->GetSpaceID()] == nullptr);
	AdaptiveVTs[AdaptiveVT->GetSpaceID()] = AdaptiveVT;
	return AdaptiveVT;
}

void FVirtualTextureSystem::DestroyAdaptiveVirtualTexture(IAdaptiveVirtualTexture* AdaptiveVT)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	check(AdaptiveVTs[AdaptiveVT->GetSpaceID()] == AdaptiveVT);
	AdaptiveVTs[AdaptiveVT->GetSpaceID()] = nullptr;
	AdaptiveVT->Destroy(this);
}

FVirtualTextureProducerHandle FVirtualTextureSystem::RegisterProducer(FRHICommandListBase& RHICmdList, const FVTProducerDescription& InDesc, IVirtualTexture* InProducer)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	return Producers.RegisterProducer(RHICmdList, this, InDesc, InProducer);
}

void FVirtualTextureSystem::ReleaseProducer(const FVirtualTextureProducerHandle& Handle)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	Producers.ReleaseProducer(this, Handle);
}

void FVirtualTextureSystem::AddProducerDestroyedCallback(const FVirtualTextureProducerHandle& Handle, FVTProducerDestroyedFunction* Function, void* Baton)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	Producers.AddDestroyedCallback(Handle, Function, Baton);
}

uint32 FVirtualTextureSystem::RemoveAllProducerDestroyedCallbacks(const void* Baton)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	return Producers.RemoveAllCallbacks(Baton);
}

FVirtualTextureProducer* FVirtualTextureSystem::FindProducer(const FVirtualTextureProducerHandle& Handle)
{
	return Producers.FindProducer(Handle);
}

FVirtualTextureSpace* FVirtualTextureSystem::AcquireSpace(FRHICommandListBase& RHICmdList, const FVTSpaceDescription& InDesc, uint8 InForceSpaceID, FAllocatedVirtualTexture* AllocatedVT)
{
	check(!bUpdating);
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	uint32 NumFailedAllocations = 0u;

	// If InDesc requests a private space, don't reuse any existing spaces (unless it is a forced space)
	if (!InDesc.bPrivateSpace || InForceSpaceID != 0xff)
	{
		for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
		{
			if (SpaceIndex == InForceSpaceID || InForceSpaceID == 0xff)
			{
				FVirtualTextureSpace* Space = Spaces[SpaceIndex].Get();
				if (Space && Space->GetDescription() == InDesc)
				{
					const int32 PagetableMemory = Space->GetSizeInBytes();
					const uint32 vAddress = Space->AllocateVirtualTexture(AllocatedVT);
					if (vAddress != ~0u)
					{
						const int32 NewPagetableMemory = Space->GetSizeInBytes();
						INC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, NewPagetableMemory - PagetableMemory);

						AllocatedVT->AssignVirtualAddress(vAddress);
						Space->AddRef();
						return Space;
					}
					else
					{
						++NumFailedAllocations;
					}
				}
			}
		}
	}

	// Try to allocate a new space
	if (InForceSpaceID == 0xff)
	{
		for (uint32 SpaceIndex = 0u; SpaceIndex < MaxSpaces; ++SpaceIndex)
		{
			if (!Spaces[SpaceIndex])
			{
				const uint32 InitialPageTableSize = InDesc.bPrivateSpace ? InDesc.MaxSpaceSize : FMath::Max(AllocatedVT->GetWidthInTiles(), AllocatedVT->GetHeightInTiles());
				FVirtualTextureSpace* Space = new FVirtualTextureSpace(this, SpaceIndex, InDesc, InitialPageTableSize);
				Spaces[SpaceIndex].Reset(Space);
				NumAllocatedSpaces++;
				INC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
				Space->InitResource(RHICmdList);

				const uint32 vAddress = Space->AllocateVirtualTexture(AllocatedVT);
				AllocatedVT->AssignVirtualAddress(vAddress);

				Space->AddRef();
				return Space;
			}
		}
	}

	// Out of space slots
	checkf(false, TEXT("Failed to acquire space for VT (%d x %d), failed to allocate from %d existing matching spaces"),
		AllocatedVT->GetWidthInTiles(), AllocatedVT->GetHeightInTiles(), NumFailedAllocations);
	return nullptr;
}

void FVirtualTextureSystem::ReleaseSpace(FVirtualTextureSpace* Space)
{
	check(!bUpdating);
	const uint32 NumRefs = Space->Release();
	if (NumRefs == 0u && Space->GetDescription().bPrivateSpace)
	{
		// Private spaces are destroyed when ref count reaches 0
		// This can only happen on render thread, so we can call ReleaseResource() directly and then delete the pointer immediately
		DEC_MEMORY_STAT_BY(STAT_TotalPagetableMemory, Space->GetSizeInBytes());
		Space->ReleaseResource();
		Spaces[Space->GetID()].Reset();
		NumAllocatedSpaces--;
	}
}

/** Get the extra physical space description that depends on the virtual texture pool config. */
void GetPhysicalSpaceExtraDescription(FVTPhysicalSpaceDescription const& InDesc, FVTPhysicalSpaceDescriptionExt& OutDescExt)
{
	// Find matching config from pool settings.
	FVirtualTextureSpacePoolConfig Config;
	VirtualTexturePool::FindPoolConfig(InDesc.Format, InDesc.NumLayers, InDesc.TileSize, Config);
	int32 SizeInMegabyte = Config.SizeInMegabyte;

	// Adjust found config for scaling.
	const float Scale = Config.bAllowSizeScale ? VirtualTexturePool::GetPoolSizeScale() : 1.f;
	SizeInMegabyte = (int32)(Scale * (float)SizeInMegabyte);
	if (Scale < 1.f && Config.MinScaledSizeInMegabyte > 0)
	{
		SizeInMegabyte = FMath::Max(SizeInMegabyte, Config.MinScaledSizeInMegabyte);
	}
	if (Scale > 1.f && Config.MaxScaledSizeInMegabyte > 0)
	{
		SizeInMegabyte = FMath::Min(SizeInMegabyte, Config.MaxScaledSizeInMegabyte);
	}
	const int32 PoolSizeInBytes = SizeInMegabyte * 1024u * 1024u;

	// Get size of a single tile.
	const FPixelFormatInfo& FormatInfo = GPixelFormats[InDesc.Format[0]];
	check(InDesc.TileSize % FormatInfo.BlockSizeX == 0);
	check(InDesc.TileSize % FormatInfo.BlockSizeY == 0);
	SIZE_T TileSizeBytes = 0;
	for (int32 Layer = 0; Layer < InDesc.NumLayers; ++Layer)
	{
		TileSizeBytes += CalculateImageBytes(InDesc.TileSize, InDesc.TileSize, 0, InDesc.Format[Layer]);
	}

	// Calculate final size in tiles.
	// Loop to find matching pool count if necessary.
	int32 TileWidthHeight = 0;
	int32 PoolCount = 1;
	while (1)
	{
		const uint32 MaxTiles = FMath::Max((uint32)(PoolSizeInBytes / (PoolCount * TileSizeBytes)), 1u);

		TileWidthHeight = FMath::FloorToInt(FMath::Sqrt((float)MaxTiles));

		if (TileWidthHeight * InDesc.TileSize > GetMax2DTextureDimension())
		{
			// A good option to support extremely large caches would be to allow additional slices in an array here for caches...
			// Just try to use the maximum texture size for now
			TileWidthHeight = GetMax2DTextureDimension() / InDesc.TileSize;
			break;
		}

		const int32 SplitPhysicalPoolSize = VirtualTexturePool::GetSplitPhysicalPoolSize();
		if (SplitPhysicalPoolSize <= 0 || TileWidthHeight <= SplitPhysicalPoolSize)
		{
			break;
		}

		PoolCount++;
	}

	OutDescExt.TileWidthHeight = TileWidthHeight;
	OutDescExt.PoolCount = PoolCount;
	OutDescExt.bEnableResidencyMipMapBias = Config.bEnableResidencyMipMapBias;
}

/** Cached version of GetPhysicalSpaceExtraDescription() to avoid regularly repeating the heavy work in that function. */
void GetPhysicalSpaceExtraDescription_Cached(FVTPhysicalSpaceDescription const& InDesc, FVTPhysicalSpaceDescriptionExt& OutDescExt)
{
	static TMap<FVTPhysicalSpaceDescription, FVTPhysicalSpaceDescriptionExt> Map;

	// Invalidate the cache if any config settings change.
	uint32 PhysicalPoolSettingsHash = VirtualTexturePool::GetConfigHash();
	static uint32 LastPhysicalPoolSettingsHash = PhysicalPoolSettingsHash;
	if (LastPhysicalPoolSettingsHash != PhysicalPoolSettingsHash)
	{
		LastPhysicalPoolSettingsHash = PhysicalPoolSettingsHash;
		Map.Reset();
	}

	FVTPhysicalSpaceDescriptionExt* InitDescriptionPtr = Map.Find(InDesc);
	if (InitDescriptionPtr == nullptr)
	{
		GetPhysicalSpaceExtraDescription(InDesc, OutDescExt);
		Map.Add(InDesc, OutDescExt);
	}
	else
	{
		OutDescExt = *InitDescriptionPtr;
	}
}

FVirtualTexturePhysicalSpace* FVirtualTextureSystem::AcquirePhysicalSpace(FRHICommandListBase& RHICmdList, const FVTPhysicalSpaceDescription& InDesc)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	// Get extra setup information from the virtual pool configs.
	FVTPhysicalSpaceDescriptionExt DescExt;
	GetPhysicalSpaceExtraDescription_Cached(InDesc, DescExt);

	// Find matching pools.
	// We support multiple matching pools to allow for 16bit page table memory optimization.
	TArray<int32, TInlineAllocator<8>> Matching;
	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace && PhysicalSpace->GetDescription() == InDesc && PhysicalSpace->GetDescriptionExt() == DescExt)
		{
			Matching.Add(i);
		}
	}

	if (DescExt.PoolCount <= Matching.Num())
	{
		// Randomly select from any pools that exist.
		int32 RandomIndex = FMath::RandHelper(Matching.Num());
		return PhysicalSpaces[Matching[RandomIndex]];
	}
	
	// Not reached maximum matching pool count yet so create a new pool.
	uint32 ID = PhysicalSpaces.Num();
	check(ID <= 0x0fff);

	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		if (!PhysicalSpaces[i])
		{
			ID = i;
			break;
		}
	}

	if (ID == PhysicalSpaces.Num())
	{
		PhysicalSpaces.AddZeroed();
	}

	FVirtualTexturePhysicalSpace* PhysicalSpace = new FVirtualTexturePhysicalSpace(ID, InDesc, DescExt);
	PhysicalSpaces[ID] = PhysicalSpace;

	return PhysicalSpace;
}

void FVirtualTextureSystem::ReleasePendingSpaces()
{
	for (int32 Id = 0; Id < PhysicalSpaces.Num(); ++Id)
	{
		// Physical space is released when ref count hits 0
		// Might need to have some mechanism to hold an extra reference if we know we will be recycling very soon (such when doing level reload)
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[Id];
		if ((bool)PhysicalSpace && PhysicalSpace->GetRefCount() == 0u)
		{
			const FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
			check(PagePool.GetNumMappedPages() == 0u);
			check(PagePool.GetNumLockedPages() == 0u);

			PhysicalSpace->ReleaseResource();
			delete PhysicalSpace;
			PhysicalSpaces[Id] = nullptr;
		}
	}
}

void FVirtualTextureSystem::LockTile(const FVirtualTextureLocalTile& Tile)
{
	check(!bUpdating);

	if (TileLocks.Lock(Tile))
	{
		checkSlow(!TilesToLock.Contains(Tile));
		TilesToLock.Add(Tile);
	}
}

static void UnlockTileInternal(const FVirtualTextureProducerHandle& ProducerHandle, const FVirtualTextureProducer* Producer, const FVirtualTextureLocalTile& Tile, uint32 Frame)
{
	for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
		FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
		const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
		if (pAddress != ~0u)
		{
			PagePool.Unlock(Frame, pAddress);
		}
	}
}

void FVirtualTextureSystem::UnlockTile(const FVirtualTextureLocalTile& Tile, const FVirtualTextureProducer* Producer)
{
	check(!bUpdating);

	if (TileLocks.Unlock(Tile))
	{
		// Tile is no longer locked
		const int32 NumTilesRemoved = TilesToLock.Remove(Tile);
		check(NumTilesRemoved <= 1);
		// If tile was still in the 'TilesToLock' list, that means it was never actually locked, so we don't need to do the unlock here
		if (NumTilesRemoved == 0)
		{
			UnlockTileInternal(Tile.GetProducerHandle(), Producer, Tile, Frame);
		}
	}
}

void FVirtualTextureSystem::ForceUnlockAllTiles(const FVirtualTextureProducerHandle& ProducerHandle, const FVirtualTextureProducer* Producer)
{
	check(!bUpdating);

	TArray<FVirtualTextureLocalTile> TilesToUnlock;
	TileLocks.ForceUnlockAll(ProducerHandle, TilesToUnlock);

	for (const FVirtualTextureLocalTile& Tile : TilesToUnlock)
	{
		const int32 NumTilesRemoved = TilesToLock.Remove(Tile);
		check(NumTilesRemoved <= 1);
		if (NumTilesRemoved == 0)
		{
			UnlockTileInternal(ProducerHandle, Producer, Tile, Frame);
		}
	}
}

static float ComputeMipLevel(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize)
{
	const uint32 TextureWidth = AllocatedVT->GetWidthInPixels();
	const uint32 TextureHeight = AllocatedVT->GetHeightInPixels();
	const FVector2D dfdx(TextureWidth / InScreenSpaceSize.X, 0.0f);
	const FVector2D dfdy(0.0f, TextureHeight / InScreenSpaceSize.Y);
	const float ppx = FVector2D::DotProduct(dfdx, dfdx);
	const float ppy = FVector2D::DotProduct(dfdy, dfdy);
	return 0.5f * FMath::Log2(FMath::Max(ppx, ppy));
}

void FVirtualTextureSystem::RequestTiles(const FVector2D& InScreenSpaceSize, int32 InMipLevel)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);

	for (const auto& Pair : AllocatedVTs)
	{
		RequestTilesInternal(Pair.Value, InScreenSpaceSize, InMipLevel);
	}
}

void FVirtualTextureSystem::RequestTiles(const FMaterialRenderProxy* InMaterialRenderProxy, const FVector2D& InScreenSpaceSize, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);

	for (IAllocatedVirtualTexture* AllocatedVT : InMaterialRenderProxy->UniformExpressionCache[InFeatureLevel].AllocatedVTs)
	{
		if (AllocatedVT != nullptr)
		{
			RequestTilesInternal(AllocatedVT, InScreenSpaceSize, INDEX_NONE);
		}
	}
}

void FVirtualTextureSystem::RequestTilesInternal(const IAllocatedVirtualTexture* InAllocatedVT, const FVector2D& InScreenSpaceSize, int32 InMipLevel)
{
	if (InMipLevel < 0)
	{
		const uint32 vMaxLevel = InAllocatedVT->GetMaxLevel();
		const float vLevel = ComputeMipLevel(InAllocatedVT, InScreenSpaceSize);
		const int32 vMipLevelDown = FMath::Clamp((int32)FMath::FloorToInt(vLevel), 0, (int32)vMaxLevel);

		RequestTilesInternal(InAllocatedVT, vMipLevelDown);
		if (vMipLevelDown + 1u <= vMaxLevel)
		{
			// Need to fetch 2 levels to support trilinear filtering
			RequestTilesInternal(InAllocatedVT, vMipLevelDown + 1u);
		}
	}
	else
	{
		RequestTilesInternal(InAllocatedVT, InMipLevel);
	}
}

void FVirtualTextureSystem::RequestTiles(IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel)
{
	UE::TScopeLock Lock(Mutex);
	if (InMipLevel >= 0)
	{
		RequestTilesForRegionInternal(AllocatedVT, InScreenSpaceSize, InViewportPosition, InViewportSize, InUV0, InUV1, InMipLevel);
	}
	else
	{
		const uint32 vMaxLevel = AllocatedVT->GetMaxLevel();
		const float vLevel = ComputeMipLevel(AllocatedVT, InScreenSpaceSize); // TODO: ComputeMipLevel() is incorrect if not using the whole UV range
		const int32 vMipLevelDown = FMath::Clamp((int32)FMath::FloorToInt(vLevel), 0, (int32)vMaxLevel);

		RequestTilesForRegionInternal(AllocatedVT, InScreenSpaceSize, InViewportPosition, InViewportSize, InUV0, InUV1, vMipLevelDown);
		if (vMipLevelDown + 1u <= vMaxLevel)
		{
			// Need to fetch 2 levels to support trilinear filtering
			RequestTilesForRegionInternal(AllocatedVT, InScreenSpaceSize, InViewportPosition, InViewportSize, InUV0, InUV1, vMipLevelDown + 1u);
		}
	}
}

void FVirtualTextureSystem::RequestTilesForRegion(IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel)
{
	// RequestTilesForRegion() used to require that the viewport position was negated which felt wrong. Correct this when calling the new implementation.
	RequestTiles(AllocatedVT, InScreenSpaceSize, -InViewportPosition, InViewportSize, InUV0, InUV1, InMipLevel);
}

void FVirtualTextureSystem::LoadPendingTiles(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel)
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);

	TArray<uint32> PackedTiles;
	if (RequestedPackedTiles.Num() > 0)
	{
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		FVirtualTextureUpdateSettings Settings;
		FConcurrentLinearBulkObjectAllocator Allocator;

		FUniquePageList* UniquePageList = Allocator.Create<FUniquePageList>();
		UniquePageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			UniquePageList->Add(Tile, 0xffff);
		}

		FUniqueRequestList* RequestList = Allocator.Create<FUniqueRequestList>(Allocator);
		RequestList->Initialize();
		GatherRequests(RequestList, UniquePageList, Frame, Allocator, Settings);
		// No need to sort requests, since we're submitting all of them here (no throttling)
		AllocateResources(GraphBuilder);
		SubmitRequests(GraphBuilder.RHICmdList, FeatureLevel, Allocator, Settings, RequestList, false);
		FinalizeRequests(GraphBuilder);
	}
}

void FVirtualTextureSystem::RequestTilesInternal(const IAllocatedVirtualTexture* AllocatedVT, int32 InMipLevel)
{
	const int32 MipWidthInTiles = FMath::Max<int32>(AllocatedVT->GetWidthInTiles() >> InMipLevel, 1);
	const int32 MipHeightInTiles = FMath::Max<int32>(AllocatedVT->GetHeightInTiles() >> InMipLevel, 1);
	const uint32 vBaseTileX = AllocatedVT->GetVirtualPageX() >> InMipLevel;
	const uint32 vBaseTileY = AllocatedVT->GetVirtualPageY() >> InMipLevel;

	for (int32 TilePositionY = 0; TilePositionY < MipHeightInTiles; TilePositionY++)
	{
		const uint32 vGlobalTileY = vBaseTileY + TilePositionY;
		for (int32 TilePositionX = 0; TilePositionX < MipWidthInTiles; TilePositionX++)
		{
			const uint32 vGlobalTileX = vBaseTileX + TilePositionX;
			const uint32 EncodedTile = EncodePage(AllocatedVT->GetSpaceID(), InMipLevel, vGlobalTileX, vGlobalTileY);
			RequestedPackedTiles.Add(EncodedTile);
		}
	}
}

static int32 WrapTilePosition(int32 Position, int32 Size)
{
	const int32 Result = Position % Size;
	return (Result >= 0) ? Result : Result + Size;
}

void FVirtualTextureSystem::RequestTilesForRegionInternal(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel)
{
	// Screen size must be a least a pixel
	FVector2D ScreenSize = FVector2D::Max(InScreenSpaceSize, FVector2D::One());

	// TopLeft vs BottomRight - In viewport space
	FVector2D TextureTopLeftViewportSpace = InViewportPosition;
	FVector2D TextureBottomRightViewportSpace = InViewportPosition + ScreenSize;

	// TopLeft vs BottomRight - Clamped to viewport
	FVector2D TextureTopLeftViewportSpaceClamped = FVector2D::Clamp(TextureTopLeftViewportSpace, FVector2D::Zero(), InViewportSize);
	FVector2D TextureBottomRightViewportSpaceClamped = FVector2D::Clamp(TextureBottomRightViewportSpace, FVector2D::Zero(), InViewportSize);

	// Range of initial screen size for TopLeft & BottomRight
	// For example, if 10% of the image is outside each side of the viewport, LerpTopLeft & LerpBottomRight would be (0.1, 0.1) & (0.9, 0.9), respectively
	FVector2D LerpTopLeft = FVector2D::Clamp((TextureTopLeftViewportSpaceClamped - TextureTopLeftViewportSpace) / ScreenSize, FVector2D::Zero(), FVector2D::One());
	FVector2D LerpBottomRight = FVector2D::Clamp((TextureBottomRightViewportSpaceClamped - TextureTopLeftViewportSpace) / ScreenSize, FVector2D::Zero(), FVector2D::One());
	
	const int32 WidthInBlocks = AllocatedVT->GetWidthInBlocks();
	const int32 HeightInBlocks = AllocatedVT->GetHeightInBlocks();

	// Map coordinates to UV space
	const float PositionU0 = FMath::Lerp(InUV0.X, InUV1.X, LerpTopLeft.X) / WidthInBlocks;
	const float PositionV0 = FMath::Lerp(InUV0.Y, InUV1.Y, LerpTopLeft.Y) / HeightInBlocks;
	const float PositionU1 = FMath::Lerp(InUV0.X, InUV1.X, LerpBottomRight.X) / WidthInBlocks;
	const float PositionV1 = FMath::Lerp(InUV0.Y, InUV1.Y, LerpBottomRight.Y) / HeightInBlocks;

	// Map UVs to tile coordinates
	const int32 MipWidthInTiles = FMath::Max<int32>(AllocatedVT->GetWidthInTiles() >> InMipLevel, 1);
	const int32 MipHeightInTiles = FMath::Max<int32>(AllocatedVT->GetHeightInTiles() >> InMipLevel, 1);
	const int32 TilePositionX0 = FMath::FloorToInt(FMath::Min(PositionU0, PositionU1) * MipWidthInTiles);
	const int32 TilePositionY0 = FMath::FloorToInt(FMath::Min(PositionV0, PositionV1) * MipHeightInTiles);
	const int32 TilePositionX1 = FMath::CeilToInt(FMath::Max(PositionU0, PositionU1) * MipWidthInTiles);
	const int32 TilePositionY1 = FMath::CeilToInt(FMath::Max(PositionV0, PositionV1) * MipHeightInTiles);

	// RequestedPackedTiles stores packed tiles with vPosition shifted relative to current mip level
	const uint32 vBaseTileX = AllocatedVT->GetVirtualPageX() >> InMipLevel;
	const uint32 vBaseTileY = AllocatedVT->GetVirtualPageY() >> InMipLevel;

	for (int32 TilePositionY = TilePositionY0; TilePositionY < TilePositionY1; TilePositionY++)
	{
		const uint32 vGlobalTileY = vBaseTileY + WrapTilePosition(TilePositionY, MipHeightInTiles);
		for (int32 TilePositionX = TilePositionX0; TilePositionX < TilePositionX1; TilePositionX++)
		{
			const uint32 vGlobalTileX = vBaseTileX + WrapTilePosition(TilePositionX, MipWidthInTiles);
			const uint32 EncodedTile = EncodePage(AllocatedVT->GetSpaceID(), InMipLevel, vGlobalTileX, vGlobalTileY);
			RequestedPackedTiles.Add(EncodedTile);
		}
	}
}

void FVirtualTextureSystem::FeedbackAnalysisTask(const FFeedbackAnalysisParameters& Parameters)
{
	FUniquePageList* RESTRICT RequestedPageList = Parameters.UniquePageList;
	const uint32* RESTRICT Buffer = Parameters.FeedbackBuffer;
	const uint32 BufferSize = Parameters.FeedbackSize;

	// Combine simple runs of identical requests
	uint32 LastPixel = 0xffffffff;
	uint32 LastCount = 0;

	for (uint32 Index = 0; Index < BufferSize; Index++)
	{
		const uint32 Pixel = Buffer[Index];
		if (Pixel == LastPixel)
		{
			LastCount++;
			continue;
		}

		if (LastPixel != 0xffffffff)
		{
			RequestedPageList->Add(LastPixel, LastCount);
		}

		LastPixel = Pixel;
		LastCount = 1;
	}

	if (LastPixel != 0xffffffff)
	{
		RequestedPageList->Add(LastPixel, LastCount);
	}
}

void FVirtualTextureSystem::GatherRequests(FUniqueRequestList* MergedRequestList, const FUniquePageList* UniquePageList, uint32 FrameRequested, FConcurrentLinearBulkObjectAllocator& Allocator, FVirtualTextureUpdateSettings const& Settings)
{
	const uint32 MaxNumGatherTasks = FMath::Clamp((uint32)Settings.NumGatherTasks, 1u, MaxNumTasks);
	const uint32 PageUpdateFlushCount = FMath::Min<uint32>(Settings.MaxGatherPagesBeforeFlush, FPageUpdateBuffer::PageCapacity);

	FGatherRequestsParameters GatherRequestsParameters[MaxNumTasks];
	uint32 NumGatherTasks = 0u;
	{
		const uint32 MinNumPagesPerTask = 64u;
		const uint32 NumPagesPerTask = FMath::Max(FMath::DivideAndRoundUp(UniquePageList->GetNum(), MaxNumGatherTasks), MinNumPagesPerTask);
		const uint32 NumPages = UniquePageList->GetNum();
		uint32 StartPageIndex = 0u;
		while (StartPageIndex < NumPages)
		{
			const uint32 NumPagesForTask = FMath::Min(NumPagesPerTask, NumPages - StartPageIndex);
			if (NumPagesForTask > 0u)
			{
				const uint32 TaskIndex = NumGatherTasks++;
				FGatherRequestsParameters& Params = GatherRequestsParameters[TaskIndex];
				Params.System = this;
				Params.FrameRequested = FrameRequested;
				Params.bForceContinuousUpdate = Settings.bForceContinuousUpdate;
				Params.UniquePageList = UniquePageList;
				Params.PageUpdateFlushCount = PageUpdateFlushCount;
				Params.PageUpdateBuffers = Allocator.CreateArray<FPageUpdateBuffer>(PhysicalSpaces.Num());
				if (TaskIndex == 0u)
				{
					Params.RequestList = MergedRequestList;
				}
				else
				{
					Params.RequestList = Allocator.Create<FUniqueRequestList>(Allocator);
				}
				Params.PageStartIndex = StartPageIndex;
				Params.NumPages = NumPagesForTask;
				StartPageIndex += NumPagesForTask;
			}
		}
	}

	// Kick all of the tasks
	FGraphEventArray Tasks;
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
		Tasks.Reserve(NumGatherTasks - 1u);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			Tasks.Add(TGraphTask<FGatherRequestsTask>::CreateTask().ConstructAndDispatchWhenReady(GatherRequestsParameters[TaskIndex]));
		}
	}

	if (NumGatherTasks > 0u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Gather);

		// first task can run on this thread
		GatherRequestsTask(GatherRequestsParameters[0]);

		// Wait for them to complete
		if (Tasks.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_WaitTasks);

			FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks);
		}
	}

	// Merge request lists for all tasks
	if (NumGatherTasks > 1u)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergeRequests);
		for (uint32 TaskIndex = 1u; TaskIndex < NumGatherTasks; ++TaskIndex)
		{
			MergedRequestList->MergeRequests(GatherRequestsParameters[TaskIndex].RequestList, Allocator);
		}
	}
}

void FVirtualTextureSystem::AddPageUpdate(FPageUpdateBuffer* Buffers, uint32 FlushCount, uint32 PhysicalSpaceID, uint16 pAddress)
{
	FPageUpdateBuffer& RESTRICT Buffer = Buffers[PhysicalSpaceID];
	if (pAddress == Buffer.PrevPhysicalAddress)
	{
		return;
	}
	Buffer.PrevPhysicalAddress = pAddress;

	bool bLocked = false;
	if (Buffer.NumPages >= FlushCount)
	{
		// Once we've passed a certain threshold of pending pages to update, try to take the lock then apply the updates
		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
		FCriticalSection& RESTRICT Lock = PagePool.GetLock();

		if (Buffer.NumPages >= FPageUpdateBuffer::PageCapacity)
		{
			// If we've reached capacity, need to take the lock no matter what, may potentially block here
			Lock.Lock();
			bLocked = true;
		}
		else
		{
			// try to take the lock, but avoid stalling
			bLocked = Lock.TryLock();
		}

		if(bLocked)
		{
			const uint32 CurrentFrame = Frame;
			PagePool.UpdateUsage(CurrentFrame, pAddress); // Update current request now, if we manage to get the lock
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(CurrentFrame, Buffer.PhysicalAddresses[i]);
			}
			Lock.Unlock();
			Buffer.NumPageUpdates += (Buffer.NumPages + 1u);
			Buffer.NumPages = 0u;
		}
	}

	// Only need to buffer if we didn't lock (otherwise this has already been updated)
	if (!bLocked)
	{
		check(Buffer.NumPages < FPageUpdateBuffer::PageCapacity);
		Buffer.PhysicalAddresses[Buffer.NumPages++] = pAddress;
	}
}

void FVirtualTextureSystem::GatherRequestsTask(const FGatherRequestsParameters& Parameters)
{
	const FUniquePageList* RESTRICT UniquePageList = Parameters.UniquePageList;
	FPageUpdateBuffer* RESTRICT PageUpdateBuffers = Parameters.PageUpdateBuffers;
	FUniqueRequestList* RESTRICT RequestList = Parameters.RequestList;
	const uint32 PageUpdateFlushCount = Parameters.PageUpdateFlushCount;
	const uint32 PageEndIndex = Parameters.PageStartIndex + Parameters.NumPages;
	const bool bForceContinuousUpdate = Parameters.bForceContinuousUpdate;

	uint32 NumRequestsPages = 0u;
	uint32 NumResidentPages = 0u;
	uint32 NumNonResidentPages = 0u;
	uint32 NumPrefetchPages = 0u;

	for (uint32 i = Parameters.PageStartIndex; i < PageEndIndex; ++i)
	{
		const uint32 PageEncoded = UniquePageList->GetPage(i);
		const uint32 PageCount = UniquePageList->GetCount(i);

		// Decode page
		const uint32 ID = (PageEncoded >> 28);
		const FVirtualTextureSpace* RESTRICT Space = GetSpace(ID);
		if (Space == nullptr)
		{
			continue;
		}

		const uint32 vLevelPlusOne = ((PageEncoded >> 24) & 0x0f);
		const uint32 vLevel = FMath::Max(vLevelPlusOne, 1u) - 1;
		
		// vPageX/Y passed from shader are relative to the given vLevel, we shift them up so be relative to level0
		// TODO - should we just do this in the shader?
		const uint32 vPageX = (PageEncoded & 0xfff) << vLevel;
		const uint32 vPageY = ((PageEncoded >> 12) & 0xfff) << vLevel;
		
		const uint32 vAddress = FMath::MortonCode2(vPageX) | (FMath::MortonCode2(vPageY) << 1);
	
		const FAdaptiveVirtualTexture* RESTRICT AdaptiveVT = AdaptiveVTs[ID];
		if (AdaptiveVT != nullptr && vLevelPlusOne <= 1)
		{
			uint32 AdaptiveAllocationRequest = AdaptiveVT->GetPackedAllocationRequest(vAddress, vLevelPlusOne, Frame);
			if (AdaptiveAllocationRequest != 0)
			{
				RequestList->AddAdaptiveAllocationRequest(AdaptiveAllocationRequest);
			}
		}

		uint32 PageTableLayersToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0 };
		uint32 NumPageTableLayersToLoad = 0u;
		{
			const FTexturePage VirtualPage(vLevel, vAddress);
			const uint16 VirtualPageHash = MurmurFinalize32(VirtualPage.Packed);
			for (uint32 PageTableLayerIndex = 0u; PageTableLayerIndex < Space->GetNumPageTableLayers(); ++PageTableLayerIndex)
			{
				const FTexturePageMap& RESTRICT PageMap = Space->GetPageMapForPageTableLayer(PageTableLayerIndex);

				++NumRequestsPages;
				const FPhysicalSpaceIDAndAddress PhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(VirtualPage, VirtualPageHash);
				if (PhysicalSpaceIDAndAddress.Packed != ~0u)
				{
#if DO_GUARD_SLOW
					const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID);
					checkSlow(PhysicalSpaceIDAndAddress.pAddress < PhysicalSpace->GetNumTiles());
#endif // DO_GUARD_SLOW

					// Page is already resident, just need to update LRU free list
					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpaceIDAndAddress.PhysicalSpaceID, PhysicalSpaceIDAndAddress.pAddress);

					// If continuous update flag is set then add this to pages which can be potentially updated if we have spare upload bandwidth
					//todo[vt]: Would be better to test continuous update flag *per producer*, but this would require extra indirection so need to profile first
					if (bForceContinuousUpdate || GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetDescription().bContinuousUpdate)
					{
						FTexturePagePool& RESTRICT PagePool = GetPhysicalSpace(PhysicalSpaceIDAndAddress.PhysicalSpaceID)->GetPagePool();
						const FVirtualTextureLocalTile LocalTile = PagePool.GetLocalTileFromPhysicalAddress(PhysicalSpaceIDAndAddress.pAddress);
						RequestList->AddContinuousUpdateRequest(LocalTile);
					}

					++PageUpdateBuffers[PhysicalSpaceIDAndAddress.PhysicalSpaceID].WorkingSetSize;
					++NumResidentPages;
				}
				else
				{
					// Page not resident, store for later processing
					PageTableLayersToLoad[NumPageTableLayersToLoad++] = PageTableLayerIndex;
				}
			}
		}

		if (NumPageTableLayersToLoad == 0u)
		{
			// All pages are resident and properly mapped, we're done
			// This is the fast path, as most frames should generally have the majority of tiles already mapped
			continue;
		}

		// Need to resolve AllocatedVT in order to determine which pages to load
		const FAllocatedVirtualTexture* RESTRICT AllocatedVT = Space->GetAllocator().Find(vAddress);
		if (!AllocatedVT)
		{
			if (CVarVTVerbose.GetValueOnAnyThread())
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("Space %i, vAddr %i@%i is not allocated to any AllocatedVT but was still requested."), ID, vAddress, vLevel);
			}
			continue;
		}

		if (AllocatedVT->GetFrameAllocated() > Parameters.FrameRequested)
		{
			// If the VT was allocated after the frame that generated this feedback, it's no longer valid
			continue;
		}

		const uint32 MaxLevel = AllocatedVT->GetMaxLevel();

		check(AllocatedVT->GetNumPageTableLayers() == Space->GetNumPageTableLayers());
		if (vLevel > MaxLevel)
		{
			// Requested level is outside the given allocated VT
			// This can happen for requests made by expanding mips, since we don't know the current allocated VT in that context
			check(NumPageTableLayersToLoad == Space->GetNumPageTableLayers()); // no pages from this request should have been resident
			check(NumRequestsPages >= Space->GetNumPageTableLayers()); // don't want to track these requests, since it turns out they're not valid
			NumRequestsPages -= Space->GetNumPageTableLayers();
			continue;
		}

		// Build producer local layer masks from physical layers that we need to load
		uint8 ProducerGroupMaskToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };
		uint8 ProducerTextureLayerMaskToLoad[VIRTUALTEXTURE_SPACE_MAXLAYERS] = { 0u };

		const uint32 NumUniqueProducers = AllocatedVT->GetNumUniqueProducers();

		for (uint32 LoadPageTableLayerIndex = 0u; LoadPageTableLayerIndex < NumPageTableLayersToLoad; ++LoadPageTableLayerIndex)
		{
			const uint32 PageTableLayerIndex = PageTableLayersToLoad[LoadPageTableLayerIndex];
			const uint32 ProducerIndex = AllocatedVT->GetProducerIndexForPageTableLayer(PageTableLayerIndex);
			check(ProducerIndex < NumUniqueProducers);
			
			const uint32 ProducerTextureLayerMask = AllocatedVT->GetProducerTextureLayerMaskForPageTableLayer(PageTableLayerIndex);
			ProducerTextureLayerMaskToLoad[ProducerIndex] |= ProducerTextureLayerMask;
			
			const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(PageTableLayerIndex);
			ProducerGroupMaskToLoad[ProducerIndex] |= 1 << ProducerPhysicalGroupIndex;

			const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = AllocatedVT->GetPhysicalSpaceForPageTableLayer(PageTableLayerIndex);
			if (PhysicalSpace)
			{
				++PageUpdateBuffers[PhysicalSpace->GetID()].WorkingSetSize;
			}
		}

		const uint32 vDimensions = Space->GetDimensions();
		const uint32 AllocatedPageX = AllocatedVT->GetVirtualPageX();
		const uint32 AllocatedPageY = AllocatedVT->GetVirtualPageY();

		check(vAddress >= AllocatedVT->GetVirtualAddress());
		check(vPageX >= AllocatedPageX);
		check(vPageY >= AllocatedPageY);

		for (uint32 ProducerIndex = 0u; ProducerIndex < NumUniqueProducers; ++ProducerIndex)
		{
			uint8 GroupMaskToLoad = ProducerGroupMaskToLoad[ProducerIndex];
			if (GroupMaskToLoad == 0u)
			{
				continue;
			}

			const FVirtualTextureProducerHandle ProducerHandle = AllocatedVT->GetUniqueProducerHandle(ProducerIndex);
			const FVirtualTextureProducer* RESTRICT Producer = Producers.FindProducer(ProducerHandle);
			if (!Producer)
			{
				continue;
			}

			const uint32 ProducerMipBias = AllocatedVT->GetUniqueProducerMipBias(ProducerIndex);
			const uint32 ProducerMaxLevel = Producer->GetMaxLevel();

			// here vLevel is clamped against ProducerMipBias, as ProducerMipBias represents the most detailed level of this producer, relative to the allocated VT
			// used to rescale vAddress to the correct tile within the given mip level
			uint32 Mapping_vLevel = FMath::Max(vLevel, ProducerMipBias);

			// Local_vLevel is the level within the producer that we want to allocate/map
			// here we subtract ProducerMipBias, which effectively matches more detailed mips of lower resolution producers with less detailed mips of higher resolution producers
			uint32 Local_vLevel = Mapping_vLevel - ProducerMipBias;

			const uint32 Local_vPageX = (vPageX - AllocatedPageX) >> Mapping_vLevel;
			const uint32 Local_vPageY = (vPageY - AllocatedPageY) >> Mapping_vLevel;
			uint32 Local_vAddress = FMath::MortonCode2(Local_vPageX) | (FMath::MortonCode2(Local_vPageY) << 1);

			const uint32 LocalMipBias = Producer->GetVirtualTexture()->GetLocalMipBias(Local_vLevel, Local_vAddress);
			if (LocalMipBias > 0u)
			{
				Local_vLevel += LocalMipBias;
				Mapping_vLevel += LocalMipBias;
				Local_vAddress >>= (LocalMipBias * vDimensions);
			}

			uint8 ProducerPhysicalGroupMaskToPrefetchForLevel[16] = { 0u };
			uint32 MaxPrefetchLocal_vLevel = Local_vLevel;

			// Iterate local layers that we found unmapped
			for (uint32 ProducerGroupIndex = 0u; ProducerGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerGroupIndex)
			{
				if ((GroupMaskToLoad & (1u << ProducerGroupIndex)) == 0u)
				{
					continue;
				}

				const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerGroupIndex);
				const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

				// Find the highest resolution tile that's currently loaded
				const uint32 Allocated_pAddress = PagePool.FindNearestPageAddress(ProducerHandle, ProducerGroupIndex, Local_vAddress, Local_vLevel, ProducerMaxLevel);

				bool bRequestedPageWasResident = false;
				uint32 AllocatedLocal_vLevel = MaxLevel;
				if (Allocated_pAddress != ~0u)
				{
					AllocatedLocal_vLevel = PagePool.GetLocalLevelForAddress(Allocated_pAddress);
					check(AllocatedLocal_vLevel >= Local_vLevel);

					const uint32 AllocatedMapping_vLevel = AllocatedLocal_vLevel + ProducerMipBias;
					uint32 Allocated_vLevel = FMath::Min(AllocatedMapping_vLevel, MaxLevel);
					if (AllocatedLocal_vLevel == Local_vLevel)
					{
						// page at the requested level was already resident, no longer need to load
						bRequestedPageWasResident = true;
						// We can map the already resident page at the original requested vLevel
						// This may be different from Allocated_vLevel when various biases are involved
						// Without this, we'll never see anything mapped to the original requested level
						Allocated_vLevel = vLevel;
						GroupMaskToLoad &= ~(1u << ProducerGroupIndex);
						++NumResidentPages;
					}

					ensure(Allocated_vLevel <= MaxLevel);
					const uint32 Allocated_vAddress = vAddress & (0xffffffff << (Allocated_vLevel * vDimensions));

					AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpace->GetID(), Allocated_pAddress);

					const FPhysicalSpaceIDAndAddress PhysicalSpaceIDAndAddress(PhysicalSpace->GetID(), Allocated_pAddress);
					uint32 NumMappedPages = 0u;
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 PageTableLayerIndex = PageTableLayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(PageTableLayerIndex) == ProducerGroupIndex &&
							AllocatedVT->GetProducerIndexForPageTableLayer(PageTableLayerIndex) == ProducerIndex)
						{
							// if we found a lower resolution tile than was requested, it may have already been mapped, check for that first
							const FTexturePageMap& PageMap = Space->GetPageMapForPageTableLayer(PageTableLayerIndex);
							const FPhysicalSpaceIDAndAddress PrevPhysicalSpaceIDAndAddress = PageMap.FindPagePhysicalSpaceIDAndAddress(Allocated_vLevel, Allocated_vAddress);

							// either it wasn't mapped, or it's mapped to the current physical address...
							// otherwise that means that the same local tile is mapped to two separate physical addresses, which is an error
							ensure(PrevPhysicalSpaceIDAndAddress.Packed == ~0u || PrevPhysicalSpaceIDAndAddress.Packed == 0u || PrevPhysicalSpaceIDAndAddress == PhysicalSpaceIDAndAddress);

							if (PrevPhysicalSpaceIDAndAddress.Packed == ~0u)
							{
								// map the page now if it wasn't already mapped
								RequestList->AddDirectMappingRequest(Space->GetID(), PhysicalSpace->GetID(), PageTableLayerIndex, MaxLevel, Allocated_vAddress, Allocated_vLevel, AllocatedMapping_vLevel, Allocated_pAddress);
							}
							++NumMappedPages;
						}
					}
					check(NumMappedPages > 0u);
				}

				if (!bRequestedPageWasResident)
				{
					// page not resident...see if we want to prefetch a page with resolution incrementally larger than what's currently resident
					// this means we'll ultimately load more data, but these lower resolution pages should load much faster than the requested high resolution page
					// this should make popping less noticeable
					uint32 PrefetchLocal_vLevel = AllocatedLocal_vLevel - FMath::Min(2u, AllocatedLocal_vLevel);
					PrefetchLocal_vLevel = FMath::Min<uint32>(PrefetchLocal_vLevel, MaxLevel - ProducerMipBias);
					if (PrefetchLocal_vLevel > Local_vLevel)
					{
						ProducerPhysicalGroupMaskToPrefetchForLevel[PrefetchLocal_vLevel] |= (1u << ProducerGroupIndex);
						MaxPrefetchLocal_vLevel = FMath::Max(MaxPrefetchLocal_vLevel, PrefetchLocal_vLevel);
						++NumPrefetchPages;
					}
					++NumNonResidentPages;
				}
			}

			// Check to see if we have any levels to prefetch
			for (uint32 PrefetchLocal_vLevel = Local_vLevel + 1u; PrefetchLocal_vLevel <= MaxPrefetchLocal_vLevel; ++PrefetchLocal_vLevel)
			{
				uint32 ProducerPhysicalGroupMaskToPrefetch = ProducerPhysicalGroupMaskToPrefetchForLevel[PrefetchLocal_vLevel];
				if (ProducerPhysicalGroupMaskToPrefetch != 0u)
				{
					const uint32 PrefetchLocal_vAddress = Local_vAddress >> ((PrefetchLocal_vLevel - Local_vLevel) * vDimensions);

					// If we want to prefetch any layers for a given level, need to ensure that we request all the layers that aren't currently loaded
					// This is required since the VT producer interface needs to be able to write data for all layers if desired, so we need to make sure that all layers are allocated
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						if ((ProducerPhysicalGroupMaskToPrefetch & (1u << ProducerPhysicalGroupIndex)) == 0u)
						{
							const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, PrefetchLocal_vAddress, PrefetchLocal_vLevel);
							if (pAddress == ~0u)
							{
								ProducerPhysicalGroupMaskToPrefetch |= (1u << ProducerPhysicalGroupIndex);
								++NumPrefetchPages;
							}
							else
							{
								// Need to mark the page as recently used, otherwise it may be evicted later this frame
								AddPageUpdate(PageUpdateBuffers, PageUpdateFlushCount, PhysicalSpace->GetID(), pAddress);
							}
						}
					}

					const bool bStreamingRequest = Producer->GetVirtualTexture()->IsPageStreamed(PrefetchLocal_vLevel, PrefetchLocal_vAddress);
					const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, PrefetchLocal_vAddress, PrefetchLocal_vLevel), ProducerPhysicalGroupMaskToPrefetch, PageCount, bStreamingRequest);
					if (LoadRequestIndex != 0xffff)
					{
						const uint32 PrefetchMapping_vLevel = PrefetchLocal_vLevel + ProducerMipBias;
						ensure(PrefetchMapping_vLevel <= MaxLevel);
						const uint32 Prefetch_vAddress = vAddress & (0xffffffff << (PrefetchMapping_vLevel * vDimensions));
						for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
						{
							const uint32 LayerIndex = PageTableLayersToLoad[LoadLayerIndex];
							if (AllocatedVT->GetProducerIndexForPageTableLayer(LayerIndex) == ProducerIndex)
							{
								const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(LayerIndex);
								if (ProducerPhysicalGroupMaskToPrefetch & (1u << ProducerPhysicalGroupIndex))
								{
									RequestList->AddMappingRequest(LoadRequestIndex, ProducerPhysicalGroupIndex, ID, LayerIndex, MaxLevel, Prefetch_vAddress, PrefetchMapping_vLevel, PrefetchMapping_vLevel);
								}
							}
						}
					}
				}
			}

			if (GroupMaskToLoad != 0u)
			{
				const bool bStreamingRequest = Producer->GetVirtualTexture()->IsPageStreamed(Local_vLevel, Local_vAddress);
				const uint16 LoadRequestIndex = RequestList->AddLoadRequest(FVirtualTextureLocalTile(ProducerHandle, Local_vAddress, Local_vLevel), GroupMaskToLoad, PageCount, bStreamingRequest);
				if (LoadRequestIndex != 0xffff)
				{
					for (uint32 LoadLayerIndex = 0u; LoadLayerIndex < NumPageTableLayersToLoad; ++LoadLayerIndex)
					{
						const uint32 LayerIndex = PageTableLayersToLoad[LoadLayerIndex];
						if (AllocatedVT->GetProducerIndexForPageTableLayer(LayerIndex) == ProducerIndex)
						{
							const uint32 ProducerPhysicalGroupIndex = AllocatedVT->GetProducerPhysicalGroupIndexForPageTableLayer(LayerIndex);
							if (GroupMaskToLoad & (1u << ProducerPhysicalGroupIndex))
							{
								RequestList->AddMappingRequest(LoadRequestIndex, ProducerPhysicalGroupIndex, ID, LayerIndex, MaxLevel, vAddress, vLevel, Mapping_vLevel);
							}
						}
					}
				}
			}
		}
	}

	for (uint32 PhysicalSpaceID = 0u; PhysicalSpaceID < (uint32)PhysicalSpaces.Num(); ++PhysicalSpaceID)
	{
		if (PhysicalSpaces[PhysicalSpaceID] == nullptr)
		{
			continue;
		}

		FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = GetPhysicalSpace(PhysicalSpaceID);
		FPageUpdateBuffer& RESTRICT Buffer = PageUpdateBuffers[PhysicalSpaceID];

		if (Buffer.NumPages > 0u)
		{
			Buffer.NumPageUpdates += Buffer.NumPages;
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();

			FScopeLock Lock(&PagePool.GetLock());
			for (uint32 i = 0u; i < Buffer.NumPages; ++i)
			{
				PagePool.UpdateUsage(Frame, Buffer.PhysicalAddresses[i]);
			}
		}
		
		INC_DWORD_STAT_BY(STAT_NumPageUpdate, Buffer.NumPageUpdates);
	}

	INC_DWORD_STAT_BY(STAT_NumPageVisible, NumRequestsPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleResident, NumResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPageVisibleNotResident, NumNonResidentPages);
	INC_DWORD_STAT_BY(STAT_NumPagePrefetch, NumPrefetchPages);
}

void FVirtualTextureSystem::GetContinuousUpdatesToProduce(FUniqueRequestList const* RequestList, int32 MaxTilesToProduce, int32 MaxContinuousUpdates)
{
	const int32 NumContinuousUpdateRequests = (int32)RequestList->GetNumContinuousUpdateRequests();
	
	// Negative maximum continous updates allows for uncapped requests
	if (MaxContinuousUpdates < 0)
	{
		for (int32 i = 0; i < NumContinuousUpdateRequests; ++i)
		{
			ContinuousUpdateTilesToProduce.Add(RequestList->GetContinuousUpdateRequest(i));
		}
	}
	else
	{
		const int32 MaxContinousUpdates = FMath::Min(MaxContinuousUpdates, NumContinuousUpdateRequests);

		int32 NumContinuousUpdates = 0;
		while (NumContinuousUpdates < MaxContinousUpdates && ContinuousUpdateTilesToProduce.Num() < MaxTilesToProduce)
		{
			// Note it's possible that we add a duplicate value to the TSet here, and so MappedTilesToProduce doesn't grow.
			// But ending up with fewer continuous updates then the maximum is OK.
			int32 RandomIndex = FMath::Rand() % NumContinuousUpdateRequests;
			ContinuousUpdateTilesToProduce.Add(RequestList->GetContinuousUpdateRequest(RandomIndex));
			NumContinuousUpdates++;
		}
	}
}

void FVirtualTextureSystem::UpdateResidencyTracking() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::UpdateResidencyTracking);
	SCOPE_CYCLE_COUNTER(STAT_ResidencyTracking);

	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace)
		{
			PhysicalSpace->UpdateResidencyTracking(Frame);
		}
	}
}

void FVirtualTextureSystem::GrowPhysicalPools() const
{
	if (!VirtualTexturePool::GetPoolAutoGrow())
	{
		return;
	}

	TArray<FVirtualTextureSpacePoolConfig> Configs;
	for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
		if (PhysicalSpace && PhysicalSpace->GetLastFrameOversubscribed() == Frame)
		{
			FVirtualTextureSpacePoolConfig& Config = Configs.AddDefaulted_GetRef();
			const FVTPhysicalSpaceDescription& Desc = PhysicalSpace->GetDescription();
			Config.Formats.Append(Desc.Format, Desc.NumLayers);
			Config.MaxTileSize = Config.MinTileSize = Desc.TileSize;

			// Increase pool by 1 tile or 4MB, whichever is greater.
			const int32 TileCount = PhysicalSpace->GetSizeInTiles();
			const int32 TileSizeInBytes = PhysicalSpace->GetTileSizeInBytes();
			const int32 CurrentSizeInBytes = TileCount * TileCount * TileSizeInBytes;
			const int32 NextSizeInBytes = (TileCount + 1) * (TileCount + 1) * TileSizeInBytes;
			const int32 MinIncreaseInBytes = 4 * 1024 * 1024;
			const int32 ClampedNextSizeInBytes = FMath::Max(NextSizeInBytes, CurrentSizeInBytes + MinIncreaseInBytes);
			Config.SizeInMegabyte = FMath::DivideAndRoundUp(ClampedNextSizeInBytes, 1024 * 1024);
		}
	}

	if (Configs.Num())
	{
		VirtualTexturePool::AddOrModifyTransientPoolConfigs_RenderThread(Configs);
	}
}

void FVirtualTextureSystem::SubmitRequestsFromLocalTileList(FRHICommandList& RHICmdList, TArray<FVirtualTextureLocalTile>& OutDeferredTiles, const TSet<FVirtualTextureLocalTile>& LocalTileList, EVTProducePageFlags Flags, ERHIFeatureLevel::Type FeatureLevel, uint32 MaxRequestsToProduce)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	uint32 NumPagesProduced = 0;
	for (const FVirtualTextureLocalTile& Tile : LocalTileList)
	{
		const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
		FVirtualTextureProducer const* Producer = Producers.FindProducer(ProducerHandle);
		if (Producer == nullptr)
		{
			// If we didn't process the tile last frame and deferred processing and if the producer got removed in the meantime we end up here.
			// Just throw away the request.
			continue;
		}

		// Fill targets for each layer
		// Each producer can have multiple physical layers
		// If the phys layer is mapped then we get the textures it owns and map them into the producer local slots and set the flags
		uint32 LayerMask = 0;
		FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
		for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer->GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
		{
			FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
			FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
			const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
			if (pAddress != ~0u)
			{
				int32 PhysicalLocalTextureIndex = 0;
				for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer->GetNumTextureLayers(); ++ProducerLayerIndex)
				{
					if (Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
					{
						ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
						ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
						ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
						LayerMask |= 1 << ProducerLayerIndex;
						PhysicalLocalTextureIndex++;
					}
				}
			}
		}

		if (LayerMask == 0)
		{
			// If we don't have anything mapped then we can ignore (since we only want to refresh existing mapped data)
			continue;
		}

		if (MaxRequestsToProduce > 0 && NumPagesProduced >= MaxRequestsToProduce)
		{
			// Keep the request for the next frame?
			OutDeferredTiles.Add(Tile);
			continue;
		}

		FVTRequestPageResult RequestPageResult = Producer->GetVirtualTexture()->RequestPageData(
			RHICmdList, ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress, EVTRequestPagePriority::High);

		if (RequestPageResult.Status != EVTRequestPageStatus::Available)
		{
			// Keep the request for the next frame?
			OutDeferredTiles.Add(Tile);
			continue;
		}

		IVirtualTextureFinalizer* VTFinalizer = Producer->GetVirtualTexture()->ProducePageData(
			RHICmdList, FeatureLevel,
			Flags,
			ProducerHandle, LayerMask, Tile.Local_vLevel, Tile.Local_vAddress,
			RequestPageResult.Handle,
			ProduceTarget);

		if (VTFinalizer != nullptr)
		{
			// Add the finalizer here but note that we don't call Finalize until SubmitRequests()
			Finalizers.AddUnique(VTFinalizer);
		}

		NumPagesProduced++;
	}
}

void FVirtualTextureSystem::SubmitPreMappedRequests(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint32 MaxMappedTilesToProduce)
{
	check(TransientCollectedPages.Num() == 0);

	{
		INC_DWORD_STAT_BY(STAT_NumMappedPageUpdate, MappedTilesToProduce.Num());
		SubmitRequestsFromLocalTileList(RHICmdList, TransientCollectedPages, MappedTilesToProduce, EVTProducePageFlags::None, FeatureLevel, MaxMappedTilesToProduce);
		MappedTilesToProduce.Reset();
		MappedTilesToProduce.Append(TransientCollectedPages);
		TransientCollectedPages.Reset();
	}

	{
		INC_DWORD_STAT_BY(STAT_NumContinuousPageUpdate, ContinuousUpdateTilesToProduce.Num());
		SubmitRequestsFromLocalTileList(RHICmdList, TransientCollectedPages, ContinuousUpdateTilesToProduce, EVTProducePageFlags::ContinuousUpdate, FeatureLevel, 0);
		ContinuousUpdateTilesToProduce.Reset();
		TransientCollectedPages.Reset();
	}
}

void FVirtualTextureSystem::SubmitThrottledRequests(FRHICommandList& RHICmdList, FVirtualTextureUpdater* Updater, bool bContinousUpdates)
{
	const FVirtualTextureUpdateSettings& Settings   = Updater->Settings;
	const ERHIFeatureLevel::Type  FeatureLevel      = Updater->FeatureLevel;
	FConcurrentLinearBulkObjectAllocator& Allocator = Updater->Allocator;
	FUniqueRequestList* MergedRequestList           = Updater->MergedRequestList;

	if (MergedRequestList->GetNumAdaptiveAllocationRequests() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_QueueAdaptiveRequests);
		FAdaptiveVirtualTexture::QueuePackedAllocationRequests(this, &MergedRequestList->GetAdaptiveAllocationRequest(0), MergedRequestList->GetNumAdaptiveAllocationRequests(), Frame);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Sort);

		// Limit the number of uploads (account for MappedTilesToProduce this frame but only for RVTs since SVT pages will take a while to load and likely won't be produced this frame).
		// Use a separate budget for SVTs since we may want more of them to go through so that I/O can be initiated as early as possible
		const int32 MaxNonStreamingLoadRequests = FMath::Max(Settings.MaxRVTPageUploads - MappedTilesToProduce.Num(), 1);
		const int32 MaxStreamingLoadRequests = Settings.MaxSVTPageUploads;
		check(MaxStreamingLoadRequests >= 0);
		// 0 is a special value that enables the old behavior where all pages are limited by a single budget
		const bool bUseCombinedLimit = !MaxStreamingLoadRequests;
		const uint32 OldNumLoadRequests = MergedRequestList->GetNumLoadRequests();

		MergedRequestList->SortRequests(Producers, Allocator, MaxNonStreamingLoadRequests, MaxStreamingLoadRequests, bUseCombinedLimit);

		if (MergedRequestList->GetNumLoadRequests() < OldNumLoadRequests)
		{
			// Dropping requests is normal but track to log here if we want to tune settings.
			if (CVarVTVerbose.GetValueOnRenderThread())
			{
				UE_LOG(LogConsoleResponse, Display, TEXT("VT dropped %d load requests."), MergedRequestList->GetNumLoadRequests() - OldNumLoadRequests);
			}
		}
	}

	if (bContinousUpdates)
	{
		// After sorting and clamping the load requests, if we still have unused upload bandwidth then use it to add some continuous updates.
		// Not taking SVT requests into account since they take a while to load and likely won't be produced this frame
		const int32 MaxTilesToProduce = FMath::Max(Settings.MaxRVTPageUploads - MappedTilesToProduce.Num() - (int32)MergedRequestList->GetNumNonStreamingLoadRequests(), 0);
		const int32 MaxContinuousUpdates = Settings.MaxContinuousUpdates;

		GetContinuousUpdatesToProduce(MergedRequestList, MaxTilesToProduce, MaxContinuousUpdates);
	}

	// Track total number of requests made this frame.
	Updater->NumProcessedLoadRequests += MergedRequestList->GetNumLoadRequests();

	// Submit the requests to produce pages that are already mapped
	SubmitPreMappedRequests(RHICmdList, FeatureLevel, Settings.MaxRVTPageUploads);

	// Submit the merged requests
	SubmitRequests(RHICmdList, FeatureLevel, Allocator, Settings, MergedRequestList, true);
}

void FVirtualTextureSystem::SubmitRequests(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FConcurrentLinearBulkObjectAllocator& Allocator, FVirtualTextureUpdateSettings const& Settings, FUniqueRequestList* RequestList, bool bAsync)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::SubmitRequests);
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);

	// Allocate space to hold the physical address we allocate for each page load (1 page per layer per request)
	uint32* RequestPhysicalAddress = Allocator.MallocAndMemsetArray<uint32>(RequestList->GetNumLoadRequests() * VIRTUALTEXTURE_SPACE_MAXLAYERS, 0xFF);

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Submit);

		struct FProducePageDataPrepareTask
		{
			IVirtualTexture* VirtualTexture;
			EVTProducePageFlags Flags;
			FVirtualTextureProducerHandle ProducerHandle;
			uint8 LayerMask;
			uint8 vLevel;
			uint32 vAddress;
			uint64 RequestHandle;
			FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
		};

		TArray<FProducePageDataPrepareTask> PrepareTasks;
		PrepareTasks.Reserve(RequestList->GetNumLoadRequests());

#if WITH_EDITOR
		// Always force loading of locked root pages in editor to be sure that any build steps give best quality.
		const bool bSyncProduceLockedTiles = true;
#else
		// if bSyncProduceLockedTiles is false then we may render with a VT before the root pages are mapped.
		// When that happens the shader switches to using the single color fallback value instead of sampling the VT.
		// When the root page is finally mapped we will return to normal high quality VT sampling.
		// todo[VT]: Make root pages always resident so that we never need to load sync the root pages.
		const bool bSyncProduceLockedTiles = CVarVTSyncProduceLockedTiles.GetValueOnRenderThread() != 0;
#endif

		bool bWaitForProducers = false;

		const uint32 MaxPagesProduced = Settings.MaxPagesProduced;
		const uint32 PageFreeThreshold = VirtualTextureScalability::GetPageFreeThreshold();
		uint32 NumStacksProduced = 0u;
		uint32 NumPagesProduced = 0u;
		uint32 NumPageAllocateFails = 0u;

		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumLoadRequests(); ++RequestIndex)
		{
			const bool bLockTile = RequestList->IsLocked(RequestIndex);
			const bool bForceProduceTile = !bAsync || (bLockTile && bSyncProduceLockedTiles);
			const FVirtualTextureLocalTile TileToLoad = RequestList->GetLoadRequest(RequestIndex);
			const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
			const FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);

			const uint32 ProducerPhysicalGroupMask = RequestList->GetGroupMask(RequestIndex);
			uint32 ProducerTextureLayerMask = 0;
			for (uint32 ProducerLayerIndex = 0; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
			{
				if (ProducerPhysicalGroupMask & (1 << Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex)))
				{
					ProducerTextureLayerMask |= (1 << ProducerLayerIndex);
				}
			}

			const EVTRequestPagePriority Priority = bLockTile ? EVTRequestPagePriority::High : EVTRequestPagePriority::Normal;
			FVTRequestPageResult RequestPageResult = Producer.GetVirtualTexture()->RequestPageData(RHICmdList, ProducerHandle, ProducerTextureLayerMask, TileToLoad.Local_vLevel, TileToLoad.Local_vAddress, Priority);
			if (RequestPageResult.Status == EVTRequestPageStatus::Pending && bForceProduceTile)
			{
				// If we're forcing production of this tile, we're OK producing data now (and possibly waiting) as long as data is pending
				RequestPageResult.Status = EVTRequestPageStatus::Available;
				bWaitForProducers = true;
			}

			if (RequestPageResult.Status == EVTRequestPageStatus::Available && !bForceProduceTile && NumPagesProduced >= MaxPagesProduced)
			{
				// Don't produce non-locked pages yet, if we're over our limit
				RequestPageResult.Status = EVTRequestPageStatus::Pending;
			}

			bool bTileLoaded = false;
			bool bTileInvalid = false;
			if (RequestPageResult.Status == EVTRequestPageStatus::Invalid)
			{
				//checkf(!bLockTile, TEXT("Tried to lock an invalid VT tile"));

				bTileInvalid = true;
				if (CVarVTVerbose.GetValueOnAnyThread())
				{
					UE_LOG(LogConsoleResponse, Display, TEXT("vAddr %i@%i is not a valid request for AllocatedVT but is still requested."), TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
				}
			}
			else if (RequestPageResult.Status == EVTRequestPageStatus::Available)
			{
				FVTProduceTargetLayer ProduceTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				uint32 Allocate_pAddress[VIRTUALTEXTURE_SPACE_MAXLAYERS];
				FMemory::Memset(Allocate_pAddress, 0xff);

				// try to allocate a page for each layer we need to load
				bool bProduceTargetValid = true;
				for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
				{
					// If mask isn't set, we must already have a physical tile allocated for this layer, don't need to allocate another one
					if (ProducerPhysicalGroupMask & (1u << ProducerPhysicalGroupIndex))
					{
						FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
						FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
						if (PagePool.AnyFreeAvailable(Frame, bLockTile ? 0 : PageFreeThreshold))
						{
							const uint32 pAddress = PagePool.Alloc(this, Frame, ProducerHandle, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel, bLockTile);
							check(pAddress != ~0u);

							int32 PhysicalLocalTextureIndex = 0;
							for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
							{
								if (Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
								{
									ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
									
									PhysicalLocalTextureIndex++;

									Allocate_pAddress[ProducerPhysicalGroupIndex] = pAddress;
								}
							}

							++NumPagesProduced;
						}
						else
						{
							bProduceTargetValid = false;
							NumPageAllocateFails++;
							break;
						}
					}
				}

				if (bProduceTargetValid)
				{
					// Successfully allocated required pages, now we can make the request
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						if (ProducerPhysicalGroupMask & (1u << ProducerPhysicalGroupIndex))
						{
							// Associate the addresses we allocated with this request, so they can be mapped if required
							const uint32 pAddress = Allocate_pAddress[ProducerPhysicalGroupIndex];
							check(pAddress != ~0u);
							RequestPhysicalAddress[RequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + ProducerPhysicalGroupIndex] = pAddress;
						}
						else
						{
							// Fill in pAddress for layers that are already resident
							const FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							const FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
							checkf(pAddress != ~0u,
								TEXT("%s missing tile: LayerMask: %X, Layer %d, vAddress %06X, vLevel %d"),
								*Producer.GetName().ToString(), ProducerPhysicalGroupMask, ProducerPhysicalGroupIndex, TileToLoad.Local_vAddress, TileToLoad.Local_vLevel);
							
							int32 PhysicalLocalTextureIndex = 0;
							for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer.GetNumTextureLayers(); ++ProducerLayerIndex)
							{
								if (Producer.GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex) == ProducerPhysicalGroupIndex)
								{
									ProduceTarget[ProducerLayerIndex].TextureRHI = PhysicalSpace->GetPhysicalTexture(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].PooledRenderTarget = PhysicalSpace->GetPhysicalTexturePooledRenderTarget(PhysicalLocalTextureIndex);
									ProduceTarget[ProducerLayerIndex].pPageLocation = PhysicalSpace->GetPhysicalLocation(pAddress);
									PhysicalLocalTextureIndex++;
								}
							}
						}
					}

					{
						FProducePageDataPrepareTask& Task = PrepareTasks.AddDefaulted_GetRef();
						Task.VirtualTexture = Producer.GetVirtualTexture();
						Task.Flags = EVTProducePageFlags::None;
						Task.ProducerHandle = ProducerHandle;
						Task.LayerMask = ProducerTextureLayerMask;
						Task.vLevel = TileToLoad.Local_vLevel;
						Task.vAddress = TileToLoad.Local_vAddress;
						Task.RequestHandle = RequestPageResult.Handle;
						FMemory::Memcpy(Task.ProduceTarget, ProduceTarget, sizeof(ProduceTarget));
					}

					bTileLoaded = true;
					++NumStacksProduced;
				}
				else
				{
					// Failed to allocate required physical pages for the tile, free any pages we did manage to allocate
					for (uint32 ProducerPhysicalGroupIndex = 0u; ProducerPhysicalGroupIndex < Producer.GetNumPhysicalGroups(); ++ProducerPhysicalGroupIndex)
					{
						const uint32 pAddress = Allocate_pAddress[ProducerPhysicalGroupIndex];
						if (pAddress != ~0u)
						{
							FVirtualTexturePhysicalSpace* RESTRICT PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(ProducerPhysicalGroupIndex);
							FTexturePagePool& RESTRICT PagePool = PhysicalSpace->GetPagePool();
							PagePool.Free(this, pAddress);
						}
					}
				}
			}

			if (bLockTile && !bTileLoaded && !bTileInvalid)
			{
				// Want to lock this tile, but didn't manage to load it this frame, add it back to the list to try the lock again next frame
				TilesToLockForNextFrame.Add(TileToLoad);
			}
		}

		if (PrepareTasks.Num())
		{
			static bool bWaitForTasks = true;
			if (bWaitForProducers && bWaitForTasks)
			{
				// Wait for all producers here instead of inside each individual call to ProducePageData()
				FGraphEventArray ProducePageTasks;
				ProducePageTasks.Reserve(PrepareTasks.Num());

				for (FProducePageDataPrepareTask& Task : PrepareTasks)
				{
					if (Task.RequestHandle != 0)
					{
						Task.VirtualTexture->GatherProducePageDataTasks(Task.RequestHandle, ProducePageTasks);
					}
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::ProcessRequests_Wait);
					QUICK_SCOPE_CYCLE_COUNTER(ProcessRequests_Wait);
					FTaskGraphInterface::Get().WaitUntilTasksComplete(ProducePageTasks);
				}
			}

			for (FProducePageDataPrepareTask& Task : PrepareTasks)
			{
				IVirtualTextureFinalizer* VTFinalizer = Task.VirtualTexture->ProducePageData(RHICmdList, FeatureLevel,
					Task.Flags,
					Task.ProducerHandle, Task.LayerMask, Task.vLevel, Task.vAddress,
					Task.RequestHandle,
					Task.ProduceTarget);

				if (VTFinalizer)
				{
					Finalizers.AddUnique(VTFinalizer); // we expect the number of unique finalizers to be very limited. if this changes, we might have to do something better then gathering them every update
				}
			}
		}

		INC_DWORD_STAT_BY(STAT_NumStacksRequested, RequestList->GetNumLoadRequests());
		INC_DWORD_STAT_BY(STAT_NumStacksProduced, NumStacksProduced);
		INC_DWORD_STAT_BY(STAT_NumPageAllocateFails, NumPageAllocateFails);
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Map);

		// Update page mappings that were directly requested
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumDirectMappingRequests(); ++RequestIndex)
		{
			const FDirectMappingRequest MappingRequest = RequestList->GetDirectMappingRequest(RequestIndex);
			FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
			FVirtualTexturePhysicalSpace* PhysicalSpace = GetPhysicalSpace(MappingRequest.PhysicalSpaceID);

			PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.PageTableLayerIndex, MappingRequest.MaxLevel, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, MappingRequest.pAddress);
		}

		// Update page mappings for any requested page that completed allocation this frame
		for (uint32 RequestIndex = 0u; RequestIndex < RequestList->GetNumMappingRequests(); ++RequestIndex)
		{
			const FMappingRequest MappingRequest = RequestList->GetMappingRequest(RequestIndex);
			const uint32 pAddress = RequestPhysicalAddress[MappingRequest.LoadRequestIndex * VIRTUALTEXTURE_SPACE_MAXLAYERS + MappingRequest.ProducerPhysicalGroupIndex];
			if (pAddress != ~0u)
			{
				const FVirtualTextureLocalTile& TileToLoad = RequestList->GetLoadRequest(MappingRequest.LoadRequestIndex);
				const FVirtualTextureProducerHandle ProducerHandle = TileToLoad.GetProducerHandle();
				FVirtualTextureProducer& Producer = Producers.GetProducer(ProducerHandle);
				FVirtualTexturePhysicalSpace* PhysicalSpace = Producer.GetPhysicalSpaceForPhysicalGroup(MappingRequest.ProducerPhysicalGroupIndex);
				FVirtualTextureSpace* Space = GetSpace(MappingRequest.SpaceID);
				check(RequestList->GetGroupMask(MappingRequest.LoadRequestIndex) & (1u << MappingRequest.ProducerPhysicalGroupIndex));

				PhysicalSpace->GetPagePool().MapPage(Space, PhysicalSpace, MappingRequest.PageTableLayerIndex, MappingRequest.MaxLevel, MappingRequest.vLevel, MappingRequest.vAddress, MappingRequest.Local_vLevel, pAddress);
			}
		}
	}

	// Map any resident tiles to newly allocated VTs
	if(AllocatedVTsToMap.Num() > 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MapNew);

		uint32 Index = 0u;
		while (Index < (uint32)AllocatedVTsToMap.Num())
		{
			const IAllocatedVirtualTexture* AllocatedVT = AllocatedVTsToMap[Index];
			if (AllocatedVT->TryMapLockedTiles(this))
			{
				AllocatedVTsToMap.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
			else
			{
				Index++;
			}
		}

		AllocatedVTsToMap.Shrink();
	}
}

void FVirtualTextureSystem::FinalizeRequests(FRDGBuilder& GraphBuilder)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::FinalizeRequests);

	// Finalize requests
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_Finalize);
		RDG_EVENT_SCOPE(GraphBuilder, "VirtualTextureFinalizeRequests");
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::Finalize);
		
		for (IVirtualTextureFinalizer* VTFinalizer : Finalizers)
		{
			VTFinalizer->Finalize(GraphBuilder);
		}
		Finalizers.Reset();
	}

	// Update page tables
	{
		SCOPE_CYCLE_COUNTER(STAT_PageTableUpdates);
		RDG_EVENT_SCOPE(GraphBuilder, "VirtualTexturePageTableUpdates");
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::ApplyUpdates);

		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (Spaces[ID])
			{
				Spaces[ID]->ApplyUpdates(this, GraphBuilder);
			}
		}
	}

	for (FVirtualTexturePhysicalSpace* PhysicalSpace : PhysicalSpaces)
	{
		if (PhysicalSpace != nullptr)
		{
			PhysicalSpace->FinalizeTextures(GraphBuilder);
		}
	}

	Frame++;
}

void FVirtualTextureSystem::AllocateResources(FRDGBuilder& GraphBuilder)
{
	LLM_SCOPE(ELLMTag::VirtualTextureSystem);
	RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTextureAllocate);
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, VirtualTextureSystem_Update);

	for (uint32 ID = 0; ID < MaxSpaces; ID++)
	{
		if (Spaces[ID])
		{
			Spaces[ID]->AllocateTextures(GraphBuilder);
		}
	}
}

void FVirtualTextureSystem::GatherFeedbackRequests(FConcurrentLinearBulkObjectAllocator& Allocator, const FVirtualTextureUpdateSettings& Settings, const FVirtualTextureFeedback::FMapResult& FeedbackResult, FUniqueRequestList* MergedRequestList)
{
	FUniquePageList* MergedUniquePageList = Allocator.Create<FUniquePageList>();
	MergedUniquePageList->Initialize();

	if (Settings.bEnableFeedback)
	{
		// Create tasks to read the feedback data
		// Give each task a section of the feedback buffer to analyze
		FFeedbackAnalysisParameters FeedbackAnalysisParameters[MaxNumTasks];

		const uint32 MaxNumFeedbackTasks = FMath::Clamp((uint32)Settings.NumFeedbackTasks, 1u, MaxNumTasks);
		const uint32 FeedbackSizePerTask = FMath::DivideAndRoundUp(FeedbackResult.Size, MaxNumFeedbackTasks);

		uint32 NumFeedbackTasks = 0;
		uint32 CurrentOffset = 0;
		while (CurrentOffset < FeedbackResult.Size)
		{
			const uint32 TaskIndex = NumFeedbackTasks++;
			FFeedbackAnalysisParameters& Params = FeedbackAnalysisParameters[TaskIndex];
			Params.System = this;
			if (TaskIndex == 0u)
			{
				Params.UniquePageList = MergedUniquePageList;
			}
			else
			{
				Params.UniquePageList = Allocator.Create<FUniquePageList>();
			}
			Params.FeedbackBuffer = FeedbackResult.Data + CurrentOffset;

			const uint32 Size = FMath::Min(FeedbackSizePerTask, FeedbackResult.Size - CurrentOffset);
			Params.FeedbackSize = Size;
			CurrentOffset += Size;
		}

		// Kick the tasks
		const int32 LocalFeedbackTaskCount = Settings.bParallelFeedbackTasks ? 1 : NumFeedbackTasks;
		const int32 WorkerFeedbackTaskCount = NumFeedbackTasks - LocalFeedbackTaskCount;

		FGraphEventArray Tasks;
		if (WorkerFeedbackTaskCount > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_SubmitTasks);
			Tasks.Reserve(WorkerFeedbackTaskCount);
			for (uint32 TaskIndex = LocalFeedbackTaskCount; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				Tasks.Add(TGraphTask<FFeedbackAnalysisTask>::CreateTask().ConstructAndDispatchWhenReady(FeedbackAnalysisParameters[TaskIndex]));
			}
		}

		if (NumFeedbackTasks > 0u)
		{
			SCOPE_CYCLE_COUNTER(STAT_FeedbackAnalysis);

			for (int32 TaskIndex = 0; TaskIndex < LocalFeedbackTaskCount; ++TaskIndex)
			{
				FFeedbackAnalysisTask::DoTask(FeedbackAnalysisParameters[TaskIndex]);
			}
			if (WorkerFeedbackTaskCount > 0)
			{
				SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_WaitTasks);

				FTaskGraphInterface::Get().WaitUntilTasksComplete(Tasks, ENamedThreads::GetRenderThread_Local());
			}
		}

		if (NumFeedbackTasks > 1u)
		{
			SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_MergePages);
			for (uint32 TaskIndex = 1u; TaskIndex < NumFeedbackTasks; ++TaskIndex)
			{
				MergedUniquePageList->MergePages(FeedbackAnalysisParameters[TaskIndex].UniquePageList);
			}
		}
	}

#if WITH_EDITOR
	// If we're are recording page requests, then copy off pages to the recording buffer.
	if (PageRequestRecordHandle != ~0ull)
	{
		RecordPageRequests(MergedUniquePageList, PageRequestRecordBuffer);
	}
#endif

	// Add any page requests from recording playback.
	if (PageRequestPlaybackBuffer.Num() > 0)
	{
		if (Settings.bEnablePlayback)
		{
			// todo: We can split this into concurrent tasks. 
			FAddRequestedTilesParameters Parameters;
			Parameters.System = this;
			Parameters.LevelBias = FMath::FloorToInt(CVarVTPlaybackMipBias.GetValueOnRenderThread() + GetGlobalMipBias() + 0.5f);
			Parameters.RequestBuffer = PageRequestPlaybackBuffer.GetData();
			Parameters.NumRequests = PageRequestPlaybackBuffer.Num();
			Parameters.UniquePageList = Allocator.Create<FUniquePageList>();

			FAddRequestedTilesTask::DoTask(Parameters);
			MergedUniquePageList->MergePages(Parameters.UniquePageList);
		}

		PageRequestPlaybackBuffer.Reset(0);
	}

	// Pages from feedback buffer were generated several frames ago, so they may no longer be valid for newly allocated VTs
	static uint32 PendingFrameDelay = 3u;
	if (Frame >= PendingFrameDelay)
	{
		GatherRequests(MergedRequestList, MergedUniquePageList, Frame - PendingFrameDelay, Allocator, Settings);
	}
}

void FVirtualTextureSystem::GatherLockedTileRequests(FUniqueRequestList* MergedRequestList)
{
	for (const FVirtualTextureLocalTile& Tile : TilesToLock)
	{
		const FVirtualTextureProducerHandle ProducerHandle = Tile.GetProducerHandle();
		const FVirtualTextureProducer* Producer = Producers.FindProducer(ProducerHandle);
		checkSlow(TileLocks.IsLocked(Tile));
		if (Producer)
		{
			uint8 ProducerLayerMaskToLoad = 0u;
			for (uint32 ProducerLayerIndex = 0u; ProducerLayerIndex < Producer->GetNumTextureLayers(); ++ProducerLayerIndex)
			{
				uint32 GroupIndex = Producer->GetPhysicalGroupIndexForTextureLayer(ProducerLayerIndex);
				FVirtualTexturePhysicalSpace* PhysicalSpace = Producer->GetPhysicalSpaceForPhysicalGroup(GroupIndex);
				FTexturePagePool& PagePool = PhysicalSpace->GetPagePool();
				const uint32 pAddress = PagePool.FindPageAddress(ProducerHandle, GroupIndex, Tile.Local_vAddress, Tile.Local_vLevel);
				if (pAddress == ~0u)
				{
					ProducerLayerMaskToLoad |= (1u << ProducerLayerIndex);
				}
				else
				{
					PagePool.Lock(pAddress);
				}
			}

			if (ProducerLayerMaskToLoad != 0u)
			{
				const bool bStreamingRequest = Producer->GetVirtualTexture()->IsPageStreamed(Tile.Local_vLevel, Tile.Local_vAddress);
				const uint16 LoadRequestIndex = MergedRequestList->LockLoadRequest(FVirtualTextureLocalTile(Tile.GetProducerHandle(), Tile.Local_vAddress, Tile.Local_vLevel), ProducerLayerMaskToLoad, bStreamingRequest);
				if (LoadRequestIndex == 0xffff)
				{
					// Overflowed the request list...try to lock the tile again next frame
					TilesToLockForNextFrame.Add(Tile);
				}
			}
		}
	}
	TilesToLock.Reset();
}

void FVirtualTextureSystem::GatherPackedTileRequests(FConcurrentLinearBulkObjectAllocator& Allocator, const FVirtualTextureUpdateSettings& Settings, FUniqueRequestList* MergedRequestList)
{
	TArray<uint32> PackedTiles;
	if (RequestedPackedTiles.Num() > 0)
	{
		PackedTiles = MoveTemp(RequestedPackedTiles);
		RequestedPackedTiles.Reset();
	}

	if (PackedTiles.Num() > 0)
	{
		// Collect explicitly requested tiles
		// These tiles are generated on the current frame, so they are collected/processed in a separate list
		FUniquePageList* RequestedPageList = Allocator.Create<FUniquePageList>();
		RequestedPageList->Initialize();
		for (uint32 Tile : PackedTiles)
		{
			RequestedPageList->Add(Tile, 0xffff);
		}
		GatherRequests(MergedRequestList, RequestedPageList, Frame, Allocator, Settings);
	}
}

void FVirtualTextureSystem::BeginUpdate(FRDGBuilder& GraphBuilder, FVirtualTextureUpdater* Updater)
{
	// Mark updating to true now that we are potentially launching async tasks.
	bUpdating = true;

	if (Updater->Settings.bEnableFeedback)
	{
		SCOPE_CYCLE_COUNTER(STAT_FeedbackMap);
		Updater->FeedbackMapResult = GVirtualTextureFeedback.Map(GraphBuilder.RHICmdList);
	}

	Updater->AsyncTask = GraphBuilder.AddCommandListSetupTask([this, Updater, &Allocator = Updater->Allocator, Settings = Updater->Settings, FeedbackResult = Updater->FeedbackMapResult](FRHICommandList& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::GatherAndSubmitRequests);

		const bool bContinousUpdates = false;

		Updater->MergedRequestList = Allocator.Create<FUniqueRequestList>(Allocator);
		Updater->MergedRequestList->Initialize();

		GatherFeedbackRequests(Allocator, Settings, FeedbackResult, Updater->MergedRequestList);
		GatherLockedTileRequests(Updater->MergedRequestList);
		GatherPackedTileRequests(Allocator, Settings, Updater->MergedRequestList);
		SubmitThrottledRequests(RHICmdList, Updater, bContinousUpdates);

		// Reset the request list for the gather in EndUpdate.
		Updater->MergedRequestList->Reset(bContinousUpdates);

	}, UE::Tasks::ETaskPriority::High, Updater->bAsyncTaskAllowed);
}

void FVirtualTextureSystem::CallPendingCallbacks()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::CallPendingCallbacks);
	SCOPE_CYCLE_COUNTER(STAT_VirtualTextureSystem_Update);
	UE::TScopeLock Lock(Mutex);
	Producers.CallPendingCallbacks();
}

TUniquePtr<FVirtualTextureUpdater> FVirtualTextureSystem::BeginUpdate(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FScene* Scene, const FVirtualTextureUpdateSettings& Settings)
{
	check(IsInRenderingThread());
	check(!bUpdating);

	checkf(Producers.HasPendingCallbacks() == false, TEXT("FVirtualTextureSystem::CallPendingCallbacks(), called in UpdateAllPrimitiveSceneInfos(), must run before FVirtualTextureSystem::BeginUpdate()"));

	AllocateResources(GraphBuilder);

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, VirtualTextureSystem_Update);
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::BeginUpdate);
	SCOPE_CYCLE_COUNTER(STAT_VirtualTextureSystem_Update);
	RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTexture);

	if (!Settings.bEnablePageRequests)
	{
		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (Spaces[ID])
			{
				Spaces[ID]->FinalizeTextures(GraphBuilder);
			}
		}

		return {};
	}

	// Update Adaptive VTs. This can trigger allocation/destruction of VTs and must happen before the flush below.
	{
		SCOPE_CYCLE_COUNTER(STAT_ProcessRequests_UpdateAdaptiveAllocations);
		for (uint32 ID = 0; ID < MaxSpaces; ID++)
		{
			if (AdaptiveVTs[ID])
			{
				AdaptiveVTs[ID]->UpdateAllocations(this, GraphBuilder.RHICmdList, Frame);
			}
		}
	}

	if (bFlushCaches)
	{
		SCOPE_CYCLE_COUNTER(STAT_FlushCache);
		INC_DWORD_STAT_BY(STAT_NumFlushCache, 1);

		for (int32 i = 0; i < PhysicalSpaces.Num(); ++i)
		{
			FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[i];
			if (PhysicalSpace)
			{
				if (CVarVTProduceLockedTilesOnFlush.GetValueOnRenderThread())
				{
					// Collect locked pages to be produced again
					PhysicalSpace->GetPagePool().GetAllLockedPages(this, MappedTilesToProduce);
				}
				// Flush unlocked pages
				PhysicalSpace->GetPagePool().EvictAllPages(this);
			}
		}

		bFlushCaches = false;
	}

	DestroyPendingVirtualTextures(false);

	// Early out when no allocated VTs
	if (AllocatedVTs.Num() == 0)
	{
		MappedTilesToProduce.Reset();
		return {};
	}

	// Flush any dirty runtime virtual textures for the current scene
	if (Scene != nullptr)
	{
		// Only flush if we know that there is GPU feedback available to refill the visible data this frame
		// This prevents bugs when low frame rate causes feedback buffer to stall so that the physical cache isn't filled immediately which causes visible glitching
		if (GVirtualTextureFeedback.CanMap(GraphBuilder.RHICmdList))
		{
			// Each RVT will call FVirtualTextureSystem::FlushCache()
			Scene->FlushDirtyRuntimeVirtualTextures();
		}
	}

	TUniquePtr<FVirtualTextureUpdater> Updater(new FVirtualTextureUpdater());
	Updater->Settings = Settings;
	Updater->FeatureLevel = FeatureLevel;
	Updater->bAsyncTaskAllowed = Settings.bEnableAsyncTasks && CVarVTAsyncPageRequestTask.GetValueOnRenderThread();

	if (Updater->bAsyncTaskAllowed)
	{
		BeginUpdate(GraphBuilder, Updater.Get());
	}

	return MoveTemp(Updater);
}

void FVirtualTextureSystem::WaitForTasks(FVirtualTextureUpdater* Updater)
{
	if (!Updater)
	{
		return;
	}

	Updater->AsyncTask.Wait();
	bUpdating = false;
}

void FVirtualTextureSystem::EndUpdate(FRDGBuilder& GraphBuilder, TUniquePtr<FVirtualTextureUpdater>&& Updater, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInRenderingThread());

	if (!Updater)
	{
		return;
	}

	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, VirtualTextureSystem_Update);
	TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::EndUpdate);
	SCOPE_CYCLE_COUNTER(STAT_VirtualTextureSystem_Update);
	RDG_GPU_STAT_SCOPE(GraphBuilder, VirtualTexture);

	if (Updater->bAsyncTaskAllowed)
	{
		Updater->AsyncTask.Wait();
	}
	else
	{
		BeginUpdate(GraphBuilder, Updater.Get());
	}
	bUpdating = false;

	if (Updater->FeedbackMapResult.Data)
	{
		GVirtualTextureFeedback.Unmap(GraphBuilder.RHICmdList, Updater->FeedbackMapResult.MapHandle);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FVirtualTextureSystem::GatherAndSubmitRequests);
		const bool bContinousUpdates = true;

		// We only need to gather remaining requests if an async task was used early in the frame.
		if (Updater->bAsyncTaskAllowed)
		{
			GatherLockedTileRequests(Updater->MergedRequestList);
			GatherPackedTileRequests(Updater->Allocator, Updater->Settings, Updater->MergedRequestList);
		}

		SubmitThrottledRequests(GraphBuilder.RHICmdList, Updater.Get(), bContinousUpdates);
	}

	FinalizeRequests(GraphBuilder);

	Producers.NotifyRequestsCompleted();

	UpdateResidencyTracking();
	GrowPhysicalPools();


#if !UE_BUILD_SHIPPING
	UpdateCsvStats();
#endif

	ReleasePendingSpaces();

	TilesToLock = MoveTemp(TilesToLockForNextFrame);
}

void FVirtualTextureSystem::Update(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FScene* Scene, const FVirtualTextureUpdateSettings& InSettings)
{
	CallPendingCallbacks();

	FVirtualTextureUpdateSettings Settings = InSettings;
	Settings.EnableAsyncTasks(false);

	TUniquePtr<FVirtualTextureUpdater> Updater = BeginUpdate(GraphBuilder, FeatureLevel, Scene, Settings);
	EndUpdate(GraphBuilder, MoveTemp(Updater), FeatureLevel);
}

void FVirtualTextureSystem::ReleasePendingResources()
{
	check(!bUpdating);
	UE::TScopeLock Lock(Mutex);
	DestroyPendingVirtualTextures(true);
	ReleasePendingSpaces();
}

float FVirtualTextureSystem::GetGlobalMipBias() const
{
	float MaxResidencyMipMapBias = 0.f;
	for (int32 SpaceIndex = 0; SpaceIndex < PhysicalSpaces.Num(); ++SpaceIndex)
	{
		FVirtualTexturePhysicalSpace const* PhysicalSpace = PhysicalSpaces[SpaceIndex];
		const float ResidencyMipMapBias = PhysicalSpace != nullptr ? PhysicalSpace->GetResidencyMipMapBias() : 0.f;
		MaxResidencyMipMapBias = FMath::Max(MaxResidencyMipMapBias, ResidencyMipMapBias);
	}

	return UTexture2D::GetGlobalMipMapLODBias() + MaxResidencyMipMapBias;
}

bool FVirtualTextureSystem::IsPendingRootPageMap(IAllocatedVirtualTexture* AllocatedVT) const
{
	UE::TScopeLock Lock(Mutex);
	return AllocatedVTsToMap.Find(AllocatedVT) != INDEX_NONE;
}

#if !UE_BUILD_SHIPPING

void FVirtualTextureSystem::GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages)
{
	if (CVarVTResidencyNotify.GetValueOnRenderThread() == 0)
	{
		return;
	}

	for (int32 SpaceIndex = 0u; SpaceIndex < PhysicalSpaces.Num(); ++SpaceIndex)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[SpaceIndex];
		if (PhysicalSpace != nullptr && Frame <= PhysicalSpace->GetLastFrameOversubscribed() + 60u)
		{
			FString const& FormatString = PhysicalSpace->GetFormatString();
			const float MipBias = PhysicalSpace->GetResidencyMipMapBias();

			OutMessages.Add(
				FCoreDelegates::EOnScreenMessageSeverity::Warning,
				FText::Format(LOCTEXT("VTOversubscribed", "VT Pool [{0}] is oversubscribed. Setting MipBias {1}"), FText::FromString(FormatString), FText::AsNumber(MipBias)));
		}
	}
}

void FVirtualTextureSystem::UpdateCsvStats()
{
#if CSV_PROFILER
	if (CVarVTCsvStats.GetValueOnRenderThread() == 0)
	{
		return;
	}

	float MaxResidencyMipMapBias = 0.f;
	for (int32 SpaceIndex = 0u; SpaceIndex < PhysicalSpaces.Num(); ++SpaceIndex)
	{
		const FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[SpaceIndex];
		if (PhysicalSpace != nullptr)
		{
			const float ResidencyMipMapBias = PhysicalSpace->GetResidencyMipMapBias();
			MaxResidencyMipMapBias = FMath::Max(MaxResidencyMipMapBias, ResidencyMipMapBias);

			if (CVarVTCsvStats.GetValueOnRenderThread() == 2)
			{
				PhysicalSpace->UpdateCsvStats();
			}
		}
	}

	CSV_CUSTOM_STAT(VirtualTexturing, ResidencyMipBias, MaxResidencyMipMapBias, ECsvCustomStatOp::Set);
#endif
}

void FVirtualTextureSystem::DrawResidencyHud(UCanvas* InCanvas, APlayerController* InController)
{
	if (CVarVTResidencyShow.GetValueOnGameThread() == 0)
	{
		return;
	}

	int32 NumGraphs = 0;
	for (int32 SpaceIndex = 0; SpaceIndex < PhysicalSpaces.Num(); ++SpaceIndex)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[SpaceIndex];
		if (PhysicalSpace)
		{
			NumGraphs++;
		}
	}

	if (NumGraphs == 0)
	{
		return;
	}

	const FIntPoint GraphSize(250, 125);
	const FIntPoint BorderSize(25, 25);
	const FIntPoint GraphWithBorderSize = GraphSize + BorderSize * 2;

	const FIntPoint CanvasSize = FIntPoint(InCanvas->ClipX, InCanvas->ClipY);
	const int32 NumGraphsInRow = FMath::Max(CanvasSize.X / GraphWithBorderSize.X, 1);
	const int32 CanvasOffsetY = 90;

	int32 GraphIndex = 0;
	for (int32 SpaceIndex = 0; SpaceIndex < PhysicalSpaces.Num(); ++SpaceIndex)
	{
		FVirtualTexturePhysicalSpace* PhysicalSpace = PhysicalSpaces[SpaceIndex];
		if (PhysicalSpace && PhysicalSpace->IsInitialized())
		{
			int32 GraphX = GraphIndex % NumGraphsInRow;
			int32 GraphY = GraphIndex / NumGraphsInRow;

			FBox2D CanvasPosition;
			CanvasPosition.Min.X = GraphX * GraphWithBorderSize.X + BorderSize.X;
			CanvasPosition.Min.Y = GraphY * GraphWithBorderSize.Y + BorderSize.Y + CanvasOffsetY;
			CanvasPosition.Max = CanvasPosition.Min + GraphSize;

			if (CanvasPosition.Min.Y > CanvasSize.Y)
			{
				// Off screen so early out.
				break;
			}

			const bool bDrawKey = GraphIndex == 0;
			PhysicalSpace->DrawResidencyGraph(InCanvas->Canvas, CanvasPosition, bDrawKey);

			GraphIndex++;
		}
	}
}

#endif // !UE_BUILD_SHIPPING

#if WITH_EDITOR

void FVirtualTextureSystem::SetVirtualTextureRequestRecordBuffer(uint64 Handle)
{
	UE::TScopeLock Lock(Mutex);
	check(PageRequestRecordHandle == ~0ull && PageRequestRecordBuffer.Num() == 0);
	
	PageRequestRecordHandle = Handle;
	PageRequestRecordBuffer.Reset();
}

void FVirtualTextureSystem::RecordPageRequests(FUniquePageList const* UniquePageList, TSet<uint64>& OutPages)
{
	const uint32 PageCount = UniquePageList->GetNum();
	OutPages.Reserve(PageCount);

	for (uint32 PageIndex = 0; PageIndex < PageCount; ++PageIndex)
	{
		const uint32 PageId = UniquePageList->GetPage(PageIndex);

		const uint32 SpaceId = (PageId >> 28);
		const FVirtualTextureSpace* RESTRICT Space = GetSpace(SpaceId);
		if (Space == nullptr)
		{
			continue;
		}

		const uint32 vLevelPlusOne = ((PageId >> 24) & 0x0f);
		const uint32 vLevel = FMath::Max(vLevelPlusOne, 1u) - 1;
		const uint32 vPageX = (PageId & 0xfff) << vLevel;
		const uint32 vPageY = ((PageId >> 12) & 0xfff) << vLevel;

		const uint32 vAddress = FMath::MortonCode2(vPageX) | (FMath::MortonCode2(vPageY) << 1);
		const FAllocatedVirtualTexture* RESTRICT AllocatedVT = Space->GetAllocator().Find(vAddress);
		if (AllocatedVT == nullptr)
		{
			continue;
		}

		const uint32 LocalAddress = vAddress - AllocatedVT->GetVirtualAddress();
		const uint32 PersistentHash = AllocatedVT->GetPersistentHash();
		const uint64 ExportPageId = ((uint64)PersistentHash << 32)| ((uint64)vLevelPlusOne << 28) | (uint64)LocalAddress;
		
		OutPages.Add(ExportPageId);
	}
}

uint64 FVirtualTextureSystem::GetVirtualTextureRequestRecordBuffer(TSet<uint64>& OutPageRequests)
{
	UE::TScopeLock Lock(Mutex);

	if (PageRequestRecordHandle == ~0ull)
	{
		return ~0ull;
	}

	uint64 Ret = PageRequestRecordHandle;
	OutPageRequests = MoveTemp(PageRequestRecordBuffer);
	PageRequestRecordBuffer.Reset();
	PageRequestRecordHandle = ~0ull;
	return Ret;
}

#endif // WITH_EDITOR

void FVirtualTextureSystem::RequestRecordedTiles(TArray<uint64>&& InPageRequests)
{
	UE::TScopeLock Lock(Mutex);

	if (PageRequestPlaybackBuffer.Num() == 0)
	{
		PageRequestPlaybackBuffer = MoveTemp(InPageRequests);
	}
	else
	{
		PageRequestPlaybackBuffer.Append(InPageRequests);
	}
}

void FVirtualTextureSystem::AddRequestedTilesTask(const FAddRequestedTilesParameters& Parameters)
{
	FUniquePageList* RESTRICT RequestedPageList = Parameters.UniquePageList;
	const uint64* RESTRICT Buffer = Parameters.RequestBuffer;
	const uint32 BufferSize = Parameters.NumRequests;
	const uint32 LevelBias = Parameters.LevelBias;

	uint32 CurrentPersistentHash = ~0u;
	IAllocatedVirtualTexture* AllocatedVT = nullptr;

	for (uint32 Index = 0; Index < BufferSize; Index++)
	{
		uint64 PageRequest = Buffer[Index];

		// We expect requests to be at least partially sorted. 
		// So a small optimization is to only resolve the PersistentHash to AllocatedVT on change.
		const uint32 PersistentHash = (uint32)(PageRequest >> 32);
		if (CurrentPersistentHash != PersistentHash)
		{
			IAllocatedVirtualTexture** AllocatedVTPtr = PersistentVTMap.Find(PersistentHash);
			AllocatedVT = AllocatedVTPtr != nullptr ? *AllocatedVTPtr : nullptr;
			CurrentPersistentHash = PersistentHash;
		}

		if (AllocatedVT != nullptr)
		{
			const uint32 LevelPlusOne = (uint32)(PageRequest >> 28) & 0xf;
			const uint32 LocalAddress = (uint32)(PageRequest & 0xffffff);

			const uint32 BaseAddress = AllocatedVT->GetVirtualAddress();
			const uint32 Address = BaseAddress + LocalAddress;
			const uint32 BaseLevel = FMath::Max(LevelPlusOne, 1u) - 1;
			const uint32 MaxLevel = AllocatedVT->GetMaxLevel();
			const uint32 Level = FMath::Min(BaseLevel + LevelBias, MaxLevel);
			const uint32 PageX = FMath::ReverseMortonCode2(Address) >> Level;
			const uint32 PageY = FMath::ReverseMortonCode2(Address >> 1) >> Level;
			const uint32 SpaceId = AllocatedVT->GetSpaceID();
			const uint32 PageId = PageX | (PageY << 12) | ((Level+1) << 24) | (SpaceId << 28);

			RequestedPageList->Add(PageId, 0xffff);
		}
	}
}

#undef LOCTEXT_NAMESPACE
