// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#pragma once

// Dependencies.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "Engine/EngineTypes.h"
#include "Templates/PimplPtr.h"
#include "Templates/RefCounting.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "SceneTypes.h"
#include "UniformBuffer.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureLayout3d.h"
#include "ScenePrivateBase.h"
#include "RenderTargetPool.h"
#include "SceneCore.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "ShadowRendering.h"
#include "TextureLayout.h"
#include "SceneRendering.h"
#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "VolumeRendering.h"
#include "CommonRenderResources.h"
#include "VisualizeTexture.h"
#include "UnifiedBuffer.h"
#include "LightMapDensityRendering.h"
#include "VolumetricFogShared.h"
#include "DebugViewModeRendering.h"
#include "PrecomputedVolumetricLightmap.h"
#include "RayTracing/RaytracingOptions.h"
#if RHI_RAYTRACING
#include "RayTracing/RayTracingIESLightProfiles.h"
#include "RayTracing/RayTracingScene.h"
#endif
#include "Nanite/Nanite.h"
#include "Lumen/LumenViewState.h"
#include "VolumetricRenderTargetViewStateData.h"
#include "GPUScene.h"
#include "DynamicBVH.h"
#include "PrimitiveInstanceUpdateCommand.h"
#include "OIT/OIT.h"
#include "ShadingEnergyConservation.h"
#include "Experimental/Containers/RobinHoodHashTable.h"
#include "SpanAllocator.h"

/** Factor by which to grow occlusion tests **/
#define OCCLUSION_SLOP (1.0f)

/** Extern GPU stats (used in multiple modules) **/
DECLARE_GPU_STAT_NAMED_EXTERN(ShadowProjection, TEXT("Shadow Projection"));

class AWorldSettings;
class FMaterialParameterCollectionInstanceResource;
class FPrecomputedLightVolume;
class FScene;
class UDecalComponent;
class UExponentialHeightFogComponent;
class ULightComponent;
class UPlanarReflectionComponent;
class UPrimitiveComponent;
class UInstancedStaticMeshComponent;
class UReflectionCaptureComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureCube;
class UWindDirectionalSourceComponent;
class FRHIGPUBufferReadback;
class FRHIGPUTextureReadback;
class FRuntimeVirtualTextureSceneProxy;
class FLumenSceneData;
class FVirtualShadowMapArrayCacheManager;
struct FHairStrandsInstance;
struct FPathTracingState;

/** Holds information about a single primitive's occlusion. */
class FPrimitiveOcclusionHistory
{
public:
	/** The primitive the occlusion information is about. */
	FPrimitiveComponentId PrimitiveId;

	/** The occlusion query which contains the primitive's pending occlusion results. */
	FRHIRenderQuery* PendingOcclusionQuery[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];
	uint32 PendingOcclusionQueryFrames[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames]; 

	uint32 LastTestFrameNumber;
	uint32 LastConsideredFrameNumber;
	uint32 HZBTestIndex;

	/** The last time the primitive was visible. */
	float LastProvenVisibleTime;

	/** The last time the primitive was in the view frustum. */
	float LastConsideredTime;

	/** 
	 *	The pixels that were rendered the last time the primitive was drawn.
	 *	It is the ratio of pixels unoccluded to the resolution of the scene.
	 */
	float LastPixelsPercentage;

	/**
	* For things that have subqueries (foliage), this is the non-zero
	*/
	int32 CustomIndex;

	/** When things first become eligible for occlusion, then might be sweeping into the frustum, we are going to leave them at visible for a few frames, then start real queries.  */
	uint8 BecameEligibleForQueryCooldown : 6;

	uint8 WasOccludedLastFrame : 1;
	uint8 OcclusionStateWasDefiniteLastFrame : 1;

	/** whether or not this primitive was grouped the last time it was queried */
	bool bGroupedQuery[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];

private:
	/**
	 *	Whether or not we need to linearly search the history for a past entry. Scanning may be necessary if for every frame there
	 *	is a hole in PendingOcclusionQueryFrames in the same spot (ex. if for every frame PendingOcclusionQueryFrames[1] is null).
	 *	This could lead to overdraw for the frames that attempt to read these holes by getting back nothing every time.
	 *	This can occur when round robin occlusion queries are turned on while NumBufferedFrames is even.
	 */
	bool bNeedsScanOnRead;

	/**
	 *	Scan for the oldest non-stale (<= LagTolerance frames old) in the occlusion history by examining their corresponding frame numbers.
	 *	Conditions where this is needed to get a query for read-back are described for bNeedsScanOnRead.
	 *	Returns -1 if no such query exists in the occlusion history.
	 */
	inline int32 ScanOldestNonStaleQueryIndex(uint32 FrameNumber, int32 NumBufferedFrames, int32 LagTolerance) const
	{
		uint32 OldestFrame = UINT32_MAX;
		int32 OldestQueryIndex = -1;
		for (int Index = 0; Index < NumBufferedFrames; ++Index)
		{
			const uint32 ThisFrameNumber = PendingOcclusionQueryFrames[Index];
			const int32 LaggedFrames = FrameNumber - ThisFrameNumber;
			// Queries older than LagTolerance are invalid. They may have already been reused and will give incorrect results if read
			if (PendingOcclusionQuery[Index] && LaggedFrames <= LagTolerance && ThisFrameNumber < OldestFrame)
			{
				OldestFrame = ThisFrameNumber;
				OldestQueryIndex = Index;
			}
		}
		return OldestQueryIndex;
	}

public:
	/** Initialization constructor. */
	inline FPrimitiveOcclusionHistory(FPrimitiveComponentId InPrimitiveId, int32 SubQuery)
		: PrimitiveId(InPrimitiveId)
		, LastTestFrameNumber(~0u)
		, LastConsideredFrameNumber(~0u)
		, HZBTestIndex(0)
		, LastProvenVisibleTime(0.0f)
		, LastConsideredTime(0.0f)
		, LastPixelsPercentage(0.0f)
		, CustomIndex(SubQuery)
		, BecameEligibleForQueryCooldown(0)
		, WasOccludedLastFrame(false)
		, OcclusionStateWasDefiniteLastFrame(false)
		, bNeedsScanOnRead(false)
	{
		for (int32 Index = 0; Index < FOcclusionQueryHelpers::MaxBufferedOcclusionFrames; Index++)
		{
			PendingOcclusionQuery[Index] = nullptr;
			PendingOcclusionQueryFrames[Index] = 0;
			bGroupedQuery[Index] = false;
		}
	}

	inline FPrimitiveOcclusionHistory()
		: LastTestFrameNumber(~0u)
		, LastConsideredFrameNumber(~0u)
		, HZBTestIndex(0)
		, LastProvenVisibleTime(0.0f)
		, LastConsideredTime(0.0f)
		, LastPixelsPercentage(0.0f)
		, CustomIndex(0)
		, BecameEligibleForQueryCooldown(0)
		, WasOccludedLastFrame(false)
		, OcclusionStateWasDefiniteLastFrame(false)
		, bNeedsScanOnRead(false)
	{
		for (int32 Index = 0; Index < FOcclusionQueryHelpers::MaxBufferedOcclusionFrames; Index++)
		{
			PendingOcclusionQuery[Index] = nullptr;
			PendingOcclusionQueryFrames[Index] = 0;
			bGroupedQuery[Index] = false;
		}
	}

	inline void ReleaseStaleQueries(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		// No need to release. FFrameBasedOcclusionQueryPool automatically reuses stale queries
	}

	inline void ReleaseQuery(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		// No need to release. FFrameBasedOcclusionQueryPool automatically reuses stale queries
	}

	inline FRHIRenderQuery* GetQueryForEviction(uint32 FrameNumber, int32 NumBufferedFrames) const
	{
		// No need to release. FFrameBasedOcclusionQueryPool automatically reuses stale queries
		return nullptr;
	}


	inline FRHIRenderQuery* GetQueryForReading(uint32 FrameNumber, int32 NumBufferedFrames, int32 LagTolerance, bool& bOutGrouped) const
	{
		const int32 OldestQueryIndex = bNeedsScanOnRead ? ScanOldestNonStaleQueryIndex(FrameNumber, NumBufferedFrames, LagTolerance)
														: FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		const int32 LaggedFrames = FrameNumber - PendingOcclusionQueryFrames[OldestQueryIndex];
		// Nenever read from queries are older than LagTolerance. They may have already been reused and will give incorrect results
		if (OldestQueryIndex == -1 || !PendingOcclusionQuery[OldestQueryIndex] || LaggedFrames > LagTolerance)
		{
			bOutGrouped = false;
			return nullptr;
		}
		bOutGrouped = bGroupedQuery[OldestQueryIndex];
		return PendingOcclusionQuery[OldestQueryIndex];
	}

	inline void SetCurrentQuery(uint32 FrameNumber, FRHIRenderQuery* NewQuery, int32 NumBufferedFrames, bool bGrouped, bool bNeedsScan)
	{
		// Get the current occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex] = NewQuery;
		PendingOcclusionQueryFrames[QueryIndex] = FrameNumber;
		bGroupedQuery[QueryIndex] = bGrouped;

		bNeedsScanOnRead = bNeedsScan;
	}

	inline uint32 LastQuerySubmitFrame() const
	{
		uint32 Result = 0;

		for (int32 Index = 0; Index < FOcclusionQueryHelpers::MaxBufferedOcclusionFrames; Index++)
		{
			if (!bGroupedQuery[Index])
			{
				Result = FMath::Max(Result, PendingOcclusionQueryFrames[Index]);
			}
		}

		return Result;
	}
};

struct FPrimitiveOcclusionHistoryKey
{
	FPrimitiveComponentId PrimitiveId;
	int32 CustomIndex;

	FPrimitiveOcclusionHistoryKey(const FPrimitiveOcclusionHistory& Element)
		: PrimitiveId(Element.PrimitiveId)
		, CustomIndex(Element.CustomIndex)
	{
	}
	FPrimitiveOcclusionHistoryKey(FPrimitiveComponentId InPrimitiveId, int32 InCustomIndex)
		: PrimitiveId(InPrimitiveId)
		, CustomIndex(InCustomIndex)
	{
	}
};

/** Defines how the hash set indexes the FPrimitiveOcclusionHistory objects. */
struct FPrimitiveOcclusionHistoryKeyFuncs : BaseKeyFuncs<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKey>
{
	typedef FPrimitiveOcclusionHistoryKey KeyInitType;

	static KeyInitType GetSetKey(const FPrimitiveOcclusionHistory& Element)
	{
		return FPrimitiveOcclusionHistoryKey(Element);
	}

	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A.PrimitiveId == B.PrimitiveId && A.CustomIndex == B.CustomIndex;
	}

	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key.PrimitiveId.PrimIDValue) ^ (GetTypeHash(Key.CustomIndex) >> 20);
	}
};


class FIndividualOcclusionHistory
{
	FRHIPooledRenderQuery PendingOcclusionQuery[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames];
	uint32 PendingOcclusionQueryFrames[FOcclusionQueryHelpers::MaxBufferedOcclusionFrames]; // not initialized...this is ok

public:

	inline void ReleaseStaleQueries(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		for (uint32 DeltaFrame = NumBufferedFrames; DeltaFrame > 0; DeltaFrame--)
		{
			if (FrameNumber >= (DeltaFrame - 1))
			{
				uint32 TestFrame = FrameNumber - (DeltaFrame - 1);
				const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(TestFrame, NumBufferedFrames);
				if (PendingOcclusionQueryFrames[QueryIndex] != TestFrame)
				{
					PendingOcclusionQuery[QueryIndex].ReleaseQuery();
				}
			}
		}
	}
	inline void ReleaseQuery(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex].ReleaseQuery();
	}

	inline FRHIRenderQuery* GetPastQuery(uint32 FrameNumber, int32 NumBufferedFrames)
	{
		// Get the oldest occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryLookupIndex(FrameNumber, NumBufferedFrames);
		if (PendingOcclusionQuery[QueryIndex].GetQuery() && PendingOcclusionQueryFrames[QueryIndex] == FrameNumber - uint32(NumBufferedFrames))
		{
			return PendingOcclusionQuery[QueryIndex].GetQuery();
		}
		return nullptr;
	}

	inline void SetCurrentQuery(uint32 FrameNumber, FRHIPooledRenderQuery&& NewQuery, int32 NumBufferedFrames)
	{
		// Get the current occlusion query
		const uint32 QueryIndex = FOcclusionQueryHelpers::GetQueryIssueIndex(FrameNumber, NumBufferedFrames);
		PendingOcclusionQuery[QueryIndex] = MoveTemp(NewQuery);
		PendingOcclusionQueryFrames[QueryIndex] = FrameNumber;
	}
};

/**
 * Distance cull fading uniform buffer containing fully faded in.
 */
class FGlobalDistanceCullFadeUniformBuffer : public TUniformBuffer< FDistanceCullFadeUniformShaderParameters >
{
public:
	/** Default constructor. */
	FGlobalDistanceCullFadeUniformBuffer()
	{
		FDistanceCullFadeUniformShaderParameters Parameters;
		Parameters.FadeTimeScaleBias.X = 0.0f;
		Parameters.FadeTimeScaleBias.Y = 1.0f;
		SetContents(Parameters);
	}
};

/** Global primitive uniform buffer resource containing faded in */
extern TGlobalResource< FGlobalDistanceCullFadeUniformBuffer > GDistanceCullFadedInUniformBuffer;

/**
 * Dither uniform buffer containing fully faded in.
 */
class FGlobalDitherUniformBuffer : public TUniformBuffer< FDitherUniformShaderParameters >
{
public:
	/** Default constructor. */
	FGlobalDitherUniformBuffer()
	{
		FDitherUniformShaderParameters Parameters;
		Parameters.LODFactor = 0.0f;
		SetContents(Parameters);
	}
};

/** Global primitive uniform buffer resource containing faded in */
extern TGlobalResource< FGlobalDitherUniformBuffer > GDitherFadedInUniformBuffer;

/**
 * Stores fading state for a single primitive in a single view
 */
class FPrimitiveFadingState
{
public:
	FPrimitiveFadingState()
		: FadeTimeScaleBias(ForceInitToZero)
		, FrameNumber(0)
		, EndTime(0.0f)
		, bIsVisible(false)
		, bValid(false)
	{
	}

	/** Scale and bias to use on time to calculate fade opacity */
	FVector2D FadeTimeScaleBias;

	/** The uniform buffer for the fade parameters */
	FDistanceCullFadeUniformBufferRef UniformBuffer;

	/** Frame number when last updated */
	uint32 FrameNumber;

	/** Time when fade will be finished. */
	float EndTime;

	/** Currently visible? */
	bool bIsVisible;

	/** Valid? */
	bool bValid;
};

class FGlobalDistanceFieldCacheTypeState
{
public:
	TArray<FRenderBounds> PrimitiveModifiedBounds;
};

class FGlobalDistanceFieldClipmapState
{
public:

	FGlobalDistanceFieldClipmapState()
	{
		FullUpdateOriginInPages = FIntVector::ZeroValue;
		LastPartialUpdateOriginInPages = FIntVector::ZeroValue;
		CachedClipmapCenter = FVector3f(0.0f, 0.0f, 0.0f);
		CachedClipmapExtent = 0.0f;
		CacheClipmapInfluenceRadius = 0.0f;
		CacheMostlyStaticSeparately = 1;
		LastUsedSceneDataForFullUpdate = nullptr;
	}

	FIntVector FullUpdateOriginInPages;
	FIntVector LastPartialUpdateOriginInPages;
	uint32 CacheMostlyStaticSeparately;

	FVector3f CachedClipmapCenter;
	float CachedClipmapExtent;
	float CacheClipmapInfluenceRadius;

	FGlobalDistanceFieldCacheTypeState Cache[GDF_Num];

	// Used to perform a full update of the clip map when the scene data changes
	const class FDistanceFieldSceneData* LastUsedSceneDataForFullUpdate;
};

/** Maps a single primitive to it's per-view fading state data */
typedef TMap<FPrimitiveComponentId, FPrimitiveFadingState> FPrimitiveFadingStateMap;

class FOcclusionRandomStream
{
	enum {NumSamples = 3571};
public:

	/** Default constructor - should set seed prior to use. */
	FOcclusionRandomStream()
		: CurrentSample(0)
	{
		FRandomStream RandomStream(0x83246);
		for (int32 Index = 0; Index < NumSamples; Index++)
		{
			Samples[Index] = RandomStream.GetFraction();
		}
		Samples[0] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[NumSamples/3] = 0.0f; // we want to make sure we have at least a few zeros
		Samples[(NumSamples*2)/3] = 0.0f; // we want to make sure we have at least a few zeros
	}

	/** @return A random number between 0 and 1. */
	inline float GetFraction()
	{
		if (CurrentSample >= NumSamples)
		{
			CurrentSample = 0;
		}
		return Samples[CurrentSample++];
	}
private:

	/** Index of the last sample we produced **/
	uint32 CurrentSample;
	/** A list of float random samples **/
	float Samples[NumSamples];
};

/** Random table for occlusion **/
extern FOcclusionRandomStream GOcclusionRandomStream;


/** HLOD tree persistent fading and visibility state */
class FHLODVisibilityState
{
public:
	FHLODVisibilityState()
		: TemporalLODSyncTime(0.0f)
		, FOVDistanceScaleSq(1.0f)
		, UpdateCount(0)
	{}

	bool IsNodeFading(const int32 PrimIndex) const
	{
		checkSlow(IsValidPrimitiveIndex(PrimIndex));
		return PrimitiveFadingLODMap[PrimIndex];
	}

	bool IsNodeFadingOut(const int32 PrimIndex) const
	{
		checkSlow(IsValidPrimitiveIndex(PrimIndex));
		return PrimitiveFadingOutLODMap[PrimIndex];
	}

