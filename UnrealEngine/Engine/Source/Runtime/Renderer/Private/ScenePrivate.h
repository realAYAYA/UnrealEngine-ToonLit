// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScenePrivate.h: Private scene manager definitions.
=============================================================================*/

#pragma once

// Dependencies.

#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "Templates/PimplPtr.h"
#include "Templates/RefCounting.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "RendererInterface.h"
#include "SceneUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TextureLayout3d.h"
#include "ScenePrivateBase.h"
#include "RenderTargetPool.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "DepthRendering.h"
#include "SceneHitProxyRendering.h"
#include "TextureLayout.h"
#include "SceneRendering.h"
#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "VolumeRendering.h"
#include "CommonRenderResources.h"
#include "VisualizeTexture.h"
#include "UnifiedBuffer.h"
#include "DebugViewModeRendering.h"
#include "PrecomputedVolumetricLightmap.h"
#include "RayTracing/RaytracingOptions.h"
#if RHI_RAYTRACING
#include "RayTracing/RayTracingScene.h"
#endif
#include "Nanite/Nanite.h"
#include "Lumen/LumenViewState.h"
#include "ManyLights/ManyLightsViewState.h"
#include "VolumetricRenderTargetViewStateData.h"
#include "GPUScene.h"
#include "DynamicBVH.h"
#include "OIT/OIT.h"
#include "ShadingEnergyConservation.h"
#include "Substrate/Glint/GlintShadingLUTs.h"
#include "Experimental/Containers/RobinHoodHashTable.h"
#include "SpanAllocator.h"
#include "GlobalDistanceField.h"
#include "Algo/RemoveIf.h"
#include "UObject/Package.h"
#include "LightFunctionAtlas.h"
#include "SceneExtensions.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"

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
class FDistanceFieldObjectBuffers;
struct FHairStrandsInstance;
struct FPathTracingState;
class FSparseVolumeTextureViewerSceneProxy;
class FExponentialHeightFogSceneInfo;
class FStaticMeshBatch;
class FShadowScene;
class FSceneLightInfoUpdates;
class FSceneCulling;

/**
 * Describes all light modifications to the scene by recording the light scene IDs.
 * TODO: If needed, we could add a reference to the FLightUpdates (which contains the commands) since this would enable systems to consume out the delta updates as they come in.
 *       If this is useful we must ensure FLightUpdates are kept alive as long as any async tasks might require.
 */
struct FLightSceneChangeSet
{
	// IDs of all lights before they were removed, IDs in this array may not be valid at all times when the change-set is used (depends on whether the callback site is before or after the given lights are removed from the scene).
	TConstArrayView<int32> RemovedLightIds;
	// IDs of all lights added to the scene, only available after all lights are added to the scene, may contain the same ID's as removed, as they may be reused.
	TConstArrayView<int32> AddedLightIds;
	// IDs of updated lights, does not contain any from the above, since 'add' implies the update of all aspects and 'remove' implies cancellation of all updates. 
	// The updated arrays are not disjoint as a light may have both types of update applied.
	TConstArrayView<int32> TransformUpdatedLightIds;
	TConstArrayView<int32> ColorUpdatedLightIds;
};

/**
 * Change set that is valid before removes are processed and the scene data modified.
 * The referenced arrays have RDG life-time and can be safely used in RDG tasks.
 * However, the referenced data (primitive/proxy) and meaning of the persistent ID is not generally valid past the call in which this is passed. 
 * Thus, care need to be excercised.
 */
class FScenePreUpdateChangeSet
{
public:
	TConstArrayView<FPersistentPrimitiveIndex> RemovedPrimitiveIds;
	TConstArrayView<FPrimitiveSceneInfo*> RemovedPrimitiveSceneInfos;
	TConstArrayView<FPersistentPrimitiveIndex> UpdatedPrimitiveIds;
	TConstArrayView<FPrimitiveSceneInfo*> UpdatedPrimitiveSceneInfos;
};

/**
 * Change set that is valid before after adds are processed and the scene data is modified.
 * The referenced arrays have RDG life-time and can be safely used in RDG tasks.
 */
class FScenePostUpdateChangeSet
{
public:
	TConstArrayView<FPersistentPrimitiveIndex> AddedPrimitiveIds;
	TConstArrayView<FPrimitiveSceneInfo*> AddedPrimitiveSceneInfos;
	TConstArrayView<FPersistentPrimitiveIndex> UpdatedPrimitiveIds;
	TConstArrayView<FPrimitiveSceneInfo*> UpdatedPrimitiveSceneInfos;
};

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

	explicit FPrimitiveOcclusionHistoryKey(const FPrimitiveOcclusionHistory& Element)
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

inline uint32 GetTypeHash(const FPrimitiveOcclusionHistoryKey& Key)
{
	return GetTypeHash(Key.PrimitiveId.PrimIDValue) ^ (GetTypeHash(Key.CustomIndex) >> 20);
}

