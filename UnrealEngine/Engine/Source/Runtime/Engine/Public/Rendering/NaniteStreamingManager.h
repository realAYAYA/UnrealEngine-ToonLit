// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NaniteResources.h"
#include "UnifiedBuffer.h"
#include "RenderGraphBuilder.h"
#include "RHIGPUReadback.h"

namespace UE
{
	namespace DerivedData
	{
		class FRequestOwner; // Can't include DDC headers from here, so we have to forward declare
		struct FCacheGetChunkRequest;
	}
}

namespace Nanite
{

struct FPageKey
{
	uint32 RuntimeResourceID	= INDEX_NONE;
	uint32 PageIndex			= INDEX_NONE;
};

FORCEINLINE uint32 GetTypeHash( const FPageKey& Key )
{
	return Key.RuntimeResourceID * 0xFC6014F9u + Key.PageIndex * 0x58399E77u;
}

FORCEINLINE bool operator==( const FPageKey& A, const FPageKey& B )
{
	return A.RuntimeResourceID == B.RuntimeResourceID && A.PageIndex == B.PageIndex;
}

FORCEINLINE bool operator!=(const FPageKey& A, const FPageKey& B)
{
	return !(A == B);
}

FORCEINLINE bool operator<(const FPageKey& A, const FPageKey& B)
{
	return A.RuntimeResourceID != B.RuntimeResourceID ? A.RuntimeResourceID < B.RuntimeResourceID : A.PageIndex < B.PageIndex;
}


// Before deduplication
struct FGPUStreamingRequest
{
	uint32		RuntimeResourceID_Magic;
	uint32		PageIndex_NumPages_Magic;
	uint32		Priority_Magic;
};

// After deduplication
struct FStreamingRequest
{
	FPageKey	Key;
	uint32		Priority;
};

FORCEINLINE bool operator<(const FStreamingRequest& A, const FStreamingRequest& B)
{
	return A.Key != B.Key ? A.Key < B.Key : A.Priority > B.Priority;
}

struct FStreamingPageInfo
{
	FStreamingPageInfo* Next;
	FStreamingPageInfo* Prev;

	FPageKey	RegisteredKey;
	FPageKey	ResidentKey;
	
	uint32		GPUPageIndex;
	uint32		LatestUpdateIndex;
	uint32		RefCount;
};

struct FRootPageInfo
{
	uint32	RuntimeResourceID;
	uint32	NumClusters;
};

struct FPendingPage
{
#if WITH_EDITOR
	FSharedBuffer			SharedBuffer;
	enum class EState
	{
		Pending,
		Ready,
		Failed,
	} State = EState::Pending;
	uint32					RetryCount = 0;
#else
	FIoBuffer				RequestBuffer;
	FBulkDataBatchReadRequest Request;
#endif

	uint32					GPUPageIndex = INDEX_NONE;
	FPageKey				InstallKey;
#if !UE_BUILD_SHIPPING
	uint32					BytesLeftToStream = 0;
#endif
};

class FRequestsHashTable;
class FStreamingPageUploader;

struct FAsyncState
{
	FRHIGPUBufferReadback*	LatestReadbackBuffer		= nullptr;
	const uint32*			LatestReadbackBufferPtr		= nullptr;
	uint32					NumReadyPages				= 0;
	bool					bUpdateActive				= false;
	bool					bBuffersTransitionedToWrite = false;
};

/*
 * Streaming manager for Nanite.
 */
class FStreamingManager : public FRenderResource
{
public:
	FStreamingManager();
	
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	void	Add( FResources* Resources );
	void	Remove( FResources* Resources );

	ENGINE_API void BeginAsyncUpdate(FRDGBuilder& GraphBuilder);			// Called once per frame before any Nanite rendering has occurred. Must be called before EndUpdate.
	ENGINE_API void EndAsyncUpdate(FRDGBuilder& GraphBuilder);				// Called once per frame before any Nanite rendering has occurred. Must be called after BeginUpdate.
	ENGINE_API bool IsAsyncUpdateInProgress();
	ENGINE_API void	SubmitFrameStreamingRequests(FRDGBuilder& GraphBuilder);		// Called once per frame after the last request has been added.