	bool IsNodeForcedVisible(const int32 PrimIndex) const
	{
		checkSlow(IsValidPrimitiveIndex(PrimIndex));
		return  ForcedVisiblePrimitiveMap[PrimIndex];
	}

	bool IsNodeForcedHidden(const int32 PrimIndex) const
	{
		checkSlow(IsValidPrimitiveIndex(PrimIndex));
		return ForcedHiddenPrimitiveMap[PrimIndex];
	}

	bool IsValidPrimitiveIndex(const int32 PrimIndex) const
	{
		return ForcedHiddenPrimitiveMap.IsValidIndex(PrimIndex);
	}

	TBitArray<>	PrimitiveFadingLODMap;
	TBitArray<>	PrimitiveFadingOutLODMap;
	TBitArray<>	ForcedVisiblePrimitiveMap;
	TBitArray<>	ForcedHiddenPrimitiveMap;
	float		TemporalLODSyncTime;
	float		FOVDistanceScaleSq;
	uint16		UpdateCount;
};

/** HLOD scene node persistent fading and visibility state */
struct FHLODSceneNodeVisibilityState
{
	FHLODSceneNodeVisibilityState()
		: UpdateCount(0)
		, bWasVisible(0)
		, bIsVisible(0)
		, bIsFading(0)
	{}

	/** Last updated FrameCount */
	uint16 UpdateCount;

	/** Persistent visibility states */
	uint16 bWasVisible	: 1;
	uint16 bIsVisible	: 1;
	uint16 bIsFading	: 1;
};

struct FShaderPrintStateData
{
	TRefCountPtr<FRDGPooledBuffer> StateBuffer;
	TRefCountPtr<FRDGPooledBuffer> EntryBuffer;
	FVector PreViewTranslation = FVector::ZeroVector;
	bool bIsLocked = false;

	void Release()
	{
		bIsLocked = false;
		PreViewTranslation = FVector::ZeroVector;
		StateBuffer = nullptr;
		EntryBuffer = nullptr;
	}
};

// Some resources used across frames can prevent execution of PS, CS and VS work across overlapping frames work.
// This struct is used to transparently double buffer the sky aerial perspective volume on some platforms,
// in order to make sure two consecutive frames have no resource dependencies, resulting in no cross frame barrier/sync point.
struct FPersistentSkyAtmosphereData
{
	FPersistentSkyAtmosphereData();

	void InitialiseOrNextFrame(ERHIFeatureLevel::Type FeatureLevel, FPooledRenderTargetDesc& AerialPerspectiveDesc, FRHICommandListImmediate& RHICmdList);

	TRefCountPtr<IPooledRenderTarget> GetCurrentCameraAerialPerspectiveVolume();

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

private:
	bool bInitialised;
	int32 CurrentScreenResolution;
	int32 CurrentDepthResolution;
	EPixelFormat CurrentTextureAerialLUTFormat;
	TRefCountPtr<IPooledRenderTarget> CameraAerialPerspectiveVolumes[2];
	uint8 CameraAerialPerspectiveVolumeCount;
	uint8 CameraAerialPerspectiveVolumeIndex;
};

struct FPersistentGlobalDistanceFieldData : public FThreadSafeRefCountedObject
{
	// Array of ClipmapIndex
	TArray<int32> DeferredUpdates[GDF_Num];

	int32	UpdateFrame = 0;
	bool	bFirstFrame = true;

	bool	bInitializedOrigins = false;
	bool	bPendingReset = false;
	FGlobalDistanceFieldClipmapState ClipmapState[GlobalDistanceField::MaxClipmaps];
	int32	UpdateIndex = 0;
	FVector	CameraVelocityOffset = FVector(0);
	bool	bUpdateViewOrigin = true;
	FVector	LastViewOrigin = FVector(0);
#if WITH_MGPU
	FRHIGPUMask LastGPUMask;
#endif

	TRefCountPtr<FRDGPooledBuffer> PageFreeListAllocatorBuffer;
	TRefCountPtr<FRDGPooledBuffer> PageFreeListBuffer;
	TRefCountPtr<IPooledRenderTarget> PageAtlasTexture;
	TRefCountPtr<IPooledRenderTarget> CoverageAtlasTexture;
	TRefCountPtr<FRDGPooledBuffer> PageObjectGridBuffer;
	TRefCountPtr<IPooledRenderTarget> PageTableCombinedTexture;
	TRefCountPtr<IPooledRenderTarget> PageTableLayerTextures[GDF_Num];
	TRefCountPtr<IPooledRenderTarget> MipTexture;

	uint64 GetGPUSizeBytes(bool bLogSizes) const;
};

/**
 * The scene manager's private implementation of persistent view state.
 * This class is associated with a particular camera across multiple frames by the game thread.
 * The game thread calls FRendererModule::AllocateViewState to create an instance of this private implementation.
 */
class FSceneViewState : public FSceneViewStateInterface, public FRenderResource
{
public:

	class FProjectedShadowKey
	{
	public:

		inline bool operator == (const FProjectedShadowKey &Other) const
		{
			return (PrimitiveId == Other.PrimitiveId && Light == Other.Light && ShadowSplitIndex == Other.ShadowSplitIndex && bTranslucentShadow == Other.bTranslucentShadow);
		}

		FProjectedShadowKey(const FProjectedShadowInfo& ProjectedShadowInfo)
			: PrimitiveId(ProjectedShadowInfo.GetParentSceneInfo() ? ProjectedShadowInfo.GetParentSceneInfo()->PrimitiveComponentId : FPrimitiveComponentId())
			, Light(ProjectedShadowInfo.GetLightSceneInfo().Proxy->GetLightComponent())
			, ShadowSplitIndex(ProjectedShadowInfo.CascadeSettings.ShadowSplitIndex)
			, bTranslucentShadow(ProjectedShadowInfo.bTranslucentShadow)
		{
		}

		FProjectedShadowKey(FPrimitiveComponentId InPrimitiveId, const ULightComponent* InLight, int32 InSplitIndex, bool bInTranslucentShadow)
			: PrimitiveId(InPrimitiveId)
			, Light(InLight)
			, ShadowSplitIndex(InSplitIndex)
			, bTranslucentShadow(bInTranslucentShadow)
		{
		}

		friend inline uint32 GetTypeHash(const FSceneViewState::FProjectedShadowKey& Key)
		{
			return PointerHash(Key.Light,GetTypeHash(Key.PrimitiveId));
		}

	private:
		FPrimitiveComponentId PrimitiveId;
		const ULightComponent* Light;
		int32 ShadowSplitIndex;
		bool bTranslucentShadow;
	};

	uint32 UniqueID;

	/**
	 * The scene pointer may be NULL -- it's filled in by certain API calls that require a FSceneViewState and FScene to know about each other,
	 * such as FSceneViewState::AddVirtualShadowMapCache.  Whenever a ViewState and Scene get linked, this pointer is set, and a pointer
	 * to the ViewState is added to an array in the Scene.  The linking is necessary in cases where incremental FScene updates need to be
	 * reflected in cached data stored in FSceneViewState.
	 */
	FScene* Scene;

	typedef TMap<FSceneViewState::FProjectedShadowKey, FRHIPooledRenderQuery> ShadowKeyOcclusionQueryMap;
	TArray<ShadowKeyOcclusionQueryMap, TInlineAllocator<FOcclusionQueryHelpers::MaxBufferedOcclusionFrames> > ShadowOcclusionQueryMaps;

	/** The view's occlusion query pool. */
	FRenderQueryPoolRHIRef OcclusionQueryPool;
	FFrameBasedOcclusionQueryPool PrimitiveOcclusionQueryPool;

	FHZBOcclusionTester HZBOcclusionTests;

	FPersistentSkyAtmosphereData PersistentSkyAtmosphereData;

	/** Storage to which compressed visibility chunks are uncompressed at runtime. */
	TArray<uint8> DecompressedVisibilityChunk;

	/** Cached visibility data from the last call to GetPrecomputedVisibilityData. */
	const TArray<uint8>* CachedVisibilityChunk;
	int32 CachedVisibilityHandlerId;
	int32 CachedVisibilityBucketIndex;
	int32 CachedVisibilityChunkIndex;

	uint32		PendingPrevFrameNumber;
	uint32		PrevFrameNumber;
	float		LastRenderTime;
	float		LastRenderTimeDelta;
	float		MotionBlurTimeScale;
	float		MotionBlurTargetDeltaTime;
	FMatrix44f	PrevViewMatrixForOcclusionQuery;
	FVector		PrevViewOriginForOcclusionQuery;

#if RHI_RAYTRACING
	/** Number of consecutive frames the camera is static */
	uint32 NumCameraStaticFrames;
	int32 RayTracingNumIterations;
#endif

	// A counter incremented once each time this view is rendered.
	uint32 OcclusionFrameCounter;

	/** Used by states that have IsViewParent() == true to store primitives for child states. */
	TSet<FPrimitiveComponentId> ParentPrimitives;

	/** For this view, the set of primitives that are currently fading, either in or out. */
	FPrimitiveFadingStateMap PrimitiveFadingStates;

	FIndirectLightingCacheAllocation* TranslucencyLightingCacheAllocations[TVC_MAX];

	TMap<int32, FIndividualOcclusionHistory> PlanarReflectionOcclusionHistories;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Are we currently in the state of freezing rendering? (1 frame where we gather what was rendered) */
	uint32 bIsFreezing : 1;

	/** Is rendering currently frozen? */
	uint32 bIsFrozen : 1;

	/** True if the CachedViewMatrices is holding frozen view matrices, otherwise false */
	uint32 bIsFrozenViewMatricesCached : 1;

	/** The set of primitives that were rendered the frame that we froze rendering */
	TSet<FPrimitiveComponentId> FrozenPrimitives;

	/** The cache view matrices at the time of freezing or the cached debug fly cam's view matrices. */
	FViewMatrices CachedViewMatrices;
#endif

	/** HLOD persistent fading and visibility state */
	FHLODVisibilityState HLODVisibilityState;
	TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState> HLODSceneNodeVisibilityStates;

	void UpdatePreExposure(FViewInfo& View);

private:
	/** The current frame PreExposure */
	float PreExposure;

	/** Whether to get the last exposure from GPU */
	bool bUpdateLastExposure;

	// to implement eye adaptation / auto exposure changes over time
	// SM5 and above should use RenderTarget and ES3_1 for mobile should use RWBuffer for read back.
	class FEyeAdaptationManager
	{
	public:
		FEyeAdaptationManager();

		void SafeRelease();

		/** Get the last frame exposure value (used to compute pre-exposure) */
		float GetLastExposure() const { return LastExposure; }

		/** Get the last frame average scene luminance (used for exposure compensation curve) */
		float GetLastAverageSceneLuminance() const { return LastAverageSceneLuminance; }

		const TRefCountPtr<IPooledRenderTarget>& GetCurrentTexture() const
		{
			return GetTexture(CurrentBuffer);
		}

		/** Return current Render Target */
		const TRefCountPtr<IPooledRenderTarget>& GetCurrentTexture(FRHICommandList& RHICmdList)
		{
			return GetOrCreateTexture(RHICmdList, CurrentBuffer);
		}

		/** Reverse the current/last order of the targets */
		void SwapTextures();

		/** Update Last Exposure with the most recent value */
		void UpdateLastExposureFromTexture();

		/** Enqueue a pass to readback current exposure */
		void EnqueueExposureTextureReadback(FRDGBuilder& GraphBuilder);

		const TRefCountPtr<FRDGPooledBuffer>& GetCurrentBuffer() const
		{
			return GetBuffer(CurrentBuffer);
		}

		const TRefCountPtr<FRDGPooledBuffer>& GetCurrentBuffer(FRDGBuilder& GraphBuilder)
		{
			return GetOrCreateBuffer(GraphBuilder, CurrentBuffer);
		}

		void SwapBuffers();
		void UpdateLastExposureFromBuffer();
		void EnqueueExposureBufferReadback(FRDGBuilder& GraphBuilder);

		uint64 GetGPUSizeBytes(bool bLogSizes) const;

	private:
		const TRefCountPtr<IPooledRenderTarget>& GetTexture(uint32 TextureIndex) const;
		const TRefCountPtr<IPooledRenderTarget>& GetOrCreateTexture(FRHICommandList& RHICmdList, uint32 TextureIndex);

		const TRefCountPtr<FRDGPooledBuffer>& GetBuffer(uint32 BufferIndex) const;
		const TRefCountPtr<FRDGPooledBuffer>& GetOrCreateBuffer(FRDGBuilder& GraphBuilder, uint32 BufferIndex);

		FRHIGPUBufferReadback* GetLatestReadbackBuffer();
		FRHIGPUTextureReadback* GetLatestReadbackTexture();

		static const int32 NUM_BUFFERS = 2;

		int32 CurrentBuffer = 0;

		float LastExposure = 0;
		float LastAverageSceneLuminance = 0; // 0 means invalid. Used for Exposure Compensation Curve.

		// Exposure texture/buffer is double buffered
		TRefCountPtr<IPooledRenderTarget> PooledRenderTarget[NUM_BUFFERS];
		TArray<FRHIGPUTextureReadback*> ExposureReadbackTextures;

		// ES3.1 feature level. For efficent readback use buffers instead of textures
		TRefCountPtr<FRDGPooledBuffer> ExposureBufferData[NUM_BUFFERS];
		TArray<FRHIGPUBufferReadback*> ExposureReadbackBuffers;

		static const uint32 MAX_READBACK_BUFFERS = 4;
		uint32 ReadbackBuffersWriteIndex = 0;
		uint32 ReadbackBuffersNumPending = 0;

	} EyeAdaptationManager;

	// The LUT used by tonemapping.  In stereo this is only computed and stored by the Left Eye.
	TRefCountPtr<IPooledRenderTarget> CombinedLUTRenderTarget;

	// LUT is only valid after it has been computed, not on allocation of the RT
	bool bValidTonemappingLUT = false;


	// used by the Postprocess Material Blending system to avoid recreation and garbage collection of MIDs
	TArray<UMaterialInstanceDynamic*> MIDPool;
	uint32 MIDUsedCount;

	// counts up by one each frame, warped in 0..3 range, ResetViewState() puts it back to 0
	int32 DistanceFieldTemporalSampleIndex;

	// whether this view is a stereo counterpart to a primary view
	bool bIsStereoView;

	// The whether or not round-robin occlusion querying is enabled for this view
	bool bRoundRobinOcclusionEnabled;

public:
	
	// if TemporalAA is on this cycles through 0..TemporalAASampleCount-1, ResetViewState() puts it back to 0
	int32 TemporalAASampleIndex;

	// counts up by one each frame, warped in 0..7 range, ResetViewState() puts it back to 0
	uint32 FrameIndex;
	
	/** Informations of to persist for the next frame's FViewInfo::PrevViewInfo.
	 *
	 * Under normal use case (temporal histories are not frozen), this gets cleared after setting FViewInfo::PrevViewInfo
	 * after being copied to FViewInfo::PrevViewInfo. New temporal histories get directly written to it.
	 *
	 * When temporal histories are frozen (pause command, or r.Test.FreezeTemporalHistories), this keeps it's values, and the currently
	 * rendering FViewInfo should not update it. Refer to FViewInfo::bStatePrevViewInfoIsReadOnly.
	 */
	FPreviousViewInfo PrevFrameViewInfo;

	// Temporal AA result for light shafts of last frame
	FTemporalAAHistory LightShaftOcclusionHistory;
	// Temporal AA result for light shafts of last frame
	TMap<const ULightComponent*, TUniquePtr<FTemporalAAHistory> > LightShaftBloomHistoryRTs;

	FIntRect DistanceFieldAOHistoryViewRect;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldAOHistoryRT;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldIrradianceHistoryRT;

	// Burley Subsurface scattering variance texture from the last frame.
	TRefCountPtr<IPooledRenderTarget> SubsurfaceScatteringQualityHistoryRT;

	FLumenViewState Lumen;

	// Pre-computed filter in spectral (i.e. FFT) domain along with data to determine if we need to up date it
	struct {
		/// @cond DOXYGEN_WARNINGS
		void SafeRelease()
		{
			Physical = nullptr;
			PhysicalRHI = nullptr;
			Spectral.SafeRelease();
			ConstantsBuffer.SafeRelease();
		}
		/// @endcond

		// The 2d fourier transform of the physical space texture.
		TRefCountPtr<IPooledRenderTarget> Spectral;
		TRefCountPtr<FRDGPooledBuffer> ConstantsBuffer;

		// The physical space source texture
		UTexture2D* Physical = nullptr;
		FRHITexture* PhysicalRHI = nullptr;

		// The Scale * 100 = percentage of the image space that the physical kernel represents.
		// e.g. Scale = 1 indicates that the physical kernel image occupies the same size 
		// as the image to be processed with the FFT convolution.
		float Scale = 0.f;

		// The size of the viewport for which the spectral kernel was calculated. 
		FIntPoint ImageSize;

		// Mip level of the physical space source texture used when caching the spectral space texture.
		uint32 PhysicalMipLevel;
	} BloomFFTKernel;

	// Film grain
	struct
	{
		/// @cond DOXYGEN_WARNINGS
		void SafeRelease()
		{
			Texture = nullptr;
			TextureRHI = nullptr;
			ConstantsBuffer.SafeRelease();
		}
		/// @endcond

		UTexture2D* Texture = nullptr;
		FRHITexture* TextureRHI = nullptr;
		TRefCountPtr<FRDGPooledBuffer> ConstantsBuffer;
	} FilmGrainCache;

	// Cached material texture samplers
	float MaterialTextureCachedMipBias;
	float LandscapeCachedMipBias;
	FSamplerStateRHIRef MaterialTextureBilinearWrapedSamplerCache;
	FSamplerStateRHIRef MaterialTextureBilinearClampedSamplerCache;
	FSamplerStateRHIRef LandscapeWeightmapSamplerCache;

#if RHI_RAYTRACING
	// Invalidates cached results related to the path tracer so accumulated rendering can start over
	void PathTracingInvalidate(bool InvalidateAnimationStates = true);
	virtual uint32 GetPathTracingSampleIndex() const override;
	virtual uint32 GetPathTracingSampleCount() const override;

