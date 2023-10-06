// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "SparseVolumeTexture/ISparseVolumeTextureStreamingManager.h"
#include "IO/IoDispatcher.h"
#include "Memory/SharedBuffer.h"
#include "Serialization/BulkData.h"
#include "RenderResource.h"
#include "Containers/IntrusiveDoubleLinkedList.h"
#include "Containers/Map.h"
#include "Containers/StaticArray.h"
#include "Containers/Union.h"
#include "RenderGraphBuilder.h"

class UStreamableSparseVolumeTexture;
class USparseVolumeTextureFrame;

namespace UE
{
	namespace DerivedData
	{
		class FRequestOwner; // Can't include DDC headers from here, so we have to forward declare
		struct FCacheGetChunkRequest;
	}
}

namespace UE
{
namespace SVT
{

struct FResources;
struct FMipLevelStreamingInfo;
class FTextureRenderResources;

// Uniquely identifies the mip level of a frame in a static or animated SparseVolumeTexture
struct FMipLevelKey
{
	UStreamableSparseVolumeTexture* SVT = nullptr; // This struct is used on the rendering thread. Do not dereference!
	uint16 FrameIndex = INDEX_NONE;
	uint16 MipLevelIndex = INDEX_NONE;

	friend FORCEINLINE uint32 GetTypeHash(const FMipLevelKey& Key)
	{
		return HashCombine(GetTypeHash(Key.SVT), GetTypeHash(((uint32)Key.FrameIndex << 16u) | (uint32)Key.MipLevelIndex));
	}

	FORCEINLINE bool operator==(const FMipLevelKey& Other) const
	{
		return (SVT == Other.SVT) && (FrameIndex == Other.FrameIndex) && (MipLevelIndex == Other.MipLevelIndex);
	}

	FORCEINLINE bool operator!=(const FMipLevelKey& Other) const
	{
		return !(*this == Other);
	}

	FORCEINLINE bool operator<(const FMipLevelKey& Other) const
	{
		return SVT != Other.SVT ? SVT < Other.SVT : FrameIndex != Other.FrameIndex ? FrameIndex < Other.FrameIndex : MipLevelIndex > Other.MipLevelIndex;
	}
};

struct FStreamingRequest
{
	static constexpr uint32 BlockingPriority = 0xFFFFFFFFu;
	FMipLevelKey Key;
	uint32 Priority; // A higher value means a higher priority, 0xFFFFFF means blocking

	FORCEINLINE bool operator<(const FStreamingRequest& Other) const
	{
		return Key != Other.Key ? Key < Other.Key : Priority > Other.Priority;
	}
};

class FStreamingManager : public FRenderResource, public IStreamingManager
{
public:
	FStreamingManager();

	//~ Begin FRenderResource Interface.
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;
	//~ End FRenderResource Interface.

	//~ Begin IStreamingManager Interface.
	virtual void Add_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture) override;
	virtual void Remove_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture) override;
	virtual void Request_GameThread(UStreamableSparseVolumeTexture* SparseVolumeTexture, float FrameIndex, int32 MipLevel, bool bBlocking) override;
	virtual void Update_GameThread() override;
	
	virtual void Request(UStreamableSparseVolumeTexture* SparseVolumeTexture, float FrameIndex, int32 MipLevel, bool bBlocking) override;
	virtual void BeginAsyncUpdate(FRDGBuilder& GraphBuilder, bool bBlocking) override;
	virtual void EndAsyncUpdate(FRDGBuilder& GraphBuilder) override;
	//~ End IStreamingManager Interface.

private:
	friend class FStreamingUpdateTask;

	// SVT_TODO: actually implement streaming in/out of page table mips greater than this value.
	static constexpr int32 NumAlwaysResidentPageTableMipLevels = 6;

	// Represents the physical tile data texture that serves as backing memory for the streamed in tiles. While this is treated as a single logical texture,
	// it currently supports up to two actual RHI textures.
	struct FTileDataTexture : public FRenderResource
	{
	public:
		static constexpr uint32 PhysicalCoordMask = (1u << 24u) - 1u; // Lower 24 bits are used for storing XYZ in 8 bit each. Upper 8 bit can be used by the caller. 

