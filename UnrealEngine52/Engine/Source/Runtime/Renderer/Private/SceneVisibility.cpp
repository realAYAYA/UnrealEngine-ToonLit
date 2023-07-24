// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneVisibility.cpp: Scene visibility determination.
=============================================================================*/

#include "ScenePrivate.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "EngineStats.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "SceneInterface.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "ScenePrivateBase.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneCore.h"
#include "SceneOcclusion.h"
#include "LightSceneInfo.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "DynamicPrimitiveDrawing.h"
#include "ScenePrivate.h"
#include "FXSystem.h"
#include "PostProcess/PostProcessing.h"
#include "SceneView.h"
#include "SkyAtmosphereRendering.h"
#include "Engine/LODActor.h"
#include "GPUScene.h"
#include "TranslucentRendering.h"
#include "Async/ParallelFor.h"
#include "HairStrands/HairStrandsRendering.h"
#include "HairStrands/HairStrandsData.h"
#include "RectLightSceneProxy.h"
#include "Math/Halton.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "Algo/Unique.h"
#include "InstanceCulling/InstanceCullingManager.h"
#include "PostProcess/TemporalAA.h"
#include "RayTracing/RayTracingInstanceCulling.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"
#include "RendererModule.h"
#include "SceneViewExtension.h"
#include "RenderCore.h"
#include "StaticMeshBatch.h"
#include "UnrealEngine.h"

#if !UE_BUILD_SHIPPING
#include "ViewDebug.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/MaterialInterface.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/SkeletalMeshRenderData.h"
#endif

/*------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------*/