	// Keeps track of the internal path tracer state
	TPimplPtr<FPathTracingState> PathTracingState;
	uint32 PathTracingInvalidationCounter = 0;

	// IES light profiles
	FIESLightProfileResource IESLightProfileResources;

	// Ray Traced Reflection Imaginary GBuffer Data containing a pseudo-geometric representation of the reflected surface(s)
	TRefCountPtr<IPooledRenderTarget> ImaginaryReflectionGBufferA;
	TRefCountPtr<IPooledRenderTarget> ImaginaryReflectionDepthZ;
	TRefCountPtr<IPooledRenderTarget> ImaginaryReflectionVelocity;

	// Ray Traced Sky Light Sample Direction Data
	TRefCountPtr<FRDGPooledBuffer> SkyLightVisibilityRaysBuffer;
	FIntVector SkyLightVisibilityRaysDimensions;

	// Ray Traced Global Illumination Gather Point Data
	TRefCountPtr<FRDGPooledBuffer> GatherPointsBuffer;
	FIntVector GatherPointsResolution;
	// todo: shared definition for maximum gather points per-pixel (32)
	TStaticArray<FMatrix, 32> GatherPointsViewHistory;
	uint32 GatherPointsCount;

	// Last valid RTPSO is saved, so it could be used as fallback in future frames if background PSO compilation is enabled.
	// This RTPSO can be used only if the only difference from previous PSO is the material hit shaders.
	FRayTracingPipelineStateSignature LastRayTracingMaterialPipelineSignature;
#endif

	TUniquePtr<FForwardLightingViewResources> ForwardLightingResources;

	float LightScatteringHistoryPreExposure;
	TRefCountPtr<IPooledRenderTarget> LightScatteringHistory;
	TRefCountPtr<IPooledRenderTarget> PrevLightScatteringConservativeDepthTexture;

	/** Potentially shared to save memory in cases where multiple view states share a common origin, such as cube map capture faces. */
	TRefCountPtr<FPersistentGlobalDistanceFieldData> GlobalDistanceFieldData;

	/** This is float since it is derived off of UWorld::RealTimeSeconds, which is relative to BeginPlay time. */
	float LastAutoDownsampleChangeTime;
	float SmoothedHalfResTranslucencyGPUDuration;
	float SmoothedFullResTranslucencyGPUDuration;

	/** Current desired state of auto-downsampled separate translucency for this view. */
	bool bShouldAutoDownsampleTranslucency;

	// Is DOFHistoryRT set from DepthOfField?
	bool bDOFHistory;
	// Is DOFHistoryRT2 set from DepthOfField?
	bool bDOFHistory2;

	// Sequencer state for view management
	ESequencerState SequencerState;

	FTemporalLODState TemporalLODState;

	FVolumetricRenderTargetViewStateData VolumetricCloudRenderTarget;
	FTemporalRenderTargetState VolumetricCloudShadowRenderTarget[NUM_ATMOSPHERE_LIGHTS];
	FMatrix VolumetricCloudShadowmapPreviousTranslatedWorldToLightClipMatrix[NUM_ATMOSPHERE_LIGHTS];
	FVector VolumetricCloudShadowmapPreviousAtmosphericLightPos[NUM_ATMOSPHERE_LIGHTS];
	FVector VolumetricCloudShadowmapPreviousAnchorPoint[NUM_ATMOSPHERE_LIGHTS];
	FVector VolumetricCloudShadowmapPreviousAtmosphericLightDir[NUM_ATMOSPHERE_LIGHTS];

	FHairStrandsViewStateData HairStrandsViewStateData;

	FShaderPrintStateData ShaderPrintStateData;

	FShadingEnergyConservationStateData ShadingEnergyConservationData;

	bool bVirtualShadowMapCacheAdded;
	bool bLumenSceneDataAdded;
	float LumenSurfaceCacheResolution;

	FVirtualShadowMapArrayCacheManager* ViewVirtualShadowMapCache;

	// call after OnFrameRenderingSetup()
	virtual uint32 GetCurrentTemporalAASampleIndex() const
	{
		return TemporalAASampleIndex;
	}

	// Returns the index of the frame with a desired power of two modulus.
	inline uint32 GetFrameIndex(uint32 Pow2Modulus) const
	{
		check(FMath::IsPowerOfTwo(Pow2Modulus));
		return FrameIndex & (Pow2Modulus - 1);
	}

	// Returns 32bits frame index.
	inline uint32 GetFrameIndex() const
	{
		return FrameIndex;
	}

	// to make rendering more deterministic
	virtual void ResetViewState()
	{
		TemporalAASampleIndex = 0;
		FrameIndex = 0;
		DistanceFieldTemporalSampleIndex = 0;
		PreExposure = 1.f;

		ReleaseDynamicRHI();
	}

	void SetupDistanceFieldTemporalOffset(const FSceneViewFamily& Family)
	{
		if (!Family.bWorldIsPaused)
		{
			DistanceFieldTemporalSampleIndex++;
		}

		if(DistanceFieldTemporalSampleIndex >= 4)
		{
			DistanceFieldTemporalSampleIndex = 0;
		}
	}

	uint32 GetDistanceFieldTemporalSampleIndex() const
	{
		return DistanceFieldTemporalSampleIndex;
	}

	/** Default constructor. */
	FSceneViewState(ERHIFeatureLevel::Type FeatureLevel, FSceneViewState* ShareOriginTarget);

	virtual ~FSceneViewState();

	// called every frame after the view state was updated
	void UpdateLastRenderTime(const FSceneViewFamily& Family)
	{
		// The editor can trigger multiple update calls within a frame
		if(Family.Time.GetRealTimeSeconds() != LastRenderTime)
		{
			LastRenderTimeDelta = Family.Time.GetRealTimeSeconds() - LastRenderTime;
			LastRenderTime = Family.Time.GetRealTimeSeconds();
		}
	}

	// InScene is passed in, as the Scene pointer in the class itself may be null, if it was allocated without a scene.
	void TrimHistoryRenderTargets(const FScene* InScene);

	/**
	 * Calculates and stores the scale factor to apply to motion vectors based on the current game
	 * time and view post process settings.
	 */
	void UpdateMotionBlurTimeScale(const FViewInfo& View);

	/** 
	 * Called every frame after UpdateLastRenderTime, sets up the information for the lagged temporal LOD transition
	 */
	void UpdateTemporalLODTransition(const FViewInfo& View)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (bIsFrozen)
		{
			return;
		}
#endif

		TemporalLODState.UpdateTemporalLODTransition(View, LastRenderTime);
	}

	/** 
	 * Returns an array of visibility data for the given view, or NULL if none exists. 
	 * The data bits are indexed by VisibilityId of each primitive in the scene.
	 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
	 * InScene is passed in, as the Scene pointer in the class itself may be null, if it was allocated without a scene.
	 */
	const uint8* GetPrecomputedVisibilityData(FViewInfo& View, const FScene* InScene);

	/**
	 * Cleans out old entries from the primitive occlusion history, and resets unused pending occlusion queries.
	 * @param MinHistoryTime - The occlusion history for any primitives which have been visible and unoccluded since
	 *							this time will be kept.  The occlusion history for any primitives which haven't been
	 *							visible and unoccluded since this time will be discarded.
	 * @param MinQueryTime - The pending occlusion queries older than this will be discarded.
	 */
	void TrimOcclusionHistory(float CurrentTime, float MinHistoryTime, float MinQueryTime, int32 FrameNumber);

	inline void UpdateRoundRobin(const bool bUseRoundRobin)
	{
		bRoundRobinOcclusionEnabled = bUseRoundRobin;
	}

	inline bool IsRoundRobinEnabled() const
	{
		return bRoundRobinOcclusionEnabled;
	}

	/**
	 * Checks whether a shadow is occluded this frame.
	 * @param Primitive - The shadow subject.
	 * @param Light - The shadow source.
	 */
	bool IsShadowOccluded(FSceneViewState::FProjectedShadowKey ShadowKey, int32 NumBufferedFrames) const;

	/**
	* Retrieve a single-pixel render targets with intra-frame state for use in eye adaptation post processing.
	*/
	IPooledRenderTarget* GetCurrentEyeAdaptationTexture() const override final
	{
		IPooledRenderTarget* Texture = EyeAdaptationManager.GetCurrentTexture().GetReference();
		check(bValidEyeAdaptationTexture && Texture);
		return Texture;
	}

	IPooledRenderTarget* GetCurrentEyeAdaptationTexture(FRHICommandList& RHICmdList)
	{
		bValidEyeAdaptationTexture = true;
		return EyeAdaptationManager.GetCurrentTexture(RHICmdList).GetReference();
	}

	/** Swaps the double-buffer targets used in eye adaptation */
	void SwapEyeAdaptationTextures()
	{
		EyeAdaptationManager.SwapTextures();
	}

	void UpdateEyeAdaptationLastExposureFromTexture()
	{
		if (bUpdateLastExposure && bValidEyeAdaptationTexture)
		{
			EyeAdaptationManager.UpdateLastExposureFromTexture();
		}
	}

	void EnqueueEyeAdaptationExposureTextureReadback(FRDGBuilder& GraphBuilder)
	{
		if (bUpdateLastExposure && bValidEyeAdaptationTexture)
		{
			EyeAdaptationManager.EnqueueExposureTextureReadback(GraphBuilder);
		}
	}

	FRDGPooledBuffer* GetCurrentEyeAdaptationBuffer() const override final
	{
		FRDGPooledBuffer* Buffer = EyeAdaptationManager.GetCurrentBuffer().GetReference();
		check(bValidEyeAdaptationBuffer && Buffer);
		return Buffer;
	}

	FRDGPooledBuffer* GetCurrentEyeAdaptationBuffer(FRDGBuilder& GraphBuilder)
	{
		bValidEyeAdaptationBuffer = true;
		return EyeAdaptationManager.GetCurrentBuffer(GraphBuilder).GetReference();
	}

	void SwapEyeAdaptationBuffers()
	{
		EyeAdaptationManager.SwapBuffers();
	}

	void UpdateEyeAdaptationLastExposureFromBuffer()
	{
		if (bUpdateLastExposure && bValidEyeAdaptationBuffer)
		{
			EyeAdaptationManager.UpdateLastExposureFromBuffer();
		}
	}

	void EnqueueEyeAdaptationExposureBufferReadback(FRDGBuilder& GraphBuilder)
	{
		if (bUpdateLastExposure && bValidEyeAdaptationBuffer)
		{
			EyeAdaptationManager.EnqueueExposureBufferReadback(GraphBuilder);
		}
	}

#if WITH_MGPU
	void BroadcastEyeAdaptationTemporalEffect(FRHICommandList& RHICmdList);
	void WaitForEyeAdaptationTemporalEffect(FRHICommandList& RHICmdList);
#endif

	float GetLastEyeAdaptationExposure() const
	{
		return EyeAdaptationManager.GetLastExposure();
	}

	float GetLastAverageSceneLuminance() const
	{
		return EyeAdaptationManager.GetLastAverageSceneLuminance();
	}

	bool HasValidTonemappingLUT() const
	{
		return bValidTonemappingLUT;
	}

	void SetValidTonemappingLUT(bool bValid = true)
	{
		bValidTonemappingLUT = bValid;
	}

	static FPooledRenderTargetDesc CreateLUTRenderTarget(const int32 LUTSize, const bool bUseVolumeLUT, const bool bNeedUAV, const bool bNeedFloatOutput)
	{
		// Create the texture needed for the tonemapping LUT in one place
		EPixelFormat LUTPixelFormat = PF_A2B10G10R10;
		if (!GPixelFormats[LUTPixelFormat].Supported)
		{
			LUTPixelFormat = PF_R8G8B8A8;
		}
		if (bNeedFloatOutput)
		{
			LUTPixelFormat = PF_FloatRGBA;
		}

		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(LUTSize * LUTSize, LUTSize), LUTPixelFormat, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource, false);
		Desc.Flags |= bNeedUAV ? TexCreate_UAV : TexCreate_RenderTargetable;

		if (bUseVolumeLUT)
		{
			Desc.Extent = FIntPoint(LUTSize, LUTSize);
			Desc.Depth = LUTSize;
		}

		Desc.DebugName = TEXT("CombineLUTs");
		Desc.Flags |= GFastVRamConfig.CombineLUTs;

		return Desc;
	}

	// Returns a reference to the render target used for the LUT.  Allocated on the first request.
	IPooledRenderTarget* GetTonemappingLUT(FRHICommandList& RHICmdList, const int32 LUTSize, const bool bUseVolumeLUT, const bool bNeedUAV, const bool bNeedFloatOutput)
	{
		if (CombinedLUTRenderTarget.IsValid() == false || 
			CombinedLUTRenderTarget->GetDesc().Extent.Y != LUTSize ||
			((CombinedLUTRenderTarget->GetDesc().Depth != 0) != bUseVolumeLUT) ||
			!!(CombinedLUTRenderTarget->GetDesc().Flags & TexCreate_UAV) != bNeedUAV ||
			(CombinedLUTRenderTarget->GetDesc().Format == PF_FloatRGBA) != bNeedFloatOutput)
		{
			// Create the texture needed for the tonemapping LUT
			FPooledRenderTargetDesc Desc = CreateLUTRenderTarget(LUTSize, bUseVolumeLUT, bNeedUAV, bNeedFloatOutput);

			GRenderTargetPool.FindFreeElement(RHICmdList, Desc, CombinedLUTRenderTarget, Desc.DebugName);
		}

		return CombinedLUTRenderTarget.GetReference();
	}

	IPooledRenderTarget* GetTonemappingLUT() const
	{
		return CombinedLUTRenderTarget.GetReference();
	}

	// FRenderResource interface.
	virtual void InitDynamicRHI() override
	{
		HZBOcclusionTests.InitDynamicRHI();
	}

	virtual void ReleaseDynamicRHI() override
	{
		HZBOcclusionTests.ReleaseDynamicRHI();
		EyeAdaptationManager.SafeRelease();
		bValidEyeAdaptationTexture = false;
		bValidEyeAdaptationBuffer = false;
	}

	// FSceneViewStateInterface
	RENDERER_API virtual void Destroy() override;

	virtual FSceneViewState* GetConcreteViewState() override
	{
		return this;
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{

		Collector.AddReferencedObjects(MIDPool);

		if (BloomFFTKernel.Physical)
		{
			Collector.AddReferencedObject(BloomFFTKernel.Physical);
		}

		if (FilmGrainCache.Texture)
		{
			Collector.AddReferencedObject(FilmGrainCache.Texture);
		}
	}

	// needed for GetReusableMID()
	virtual void OnStartPostProcessing(FSceneView& CurrentView) override
	{
		check(IsInGameThread());

		// Needs to be done once for all viewstates.  If multiple FSceneViews are sharing the same ViewState, this will cause problems.
		// Sharing should be illegal right now though.
		MIDUsedCount = 0;
	}

	/** Returns the current PreExposure value. PreExposure is a custom scale applied to the scene color to prevent buffer overflow. */
	virtual float GetPreExposure() const override
	{
		return PreExposure;
	}

	// Note: OnStartPostProcessing() needs to be called each frame for each view
	virtual UMaterialInstanceDynamic* GetReusableMID(class UMaterialInterface* InSource) override
	{		
		check(IsInGameThread());
		check(InSource);

		// 0 or MID (MaterialInstanceDynamic) pointer
		auto InputAsMID = Cast<UMaterialInstanceDynamic>(InSource);

		// fixup MID parents as this is not allowed, take the next MIC or Material.
		UMaterialInterface* ParentOfTheNewMID = InputAsMID ? ToRawPtr(InputAsMID->Parent) : InSource;

		// this is not allowed and would cause an error later in the code
		check(!ParentOfTheNewMID->IsA(UMaterialInstanceDynamic::StaticClass()));

		UMaterialInstanceDynamic* NewMID = 0;

		if(MIDUsedCount < (uint32)MIDPool.Num())
		{
			NewMID = MIDPool[MIDUsedCount];

			if(NewMID->Parent != ParentOfTheNewMID)
			{
				// create a new one
				// garbage collector will remove the old one
				// this should not happen too often
				NewMID = UMaterialInstanceDynamic::Create(ParentOfTheNewMID, 0);
				MIDPool[MIDUsedCount] = NewMID;
			}

			// reusing an existing object means we need to clear out the Vector and Scalar parameters
			NewMID->ClearParameterValues();
		}
		else
		{
			NewMID = UMaterialInstanceDynamic::Create(ParentOfTheNewMID, 0);
			check(NewMID);

			MIDPool.Add(NewMID);
		}

		if(InputAsMID)
		{
			// parent is an MID so we need to copy the MID Vector and Scalar parameters over
			NewMID->CopyInterpParameters(InputAsMID);
		}

		check(NewMID->GetRenderProxy());
		MIDUsedCount++;
		return NewMID;
	}

	virtual void ClearMIDPool() override
	{
		check(IsInGameThread());
		MIDPool.Empty();
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual const FViewMatrices* GetFrozenViewMatrices() const override
	{
		if (bIsFrozen && bIsFrozenViewMatricesCached)
		{
			return &CachedViewMatrices;
		}
		return nullptr;
	}

	virtual void ActivateFrozenViewMatrices(FSceneView& SceneView) override
	{
		auto* ViewState = static_cast<FSceneViewState*>(SceneView.State);
		if (ViewState->bIsFrozen)
		{
			check(ViewState->bIsFrozenViewMatricesCached);

			Swap(SceneView.ViewMatrices, ViewState->CachedViewMatrices);
			ViewState->bIsFrozenViewMatricesCached = false;
		}
	}

	virtual void RestoreUnfrozenViewMatrices(FSceneView& SceneView) override
	{
		auto* ViewState = static_cast<FSceneViewState*>(SceneView.State);
		if (ViewState->bIsFrozen)
		{
			check(!ViewState->bIsFrozenViewMatricesCached);

			Swap(SceneView.ViewMatrices, ViewState->CachedViewMatrices);
			ViewState->bIsFrozenViewMatricesCached = true;
		}
	}
#endif

	virtual FTemporalLODState& GetTemporalLODState() override
	{
		return TemporalLODState;
	}

	virtual const FTemporalLODState& GetTemporalLODState() const override
	{
		return TemporalLODState;
	}

	float GetTemporalLODTransition() const override
	{
		return TemporalLODState.GetTemporalLODTransition(LastRenderTime);
	}

	uint32 GetViewKey() const override
	{
		return UniqueID;
	}

	uint32 GetOcclusionFrameCounter() const
	{
		return OcclusionFrameCounter;
	}

	virtual SIZE_T GetSizeBytes() const override;
	uint64 GetGPUSizeBytes(bool bLogSizes = false) const;

	virtual void SetSequencerState(ESequencerState InSequencerState) override
	{
		SequencerState = InSequencerState;
	}

	virtual ESequencerState GetSequencerState() override
	{
		return SequencerState;
	}

	virtual void AddVirtualShadowMapCache(FSceneInterface* InScene) override;
	virtual void RemoveVirtualShadowMapCache(FSceneInterface* InScene) override;
	virtual bool HasVirtualShadowMapCache() const override;
	virtual FVirtualShadowMapArrayCacheManager* GetVirtualShadowMapCache(const FScene* InScene) const override;

	virtual void AddLumenSceneData(FSceneInterface* InScene, float SurfaceCacheResolution) override;
	virtual void RemoveLumenSceneData(FSceneInterface* InScene) override;
	virtual bool HasLumenSceneData() const override;

	/** Information about visibility/occlusion states in past frames for individual primitives. */
	TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs> PrimitiveOcclusionHistorySet;
};

/** Rendering resource class that manages a cubemap array for reflections. */
class FReflectionEnvironmentCubemapArray : public FRenderResource
{
public:

	FReflectionEnvironmentCubemapArray(ERHIFeatureLevel::Type InFeatureLevel)
		: FRenderResource(InFeatureLevel)
		, MaxCubemaps(0)
		, CubemapSize(0)
	{}

	virtual void InitDynamicRHI() override;
	virtual void ReleaseDynamicRHI() override;

	/** 
	 * Updates the maximum number of cubemaps that this array is allocated for.
	 * This reallocates the resource but does not copy over the old contents. 
	 */
	void UpdateMaxCubemaps(uint32 InMaxCubemaps, int32 CubemapSize);

	/**
	* Updates the maximum number of cubemaps that this array is allocated for.
	* This reallocates the resource and copies over the old contents, preserving indices
	*/
	void ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 CubemapSize, const TArray<int32>& IndexRemapping);

	int32 GetMaxCubemaps() const { return MaxCubemaps; }
	int32 GetCubemapSize() const { return CubemapSize; }
	bool IsValid() const { return IsValidRef(ReflectionEnvs); }
	const TRefCountPtr<IPooledRenderTarget>& GetRenderTarget() const { return ReflectionEnvs; }
	void Reset();

protected:
	uint32 MaxCubemaps;
	int32 CubemapSize;
	TRefCountPtr<IPooledRenderTarget> ReflectionEnvs;

	void ReleaseCubeArray();
};