inline bool operator==(const FPrimitiveOcclusionHistoryKey& A, const FPrimitiveOcclusionHistoryKey& B)
{
	return A.PrimitiveId == B.PrimitiveId && A.CustomIndex == B.CustomIndex;
}

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
		return A == B;
	}

	static uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
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
	void InitContents()
	{
		FDistanceCullFadeUniformShaderParameters Parameters;
		Parameters.FadeTimeScaleBias.X = 0.0f;
		Parameters.FadeTimeScaleBias.Y = 1.0f;
		SetContents(FRenderResource::GetImmediateCommandList(), Parameters);
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
	void InitContents()
	{
		FDitherUniformShaderParameters Parameters;
		Parameters.LODFactor = 0.0f;
		SetContents(FRenderResource::GetImmediateCommandList(), Parameters);
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
	TArray<FBox> PrimitiveModifiedBounds;
};

class FGlobalDistanceFieldClipmapState
{
public:

	FGlobalDistanceFieldClipmapState()
	{
		FullUpdateOriginInPages = FInt64Vector::ZeroValue;
		LastPartialUpdateOriginInPages = FInt64Vector::ZeroValue;
		CachedClipmapCenter = FVector3f(0.0f, 0.0f, 0.0f);
		CachedClipmapExtent = 0.0f;
		CacheClipmapInfluenceRadius = 0.0f;
		CacheMostlyStaticSeparately = 1;
		LastUsedSceneDataForFullUpdate = nullptr;
	}

	FInt64Vector FullUpdateOriginInPages;
	FInt64Vector LastPartialUpdateOriginInPages;
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
		uint32 Current = CurrentSample.fetch_add(1);

		if (Current >= NumSamples)
		{
			Current++;
			CurrentSample.compare_exchange_strong(Current, 0);
			Current = 0;
			// It is intended here to not check if exchange worked or failed. 
			// It might be overkill to call recursively GetFraction if exchange failed. 
			// Another thread might already have reset CurrentSample and it is acceptable 
			// to have two threads returning the same Fraction at index 0
		}
		float Fraction = Samples[Current];
		return Fraction;
	}
private:

	/** Index of the last sample we produced **/
	std::atomic<uint32> CurrentSample;
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

	void InitialiseOrNextFrame(ERHIFeatureLevel::Type FeatureLevel, FPooledRenderTargetDesc& AerialPerspectiveDesc, FRHICommandListImmediate& RHICmdList, bool bSeparatedAtmosphereMieRayLeigh);

	TRefCountPtr<IPooledRenderTarget> GetCurrentCameraAerialPerspectiveVolume();
	TRefCountPtr<IPooledRenderTarget> GetCurrentCameraAerialPerspectiveVolumeMieOnly();
	TRefCountPtr<IPooledRenderTarget> GetCurrentCameraAerialPerspectiveVolumeRayOnly();

	uint64 GetGPUSizeBytes(bool bLogSizes) const;

private:
	bool bInitialised;
	int32 CurrentScreenResolution;
	int32 CurrentDepthResolution;
	EPixelFormat CurrentTextureAerialLUTFormat;
	TRefCountPtr<IPooledRenderTarget> CameraAerialPerspectiveVolumes[2];
	TRefCountPtr<IPooledRenderTarget> CameraAerialPerspectiveVolumesMieOnly[2];
	TRefCountPtr<IPooledRenderTarget> CameraAerialPerspectiveVolumesRayOnly[2];
	uint8 CameraAerialPerspectiveVolumeCount;
	uint8 CameraAerialPerspectiveVolumeIndex;
	bool bSeparatedAtmosphereMieRayLeigh;
};

struct FGlobalDistanceFieldStreamingReadback
{
	FGlobalDistanceFieldStreamingReadback()
	{
		PendingStreamingReadbackBuffers.SetNum(MaxPendingStreamingReadbackBuffers);
	}

	TArray<TUniquePtr<FRHIGPUBufferReadback>> PendingStreamingReadbackBuffers;
	uint32 MaxPendingStreamingReadbackBuffers = 4;
	uint32 ReadbackBuffersWriteIndex = 0;
	uint32 ReadbackBuffersNumPending = 0;
};

struct FPersistentGlobalDistanceFieldData : public FThreadSafeRefCountedObject
{
	// Array of ClipmapIndex
	TArray<int32> DeferredUpdates[GDF_Num];
	TArray<int32> DeferredUpdatesForMeshSDFStreaming[GDF_Num];

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

	FGlobalDistanceFieldStreamingReadback StreamingReadback[GDF_Num];

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

class FOcclusionFeedback : public FRenderResource
{
public:
	FOcclusionFeedback();
	~FOcclusionFeedback();

	// FRenderResource interface
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	void AddPrimitive(const FPrimitiveOcclusionHistoryKey& PrimitiveKey, const FVector& BoundsOrigin, const FVector& BoundsBoxExtent, FGlobalDynamicVertexBuffer& DynamicVertexBuffer);

	void BeginOcclusionScope(FRDGBuilder& GraphBuilder);
	void EndOcclusionScope(FRDGBuilder& GraphBuilder);

	/** Renders the current batch and resets the batch state. */
	void SubmitOcclusionDraws(FRHICommandList& RHICmdList, FViewInfo& View);

	void ReadbackResults(FRHICommandList& RHICmdList);
	void AdvanceFrame(uint32 OcclusionFrameCounter);

	inline FRDGBuffer* GetGPUFeedbackBuffer() const
	{
		return GPUFeedbackBuffer;
	}

	inline bool IsOccluded(const FPrimitiveOcclusionHistoryKey& PrimitiveKey) const
	{
		return LatestOcclusionResults.Contains(PrimitiveKey);
	}

private:
	struct FOcclusionBatch
	{
		FGlobalDynamicVertexBuffer::FAllocation VertexAllocation;
		uint32 NumBatchedPrimitives;
	};

	/** The pending batches. */
	TArray<FOcclusionBatch, TInlineAllocator<3>> BatchOcclusionQueries;

	FRDGBuffer* GPUFeedbackBuffer{};

	struct FOcclusionBuffer
	{
		TArray<FPrimitiveOcclusionHistoryKey> BatchedPrimitives;
		FRHIGPUBufferReadback* ReadbackBuffer = nullptr;
		uint32 OcclusionFrameCounter = 0u;
	};

	FOcclusionBuffer OcclusionBuffers[3];
	uint32 CurrentBufferIndex;

	TSet<FPrimitiveOcclusionHistoryKey> LatestOcclusionResults;
	uint32 ResultsOcclusionFrameCounter;

	//
	FVertexDeclarationRHIRef OcclusionVertexDeclarationRHI;
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

		FProjectedShadowKey(const FProjectedShadowInfo& ProjectedShadowInfo);

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
	 * Whenever a ViewState and Scene get linked, this pointer is set, and a pointer to the ViewState is added to an array in the Scene.
	 * The linking is necessary in cases where incremental FScene updates need to be reflected in cached data stored in FSceneViewState.
	 */
	FScene* Scene;

	typedef TMap<FSceneViewState::FProjectedShadowKey, FRHIPooledRenderQuery> ShadowKeyOcclusionQueryMap;
	TArray<ShadowKeyOcclusionQueryMap, TInlineAllocator<FOcclusionQueryHelpers::MaxBufferedOcclusionFrames> > ShadowOcclusionQueryMaps;

	/** The view's occlusion query pool. */
	FRenderQueryPoolRHIRef OcclusionQueryPool;
	FFrameBasedOcclusionQueryPool PrimitiveOcclusionQueryPool;

	FHZBOcclusionTester HZBOcclusionTests;
	FOcclusionFeedback OcclusionFeedback;

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

	// A counter incremented once each time this view is rendered.
	uint32 OcclusionFrameCounter;

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

