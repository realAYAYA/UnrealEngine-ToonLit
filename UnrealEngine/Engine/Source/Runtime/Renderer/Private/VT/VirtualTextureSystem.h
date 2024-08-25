// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#if !UE_BUILD_SHIPPING
#include "Misc/CoreDelegates.h"
#endif
#include "RHI.h"
#include "RendererInterface.h"
#include "Templates/UniquePtr.h"
#include "VT/VirtualTextureProducer.h"
#include "VT/TexturePageLocks.h"
#include "VirtualTexturing.h"
#include "VT/VirtualTextureFeedback.h"
#include "Tasks/Task.h"
#include "Async/RecursiveMutex.h"

class FAdaptiveVirtualTexture;
class FAllocatedVirtualTexture;
class FScene;
class FUniquePageList;
class FUniqueRequestList;
class FVirtualTexturePhysicalSpace;
class FVirtualTextureProducer;
class FVirtualTextureSpace;
class FVirtualTextureSystem;
struct FVTSpaceDescription;
struct FVTPhysicalSpaceDescription;
union FPhysicalSpaceIDAndAddress;
struct FFeedbackAnalysisParameters;
struct FAddRequestedTilesParameters;
struct FGatherRequestsParameters;
struct FPageUpdateBuffer;

extern uint32 GetTypeHash(const FAllocatedVTDescription& Description);

struct FVirtualTextureUpdateSettings
{
	FVirtualTextureUpdateSettings();

	/** Force settings so that throttling page uploads is effectively disabled. */
	FVirtualTextureUpdateSettings& EnableThrottling(bool bEnable)
	{
		if (!bEnable)
		{
			MaxRVTPageUploads = 99999;
			MaxSVTPageUploads = 99999;
			MaxPagesProduced = 99999;
		}
		return *this;
	}

	/** Force virtual texture updates to be done synchronously. */
	FVirtualTextureUpdateSettings& EnableAsyncTasks(bool bEnable = true)
	{
		bEnableAsyncTasks = bEnable;
		return *this;
	}

	/** Do not perform any updates related to read backs. */
	FVirtualTextureUpdateSettings& EnablePageRequests(bool bEnable = true)
	{
		bEnablePageRequests = bEnable;
		return *this;
	}

	bool bEnableAsyncTasks = true;
	bool bEnablePageRequests = true;
	bool bEnableFeedback;
	bool bEnablePlayback;
	bool bForceContinuousUpdate;
	bool bParallelFeedbackTasks;
	int32 NumFeedbackTasks;
	int32 NumGatherTasks;
	int32 MaxGatherPagesBeforeFlush;
	int32 MaxRVTPageUploads;
	int32 MaxSVTPageUploads;
	int32 MaxPagesProduced;
	int32 MaxContinuousUpdates;
};

class FVirtualTextureUpdater
{
public:
	UE::Tasks::FTask GetTask() const
	{
		return AsyncTask;
	}

private:
	FVirtualTextureUpdateSettings Settings;
	FConcurrentLinearBulkObjectAllocator Allocator;
	FUniqueRequestList* MergedRequestList = nullptr;
	FVirtualTextureFeedback::FMapResult FeedbackMapResult;
	UE::Tasks::FTask AsyncTask;
	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::Num;
	uint32 NumProcessedLoadRequests = 0;
	bool bAsyncTaskAllowed = false;

	friend FVirtualTextureSystem;
};

class FVirtualTextureSystem
{
public:
	static void Initialize();
	static void Shutdown();
	static FVirtualTextureSystem& Get();

	uint32 GetFrame() const { return Frame; }

	void Update(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FScene* Scene, const FVirtualTextureUpdateSettings& Settings);

	// Called in FSceneRenderer::UpdateScene, before FVirtualTextureSystem::BeginUpdate -- see comments there for more info
	void CallPendingCallbacks();

	TUniquePtr<FVirtualTextureUpdater> BeginUpdate(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FScene* Scene, const FVirtualTextureUpdateSettings& Settings);
	void WaitForTasks(FVirtualTextureUpdater* Updater);
	void EndUpdate(FRDGBuilder& GraphBuilder, TUniquePtr<FVirtualTextureUpdater>&& Updater, ERHIFeatureLevel::Type FeatureLevel);

	void ReleasePendingResources();

	IAllocatedVirtualTexture* AllocateVirtualTexture(FRHICommandListBase& RHICmdList, const FAllocatedVTDescription& Desc);
	void DestroyVirtualTexture(IAllocatedVirtualTexture* AllocatedVT);

	FVirtualTextureProducerHandle RegisterProducer(FRHICommandListBase& RHICmdList, const FVTProducerDescription& InDesc, IVirtualTexture* InProducer);
	void ReleaseProducer(const FVirtualTextureProducerHandle& Handle);
	void AddProducerDestroyedCallback(const FVirtualTextureProducerHandle& Handle, FVTProducerDestroyedFunction* Function, void* Baton);
	uint32 RemoveAllProducerDestroyedCallbacks(const void* Baton);