/** Per-component reflection capture state that needs to persist through a re-register. */
class FCaptureComponentSceneState
{
public:
	/** Index of the cubemap in the array for this capture component. */
	int32 CubemapIndex;

	float AverageBrightness;

	FCaptureComponentSceneState(int32 InCubemapIndex) :
		CubemapIndex(InCubemapIndex),
		AverageBrightness(0.0f)
	{}

	bool operator==(const FCaptureComponentSceneState& Other) const 
	{
		return CubemapIndex == Other.CubemapIndex;
	}
};

struct FReflectionCaptureSortData
{
	FVector3f RelativePosition;
	FVector3f TilePosition;
	FMatrix44f BoxTransform;
	uint32 Guid;
	int32 CubemapIndex;
	float Radius;
	FVector4f CaptureProperties;
	FVector4f BoxScales;
	FVector4f CaptureOffsetAndAverageBrightness;
	FReflectionCaptureProxy* CaptureProxy;

	bool operator < (const FReflectionCaptureSortData& Other) const
	{
		if (Radius != Other.Radius)
		{
			return Radius < Other.Radius;
		}
		else
		{
			return Guid < Other.Guid;
		}
	}
};


struct FReflectionCaptureCacheEntry
{
	int32 RefCount;
	FCaptureComponentSceneState SceneState;
};


struct FReflectionCaptureCache
{
public:

	const FCaptureComponentSceneState* Find(const FGuid& MapBuildDataId) const;
	FCaptureComponentSceneState* Find(const FGuid& MapBuildDataId);

	const FCaptureComponentSceneState* Find(const UReflectionCaptureComponent* Component) const;
	FCaptureComponentSceneState* Find(const UReflectionCaptureComponent* Component);
	const FCaptureComponentSceneState& FindChecked(const UReflectionCaptureComponent* Component) const;
	FCaptureComponentSceneState& FindChecked(const UReflectionCaptureComponent* Component);

	FCaptureComponentSceneState& Add(const UReflectionCaptureComponent* Component, const FCaptureComponentSceneState& Value);
	FCaptureComponentSceneState* AddReference(const UReflectionCaptureComponent* Component);
	bool Remove(const UReflectionCaptureComponent* Component);
	int32 Prune(const TSet<FGuid> KeysToKeep, TArray<int32>& ReleasedIndices);

	int32 GetKeys(TArray<FGuid>& OutKeys) const;
	int32 GetKeys(TSet<FGuid>& OutKeys) const;

	void Empty();

protected:

	bool RemapRegisteredComponentMapBuildDataId(const UReflectionCaptureComponent* Component);
	void RegisterComponentMapBuildDataId(const UReflectionCaptureComponent* Component);
	void UnregisterComponentMapBuildDataId(const UReflectionCaptureComponent* Component);

	// Different map build data id of a capture might share the same capture component while editing (e.g., when they move).
	// need to replace it with the new one.
	TMap<const UReflectionCaptureComponent*, FGuid> RegisteredComponentMapBuildDataIds;

	TMap<FGuid, FReflectionCaptureCacheEntry> CaptureData;
};

/** Scene state used to manage the reflection environment feature. */
class FReflectionEnvironmentSceneData
{
public:

	/** 
	 * Set to true for one frame whenever RegisteredReflectionCaptures or the transforms of any registered reflection proxy has changed,
	 * Which allows one frame to update cached proxy associations.
	 */
	bool bRegisteredReflectionCapturesHasChanged;

	/** True if AllocatedReflectionCaptureState has changed. Allows to update cached single capture id. */
	bool AllocatedReflectionCaptureStateHasChanged;

	/** The rendering thread's list of visible reflection captures in the scene. */
	TArray<FReflectionCaptureProxy*> RegisteredReflectionCaptures;
	TArray<FSphere> RegisteredReflectionCapturePositionAndRadius;

	/** 
	 * Cubemap array resource which contains the captured scene for each reflection capture.
	 * This is indexed by the value of AllocatedReflectionCaptureState.CaptureIndex.
	 */
	FReflectionEnvironmentCubemapArray CubemapArray;

	/** Rendering thread map from component to scene state.  This allows storage of RT state that needs to persist through a component re-register. */
	FReflectionCaptureCache AllocatedReflectionCaptureState;

	/** Rendering bitfield to track cubemap slots used. Needs to kept in sync with AllocatedReflectionCaptureState */
	TBitArray<> CubemapArraySlotsUsed;

	/** Sorted scene reflection captures for upload to the GPU. */
	TArray<FReflectionCaptureSortData> SortedCaptures;
	int32 NumBoxCaptures;
	int32 NumSphereCaptures;

	/** 
	 * Game thread list of reflection components that have been allocated in the cubemap array. 
	 * These are not necessarily all visible or being rendered, but their scene state is stored in the cubemap array.
	 */
	TSparseArray<UReflectionCaptureComponent*> AllocatedReflectionCapturesGameThread;

	/** Game thread tracking of what size this scene has allocated for the cubemap array. */
	int32 MaxAllocatedReflectionCubemapsGameThread;

	/** Game thread tracking of what size cubemaps are in the cubemap array. */
	int32 ReflectionCaptureSizeGameThread;
	int32 DesiredReflectionCaptureSizeGameThread;

	FReflectionEnvironmentSceneData(ERHIFeatureLevel::Type InFeatureLevel) :
		bRegisteredReflectionCapturesHasChanged(true),
		AllocatedReflectionCaptureStateHasChanged(false),
		CubemapArray(InFeatureLevel),
		MaxAllocatedReflectionCubemapsGameThread(0),
		ReflectionCaptureSizeGameThread(0),
		DesiredReflectionCaptureSizeGameThread(0)
	{}

	/** Set Data necessary to determine if GPU resources will need future updates */
	void SetGameThreadTrackingData(int32 MaxAllocatedCubemaps, int32 CaptureSize, int32 DesiredCaptureSize);

	/** Do the resources on the GPU match our desired state?  If not, reallocation will be necessary. */
	bool DoesAllocatedDataNeedUpdate(int32 DesiredMaxCubemaps, int32 DesiredCaptureSize) const;

	void ResizeCubemapArrayGPU(uint32 InMaxCubemaps, int32 InCubemapSize);

	/** Resets the structure to empty, useful if you want to shrink the allocation. */
	void Reset(FScene* Scene);
};

/** Scene state used to manage hair strands. */
class FHairStrandsSceneData
{
public:
	TArray<FHairStrandsInstance*> RegisteredProxies;
};

class FVolumetricLightmapInterpolation
{
public:
	FVector4f IndirectLightingSHCoefficients0[3];
	FVector4f IndirectLightingSHCoefficients1[3];
	FVector4f IndirectLightingSHCoefficients2;
	FVector4f IndirectLightingSHSingleCoefficient;
	FVector4f PointSkyBentNormal;
	float DirectionalLightShadowing;
	uint32 LastUsedSceneFrameNumber;
};

class FVolumetricLightmapSceneData
{
public:

	FVolumetricLightmapSceneData(FScene* InScene)
		: Scene(InScene)
	{
		GlobalVolumetricLightmap.Data = &GlobalVolumetricLightmapData;
	}

	bool HasData() const;
	void AddLevelVolume(const class FPrecomputedVolumetricLightmap* InVolume, EShadingPath ShadingPath, bool bIsPersistentLevel);
	void RemoveLevelVolume(const class FPrecomputedVolumetricLightmap* InVolume);
	const FPrecomputedVolumetricLightmap* GetLevelVolumetricLightmap() const;

	TMap<FVector, FVolumetricLightmapInterpolation> CPUInterpolationCache;

	FPrecomputedVolumetricLightmapData GlobalVolumetricLightmapData;
private:
	FScene* Scene;
	FPrecomputedVolumetricLightmap GlobalVolumetricLightmap;
	const FPrecomputedVolumetricLightmap* PersistentLevelVolumetricLightmap = nullptr;
	TArray<const FPrecomputedVolumetricLightmap*> LevelVolumetricLightmaps;
};

class FPrimitiveAndInstance
{
public:

	FPrimitiveAndInstance(const FMatrix& InLocalToWorld, const FBox& InWorldBounds, FPrimitiveSceneInfo* InPrimitive, int32 InInstanceIndex)
	: LocalToWorld(InLocalToWorld)
	, WorldBounds(InWorldBounds)
	, InstanceIndex(InInstanceIndex)
	, Primitive(InPrimitive)
	{
	}

	FMatrix LocalToWorld;
	FBox WorldBounds;
	int32 InstanceIndex;
	FPrimitiveSceneInfo* Primitive;
};

class FPrimitiveRemoveInfo
{
public:
	FPrimitiveRemoveInfo(const FPrimitiveSceneInfo* InPrimitive)
	: Primitive(InPrimitive)
	, bOftenMoving(InPrimitive->Proxy->IsOftenMoving())
	, DistanceFieldInstanceIndices(Primitive->DistanceFieldInstanceIndices)
	{
		float SelfShadowBias;
		InPrimitive->Proxy->GetDistanceFieldAtlasData(DistanceFieldData, SelfShadowBias);
	}

	/** 
	 * Must not be dereferenced after creation, the primitive was removed from the scene and deleted
	 * Value of the pointer is still useful for map lookups
	 */
	const FPrimitiveSceneInfo* Primitive;

	bool bOftenMoving;

	TArray<int32, TInlineAllocator<1>> DistanceFieldInstanceIndices;

	const FDistanceFieldVolumeData* DistanceFieldData;
};

class FHeightFieldPrimitiveRemoveInfo : public FPrimitiveRemoveInfo
{
public:
	FHeightFieldPrimitiveRemoveInfo(const FPrimitiveSceneInfo* InPrimitive)
		: FPrimitiveRemoveInfo(InPrimitive)
	{
		const FBoxSphereBounds Bounds = InPrimitive->Proxy->GetBounds();
		WorldBounds = Bounds.GetBox();
	}

	FBox WorldBounds;
};

/** Identifies a mip of a distance field atlas. */
class FDistanceFieldAssetMipId
{
public:

	FDistanceFieldAssetMipId(FSetElementId InAssetId, int32 InReversedMipIndex = 0) :
		AssetId(InAssetId),
		ReversedMipIndex(InReversedMipIndex)
	{}

	FSetElementId AssetId;
	int32 ReversedMipIndex;
};

/** Stores distance field mip relocation data. */
class FDistanceFieldAssetMipRelocation
{
public:
	FDistanceFieldAssetMipRelocation(FIntVector InIndirectionDimensions, FIntVector InSrcPosition, FIntVector InDstPosition) :
		IndirectionDimensions(InIndirectionDimensions),
		SrcPosition(InSrcPosition),
		DstPosition(InDstPosition)
	{}

	FIntVector IndirectionDimensions;
	FIntVector SrcPosition;
	FIntVector DstPosition;
};

/** Stores state about a distance field mip that is tracked by the scene. */
class FDistanceFieldAssetMipState
{
public:

	FDistanceFieldAssetMipState() :
		IndirectionDimensions(FIntVector(0, 0, 0)),
		IndirectionTableOffset(-1),
		NumBricks(0)
	{}

	FIntVector IndirectionDimensions;
	int32 IndirectionTableOffset;
	FIntVector IndirectionAtlasOffset;
	int32 NumBricks;
	TArray<int32, TInlineAllocator<4>> AllocatedBlocks;
};

class FDistanceFieldAssetState
{
public:

	FDistanceFieldAssetState() :
		BuiltData(nullptr),
		RefCount(0),
		WantedNumMips(0)
	{}

	const FDistanceFieldVolumeData* BuiltData;
	int32 RefCount;
	int32 WantedNumMips;
	TArray<FDistanceFieldAssetMipState, TInlineAllocator<3>> ReversedMips;
};

struct TFDistanceFieldAssetStateFuncs : BaseKeyFuncs<FDistanceFieldAssetState, const FDistanceFieldVolumeData*, /* bInAllowDuplicateKeys = */ false>
{
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.BuiltData;
	}
	static bool Matches(KeyInitType A,KeyInitType B)
	{
		return A == B;
	}
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return PointerHash(Key);
	}
};

class FDistanceFieldBlockAllocator
{
public:
	void Allocate(int32 NumBlocks, TArray<int32, TInlineAllocator<4>>& OutBlocks);

	void Free(const TArray<int32, TInlineAllocator<4>>& ElementRange);

	int32 GetMaxSize() const 
	{ 
		return MaxNumBlocks; 
	}

	int32 GetAllocatedSize() const
	{
		return MaxNumBlocks - FreeBlocks.Num();
	}

private:
	int32 MaxNumBlocks = 0;
	TArray<int32, TInlineAllocator<4>> FreeBlocks;
};

struct FDistanceFieldReadRequest
{
	// SDF scene context
	FSetElementId AssetSetId;
	int32 ReversedMipIndex = 0;
	int32 NumDistanceFieldBricks = 0;
	uint64 BuiltDataId = 0;

	// Used when BulkData is nullptr
	const uint8* AlwaysLoadedDataPtr = nullptr;

	// Inputs of read request
	const FByteBulkData* BulkData = nullptr;
	uint32 BulkOffset = 0;
	uint32 BulkSize = 0;

	// Outputs of read request
	uint8* ReadOutputDataPtr = nullptr;
	FIoRequest Request;
	IAsyncReadFileHandle* AsyncHandle = nullptr;
	IAsyncReadRequest* AsyncRequest = nullptr;
};

struct FDistanceFieldAsyncUpdateParameters
{
	FDistanceFieldSceneData* DistanceFieldSceneData = nullptr;
	FIntVector4* BrickUploadCoordinatesPtr = nullptr;
	uint8* BrickUploadDataPtr = nullptr;

	uint32* IndirectionIndicesUploadPtr = nullptr;
	FVector4f* IndirectionDataUploadPtr = nullptr;

	TArray<FDistanceFieldReadRequest> NewReadRequests;
	TArray<FDistanceFieldReadRequest> ReadRequestsToUpload;
	TArray<FDistanceFieldReadRequest> ReadRequestsToCleanUp;
};

/** Scene data used to manage distance field object buffers on the GPU. */
class FDistanceFieldSceneData
{
public:

	FDistanceFieldSceneData(EShaderPlatform ShaderPlatform);
	~FDistanceFieldSceneData();

	void AddPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void UpdatePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void RemovePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void Release();
	void VerifyIntegrity();
	void ListMeshDistanceFields(bool bDumpAssetStats) const;