	/** The current frame PreExposure */
	float PreExposure;

	/** Whether to get the last exposure from GPU */
	bool bUpdateLastExposure;

private:
	
	// to implement eye adaptation / auto exposure changes over time
	class FEyeAdaptationManager
	{
	public:
		FEyeAdaptationManager();

		void SafeRelease();

		/** Get the last frame exposure value (used to compute pre-exposure) */
		float GetLastExposure() const { return LastExposure; }

		/** Get the last frame average local exposure approximation value (used to compute pre-exposure) */
		float GetLastAverageLocalExposure() const { return LastAverageLocalExposure; }

		/** Get the last frame average scene luminance (used for exposure compensation curve) */
		float GetLastAverageSceneLuminance() const { return LastAverageSceneLuminance; }

		UE_DEPRECATED(5.2, "Use GetCurrentBuffer() instead.")
		const TRefCountPtr<IPooledRenderTarget>& GetCurrentTexture() const
		{
			return GetTexture(CurrentBufferIndex);
		}

		UE_DEPRECATED(5.2, "Use GetCurrentBuffer() instead.")
		const TRefCountPtr<IPooledRenderTarget>& GetCurrentTexture(FRDGBuilder& GraphBuilder)
		{
			return GetOrCreateTexture(GraphBuilder, CurrentBufferIndex);
		}

		const TRefCountPtr<FRDGPooledBuffer>& GetCurrentBuffer() const
		{
			return GetBuffer(CurrentBufferIndex);
		}

		const TRefCountPtr<FRDGPooledBuffer>& GetCurrentBuffer(FRDGBuilder& GraphBuilder)
		{
			return GetOrCreateBuffer(GraphBuilder, CurrentBufferIndex);
		}

		void SwapBuffers();
		void UpdateLastExposureFromBuffer();
		void EnqueueExposureBufferReadback(FRDGBuilder& GraphBuilder);

		uint64 GetGPUSizeBytes(bool bLogSizes) const;

	private:
		const TRefCountPtr<IPooledRenderTarget>& GetTexture(uint32 TextureIndex) const;
		const TRefCountPtr<IPooledRenderTarget>& GetOrCreateTexture(FRDGBuilder& GraphBuilder, uint32 TextureIndex);

		const TRefCountPtr<FRDGPooledBuffer>& GetBuffer(uint32 BufferIndex) const;
		const TRefCountPtr<FRDGPooledBuffer>& GetOrCreateBuffer(FRDGBuilder& GraphBuilder, uint32 BufferIndex);

		FRHIGPUBufferReadback* GetLatestReadbackBuffer();

		// TODO: Do we need to double buffer?
		// - for readback we copy data to readback buffers
		// - do we ever need to access prev frame exposure AFTER current frame exposure has been calculated?
		// - should at least make it more explicit/safe by having GetCurrentBuffer() and GetPreviousBuffer()
		//		and assert if current is accessed too early in frame.
		static const int32 NUM_BUFFERS = 2;

		static const int32 EXPOSURE_BUFFER_SIZE_IN_VECTOR4 = 2;

		int32 CurrentBufferIndex = 0;

		float LastExposure = 0;
		float LastAverageLocalExposure = 1.0f;
		float LastAverageSceneLuminance = 0; // 0 means invalid. Used for Exposure Compensation Curve.

		UE_DEPRECATED(5.2, "Use ExposureBufferData instead.")
		TRefCountPtr<IPooledRenderTarget> PooledRenderTarget[NUM_BUFFERS];

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
	TArray<TObjectPtr<UMaterialInstanceDynamic>> MIDPool;
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
	FManyLightsViewState ManyLights;

	// Heterogeneous Volumes cached data stores
	TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters> OrthoVoxelGridUniformBuffer = nullptr;
	TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters> FrustumVoxelGridUniformBuffer = nullptr;

	TRDGUniformBufferRef<FAdaptiveVolumetricShadowMapUniformBufferParameters> AdaptiveVolumetricCameraMapUniformBuffer = nullptr;
	TMap<int32, TRDGUniformBufferRef<FAdaptiveVolumetricShadowMapUniformBufferParameters>> AdaptiveVolumetricShadowMapUniformBufferMap;

	// Map from Light ID in GPU Scene to index in the View's ForwardLightData array
	// This is stored in ViewState so we can access previous frame mapping
	TMap<int32, int32> LightSceneIdToLocalLightIndex;

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
		TObjectPtr<UTexture2D> Physical = nullptr;
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

		TObjectPtr<UTexture2D> Texture = nullptr;
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

	// Ray Traced Sky Light Sample Direction Data
	TRefCountPtr<FRDGPooledBuffer> SkyLightVisibilityRaysBuffer;
	FIntVector SkyLightVisibilityRaysDimensions;

	// Last valid RTPSO is saved, so it could be used as fallback in future frames if background PSO compilation is enabled.
	// This RTPSO can be used only if the only difference from previous PSO is the material hit shaders.
	FRayTracingPipelineStateSignature LastRayTracingMaterialPipelineSignature;

	// List of landscape ray tracing state associated with this view, so it can be cleaned up if the view gets deleted.
	TPimplPtr<FLandscapeRayTracingStateList> LandscapeRayTracingStates;

	virtual void SetLandscapeRayTracingStates(TPimplPtr<FLandscapeRayTracingStateList>&& InLandscapeRayTracingStates) final { LandscapeRayTracingStates = MoveTemp(InLandscapeRayTracingStates); }
	virtual FLandscapeRayTracingStateList* GetLandscapeRayTracingStates() const final { return LandscapeRayTracingStates.Get(); }
#endif

	TUniquePtr<FForwardLightingViewResources> ForwardLightingResources;

	float LightScatteringHistoryPreExposure;
	FVector2f PrevLightScatteringViewGridUVToViewRectVolumeUV;
	FVector2f VolumetricFogPrevViewGridRectUVToResourceUV;
	FVector2f VolumetricFogPrevUVMax;
	FVector2f VolumetricFogPrevUVMaxForTemporalBlend;
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

	virtual FRDGTextureRef GetVolumetricCloudTexture(FRDGBuilder& GraphBuilder) override
	{
		return VolumetricCloudRenderTarget.GetDstVolumetricReconstructRT(GraphBuilder);
	}

	FHairStrandsViewStateData HairStrandsViewStateData;