static float GWireframeCullThreshold = 5.0f;
static FAutoConsoleVariableRef CVarWireframeCullThreshold(
	TEXT("r.WireframeCullThreshold"),
	GWireframeCullThreshold,
	TEXT("Threshold below which objects in ortho wireframe views will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForLights = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForLights(
	TEXT("r.MinScreenRadiusForLights"),
	GMinScreenRadiusForLights,
	TEXT("Threshold below which lights will be culled."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForDepthPrepass = 0.03f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForDepthPrepass(
	TEXT("r.MinScreenRadiusForDepthPrepass"),
	GMinScreenRadiusForDepthPrepass,
	TEXT("Threshold below which meshes will be culled from depth only pass."),
	ECVF_RenderThreadSafe
	);

float GMinScreenRadiusForCSMDepth = 0.01f;
static FAutoConsoleVariableRef CVarMinScreenRadiusForCSMDepth(
	TEXT("r.MinScreenRadiusForCSMDepth"),
	GMinScreenRadiusForCSMDepth,
	TEXT("Threshold below which meshes will be culled from CSM depth pass."),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTemporalAASamples(
	TEXT("r.TemporalAASamples"),
	8,
	TEXT("Number of jittered positions for temporal AA (4, 8=default, 16, 32, 64)."),
	ECVF_RenderThreadSafe);

static int32 GHZBOcclusion = 0;
static FAutoConsoleVariableRef CVarHZBOcclusion(
	TEXT("r.HZBOcclusion"),
	GHZBOcclusion,
	TEXT("Defines which occlusion system is used.\n")
	TEXT(" 0: Hardware occlusion queries\n")
	TEXT(" 1: Use HZB occlusion system (default, less GPU and CPU cost, more conservative results)")
	TEXT(" 2: Force HZB occlusion system (overrides rendering platform preferences)"),
	ECVF_RenderThreadSafe
	);

int32 GOcclusionFeedback_Enable = 0;
static FAutoConsoleVariableRef CVarOcclusionFeedback_Enable(
	TEXT("r.OcclusionFeedback.Enable"),
	GOcclusionFeedback_Enable,
	TEXT("Whether to enable occlusion system based on a rendering feedback. Currently works only with a mobile rendering\n"),
	ECVF_RenderThreadSafe
);

static int32 GVisualizeOccludedPrimitives = 0;
static FAutoConsoleVariableRef CVarVisualizeOccludedPrimitives(
	TEXT("r.VisualizeOccludedPrimitives"),
	GVisualizeOccludedPrimitives,
	TEXT("Draw boxes for all occluded primitives"),
	ECVF_RenderThreadSafe | ECVF_Cheat
	);

static int32 GAllowSubPrimitiveQueries = 1;
static FAutoConsoleVariableRef CVarAllowSubPrimitiveQueries(
	TEXT("r.AllowSubPrimitiveQueries"),
	GAllowSubPrimitiveQueries,
	TEXT("Enables sub primitive queries, currently only used by hierarchical instanced static meshes. 1: Enable, 0 Disabled. When disabled, one query is used for the entire proxy."),
	ECVF_RenderThreadSafe
	);

RENDERER_API TAutoConsoleVariable<float> CVarStaticMeshLODDistanceScale(
	TEXT("r.StaticMeshLODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete LOD for static meshes. (defaults to 1)\n")
	TEXT("(higher values make LODs transition earlier, e.g., 2 is twice as fast / half the distance)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMinAutomaticViewMipBias(
	TEXT("r.ViewTextureMipBias.Min"),
	-2.0f,
	TEXT("Automatic view mip bias's minimum value (default to -2)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarMinAutomaticViewMipBiasOffset(
	TEXT("r.ViewTextureMipBias.Offset"),
	-0.3,
	TEXT("Automatic view mip bias's constant offset (default to -0.3)."),
	ECVF_RenderThreadSafe);


static int32 GOcclusionCullParallelPrimFetch = 0;
static FAutoConsoleVariableRef CVarOcclusionCullParallelPrimFetch(
	TEXT("r.OcclusionCullParallelPrimFetch"),
	GOcclusionCullParallelPrimFetch,
	TEXT("Enables Parallel Occlusion Cull primitive fetch."),
	ECVF_RenderThreadSafe
	);

static int32 GILCUpdatePrimTaskEnabled = 1;

static FAutoConsoleVariableRef CVarILCUpdatePrimitivesTask(
	TEXT("r.Cache.UpdatePrimsTaskEnabled"),
	GILCUpdatePrimTaskEnabled,
	TEXT("Enable threading for ILC primitive update.  Will overlap with the rest the end of InitViews."),
	ECVF_RenderThreadSafe
	);

int32 GEarlyInitDynamicShadows = 1;
static FAutoConsoleVariableRef CVarEarlyInitDynamicShadows(
	TEXT("r.EarlyInitDynamicShadows"),
	GEarlyInitDynamicShadows,
	TEXT("Starts shadow culling tasks earlier in the frame."),
	ECVF_RenderThreadSafe
);

static int32 GFramesNotOcclusionTestedToExpandBBoxes = 5;
static FAutoConsoleVariableRef CVarFramesNotOcclusionTestedToExpandBBoxes(
	TEXT("r.GFramesNotOcclusionTestedToExpandBBoxes"),
	GFramesNotOcclusionTestedToExpandBBoxes,
	TEXT("If we don't occlusion test a primitive for this many frames, then we expand the BBox when we do occlusion test it for a few frames. See also r.ExpandNewlyOcclusionTestedBBoxesAmount, r.FramesToExpandNewlyOcclusionTestedBBoxes"),
	ECVF_RenderThreadSafe
);

static int32 GFramesToExpandNewlyOcclusionTestedBBoxes = 2;
static FAutoConsoleVariableRef CVarFramesToExpandNewlyOcclusionTestedBBoxes(
	TEXT("r.FramesToExpandNewlyOcclusionTestedBBoxes"),
	GFramesToExpandNewlyOcclusionTestedBBoxes,
	TEXT("If we don't occlusion test a primitive for r.GFramesNotOcclusionTestedToExpandBBoxes frames, then we expand the BBox when we do occlusion test it for this number of frames. See also r.GFramesNotOcclusionTestedToExpandBBoxes, r.ExpandNewlyOcclusionTestedBBoxesAmount"),
	ECVF_RenderThreadSafe
);

static float GExpandNewlyOcclusionTestedBBoxesAmount = 0.0f;
static FAutoConsoleVariableRef CVarExpandNewlyOcclusionTestedBBoxesAmount(
	TEXT("r.ExpandNewlyOcclusionTestedBBoxesAmount"),
	GExpandNewlyOcclusionTestedBBoxesAmount,
	TEXT("If we don't occlusion test a primitive for r.GFramesNotOcclusionTestedToExpandBBoxes frames, then we expand the BBox when we do occlusion test it for a few frames by this amount. See also r.FramesToExpandNewlyOcclusionTestedBBoxes, r.GFramesNotOcclusionTestedToExpandBBoxes."),
	ECVF_RenderThreadSafe
);

static float GExpandAllTestedBBoxesAmount = 0.0f;
static FAutoConsoleVariableRef CVarExpandAllTestedBBoxesAmount(
	TEXT("r.ExpandAllOcclusionTestedBBoxesAmount"),
	GExpandAllTestedBBoxesAmount,
	TEXT("Amount to expand all occlusion test bounds by."),
	ECVF_RenderThreadSafe
);

static float GNeverOcclusionTestDistance = 0.0f;
static FAutoConsoleVariableRef CVarNeverOcclusionTestDistance(
	TEXT("r.NeverOcclusionTestDistance"),
	GNeverOcclusionTestDistance,
	TEXT("When the distance between the viewpoint and the bounding sphere center is less than this, never occlusion cull."),
	ECVF_RenderThreadSafe | ECVF_Scalability
);

static int32 GForceSceneHasDecals = 0;
static FAutoConsoleVariableRef CVarForceSceneHasDecals(
	TEXT("r.ForceSceneHasDecals"),
	GForceSceneHasDecals,
	TEXT("Whether to always assume that scene has decals, so we don't switch depth state conditionally. This can significantly reduce total number of PSOs at a minor GPU cost."),
	ECVF_RenderThreadSafe
);

static float GCameraCutTranslationThreshold = 10000.0f;
static FAutoConsoleVariableRef CVarCameraCutTranslationThreshold(
	TEXT("r.CameraCutTranslationThreshold"),
	GCameraCutTranslationThreshold,
	TEXT("The maximum camera translation disatance in centimeters allowed between two frames before a camera cut is automatically inserted."),
	ECVF_RenderThreadSafe
);

/** Distance fade cvars */
static int32 GDisableLODFade = false;
static FAutoConsoleVariableRef CVarDisableLODFade( TEXT("r.DisableLODFade"), GDisableLODFade, TEXT("Disable fading for distance culling"), ECVF_RenderThreadSafe );

static float GFadeTime = 0.25f;
static FAutoConsoleVariableRef CVarLODFadeTime( TEXT("r.LODFadeTime"), GFadeTime, TEXT("How long LOD takes to fade (in seconds)."), ECVF_RenderThreadSafe );

static float GDistanceFadeMaxTravel = 1000.0f;
static FAutoConsoleVariableRef CVarDistanceFadeMaxTravel( TEXT("r.DistanceFadeMaxTravel"), GDistanceFadeMaxTravel, TEXT("Max distance that the player can travel during the fade time."), ECVF_RenderThreadSafe );

static TAutoConsoleVariable<int32> CVarParallelInitViews(
	TEXT("r.ParallelInitViews"),
	1,
	TEXT("Toggles parallel init views. 0 = off; 1 = on"),
	ECVF_RenderThreadSafe
	);          

float GLightMaxDrawDistanceScale = 1.0f;
static FAutoConsoleVariableRef CVarLightMaxDrawDistanceScale(
	TEXT("r.LightMaxDrawDistanceScale"),
	GLightMaxDrawDistanceScale,
	TEXT("Scale applied to the MaxDrawDistance of lights.  Useful for fading out local lights more aggressively on some platforms."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<bool> CVarEnableFrustumCull(
	TEXT("r.EnableFrustumCull"),
	true,
	TEXT("Enables or disables frustum culling.  Useful for comparing results to ensure culling is functioning properly."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarAlsoUseSphereForFrustumCull(
	TEXT("r.AlsoUseSphereForFrustumCull"),
	0,
	TEXT("Performance tweak. If > 0, then use a sphere cull before and in addition to a box for frustum culling."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarUseFastIntersect(
	TEXT("r.UseFastIntersect"),
	1,
	TEXT("Use optimized 8 plane fast intersection code if we have 8 permuted planes."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarUseVisibilityOctree(
	TEXT("r.UseVisibilityOctree"), 
	0, 
	TEXT("Use the octree for visibility calculations."), 
	ECVF_RenderThreadSafe);

static bool GOcclusionSingleRHIThreadStall = false;
static FAutoConsoleVariableRef CVarOcclusionSingleRHIThreadStall(
	TEXT("r.Occlusion.SingleRHIThreadStall"),
	GOcclusionSingleRHIThreadStall,
	TEXT("Enable a single RHI thread stall before polling occlusion queries. This will only happen if the RHI's occlusion queries would normally stall the RHI thread themselves."),
	ECVF_RenderThreadSafe
);

#if !UE_BUILD_SHIPPING

static TAutoConsoleVariable<int32> CVarTAADebugOverrideTemporalIndex(
	TEXT("r.TemporalAA.Debug.OverrideTemporalIndex"), -1,
	TEXT("Override the temporal index for debugging purposes."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarFreezeTemporalSequences(
	TEXT("r.Test.FreezeTemporalSequences"), 0,
	TEXT("Freezes all temporal sequences."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarFreezeTemporalHistories(
	TEXT("r.Test.FreezeTemporalHistories"), 0,
	TEXT("Freezes all temporal histories as well as the temporal sequence."),
	ECVF_RenderThreadSafe);

static bool bDumpPrimitivesNextFrame = false;
static bool bDumpDetailedPrimitivesNextFrame = false;

static FAutoConsoleCommand CVarDumpPrimitives(
	TEXT("DumpPrimitives"),
	TEXT("Writes out all scene primitive names to a CSV file"),
	FConsoleCommandDelegate::CreateStatic([] { bDumpPrimitivesNextFrame = true; }),
	ECVF_Default);

static FAutoConsoleCommand CVarDrawPrimitiveDebugData(
	TEXT("DumpDetailedPrimitives"),
	TEXT("Writes out all scene primitive details to a CSV file"),
	FConsoleCommandDelegate::CreateStatic([] { bDumpDetailedPrimitivesNextFrame = !bDumpDetailedPrimitivesNextFrame; }),
	ECVF_Default);

#endif

DECLARE_CYCLE_STAT(TEXT("Occlusion Readback"), STAT_CLMM_OcclusionReadback, STATGROUP_CommandListMarkers);
DECLARE_CYCLE_STAT(TEXT("After Occlusion Readback"), STAT_CLMM_AfterOcclusionReadback, STATGROUP_CommandListMarkers);

/*------------------------------------------------------------------------------
	Visibility determination.
------------------------------------------------------------------------------*/

/**
 * Update a primitive's fading state.
 * @param FadingState - State to update.
 * @param View - The view for which to update.
 * @param bVisible - Whether the primitive should be visible in the view.
 */
static void UpdatePrimitiveFadingStateHelper(FPrimitiveFadingState& FadingState, const FViewInfo& View, bool bVisible)
{
	if (FadingState.bValid)
	{
		if (FadingState.bIsVisible != bVisible)
		{
			float CurrentRealTime = View.Family->Time.GetRealTimeSeconds();

			// Need to kick off a fade, so make sure that we have fading state for that
			if( !IsValidRef(FadingState.UniformBuffer) )
			{
				// Primitive is not currently fading.  Start a new fade!
				FadingState.EndTime = CurrentRealTime + GFadeTime;

				if( bVisible )
				{
					// Fading in
					// (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = 1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = -CurrentRealTime / GFadeTime;
				}
				else
				{
					// Fading out
					// 1 - (Time - StartTime) / FadeTime
					FadingState.FadeTimeScaleBias.X = -1.0f / GFadeTime;
					FadingState.FadeTimeScaleBias.Y = 1.0f + CurrentRealTime / GFadeTime;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FVector2f(FadingState.FadeTimeScaleBias);	// LWC_TODO: Precision loss
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
			else
			{
				// Reverse fading direction but maintain current opacity
				// Solve for d: a*x+b = -a*x+d
				FadingState.FadeTimeScaleBias.Y = 2.0f * CurrentRealTime * FadingState.FadeTimeScaleBias.X + FadingState.FadeTimeScaleBias.Y;
				FadingState.FadeTimeScaleBias.X = -FadingState.FadeTimeScaleBias.X;
				
				if( bVisible )
				{
					// Fading in
					// Solve for x: a*x+b = 1
					FadingState.EndTime = ( 1.0f - FadingState.FadeTimeScaleBias.Y ) / FadingState.FadeTimeScaleBias.X;
				}
				else
				{
					// Fading out
					// Solve for x: a*x+b = 0
					FadingState.EndTime = -FadingState.FadeTimeScaleBias.Y / FadingState.FadeTimeScaleBias.X;
				}

				FDistanceCullFadeUniformShaderParameters Uniforms;
				Uniforms.FadeTimeScaleBias = FVector2f(FadingState.FadeTimeScaleBias);	// LWC_TODO: Precision loss
				FadingState.UniformBuffer = FDistanceCullFadeUniformBufferRef::CreateUniformBufferImmediate( Uniforms, UniformBuffer_MultiFrame );
			}
		}
	}

	FadingState.FrameNumber = View.Family->FrameNumber;
	FadingState.bIsVisible = bVisible;
	FadingState.bValid = true;
}

bool FViewInfo::IsDistanceCulled( float DistanceSquared, float MinDrawDistance, float InMaxDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	bool bMayBeFading;
	bool bFadingIn;
	bool bDistanceCulled = IsDistanceCulled_AnyThread(DistanceSquared, MinDrawDistance, InMaxDrawDistance, PrimitiveSceneInfo, bMayBeFading, bFadingIn);

	if (bMayBeFading)
	{
		bDistanceCulled = UpdatePrimitiveFadingState(PrimitiveSceneInfo, bFadingIn);
	}

	return bDistanceCulled;
}

bool FViewInfo::IsDistanceCulled_AnyThread(float DistanceSquared, float MinDrawDistance, float InMaxDrawDistance, const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool& bOutMayBeFading, bool& bOutFadingIn) const
{
	const float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
	const float FadeRadius = GDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	const float MaxDrawDistance = InMaxDrawDistance * MaxDrawDistanceScale;

	bool bHasMaxDrawDistance = InMaxDrawDistance != FLT_MAX;
	bool bHasMinDrawDistance = InMaxDrawDistance > 0;
	bOutMayBeFading = false;


	if (!bHasMaxDrawDistance && !bHasMinDrawDistance)
	{
		return false;
	}

	// If cull distance is disabled, always show (except foliage)
	if (Family->EngineShowFlags.DistanceCulledPrimitives && !PrimitiveSceneInfo->Proxy->IsDetailMesh())
	{
		return false;
	}

	// The primitive is always culled if it exceeds the max fade distance.
	if ((bHasMaxDrawDistance && DistanceSquared > FMath::Square(MaxDrawDistance + FadeRadius)) || (bHasMinDrawDistance && DistanceSquared < FMath::Square(MinDrawDistance)))
	{
		return true;
	}

	const bool bDistanceCulled = bHasMaxDrawDistance && (DistanceSquared > FMath::Square(MaxDrawDistance));
	const bool bMayBeFading = bHasMaxDrawDistance && (DistanceSquared > FMath::Square(MaxDrawDistance - FadeRadius));
	
	if (!GDisableLODFade && bMayBeFading && State != NULL && !bDisableDistanceBasedFadeTransitions && PrimitiveSceneInfo->Proxy->IsUsingDistanceCullFade())
	{
		// Don't update primitive fading state yet because current thread may be not render thread
		bOutMayBeFading = true;
		bOutFadingIn = !bDistanceCulled;
	}

	return bDistanceCulled && !bOutMayBeFading;
}

bool FViewInfo::UpdatePrimitiveFadingState(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bFadingIn)
{
	// Update distance-based visibility and fading state if it has not already been updated.
	const int32 PrimitiveIndex = PrimitiveSceneInfo->GetIndex();
	const FRelativeBitReference PrimitiveBit(PrimitiveIndex);
	bool bStillFading = false;

	if (!PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit))
	{
		FPrimitiveFadingState& FadingState = ((FSceneViewState*)State)->PrimitiveFadingStates.FindOrAdd(PrimitiveSceneInfo->PrimitiveComponentId);
		UpdatePrimitiveFadingStateHelper(FadingState, *this, bFadingIn);
		FRHIUniformBuffer* UniformBuffer = FadingState.UniformBuffer;
		bStillFading = UniformBuffer != nullptr;
		PrimitiveFadeUniformBuffers[PrimitiveIndex] = UniformBuffer;
		PrimitiveFadeUniformBufferMap[PrimitiveIndex] = UniformBuffer != nullptr;
		PotentiallyFadingPrimitiveMap.AccessCorrespondingBit(PrimitiveBit) = true;
	}

	// If we're still fading then make sure the object is still drawn, even if it's beyond the max draw distance
	return !bFadingIn && !bStillFading;
}

FORCEINLINE bool IntersectBox8Plane(const FVector& InOrigin, const FVector& InExtent, const FPlane*PermutedPlanePtr)
{
	// this removes a lot of the branches as we know there's 8 planes
	// copied directly out of ConvexVolume.cpp
	const VectorRegister Origin = VectorLoadFloat3(&InOrigin);
	const VectorRegister Extent = VectorLoadFloat3(&InExtent);

	const VectorRegister PlanesX_0 = VectorLoadAligned(&PermutedPlanePtr[0]);
	const VectorRegister PlanesY_0 = VectorLoadAligned(&PermutedPlanePtr[1]);
	const VectorRegister PlanesZ_0 = VectorLoadAligned(&PermutedPlanePtr[2]);
	const VectorRegister PlanesW_0 = VectorLoadAligned(&PermutedPlanePtr[3]);

	const VectorRegister PlanesX_1 = VectorLoadAligned(&PermutedPlanePtr[4]);
	const VectorRegister PlanesY_1 = VectorLoadAligned(&PermutedPlanePtr[5]);
	const VectorRegister PlanesZ_1 = VectorLoadAligned(&PermutedPlanePtr[6]);
	const VectorRegister PlanesW_1 = VectorLoadAligned(&PermutedPlanePtr[7]);

	// Splat origin into 3 vectors
	VectorRegister OrigX = VectorReplicate(Origin, 0);
	VectorRegister OrigY = VectorReplicate(Origin, 1);
	VectorRegister OrigZ = VectorReplicate(Origin, 2);
	// Splat the already abs Extent for the push out calculation
	VectorRegister AbsExtentX = VectorReplicate(Extent, 0);
	VectorRegister AbsExtentY = VectorReplicate(Extent, 1);
	VectorRegister AbsExtentZ = VectorReplicate(Extent, 2);

	// Calculate the distance (x * x) + (y * y) + (z * z) - w
	VectorRegister DistX_0 = VectorMultiply(OrigX, PlanesX_0);
	VectorRegister DistY_0 = VectorMultiplyAdd(OrigY, PlanesY_0, DistX_0);
	VectorRegister DistZ_0 = VectorMultiplyAdd(OrigZ, PlanesZ_0, DistY_0);
	VectorRegister Distance_0 = VectorSubtract(DistZ_0, PlanesW_0);
	// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
	VectorRegister PushX_0 = VectorMultiply(AbsExtentX, VectorAbs(PlanesX_0));
	VectorRegister PushY_0 = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY_0), PushX_0);
	VectorRegister PushOut_0 = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ_0), PushY_0);

	// Check for completely outside
	if (VectorAnyGreaterThan(Distance_0, PushOut_0))
	{
		return false;
	}

	// Calculate the distance (x * x) + (y * y) + (z * z) - w
	VectorRegister DistX_1 = VectorMultiply(OrigX, PlanesX_1);
	VectorRegister DistY_1 = VectorMultiplyAdd(OrigY, PlanesY_1, DistX_1);
	VectorRegister DistZ_1 = VectorMultiplyAdd(OrigZ, PlanesZ_1, DistY_1);
	VectorRegister Distance_1 = VectorSubtract(DistZ_1, PlanesW_1);
	// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
	VectorRegister PushX_1 = VectorMultiply(AbsExtentX, VectorAbs(PlanesX_1));
	VectorRegister PushY_1 = VectorMultiplyAdd(AbsExtentY, VectorAbs(PlanesY_1), PushX_1);
	VectorRegister PushOut_1 = VectorMultiplyAdd(AbsExtentZ, VectorAbs(PlanesZ_1), PushY_1);

	// Check for completely outside
	if (VectorAnyGreaterThan(Distance_1, PushOut_1))
	{
		return false;
	}
	return true;
}

static int32 FrustumCullNumWordsPerTask = 128;
static FAutoConsoleVariableRef CVarFrustumCullNumWordsPerTask(
	TEXT("r.FrustumCullNumWordsPerTask"),
	FrustumCullNumWordsPerTask,
	TEXT("Performance tweak. Controls the granularity for the ParallelFor for frustum culling."),
	ECVF_Default
	);

static TAutoConsoleVariable CVarNaniteMeshsAlwaysVisible(
	TEXT("r.Nanite.PrimitivesAlwaysVisible"),
	0,
	TEXT("True - All Nanite primitives skip culling phases, False - All Nanite primitives are run through the culling phase."),
	ECVF_Default
);

// Access when not on the render thread
FORCEINLINE bool IsAlwaysVisible(const FScene* RESTRICT Scene, int32 Index, bool bNaniteAlwaysVisible)
{
	return bNaniteAlwaysVisible ? Scene->PrimitiveFlagsCompact[Index].bIsNaniteMesh : false;
}

// Non template version
FORCEINLINE bool IsAlwaysVisible(const FScene* RESTRICT Scene, int32 Index)
{
	if (CVarNaniteMeshsAlwaysVisible.GetValueOnRenderThread())
	{
		return Scene->PrimitiveFlagsCompact[Index].bIsNaniteMesh;
	}

	return false;
}

struct FPrimitiveCullingFlags
{
	bool bShouldVisibilityCull;
	bool bUseCustomCulling;
	bool bAlsoUseSphereTest;
	bool bUseFastIntersect;
	bool bUseVisibilityOctree;
	bool bNaniteAlwaysVisible;
	bool bHasHiddenPrimitives;
	bool bHasShowOnlyPrimitives;
};

// Returns true if the frustum and bounds intersect
FORCEINLINE bool IsPrimitiveVisible(FViewInfo& View, const FPlane* PermutedPlanePtr, const FPrimitiveBounds& RESTRICT Bounds, int32 VisibilityId, const FPrimitiveCullingFlags& Flags)
{
	// The custom culling and sphere culling are additional tests, meaning that if they pass, the
	// remaining culling tests will still be performed.  If any of the tests fail, then the primitive
	// is culled, and the remaining tests do not need be performed

	if (Flags.bUseCustomCulling && !View.CustomVisibilityQuery->IsVisible(VisibilityId, FBoxSphereBounds(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent, Bounds.BoxSphereBounds.SphereRadius)))
	{
		return false;
	}

	if (Flags.bAlsoUseSphereTest && !View.ViewFrustum.IntersectSphere(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius))
	{
		return false;
	}

	if (Flags.bUseFastIntersect)
	{
		return IntersectBox8Plane(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent, PermutedPlanePtr);
	}
	else
	{
		return View.ViewFrustum.IntersectBox(Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.BoxExtent);
	}
}

FORCEINLINE bool IsPrimitiveHidden(const FScene* RESTRICT Scene, FViewInfo& View, int PrimitiveIndex, const FPrimitiveCullingFlags& Flags)
{
	// If any primitives are explicitly hidden, remove them now.
	if (Flags.bHasHiddenPrimitives && View.HiddenPrimitives.Contains(Scene->PrimitiveComponentIds[PrimitiveIndex]))
	{
		return true;
	}

	// If the view has any show only primitives, hide everything else
	if (Flags.bHasShowOnlyPrimitives && !View.ShowOnlyPrimitives->Contains(Scene->PrimitiveComponentIds[PrimitiveIndex]))
	{
		return true;
	}

	return false;
}

#if RHI_RAYTRACING

FORCEINLINE bool ShouldCullForRayTracing(const FScene* RESTRICT Scene, FViewInfo& View, int32 PrimitiveIndex)
{
	const FRayTracingCullingParameters& RayTracingCullingParameters = View.RayTracingCullingParameters;

	if (RayTracing::CullPrimitiveByFlags(RayTracingCullingParameters, Scene, PrimitiveIndex))
	{
		return true;
	}

	const bool bIsFarFieldPrimitive = EnumHasAnyFlags(Scene->PrimitiveRayTracingFlags[PrimitiveIndex], ERayTracingPrimitiveFlags::FarField);
	const Experimental::FHashElementId GroupId = Scene->PrimitiveRayTracingGroupIds[PrimitiveIndex];

	if (RayTracingCullingParameters.bCullUsingGroupIds && GroupId.IsValid())
	{
		const FBoxSphereBounds& GroupBounds = Scene->PrimitiveRayTracingGroups.GetByElementId(GroupId).Value.Bounds;
		const float GroupMinDrawDistance = Scene->PrimitiveRayTracingGroups.GetByElementId(GroupId).Value.MinDrawDistance;
		return RayTracing::ShouldCullBounds(RayTracingCullingParameters, GroupBounds, GroupMinDrawDistance, bIsFarFieldPrimitive);
	}
	else
	{
		const FPrimitiveBounds& RESTRICT Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
		return RayTracing::ShouldCullBounds(RayTracingCullingParameters, Bounds.BoxSphereBounds, Bounds.MinDrawDistance, bIsFarFieldPrimitive);
	}
};
#endif //RHI_RAYTRACING

static FORCEINLINE void CullOctree(const FScene* RESTRICT Scene, FViewInfo& View, const FPrimitiveCullingFlags& Flags, FSceneBitArray& OutVisibleNodes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SceneVisibility_CullOctree);

	// Two bits per octree node, 1st bit is Inside Frustum, 2nd bit is Outside Frustum
	OutVisibleNodes.Init(false, Scene->PrimitiveOctree.GetNumNodes() * 2);

	Scene->PrimitiveOctree.FindNodesWithPredicate(
		[&View, &OutVisibleNodes, &Flags](FScenePrimitiveOctree::FNodeIndex ParentNodeIndex, FScenePrimitiveOctree::FNodeIndex NodeIndex, const FBoxCenterAndExtent& NodeBounds)
		{
			// If the parent node is completely contained there is no need to test containment
			if (ParentNodeIndex != INDEX_NONE && !OutVisibleNodes[(ParentNodeIndex * 2) + 1])
			{
				OutVisibleNodes[NodeIndex * 2] = true;
				OutVisibleNodes[NodeIndex * 2 + 1] = false;
				return true;
			}

			const FPlane* PermutedPlanePtr = View.ViewFrustum.PermutedPlanes.GetData();
			bool bIntersects = false;

			if (Flags.bUseFastIntersect)
			{
				bIntersects = IntersectBox8Plane(NodeBounds.Center, NodeBounds.Extent, PermutedPlanePtr);
			}
			else
			{
				bIntersects = View.ViewFrustum.IntersectBox(NodeBounds.Center, NodeBounds.Extent);
			}

			if (bIntersects)
			{
				OutVisibleNodes[NodeIndex * 2] = true;
				OutVisibleNodes[NodeIndex * 2 + 1] = View.ViewFrustum.GetBoxIntersectionOutcode(NodeBounds.Center, NodeBounds.Extent).GetOutside();
			}

			return bIntersects;
		},
		[](FScenePrimitiveOctree::FNodeIndex /*ParentNodeIndex*/, FScenePrimitiveOctree::FNodeIndex /*NodeIndex*/, const FBoxCenterAndExtent& /*NodeBounds*/)
		{

		});
}

static void PrimitiveCullTask(FThreadSafeCounter& NumCulledPrimitives, const FScene* RESTRICT Scene, FViewInfo& View, FPrimitiveCullingFlags Flags, float MaxDrawDistanceScale, const FHLODVisibilityState* const HLODState, const FSceneBitArray& VisibleNodes, int32 TaskIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SceneVisibility_PrimitiveCull);
	SCOPED_NAMED_EVENT(SceneVisibility_PrimitiveCull, FColor::Red);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PrimitiveCull_Loop);

	FTaskTagScope TaskTagScope(ETaskTag::EParallelRenderingThread);

	bool bDisableLODFade = GDisableLODFade || View.bDisableDistanceBasedFadeTransitions;
	const FPlane* PermutedPlanePtr = View.ViewFrustum.PermutedPlanes.GetData();
	const int32 BitArrayNumInner = View.PrimitiveVisibilityMap.Num();
	FVector ViewOriginForDistanceCulling = View.ViewMatrices.GetViewOrigin();
	float FadeRadius = bDisableLODFade ? 0.0f : GDistanceFadeMaxTravel;
	uint8 CustomVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;

	uint32 NumPrimitivesCulledForTask = 0;

	// Primitives may be explicitly removed from stereo views when using mono
	const int32 TaskWordOffset = TaskIndex * FrustumCullNumWordsPerTask;

	FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();

	for (int32 WordIndex = TaskWordOffset; WordIndex < TaskWordOffset + FrustumCullNumWordsPerTask && WordIndex * NumBitsPerDWORD < BitArrayNumInner; WordIndex++)
	{
		uint32 Mask = 0x1;
		uint32 VisBits = 0;
		uint32 FadingBits = 0;

		// If visibility culling is disabled, make sure to use the existing visibility state
		if (!Flags.bShouldVisibilityCull)
		{
			VisBits = View.PrimitiveVisibilityMap.GetData()[WordIndex];
		}
		

#if RHI_RAYTRACING
		uint32 RayTracingBits = 0;
#endif //RHI_RAYTRACING
		for (int32 BitSubIndex = 0; BitSubIndex < NumBitsPerDWORD && WordIndex * NumBitsPerDWORD + BitSubIndex < BitArrayNumInner; BitSubIndex++, Mask <<= 1)
		{
			int32 Index = WordIndex * NumBitsPerDWORD + BitSubIndex;
			bool bPrimitiveIsHidden = IsPrimitiveHidden(Scene, View, Index, Flags);
			bool bIsVisible = Flags.bShouldVisibilityCull ? true : (VisBits & Mask) == Mask;
			
			bIsVisible = bIsVisible && !bPrimitiveIsHidden;

#if RHI_RAYTRACING
			bool bIsVisibleInRayTracing = true;

			if (bPrimitiveIsHidden || ShouldCullForRayTracing(Scene, View, Index))
			{
				bIsVisibleInRayTracing = false;
			}
#endif //RHI_RAYTRACING

			const FPrimitiveBounds& RESTRICT Bounds = Scene->PrimitiveBounds[Index];

			// Handle primitives that are not always visible.
			if (Flags.bShouldVisibilityCull && bIsVisible && !IsAlwaysVisible(Scene, Index, Flags.bNaniteAlwaysVisible))
			{
				bool bShouldDistanceCull = true;
				bool bPartiallyOutside = true;
				bool bShouldFrustumCull = true;

				// Fading HLODs and their children must be visible, objects hidden by HLODs can be culled
				if (HLODState)
				{
					if (HLODState->IsNodeForcedVisible(Index))
					{
						bShouldDistanceCull = false;
					}
					else if (HLODState->IsNodeForcedHidden(Index))
					{
						bIsVisible = false;
					}
				}

				// Frustum first
				bShouldFrustumCull = bShouldFrustumCull && bIsVisible;
				if (bShouldFrustumCull)
				{
					if (Flags.bUseVisibilityOctree)
					{
						// If the parent octree node was completely contained by the frustum, there is no need do an additional frustum test on the primitive bounds
						// If the parent octree node is partially in the frustum, perform an additional test on the primitive bounds
						uint32 OctreeNodeIndex = Scene->PrimitiveOctreeIndex[Index];

						bIsVisible = VisibleNodes[OctreeNodeIndex * 2];
						bPartiallyOutside = VisibleNodes[OctreeNodeIndex * 2 + 1];
					}

					if (bIsVisible)
					{
						int32 VisibilityId = INDEX_NONE;

						if (Flags.bUseCustomCulling &&
							((Scene->PrimitiveOcclusionFlags[Index] & CustomVisibilityFlags) == CustomVisibilityFlags))
						{
							VisibilityId = Scene->Primitives[Index]->Proxy->GetVisibilityId();
						}

						bIsVisible = !bPartiallyOutside || IsPrimitiveVisible(View, PermutedPlanePtr, Bounds, VisibilityId, Flags);
					}
				}

				// Distance cull if frustum cull passed
				bShouldDistanceCull = bShouldDistanceCull && bIsVisible;
				if (bShouldDistanceCull)
				{
					// If cull distance is disabled, always show the primitive (except foliage)
					if (View.Family->EngineShowFlags.DistanceCulledPrimitives
						&& !Scene->Primitives[Index]->Proxy->IsDetailMesh()) // Proxy call is intentionally behind the DistancedCulledPrimitives check to prevent an expensive memory read
					{
						bShouldDistanceCull = false;
					}
				}

				if (bShouldDistanceCull)
				{
					// Preserve infinite draw distance
					bool bHasMaxDrawDistance = Bounds.MaxCullDistance < FLT_MAX;
					bool bHasMinDrawDistance = Bounds.MinDrawDistance > 0;

					if (bHasMaxDrawDistance || bHasMinDrawDistance)
					{
						float MaxDrawDistance = Bounds.MaxCullDistance * MaxDrawDistanceScale;
						float MinDrawDistanceSq = FMath::Square(Bounds.MinDrawDistance);
						float DistanceSquared = FVector::DistSquared(Bounds.BoxSphereBounds.Origin, ViewOriginForDistanceCulling);

						// Always test the fade in distance.  If a primitive was set to always draw, it may need to be faded in.
						if (bHasMaxDrawDistance)
						{
							float MaxFadeDistanceSquared = FMath::Square(MaxDrawDistance + FadeRadius);
							float MinFadeDistanceSquared = FMath::Square(MaxDrawDistance - FadeRadius);
							if ((DistanceSquared < MaxFadeDistanceSquared && DistanceSquared > MinFadeDistanceSquared)
								&& Scene->Primitives[Index]->Proxy->IsUsingDistanceCullFade())  // Proxy call is intentionally behind the fade check to prevent an expensive memory read
							{
								FadingBits |= Mask;
							}
						}

						// Check for distance culling first
						const bool bFarDistanceCulled = bHasMaxDrawDistance && (DistanceSquared > FMath::Square(MaxDrawDistance));
						const bool bNearDistanceCulled = bHasMinDrawDistance && (DistanceSquared < MinDrawDistanceSq);
						bool bIsDistanceCulled = bNearDistanceCulled || bFarDistanceCulled;

						if (bIsDistanceCulled)
						{
							bIsVisible = false;
						}

#if RHI_RAYTRACING
						if (bFarDistanceCulled)
						{
							bIsVisibleInRayTracing = false;
						}
#endif //RHI_RAYTRACING
					}
				}
			}

			if (bIsVisible)
			{
				// The primitive is visible!
				VisBits |= Mask;
			}
			else
			{
				STAT(++NumPrimitivesCulledForTask);
			}

#if RHI_RAYTRACING
			if (bIsVisibleInRayTracing)
			{
				RayTracingBits |= Mask;
			}
#endif //RHI_RAYTRACING
		}

		if (Flags.bShouldVisibilityCull && FadingBits)
		{
			checkSlow(!View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex]); // this should start at zero
			View.PotentiallyFadingPrimitiveMap.GetData()[WordIndex] = FadingBits;
		}

		if (Flags.bShouldVisibilityCull && VisBits)
		{
			checkSlow(!View.PrimitiveVisibilityMap.GetData()[WordIndex]); // this should start at zero
			View.PrimitiveVisibilityMap.GetData()[WordIndex] = VisBits;
		}

#if RHI_RAYTRACING
		if (RayTracingBits)
		{
			checkSlow(!View.PrimitiveRayTracingVisibilityMap.GetData()[WordIndex]); // this should start at zero
			View.PrimitiveRayTracingVisibilityMap.GetData()[WordIndex] = RayTracingBits;
		}
#endif
	}

	STAT(NumCulledPrimitives.Add(NumPrimitivesCulledForTask));
}

static int32 PrimitiveCull(const FScene* RESTRICT Scene, FViewInfo& View, bool bShouldVisibilityCull)
{
	FPrimitiveCullingFlags Flags;
	Flags.bShouldVisibilityCull = bShouldVisibilityCull;
	Flags.bUseCustomCulling = View.CustomVisibilityQuery && View.CustomVisibilityQuery->Prepare();
	Flags.bAlsoUseSphereTest = CVarAlsoUseSphereForFrustumCull.GetValueOnRenderThread() > 0;
	Flags.bUseFastIntersect = (View.ViewFrustum.PermutedPlanes.Num() == 8) && CVarUseFastIntersect.GetValueOnRenderThread();
	Flags.bUseVisibilityOctree = CVarUseVisibilityOctree.GetValueOnRenderThread() > 0;
	Flags.bNaniteAlwaysVisible = CVarNaniteMeshsAlwaysVisible.GetValueOnRenderThread() > 0;
	Flags.bHasHiddenPrimitives = View.HiddenPrimitives.Num() > 0;
	Flags.bHasShowOnlyPrimitives = View.ShowOnlyPrimitives.IsSet();

#if RHI_RAYTRACING
	View.RayTracingCullingParameters.Init(View);
#endif

	SCOPE_CYCLE_COUNTER(STAT_PrimitiveCull);

	FSceneBitArray VisibleNodes;

	if (bShouldVisibilityCull && Flags.bUseVisibilityOctree)
	{
		CullOctree(Scene, View, Flags, VisibleNodes);
	}

	//Primitives per ParallelFor task
	//Using async FrustumCull. Thanks Yager! See https://udn.unrealengine.com/questions/252385/performance-of-frustumcull.html
	//Performance varies on total primitive count and tasks scheduled. Check the mentioned link above for some measurements.
	//There have been some changes as compared to the code measured in the link

	FThreadSafeCounter NumCulledPrimitives;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	const bool bHLODActive = Scene->SceneLODHierarchy.IsActive();
	const FHLODVisibilityState* const HLODState = bHLODActive && ViewState ? &ViewState->HLODVisibilityState : nullptr;
	float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale * GetCachedScalabilityCVars().CalculateFieldOfViewDistanceScale(View.DesiredFOV);

	const int32 BitArrayNum = View.PrimitiveVisibilityMap.Num();
	const int32 BitArrayWords = FMath::DivideAndRoundUp(BitArrayNum, (int32)NumBitsPerDWORD);
	const int32 NumTasks = FMath::DivideAndRoundUp(BitArrayWords, FrustumCullNumWordsPerTask);

	ParallelFor(NumTasks,
		[&NumCulledPrimitives, Scene, &View, MaxDrawDistanceScale, HLODState, &VisibleNodes, &Flags](int32 TaskIndex)
		{
			PrimitiveCullTask(NumCulledPrimitives, Scene, View, Flags, MaxDrawDistanceScale, HLODState, VisibleNodes, TaskIndex);
		},
		!FApp::ShouldUseThreadingForPerformance() || (Flags.bUseCustomCulling && !View.CustomVisibilityQuery->IsThreadsafe()) || CVarParallelInitViews.GetValueOnRenderThread() == 0 || !IsInActualRenderingThread()
		);


	return NumCulledPrimitives.GetValue();
}

/**
 * Updated primitive fading states for the view.
 */
static void UpdatePrimitiveFading(const FScene* Scene, FViewInfo& View)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveFading);

	FSceneViewState* ViewState = (FSceneViewState*)View.State;

	if (ViewState)
	{
		uint32 PrevFrameNumber = ViewState->PrevFrameNumber;
		float CurrentRealTime = View.Family->Time.GetRealTimeSeconds();

		// First clear any stale fading states.
		for (FPrimitiveFadingStateMap::TIterator It(ViewState->PrimitiveFadingStates); It; ++It)
		{
			FPrimitiveFadingState& FadingState = It.Value();
			if (FadingState.FrameNumber != PrevFrameNumber ||
				(IsValidRef(FadingState.UniformBuffer) && CurrentRealTime >= FadingState.EndTime))
			{
				It.RemoveCurrent();
			}
		}

		// Should we allow fading transitions at all this frame?  For frames where the camera moved
		// a large distance or where we haven't rendered a view in awhile, it's best to disable
		// fading so users don't see unexpected object transitions.
		if (!GDisableLODFade && !View.bDisableDistanceBasedFadeTransitions)
		{
			// Do a pass over potentially fading primitives and update their states.
			for (FSceneSetBitIterator BitIt(View.PotentiallyFadingPrimitiveMap); BitIt; ++BitIt)
			{
				bool bVisible = View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt);
				FPrimitiveFadingState& FadingState = ViewState->PrimitiveFadingStates.FindOrAdd(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				UpdatePrimitiveFadingStateHelper(FadingState, View, bVisible);
				FRHIUniformBuffer* UniformBuffer = FadingState.UniformBuffer;
				if (UniformBuffer && !bVisible)
				{
					// If the primitive is fading out make sure it remains visible.
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = true;

#if RHI_RAYTRACING
					// Cannot just assume the ray tracing visibility will be true, so a complete recalculation for its culling needs to happen
					// This should be a very rare occurrence, so the hit is not worrisome.
					// TODO:  Could this be moved into the actual culling phase?

					if (!ShouldCullForRayTracing(Scene, View, BitIt.GetIndex()))
					{
						View.PrimitiveRayTracingVisibilityMap.AccessCorrespondingBit(BitIt) = true;
					}
#endif //RHI_RAYTRACING
				}
				View.PrimitiveFadeUniformBuffers[BitIt.GetIndex()] = UniformBuffer;
				View.PrimitiveFadeUniformBufferMap[BitIt.GetIndex()] = UniformBuffer != nullptr;
			}
		}
	}
}

struct FOcclusionBounds
{
	FOcclusionBounds(FPrimitiveOcclusionHistory* InPrimitiveOcclusionHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent, bool bInGroupedQuery)
		: PrimitiveOcclusionHistory(InPrimitiveOcclusionHistory)
		, BoundsOrigin(InBoundsOrigin)
		, BoundsExtent(InBoundsExtent)
		, bGroupedQuery(bInGroupedQuery)
	{
	}
	FOcclusionBounds(FPrimitiveOcclusionHistoryKey InPrimitiveOcclusionHistoryKey, const FVector& InBoundsOrigin, const FVector& InBoundsExtent, uint32 InLastQuerySubmitFrame)
		: PrimitiveOcclusionHistoryKey(InPrimitiveOcclusionHistoryKey)
		, BoundsOrigin(InBoundsOrigin)
		, BoundsExtent(InBoundsExtent)
		, LastQuerySubmitFrame(InLastQuerySubmitFrame)
	{
	}
	union 
	{
		FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory;
		FPrimitiveOcclusionHistoryKey PrimitiveOcclusionHistoryKey;
	};

	FVector BoundsOrigin;
	FVector BoundsExtent;	
	union 
	{
		bool bGroupedQuery;
		uint32 LastQuerySubmitFrame;
	};
};

struct FHZBBound
{
	FHZBBound(FPrimitiveOcclusionHistory* InTargetHistory, const FVector& InBoundsOrigin, const FVector& InBoundsExtent)
	: TargetHistory(InTargetHistory)
	, BoundsOrigin(InBoundsOrigin)
	, BoundsExtent(InBoundsExtent)
	{}

	FPrimitiveOcclusionHistory* const TargetHistory;
	const FVector BoundsOrigin;
	const FVector BoundsExtent;
};

#define BALANCE_LOAD 1
#define QUERY_SANITY_CHECK 0

struct FVisForPrimParams
{
	FVisForPrimParams(){}

	FVisForPrimParams(const FScene* InScene,
						FViewInfo* InView,
						FViewElementPDI* InOcclusionPDI,
						int32 InStartIndex,
						int32 InNumToProcess,
						bool bInSubmitQueries,
						bool bInHZBOcclusion,
						TArray<FOcclusionBounds>& OutQueriesToRun,
						TArray<bool>& OutSubIsOccluded)
		: Scene(InScene)
		, View(InView)
		, OcclusionPDI(InOcclusionPDI)		
		, StartIndex(InStartIndex)
		, NumToProcess(InNumToProcess)
		, bSubmitQueries(bInSubmitQueries)
		, bHZBOcclusion(bInHZBOcclusion)
		, QueriesToAdd(&OutQueriesToRun)
		, SubIsOccluded(&OutSubIsOccluded)
	{
		OutQueriesToRun.Reset();
		OutSubIsOccluded.Reset();
	}

	void Init(	const FScene* InScene,
				FViewInfo* InView,
				int32 InStartIndex,
				int32 InNumToProcess,
				bool bInSubmitQueries,
				bool bInHZBOcclusion,
				TArray<FPrimitiveOcclusionHistory>& OutOcclusionHistory,
				TArray<FPrimitiveOcclusionHistory*>& OutQueriesToRelease,
				TArray<FHZBBound>& OutHZBBounds,
				TArray<FOcclusionBounds>& OutQueriesToRun,
				TArray<bool>& OutSubIsOccluded)
	{
		OutOcclusionHistory.Reset();
		OutQueriesToRelease.Reset();
		OutHZBBounds.Reset();
		OutQueriesToRun.Reset();
		OutSubIsOccluded.Reset();

		Scene = InScene;
		View = InView;
		StartIndex = InStartIndex;
		NumToProcess = InNumToProcess;
		bSubmitQueries = bInSubmitQueries;
		bHZBOcclusion = bInHZBOcclusion;
		InsertPrimitiveOcclusionHistory = &OutOcclusionHistory;
		QueriesToRelease = &OutQueriesToRelease;
		HZBBoundsToAdd = &OutHZBBounds;
		QueriesToAdd = &OutQueriesToRun;
		SubIsOccluded = &OutSubIsOccluded;
	}

	const FScene* Scene{};
	FViewInfo* View{};
	FViewElementPDI* OcclusionPDI{};
	int32 StartIndex{};
	int32 NumToProcess{};
	bool bSubmitQueries{};
	bool bHZBOcclusion{};

	// Whether the entries written into the history need to be read using a scan search (see FPrimitiveOcclusionHistory::bNeedsScanOnRead)
	bool bNeedsScanOnRead{};

	//occlusion history to insert into.  In parallel these will be all merged back into the view's history on the main thread.
	//use TChunkedArray so pointers to the new FPrimitiveOcclusionHistory's won't change if the array grows.	
	TArray<FPrimitiveOcclusionHistory>*		InsertPrimitiveOcclusionHistory{};
	TArray<FPrimitiveOcclusionHistory*>*	QueriesToRelease{};
	TArray<FHZBBound>*						HZBBoundsToAdd{};
	TArray<FOcclusionBounds>*				QueriesToAdd{};
	int32									NumOccludedPrims{};
	TArray<bool>*							SubIsOccluded{};
};

//This function is shared between the single and multi-threaded versions.  Modifications to any primitives indexed by BitIt should be ok
//since only one of the task threads will ever reference it.  However, any modifications to shared state like the ViewState must be buffered
//to be recombined later.
template<bool bSingleThreaded>
static void FetchVisibilityForPrimitives_Range(FVisForPrimParams& Params, FGlobalDynamicVertexBuffer* DynamicVertexBufferIfSingleThreaded)
{
	SCOPED_NAMED_EVENT(FetchVisibilityForPrimitives_Range, FColor::Magenta);
	int32 NumOccludedPrimitives = 0;
	
	const FScene* Scene				= Params.Scene;
	FViewInfo& View					= *Params.View;
	FViewElementPDI* OcclusionPDI	= Params.OcclusionPDI;
	const int32 StartIndex			= Params.StartIndex;
	const int32 NumToProcess		= Params.NumToProcess;
	const bool bSubmitQueries		= Params.bSubmitQueries;
	const bool bHZBOcclusion		= Params.bHZBOcclusion;

	const float PrimitiveProbablyVisibleTime = GEngine->PrimitiveProbablyVisibleTime;

	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames(Scene->GetFeatureLevel());
	bool bClearQueries = !View.Family->EngineShowFlags.HitProxies;
	const float CurrentRealTime = View.Family->Time.GetRealTimeSeconds();
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;
	FOcclusionFeedback& OcclusionFeedback = ViewState->OcclusionFeedback;
	const bool bUseOcclusionFeedback = bSingleThreaded && OcclusionFeedback.IsInitialized();

	int32 ReadBackLagTolerance = NumBufferedFrames;

	const bool bIsStereoView = IStereoRendering::IsStereoEyeView(View);
	const bool bUseRoundRobinOcclusion = bIsStereoView && !View.bIsSceneCapture && View.ViewState->IsRoundRobinEnabled();
	if (bUseRoundRobinOcclusion)
	{
		// We don't allow clearing of a history entry if we do not also submit an occlusion query to replace the deleted one
		// as we want to keep the history as full as possible
		bClearQueries &= bSubmitQueries;

		// However, if this frame happens to be the first frame, then we clear anyway since in the first frame we should not be
		// reading past queries
		bClearQueries |= View.bIgnoreExistingQueries;

		// Round-robin occlusion culling involves reading frames that could be twice as stale as without round-robin
		ReadBackLagTolerance = NumBufferedFrames * 2;
	}
	// Round robin occlusion culling can make holes in the occlusion history which would require scanning the history when reading
	Params.bNeedsScanOnRead = bUseRoundRobinOcclusion;

	TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;
	TArray<FPrimitiveOcclusionHistory>* InsertPrimitiveOcclusionHistory = Params.InsertPrimitiveOcclusionHistory;
	TArray<FPrimitiveOcclusionHistory*>* QueriesToRelease = Params.QueriesToRelease;
	TArray<FHZBBound>* HZBBoundsToAdd = Params.HZBBoundsToAdd;
	TArray<FOcclusionBounds>* QueriesToAdd = Params.QueriesToAdd;	

	const bool bNewlyConsideredBBoxExpandActive = GExpandNewlyOcclusionTestedBBoxesAmount > 0.0f && GFramesToExpandNewlyOcclusionTestedBBoxes > 0 && GFramesNotOcclusionTestedToExpandBBoxes > 0;
	const float NeverOcclusionTestDistanceSquared = GNeverOcclusionTestDistance * GNeverOcclusionTestDistance;
	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();

	const int32 ReserveAmount = NumToProcess;
	int32 NumQueriesToReserve = NumToProcess;
	if (!bSingleThreaded)
	{		
		check(InsertPrimitiveOcclusionHistory);
		check(QueriesToRelease);
		check(HZBBoundsToAdd);
		check(QueriesToAdd);

		// We need to calculuate the actual number of queries to reserve since the pointers to InsertPrimitiveOcclusionHistory need to be preserved.
		if (GAllowSubPrimitiveQueries && !View.bDisableQuerySubmissions)
		{
			NumQueriesToReserve = 0;
			int32 NumProcessed = 0;
#if BALANCE_LOAD
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#else
			for (TBitArray<SceneRenderingBitArrayAllocator>::FIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#endif
			{
#if !BALANCE_LOAD		
				if (!View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt))
				{
					continue;
				}
#endif

				int32 Index = BitIt.GetIndex();

				const uint8 OcclusionFlags = Scene->PrimitiveOcclusionFlags[Index];

				if ((OcclusionFlags & EOcclusionFlags::CanBeOccluded) == 0)
				{
					continue;
				}

				if ((OcclusionFlags & EOcclusionFlags::HasSubprimitiveQueries) != 0)
				{
					NumQueriesToReserve += Scene->Primitives[Index]->Proxy->GetOcclusionQueries(&View)->Num();
				}
				else
				{
					NumQueriesToReserve++;
				}
			}
		}

		//avoid doing reallocs as much as possible.  Unlikely to make an entry per processed element.		
		InsertPrimitiveOcclusionHistory->Reserve(NumQueriesToReserve);
		QueriesToRelease->Reserve(ReserveAmount);
		HZBBoundsToAdd->Reserve(ReserveAmount);
		QueriesToAdd->Reserve(ReserveAmount);
	}
	
	int32 NumProcessed = 0;
	int32 NumTotalPrims = View.PrimitiveVisibilityMap.Num();
	int32 NumTotalDefUnoccluded = View.PrimitiveDefinitelyUnoccludedMap.Num();

	{
		// If we're going to stall the RHI thread for one query, we should stall it for all of them.
		// !(View.bIgnoreExistingQueries || bHZBOcclusion) is the code path that calls GetQueryForReading.
		const bool bShouldStallRHIThread = bSingleThreaded && GOcclusionSingleRHIThreadStall && !GSupportsParallelOcclusionQueries && IsInRenderingThread() && !(View.bIgnoreExistingQueries || bHZBOcclusion);
		FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList(), bShouldStallRHIThread);

		SCOPED_NAMED_EVENT_F(TEXT("forEach over %d entries"), FColor::Magenta, NumToProcess);
	
		//if we are load balanced then we iterate only the set bits, and the ranges have been pre-selected to evenly distribute set bits among the tasks with no overlaps.
		//if not, then the entire array is evenly divided by range.
#if BALANCE_LOAD
		for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#else
		for (TBitArray<SceneRenderingBitArrayAllocator>::FIterator BitIt(View.PrimitiveVisibilityMap, StartIndex); BitIt && (NumProcessed < NumToProcess); ++BitIt, ++NumProcessed)
#endif
		{
#if !BALANCE_LOAD		
			if (!View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt))
			{
				continue;
			}
#endif
			int32 Index = BitIt.GetIndex();

			const uint8 OcclusionFlags = Scene->PrimitiveOcclusionFlags[Index];

			if ((OcclusionFlags & EOcclusionFlags::CanBeOccluded) == 0)
			{
				View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
				continue;
			}

			//we can't allow the prim history insertion array to realloc or it will invalidate pointers in the other output arrays.
			const bool bCanAllocPrimHistory = bSingleThreaded || InsertPrimitiveOcclusionHistory->Num() < InsertPrimitiveOcclusionHistory->Max();

#if WITH_EDITOR
			bool bCanBeOccluded = true;
			if (GIsEditor)
			{
				if (Scene->PrimitivesSelected[Index])
				{
					// to render occluded outline for selected objects
					bCanBeOccluded = false;
				}
			}
#else
			constexpr bool bCanBeOccluded = true;
#endif

			int32 NumSubQueries = 1;
			bool bSubQueries = false;
			const TArray<FBoxSphereBounds>* SubBounds = nullptr;

			check(Params.SubIsOccluded);
			TArray<bool>& SubIsOccluded = *Params.SubIsOccluded;
			int32 SubIsOccludedStart = SubIsOccluded.Num();
			if ((OcclusionFlags & EOcclusionFlags::HasSubprimitiveQueries) && GAllowSubPrimitiveQueries && !View.bDisableQuerySubmissions)
			{
				FPrimitiveSceneProxy* Proxy = Scene->Primitives[Index]->Proxy;
				SubBounds = Proxy->GetOcclusionQueries(&View);
				NumSubQueries = SubBounds->Num();
				bSubQueries = true;
				if (!NumSubQueries)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					continue;
				}
				SubIsOccluded.Reserve(NumSubQueries);
			}

			bool bAllSubOcclusionStateIsDefinite = true;
			bool bAllSubOccluded = true;
			FPrimitiveComponentId PrimitiveId = Scene->PrimitiveComponentIds[Index];

			for (int32 SubQuery = 0; SubQuery < NumSubQueries; SubQuery++)
			{
				FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery));

				bool bIsOccluded = false;
				bool bOcclusionStateIsDefinite = false;

				if (!PrimitiveOcclusionHistory)
				{
					// If the primitive doesn't have an occlusion history yet, create it.
					if (bSingleThreaded)
					{
						// In singlethreaded mode we can safely modify the view's history directly.
						PrimitiveOcclusionHistory = &ViewPrimitiveOcclusionHistory[
							ViewPrimitiveOcclusionHistory.Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
						];
					}
					else if (bCanAllocPrimHistory)
					{
						// In multithreaded mode we have to buffer the new histories and add them to the view during a post-combine
						PrimitiveOcclusionHistory = &(*InsertPrimitiveOcclusionHistory)[
							InsertPrimitiveOcclusionHistory->Add(FPrimitiveOcclusionHistory(PrimitiveId, SubQuery))
						];
					}

					// If the primitive hasn't been visible recently enough to have a history, treat it as unoccluded this frame so it will be rendered as an occluder and its true occlusion state can be determined.
					// already set bIsOccluded = false;

					// Flag the primitive's occlusion state as indefinite, which will force it to be queried this frame.
					// The exception is if the primitive isn't occludable, in which case we know that it's definitely unoccluded.
					bOcclusionStateIsDefinite = !bCanBeOccluded;
				}
				else
				{
					if (View.bIgnoreExistingQueries)
					{
						// If the view is ignoring occlusion queries, the primitive is definitely unoccluded.
						// already set bIsOccluded = false;
						bOcclusionStateIsDefinite = View.bDisableQuerySubmissions;
					}
					else if (bCanBeOccluded)
					{
						if (bUseOcclusionFeedback)
						{
							bIsOccluded = OcclusionFeedback.IsOccluded(PrimitiveId);
							bOcclusionStateIsDefinite = true;
						}
						else if (bHZBOcclusion)
						{
							if (HZBOcclusionTests.IsValidFrame(PrimitiveOcclusionHistory->LastTestFrameNumber))
							{
								bIsOccluded = !HZBOcclusionTests.IsVisible(PrimitiveOcclusionHistory->HZBTestIndex);
								bOcclusionStateIsDefinite = true;
							}
						}
						else
						{
							// Read the occlusion query results.
							uint64 NumSamples = 0;
							bool bGrouped = false;
							FRHIRenderQuery* PastQuery = PrimitiveOcclusionHistory->GetQueryForReading(OcclusionFrameCounter, NumBufferedFrames, ReadBackLagTolerance, bGrouped);
							if (PastQuery)
							{
								//int32 RefCount = PastQuery.GetReference()->GetRefCount();
								// NOTE: RHIGetOcclusionQueryResult should never fail when using a blocking call, rendering artifacts may show up.
								if (RHIGetRenderQueryResult(PastQuery, NumSamples, true))
								{
									// we render occlusion without MSAA
									uint32 NumPixels = (uint32)NumSamples;

									// The primitive is occluded if none of its bounding box's pixels were visible in the previous frame's occlusion query.
									bIsOccluded = (NumPixels == 0);


									if (!bIsOccluded)
									{
										checkSlow(View.OneOverNumPossiblePixels > 0.0f);
										PrimitiveOcclusionHistory->LastPixelsPercentage = NumPixels * View.OneOverNumPossiblePixels;
									}
									else
									{
										PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
									}


									// Flag the primitive's occlusion state as definite if it wasn't grouped.
									bOcclusionStateIsDefinite = !bGrouped;
								}
								else
								{
									// If the occlusion query failed, treat the primitive as visible.  
									// already set bIsOccluded = false;
								}
							}
							else
							{
								if (NumBufferedFrames > 1 || GRHIMaximumReccommendedOustandingOcclusionQueries < MAX_int32)
								{
									// If there's no occlusion query for the primitive, assume it is whatever it was last frame
									bIsOccluded = PrimitiveOcclusionHistory->WasOccludedLastFrame;
									bOcclusionStateIsDefinite = PrimitiveOcclusionHistory->OcclusionStateWasDefiniteLastFrame;
								}
								else
								{
									// If there's no occlusion query for the primitive, set it's visibility state to whether it has been unoccluded recently.
									bIsOccluded = (PrimitiveOcclusionHistory->LastProvenVisibleTime + GEngine->PrimitiveProbablyVisibleTime < CurrentRealTime);
									// the state was definite last frame, otherwise we would have ran a query
									bOcclusionStateIsDefinite = true;
								}
								if (bIsOccluded)
								{
									PrimitiveOcclusionHistory->LastPixelsPercentage = 0.0f;
								}
								else
								{
									PrimitiveOcclusionHistory->LastPixelsPercentage = GEngine->MaxOcclusionPixelsFraction;
								}
							}
						}

						if (GVisualizeOccludedPrimitives && OcclusionPDI && bIsOccluded)
						{
							const FBoxSphereBounds& Bounds = bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[Index];
							DrawWireBox(OcclusionPDI, Bounds.GetBox(), FColor(50, 255, 50), SDPG_Foreground);
						}
					}
					else
					{
						// Primitives that aren't occludable are considered definitely unoccluded.
						// already set bIsOccluded = false;
						bOcclusionStateIsDefinite = true;
					}

					if (bClearQueries)
					{
						if (bSingleThreaded)
						{
							PrimitiveOcclusionHistory->ReleaseQuery(OcclusionFrameCounter, NumBufferedFrames);
						}
						else
						{
							if (PrimitiveOcclusionHistory->GetQueryForEviction(OcclusionFrameCounter, NumBufferedFrames))
							{
								QueriesToRelease->Add(PrimitiveOcclusionHistory);
							}
						}
					}
				}

				if (PrimitiveOcclusionHistory)
				{
					if (bSubmitQueries && bCanBeOccluded)
					{
						bool bSkipNewlyConsidered = false;

						if (bNewlyConsideredBBoxExpandActive)
						{
							if (!PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown && OcclusionFrameCounter - PrimitiveOcclusionHistory->LastConsideredFrameNumber > uint32(GFramesNotOcclusionTestedToExpandBBoxes))
							{
								PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown = GFramesToExpandNewlyOcclusionTestedBBoxes;
							}

							bSkipNewlyConsidered = !!PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown;

							if (bSkipNewlyConsidered)
							{
								PrimitiveOcclusionHistory->BecameEligibleForQueryCooldown--;
							}
						}


						bool bAllowBoundsTest;
						const FBoxSphereBounds OcclusionBounds = (bSubQueries ? (*SubBounds)[SubQuery] : Scene->PrimitiveOcclusionBounds[Index]).ExpandBy(GExpandAllTestedBBoxesAmount + (bSkipNewlyConsidered ? GExpandNewlyOcclusionTestedBBoxesAmount : 0.0));
						if (FVector::DistSquared(ViewOrigin, OcclusionBounds.Origin) < NeverOcclusionTestDistanceSquared)
						{
							bAllowBoundsTest = false;
						}
						else if (View.bHasNearClippingPlane)
						{
							bAllowBoundsTest = View.NearClippingPlane.PlaneDot(OcclusionBounds.Origin) <
								-(FVector::BoxPushOut(View.NearClippingPlane, OcclusionBounds.BoxExtent));

						}
						else if (!View.IsPerspectiveProjection())
						{
							// Transform parallel near plane
							static_assert((int32)ERHIZBuffer::IsInverted != 0, "Check equation for culling!");
							bAllowBoundsTest = View.WorldToScreen(OcclusionBounds.Origin).Z - View.ViewMatrices.GetProjectionMatrix().M[2][2] * OcclusionBounds.SphereRadius < 1;
						}
						else
						{
							bAllowBoundsTest = OcclusionBounds.SphereRadius < HALF_WORLD_MAX;
						}

						if (bAllowBoundsTest)
						{
							PrimitiveOcclusionHistory->LastTestFrameNumber = OcclusionFrameCounter;

							if (bUseOcclusionFeedback)
							{
								const FVector BoundOrigin = OcclusionBounds.Origin + View.ViewMatrices.GetPreViewTranslation();
								const FVector BoundExtent = OcclusionBounds.BoxExtent;
								OcclusionFeedback.AddPrimitive(PrimitiveId, BoundOrigin, BoundExtent, *DynamicVertexBufferIfSingleThreaded);
							}
							else if (bHZBOcclusion)
							{
								// Always run
								if (bSingleThreaded)
								{
									PrimitiveOcclusionHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
								}
								else
								{
									HZBBoundsToAdd->Emplace(PrimitiveOcclusionHistory, OcclusionBounds.Origin, OcclusionBounds.BoxExtent);
								}
							}
							else
							{
								// decide if a query should be run this frame
								bool bRunQuery, bGroupedQuery;

								if (!bSubQueries && // sub queries are never grouped, we assume the custom code knows what it is doing and will group internally if it wants
									(OcclusionFlags & EOcclusionFlags::AllowApproximateOcclusion))
								{
									if (bIsOccluded)
									{
										// Primitives that were occluded the previous frame use grouped queries.
										bGroupedQuery = true;
										bRunQuery = true;
									}
									else if (bOcclusionStateIsDefinite)
									{
										bGroupedQuery = false;
										float Rnd = GOcclusionRandomStream.GetFraction();
										if (GRHISupportsExactOcclusionQueries)
										{
											float FractionMultiplier = FMath::Max(PrimitiveOcclusionHistory->LastPixelsPercentage / GEngine->MaxOcclusionPixelsFraction, 1.0f);
											bRunQuery = (FractionMultiplier * Rnd) < GEngine->MaxOcclusionPixelsFraction;
										}
										else
										{
											bRunQuery = CurrentRealTime - PrimitiveOcclusionHistory->LastProvenVisibleTime > PrimitiveProbablyVisibleTime * (0.5f * 0.25f * Rnd);
										}
									}
									else
									{
										bGroupedQuery = false;
										bRunQuery = true;
									}
								}
								else
								{
									// Primitives that need precise occlusion results use individual queries.
									bGroupedQuery = false;
									bRunQuery = true;
								}

								if (bRunQuery)
								{
									const FVector BoundOrigin = OcclusionBounds.Origin + View.ViewMatrices.GetPreViewTranslation();
									const FVector BoundExtent = OcclusionBounds.BoxExtent;

									if (bSingleThreaded)
									{
										checkSlow(DynamicVertexBufferIfSingleThreaded);

										if (GRHIMaximumReccommendedOustandingOcclusionQueries < MAX_int32 && !bGroupedQuery)
										{
											QueriesToAdd->Emplace(FPrimitiveOcclusionHistoryKey(PrimitiveId, SubQuery), BoundOrigin, BoundExtent, PrimitiveOcclusionHistory->LastQuerySubmitFrame());
										}
										else
										{
											PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
												bGroupedQuery ?
												View.GroupedOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent, *DynamicVertexBufferIfSingleThreaded) :
												View.IndividualOcclusionQueries.BatchPrimitive(BoundOrigin, BoundExtent, *DynamicVertexBufferIfSingleThreaded),
												NumBufferedFrames,
												bGroupedQuery,
												Params.bNeedsScanOnRead
											);
										}
									}
									else
									{
										QueriesToAdd->Emplace(PrimitiveOcclusionHistory, BoundOrigin, BoundExtent, bGroupedQuery);
									}
								}
							}
						}
						else
						{
							// If the primitive's bounding box intersects the near clipping plane, treat it as definitely unoccluded.
							bIsOccluded = false;
							bOcclusionStateIsDefinite = true;
						}
					}
					// Set the primitive's considered time to keep its occlusion history from being trimmed.
					PrimitiveOcclusionHistory->LastConsideredTime = CurrentRealTime;
					if (!bIsOccluded && bOcclusionStateIsDefinite)
					{
						PrimitiveOcclusionHistory->LastProvenVisibleTime = CurrentRealTime;
					}
					PrimitiveOcclusionHistory->LastConsideredFrameNumber = OcclusionFrameCounter;
					PrimitiveOcclusionHistory->WasOccludedLastFrame = bIsOccluded;
					PrimitiveOcclusionHistory->OcclusionStateWasDefiniteLastFrame = bOcclusionStateIsDefinite;
				}

				if (bSubQueries)
				{
					SubIsOccluded.Add(bIsOccluded);
					if (!bIsOccluded)
					{
						bAllSubOccluded = false;
					}
					if (bIsOccluded || !bOcclusionStateIsDefinite)
					{
						bAllSubOcclusionStateIsDefinite = false;
					}
				}
				else
				{

					if (bIsOccluded)
					{
						View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
						STAT(NumOccludedPrimitives++);
					}
					else if (bOcclusionStateIsDefinite)
					{
						View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
					}
				}
			}

			if (bSubQueries)
			{
				if (SubIsOccluded.Num() > 0)
				{
					FPrimitiveSceneProxy* Proxy = Scene->Primitives[Index]->Proxy;
					Proxy->AcceptOcclusionResults(&View, &SubIsOccluded, SubIsOccludedStart, SubIsOccluded.Num() - SubIsOccludedStart);
				}

				if (bAllSubOccluded)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					STAT(NumOccludedPrimitives++);
				}
				else if (bAllSubOcclusionStateIsDefinite)
				{
					View.PrimitiveDefinitelyUnoccludedMap.AccessCorrespondingBit(BitIt) = true;
				}
			}
		}
	}
	check(NumTotalDefUnoccluded == View.PrimitiveDefinitelyUnoccludedMap.Num());
	check(NumTotalPrims == View.PrimitiveVisibilityMap.Num());
	check(!InsertPrimitiveOcclusionHistory || InsertPrimitiveOcclusionHistory->Num() <= NumQueriesToReserve);
	Params.NumOccludedPrims = NumOccludedPrimitives;	
}

static int32 FetchVisibilityForPrimitives(const FScene* Scene, FViewInfo& View, const bool bSubmitQueries, const bool bHZBOcclusion, FGlobalDynamicVertexBuffer& DynamicVertexBuffer)
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(FetchVisibilityForPrimitives);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitives);
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	SCOPED_NAMED_EVENT(FetchVisibilityForPrimitives, FColor::Magenta);
	
	static int32 SubIsOccludedArrayIndex = 0;
	SubIsOccludedArrayIndex = 1 - SubIsOccludedArrayIndex;

	const int32 NumBufferedFrames = FOcclusionQueryHelpers::GetNumBufferedFrames(Scene->GetFeatureLevel());
	uint32 OcclusionFrameCounter = ViewState->OcclusionFrameCounter;
	TSet<FPrimitiveOcclusionHistory, FPrimitiveOcclusionHistoryKeyFuncs>& ViewPrimitiveOcclusionHistory = ViewState->PrimitiveOcclusionHistorySet;

	if (GOcclusionCullParallelPrimFetch && GSupportsParallelOcclusionQueries)
	{
		SCOPED_NAMED_EVENT(FetchVisibilityParallel, FColor::Magenta);
		constexpr int32 MaxNumCullTasks = 8;
		constexpr int32 ActualNumCullTasks = 8;
		constexpr int32 NumOutputArrays = MaxNumCullTasks;

		//params for each task
		FVisForPrimParams Params[NumOutputArrays];

		//output arrays for each task
		TArray<FPrimitiveOcclusionHistory> OutputOcclusionHistory[NumOutputArrays];
		TArray<FPrimitiveOcclusionHistory*> OutQueriesToRelease[NumOutputArrays];
		TArray<FHZBBound> OutHZBBounds[NumOutputArrays];
		TArray<FOcclusionBounds> OutQueriesToRun[NumOutputArrays];

		static TArray<bool> FrameSubIsOccluded[NumOutputArrays][FSceneView::NumBufferedSubIsOccludedArrays];

		//optionally balance the tasks by how the visible primitives are distributed in the array rather than just breaking up the array by range.
		//should make the tasks more equal length.
#if BALANCE_LOAD
		int32 StartIndices[NumOutputArrays] = { 0 };
		int32 ProcessRange[NumOutputArrays] = { 0 };
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesPreProcess);
			int32 NumBitsSet = 0;
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt, ++NumBitsSet)
			{
			}

			int32 BitsPerTask = NumBitsSet / ActualNumCullTasks;
			int32 NumBitsForRange = 0;
			int32 CurrentStartIndex = 0;
			int32 RangeToSet = 0;

			//accumulate set bits for each task until we reach the target, then set the start/end and move on.
			for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt && RangeToSet < (ActualNumCullTasks - 1); ++BitIt)
			{
				++NumBitsForRange;
				if (NumBitsForRange == BitsPerTask)
				{
					StartIndices[RangeToSet] = CurrentStartIndex;
					ProcessRange[RangeToSet] = NumBitsForRange;

					++RangeToSet;
					NumBitsForRange = 0;
					CurrentStartIndex = BitIt.GetIndex() + 1;
				}
			}

			//final range is the rest of the set bits, no matter how many there are.
			StartIndices[ActualNumCullTasks - 1] = CurrentStartIndex;
			ProcessRange[ActualNumCullTasks - 1] = NumBitsSet - (BitsPerTask * 3);
		}