	void UpdateDistanceFieldObjectBuffers(
		FRDGBuilder& GraphBuilder,
		FRDGExternalAccessQueue& ExternalAccessQueue,
		FScene* Scene,
		TArray<FDistanceFieldAssetMipId>& DistanceFieldAssetAdds,
		TArray<FSetElementId>& DistanceFieldAssetRemoves);

	void UpdateDistanceFieldAtlas(
		FRDGBuilder& GraphBuilder,
		FRDGExternalAccessQueue& ExternalAccessQueue,
		const FViewInfo& View,
		FScene* Scene,
		bool bLumenEnabled,
		FGlobalShaderMap* GlobalShaderMap,
		TArray<FDistanceFieldAssetMipId>& DistanceFieldAssetAdds,
		TArray<FSetElementId>& DistanceFieldAssetRemoves);

	bool HasPendingOperations() const
	{
		return PendingAddOperations.Num() > 0 || PendingUpdateOperations.Num() > 0 || PendingRemoveOperations.Num() > 0;
	}

	bool HasPendingHeightFieldOperations() const
	{
		return PendingHeightFieldAddOps.Num() > 0 || PendingHeightFieldUpdateOps.Num() > 0 || PendingHeightFieldRemoveOps.Num() > 0;
	}

	bool HasPendingRemovePrimitive(const FPrimitiveSceneInfo* Primitive) const
	{
		for (int32 RemoveIndex = 0; RemoveIndex < PendingRemoveOperations.Num(); ++RemoveIndex)
		{
			if (PendingRemoveOperations[RemoveIndex].Primitive == Primitive)
			{
				return true;
			}
		}

		return false;
	}

	bool HasPendingRemoveHeightFieldPrimitive(const FPrimitiveSceneInfo* Primitive) const
	{
		for (int32 RemoveIndex = 0; RemoveIndex < PendingHeightFieldRemoveOps.Num(); ++RemoveIndex)
		{
			if (PendingHeightFieldRemoveOps[RemoveIndex].Primitive == Primitive)
			{
				return true;
			}
		}

		return false;
	}

	inline bool CanUse16BitObjectIndices() const
	{
		return bCanUse16BitObjectIndices && (NumObjectsInBuffer < (1 << 16));
	}

	bool CanUse16BitHeightFieldObjectIndices() const
	{
		return bCanUse16BitObjectIndices && (NumHeightFieldObjectsInBuffer < 65536);
	}

	const class FDistanceFieldObjectBuffers* GetCurrentObjectBuffers() const
	{
		return ObjectBuffers;
	}

	const class FHeightFieldObjectBuffers* GetHeightFieldObjectBuffers() const
	{
		return HeightFieldObjectBuffers;
	}

	int32 NumObjectsInBuffer;
	int32 NumHeightFieldObjectsInBuffer;
	class FDistanceFieldObjectBuffers* ObjectBuffers;
	class FHeightFieldObjectBuffers* HeightFieldObjectBuffers;

	FRDGScatterUploadBuffer UploadHeightFieldDataBuffer;
	FRDGScatterUploadBuffer UploadHeightFieldBoundsBuffer;
	FRDGScatterUploadBuffer UploadDistanceFieldDataBuffer;
	FRDGScatterUploadBuffer UploadDistanceFieldBoundsBuffer;
	
	TArray<int32> IndicesToUpdateInObjectBuffers;
	TArray<int32> IndicesToUpdateInHeightFieldObjectBuffers;

	TSet<FDistanceFieldAssetState, TFDistanceFieldAssetStateFuncs> AssetStateArray;
	TRefCountPtr<FRDGPooledBuffer> AssetDataBuffer;
	FRDGScatterUploadBuffer AssetDataUploadBuffer;

	TArray<FRHIGPUBufferReadback*> StreamingRequestReadbackBuffers;
	uint32 MaxStreamingReadbackBuffers = 4;
	uint32 ReadbackBuffersWriteIndex = 0;
	uint32 ReadbackBuffersNumPending = 0;

	FGrowOnlySpanAllocator IndirectionTableAllocator;
	TRefCountPtr<FRDGPooledBuffer> IndirectionTable;
	FRDGScatterUploadBuffer IndirectionTableUploadBuffer;

	TRefCountPtr<IPooledRenderTarget> IndirectionAtlas;
	FTextureLayout3d IndirectionAtlasLayout;
	FReadBuffer IndirectionUploadIndicesBuffer;
	FReadBuffer IndirectionUploadDataBuffer;

	FDistanceFieldBlockAllocator DistanceFieldAtlasBlockAllocator;
	TRefCountPtr<IPooledRenderTarget> DistanceFieldBrickVolumeTexture;
	FIntVector BrickTextureDimensionsInBricks;
	FReadBuffer BrickUploadCoordinatesBuffer;
	FReadBuffer BrickUploadDataBuffer;

	TArray<FDistanceFieldReadRequest> ReadRequests;

	/** Stores the primitive and instance index of every entry in the object buffer. */
	TArray<FPrimitiveAndInstance> PrimitiveInstanceMapping;
	TArray<FPrimitiveSceneInfo*> HeightfieldPrimitives;
	/** Pending operations on the object buffers to be processed next frame. */
	TSet<FPrimitiveSceneInfo*> PendingAddOperations;
	TSet<FPrimitiveSceneInfo*> PendingThrottledOperations;
	TSet<FPrimitiveSceneInfo*> PendingUpdateOperations;
	TArray<FPrimitiveRemoveInfo> PendingRemoveOperations;
	TArray<FRenderBounds> PrimitiveModifiedBounds[GDF_Num];

	TSet<FPrimitiveSceneInfo*> PendingHeightFieldAddOps;
	TSet<FPrimitiveSceneInfo*> PendingHeightFieldUpdateOps;
	TArray<FHeightFieldPrimitiveRemoveInfo> PendingHeightFieldRemoveOps;

	int32 HeightFieldAtlasGeneration;
	int32 HFVisibilityAtlasGenerattion;

	bool bTrackAllPrimitives;
	bool bCanUse16BitObjectIndices;

private:

	void ProcessStreamingRequestsFromGPU(
		TArray<FDistanceFieldReadRequest>& NewReadRequests,
		TArray<FDistanceFieldAssetMipId>& AssetDataUploads);

	void ProcessReadRequests(
		TArray<FDistanceFieldAssetMipId>& AssetDataUploads,
		TArray<FDistanceFieldAssetMipId>& DistanceFieldAssetMipAdds,
		TArray<FDistanceFieldReadRequest>& ReadRequestsToUpload,
		TArray<FDistanceFieldReadRequest>& ReadRequestsToCleanUp);

	FRDGTexture* ResizeBrickAtlasIfNeeded(FRDGBuilder& GraphBuilder, FGlobalShaderMap* GlobalShaderMap);

	bool ResizeIndirectionAtlasIfNeeded(FRDGBuilder& GraphBuilder, FGlobalShaderMap* GlobalShaderMap, FRDGTexture*& OutTexture);

	void DefragmentIndirectionAtlas(FIntVector MinSize, TArray<FDistanceFieldAssetMipRelocation>& Relocations);

	void UploadAssetData(FRDGBuilder& GraphBuilder, const TArray<FDistanceFieldAssetMipId>& AssetDataUploads, FRDGBuffer* AssetDataBufferRDG);
	
	void UploadAllAssetData(FRDGBuilder& GraphBuilder, FRDGBuffer* AssetDataBufferRDG);

	void AsyncUpdate(FDistanceFieldAsyncUpdateParameters UpdateParameters);

	void GenerateStreamingRequests(
		FRDGBuilder& GraphBuilder, 
		const FViewInfo& View,
		FScene* Scene,
		bool bLumenEnabled,
		FGlobalShaderMap* GlobalShaderMap);

	friend class FDistanceFieldStreamingUpdateTask;
};

/** Stores data for an allocation in the FIndirectLightingCache. */
class FIndirectLightingCacheBlock
{
public:

	FIndirectLightingCacheBlock() :
		MinTexel(FIntVector(0, 0, 0)),
		TexelSize(0),
		Min(FVector(0, 0, 0)),
		Size(FVector(0, 0, 0)),
		bHasEverBeenUpdated(false)
	{}

	FIntVector MinTexel;
	int32 TexelSize;
	FVector Min;
	FVector Size;
	bool bHasEverBeenUpdated;
};

/** Stores information about an indirect lighting cache block to be updated. */
class FBlockUpdateInfo
{
public:

	FBlockUpdateInfo(const FIndirectLightingCacheBlock& InBlock, FIndirectLightingCacheAllocation* InAllocation) :
		Block(InBlock),
		Allocation(InAllocation)
	{}

	FIndirectLightingCacheBlock Block;
	FIndirectLightingCacheAllocation* Allocation;
};

/** Information about the primitives that are attached together. */
class FAttachmentGroupSceneInfo
{
public:

	/** The parent primitive, which is the root of the attachment tree. */
	FPrimitiveSceneInfo* ParentSceneInfo;

	/** The primitives in the attachment group. */
	TArray<FPrimitiveSceneInfo*> Primitives;

	FAttachmentGroupSceneInfo() :
		ParentSceneInfo(nullptr)
	{}
};

struct FILCUpdatePrimTaskData
{
	FGraphEventRef TaskRef;
	TMap<FIntVector, FBlockUpdateInfo> OutBlocksToUpdate;
	TArray<FIndirectLightingCacheAllocation*> OutTransitionsOverTimeToUpdate;
	TArray<FPrimitiveSceneInfo*> OutPrimitivesToUpdateStaticMeshes;
};

/** 
 * Implements a volume texture atlas for caching indirect lighting on a per-object basis.
 * The indirect lighting is interpolated from Lightmass SH volume lighting samples.
 */
class FIndirectLightingCache : public FRenderResource
{
public:	

	/** true for the editor case where we want a better preview for object that have no valid lightmaps */
	FIndirectLightingCache(ERHIFeatureLevel::Type InFeatureLevel);

	// FRenderResource interface
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	/** Allocates a block in the volume texture atlas for a primitive. */
	FIndirectLightingCacheAllocation* AllocatePrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bUnbuiltPreview);

	/** Releases the indirect lighting allocation for the given primitive. */
	void ReleasePrimitive(FPrimitiveComponentId PrimitiveId);

	FIndirectLightingCacheAllocation* FindPrimitiveAllocation(FPrimitiveComponentId PrimitiveId) const;	

	/** Updates indirect lighting in the cache based on visibility synchronously. */
	void UpdateCache(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview);

	/** Starts a task to update the cache primitives.  Results and task ref returned in the FILCUpdatePrimTaskData structure */
	void StartUpdateCachePrimitivesTask(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview, FILCUpdatePrimTaskData& OutTaskData);

	/** Wait on a previously started task and complete any block updates and debug draw */
	void FinalizeCacheUpdates(FScene* Scene, FSceneRenderer& Renderer, FILCUpdatePrimTaskData& TaskData);

	/** Force all primitive allocations to be re-interpolated. */
	void SetLightingCacheDirty(FScene* Scene, const FPrecomputedLightVolume* Volume);

	// Accessors
	FRHITexture* GetTexture0() { return Texture0->GetRHI(); }
	FRHITexture* GetTexture1() { return Texture1->GetRHI(); }
	FRHITexture* GetTexture2() { return Texture2->GetRHI(); }

private:
	/** Internal helper to determine if indirect lighting is enabled at all */
	bool IndirectLightingAllowed(FScene* Scene, FSceneRenderer& Renderer) const;

	void ProcessPrimitiveUpdate(FScene* Scene, FViewInfo& View, int32 PrimitiveIndex, bool bAllowUnbuiltPreview, bool bAllowVolumeSample, TMap<FIntVector, FBlockUpdateInfo>& OutBlocksToUpdate, TArray<FIndirectLightingCacheAllocation*>& OutTransitionsOverTimeToUpdate, TArray<FPrimitiveSceneInfo*>& OutPrimitivesToUpdateStaticMeshes);

	/** Internal helper to perform the work of updating the cache primitives.  Can be done on any thread as a task */
	void UpdateCachePrimitivesInternal(FScene* Scene, FSceneRenderer& Renderer, bool bAllowUnbuiltPreview, TMap<FIntVector, FBlockUpdateInfo>& OutBlocksToUpdate, TArray<FIndirectLightingCacheAllocation*>& OutTransitionsOverTimeToUpdate, TArray<FPrimitiveSceneInfo*>& OutPrimitivesToUpdateStaticMeshes);

	/** Internal helper to perform blockupdates and transition updates on the results of UpdateCachePrimitivesInternal.  Must be on render thread. */
	void FinalizeUpdateInternal_RenderThread(FScene* Scene, FSceneRenderer& Renderer, TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate, const TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate, TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdateStaticMeshes);

	/** Internal helper which adds an entry to the update lists for this allocation, if needed (due to movement, etc). Returns true if the allocation was updated or will be udpated */
	bool UpdateCacheAllocation(
		const FBoxSphereBounds& Bounds, 
		int32 BlockSize,
		bool bPointSample,
		bool bUnbuiltPreview,
		FIndirectLightingCacheAllocation*& Allocation, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate,
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate);	

	/** 
	 * Creates a new allocation if needed, caches the result in PrimitiveSceneInfo->IndirectLightingCacheAllocation, 
	 * And adds an entry to the update lists when an update is needed. 
	 */
	void UpdateCachePrimitive(
		const TMap<FPrimitiveComponentId, FAttachmentGroupSceneInfo>& AttachmentGroups,
		FPrimitiveSceneInfo* PrimitiveSceneInfo,
		bool bAllowUnbuiltPreview, 
		bool bAllowVolumeSample, 
		TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate, 
		TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate,
		TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdateStaticMeshes);

	/** Updates the contents of the volume texture blocks in BlocksToUpdate. */
	void UpdateBlocks(FScene* Scene, FViewInfo* DebugDrawingView, TMap<FIntVector, FBlockUpdateInfo>& BlocksToUpdate);

	/** Updates any outstanding transitions with a new delta time. */
	void UpdateTransitionsOverTime(const TArray<FIndirectLightingCacheAllocation*>& TransitionsOverTimeToUpdate, float DeltaWorldTime) const;

	/** Creates an allocation to be used outside the indirect lighting cache and a block to be used internally. */
	FIndirectLightingCacheAllocation* CreateAllocation(int32 BlockSize, const FBoxSphereBounds& Bounds, bool bPointSample, bool bUnbuiltPreview);	

	/** Block accessors. */
	FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin);
	const FIndirectLightingCacheBlock& FindBlock(FIntVector TexelMin) const;

	/** Block operations. */
	void DeallocateBlock(FIntVector Min, int32 Size);
	bool AllocateBlock(int32 Size, FIntVector& OutMin);

	/**
	 * Updates an allocation block in the cache, by re-interpolating values and uploading to the cache volume texture.
	 * @param DebugDrawingView can be 0
	 */
	void UpdateBlock(FScene* Scene, FViewInfo* DebugDrawingView, FBlockUpdateInfo& Block);

	/** Interpolates a single SH sample from all levels. */
	void InterpolatePoint(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block,
		float& OutDirectionalShadowing, 
		FSHVectorRGB3& OutIncidentRadiance,
		FVector& OutSkyBentNormal);

	/** Interpolates SH samples for a block from all levels. */
	void InterpolateBlock(
		FScene* Scene, 
		const FIndirectLightingCacheBlock& Block, 
		TArray<float>& AccumulatedWeight, 
		TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance);

	/** 
	 * Normalizes, adjusts for SH ringing, and encodes SH samples into a texture format.
	 * @param DebugDrawingView can be 0
	 */
	void EncodeBlock(
		FViewInfo* DebugDrawingView,
		const FIndirectLightingCacheBlock& Block, 
		const TArray<float>& AccumulatedWeight, 
		const TArray<FSHVectorRGB2>& AccumulatedIncidentRadiance,
		TArray<FFloat16Color>& Texture0Data,
		TArray<FFloat16Color>& Texture1Data,
		TArray<FFloat16Color>& Texture2Data		
	);

	/** Helper that calculates an effective world position min and size given a bounds. */
	void CalculateBlockPositionAndSize(const FBoxSphereBounds& Bounds, int32 TexelSize, FVector& OutMin, FVector& OutSize) const;

	/** Helper that calculates a scale and add to convert world space position into volume texture UVs for a given block. */
	void CalculateBlockScaleAndAdd(FIntVector InTexelMin, int32 AllocationTexelSize, FVector InMin, FVector InSize, FVector& OutScale, FVector& OutAdd, FVector& OutMinUV, FVector& OutMaxUV) const;

	/** true: next rendering we update all entries no matter if they are visible to avoid further hitches */
	bool bUpdateAllCacheEntries;

	/** Size of the volume texture cache. */
	int32 CacheSize;

	/** Volume textures that store SH indirect lighting, interpolated from Lightmass volume samples. */
	TRefCountPtr<IPooledRenderTarget> Texture0;
	TRefCountPtr<IPooledRenderTarget> Texture1;
	TRefCountPtr<IPooledRenderTarget> Texture2;

	/** Tracks the allocation state of the atlas. */
	TMap<FIntVector, FIndirectLightingCacheBlock> VolumeBlocks;

	/** Tracks used sections of the volume texture atlas. */
	FTextureLayout3d BlockAllocator;

	int32 NextPointId;

	/** Tracks primitive allocations by component, so that they persist across re-registers. */
	TMap<FPrimitiveComponentId, FIndirectLightingCacheAllocation*> PrimitiveAllocations;

	friend class FUpdateCachePrimitivesTask;
};

/**
 * Bounding information used to cull primitives in the scene.
 */