	FShaderPrintStateData ShaderPrintStateData;

	FShadingEnergyConservationStateData ShadingEnergyConservationData;

	FGlintShadingLUTsStateData GlintShadingLUTsData;

	bool bLumenSceneDataAdded;
	float LumenSurfaceCacheResolution;

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

		ReleaseRHI();
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

	UE_DEPRECATED(5.2, "Use GetCurrentEyeAdaptationBuffer() instead.")
	IPooledRenderTarget* GetCurrentEyeAdaptationTexture() const override final
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		IPooledRenderTarget* Texture = EyeAdaptationManager.GetCurrentTexture().GetReference();
		check(bValidEyeAdaptationTexture && Texture);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		return Texture;
	}

	UE_DEPRECATED(5.2, "Use GetCurrentEyeAdaptationTexture() instead.")
	IPooledRenderTarget* GetCurrentEyeAdaptationTexture(FRDGBuilder& GraphBuilder)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bValidEyeAdaptationTexture = true;
		return EyeAdaptationManager.GetCurrentTexture(GraphBuilder).GetReference();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

	float GetLastEyeAdaptationExposure() const
	{
		return EyeAdaptationManager.GetLastExposure();
	}

	float GetLastAverageLocalExposure() const
	{
		return EyeAdaptationManager.GetLastAverageLocalExposure();
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
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		HZBOcclusionTests.InitRHI(RHICmdList);
	}

	virtual void ReleaseRHI() override
	{
		HZBOcclusionTests.ReleaseRHI();
		EyeAdaptationManager.SafeRelease();
		OcclusionFeedback.ReleaseResource();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		bValidEyeAdaptationTexture = false;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
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

		check(!FApp::CanEverRender() || NewMID->GetRenderProxy());
		MIDUsedCount++;
		return NewMID;
	}