	FRDGBuffer* GetStreamingRequestsBuffer(FRDGBuilder& GraphBuilder) const
	{
		return GraphBuilder.RegisterExternalBuffer(StreamingRequestsBuffer);
	}

	uint32		GetStreamingRequestsBufferVersion() { return StreamingRequestsBufferVersion; }

	FRDGBufferSRV* GetHierarchySRV(FRDGBuilder& GraphBuilder) const
	{
		return GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(Hierarchy.DataBuffer));
	}

	FRDGBufferSRV* GetClusterPageDataSRV(FRDGBuilder& GraphBuilder) const
	{
		return GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ClusterPageData.DataBuffer));
	}

	FRDGBufferSRV* GetImposterDataSRV(FRDGBuilder& GraphBuilder) const
	{
		return GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(ImposterData.DataBuffer));
	}

	uint32		GetMaxStreamingPages() const			{ return MaxStreamingPages; }

	inline bool HasResourceEntries() const
	{
		return !RuntimeResourceMap.IsEmpty();
	}

	TSet<uint32> GetAndClearModifiedResources()
	{
		return MoveTemp(ModifiedResources);
	}

	ENGINE_API void		PrefetchResource(const FResources* Resource, uint32 NumFramesUntilRender);
	ENGINE_API void		RequestNanitePages(TArrayView<uint32> RequestData);
#if WITH_EDITOR
	ENGINE_API uint64	GetRequestRecordBuffer(TArray<uint32>& OutRequestData);
	ENGINE_API void		SetRequestRecordBuffer(uint64 Handle);
#endif
	
private:
	friend class FStreamingUpdateTask;

	struct FHeapBuffer
	{
		int32					TotalUpload = 0;

		FGrowOnlySpanAllocator	Allocator;

		FRDGScatterUploadBuffer	UploadBuffer;
		TRefCountPtr<FRDGPooledBuffer>			DataBuffer;

		void Release()
		{
			UploadBuffer = {};
			DataBuffer = {};
		}
	};

	struct FResourcePrefetch
	{
		uint32 RuntimeResourceID;
		uint32 NumFramesUntilRender;
	};

	FHeapBuffer				ClusterPageData;	// FPackedCluster*, GeometryData { Index, Position, TexCoord, TangentX, TangentZ }*
	FRDGScatterUploadBuffer ClusterFixupUploadBuffer;
	FHeapBuffer				Hierarchy;
	FHeapBuffer				ImposterData;
	TRefCountPtr< FRDGPooledBuffer > StreamingRequestsBuffer;

	uint32					StreamingRequestsBufferVersion;
	uint32					MaxStreamingPages;
	uint32					MaxPendingPages;
	uint32					MaxPageInstallsPerUpdate;
	uint32					MaxStreamingReadbackBuffers;

	uint32					ReadbackBuffersWriteIndex;
	uint32					ReadbackBuffersNumPending;

	TArray<uint32>			NextRootPageVersion;
	uint32					NextUpdateIndex;
	uint32					NumRegisteredStreamingPages;
	uint32					NumPendingPages;
	uint32					NextPendingPageIndex;

	uint32					StatNumRootPages;
	uint32					StatPeakRootPages;
	uint32					StatPeakAllocatedRootPages;

	TArray<FRootPageInfo>	RootPageInfos;

#if !UE_BUILD_SHIPPING
	uint64					PrevUpdateTick;