		FIntVector3 ResolutionInTiles;
		int32 PhysicalTilesCapacity;
		EPixelFormat FormatA;
		EPixelFormat FormatB;
		FVector4f FallbackValueA;
		FVector4f FallbackValueB;
		FTextureRHIRef TileDataTextureARHIRef;
		FTextureRHIRef TileDataTextureBRHIRef;
		TUniquePtr<class FTileUploader> TileUploader;
		int32 NumTilesToUpload;
		int32 NumVoxelsToUploadA;
		int32 NumVoxelsToUploadB;

		// Constructor. May change the requested ResolutionInTiles (and resulting PhysicalTilesCapacity) if it exceeds hardware limits.
		FTileDataTexture(const FIntVector3& ResolutionInTiles, EPixelFormat FormatA, EPixelFormat FormatB, const FVector4f& FallbackValueA, const FVector4f& FallbackValueB);

		// Allocate a tile slot in the texture. The resulting value is a packed coordinate (8 bit per component) of the allocated slot or INDEX_NONE if the allocation failed.
		// The upper 8 bit are free to be used by the caller.
		uint32 Allocate() 
		{ 
			check(PhysicalTilesCapacity == TileCoords.Num());
			return TileCoords.IsValidIndex(NextFreeTileCoordIndex) ? TileCoords[NextFreeTileCoordIndex++] : INDEX_NONE; 
		}

		// Frees a previously allocated tile slot. The upper 8 bit (user data) are automatically cleared by this function.
		void Free(uint32 PackedPhysicalCoord) 
		{ 
			check(PackedPhysicalCoord != INDEX_NONE)
			PackedPhysicalCoord &= PhysicalCoordMask;
			check(PhysicalTilesCapacity == TileCoords.Num());
			check(NextFreeTileCoordIndex > 0);
#if DO_GUARD_SLOW
			for (int32 i = NextFreeTileCoordIndex; i < PhysicalTilesCapacity; ++i)
			{
				check(TileCoords[i] != PackedPhysicalCoord);
			}
#endif
			TileCoords[--NextFreeTileCoordIndex] = PackedPhysicalCoord;
		}

		// Number of tiles available for allocation.
		int32 GetNumAvailableTiles() { return PhysicalTilesCapacity - NextFreeTileCoordIndex; }
		virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
		virtual void ReleaseRHI() override;

	private:

		TArray<uint32> TileCoords;
		int32 NextFreeTileCoordIndex = 0;
		uint32 NumRefs = 0;
	};

	// Represents a context or "window" into the frame sequence that moves along the playback direction. It is used to cache the prefetch direction.
	struct FStreamingWindow
	{
		static constexpr int32 WindowSize = 5;
		float CenterFrame = -1.0f; // Frame index this window is centered around
		float LastCenterFrame = -1.0f;
		int32 NumRequestsThisUpdate = 0;
		uint32 LastRequested = 0;
		bool bPlayForward = false;
		bool bPlayBackward = false;
	};

	// One per frame of each SVT
	struct FFrameInfo
	{
		const FResources* Resources;						// Initialized on the game thread
		FTextureRenderResources* TextureRenderResources;	// Initialized on the game thread

		int32 NumMipLevels; // Number of actual mip levels of this frame. Depending on virtual volume extents, frames can have different numbers of levels.
		int32 LowestRequestedMipLevel; // Lowest mip level that should be resident. Can be lower than LowestResidentMipLevel when streaming in new mips. Stream-out is instant, so it should never be higher.
		int32 LowestResidentMipLevel; // Actually resident on the GPU
		TArray<TArray<uint32>> TileAllocations; // TileAllocations[MipLevel][PhysicalTileIndex]
		FTextureRHIRef PageTableTextureRHIRef;
	};