	virtual void ClearMIDPool(FStringView MidParentRootPath = {}) override
	{
		check(IsInGameThread());
		if (MidParentRootPath.IsEmpty())
		{
			MIDPool.Empty();
			return;
		}

		FNameBuilder PackagePath;
		const int32 RemoveNum = Algo::RemoveIf(MIDPool, [&PackagePath, MidParentRootPath](UMaterialInstanceDynamic* MID) -> bool
		{
			if (MID->Parent)
			{
				MID->Parent->GetPackage()->GetFName().ToString(PackagePath);
				return FStringView(PackagePath).StartsWith(MidParentRootPath);
			}
			return false;
		});

		MIDPool.SetNum(RemoveNum);
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

	virtual void AddLumenSceneData(FSceneInterface* InScene, float SurfaceCacheResolution) override;
	virtual void RemoveLumenSceneData(FSceneInterface* InScene) override;
	virtual bool HasLumenSceneData() const override;

	struct FOcclusion
	{
		/** Information about visibility/occlusion states in past frames for individual primitives. */
		TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs> PrimitiveOcclusionHistorySet;

		/** The last occlusion query of last frame to test in the following frame to block the GPU. */
		FRHIRenderQuery* LastOcclusionQuery = nullptr;

		/** The number of queries requested last frame. */
		uint32 NumRequestedQueries = 0;
	
	} Occlusion;
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

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

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
	FDFVector3 Position;
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
	FHairTransientResources* TransientResources = nullptr;
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
	: Primitive(InPrimitive)
	, InstanceIndex(InInstanceIndex)
	{
		SetTransformAndBounds(InLocalToWorld, InWorldBounds);
	}

	FPrimitiveSceneInfo* Primitive;

	FVector Origin;
	FVector3f TransformRows[3];

	FBox3f WorldBoundsRelativeToOrigin;

	int32 InstanceIndex;

	FORCEINLINE void SetTransformAndBounds(const FMatrix& InLocalToWorld, const FBox& InWorldBounds)
	{
		TransformRows[0] = FVector3f((float)InLocalToWorld.M[0][0], (float)InLocalToWorld.M[0][1], (float)InLocalToWorld.M[0][2]);
		TransformRows[1] = FVector3f((float)InLocalToWorld.M[1][0], (float)InLocalToWorld.M[1][1], (float)InLocalToWorld.M[1][2]);
		TransformRows[2] = FVector3f((float)InLocalToWorld.M[2][0], (float)InLocalToWorld.M[2][1], (float)InLocalToWorld.M[2][2]);
		Origin = FVector(InLocalToWorld.M[3][0], InLocalToWorld.M[3][1], InLocalToWorld.M[3][2]);

		WorldBoundsRelativeToOrigin = (FBox3f)(InWorldBounds.ShiftBy(-Origin));
	}

	FORCEINLINE FMatrix GetLocalToWorld() const
	{
		FMatrix Matrix;
		Matrix.M[0][0] = TransformRows[0].X;
		Matrix.M[0][1] = TransformRows[0].Y;
		Matrix.M[0][2] = TransformRows[0].Z;
		Matrix.M[0][3] = 0.0f;
		Matrix.M[1][0] = TransformRows[1].X;
		Matrix.M[1][1] = TransformRows[1].Y;
		Matrix.M[1][2] = TransformRows[1].Z;
		Matrix.M[1][3] = 0.0f;
		Matrix.M[2][0] = TransformRows[2].X;
		Matrix.M[2][1] = TransformRows[2].Y;
		Matrix.M[2][2] = TransformRows[2].Z;
		Matrix.M[2][3] = 0.0f;
		Matrix.M[3][0] = Origin.X;
		Matrix.M[3][1] = Origin.Y;
		Matrix.M[3][2] = Origin.Z;
		Matrix.M[3][3] = 1.0f;
		return Matrix;
	}

	FORCEINLINE FBox GetWorldBounds() const
	{
		FBox WorldBoundsRelativeToOriginDoublePrecision = (FBox)WorldBoundsRelativeToOrigin;
		return WorldBoundsRelativeToOriginDoublePrecision.ShiftBy(Origin);
	}
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

struct FDistanceFieldReadRequest;
struct FDistanceFieldAsyncUpdateParameters;

/** Scene data used to manage distance field object buffers on the GPU. */
class FDistanceFieldSceneData
{
public:
	FDistanceFieldSceneData(FDistanceFieldSceneData&&);
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

	bool HasPendingUploads() const
	{
		return IndicesToUpdateInObjectBuffers.Num() > 0;
	}

	bool HasPendingOperations() const
	{
		return PendingAddOperations.Num() > 0 || PendingUpdateOperations.Num() > 0 || PendingRemoveOperations.Num() > 0;
	}

	bool HasPendingHeightFieldOperations() const
	{
		return PendingHeightFieldAddOps.Num() > 0 || PendingHeightFieldRemoveOps.Num() > 0;
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

	bool HasPendingStreaming() const
	{
		return ReadRequests.Num() > 0;
	}

	inline bool CanUse16BitObjectIndices() const
	{
		return bCanUse16BitObjectIndices && (NumObjectsInBuffer < (1 << 16));
	}

	const FDistanceFieldObjectBuffers* GetCurrentObjectBuffers() const
	{
		return ObjectBuffers;
	}

	const FDistanceFieldObjectBuffers* GetHeightFieldObjectBuffers() const
	{
		return HeightFieldObjectBuffers;
	}

	int32 NumObjectsInBuffer;
	FDistanceFieldObjectBuffers* ObjectBuffers;
	FDistanceFieldObjectBuffers* HeightFieldObjectBuffers;

	FRDGScatterUploadBuffer UploadHeightFieldDataBuffer;
	FRDGScatterUploadBuffer UploadHeightFieldBoundsBuffer;
	FRDGScatterUploadBuffer UploadDistanceFieldDataBuffer;
	FRDGScatterUploadBuffer UploadDistanceFieldBoundsBuffer;

	// track indices that need to be updated using both an array and a set
	// array is used for fast iteration and support ParallelFor
	// set is used to prevent duplicate indices
	TArray<int32> IndicesToUpdateInObjectBuffers;
	TSet<int32> IndicesToUpdateInObjectBuffersSet;

	TSet<FDistanceFieldAssetState, TFDistanceFieldAssetStateFuncs> AssetStateArray;
	TRefCountPtr<FRDGPooledBuffer> AssetDataBuffer;
	FRDGScatterUploadBuffer AssetDataUploadBuffer;

	TArray<FRHIGPUBufferReadback*> StreamingRequestReadbackBuffers;
	uint32 MaxStreamingReadbackBuffers = 4;
	uint32 ReadbackBuffersWriteIndex = 0;
	uint32 ReadbackBuffersNumPending = 0;

	FGrowOnlySpanAllocator IndirectionTableAllocator;
	TRefCountPtr<FRDGPooledBuffer> IndirectionTable;
	FRDGAsyncScatterUploadBuffer IndirectionTableUploadBuffer;

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
	TSet<FPrimitiveSceneInfo*> PendingUpdateOperations;
	TArray<FPrimitiveRemoveInfo> PendingRemoveOperations;
	TArray<FBox> PrimitiveModifiedBounds[GDF_Num];

	TSet<FPrimitiveSceneInfo*> PendingHeightFieldAddOps;
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

	void AsyncUpdate(FRHICommandListBase& RHICmdList, FDistanceFieldAsyncUpdateParameters& UpdateParameters);

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
	virtual void InitRHI(FRHICommandListBase& RHICmdList);
	virtual void ReleaseRHI();

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
	void UpdateVisibilityStates(FViewInfo& View, UE::Tasks::FTaskEvent& FlushCachedShadowsTaskEvent);

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
	TBitArray<> StaticShadowSubjectPersistentPrimitiveIdMap;

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

		StaticShadowSubjectPersistentPrimitiveIdMap.SetRange(0, StaticShadowSubjectPersistentPrimitiveIdMap.Num(), false);
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

	/** Nanite shading material commands. These are stored on the scene as they are computed at FPrimitiveSceneInfo::AddToScene time. */
	FNaniteMaterialCommands NaniteMaterials[ENaniteMeshPass::Num];
	FNaniteShadingCommands NaniteShadingCommands[ENaniteMeshPass::Num];

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

	/** Index into primitive arrays where the always visible partition starts. */
	uint32 PrimitivesAlwaysVisibleOffset = ~0u;

	/** Packed array of primitives in the scene. */
	TArray<FPrimitiveSceneInfo*> Primitives;
	/** Packed array of all transforms in the scene. */
	TScenePrimitiveArray<FMatrix> PrimitiveTransforms;
	/** Packed array of primitive scene proxies in the scene. */
	TArray<FPrimitiveSceneProxy*> PrimitiveSceneProxies;
	/** Packed array of primitive bounds. */
	TScenePrimitiveArray<FPrimitiveBounds> PrimitiveBounds;
	/** Packed array of primitive flags. */
	TArray<FPrimitiveFlagsCompact> PrimitiveFlagsCompact;
	/** Packed array of precomputed primitive visibility IDs. */
	TArray<FPrimitiveVisibilityId> PrimitiveVisibilityIds;
	/**Array of primitive octree node index**/
	TArray<uint32> PrimitiveOctreeIndex;
	/** Packed array of primitive occlusion flags. See EOcclusionFlags. */
	TArray<uint8> PrimitiveOcclusionFlags;
	/** Packed array of primitive occlusion bounds. */
	TScenePrimitiveArray<FBoxSphereBounds> PrimitiveOcclusionBounds;
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
	TBitArray<> PrimitivesNeedingUniformBufferUpdate;

	TArray<int32> PersistentPrimitiveIdToIndexMap;

	/**
	 * Defines a bucket "type" in the sorted order of the primitive arrays, as defined by the type-offset table.
	 */
	struct FPrimitiveSceneProxyType
	{
		FPrimitiveSceneProxyType(const FPrimitiveSceneProxy *PrimitiveSceneProxy);
		bool operator ==(const FPrimitiveSceneProxyType&) const = default;

		SIZE_T ProxyTypeHash; 
		bool bIsAlwaysVisible;
	};

	struct FTypeOffsetTableEntry
	{
		FTypeOffsetTableEntry(const FPrimitiveSceneProxyType &InPrimitiveSceneProxyType, uint32 InOffset) : PrimitiveSceneProxyType(InPrimitiveSceneProxyType), Offset(InOffset) {}
		FPrimitiveSceneProxyType PrimitiveSceneProxyType;
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
	/** What mode was the cached RT commands prepared for last? */
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
	TArray<FDeferredDecalProxy*> Decals;

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

	/**	Stores persistent virtual shadow map data */
	FVirtualShadowMapArrayCacheManager* VirtualShadowMapCache;

	/**
	 * Stores scene-aspects needed for shadow rendering.
	 */
	FShadowScene* ShadowScene;

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

	TArray<FLocalFogVolumeSceneProxy*> LocalFogVolumes;

	TArray<FSparseVolumeTextureViewerSceneProxy*> SparseVolumeTextureViewers;

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

	/** Substrate data shared between all views. */
	FSubstrateSceneData SubstrateSceneData;

	LightFunctionAtlas::FLightFunctionAtlasSceneData LightFunctionAtlasSceneData;

	/** Mask used to determine whether primitives that draw to a runtime virtual texture should also be drawn in the main pass. */
	uint8 RuntimeVirtualTexturePrimitiveHideMaskEditor;
	uint8 RuntimeVirtualTexturePrimitiveHideMaskGame;

	float DefaultMaxDistanceFieldOcclusionDistance;

	float GlobalDistanceFieldViewDistance;

	float DynamicIndirectShadowsSelfShadowingIntensity;

	FSpanAllocator PersistentPrimitiveIdAllocator;

#if WITH_EDITOR
	/** Editor Pixel inspector */
	FPixelInspectorData PixelInspectorData;
#endif //WITH_EDITOR

#if RHI_RAYTRACING
	class FRayTracingDynamicGeometryCollection* RayTracingDynamicGeometryCollection;
	class FRayTracingSkinnedGeometryUpdateQueue* RayTracingSkinnedGeometryUpdateQueue;
#endif

	/** Collection of scene render extensions. */
	FSceneExtensions SceneExtensions;

	/** List of all the custom render passes that will run the next time the scene is rendered. */
	TArray<FCustomRenderPassRendererInput> CustomRenderPassRendererInputs;

	/** Initialization constructor. */
	FScene(UWorld* InWorld, bool bInRequiresHitProxies,bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel);

	virtual ~FScene();

	FString GetFullWorldName() const { return FullWorldName; }

	using FSceneInterface::UpdateAllPrimitiveSceneInfos;

	struct FUpdateParameters
	{
		EUpdateAllPrimitiveSceneInfosAsyncOps AsyncOps = EUpdateAllPrimitiveSceneInfosAsyncOps::None;
		UE::Tasks::FTask GPUSceneUpdateTaskPrerequisites;
		bool bDestruction = false;

		struct
		{
			TFunction<void(const UE::Tasks::FTask&)> PostStaticMeshUpdate;

		} Callbacks;
	};

	void Update(FRDGBuilder& GraphBuilder, const FUpdateParameters& Parameters);

	// FSceneInterface interface.
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) override;
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) override;
	virtual void BatchAddPrimitives(TArrayView<UPrimitiveComponent*> InPrimitives) override;
	virtual void BatchRemovePrimitives(TArrayView<UPrimitiveComponent*> InPrimitives) override;
	virtual void BatchReleasePrimitives(TArrayView<UPrimitiveComponent*> InPrimitives) override;
	virtual void UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, EUpdateAllPrimitiveSceneInfosAsyncOps AsyncOps = EUpdateAllPrimitiveSceneInfosAsyncOps::None) override;
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveInstances(UInstancedStaticMeshComponent* Primitive) override;
	virtual void UpdatePrimitiveOcclusionBoundsSlack(UPrimitiveComponent* Primitive, float NewSlack) override;
	virtual void UpdatePrimitiveDrawDistance(UPrimitiveComponent* Primitive, float MinDrawDistance, float MaxDrawDistance, float VirtualTextureMaxDrawDistance) override;
	virtual void UpdateInstanceCullDistance(UPrimitiveComponent* Primitive, float StartCullDistance, float EndCullDistance) override;
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) override;
	virtual void UpdateCustomPrimitiveData(UPrimitiveComponent* Primitive) override;
	virtual void UpdatePrimitiveDistanceFieldSceneData_GameThread(UPrimitiveComponent* Primitive) override;
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimitiveIndex) const final;
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(FPrimitiveComponentId PrimitiveId) const final;
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(const FPersistentPrimitiveIndex& PersistentPrimitiveIndex) const final;
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
	virtual void BatchUpdateDecals(TArray<FDeferredDecalUpdateParams>&& UpdateParams) override;
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
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, UTextureCube* SourceCubemap, FTexture* OutProcessedTexture, float& OutAverageBrightness, FSHVectorRGB3& OutIrradianceEnvironmentMap, TArray<FFloat16Color>* OutRadianceMap, FLinearColor* SpecifiedCubemapColorScale) override;
	virtual void AllocateAndCaptureFrameSkyEnvMap(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, FViewInfo& MainView, bool bShouldRenderSkyAtmosphere, bool bShouldRenderVolumetricCloud, FInstanceCullingManager& InstanceCullingManager, FRDGExternalAccessQueue& ExternalAccessQueue) override;
	virtual void ValidateSkyLightRealTimeCapture(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture) override;
	virtual void ProcessAndRenderIlluminanceMeter(FRDGBuilder& GraphBuilder, TArrayView<FViewInfo> Views, FRDGTextureRef SceneColorTexture);
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

	virtual void GetLightIESAtlasSlot(const FLightSceneProxy* Proxy, FLightRenderParameters* Out) override;
	virtual void GetRectLightAtlasSlot(const FRectLightSceneProxy* Proxy, FLightRenderParameters* Out) override;

	virtual void AddLocalFogVolume(class FLocalFogVolumeSceneProxy* FogProxy) override;
	virtual void RemoveLocalFogVolume(class FLocalFogVolumeSceneProxy* FogProxy) override;
	virtual bool HasAnyLocalFogVolume() const override;

	virtual void AddSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy, bool bStaticLightingBuilt) override;
	virtual void RemoveSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy) override;
	virtual FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() override { return SkyAtmosphere; }
	virtual const FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() const override { return SkyAtmosphere; }

	virtual void AddSparseVolumeTextureViewer(FSparseVolumeTextureViewerSceneProxy* SVTV) override;
	virtual void RemoveSparseVolumeTextureViewer(FSparseVolumeTextureViewerSceneProxy* SVTV) override;

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
	virtual bool RequestUniformBufferUpdate(FPrimitiveSceneInfo& PrimitiveSceneInfo) override;

	virtual void RefreshNaniteRasterBins(FPrimitiveSceneInfo& PrimitiveSceneInfo) override;
	virtual void ReloadNaniteFixedFunctionBins() override;

	FVirtualShadowMapArrayCacheManager* GetVirtualShadowMapCache() const { return VirtualShadowMapCache; }

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
	virtual void AddPrimitive(FPrimitiveSceneDesc* Primitive) override;
	virtual void RemovePrimitive(FPrimitiveSceneDesc* Primitive) override;
	virtual void ReleasePrimitive(FPrimitiveSceneDesc* Primitive) override;
	virtual void UpdatePrimitiveTransform(FPrimitiveSceneDesc* Primitive) override;

	virtual void BatchAddPrimitives(TArrayView<FPrimitiveSceneDesc*> InPrimitives) override;
	virtual void BatchRemovePrimitives(TArrayView<FPrimitiveSceneDesc*> InPrimitives) override;
	virtual void BatchReleasePrimitives(TArrayView<FPrimitiveSceneDesc*> InPrimitives) override;
		
	virtual void UpdateCustomPrimitiveData(FPrimitiveSceneDesc* Primitive, const FCustomPrimitiveData& CustomPrimitiveData) override;

	virtual void UpdatePrimitiveInstances(FInstancedStaticMeshSceneDesc* Primitive) override;

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

	virtual void UpdateEarlyZPassMode() override;

	virtual void Release() override;
	virtual UWorld* GetWorld() const override { return World; }

	/** Finds the closest reflection capture to a point in space, accounting influence radius */
	const FReflectionCaptureProxy* FindClosestReflectionCapture(FVector Position) const;

	const class FPlanarReflectionSceneProxy* FindClosestPlanarReflection(const FBoxSphereBounds& Bounds) const;

	const class FPlanarReflectionSceneProxy* GetForwardPassGlobalPlanarReflection() const;

	void FindClosestReflectionCaptures(FVector Position, const FReflectionCaptureProxy* (&SortedByDistanceOUT)[FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies]) const;

	int64 GetCachedWholeSceneShadowMapsSize() const;

	/**
	 * Get the default base pass depth stencil access
	 */
	static FExclusiveDepthStencil::Type GetDefaultBasePassDepthStencilAccess(ERHIFeatureLevel::Type InFeatureLevel);

	/**
	 * Get the default base pass depth stencil access
	 */
	static void GetEarlyZPassMode(ERHIFeatureLevel::Type InFeatureLevel, EDepthDrawingMode& OutZPassMode, bool& bOutEarlyZPassMovable);

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const override;

	SIZE_T GetSizeBytes() const;

	/**
	* Return the scene to be used for rendering
	*/
	virtual FScene* GetRenderScene() final { return this; }
	virtual const FScene* GetRenderScene() const final { return this; }

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

	virtual void ApplyWorldOffset(const FVector& InOffset) override;

	virtual void OnLevelAddedToWorld(const FName& InLevelName, UWorld* InWorld, bool bIsLightingScenario) override;
	virtual void OnLevelRemovedFromWorld(const FName& LevelRemovedName, UWorld* InWorld, bool bIsLightingScenario) override;

	virtual bool HasAnyLights() const override 
	{ 
		check(IsInGameThread());
		return NumVisibleLights_GameThread > 0 || NumEnabledSkylights_GameThread > 0; 
	}

	virtual bool IsEditorScene() const override { return bIsEditorScene; }

	bool ShouldRenderSkylightInBasePass(bool bIsTranslucent) const;

	virtual TConstArrayView<FPrimitiveSceneProxy*> GetPrimitiveSceneProxies() const final
	{
		return PrimitiveSceneProxies;
	}

	virtual TConstArrayView<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const final
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

	virtual void EndFrame(FRHICommandListImmediate& RHICmdList) override
	{
		// Performs a final update of any queued scene primitives in the case where the scene wasn't rendered to avoid a build-up of queued data that is never flushed.
		if (LastUpdateFrameCounter != GFrameCounterRenderThread)
		{
			UpdateAllPrimitiveSceneInfos(RHICmdList);
		}
	}

	/**
	 * Returns the current "FrameNumber" where frame corresponds to how many times FRendererModule::BeginRenderingViewFamilies has been called.
	 * Thread safe, and returns a different copy for game/render thread. GetFrameNumberRenderThread can only be called from the render thread. 
	 */
	virtual uint32 GetFrameNumber() const override;
	inline uint32 GetFrameNumberRenderThread() const { return SceneFrameNumberRenderThread; }

	virtual void IncrementFrameNumber() override;

	void DumpMeshDrawCommandMemoryStats();

	void CreateLightPrimitiveInteractionsForPrimitive(FPrimitiveSceneInfo* PrimitiveInfo);

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
		if (PersistentPrimitiveIndex.IsValid() && PersistentPrimitiveIndex.Index < PersistentPrimitiveIdToIndexMap.Num())
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

	void WaitForCreateLightPrimitiveInteractionsTask()
	{
		CreateLightPrimitiveInteractionsTask.Wait();
	}

	UE::Tasks::FTask GetCreateLightPrimitiveInteractionsTask() const
	{
		return CreateLightPrimitiveInteractionsTask;
	}

	void WaitForGPUSkinCacheTask()
	{
		GPUSkinCacheTask.Wait();
	}

	UE::Tasks::FTask GetGPUSkinCacheTask() const
	{
		return GPUSkinCacheTask;
	}

	void WaitForCacheMeshDrawCommandsTask()
	{
		CacheMeshDrawCommandsTask.Wait();
	}

	UE::Tasks::FTask GetCacheMeshDrawCommandsTask() const
	{
		return CacheMeshDrawCommandsTask;
	}

	void WaitForCacheNaniteMaterialBinsTask()
	{
		CacheNaniteMaterialBinsTask.Wait();
	}

	UE::Tasks::FTask GetCacheNaniteMaterialBinsTask() const
	{
		return CacheNaniteMaterialBinsTask;
	}