struct FPrimitiveBounds
{
	FBoxSphereBounds BoxSphereBounds;
	/** Square of the minimum draw distance for the primitive. */
	float MinDrawDistance;
	/** Maximum draw distance for the primitive. */
	float MaxDrawDistance;
	/** Maximum cull distance for the primitive. This is only different from the MaxDrawDistance for HLOD.*/
	float MaxCullDistance;
};

/**
 * Precomputed primitive visibility ID.
 */
struct FPrimitiveVisibilityId
{
	/** Index in to the byte where precomputed occlusion data is stored. */
	int32 ByteIndex;
	/** Mast of the bit where precomputed occlusion data is stored. */
	uint8 BitMask;
};

/**
 * Flags that affect how primitives are occlusion culled.
 */
namespace EOcclusionFlags
{
	enum Type
	{
		/** No flags. */
		None = 0x0,
		/** Indicates the primitive can be occluded. */
		CanBeOccluded = 0x1,
		/** Allow the primitive to be batched with others to determine occlusion. */
		AllowApproximateOcclusion = 0x4,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasPrecomputedVisibility = 0x8,
		/** Indicates the primitive has a valid ID for precomputed visibility. */
		HasSubprimitiveQueries = 0x10,
	};
};

/** Velocity state for a single component. */
class FComponentVelocityData
{
public:

	FPrimitiveSceneInfo* PrimitiveSceneInfo;
	FMatrix LocalToWorld;
	FMatrix PreviousLocalToWorld;
	mutable uint64 LastFrameUsed;
	uint64 LastFrameUpdated;
	bool bPreviousLocalToWorldValid = false;
};

/**
 * Tracks primitive transforms so they will be persistent across rendering state recreates.
 */
class FSceneVelocityData
{
public:

	/**
	 * Must be called once per frame, even when there are multiple BeginDrawingViewports.
	 */
	void StartFrame(FScene* Scene);

	/** 
	 * Looks up the PreviousLocalToWorld state for the given component.  Returns false if none is found (the primitive has never been moved). 
	 */
	inline bool GetComponentPreviousLocalToWorld(FPrimitiveComponentId PrimitiveComponentId, FMatrix& OutPreviousLocalToWorld) const
	{
		const FComponentVelocityData* VelocityData = ComponentData.Find(PrimitiveComponentId);

		if (VelocityData && VelocityData->PrimitiveSceneInfo)
		{
			check(VelocityData->bPreviousLocalToWorldValid);
			VelocityData->LastFrameUsed = InternalFrameIndex;
			OutPreviousLocalToWorld = VelocityData->PreviousLocalToWorld;
			return true;
		}

		return false;
	}

	/** 
	 * Updates a primitives current LocalToWorld state.
	 */
	void UpdateTransform(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMatrix& LocalToWorld, const FMatrix& PreviousLocalToWorld)
	{
		check(PrimitiveSceneInfo->Proxy->HasDynamicTransform());

		FComponentVelocityData& VelocityData = ComponentData.FindOrAdd(PrimitiveSceneInfo->PrimitiveComponentId);
		VelocityData.LocalToWorld = LocalToWorld;
		VelocityData.LastFrameUsed = InternalFrameIndex;
		VelocityData.LastFrameUpdated = InternalFrameIndex;
		VelocityData.PrimitiveSceneInfo = PrimitiveSceneInfo;

		// If this transform state is newly added, use the passed in PreviousLocalToWorld for this frame
		if (!VelocityData.bPreviousLocalToWorldValid)
		{
			VelocityData.PreviousLocalToWorld = PreviousLocalToWorld;
			VelocityData.bPreviousLocalToWorldValid = true;
		}
	}

	void RemoveFromScene(FPrimitiveComponentId PrimitiveComponentId, bool bImmediate)
	{
		if (bImmediate)
		{
			ComponentData.Remove(PrimitiveComponentId);
		}
		else
		{
			FComponentVelocityData* VelocityData = ComponentData.Find(PrimitiveComponentId);

			if (VelocityData)
			{
				VelocityData->PrimitiveSceneInfo = nullptr;
			}
		}
	}

	/** 
	 * Overrides a primitive's previous LocalToWorld matrix for this frame only
	 */
	void OverridePreviousTransform(FPrimitiveComponentId PrimitiveComponentId, const FMatrix& PreviousLocalToWorld)
	{
		FComponentVelocityData* VelocityData = ComponentData.Find(PrimitiveComponentId);
		if (VelocityData)
		{
			VelocityData->PreviousLocalToWorld = PreviousLocalToWorld;
			VelocityData->bPreviousLocalToWorldValid = true;
		}
	}

	void ApplyOffset(FVector Offset)
	{
		for (TMap<FPrimitiveComponentId, FComponentVelocityData>::TIterator It(ComponentData); It; ++It)
		{
			FComponentVelocityData& VelocityData = It.Value();
			VelocityData.LocalToWorld.SetOrigin(VelocityData.LocalToWorld.GetOrigin() + Offset);
			VelocityData.PreviousLocalToWorld.SetOrigin(VelocityData.PreviousLocalToWorld.GetOrigin() + Offset);
		}
	}

private:

	uint64 InternalFrameIndex = 0;
	TMap<FPrimitiveComponentId, FComponentVelocityData> ComponentData;
};

class FLODSceneTree
{
public:
	FLODSceneTree(FScene* InScene)
		: Scene(InScene)
	{
	}

	/** Information about the primitives that are attached together. */
	struct FLODSceneNode
	{
		/** Children scene infos. */
		TArray<FPrimitiveSceneInfo*> ChildrenSceneInfos;

		/** The primitive. */
		FPrimitiveSceneInfo* SceneInfo;

		FLODSceneNode()
			: SceneInfo(nullptr)
		{
		}

		void AddChild(FPrimitiveSceneInfo* NewChild)
		{
			if (NewChild)
			{
				ChildrenSceneInfos.AddUnique(NewChild);
			}
		}

		void RemoveChild(FPrimitiveSceneInfo* ChildToDelete)
		{
			if (ChildToDelete)
			{
				ChildrenSceneInfos.Remove(ChildToDelete);
			}
		}
	};

	void AddChildNode(FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo);
	void RemoveChildNode(FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo);

	void UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo);
	void UpdateVisibilityStates(FViewInfo& View);

	void ClearVisibilityState(FViewInfo& View);

	bool IsActive() const { return (SceneNodes.Num() > 0); }

private:

	/** Scene this Tree belong to */
	FScene* Scene;

	/** The LOD groups in the scene.  The map key is the current primitive who has children. */
	TMap<FPrimitiveComponentId, FLODSceneNode> SceneNodes;

	/** Recursive state updates */
	void ApplyNodeFadingToChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FHLODSceneNodeVisibilityState& NodeVisibility, const bool bIsFading, const bool bIsFadingOut);
	void HideNodeChildren(FSceneViewState* ViewState, FLODSceneNode& Node);
};

// Enable the DEBUG_CSM_CACHING to make the debugging CSM caching with RenderDoc more easier
#define DEBUG_CSM_CACHING 0

class FCachedShadowMapData
{
public:
	FWholeSceneProjectedShadowInitializer Initializer;
	FShadowMapRenderTargetsRefCounted ShadowMap;
	float LastUsedTime;
	bool bCachedShadowMapHasPrimitives;
	bool bCachedShadowMapHasNaniteGeometry;

	/**
	* The static meshes cast shadow on this cached csm
	*/
	TBitArray<> StaticShadowSubjectMap;

	FIntPoint ShadowBufferResolution;
	FVector PreShadowTranslation;
	float MaxSubjectZ;
	float MinSubjectZ;

	/**
	* The extra static meshes cast shadow in last frame, if it exceeds the r.Shadow.MaxCSMScrollingStaticShadowSubjects, the cached csm should be rebuilt.
	*/
	int32 LastFrameExtraStaticShadowSubjects;

	void InvalidateCachedShadow()
	{
		ShadowMap.Release();

		StaticShadowSubjectMap.SetRange(0, StaticShadowSubjectMap.Num(), false);
	}

	FCachedShadowMapData(const FWholeSceneProjectedShadowInitializer& InInitializer, float InLastUsedTime) :
		Initializer(InInitializer),
		LastUsedTime(InLastUsedTime),
		bCachedShadowMapHasPrimitives(true),
		bCachedShadowMapHasNaniteGeometry(false)
	{}
};

#if WITH_EDITOR
class FPixelInspectorData
{
public:
	FPixelInspectorData();

	void InitializeBuffers(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDEF, int32 bufferIndex);

	bool AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest);

	//Hold the buffer array
	TMap<FVector2f, FPixelInspectorRequest *> Requests;

	FRenderTarget* RenderTargetBufferDepth[2];
	FRenderTarget* RenderTargetBufferFinalColor[2];
	FRenderTarget* RenderTargetBufferHDR[2];
	FRenderTarget* RenderTargetBufferSceneColor[2];
	FRenderTarget* RenderTargetBufferA[2];
	FRenderTarget* RenderTargetBufferBCDEF[2];
};
#endif //WITH_EDITOR

class FPersistentUniformBuffers
{
public:
	FPersistentUniformBuffers() = default;

	void Initialize();
	void Clear();


	/** Mobile Directional Lighting uniform buffers, one for each lighting channel 
	  * The first is used for primitives with no lighting channels set.
	  */
	TUniformBufferRef<FMobileDirectionalLightShaderParameters> MobileDirectionalLightUniformBuffers[NUM_LIGHTING_CHANNELS+1];
	TUniformBufferRef<FMobileReflectionCaptureShaderParameters> MobileSkyReflectionUniformBuffer;
};

#if RHI_RAYTRACING
struct FMeshComputeDispatchCommand
{
	FMeshDrawShaderBindings ShaderBindings;
	TShaderRef<class FRayTracingDynamicGeometryConverterCS> MaterialShader;

	uint32 NumMaxVertices;
	FRWBuffer* TargetBuffer;
};

enum class ERayTracingMeshCommandsMode : uint8 {
	RAY_TRACING,
	PATH_TRACING,
	LIGHTMAP_TRACING,
};

#endif

struct FLumenSceneDataKey
{
	uint32 ViewUniqueId;		// Zero if not view specific
	uint32 GPUIndex;			// INDEX_NONE if not GPU specific

	friend FORCEINLINE bool operator == (const FLumenSceneDataKey& A, const FLumenSceneDataKey& B)
	{
		return A.ViewUniqueId == B.ViewUniqueId && A.GPUIndex == B.GPUIndex;
	}

	friend FORCEINLINE uint32 GetTypeHash(const FLumenSceneDataKey& Key)
	{
		return HashCombine(GetTypeHash(Key.ViewUniqueId), GetTypeHash(Key.GPUIndex));
	}
};

typedef TMap<FLumenSceneDataKey, FLumenSceneData*> FLumenSceneDataMap;

class FLumenSceneDataIterator
{
public:
	FLumenSceneDataIterator(const FScene* InScene);
	FLumenSceneDataIterator& operator++();

	FORCEINLINE explicit operator bool() const { return LumenSceneData != nullptr; }
	FORCEINLINE bool operator !() const { return LumenSceneData == nullptr; }
	FORCEINLINE FLumenSceneData* operator->() const { return LumenSceneData; }
	FORCEINLINE FLumenSceneData& operator*() const { return *LumenSceneData; }

private:
	const FScene* Scene;
	FLumenSceneData* LumenSceneData;
	FLumenSceneDataMap::TConstIterator NextSceneData;
};

/** 
 * Renderer scene which is private to the renderer module.
 * Ordinarily this is the renderer version of a UWorld, but an FScene can be created for previewing in editors which don't have a UWorld as well.
 * The scene stores renderer state that is independent of any view or frame, with the primary actions being adding and removing of primitives and lights.
 */
class FScene : public FSceneInterface
{
public:

	/** An optional world associated with the scene. */
	UWorld* World;

	/** An optional FX system associated with the scene. */
	class FFXSystemInterface* FXSystem;

	/** List of view states associated with the scene. */
	TArray<FSceneViewState*> ViewStates;

	FPersistentUniformBuffers UniformBuffers;

	/** Instancing state buckets.  These are stored on the scene as they are precomputed at FPrimitiveSceneInfo::AddToScene time. */
	FStateBucketMap CachedMeshDrawCommandStateBuckets[EMeshPass::Num];
	FCachedPassMeshDrawList CachedDrawLists[EMeshPass::Num];

#if RHI_RAYTRACING
	FCachedRayTracingMeshCommandStorage CachedRayTracingMeshCommands;

	/** Force a refresh of all cached ray tracing data in the scene (when path tracing mode changes or coarse mesh streaming for example). */
	void RefreshRayTracingMeshCommandCache();
	
	void RefreshRayTracingInstances();
#endif

	/* Mobile specific. Auxilary data for each cached command state bucket. This data can be used during mesh draw command batching */
	TArray<FStateBucketAuxData> CachedStateBucketsAuxData[EMeshPass::Num];

	/** Nanite shading material commands. These are stored on the scene as they are computed at FPrimitiveSceneInfo::AddToScene time. */
	FNaniteMaterialCommands NaniteMaterials[ENaniteMeshPass::Num];

	/** Nanite raster and shading pipelines. These are stored on the scene as they are computed at FPrimitiveSceneInfo::AddToScene time. */
	FNaniteRasterPipelines  NaniteRasterPipelines[ENaniteMeshPass::Num];
	FNaniteShadingPipelines NaniteShadingPipelines[ENaniteMeshPass::Num];

	/** Nanite material visibility references. These are stored on the scene as they are computed at FPrimitiveSceneInfo::AddToScene time. */
	FNaniteVisibility NaniteVisibility[ENaniteMeshPass::Num];

	/**
	 * The following arrays are densely packed primitive data needed by various
	 * rendering passes. PrimitiveSceneInfo->PackedIndex maintains the index
	 * where data is stored in these arrays for a given primitive.
	 */

	/** Packed array of primitives in the scene. */
	TArray<FPrimitiveSceneInfo*> Primitives;
	/** Packed array of all transforms in the scene. */
	TArray<FMatrix> PrimitiveTransforms;
	/** Packed array of primitive scene proxies in the scene. */
	TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	/** Packed array of primitive bounds. */
	TArray<FPrimitiveBounds> PrimitiveBounds;
	/** Packed array of primitive flags. */
	TArray<FPrimitiveFlagsCompact> PrimitiveFlagsCompact;
	/** Packed array of precomputed primitive visibility IDs. */
	TArray<FPrimitiveVisibilityId> PrimitiveVisibilityIds;
	/**Array of primitive octree node index**/
	TArray<uint32> PrimitiveOctreeIndex;
	/** Packed array of primitive occlusion flags. See EOcclusionFlags. */
	TArray<uint8> PrimitiveOcclusionFlags;
	/** Packed array of primitive occlusion bounds. */
	TArray<FBoxSphereBounds> PrimitiveOcclusionBounds;
	/** Packed array of primitive components associated with the primitive. */
	TArray<FPrimitiveComponentId> PrimitiveComponentIds;
	/** Packed array of runtime virtual texture flags. */
	TArray<FPrimitiveVirtualTextureFlags> PrimitiveVirtualTextureFlags;
	/** Packed array of runtime virtual texture lod info. */
	TArray<FPrimitiveVirtualTextureLodInfo> PrimitiveVirtualTextureLod;
#if RHI_RAYTRACING
	/** Packed array of ray tracing primitive caching flags*/
	TArray<ERayTracingPrimitiveFlags> PrimitiveRayTracingFlags;
	/** Packed array of ray tracing primitive group id hash indices. */
	TArray<Experimental::FHashElementId> PrimitiveRayTracingGroupIds;
	/** Aggregate bounds for all primitives which share a ray tracing group id. */
	struct FRayTracingCullingGroup
	{
		FBoxSphereBounds Bounds;
		float MinDrawDistance = 0.0f;
		TArray<FPrimitiveSceneInfo*> Primitives;
	};
	Experimental::TRobinHoodHashMap<int32, FRayTracingCullingGroup> PrimitiveRayTracingGroups;
#endif

	TMap<FName, TArray<FPrimitiveSceneInfo*>> PrimitivesNeedingLevelUpdateNotification;

#if WITH_EDITOR
	/** Packed bit array of primitives which are selected in the editor. */
	TBitArray<> PrimitivesSelected;
#endif

	TBitArray<> PrimitivesNeedingStaticMeshUpdate;
	TSet<FPrimitiveSceneInfo*> PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck;

	TArray<int32> PersistentPrimitiveIdToIndexMap;

	struct FTypeOffsetTableEntry
	{
		FTypeOffsetTableEntry(SIZE_T InPrimitiveSceneProxyType, uint32 InOffset) : PrimitiveSceneProxyType(InPrimitiveSceneProxyType), Offset(InOffset) {}
		SIZE_T PrimitiveSceneProxyType;
		uint32 Offset; //(e.g. prefix sum where the next type starts)
	};
	/* During insertion and deletion, used to skip large chunks of items of the same type */
	TArray<FTypeOffsetTableEntry> TypeOffsetTable;

	/** The lights in the scene. */
	using FLightSceneInfoCompactSparseArray = TSparseArray<FLightSceneInfoCompact, TAlignedSparseArrayAllocator<alignof(FLightSceneInfoCompact)>>;
	FLightSceneInfoCompactSparseArray Lights;

	/** 
	 * Lights in the scene which are invisible, but still needed by the editor for previewing. 
	 * Lights in this array cannot be in the Lights array.  They also are not fully set up, as AddLightSceneInfo_RenderThread is not called for them.
	 */
	FLightSceneInfoCompactSparseArray InvisibleLights;

	/** Shadow casting lights that couldn't get a shadowmap channel assigned and therefore won't have valid dynamic shadows, forward renderer only. */
	TArray<FString> OverflowingDynamicShadowedLights;

	/** Early Z pass mode. */
	EDepthDrawingMode EarlyZPassMode;

	/** Early Z pass movable. */
	bool bEarlyZPassMovable;