#endif

		const int32 NumPrims = View.PrimitiveVisibilityMap.Num();
		const int32 NumPerTask = NumPrims / ActualNumCullTasks;
		int32 StartIndex = 0;

		int32 NumTasks = 0;
		for (int32 i = 0; i < ActualNumCullTasks && (StartIndex < NumPrims); ++i, ++NumTasks)
		{
			const int32 NumToProcess = (i == (ActualNumCullTasks - 1)) ? (NumPrims - StartIndex) : NumPerTask;

			Params[i].Init(
				Scene,
				&View,
#if BALANCE_LOAD
				StartIndices[i],
				ProcessRange[i],
#else
				StartIndex,
				NumToProcess,
#endif
				bSubmitQueries,
				bHZBOcclusion,
				OutputOcclusionHistory[i],
				OutQueriesToRelease[i],
				OutHZBBounds[i],
				OutQueriesToRun[i],
				FrameSubIsOccluded[i][SubIsOccludedArrayIndex]
			);

			StartIndex += NumToProcess;
		}

		ParallelFor(NumTasks,
			[&Params](int32 Index)
			{
				FetchVisibilityForPrimitives_Range<false>(Params[Index], nullptr);
			},
			!(FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0 && IsInActualRenderingThread())
		);

		FHZBOcclusionTester& HZBOcclusionTests = ViewState->HZBOcclusionTests;		

		int32 NumOccludedPrims = 0;
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesCombine);
			SCOPED_NAMED_EVENT(FetchVisibilityForPrimitivesCombine, FColor::Magenta);

#if QUERY_SANITY_CHECK
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_FetchVisibilityForPrimitivesSanity);
				TSet<int32> ReleaseQuerySet;
				TSet<int32> RunQuerySet;
				for (int32 i = 0; i < NumTasks; ++i)
				{
					bool bAlreadyIn = false;
					for (FPrimitiveOcclusionHistory* History : OutQueriesToRelease[i])
					{
						ReleaseQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double released query."), History->PrimitiveId.PrimIDValue);
					}

					for (const FOcclusionBounds& OcclusionBounds : OutQueriesToRun[i])
					{
						FPrimitiveOcclusionHistory* History = OcclusionBounds->PrimitiveOcclusionHistory;
						RunQuerySet.Add(History->PrimitiveId.PrimIDValue, &bAlreadyIn);
						checkf(!bAlreadyIn, TEXT("Prim: %i double run query."), History->PrimitiveId.PrimIDValue);
					}					
				}
			}
#endif

			//Add/Release query ops use stored PrimitiveHistory pointers. We must do ALL of these from all tasks before adding any new PrimitiveHistories to the view.
			//Adding new histories to the view could cause the array to resize which would invalidate all the stored output pointers for the other operations.
			for (int32 i = 0; i < NumTasks; ++i)
			{
				//HZB output
				for (const FHZBBound& HZBBounds : OutHZBBounds[i])
				{
					HZBBounds.TargetHistory->HZBTestIndex = HZBOcclusionTests.AddBounds(HZBBounds.BoundsOrigin, HZBBounds.BoundsExtent);
				}

				//Manual query release handling
				for (FPrimitiveOcclusionHistory* History : OutQueriesToRelease[i])
				{
					History->ReleaseQuery(OcclusionFrameCounter, NumBufferedFrames);
				}
				
				//New query batching
				for (const FOcclusionBounds& OcclusionBounds : OutQueriesToRun[i])
				{
					OcclusionBounds.PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						OcclusionBounds.bGroupedQuery ?
						View.GroupedOcclusionQueries.BatchPrimitive(OcclusionBounds.BoundsOrigin, OcclusionBounds.BoundsExtent, DynamicVertexBuffer) :
						View.IndividualOcclusionQueries.BatchPrimitive(OcclusionBounds.BoundsOrigin, OcclusionBounds.BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						OcclusionBounds.bGroupedQuery,
						Params[i].bNeedsScanOnRead
						);
				}
			}

			//now add new primitive histories to the view. may resize the view's array.
			for (int32 i = 0; i < NumTasks; ++i)
			{											
				ViewPrimitiveOcclusionHistory.Append(MoveTemp(OutputOcclusionHistory[i]));

				//accumulate occluded prims across tasks
				NumOccludedPrims += Params[i].NumOccludedPrims;
			}
		}
		
		return NumOccludedPrims;
	}
	else
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FetchVisibilityOther);
		
		static TArray<FOcclusionBounds> PendingIndividualQueriesWhenOptimizing;

		FViewElementPDI OcclusionPDI(&View, nullptr, nullptr);
		int32 StartIndex = 0;
		int32 NumToProcess = View.PrimitiveVisibilityMap.Num();				
		FVisForPrimParams Params(
			Scene,
			&View,
			&OcclusionPDI,
			StartIndex,
			NumToProcess,
			bSubmitQueries,
			bHZBOcclusion,
			PendingIndividualQueriesWhenOptimizing,
			//SubIsOccluded stuff needs a frame's lifetime
			View.FrameSubIsOccluded[SubIsOccludedArrayIndex]
			);

		FetchVisibilityForPrimitives_Range<true>(Params, &DynamicVertexBuffer);

		int32 IndQueries = PendingIndividualQueriesWhenOptimizing.Num();
		if (IndQueries)
		{
			static TArray<FOcclusionBounds*> PendingIndividualQueriesWhenOptimizingSorter;
			PendingIndividualQueriesWhenOptimizingSorter.Reset();

			int32 SoftMaxQueries = GRHIMaximumReccommendedOustandingOcclusionQueries / FMath::Min(NumBufferedFrames, 2); // extra RHIT frame does not count
			int32 UsedQueries = View.GroupedOcclusionQueries.GetNumBatchOcclusionQueries();

			int32 FirstQueryToDo = 0;
			int32 QueriesToDo = IndQueries;


			if (SoftMaxQueries < UsedQueries + IndQueries)
			{
				QueriesToDo = (IndQueries + 9) / 10;  // we need to make progress, even if it means stalling and waiting for the GPU. At a minimum, we will do 10%

				if (SoftMaxQueries > UsedQueries + QueriesToDo)
				{
					// we can do more than the minimum
					QueriesToDo = SoftMaxQueries - UsedQueries;
				}
			}
			if (QueriesToDo == IndQueries)
			{
				for (int32 Index = 0; Index < IndQueries; Index++)
				{
					FOcclusionBounds* RunQueriesIter = &PendingIndividualQueriesWhenOptimizing[Index];
					FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(RunQueriesIter->PrimitiveOcclusionHistoryKey);

					PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						false,
						Params.bNeedsScanOnRead
					);
				}
			}
			else
			{
				check(QueriesToDo < IndQueries);
				PendingIndividualQueriesWhenOptimizingSorter.Reserve(PendingIndividualQueriesWhenOptimizing.Num());
				for (int32 Index = 0; Index < IndQueries; Index++)
				{
					FOcclusionBounds* RunQueriesIter = &PendingIndividualQueriesWhenOptimizing[Index];
					PendingIndividualQueriesWhenOptimizingSorter.Add(RunQueriesIter);
				}

				PendingIndividualQueriesWhenOptimizingSorter.Sort(
					[](const FOcclusionBounds& A, const FOcclusionBounds& B) 
					{
						return A.LastQuerySubmitFrame < B.LastQuerySubmitFrame;
					}
				);
				for (int32 Index = 0; Index < QueriesToDo; Index++)
				{
					FOcclusionBounds* RunQueriesIter = PendingIndividualQueriesWhenOptimizingSorter[Index];
					FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = ViewPrimitiveOcclusionHistory.Find(RunQueriesIter->PrimitiveOcclusionHistoryKey);
					PrimitiveOcclusionHistory->SetCurrentQuery(OcclusionFrameCounter,
						View.IndividualOcclusionQueries.BatchPrimitive(RunQueriesIter->BoundsOrigin, RunQueriesIter->BoundsExtent, DynamicVertexBuffer),
						NumBufferedFrames,
						false,
						Params.bNeedsScanOnRead
					);
				}
			}


			// lets prevent this from staying too large for too long
			if (PendingIndividualQueriesWhenOptimizing.GetSlack() > IndQueries * 4)
			{
				PendingIndividualQueriesWhenOptimizing.Empty();
				PendingIndividualQueriesWhenOptimizingSorter.Empty();
			}
			else
			{
				PendingIndividualQueriesWhenOptimizing.Reset();
				PendingIndividualQueriesWhenOptimizingSorter.Reset();
			}
		}
		return Params.NumOccludedPrims;
	}
}

/**
 * Cull occluded primitives in the view.
 */
static int32 OcclusionCull(FRHICommandListImmediate& RHICmdList, const FScene* Scene, FViewInfo& View, FGlobalDynamicVertexBuffer& DynamicVertexBuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_OcclusionCull);	
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_OcclusionReadback));

	// INITVIEWS_TODO: This could be more efficient if broken up in to separate concerns:
	// - What is occluded?
	// - For which primitives should we render occlusion queries?
	// - Generate occlusion query geometry.

	int32 NumOccludedPrimitives = 0;
	FSceneViewState* ViewState = (FSceneViewState*)View.State;
	
	// Disable HZB on OpenGL platforms to avoid rendering artifacts
	// It can be forced on by setting HZBOcclusion to 2
	bool bHZBOcclusion = !IsOpenGLPlatform(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]);
	bHZBOcclusion = bHZBOcclusion && GHZBOcclusion;
	bHZBOcclusion = bHZBOcclusion && FDataDrivenShaderPlatformInfo::GetSupportsHZBOcclusion(GShaderPlatformForFeatureLevel[Scene->GetFeatureLevel()]);
	bHZBOcclusion = bHZBOcclusion || (GHZBOcclusion == 2);

	// Use precomputed visibility data if it is available.
	if (View.PrecomputedVisibilityData)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_LookupPrecomputedVisibility);

		FViewElementPDI OcclusionPDI(&View, nullptr, nullptr);
		uint8 PrecomputedVisibilityFlags = EOcclusionFlags::CanBeOccluded | EOcclusionFlags::HasPrecomputedVisibility;
		for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
		{
			if ((Scene->PrimitiveOcclusionFlags[BitIt.GetIndex()] & PrecomputedVisibilityFlags) == PrecomputedVisibilityFlags)
			{
				FPrimitiveVisibilityId VisibilityId = Scene->PrimitiveVisibilityIds[BitIt.GetIndex()];
				if ((View.PrecomputedVisibilityData[VisibilityId.ByteIndex] & VisibilityId.BitMask) == 0)
				{
					View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					INC_DWORD_STAT_BY(STAT_StaticallyOccludedPrimitives,1);
					STAT(NumOccludedPrimitives++);

					if (GVisualizeOccludedPrimitives)
					{
						const FBoxSphereBounds& Bounds = Scene->PrimitiveOcclusionBounds[BitIt.GetIndex()];
						DrawWireBox(&OcclusionPDI, Bounds.GetBox(), FColor(100, 50, 50), SDPG_Foreground);
					}
				}
			}
		}
	}

	float CurrentRealTime = View.Family->Time.GetRealTimeSeconds();
	if (ViewState)
	{
		bool bSubmitQueries = !View.bDisableQuerySubmissions;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		bSubmitQueries = bSubmitQueries && !ViewState->HasViewParent() && !ViewState->bIsFrozen;
#endif

		if (ViewState->OcclusionFeedback.IsInitialized())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_OcclusionFeedback_ReadbackResults);
			ViewState->OcclusionFeedback.ReadbackResults(RHICmdList);
			ViewState->OcclusionFeedback.AdvanceFrame(ViewState->OcclusionFrameCounter);
		}
		else if( bHZBOcclusion )
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_MapHZBResults);
			check(!ViewState->HZBOcclusionTests.IsValidFrame(ViewState->OcclusionFrameCounter));
			ViewState->HZBOcclusionTests.MapResults(RHICmdList);
		}
 
		// Perform round-robin occlusion queries
		if (View.ViewState->IsRoundRobinEnabled() &&
			!View.bIsSceneCapture && // We only round-robin on the main renderer (not scene captures)
			!View.bIgnoreExistingQueries && // We do not alternate occlusion queries when we want to refresh the occlusion history
			(IStereoRendering::IsStereoEyeView(View))) // Only relevant to stereo views
		{
			// For even frames, prevent left eye from occlusion querying
			// For odd frames, prevent right eye from occlusion querying
			const bool FrameParity = ((View.ViewState->PrevFrameNumber & 0x01) == 1);
			bSubmitQueries &= (FrameParity  && IStereoRendering::IsAPrimaryView(View)) ||
								(!FrameParity && IStereoRendering::IsASecondaryView(View));
		}

		View.ViewState->PrimitiveOcclusionQueryPool.AdvanceFrame(
			ViewState->OcclusionFrameCounter,
			FOcclusionQueryHelpers::GetNumBufferedFrames(Scene->GetFeatureLevel()),
			View.ViewState->IsRoundRobinEnabled() && !View.bIsSceneCapture && IStereoRendering::IsStereoEyeView(View));

		NumOccludedPrimitives += FetchVisibilityForPrimitives(Scene, View, bSubmitQueries, bHZBOcclusion, DynamicVertexBuffer);

		if( bHZBOcclusion )
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_HZBUnmapResults);

			ViewState->HZBOcclusionTests.UnmapResults(RHICmdList);

			if( bSubmitQueries )
			{
				ViewState->HZBOcclusionTests.SetValidFrameNumber(ViewState->OcclusionFrameCounter);
			}
		}

		if (View.FeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			// Initialize/release OcclusionFeedback system on demand
			if (GOcclusionFeedback_Enable == 0 && ViewState->OcclusionFeedback.IsInitialized())
			{
				ViewState->OcclusionFeedback.ReleaseResource();
			}
			else if (GOcclusionFeedback_Enable != 0 && !ViewState->OcclusionFeedback.IsInitialized())
			{
				ViewState->OcclusionFeedback.InitResource();
			}
		}
	}
	RHICmdList.SetCurrentStat(GET_STATID(STAT_CLMM_AfterOcclusionReadback));
	return NumOccludedPrimitives;
}

const int32 InputsPrimNumPerRelevancePacket = 128;
const int32 AverageMeshBatchNumPerRelevancePacket = InputsPrimNumPerRelevancePacket * 2;

template<class T, int TAmplifyFactor = 1>
struct FRelevancePrimSet
{
	enum
	{
		MaxInputPrims = InputsPrimNumPerRelevancePacket - 1, // leave space for NumPrims.
		MaxOutputPrims = MaxInputPrims * TAmplifyFactor
	};
	int32 NumPrims;

	T Prims[MaxOutputPrims];

	FORCEINLINE FRelevancePrimSet()
		: NumPrims(0)
	{
		//FMemory::Memzero(Prims, sizeof(T) * GetMaxOutputPrim());
	}
	FORCEINLINE void AddPrim(T Prim)
	{
		checkSlow(NumPrims < MaxOutputPrims);
		Prims[NumPrims++] = Prim;
	}
	FORCEINLINE bool IsFull() const
	{
		return NumPrims >= MaxOutputPrims;
	}
	template<class TARRAY>
	FORCEINLINE void AppendTo(TARRAY& DestArray)
	{
		DestArray.Append(Prims, NumPrims);
	}
};

struct FMarkRelevantStaticMeshesForViewData
{
	FVector ViewOrigin;
	int32 ForcedLODLevel;
	float LODScale;
	float MinScreenRadiusForCSMDepthSquared;
	float MinScreenRadiusForDepthPrepassSquared;
	bool bFullEarlyZPass;

	FMarkRelevantStaticMeshesForViewData(FViewInfo& View)
	{
		ViewOrigin = View.ViewMatrices.GetViewOrigin();

		// outside of the loop to be more efficient
		ForcedLODLevel = (View.Family->EngineShowFlags.LOD) ? GetCVarForceLOD() : 0;

		LODScale = CVarStaticMeshLODDistanceScale.GetValueOnRenderThread() * View.LODDistanceFactor;

		MinScreenRadiusForCSMDepthSquared = GMinScreenRadiusForCSMDepth * GMinScreenRadiusForCSMDepth;
		MinScreenRadiusForDepthPrepassSquared = GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass;

		extern bool ShouldForceFullDepthPass(EShaderPlatform ShaderPlatform);
		EShaderPlatform ShaderPlatform = View.GetShaderPlatform();
		if (IsMobilePlatform(ShaderPlatform))
		{
			FScene* Scene = View.Family->Scene->GetRenderScene();
			bFullEarlyZPass = (Scene && Scene->EarlyZPassMode == DDM_AllOpaque);
		}
		else
		{
			bFullEarlyZPass = ShouldForceFullDepthPass(ShaderPlatform);
		}
	}
};