	// Used to keep track of the least-recently-used order of mip levels. This is done per mip level and not per frame, so that lower resolution mip levels are kept in memory for longer.
	struct FLRUNode : public TIntrusiveDoubleLinkedListNode<FLRUNode>
	{
		FLRUNode* NextHigherMipLevel = nullptr;
		uint32 LastRequested = INDEX_NONE;
		uint32 RefCount = 0; // Keep track of how many lower mip levels have a dependency on this one
		uint32 PendingMipLevelIndex = INDEX_NONE;
		int16 FrameIndex = INDEX_NONE;
		int16 MipLevelIndex = INDEX_NONE;
	};

	// Streaming manager internal data for each SVT
	struct FStreamingInfo
	{
		EPixelFormat FormatA;
		EPixelFormat FormatB;
		FVector4f FallbackValueA;
		FVector4f FallbackValueB;
		int32 NumMipLevelsGlobal; // Maximum number of mip levels in the sequence
		uint32 LastRequested; // Last update index when some frame in this SVT was requested
		TArray<FFrameInfo> PerFrameInfo;
		TArray<FLRUNode> LRUNodes;
		TArray<TIntrusiveDoubleLinkedList<FLRUNode>> PerMipLRULists; // PerMipLRULists[MipLevel]
		TArray<FStreamingWindow> StreamingWindows;

		TUniquePtr<FTileDataTexture> TileDataTexture;
		TRefCountPtr<FRDGPooledBuffer> StreamingInfoBuffer; // One uint32 per frame storing the LowestResidentMipLevel
		FShaderResourceViewRHIRef StreamingInfoBufferSRVRHIRef;
		TBitArray<> DirtyStreamingInfoData; // One bit per frame, potentially marking the streaming info data as dirty/in need of an update
	};

	// Represents an IO request for a mip level
	struct FPendingMipLevel
	{
		UStreamableSparseVolumeTexture* SparseVolumeTexture = nullptr; // Do not dereference!
		int32 FrameIndex = INDEX_NONE;
		int32 MipLevelIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
		FSharedBuffer SharedBuffer;
		enum class EState
		{
			None,
			DDC_Pending,
			DDC_Ready,
			DDC_Failed,
			Memory,
			Disk,
		} State = EState::None;
		uint32 RetryCount = 0;
		uint32 RequestVersion = 0;
#endif
		FIoBuffer RequestBuffer;
		FBulkDataBatchReadRequest Request;
		uint32 IssuedInFrame = 0;
		bool bBlocking = false;

		void Reset()
		{
			SparseVolumeTexture = nullptr;
			FrameIndex = INDEX_NONE;
			MipLevelIndex = INDEX_NONE;
#if WITH_EDITORONLY_DATA
			SharedBuffer.Reset();
			State = EState::None;
			RetryCount = 0;
			++RequestVersion;
#endif
			Request.Reset();
			RequestBuffer = {};
			IssuedInFrame = 0;
			bBlocking = false;
		}
	};

	// Encapsulates all the src/dst pointers for memcpy-ing the streamed data into GPU memory.
	// See the comment on FMipLevelStreamingInfo for details about these pointers.
	struct FUploadTask
	{
		struct FTileDataTask
		{
			TStaticArray<uint8*, 2> DstOccupancyBitsPtrs;
			TStaticArray<uint8*, 2> DstTileDataOffsetsPtrs;
			TStaticArray<uint8*, 2> DstTileDataPtrs;
			uint8* DstPhysicalTileCoords;
			TStaticArray<const uint8*, 2> SrcOccupancyBitsPtrs;
			TStaticArray<const uint8*, 2> SrcTileDataOffsetsPtrs;
			TStaticArray<const uint8*, 2> SrcTileDataPtrs;
			const uint8* SrcPhysicalTileCoords;
			TStaticArray<uint32, 2> TileDataBaseOffsets;
			TStaticArray<int32, 2> TileDataSizes;
			int32 NumPhysicalTiles;
		};

		struct FPageTableTask
		{
			FPendingMipLevel* PendingMipLevel;
			uint8* DstPageCoords;
			uint8* DstPageEntries;
			const uint8* SrcPageCoords;
			const uint8* SrcPageEntries;
			int32 NumPageTableUpdates;
		};