	/** Default base pass depth stencil access. */
	FExclusiveDepthStencil::Type DefaultBasePassDepthStencilAccess;

	/** Default base pass depth stencil access used to cache mesh draw commands. */
	FExclusiveDepthStencil::Type CachedDefaultBasePassDepthStencilAccess;

	/** True if a change to SkyLight / Lighting has occurred that requires static draw lists to be updated. */
	bool bScenesPrimitivesNeedStaticMeshElementUpdate;

	/** This counter will be incremented anytime something about the scene changed that should invalidate the path traced accumulation buffers. */
	TAtomic<uint32> PathTracingInvalidationCounter;

#if RHI_RAYTRACING
	/** What mode where the cached RT commands prepared for last? */
	ERayTracingMeshCommandsMode CachedRayTracingMeshCommandsMode;
#endif

	/** The scene's sky light, if any. */
	FSkyLightSceneProxy* SkyLight;

	/** Contains the sky env map irradiance as spherical harmonics. */
	TRefCountPtr<FRDGPooledBuffer> SkyIrradianceEnvironmentMap;

	/** The SkyView LUT used when rendering sky material sampling this lut into the realtime capture sky env map. It must be generated at the skylight position*/
	TRefCountPtr<IPooledRenderTarget> RealTimeReflectionCaptureSkyAtmosphereViewLutTexture;

	/** The Camera 360 AP is used when rendering sky material sampling this lut or volumetric clouds into the realtime capture sky env map. It must be generated at the skylight position*/
	TRefCountPtr<IPooledRenderTarget> RealTimeReflectionCaptureCamera360APLutTexture;

	/** If sky light bRealTimeCaptureEnabled is true, used to render the sky env map (sky, sky dome mesh or clouds). */
	TRefCountPtr<IPooledRenderTarget> CapturedSkyRenderTarget;	// Needs to be a IPooledRenderTarget because it must be created before the View uniform buffer is created.

	/** These store the result of the sky env map GGX specular convolution. */
	TRefCountPtr<IPooledRenderTarget> ConvolvedSkyRenderTarget[2];

	/** The index of the ConvolvedSkyRenderTarget to use when rendering meshes. -1 when not initialised. */
	int32 ConvolvedSkyRenderTargetReadyIndex;

	struct FRealTimeSlicedReflectionCapture
	{
		/** We always enforce a complete one the first frame even with time slicing for correct start up lighting.*/
		enum class EFirstFrameState
		{
			INIT = 0,
			FIRST_FRAME = 1,
			BEYOND_FIRST_FRAME = 2,
		} FirstFrameState = EFirstFrameState::INIT;

		/** The current progress of the real time reflection capture when time sliced. */
		int32 State = -1;

		/** The current progress of each sub step of a state capture of the real time reflection capture when time sliced. */
		int32 StateSubStep = 0;

		/** Keeps track of which GPUs have been initialized with a full cube map */
		uint32 GpusWithFullCube = 0;

		/** Keeps track of which GPUs calculations have been done in the frame */
		uint32 GpusHandledThisFrame = 0;

		/** Cache of Frame Number, used to detect first viewfamily */
		uint64 FrameNumber = uint64(-1);
	} RealTimeSlicedReflectionCapture;

	/**
	 * The path tracer uses its own representation of the skylight. These textures
	 * are updated lazily by the path tracer when missing. Any code that modifies
	 * the skylight appearance should simply reset these pointers.
	 * 
	 * We also remember the last used color so we can detect changes that would require rebuilding the tables
	 */
	TRefCountPtr<IPooledRenderTarget> PathTracingSkylightTexture;
	TRefCountPtr<IPooledRenderTarget> PathTracingSkylightPdf;
	FLinearColor PathTracingSkylightColor;

	/** Used to track the order that skylights were enabled in. */
	TArray<FSkyLightSceneProxy*> SkyLightStack;

	/** The directional light to use for simple dynamic lighting, if any. */
	FLightSceneInfo* SimpleDirectionalLight;

	/** For the mobile renderer, the first directional light in each lighting channel. */
	FLightSceneInfo* MobileDirectionalLights[NUM_LIGHTING_CHANNELS];

	/** The light sources for atmospheric effects, if any. */
	FLightSceneInfo* AtmosphereLights[NUM_ATMOSPHERE_LIGHTS];

	TArray<FLightSceneInfo*, TInlineAllocator<4>> DirectionalLights;

	/** The decals in the scene. */
	TSparseArray<FDeferredDecalProxy*> Decals;

	/** Potential capsule shadow casters registered to the scene. */
	TArray<FPrimitiveSceneInfo*> DynamicIndirectCasterPrimitives; 

	TArray<class FPlanarReflectionSceneProxy*> PlanarReflections;
	TArray<class UPlanarReflectionComponent*> PlanarReflections_GameThread;

	/** State needed for the reflection environment feature. */
	FReflectionEnvironmentSceneData ReflectionSceneData;

	/** The hair strands in the scene. */
	FHairStrandsSceneData HairStrandsSceneData;

	/** The OIT resources in the scene. */
	FOITSceneData OITSceneData;

	/** 
	 * Precomputed lighting volumes in the scene, used for interpolating dynamic object lighting.
	 * These are typically one per streaming level and they store volume lighting samples computed by Lightmass. 
	 */
	TArray<const FPrecomputedLightVolume*> PrecomputedLightVolumes;

	/** Interpolates and caches indirect lighting for dynamic objects. */
	FIndirectLightingCache IndirectLightingCache;

	FVolumetricLightmapSceneData VolumetricLightmapSceneData;
	
	FGPUScene GPUScene;

#if RHI_RAYTRACING
	/** Persistently-allocated ray tracing scene data. */
	FRayTracingScene RayTracingScene;
	FRayTracingScene HeterogeneousVolumesRayTracingScene;

	bool bHasRayTracedLights = false;
	void UpdateRayTracedLights();
#endif // RHI_RAYTRACING

	/** Distance field object scene data. */
	FDistanceFieldSceneData DistanceFieldSceneData;

	FLumenSceneData* DefaultLumenSceneData;
	FLumenSceneDataMap PerViewOrGPULumenSceneData;

	/** Map from light id to the cached shadowmap data for that light. */
	TMap<int32, TArray<FCachedShadowMapData>> CachedShadowMaps;
	
	/** Atlas HZB textures from the previous render. */
	TArray<TRefCountPtr<IPooledRenderTarget>>	PrevAtlasHZBs;
	TArray<TRefCountPtr<IPooledRenderTarget>>	PrevAtlasCompleteHZBs;

	TRefCountPtr<IPooledRenderTarget> PreShadowCacheDepthZ;

	/** Preshadows that are currently cached in the PreshadowCache render target. */
	TArray<TRefCountPtr<FProjectedShadowInfo> > CachedPreshadows;

	/**
	 * Virtual shadow maps can use a default cache stored in the FScene, or define a separate cache per view (AddVirtualShadowMapCache)
	 * for improved performance.  A linked list of caches for the FScene is maintained to allow scene updates to be propagated to all
	 * caches as needed.  If a cache is never used, the GPU resources for it are never allocated, so the overhead of the default cache
	 * should be minimal when not in use.
	 */
	FVirtualShadowMapArrayCacheManager* DefaultVirtualShadowMapCache;

	/** Texture layout that tracks current allocations in the PreshadowCache render target. */
	FTextureLayout PreshadowCacheLayout;

	/** The static meshes in the scene. */
	TSparseArray<FStaticMeshBatch*> StaticMeshes;
	/** The exponential fog components in the scene. */
	TArray<FExponentialHeightFogSceneInfo> ExponentialFogs;

	/** The sky/atmosphere components of the scene. */
	FSkyAtmosphereRenderSceneInfo* SkyAtmosphere;

	/** Used to track the order that skylights were enabled in. */
	TArray<FSkyAtmosphereSceneProxy*> SkyAtmosphereStack;

	/** The sky/atmosphere components of the scene. */
	FVolumetricCloudRenderSceneInfo* VolumetricCloud;

	/** Used to track the order that skylights were enabled in. */
	TArray<FVolumetricCloudSceneProxy*> VolumetricCloudStack;

	/** Global Field Manager */
	class FPhysicsFieldSceneProxy* PhysicsField = nullptr;

	/** The wind sources in the scene. */
	TArray<class FWindSourceSceneProxy*> WindSources;

	/** Wind source components, tracked so the game thread can also access wind parameters */
	TArray<UWindDirectionalSourceComponent*> WindComponents_GameThread;

	/** SpeedTree wind objects in the scene. FLocalVertexFactoryShaderParametersBase needs to lookup by FVertexFactory, but wind objects are per tree (i.e. per UStaticMesh)*/
	TMap<const UStaticMesh*, struct FSpeedTreeWindComputation*> SpeedTreeWindComputationMap;
	TMap<FVertexFactory*, const UStaticMesh*> SpeedTreeVertexFactoryMap;

	/** The attachment groups in the scene.  The map key is the attachment group's root primitive. */
	TMap<FPrimitiveComponentId,FAttachmentGroupSceneInfo> AttachmentGroups;

	/** Precomputed visibility data for the scene. */
	const FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler;

	/** An octree containing the shadow-casting local lights in the scene. */
	FSceneLightOctree LocalShadowCastingLightOctree;
	/** An array containing IDs of shadow-casting directional lights in the scene. */
	TArray<int32> DirectionalShadowCastingLightIDs;

	/** An octree containing the primitives in the scene. */
	FScenePrimitiveOctree PrimitiveOctree;

	FDynamicBVH<4> InstanceBVH;

	/** Indicates whether this scene requires hit proxy rendering. */
	bool bRequiresHitProxies;

	/** Whether this is an editor scene. */
	bool bIsEditorScene;

	/** Set by the rendering thread to signal to the game thread that the scene needs a static lighting build. */
	volatile mutable int32 NumUncachedStaticLightingInteractions;

	volatile mutable int32 NumUnbuiltReflectionCaptures;

	/** Track numbers of various lights types on mobile, used to show warnings for disabled shader permutations. */
	int32 NumMobileStaticAndCSMLights_RenderThread;
	int32 NumMobileMovableDirectionalLights_RenderThread;

	FSceneVelocityData VelocityData;

	/** GPU Skinning cache, if enabled */
	class FGPUSkinCache* GPUSkinCache;

	/* Array of registered compute work schedulers*/
	TArray<class IComputeTaskWorker*> ComputeTaskWorkers;

	/** Uniform buffers for parameter collections with the corresponding Ids. */
	TMap<FGuid, FUniformBufferRHIRef> ParameterCollections;

	/** LOD Tree Holder for massive LOD system */
	FLODSceneTree SceneLODHierarchy;

	/** The runtime virtual textures in the scene. */
	TSparseArray<FRuntimeVirtualTextureSceneProxy*> RuntimeVirtualTextures;

	/** Strata data shared between all views. */
	FStrataSceneData StrataSceneData;

	/** Mask used to determine whether primitives that draw to a runtime virtual texture should also be drawn in the main pass. */
	uint8 RuntimeVirtualTexturePrimitiveHideMaskEditor;
	uint8 RuntimeVirtualTexturePrimitiveHideMaskGame;

	float DefaultMaxDistanceFieldOcclusionDistance;

	float GlobalDistanceFieldViewDistance;

	float DynamicIndirectShadowsSelfShadowingIntensity;

	const FReadOnlyCVARCache& ReadOnlyCVARCache;

	FSpanAllocator PersistentPrimitiveIdAllocator;

#if WITH_EDITOR
	/** Editor Pixel inspector */
	FPixelInspectorData PixelInspectorData;
#endif //WITH_EDITOR

#if RHI_RAYTRACING
	class FRayTracingDynamicGeometryCollection* RayTracingDynamicGeometryCollection;
	class FRayTracingSkinnedGeometryUpdateQueue* RayTracingSkinnedGeometryUpdateQueue;
#endif

	/** Initialization constructor. */
	FScene(UWorld* InWorld, bool bInRequiresHitProxies,bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FScene();

	using FSceneInterface::UpdateAllPrimitiveSceneInfos;

	// FSceneInterface interface.
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) override;
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, bool bAsyncCreateLPIs = false) override;
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveInstances(UInstancedStaticMeshComponent* Primitive) override;
	virtual void UpdatePrimitiveOcclusionBoundsSlack(UPrimitiveComponent* Primitive, float NewSlack) override;
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) override;
	virtual void UpdateCustomPrimitiveData(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveDistanceFieldSceneData_GameThread(UPrimitiveComponent* Primitive) override;
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimitiveIndex) override;
	virtual bool GetPreviousLocalToWorld(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld) const override;
	virtual void AddLight(ULightComponent* Light) override;
	virtual void RemoveLight(ULightComponent* Light) override;
	virtual void AddInvisibleLight(ULightComponent* Light) override;
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) override;
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) override;
	virtual bool HasSkyLightRequiringLightingBuild() const override;
	virtual bool HasAtmosphereLightRequiringLightingBuild() const override;
	virtual void AddDecal(UDecalComponent* Component) override;
	virtual void RemoveDecal(UDecalComponent* Component) override;
	virtual void UpdateDecalTransform(UDecalComponent* Decal) override;
	virtual void UpdateDecalFadeOutTime(UDecalComponent* Decal) override;
	virtual void UpdateDecalFadeInTime(UDecalComponent* Decal) override;
	virtual void AddReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void RemoveReflectionCapture(UReflectionCaptureComponent* Component) override;
	virtual void GetReflectionCaptureData(UReflectionCaptureComponent* Component, class FReflectionCaptureData& OutCaptureData) override;
	virtual void UpdateReflectionCaptureTransform(UReflectionCaptureComponent* Component) override;
	virtual void ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent) override;
	virtual void AddPlanarReflection(class UPlanarReflectionComponent* Component) override;
	virtual void RemovePlanarReflection(UPlanarReflectionComponent* Component) override;
	virtual void UpdatePlanarReflectionTransform(UPlanarReflectionComponent* Component) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponent2D* CaptureComponent) override;
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponentCube* CaptureComponent) override;
	virtual void UpdatePlanarReflectionContents(UPlanarReflectionComponent* CaptureComponent, FSceneRenderer& MainSceneRenderer) override;
	virtual void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures, const TCHAR* CaptureReason, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick) override;
	virtual void ResetReflectionCaptures(bool bOnlyIfOOM) override;
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, UTextureCube* SourceCubemap, FTexture* OutProcessedTexture, float& OutAverageBrightness, FSHVectorRGB3& OutIrradianceEnvironmentMap, TArray<FFloat16Color>* OutRadianceMap) override; 
	virtual void AllocateAndCaptureFrameSkyEnvMap(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, FViewInfo& MainView, bool bShouldRenderSkyAtmosphere, bool bShouldRenderVolumetricCloud, FInstanceCullingManager& InstanceCullingManager) override;
	virtual void ValidateSkyLightRealTimeCapture(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture) override;
	virtual void AddPrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual void RemovePrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) override;
	virtual bool HasPrecomputedVolumetricLightmap_RenderThread() const override;
	virtual void AddPrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume, bool bIsPersistentLevel) override;
	virtual void RemovePrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) override;
	virtual void AddRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component) override;
	virtual void RemoveRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component) override;
	virtual void GetRuntimeVirtualTextureHidePrimitiveMask(uint8& bHideMaskEditor, uint8& bHideMaskGame) const override;
	virtual void InvalidateRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component, FBoxSphereBounds const& WorldBounds) override;
	virtual void InvalidatePathTracedOutput() override;
	virtual void InvalidateLumenSurfaceCache_GameThread(UPrimitiveComponent* Component) override;
	virtual void GetPrimitiveUniformShaderParameters_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool& bHasPrecomputedVolumetricLightmap, FMatrix& PreviousLocalToWorld, int32& SingleCaptureIndex, bool& bOutputVelocity) const override;
	virtual void UpdateLightTransform(ULightComponent* Light) override;
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) override;
	virtual void AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual void RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent) override;
	virtual bool HasAnyExponentialHeightFog() const override;

	virtual void AddHairStrands(FHairStrandsInstance* Proxy) override;
	virtual void RemoveHairStrands(FHairStrandsInstance* Proxy) override;

	virtual void GetRectLightAtlasSlot(const FRectLightSceneProxy* Proxy, FLightRenderParameters* Out) override;

	virtual void AddSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy, bool bStaticLightingBuilt) override;
	virtual void RemoveSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy) override;
	virtual FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() override { return SkyAtmosphere; }
	virtual const FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() const override { return SkyAtmosphere; }

	virtual void SetPhysicsField(class FPhysicsFieldSceneProxy* PhysicsFieldSceneProxy) override;
	virtual void ResetPhysicsField() override;
	virtual void ShowPhysicsField() override;
	virtual void UpdatePhysicsField(FRDGBuilder& GraphBuilder, FViewInfo& View) override;

	virtual void AddVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) override;
	virtual void RemoveVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) override;
	virtual FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() override { return VolumetricCloud; }
	virtual const FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() const override { return VolumetricCloud; }

	virtual void AddWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual void RemoveWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual void UpdateWindSource(UWindDirectionalSourceComponent* WindComponent) override;
	virtual const TArray<FWindSourceSceneProxy*>& GetWindSources_RenderThread() const override;
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override;
	virtual void AddSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void RemoveSpeedTreeWind_RenderThread(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh) override;
	virtual void UpdateSpeedTreeWind(double CurrentTime) override;
	virtual FRHIUniformBuffer* GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) const override;
	virtual void DumpUnbuiltLightInteractions( FOutputDevice& Ar ) const override;
	virtual void UpdateParameterCollections(const TArray<FMaterialParameterCollectionInstanceResource*>& InParameterCollections) override;

	virtual bool RequestGPUSceneUpdate(FPrimitiveSceneInfo& PrimitiveSceneInfo, EPrimitiveDirtyState PrimitiveDirtyState) override;

	FVirtualShadowMapArrayCacheManager* GetVirtualShadowMapCache(FSceneView& View) const;
	void GetAllVirtualShadowMapCacheManagers(TArray<FVirtualShadowMapArrayCacheManager*, SceneRenderingAllocator>& OutCacheManagers) const;

	FLumenSceneData* FindLumenSceneData(uint32 ViewKey, uint32 GPUIndex) const;
	inline FLumenSceneData* GetLumenSceneData(const FViewInfo& View) const
	{
		if (View.ViewLumenSceneData)
		{
			return View.ViewLumenSceneData;
		}
		else
		{
			return FindLumenSceneData(View.ViewState ? View.ViewState->GetViewKey() : 0, View.GPUMask.GetFirstIndex());
		}
	}
	inline FLumenSceneData* GetLumenSceneData(const FSceneView& View) const
	{
		// Should we assert that this is only called for FViewInfo (meaning inside scene renderer)?
		if (View.bIsViewInfo)
		{
			return GetLumenSceneData((const FViewInfo&)View);
		}
		else
		{
			return FindLumenSceneData(View.State ? View.State->GetViewKey() : 0, View.GPUMask.GetFirstIndex());
		}
	}

	bool HasSkyAtmosphere() const
	{
		return (SkyAtmosphere != nullptr);
	}
	bool HasVolumetricCloud() const
	{
		return (VolumetricCloud != nullptr);
	}

	bool IsSecondAtmosphereLightEnabled()
	{
		// If the second light is not null then we enable the second light.
		// We do not do any light1 to light0 remapping if light0 is null.
		return AtmosphereLights[1] != nullptr;
	}

	// Reset all the light to default state "not being affected by atmosphere". Should only be called from render side.
	void ResetAtmosphereLightsProperties();

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const override;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* InPrecomputedVisibilityHandler) override;

	/** Updates all static draw lists. */
	virtual void UpdateStaticDrawLists() override;

	/** Update render states that possibly cached inside renderer, like mesh draw commands. More lightweight than re-registering the scene proxy. */
	virtual void UpdateCachedRenderStates(FPrimitiveSceneProxy* SceneProxy) override;

	/** Updates PrimitivesSelected array for this PrimitiveSceneInfo */
	virtual void UpdatePrimitiveSelectedState_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsSelected) override;
	virtual void UpdatePrimitiveVelocityState_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsBeingMoved) override;

	virtual void Release() override;
	virtual UWorld* GetWorld() const override { return World; }

	/** Finds the closest reflection capture to a point in space, accounting influence radius */
	const FReflectionCaptureProxy* FindClosestReflectionCapture(FVector Position) const;

	const class FPlanarReflectionSceneProxy* FindClosestPlanarReflection(const FBoxSphereBounds& Bounds) const;

	const class FPlanarReflectionSceneProxy* GetForwardPassGlobalPlanarReflection() const;

	void FindClosestReflectionCaptures(FVector Position, const FReflectionCaptureProxy* (&SortedByDistanceOUT)[FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies]) const;

	int64 GetCachedWholeSceneShadowMapsSize() const;

	void UpdateEarlyZPassMode();

	/**
	 * Get the default base pass depth stencil access
	 */
	static FExclusiveDepthStencil::Type GetDefaultBasePassDepthStencilAccess(ERHIFeatureLevel::Type InFeatureLevel);

	/**
	 * Get the default base pass depth stencil access
	 */
	static void GetEarlyZPassMode(ERHIFeatureLevel::Type InFeatureLevel, EDepthDrawingMode& OutZPassMode, bool& bOutEarlyZPassMovable);

	/**
	 * Marks static mesh elements as needing an update if necessary.
	 */
	void ConditionalMarkStaticMeshElementsForUpdate();

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const override;

	SIZE_T GetSizeBytes() const;

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene() override
	{
		return this;
	}
	virtual void OnWorldCleanup() override;


	virtual void UpdateSceneSettings(AWorldSettings* WorldSettings) override;

	virtual class FGPUSkinCache* GetGPUSkinCache() override
	{
		return GPUSkinCache;
	}

	virtual void GetComputeTaskWorkers(TArray<class IComputeTaskWorker*>& OutWorkers) const override
	{
		OutWorkers = ComputeTaskWorkers;
	}