namespace EMarkMaskBits
{
	enum Type
	{
		StaticMeshVisibilityMapMask = 0x2,
		StaticMeshFadeOutDitheredLODMapMask = 0x10,
		StaticMeshFadeInDitheredLODMapMask = 0x20,
	};
}

typedef TArray<FVisibleMeshDrawCommand> FPassDrawCommandArray;
typedef TArray<const FStaticMeshBatch*> FPassDrawCommandBuildRequestArray;

struct FDrawCommandRelevancePacket
{
	FDrawCommandRelevancePacket()
	{
		bUseCachedMeshDrawCommands = UseCachedMeshDrawCommands();

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; ++PassIndex)
		{
			NumDynamicBuildRequestElements[PassIndex] = 0;
		}
	}

	FPassDrawCommandArray VisibleCachedDrawCommands[EMeshPass::Num];
	FPassDrawCommandBuildRequestArray DynamicBuildRequests[EMeshPass::Num];
	int32 NumDynamicBuildRequestElements[EMeshPass::Num];
	bool bUseCachedMeshDrawCommands;

	void AddCommandsForMesh(
		int32 PrimitiveIndex, 
		const FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FStaticMeshBatchRelevance& RESTRICT StaticMeshRelevance, 
		const FStaticMeshBatch& RESTRICT StaticMesh, 
		const FScene* RESTRICT Scene, 
		bool bCanCache, 
		EMeshPass::Type PassType)
	{
		const EShadingPath ShadingPath = Scene->GetShadingPath();
		const bool bUseCachedMeshCommand = bUseCachedMeshDrawCommands
			&& !!(FPassProcessorManager::GetPassFlags(ShadingPath, PassType) & EMeshPassFlags::CachedMeshCommands)
			&& StaticMeshRelevance.bSupportsCachingMeshDrawCommands
			&& bCanCache;

		if (bUseCachedMeshCommand)
		{
			const int32 StaticMeshCommandInfoIndex = StaticMeshRelevance.GetStaticMeshCommandInfoIndex(PassType);
			if (StaticMeshCommandInfoIndex >= 0)
			{
				const FCachedMeshDrawCommandInfo& CachedMeshDrawCommand = InPrimitiveSceneInfo->StaticMeshCommandInfos[StaticMeshCommandInfoIndex];
				const FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[PassType];

				// AddUninitialized_GetRef()
				VisibleCachedDrawCommands[(uint32)PassType].AddUninitialized();
				FVisibleMeshDrawCommand& NewVisibleMeshDrawCommand = VisibleCachedDrawCommands[(uint32)PassType].Last();

				const FMeshDrawCommand* MeshDrawCommand = CachedMeshDrawCommand.StateBucketId >= 0
					? &Scene->CachedMeshDrawCommandStateBuckets[PassType].GetByElementId(CachedMeshDrawCommand.StateBucketId).Key
					: &SceneDrawList.MeshDrawCommands[CachedMeshDrawCommand.CommandIndex];

				NewVisibleMeshDrawCommand.Setup(
					MeshDrawCommand,
					FMeshDrawCommandPrimitiveIdInfo(PrimitiveIndex, InPrimitiveSceneInfo->GetInstanceSceneDataOffset()),
					CachedMeshDrawCommand.StateBucketId,
					CachedMeshDrawCommand.MeshFillMode,
					CachedMeshDrawCommand.MeshCullMode,
					CachedMeshDrawCommand.Flags,
					CachedMeshDrawCommand.SortKey);
			}
		}
		else
		{
			NumDynamicBuildRequestElements[PassType] += StaticMeshRelevance.NumElements;
			DynamicBuildRequests[PassType].Add(&StaticMesh);
		}
	}
};

struct FRelevancePacket : public FSceneRenderingAllocatorObject<FRelevancePacket>
{
	const float CurrentWorldTime;
	const float DeltaWorldTime;

	FRHICommandListImmediate& RHICmdList;
	const FScene* Scene;
	const FViewInfo& View;
	const FViewCommands& ViewCommands;
	const uint8 ViewBit;
	const FMarkRelevantStaticMeshesForViewData& ViewData;
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks;
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks;
	uint8* RESTRICT MarkMasks;

	FRelevancePrimSet<int32> Input;
	FRelevancePrimSet<int32> RelevantStaticPrimitives;
	FRelevancePrimSet<int32> NotDrawRelevant;
	FRelevancePrimSet<int32> TranslucentSelfShadowPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> VisibleDynamicPrimitivesWithSimpleLights;
	int32 NumVisibleDynamicPrimitives;
	int32 NumVisibleDynamicEditorPrimitives;
	FMeshPassMask VisibleDynamicMeshesPassMask;
	FTranslucenyPrimCount TranslucentPrimCount;
	bool bHasDistortionPrimitives;
	bool bHasCustomDepthPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> LazyUpdatePrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> DirtyIndirectLightingCacheBufferPrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> RecachedReflectionCapturePrimitives;
#if WITH_EDITOR
	FRelevancePrimSet<FPrimitiveSceneInfo*> EditorVisualizeLevelInstancePrimitives;
	FRelevancePrimSet<FPrimitiveSceneInfo*> EditorSelectedPrimitives;
#endif

	TArray<FMeshDecalBatch> MeshDecalBatches;
	TArray<FVolumetricMeshBatch> VolumetricMeshBatches;
	TArray<FVolumetricMeshBatch> HeterogeneousVolumesMeshBatches;
	TArray<FSkyMeshBatch> SkyMeshBatches;
	TArray<FSortedTrianglesMeshBatch> SortedTrianglesMeshBatches;
	FDrawCommandRelevancePacket DrawCommandPacket;
	TSet<uint32> CustomDepthStencilValues;
	FRelevancePrimSet<FPrimitiveInstanceRange> NaniteCustomDepthInstances;

	struct FPrimitiveLODMask
	{
		FPrimitiveLODMask()
			: PrimitiveIndex(INDEX_NONE)
		{}

		FPrimitiveLODMask(const int32 InPrimitiveIndex, const FLODMask& InLODMask)
			: PrimitiveIndex(InPrimitiveIndex)
			, LODMask(InLODMask)
		{}

		int32 PrimitiveIndex;
		FLODMask LODMask;
	};

	FRelevancePrimSet<FPrimitiveLODMask> PrimitivesLODMask; // group both lod mask with primitive index to be able to properly merge them in the view

	uint16 CombinedShadingModelMask;
	uint8 StrataUintPerPixel;
	uint8 StrataBSDFCountMask;
	bool bUsesGlobalDistanceField;
	bool bUsesLightingChannels;
	bool bTranslucentSurfaceLighting;
	bool bUsesSceneDepth;
	bool bUsesCustomDepth;
	bool bUsesCustomStencil;
	bool bSceneHasSkyMaterial;
	bool bHasSingleLayerWaterMaterial;
	bool bHasTranslucencySeparateModulation;
	bool bHasStandardTranslucencyModulation;

	FRelevancePacket(
		FRHICommandListImmediate& InRHICmdList,
		const FScene* InScene, 
		const FViewInfo& InView, 
		const FViewCommands& InViewCommands,
		uint8 InViewBit,
		const FMarkRelevantStaticMeshesForViewData& InViewData,
		FPrimitiveViewMasks& InOutHasDynamicMeshElementsMasks,
		FPrimitiveViewMasks& InOutHasDynamicEditorMeshElementsMasks,
		uint8* InMarkMasks)

		: CurrentWorldTime(InView.Family->Time.GetWorldTimeSeconds())
		, DeltaWorldTime(InView.Family->Time.GetDeltaWorldTimeSeconds())
		, RHICmdList(InRHICmdList)
		, Scene(InScene)
		, View(InView)
		, ViewCommands(InViewCommands)
		, ViewBit(InViewBit)
		, ViewData(InViewData)
		, OutHasDynamicMeshElementsMasks(InOutHasDynamicMeshElementsMasks)
		, OutHasDynamicEditorMeshElementsMasks(InOutHasDynamicEditorMeshElementsMasks)
		, MarkMasks(InMarkMasks)
		, NumVisibleDynamicPrimitives(0)
		, NumVisibleDynamicEditorPrimitives(0)
		, bHasDistortionPrimitives(false)
		, bHasCustomDepthPrimitives(false)
		, CombinedShadingModelMask(0)
		, StrataUintPerPixel(0)
		, StrataBSDFCountMask(0)
		, bUsesGlobalDistanceField(false)
		, bUsesLightingChannels(false)
		, bTranslucentSurfaceLighting(false)
		, bUsesSceneDepth(false)
		, bUsesCustomDepth(false)
		, bUsesCustomStencil(false)
		, bSceneHasSkyMaterial(false)
		, bHasSingleLayerWaterMaterial(false)
		, bHasTranslucencySeparateModulation(false)
		, bHasStandardTranslucencyModulation(false)
	{
	}

	void AnyThreadTask()
	{
		FOptionalTaskTagScope Scope(ETaskTag::EParallelRenderingThread);
		ComputeRelevance();
		MarkRelevant();
	}

	void ComputeRelevance()
	{
		CombinedShadingModelMask = 0;
		StrataUintPerPixel = 0;
		StrataBSDFCountMask = 0;
		bSceneHasSkyMaterial = 0;
		bHasSingleLayerWaterMaterial = 0;
		bHasTranslucencySeparateModulation = 0;
		bHasStandardTranslucencyModulation = 0;
		bUsesGlobalDistanceField = false;
		bUsesLightingChannels = false;
		bTranslucentSurfaceLighting = false;
		const EShadingPath ShadingPath = Scene->GetShadingPath();
		const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();

		SCOPE_CYCLE_COUNTER(STAT_ComputeViewRelevance);
		for (int32 Index = 0; Index < Input.NumPrims; Index++)
		{
			int32 BitIndex = Input.Prims[Index];
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[BitIndex];
			FPrimitiveViewRelevance& ViewRelevance = const_cast<FPrimitiveViewRelevance&>(View.PrimitiveViewRelevanceMap[BitIndex]);
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(&View);
			ViewRelevance.bInitializedThisFrame = true;

			const bool bStaticRelevance = ViewRelevance.bStaticRelevance;
			const bool bDrawRelevance = ViewRelevance.bDrawRelevance;
			const bool bDynamicRelevance = ViewRelevance.bDynamicRelevance;
			const bool bShadowRelevance = ViewRelevance.bShadowRelevance;
			const bool bEditorRelevance = ViewRelevance.bEditorPrimitiveRelevance;
			const bool bEditorVisualizeLevelInstanceRelevance = ViewRelevance.bEditorVisualizeLevelInstanceRelevance;
			const bool bEditorSelectionRelevance = ViewRelevance.bEditorStaticSelectionRelevance;
			const bool bTranslucentRelevance = ViewRelevance.HasTranslucency();

			const bool bHairStrandsEnabled = ViewRelevance.bHairStrands && IsHairStrandsEnabled(EHairStrandsShaderType::All, Scene->GetShaderPlatform());

			if (View.bIsReflectionCapture && !PrimitiveSceneInfo->Proxy->IsVisibleInReflectionCaptures())
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

			if (bStaticRelevance && (bDrawRelevance || bShadowRelevance))
			{
				RelevantStaticPrimitives.AddPrim(BitIndex);
			}

			if (!bDrawRelevance)
			{
				NotDrawRelevant.AddPrim(BitIndex);
				continue;
			}

		#if WITH_EDITOR
			if (bEditorVisualizeLevelInstanceRelevance)
			{
				EditorVisualizeLevelInstancePrimitives.AddPrim(PrimitiveSceneInfo);
			}

			if (bEditorSelectionRelevance)
			{
				EditorSelectedPrimitives.AddPrim(PrimitiveSceneInfo);
			}
		#endif

			if (bEditorRelevance)
			{
				++NumVisibleDynamicEditorPrimitives;

				if (GIsEditor)
				{
					OutHasDynamicEditorMeshElementsMasks[BitIndex] |= ViewBit;
				}
			}
			else if(bDynamicRelevance)
			{
				// Keep track of visible dynamic primitives.
				++NumVisibleDynamicPrimitives;
				OutHasDynamicMeshElementsMasks[BitIndex] |= ViewBit;

				if (ViewRelevance.bHasSimpleLights)
				{
					VisibleDynamicPrimitivesWithSimpleLights.AddPrim(PrimitiveSceneInfo);
				}
			}
			else if (bHairStrandsEnabled)
			{
				// Strands MeshElement
				++NumVisibleDynamicPrimitives;
				OutHasDynamicMeshElementsMasks[BitIndex] |= ViewBit;
			}

			if (bTranslucentRelevance && !bEditorRelevance && ViewRelevance.bRenderInMainPass)
			{
				if (View.Family->AllowTranslucencyAfterDOF())
				{
					if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)))
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyStandard, ViewRelevance.bUsesSceneColorCopy);
					}

					if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)) && ViewRelevance.bTranslucencyModulate && View.Family->AllowStandardTranslucencySeparated())
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyStandardModulate, ViewRelevance.bUsesSceneColorCopy);
					}

					if (ViewRelevance.bSeparateTranslucency)
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyAfterDOF, ViewRelevance.bUsesSceneColorCopy);
					}

					if (ViewRelevance.bSeparateTranslucency && ViewRelevance.bTranslucencyModulate)
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyAfterDOFModulate, ViewRelevance.bUsesSceneColorCopy);
					}

					if (ViewRelevance.bPostMotionBlurTranslucency)
					{
						TranslucentPrimCount.Add(ETranslucencyPass::TPT_TranslucencyAfterMotionBlur, ViewRelevance.bUsesSceneColorCopy);
					}
				}
				else // Otherwise, everything is rendered in a single bucket. This is not related to whether DOF is currently enabled or not.
				{
					// When using all translucency, Standard and AfterDOF are sorted together instead of being rendered like 2 buckets.
					TranslucentPrimCount.Add(ETranslucencyPass::TPT_AllTranslucency, ViewRelevance.bUsesSceneColorCopy);
				}

				if (ViewRelevance.bDistortion)
				{
					bHasDistortionPrimitives = true;
				}
			}
			
			CombinedShadingModelMask |= ViewRelevance.ShadingModelMask;
			StrataUintPerPixel = FMath::Max(StrataUintPerPixel, ViewRelevance.StrataUintPerPixel);
			StrataBSDFCountMask |= ViewRelevance.StrataBSDFCountMask;
			bUsesGlobalDistanceField |= ViewRelevance.bUsesGlobalDistanceField;
			bUsesLightingChannels |= ViewRelevance.bUsesLightingChannels;
			bTranslucentSurfaceLighting |= ViewRelevance.bTranslucentSurfaceLighting;
			bUsesSceneDepth |= ViewRelevance.bUsesSceneDepth;
			bUsesCustomDepth |= (ViewRelevance.CustomDepthStencilUsageMask & 1) > 0;
			bUsesCustomStencil |= (ViewRelevance.CustomDepthStencilUsageMask & (1 << 1)) > 0;
			bSceneHasSkyMaterial |= ViewRelevance.bUsesSkyMaterial;
			bHasSingleLayerWaterMaterial |= ViewRelevance.bUsesSingleLayerWaterMaterial;
			bHasStandardTranslucencyModulation |= ViewRelevance.bNormalTranslucency && ViewRelevance.bTranslucencyModulate && View.Family->AllowStandardTranslucencySeparated();
			bHasTranslucencySeparateModulation |= ViewRelevance.bSeparateTranslucency && ViewRelevance.bTranslucencyModulate;

			if (ViewRelevance.bRenderCustomDepth)
			{
				bHasCustomDepthPrimitives = true;
				CustomDepthStencilValues.Add(PrimitiveSceneInfo->Proxy->GetCustomDepthStencilValue());

				if (PrimitiveSceneInfo->Proxy->IsNaniteMesh())
				{
					check(PrimitiveSceneInfo->IsIndexValid());
					NaniteCustomDepthInstances.AddPrim(
						FPrimitiveInstanceRange {
							PrimitiveSceneInfo->GetIndex(),
							PrimitiveSceneInfo->GetInstanceSceneDataOffset(),
							PrimitiveSceneInfo->GetNumInstanceSceneDataEntries()
						}
					);
				}
			}

			extern bool GUseTranslucencyShadowDepths;
			if (GUseTranslucencyShadowDepths && ViewRelevance.bTranslucentSelfShadow)
			{
				TranslucentSelfShadowPrimitives.AddPrim(BitIndex);
			}

			// INITVIEWS_TODO: Do this in a separate pass? There are no dependencies
			// here except maybe ParentPrimitives. This could be done in a 
			// low-priority background task and forgotten about.

			PrimitiveSceneInfo->LastRenderTime = CurrentWorldTime;

			// If the primitive is definitely unoccluded or if in Wireframe mode and the primitive is estimated
			// to be unoccluded, then update the primitive components's LastRenderTime 
			// on the game thread. This signals that the primitive is visible.
			if (View.PrimitiveDefinitelyUnoccludedMap[BitIndex] || (View.Family->EngineShowFlags.Wireframe && View.PrimitiveVisibilityMap[BitIndex]))
			{
				PrimitiveSceneInfo->UpdateComponentLastRenderTime(CurrentWorldTime, /*bUpdateLastRenderTimeOnScreen=*/true);
			}

			// Cache the nearest reflection proxy if needed
			if (PrimitiveSceneInfo->NeedsReflectionCaptureUpdate())
			{
				// mobile should not have any outstanding reflection capture update requests at this point, except for when lighting isn't rebuilt		
				PrimitiveSceneInfo->CacheReflectionCaptures();

				// With forward shading we need to track reflection capture cache updates
				// in order to update primitive's uniform buffer's closest reflection capture id.
				if (IsForwardShadingEnabled(Scene->GetShaderPlatform()))
				{
					RecachedReflectionCapturePrimitives.AddPrim(PrimitiveSceneInfo);
				}
			}

			if (PrimitiveSceneInfo->NeedsUniformBufferUpdate())
			{
				LazyUpdatePrimitives.AddPrim(PrimitiveSceneInfo);
			}
			if (PrimitiveSceneInfo->NeedsIndirectLightingCacheBufferUpdate())
			{
				DirtyIndirectLightingCacheBufferPrimitives.AddPrim(PrimitiveSceneInfo);
			}
		}
	}

	void MarkRelevant()
	{
		SCOPE_CYCLE_COUNTER(STAT_StaticRelevance);

		// using a local counter to reduce memory traffic
		int32 NumVisibleStaticMeshElements = 0;
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		const FSceneViewState* ViewState = (FSceneViewState*)View.State;
		const EShadingPath ShadingPath = Scene->GetShadingPath();
		const bool bMobileMaskedInEarlyPass = (ShadingPath == EShadingPath::Mobile) &&  Scene->EarlyZPassMode == DDM_MaskedOnly;
		const bool bMobileBasePassAlwaysUsesCSM = (ShadingPath == EShadingPath::Mobile) && MobileBasePassAlwaysUsesCSM(Scene->GetShaderPlatform());
		const bool bVelocityPassWritesDepth = Scene->EarlyZPassMode == DDM_AllOpaqueNoVelocity;
		const bool bHLODActive = Scene->SceneLODHierarchy.IsActive();
		const FHLODVisibilityState* const HLODState = bHLODActive && ViewState ? &ViewState->HLODVisibilityState : nullptr;
		float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
		MaxDrawDistanceScale *= GetCachedScalabilityCVars().CalculateFieldOfViewDistanceScale(View.DesiredFOV);

		
		for (int32 StaticPrimIndex = 0, Num = RelevantStaticPrimitives.NumPrims; StaticPrimIndex < Num; ++StaticPrimIndex)
		{
			int32 PrimitiveIndex = RelevantStaticPrimitives.Prims[StaticPrimIndex];
			const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];
			const FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[PrimitiveIndex];
			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];
			const bool bIsPrimitiveDistanceCullFading = View.PrimitiveFadeUniformBufferMap[PrimitiveIndex];

			const int8 CurFirstLODIdx = PrimitiveSceneInfo->Proxy->GetCurrentFirstLODIdx_RenderThread();
			check(CurFirstLODIdx >= 0);
			float MeshScreenSizeSquared = 0;
			FLODMask LODToRender = ComputeLODForMeshes(PrimitiveSceneInfo->StaticMeshRelevances, View, Bounds.BoxSphereBounds.Origin, Bounds.BoxSphereBounds.SphereRadius, ViewData.ForcedLODLevel, MeshScreenSizeSquared, CurFirstLODIdx, ViewData.LODScale);

			PrimitivesLODMask.AddPrim(FRelevancePacket::FPrimitiveLODMask(PrimitiveIndex, LODToRender));

			const bool bIsHLODFading = HLODState ? HLODState->IsNodeFading(PrimitiveIndex) : false;
			const bool bIsHLODFadingOut = HLODState ? HLODState->IsNodeFadingOut(PrimitiveIndex) : false;
			const bool bIsLODDithered = LODToRender.IsDithered();

			float DistanceSquared = (Bounds.BoxSphereBounds.Origin - ViewData.ViewOrigin).SizeSquared();
			const float LODFactorDistanceSquared = DistanceSquared * FMath::Square(ViewData.LODScale);
			const bool bDrawShadowDepth = FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > ViewData.MinScreenRadiusForCSMDepthSquared * LODFactorDistanceSquared;
			const bool bDrawDepthOnly = ViewData.bFullEarlyZPass || ((ShadingPath != EShadingPath::Mobile) && (FMath::Square(Bounds.BoxSphereBounds.SphereRadius) > GMinScreenRadiusForDepthPrepass * GMinScreenRadiusForDepthPrepass * LODFactorDistanceSquared));

			const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();

			const int32 NumStaticMeshes = PrimitiveSceneInfo->StaticMeshRelevances.Num();
			for(int32 MeshIndex = 0;MeshIndex < NumStaticMeshes;MeshIndex++)
			{
				const FStaticMeshBatchRelevance& StaticMeshRelevance = PrimitiveSceneInfo->StaticMeshRelevances[MeshIndex];
				const FStaticMeshBatch& StaticMesh = PrimitiveSceneInfo->StaticMeshes[MeshIndex];

				if (StaticMesh.bOverlayMaterial && !View.Family->EngineShowFlags.DistanceCulledPrimitives)
				{
					// Overlay mesh can have its own cull distance that is shorter than primitive cull distance
					float OverlayMaterialMaxDrawDistance = StaticMeshRelevance.ScreenSize;
					if (OverlayMaterialMaxDrawDistance > 1.f && OverlayMaterialMaxDrawDistance != FLT_MAX)
					{
						if (DistanceSquared > FMath::Square(OverlayMaterialMaxDrawDistance * MaxDrawDistanceScale))
						{
							// distance culled
							continue;
						}
					}
				}

				if (LODToRender.ContainsLOD(StaticMeshRelevance.LODIndex))
				{
					uint8 MarkMask = 0;
					bool bHiddenByHLODFade = false; // Hide mesh LOD levels that HLOD is substituting

					if (bIsHLODFading)
					{
						if (bIsHLODFadingOut)
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[1] == StaticMeshRelevance.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;	
							}
						}
						else
						{
							if (bIsLODDithered && LODToRender.DitheredLODIndices[0] == StaticMeshRelevance.LODIndex)
							{
								bHiddenByHLODFade = true;
							}
							else
							{
								MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
							}
						}
					}
					else if (bIsLODDithered)
					{
						if (LODToRender.DitheredLODIndices[0] == StaticMeshRelevance.LODIndex)
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask;
						}
						else
						{
							MarkMask |= EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask;
						}
					}

					// Don't cache if it requires per view per mesh state for LOD dithering or distance cull fade.
					const bool bIsMeshDitheringLOD = StaticMeshRelevance.bDitheredLODTransition && (MarkMask & (EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask | EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask));
					const bool bCanCache = !bIsPrimitiveDistanceCullFading && !bIsMeshDitheringLOD;
					
					if (ViewRelevance.bDrawRelevance)
					{
						if ((StaticMeshRelevance.bUseForMaterial || StaticMeshRelevance.bUseAsOccluder)
							&& (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth || ViewRelevance.bRenderInDepthPass)
							&& !bHiddenByHLODFade)
						{
							// Add velocity commands first to track for case where velocity pass writes depth.
							bool bIsMeshInVelocityPass = false;
							if (StaticMeshRelevance.bUseForMaterial && ViewRelevance.bRenderInMainPass)
							{
								if (ViewRelevance.HasVelocity())
								{
									const FPrimitiveSceneProxy* PrimitiveSceneProxy = PrimitiveSceneInfo->Proxy;

									if (FVelocityMeshProcessor::PrimitiveHasVelocityForView(View, PrimitiveSceneProxy))
									{
										if (ViewRelevance.bVelocityRelevance &&
											FOpaqueVelocityMeshProcessor::PrimitiveCanHaveVelocity(View.GetShaderPlatform(), PrimitiveSceneProxy) &&
											FOpaqueVelocityMeshProcessor::PrimitiveHasVelocityForFrame(PrimitiveSceneProxy))
										{
											DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::Velocity);
											bIsMeshInVelocityPass = true;
										}

										if (ViewRelevance.bOutputsTranslucentVelocity &&
											FTranslucentVelocityMeshProcessor::PrimitiveCanHaveVelocity(View.GetShaderPlatform(), PrimitiveSceneProxy) &&
											FTranslucentVelocityMeshProcessor::PrimitiveHasVelocityForFrame(PrimitiveSceneProxy))
										{
											DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucentVelocity);
										}
									}
								}
							}

							// Add depth commands.
							if (StaticMeshRelevance.bUseForDepthPass && (bDrawDepthOnly || (bMobileMaskedInEarlyPass && ViewRelevance.bMasked)))
							{
								if (!(bIsMeshInVelocityPass && bVelocityPassWritesDepth))
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::DepthPass);
								}
#if RHI_RAYTRACING
								if (IsRayTracingEnabled())
								{
									if (MarkMask & EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::DitheredLODFadingOutMaskPass);
									}
								}
#endif
							}

							// Mark static mesh as visible for rendering
							if (StaticMeshRelevance.bUseForMaterial && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth))
							{
								// Specific logic for mobile packets
								if (ShadingPath == EShadingPath::Mobile)
								{
									// Skydome must not be added to base pass bucket
									if (!StaticMeshRelevance.bUseSkyMaterial)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::BasePass);
										if (!bMobileBasePassAlwaysUsesCSM)
										{
											DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::MobileBasePassCSM);
										}
									}
									else
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::SkyPass);
									}
									// bUseSingleLayerWaterMaterial is added to BasePass on Mobile. No need to add it to SingleLayerWaterPass

									MarkMask |= EMarkMaskBits::StaticMeshVisibilityMapMask;
								}
								else // Regular shading path
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::BasePass);
									MarkMask |= EMarkMaskBits::StaticMeshVisibilityMapMask;

									if (StaticMeshRelevance.bUseSkyMaterial)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::SkyPass);
									}
									if (StaticMeshRelevance.bUseSingleLayerWaterMaterial)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::SingleLayerWaterPass);
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::SingleLayerWaterDepthPrepass);
									}
								}

								if (StaticMeshRelevance.bUseAnisotropy)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::AnisotropyPass);
								}

								if (ViewRelevance.bRenderCustomDepth)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::CustomDepth);
								}

								if (bAddLightmapDensityCommands)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::LightmapDensity);
								}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
								else if (View.Family->UseDebugViewPS())
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::DebugViewMode);
								}
#endif

#if WITH_EDITOR
								if (StaticMeshRelevance.bSelectable)
								{
									if (View.bAllowTranslucentPrimitivesInHitProxy)
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::HitProxy);
									}
									else
									{
										DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::HitProxyOpaqueOnly);
									}
								}
#endif
								++NumVisibleStaticMeshElements;

								INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, StaticMesh.GetNumPrimitives());
							}
						}

						if (StaticMeshRelevance.bUseForMaterial
							&& ViewRelevance.HasTranslucency()
							&& !ViewRelevance.bEditorPrimitiveRelevance
							&& ViewRelevance.bRenderInMainPass)
						{
							if (View.Family->AllowTranslucencyAfterDOF())
							{
								if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)))
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyStandard);
								}

								if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)) && ViewRelevance.bTranslucencyModulate && View.Family->AllowStandardTranslucencySeparated())
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyStandardModulate);
								}

								if (ViewRelevance.bSeparateTranslucency)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAfterDOF);
								}

								if (ViewRelevance.bSeparateTranslucency && ViewRelevance.bTranslucencyModulate)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAfterDOFModulate);
								}

								if (ViewRelevance.bPostMotionBlurTranslucency)
								{
									DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAfterMotionBlur);
								}
							}
							else
							{
								// Otherwise, everything is rendered in a single bucket. This is not related to whether DOF is currently enabled or not.
								// When using all translucency, Standard and AfterDOF are sorted together instead of being rendered like 2 buckets.
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::TranslucencyAll);
							}

							if (ViewRelevance.bTranslucentSurfaceLighting)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::LumenTranslucencyRadianceCacheMark);
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::LumenFrontLayerTranslucencyGBuffer);
							}

							if (ViewRelevance.bDistortion)
							{
								DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::Distortion);
							}
						}

#if WITH_EDITOR
						if (ViewRelevance.bEditorVisualizeLevelInstanceRelevance)
						{
							DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::EditorLevelInstance);
						}

						if (ViewRelevance.bEditorStaticSelectionRelevance)
						{
							DrawCommandPacket.AddCommandsForMesh(PrimitiveIndex, PrimitiveSceneInfo, StaticMeshRelevance, StaticMesh, Scene, bCanCache, EMeshPass::EditorSelection);
						}