#if RHI_RAYTRACING
	void WaitForCacheRayTracingPrimitivesTask()
	{
		CacheRayTracingPrimitivesTask.Wait();
	}

	UE::Tasks::FTask GetCacheRayTracingPrimitivesTask()
	{
		return CacheRayTracingPrimitivesTask;
	}
#endif

	void LumenAddPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenUpdatePrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenInvalidateSurfaceCacheForPrimitive(FPrimitiveSceneInfo* InPrimitive);
	void LumenRemovePrimitive(FPrimitiveSceneInfo* InPrimitive, int32 PrimitiveIndex);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void DebugRender(TArrayView<FViewInfo> Views);
#endif

	template<typename TExtension>
	TExtension* GetExtensionPtr() { return SceneExtensions.GetExtensionPtr<TExtension>(); }
	template<typename TExtension>
	const TExtension* GetExtensionPtr() const { return SceneExtensions.GetExtensionPtr<TExtension>(); }
	template<typename TExtension>
	TExtension& GetExtension() { return SceneExtensions.GetExtension<TExtension>(); }
	template<typename TExtension>
	const TExtension& GetExtension() const { return SceneExtensions.GetExtension<TExtension>(); }

	virtual bool AddCustomRenderPass(const FSceneViewFamily* ViewFamily, const FCustomRenderPassRendererInput& CustomRenderPassInput);

	FSceneCulling* SceneCulling = nullptr;

	class FInstanceCullingOcclusionQueryRenderer* InstanceCullingOcclusionQueryRenderer = nullptr;

	/**
	 * Light scene change delegates, may be used to hook in subsystems that need to respond to light scene changes.
	 * Note, all the light scene changes are applied _before_ all the primitive scene infos are updated.
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSceneLightSceneInfoUpdateDelegate, FRDGBuilder& , const FLightSceneChangeSet&);
	/**
	 * This delegate is invoked during the scene update phase _before_ the scene has had any light changes applied.
	 * Thus, AddedLightIds is not valid in the change set as the added lights do not have assigned IDs yet.
	 * IF using this to drive an async task, care must be taken as the (light) scene will be modified directly after.
	 */
	FSceneLightSceneInfoUpdateDelegate OnPreLightSceneInfoUpdate;
	/**
	 * This delegate is invoked during the scene update phase _after_ all light changes are applied.
	 * Thus, RemovedLightIds may contain ID's that are no longer valid or are now referencing newly added lights.
	 * IF using this to drive an async task, the core light scene info may be used, but primitive scene updates will still be ongoing (e.g., light/primitive interactions may change).
	 */
	FSceneLightSceneInfoUpdateDelegate OnPostLightSceneInfoUpdate;