	IAdaptiveVirtualTexture* AllocateAdaptiveVirtualTexture(FRHICommandListBase& RHICmdList, const FAdaptiveVTDescription& AdaptiveVTDesc, const FAllocatedVTDescription& AllocatedVTDesc);
	void DestroyAdaptiveVirtualTexture(IAdaptiveVirtualTexture* AdaptiveVT);

	void RequestTiles(const FVector2D& InScreenSpaceSize, int32 InMipLevel = -1);
	void RequestTiles(const FMaterialRenderProxy* InMaterialRenderProxy, const FVector2D& InScreenSpaceSize, ERHIFeatureLevel::Type InFeatureLevel);

	/**
	 * Helper function to request loading of tiles for a virtual texture that will be displayed in the UI. 
	 * It will request only the tiles that will be visible after clipping to the provided viewport.
	 * @param AllocatedVT			The virtual texture.
	 * @param InScreenSpaceSize		Size on screen at which the texture is to be displayed.
	 * @param InViewportPosition	Position in the viewport where the texture will be displayed.
	 * @param InViewportSize		Size of the viewport.
	 * @param InUV0					UV coordinate to use for the top left corner of the texture.
	 * @param InUV1					UV coordinate to use for the bottom right corner of the texture.
	 * @param InMipLevel [optional] Specific mip level to fetch tiles for.
	 */
	void RequestTiles(IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel = -1);

	UE_DEPRECATED(5.4, "Use RequestTiles() overloads that takes similar parameters. Make sure not to negate the InViewportPosition.")
	void RequestTilesForRegion(IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel = -1);

	void LoadPendingTiles(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel);
	
#if WITH_EDITOR
	void SetVirtualTextureRequestRecordBuffer(uint64 Handle);
	uint64 GetVirtualTextureRequestRecordBuffer(TSet<uint64>& OutPageRequests);
#endif
	void RequestRecordedTiles(TArray<uint64>&& InPageRequests);

	void FlushCache();
	void FlushCache(FVirtualTextureProducerHandle const& ProducerHandle, int32 SpaceID, FIntRect const& TextureRegion, uint32 MaxLevelToEvict, uint32 MaxAgeToKeepMapped);

	float GetGlobalMipBias() const;

	bool IsPendingRootPageMap(IAllocatedVirtualTexture* AllocatedVT) const;

private:
	friend class FFeedbackAnalysisTask;
	friend class FAddRequestedTilesTask;
	friend class FGatherRequestsTask;
	friend class FAllocatedVirtualTexture;
	friend class FAdaptiveVirtualTexture;
	friend class FVirtualTextureProducer;
	friend class FVirtualTextureProducerCollection;
	friend class FTexturePageMap;
	friend class FTexturePagePool;

	FVirtualTextureSystem();
	~FVirtualTextureSystem();

	FVirtualTextureSpace* AcquireSpace(FRHICommandListBase& RHICmdList, const FVTSpaceDescription& InDesc, uint8 InForceSpaceID, FAllocatedVirtualTexture* AllocatedVT);
	void ReleaseSpace(FVirtualTextureSpace* Space);

	FVirtualTextureProducer* FindProducer(const FVirtualTextureProducerHandle& Handle);
	FVirtualTexturePhysicalSpace* AcquirePhysicalSpace(FRHICommandListBase& RHICmdList, const FVTPhysicalSpaceDescription& InDesc);

	FVirtualTextureSpace* GetSpace(uint8 ID) const { check(ID < MaxSpaces); return Spaces[ID].Get(); }
	FAdaptiveVirtualTexture* GetAdaptiveVirtualTexture(uint8 ID) const { check(ID < MaxSpaces); return AdaptiveVTs[ID]; }
	FVirtualTexturePhysicalSpace* GetPhysicalSpace(uint16 ID) const { check(PhysicalSpaces[ID]);  return PhysicalSpaces[ID]; }

	void LockTile(const FVirtualTextureLocalTile& Tile);
	void UnlockTile(const FVirtualTextureLocalTile& Tile, const FVirtualTextureProducer* Producer);
	void ForceUnlockAllTiles(const FVirtualTextureProducerHandle& ProducerHandle, const FVirtualTextureProducer* Producer);

	void BeginUpdate(FRDGBuilder& GraphBuilder, FVirtualTextureUpdater* Updater);

	void AllocateResources(FRDGBuilder& GraphBuilder);
	void DestroyPendingVirtualTextures(bool bForceDestroyAll);
	void ReleasePendingSpaces();

	void RequestTilesForRegionInternal(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, const FVector2D& InViewportPosition, const FVector2D& InViewportSize, const FVector2D& InUV0, const FVector2D& InUV1, int32 InMipLevel);
	void RequestTilesInternal(const IAllocatedVirtualTexture* AllocatedVT, int32 InMipLevel);
	void RequestTilesInternal(const IAllocatedVirtualTexture* AllocatedVT, const FVector2D& InScreenSpaceSize, int32 InMipLevel);
	