#endif

						if (ViewRelevance.bHasVolumeMaterialDomain)
						{
							if (ShouldRenderMeshBatchWithHeterogeneousVolumes(&StaticMesh, PrimitiveSceneInfo->Proxy, View.FeatureLevel))
							{
								HeterogeneousVolumesMeshBatches.AddUninitialized(1);
								FVolumetricMeshBatch& BatchAndProxy = HeterogeneousVolumesMeshBatches.Last();
								BatchAndProxy.Mesh = &StaticMesh;
								BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
							}
							else
							{
								VolumetricMeshBatches.AddUninitialized(1);
								FVolumetricMeshBatch& BatchAndProxy = VolumetricMeshBatches.Last();
								BatchAndProxy.Mesh = &StaticMesh;
								BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
							}
						}

						if (ViewRelevance.bUsesSkyMaterial)
						{
							SkyMeshBatches.AddUninitialized(1);
							FSkyMeshBatch& BatchAndProxy = SkyMeshBatches.Last();
							BatchAndProxy.Mesh = &StaticMesh;
							BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
							BatchAndProxy.bVisibleInMainPass = ViewRelevance.bRenderInMainPass;
							BatchAndProxy.bVisibleInRealTimeSkyCapture = PrimitiveSceneInfo->bVisibleInRealTimeSkyCapture;
						}

						if (ViewRelevance.HasTranslucency() && PrimitiveSceneInfo->Proxy->SupportsSortedTriangles()) // Need to check material as well
						{
							SortedTrianglesMeshBatches.AddUninitialized(1);
							FSortedTrianglesMeshBatch& BatchAndProxy = SortedTrianglesMeshBatches.Last();
							BatchAndProxy.Mesh = &StaticMesh;
							BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
						}

						// FIXME: Now if a primitive has one batch with a decal material all primitive mesh batches will be added as decals
						// Because ViewRelevance is a sum of all material relevances in the primitive
						if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDecal && StaticMeshRelevance.bUseForMaterial)
						{
							MeshDecalBatches.AddUninitialized(1);
							FMeshDecalBatch& BatchAndProxy = MeshDecalBatches.Last();
							BatchAndProxy.Mesh = &StaticMesh;
							BatchAndProxy.Proxy = PrimitiveSceneInfo->Proxy;
							BatchAndProxy.SortKey = PrimitiveSceneInfo->Proxy->GetTranslucencySortPriority();
						}
					}

					if (MarkMask)
					{
						MarkMasks[StaticMeshRelevance.Id] = MarkMask;
					}
				}
			}
		}
		static_assert(sizeof(WriteView.NumVisibleStaticMeshElements) == sizeof(int32), "Atomic is the wrong size");
		FPlatformAtomics::InterlockedAdd((volatile int32*)&WriteView.NumVisibleStaticMeshElements, NumVisibleStaticMeshElements);
	}

	void RenderThreadFinalize()
	{
		FViewInfo& WriteView = const_cast<FViewInfo&>(View);
		FViewCommands& WriteViewCommands = const_cast<FViewCommands&>(ViewCommands);
		
		for (int32 Index = 0; Index < NotDrawRelevant.NumPrims; Index++)
		{
			WriteView.PrimitiveVisibilityMap[NotDrawRelevant.Prims[Index]] = false;
		}

#if WITH_EDITOR
		auto AddRelevantHitProxiesToArray = [](FRelevancePrimSet<FPrimitiveSceneInfo*>& PrimSet, TArray<uint32>& OutHitProxyArray)
		{
			int32 TotalHitProxiesToAdd = 0;
			for (int32 Idx = 0; Idx < PrimSet.NumPrims; ++Idx)
			{
				if (PrimSet.Prims[Idx]->NaniteHitProxyIds.Num())
				{
					TotalHitProxiesToAdd += PrimSet.Prims[Idx]->NaniteHitProxyIds.Num();
				}
			}

			OutHitProxyArray.Reserve(OutHitProxyArray.Num() + TotalHitProxiesToAdd);

			for (int32 Idx = 0; Idx < PrimSet.NumPrims; ++Idx)
			{
				if (PrimSet.Prims[Idx]->NaniteHitProxyIds.Num())
				{
					for (uint32 IdValue : PrimSet.Prims[Idx]->NaniteHitProxyIds)
					{
						OutHitProxyArray.Add(IdValue);
					}
				}
			}
		};

		// Add hit proxies from editing LevelInstance Nanite primitives
		AddRelevantHitProxiesToArray(EditorVisualizeLevelInstancePrimitives, WriteView.EditorVisualizeLevelInstanceIds);

		// Add hit proxies from selected Nanite primitives.
		AddRelevantHitProxiesToArray(EditorSelectedPrimitives, WriteView.EditorSelectedHitProxyIds);
#endif

		WriteView.ShadingModelMaskInView |= CombinedShadingModelMask;
		WriteView.bUsesGlobalDistanceField |= bUsesGlobalDistanceField;
		WriteView.bUsesLightingChannels |= bUsesLightingChannels;
		WriteView.bTranslucentSurfaceLighting |= bTranslucentSurfaceLighting;
		WriteView.bUsesSceneDepth |= bUsesSceneDepth;
		WriteView.bSceneHasSkyMaterial |= bSceneHasSkyMaterial;
		WriteView.bHasSingleLayerWaterMaterial |= bHasSingleLayerWaterMaterial;
		WriteView.bHasTranslucencySeparateModulation |= bHasTranslucencySeparateModulation;
		WriteView.bHasStandardTranslucencyModulation |= bHasStandardTranslucencyModulation;
		VisibleDynamicPrimitivesWithSimpleLights.AppendTo(WriteView.VisibleDynamicPrimitivesWithSimpleLights);
		WriteView.NumVisibleDynamicPrimitives += NumVisibleDynamicPrimitives;
		WriteView.NumVisibleDynamicEditorPrimitives += NumVisibleDynamicEditorPrimitives;
		WriteView.TranslucentPrimCount.Append(TranslucentPrimCount);
		WriteView.bHasDistortionPrimitives |= bHasDistortionPrimitives;
		WriteView.bHasCustomDepthPrimitives |= bHasCustomDepthPrimitives;
		WriteView.CustomDepthStencilValues.Append(CustomDepthStencilValues);
		NaniteCustomDepthInstances.AppendTo(WriteView.NaniteCustomDepthInstances);
		WriteView.bUsesCustomDepth |= bUsesCustomDepth;
		WriteView.bUsesCustomStencil |= bUsesCustomStencil;
		WriteView.StrataViewData.MaxBSDFCount = FMath::Max(WriteView.StrataViewData.MaxBSDFCount, 8u - FMath::CountLeadingZeros8(StrataBSDFCountMask));
		WriteView.StrataViewData.MaxBytePerPixel = FMath::Max(WriteView.StrataViewData.MaxBytePerPixel, StrataUintPerPixel * 4u);
		DirtyIndirectLightingCacheBufferPrimitives.AppendTo(WriteView.DirtyIndirectLightingCacheBufferPrimitives);

		WriteView.MeshDecalBatches.Append(MeshDecalBatches);
		WriteView.VolumetricMeshBatches.Append(VolumetricMeshBatches);
		WriteView.HeterogeneousVolumesMeshBatches.Append(HeterogeneousVolumesMeshBatches);
		WriteView.SkyMeshBatches.Append(SkyMeshBatches);
		WriteView.SortedTrianglesMeshBatches.Append(SortedTrianglesMeshBatches);

		for (int32 Index = 0; Index < RecachedReflectionCapturePrimitives.NumPrims; ++Index)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = RecachedReflectionCapturePrimitives.Prims[Index];

			PrimitiveSceneInfo->MarkGPUStateDirty(EPrimitiveDirtyState::ChangedAll);
			PrimitiveSceneInfo->ConditionalUpdateUniformBuffer(RHICmdList);
		}

		for (int32 Index = 0; Index < LazyUpdatePrimitives.NumPrims; Index++)
		{
			LazyUpdatePrimitives.Prims[Index]->ConditionalUpdateUniformBuffer(RHICmdList);
		}

		for (int32 i = 0; i < PrimitivesLODMask.NumPrims; ++i)
		{
			WriteView.PrimitivesLODMask[PrimitivesLODMask.Prims[i].PrimitiveIndex] = PrimitivesLODMask.Prims[i].LODMask;
		}

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
		{
			FPassDrawCommandArray& SrcCommands = DrawCommandPacket.VisibleCachedDrawCommands[PassIndex];
			FMeshCommandOneFrameArray& DstCommands = WriteViewCommands.MeshCommands[PassIndex];
			if (SrcCommands.Num() > 0)
			{
				static_assert(sizeof(SrcCommands[0]) == sizeof(DstCommands[0]), "Memcpy sizes must match.");
				const int32 PrevNum = DstCommands.AddUninitialized(SrcCommands.Num());
				FMemory::Memcpy(&DstCommands[PrevNum], &SrcCommands[0], SrcCommands.Num() * sizeof(SrcCommands[0]));
			}

			FPassDrawCommandBuildRequestArray& SrcRequests = DrawCommandPacket.DynamicBuildRequests[PassIndex];
			TArray<const FStaticMeshBatch*, SceneRenderingAllocator>& DstRequests = WriteViewCommands.DynamicMeshCommandBuildRequests[PassIndex];
			if (SrcRequests.Num() > 0)
			{
				static_assert(sizeof(SrcRequests[0]) == sizeof(DstRequests[0]), "Memcpy sizes must match.");
				const int32 PrevNum = DstRequests.AddUninitialized(SrcRequests.Num());
				FMemory::Memcpy(&DstRequests[PrevNum], &SrcRequests[0], SrcRequests.Num() * sizeof(SrcRequests[0]));
			}

			WriteViewCommands.NumDynamicMeshCommandBuildRequestElements[PassIndex] += DrawCommandPacket.NumDynamicBuildRequestElements[PassIndex];
		}

		// Prepare translucent self shadow uniform buffers.
		for (int32 Index = 0; Index < TranslucentSelfShadowPrimitives.NumPrims; ++Index)
		{
			const int32 PrimitiveIndex = TranslucentSelfShadowPrimitives.Prims[Index];

			FUniformBufferRHIRef& UniformBuffer = WriteView.TranslucentSelfShadowUniformBufferMap.FindOrAdd(PrimitiveIndex);

			if (!UniformBuffer)
			{
				FTranslucentSelfShadowUniformParameters Parameters;
				SetupTranslucentSelfShadowUniformParameters(nullptr, Parameters);
				UniformBuffer = FTranslucentSelfShadowUniformParameters::CreateUniformBuffer(Parameters, EUniformBufferUsage::UniformBuffer_SingleFrame);
			}
		}
	}
};

static void ComputeAndMarkRelevanceForViewParallel(
	FRHICommandListImmediate& RHICmdList,
	const FScene* Scene,
	FViewInfo& View,
	FViewCommands& ViewCommands,
	uint8 ViewBit,
	FPrimitiveViewMasks& OutHasDynamicMeshElementsMasks,
	FPrimitiveViewMasks& OutHasDynamicEditorMeshElementsMasks
	)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSceneRenderer_ComputeAndMarkRelevanceForViewParallel);

	check(OutHasDynamicMeshElementsMasks.Num() == Scene->Primitives.Num());

	FFrozenSceneViewMatricesGuard FrozenMatricesGuard(View);
	const FMarkRelevantStaticMeshesForViewData ViewData(View);

	FConcurrentLinearBulkObjectAllocator Allocator;
	int32 NumMesh = View.StaticMeshVisibilityMap.Num();
	uint8* RESTRICT MarkMasks = (uint8*)Allocator.Malloc(NumMesh + 31, 8); // some padding to simplify the high speed transpose
	FMemory::Memzero(MarkMasks, NumMesh + 31);

	int32 EstimateOfNumPackets = NumMesh / (FRelevancePrimSet<int32>::MaxInputPrims * 4);

	TArray<FRelevancePacket*,SceneRenderingAllocator> Packets;
	Packets.Reserve(EstimateOfNumPackets);

	bool WillExecuteInParallel = FApp::ShouldUseThreadingForPerformance() && CVarParallelInitViews.GetValueOnRenderThread() > 0 && IsInActualRenderingThread();

	{
		FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap);
		if (BitIt)
		{

			FRelevancePacket* Packet = new FRelevancePacket(
				RHICmdList,
				Scene, 
				View, 
				ViewCommands,
				ViewBit,
				ViewData,
				OutHasDynamicMeshElementsMasks,
				OutHasDynamicEditorMeshElementsMasks,
				MarkMasks);
			Packets.Add(Packet);

			while (1)
			{
				Packet->Input.AddPrim(BitIt.GetIndex());
				++BitIt;
				if (Packet->Input.IsFull() || !BitIt)
				{
					if (!BitIt)
					{
						break;
					}
					else
					{
						Packet = new FRelevancePacket(
							RHICmdList,
							Scene, 
							View, 
							ViewCommands,
							ViewBit,
							ViewData,
							OutHasDynamicMeshElementsMasks,
							OutHasDynamicEditorMeshElementsMasks,
							MarkMasks);
						Packets.Add(Packet);
					}
				}
			}
		}
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_ParallelFor);
		ParallelFor(Packets.Num(), 
			[&Packets](int32 Index)
			{
				Packets[Index]->AnyThreadTask();
			},
			!WillExecuteInParallel
		);
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_RenderThreadFinalize);

		for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
		{
			int32 NumVisibleCachedMeshDrawCommands = 0;
			int32 NumDynamicBuildRequests = 0;

			for (auto Packet : Packets)
			{
				NumVisibleCachedMeshDrawCommands += Packet->DrawCommandPacket.VisibleCachedDrawCommands[PassIndex].Num();
				NumDynamicBuildRequests += Packet->DrawCommandPacket.DynamicBuildRequests[PassIndex].Num();
			}

			ViewCommands.MeshCommands[PassIndex].Reserve(NumVisibleCachedMeshDrawCommands);
			ViewCommands.DynamicMeshCommandBuildRequests[PassIndex].Reserve(NumDynamicBuildRequests);
		}

		for (auto Packet : Packets)
		{
			Packet->RenderThreadFinalize();
			delete Packet;
		}

		Packets.Empty();
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ComputeAndMarkRelevanceForViewParallel_TransposeMeshBits);
	check(View.StaticMeshVisibilityMap.Num() == NumMesh && 
		View.StaticMeshFadeOutDitheredLODMap.Num() == NumMesh && 
		View.StaticMeshFadeInDitheredLODMap.Num() == NumMesh
		);
	uint32* RESTRICT StaticMeshVisibilityMap_Words = View.StaticMeshVisibilityMap.GetData();
	uint32* RESTRICT StaticMeshFadeOutDitheredLODMap_Words = View.StaticMeshFadeOutDitheredLODMap.GetData();
	uint32* RESTRICT StaticMeshFadeInDitheredLODMap_Words = View.StaticMeshFadeInDitheredLODMap.GetData();
	const uint64* RESTRICT MarkMasks64 = (const uint64* RESTRICT)MarkMasks;
	const uint8* RESTRICT MarkMasks8 = MarkMasks;
	for (int32 BaseIndex = 0; BaseIndex < NumMesh; BaseIndex += 32)
	{
		uint32 StaticMeshVisibilityMap_Word = 0;
		uint32 StaticMeshFadeOutDitheredLODMap_Word = 0;
		uint32 StaticMeshFadeInDitheredLODMap_Word = 0;
		uint32 Mask = 1;
		bool bAny = false;
		for (int32 QWordIndex = 0; QWordIndex < 4; QWordIndex++)
		{
			if (*MarkMasks64++)
			{
				for (int32 ByteIndex = 0; ByteIndex < 8; ByteIndex++, Mask <<= 1, MarkMasks8++)
				{
					uint8 MaskMask = *MarkMasks8;
					StaticMeshVisibilityMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshVisibilityMapMask) ? Mask : 0;
					StaticMeshFadeOutDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeOutDitheredLODMapMask) ? Mask : 0;
					StaticMeshFadeInDitheredLODMap_Word |= (MaskMask & EMarkMaskBits::StaticMeshFadeInDitheredLODMapMask) ? Mask : 0;
				}
				bAny = true;
			}
			else
			{
				MarkMasks8 += 8;
				Mask <<= 8;
			}
		}
		if (bAny)
		{
			checkSlow(!*StaticMeshVisibilityMap_Words && !*StaticMeshFadeOutDitheredLODMap_Words && !*StaticMeshFadeInDitheredLODMap_Words);
			*StaticMeshVisibilityMap_Words = StaticMeshVisibilityMap_Word;
			*StaticMeshFadeOutDitheredLODMap_Words = StaticMeshFadeOutDitheredLODMap_Word;
			*StaticMeshFadeInDitheredLODMap_Words = StaticMeshFadeInDitheredLODMap_Word;
		}
		StaticMeshVisibilityMap_Words++;
		StaticMeshFadeOutDitheredLODMap_Words++;
		StaticMeshFadeInDitheredLODMap_Words++;
	}
}

void ComputeDynamicMeshRelevance(EShadingPath ShadingPath, bool bAddLightmapDensityCommands, const FPrimitiveViewRelevance& ViewRelevance, const FMeshBatchAndRelevance& MeshBatch, FViewInfo& View, FMeshPassMask& PassMask, FPrimitiveSceneInfo* PrimitiveSceneInfo, const FPrimitiveBounds& Bounds)
{
	const int32 NumElements = MeshBatch.Mesh->Elements.Num();

	if (ViewRelevance.bDrawRelevance && (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth || ViewRelevance.bRenderInDepthPass))
	{
		PassMask.Set(EMeshPass::DepthPass);
		View.NumVisibleDynamicMeshElements[EMeshPass::DepthPass] += NumElements;

		if (ViewRelevance.bRenderInMainPass || ViewRelevance.bRenderCustomDepth)
		{
			PassMask.Set(EMeshPass::BasePass);
			View.NumVisibleDynamicMeshElements[EMeshPass::BasePass] += NumElements;
			
			if (ViewRelevance.bUsesSkyMaterial)
			{
				PassMask.Set(EMeshPass::SkyPass);
				View.NumVisibleDynamicMeshElements[EMeshPass::SkyPass] += NumElements;
			}

			if (ViewRelevance.bUsesAnisotropy)
			{
				PassMask.Set(EMeshPass::AnisotropyPass);
				View.NumVisibleDynamicMeshElements[EMeshPass::AnisotropyPass] += NumElements;
			}

			if (ShadingPath == EShadingPath::Mobile)
			{
				PassMask.Set(EMeshPass::MobileBasePassCSM);
				View.NumVisibleDynamicMeshElements[EMeshPass::MobileBasePassCSM] += NumElements;
			}

			if (ViewRelevance.bRenderCustomDepth)
			{
				PassMask.Set(EMeshPass::CustomDepth);
				View.NumVisibleDynamicMeshElements[EMeshPass::CustomDepth] += NumElements;
			}

			if (bAddLightmapDensityCommands)
			{
				PassMask.Set(EMeshPass::LightmapDensity);
				View.NumVisibleDynamicMeshElements[EMeshPass::LightmapDensity] += NumElements;
			}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			else if (View.Family->UseDebugViewPS())
			{
				PassMask.Set(EMeshPass::DebugViewMode);
				View.NumVisibleDynamicMeshElements[EMeshPass::DebugViewMode] += NumElements;
			}
#endif

#if WITH_EDITOR
			if (View.bAllowTranslucentPrimitivesInHitProxy)
			{
				PassMask.Set(EMeshPass::HitProxy);
				View.NumVisibleDynamicMeshElements[EMeshPass::HitProxy] += NumElements;
			}
			else
			{
				PassMask.Set(EMeshPass::HitProxyOpaqueOnly);
				View.NumVisibleDynamicMeshElements[EMeshPass::HitProxyOpaqueOnly] += NumElements;
			}
#endif

			if (ViewRelevance.bVelocityRelevance)
			{
				PassMask.Set(EMeshPass::Velocity);
				View.NumVisibleDynamicMeshElements[EMeshPass::Velocity] += NumElements;
			}

			if (ViewRelevance.bOutputsTranslucentVelocity)
			{
				PassMask.Set(EMeshPass::TranslucentVelocity);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucentVelocity] += NumElements;
			}

			if (ViewRelevance.bUsesSingleLayerWaterMaterial)
			{
				PassMask.Set(EMeshPass::SingleLayerWaterPass);
				View.NumVisibleDynamicMeshElements[EMeshPass::SingleLayerWaterPass] += NumElements;
				PassMask.Set(EMeshPass::SingleLayerWaterDepthPrepass);
				View.NumVisibleDynamicMeshElements[EMeshPass::SingleLayerWaterDepthPrepass] += NumElements;
			}
		}
	}

	if (ViewRelevance.HasTranslucency()
		&& !ViewRelevance.bEditorPrimitiveRelevance
		&& ViewRelevance.bRenderInMainPass)
	{
		if (View.Family->AllowTranslucencyAfterDOF())
		{
			if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)))
			{
				PassMask.Set(EMeshPass::TranslucencyStandard);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyStandard] += NumElements;
			}

			if ((ViewRelevance.bNormalTranslucency || (View.AutoBeforeDOFTranslucencyBoundary > 0.0f && ViewRelevance.bSeparateTranslucency)) && ViewRelevance.bTranslucencyModulate && View.Family->AllowStandardTranslucencySeparated())
			{
				PassMask.Set(EMeshPass::TranslucencyStandardModulate);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyStandardModulate] += NumElements;
			}

			if (ViewRelevance.bSeparateTranslucency)
			{
				PassMask.Set(EMeshPass::TranslucencyAfterDOF);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAfterDOF] += NumElements;
			}

			if (ViewRelevance.bSeparateTranslucency && ViewRelevance.bTranslucencyModulate)
			{
				PassMask.Set(EMeshPass::TranslucencyAfterDOFModulate);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAfterDOFModulate] += NumElements;
			}

			if (ViewRelevance.bPostMotionBlurTranslucency)
			{
				PassMask.Set(EMeshPass::TranslucencyAfterMotionBlur);
				View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAfterMotionBlur] += NumElements;
			}
		}
		else
		{
			PassMask.Set(EMeshPass::TranslucencyAll);
			View.NumVisibleDynamicMeshElements[EMeshPass::TranslucencyAll] += NumElements;
		}

		if (ViewRelevance.bTranslucentSurfaceLighting)
		{
			PassMask.Set(EMeshPass::LumenTranslucencyRadianceCacheMark);
			View.NumVisibleDynamicMeshElements[EMeshPass::LumenTranslucencyRadianceCacheMark] += NumElements;

			PassMask.Set(EMeshPass::LumenFrontLayerTranslucencyGBuffer);
			View.NumVisibleDynamicMeshElements[EMeshPass::LumenFrontLayerTranslucencyGBuffer] += NumElements;
		}

		if (ViewRelevance.bDistortion)
		{
			PassMask.Set(EMeshPass::Distortion);
			View.NumVisibleDynamicMeshElements[EMeshPass::Distortion] += NumElements;
		}
	}

#if WITH_EDITOR
	if (ViewRelevance.bDrawRelevance)
	{
		PassMask.Set(EMeshPass::EditorSelection);
		View.NumVisibleDynamicMeshElements[EMeshPass::EditorSelection] += NumElements;

		PassMask.Set(EMeshPass::EditorLevelInstance);
		View.NumVisibleDynamicMeshElements[EMeshPass::EditorLevelInstance] += NumElements;
	}

	// Hair strands are not rendered into the base pass (bRenderInMainPass=0) and so this 
	// adds a special pass for allowing hair strands to be selectable.
	if (ViewRelevance.bHairStrands)
	{
		const EMeshPass::Type MeshPassType = View.bAllowTranslucentPrimitivesInHitProxy ? EMeshPass::HitProxy : EMeshPass::HitProxyOpaqueOnly;
		PassMask.Set(MeshPassType);
		View.NumVisibleDynamicMeshElements[MeshPassType] += NumElements;
	}
#endif

	if (ViewRelevance.bHasVolumeMaterialDomain)
	{
		if (ShouldRenderMeshBatchWithHeterogeneousVolumes(MeshBatch.Mesh, MeshBatch.PrimitiveSceneProxy, View.FeatureLevel))
		{
			View.HeterogeneousVolumesMeshBatches.AddUninitialized(1);
			FVolumetricMeshBatch& BatchAndProxy = View.HeterogeneousVolumesMeshBatches.Last();
			BatchAndProxy.Mesh = MeshBatch.Mesh;
			BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
		}
		else
		{
			View.VolumetricMeshBatches.AddUninitialized(1);
			FVolumetricMeshBatch& BatchAndProxy = View.VolumetricMeshBatches.Last();
			BatchAndProxy.Mesh = MeshBatch.Mesh;
			BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
		}
	}

	if (ViewRelevance.bUsesSkyMaterial)
	{
		View.SkyMeshBatches.AddUninitialized(1);
		FSkyMeshBatch& BatchAndProxy = View.SkyMeshBatches.Last();
		BatchAndProxy.Mesh = MeshBatch.Mesh;
		BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
		BatchAndProxy.bVisibleInMainPass = ViewRelevance.bRenderInMainPass;
		BatchAndProxy.bVisibleInRealTimeSkyCapture = PrimitiveSceneInfo->bVisibleInRealTimeSkyCapture;
	}
	
	if (ViewRelevance.HasTranslucency() && PrimitiveSceneInfo->Proxy->SupportsSortedTriangles())
	{
		View.SortedTrianglesMeshBatches.AddUninitialized(1);
		FSortedTrianglesMeshBatch& BatchAndProxy = View.SortedTrianglesMeshBatches.Last();
		BatchAndProxy.Mesh = MeshBatch.Mesh;
		BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
	}

	if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDecal)
	{
		View.MeshDecalBatches.AddUninitialized(1);
		FMeshDecalBatch& BatchAndProxy = View.MeshDecalBatches.Last();
		BatchAndProxy.Mesh = MeshBatch.Mesh;
		BatchAndProxy.Proxy = MeshBatch.PrimitiveSceneProxy;
		BatchAndProxy.SortKey = MeshBatch.PrimitiveSceneProxy->GetTranslucencySortPriority();
	}
	
	const bool bIsHairStrandsCompatible = ViewRelevance.bHairStrands && IsHairStrandsEnabled(EHairStrandsShaderType::All, View.GetShaderPlatform());
	if (bIsHairStrandsCompatible)
	{
		if (HairStrands::IsHairStrandsVF(MeshBatch.Mesh) && HairStrands::IsHairVisible(MeshBatch))
		{
			View.HairStrandsMeshElements.AddUninitialized(1);
			FMeshBatchAndRelevance& BatchAndProxy = View.HairStrandsMeshElements.Last();
			BatchAndProxy = MeshBatch;
		}

		if (HairStrands::IsHairCardsVF(MeshBatch.Mesh) && ViewRelevance.bRenderInMainPass)
		{
			View.HairCardsMeshElements.AddUninitialized(1);
			FMeshBatchAndRelevance& BatchAndProxy = View.HairCardsMeshElements.Last();
			BatchAndProxy = MeshBatch;
		}
	}
}

void FSceneRenderer::GatherDynamicMeshElements(
	TArray<FViewInfo>& InViews, 
	const FScene* InScene, 
	const FSceneViewFamily& InViewFamily, 
	FGlobalDynamicIndexBuffer& DynamicIndexBuffer,
	FGlobalDynamicVertexBuffer& DynamicVertexBuffer,
	FGlobalDynamicReadBuffer& DynamicReadBuffer,
	const FPrimitiveViewMasks& HasDynamicMeshElementsMasks, 
	const FPrimitiveViewMasks& HasDynamicEditorMeshElementsMasks,
	FMeshElementCollector& Collector)
{
	SCOPE_CYCLE_COUNTER(STAT_GetDynamicMeshElements);

	int32 NumPrimitives = InScene->Primitives.Num();
	check(HasDynamicMeshElementsMasks.Num() == NumPrimitives);

	int32 ViewCount = InViews.Num();
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(
				&InViews[ViewIndex], 
				&InViews[ViewIndex].DynamicMeshElements,
				&InViews[ViewIndex].SimpleElementCollector,
				&InViews[ViewIndex].DynamicPrimitiveCollector,
				InViewFamily.GetFeatureLevel(),
				&DynamicIndexBuffer,
				&DynamicVertexBuffer,
				&DynamicReadBuffer);
		}

		const EShadingPath ShadingPath = Scene->GetShadingPath();

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				// If a mesh is visible in a secondary view, mark it as visible in the primary view
				uint8 ViewMaskFinal = ViewMask;
				for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
				{
					FViewInfo& View = InViews[ViewIndex];
					if (ViewMask & (1 << ViewIndex) && IStereoRendering::IsASecondaryView(View))
					{
						ViewMaskFinal |= 1 << InViews[ViewIndex].PrimaryViewIndex;
					}
				}

				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				const FPrimitiveBounds& Bounds = InScene->PrimitiveBounds[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);

				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMaskFinal, Collector);

				// Compute DynamicMeshElementsMeshPassRelevance for this primitive.
				for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
				{
					if (ViewMaskFinal & (1 << ViewIndex))
					{
						FViewInfo& View = InViews[ViewIndex];
						const bool bAddLightmapDensityCommands = View.Family->EngineShowFlags.LightMapDensity && AllowDebugViewmodes();
						const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];

						const int32 LastNumDynamicMeshElements = View.DynamicMeshElementsPassRelevance.Num();
						View.DynamicMeshElementsPassRelevance.SetNum(View.DynamicMeshElements.Num());

						for (int32 ElementIndex = LastNumDynamicMeshElements; ElementIndex < View.DynamicMeshElements.Num(); ++ElementIndex)
						{
							const FMeshBatchAndRelevance& MeshBatch = View.DynamicMeshElements[ElementIndex];
							FMeshPassMask& PassRelevance = View.DynamicMeshElementsPassRelevance[ElementIndex];

							ComputeDynamicMeshRelevance(ShadingPath, bAddLightmapDensityCommands, ViewRelevance, MeshBatch, View, PassRelevance, PrimitiveSceneInfo, Bounds);
						}
					}
				}
			}

			// Mark DynamicMeshEndIndices end.
			for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
			{
				InViews[ViewIndex].DynamicMeshEndIndices[PrimitiveIndex] = Collector.GetMeshBatchCount(ViewIndex);
			}
		}
	}

	if (GIsEditor)
	{
		Collector.ClearViewMeshArrays();

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ViewIndex++)
		{
			Collector.AddViewMeshArrays(
				&InViews[ViewIndex], 
				&InViews[ViewIndex].DynamicEditorMeshElements, 
				&InViews[ViewIndex].EditorSimpleElementCollector, 
				&InViews[ViewIndex].DynamicPrimitiveCollector,
				InViewFamily.GetFeatureLevel(),
				&DynamicIndexBuffer,
				&DynamicVertexBuffer,
				&DynamicReadBuffer);
		}

		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			const uint8 ViewMask = HasDynamicEditorMeshElementsMasks[PrimitiveIndex];

			if (ViewMask != 0)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = InScene->Primitives[PrimitiveIndex];
				Collector.SetPrimitive(PrimitiveSceneInfo->Proxy, PrimitiveSceneInfo->DefaultDynamicHitProxyId);

				PrimitiveSceneInfo->Proxy->GetDynamicMeshElements(InViewFamily.Views, InViewFamily, ViewMask, Collector);
			}
		}
	}
	MeshCollector.ProcessTasks();
}

/**
 * Helper for InitViews to detect large camera movement, in both angle and position.
 */