protected:

private:

	template<class T> 	
	void BatchAddPrimitivesInternal(TArrayView<T*> InPrimitives);

	template<class T> 	
	void BatchRemovePrimitivesInternal(TArrayView<T*> InPrimitives);

	template<class T> 	
	void BatchReleasePrimitivesInternal(TArrayView<T*> InPrimitives);	

	template<class T> 	
	void UpdatePrimitiveTransformInternal(T* Primitive);
	
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
	bool RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo);

	/** Updates a primitive's transform, called on the rendering thread. */
	void UpdatePrimitiveTransform_RenderThread(FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& OwnerPosition, const TOptional<FTransform>& PreviousTransform);

	void UpdatePrimitiveOcclusionBoundsSlack_RenderThread(const FPrimitiveSceneProxy* PrimitiveSceneProxy, float NewSlack);

	void UpdateCustomPrimitiveData(FPrimitiveSceneProxy* SceneProxy, const FCustomPrimitiveData& CustomPrimitiveData);

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

	void UpdateLightTransform_RenderThread(int32 LightId, FLightSceneInfo* LightSceneInfo, const struct FUpdateLightTransformParameters& Parameters);

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
	void ApplyWorldOffset_RenderThread(FRHICommandListBase& RHICmdList, const FVector& InOffset);

	void ProcessAtmosphereLightRemoval_RenderThread(FLightSceneInfo* LightSceneInfo);
	void ProcessAtmosphereLightAddition_RenderThread(FLightSceneInfo* LightSceneInfo);

	/**
	 * Process all scene updates for lights, returns the change-set, which references arrays allocated with a RDG builder life-time.
	 */
	FLightSceneChangeSet UpdateAllLightSceneInfos(FRDGBuilder& GraphBuilder);

