// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/App.h"
#include "Engine/TextureStreamingTypes.h"
#include "Serialization/BulkData.h"
#include "Templates/RefCounting.h"
#include "RenderAssetUpdate.h"
#include "Streaming/StreamableRenderResourceState.h"
#include "PerQualityLevelProperties.h"
#include "StreamableRenderAsset.generated.h"

#define STREAMABLERENDERASSET_NODEFAULT(FuncName) LowLevelFatalError(TEXT("UStreamableRenderAsset::%s has no default implementation"), TEXT(#FuncName))
 // Allows yield to lower priority threads
#define RENDER_ASSET_STREAMING_SLEEP_DT (0.010f)

namespace Nanite
{
	class FCoarseMeshStreamingManager;
}

enum class EStreamableRenderAssetType : uint8
{
	None,
	Texture,
	StaticMesh,
	SkeletalMesh,
	LandscapeMeshMobile UE_DEPRECATED(5.1, "LandscapeMeshMobile is now deprecated and will be removed."),
	NaniteCoarseMesh,
};

UCLASS(Abstract, MinimalAPI)
class UStreamableRenderAsset : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Destructor */
	ENGINE_API virtual ~UStreamableRenderAsset() {};

	/** Get an integer representation of the LOD group */
	virtual int32 GetLODGroupForStreaming() const
	{
		return 0;
	}

	virtual int32 CalcCumulativeLODSize(int32 NumLODs) const
	{
		STREAMABLERENDERASSET_NODEFAULT(CalcCumulativeLODSize);
		return -1;
	}

	virtual FIoFilenameHash GetMipIoFilenameHash(const int32 MipIndex) const
	{
		STREAMABLERENDERASSET_NODEFAULT(GetMipIoFilenameHash);
		return INVALID_IO_FILENAME_HASH;
	}

	virtual bool DoesMipDataExist(const int32 MipIndex) const
	{
		STREAMABLERENDERASSET_NODEFAULT(DoesMipDataExist);
		return false;
	}

	/** Tries to cancel a pending LOD change request. Requests cannot be canceled if they are in the finalization phase. */
	ENGINE_API void CancelPendingStreamingRequest();

	/** Whether a stream in / out request is pending. Only one request can exists at anytime for a streamable asset. This also includes the resource initialization! */
	ENGINE_API bool HasPendingInitOrStreaming(bool bWaitForLODTransition = false) const;

	/** Whether there is a pending update and it is locked within an update step. Used to prevent dealocks in SuspendRenderAssetStreaming(). */
	bool IsPendingStreamingRequestLocked() const;

	/**
	* Unload some mips from memory. Only usable if the asset is streamable.
	*
	* @param NewMipCount - The desired mip count after the mips are unloaded.
	* @return Whether any mips were requested to be unloaded.
	*/
	virtual bool StreamOut(int32 NewMipCount)
	{
		STREAMABLERENDERASSET_NODEFAULT(StreamOut);
		return false;
	}

	/**
	* Loads mips from disk to memory. Only usable if the asset is streamable.
	*
	* @param NewMipCount - The desired mip count after the mips are loaded.
	* @param bHighPrio   - true if the load request is of high priority and must be issued before other asset requests.
	* @return Whether any mips were resquested to be loaded.
	*/
	virtual bool StreamIn(int32 NewMipCount, bool bHighPrio)
	{
		STREAMABLERENDERASSET_NODEFAULT(StreamIn);
		return false;
	}

	/**
	* Invalidates per-asset last render time. Mainly used to opt in UnknownRefHeuristic
	* during LOD index calculation. See FStreamingRenderAsset::bUseUnknownRefHeuristic
	*/
	virtual void InvalidateLastRenderTimeForStreaming() {}

	/**
	* Get the per-asset last render time. FLT_MAX means never use UnknownRefHeuristic
	* and the asset will only keep non-streamable LODs when there is no instance/reference
	* in the scene
	*/
	virtual float GetLastRenderTimeForStreaming() const { return FLT_MAX; }

	/**
	* Returns whether miplevels should be forced resident.
	*
	* @return true if either transient or serialized override requests miplevels to be resident, false otherwise
	*/
	virtual bool ShouldMipLevelsBeForcedResident() const
	{
		return bGlobalForceMipLevelsToBeResident
			|| bForceMiplevelsToBeResident
			|| ForceMipLevelsToBeResidentTimestamp >= FApp::GetCurrentTime();
	}

	/**
	* Register a callback to get notified when a certain mip or LOD is resident or evicted.
	* @param Component The context component
	* @param LODIdx The mip or LOD level to wait for
	* @param TimeoutSecs Timeout in seconds
	* @param bOnStreamIn Whether to get notified when the specified level is resident or evicted
	* @param Callback The callback to call
	*/
	ENGINE_API void RegisterMipLevelChangeCallback(UPrimitiveComponent* Component, int32 LODIdx, float TimeoutSecs, bool bOnStreamIn, FLODStreamingCallback&& Callback);

	/**
	* Remove mip level change callbacks registered by a component.
	* @param Component The context component
	*/
	ENGINE_API void RemoveMipLevelChangeCallback(UPrimitiveComponent* Component);

	/**
	* Not thread safe and must be called on GT timeline but not necessarily on GT (e.g. ParallelFor called from GT).
	*/
	ENGINE_API void RemoveAllMipLevelChangeCallbacks();

	/**
	* Invoke registered mip level change callbacks.
	* @param DeferredTickCBAssets Non-null if not called on GT. An array that collects assets with CBs that need to be called later on GT
	*/
	ENGINE_API void TickMipLevelChangeCallbacks(TArray<UStreamableRenderAsset*>* DeferredTickCBAssets);

	/**
	* Tells the streaming system that it should force all mip-levels to be resident for a number of seconds.
	* @param Seconds					Duration in seconds
	* @param CinematicTextureGroups	Bitfield indicating which texture groups that use extra high-resolution mips
	*/
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	ENGINE_API void SetForceMipLevelsToBeResident(float Seconds, int32 CinematicLODGroupMask = 0);

	/**
	* Returns the cached combined LOD bias based on texture LOD group and LOD bias.
	* @return	LOD bias
	*/
	ENGINE_API int32 GetCachedLODBias() const 
	{ 
		return CachedCombinedLODBias; 
	}

	/** Return the streaming state of the render resources. Mirrors the state and lowers cache misses. Cleared if there are no resources. */
	FORCEINLINE const FStreamableRenderResourceState& GetStreamableResourceState() const 
	{ 
		return CachedSRRState; 
	}

	/** Whether the current asset render resource is streamable from a gamethread timeline.*/
	FORCEINLINE bool RenderResourceSupportsStreaming() const { return CachedSRRState.bSupportsStreaming && CachedSRRState.MaxNumLODs > CachedSRRState.NumNonStreamingLODs; }

	/** Whether the current asset render resource is streamable from a gamethread timeline.*/
	FORCEINLINE bool IsStreamable() const { return StreamingIndex != INDEX_NONE; }

	/** Links texture to the texture streaming manager. */
	ENGINE_API void LinkStreaming();
	/** Unlinks texture from the texture streaming manager. */
	ENGINE_API void UnlinkStreaming();

	/** Whether all miplevels of this texture have been fully streamed in, LOD settings permitting. Note that if optional mips are not mounted it will always return false. */
	ENGINE_API bool IsFullyStreamedIn();

	FORCEINLINE int32 GetStreamingIndex() const { return StreamingIndex; }

	/** Wait for any pending operation (like InitResource and Streaming) to complete. */
	ENGINE_API void WaitForPendingInitOrStreaming(bool bWaitForLODTransition = false, bool bSendCompletionEvents = false);

	/** Wait for any pending operation and make sure that the asset streamer has performed requested ops on this asset. */
	ENGINE_API void WaitForStreaming(bool bWaitForLODTransition = false, bool bSendCompletionEvents = false);

	void TickStreaming(bool bSendCompletionEvents = false, TArray<UStreamableRenderAsset*>* DeferredTickCBAssets = nullptr);

	virtual EStreamableRenderAssetType GetRenderAssetType() const { return EStreamableRenderAssetType::None; }

	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;

	const FPerQualityLevelInt& GetNoRefStreamingLODBias() const
	{
		return NoRefStreamingLODBias;
	}

	void SetNoRefStreamingLODBias(FPerQualityLevelInt NewValue)
	{
		NoRefStreamingLODBias = MoveTemp(NewValue);
	}

	ENGINE_API int32 GetCurrentNoRefStreamingLODBias() const;