static bool IsLargeCameraMovement(FSceneView& View, const FMatrix& PrevViewMatrix, const FVector& PrevViewOrigin, float CameraRotationThreshold, float CameraTranslationThreshold)
{
	float RotationThreshold = FMath::Cos(FMath::DegreesToRadians(CameraRotationThreshold));
	float ViewRightAngle = View.ViewMatrices.GetViewMatrix().GetColumn(0) | PrevViewMatrix.GetColumn(0);
	float ViewUpAngle = View.ViewMatrices.GetViewMatrix().GetColumn(1) | PrevViewMatrix.GetColumn(1);
	float ViewDirectionAngle = View.ViewMatrices.GetViewMatrix().GetColumn(2) | PrevViewMatrix.GetColumn(2);

	FVector Distance = FVector(View.ViewMatrices.GetViewOrigin()) - PrevViewOrigin;
	return 
		ViewRightAngle < RotationThreshold ||
		ViewUpAngle < RotationThreshold ||
		ViewDirectionAngle < RotationThreshold ||
		Distance.SizeSquared() > CameraTranslationThreshold * CameraTranslationThreshold;
}

void FSceneRenderer::PreVisibilityFrameSetup(FRDGBuilder& GraphBuilder, const FSceneTexturesConfig& SceneTexturesConfig)
{
	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	// Notify the RHI we are beginning to render a scene.
	RHICmdList.BeginScene();

	if (Views.Num() > 0 && !ViewFamily.EngineShowFlags.HitProxies)
	{
		FHairStrandsBookmarkParameters Parameters = CreateHairStrandsBookmarkParameters(Scene, Views, AllFamilyViews);
		if (Parameters.HasInstances())
		{
			RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessLODSelection, Parameters);
		}
	}

	if (IsHairStrandsEnabled(EHairStrandsShaderType::All, Scene->GetShaderPlatform()) && Views.Num() > 0 && !ViewFamily.EngineShowFlags.HitProxies)
	{
		// If we are rendering from scene capture we don't need to run another time the hair bookmarks.
		if (Views[0].AllowGPUParticleUpdate())
		{
			FHairStrandsBookmarkParameters Parameters = CreateHairStrandsBookmarkParameters(Scene, Views, AllFamilyViews);
			RunHairStrandsBookmark(GraphBuilder, EHairStrandsBookmark::ProcessGuideInterpolation, Parameters);
		}
	}

	// Notify the FX system that the scene is about to perform visibility checks.

	if (FXSystem && Views.IsValidIndex(0))
	{
		FXSystem->PreInitViews(GraphBuilder, Views[0].AllowGPUParticleUpdate() && !ViewFamily.EngineShowFlags.HitProxies);
	}

#if WITH_EDITOR
	// Draw lines to lights affecting this mesh if its selected.
	if (ViewFamily.EngineShowFlags.LightInfluences)
	{
		for (TConstSetBitIterator<> It(Scene->PrimitivesSelected); It; ++It)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[It.GetIndex()];
			FLightPrimitiveInteraction *LightList = PrimitiveSceneInfo->LightList;
			while (LightList)
			{
				const FLightSceneInfo* LightSceneInfo = LightList->GetLight();

				bool bDynamic = true;
				bool bRelevant = false;
				bool bLightMapped = true;
				bool bShadowMapped = false;
				PrimitiveSceneInfo->Proxy->GetLightRelevance(LightSceneInfo->Proxy, bDynamic, bRelevant, bLightMapped, bShadowMapped);

				if (bRelevant)
				{
					// Draw blue for light-mapped lights and orange for dynamic lights
					const FColor LineColor = bLightMapped ? FColor(0,140,255) : FColor(255,140,0);
					for (int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
					{
						FViewInfo& View = Views[ViewIndex];
						FViewElementPDI LightInfluencesPDI(&View,nullptr,&View.DynamicPrimitiveCollector);
						LightInfluencesPDI.DrawLine(PrimitiveSceneInfo->Proxy->GetBounds().Origin, LightSceneInfo->Proxy->GetLightToWorld().GetOrigin(), LineColor, SDPG_World);
					}
				}
				LightList = LightList->GetNextLight();
			}
		}
	}
#endif

#if UE_BUILD_SHIPPING
	const bool bFreezeTemporalHistories = false;
	const bool bFreezeTemporalSequences = false;
#else
	bool bFreezeTemporalHistories = CVarFreezeTemporalHistories.GetValueOnRenderThread() != 0;
	bool bFreezeTemporalSequences = bFreezeTemporalHistories || CVarFreezeTemporalSequences.GetValueOnRenderThread() != 0;
#endif

	// Load this field once so it has a consistent value for all views (and to avoid the atomic load in the loop).
	// While the value may not be perfectly in sync when we render other view families, this is ok as this
	// invalidation mechanism is only used for interactive rendering where we expect to be constantly drawing the scene.
	// Therefore it is acceptable for some view families to be a frame or so behind others.
	uint32 CurrentPathTracingInvalidationCounter = Scene->PathTracingInvalidationCounter.Load();

	// Setup motion blur parameters (also check for camera movement thresholds)
	for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		FSceneViewState* ViewState = View.ViewState;

		check(View.VerifyMembersChecks());

		// Once per render increment the occlusion frame counter.
		if (ViewState)
		{
			ViewState->OcclusionFrameCounter++;
		}

		// HighResScreenshot should get best results so we don't do the occlusion optimization based on the former frame
		extern bool GIsHighResScreenshot;
		const bool bIsHitTesting = ViewFamily.EngineShowFlags.HitProxies;
		// Don't test occlusion queries in collision viewmode as they can be bigger then the rendering bounds.
		const bool bCollisionView = ViewFamily.EngineShowFlags.CollisionVisibility || ViewFamily.EngineShowFlags.CollisionPawn;
		if (GIsHighResScreenshot || !DoOcclusionQueries() || bIsHitTesting || bCollisionView || ViewFamily.EngineShowFlags.DisableOcclusionQueries)
		{
			View.bDisableQuerySubmissions = true;
			View.bIgnoreExistingQueries = true;
		}

		// set up the screen area for occlusion
		{
			float OcclusionPixelMultiplier = 1.0f;
			if (UseDownsampledOcclusionQueries())
			{
				OcclusionPixelMultiplier = 1.0f / static_cast<float>(FMath::Square(SceneTexturesConfig.SmallDepthDownsampleFactor));
			}
			float NumPossiblePixels = static_cast<float>(View.ViewRect.Width() * View.ViewRect.Height()) * OcclusionPixelMultiplier;
			View.OneOverNumPossiblePixels = NumPossiblePixels > 0.0 ? 1.0f / NumPossiblePixels : 0.0f;
		}

		// Still need no jitter to be set for temporal feedback on SSR (it is enabled even when temporal AA is off).
		check(View.TemporalJitterPixels.X == 0.0f);
		check(View.TemporalJitterPixels.Y == 0.0f);
		
		// Cache the projection matrix b		
		// Cache the projection matrix before AA is applied
		View.ViewMatrices.SaveProjectionNoAAMatrix();

		if (ViewState)
		{
			check(View.bStatePrevViewInfoIsReadOnly);
			View.bStatePrevViewInfoIsReadOnly = ViewFamily.bWorldIsPaused || ViewFamily.EngineShowFlags.HitProxies || bFreezeTemporalHistories;

			ViewState->SetupDistanceFieldTemporalOffset(ViewFamily);

			if (!View.bStatePrevViewInfoIsReadOnly && !bFreezeTemporalSequences)
			{
				ViewState->FrameIndex++;
			}

			if (View.OverrideFrameIndexValue.IsSet())
			{
				ViewState->FrameIndex = View.OverrideFrameIndexValue.GetValue();
			}
		}
		
		// Subpixel jitter for temporal AA
		int32 CVarTemporalAASamplesValue = CVarTemporalAASamples.GetValueOnRenderThread();

		EMainTAAPassConfig TAAConfig = ITemporalUpscaler::GetMainTAAPassConfig(View);

		bool bTemporalUpsampling = View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale;
		
		// Apply a sub pixel offset to the view.
		if (IsTemporalAccumulationBasedMethod(View.AntiAliasingMethod) && ViewState && (CVarTemporalAASamplesValue > 0 || bTemporalUpsampling) && View.bAllowTemporalJitter)
		{
			float EffectivePrimaryResolutionFraction = float(View.ViewRect.Width()) / float(View.GetSecondaryViewRectSize().X);

			// Compute number of TAA samples.
			int32 TemporalAASamples;
			{
				if (TAAConfig == EMainTAAPassConfig::TSR)
				{
					// Force the number of AA sample to make sure the quality doesn't get
					// compromised by previously set settings for Gen4 TAA
					TemporalAASamples = 8;
				}
				else
				{
					TemporalAASamples = FMath::Clamp(CVarTemporalAASamplesValue, 1, 255);
				}

				if (bTemporalUpsampling)
				{
					// When doing TAA upsample with screen percentage < 100%, we need extra temporal samples to have a
					// constant temporal sample density for final output pixels to avoid output pixel aligned converging issues.
					TemporalAASamples = FMath::RoundToInt(float(TemporalAASamples) * FMath::Max(1.f, 1.f / (EffectivePrimaryResolutionFraction * EffectivePrimaryResolutionFraction)));
				}
				else if (CVarTemporalAASamplesValue == 5)
				{
					TemporalAASamples = 4;
				}

				// Use immediately higher prime number to break up coherence between the TAA jitter sequence and any
				// other random signal that are power of two of View.StateFrameIndex
				if (TAAConfig == EMainTAAPassConfig::TSR)
				{
					static const int8 kFirstPrimeNumbers[25] = {
						2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97,
					};

					for (int32 PrimeNumberId = 4; PrimeNumberId < UE_ARRAY_COUNT(kFirstPrimeNumbers); PrimeNumberId++)
					{
						if (int32(kFirstPrimeNumbers[PrimeNumberId]) >= TemporalAASamples)
						{
							TemporalAASamples = int32(kFirstPrimeNumbers[PrimeNumberId]);
							break;
						}
					}
				}
			}

			// Compute the new sample index in the temporal sequence.
			int32 TemporalSampleIndex = ViewState->TemporalAASampleIndex + 1;
			if (TemporalSampleIndex >= TemporalAASamples || View.bCameraCut)
			{
				TemporalSampleIndex = 0;
			}

			#if !UE_BUILD_SHIPPING
			if (CVarTAADebugOverrideTemporalIndex.GetValueOnRenderThread() >= 0)
			{
				TemporalSampleIndex = CVarTAADebugOverrideTemporalIndex.GetValueOnRenderThread();
			}
			#endif

			// Updates view state.
			if (!View.bStatePrevViewInfoIsReadOnly && !bFreezeTemporalSequences)
			{
				ViewState->TemporalAASampleIndex = TemporalSampleIndex;
			}

			// Choose sub pixel sample coordinate in the temporal sequence.
			float SampleX, SampleY;
			if (View.PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
			{
				// Uniformly distribute temporal jittering in [-.5; .5], because there is no longer any alignement of input and output pixels.
				SampleX = Halton(TemporalSampleIndex + 1, 2) - 0.5f;
				SampleY = Halton(TemporalSampleIndex + 1, 3) - 0.5f;

				View.MaterialTextureMipBias = -(FMath::Max(-FMath::Log2(EffectivePrimaryResolutionFraction), 0.0f) ) + CVarMinAutomaticViewMipBiasOffset.GetValueOnRenderThread();
				View.MaterialTextureMipBias = FMath::Max(View.MaterialTextureMipBias, CVarMinAutomaticViewMipBias.GetValueOnRenderThread());
			}
			else if( CVarTemporalAASamplesValue == 2 )
			{
				// 2xMSAA
				// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
				//   N.
				//   .S
				float SamplesX[] = { -4.0f/16.0f, 4.0/16.0f };
				float SamplesY[] = { -4.0f/16.0f, 4.0/16.0f };
				check(TemporalAASamples == UE_ARRAY_COUNT(SamplesX));
				SampleX = SamplesX[ TemporalSampleIndex ];
				SampleY = SamplesY[ TemporalSampleIndex ];
			}
			else if( CVarTemporalAASamplesValue == 3 )
			{
				// 3xMSAA
				//   A..
				//   ..B
				//   .C.
				// Rolling circle pattern (A,B,C).
				float SamplesX[] = { -2.0f/3.0f,  2.0/3.0f,  0.0/3.0f };
				float SamplesY[] = { -2.0f/3.0f,  0.0/3.0f,  2.0/3.0f };
				check(TemporalAASamples == UE_ARRAY_COUNT(SamplesX));
				SampleX = SamplesX[ TemporalSampleIndex ];
				SampleY = SamplesY[ TemporalSampleIndex ];
			}
			else if( CVarTemporalAASamplesValue == 4 )
			{
				// 4xMSAA
				// Pattern docs: http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx
				//   .N..
				//   ...E
				//   W...
				//   ..S.
				// Rolling circle pattern (N,E,S,W).
				float SamplesX[] = { -2.0f/16.0f,  6.0/16.0f, 2.0/16.0f, -6.0/16.0f };
				float SamplesY[] = { -6.0f/16.0f, -2.0/16.0f, 6.0/16.0f,  2.0/16.0f };
				check(TemporalAASamples == UE_ARRAY_COUNT(SamplesX));
				SampleX = SamplesX[ TemporalSampleIndex ];
				SampleY = SamplesY[ TemporalSampleIndex ];
			}
			else if( CVarTemporalAASamplesValue == 5 )
			{
				// Compressed 4 sample pattern on same vertical and horizontal line (less temporal flicker).
				// Compressed 1/2 works better than correct 2/3 (reduced temporal flicker).
				//   . N .
				//   W . E
				//   . S .
				// Rolling circle pattern (N,E,S,W).
				float SamplesX[] = {  0.0f/2.0f,  1.0/2.0f,  0.0/2.0f, -1.0/2.0f };
				float SamplesY[] = { -1.0f/2.0f,  0.0/2.0f,  1.0/2.0f,  0.0/2.0f };
				check(TemporalAASamples == UE_ARRAY_COUNT(SamplesX));
				SampleX = SamplesX[ TemporalSampleIndex ];
				SampleY = SamplesY[ TemporalSampleIndex ];
			}
			else
			{
				float u1 = Halton( TemporalSampleIndex + 1, 2 );
				float u2 = Halton( TemporalSampleIndex + 1, 3 );

				// Generates samples in normal distribution
				// exp( x^2 / Sigma^2 )
					
				static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAAFilterSize"));
				float FilterSize = CVar->GetFloat();

				// Scale distribution to set non-unit variance
				// Variance = Sigma^2
				float Sigma = 0.47f * FilterSize;

				// Window to [-0.5, 0.5] output
				// Without windowing we could generate samples far away on the infinite tails.
				float OutWindow = 0.5f;
				float InWindow = FMath::Exp( -0.5 * FMath::Square( OutWindow / Sigma ) );
					
				// Box-Muller transform
				float Theta = 2.0f * PI * u2;
				float r = Sigma * FMath::Sqrt( -2.0f * FMath::Loge( (1.0f - u1) * InWindow + u1 ) );
					
				SampleX = r * FMath::Cos( Theta );
				SampleY = r * FMath::Sin( Theta );
			}

			View.TemporalJitterSequenceLength = TemporalAASamples;
			View.TemporalJitterIndex = TemporalSampleIndex;
			View.TemporalJitterPixels.X = SampleX;
			View.TemporalJitterPixels.Y = SampleY;

			View.ViewMatrices.HackAddTemporalAAProjectionJitter(FVector2D(SampleX * 2.0f / View.ViewRect.Width(), SampleY * -2.0f / View.ViewRect.Height()));
		}

		// Setup a new FPreviousViewInfo from current frame infos.
		FPreviousViewInfo NewPrevViewInfo;
		{
			NewPrevViewInfo.ViewRect = View.ViewRect;
			NewPrevViewInfo.ViewMatrices = View.ViewMatrices;
			NewPrevViewInfo.ViewRect = View.ViewRect;
		}

		if ( ViewState )
		{
			// update previous frame matrices in case world origin was rebased on this frame
			if (!View.OriginOffsetThisFrame.IsZero())
			{
				ViewState->PrevFrameViewInfo.ViewMatrices.ApplyWorldOffset(View.OriginOffsetThisFrame);
			}

			// determine if we are initializing or we should reset the persistent state
			const float DeltaTime = View.Family->Time.GetRealTimeSeconds() - ViewState->LastRenderTime;
			const bool bFirstFrameOrTimeWasReset = DeltaTime < -0.0001f || ViewState->LastRenderTime < 0.0001f;
			const bool bIsLargeCameraMovement = IsLargeCameraMovement(
				View,
				ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(),
				ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(),
				75.0f, GCameraCutTranslationThreshold);
			const bool bResetCamera = (bFirstFrameOrTimeWasReset || View.bCameraCut || bIsLargeCameraMovement || View.bForceCameraVisibilityReset);
			
#if RHI_RAYTRACING
			static const auto CVarTemporalDenoiser = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.PathTracing.TemporalDenoiser.mode"));
			const int TemporalDenoiserMode = CVarTemporalDenoiser ? CVarTemporalDenoiser->GetValueOnAnyThread() : 0;

			if (View.bIsOfflineRender)
			{
				// In the offline context, we want precise control over when to restart the path tracer's accumulation to allow for motion blur
				// So we use the camera cut signal only. In particular - we should not use bForceCameraVisibilityReset since this has
				// interactions with the motion blur post process effect in tiled rendering (see comment below).
				if (View.bCameraCut || View.bForcePathTracerReset)
				{
					const bool bClearTemporalDenoisingHistory = (TemporalDenoiserMode == 1) ? View.bCameraCut : true;
					ViewState->PathTracingInvalidate(bClearTemporalDenoisingHistory);
				}
			}
			else
			{
				// for interactive usage - any movement or scene change should restart the path tracer

				// Note: 0.18 deg is the minimum angle for avoiding numerical precision issue (which would cause constant invalidation)
				const bool bIsCameraMove = IsLargeCameraMovement(
					View,
					ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(),
					ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(),
					0.18f /*degree*/, 0.1f /*cm*/);
				const bool bIsProjMatrixDifferent = View.ViewMatrices.GetProjectionNoAAMatrix() != View.ViewState->PrevFrameViewInfo.ViewMatrices.GetProjectionNoAAMatrix();

				// For each view, we remember what the invalidation counter was set to last time we were here so we can catch all changes
				const bool bNeedsInvalidation = ViewState->PathTracingInvalidationCounter != CurrentPathTracingInvalidationCounter;
				ViewState->PathTracingInvalidationCounter = CurrentPathTracingInvalidationCounter;
				if (bNeedsInvalidation ||
					bIsProjMatrixDifferent ||
					bIsCameraMove ||
					View.bCameraCut ||
					View.bForceCameraVisibilityReset ||
					View.bForcePathTracerReset)
				{
					const bool bClearTemporalDenoisingHistory = (TemporalDenoiserMode == 2) ? View.bCameraCut : true;
					ViewState->PathTracingInvalidate(bClearTemporalDenoisingHistory);
				}
			}

#endif // RHI_RAYTRACING

			if (bResetCamera)
			{
				View.PrevViewInfo = NewPrevViewInfo;

				// PT: If the motion blur shader is the last shader in the post-processing chain then it is the one that is
				//     adjusting for the viewport offset.  So it is always required and we can't just disable the work the
				//     shader does.  The correct fix would be to disable the effect when we don't need it and to properly mark
				//     the uber-postprocessing effect as the last effect in the chain.

				View.bPrevTransformsReset = true;
			}
			else
			{
				View.PrevViewInfo = ViewState->PrevFrameViewInfo;
			}

			// Replace previous view info of the view state with this frame, clearing out references over render target.
			if (!View.bStatePrevViewInfoIsReadOnly)
			{
				ViewState->PrevFrameViewInfo = NewPrevViewInfo;
			}

			// If the view has a previous view transform, then overwrite the previous view info for the _current_ frame.
			if (View.PreviousViewTransform.IsSet())
			{
				// Note that we must ensure this transform ends up in ViewState->PrevFrameViewInfo else it will be used to calculate the next frame's motion vectors as well
				View.PrevViewInfo.ViewMatrices.UpdateViewMatrix(View.PreviousViewTransform->GetTranslation(), View.PreviousViewTransform->GetRotation().Rotator());
			}

			// detect conditions where we should reset occlusion queries
			if (bFirstFrameOrTimeWasReset || 
				ViewState->LastRenderTime + GEngine->PrimitiveProbablyVisibleTime < View.Family->Time.GetRealTimeSeconds() ||
				View.bCameraCut ||
				View.bForceCameraVisibilityReset ||
				IsLargeCameraMovement(
					View, 
				    FMatrix(ViewState->PrevViewMatrixForOcclusionQuery), 
				    ViewState->PrevViewOriginForOcclusionQuery, 
				    GEngine->CameraRotationThreshold, GEngine->CameraTranslationThreshold))
			{
				View.bIgnoreExistingQueries = true;
				View.bDisableDistanceBasedFadeTransitions = true;
			}

			// Turn on/off round-robin occlusion querying in the ViewState
			static const auto CVarRROCC = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.RoundRobinOcclusion"));
			const bool bEnableRoundRobin = CVarRROCC ? (CVarRROCC->GetValueOnAnyThread() != false) : false;
			if (bEnableRoundRobin != ViewState->IsRoundRobinEnabled())
			{
				ViewState->UpdateRoundRobin(bEnableRoundRobin);
				View.bIgnoreExistingQueries = true;
			}

			ViewState->PrevViewMatrixForOcclusionQuery = FMatrix44f(View.ViewMatrices.GetViewMatrix());	// LWC_TODO: Precision loss
			ViewState->PrevViewOriginForOcclusionQuery = View.ViewMatrices.GetViewOrigin();
				
			// store old view matrix and detect conditions where we should reset motion blur 
#if RHI_RAYTRACING
			{
				if (bResetCamera || IsLargeCameraMovement(View, ViewState->PrevFrameViewInfo.ViewMatrices.GetViewMatrix(), ViewState->PrevFrameViewInfo.ViewMatrices.GetViewOrigin(), 0.1f, 0.1f))
				{
					ViewState->RayTracingNumIterations = 1;
				}
				else
				{
					ViewState->RayTracingNumIterations++;
				}
			}
#endif // RHI_RAYTRACING

			// we don't use DeltaTime as it can be 0 (in editor) and is computed by subtracting floats (loses precision over time)
			// Clamp DeltaWorldTime to reasonable values for the purposes of motion blur, things like TimeDilation can make it very small
			// Offline renders always control the timestep for the view and always need the timescales calculated.
			if (!ViewFamily.bWorldIsPaused || View.bIsOfflineRender)
			{
				ViewState->UpdateMotionBlurTimeScale(View);
			}
			

			ViewState->PrevFrameNumber = ViewState->PendingPrevFrameNumber;
			ViewState->PendingPrevFrameNumber = View.Family->FrameNumber;

			// This finishes the update of view state
			ViewState->UpdateLastRenderTime(*View.Family);

			ViewState->UpdateTemporalLODTransition(View);
		}
		else
		{
			// Without a viewstate, we just assume that camera has not moved.
			View.PrevViewInfo = NewPrevViewInfo;
		}
	}

#if RHI_RAYTRACING
	if (Scene && Views.Num())
	{
		const int32 ReferenceViewIndex = 0;
		const FViewInfo& ReferenceView = Views[ReferenceViewIndex];

		Scene->RayTracingScene.InitPreViewTranslation(ReferenceView.ViewMatrices);
		Scene->RayTracingScene.bNeedsDebugInstanceGPUSceneIndexBuffer = IsRayTracingInstanceOverlapEnabled(ReferenceView);
	}
#endif

	// Setup global dither fade in and fade out uniform buffers.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		FDitherUniformShaderParameters DitherUniformShaderParameters;
		DitherUniformShaderParameters.LODFactor = View.GetTemporalLODTransition();
		View.DitherFadeOutUniformBuffer = FDitherUniformBufferRef::CreateUniformBufferImmediate(DitherUniformShaderParameters, UniformBuffer_SingleFrame);

		DitherUniformShaderParameters.LODFactor = View.GetTemporalLODTransition() - 1.0f;
		View.DitherFadeInUniformBuffer = FDitherUniformBufferRef::CreateUniformBufferImmediate(DitherUniformShaderParameters, UniformBuffer_SingleFrame);
	}

	for (const auto& ViewExtension : ViewFamily.ViewExtensions)
	{
		ViewExtension->PreInitViews_RenderThread(GraphBuilder);
	}
}

void FSceneViewState::UpdateMotionBlurTimeScale(const FViewInfo& View)
{
	const int32 MotionBlurTargetFPS = View.FinalPostProcessSettings.MotionBlurTargetFPS;

	// Ensure we can divide by the Delta Time later without a divide by zero.
	float DeltaRealTime = FMath::Max(View.Family->Time.GetDeltaRealTimeSeconds(), SMALL_NUMBER);

	// Track the current FPS by using an exponential moving average of the current delta time.
	if (MotionBlurTargetFPS <= 0)
	{
		// Keep motion vector lengths stable for paused sequencer frames.
		if (GetSequencerState() == ESS_Paused)
		{
			// Reset the moving average to the current delta time.
			MotionBlurTargetDeltaTime = DeltaRealTime;
		}
		else
		{
			// Smooth the target delta time using a moving average.
			MotionBlurTargetDeltaTime = FMath::Lerp(MotionBlurTargetDeltaTime, DeltaRealTime, 0.1f);
		}
	}
	else // Track a fixed target FPS.
	{
		// Keep motion vector lengths stable for paused sequencer frames. Assumes a 60 FPS tick.
		// Tuned for content compatibility with existing content when target is the default 30 FPS.
		if (GetSequencerState() == ESS_Paused)
		{
			DeltaRealTime = 1.0f / 60.0f;
		}


		MotionBlurTargetDeltaTime = 1.0f / static_cast<float>(MotionBlurTargetFPS);
	}

	MotionBlurTimeScale = MotionBlurTargetDeltaTime / DeltaRealTime;
}

void UpdateReflectionSceneData(FScene* Scene)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateReflectionSceneData)
	SCOPED_NAMED_EVENT(UpdateReflectionScene, FColor::Red);


	FReflectionEnvironmentSceneData& ReflectionSceneData = Scene->ReflectionSceneData;

	ReflectionSceneData.SortedCaptures.Reset(ReflectionSceneData.RegisteredReflectionCaptures.Num());
	ReflectionSceneData.NumBoxCaptures = 0;
	ReflectionSceneData.NumSphereCaptures = 0;

	const int32 MaxCubemaps = ReflectionSceneData.CubemapArray.GetMaxCubemaps();
	int32_t PlatformMaxNumReflectionCaptures = FMath::Min(FMath::FloorToInt(GMaxTextureArrayLayers / 6.0f), GMaxNumReflectionCaptures);

	// Pack visible reflection captures into the uniform buffer, each with an index to its cubemap array entry.
	// GPUScene primitive data stores closest reflection capture as index into this buffer, so this index which must be invalidate every time OutSortData contents change.
	for (int32 ReflectionProxyIndex = 0; ReflectionProxyIndex < ReflectionSceneData.RegisteredReflectionCaptures.Num() && ReflectionSceneData.SortedCaptures.Num() < PlatformMaxNumReflectionCaptures; ReflectionProxyIndex++)
	{
		FReflectionCaptureProxy* CurrentCapture = ReflectionSceneData.RegisteredReflectionCaptures[ReflectionProxyIndex];

		FReflectionCaptureSortData NewSortEntry;

		NewSortEntry.CubemapIndex = -1;
		NewSortEntry.CaptureOffsetAndAverageBrightness = FVector4f(CurrentCapture->CaptureOffset, 1.0f);
		NewSortEntry.CaptureProxy = CurrentCapture;
		if (SupportsTextureCubeArray(Scene->GetFeatureLevel()))
		{
			FCaptureComponentSceneState* ComponentStatePtr = ReflectionSceneData.AllocatedReflectionCaptureState.Find(CurrentCapture->Component);
			if (!ComponentStatePtr)
			{
				// Skip reflection captures without built data to upload
				continue;
			}

			NewSortEntry.CubemapIndex = ComponentStatePtr->CubemapIndex;
			check(NewSortEntry.CubemapIndex < MaxCubemaps || NewSortEntry.CubemapIndex == 0);
			NewSortEntry.CaptureOffsetAndAverageBrightness.W = ComponentStatePtr->AverageBrightness;
		}

		NewSortEntry.Guid = CurrentCapture->Guid;
		NewSortEntry.RelativePosition = CurrentCapture->RelativePosition;
		NewSortEntry.TilePosition = CurrentCapture->TilePosition;
		NewSortEntry.Radius = CurrentCapture->InfluenceRadius;
		float ShapeTypeValue = (float)CurrentCapture->Shape;
		NewSortEntry.CaptureProperties = FVector4f(CurrentCapture->Brightness, NewSortEntry.CubemapIndex, ShapeTypeValue, 0);

		if (CurrentCapture->Shape == EReflectionCaptureShape::Plane)
		{
			//planes count as boxes in the compute shader.
			++ReflectionSceneData.NumBoxCaptures;
			NewSortEntry.BoxTransform = FMatrix44f(
				FPlane4f(CurrentCapture->LocalReflectionPlane),
				FPlane4f((FVector4f)CurrentCapture->ReflectionXAxisAndYScale), // LWC_TODO: precision loss
				FPlane4f(0, 0, 0, 0),
				FPlane4f(0, 0, 0, 0));

			NewSortEntry.BoxScales = FVector4f(0);
		}
		else if (CurrentCapture->Shape == EReflectionCaptureShape::Sphere)
		{
			++ReflectionSceneData.NumSphereCaptures;
		}
		else
		{
			++ReflectionSceneData.NumBoxCaptures;
			NewSortEntry.BoxTransform = CurrentCapture->BoxTransform;
			NewSortEntry.BoxScales = FVector4f(CurrentCapture->BoxScales, CurrentCapture->BoxTransitionDistance);
		}

		ReflectionSceneData.SortedCaptures.Add(NewSortEntry);
	}

	ReflectionSceneData.SortedCaptures.Sort();

	for (int32 CaptureIndex = 0; CaptureIndex < ReflectionSceneData.SortedCaptures.Num(); CaptureIndex++)
	{
		ReflectionSceneData.SortedCaptures[CaptureIndex].CaptureProxy->SortedCaptureIndex = CaptureIndex;
	}


	// If SortedCaptures change, then in case of forward renderer all scene primitives need to be updated, as they 
	// store index into sorted reflection capture uniform buffer for the forward renderer.
	if (IsForwardShadingEnabled(Scene->GetShaderPlatform()) && ReflectionSceneData.AllocatedReflectionCaptureStateHasChanged)
	{
		const int32 NumPrimitives = Scene->Primitives.Num();
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
		{
			Scene->Primitives[PrimitiveIndex]->SetNeedsUniformBufferUpdate(true);
		}

		Scene->GPUScene.bUpdateAllPrimitives = true;

		ReflectionSceneData.AllocatedReflectionCaptureStateHasChanged = false;
	}


	// Mark all primitives for reflection proxy update
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_MarkAllPrimitivesForReflectionProxyUpdate);

		if (Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged)
		{
			// Mobile needs to re-cache all mesh commands when scene capture data has changed
			const bool bNeedsStaticMeshUpdate = Scene->GetShadingPath() == EShadingPath::Mobile;
			
			// Mark all primitives as needing an update
			// Note: Only visible primitives will actually update their reflection proxy
			for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
			{
				FPrimitiveSceneInfo* Primitive = Scene->Primitives[PrimitiveIndex];
				Primitive->RemoveCachedReflectionCaptures();

				if (bNeedsStaticMeshUpdate)
				{
					Primitive->CacheReflectionCaptures();
					Primitive->BeginDeferredUpdateStaticMeshes();
				}
			}

			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = false;
		}
	}
}