	void SubmitRequestsFromLocalTileList(FRHICommandList& RHICmdList, TArray<FVirtualTextureLocalTile>& OutDeferredTiles, const TSet<FVirtualTextureLocalTile>& LocalTileList, EVTProducePageFlags Flags, ERHIFeatureLevel::Type FeatureLevel, uint32 MaxRequestsToProduce);

	void GatherFeedbackRequests(FConcurrentLinearBulkObjectAllocator& Allocator, const FVirtualTextureUpdateSettings& Settings, const FVirtualTextureFeedback::FMapResult& FeedbackResult, FUniqueRequestList* MergedRequestList);
	void GatherLockedTileRequests(FUniqueRequestList* MergedRequestList);
	void GatherPackedTileRequests(FConcurrentLinearBulkObjectAllocator& Allocator, const FVirtualTextureUpdateSettings& Settings, FUniqueRequestList* MergedRequestList);

	void SubmitPreMappedRequests(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint32 MaxMappedTilesToProduce);

	void SubmitThrottledRequests(FRHICommandList& RHICmdList, FVirtualTextureUpdater* Updater, bool bContinuousUpdates);
	void SubmitRequests(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, FConcurrentLinearBulkObjectAllocator& Allocator, FVirtualTextureUpdateSettings const& Settings, FUniqueRequestList* RequestList, bool bAsync);
	void FinalizeRequests(FRDGBuilder& GraphBuilder);

	void GatherRequests(FUniqueRequestList* MergedRequestList, const FUniquePageList* UniquePageList, uint32 FrameRequested, FConcurrentLinearBulkObjectAllocator& Allocator, FVirtualTextureUpdateSettings const& Settings);

	void AddPageUpdate(FPageUpdateBuffer* Buffers, uint32 FlushCount, uint32 PhysicalSpaceID, uint16 pAddress);

	void FeedbackAnalysisTask(const FFeedbackAnalysisParameters& Parameters);
	void AddRequestedTilesTask(const FAddRequestedTilesParameters& Parameters);
	void GatherRequestsTask(const FGatherRequestsParameters& Parameters);

	void GetContinuousUpdatesToProduce(FUniqueRequestList const* RequestList, int32 MaxTilesToProduce, int32 MaxContinuousUpdates);

	void UpdateResidencyTracking() const;
	void GrowPhysicalPools() const;

#if WITH_EDITOR
	void RecordPageRequests(FUniquePageList const* UniquePageList, TSet<uint64>& OutPages);
#endif

	uint32	Frame;

	mutable UE::FRecursiveMutex Mutex;

	static const uint32 MaxNumTasks = 16;
	static const uint32 MaxSpaces = 16;
	uint32 NumAllocatedSpaces = 0;
	TUniquePtr<FVirtualTextureSpace> Spaces[MaxSpaces];
	TArray<FVirtualTexturePhysicalSpace*> PhysicalSpaces;
	FVirtualTextureProducerCollection Producers;

	TArray<IAllocatedVirtualTexture*> PendingDeleteAllocatedVTs;

	TMap<FAllocatedVTDescription, FAllocatedVirtualTexture*> AllocatedVTs;
	TArray<IAllocatedVirtualTexture*> AllocatedVTsToMap;
	TMap<uint32, IAllocatedVirtualTexture*> PersistentVTMap;

	FAdaptiveVirtualTexture* AdaptiveVTs[MaxSpaces] = { nullptr };

	bool bUpdating = false;
	bool bFlushCaches;
	void FlushCachesFromConsole();
	FAutoConsoleCommand FlushCachesCommand;

	void DumpFromConsole();
	FAutoConsoleCommand DumpCommand;

	void ListPhysicalPoolsFromConsole();
	FAutoConsoleCommand ListPhysicalPools;

	void DumpPoolUsageFromConsole();
	FAutoConsoleCommand DumpPoolUsageCommand;

#if WITH_EDITOR
	void SaveAllocatorImagesFromConsole();
	FAutoConsoleCommand SaveAllocatorImages;
#endif

	TArray<uint32> RequestedPackedTiles;

	TArray<FVirtualTextureLocalTile> TilesToLock;
	TArray<FVirtualTextureLocalTile> TilesToLockForNextFrame;
	FTexturePageLocks TileLocks;

	TSet<FVirtualTextureLocalTile> ContinuousUpdateTilesToProduce;
	TSet<FVirtualTextureLocalTile> MappedTilesToProduce;
	TArray<FVirtualTextureLocalTile> TransientCollectedPages;
	TArray<IVirtualTextureFinalizer*> Finalizers;

#if WITH_EDITOR
	uint64 PageRequestRecordHandle;
	TSet<uint64> PageRequestRecordBuffer;
#endif
	TArray<uint64> PageRequestPlaybackBuffer;

#if !UE_BUILD_SHIPPING
	void GetOnScreenMessages(FCoreDelegates::FSeverityMessageMap& OutMessages);
	FDelegateHandle OnScreenMessageDelegateHandle;
	void DrawResidencyHud(class UCanvas*, class APlayerController*);
	FDelegateHandle	DrawResidencyHudDelegateHandle;
	void UpdateCsvStats();
#endif
};