private:

	/**
	 * Update tracked scene state for cached CSM shadows
	 */
	void UpdateCachedShadowState(const FScenePreUpdateChangeSet &ScenePreUpdateChangeSet, const FScenePostUpdateChangeSet &ScenePostUpdateChangeSet);

	FString FullWorldName;
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
		FBoxSphereBounds WorldBounds;
		FBoxSphereBounds LocalBounds;
		FBoxSphereBounds StaticMeshBounds;
	};

	void UpdatePrimitiveInstances(FUpdateInstanceCommand& UpdateParams);

	struct FLevelCommand
	{
		enum class EOp
		{
			Add,
			Remove
		};

		FName Name;
		EOp Op;
	};

	Experimental::TRobinHoodHashMap<FPrimitiveSceneInfo*, FPrimitiveComponentId> UpdatedAttachmentRoots;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FCustomPrimitiveData> UpdatedCustomPrimitiveParams;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FUpdateTransformCommand> UpdatedTransforms;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FUpdateInstanceCommand> UpdatedInstances;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneInfo*, FMatrix> OverridenPreviousTransforms;
	Experimental::TRobinHoodHashMap<const FPrimitiveSceneProxy*, float> UpdatedOcclusionBoundsSlacks;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FVector2f> UpdatedInstanceCullDistance;
	Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, FVector3f> UpdatedDrawDistance;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> AddedPrimitiveSceneInfos;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> RemovedPrimitiveSceneInfos;
	Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*> DistanceFieldSceneDataUpdates;
	TArray<FLevelCommand> LevelCommands;
	TSet<FPrimitiveSceneInfo*> DeletedPrimitiveSceneInfos;

	UE::Tasks::FTask CreateLightPrimitiveInteractionsTask;
	UE::Tasks::FTask GPUSkinCacheTask;
	UE::Tasks::FTask CacheMeshDrawCommandsTask;
	UE::Tasks::FTask CacheNaniteMaterialBinsTask;
#if RHI_RAYTRACING
	UE::Tasks::FTask CacheRayTracingPrimitivesTask;
#endif

	FSceneLightInfoUpdates *SceneLightInfoUpdates;

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

	/** Frame number incremented per-family (except if there are multiple view families in one render call) viewing this scene. */
	uint32 SceneFrameNumber;
	uint32 SceneFrameNumberRenderThread;

	uint32 LastUpdateFrameCounter = UINT32_MAX;

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