#if !UE_BUILD_SHIPPING
static uint32 GetDrawCountFromPrimitiveSceneInfo(FScene* Scene, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	uint32 DrawCount = 0;
	for (const FCachedMeshDrawCommandInfo& CachedCommand : PrimitiveSceneInfo->StaticMeshCommandInfos)
	{
		if (CachedCommand.MeshPass != EMeshPass::BasePass)
			continue;

		if (CachedCommand.StateBucketId != INDEX_NONE || CachedCommand.CommandIndex >= 0)
		{
			DrawCount++;
		}
	}

	return DrawCount;
}


FViewDebugInfo FViewDebugInfo::Instance;

FViewDebugInfo::FViewDebugInfo()
{
	bHasEverUpdated = false;
	bIsOutdated = true;
	bShouldUpdate = false;
	bShouldCaptureSingleFrame = false;
}

void FViewDebugInfo::ProcessPrimitive(FPrimitiveSceneInfo* PrimitiveSceneInfo, const FViewInfo& View, FScene* Scene, const UPrimitiveComponent* DebugComponent)
{
	if (!DebugComponent->IsRegistered())
	{
		return;
	}
	AActor* Actor = DebugComponent->GetOwner();
	FString FullName = DebugComponent->GetName();
	const uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

	TArray<UMaterialInterface*> Materials;
	DebugComponent->GetUsedMaterials(Materials);
	const int32 LOD = PrimitiveSceneInfo->Proxy ? PrimitiveSceneInfo->Proxy->GetLOD(&View) : INDEX_NONE;
	int32 Triangles = 0;
	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(DebugComponent))
	{
		Triangles = StaticMeshComponent->GetStaticMesh()->GetNumTriangles(LOD);
	}
	else if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(DebugComponent))
	{
		for (const FSkeletalMeshLODRenderData& RenderData : SkeletalMeshComponent->GetSkeletalMeshRenderData()->LODRenderData)
		{
			Triangles += RenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num() / 3;
		}
	}
		
	const FPrimitiveInfo PrimitiveInfo = {
		Actor,
		DebugComponent->ComponentId,
		const_cast<UPrimitiveComponent*>(DebugComponent), // This is probably a bad idea, find alternative
		PrimitiveSceneInfo,
		MoveTemp(Materials),
		MoveTemp(FullName),
		DrawCount,
		Triangles,
		LOD
	};

	Primitives.Add(PrimitiveInfo);
}

void FViewDebugInfo::DumpToCSV() const
{
	const FString OutputPath = FPaths::ProfilingDir() / TEXT("Primitives") / FString::Printf(TEXT("PrimitivesDetailed-%s.csv"), *FDateTime::Now().ToString());
	const bool bSuppressViewer = true;
	FDiagnosticTableViewer DrawViewer(*OutputPath, bSuppressViewer);
	DrawViewer.AddColumn(TEXT("Name"));
	DrawViewer.AddColumn(TEXT("ActorClass"));
	DrawViewer.AddColumn(TEXT("Actor"));
	DrawViewer.AddColumn(TEXT("Location"));
	DrawViewer.AddColumn(TEXT("NumMaterials"));
	DrawViewer.AddColumn(TEXT("Materials"));
	DrawViewer.AddColumn(TEXT("NumDraws"));
	DrawViewer.AddColumn(TEXT("LOD"));
	DrawViewer.AddColumn(TEXT("Triangles"));
	DrawViewer.CycleRow();

	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	const FPrimitiveSceneInfo* LastPrimitiveSceneInfo = nullptr;
	for (const FPrimitiveInfo& Primitive : Primitives)
	{
		if (Primitive.PrimitiveSceneInfo != LastPrimitiveSceneInfo)
		{
			DrawViewer.AddColumn(*Primitive.Name);
			DrawViewer.AddColumn(Primitive.Owner ? *Primitive.Owner->GetClass()->GetName() : TEXT(""));
			DrawViewer.AddColumn(Primitive.Owner ? *Primitive.Owner->GetFullName() : TEXT(""));
			DrawViewer.AddColumn(Primitive.Owner ?
				*FString::Printf(TEXT("{%s}"), *Primitive.Owner->GetActorLocation().ToString()) : TEXT(""));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.Materials.Num()));
			FString Materials = "[";
			for (int i = 0; i < Primitive.Materials.Num(); i++)
			{
				if (Primitive.Materials[i] && Primitive.Materials[i]->GetMaterial())
				{
					Materials += Primitive.Materials[i]->GetMaterial()->GetName();
				}
				else
				{
					Materials += "Null";
				}
				
				if (i < Primitive.Materials.Num() - 1)
				{
					Materials += ", ";
				}
			}
			Materials += "]";
			DrawViewer.AddColumn(*FString::Printf(TEXT("%s"), *Materials));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.DrawCount));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.LOD));
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.TriangleCount));
			DrawViewer.CycleRow();

			LastPrimitiveSceneInfo = Primitive.PrimitiveSceneInfo;
		}
	}
}

void FViewDebugInfo::CaptureNextFrame()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = true;
	bShouldUpdate = true;
}

void FViewDebugInfo::EnableLiveCapture()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = false;
	bShouldUpdate = true;
}

void FViewDebugInfo::DisableLiveCapture()
{
	FRWScopeLock ScopeLock(Lock, SLT_Write);
	bShouldCaptureSingleFrame = false;
	bShouldUpdate = false;
}

bool FViewDebugInfo::HasEverUpdated() const
{
	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	return bHasEverUpdated;
}

bool FViewDebugInfo::IsOutOfDate() const
{
	FRWScopeLock ScopeLock(Lock, SLT_ReadOnly);
	return bIsOutdated;
}

void FSceneRenderer::ProcessPrimitives(const FViewInfo& View, const FViewCommands& ViewCommands) const
{
	FViewDebugInfo& DebugInfo = FViewDebugInfo::Instance;
	{
		FRWScopeLock ScopeLock(DebugInfo.Lock, SLT_Write);
		DebugInfo.bIsOutdated = true;
	
		if (!DebugInfo.bShouldUpdate && !bDumpDetailedPrimitivesNextFrame)
		{
			return;
		}

		if (DebugInfo.bShouldCaptureSingleFrame)
		{
			DebugInfo.bShouldCaptureSingleFrame = false;
			DebugInfo.bShouldUpdate = false;
		}

		// TODO: Add profiling to this function
	
		DebugInfo.Primitives.Empty(ViewCommands.MeshCommands[EMeshPass::BasePass].Num() + ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass].Num());

		for (const FVisibleMeshDrawCommand& Mesh : ViewCommands.MeshCommands[EMeshPass::BasePass])
		{
			const int32 PrimitiveId = Mesh.PrimitiveIdInfo.ScenePrimitiveId;
			if (PrimitiveId >= 0 && PrimitiveId < Scene->Primitives.Num())
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveId];
				DebugInfo.ProcessPrimitive(PrimitiveSceneInfo, View, Scene, PrimitiveSceneInfo->ComponentForDebuggingOnly);
			}
		}

		for (const FStaticMeshBatch* StaticMeshBatch : ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass])
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = StaticMeshBatch->PrimitiveSceneInfo;
			DebugInfo.ProcessPrimitive(PrimitiveSceneInfo, View, Scene, PrimitiveSceneInfo->ComponentForDebuggingOnly);
		}

		DebugInfo.bHasEverUpdated = true;
		DebugInfo.bIsOutdated = false;
	}
	DebugInfo.OnUpdate.Broadcast();
	
	if (bDumpDetailedPrimitivesNextFrame)
	{
		DebugInfo.DumpToCSV();
		bDumpDetailedPrimitivesNextFrame = false;
	}
	
}

void FSceneRenderer::DumpPrimitives(const FViewCommands& ViewCommands)
{
	if (!bDumpPrimitivesNextFrame)
	{
		return;
	}

	bDumpPrimitivesNextFrame = false;

	struct FPrimitiveInfo
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo;
		FString Name;
		uint32 DrawCount;

		bool operator<(const FPrimitiveInfo& Other) const
		{
			// Sort by name to group similar assets together, then by exact primitives so we can ignore duplicates
			const int32 NameCompare = Name.Compare(Other.Name);
			if (NameCompare != 0)
			{
				return NameCompare < 0;
			}

			return PrimitiveSceneInfo < Other.PrimitiveSceneInfo;
		}
	};

	TArray<FPrimitiveInfo> Primitives;
	Primitives.Reserve(ViewCommands.MeshCommands[EMeshPass::BasePass].Num() + ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass].Num());

	{
		for (const FVisibleMeshDrawCommand& Mesh : ViewCommands.MeshCommands[EMeshPass::BasePass])
		{
			int32 PrimitiveId = Mesh.PrimitiveIdInfo.ScenePrimitiveId;
			if (PrimitiveId >= 0 && PrimitiveId < Scene->Primitives.Num())
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveId];
				FString FullName = PrimitiveSceneInfo->ComponentForDebuggingOnly->GetFullName();

				uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

				Primitives.Add({ PrimitiveSceneInfo, MoveTemp(FullName), DrawCount });
			}
		}

		for (const FStaticMeshBatch* StaticMeshBatch : ViewCommands.DynamicMeshCommandBuildRequests[EMeshPass::BasePass])
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = StaticMeshBatch->PrimitiveSceneInfo;
			FString FullName = PrimitiveSceneInfo->ComponentForDebuggingOnly->GetFullName();

			uint32 DrawCount = GetDrawCountFromPrimitiveSceneInfo(Scene, PrimitiveSceneInfo);

			Primitives.Add({ PrimitiveSceneInfo, MoveTemp(FullName), DrawCount });
		}
	}

	Primitives.Sort();

	const FString OutputPath = FPaths::ProfilingDir() / TEXT("Primitives") / FString::Printf(TEXT("Primitives-%s.csv"), *FDateTime::Now().ToString());
	const bool bSuppressViewer = true;
	FDiagnosticTableViewer DrawViewer(*OutputPath, bSuppressViewer);
	DrawViewer.AddColumn(TEXT("Name"));
	DrawViewer.AddColumn(TEXT("NumDraws"));
	DrawViewer.CycleRow();

	const FPrimitiveSceneInfo* LastPrimitiveSceneInfo = nullptr;
	for (const FPrimitiveInfo& Primitive : Primitives)
	{
		if (Primitive.PrimitiveSceneInfo != LastPrimitiveSceneInfo)
		{
			DrawViewer.AddColumn(*Primitive.Name);
			DrawViewer.AddColumn(*FString::Printf(TEXT("%d"), Primitive.DrawCount));
			DrawViewer.CycleRow();

			LastPrimitiveSceneInfo = Primitive.PrimitiveSceneInfo;
		}
	}
}
#endif

#if WITH_EDITOR

static void UpdateHitProxyIdBuffer(
	TArray<uint32>& HitProxyIds,
	FDynamicReadBuffer& DynamicReadBuffer)
{
	Algo::Sort(HitProxyIds);
	int32 EndIndex = Algo::Unique(HitProxyIds);
	HitProxyIds.RemoveAt(EndIndex, HitProxyIds.Num() - EndIndex);

	uint32 IdCount = HitProxyIds.Num();
	uint32 BufferCount = FMath::Max(FMath::RoundUpToPowerOfTwo(IdCount), 1u);

	if (DynamicReadBuffer.NumBytes != BufferCount)
	{
		DynamicReadBuffer.Initialize(TEXT("DynamicReadBuffer"), sizeof(uint32), BufferCount, PF_R32_UINT, BUF_Dynamic);
	}

	DynamicReadBuffer.Lock();
	{
		uint32* Data = reinterpret_cast<uint32*>(DynamicReadBuffer.MappedBuffer);

		for (uint32 i = 0; i < IdCount; ++i)
		{
			Data[i] = HitProxyIds[i];
		}

		uint32 FillValue = IdCount == 0 ? 0 : HitProxyIds.Last();

		for (uint32 i = IdCount; i < BufferCount; ++i)
		{
			Data[i] = FillValue;
		}
	}
	DynamicReadBuffer.Unlock();
}

#endif

void FSceneRenderer::ComputeViewVisibility(
	FRHICommandListImmediate& RHICmdList,
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess,
	FViewVisibleCommandsPerView& ViewCommandsPerView, 
	FGlobalDynamicIndexBuffer& DynamicIndexBuffer,
	FGlobalDynamicVertexBuffer& DynamicVertexBuffer,
	FGlobalDynamicReadBuffer& DynamicReadBuffer, 
	FInstanceCullingManager& InstanceCullingManager)
{
	SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime);
	SCOPED_NAMED_EVENT(FSceneRenderer_ComputeViewVisibility, FColor::Magenta);

	STAT(int32 NumProcessedPrimitives = 0);
	STAT(int32 NumCulledPrimitives = 0);
	STAT(int32 NumOccludedPrimitives = 0);

	/**
	  * UpdateStaticMeshes removes and re-creates cached FMeshDrawCommands.  If there are multiple scene renderers being run together,
	  * we need allocated pipeline state IDs not to change, in case async tasks related to prior scene renderers are still in flight
	  * (FSubmitNaniteMaterialPassCommandsAnyThreadTask or FDrawVisibleMeshCommandsAnyThreadTask).  So we freeze pipeline state IDs,
	  * preventing them from being de-allocated even if their reference count temporarily goes to zero during calls to
	  * RemoveCachedMeshDrawCommands followed by CacheMeshDrawCommands (or the Nanite equivalent).
	  *
	  * Note that on the first scene renderer, we do want to de-allocate items, so they can be permanently released if no longer in use
	  * (for example, if there was an impactful change to a render proxy by game logic), but the assumption is that sequential renders
	  * of the same scene from different views can't make such changes.
	  */
	if (!bIsFirstSceneRenderer)
	{
		FGraphicsMinimalPipelineStateId::FreezeIdTable(true);
	}

	UE::Tasks::FTask ComputeLightVisibilityTask = LaunchSceneRenderTask(UE_SOURCE_LOCATION, [this]
	{
		ComputeLightVisibility();
	});

	int32 NumPrimitives = Scene->Primitives.Num();
	float CurrentRealTime = ViewFamily.Time.GetRealTimeSeconds();

	FPrimitiveViewMasks HasDynamicMeshElementsMasks;
	HasDynamicMeshElementsMasks.AddZeroed(NumPrimitives);

	FPrimitiveViewMasks HasDynamicEditorMeshElementsMasks;

	if (GIsEditor)
	{
		HasDynamicEditorMeshElementsMasks.AddZeroed(NumPrimitives);
	}

	UpdateReflectionSceneData(Scene);

	uint8 ViewBit = 0x1;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FSceneRenderer_Views);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex, ViewBit <<= 1)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("View %d"), ViewIndex));
			STAT(NumProcessedPrimitives += NumPrimitives);

			FViewInfo& View = Views[ViewIndex];
			FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];
			FSceneViewState* ViewState = (FSceneViewState*)View.State;

			const bool bIsSinglePassStereo = View.bIsInstancedStereoEnabled || View.bIsMobileMultiViewEnabled;

			// Allocate the view's visibility maps.
			View.PrimitiveVisibilityMap.Init(false, Scene->Primitives.Num());
			View.PrimitiveRayTracingVisibilityMap.Init(false, Scene->Primitives.Num());

			// These are not initialized here, as we overwrite the whole array in GatherDynamicMeshElements().
			View.DynamicMeshEndIndices.SetNumUninitialized(Scene->Primitives.Num());
			View.PrimitiveDefinitelyUnoccludedMap.Init(false, Scene->Primitives.Num());
			View.PotentiallyFadingPrimitiveMap.Init(false, Scene->Primitives.Num());
			View.PrimitiveFadeUniformBuffers.AddZeroed(Scene->Primitives.Num());
			View.PrimitiveFadeUniformBufferMap.Init(false, Scene->Primitives.Num());
			View.StaticMeshVisibilityMap.Init(false, Scene->StaticMeshes.GetMaxIndex());
			View.StaticMeshFadeOutDitheredLODMap.Init(false, Scene->StaticMeshes.GetMaxIndex());
			View.StaticMeshFadeInDitheredLODMap.Init(false, Scene->StaticMeshes.GetMaxIndex());
			View.PrimitivesLODMask.Init(FLODMask(), Scene->Primitives.Num());

			// The dirty list allocation must take into account the max possible size because when GILCUpdatePrimTaskEnabled is true,
			// the indirect lighting cache will be update on by threaded job, which can not do reallocs on the buffer (since it uses the SceneRenderingAllocator).
			View.DirtyIndirectLightingCacheBufferPrimitives.Reserve(Scene->Primitives.Num());

			View.PrimitiveViewRelevanceMap.Reset(Scene->Primitives.Num());
			View.PrimitiveViewRelevanceMap.AddZeroed(Scene->Primitives.Num());

			// If this is the visibility-parent of other views, reset its ParentPrimitives list.
			const bool bIsParent = ViewState && ViewState->IsViewParent();
			if (bIsParent)
			{
				// PVS-Studio does not understand the validation of ViewState above, so we're disabling
				// its warning that ViewState may be null:
				ViewState->ParentPrimitives.Reset(); //-V595
			}

			if (ViewState)
			{
				SCOPE_CYCLE_COUNTER(STAT_DecompressPrecomputedOcclusion);
				View.PrecomputedVisibilityData = ViewState->GetPrecomputedVisibilityData(View, Scene);
			}
			else
			{
				View.PrecomputedVisibilityData = nullptr;
			}

			if (View.PrecomputedVisibilityData)
			{
				bUsedPrecomputedVisibility = true;
			}

			bool bNeedsFrustumCulling = CVarEnableFrustumCull.GetValueOnRenderThread();

			// Development builds sometimes override frustum culling, e.g. dependent views in the editor.
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (ViewState)
			{
#if WITH_EDITOR
				// For visibility child views, check if the primitive was visible in the parent view.
				const FSceneViewState* const ViewParent = (FSceneViewState*)ViewState->GetViewParent();
				if (ViewParent)
				{
					bNeedsFrustumCulling = false;
					for (int32 Index = 0; Index < View.PrimitiveVisibilityMap.Num(); ++Index)
					{
						if (ViewParent->ParentPrimitives.Contains(Scene->PrimitiveComponentIds[Index]) || 
							IsAlwaysVisible(Scene, Index))
						{
							View.PrimitiveVisibilityMap[Index] = true;
						}
					}
				}
#endif
				// For views with frozen visibility, check if the primitive is in the frozen visibility set.
				if (ViewState->bIsFrozen)
				{
					bNeedsFrustumCulling = false;
					for (int32 Index = 0; Index < View.PrimitiveVisibilityMap.Num(); ++Index)
					{
						if (ViewState->FrozenPrimitives.Contains(Scene->PrimitiveComponentIds[Index]) ||
							IsAlwaysVisible(Scene, Index))
						{
							View.PrimitiveVisibilityMap[Index] = true;
						}
					}
				}
			}
#endif

			// Most views use standard frustum culling.
			if(bNeedsFrustumCulling)
			{
				// Update HLOD transition/visibility states to allow use during distance culling
				FLODSceneTree& HLODTree = Scene->SceneLODHierarchy;
				if (HLODTree.IsActive())
				{
					QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_HLODUpdate);
					HLODTree.UpdateVisibilityStates(View);
				}
				else
				{
					HLODTree.ClearVisibilityState(View);
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FSceneRenderer_Cull);
#if RHI_RAYTRACING
				if (bAnyRayTracingPassEnabled)
				{
					// The logic inside PrimitiveCull makes use of ShouldCullForRayTracing to decide if the primitive should be considered for raytracing
					// Therefore we must be sure we are done with caching the mesh draw commands (which include raytracing caches) before it runs if raytracing is being used.
					Scene->WaitForCacheMeshDrawCommandsTask();
				}
#endif
				int32 NumCulledPrimitivesForView = PrimitiveCull(Scene, View, bNeedsFrustumCulling);
				STAT(NumCulledPrimitives += NumCulledPrimitivesForView);
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FSceneRenderer_UpdatePrimitiveFading);
				UpdatePrimitiveFading(Scene, View);
			}

			if (View.ShowOnlyPrimitives.IsSet())
			{
				View.bHasNoVisiblePrimitive = View.ShowOnlyPrimitives->Num() == 0;
			}

			if (View.bStaticSceneOnly)
			{
				for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					// Reflection captures should only capture objects that won't move, since reflection captures won't update at runtime
					if (!Scene->Primitives[BitIt.GetIndex()]->Proxy->HasStaticLighting())
					{
						View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					}
				}
			}

			// Cull small objects in wireframe in ortho views
			// This is important for performance in the editor because wireframe disables any kind of occlusion culling
			if (View.Family->EngineShowFlags.Wireframe)
			{
				float ScreenSizeScale = FMath::Max(View.ViewMatrices.GetProjectionMatrix().M[0][0] * View.ViewRect.Width(), View.ViewMatrices.GetProjectionMatrix().M[1][1] * View.ViewRect.Height());
				for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					if (ScreenSizeScale * Scene->PrimitiveBounds[BitIt.GetIndex()].BoxSphereBounds.SphereRadius <= GWireframeCullThreshold)
					{
						View.PrimitiveVisibilityMap.AccessCorrespondingBit(BitIt) = false;
					}
				}
			}

			// Occlusion cull for all primitives in the view frustum, but not in wireframe.
			if (!View.Family->EngineShowFlags.Wireframe)
			{
				int32 NumOccludedPrimitivesInView = OcclusionCull(RHICmdList, Scene, View, DynamicVertexBuffer);
				STAT(NumOccludedPrimitives += NumOccludedPrimitivesInView);
			}

			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_ViewVisibilityTime_ConditionalUpdateStaticMeshes);
				SCOPED_NAMED_EVENT(FSceneRenderer_UpdateStaticMeshes, FColor::Red);

				Scene->WaitForCacheMeshDrawCommandsTask();
				Scene->ConditionalMarkStaticMeshElementsForUpdate();

				TArray<FPrimitiveSceneInfo*> AddedSceneInfos;
				for (TSet<FPrimitiveSceneInfo*>::TIterator It(Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck); It; ++It)
				{
					FPrimitiveSceneInfo* Primitive = *It;
					if (Primitive->NeedsUpdateStaticMeshes())
					{
						AddedSceneInfos.Add(Primitive);
					}
				}

				for (TConstDualSetBitIterator<SceneRenderingBitArrayAllocator, FDefaultBitArrayAllocator> BitIt(View.PrimitiveVisibilityMap, Scene->PrimitivesNeedingStaticMeshUpdate); BitIt; ++BitIt)
				{
					int32 PrimitiveIndex = BitIt.GetIndex();
					FPrimitiveSceneInfo* SceneInfo = Scene->Primitives[PrimitiveIndex];

					if (!Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Contains(SceneInfo))
					{
						AddedSceneInfos.Add(Scene->Primitives[PrimitiveIndex]);
					}
				}

				if (AddedSceneInfos.Num() > 0)
				{
					FPrimitiveSceneInfo::UpdateStaticMeshes(Scene, AddedSceneInfos, EUpdateStaticMeshFlags::AllCommands);
				}

				Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Reset();
			}

			// Single-pass stereo views can't compute relevance until all views are visibility culled
			if (!bIsSinglePassStereo)
			{
				SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
				ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewCommands, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks);
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// Store the primitive for parent occlusion rendering.
			if (FPlatformProperties::SupportsWindowedMode() && ViewState && ViewState->IsViewParent())
			{
				for (FSceneDualSetBitIterator BitIt(View.PrimitiveVisibilityMap, View.PrimitiveDefinitelyUnoccludedMap); BitIt; ++BitIt)
				{
					ViewState->ParentPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				}
			}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// if we are freezing the scene, then remember the primitives that are rendered.
			if (ViewState && ViewState->bIsFreezing)
			{
				for (FSceneSetBitIterator BitIt(View.PrimitiveVisibilityMap); BitIt; ++BitIt)
				{
					ViewState->FrozenPrimitives.Add(Scene->PrimitiveComponentIds[BitIt.GetIndex()]);
				}
			}
#endif

			// TODO: right now decals visibility computed right before rendering them, ideally it should be done in InitViews and this flag should be replaced with list of visible decals  
			// Currently used to disable stencil operations in forward base pass when scene has no any decals
			View.bSceneHasDecals = (Scene->Decals.Num() > 0) || (GForceSceneHasDecals != 0);

			if (bIsSinglePassStereo && IStereoRendering::IsASecondaryView(View) && Views.IsValidIndex(View.PrimaryViewIndex))
			{
				// Ensure primitives from the secondary view are visible in the primary view
				FSceneBitArray& PrimaryVis = Views[View.PrimaryViewIndex].PrimitiveVisibilityMap;
				const FSceneBitArray& SecondaryVis = View.PrimitiveVisibilityMap;

				check(PrimaryVis.Num() == SecondaryVis.Num());

				const uint32 NumWords = FMath::DivideAndRoundUp(PrimaryVis.Num(), NumBitsPerDWORD);
				uint32* const PrimaryData = PrimaryVis.GetData();
				const uint32* const SecondaryData = SecondaryVis.GetData();

				for (uint32 Index = 0; Index < NumWords; ++Index)
				{
					PrimaryData[Index] |= SecondaryData[Index];
				}
			}
		}
	}
	
	ViewBit = 0x1;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex, ViewBit <<= 1)
	{
		FViewInfo& View = Views[ViewIndex];
		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];
		
		if (View.bIsInstancedStereoEnabled || View.bIsMobileMultiViewEnabled)
		{
			SCOPE_CYCLE_COUNTER(STAT_ViewRelevance);
			ComputeAndMarkRelevanceForViewParallel(RHICmdList, Scene, View, ViewCommands, ViewBit, HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks);
		}
	}

	ComputeLightVisibilityTask.Wait();
	Scene->WaitForCreateLightPrimitiveInteractionsTask();

	PreGatherDynamicMeshElements();

	{
		SCOPED_NAMED_EVENT(FSceneRenderer_GatherDynamicMeshElements, FColor::Yellow);
		// Gather FMeshBatches from scene proxies
		GatherDynamicMeshElements(Views, Scene, ViewFamily, DynamicIndexBuffer, DynamicVertexBuffer, DynamicReadBuffer,
			HasDynamicMeshElementsMasks, HasDynamicEditorMeshElementsMasks, MeshCollector);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

	#if WITH_EDITOR
		UpdateHitProxyIdBuffer(View.EditorSelectedHitProxyIds, View.EditorSelectedBuffer);
		UpdateHitProxyIdBuffer(View.EditorVisualizeLevelInstanceIds, View.EditorVisualizeLevelInstanceBuffer);
	#endif

		if (!View.ShouldRenderView())
		{
			continue;
		}

		FViewCommands& ViewCommands = ViewCommandsPerView[ViewIndex];

#if !UE_BUILD_SHIPPING
		DumpPrimitives(ViewCommands);
		ProcessPrimitives(View, ViewCommands);
#endif

		SetupMeshPass(View, BasePassDepthStencilAccess, ViewCommands, InstanceCullingManager);
	}

	INC_DWORD_STAT_BY(STAT_ProcessedPrimitives,NumProcessedPrimitives);
	INC_DWORD_STAT_BY(STAT_CulledPrimitives,NumCulledPrimitives);
	INC_DWORD_STAT_BY(STAT_OccludedPrimitives,NumOccludedPrimitives);

	// See comment where this is called above
	if (!bIsFirstSceneRenderer)
	{
		FGraphicsMinimalPipelineStateId::FreezeIdTable(false);
	}
}

void FDeferredShadingSceneRenderer::ComputeLightVisibility()
{
	FSceneRenderer::ComputeLightVisibility();

	CreateIndirectCapsuleShadows();

	SetupVolumetricFog();
}

void FSceneRenderer::ComputeLightVisibility()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Light_Visibility);

	VisibleLightInfos.AddDefaulted(Scene->Lights.GetMaxIndex());

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		View.VisibleLightInfos.Empty(Scene->Lights.GetMaxIndex());

		for (int32 LightIndex = 0; LightIndex < Scene->Lights.GetMaxIndex(); LightIndex++)
		{
			new (View.VisibleLightInfos) FVisibleLightViewInfo();
		}
	}

	const bool bSetupMobileLightShafts = FeatureLevel <= ERHIFeatureLevel::ES3_1 && ShouldRenderLightShafts(ViewFamily);

	// determine visibility of each light
	for(auto LightIt = Scene->Lights.CreateConstIterator();LightIt;++LightIt)
	{
		const FLightSceneInfoCompact& LightSceneInfoCompact = *LightIt;
		const FLightSceneInfo* LightSceneInfo = LightSceneInfoCompact.LightSceneInfo;

		// view frustum cull lights in each view
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{		
			const FLightSceneProxy* Proxy = LightSceneInfo->Proxy;
			FViewInfo& View = Views[ViewIndex];
			FVisibleLightViewInfo& VisibleLightViewInfo = View.VisibleLightInfos[LightIt.GetIndex()];
			// dir lights are always visible, and point/spot only if in the frustum
			if( Proxy->GetLightType() == LightType_Point ||
				Proxy->GetLightType() == LightType_Spot ||
				Proxy->GetLightType() == LightType_Rect )
			{
				const FSphere& BoundingSphere = Proxy->GetBoundingSphere();
				const bool bInViewFrustum = View.ViewFrustum.IntersectSphere(BoundingSphere.Center, BoundingSphere.W);

				if (View.IsPerspectiveProjection())
				{
					const float DistanceSquared = (BoundingSphere.Center - View.ViewMatrices.GetViewOrigin()).SizeSquared();
					const float MaxDistSquared = Proxy->GetMaxDrawDistance() * Proxy->GetMaxDrawDistance() * GLightMaxDrawDistanceScale * GLightMaxDrawDistanceScale;
					const bool bDrawLight = (FMath::Square(FMath::Min(0.0002f, GMinScreenRadiusForLights / BoundingSphere.W) * View.LODDistanceFactor) * DistanceSquared < 1.0f)
												&& (MaxDistSquared == 0 || DistanceSquared < MaxDistSquared);
							
					VisibleLightViewInfo.bInViewFrustum = bDrawLight && bInViewFrustum;
					VisibleLightViewInfo.bInDrawRange = bDrawLight;
				}
				else
				{
					VisibleLightViewInfo.bInViewFrustum = bInViewFrustum;
					VisibleLightViewInfo.bInDrawRange = true;
				}
			}
			else
			{
				VisibleLightViewInfo.bInViewFrustum = true;
				VisibleLightViewInfo.bInDrawRange = true;
				// Setup single sun-shaft from direction lights for mobile.
				if (bSetupMobileLightShafts && LightSceneInfo->bEnableLightShaftBloom && ShouldRenderLightShaftsForLight(View, *LightSceneInfo->Proxy))
				{
					View.MobileLightShaft = GetMobileLightShaftInfo(View, *LightSceneInfo);
				}
			}

			// Draw shapes for reflection captures
			if( View.bIsReflectionCapture 
				&& VisibleLightViewInfo.bInViewFrustum
				&& Proxy->HasStaticLighting() 
				&& Proxy->GetLightType() != LightType_Directional )
			{
				FVector Origin = Proxy->GetOrigin();
				FVector ToLight = Origin - View.ViewMatrices.GetViewOrigin();
				float DistanceSqr = ToLight | ToLight;
				float Radius = Proxy->GetRadius();

				if( DistanceSqr < Radius * Radius )
				{
					View.VisibleReflectionCaptureLights.Emplace(Proxy);
				}
			}
		}
	}

	InitFogConstants();
}