protected:
	
	// Also returns false if the render resource is non existent, to prevent stalling on an event that will never complete.
	virtual bool HasPendingRenderResourceInitialization() const { return false; }

	virtual bool HasPendingLODTransition() const { return false; }

	struct FLODStreamingCallbackPayload
	{
		UPrimitiveComponent* Component;
		double Deadline;
		int32 ExpectedResidentMips;
		bool bOnStreamIn;
		FLODStreamingCallback Callback;

		FLODStreamingCallbackPayload(UPrimitiveComponent* InComponent, double InDeadline, int32 InExpectedResidentMips, bool bInOnStreamIn, FLODStreamingCallback&& InCallback)
			: Component(InComponent)
			, Deadline(InDeadline)
			, ExpectedResidentMips(InExpectedResidentMips)
			, bOnStreamIn(bInOnStreamIn)
			, Callback(MoveTemp(InCallback))
		{}
	};

	TArray<FLODStreamingCallbackPayload> MipChangeCallbacks;

	/** An update structure used to handle streaming strategy. */
	TRefCountPtr<class FRenderAssetUpdate> PendingUpdate;

	/** WorldSettings timestamp that tells the streamer to force all miplevels to be resident up until that time. */
	UPROPERTY(transient)
	double ForceMipLevelsToBeResidentTimestamp;