		TUnion<FTileDataTask, FPageTableTask> Union;
	};

	struct FPageTableClear
	{
		FTextureRHIRef PageTableTexture;
		int32 MipLevel;
	};

	struct FAsyncState
	{
		int32 NumReadyMipLevels = 0;
		bool bUpdateActive = false;
		bool bUpdateIsAsync = false;
	};

	// Stores info extracted from the UStreamableSparseVolumeTexture and its USparseVolumeTextureFrames so that we don't need to access these uobjects on the rendering thread
	struct FNewSparseVolumeTextureInfo
	{
		UStreamableSparseVolumeTexture* SVT = nullptr; // Do not dereference!
		EPixelFormat FormatA = PF_Unknown;
		EPixelFormat FormatB = PF_Unknown;
		FVector4f FallbackValueA = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		FVector4f FallbackValueB = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		int32 NumMipLevelsGlobal = 0;
		TArray<FFrameInfo> FrameInfo; // Only Resources and TextureRenderResources are initialized
	};

	TMap<UStreamableSparseVolumeTexture*, TUniquePtr<FStreamingInfo>> StreamingInfo; // Do not dereference the key! We just read the pointer itself.
	TMap<FMipLevelKey, uint32> RequestsHashTable;
	TArray<FPendingMipLevel> PendingMipLevels;
#if WITH_EDITORONLY_DATA
	TUniquePtr<UE::DerivedData::FRequestOwner> RequestOwner = nullptr;
	TUniquePtr<UE::DerivedData::FRequestOwner> RequestOwnerBlocking = nullptr;
#endif

	TUniquePtr<class FPageTableUpdater> PageTableUpdater = nullptr;
	TUniquePtr<class FStreamingInfoBufferUpdater> StreamingInfoBufferUpdater = nullptr;
	FGraphEventArray AsyncTaskEvents;
	FAsyncState AsyncState;
	int32 MaxPendingMipLevels = 0;
	int32 NumPendingMipLevels = 0;
	int32 NextPendingMipLevelIndex = 0;
	uint32 NextUpdateIndex = 1;

	// Transient lifetime
	TArray<FStreamingRequest> ParentRequestsToAdd;
	TSet<FTileDataTexture*> TileDataTexturesToUpdate;
	TSet<FStreamingInfo*> SVTsWithInvalidatedStreamingInfoBuffer; // Changes in the lowest resident mip level cause invalidation
	TArray<FStreamingRequest> PrioritizedRequestsHeap;
	TArray<FStreamingRequest> SelectedRequests;
	TArray<FPageTableClear> PageTableClears;
	TArray<FUploadTask> UploadTasks; // accessed on the async thread
	TArray<FPendingMipLevel*> UploadCleanupTasks; // accessed on the async thread

	void AddInternal(FRDGBuilder& GraphBuilder, FNewSparseVolumeTextureInfo&& NewSVTInfo);
	void RemoveInternal(UStreamableSparseVolumeTexture* SparseVolumeTexture);
	bool AddRequest(const FStreamingRequest& Request); // Returns true when the request is new or it has been updated with a higher priority
	void AsyncUpdate();
	void AddParentRequests(); // Add requests for all parent mip levels
	void SelectHighestPriorityRequestsAndUpdateLRU(int32 MaxSelectedRequests);
	void IssueRequests(int32 MaxSelectedRequests);
	void StreamOutMipLevel(FStreamingInfo* SVTInfo, FLRUNode* LRUNode);
	int32 DetermineReadyMipLevels();
	void InstallReadyMipLevels();
	FStreamingInfo* FindStreamingInfo(UStreamableSparseVolumeTexture* Key); // Returns nullptr if the key can't be found

#if WITH_EDITORONLY_DATA
	UE::DerivedData::FCacheGetChunkRequest BuildDDCRequest(const FResources& Resources, const FMipLevelStreamingInfo& MipLevelStreamingInfo, const uint32 PendingMipLevelIndex);
	void RequestDDCData(TConstArrayView<UE::DerivedData::FCacheGetChunkRequest> DDCRequests, bool bBlocking);
#endif
};

}
}