void FSceneRenderer::GatherReflectionCaptureLightMeshElements()
{
	// view frustum cull lights in each view
	for (FViewInfo& View : Views)
	{
		for (const FLightSceneProxy* Proxy : View.VisibleReflectionCaptureLights)
		{
			FVector Origin = Proxy->GetOrigin();
			FVector ToLight = Origin - View.ViewMatrices.GetViewOrigin();
			float DistanceSqr = ToLight | ToLight;
			float Radius = Proxy->GetRadius();

			FLightRenderParameters LightParameters;
			Proxy->GetLightShaderParameters(LightParameters);

			// Force to be at least 0.75 pixels
			float CubemapSize = (float)IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ReflectionCaptureResolution"))->GetValueOnAnyThread();
			float Distance = FMath::Sqrt(DistanceSqr);
			float MinRadius = Distance * 0.75f / CubemapSize;
			LightParameters.SourceRadius = FMath::Max(MinRadius, LightParameters.SourceRadius);

			// Snap to cubemap pixel center to reduce aliasing
			FVector Scale = ToLight.GetAbs();
			int32 MaxComponent = Scale.X > Scale.Y ? (Scale.X > Scale.Z ? 0 : 2) : (Scale.Y > Scale.Z ? 1 : 2);
			for (int32 k = 1; k < 3; k++)
			{
				float Projected = ToLight[(MaxComponent + k) % 3] / Scale[MaxComponent];
				float Quantized = (FMath::RoundToFloat(Projected * (0.5f * CubemapSize) - 0.5f) + 0.5f) / (0.5f * CubemapSize);
				ToLight[(MaxComponent + k) % 3] = Quantized * Scale[MaxComponent];
			}
			Origin = ToLight + View.ViewMatrices.GetViewOrigin();

			FLinearColor Color(LightParameters.Color.R, LightParameters.Color.G, LightParameters.Color.B, LightParameters.FalloffExponent);
			const bool bIsRectLight = Proxy->IsRectLight();
			if (!bIsRectLight)
			{
				const float SphereArea = (4.0f * PI) * FMath::Square(LightParameters.SourceRadius);
				const float CylinderArea = (2.0f * PI) * LightParameters.SourceRadius * LightParameters.SourceLength;
				const float SurfaceArea = SphereArea + CylinderArea;
				Color *= 4.0f / SurfaceArea;
			}

			if (Proxy->IsInverseSquared())
			{
				float LightRadiusMask = FMath::Square(1.0f - FMath::Square(DistanceSqr * FMath::Square(LightParameters.InvRadius)));
				Color.A = LightRadiusMask;
			}
			else
			{
				// Remove inverse square falloff
				Color *= DistanceSqr + 1.0f;

				// Apply falloff
				Color.A = FMath::Pow(1.0f - DistanceSqr * FMath::Square(LightParameters.InvRadius), LightParameters.FalloffExponent);
			}

			// Spot falloff
			FVector L = ToLight.GetSafeNormal();
			Color.A *= FMath::Square(FMath::Clamp(((L | (FVector)LightParameters.Direction) - LightParameters.SpotAngles.X) * LightParameters.SpotAngles.Y, 0.0f, 1.0f));

			Color.A *= LightParameters.SpecularScale;

			// Rect is one sided
			if (bIsRectLight && (L | (FVector)LightParameters.Direction) < 0.0f)
				continue;

			UTexture* SurfaceTexture = nullptr;
			if (bIsRectLight)
			{
				const FRectLightSceneProxy* RectLightProxy = (const FRectLightSceneProxy*)Proxy;
				SurfaceTexture = RectLightProxy->SourceTexture;
			}

			FMaterialRenderProxy* ColoredMeshInstance = nullptr;
			if (SurfaceTexture)
			{
				ColoredMeshInstance = Allocator.Create<FColoredTexturedMaterialRenderProxy>(GEngine->EmissiveMeshMaterial->GetRenderProxy(), Color, NAME_Color, SurfaceTexture, NAME_LinearColor);
			}
			else
			{
				ColoredMeshInstance = Allocator.Create<FColoredMaterialRenderProxy>(GEngine->EmissiveMeshMaterial->GetRenderProxy(), Color, NAME_Color);
			}

			FMatrix LightToWorld = Proxy->GetLightToWorld();
			LightToWorld.RemoveScaling();

			FViewElementPDI LightPDI(&View, NULL, &View.DynamicPrimitiveCollector);

			if (bIsRectLight)
			{
				DrawBox(&LightPDI, LightToWorld, FVector(0.0f, LightParameters.SourceRadius, LightParameters.SourceLength), ColoredMeshInstance, SDPG_World);
			}
			else if (LightParameters.SourceLength > 0.0f)
			{
				DrawSphere(&LightPDI, Origin + 0.5f * LightParameters.SourceLength * LightToWorld.GetUnitAxis(EAxis::Z), FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World);
				DrawSphere(&LightPDI, Origin - 0.5f * LightParameters.SourceLength * LightToWorld.GetUnitAxis(EAxis::Z), FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World);
				DrawCylinder(&LightPDI, Origin, LightToWorld.GetUnitAxis(EAxis::X), LightToWorld.GetUnitAxis(EAxis::Y), LightToWorld.GetUnitAxis(EAxis::Z), LightParameters.SourceRadius, 0.5f * LightParameters.SourceLength, 36, ColoredMeshInstance, SDPG_World);
			}
			else
			{
				DrawSphere(&LightPDI, Origin, FRotator::ZeroRotator, LightParameters.SourceRadius * FVector::OneVector, 36, 24, ColoredMeshInstance, SDPG_World);
			}
		}

		View.VisibleReflectionCaptureLights.Empty();
	}
}

void FSceneRenderer::PostVisibilityFrameSetup(FILCUpdatePrimTaskData& OutILCTaskData)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_Sort);
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{		
			FViewInfo& View = Views[ViewIndex];

			View.MeshDecalBatches.Sort();

			if (View.State)
			{
				((FSceneViewState*)View.State)->TrimHistoryRenderTargets(Scene);
			}
		}
	}

	GatherReflectionCaptureLightMeshElements();

	if (ViewFamily.EngineShowFlags.HitProxies == 0 && Scene->PrecomputedLightVolumes.Num() > 0
		&& GILCUpdatePrimTaskEnabled && FPlatformProcess::SupportsMultithreading())
	{
		Scene->IndirectLightingCache.StartUpdateCachePrimitivesTask(Scene, *this, true, OutILCTaskData);
	}
}

uint32 GetShadowQuality();
void UpdateHairResources(FRDGBuilder& GraphBuilder, const FViewInfo& View);

/** 
* Performs once per frame setup prior to visibility determination.
*/
void FDeferredShadingSceneRenderer::PreVisibilityFrameSetup(FRDGBuilder& GraphBuilder, const FSceneTexturesConfig& SceneTexturesConfig)
{
	// Possible stencil dither optimization approach
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		View.bAllowStencilDither = DepthPass.bDitheredLODTransitionsUseStencil;
	}

	FSceneRenderer::PreVisibilityFrameSetup(GraphBuilder, SceneTexturesConfig);
}

/**
 * Initialize scene's views.
 * Check visibility, build visible mesh commands, etc.
 */
void FDeferredShadingSceneRenderer::BeginInitViews(FRDGBuilder& GraphBuilder, const FSceneTexturesConfig& SceneTexturesConfig, FExclusiveDepthStencil::Type BasePassDepthStencilAccess, struct FILCUpdatePrimTaskData& ILCTaskData, FInstanceCullingManager& InstanceCullingManager)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViews, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsTime);
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, InitViews_Scene);

	PreVisibilityFrameSetup(GraphBuilder, SceneTexturesConfig);

	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	// Create GPU-side representation of the view for instance culling.
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		Views[ViewIndex].GPUSceneViewId = InstanceCullingManager.RegisterView(Views[ViewIndex]);
	}

	{
		// This is to init the ViewUniformBuffer before rendering for the Niagara compute shader.
		// This needs to run before ComputeViewVisibility() is called, but the views normally initialize the ViewUniformBuffer after that (at the end of this method).
		if (FXSystem && FXSystem->RequiresEarlyViewUniformBuffer() && Views.IsValidIndex(0))
		{
			// during ISR, instanced view RHI resources need to be initialized first.
			if (FViewInfo* InstancedView = const_cast<FViewInfo*>(Views[0].GetInstancedView()))
			{
				InstancedView->InitRHIResources();
			}
			Views[0].InitRHIResources();
			FXSystem->PostInitViews(GraphBuilder, Views, !ViewFamily.EngineShowFlags.HitProxies);
		}
	}

	LumenScenePDIVisualization();
	
	FViewVisibleCommandsPerView ViewCommandsPerView;
	ViewCommandsPerView.SetNum(Views.Num());

	ComputeViewVisibility(RHICmdList, BasePassDepthStencilAccess, ViewCommandsPerView, DynamicIndexBufferForInitViews, DynamicVertexBufferForInitViews, DynamicReadBufferForInitViews, InstanceCullingManager);

	// This must happen before we start initialising and using views.
	if (Scene)
	{
		UpdateSkyIrradianceGpuBuffer(GraphBuilder, ViewFamily.EngineShowFlags, Scene->SkyLight, Scene->SkyIrradianceEnvironmentMap);
	}

	// Initialise Sky/View resources before the view global uniform buffer is built.
	if (ShouldRenderSkyAtmosphere(Scene, ViewFamily.EngineShowFlags))
	{
		InitSkyAtmosphereForViews(RHICmdList);
	}

	PostVisibilityFrameSetup(ILCTaskData);

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_InitRHIResources);
		// initialize per-view uniform buffer. Do it from back to front because secondary stereo view follows its primary one, but primary needs to know the instanced's params
		for (int32 ViewIndex = Views.Num() - 1; ViewIndex >= 0; --ViewIndex)
		{
			FViewInfo& View = Views[ViewIndex];
			// Set the pre-exposure before initializing the constant buffers.
			if (View.ViewState)
			{
				View.ViewState->UpdatePreExposure(View);
			}

			// Initialize the view's RHI resources.
			UpdateHairResources(GraphBuilder, View);
			View.InitRHIResources();
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_OnStartRender);
		OnStartRender(RHICmdList);
	}

	if (GDynamicRHI->RHIIncludeOptionalFlushes())
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
	}

	WaitForPrepareDynamicShadowsTask(CurrentDynamicShadowsTaskData);
}

template<class T>
void CreateReflectionCaptureUniformBuffer(const TArray<FReflectionCaptureSortData>& SortedCaptures, TUniformBufferRef<T>& OutReflectionCaptureUniformBuffer)
{
	T SamplePositionsBuffer;
	for (int32 CaptureIndex = 0; CaptureIndex < SortedCaptures.Num(); CaptureIndex++)
	{
		SamplePositionsBuffer.PositionAndRadius[CaptureIndex] = FVector4f(SortedCaptures[CaptureIndex].RelativePosition, SortedCaptures[CaptureIndex].Radius);
		SamplePositionsBuffer.TilePosition[CaptureIndex] = FVector4f(SortedCaptures[CaptureIndex].TilePosition, 0);

		SamplePositionsBuffer.CaptureProperties[CaptureIndex] = SortedCaptures[CaptureIndex].CaptureProperties;
		SamplePositionsBuffer.CaptureOffsetAndAverageBrightness[CaptureIndex] = SortedCaptures[CaptureIndex].CaptureOffsetAndAverageBrightness;
		SamplePositionsBuffer.BoxTransform[CaptureIndex] = SortedCaptures[CaptureIndex].BoxTransform;
		SamplePositionsBuffer.BoxScales[CaptureIndex] = SortedCaptures[CaptureIndex].BoxScales;
	}

	OutReflectionCaptureUniformBuffer = TUniformBufferRef<T>::CreateUniformBufferImmediate(SamplePositionsBuffer, UniformBuffer_SingleFrame);
}

void FSceneRenderer::SetupSceneReflectionCaptureBuffer(FRHICommandListImmediate& RHICmdList)
{
	const TArray<FReflectionCaptureSortData>& SortedCaptures = Scene->ReflectionSceneData.SortedCaptures;

	TUniformBufferRef<FMobileReflectionCaptureShaderData> MobileReflectionCaptureUniformBuffer;
	TUniformBufferRef<FReflectionCaptureShaderData> ReflectionCaptureUniformBuffer;

	if (IsMobilePlatform(ShaderPlatform))
	{
		CreateReflectionCaptureUniformBuffer(SortedCaptures, MobileReflectionCaptureUniformBuffer);
	}
	else
	{
		CreateReflectionCaptureUniformBuffer(SortedCaptures, ReflectionCaptureUniformBuffer);
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		if (IsMobilePlatform(ShaderPlatform))
		{
			View.MobileReflectionCaptureUniformBuffer = MobileReflectionCaptureUniformBuffer;
		}
		else
		{
			View.ReflectionCaptureUniformBuffer = ReflectionCaptureUniformBuffer;
		}
		
		View.NumBoxReflectionCaptures = 0;
		View.NumSphereReflectionCaptures = 0;
		View.FurthestReflectionCaptureDistance = 0.0f;

		if (View.Family->EngineShowFlags.ReflectionEnvironment 
			// Avoid feedback
			&& !View.bIsReflectionCapture)
		{
			View.NumBoxReflectionCaptures = Scene->ReflectionSceneData.NumBoxCaptures;
			View.NumSphereReflectionCaptures = Scene->ReflectionSceneData.NumSphereCaptures;

			for (int32 CaptureIndex = 0; CaptureIndex < SortedCaptures.Num(); CaptureIndex++)
			{
				FLargeWorldRenderPosition AbsolutePosition(SortedCaptures[CaptureIndex].TilePosition, SortedCaptures[CaptureIndex].RelativePosition);
				const FSphere BoundingSphere(AbsolutePosition.GetAbsolute(), SortedCaptures[CaptureIndex].Radius);

				const float Distance = View.ViewMatrices.GetViewMatrix().TransformPosition(BoundingSphere.Center).Z + BoundingSphere.W;

				View.FurthestReflectionCaptureDistance = FMath::Max(View.FurthestReflectionCaptureDistance, Distance);
			}
		}
	}
}

void FDeferredShadingSceneRenderer::EndInitViews(FRDGBuilder& GraphBuilder, FLumenSceneFrameTemporaries& FrameTemporaries, struct FILCUpdatePrimTaskData& ILCTaskData, FInstanceCullingManager& InstanceCullingManager, FRDGExternalAccessQueue& ExternalAccessQueue)
{
	SCOPED_NAMED_EVENT(FDeferredShadingSceneRenderer_InitViewsAfterPrepass, FColor::Emerald);
	SCOPE_CYCLE_COUNTER(STAT_InitViewsPossiblyAfterPrepass);

	FRHICommandListImmediate& RHICmdList = GraphBuilder.RHICmdList;

	const bool bHasRayTracedOverlay = HasRayTracedOverlay(ViewFamily);

	if (ViewFamily.EngineShowFlags.DynamicShadows 
		&& !ViewFamily.EngineShowFlags.HitProxies
		&& !bHasRayTracedOverlay)
	{
		// Setup dynamic shadows.
		if (CurrentDynamicShadowsTaskData)
		{
			FinishInitDynamicShadows(GraphBuilder, CurrentDynamicShadowsTaskData, DynamicIndexBufferForInitShadows, DynamicVertexBufferForInitShadows, DynamicReadBufferForInitShadows, InstanceCullingManager, ExternalAccessQueue);
			CurrentDynamicShadowsTaskData = nullptr;
		}
		else
		{
			InitDynamicShadows(GraphBuilder, DynamicIndexBufferForInitShadows, DynamicVertexBufferForInitShadows, DynamicReadBufferForInitShadows, InstanceCullingManager, ExternalAccessQueue);
		}

		if (GDynamicRHI->RHIIncludeOptionalFlushes())
		{
			RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
		}
	}

	// If parallel ILC update is disabled, then process it in place.
	if (ViewFamily.EngineShowFlags.HitProxies == 0
		&& Scene->PrecomputedLightVolumes.Num() > 0
		&& !(GILCUpdatePrimTaskEnabled && FPlatformProcess::SupportsMultithreading()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PostVisibilityFrameSetup_IndirectLightingCache_Update);
		check(!ILCTaskData.TaskRef.IsValid());
		Scene->IndirectLightingCache.UpdateCache(Scene, *this, true);
	}

	// If we kicked off ILC update via task, wait and finalize.
	if (ILCTaskData.TaskRef.IsValid())
	{
		Scene->IndirectLightingCache.FinalizeCacheUpdates(Scene, *this, ILCTaskData);
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_InitViews_UpdatePrimitiveIndirectLightingCacheBuffers);
		// Now that the indirect lighting cache is updated, we can update the primitive precomputed lighting buffers.
		UpdatePrimitiveIndirectLightingCacheBuffers();
	}

	SeparateTranslucencyDimensions = UpdateSeparateTranslucencyDimensions(*this);

	SetupSceneReflectionCaptureBuffer(RHICmdList);

	BeginUpdateLumenSceneTasks(GraphBuilder, FrameTemporaries);
}

/*------------------------------------------------------------------------------
	FLODSceneTree Implementation
------------------------------------------------------------------------------*/
void FLODSceneTree::AddChildNode(const FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (ParentId.IsValid() && ChildSceneInfo)
	{
		FLODSceneNode* Parent = SceneNodes.Find(ParentId);

		// If parent SceneNode hasn't been created yet (possible, depending on the order actors are added to the scene)
		if (!Parent)
		{
			// Create parent SceneNode, assign correct SceneInfo
			Parent = &SceneNodes.Add(ParentId, FLODSceneNode());

			int32 ParentIndex = Scene->PrimitiveComponentIds.Find(ParentId);
			if (ParentIndex != INDEX_NONE)
			{
				Parent->SceneInfo = Scene->Primitives[ParentIndex];
				check(Parent->SceneInfo->PrimitiveComponentId == ParentId);
			}
		}

		Parent->AddChild(ChildSceneInfo);
	}
}

void FLODSceneTree::RemoveChildNode(const FPrimitiveComponentId ParentId, FPrimitiveSceneInfo* ChildSceneInfo)
{
	if (ParentId.IsValid() && ChildSceneInfo)
	{
		if (FLODSceneNode* Parent = SceneNodes.Find(ParentId))
		{
			Parent->RemoveChild(ChildSceneInfo);

			// Delete from scene if no children remain
			if (Parent->ChildrenSceneInfos.Num() == 0)
			{
				SceneNodes.Remove(ParentId);
			}
		}
	}
}

void FLODSceneTree::UpdateNodeSceneInfo(FPrimitiveComponentId NodeId, FPrimitiveSceneInfo* SceneInfo)
{
	if (FLODSceneNode* Node = SceneNodes.Find(NodeId))
	{
		Node->SceneInfo = SceneInfo;
	}
}

void FLODSceneTree::ClearVisibilityState(FViewInfo& View)
{
	if (FSceneViewState* ViewState = (FSceneViewState*)View.State)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Skip update logic when frozen
		if (ViewState->bIsFrozen)
		{
			return;
		}
#endif
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;

		if(HLODState.IsValidPrimitiveIndex(0))
		{
			HLODState.PrimitiveFadingLODMap.Empty(0);
			HLODState.PrimitiveFadingOutLODMap.Empty(0);
			HLODState.ForcedVisiblePrimitiveMap.Empty(0);
			HLODState.ForcedHiddenPrimitiveMap.Empty(0);
		}

		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;

		if(VisibilityStates.Num() > 0)
		{
			VisibilityStates.Empty(0);
		}
	}
}

void FLODSceneTree::UpdateVisibilityStates(FViewInfo& View)
{
	if (FSceneViewState* ViewState = (FSceneViewState*)View.State)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Skip update logic when frozen
		if (ViewState->bIsFrozen)
		{
			return;
		}
#endif

		// Per-frame initialization
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;

		HLODState.PrimitiveFadingLODMap.Init(false, Scene->Primitives.Num());
		HLODState.PrimitiveFadingOutLODMap.Init(false, Scene->Primitives.Num());
		HLODState.ForcedVisiblePrimitiveMap.Init(false, Scene->Primitives.Num());
		HLODState.ForcedHiddenPrimitiveMap.Init(false, Scene->Primitives.Num());
		TArray<FPrimitiveViewRelevance, SceneRenderingAllocator>& RelevanceMap = View.PrimitiveViewRelevanceMap;

		if (HLODState.PrimitiveFadingLODMap.Num() != Scene->Primitives.Num())
		{
			checkf(0, TEXT("HLOD update incorrectly allocated primitive maps"));
			return;
		}		

		int32 UpdateCount = ++HLODState.UpdateCount;

		// Update persistent state on temporal dither sync frames
		const FTemporalLODState& LODState = ViewState->GetTemporalLODState();
		bool bSyncFrame = false;
		
		if (HLODState.TemporalLODSyncTime != LODState.TemporalLODTime[0])
		{
			HLODState.TemporalLODSyncTime = LODState.TemporalLODTime[0];
			bSyncFrame = true;

			// Only update our scaling on sync frames else we might end up changing transition direction mid-fade	
			const FCachedSystemScalabilityCVars& ScalabilityCVars = GetCachedScalabilityCVars();
			if (ScalabilityCVars.FieldOfViewAffectsHLOD)
			{
				HLODState.FOVDistanceScaleSq = ScalabilityCVars.CalculateFieldOfViewDistanceScale(View.DesiredFOV);
				HLODState.FOVDistanceScaleSq *= HLODState.FOVDistanceScaleSq;
			}
			else
			{
				HLODState.FOVDistanceScaleSq = 1.f;
			}
		}

		for (auto Iter = SceneNodes.CreateIterator(); Iter; ++Iter)
		{
			FLODSceneNode& Node = Iter.Value();
			FPrimitiveSceneInfo* SceneInfo = Node.SceneInfo;

			if (!SceneInfo || !SceneInfo->PrimitiveComponentId.IsValid() || !SceneInfo->IsIndexValid())
			{
				continue;
			}

			FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(SceneInfo->PrimitiveComponentId);
			const TArray<FStaticMeshBatchRelevance>& NodeMeshRelevances = SceneInfo->StaticMeshRelevances;

			// Ignore already updated nodes, or those that we can't work with
			if (NodeVisibility.UpdateCount == UpdateCount || !NodeMeshRelevances.IsValidIndex(0))
			{
				continue;
			}

			const int32 NodeIndex = SceneInfo->GetIndex();	

			if (!Scene->PrimitiveBounds.IsValidIndex(NodeIndex))
			{
				checkf(0, TEXT("A HLOD Node's PrimitiveSceneInfo PackedIndex was out of Scene.Primitive bounds!"));
				continue;
			}

			FPrimitiveBounds& Bounds = Scene->PrimitiveBounds[NodeIndex];
			const bool bForcedIntoView = FMath::IsNearlyZero(Bounds.MinDrawDistance);

			// Update visibility states of this node and owned children
			const float DistanceSquared = Bounds.BoxSphereBounds.ComputeSquaredDistanceFromBoxToPoint(View.ViewMatrices.GetViewOrigin());
			const bool bNearCulled = DistanceSquared < FMath::Square(Bounds.MinDrawDistance) * HLODState.FOVDistanceScaleSq;
			const bool bFarCulled = DistanceSquared > Bounds.MaxDrawDistance * Bounds.MaxDrawDistance * HLODState.FOVDistanceScaleSq;
			const bool bIsInDrawRange = !bNearCulled && !bFarCulled;

			const bool bWasFadingPreUpdate = !!NodeVisibility.bIsFading;
			const bool bIsDitheredTransition = NodeMeshRelevances[0].bDitheredLODTransition;

			if (bIsDitheredTransition && !bForcedIntoView)
			{		
				// Update fading state with syncs
				if (bSyncFrame)
				{
					// Fade when HLODs change threshold
					const bool bChangedRange = bIsInDrawRange != !!NodeVisibility.bWasVisible;

					if (NodeVisibility.bIsFading)
					{
						NodeVisibility.bIsFading = false;
					}
					else if (bChangedRange)
					{
						NodeVisibility.bIsFading = true;
					}

					NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
					NodeVisibility.bIsVisible = bIsInDrawRange;
				}
			}
			else
			{
				// Instant transitions without dithering
				NodeVisibility.bWasVisible = NodeVisibility.bIsVisible;
				NodeVisibility.bIsVisible = bIsInDrawRange || bForcedIntoView;
				NodeVisibility.bIsFading = false;
			}

			// Flush cached lighting data when changing visible contents
			if (NodeVisibility.bIsVisible != NodeVisibility.bWasVisible || bWasFadingPreUpdate || NodeVisibility.bIsFading)
			{
				FLightPrimitiveInteraction* NodeLightList = SceneInfo->LightList;
				while (NodeLightList)
				{
					NodeLightList->FlushCachedShadowMapData();
					NodeLightList = NodeLightList->GetNextLight();
				}
			}

			// Force fully disabled view relevance so shadows don't attempt to recompute
			if (!NodeVisibility.bIsVisible)
			{
				if (RelevanceMap.IsValidIndex(NodeIndex))
				{
					FPrimitiveViewRelevance& ViewRelevance = RelevanceMap[NodeIndex];
					FMemory::Memzero(&ViewRelevance, sizeof(FPrimitiveViewRelevance));
					ViewRelevance.bInitializedThisFrame = true;
				}
				else
				{
					checkf(0, TEXT("A HLOD Node's PrimitiveSceneInfo PackedIndex was out of View.Relevancy bounds!"));
				}
			}

			// NOTE: We update our children last as HideNodeChildren can add new visibility
			// states, potentially invalidating our cached reference above, NodeVisibility
			if (NodeVisibility.bIsFading)
			{
				// Fade until state back in sync
				HLODState.PrimitiveFadingLODMap[NodeIndex] = true;
				HLODState.PrimitiveFadingOutLODMap[NodeIndex] = !NodeVisibility.bIsVisible;
				HLODState.ForcedVisiblePrimitiveMap[NodeIndex] = true;
				ApplyNodeFadingToChildren(ViewState, Node, NodeVisibility, true, !!NodeVisibility.bIsVisible);
			}
			else if (NodeVisibility.bIsVisible)
			{
				// If stable and visible, override hierarchy visibility
				HLODState.ForcedVisiblePrimitiveMap[NodeIndex] = true;
				HideNodeChildren(ViewState, Node);
			}
			else
			{
				// Not visible and waiting for a transition to fade, keep HLOD hidden
				HLODState.ForcedHiddenPrimitiveMap[NodeIndex] = true;

				// Also hide children when performing far culling
				if (bFarCulled)
				{
					HideNodeChildren(ViewState, Node);
				}
			}
		}
	}	
}

void FLODSceneTree::ApplyNodeFadingToChildren(FSceneViewState* ViewState, FLODSceneNode& Node, FHLODSceneNodeVisibilityState& NodeVisibility, const bool bIsFading, const bool bIsFadingOut)
{
	checkSlow(ViewState);
	if (Node.SceneInfo)
	{
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		NodeVisibility.UpdateCount = HLODState.UpdateCount;

		// Force visibility during fades
		for (const auto Child : Node.ChildrenSceneInfos)
		{
			if (!Child || !Child->PrimitiveComponentId.IsValid() || !Child->IsIndexValid())
			{
				continue;
			}

			const int32 ChildIndex = Child->GetIndex();

			if (!HLODState.PrimitiveFadingLODMap.IsValidIndex(ChildIndex))
			{
				checkf(0, TEXT("A HLOD Child's PrimitiveSceneInfo PackedIndex was out of FadingMap's bounds!"));
				continue;
			}
		
			HLODState.PrimitiveFadingLODMap[ChildIndex] = bIsFading;
			HLODState.PrimitiveFadingOutLODMap[ChildIndex] = bIsFadingOut;
			HLODState.ForcedHiddenPrimitiveMap[ChildIndex] = false;

			if (bIsFading)
			{
				HLODState.ForcedVisiblePrimitiveMap[ChildIndex] = true;
			}

			// Fading only occurs at the adjacent hierarchy level, below should be hidden
			if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
			{
				HideNodeChildren(ViewState, *ChildNode);
			}
		}
	}
}

void FLODSceneTree::HideNodeChildren(FSceneViewState* ViewState, FLODSceneNode& Node)
{
	checkSlow(ViewState);
	if (Node.SceneInfo)
	{
		FHLODVisibilityState& HLODState = ViewState->HLODVisibilityState;
		TMap<FPrimitiveComponentId, FHLODSceneNodeVisibilityState>& VisibilityStates = ViewState->HLODSceneNodeVisibilityStates;
		FHLODSceneNodeVisibilityState& NodeVisibility = VisibilityStates.FindOrAdd(Node.SceneInfo->PrimitiveComponentId);

		if (NodeVisibility.UpdateCount != HLODState.UpdateCount)
		{
			NodeVisibility.UpdateCount = HLODState.UpdateCount;

			for (const auto Child : Node.ChildrenSceneInfos)
			{
				if (!Child || !Child->PrimitiveComponentId.IsValid() || !Child->IsIndexValid())
				{
					continue;
				}

				const int32 ChildIndex = Child->GetIndex();

				if (!HLODState.ForcedHiddenPrimitiveMap.IsValidIndex(ChildIndex))
				{
					checkf(0, TEXT("A HLOD Child's PrimitiveSceneInfo PackedIndex was out of ForcedHidden's bounds!"));
					continue;
				}

				HLODState.ForcedHiddenPrimitiveMap[ChildIndex] = true;
				
				// Clear the force visible flag in case the child was processed before it's parent
				HLODState.ForcedVisiblePrimitiveMap[ChildIndex] = false;

				if (FLODSceneNode* ChildNode = SceneNodes.Find(Child->PrimitiveComponentId))
				{
					HideNodeChildren(ViewState, *ChildNode);
				}
			}
		}
	}
}