public:
	/** Number of mip-levels to use for cinematic quality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LevelOfDetail, AdvancedDisplay)
	int32 NumCinematicMipLevels;

protected:
	UPROPERTY()
	FPerQualityLevelInt NoRefStreamingLODBias;

	/** FStreamingRenderAsset index used by the texture streaming system. */
	UPROPERTY(transient, duplicatetransient, NonTransactional)
	int32 StreamingIndex = INDEX_NONE;

	/** Cached combined group and texture LOD bias to use.	*/
	UPROPERTY(transient)
	int32 CachedCombinedLODBias;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LevelOfDetail, AssetRegistrySearchable, AdvancedDisplay)
	uint8 NeverStream : 1;

	/** Global and serialized version of ForceMiplevelsToBeResident.				*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LevelOfDetail, meta = (DisplayName = "Global Force Resident Mip Levels"), AdvancedDisplay)
	uint8 bGlobalForceMipLevelsToBeResident : 1;

	/** Whether some mips might be streamed soon. If false, the texture is not planned resolution will be stable. */
	UPROPERTY(transient, NonTransactional)
	uint8 bHasStreamingUpdatePending : 1;

	/** Override whether to fully stream even if texture hasn't been rendered.	*/
	UPROPERTY(transient)
	uint8 bForceMiplevelsToBeResident : 1;

	/** When forced fully resident, ignores the streaming mip bias used to accommodate memory constraints. */
	UPROPERTY(transient)
	uint8 bIgnoreStreamingMipBias : 1;

protected:
	/** Whether to use the extra cinematic quality mip-levels, when we're forcing mip-levels to be resident. */
	UPROPERTY(transient)
	uint8 bUseCinematicMipLevels : 1;

	/** Gamethread coherent state of the resource. Updated whenever graphic resources would from enqueued render commands. */
	FStreamableRenderResourceState CachedSRRState;

	friend struct FRenderAssetStreamingManager;
	friend struct FStreamingRenderAsset;
	friend class Nanite::FCoarseMeshStreamingManager;
};