#endif

	TArray< FRHIGPUBufferReadback* >		StreamingRequestReadbackBuffers;
	TArray< FResources* >					PendingAdds;

	TMap< uint32, FResources* >				RuntimeResourceMap;
	TMultiMap< uint32, FResources* >		PersistentHashResourceMap;			// TODO: MultiMap to handle potential collisions and issues with there temporarily being two meshes with the same hash because of unordered add/remove.
	TMap< FPageKey, FStreamingPageInfo* >	RegisteredStreamingPagesMap;		// This is updated immediately.
	TMap< FPageKey, FStreamingPageInfo* >	CommittedStreamingPageMap;			// This update is deferred to the point where the page has been loaded and committed to memory.
	TArray< FStreamingRequest >				PrioritizedRequestsHeap;
	FStreamingPageInfo						StreamingPageLRU;

	TSet<uint32>							ModifiedResources;

	FStreamingPageInfo*						StreamingPageInfoFreeList;
	TArray< FStreamingPageInfo >			StreamingPageInfos;
	TArray< FFixupChunk* >					StreamingPageFixupChunks;			// Fixup information for resident streaming pages. We need to keep this around to be able to uninstall pages.

	TArray< FPendingPage >					PendingPages;
#if !WITH_EDITOR
	TArray< uint8 >							PendingPageStagingMemory;
#endif
	TArray< uint32 >						GPUPageDependencies;

	FRequestsHashTable*						RequestsHashTable = nullptr;
	FStreamingPageUploader*					PageUploader = nullptr;

	FGraphEventArray						AsyncTaskEvents;
	FAsyncState								AsyncState;

#if WITH_EDITOR
	UE::DerivedData::FRequestOwner*			RequestOwner;

	uint64									PageRequestRecordHandle = (uint64)-1;
	TMap<FPageKey, uint32>					PageRequestRecordMap;
#endif
	TArray<uint32>							PendingExplicitRequests;
	TArray<FResourcePrefetch>				PendingResourcePrefetches;

	void AddPendingExplicitRequests();
	void AddPendingResourcePrefetchRequests();

	void CollectDependencyPages( FResources* Resources, TSet< FPageKey >& DependencyPages, const FPageKey& Key );
	void SelectStreamingPages( FResources* Resources, TArray< FPageKey >& SelectedPages, TSet<FPageKey>& SelectedPagesSet, uint32 RuntimeResourceID, uint32 PageIndex, uint32 MaxSelectedPages );

	void RegisterStreamingPage( FStreamingPageInfo* Page, const FPageKey& Key );
	void UnregisterPage( const FPageKey& Key );
	void MovePageToFreeList( FStreamingPageInfo* Page );

	void ApplyFixups( const FFixupChunk& FixupChunk, const FResources& Resources, bool bIsUninstall );

	bool ArePageDependenciesCommitted(uint32 RuntimeResourceID, uint32 DependencyPageStart, uint32 DependencyPageNum);

	uint32 GPUPageIndexToGPUOffset(uint32 PageIndex) const;

	void ProcessNewResources( FRDGBuilder& GraphBuilder);
	
	uint32 DetermineReadyPages();
	void InstallReadyPages( uint32 NumReadyPages );

	void AsyncUpdate();

	void ClearStreamingRequestCount(FRDGBuilder& GraphBuilder, FRDGBufferUAVRef BufferUAVRef);
#if DO_CHECK
	void VerifyPageLRU( FStreamingPageInfo& List, uint32 TargetListLength, bool bCheckUpdateIndex );
#endif

#if NANITE_SANITY_CHECK_STREAMING_REQUESTS
	void SanityCheckStreamingRequests(const FGPUStreamingRequest* StreamingRequestsPtr, const uint32 NumStreamingRequests);
#endif

#if WITH_EDITOR
	void RecordGPURequests();
	UE::DerivedData::FCacheGetChunkRequest BuildDDCRequest(const FResources& Resources, const FPageStreamingState& PageStreamingState, const uint32 PendingPageIndex);
	void RequestDDCData(TConstArrayView<UE::DerivedData::FCacheGetChunkRequest> DDCRequests);
#endif
};

extern ENGINE_API TGlobalResource< FStreamingManager > GStreamingManager;

} // namespace Nanite