#if RHI_RAYTRACING
	virtual void UpdateCachedRayTracingState(class FPrimitiveSceneProxy* SceneProxy) override;
	virtual FRayTracingDynamicGeometryCollection* GetRayTracingDynamicGeometryCollection() override
	{
		return RayTracingDynamicGeometryCollection;
	}
	virtual FRayTracingSkinnedGeometryUpdateQueue* GetRayTracingSkinnedGeometryUpdateQueue() override
	{
		return RayTracingSkinnedGeometryUpdateQueue;
	}
#endif

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) override;

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() override;

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const override;

	FRHIUniformBuffer* GetParameterCollectionBuffer(const FGuid& InId) const
	{
		const FUniformBufferRHIRef* ExistingUniformBuffer = ParameterCollections.Find(InId);

		if (ExistingUniformBuffer)
		{
			return *ExistingUniformBuffer;
		}

		return nullptr;
	}

	virtual void ApplyWorldOffset(FVector InOffset) override;

	virtual void OnLevelAddedToWorld(FName InLevelName, UWorld* InWorld, bool bIsLightingScenario) override;
	virtual void OnLevelRemovedFromWorld(FName LevelRemovedName, UWorld* InWorld, bool bIsLightingScenario) override;

	virtual bool HasAnyLights() const override 
	{ 
		check(IsInGameThread());
		return NumVisibleLights_GameThread > 0 || NumEnabledSkylights_GameThread > 0; 
	}

	virtual bool IsEditorScene() const override { return bIsEditorScene; }

	bool ShouldRenderSkylightInBasePass(EBlendMode BlendMode) const
	{
		bool bRenderSkyLight = SkyLight && !SkyLight->bHasStaticLighting && !(ShouldRenderRayTracingSkyLight(SkyLight) && !IsForwardShadingEnabled(GetShaderPlatform()));

		if (IsTranslucentBlendMode(BlendMode))
		{
			// Both stationary and movable skylights are applied in base pass for translucent materials
			bRenderSkyLight = bRenderSkyLight
				&& (ReadOnlyCVARCache.bEnableStationarySkylight || !SkyLight->bWantsStaticShadowing);
		}
		else
		{
			// For opaque materials, stationary skylight is applied in base pass but movable skylight
			// is applied in a separate render pass (bWantssStaticShadowing means stationary skylight)
			bRenderSkyLight = bRenderSkyLight
				&& ((ReadOnlyCVARCache.bEnableStationarySkylight && SkyLight->bWantsStaticShadowing)
					|| (!SkyLight->bWantsStaticShadowing
						&& (IsForwardShadingEnabled(GetShaderPlatform()) || IsMobilePlatform(GetShaderPlatform()))));
		}

		return bRenderSkyLight;
	}

	virtual TArray<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const override
	{
		return PrimitiveComponentIds;
	}

	/** Get the scene index of the FRuntimeVirtualTextureSceneProxy associated with the producer. */
	uint32 GetRuntimeVirtualTextureSceneIndex(uint32 ProducerId);

	/** Flush any dirty runtime virtual texture pages */
	void FlushDirtyRuntimeVirtualTextures();

#if WITH_EDITOR
	virtual bool InitializePixelInspector(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDEF, int32 BufferIndex) override;

	virtual bool AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest) override;
#endif //WITH_EDITOR

	virtual void StartFrame() override
	{
		VelocityData.StartFrame(this);
	}

	virtual uint32 GetFrameNumber() const override
	{
		return SceneFrameNumber;
	}

	virtual void IncrementFrameNumber() override
	{
		++SceneFrameNumber;
	}

	/** Debug function to abtest lazy static mesh drawlists. */
	void UpdateDoLazyStaticMeshUpdate(FRHICommandListImmediate& CmdList);

	void DumpMeshDrawCommandMemoryStats();

	void CreateLightPrimitiveInteractionsForPrimitive(FPrimitiveSceneInfo* PrimitiveInfo, bool bAsyncCreateLPIs);

	FORCEINLINE TArray<FCachedShadowMapData>* GetCachedShadowMapDatas(int32 LightID)
	{
		return CachedShadowMaps.Find(LightID);
	}

	FORCEINLINE FCachedShadowMapData& GetCachedShadowMapDataRef(int32 LightID, int32 ShadowMapIndex = 0)
	{
		TArray<FCachedShadowMapData>& CachedShadowMapDatas = CachedShadowMaps.FindChecked(LightID);

		checkSlow(ShadowMapIndex >= 0 && ShadowMapIndex < CachedShadowMapDatas.Num());

		return CachedShadowMapDatas[ShadowMapIndex];
	}

	FORCEINLINE const FCachedShadowMapData& GetCachedShadowMapDataRef(int32 LightID, int32 ShadowMapIndex = 0) const
	{
		const TArray<FCachedShadowMapData>& CachedShadowMapDatas = CachedShadowMaps.FindChecked(LightID);

		checkSlow(ShadowMapIndex >= 0 && ShadowMapIndex < CachedShadowMapDatas.Num());

		return CachedShadowMapDatas[ShadowMapIndex];
	}

	FORCEINLINE const FCachedShadowMapData* GetCachedShadowMapData(int32 LightID, int32 ShadowMapIndex = 0) const
	{
		const TArray<FCachedShadowMapData>& CachedShadowMapDatas = CachedShadowMaps.FindChecked(LightID);

		checkSlow(ShadowMapIndex >= 0 && ShadowMapIndex < CachedShadowMapDatas.Num());

		return &CachedShadowMapDatas[ShadowMapIndex];
	}

	bool IsPrimitiveBeingRemoved(FPrimitiveSceneInfo* PrimitiveSceneInfo) const;

	/**
	 * Maximum used persistent Primitive Index, use to size arrays that store primitive data indexed by FPrimitiveSceneInfo::PersistentIndex.
	 * Only changes during UpdateAllPrimitiveSceneInfos.
	 */
	inline int32 GetMaxPersistentPrimitiveIndex() const { return PersistentPrimitiveIdAllocator.GetMaxSize(); }

	FORCEINLINE int32 GetPrimitiveIndex(const FPersistentPrimitiveIndex& PersistentPrimitiveIndex) const
	{ 
		if (PersistentPrimitiveIndex.Index < PersistentPrimitiveIdToIndexMap.Num())
		{
			return PersistentPrimitiveIdToIndexMap[PersistentPrimitiveIndex.Index];
		}
		return INDEX_NONE;
	}

	bool GetForceNoPrecomputedLighting() const
	{
		return bForceNoPrecomputedLighting;
	}

	FLumenSceneDataIterator GetLumenSceneDataIterator() const
	{
		return FLumenSceneDataIterator(this);
	}

	void LumenAddPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenUpdatePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenInvalidateSurfaceCacheForPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenRemovePrimitive(FPrimitiveSceneInfo* InPrimitive, int32 PrimitiveIndex);

protected:

private:
	void RemoveViewLumenSceneData_RenderThread(FSceneViewStateInterface* ViewState);
	void RemoveViewState_RenderThread(FSceneViewStateInterface*);

	/**
	 * Ensures the packed primitive arrays contain the same number of elements.
	 */
	void CheckPrimitiveArrays(int MaxTypeOffsetIndex = -1);

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 * Render thread version of function.
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	void GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const;

	/**
	 * Adds a primitive to the scene.  Called in the rendering thread by AddPrimitive.
	 * @param PrimitiveSceneInfo - The primitive being added.
	 */
	void AddPrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, const TOptional<FTransform>& PreviousTransform);

	/**
	 * Removes a primitive from the scene.  Called in the rendering thread by RemovePrimitive.
	 * @param PrimitiveSceneInfo - The primitive being removed.
	 */
	void RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Updates a primitive's transform, called on the rendering thread. */
	void UpdatePrimitiveTransform_RenderThread(FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition, const TOptional<FTransform>& PreviousTransform);

	void UpdatePrimitiveOcclusionBoundsSlack_RenderThread(const FPrimitiveSceneProxy* PrimitiveSceneProxy, float NewSlack);

	/** Updates a single primitive's lighting attachment root. */
	void UpdatePrimitiveLightingAttachmentRoot(UPrimitiveComponent* Primitive);

	void AssignAvailableShadowMapChannelForLight(FLightSceneInfo* LightSceneInfo);

	/**
	 * Adds a light to the scene.  Called in the rendering thread by AddLight.
	 * @param LightSceneInfo - The light being added.
	 */
	void AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	/**
	 * Adds a decal to the scene.  Called in the rendering thread by AddDecal or RemoveDecal.
	 * @param Component - The object that should being added or removed.
	 * @param bAdd true:add, FASLE:remove
	 */
	void AddOrRemoveDecal_RenderThread(FDeferredDecalProxy* Component, bool bAdd);

	/**
	 * Removes a light from the scene.  Called in the rendering thread by RemoveLight.
	 * @param LightSceneInfo - The light being removed.
	 */
	void RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo);

	void UpdateLightTransform_RenderThread(FLightSceneInfo* LightSceneInfo, const struct FUpdateLightTransformParameters& Parameters);

	/** 
	* Updates the contents of the given reflection capture by rendering the scene. 
	* This must be called on the game thread.
	*/
	void CaptureOrUploadReflectionCapture(UReflectionCaptureComponent* CaptureComponent, int32 ReflectionCaptureSize, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick);

	/** Updates the contents of all reflection captures in the scene.  Must be called from the game thread. */
	void UpdateAllReflectionCaptures(const TCHAR* CaptureReason, int32 ReflectionCaptureSize, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick);

	/** Updates all static draw lists. */
	void UpdateStaticDrawLists_RenderThread(FRHICommandListImmediate& RHICmdList);

	/** Add a runtime virtual texture proxy to the scene. Called in the rendering thread by AddRuntimeVirtualTexture. */
	void AddRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy);
	/** Update a runtime virtual texture proxy to the scene. Called in the rendering thread by AddRuntimeVirtualTexture. */
	void UpdateRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy, FRuntimeVirtualTextureSceneProxy* SceneProxyToReplace);
	/** Remove a runtime virtual texture proxy from the scene. Called in the rendering thread by RemoveRuntimeVirtualTexture. */
	void RemoveRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy);
	/** Update the scene primitive data after completing operations that add or remove runtime virtual textures. This can be slow and should be called rarely. */
	void UpdateRuntimeVirtualTextureForAllPrimitives_RenderThread();

	/**
	 * Shifts scene data by provided delta
	 * Called on world origin changes
	 * 
	 * @param	InOffset	Delta to shift scene by
	 */
	void ApplyWorldOffset_RenderThread(const FVector& InOffset);

	/**
	 * Notification from game thread that level was added to a world
	 *
	 * @param	InLevelName		Level name
	 */
	void OnLevelAddedToWorld_RenderThread(FName InLevelName);

	/**
	 * Notification from game thread that level was removed from a world
	 *
	 * @param	InLevelName		Level name
	 */
	void OnLevelRemovedFromWorld_RenderThread(FName InLevelName);

	void ProcessAtmosphereLightRemoval_RenderThread(FLightSceneInfo* LightSceneInfo);
	void ProcessAtmosphereLightAddition_RenderThread(FLightSceneInfo* LightSceneInfo);

private:
#if RHI_RAYTRACING
	void UpdateRayTracingGroupBounds_AddPrimitives(const Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*>& PrimitiveSceneInfos);
	void UpdateRayTracingGroupBounds_RemovePrimitives(const Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*>& PrimitiveSceneInfos);
	template<typename ValueType>
	inline void UpdateRayTracingGroupBounds_UpdatePrimitives(const Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, ValueType>& UpdatedTransforms);
#endif

	struct FUpdateTransformCommand
	{
		FBoxSphereBounds WorldBounds;
		FBoxSphereBounds LocalBounds; 
		FMatrix LocalToWorld; 
		FVector AttachmentRootPosition;
	};

	struct FUpdateInstanceCommand
	{
		FPrimitiveSceneProxy* PrimitiveSceneProxy{ nullptr };
		FInstanceUpdateCmdBuffer CmdBuffer;
		FBoxSphereBounds WorldBounds;
		FBoxSphereBounds LocalBounds;
		FBoxSphereBounds StaticMeshBounds;
	};

	Experimental::TRobinHoodHashMap<FPrimitiveSceneInfo*, FPrimitiveComponentId> UpdatedAttachmentRoots;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FCustomPrimitiveData> UpdatedCustomPrimitiveParams;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FUpdateTransformCommand> UpdatedTransforms;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FUpdateInstanceCommand> UpdatedInstances;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneInfo*, FMatrix> OverridenPreviousTransforms;
	Experimental::TRobinHoodHashMap<const FPrimitiveSceneProxy*, float> UpdatedOcclusionBoundsSlacks;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> AddedPrimitiveSceneInfos;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> RemovedPrimitiveSceneInfos;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> DistanceFieldSceneDataUpdates;

	/** 
	 * The number of visible lights in the scene
	 * Note: This is tracked on the game thread!
	 */
	int32 NumVisibleLights_GameThread;

	/** 
	 * Whether the scene has a valid sky light.
	 * Note: This is tracked on the game thread!
	 */
	int32 NumEnabledSkylights_GameThread;

	/** Frame number incremented per-family viewing this scene. */
	uint32 SceneFrameNumber;

	/** Whether world settings has bForceNoPrecomputedLighting set */
	bool bForceNoPrecomputedLighting;

	friend class FSceneViewState;
};

inline bool ShouldIncludeDomainInMeshPass(EMaterialDomain Domain)
{
	// Non-Surface domains can be applied to static meshes for thumbnails or material editor preview
	// Volume domain materials however must only be rendered in the voxelization pass
	return Domain != MD_Volume;
}

#include "BasePassRendering.inl" // IWYU pragma: export

