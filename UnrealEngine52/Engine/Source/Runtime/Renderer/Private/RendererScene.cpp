// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Scene.cpp: Scene manager implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Engine/Level.h"
#include "Engine/TextureLightProfile.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/PlatformFileManager.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Misc/PackageName.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "Components/ActorComponent.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "SceneTypes.h"
#include "SceneInterface.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsField/PhysicsFieldComponent.h"
#include "MaterialShared.h"
#include "SceneManagement.h"
#include "PrecomputedLightVolume.h"
#include "PrecomputedVolumetricLightmap.h"
#include "Components/LightComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Components/DecalComponent.h"
#include "Components/ReflectionCaptureComponent.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ScenePrivateBase.h"
#include "SceneCore.h"
#include "Rendering/MotionVectorSimulation.h"
#include "PrimitiveSceneInfo.h"
#include "LightSceneInfo.h"
#include "LightMapRendering.h"
#include "SkyAtmosphereRendering.h"
#include "BasePassRendering.h"
#include "MobileBasePassRendering.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "StaticMeshResources.h"
#include "ParameterCollection.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "DistanceFieldAtlas.h"
#include "EngineModule.h"
#include "FXSystem.h"
#include "DistanceFieldLightingShared.h"
#include "SpeedTreeWind.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Lumen/LumenSceneData.h"
#include "Lumen/LumenSceneRendering.h"
#include "PlanarReflectionSceneProxy.h"
#include "Engine/StaticMesh.h"
#include "GPUSkinCache.h"
#include "ComputeSystemInterface.h"
#include "DynamicShadowMapChannelBindingHelper.h"
#include "GPUScene.h"
#include "HAL/LowLevelMemTracker.h"
#include "VT/RuntimeVirtualTextureSceneProxy.h"
#include "HairStrandsInterface.h"
#include "VelocityRendering.h"
#include "RectLightSceneProxy.h"
#include "RectLightTextureManager.h"
#include "RenderCore.h"
#include "IESTextureManager.h"
#include "Materials/MaterialRenderProxy.h"

#if RHI_RAYTRACING
#include "Nanite/NaniteRayTracing.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RayTracingSkinnedGeometry.h"
#include "RayTracing/RayTracingScene.h"
#endif
#include "RHIGPUReadback.h"
#include "ShaderPrint.h"

#include "VirtualShadowMaps/VirtualShadowMapCacheManager.h"

#if WITH_EDITOR
#include "Rendering/StaticLightingSystemInterface.h"
#endif

#define VALIDATE_PRIMITIVE_PACKED_INDEX 0

/** Affects BasePassPixelShader.usf so must relaunch editor to recompile shaders. */
static TAutoConsoleVariable<int32> CVarEarlyZPassOnlyMaterialMasking(
	TEXT("r.EarlyZPassOnlyMaterialMasking"),
	0,
	TEXT("Whether to compute materials' mask opacity only in early Z pass. Changing this setting requires restarting the editor.\n")
	TEXT("Note: Needs r.EarlyZPass == 2 && r.EarlyZPassMovable == 1"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

/** Affects MobileBasePassPixelShader.usf so must relaunch editor to recompile shaders. */
static TAutoConsoleVariable<int32> CVarMobileEarlyZPassOnlyMaterialMasking(
	TEXT("r.Mobile.EarlyZPassOnlyMaterialMasking"),
	0,
	TEXT("Whether to compute materials' mask opacity only in early Z pass for Mobile platform. Changing this setting requires restarting the editor.\n")
	TEXT("<=0: off\n")
	TEXT("  1: on\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

TAutoConsoleVariable<int32> CVarEarlyZPass(
	TEXT("r.EarlyZPass"),
	3,
	TEXT("Whether to use a depth only pass to initialize Z culling for the base pass. Cannot be changed at runtime.\n")
	TEXT("Note: also look at r.EarlyZPassMovable\n")
	TEXT("  0: off\n")
	TEXT("  1: good occluders only: not masked, and large on screen\n")
	TEXT("  2: all opaque (including masked)\n")
	TEXT("  x: use built in heuristic (default is 3)"),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarMobileEarlyZPass(
	TEXT("r.Mobile.EarlyZPass"),
	0,
	TEXT("Whether to use a depth only pass to initialize Z culling for the mobile base pass.\n")
	TEXT("  0: off\n")
	TEXT("  1: all opaque \n"),
	ECVF_Scalability
);

static TAutoConsoleVariable<int32> CVarBasePassWriteDepthEvenWithFullPrepass(
	TEXT("r.BasePassWriteDepthEvenWithFullPrepass"),
	0,
	TEXT("0 to allow a readonly base pass, which skips an MSAA depth resolve, and allows masked materials to get EarlyZ (writing to depth while doing clip() disables EarlyZ) (default)\n")
	TEXT("1 to force depth writes in the base pass.  Useful for debugging when the prepass and base pass don't match what they render."));

DECLARE_CYCLE_STAT(TEXT("DeferredShadingSceneRenderer MotionBlurStartFrame"), STAT_FDeferredShadingSceneRenderer_MotionBlurStartFrame, STATGROUP_SceneRendering);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FDistanceCullFadeUniformShaderParameters, "PrimitiveFade");

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FDitherUniformShaderParameters, "PrimitiveDither");

/** Global primitive uniform buffer resource containing distance cull faded in */
TGlobalResource< FGlobalDistanceCullFadeUniformBuffer > GDistanceCullFadedInUniformBuffer;

/** Global primitive uniform buffer resource containing dither faded in */
TGlobalResource< FGlobalDitherUniformBuffer > GDitherFadedInUniformBuffer;

static FThreadSafeCounter FSceneViewState_UniqueID;

struct FUpdateLightTransformParameters
{
	FMatrix LightToWorld;
	FVector4 Position;
};

struct FUpdateLightCommand
{
	enum class EAddOrRemove
	{
		Add,
		Remove,
		None,
	};

	struct FColorParameters
	{
		FLinearColor NewColor;
		float NewIndirectLightingScale;
		float NewVolumetricScatteringIntensity;
	};

	using FTransformParameters = FUpdateLightTransformParameters;

	inline bool IsAdd() const { return AddOrRemove == EAddOrRemove::Add; }
	inline bool IsRemove() const { return AddOrRemove == EAddOrRemove::Remove; }

	void Set(const FColorParameters& InColorParameters)
	{
		bHasColor = true;
		ColorParameters = InColorParameters;
	}
	void Set(const FTransformParameters& InTransformParameters)
	{
		bHasTransform = true;
		TransformParameters = InTransformParameters;
	}
	void Set(const EAddOrRemove& InAddOrRemove)
	{
		AddOrRemove = InAddOrRemove;
	}

	FUpdateLightCommand(const FColorParameters& InColorParameters, FLightSceneInfo* InLightSceneInfo) : LightSceneInfo(InLightSceneInfo), AddOrRemove(EAddOrRemove::None), bHasTransform(false) { Set(InColorParameters); }
	FUpdateLightCommand(const FTransformParameters& InTransformParameters, FLightSceneInfo* InLightSceneInfo) : LightSceneInfo(InLightSceneInfo), AddOrRemove(EAddOrRemove::None), bHasColor(false) { Set(InTransformParameters); }
	// Crtor for add/re,pve
	FUpdateLightCommand(EAddOrRemove InAddOrRemove, FLightSceneInfo* InLightSceneInfo) : LightSceneInfo(InLightSceneInfo), AddOrRemove(InAddOrRemove), bHasTransform(false), bHasColor(false) {}

	FTransformParameters TransformParameters;
	FColorParameters ColorParameters;
	FLightSceneInfo* LightSceneInfo;
	EAddOrRemove AddOrRemove;
	uint32 bHasTransform : 1;
	uint32 bHasColor : 1;
};

class FSceneLightInfoUpdates
{
public:
	Experimental::TRobinHoodHashMap<FLightSceneInfo*, FUpdateLightCommand> Commands;

	int32 NumAdds = 0;
	int32 NumRemoves = 0;
	int32 NumUpdates = 0;

	template <typename PayloadT>
	void Enqueue(const PayloadT& Payload, FLightSceneInfo* LightSceneInfo)
	{
		// Allocate a new slot for update data
		bool bWasAlreadyInMap = false;
		FUpdateLightCommand* Command = Commands.FindOrAdd(LightSceneInfo, FUpdateLightCommand(Payload, LightSceneInfo), bWasAlreadyInMap);
		if (bWasAlreadyInMap)
		{
			check(LightSceneInfo == Command->LightSceneInfo);
			Command->Set(Payload);
		}
	}

	void Reset()
	{
		Commands.Empty();
		NumAdds = 0;
		NumRemoves = 0;
		NumUpdates = 0;
	}
};


#define ENABLE_LOG_PRIMITIVE_INSTANCE_ID_STATS_TO_CSV 0

#if ENABLE_LOG_PRIMITIVE_INSTANCE_ID_STATS_TO_CSV

int32 GDumpPrimitiveAllocatorStatsToCSV = 0;
FAutoConsoleVariableRef CVarDumpPrimitiveStatsToCSV(
	TEXT("r.DumpPrimitiveStatsToCSV"),
	GDumpPrimitiveAllocatorStatsToCSV,
	TEXT("Dump primitive and instance stats to CSV\n")
	TEXT(" 0 - stop recording, dump to csv and clear array.\n")
	TEXT(" 1 - start recording into array\n")
	TEXT(" 2 - dump to csv and continue recording without clearing array\n"),
	ECVF_RenderThreadSafe
);

static constexpr int32 StatStride = 8;
TArray<int32> GPrimitiveAllocatorStats;

void DumpPrimitiveAllocatorStats()
{
	if (!GPrimitiveAllocatorStats.IsEmpty())
	{
		FString FileName = FPaths::ProjectLogDir() / TEXT("PrimitiveStats-") + FDateTime::Now().ToString() + TEXT(".csv");

		UE_LOG(LogRenderer, Log, TEXT("Dumping primitive allocator stats trace to: '%s'"), *FileName);

		FArchive* FileToLogTo = IFileManager::Get().CreateFileWriter(*FileName, false);
		
		ensure(FileToLogTo);
		if (FileToLogTo)
		{
			static const FString StatNames[StatStride] =
			{
				TEXT("NumPrimitives"),
				TEXT("MaxPersistent"),
				TEXT("NumPersistentAllocated"),
				TEXT("PersistentFreeListSizeBC"),
				TEXT("PersistentPendingListSizeBC"),
				TEXT("PersistentFreeListSize"),
				TEXT("MaxInstances"),
				TEXT("NumInstancesAllocated"),
			};

			// Print header
			FString StringToPrint;
			for (int32 Index = 0; Index < StatStride; ++Index)
			{
				if (!StringToPrint.IsEmpty())
				{
					StringToPrint += TEXT(",");
				}
				if (Index < int32(UE_ARRAY_COUNT(StatNames)))
				{
					StringToPrint.Append(StatNames[Index]);
				}
				else
				{
					StringToPrint.Appendf(TEXT("Stat_%d"), Index);
				}
			}

			StringToPrint += TEXT("\n");
			FileToLogTo->Serialize(TCHAR_TO_ANSI(*StringToPrint), StringToPrint.Len());

			int32 Num = GPrimitiveAllocatorStats.Num() / StatStride;
			for (int32 Ind = 0; Ind < Num; ++Ind)
			{
				StringToPrint.Empty();

				for (int32 StatInd = 0; StatInd < StatStride; ++StatInd)
				{
					if (!StringToPrint.IsEmpty())
					{
						StringToPrint.Append(TEXT(","));
					}

					StringToPrint.Appendf(TEXT("%d"), GPrimitiveAllocatorStats[Ind * StatStride + StatInd]);
				}

				StringToPrint += TEXT("\n");
				FileToLogTo->Serialize(TCHAR_TO_ANSI(*StringToPrint), StringToPrint.Len());
			}


			FileToLogTo->Close();
		}
	}
}

void UpdatePrimitiveAllocatorStats(int32 NumPrimitives, int32 MaxPersistent, int32 NumPersistentAllocated, int32 PersistemFreeListSize, int32 PersistemFreeListSizeBC, int32 PersistentPendingListSizeBC, int32 MaxInstances, int32 NumInstancesAllocated)
{
	// Dump and clear when turning off the cvar
	if (GDumpPrimitiveAllocatorStatsToCSV == 0 && !GPrimitiveAllocatorStats.IsEmpty())
	{
		DumpPrimitiveAllocatorStats();
		GPrimitiveAllocatorStats.Empty();
	}

	// Dump snapshot (and don't clear or stop) when cvar is set to 2
	if (GDumpPrimitiveAllocatorStatsToCSV == 2 && !GPrimitiveAllocatorStats.IsEmpty())
	{
		DumpPrimitiveAllocatorStats();
		GDumpPrimitiveAllocatorStatsToCSV = 1;
	}

	if (GDumpPrimitiveAllocatorStatsToCSV != 0)
	{
		GPrimitiveAllocatorStats.Add(NumPrimitives);
		GPrimitiveAllocatorStats.Add(MaxPersistent);
		GPrimitiveAllocatorStats.Add(NumPersistentAllocated);
		GPrimitiveAllocatorStats.Add(PersistemFreeListSizeBC);
		GPrimitiveAllocatorStats.Add(PersistentPendingListSizeBC);
		GPrimitiveAllocatorStats.Add(PersistemFreeListSize);
		GPrimitiveAllocatorStats.Add(MaxInstances);
		GPrimitiveAllocatorStats.Add(NumInstancesAllocated);
	}
}
#endif // ENABLE_LOG_PRIMITIVE_INSTANCE_ID_STATS_TO_CSV

/**
 * Holds the info to update SpeedTree wind per unique tree object in the scene, instead of per instance
 */
struct FSpeedTreeWindComputation
{
	explicit FSpeedTreeWindComputation() :
		ReferenceCount(1)
	{
	}

	/** SpeedTree wind object */
	FSpeedTreeWind Wind;

	/** Uniform buffer shared between trees of the same type. */
	TUniformBufferRef<FSpeedTreeUniformParameters> UniformBuffer;

	int32 ReferenceCount;
};

FPersistentSkyAtmosphereData::FPersistentSkyAtmosphereData()
	: bInitialised(false)
	, CurrentScreenResolution(0)
	, CurrentDepthResolution(0)
	, CurrentTextureAerialLUTFormat(PF_Unknown)
	, CameraAerialPerspectiveVolumeIndex(0)
	, bSeparatedAtmosphereMieRayLeigh(false)
{
}
void FPersistentSkyAtmosphereData::InitialiseOrNextFrame(ERHIFeatureLevel::Type FeatureLevel, FPooledRenderTargetDesc& AerialPerspectiveDesc, FRHICommandListImmediate& RHICmdList, bool bSeparatedAtmosphereMieRayLeighIn)
{
	if (!bInitialised || (bInitialised && ((AerialPerspectiveDesc.Extent.X != CurrentScreenResolution) || (AerialPerspectiveDesc.Depth != CurrentDepthResolution) 
		|| (AerialPerspectiveDesc.Format != CurrentTextureAerialLUTFormat) || (bSeparatedAtmosphereMieRayLeigh != bSeparatedAtmosphereMieRayLeighIn))))
	{
		bSeparatedAtmosphereMieRayLeigh = bSeparatedAtmosphereMieRayLeighIn;
		CameraAerialPerspectiveVolumeCount = FeatureLevel == ERHIFeatureLevel::ES3_1 ? 2 : 1;
		for (int i = 0; i < CameraAerialPerspectiveVolumeCount; ++i)
		{
			GRenderTargetPool.FindFreeElement(RHICmdList, AerialPerspectiveDesc, CameraAerialPerspectiveVolumes[i], 
				i==0 ? TEXT("SkyAtmosphere.CameraAPVolume0") : TEXT("SkyAtmosphere.CameraAPVolume1"));
			if (bSeparatedAtmosphereMieRayLeigh)
			{
				GRenderTargetPool.FindFreeElement(RHICmdList, AerialPerspectiveDesc, CameraAerialPerspectiveVolumesMieOnly[i],
					i == 0 ? TEXT("SkyAtmosphere.CameraAPVolumeMieOnly0") : TEXT("SkyAtmosphere.CameraAPVolumeMieOnly1"));
				GRenderTargetPool.FindFreeElement(RHICmdList, AerialPerspectiveDesc, CameraAerialPerspectiveVolumesRayOnly[i],
					i == 0 ? TEXT("SkyAtmosphere.CameraAPVolumeRayOnly0") : TEXT("SkyAtmosphere.CameraAPVolumeRayOnly1"));
			}
			else
			{
				CameraAerialPerspectiveVolumesMieOnly[i] = nullptr;
				CameraAerialPerspectiveVolumesRayOnly[i] = nullptr;
			}
		}
		bInitialised = true;
		CurrentScreenResolution = AerialPerspectiveDesc.Extent.X;
		CurrentDepthResolution = AerialPerspectiveDesc.Depth;
		CurrentTextureAerialLUTFormat = AerialPerspectiveDesc.Format;
	}

	CameraAerialPerspectiveVolumeIndex = (CameraAerialPerspectiveVolumeIndex + 1) % CameraAerialPerspectiveVolumeCount;
}
TRefCountPtr<IPooledRenderTarget> FPersistentSkyAtmosphereData::GetCurrentCameraAerialPerspectiveVolume()
{
	check(CameraAerialPerspectiveVolumes[CameraAerialPerspectiveVolumeIndex].IsValid());
	return CameraAerialPerspectiveVolumes[CameraAerialPerspectiveVolumeIndex];
}
TRefCountPtr<IPooledRenderTarget> FPersistentSkyAtmosphereData::GetCurrentCameraAerialPerspectiveVolumeMieOnly()
{
	check(CameraAerialPerspectiveVolumesMieOnly[CameraAerialPerspectiveVolumeIndex].IsValid());
	return CameraAerialPerspectiveVolumesMieOnly[CameraAerialPerspectiveVolumeIndex];
}
TRefCountPtr<IPooledRenderTarget> FPersistentSkyAtmosphereData::GetCurrentCameraAerialPerspectiveVolumeRayOnly()
{
	check(CameraAerialPerspectiveVolumesRayOnly[CameraAerialPerspectiveVolumeIndex].IsValid());
	return CameraAerialPerspectiveVolumesRayOnly[CameraAerialPerspectiveVolumeIndex];
}

/** Default constructor. */
FSceneViewState::FSceneViewState(ERHIFeatureLevel::Type FeatureLevel, FSceneViewState* ShareOriginTarget)
	: OcclusionQueryPool(RHICreateRenderQueryPool(RQT_Occlusion))
{
	// Set FeatureLevel to a valid value, so we get Init/ReleaseDynamicRHI calls on FeatureLevel changes
	SetFeatureLevel(FeatureLevel);
	
	UniqueID = FSceneViewState_UniqueID.Increment();
	Scene = nullptr;
	OcclusionFrameCounter = 0;
	LastRenderTime = -FLT_MAX;
	LastRenderTimeDelta = 0.0f;
	MotionBlurTimeScale = 1.0f;
	MotionBlurTargetDeltaTime = 1.0f / 60.0f; // Start with a reasonable default of 60hz.
	PrevViewMatrixForOcclusionQuery.SetIdentity();
	PrevViewOriginForOcclusionQuery = FVector::ZeroVector;
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bIsFreezing = false;
	bIsFrozen = false;
	bIsFrozenViewMatricesCached = false;
#endif
	// Register this object as a resource, so it will receive device reset notifications.
	if ( IsInGameThread() )
	{
		BeginInitResource(this);
	}
	else
	{
		InitResource();
	}
	CachedVisibilityChunk = NULL;
	CachedVisibilityHandlerId = INDEX_NONE;
	CachedVisibilityBucketIndex = INDEX_NONE;
	CachedVisibilityChunkIndex = INDEX_NONE;
	MIDUsedCount = 0;
	TemporalAASampleIndex = 0;
	FrameIndex = 0;
	DistanceFieldTemporalSampleIndex = 0;
	bDOFHistory = true;
	bDOFHistory2 = true;
	
	// Sets the mipbias to invalid large number.
	MaterialTextureCachedMipBias = BIG_NUMBER;
	LandscapeCachedMipBias = BIG_NUMBER;

	SequencerState = ESS_None;

	bIsStereoView = false;

	bRoundRobinOcclusionEnabled = false;

	for (int32 CascadeIndex = 0; CascadeIndex < UE_ARRAY_COUNT(TranslucencyLightingCacheAllocations); CascadeIndex++)
	{
		TranslucencyLightingCacheAllocations[CascadeIndex] = NULL;
	}

	if (ShareOriginTarget)
	{
		GlobalDistanceFieldData = ShareOriginTarget->GlobalDistanceFieldData;
	}
	else
	{
		GlobalDistanceFieldData = new FPersistentGlobalDistanceFieldData;
	}

	ShadowOcclusionQueryMaps.Empty(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);
	ShadowOcclusionQueryMaps.AddZeroed(FOcclusionQueryHelpers::MaxBufferedOcclusionFrames);	

	LastAutoDownsampleChangeTime = 0;
	SmoothedHalfResTranslucencyGPUDuration = 0;
	SmoothedFullResTranslucencyGPUDuration = 0;
	bShouldAutoDownsampleTranslucency = false;

	PreExposure = 1.f;
	bUpdateLastExposure = false;

#if RHI_RAYTRACING
	GatherPointsBuffer = nullptr;
	GatherPointsResolution = FIntVector(0, 0, 0);
#endif

	bVirtualShadowMapCacheAdded = false;
	bLumenSceneDataAdded = false;
	LumenSurfaceCacheResolution = 1.0f;

	ViewVirtualShadowMapCache = nullptr;

	// OcclusionFeedback works only with mobile rendering atm
	if (FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		extern int32 GOcclusionFeedback_Enable;
		if (GOcclusionFeedback_Enable != 0)
		{
			BeginInitResource(&OcclusionFeedback);
		}
	}
}

void DestroyRenderResource(FRenderResource* RenderResource)
{
	if (RenderResource) 
	{
		ENQUEUE_RENDER_COMMAND(DestroySceneViewStateRenderResource)(
			[RenderResource](FRHICommandList&)
			{
				RenderResource->ReleaseResource();
				delete RenderResource;
			});
	}
}

void DestroyRWBuffer(FRWBuffer* RWBuffer)
{
	ENQUEUE_RENDER_COMMAND(DestroyRWBuffer)(
		[RWBuffer](FRHICommandList&)
		{
			delete RWBuffer;
		});
}

FSceneViewState::~FSceneViewState()
{
	CachedVisibilityChunk = NULL;
	ShadowOcclusionQueryMaps.Reset();

	for (int32 CascadeIndex = 0; CascadeIndex < UE_ARRAY_COUNT(TranslucencyLightingCacheAllocations); CascadeIndex++)
	{
		delete TranslucencyLightingCacheAllocations[CascadeIndex];
	}

	HairStrandsViewStateData.Release();
	ShaderPrintStateData.Release();

	if (ViewVirtualShadowMapCache)
	{
		delete ViewVirtualShadowMapCache;
		ViewVirtualShadowMapCache = nullptr;
		bVirtualShadowMapCacheAdded = false;
	}

	if (Scene)
	{
		Scene->RemoveViewState_RenderThread(this);
	}
}

void FScene::RemoveViewLumenSceneData_RenderThread(FSceneViewStateInterface* ViewState)
{
	FLumenSceneDataKey ByViewKey = { ViewState->GetViewKey(), (uint32)INDEX_NONE };
	FLumenSceneData* const* Found = PerViewOrGPULumenSceneData.Find(ByViewKey);
	if (Found)
	{
		delete* Found;
		PerViewOrGPULumenSceneData.Remove(ByViewKey);
	}
}

void FScene::RemoveViewState_RenderThread(FSceneViewStateInterface* ViewState)
{
	for (int32 ViewStateIndex = 0; ViewStateIndex < ViewStates.Num(); ViewStateIndex++)
	{
		if (ViewStates[ViewStateIndex] == ViewState)
		{
			ViewStates.RemoveAt(ViewStateIndex);
			break;
		}
	}

	RemoveViewLumenSceneData_RenderThread(ViewState);
}


#if WITH_EDITOR

FPixelInspectorData::FPixelInspectorData()
{
	for (int32 i = 0; i < 2; ++i)
	{
		RenderTargetBufferFinalColor[i] = nullptr;
		RenderTargetBufferDepth[i] = nullptr;
		RenderTargetBufferSceneColor[i] = nullptr;
		RenderTargetBufferHDR[i] = nullptr;
		RenderTargetBufferA[i] = nullptr;
		RenderTargetBufferBCDEF[i] = nullptr;
	}
}

void FPixelInspectorData::InitializeBuffers(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDEF, int32 BufferIndex)
{
	RenderTargetBufferFinalColor[BufferIndex] = BufferFinalColor;
	RenderTargetBufferDepth[BufferIndex] = BufferDepth;
	RenderTargetBufferSceneColor[BufferIndex] = BufferSceneColor;
	RenderTargetBufferHDR[BufferIndex] = BufferHDR;
	RenderTargetBufferA[BufferIndex] = BufferA;
	RenderTargetBufferBCDEF[BufferIndex] = BufferBCDEF;

	check(RenderTargetBufferBCDEF[BufferIndex] != nullptr);
	
	FIntPoint BufferSize = RenderTargetBufferBCDEF[BufferIndex]->GetSizeXY();
	check(BufferSize.X == 4 && BufferSize.Y == 1);

	if (RenderTargetBufferA[BufferIndex] != nullptr)
	{
		BufferSize = RenderTargetBufferA[BufferIndex]->GetSizeXY();
		check(BufferSize.X == 1 && BufferSize.Y == 1);
	}
	
	if (RenderTargetBufferFinalColor[BufferIndex] != nullptr)
	{
		BufferSize = RenderTargetBufferFinalColor[BufferIndex]->GetSizeXY();
		//The Final color grab an area and can change depending on the setup
		//It should at least contain 1 pixel but can be 3x3 or more
		check(BufferSize.X > 0 && BufferSize.Y > 0);
	}

	if (RenderTargetBufferDepth[BufferIndex] != nullptr)
	{
		BufferSize = RenderTargetBufferDepth[BufferIndex]->GetSizeXY();
		check(BufferSize.X == 1 && BufferSize.Y == 1);
	}

	if (RenderTargetBufferSceneColor[BufferIndex] != nullptr)
	{
		BufferSize = RenderTargetBufferSceneColor[BufferIndex]->GetSizeXY();
		check(BufferSize.X == 1 && BufferSize.Y == 1);
	}

	if (RenderTargetBufferHDR[BufferIndex] != nullptr)
	{
		BufferSize = RenderTargetBufferHDR[BufferIndex]->GetSizeXY();
		check(BufferSize.X == 1 && BufferSize.Y == 1);
	}
}

bool FPixelInspectorData::AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest)
{
	if (PixelInspectorRequest == nullptr)
		return false;
	FVector2f ViewportUV = PixelInspectorRequest->SourceViewportUV;
	if (Requests.Contains(ViewportUV))
		return false;
	
	//Remove the oldest request since the new request use the buffer
	if (Requests.Num() > 1)
	{
		FVector2f FirstKey(-1, -1);
		for (auto kvp : Requests)
		{
			FirstKey = kvp.Key;
			break;
		}
		if (Requests.Contains(FirstKey))
		{
			Requests.Remove(FirstKey);
		}
	}
	Requests.Add(ViewportUV, PixelInspectorRequest);
	return true;
}

#endif //WITH_EDITOR

bool IncludePrimitiveInDistanceFieldSceneData(bool bTrackAllPrimitives, const FPrimitiveSceneProxy* Proxy)
{
	return PrimitiveNeedsDistanceFieldSceneData(
		bTrackAllPrimitives, 
		Proxy->CastsDynamicIndirectShadow(), 
		Proxy->AffectsDistanceFieldLighting(), 
		Proxy->IsDrawnInGame(),  
		Proxy->CastsHiddenShadow(), 
		Proxy->CastsDynamicShadow(),
		Proxy->AffectsDynamicIndirectLighting(),
		Proxy->AffectsIndirectLightingWhileHidden());
}

void FDistanceFieldSceneData::AddPrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (IncludePrimitiveInDistanceFieldSceneData(bTrackAllPrimitives, Proxy))
	{
		if (Proxy->SupportsHeightfieldRepresentation())
		{
			UTexture2D* HeightAndNormal;
			UTexture2D* DiffuseColor;
			UTexture2D* Visibility;
			FHeightfieldComponentDescription Desc(FMatrix::Identity, InPrimitive->GetInstanceSceneDataOffset());
			Proxy->GetHeightfieldRepresentation(HeightAndNormal, DiffuseColor, Visibility, Desc);
			GHeightFieldTextureAtlas.AddAllocation(HeightAndNormal);

			if (Visibility)
			{
				check(Desc.VisibilityChannel >= 0 && Desc.VisibilityChannel < 4);
				GHFVisibilityTextureAtlas.AddAllocation(Visibility, Desc.VisibilityChannel);
			}

			checkSlow(!PendingHeightFieldAddOps.Contains(InPrimitive));
			PendingHeightFieldAddOps.Add(InPrimitive);
		}

		if (Proxy->SupportsDistanceFieldRepresentation())
		{
			checkSlow(!PendingAddOperations.Contains(InPrimitive));
			checkSlow(!PendingUpdateOperations.Contains(InPrimitive));
			PendingAddOperations.Add(InPrimitive);
		}
	}
}

void FDistanceFieldSceneData::UpdatePrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	const FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (IncludePrimitiveInDistanceFieldSceneData(bTrackAllPrimitives, Proxy)
		&& Proxy->SupportsDistanceFieldRepresentation() 
		&& !PendingAddOperations.Contains(InPrimitive)
		// This is needed to prevent infinite buildup when DF features are off such that the pending operations don't get consumed
		&& !PendingUpdateOperations.Contains(InPrimitive)
		// This can happen when the primitive fails to allocate from the SDF atlas
		&& InPrimitive->DistanceFieldInstanceIndices.Num() > 0)
	{
		PendingUpdateOperations.Add(InPrimitive);
	}
}

void FDistanceFieldSceneData::RemovePrimitive(FPrimitiveSceneInfo* InPrimitive)
{
	FPrimitiveSceneProxy* Proxy = InPrimitive->Proxy;

	if (IncludePrimitiveInDistanceFieldSceneData(bTrackAllPrimitives, Proxy))
	{
		if (Proxy->SupportsDistanceFieldRepresentation())
		{
			PendingAddOperations.Remove(InPrimitive);
			PendingUpdateOperations.Remove(InPrimitive);
			PendingThrottledOperations.Remove(InPrimitive);

			if (InPrimitive->DistanceFieldInstanceIndices.Num() > 0)
			{
				PendingRemoveOperations.Add(FPrimitiveRemoveInfo(InPrimitive));
			}
			
			InPrimitive->DistanceFieldInstanceIndices.Empty();
		}

		if (Proxy->SupportsHeightfieldRepresentation())
		{
			UTexture2D* HeightAndNormal;
			UTexture2D* DiffuseColor;
			UTexture2D* Visibility;
			FHeightfieldComponentDescription Desc(FMatrix::Identity, InPrimitive->GetInstanceSceneDataOffset());
			Proxy->GetHeightfieldRepresentation(HeightAndNormal, DiffuseColor, Visibility, Desc);
			GHeightFieldTextureAtlas.RemoveAllocation(HeightAndNormal);

			if (Visibility)
			{
				GHFVisibilityTextureAtlas.RemoveAllocation(Visibility);
			}

			PendingHeightFieldAddOps.Remove(InPrimitive);

			if (InPrimitive->DistanceFieldInstanceIndices.Num() > 0)
			{
				PendingHeightFieldRemoveOps.Add(FHeightFieldPrimitiveRemoveInfo(InPrimitive));
			}

			InPrimitive->DistanceFieldInstanceIndices.Empty();
		}
	}

	checkf(!PendingAddOperations.Contains(InPrimitive), TEXT("Primitive is being removed from the scene, but didn't remove from Distance Field Scene properly - a crash will occur when processing PendingAddOperations.  This can happen if the proxy's properties have changed without recreating its render state."));
	checkf(!PendingUpdateOperations.Contains(InPrimitive), TEXT("Primitive is being removed from the scene, but didn't remove from Distance Field Scene properly - a crash will occur when processing PendingUpdateOperations.  This can happen if the proxy's properties have changed without recreating its render state."));
	checkf(!PendingThrottledOperations.Contains(InPrimitive), TEXT("Primitive is being removed from the scene, but didn't remove from Distance Field Scene properly - a crash will occur when processing PendingThrottledOperations.  This can happen if the proxy's properties have changed without recreating its render state."));
	
	checkf(!PendingHeightFieldAddOps.Contains(InPrimitive), TEXT("Primitive is being removed from the scene, but didn't remove from Distance Field Scene properly - a crash will occur when processing PendingHeightFieldAddOps.  This can happen if the proxy's properties have changed without recreating its render state."));
}

void FDistanceFieldSceneData::Release()
{
	if (ObjectBuffers != nullptr)
	{
		ObjectBuffers->Release();
	}

	for (int32 BufferIndex = 0; BufferIndex < StreamingRequestReadbackBuffers.Num(); ++BufferIndex)
	{
		if (StreamingRequestReadbackBuffers[BufferIndex])
	{
			delete StreamingRequestReadbackBuffers[BufferIndex];
			StreamingRequestReadbackBuffers[BufferIndex] = nullptr;
		}
	}
}

void FDistanceFieldSceneData::VerifyIntegrity()
{
#if DO_CHECK
	check(NumObjectsInBuffer == PrimitiveInstanceMapping.Num());

	for (int32 PrimitiveInstanceIndex = 0; PrimitiveInstanceIndex < PrimitiveInstanceMapping.Num(); PrimitiveInstanceIndex++)
	{
		const FPrimitiveAndInstance& PrimitiveAndInstance = PrimitiveInstanceMapping[PrimitiveInstanceIndex];

		check(PrimitiveAndInstance.Primitive && PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices.Num() > 0);
		check(PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices.IsValidIndex(PrimitiveAndInstance.InstanceIndex));

		const int32 InstanceIndex = PrimitiveAndInstance.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstance.InstanceIndex];
		check(InstanceIndex == PrimitiveInstanceIndex || InstanceIndex == -1);
	}
#endif
}

void FScene::UpdateSceneSettings(AWorldSettings* WorldSettings)
{
	FScene* Scene = this;
	float InDefaultMaxDistanceFieldOcclusionDistance = WorldSettings->DefaultMaxDistanceFieldOcclusionDistance;
	float InGlobalDistanceFieldViewDistance = WorldSettings->GlobalDistanceFieldViewDistance;
	float InDynamicIndirectShadowsSelfShadowingIntensity = FMath::Clamp(WorldSettings->DynamicIndirectShadowsSelfShadowingIntensity, 0.0f, 1.0f);
	ENQUEUE_RENDER_COMMAND(UpdateSceneSettings)(
		[Scene, InDefaultMaxDistanceFieldOcclusionDistance, InGlobalDistanceFieldViewDistance, InDynamicIndirectShadowsSelfShadowingIntensity](FRHICommandList& RHICmdList)
	{
		Scene->DefaultMaxDistanceFieldOcclusionDistance = InDefaultMaxDistanceFieldOcclusionDistance;
		Scene->GlobalDistanceFieldViewDistance = InGlobalDistanceFieldViewDistance;
		Scene->DynamicIndirectShadowsSelfShadowingIntensity = InDynamicIndirectShadowsSelfShadowingIntensity;
	});
}

/**
 * Sets the FX system associated with the scene.
 */
void FScene::SetFXSystem( class FFXSystemInterface* InFXSystem )
{
	FXSystem = InFXSystem;
}

/**
 * Get the FX system associated with the scene.
 */
FFXSystemInterface* FScene::GetFXSystem()
{
	return FXSystem;
}

static uint64 GetTextureGPUSizeBytes(const FTexture2DRHIRef& Target, bool bLogSizes)
{
	uint64 Size = Target.IsValid() ? Target->GetDesc().CalcMemorySizeEstimate() : 0;
	if (bLogSizes && Size)
	{
		UE_LOG(LogRenderer, Log, TEXT("LogSizes\tTexture\t%s\t%llu"), *Target->GetName().ToString(), Size);
	}
	return Size;
}

static uint64 GetRenderTargetGPUSizeBytes(const TRefCountPtr<IPooledRenderTarget>& Target, bool bLogSizes)
{
	uint64 Size = Target.IsValid() ? Target->ComputeMemorySize() : 0;
	if (bLogSizes && Size)
	{
		UE_LOG(LogRenderer, Log, TEXT("LogSizes\tRenderTarget\t%s\t%llu"), Target->GetDesc().DebugName, Size);
	}
	return Size;
}

static uint64 GetBufferGPUSizeBytes(const TRefCountPtr<FRDGPooledBuffer>& Buffer, bool bLogSizes)
{
	uint64 Size = Buffer.IsValid() ? Buffer->GetSize() : 0;
	if (bLogSizes && Size)
	{
		const TCHAR* Name = Buffer->GetName();
		UE_LOG(LogRenderer, Log, TEXT("LogSizes\tBuffer\t%s\t%llu"), Name ? Name : TEXT("UNKNOWN"), Size);
	}
	return Size;
}

static uint64 GetTextureReadbackGPUSizeBytes(const FRHIGPUTextureReadback* TextureReadback, bool bLogSizes)
{
	uint64 Size = TextureReadback ? TextureReadback->GetGPUSizeBytes() : 0;
	if (bLogSizes && Size)
	{
		UE_LOG(LogRenderer, Log, TEXT("LogSizes\tTextureReadback\t%s\t%llu"), *TextureReadback->GetName().ToString(), Size);
	}
	return Size;
}

static uint64 GetBufferReadbackGPUSizeBytes(const FRHIGPUBufferReadback* BufferReadback, bool bLogSizes)
{
	uint64 Size = BufferReadback ? BufferReadback->GetGPUSizeBytes() : 0;
	if (bLogSizes && Size)
	{
		UE_LOG(LogRenderer, Log, TEXT("LogSizes\tBufferReadback\t%s\t%llu"), *BufferReadback->GetName().ToString(), Size);
	}
	return Size;
}

uint64 FHZBOcclusionTester::GetGPUSizeBytes(bool bLogSizes) const
{
	return ResultsReadback.IsValid() ? GetTextureReadbackGPUSizeBytes(ResultsReadback.Get(), bLogSizes) : 0;
}

uint64 FPersistentSkyAtmosphereData::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (int32 VolumeIndex = 0; VolumeIndex < UE_ARRAY_COUNT(CameraAerialPerspectiveVolumes); VolumeIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(CameraAerialPerspectiveVolumes[VolumeIndex], bLogSizes);
		TotalSize += GetRenderTargetGPUSizeBytes(CameraAerialPerspectiveVolumesMieOnly[VolumeIndex], bLogSizes);
		TotalSize += GetRenderTargetGPUSizeBytes(CameraAerialPerspectiveVolumesRayOnly[VolumeIndex], bLogSizes);
	}
	return TotalSize;
}

uint64 FSceneViewState::FEyeAdaptationManager::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	for (int32 TargetIndex = 0; TargetIndex < UE_ARRAY_COUNT(PooledRenderTarget); TargetIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(PooledRenderTarget[TargetIndex], bLogSizes);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	for (int32 BufferIndex = 0; BufferIndex < UE_ARRAY_COUNT(ExposureBufferData); BufferIndex++)
	{
		TotalSize += GetBufferGPUSizeBytes(ExposureBufferData[BufferIndex], bLogSizes);
	}
	for (FRHIGPUBufferReadback* ReadbackBuffer : ExposureReadbackBuffers)
	{
		TotalSize += GetBufferReadbackGPUSizeBytes(ReadbackBuffer, bLogSizes);
	}
	return TotalSize;
}

uint64 FTemporalAAHistory::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (int32 TargetIndex = 0; TargetIndex < kRenderTargetCount; TargetIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(RT[TargetIndex], bLogSizes);
	}
	return TotalSize;
}

uint64 FTSRHistory::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetRenderTargetGPUSizeBytes(ColorArray, bLogSizes) +
		GetRenderTargetGPUSizeBytes(Metadata, bLogSizes) +
		GetRenderTargetGPUSizeBytes(TranslucencyAlpha, bLogSizes) +
		GetRenderTargetGPUSizeBytes(SubpixelDetails, bLogSizes) +
		GetRenderTargetGPUSizeBytes(Guide, bLogSizes);
}

uint64 FScreenSpaceDenoiserHistory::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (int32 TargetIndex = 0; TargetIndex < RTCount; TargetIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(RT[TargetIndex], bLogSizes);
	}
	TotalSize += GetRenderTargetGPUSizeBytes(TileClassification, bLogSizes);
	return TotalSize;
}

uint64 FPreviousViewInfo::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize =
		GetRenderTargetGPUSizeBytes(DepthBuffer, bLogSizes) +
		GetRenderTargetGPUSizeBytes(GBufferA, bLogSizes) +
		GetRenderTargetGPUSizeBytes(GBufferB, bLogSizes) +
		GetRenderTargetGPUSizeBytes(GBufferC, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ImaginaryReflectionDepthBuffer, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ImaginaryReflectionGBufferA, bLogSizes) +
		GetRenderTargetGPUSizeBytes(HZB, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NaniteHZB, bLogSizes) +
		GetRenderTargetGPUSizeBytes(CompressedDepthViewNormal, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ImaginaryReflectionCompressedDepthViewNormal, bLogSizes) +
		GetRenderTargetGPUSizeBytes(CompressedOpaqueDepth, bLogSizes) +
		GetRenderTargetGPUSizeBytes(CompressedOpaqueShadingModel, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ScreenSpaceRayTracingInput, bLogSizes) +
		GetRenderTargetGPUSizeBytes(SeparateTranslucency, bLogSizes) +
		TemporalAAHistory.GetGPUSizeBytes(bLogSizes) +
		TSRHistory.GetGPUSizeBytes(bLogSizes) +
		GetRenderTargetGPUSizeBytes(HalfResTemporalAAHistory, bLogSizes) +
		DOFSetupHistory.GetGPUSizeBytes(bLogSizes) +
		SSRHistory.GetGPUSizeBytes(bLogSizes) +
		WaterSSRHistory.GetGPUSizeBytes(bLogSizes) +
		RoughRefractionHistory.GetGPUSizeBytes(bLogSizes) +
		HairHistory.GetGPUSizeBytes(bLogSizes) +
		EditorPrimtiveDepthHistory.GetGPUSizeBytes(bLogSizes) +
		CustomSSRInput.GetGPUSizeBytes(bLogSizes) +
		ReflectionsHistory.GetGPUSizeBytes(bLogSizes) +
		WaterReflectionsHistory.GetGPUSizeBytes(bLogSizes) +
		AmbientOcclusionHistory.GetGPUSizeBytes(bLogSizes) +
		GetRenderTargetGPUSizeBytes(GTAOHistory.RT, bLogSizes) +
		DiffuseIndirectHistory.GetGPUSizeBytes(bLogSizes) +
		SkyLightHistory.GetGPUSizeBytes(bLogSizes) +
		ReflectedSkyLightHistory.GetGPUSizeBytes(bLogSizes) +
		PolychromaticPenumbraHarmonicsHistory.GetGPUSizeBytes(bLogSizes) +
		GetRenderTargetGPUSizeBytes(MobileBloomSetup_EyeAdaptation, bLogSizes) +
		GetRenderTargetGPUSizeBytes(MobilePixelProjectedReflection, bLogSizes) +
		GetRenderTargetGPUSizeBytes(MobileAmbientOcclusion, bLogSizes) +
		GetRenderTargetGPUSizeBytes(VisualizeMotionVectors, bLogSizes);

	for (auto ShadowHistoryIt = ShadowHistories.begin(); ShadowHistoryIt; ++ShadowHistoryIt)
	{
		if (ShadowHistoryIt.Value().IsValid())
		{
			TotalSize += ShadowHistoryIt.Value()->GetGPUSizeBytes(bLogSizes);
		}
	}

	return TotalSize;
}

/** FLumenViewState GPU size queries */
uint64 FScreenProbeGatherTemporalState::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetRenderTargetGPUSizeBytes(DiffuseIndirectHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RoughSpecularIndirectHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NumFramesAccumulatedRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(FastUpdateModeHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NormalHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(BSDFTileHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(HistoryScreenProbeSceneDepth, bLogSizes) +
		GetRenderTargetGPUSizeBytes(HistoryScreenProbeTranslatedWorldPosition, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ProbeHistoryScreenProbeRadiance, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ImportanceSamplingHistoryScreenProbeRadiance, bLogSizes);
}

uint64 FReflectionTemporalState::GetGPUSizeBytes(bool bLogSizes) const
{
	return 
		GetRenderTargetGPUSizeBytes(SpecularIndirectHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NumFramesAccumulatedRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ResolveVarianceHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(BSDFTileHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(DepthHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NormalHistoryRT, bLogSizes);
}

uint64 FRadianceCacheState::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetRenderTargetGPUSizeBytes(RadianceProbeIndirectionTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadianceProbeAtlasTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(FinalRadianceAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(FinalIrradianceAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ProbeOcclusionAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(DepthProbeAtlasTexture, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeAllocator, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeFreeListAllocator, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeFreeList, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeLastUsedFrame, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeLastTracedFrame, bLogSizes) +
		GetBufferGPUSizeBytes(ProbeWorldOffset, bLogSizes);
}

uint64 FLumenViewState::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		ScreenProbeGatherState.GetGPUSizeBytes(bLogSizes) +
		ReflectionState.GetGPUSizeBytes(bLogSizes) +
		TranslucentReflectionState.GetGPUSizeBytes(bLogSizes) +
		GetRenderTargetGPUSizeBytes(DepthHistoryRT, bLogSizes) +
		GetRenderTargetGPUSizeBytes(TranslucencyVolume0, bLogSizes) +
		GetRenderTargetGPUSizeBytes(TranslucencyVolume1, bLogSizes) +
		RadianceCacheState.GetGPUSizeBytes(bLogSizes) +
		TranslucencyVolumeRadianceCacheState.GetGPUSizeBytes(bLogSizes);
}

/** FLumenSceneData GPU size queries */
uint64 FLumenSurfaceCacheFeedback::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (const FRHIGPUBufferReadback* ReadbackBuffer : ReadbackBuffers)
	{
		TotalSize += GetBufferReadbackGPUSizeBytes(ReadbackBuffer, bLogSizes);
	}
	return TotalSize;
}

uint64 FLumenSceneData::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetBufferGPUSizeBytes(CardBuffer, bLogSizes) +
		CardUploadBuffer.GetNumBytes() +
		GetBufferGPUSizeBytes(MeshCardsBuffer, bLogSizes) +
		MeshCardsUploadBuffer.GetNumBytes() +
		GetBufferGPUSizeBytes(HeightfieldBuffer, bLogSizes) +
		HeightfieldUploadBuffer.GetNumBytes() +
		GetBufferGPUSizeBytes(SceneInstanceIndexToMeshCardsIndexBuffer, bLogSizes) +
		SceneInstanceIndexToMeshCardsIndexUploadBuffer.GetNumBytes() +
		GetBufferGPUSizeBytes(CardPageBuffer, bLogSizes) +
		CardPageUploadBuffer.GetNumBytes() +
		GetBufferGPUSizeBytes(CardPageLastUsedBuffer, bLogSizes) +
		GetBufferGPUSizeBytes(CardPageHighResLastUsedBuffer, bLogSizes) +
		GetRenderTargetGPUSizeBytes(AlbedoAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(OpacityAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(NormalAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(EmissiveAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(DepthAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(DirectLightingAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(IndirectLightingAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityNumFramesAccumulatedAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(FinalLightingAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityTraceRadianceAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityTraceHitDistanceAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityProbeSHRedAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityProbeSHGreenAtlas, bLogSizes) +
		GetRenderTargetGPUSizeBytes(RadiosityProbeSHBlueAtlas, bLogSizes) +
		SurfaceCacheFeedback.GetGPUSizeBytes(bLogSizes) +
		GetBufferGPUSizeBytes(PageTableBuffer, bLogSizes) +
		PageTableUploadBuffer.GetNumBytes();
}

uint64 FPersistentGlobalDistanceFieldData::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize =
		GetBufferGPUSizeBytes(PageFreeListAllocatorBuffer, bLogSizes) +
		GetBufferGPUSizeBytes(PageFreeListBuffer, bLogSizes) +
		GetRenderTargetGPUSizeBytes(PageAtlasTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(CoverageAtlasTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(PageTableCombinedTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(MipTexture, bLogSizes);

	for (int32 GDFIndex = 0; GDFIndex < UE_ARRAY_COUNT(PageTableLayerTextures); GDFIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(PageTableLayerTextures[GDFIndex], bLogSizes);
	}
	return TotalSize;
}

uint64 FVolumetricRenderTargetViewStateData::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (uint32 TargetIndex = 0; TargetIndex < kRenderTargetCount; TargetIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(VolumetricReconstructRT[TargetIndex], bLogSizes);
		TotalSize += GetRenderTargetGPUSizeBytes(VolumetricReconstructRTDepth[TargetIndex], bLogSizes);
	}
	TotalSize += GetRenderTargetGPUSizeBytes(VolumetricTracingRT, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(VolumetricTracingRTDepth, bLogSizes);
	return TotalSize;
}

uint64 FTemporalRenderTargetState::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;
	for (int32 TargetIndex = 0; TargetIndex < UE_ARRAY_COUNT(RenderTargets); TargetIndex++)
	{
		TotalSize += GetRenderTargetGPUSizeBytes(RenderTargets[TargetIndex], bLogSizes);
	}
	return TotalSize;
}

uint64 FShadingEnergyConservationStateData::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetRenderTargetGPUSizeBytes(GGXSpecEnergyTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(GGXGlassEnergyTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(ClothEnergyTexture, bLogSizes) +
		GetRenderTargetGPUSizeBytes(DiffuseEnergyTexture, bLogSizes);
}

uint64 FVirtualShadowMapArrayFrameData::GetGPUSizeBytes(bool bLogSizes) const
{
	return
		GetBufferGPUSizeBytes(PageTable, bLogSizes) +
		GetBufferGPUSizeBytes(PageFlags, bLogSizes) +
		GetBufferGPUSizeBytes(ProjectionData, bLogSizes) +
		GetBufferGPUSizeBytes(PageRectBounds, bLogSizes) +
		GetBufferGPUSizeBytes(PhysicalPageMetaData, bLogSizes) +
		GetRenderTargetGPUSizeBytes(HZBPhysical, bLogSizes);
};

uint64 FVirtualShadowMapArrayCacheManager::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = PrevBuffers.GetGPUSizeBytes(bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(PhysicalPagePool, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(HZBPhysicalPagePool, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(AccumulatedStatsBuffer, bLogSizes);
	TotalSize += GetBufferReadbackGPUSizeBytes(GPUBufferReadback, bLogSizes);
	return TotalSize;
}

uint64 FSceneViewState::GetGPUSizeBytes(bool bLogSizes) const
{
	uint64 TotalSize = 0;

	// Todo, not currently computing GPU memory usage for queries or sampler states.  Are these important?  Should be small...
	//  ShadowOcclusionQueryMaps
	//  OcclusionQueryPool
	//  PrimitiveOcclusionQueryPool
	//  PlanarReflectionOcclusionHistories
	//  MaterialTextureBilinearWrapedSamplerCache
	//  MaterialTextureBilinearClampedSamplerCache

	TotalSize += HZBOcclusionTests.GetGPUSizeBytes(bLogSizes);
	TotalSize += PersistentSkyAtmosphereData.GetGPUSizeBytes(bLogSizes);
	TotalSize += EyeAdaptationManager.GetGPUSizeBytes(bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(CombinedLUTRenderTarget, bLogSizes);
	TotalSize += PrevFrameViewInfo.GetGPUSizeBytes(bLogSizes);
	TotalSize += LightShaftOcclusionHistory.GetGPUSizeBytes(bLogSizes);
	for (auto LightShaftBloomIt = LightShaftBloomHistoryRTs.begin(); LightShaftBloomIt; ++LightShaftBloomIt)
	{
		if (LightShaftBloomIt.Value().IsValid())
		{
			TotalSize += LightShaftBloomIt.Value()->GetGPUSizeBytes(bLogSizes);
		}
	}
	TotalSize += GetRenderTargetGPUSizeBytes(DistanceFieldAOHistoryRT, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(DistanceFieldIrradianceHistoryRT, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(SubsurfaceScatteringQualityHistoryRT, bLogSizes);
	TotalSize += Lumen.GetGPUSizeBytes(bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(BloomFFTKernel.Spectral, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(BloomFFTKernel.ConstantsBuffer, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(FilmGrainCache.ConstantsBuffer, bLogSizes);
#if RHI_RAYTRACING
	TotalSize += GetRenderTargetGPUSizeBytes(ImaginaryReflectionGBufferA, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(ImaginaryReflectionDepthZ, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(ImaginaryReflectionVelocity, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(SkyLightVisibilityRaysBuffer, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(GatherPointsBuffer, bLogSizes);
#endif
	TotalSize += GetRenderTargetGPUSizeBytes(LightScatteringHistory, bLogSizes);
	TotalSize += GetRenderTargetGPUSizeBytes(PrevLightScatteringConservativeDepthTexture, bLogSizes);
	if (GlobalDistanceFieldData.IsValid())
	{
		TotalSize += GlobalDistanceFieldData->GetGPUSizeBytes(bLogSizes);
	}
	TotalSize += VolumetricCloudRenderTarget.GetGPUSizeBytes(bLogSizes);
	for (int32 LightIndex = 0; LightIndex < UE_ARRAY_COUNT(VolumetricCloudShadowRenderTarget); LightIndex++)
	{
		TotalSize += VolumetricCloudShadowRenderTarget[LightIndex].GetGPUSizeBytes(bLogSizes);
	}
	TotalSize += GetBufferGPUSizeBytes(HairStrandsViewStateData.VoxelFeedbackBuffer, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(ShaderPrintStateData.EntryBuffer, bLogSizes);
	TotalSize += GetBufferGPUSizeBytes(ShaderPrintStateData.StateBuffer, bLogSizes);
	TotalSize += ShadingEnergyConservationData.GetGPUSizeBytes(bLogSizes);
	if (ViewVirtualShadowMapCache)
	{
		TotalSize += ViewVirtualShadowMapCache->GetGPUSizeBytes(bLogSizes);
	}

	// Per-view Lumen scene data is stored in a map in the FScene
	if (Scene && bLumenSceneDataAdded)
	{
		FLumenSceneDataKey ByViewKey = { GetViewKey(), (uint32)INDEX_NONE };
		FLumenSceneData** SceneData = Scene->PerViewOrGPULumenSceneData.Find(ByViewKey);

		if (SceneData)
		{
			TotalSize += (*SceneData)->GetGPUSizeBytes(bLogSizes);
		}
	}

	return TotalSize;
}

void FSceneViewState::AddVirtualShadowMapCache(FSceneInterface* InScene)
{
	check(InScene);
	if (!Scene)
	{
		Scene = (FScene*)InScene;

		// Modification of scene structure needs to happen on render thread
		ENQUEUE_RENDER_COMMAND(SceneViewStateAdd)(
			[RenderScene = Scene, RenderViewState = this](FRHICommandList&)
			{
				RenderScene->ViewStates.Add(RenderViewState);
			});
	}

	if (Scene == InScene)
	{
		// Don't add the cache if one has already been added.  We have a separate bool since the write to the cache
		// pointer is deferred to the render thread.
		if (!bVirtualShadowMapCacheAdded)
		{
			bVirtualShadowMapCacheAdded = true;

			FVirtualShadowMapArrayCacheManager* ViewCache = new FVirtualShadowMapArrayCacheManager(Scene);

			// Need to add reference to virtual shadow map cache in render thread
			ENQUEUE_RENDER_COMMAND(LinkVirtualShadowMapCache)(
				[this, ViewCache](FRHICommandListImmediate& RHICmdList)
				{
					this->ViewVirtualShadowMapCache = ViewCache;
				});
		} //-V773
	}
}

void FSceneViewState::RemoveVirtualShadowMapCache(FSceneInterface* InScene)
{
	check(InScene);
	if (Scene == InScene && bVirtualShadowMapCacheAdded)
	{
		bVirtualShadowMapCacheAdded = false;

		ENQUEUE_RENDER_COMMAND(RemoveVirtualShadowMapCache)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				delete ViewVirtualShadowMapCache;
				ViewVirtualShadowMapCache = nullptr;
			});
	}
}

bool FSceneViewState::HasVirtualShadowMapCache() const
{
	return bVirtualShadowMapCacheAdded;
}

FVirtualShadowMapArrayCacheManager* FSceneViewState::GetVirtualShadowMapCache(const FScene* InScene) const
{
	// Per-view VSM cache can only be used for the Scene the view state was previously linked to
	return Scene == InScene ? ViewVirtualShadowMapCache : nullptr;
}

FVirtualShadowMapArrayCacheManager* FScene::GetVirtualShadowMapCache(FSceneView& View) const
{
	FVirtualShadowMapArrayCacheManager* Result = DefaultVirtualShadowMapCache;
	if (View.State)
	{
		FVirtualShadowMapArrayCacheManager* ViewCache = View.State->GetVirtualShadowMapCache(this);
		if (ViewCache)
		{
			Result = ViewCache;
		}
	}
	return Result;
}

void FSceneViewState::AddLumenSceneData(FSceneInterface* InScene, float InSurfaceCacheResolution)
{
	check(InScene);
	if (!Scene)
	{
		Scene = (FScene*)InScene;

		// Modification of scene structure needs to happen on render thread
		ENQUEUE_RENDER_COMMAND(SceneViewStateAdd)(
			[RenderScene = Scene, RenderViewState = this](FRHICommandList&)
			{
				RenderScene->ViewStates.Add(RenderViewState);
			});
	}

	if (Scene == InScene && Scene->DefaultLumenSceneData)
	{
		// Don't allocate if one already exists
		if (!bLumenSceneDataAdded)
		{
			bLumenSceneDataAdded = true;
			LumenSurfaceCacheResolution = InSurfaceCacheResolution;

			FLumenSceneData* SceneData = new FLumenSceneData(Scene->DefaultLumenSceneData->bTrackAllPrimitives);
			SceneData->bViewSpecific = true;
			SceneData->SurfaceCacheResolution = FMath::Clamp(InSurfaceCacheResolution, 0.5f, 1.0f);

			// Need to add reference to Lumen scene data in render thread
			ENQUEUE_RENDER_COMMAND(LinkLumenSceneData)(
				[this, SceneData](FRHICommandListImmediate& RHICmdList)
				{
					SceneData->CopyInitialData(*Scene->DefaultLumenSceneData);

					// Key shouldn't already exist in Scene, because the bLumenSceneDataAdded flag should only allow it to be added once.
					FLumenSceneDataKey ByViewKey = { GetViewKey(), (uint32)INDEX_NONE };
					check(Scene->PerViewOrGPULumenSceneData.Find(ByViewKey) == nullptr);

					Scene->PerViewOrGPULumenSceneData.Emplace(ByViewKey, SceneData);
				});
		} //-V773
		else if (LumenSurfaceCacheResolution != InSurfaceCacheResolution)
		{
			LumenSurfaceCacheResolution = InSurfaceCacheResolution;

			ENQUEUE_RENDER_COMMAND(ChangeLumenSceneDataQuality)(
				[this, InSurfaceCacheResolution](FRHICommandListImmediate& RHICmdList)
				{
					FLumenSceneDataKey ByViewKey = { GetViewKey(), (uint32)INDEX_NONE };
					FLumenSceneData** SceneData = Scene->PerViewOrGPULumenSceneData.Find(ByViewKey);

					check(SceneData);

					(*SceneData)->SurfaceCacheResolution = FMath::Clamp(InSurfaceCacheResolution, 0.5f, 1.0f);
				});
		}
	}
}

void FSceneViewState::RemoveLumenSceneData(FSceneInterface* InScene)
{
	check(InScene);
	if (Scene == InScene && bLumenSceneDataAdded)
	{
		bLumenSceneDataAdded = false;

		ENQUEUE_RENDER_COMMAND(RemoveLumenSceneData)(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				FLumenSceneDataKey ByViewKey = { GetViewKey(), (uint32)INDEX_NONE };
				FLumenSceneData** SceneData = Scene->PerViewOrGPULumenSceneData.Find(ByViewKey);

				check(SceneData);
				delete *SceneData;

				Scene->PerViewOrGPULumenSceneData.Remove(ByViewKey);
			});
	}
}

bool FSceneViewState::HasLumenSceneData() const
{
	return bLumenSceneDataAdded;
}

FLumenSceneData* FScene::FindLumenSceneData(uint32 ViewKey, uint32 GPUIndex) const
{
	// First search by ViewKey
	FLumenSceneDataKey ByViewKey = { ViewKey, (uint32)INDEX_NONE };
	FLumenSceneData* const* Found = PerViewOrGPULumenSceneData.Find(ByViewKey);
	if (Found)
	{
		return *Found;
	}

	// Then search by GPU
	FLumenSceneDataKey ByGPUIndex = { 0, GPUIndex };
	Found = PerViewOrGPULumenSceneData.Find(ByGPUIndex);
	if (Found)
	{
		return *Found;
	}

	// If both fail, return default
	return DefaultLumenSceneData;
}

void FScene::GetAllVirtualShadowMapCacheManagers(TArray<FVirtualShadowMapArrayCacheManager*, SceneRenderingAllocator>& OutCacheManagers) const
{
	OutCacheManagers.Empty();
	if (DefaultVirtualShadowMapCache)
	{
		OutCacheManagers.Add(DefaultVirtualShadowMapCache);
	}
	for (const FSceneViewState* ViewState : ViewStates)
	{
		// Per-view VSM cache can only be used for the Scene the view state was previously linked to
		if (ViewState->ViewVirtualShadowMapCache && ViewState->Scene == this)
		{
			OutCacheManagers.Add(ViewState->ViewVirtualShadowMapCache);
		}
	}
}

void FScene::UpdateParameterCollections(const TArray<FMaterialParameterCollectionInstanceResource*>& InParameterCollections)
{
	ENQUEUE_RENDER_COMMAND(UpdateParameterCollectionsCommand)(
		[this, InParameterCollections](FRHICommandList&)
	{
		// Empty the scene's map so any unused uniform buffers will be released
		ParameterCollections.Empty();

		// Add each existing parameter collection id and its uniform buffer
		for (int32 CollectionIndex = 0; CollectionIndex < InParameterCollections.Num(); CollectionIndex++)
		{
			FMaterialParameterCollectionInstanceResource* InstanceResource = InParameterCollections[CollectionIndex];
			ParameterCollections.Add(InstanceResource->GetId(), InstanceResource->GetUniformBuffer());
		}
	});
}

bool FScene::RequestGPUSceneUpdate(FPrimitiveSceneInfo& PrimitiveSceneInfo, EPrimitiveDirtyState PrimitiveDirtyState)
{
	return PrimitiveSceneInfo.RequestGPUSceneUpdate(PrimitiveDirtyState);
}

void FScene::RefreshNaniteRasterBins(FPrimitiveSceneInfo& PrimitiveSceneInfo)
{
	PrimitiveSceneInfo.RefreshNaniteRasterBins();
}

SIZE_T FScene::GetSizeBytes() const
{
	return sizeof(*this) 
		+ Primitives.GetAllocatedSize()
		+ Lights.GetAllocatedSize()
		+ StaticMeshes.GetAllocatedSize()
		+ ExponentialFogs.GetAllocatedSize()
		+ WindSources.GetAllocatedSize()
		+ SpeedTreeVertexFactoryMap.GetAllocatedSize()
		+ SpeedTreeWindComputationMap.GetAllocatedSize()
		+ LocalShadowCastingLightOctree.GetSizeBytes()
		+ PrimitiveOctree.GetSizeBytes();
}

void FScene::OnWorldCleanup()
{
	UniformBuffers.Clear();
}

void FScene::CheckPrimitiveArrays(int MaxTypeOffsetIndex)
{
	check(Primitives.Num() == PrimitiveTransforms.Num());
	check(Primitives.Num() == PrimitiveSceneProxies.Num());
	check(Primitives.Num() == PrimitiveBounds.Num());
	check(Primitives.Num() == PrimitiveFlagsCompact.Num());
	check(Primitives.Num() == PrimitiveVisibilityIds.Num());
	check(Primitives.Num() == PrimitiveOctreeIndex.Num());
	check(Primitives.Num() == PrimitiveOcclusionFlags.Num());
	check(Primitives.Num() == PrimitiveComponentIds.Num());
	check(Primitives.Num() == PrimitiveVirtualTextureFlags.Num());
	check(Primitives.Num() == PrimitiveVirtualTextureLod.Num());
	check(Primitives.Num() == PrimitiveOcclusionBounds.Num());
#if WITH_EDITOR
	check(Primitives.Num() == PrimitivesSelected.Num());
#endif
#if RHI_RAYTRACING
	check(Primitives.Num() == PrimitiveRayTracingFlags.Num());
	check(Primitives.Num() == PrimitiveRayTracingGroupIds.Num());
#endif
	check(Primitives.Num() == PrimitivesNeedingStaticMeshUpdate.Num());

#if UE_BUILD_DEBUG
	MaxTypeOffsetIndex = MaxTypeOffsetIndex == -1 ? TypeOffsetTable.Num() : MaxTypeOffsetIndex;
	for (int i = 0; i < MaxTypeOffsetIndex; i++)
	{
		for (int j = i + 1; j < MaxTypeOffsetIndex; j++)
		{
			check(TypeOffsetTable[i].PrimitiveSceneProxyType != TypeOffsetTable[j].PrimitiveSceneProxyType);
			check(TypeOffsetTable[i].Offset <= TypeOffsetTable[j].Offset);
		}
	}

	uint32 NextOffset = 0;
	for (int i = 0; i < MaxTypeOffsetIndex; i++)
	{
		const FTypeOffsetTableEntry& Entry = TypeOffsetTable[i];
		for (uint32 Index = NextOffset; Index < Entry.Offset; Index++)
		{
			checkSlow(Primitives[Index]->Proxy == PrimitiveSceneProxies[Index]);
			SIZE_T TypeHash = PrimitiveSceneProxies[Index]->GetTypeHash();
			checkfSlow(TypeHash == Entry.PrimitiveSceneProxyType, TEXT("TypeHash: %i not matching, expected: %i"), TypeHash, Entry.PrimitiveSceneProxyType);
		}
		NextOffset = Entry.Offset;
	}
#endif
}

static void UpdateEarlyZPassModeCVarSinkFunction()
{
	static auto* CVarAntiAliasingMethod = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AntiAliasingMethod"));
	static int32 CachedAntiAliasingMethod = CVarAntiAliasingMethod->GetValueOnGameThread();
	static int32 CachedEarlyZPass = CVarEarlyZPass.GetValueOnGameThread();
	static int32 CachedBasePassWriteDepthEvenWithFullPrepass = CVarBasePassWriteDepthEvenWithFullPrepass.GetValueOnGameThread();
	static int32 CachedMobileEarlyZPass = CVarMobileEarlyZPass.GetValueOnGameThread();

	const int32 AntiAliasingMethod = CVarAntiAliasingMethod->GetValueOnGameThread();
	const int32 EarlyZPass = CVarEarlyZPass.GetValueOnGameThread();
	const int32 BasePassWriteDepthEvenWithFullPrepass = CVarBasePassWriteDepthEvenWithFullPrepass.GetValueOnGameThread();
	const int32 MobileEarlyZPass = CVarMobileEarlyZPass.GetValueOnGameThread();

	// Switching between MSAA and another AA in forward shading mode requires EarlyZPassMode to update.
	if (AntiAliasingMethod != CachedAntiAliasingMethod
		|| EarlyZPass != CachedEarlyZPass
		|| BasePassWriteDepthEvenWithFullPrepass != CachedBasePassWriteDepthEvenWithFullPrepass
		|| MobileEarlyZPass != CachedMobileEarlyZPass)
	{
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* World = *It;
			if (World && World->Scene)
			{
				FScene* Scene = (FScene*)(World->Scene);
				Scene->UpdateEarlyZPassMode();
			}
		}

		CachedAntiAliasingMethod = AntiAliasingMethod;
		CachedEarlyZPass = EarlyZPass;
		CachedBasePassWriteDepthEvenWithFullPrepass = BasePassWriteDepthEvenWithFullPrepass;
		CachedMobileEarlyZPass = MobileEarlyZPass;
	}
}

static FAutoConsoleVariableSink CVarUpdateEarlyZPassModeSink(FConsoleCommandDelegate::CreateStatic(&UpdateEarlyZPassModeCVarSinkFunction));

void FScene::DumpMeshDrawCommandMemoryStats()
{
	SIZE_T TotalCachedMeshDrawCommands = 0;
	SIZE_T TotalStaticMeshCommandInfos = 0;

	struct FPassStats
	{
		SIZE_T CachedMeshDrawCommandBytes = 0;
		SIZE_T PSOBytes = 0;
		SIZE_T ShaderBindingInlineBytes = 0;
		SIZE_T ShaderBindingHeapBytes = 0;
		SIZE_T VertexStreamsInlineBytes = 0;
		SIZE_T DebugDataBytes = 0;
		SIZE_T DrawCommandParameterBytes = 0;
		uint32 NumCommands = 0;
	};

	FPassStats AllPassStats[EMeshPass::Num];
	TArray<bool> StateBucketAccounted[EMeshPass::Num];
	for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
	{
		StateBucketAccounted[PassIndex].Empty(CachedMeshDrawCommandStateBuckets[PassIndex].GetMaxIndex());
		StateBucketAccounted[PassIndex].AddZeroed(CachedMeshDrawCommandStateBuckets[PassIndex].GetMaxIndex());
	}

	for (int32 i = 0; i < Primitives.Num(); i++)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Primitives[i];

		TotalStaticMeshCommandInfos += PrimitiveSceneInfo->StaticMeshCommandInfos.GetAllocatedSize();

		for (int32 CommandIndex = 0; CommandIndex < PrimitiveSceneInfo->StaticMeshCommandInfos.Num(); ++CommandIndex)
		{
			const FCachedMeshDrawCommandInfo& CachedCommand = PrimitiveSceneInfo->StaticMeshCommandInfos[CommandIndex];
			int PassIndex = CachedCommand.MeshPass;
			const FMeshDrawCommand* MeshDrawCommandPtr = nullptr;

			if (CachedCommand.StateBucketId != INDEX_NONE)
			{
				if (!StateBucketAccounted[PassIndex][CachedCommand.StateBucketId])
				{
					StateBucketAccounted[PassIndex][CachedCommand.StateBucketId] = true;
					MeshDrawCommandPtr = &CachedMeshDrawCommandStateBuckets[PassIndex].GetByElementId(CachedCommand.StateBucketId).Key;
				}
			}
			else if (CachedCommand.CommandIndex >= 0)
			{
				FCachedPassMeshDrawList& PassDrawList = CachedDrawLists[CachedCommand.MeshPass];
				MeshDrawCommandPtr = &PassDrawList.MeshDrawCommands[CachedCommand.CommandIndex];
			}

			if (MeshDrawCommandPtr)
			{
				const FMeshDrawCommand& MeshDrawCommand = *MeshDrawCommandPtr;
				FPassStats& PassStats = AllPassStats[CachedCommand.MeshPass];
				SIZE_T CommandBytes = sizeof(MeshDrawCommand) + MeshDrawCommand.GetAllocatedSize();
				PassStats.CachedMeshDrawCommandBytes += CommandBytes;
				TotalCachedMeshDrawCommands += MeshDrawCommand.GetAllocatedSize();
				PassStats.PSOBytes += sizeof(MeshDrawCommand.CachedPipelineId);
				PassStats.ShaderBindingInlineBytes += sizeof(MeshDrawCommand.ShaderBindings);
				PassStats.ShaderBindingHeapBytes += MeshDrawCommand.ShaderBindings.GetAllocatedSize();
				PassStats.VertexStreamsInlineBytes += sizeof(MeshDrawCommand.VertexStreams);
				PassStats.DebugDataBytes += MeshDrawCommand.GetDebugDataSize();
				PassStats.DrawCommandParameterBytes += sizeof(MeshDrawCommand.IndexBuffer) + sizeof(MeshDrawCommand.FirstIndex) + sizeof(MeshDrawCommand.NumPrimitives) + sizeof(MeshDrawCommand.NumInstances) + sizeof(MeshDrawCommand.VertexParams); //-V568
				PassStats.NumCommands++;
			}
		}
	}

	for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
	{
		TotalCachedMeshDrawCommands += CachedMeshDrawCommandStateBuckets[PassIndex].GetAllocatedSize();
	}

	for (int32 i = 0; i < EMeshPass::Num; i++)
	{
		TotalCachedMeshDrawCommands += CachedDrawLists[i].MeshDrawCommands.GetAllocatedSize();
	}

	for (int32 i = 0; i < EMeshPass::Num; i++)
	{
		const FPassStats& PassStats = AllPassStats[i];

		if (PassStats.NumCommands > 0)
		{
			UE_LOG(LogRenderer, Log, TEXT("%s: %.1fKb for %u CachedMeshDrawCommands"), GetMeshPassName((EMeshPass::Type)i), PassStats.CachedMeshDrawCommandBytes / 1024.0f, PassStats.NumCommands);

			if (PassStats.CachedMeshDrawCommandBytes > 1024 && i <= EMeshPass::BasePass)
			{
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes PSO"), PassStats.PSOBytes / (float)PassStats.NumCommands);
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes ShaderBindingInline"), PassStats.ShaderBindingInlineBytes / (float)PassStats.NumCommands);
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes ShaderBindingHeap"), PassStats.ShaderBindingHeapBytes / (float)PassStats.NumCommands);
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes VertexStreamsInline"), PassStats.VertexStreamsInlineBytes / (float)PassStats.NumCommands);
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes DebugData"), PassStats.DebugDataBytes / (float)PassStats.NumCommands);
				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes DrawCommandParameters"), PassStats.DrawCommandParameterBytes / (float)PassStats.NumCommands);

				const SIZE_T Other = PassStats.CachedMeshDrawCommandBytes -
					(PassStats.PSOBytes +
					PassStats.ShaderBindingInlineBytes +
					PassStats.ShaderBindingHeapBytes +
					PassStats.VertexStreamsInlineBytes +
					PassStats.DebugDataBytes +
					PassStats.DrawCommandParameterBytes);

				UE_LOG(LogRenderer, Log, TEXT("     avg %.1f bytes Other"), Other / (float)PassStats.NumCommands);
			}
		}
	}

	UE_LOG(LogRenderer, Log, TEXT("sizeof(FMeshDrawCommand) %u"), sizeof(FMeshDrawCommand));
	UE_LOG(LogRenderer, Log, TEXT("Total cached MeshDrawCommands %.3fMb"), TotalCachedMeshDrawCommands / 1024.0f / 1024.0f);
	UE_LOG(LogRenderer, Log, TEXT("Primitive StaticMeshCommandInfos %.1fKb"), TotalStaticMeshCommandInfos / 1024.0f);
	UE_LOG(LogRenderer, Log, TEXT("GPUScene CPU structures %.1fKb"), GPUScene.PrimitivesToUpdate.GetAllocatedSize() / 1024.0f);
	UE_LOG(LogRenderer, Log, TEXT("PSO persistent Id table %.1fKb %d elements"), FGraphicsMinimalPipelineStateId::GetPersistentIdTableSize() / 1024.0f, FGraphicsMinimalPipelineStateId::GetPersistentIdNum());
	UE_LOG(LogRenderer, Log, TEXT("PSO one frame Id %.1fKb"), FGraphicsMinimalPipelineStateId::GetLocalPipelineIdTableSize() / 1024.0f);
}

template<typename T>
static void TArraySwapElements(TArray<T>& Array, int i1, int i2)
{
	T tmp = Array[i1];
	Array[i1] = Array[i2];
	Array[i2] = tmp;
}

static void TBitArraySwapElements(TBitArray<>& Array, int32 i1, int32 i2)
{
	FBitReference BitRef1 = Array[i1];
	FBitReference BitRef2 = Array[i2];
	bool Bit1 = BitRef1;
	bool Bit2 = BitRef2;
	BitRef1 = Bit2;
	BitRef2 = Bit1;
}

void FScene::AddPrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, const TOptional<FTransform>& PreviousTransform)
{
	check(IsInRenderingThread());
	check(PrimitiveSceneInfo->PackedIndex == INDEX_NONE);
	check(AddedPrimitiveSceneInfos.Find(PrimitiveSceneInfo) == nullptr);
	AddedPrimitiveSceneInfos.FindOrAdd(PrimitiveSceneInfo);
	if (PreviousTransform.IsSet())
	{
		OverridenPreviousTransforms.Update(PrimitiveSceneInfo, PreviousTransform.GetValue().ToMatrixWithScale());
	}
}

/**
 * Verifies that a component is added to the proper scene
 *
 * @param Component Component to verify
 * @param World World who's scene the primitive is being attached to
 */
FORCEINLINE static void VerifyProperPIEScene(UPrimitiveComponent* Component, UWorld* World)
{
	checkfSlow(Component->GetOuter() == GetTransientPackage() || 
		(FPackageName::GetLongPackageAssetName(Component->GetOutermostObject()->GetPackage()->GetName()).StartsWith(PLAYWORLD_PACKAGE_PREFIX) == 
		FPackageName::GetLongPackageAssetName(World->GetPackage()->GetName()).StartsWith(PLAYWORLD_PACKAGE_PREFIX)),
		TEXT("The component %s was added to the wrong world's scene (due to PIE). The callstack should tell you why"), 
		*Component->GetFullName()
		);
}

void FPersistentUniformBuffers::Clear()
{
	for (auto& UniformBuffer : MobileDirectionalLightUniformBuffers)
	{
		UniformBuffer.SafeRelease();
	}
	MobileSkyReflectionUniformBuffer.SafeRelease();

	Initialize();
}

void FPersistentUniformBuffers::Initialize()
{
	FViewUniformShaderParameters ViewUniformBufferParameters;


	FMobileDirectionalLightShaderParameters MobileDirectionalLightShaderParameters = {};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(MobileDirectionalLightUniformBuffers); ++Index)
	{
		// UniformBuffer_SingleFrame here is an optimization as this buffer gets uploaded everyframe
		MobileDirectionalLightUniformBuffers[Index] = TUniformBufferRef<FMobileDirectionalLightShaderParameters>::CreateUniformBufferImmediate(MobileDirectionalLightShaderParameters, UniformBuffer_MultiFrame, EUniformBufferValidation::None);
	}

	const FMobileReflectionCaptureShaderParameters* DefaultMobileSkyReflectionParameters = (const FMobileReflectionCaptureShaderParameters*)GDefaultMobileReflectionCaptureUniformBuffer.GetContents();
	MobileSkyReflectionUniformBuffer = TUniformBufferRef<FMobileReflectionCaptureShaderParameters>::CreateUniformBufferImmediate(*DefaultMobileSkyReflectionParameters, UniformBuffer_MultiFrame, EUniformBufferValidation::None);
}

TSet<IPersistentViewUniformBufferExtension*> PersistentViewUniformBufferExtensions;

void FRendererModule::RegisterPersistentViewUniformBufferExtension(IPersistentViewUniformBufferExtension* Extension)
{
	PersistentViewUniformBufferExtensions.Add(Extension);
}

FScene::FScene(UWorld* InWorld, bool bInRequiresHitProxies, bool bInIsEditorScene, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel)
:	FSceneInterface(InFeatureLevel)
,	World(InWorld)
,	FXSystem(nullptr)
,	bScenesPrimitivesNeedStaticMeshElementUpdate(false)
,   PathTracingInvalidationCounter(0)
#if RHI_RAYTRACING
,   CachedRayTracingMeshCommandsMode(ERayTracingMeshCommandsMode::RAY_TRACING)
#endif
,	SkyLight(NULL)
,	ConvolvedSkyRenderTargetReadyIndex(-1)
,   PathTracingSkylightColor(0, 0, 0, 0)
,	SimpleDirectionalLight(NULL)
,	ReflectionSceneData(InFeatureLevel)
,	IndirectLightingCache(InFeatureLevel)
,	VolumetricLightmapSceneData(this)
,	DistanceFieldSceneData(GShaderPlatformForFeatureLevel[InFeatureLevel])
,	DefaultLumenSceneData(nullptr)
,	PreshadowCacheLayout(0, 0, 0, 0, false)
,	SkyAtmosphere(NULL)
,	VolumetricCloud(NULL)
,	PrecomputedVisibilityHandler(NULL)
,	LocalShadowCastingLightOctree(FVector::ZeroVector, UE_OLD_HALF_WORLD_MAX)
,	PrimitiveOctree(FVector::ZeroVector, HALF_WORLD_MAX)
,	bRequiresHitProxies(bInRequiresHitProxies)
,	bIsEditorScene(bInIsEditorScene)
,	NumUncachedStaticLightingInteractions(0)
,	NumUnbuiltReflectionCaptures(0)
,	NumMobileStaticAndCSMLights_RenderThread(0)
,	NumMobileMovableDirectionalLights_RenderThread(0)
,	GPUSkinCache(nullptr)
,	SceneLODHierarchy(this)
,	RuntimeVirtualTexturePrimitiveHideMaskEditor(0)
,	RuntimeVirtualTexturePrimitiveHideMaskGame(0)
,	DefaultMaxDistanceFieldOcclusionDistance(InWorld->GetWorldSettings()->DefaultMaxDistanceFieldOcclusionDistance)
,	GlobalDistanceFieldViewDistance(InWorld->GetWorldSettings()->GlobalDistanceFieldViewDistance)
,	DynamicIndirectShadowsSelfShadowingIntensity(FMath::Clamp(InWorld->GetWorldSettings()->DynamicIndirectShadowsSelfShadowingIntensity, 0.0f, 1.0f))
,	ReadOnlyCVARCache(FReadOnlyCVARCache::Get())
#if RHI_RAYTRACING
,	RayTracingDynamicGeometryCollection(nullptr)
,	RayTracingSkinnedGeometryUpdateQueue(nullptr)
#endif
,	NumVisibleLights_GameThread(0)
,	NumEnabledSkylights_GameThread(0)
,	SceneFrameNumber(0)
,	SceneFrameNumberRenderThread(0)
,	bForceNoPrecomputedLighting(InWorld->GetWorldSettings()->bForceNoPrecomputedLighting)
{
	FMemory::Memzero(MobileDirectionalLights);
	FMemory::Memzero(AtmosphereLights);

	check(World);
	World->Scene = this;

	FeatureLevel = World->FeatureLevel;

	GPUScene.SetEnabled(FeatureLevel);

	if (World->FXSystem)
	{
		FFXSystemInterface::Destroy(World->FXSystem);
	}

	if (bCreateFXSystem)
	{
		World->CreateFXSystem();
	}
	else
	{
		World->FXSystem = NULL;
		SetFXSystem(NULL);
	}

	if (IsGPUSkinCacheAvailable(GetFeatureLevelShaderPlatform(InFeatureLevel)))
	{
		const bool bRequiresMemoryLimit = !bInIsEditorScene;
		GPUSkinCache = new FGPUSkinCache(InFeatureLevel, bRequiresMemoryLimit, World);
	}

	ComputeSystemInterface::CreateWorkers(this, ComputeTaskWorkers);

#if RHI_RAYTRACING
	if (IsRayTracingAllowed())
	{
		RayTracingDynamicGeometryCollection = new FRayTracingDynamicGeometryCollection();
		RayTracingSkinnedGeometryUpdateQueue = new FRayTracingSkinnedGeometryUpdateQueue();
	}
#endif

	World->UpdateParameterCollectionInstances(false, false);

	FPersistentUniformBuffers* PersistentUniformBuffers = &UniformBuffers;
	ENQUEUE_RENDER_COMMAND(InitializeUniformBuffers)(
		[PersistentUniformBuffers](FRHICommandListImmediate& RHICmdList)
	{
		PersistentUniformBuffers->Initialize();
	});

	UpdateEarlyZPassMode();

	DefaultLumenSceneData = new FLumenSceneData(GShaderPlatformForFeatureLevel[InFeatureLevel], InWorld->WorldType);

	// We use a default Virtual Shadow Map cache, if one hasn't been allocated for a specific view.  GPU resources for
	// a cache aren't allocated until the cache is actually used, so this shouldn't waste any GPU memory when not in use.
	DefaultVirtualShadowMapCache = new FVirtualShadowMapArrayCacheManager(this);

	SceneLightInfoUpdates = new FSceneLightInfoUpdates;
}

FScene::~FScene()
{
#if 0 // if you have component that has invalid scene, try this code to see this is reason. 
	for (FThreadSafeObjectIterator Iter(UActorComponent::StaticClass()); Iter; ++Iter)
	{
		UActorComponent * ActorComp = CastChecked<UActorComponent>(*Iter);
		if (ActorComp->GetScene() == this)
		{
			UE_LOG(LogRenderer, Log, TEXT("%s's scene is going to get invalidated"), *ActorComp->GetName());
		}
	}
#endif

	checkf(RemovedPrimitiveSceneInfos.Num() == 0, TEXT("All pending primitive removal operations are expected to be flushed when the scene is destroyed. Remaining operations are likely to cause a memory leak."));
	checkf(AddedPrimitiveSceneInfos.Num() == 0, TEXT("All pending primitive addition operations are expected to be flushed when the scene is destroyed. Remaining operations are likely to cause a memory leak."));
	checkf(Primitives.Num() == 0, TEXT("All primitives are expected to be removed before the scene is destroyed. Remaining primitives are likely to cause a memory leak."));

	// Unlink any view states from the scene
	for (FSceneViewState* ViewState : ViewStates)
	{
		check(ViewState->Scene == this);
		ViewState->Scene = nullptr;

		if (ViewState->ViewVirtualShadowMapCache)
		{
			delete ViewState->ViewVirtualShadowMapCache;
			ViewState->ViewVirtualShadowMapCache = nullptr;
		}
	}
	ViewStates.Empty();

	// Delete default cache
	if (DefaultVirtualShadowMapCache)
	{
		delete DefaultVirtualShadowMapCache;
		DefaultVirtualShadowMapCache = nullptr;
	}

	if (DefaultLumenSceneData)
	{
		delete DefaultLumenSceneData;
		DefaultLumenSceneData = nullptr;
	}

	for (FLumenSceneDataMap::TConstIterator LumenSceneData(PerViewOrGPULumenSceneData); LumenSceneData; ++LumenSceneData)
	{
		delete LumenSceneData.Value();
	}
	PerViewOrGPULumenSceneData.Empty();

	ReflectionSceneData.CubemapArray.ReleaseResource();
	IndirectLightingCache.ReleaseResource();
	DistanceFieldSceneData.Release();

	delete GPUSkinCache;
	GPUSkinCache = nullptr;

	ComputeSystemInterface::DestroyWorkers(this, ComputeTaskWorkers);

#if RHI_RAYTRACING
	delete RayTracingDynamicGeometryCollection;
	RayTracingDynamicGeometryCollection = nullptr;
	delete RayTracingSkinnedGeometryUpdateQueue;
	RayTracingSkinnedGeometryUpdateQueue = nullptr;
#endif // RHI_RAYTRACING

	checkf(RemovedPrimitiveSceneInfos.Num() == 0, TEXT("Leaking %d FPrimitiveSceneInfo instances."), RemovedPrimitiveSceneInfos.Num()); // Ensure UpdateAllPrimitiveSceneInfos() is called before destruction.

	delete SceneLightInfoUpdates;
}

void FScene::AddPrimitive(UPrimitiveComponent* Primitive)
{
	LLM_SCOPED_TAG_WITH_OBJECT_IN_SET(Primitive->GetOutermost(), ELLMTagSet::Assets);

	SCOPE_CYCLE_COUNTER(STAT_AddScenePrimitiveGT);
	SCOPED_NAMED_EVENT(FScene_AddPrimitive, FColor::Green);

	checkf(!Primitive->IsUnreachable(), TEXT("%s"), *Primitive->GetFullName());

	const float WorldTime = GetWorld()->GetTimeSeconds();
	// Save the world transform for next time the primitive is added to the scene
	float DeltaTime = WorldTime - Primitive->LastSubmitTime;
	if ( DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f )
	{
		// Time was reset?
		Primitive->LastSubmitTime = WorldTime;
	}
	else if ( DeltaTime > 0.0001f )
	{
		// First call for the new frame?
		Primitive->LastSubmitTime = WorldTime;
	}

	checkf(!Primitive->SceneProxy, TEXT("Primitive has already been added to the scene!"));

	// Create the primitive's scene proxy.
	FPrimitiveSceneProxy* PrimitiveSceneProxy = Primitive->CreateSceneProxy();
	Primitive->SceneProxy = PrimitiveSceneProxy;
	if(!PrimitiveSceneProxy)
	{
		// Primitives which don't have a proxy are irrelevant to the scene manager.
		return;
	}

	// Create the primitive scene info.
	FPrimitiveSceneInfo* PrimitiveSceneInfo = new FPrimitiveSceneInfo(Primitive, this);
	PrimitiveSceneProxy->PrimitiveSceneInfo = PrimitiveSceneInfo;

	// Cache the primitives initial transform.
	FMatrix RenderMatrix = Primitive->GetRenderMatrix();
	FVector AttachmentRootPosition = Primitive->GetActorPositionForRenderer();

	struct FCreateRenderThreadParameters
	{
		FPrimitiveSceneProxy* PrimitiveSceneProxy;
		FMatrix RenderMatrix;
		FBoxSphereBounds WorldBounds;
		FVector AttachmentRootPosition;
		FBoxSphereBounds LocalBounds;
	};
	FCreateRenderThreadParameters Params =
	{
		PrimitiveSceneProxy,
		RenderMatrix,
		Primitive->Bounds,
		AttachmentRootPosition,
		Primitive->GetLocalBounds()
	};

	// Help track down primitive with bad bounds way before the it gets to the Renderer
	ensureMsgf(!Primitive->Bounds.ContainsNaN(),
			TEXT("Nans found on Bounds for Primitive %s: Origin %s, BoxExtent %s, SphereRadius %f"), *Primitive->GetName(), *Primitive->Bounds.Origin.ToString(), *Primitive->Bounds.BoxExtent.ToString(), Primitive->Bounds.SphereRadius);

	INC_DWORD_STAT_BY( STAT_GameToRendererMallocTotal, PrimitiveSceneProxy->GetMemoryFootprint() + PrimitiveSceneInfo->GetMemoryFootprint() );

	// Verify the primitive is valid
	VerifyProperPIEScene(Primitive, World);

	// Increment the attachment counter, the primitive is about to be attached to the scene.
	Primitive->AttachmentCounter.Increment();

	// Create any RenderThreadResources required and send a command to the rendering thread to add the primitive to the scene.
	FScene* Scene = this;

	// If this primitive has a simulated previous transform, ensure that the velocity data for the scene representation is correct
	TOptional<FTransform> PreviousTransform = FMotionVectorSimulation::Get().GetPreviousTransform(Primitive);

	ENQUEUE_RENDER_COMMAND(AddPrimitiveCommand)(
		[Params = MoveTemp(Params), Scene, PrimitiveSceneInfo, PreviousTransform = MoveTemp(PreviousTransform)](FRHICommandListImmediate& RHICmdList)
		{
			FPrimitiveSceneProxy* SceneProxy = Params.PrimitiveSceneProxy;
			FScopeCycleCounter Context(SceneProxy->GetStatId());
			SceneProxy->SetTransform(Params.RenderMatrix, Params.WorldBounds, Params.LocalBounds, Params.AttachmentRootPosition);

			// Create any RenderThreadResources required.
			SceneProxy->CreateRenderThreadResources();

			Scene->AddPrimitiveSceneInfo_RenderThread(PrimitiveSceneInfo, PreviousTransform);
		});

}

static int32 GWarningOnRedundantTransformUpdate = 0;
static FAutoConsoleVariableRef CVarWarningOnRedundantTransformUpdate(
	TEXT("r.WarningOnRedundantTransformUpdate"),
	GWarningOnRedundantTransformUpdate,
	TEXT("Produce a warning when UpdatePrimitiveTransform is called redundantly."),
	ECVF_Default
);

static int32 GSkipRedundantTransformUpdate = 1;
static FAutoConsoleVariableRef CVarSkipRedundantTransformUpdate(
	TEXT("r.SkipRedundantTransformUpdate"),
	GSkipRedundantTransformUpdate,
	TEXT("Skip updates UpdatePrimitiveTransform is called redundantly, if the proxy allows it."),
	ECVF_Default
);

void FScene::UpdatePrimitiveTransform_RenderThread(FPrimitiveSceneProxy* PrimitiveSceneProxy, const FBoxSphereBounds& WorldBounds, const FBoxSphereBounds& LocalBounds, const FMatrix& LocalToWorld, const FVector& AttachmentRootPosition, const TOptional<FTransform>& PreviousTransform)
{
	check(IsInRenderingThread());

#if VALIDATE_PRIMITIVE_PACKED_INDEX
	if (AddedPrimitiveSceneInfos.Find(PrimitiveSceneProxy->GetPrimitiveSceneInfo()) != nullptr)
	{
		check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex == INDEX_NONE);
	}
	else
	{
		check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex != INDEX_NONE);
	}

	check(RemovedPrimitiveSceneInfos.Find(PrimitiveSceneProxy->GetPrimitiveSceneInfo()) == nullptr);
#endif

	UpdatedTransforms.Update(PrimitiveSceneProxy, { WorldBounds, LocalBounds, LocalToWorld, AttachmentRootPosition });

	if (PreviousTransform.IsSet())
	{
		OverridenPreviousTransforms.Update(PrimitiveSceneProxy->GetPrimitiveSceneInfo(), PreviousTransform.GetValue().ToMatrixWithScale());
	}
}

void FScene::UpdatePrimitiveOcclusionBoundsSlack_RenderThread(const FPrimitiveSceneProxy* PrimitiveSceneProxy, float NewSlack)
{
	check(IsInRenderingThread());

#if VALIDATE_PRIMITIVE_PACKED_INDEX
	if (AddedPrimitiveSceneInfos.Find(PrimitiveSceneProxy->GetPrimitiveSceneInfo()) != nullptr)
	{
		check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex == INDEX_NONE);
	}
	else
	{
		check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex != INDEX_NONE);
	}

	check(RemovedPrimitiveSceneInfos.Find(PrimitiveSceneProxy->GetPrimitiveSceneInfo()) == nullptr);
#endif

	UpdatedOcclusionBoundsSlacks.Update(PrimitiveSceneProxy, NewSlack);
}

void FScene::UpdatePrimitiveTransform(UPrimitiveComponent* Primitive)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveTransformGT);
	SCOPED_NAMED_EVENT(FScene_UpdatePrimitiveTransform, FColor::Yellow);

	// Save the world transform for next time the primitive is added to the scene
	const float WorldTime = GetWorld()->GetTimeSeconds();
	float DeltaTime = WorldTime - Primitive->LastSubmitTime;
	if (DeltaTime < -0.0001f || Primitive->LastSubmitTime < 0.0001f)
	{
		// Time was reset?
		Primitive->LastSubmitTime = WorldTime;
	}
	else if (DeltaTime > 0.0001f)
	{
		// First call for the new frame?
		Primitive->LastSubmitTime = WorldTime;
	}

	if (Primitive->SceneProxy)
	{
		// Check if the primitive needs to recreate its proxy for the transform update.
		if (Primitive->ShouldRecreateProxyOnUpdateTransform())
		{
			// Re-add the primitive from scratch to recreate the primitive's proxy.
			RemovePrimitive(Primitive);
			AddPrimitive(Primitive);
		}
		else
		{
			FVector AttachmentRootPosition = Primitive->GetActorPositionForRenderer();

			struct FPrimitiveUpdateParams
			{
				FScene* Scene;
				FPrimitiveSceneProxy* PrimitiveSceneProxy;
				FBoxSphereBounds WorldBounds;
				FBoxSphereBounds LocalBounds;
				FMatrix LocalToWorld;
				TOptional<FTransform> PreviousTransform;
				FVector AttachmentRootPosition;
			};

			FPrimitiveUpdateParams UpdateParams;
			UpdateParams.Scene = this;
			UpdateParams.PrimitiveSceneProxy = Primitive->SceneProxy;
			UpdateParams.WorldBounds = Primitive->Bounds;
			UpdateParams.LocalToWorld = Primitive->GetRenderMatrix();
			UpdateParams.AttachmentRootPosition = AttachmentRootPosition;
			UpdateParams.LocalBounds = Primitive->GetLocalBounds();
			UpdateParams.PreviousTransform = FMotionVectorSimulation::Get().GetPreviousTransform(Primitive);

			// Help track down primitive with bad bounds way before the it gets to the renderer.
			ensureMsgf(!UpdateParams.WorldBounds.BoxExtent.ContainsNaN() && !UpdateParams.WorldBounds.Origin.ContainsNaN() && !FMath::IsNaN(UpdateParams.WorldBounds.SphereRadius) && FMath::IsFinite(UpdateParams.WorldBounds.SphereRadius),
				TEXT("NaNs found on Bounds for Primitive %s: Owner: %s, Resource: %s, Level: %s, Origin: %s, BoxExtent: %s, SphereRadius: %f"),
				*Primitive->GetName(),
				*Primitive->SceneProxy->GetOwnerName().ToString(),
				*Primitive->SceneProxy->GetResourceName().ToString(),
				*Primitive->SceneProxy->GetLevelName().ToString(),
				*UpdateParams.WorldBounds.Origin.ToString(),
				*UpdateParams.WorldBounds.BoxExtent.ToString(),
				UpdateParams.WorldBounds.SphereRadius
			);

			bool bPerformUpdate = true;

			const bool bAllowSkip = GSkipRedundantTransformUpdate && Primitive->SceneProxy->CanSkipRedundantTransformUpdates();
			if (bAllowSkip || GWarningOnRedundantTransformUpdate)
			{
				if (Primitive->SceneProxy->WouldSetTransformBeRedundant_AnyThread(
					UpdateParams.LocalToWorld,
					UpdateParams.WorldBounds,
					UpdateParams.LocalBounds,
					UpdateParams.AttachmentRootPosition))
				{
					if (bAllowSkip)
					{
						// Do not perform the transform update!
						bPerformUpdate = false;
					}
					else
					{
						// Not skipping, and warnings are enabled.
						UE_LOG(LogRenderer, Warning,
							TEXT("Redundant UpdatePrimitiveTransform for Primitive %s: Owner: %s, Resource: %s, Level: %s"),
							*Primitive->GetName(),
							*Primitive->SceneProxy->GetOwnerName().ToString(),
							*Primitive->SceneProxy->GetResourceName().ToString(),
							*Primitive->SceneProxy->GetLevelName().ToString()
						);
					}
				}
			}

			if (bPerformUpdate)
			{
				ENQUEUE_RENDER_COMMAND(UpdateTransformCommand)(
					[UpdateParams](FRHICommandListImmediate& RHICmdList)
					{
						FScopeCycleCounter Context(UpdateParams.PrimitiveSceneProxy->GetStatId());
						UpdateParams.Scene->UpdatePrimitiveTransform_RenderThread(
							UpdateParams.PrimitiveSceneProxy,
							UpdateParams.WorldBounds,
							UpdateParams.LocalBounds,
							UpdateParams.LocalToWorld,
							UpdateParams.AttachmentRootPosition,
							UpdateParams.PreviousTransform
						);
					}
				);
			}
		}
	}
	else
	{
		// If the primitive doesn't have a scene info object yet, it must be added from scratch.
		AddPrimitive(Primitive);
	}
}

void FScene::UpdatePrimitiveOcclusionBoundsSlack(UPrimitiveComponent* Primitive, float NewSlack)
{
	if (Primitive->SceneProxy)
	{
		const FPrimitiveSceneProxy* SceneProxy = Primitive->SceneProxy;
		ENQUEUE_RENDER_COMMAND(UpdateOcclusionBoundsSlackCmd)(
			[this, SceneProxy, NewSlack](FRHICommandListImmediate&)
			{
				UpdatePrimitiveOcclusionBoundsSlack_RenderThread(SceneProxy, NewSlack);
			});
	}
}

void FScene::UpdatePrimitiveInstances(UInstancedStaticMeshComponent* Primitive)
{
	SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveInstanceGT);
	SCOPED_NAMED_EVENT(FScene_UpdatePrimitiveInstance, FColor::Yellow);

	if (Primitive->SceneProxy)
	{
		FUpdateInstanceCommand UpdateParams;
		UpdateParams.PrimitiveSceneProxy = Primitive->SceneProxy;
		UpdateParams.WorldBounds = Primitive->Bounds;
		UpdateParams.LocalBounds = Cast<UPrimitiveComponent>(Primitive)->GetLocalBounds();

		// #todo (jnadro) This code should not be dependent on static mesh bounds.
		UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Primitive);
		UpdateParams.StaticMeshBounds = StaticMeshComponent ? StaticMeshComponent->GetStaticMesh()->GetBounds() : FBoxSphereBounds();
		UpdateParams.CmdBuffer = Primitive->InstanceUpdateCmdBuffer;	// Copy

		ENQUEUE_RENDER_COMMAND(UpdateInstanceCommand)(
			[this, UpdateParams = MoveTemp(UpdateParams)](FRHICommandListImmediate& RHICmdList)
			{
#if VALIDATE_PRIMITIVE_PACKED_INDEX
				if (AddedPrimitiveSceneInfos.Find(UpdateParams.PrimitiveSceneProxy->GetPrimitiveSceneInfo()) != nullptr)
				{
					check(UpdateParams.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex == INDEX_NONE);
				}
				else
				{
					check(UpdateParams.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex != INDEX_NONE);
				}

				check(RemovedPrimitiveSceneInfos.Find(UpdateParams.PrimitiveSceneProxy->GetPrimitiveSceneInfo()) == nullptr);
#endif
				FScopeCycleCounter Context(UpdateParams.PrimitiveSceneProxy->GetStatId());
				UpdatedInstances.Update(UpdateParams.PrimitiveSceneProxy, UpdateParams);
			}
		);
	}
 	else
 	{
 		// If the primitive doesn't have a scene info object yet, it must be added from scratch.
 		AddPrimitive(Primitive);
 	}
}

void FScene::UpdatePrimitiveSelectedState_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsSelected)
{
	check(IsInRenderingThread());

#if WITH_EDITOR
	if (PrimitiveSceneInfo)
	{
		if (PrimitiveSceneInfo->GetIndex() != INDEX_NONE)
		{
			PrimitivesSelected[PrimitiveSceneInfo->GetIndex()] = bIsSelected;
		}
	}
#endif // WITH_EDITOR
}

void FScene::UpdatePrimitiveLightingAttachmentRoot(UPrimitiveComponent* Primitive)
{
	const UPrimitiveComponent* NewLightingAttachmentRoot = Primitive->GetLightingAttachmentRoot();

	if (NewLightingAttachmentRoot == Primitive)
	{
		NewLightingAttachmentRoot = NULL;
	}

	FPrimitiveComponentId NewComponentId = NewLightingAttachmentRoot ? NewLightingAttachmentRoot->ComponentId : FPrimitiveComponentId();

	if (Primitive->SceneProxy)
	{
		FPrimitiveSceneProxy* Proxy = Primitive->SceneProxy;
		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(UpdatePrimitiveAttachment)(
			[Scene, Proxy, NewComponentId](FRHICommandList&)
			{
				FPrimitiveSceneInfo* PrimitiveInfo = Proxy->GetPrimitiveSceneInfo();
				Scene->UpdatedAttachmentRoots.Update(PrimitiveInfo, NewComponentId);
			});
	}
}

void FScene::UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive)
{
	TArray<USceneComponent*, TInlineAllocator<1> > ProcessStack;
	ProcessStack.Push(Primitive);

	// Walk down the tree updating, because the scene's attachment data structures must be updated if the root of the attachment tree changes
	while (ProcessStack.Num() > 0)
	{
		USceneComponent* Current = ProcessStack.Pop(/*bAllowShrinking=*/ false);
		if (Current)
		{
			UPrimitiveComponent* CurrentPrimitive = Cast<UPrimitiveComponent>(Current);

			if (CurrentPrimitive
				&& CurrentPrimitive->GetWorld() 
				&& CurrentPrimitive->GetWorld()->Scene == this
				&& CurrentPrimitive->ShouldComponentAddToScene())
			{
				UpdatePrimitiveLightingAttachmentRoot(CurrentPrimitive);
			}

			ProcessStack.Append(Current->GetAttachChildren());
		}
	}
}

void FScene::UpdateCustomPrimitiveData(UPrimitiveComponent* Primitive)
{
	// This path updates the primitive data directly in the GPUScene. 
	if(Primitive->SceneProxy) 
	{
		struct FUpdateParams
		{
			FScene* Scene;
			FPrimitiveSceneProxy* PrimitiveSceneProxy;
			FCustomPrimitiveData CustomPrimitiveData;
		};

		FUpdateParams UpdateParams;
		UpdateParams.Scene = this;
		UpdateParams.PrimitiveSceneProxy = Primitive->SceneProxy;
		UpdateParams.CustomPrimitiveData = Primitive->GetCustomPrimitiveData(); 

		ENQUEUE_RENDER_COMMAND(UpdateCustomPrimitiveDataCommand)(
			[UpdateParams](FRHICommandListImmediate& RHICmdList)
			{
				UpdateParams.Scene->UpdatedCustomPrimitiveParams.Update(UpdateParams.PrimitiveSceneProxy, UpdateParams.CustomPrimitiveData);
			});
	}
}

void FScene::UpdatePrimitiveDistanceFieldSceneData_GameThread(UPrimitiveComponent* Primitive)
{
	check(IsInGameThread());

	if (Primitive->SceneProxy)
	{
		Primitive->LastSubmitTime = GetWorld()->GetTimeSeconds();

		ENQUEUE_RENDER_COMMAND(UpdatePrimDFSceneDataCmd)(
			[this, PrimitiveSceneProxy = Primitive->SceneProxy](FRHICommandList&)
			{
				if (PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
				{
					FPrimitiveSceneInfo* Info = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
					this->DistanceFieldSceneDataUpdates.FindOrAdd(Info);
				}
			});
	}
}

FPrimitiveSceneInfo* FScene::GetPrimitiveSceneInfo(int32 PrimitiveIndex)
{
	if(Primitives.IsValidIndex(PrimitiveIndex))
	{
		return Primitives[PrimitiveIndex];
	}
	return NULL;
}

FPrimitiveSceneInfo* FScene::GetPrimitiveSceneInfo(const FPersistentPrimitiveIndex& PersistentPrimitiveIndex)
{
	int32 PrimitiveIndex = GetPrimitiveIndex(PersistentPrimitiveIndex);
	return GetPrimitiveSceneInfo(PrimitiveIndex);
}

void FScene::RemovePrimitiveSceneInfo_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	check(IsInRenderingThread());

	if (AddedPrimitiveSceneInfos.Remove(PrimitiveSceneInfo))
	{
		check(PrimitiveSceneInfo->PackedIndex == INDEX_NONE);
		UpdatedTransforms.Remove(PrimitiveSceneInfo->Proxy);
		UpdatedCustomPrimitiveParams.Remove(PrimitiveSceneInfo->Proxy);
		OverridenPreviousTransforms.Remove(PrimitiveSceneInfo);
		UpdatedOcclusionBoundsSlacks.Remove(PrimitiveSceneInfo->Proxy);
		DistanceFieldSceneDataUpdates.Remove(PrimitiveSceneInfo);
		UpdatedAttachmentRoots.Remove(PrimitiveSceneInfo);

		{
			SCOPED_NAMED_EVENT(FScene_DeletePrimitiveSceneInfo, FColor::Red);
			// Delete the PrimitiveSceneInfo on the game thread after the rendering thread has processed its removal.
			// This must be done on the game thread because the hit proxy references (and possibly other members) need to be freed on the game thread.
			struct DeferDeleteHitProxies : FDeferredCleanupInterface
			{
				DeferDeleteHitProxies(TArray<TRefCountPtr<HHitProxy>>&& InHitProxies) : HitProxies(MoveTemp(InHitProxies)) {}
				TArray<TRefCountPtr<HHitProxy>> HitProxies;
			};

			BeginCleanup(new DeferDeleteHitProxies(MoveTemp(PrimitiveSceneInfo->HitProxies)));
			delete PrimitiveSceneInfo->Proxy;
			delete PrimitiveSceneInfo;
		}
	}
	else
	{
		check(PrimitiveSceneInfo->PackedIndex != INDEX_NONE);
		check(RemovedPrimitiveSceneInfos.Find(PrimitiveSceneInfo) == nullptr);
		RemovedPrimitiveSceneInfos.FindOrAdd(PrimitiveSceneInfo);
	}
}

void FScene::RemovePrimitive( UPrimitiveComponent* Primitive )
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveScenePrimitiveGT);
	SCOPED_NAMED_EVENT(FScene_RemovePrimitive, FColor::Yellow);

	FPrimitiveSceneProxy* PrimitiveSceneProxy = Primitive->SceneProxy;

	if(PrimitiveSceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();

		// Disassociate the primitive's scene proxy.
		Primitive->SceneProxy = NULL;

		// Send a command to the rendering thread to remove the primitive from the scene.
		FScene* Scene = this;
		FThreadSafeCounter* AttachmentCounter = &Primitive->AttachmentCounter;
		ENQUEUE_RENDER_COMMAND(FRemovePrimitiveCommand)(
			[Scene, PrimitiveSceneInfo, AttachmentCounter](FRHICommandList&)
			{
				PrimitiveSceneInfo->Proxy->DestroyRenderThreadResources();
				Scene->RemovePrimitiveSceneInfo_RenderThread(PrimitiveSceneInfo);
				AttachmentCounter->Decrement();
			});
	}
}

void FScene::ReleasePrimitive( UPrimitiveComponent* PrimitiveComponent )
{
	// Send a command to the rendering thread to clean up any state dependent on this primitive
	FScene* Scene = this;
	FPrimitiveComponentId PrimitiveComponentId = PrimitiveComponent->ComponentId;
	ENQUEUE_RENDER_COMMAND(FReleasePrimitiveCommand)(
		[Scene, PrimitiveComponentId](FRHICommandList&)
		{
			// Free the space in the indirect lighting cache
			Scene->IndirectLightingCache.ReleasePrimitive(PrimitiveComponentId);
		});
}

void FScene::AssignAvailableShadowMapChannelForLight(FLightSceneInfo* LightSceneInfo)
{
	FDynamicShadowMapChannelBindingHelper Helper;
	check(LightSceneInfo && LightSceneInfo->Proxy);

	// For lights with static shadowing, only check for lights intersecting the preview channel if any.
	if (LightSceneInfo->Proxy->HasStaticShadowing())
	{
		Helper.DisableAllOtherChannels(LightSceneInfo->GetDynamicShadowMapChannel());

		// If this static shadowing light does not need a (preview) channel, skip it.
		if (!Helper.HasAnyChannelEnabled())
		{
			return;
		}
	}
	else if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional)
	{
		// The implementation of forward lighting in ShadowProjectionPixelShader.usf does not support binding the directional light to channel 3.
		// This is related to the USE_FADE_PLANE feature that encodes the CSM blend factor the alpha channel.
		Helper.DisableChannel(3);
	}

	Helper.UpdateAvailableChannels(Lights, LightSceneInfo);

	const int32 NewChannelIndex = Helper.GetBestAvailableChannel();
	if (NewChannelIndex != INDEX_NONE)
	{
		// Unbind the channels previously allocated to lower priority lights.
		for (FLightSceneInfo* OtherLight : Helper.GetLights(NewChannelIndex))
		{
			OtherLight->SetDynamicShadowMapChannel(INDEX_NONE);
		}

		LightSceneInfo->SetDynamicShadowMapChannel(NewChannelIndex);

		// Try to assign new channels to lights that were just unbound.
		// Sort the lights so that they only get inserted once (prevents recursion).
		Helper.SortLightByPriority(NewChannelIndex);
		for (FLightSceneInfo* OtherLight : Helper.GetLights(NewChannelIndex))
		{
			AssignAvailableShadowMapChannelForLight(OtherLight);
		}
	}
	else
	{
		LightSceneInfo->SetDynamicShadowMapChannel(INDEX_NONE);
		OverflowingDynamicShadowedLights.AddUnique(LightSceneInfo->Proxy->GetOwnerNameOrLabel());
	}
}

void FScene::AddLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_AddSceneLightTime);
	SCOPED_NAMED_EVENT(FScene_AddLightSceneInfo_RenderThread, FColor::Green);

	check(LightSceneInfo->bVisible);

	// Add the light to the light list.
	LightSceneInfo->Id = Lights.Add(FLightSceneInfoCompact(LightSceneInfo));
	const FLightSceneInfoCompact& LightSceneInfoCompact = Lights[LightSceneInfo->Id];
	const ELightComponentType LightType = (ELightComponentType)LightSceneInfo->Proxy->GetLightType();
	const bool bDirectionalLight = LightType == LightType_Directional;

	if (bDirectionalLight)
	{
		DirectionalLights.Add(LightSceneInfo);
	}

	if (bDirectionalLight &&
		// Only use a stationary or movable light
		!(LightSceneInfo->Proxy->HasStaticLighting() 
		// if it is a Static DirectionalLight and the light has not been built, add it to MobileDirectionalLights for mobile preview.
		&& LightSceneInfo->IsPrecomputedLightingValid())
		)
	{
		// Set SimpleDirectionalLight
		if(!SimpleDirectionalLight)
		{
			SimpleDirectionalLight = LightSceneInfo;
		}

		if(GetShadingPath() == EShadingPath::Mobile)
		{
			const bool bUseCSMForDynamicObjects = LightSceneInfo->Proxy->UseCSMForDynamicObjects();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// these are tracked for disabled shader permutation warnings
			if (LightSceneInfo->Proxy->IsMovable())
			{
				NumMobileMovableDirectionalLights_RenderThread++;
			}
			if (bUseCSMForDynamicObjects)
			{
				NumMobileStaticAndCSMLights_RenderThread++;
			}
#endif
		    // Set MobileDirectionalLights entry
		    int32 FirstLightingChannel = GetFirstLightingChannelFromMask(LightSceneInfo->Proxy->GetLightingChannelMask());
		    if (FirstLightingChannel >= 0 && MobileDirectionalLights[FirstLightingChannel] == nullptr)
		    {
			    MobileDirectionalLights[FirstLightingChannel] = LightSceneInfo;
    
			    // if this light is a dynamic shadowcast then we need to update the static draw lists to pick a new lighting policy:
			    if (!LightSceneInfo->Proxy->HasStaticShadowing() || bUseCSMForDynamicObjects)
				{
		    		bScenesPrimitivesNeedStaticMeshElementUpdate = true;
				}
		    }
		}
	}

	// Register rect. light texture
	if (LightType == LightType_Rect)
	{
		FRectLightSceneProxy* RectProxy = (FRectLightSceneProxy*)LightSceneInfo->Proxy;
		RectProxy->RectAtlasId = RectLightAtlas::AddTexture(RectProxy->SourceTexture);
	}

	// Register IES texture
	if (UTextureLightProfile* IESTexture = LightSceneInfo->Proxy->GetIESTexture())
	{
		LightSceneInfo->Proxy->IESAtlasId = IESAtlas::AddTexture(IESTexture);
	}

	const EShaderPlatform ShaderPlatform = GetShaderPlatform();
	const bool bAssignShadowMapChannel = IsForwardShadingEnabled(ShaderPlatform) ||  (IsMobilePlatform(ShaderPlatform) && MobileUsesShadowMaskTexture(ShaderPlatform));
	if (bAssignShadowMapChannel && (LightSceneInfo->Proxy->CastsDynamicShadow() || LightSceneInfo->Proxy->GetLightFunctionMaterial()))
	{
		AssignAvailableShadowMapChannelForLight(LightSceneInfo);
	}

	ProcessAtmosphereLightAddition_RenderThread(LightSceneInfo);

	InvalidatePathTracedOutput();

	// Add the light to the scene.
	LightSceneInfo->AddToScene();
}

void FScene::AddLight(ULightComponent* Light)
{
	LLM_SCOPE(ELLMTag::SceneRender);

	// Create the light's scene proxy.
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();
	if(Proxy)
	{
		// Associate the proxy with the light.
		Light->SceneProxy = Proxy;

		// Update the light's transform and position.
		Proxy->SetTransform(Light->GetComponentTransform().ToMatrixNoScale(), Light->GetLightPosition());

		// Create the light scene info.
		Proxy->LightSceneInfo = new FLightSceneInfo(Proxy, true);

		INC_DWORD_STAT(STAT_SceneLights);

		// Adding a new light
		++NumVisibleLights_GameThread;

		// Send a command to the rendering thread to add the light to the scene.
		FLightSceneInfo* LightSceneInfo = Proxy->LightSceneInfo;
		ENQUEUE_RENDER_COMMAND(FAddLightCommand)(
			[this, LightSceneInfo](FRHICommandListImmediate& RHICmdList)
			{
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Scene_AddLight);
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
				bool bWasAlreadyInMap = false;
				SceneLightInfoUpdates->Commands.FindOrAdd(LightSceneInfo, FUpdateLightCommand(FUpdateLightCommand::EAddOrRemove::Add, LightSceneInfo), bWasAlreadyInMap);
				check(!bWasAlreadyInMap);
				++SceneLightInfoUpdates->NumAdds;
			});
	}
}

void FScene::AddInvisibleLight(ULightComponent* Light)
{
	// Create the light's scene proxy.
	FLightSceneProxy* Proxy = Light->CreateSceneProxy();

	if(Proxy)
	{
		// Associate the proxy with the light.
		Light->SceneProxy = Proxy;

		// Update the light's transform and position.
		Proxy->SetTransform(Light->GetComponentTransform().ToMatrixNoScale(),Light->GetLightPosition());

		// Create the light scene info.
		Proxy->LightSceneInfo = new FLightSceneInfo(Proxy, false);

		INC_DWORD_STAT(STAT_SceneLights);

		// Send a command to the rendering thread to add the light to the scene.
		FScene* Scene = this;
		FLightSceneInfo* LightSceneInfo = Proxy->LightSceneInfo;
		ENQUEUE_RENDER_COMMAND(FAddLightCommand)(
			[Scene, LightSceneInfo](FRHICommandListImmediate& RHICmdList)
			{
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
				LightSceneInfo->Id = Scene->InvisibleLights.Add(FLightSceneInfoCompact(LightSceneInfo));
			});
	}
}

void FScene::SetSkyLight(FSkyLightSceneProxy* LightProxy)
{
	check(LightProxy);
	NumEnabledSkylights_GameThread++;

	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FSetSkyLightCommand)
		([Scene, LightProxy](FRHICommandListImmediate& RHICmdList)
		{
			check(!Scene->SkyLightStack.Contains(LightProxy));
			Scene->SkyLightStack.Push(LightProxy);
			const bool bOriginalHadSkylight = Scene->ShouldRenderSkylightInBasePass(false);

			// Use the most recently enabled skylight
			Scene->SkyLight = LightProxy;

			const bool bNewHasSkylight = Scene->ShouldRenderSkylightInBasePass(false);

			if (bOriginalHadSkylight != bNewHasSkylight)
			{
				// Mark the scene as needing static draw lists to be recreated if needed
				// The base pass chooses shaders based on whether there's a skylight in the scene, and that is cached in static draw lists
				Scene->bScenesPrimitivesNeedStaticMeshElementUpdate = true;
			}
			Scene->InvalidatePathTracedOutput();
		});
}

void FScene::DisableSkyLight(FSkyLightSceneProxy* LightProxy)
{
	check(LightProxy);
	NumEnabledSkylights_GameThread--;

	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FDisableSkyLightCommand)
		([Scene, LightProxy](FRHICommandListImmediate& RHICmdList)
	{
		const bool bOriginalHadSkylight = Scene->ShouldRenderSkylightInBasePass(false);

		Scene->SkyLightStack.RemoveSingle(LightProxy);

		if (Scene->SkyLightStack.Num() > 0)
		{
			// Use the most recently enabled skylight
			Scene->SkyLight = Scene->SkyLightStack.Last();
		}
		else
		{
			Scene->SkyLight = NULL;
		}

		const bool bNewHasSkylight = Scene->ShouldRenderSkylightInBasePass(false);

		// Update the scene if we switched skylight enabled states
		if (bOriginalHadSkylight != bNewHasSkylight)
		{
			Scene->bScenesPrimitivesNeedStaticMeshElementUpdate = true;
		}
		Scene->InvalidatePathTracedOutput();
	});
}

bool FScene::HasSkyLightRequiringLightingBuild() const
{
	return SkyLight != nullptr && !SkyLight->IsMovable();
}

bool FScene::HasAtmosphereLightRequiringLightingBuild() const
{
	bool AnySunLightNotMovable = false;
	for (uint8 Index = 0; Index < NUM_ATMOSPHERE_LIGHTS; ++Index)
	{
		AnySunLightNotMovable |= AtmosphereLights[Index] != nullptr && !AtmosphereLights[Index]->Proxy->IsMovable();
	}
	return AnySunLightNotMovable;
}

void FScene::AddOrRemoveDecal_RenderThread(FDeferredDecalProxy* Proxy, bool bAdd)
{
	if(bAdd)
	{
		Decals.Add(Proxy);
		InvalidatePathTracedOutput();
	}
	else
	{
		// can be optimized
		for(TSparseArray<FDeferredDecalProxy*>::TIterator It(Decals); It; ++It)
		{
			FDeferredDecalProxy* CurrentProxy = *It;

			if (CurrentProxy == Proxy)
			{
				InvalidatePathTracedOutput();
				It.RemoveCurrent();
				delete CurrentProxy;
				break;
			}
		}
	}
}

void FScene::SetPhysicsField(FPhysicsFieldSceneProxy* PhysicsFieldSceneProxy)
{
	check(PhysicsFieldSceneProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FSetPhysicsFieldCommand)(
		[Scene, PhysicsFieldSceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->PhysicsField = PhysicsFieldSceneProxy;
		});
}

void FScene::ShowPhysicsField()
{
	// Set the shader print/debug values from game thread if
	// physics field visualisation has been enabled
	if (PhysicsField && PhysicsField->FieldResource && PhysicsField->FieldResource->FieldInfos.bShowFields)
	{
		// Force ShaderPrint on.
		ShaderPrint::SetEnabled(true);
		ShaderPrint::RequestSpaceForLines(128000);
	}
}

void FScene::ResetPhysicsField()
{
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FResetPhysicsFieldCommand)(
		[Scene](FRHICommandListImmediate& RHICmdList)
		{
			Scene->PhysicsField = nullptr;
		});
}

void FScene::UpdatePhysicsField(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	if (PhysicsField)
	{
		PhysicsField->FieldResource->FieldInfos.ViewOrigin = View.ViewMatrices.GetViewOrigin();
		if (View.Family )
		{
			PhysicsField->FieldResource->FieldInfos.bShowFields = View.Family->EngineShowFlags.PhysicsField;
		}
	}
}

void FScene::AddDecal(UDecalComponent* Component)
{
	if(!Component->SceneProxy)
	{
		// Create the decals's scene proxy.
		Component->SceneProxy = Component->CreateSceneProxy();

		INC_DWORD_STAT(STAT_SceneDecals);

		// Send a command to the rendering thread to add the light to the scene.
		FScene* Scene = this;
		FDeferredDecalProxy* Proxy = Component->SceneProxy;
		ENQUEUE_RENDER_COMMAND(FAddDecalCommand)(
			[Scene, Proxy](FRHICommandListImmediate& RHICmdList)
			{
				Scene->AddOrRemoveDecal_RenderThread(Proxy, true);
			});
	}
}

void FScene::RemoveDecal(UDecalComponent* Component)
{
	if(Component->SceneProxy)
	{
		DEC_DWORD_STAT(STAT_SceneDecals);

		// Send a command to the rendering thread to remove the light from the scene.
		FScene* Scene = this;
		FDeferredDecalProxy* Proxy = Component->SceneProxy;
		ENQUEUE_RENDER_COMMAND(FRemoveDecalCommand)(
			[Scene, Proxy](FRHICommandListImmediate& RHICmdList)
			{
				Scene->AddOrRemoveDecal_RenderThread(Proxy, false);
			});

		// Disassociate the primitive's scene proxy.
		Component->SceneProxy = NULL;
	}
}

void FScene::UpdateDecalTransform(UDecalComponent* Decal)
{
	if(Decal->SceneProxy)
	{
		//Send command to the rendering thread to update the decal's transform.
		FScene* Scene = this;
		FDeferredDecalProxy* DecalSceneProxy = Decal->SceneProxy;
		FTransform ComponentToWorldIncludingDecalSize = Decal->GetTransformIncludingDecalSize();
		FBoxSphereBounds Bounds = Decal->CalcBounds(Decal->GetComponentTransform());
		ENQUEUE_RENDER_COMMAND(UpdateTransformCommand)(
			[DecalSceneProxy, ComponentToWorldIncludingDecalSize, Bounds, Scene](FRHICommandListImmediate& RHICmdList)
			{
				// Invalidate the path tracer only if the decal was sufficiently moved
				if (!ComponentToWorldIncludingDecalSize.Equals(DecalSceneProxy->ComponentTrans, SMALL_NUMBER))
				{
					Scene->InvalidatePathTracedOutput();
				}
				// Update the primitive's transform.
				DecalSceneProxy->SetTransformIncludingDecalSize(ComponentToWorldIncludingDecalSize, Bounds);
			});
	}
}

void FScene::UpdateDecalFadeOutTime(UDecalComponent* Decal)
{
	FDeferredDecalProxy* Proxy = Decal->SceneProxy;
	if(Proxy)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float DecalFadeStartDelay = Decal->FadeStartDelay;
		float DecalFadeDuration = Decal->FadeDuration;

		ENQUEUE_RENDER_COMMAND(FUpdateDecalFadeInTimeCommand)(
			[Proxy, CurrentTime, DecalFadeStartDelay, DecalFadeDuration](FRHICommandListImmediate& RHICmdList)
		{
			if (DecalFadeDuration > 0.0f)
			{
				Proxy->InvFadeDuration = 1.0f / DecalFadeDuration;
				Proxy->FadeStartDelayNormalized = (CurrentTime + DecalFadeStartDelay + DecalFadeDuration) * Proxy->InvFadeDuration;
			}
			else
			{
				Proxy->InvFadeDuration = -1.0f;
				Proxy->FadeStartDelayNormalized = 1.0f;
			}
		});
	}
}

void FScene::UpdateDecalFadeInTime(UDecalComponent* Decal)
{
	FDeferredDecalProxy* Proxy = Decal->SceneProxy;
	if (Proxy)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float DecalFadeStartDelay = Decal->FadeInStartDelay;
		float DecalFadeDuration = Decal->FadeInDuration;

		ENQUEUE_RENDER_COMMAND(FUpdateDecalFadeInTimeCommand)(
			[Proxy, CurrentTime, DecalFadeStartDelay, DecalFadeDuration](FRHICommandListImmediate& RHICmdList)
		{
			if (DecalFadeDuration > 0.0f)
			{
				Proxy->InvFadeInDuration = 1.0f / DecalFadeDuration;
				Proxy->FadeInStartDelayNormalized = (CurrentTime + DecalFadeStartDelay) * -Proxy->InvFadeInDuration;
			}
			else
			{
				Proxy->InvFadeInDuration = 1.0f;
				Proxy->FadeInStartDelayNormalized = 0.0f;
			}
		});
	}
}

void FScene::BatchUpdateDecals(TArray<FDeferredDecalUpdateParams>&& UpdateParams)
{
	ENQUEUE_RENDER_COMMAND(FBatchUpdateDecalsCommand)(
		[Scene=this, UpdateParams_RT=MoveTemp(UpdateParams)](FRHICommandListImmediate& RHICmdList)
		{
			for (const FDeferredDecalUpdateParams& DecalUpdate : UpdateParams_RT )
			{
				if (DecalUpdate.OperationType == FDeferredDecalUpdateParams::EOperationType::RemoveFromSceneAndDelete)
				{
					Scene->AddOrRemoveDecal_RenderThread(DecalUpdate.DecalProxy, false);
					continue;
				}

				if (DecalUpdate.OperationType == FDeferredDecalUpdateParams::EOperationType::AddToSceneAndUpdate )
				{
					Scene->AddOrRemoveDecal_RenderThread(DecalUpdate.DecalProxy, true);
				}

				DecalUpdate.DecalProxy->SetTransformIncludingDecalSize(DecalUpdate.Transform, DecalUpdate.Bounds);
				DecalUpdate.DecalProxy->InitializeFadingParameters(DecalUpdate.AbsSpawnTime, DecalUpdate.FadeDuration, DecalUpdate.FadeStartDelay, DecalUpdate.FadeInDuration, DecalUpdate.FadeInStartDelay);
				DecalUpdate.DecalProxy->FadeScreenSize = DecalUpdate.FadeScreenSize;
				DecalUpdate.DecalProxy->SortOrder = DecalUpdate.SortOrder;
				DecalUpdate.DecalProxy->DecalColor = DecalUpdate.DecalColor;
			}
		}
	);
}

void FScene::AddHairStrands(FHairStrandsInstance* Proxy)
{
	if (Proxy)
	{
		check(IsInRenderingThread());
		const int32 PackedIndex = HairStrandsSceneData.RegisteredProxies.Add(Proxy);
		Proxy->RegisteredIndex = PackedIndex;
	}
}

void FScene::RemoveHairStrands(FHairStrandsInstance* Proxy)
{
	if (Proxy)
	{
		check(IsInRenderingThread());
		int32 ProxyIndex = Proxy->RegisteredIndex;
		if (HairStrandsSceneData.RegisteredProxies.IsValidIndex(ProxyIndex))
		{
			HairStrandsSceneData.RegisteredProxies.RemoveAtSwap(ProxyIndex);
		}
		Proxy->RegisteredIndex = -1;
		if (HairStrandsSceneData.RegisteredProxies.IsValidIndex(ProxyIndex))
		{
			FHairStrandsInstance* Other = HairStrandsSceneData.RegisteredProxies[ProxyIndex];
			Other->RegisteredIndex = ProxyIndex;
		}
	}
}


void FScene::GetLightIESAtlasSlot(const FLightSceneProxy* Proxy, FLightRenderParameters* Out)
{
	if (Proxy)
	{
		//check(!IsInGameThread());
		Out->IESAtlasIndex = IESAtlas::GetAtlasSlot(Proxy->IESAtlasId);
	}
}

void FScene::GetRectLightAtlasSlot(const FRectLightSceneProxy* Proxy, FLightRenderParameters* Out)
{
	if (Proxy)
	{
		//check(IsInRenderingThread());
		const RectLightAtlas::FAtlasSlotDesc Slot = RectLightAtlas::GetAtlasSlot(Proxy->RectAtlasId);
		Out->RectLightAtlasUVOffset = Slot.UVOffset;
		Out->RectLightAtlasUVScale = Slot.UVScale;
		Out->RectLightAtlasMaxLevel = Slot.MaxMipLevel;
	}
}

void FScene::AddReflectionCapture(UReflectionCaptureComponent* Component)
{
	if (!Component->SceneProxy)
	{
		Component->SceneProxy = Component->CreateSceneProxy();

		FScene* Scene = this;
		FReflectionCaptureProxy* Proxy = Component->SceneProxy;
		const FVector Position = Component->GetComponentLocation();

		ENQUEUE_RENDER_COMMAND(FAddCaptureCommand)
			([Scene, Proxy, Position](FRHICommandListImmediate& RHICmdList)
		{
			if (Proxy->bUsingPreviewCaptureData)
			{
				FPlatformAtomics::InterlockedIncrement(&Scene->NumUnbuiltReflectionCaptures);
			}

			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
			const int32 PackedIndex = Scene->ReflectionSceneData.RegisteredReflectionCaptures.Add(Proxy);

			Proxy->PackedIndex = PackedIndex;
			Scene->ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.Add(FSphere(Position, Proxy->InfluenceRadius));
			
			if (Scene->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
			{
				Proxy->UpdateMobileUniformBuffer();
			}

			checkSlow(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() == Scene->ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.Num());
		});
	}
}

void FScene::RemoveReflectionCapture(UReflectionCaptureComponent* Component)
{
	if (Component->SceneProxy)
	{
		FScene* Scene = this;
		FReflectionCaptureProxy* Proxy = Component->SceneProxy;

		ENQUEUE_RENDER_COMMAND(FRemoveCaptureCommand)
			([Scene, Proxy](FRHICommandListImmediate& RHICmdList)
		{
			if (Proxy->bUsingPreviewCaptureData)
			{
				FPlatformAtomics::InterlockedDecrement(&Scene->NumUnbuiltReflectionCaptures);
			}

			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;

			// Need to clear out all reflection captures on removal to avoid dangling pointers.
			for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); ++PrimitiveIndex)
			{
				Scene->Primitives[PrimitiveIndex]->RemoveCachedReflectionCaptures();
			}

			int32 CaptureIndex = Proxy->PackedIndex;
			Scene->ReflectionSceneData.RegisteredReflectionCaptures.RemoveAtSwap(CaptureIndex);
			Scene->ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.RemoveAtSwap(CaptureIndex);

			if (Scene->ReflectionSceneData.RegisteredReflectionCaptures.IsValidIndex(CaptureIndex))
			{
				FReflectionCaptureProxy* OtherCapture = Scene->ReflectionSceneData.RegisteredReflectionCaptures[CaptureIndex];
				OtherCapture->PackedIndex = CaptureIndex;
			}

			delete Proxy;

			checkSlow(Scene->ReflectionSceneData.RegisteredReflectionCaptures.Num() == Scene->ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.Num());
		});

		// Disassociate the primitive's scene proxy.
		Component->SceneProxy = NULL;
	}
}

void FScene::UpdateReflectionCaptureTransform(UReflectionCaptureComponent* Component)
{
	if (Component->SceneProxy)
	{
		const FReflectionCaptureMapBuildData* MapBuildData = Component->GetMapBuildData();
		bool bUsingPreviewCaptureData = MapBuildData == NULL;

		FScene* Scene = this;
		FReflectionCaptureProxy* Proxy = Component->SceneProxy;
		FMatrix Transform = Component->GetComponentTransform().ToMatrixWithScale();

		ENQUEUE_RENDER_COMMAND(FUpdateTransformCommand)
			([Scene, Proxy, Transform, bUsingPreviewCaptureData](FRHICommandListImmediate& RHICmdList)
		{
			if (Proxy->bUsingPreviewCaptureData)
			{
				FPlatformAtomics::InterlockedDecrement(&Scene->NumUnbuiltReflectionCaptures);
			}

			Proxy->bUsingPreviewCaptureData = bUsingPreviewCaptureData;

			if (Proxy->bUsingPreviewCaptureData)
			{
				FPlatformAtomics::InterlockedIncrement(&Scene->NumUnbuiltReflectionCaptures);
			}

			Scene->ReflectionSceneData.bRegisteredReflectionCapturesHasChanged = true;
			Proxy->SetTransform(Transform);

			if (Scene->GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
			{
				Proxy->UpdateMobileUniformBuffer();
			}
		});
	}
}

void FScene::ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent)
{
	bool bRemoved = false;
	for (TSparseArray<UReflectionCaptureComponent*>::TIterator It(ReflectionSceneData.AllocatedReflectionCapturesGameThread); It; ++It)
	{
		UReflectionCaptureComponent* CurrentCapture = *It;

		if (CurrentCapture == CaptureComponent)
		{
			It.RemoveCurrent();
			bRemoved = true;
			break;
		}
	}

	if (bRemoved)
	{
		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(RemoveCaptureCommand)(
			[CaptureComponent, Scene](FRHICommandListImmediate& RHICmdList)
			{
				int32 IndexToFree = -1;

				const FCaptureComponentSceneState* ComponentStatePtr = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Find(CaptureComponent);
				if (ComponentStatePtr)
				{
					// We track removed captures so we can remap them when reallocating the cubemap array
					check(ComponentStatePtr->CubemapIndex != -1);
					IndexToFree = ComponentStatePtr->CubemapIndex;
				}

				const bool bDidRemove = Scene->ReflectionSceneData.AllocatedReflectionCaptureState.Remove(CaptureComponent);
				if (bDidRemove && (IndexToFree != -1))
				{
					Scene->ReflectionSceneData.CubemapArraySlotsUsed[IndexToFree] = false;
				}
			});
	}
}

const FReflectionCaptureProxy* FScene::FindClosestReflectionCapture(FVector Position) const
{
	checkSlow(IsInParallelRenderingThread());
	float ClosestDistanceSquared = FLT_MAX;
	int32 ClosestInfluencingCaptureIndex = INDEX_NONE;

	// Linear search through the scene's reflection captures
	// ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius has been packed densely to make this coherent in memory
	for (int32 CaptureIndex = 0; CaptureIndex < ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.Num(); CaptureIndex++)
	{
		const FSphere& ReflectionCapturePositionAndRadius = ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius[CaptureIndex];

		const float DistanceSquared = (ReflectionCapturePositionAndRadius.Center - Position).SizeSquared();

		// If the Position is inside the InfluenceRadius of a ReflectionCapture
		if (DistanceSquared <= FMath::Square(ReflectionCapturePositionAndRadius.W))
		{
			// Choose the closest ReflectionCapture or record the first one found.
			if (ClosestInfluencingCaptureIndex == INDEX_NONE || DistanceSquared < ClosestDistanceSquared)
			{
				ClosestDistanceSquared = DistanceSquared;
				ClosestInfluencingCaptureIndex = CaptureIndex;
			}
		}
	}

	return ClosestInfluencingCaptureIndex != INDEX_NONE ? ReflectionSceneData.RegisteredReflectionCaptures[ClosestInfluencingCaptureIndex] : NULL;
}

const FPlanarReflectionSceneProxy* FScene::FindClosestPlanarReflection(const FBoxSphereBounds& Bounds) const
{
	checkSlow(IsInParallelRenderingThread());
	const FPlanarReflectionSceneProxy* ClosestPlanarReflection = NULL;
	float ClosestDistance = FLT_MAX;
	FBox PrimitiveBoundingBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);

	// Linear search through the scene's planar reflections
	for (int32 CaptureIndex = 0; CaptureIndex < PlanarReflections.Num(); CaptureIndex++)
	{
		FPlanarReflectionSceneProxy* CurrentPlanarReflection = PlanarReflections[CaptureIndex];
		const FBox ReflectionBounds = CurrentPlanarReflection->WorldBounds;

		if (PrimitiveBoundingBox.Intersect(ReflectionBounds))
		{
			const float Distance = FMath::Abs(CurrentPlanarReflection->ReflectionPlane.PlaneDot(Bounds.Origin));

			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestPlanarReflection = CurrentPlanarReflection;
			}
		}
	}

	return ClosestPlanarReflection;
}

const FPlanarReflectionSceneProxy* FScene::GetForwardPassGlobalPlanarReflection() const
{
	// For the forward pass just pick first planar reflection.

	if (PlanarReflections.Num() > 0)
	{
		return PlanarReflections[0];
	}

	return nullptr;
}

void FScene::FindClosestReflectionCaptures(FVector Position, const FReflectionCaptureProxy* (&SortedByDistanceOUT)[FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies]) const
{
	checkSlow(IsInParallelRenderingThread());
	static const int32 ArraySize = FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies;

	struct FReflectionCaptureDistIndex
	{
		int32 CaptureIndex;
		float CaptureDistance;
		const FReflectionCaptureProxy* CaptureProxy;
	};

	// Find the nearest n captures to this primitive. 
	const int32 NumRegisteredReflectionCaptures = ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius.Num();
	const int32 PopulateCaptureCount = FMath::Min(ArraySize, NumRegisteredReflectionCaptures);

	TArray<FReflectionCaptureDistIndex, TFixedAllocator<ArraySize>> ClosestCaptureIndices;
	ClosestCaptureIndices.AddUninitialized(PopulateCaptureCount);

	for (int32 CaptureIndex = 0; CaptureIndex < PopulateCaptureCount; CaptureIndex++)
	{
		ClosestCaptureIndices[CaptureIndex].CaptureIndex = CaptureIndex;
		ClosestCaptureIndices[CaptureIndex].CaptureDistance = (ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius[CaptureIndex].Center - Position).SizeSquared();
	}
	
	for (int32 CaptureIndex = PopulateCaptureCount; CaptureIndex < NumRegisteredReflectionCaptures; CaptureIndex++)
	{
		const float DistanceSquared = (ReflectionSceneData.RegisteredReflectionCapturePositionAndRadius[CaptureIndex].Center - Position).SizeSquared();
		for (int32 i = 0; i < ArraySize; i++)
		{
			if (DistanceSquared<ClosestCaptureIndices[i].CaptureDistance)
			{
				ClosestCaptureIndices[i].CaptureDistance = DistanceSquared;
				ClosestCaptureIndices[i].CaptureIndex = CaptureIndex;
				break;
			}
		}
	}

	for (int32 CaptureIndex = 0; CaptureIndex < PopulateCaptureCount; CaptureIndex++)
	{
		FReflectionCaptureProxy* CaptureProxy = ReflectionSceneData.RegisteredReflectionCaptures[ClosestCaptureIndices[CaptureIndex].CaptureIndex];		
		ClosestCaptureIndices[CaptureIndex].CaptureProxy = CaptureProxy;
	}
	// Sort by influence radius.
	ClosestCaptureIndices.Sort([](const FReflectionCaptureDistIndex& A, const FReflectionCaptureDistIndex& B)
		{
			if (A.CaptureProxy->InfluenceRadius != B.CaptureProxy->InfluenceRadius)
			{
				return (A.CaptureProxy->InfluenceRadius < B.CaptureProxy->InfluenceRadius);
			}
			return A.CaptureProxy->Guid < B.CaptureProxy->Guid;
		});

	FMemory::Memzero(SortedByDistanceOUT);

	for (int32 CaptureIndex = 0; CaptureIndex < PopulateCaptureCount; CaptureIndex++)
	{
		SortedByDistanceOUT[CaptureIndex] = ClosestCaptureIndices[CaptureIndex].CaptureProxy;
	}
}

int64 FScene::GetCachedWholeSceneShadowMapsSize() const
{
	int64 CachedShadowmapMemory = 0;

	for (TMap<int32, TArray<FCachedShadowMapData>>::TConstIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
	{
		const TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

		for (const auto& ShadowMapData : ShadowMapDatas)
		{
			if (ShadowMapData.ShadowMap.IsValid())
			{
				CachedShadowmapMemory += ShadowMapData.ShadowMap.ComputeMemorySize();
			}
		}
	}

	return CachedShadowmapMemory;
}

void FScene::AddPrecomputedLightVolume(const FPrecomputedLightVolume* Volume)
{
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(AddVolumeCommand)
		([Scene, Volume](FRHICommandListImmediate& RHICmdList) 
		{
			Scene->PrecomputedLightVolumes.Add(Volume);
			Scene->IndirectLightingCache.SetLightingCacheDirty(Scene, Volume);
		});
}

void FScene::RemovePrecomputedLightVolume(const FPrecomputedLightVolume* Volume)
{
	FScene* Scene = this; 

	ENQUEUE_RENDER_COMMAND(RemoveVolumeCommand)
		([Scene, Volume](FRHICommandListImmediate& RHICmdList) 
		{
			Scene->PrecomputedLightVolumes.Remove(Volume);
			Scene->IndirectLightingCache.SetLightingCacheDirty(Scene, Volume);
		});
}

void FVolumetricLightmapSceneData::AddLevelVolume(const FPrecomputedVolumetricLightmap* InVolume, EShadingPath ShadingPath, bool bIsPersistentLevel)
{
	LevelVolumetricLightmaps.Add(InVolume);

	if (bIsPersistentLevel)
	{
		PersistentLevelVolumetricLightmap = InVolume;
	}

	InVolume->Data->AddToSceneData(&GlobalVolumetricLightmapData);
	
	// Invalidate CPU lightmap lookup cache
	CPUInterpolationCache.Empty();
}

void FVolumetricLightmapSceneData::RemoveLevelVolume(const FPrecomputedVolumetricLightmap* InVolume)
{
	LevelVolumetricLightmaps.Remove(InVolume);

	InVolume->Data->RemoveFromSceneData(&GlobalVolumetricLightmapData, PersistentLevelVolumetricLightmap ? PersistentLevelVolumetricLightmap->Data->BrickDataBaseOffsetInAtlas : 0);

	if (PersistentLevelVolumetricLightmap == InVolume)
	{
		PersistentLevelVolumetricLightmap = nullptr;
	}

	// Invalidate CPU lightmap lookup cache
	CPUInterpolationCache.Empty();
}

const FPrecomputedVolumetricLightmap* FVolumetricLightmapSceneData::GetLevelVolumetricLightmap() const
{
#if WITH_EDITOR
	if (FStaticLightingSystemInterface::GetPrecomputedVolumetricLightmap(Scene->GetWorld()))
	{
		return FStaticLightingSystemInterface::GetPrecomputedVolumetricLightmap(Scene->GetWorld());
	}
#endif
	return &GlobalVolumetricLightmap;
}

bool FVolumetricLightmapSceneData::HasData() const
{
#if WITH_EDITOR
	if (FStaticLightingSystemInterface::GetPrecomputedVolumetricLightmap(Scene->GetWorld()))
	{
		return true;
	}
#endif
	if (LevelVolumetricLightmaps.Num() > 0)
	{
		if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM5)
		{
			return GlobalVolumetricLightmapData.IndirectionTexture.Texture.IsValid();
		}
		else
		{
			return GlobalVolumetricLightmapData.IndirectionTexture.Data.Num() > 0;
		}
	}

	return false;
}

bool FScene::HasPrecomputedVolumetricLightmap_RenderThread() const
{
#if WITH_EDITOR
	if (FStaticLightingSystemInterface::GetPrecomputedVolumetricLightmap(GetWorld()))
	{
		return true;
	}
#endif
	return VolumetricLightmapSceneData.HasData();
}

void FScene::AddPrecomputedVolumetricLightmap(const FPrecomputedVolumetricLightmap* Volume, bool bIsPersistentLevel)
{
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(AddVolumeCommand)
	([Scene, Volume, bIsPersistentLevel](FRHICommandListImmediate& RHICmdList)
	{
		Scene->VolumetricLightmapSceneData.AddLevelVolume(Volume, Scene->GetShadingPath(), bIsPersistentLevel);
	});
}

void FScene::RemovePrecomputedVolumetricLightmap(const FPrecomputedVolumetricLightmap* Volume)
{
	FScene* Scene = this; 

	ENQUEUE_RENDER_COMMAND(RemoveVolumeCommand)
	([Scene, Volume](FRHICommandListImmediate& RHICmdList) 
	{
		Scene->VolumetricLightmapSceneData.RemoveLevelVolume(Volume);
	});
}

void FScene::AddRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component)
{
	if (Component->SceneProxy == nullptr)
	{
		Component->SceneProxy = new FRuntimeVirtualTextureSceneProxy(Component);

		FScene* Scene = this;
		FRuntimeVirtualTextureSceneProxy* SceneProxy = Component->SceneProxy;

		ENQUEUE_RENDER_COMMAND(AddRuntimeVirtualTextureCommand)(
			[Scene, SceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->AddRuntimeVirtualTexture_RenderThread(SceneProxy);
			Scene->UpdateRuntimeVirtualTextureForAllPrimitives_RenderThread();
		});
	}
	else
	{
		// This is a component update.
		// Store the new FRuntimeVirtualTextureSceneProxy at the same index as the old (to avoid needing to update any associated primitives).
		// Defer old proxy deletion to the render thread.
		FRuntimeVirtualTextureSceneProxy* SceneProxyToReplace = Component->SceneProxy;
		Component->SceneProxy = new FRuntimeVirtualTextureSceneProxy(Component);

		FScene* Scene = this;
		FRuntimeVirtualTextureSceneProxy* SceneProxy = Component->SceneProxy;

		ENQUEUE_RENDER_COMMAND(AddRuntimeVirtualTextureCommand)(
			[Scene, SceneProxy, SceneProxyToReplace](FRHICommandListImmediate& RHICmdList)
		{
			const bool bUpdatePrimitives = SceneProxy->VirtualTexture != SceneProxyToReplace->VirtualTexture;
			Scene->UpdateRuntimeVirtualTexture_RenderThread(SceneProxy, SceneProxyToReplace);
			if (bUpdatePrimitives)
			{
				Scene->UpdateRuntimeVirtualTextureForAllPrimitives_RenderThread();
			}
		});
	}
}

void FScene::RemoveRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component)
{
	FRuntimeVirtualTextureSceneProxy* SceneProxy = Component->SceneProxy;
	if (SceneProxy != nullptr)
	{
		// Release now but defer any deletion to the render thread
		Component->SceneProxy->Release();
		Component->SceneProxy = nullptr;

		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(RemoveRuntimeVirtualTextureCommand)(
			[Scene, SceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->RemoveRuntimeVirtualTexture_RenderThread(SceneProxy);
			Scene->UpdateRuntimeVirtualTextureForAllPrimitives_RenderThread();
		});
	}
}

void FScene::AddRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy)
{
	SceneProxy->SceneIndex = RuntimeVirtualTextures.Add(SceneProxy);

	const uint8 HideFlagBit = 1 << SceneProxy->SceneIndex;
	RuntimeVirtualTexturePrimitiveHideMaskEditor &= ~HideFlagBit;
	RuntimeVirtualTexturePrimitiveHideMaskEditor |= (SceneProxy->bHidePrimitivesInEditor ? HideFlagBit : 0);
	RuntimeVirtualTexturePrimitiveHideMaskGame &= ~HideFlagBit;
	RuntimeVirtualTexturePrimitiveHideMaskGame |= (SceneProxy->bHidePrimitivesInGame ? HideFlagBit : 0);
}

void FScene::UpdateRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy, FRuntimeVirtualTextureSceneProxy* SceneProxyToReplace)
{
	const uint8 HideFlagBit = 1 << SceneProxy->SceneIndex;
	RuntimeVirtualTexturePrimitiveHideMaskEditor &= ~HideFlagBit;
	RuntimeVirtualTexturePrimitiveHideMaskEditor |= (SceneProxy->bHidePrimitivesInEditor ? HideFlagBit : 0);
	RuntimeVirtualTexturePrimitiveHideMaskGame &= ~HideFlagBit;
	RuntimeVirtualTexturePrimitiveHideMaskGame |= (SceneProxy->bHidePrimitivesInGame ? HideFlagBit : 0);

	for (TSparseArray<FRuntimeVirtualTextureSceneProxy*>::TIterator It(RuntimeVirtualTextures); It; ++It)
	{
		if (*It == SceneProxyToReplace)
		{
			SceneProxy->SceneIndex = It.GetIndex();
			*It = SceneProxy;
			delete SceneProxyToReplace;
			return;
		}
	}
	// If we get here then we didn't find the object to replace!
	check(false);
}

void FScene::RemoveRuntimeVirtualTexture_RenderThread(FRuntimeVirtualTextureSceneProxy* SceneProxy)
{
	const uint8 HideFlagBit = 1 << SceneProxy->SceneIndex;
	RuntimeVirtualTexturePrimitiveHideMaskEditor &= ~HideFlagBit;
	RuntimeVirtualTexturePrimitiveHideMaskGame &= ~HideFlagBit;

	RuntimeVirtualTextures.RemoveAt(SceneProxy->SceneIndex);
	delete SceneProxy;
}

void FScene::UpdateRuntimeVirtualTextureForAllPrimitives_RenderThread()
{
	for (int32 Index = 0; Index < Primitives.Num(); ++Index)
	{
		if (PrimitiveVirtualTextureFlags[Index].bRenderToVirtualTexture)
		{
			Primitives[Index]->UpdateRuntimeVirtualTextureFlags();
			PrimitiveVirtualTextureFlags[Index] = Primitives[Index]->GetRuntimeVirtualTextureFlags();
		}
	}
}

uint32 FScene::GetRuntimeVirtualTextureSceneIndex(uint32 ProducerId)
{
	checkSlow(IsInRenderingThread());
	for (FRuntimeVirtualTextureSceneProxy const* Proxy : RuntimeVirtualTextures)
	{
		if (Proxy->ProducerId == ProducerId)
		{
			return Proxy->SceneIndex;
		}
	}
	// Should not get here
	check(false);
	return 0;
}

void FScene::GetRuntimeVirtualTextureHidePrimitiveMask(uint8& bHideMaskEditor, uint8& bHideMaskGame) const
{
	bHideMaskEditor = RuntimeVirtualTexturePrimitiveHideMaskEditor;
	bHideMaskGame = RuntimeVirtualTexturePrimitiveHideMaskGame;
}

void FScene::InvalidateRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component, FBoxSphereBounds const& WorldBounds)
{
	if (Component->SceneProxy != nullptr)
	{
		FRuntimeVirtualTextureSceneProxy* SceneProxy = Component->SceneProxy;
		ENQUEUE_RENDER_COMMAND(RuntimeVirtualTextureComponent_SetDirty)(
			[SceneProxy, WorldBounds](FRHICommandList& RHICmdList)
		{
			SceneProxy->Dirty(WorldBounds);
		});
	}
}

void FScene::InvalidatePathTracedOutput()
{
	// NOTE: this is an atomic, so this function is ok to call from any thread
	++PathTracingInvalidationCounter;
}

void FScene::InvalidateLumenSurfaceCache_GameThread(UPrimitiveComponent* Component)
{
	check(IsInGameThread());

	if (Component->SceneProxy)
	{
		ENQUEUE_RENDER_COMMAND(InvalidateLumenSurfaceCacheCmd)(
			[this, PrimitiveSceneProxy = Component->SceneProxy](FRHICommandList&)
		{
			if (PrimitiveSceneProxy && PrimitiveSceneProxy->GetPrimitiveSceneInfo())
			{
				LumenInvalidateSurfaceCacheForPrimitive(PrimitiveSceneProxy->GetPrimitiveSceneInfo());
			}
		});
	}
}

void FScene::FlushDirtyRuntimeVirtualTextures()
{
	checkSlow(IsInRenderingThread());
	for (TSparseArray<FRuntimeVirtualTextureSceneProxy*>::TIterator It(RuntimeVirtualTextures); It; ++It)
	{
		(*It)->FlushDirtyPages();
	}
}

bool FScene::GetPreviousLocalToWorld(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld) const
{
	return VelocityData.GetComponentPreviousLocalToWorld(PrimitiveSceneInfo->PrimitiveComponentId, OutPreviousLocalToWorld);
}

void FSceneVelocityData::StartFrame(FScene* Scene)
{
	InternalFrameIndex++;

	const bool bTrimOld = InternalFrameIndex % 100 == 0;

	for (TMap<FPrimitiveComponentId, FComponentVelocityData>::TIterator It(ComponentData); It; ++It)
	{
		FComponentVelocityData& VelocityData = It.Value();
		VelocityData.PreviousLocalToWorld = VelocityData.LocalToWorld;
		VelocityData.bPreviousLocalToWorldValid = true;

		if ((InternalFrameIndex - VelocityData.LastFrameUpdated == 1) && VelocityData.PrimitiveSceneInfo)
		{
			// Force an update of the primitive data on the frame after the primitive moved, since it contains PreviousLocalToWorld
			VelocityData.PrimitiveSceneInfo->MarkGPUStateDirty(EPrimitiveDirtyState::ChangedTransform);
		}

		if (bTrimOld && (InternalFrameIndex - VelocityData.LastFrameUsed) > 10)
		{
			if (VelocityData.PrimitiveSceneInfo)
			{
				Scene->GPUScene.AddPrimitiveToUpdate(VelocityData.PrimitiveSceneInfo->GetIndex(), EPrimitiveDirtyState::ChangedOther);
			}

			It.RemoveCurrent();
		}
	}
}

void FScene::GetPrimitiveUniformShaderParameters_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool& bHasPrecomputedVolumetricLightmap, FMatrix& PreviousLocalToWorld, int32& SingleCaptureIndex, bool& bOutputVelocity) const 
{
	const FMatrix LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
	PreviousLocalToWorld = LocalToWorld;
	bOutputVelocity = false;

	const bool bHasPreviousLocalToWorld = VelocityData.GetComponentPreviousLocalToWorld(PrimitiveSceneInfo->PrimitiveComponentId, PreviousLocalToWorld);
	if (bHasPreviousLocalToWorld)
	{
		bOutputVelocity = !LocalToWorld.Equals(PreviousLocalToWorld, 0.0001f);
	}

	bHasPrecomputedVolumetricLightmap = VolumetricLightmapSceneData.HasData();

	// Get index if proxy exists, otherwise fall back to index 0 which will contain the default black cubemap
	SingleCaptureIndex = PrimitiveSceneInfo->CachedReflectionCaptureProxy ? PrimitiveSceneInfo->CachedReflectionCaptureProxy->SortedCaptureIndex : 0;
}

void FScene::UpdateLightTransform_RenderThread(int32 LightId, FLightSceneInfo* LightSceneInfo, const FUpdateLightCommand::FTransformParameters& Parameters)
{
	SCOPED_NAMED_EVENT(FScene_UpdateLightTransform_RenderThread, FColor::Yellow);

	// This is called without a valid ID when the update is fused with an 'add' command (saves redundant scene updates to do the update first)
	const bool bHasId = LightId != INDEX_NONE;
	// Don't remove directional lights when their transform changes as nothing in RemoveFromScene() depends on their transform
	const bool bRemove = bHasId && (Lights[LightId].LightType != LightType_Directional);

	if (bHasId)
	{
		if (bRemove)
		{
			// Remove the light from the scene.
			LightSceneInfo->RemoveFromScene();
		}
	}

	// Invalidate the path tracer if the transform actually changed
	// NOTE: Position is derived from the Matrix, so there is no need to check it separately
	if (!Parameters.LightToWorld.Equals(LightSceneInfo->Proxy->LightToWorld, SMALL_NUMBER))
	{
		InvalidatePathTracedOutput();
	}

	// Update the light's transform and position.
	LightSceneInfo->Proxy->SetTransform(Parameters.LightToWorld, Parameters.Position);

	// Also update the LightSceneInfoCompact (if one exists)
	if (bHasId)
	{
		checkSlow(Lights[LightId].LightSceneInfo == LightSceneInfo);
		Lights[LightId].Init(LightSceneInfo);

		// Don't re-add directional lights when their transform changes as nothing in AddToScene() depends on their transform
		if (bRemove)
		{
			// Add the light to the scene at its new location.
			LightSceneInfo->AddToScene();
		}
	}
}

void FScene::UpdateLightTransform(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		FUpdateLightCommand::FTransformParameters Parameters;
		Parameters.LightToWorld = Light->GetComponentTransform().ToMatrixNoScale();
		Parameters.Position = Light->GetLightPosition();
		
		FLightSceneInfo* LightSceneInfo = Light->SceneProxy->GetLightSceneInfo();
		if (LightSceneInfo->bVisible)
		{
			ENQUEUE_RENDER_COMMAND(UpdateLightTransform)(
				[this, LightSceneInfo, Parameters](FRHICommandListImmediate& RHICmdList)
				{
					FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
					SceneLightInfoUpdates->Enqueue(Parameters, LightSceneInfo);
					++SceneLightInfoUpdates->NumUpdates;
				});
		}
	}
}

/** 
 * Updates the color and brightness of a light which has already been added to the scene. 
 *
 * @param Light - light component to update
 */
void FScene::UpdateLightColorAndBrightness(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		FUpdateLightCommand::FColorParameters NewParameters;
		NewParameters.NewColor = Light->GetColoredLightBrightness();
		NewParameters.NewIndirectLightingScale = Light->IndirectLightingIntensity;
		NewParameters.NewVolumetricScatteringIntensity = Light->VolumetricScatteringIntensity;

		FScene* Scene = this;
		FLightSceneInfo* LightSceneInfo = Light->SceneProxy->GetLightSceneInfo();
		if (LightSceneInfo->bVisible)
		{
			ENQUEUE_RENDER_COMMAND(UpdateLightColorAndBrightness)(
				[this, LightSceneInfo, NewParameters](FRHICommandListImmediate& RHICmdList)
				{
					SceneLightInfoUpdates->Enqueue(NewParameters, LightSceneInfo);
					++SceneLightInfoUpdates->NumUpdates;
				});
		}
	}
}

void FScene::RemoveLightSceneInfo_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_RemoveSceneLightTime);
	SCOPED_NAMED_EVENT(FScene_RemoveLightSceneInfo_RenderThread, FColor::Red);

	check(LightSceneInfo->bVisible);

	const bool bDirectionalLight = LightSceneInfo->Proxy->GetLightType() == LightType_Directional;

	if (bDirectionalLight)
	{
		DirectionalLights.Remove(LightSceneInfo);
	}

	// check SimpleDirectionalLight
	if (LightSceneInfo == SimpleDirectionalLight)
	{
		SimpleDirectionalLight = nullptr;
	}

	if(GetShadingPath() == EShadingPath::Mobile)
	{
		const bool bUseCSMForDynamicObjects = LightSceneInfo->Proxy->UseCSMForDynamicObjects();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		// Tracked for disabled shader permutation warnings.
		// Condition must match that in AddLightSceneInfo_RenderThread
		if (LightSceneInfo->Proxy->GetLightType() == LightType_Directional && !LightSceneInfo->Proxy->HasStaticLighting())
		{
			if (LightSceneInfo->Proxy->IsMovable())
			{
				NumMobileMovableDirectionalLights_RenderThread--;
			}
			if (bUseCSMForDynamicObjects)
			{
				NumMobileStaticAndCSMLights_RenderThread--;
			}
		}
#endif

		// check MobileDirectionalLights
		for (int32 LightChannelIdx = 0; LightChannelIdx < UE_ARRAY_COUNT(MobileDirectionalLights); LightChannelIdx++)
		{
			if (LightSceneInfo == MobileDirectionalLights[LightChannelIdx])
			{
				MobileDirectionalLights[LightChannelIdx] = nullptr;

				// find another light that could be the new MobileDirectionalLight for this channel
				for (const FLightSceneInfoCompact& OtherLight : Lights)
				{
					if (OtherLight.LightSceneInfo != LightSceneInfo &&
						OtherLight.LightType == LightType_Directional &&
						!OtherLight.bStaticLighting &&
						GetFirstLightingChannelFromMask(OtherLight.LightSceneInfo->Proxy->GetLightingChannelMask()) == LightChannelIdx)
					{
						MobileDirectionalLights[LightChannelIdx] = OtherLight.LightSceneInfo;
						break;
					}
				}

				// if this light is a dynamic shadowcast then we need to update the static draw lists to pick a new lightingpolicy
				if (!LightSceneInfo->Proxy->HasStaticShadowing() || bUseCSMForDynamicObjects)
				{
					bScenesPrimitivesNeedStaticMeshElementUpdate = true;
				}
				break;
			}
		}
	}

	ProcessAtmosphereLightRemoval_RenderThread(LightSceneInfo);

	// Remove the light from the scene.
	LightSceneInfo->RemoveFromScene();

	// Remove the light from the lights list.
	Lights.RemoveAt(LightSceneInfo->Id);

	// TODO: move this work to FShadowScene & batch the light removals
	{
		TArray<FVirtualShadowMapArrayCacheManager*, SceneRenderingAllocator> VirtualShadowCacheManagers;
		GetAllVirtualShadowMapCacheManagers(VirtualShadowCacheManagers);

		for (FVirtualShadowMapArrayCacheManager* CacheManager : VirtualShadowCacheManagers)
		{
			CacheManager->OnLightRemoved(LightSceneInfo->Id);
		}
	}

	if (!LightSceneInfo->Proxy->HasStaticShadowing()
		&& LightSceneInfo->Proxy->CastsDynamicShadow()
		&& LightSceneInfo->GetDynamicShadowMapChannel() == -1)
	{
		OverflowingDynamicShadowedLights.Remove(LightSceneInfo->Proxy->GetOwnerNameOrLabel());
	}

	InvalidatePathTracedOutput();

	if (LightSceneInfo->Proxy->GetLightType() == LightType_Rect)
	{
		const FRectLightSceneProxy* RectProxy = (const FRectLightSceneProxy*)LightSceneInfo->Proxy;
		RectLightAtlas::RemoveTexture(RectProxy->RectAtlasId);
	}

	if (UTextureLightProfile* IESTexture = LightSceneInfo->Proxy->GetIESTexture())
	{
		IESAtlas::RemoveTexture(LightSceneInfo->Proxy->IESAtlasId);
	}

	// Free the light scene info and proxy.
	delete LightSceneInfo->Proxy;
	delete LightSceneInfo;
}

void FScene::RemoveLight(ULightComponent* Light)
{
	if(Light->SceneProxy)
	{
		FLightSceneInfo* LightSceneInfo = Light->SceneProxy->GetLightSceneInfo();

		DEC_DWORD_STAT(STAT_SceneLights);

		// Removing one visible light
		--NumVisibleLights_GameThread;

		// Disassociate the primitive's render info.
		Light->SceneProxy = nullptr;

		// Send a command to the rendering thread to queue the light for removal from the scene.
		ENQUEUE_RENDER_COMMAND(FQueueRemoveLightCommand)(
			[this, LightSceneInfo](FRHICommandListImmediate& RHICmdList)
			{
				FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());

				if (LightSceneInfo->bVisible)
				{
					auto HashElementId = SceneLightInfoUpdates->Commands.FindId(LightSceneInfo);
					// If an Add is pending, just remove the update and fall through to delete the data.
					if (HashElementId.IsValid() && SceneLightInfoUpdates->Commands.GetByElementId(HashElementId).Value.IsAdd())
					{
						SceneLightInfoUpdates->Commands.RemoveByElementId(HashElementId);
						--SceneLightInfoUpdates->NumAdds;
					}
					else
					{
						SceneLightInfoUpdates->Enqueue(FUpdateLightCommand::EAddOrRemove::Remove, LightSceneInfo);
						++SceneLightInfoUpdates->NumRemoves;
						// early out to defer the deletion
						return;
					}
				}
				else
				{
					// There should never be updates queued for lights that are not visible
					check(SceneLightInfoUpdates->Commands.Find(LightSceneInfo) == nullptr);
					// The "invisible lights" are removed at once.
					InvisibleLights.RemoveAt(LightSceneInfo->Id);
				}

				// Free the light scene info and proxy.
				delete LightSceneInfo->Proxy;
				delete LightSceneInfo;
			});
	}
}

void FScene::AddExponentialHeightFog(UExponentialHeightFogComponent* FogComponent)
{
	FScene* Scene = this;
	FExponentialHeightFogSceneInfo HeightFogSceneInfo = FExponentialHeightFogSceneInfo(FogComponent);
	ENQUEUE_RENDER_COMMAND(FAddFogCommand)(
		[Scene, HeightFogSceneInfo](FRHICommandListImmediate& RHICmdList)
		{
			// Create a FExponentialHeightFogSceneInfo for the component in the scene's fog array.
			new(Scene->ExponentialFogs) FExponentialHeightFogSceneInfo(HeightFogSceneInfo);
			Scene->InvalidatePathTracedOutput();
		});
}

void FScene::RemoveExponentialHeightFog(UExponentialHeightFogComponent* FogComponent)
{
	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(FRemoveFogCommand)(
		[Scene, FogComponent](FRHICommandListImmediate& RHICmdList)
		{
			// Remove the given component's FExponentialHeightFogSceneInfo from the scene's fog array.
			for(int32 FogIndex = 0;FogIndex < Scene->ExponentialFogs.Num();FogIndex++)
			{
				if(Scene->ExponentialFogs[FogIndex].Component == FogComponent)
				{
					Scene->ExponentialFogs.RemoveAt(FogIndex);
					Scene->InvalidatePathTracedOutput();
					break;
				}
			}
		});
}

bool FScene::HasAnyExponentialHeightFog() const
{
	return this->ExponentialFogs.Num() > 0;
}

void FScene::AddWindSource(UWindDirectionalSourceComponent* WindComponent)
{
	// if this wind component is not activated (or Auto Active is set to false), then don't add to WindSources
	if(!WindComponent->IsActive())
	{
		return;
	}
	ensure(IsInGameThread());
	WindComponents_GameThread.Add(WindComponent);

	FWindSourceSceneProxy* SceneProxy = WindComponent->CreateSceneProxy();
	WindComponent->SceneProxy = SceneProxy;

	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(FAddWindSourceCommand)(
		[Scene, SceneProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->WindSources.Add(SceneProxy);
		});
}

void FScene::RemoveWindSource(UWindDirectionalSourceComponent* WindComponent)
{
	ensure(IsInGameThread());
	WindComponents_GameThread.Remove(WindComponent);

	FWindSourceSceneProxy* SceneProxy = WindComponent->SceneProxy;
	WindComponent->SceneProxy = NULL;

	if(SceneProxy)
	{
		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(FRemoveWindSourceCommand)(
			[Scene, SceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				Scene->WindSources.Remove(SceneProxy);

				delete SceneProxy;
			});
	}
}

void FScene::UpdateWindSource(UWindDirectionalSourceComponent* WindComponent)
{
	// Recreate the scene proxy without touching WindComponents_GameThread
	// so that this function is kept thread safe when iterating in parallel
	// over components (unlike AddWindSource and RemoveWindSource)
	FWindSourceSceneProxy* const OldSceneProxy = WindComponent->SceneProxy;
	if (OldSceneProxy)
	{
		WindComponent->SceneProxy = nullptr;

		ENQUEUE_RENDER_COMMAND(FRemoveWindSourceCommand)(
			[Scene = this, OldSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				Scene->WindSources.Remove(OldSceneProxy);

				delete OldSceneProxy;
			});
	}

	if (WindComponent->IsActive())
	{
		FWindSourceSceneProxy* const NewSceneProxy = WindComponent->CreateSceneProxy();
		WindComponent->SceneProxy = NewSceneProxy;

		ENQUEUE_RENDER_COMMAND(FAddWindSourceCommand)(
			[Scene = this, NewSceneProxy](FRHICommandListImmediate& RHICmdList)
			{
				Scene->WindSources.Add(NewSceneProxy);
			});
	}

}

const TArray<FWindSourceSceneProxy*>& FScene::GetWindSources_RenderThread() const
{
	checkSlow(IsInRenderingThread());
	return WindSources;
}

void FScene::GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const
{
	FWindData AccumWindData;
	AccumWindData.PrepareForAccumulate();

	int32 NumActiveWindSources = 0;
	FVector4f AccumulatedDirectionAndSpeed(0,0,0,0);
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < WindSources.Num(); i++)
	{
		
		FVector4f CurrentDirectionAndSpeed;
		float Weight;
		const FWindSourceSceneProxy* CurrentSource = WindSources[i];
		FWindData CurrentSourceData;
		if (CurrentSource->GetWindParameters(Position, CurrentSourceData, Weight))
		{
			AccumWindData.AddWeighted(CurrentSourceData, Weight);
			TotalWeight += Weight;
			NumActiveWindSources++;
		}
	}

	AccumWindData.NormalizeByTotalWeight(TotalWeight);

	if (NumActiveWindSources == 0)
	{
		AccumWindData.Direction = FVector(1.0f, 0.0f, 0.0f);
	}
	OutDirection	= AccumWindData.Direction;
	OutSpeed		= AccumWindData.Speed;
	OutMinGustAmt	= AccumWindData.MinGustAmt;
	OutMaxGustAmt	= AccumWindData.MaxGustAmt;
}

void FScene::GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const
{
	FWindData AccumWindData;
	AccumWindData.PrepareForAccumulate();

	const int32 NumSources = WindComponents_GameThread.Num();
	int32 NumActiveSources = 0;
	float TotalWeight = 0.0f;

	// read the wind component array, this is safe for the game thread
	for(UWindDirectionalSourceComponent* Component : WindComponents_GameThread)
	{
		float Weight = 0.0f;
		FWindData CurrentComponentData;
		if(Component && Component->GetWindParameters(Position, CurrentComponentData, Weight))
		{
			AccumWindData.AddWeighted(CurrentComponentData, Weight);
			TotalWeight += Weight;
			++NumActiveSources;
		}
	}

	AccumWindData.NormalizeByTotalWeight(TotalWeight);

	if(NumActiveSources == 0)
	{
		AccumWindData.Direction = FVector(1.0f, 0.0f, 0.0f);
	}

	OutDirection = AccumWindData.Direction;
	OutSpeed = AccumWindData.Speed;
	OutMinGustAmt = AccumWindData.MinGustAmt;
	OutMaxGustAmt = AccumWindData.MaxGustAmt;
}

void FScene::GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const
{
	FWindData AccumWindData;
	AccumWindData.PrepareForAccumulate();

	int32 NumActiveWindSources = 0;
	FVector4f AccumulatedDirectionAndSpeed(0,0,0,0);
	float TotalWeight = 0.0f;
	for (int32 i = 0; i < WindSources.Num(); i++)
	{
		FVector4f CurrentDirectionAndSpeed;
		float Weight;
		const FWindSourceSceneProxy* CurrentSource = WindSources[i];
		FWindData CurrentSourceData;
		if (CurrentSource->GetDirectionalWindParameters(CurrentSourceData, Weight))
		{
			AccumWindData.AddWeighted(CurrentSourceData, Weight);			
			TotalWeight += Weight;
			NumActiveWindSources++;
		}
	}

	AccumWindData.NormalizeByTotalWeight(TotalWeight);	

	if (NumActiveWindSources == 0)
	{
		AccumWindData.Direction = FVector(1.0f, 0.0f, 0.0f);
	}
	OutDirection = AccumWindData.Direction;
	OutSpeed = AccumWindData.Speed;
	OutMinGustAmt = AccumWindData.MinGustAmt;
	OutMaxGustAmt = AccumWindData.MaxGustAmt;
}

void FScene::AddSpeedTreeWind(FVertexFactory* VertexFactory, const UStaticMesh* StaticMesh)
{
	if (StaticMesh != NULL && StaticMesh->SpeedTreeWind.IsValid() && StaticMesh->GetRenderData())
	{
		FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(FAddSpeedTreeWindCommand)(
			[Scene, StaticMesh, VertexFactory](FRHICommandListImmediate& RHICmdList)
			{
				Scene->SpeedTreeVertexFactoryMap.Add(VertexFactory, StaticMesh);

				if (Scene->SpeedTreeWindComputationMap.Contains(StaticMesh))
				{
					(*(Scene->SpeedTreeWindComputationMap.Find(StaticMesh)))->ReferenceCount++;
				}
				else
				{
					FSpeedTreeWindComputation* WindComputation = new FSpeedTreeWindComputation;
					WindComputation->Wind = *(StaticMesh->SpeedTreeWind.Get( ));

					FSpeedTreeUniformParameters UniformParameters;
					FPlatformMemory::Memzero(&UniformParameters, sizeof(UniformParameters));
					WindComputation->UniformBuffer = TUniformBufferRef<FSpeedTreeUniformParameters>::CreateUniformBufferImmediate(UniformParameters, UniformBuffer_MultiFrame);
					Scene->SpeedTreeWindComputationMap.Add(StaticMesh, WindComputation);
				}
			});
	}
}

void FScene::RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh)
{
	FSpeedTreeWindComputation** WindComputationRef = SpeedTreeWindComputationMap.Find(StaticMesh);
	if (WindComputationRef != NULL)
	{
		FSpeedTreeWindComputation* WindComputation = *WindComputationRef;

		WindComputation->ReferenceCount--;
		if (WindComputation->ReferenceCount < 1)
		{
			for (auto Iter = SpeedTreeVertexFactoryMap.CreateIterator(); Iter; ++Iter )
			{
				if (Iter.Value() == StaticMesh)
				{
					Iter.RemoveCurrent();
				}
			}

			SpeedTreeWindComputationMap.Remove(StaticMesh);
			delete WindComputation;
		}
	}
}

void FScene::UpdateSpeedTreeWind(double CurrentTime)
{
#define SET_SPEEDTREE_TABLE_FLOAT4V(name, offset) \
	UniformParameters.name = *(FVector4f*)(WindShaderValues + FSpeedTreeWind::offset); \
	UniformParameters.Prev##name = *(FVector4f*)(WindShaderValues + FSpeedTreeWind::offset + FSpeedTreeWind::NUM_SHADER_VALUES);

	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(FUpdateSpeedTreeWindCommand)(
		[Scene, CurrentTime](FRHICommandListImmediate& RHICmdList)
		{
			FVector WindDirection;
			float WindSpeed;
			float WindMinGustAmt;
			float WindMaxGustAmt;
			Scene->GetDirectionalWindParameters(WindDirection, WindSpeed, WindMinGustAmt, WindMaxGustAmt);

			for (TMap<const UStaticMesh*, FSpeedTreeWindComputation*>::TIterator It(Scene->SpeedTreeWindComputationMap); It; ++It )
			{
				const UStaticMesh* StaticMesh = It.Key();
				FSpeedTreeWindComputation* WindComputation = It.Value();

				if( !(StaticMesh->GetRenderData() && StaticMesh->SpeedTreeWind.IsValid()) )
				{
					It.RemoveCurrent();
					continue;
				}

				if (GIsEditor && StaticMesh->SpeedTreeWind->NeedsReload( ))
				{
					// reload the wind since it may have changed or been scaled differently during reimport
					StaticMesh->SpeedTreeWind->SetNeedsReload(false);
					WindComputation->Wind = *(StaticMesh->SpeedTreeWind.Get( ));
				}

				// advance the wind object
				WindComputation->Wind.SetDirection(WindDirection);
				WindComputation->Wind.SetStrength(WindSpeed);
				WindComputation->Wind.SetGustMin(WindMinGustAmt);
				WindComputation->Wind.SetGustMax(WindMaxGustAmt);
				WindComputation->Wind.Advance(true, CurrentTime);

				// copy data into uniform buffer
				const float* WindShaderValues = WindComputation->Wind.GetShaderTable();

				FSpeedTreeUniformParameters UniformParameters;
				UniformParameters.WindAnimation.Set(CurrentTime, 0.0f, 0.0f, 0.0f);
			
				SET_SPEEDTREE_TABLE_FLOAT4V(WindVector, SH_WIND_DIR_X);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindGlobal, SH_GLOBAL_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranch, SH_BRANCH_1_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchTwitch, SH_BRANCH_1_TWITCH);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchWhip, SH_BRANCH_1_WHIP);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchAnchor, SH_WIND_ANCHOR_X);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindBranchAdherences, SH_GLOBAL_DIRECTION_ADHERENCE);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindTurbulences, SH_BRANCH_1_TURBULENCE);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Ripple, SH_LEAF_1_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Tumble, SH_LEAF_1_TUMBLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf1Twitch, SH_LEAF_1_TWITCH_THROW);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Ripple, SH_LEAF_2_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Tumble, SH_LEAF_2_TUMBLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindLeaf2Twitch, SH_LEAF_2_TWITCH_THROW);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindFrondRipple, SH_FROND_RIPPLE_TIME);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingBranch, SH_ROLLING_BRANCH_FIELD_MIN);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingLeafAndDirection, SH_ROLLING_LEAF_RIPPLE_MIN);
				SET_SPEEDTREE_TABLE_FLOAT4V(WindRollingNoise, SH_ROLLING_NOISE_PERIOD);

				WindComputation->UniformBuffer.UpdateUniformBufferImmediate(UniformParameters);
			}
		});
	#undef SET_SPEEDTREE_TABLE_FLOAT4V
}

FRHIUniformBuffer* FScene::GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) const
{
	if (VertexFactory != NULL)
	{
		const UStaticMesh* const* StaticMesh = SpeedTreeVertexFactoryMap.Find(VertexFactory);
		if (StaticMesh != NULL)
		{
			const FSpeedTreeWindComputation* const * WindComputation = SpeedTreeWindComputationMap.Find(*StaticMesh);
			if (WindComputation != NULL)
			{
				return (*WindComputation)->UniformBuffer;
			}
		}
	}

	return nullptr;
}

/**
 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
 *
 * Render thread version of function.
 * 
 * @param	Primitive				Primitive to retrieve interacting lights for
 * @param	RelevantLights	[out]	Array of lights interacting with primitive
 */
void FScene::GetRelevantLights_RenderThread( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const
{
	check( Primitive );
	check( RelevantLights );
	if( Primitive->SceneProxy )
	{
		for( const FLightPrimitiveInteraction* Interaction=Primitive->SceneProxy->GetPrimitiveSceneInfo()->LightList; Interaction; Interaction=Interaction->GetNextLight() )
		{
			RelevantLights->Add( Interaction->GetLight()->Proxy->GetLightComponent() );
		}
	}
}

/**
 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
 *
 * @param	Primitive				Primitive to retrieve interacting lights for
 * @param	RelevantLights	[out]	Array of lights interacting with primitive
 */
void FScene::GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const
{
	if( Primitive && RelevantLights )
	{
		// Add interacting lights to the array.
		const FScene* Scene = this;
		ENQUEUE_RENDER_COMMAND(FGetRelevantLightsCommand)(
			[Scene, Primitive, RelevantLights](FRHICommandListImmediate& RHICmdList)
			{
				Scene->GetRelevantLights_RenderThread( Primitive, RelevantLights );
			});

		// We need to block the main thread as the rendering thread needs to finish modifying the array before we can continue.
		FlushRenderingCommands();
	}
}

/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
void FScene::SetPrecomputedVisibility(const FPrecomputedVisibilityHandler* NewPrecomputedVisibilityHandler)
{
	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(UpdatePrecomputedVisibility)(
		[Scene, NewPrecomputedVisibilityHandler](FRHICommandListImmediate& RHICmdList)
		{
			Scene->PrecomputedVisibilityHandler = NewPrecomputedVisibilityHandler;
		});
}

void FScene::UpdateStaticDrawLists_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	SCOPE_CYCLE_COUNTER(STAT_Scene_UpdateStaticDrawLists_RT);
	SCOPED_NAMED_EVENT(FScene_UpdateStaticDrawLists_RenderThread, FColor::Yellow);

	const int32 NumPrimitives = Primitives.Num();

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < NumPrimitives; ++PrimitiveIndex)
	{
		FPrimitiveSceneInfo* Primitive = Primitives[PrimitiveIndex];

		Primitive->RemoveStaticMeshes();
	}

	FPrimitiveSceneInfo::AddStaticMeshes(this, Primitives);
}

void FScene::UpdateStaticDrawLists()
{
	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(FUpdateDrawLists)(
		[Scene](FRHICommandListImmediate& RHICmdList)
		{
			Scene->UpdateStaticDrawLists_RenderThread(RHICmdList);
		});
}

void FScene::UpdateCachedRenderStates(FPrimitiveSceneProxy* SceneProxy)
{
	check(IsInRenderingThread());

	if (SceneProxy->GetPrimitiveSceneInfo())
	{
		SceneProxy->GetPrimitiveSceneInfo()->BeginDeferredUpdateStaticMeshes();
	}
}

#if RHI_RAYTRACING

void FScene::UpdateCachedRayTracingState(FPrimitiveSceneProxy* SceneProxy)
{
	check(IsInRenderingThread());

	if (SceneProxy->GetPrimitiveSceneInfo())
	{
		SceneProxy->GetPrimitiveSceneInfo()->bCachedRaytracingDataDirty = true;

		// Clear the recounted pointer as well since we don't need it anymore
		SceneProxy->GetPrimitiveSceneInfo()->CachedRayTracingInstance.GeometryRHI = nullptr;
	}
}

#endif // RHI_RAYTRACING

/**
 * @return		true if hit proxies should be rendered in this scene.
 */
bool FScene::RequiresHitProxies() const
{
	return (GIsEditor && bRequiresHitProxies);
}

void FScene::Release()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScene::Release);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Verify that no components reference this scene being destroyed
	static bool bTriggeredOnce = false;

	if (!bTriggeredOnce)
	{
		for (auto* ActorComponent : TObjectRange<UActorComponent>())
		{
			if ( !ensureMsgf(!ActorComponent->IsRegistered() || ActorComponent->GetScene() != this,
					TEXT("Component Name: %s World Name: %s Component Asset: %s"),
										*ActorComponent->GetFullName(),
										*GetWorld()->GetFullName(),
										*ActorComponent->AdditionalStatObject()->GetPathName()) )
			{
				bTriggeredOnce = true;
				break;
			}
		}
	}
#endif

	GetRendererModule().RemoveScene(this);

	// Send a command to the rendering thread to release the scene.
	FScene* Scene = this;
	ENQUEUE_RENDER_COMMAND(FReleaseCommand)(
		[Scene](FRHICommandListImmediate& RHICmdList)
		{
			// Flush any remaining batched primitive update commands before deleting the scene.
			Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);
			delete Scene;
		});
}

bool ShouldForceFullDepthPass(EShaderPlatform ShaderPlatform)
{
	const bool bNaniteEnabled = UseNanite(ShaderPlatform);
	const bool bDBufferAllowed = IsUsingDBuffers(ShaderPlatform);
	const bool bVirtualTextureEnabled = UseVirtualTexturing(ShaderPlatform);

	static const auto StencilLODDitherCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.StencilForLODDither"));
	const bool bStencilLODDither = StencilLODDitherCVar->GetValueOnAnyThread() != 0;

	static const auto AOComputeCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AmbientOcclusion.Compute"));
	const bool bAOCompute = AOComputeCVar->GetValueOnAnyThread() > 0;

	const bool bEarlyZMaterialMasking = MaskedInEarlyPass(ShaderPlatform);

	// Note: ShouldForceFullDepthPass affects which static draw lists meshes go into, so nothing it depends on can change at runtime, unless you do a FGlobalComponentRecreateRenderStateContext to propagate the cvar change
	return bNaniteEnabled || bAOCompute || bDBufferAllowed || bVirtualTextureEnabled || bStencilLODDither || bEarlyZMaterialMasking || IsForwardShadingEnabled(ShaderPlatform) || IsUsingSelectiveBasePassOutputs(ShaderPlatform);
}

void FScene::UpdateEarlyZPassMode()
{
	checkSlow(IsInGameThread());

	DefaultBasePassDepthStencilAccess = GetDefaultBasePassDepthStencilAccess(GetFeatureLevel());
	GetEarlyZPassMode(GetFeatureLevel(), EarlyZPassMode, bEarlyZPassMovable);
}

FExclusiveDepthStencil::Type FScene::GetDefaultBasePassDepthStencilAccess(ERHIFeatureLevel::Type InFeatureLevel)
{
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess = FExclusiveDepthStencil::DepthWrite_StencilWrite;

	if (GetShadingPath(InFeatureLevel) == EShadingPath::Deferred)
	{
		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);
		if (ShouldForceFullDepthPass(ShaderPlatform)
			&& CVarBasePassWriteDepthEvenWithFullPrepass.GetValueOnAnyThread() == 0)
		{
			BasePassDepthStencilAccess = FExclusiveDepthStencil::DepthRead_StencilWrite;
		}
	}

	return BasePassDepthStencilAccess;
}

void FScene::GetEarlyZPassMode(ERHIFeatureLevel::Type InFeatureLevel, EDepthDrawingMode & OutZPassMode, bool& bOutEarlyZPassMovable)
{
	OutZPassMode = DDM_NonMaskedOnly;
	bOutEarlyZPassMovable = false;

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);
	if (GetShadingPath(InFeatureLevel) == EShadingPath::Deferred)
	{
		// developer override, good for profiling, can be useful as project setting
		{
			const int32 CVarValue = CVarEarlyZPass.GetValueOnAnyThread();

				switch (CVarValue)
				{
				case 0: OutZPassMode = DDM_None; break;
				case 1: OutZPassMode = DDM_NonMaskedOnly; break;
				case 2: OutZPassMode = DDM_AllOccluders; break;
				case 3: break;	// Note: 3 indicates "default behavior" and does not specify an override
				}
		}

		if (ShouldForceFullDepthPass(ShaderPlatform))
		{
			// DBuffer decals and stencil LOD dithering force a full prepass
			const bool bDepthPassCanOutputVelocity = FVelocityRendering::DepthPassCanOutputVelocity(InFeatureLevel);
			OutZPassMode = bDepthPassCanOutputVelocity ? DDM_AllOpaqueNoVelocity : DDM_AllOpaque;
			bOutEarlyZPassMovable = bDepthPassCanOutputVelocity ? false : true;
		}
	}
	else if (GetShadingPath(InFeatureLevel) == EShadingPath::Mobile)
	{
		OutZPassMode = DDM_None;
				 
		static const bool bMaskedInEarlyPass = MaskedInEarlyPass(ShaderPlatform);
		if (bMaskedInEarlyPass)
		{
			OutZPassMode = DDM_MaskedOnly;
		}

		const bool bIsMobileAmbientOcclusionEnabled = IsMobileAmbientOcclusionEnabled(ShaderPlatform);
		const bool bMobileUsesShadowMaskTexture = MobileUsesShadowMaskTexture(ShaderPlatform);
		if (bIsMobileAmbientOcclusionEnabled || bMobileUsesShadowMaskTexture)
		{
			OutZPassMode = DDM_AllOpaque;
		}

		bool bMobileForceFullDepthPrepass = CVarMobileEarlyZPass.GetValueOnAnyThread() == 1;
		if (bMobileForceFullDepthPrepass)
		{
			OutZPassMode = DDM_AllOpaque;
		}
	}
}

void FScene::ConditionalMarkStaticMeshElementsForUpdate()
{
	if (bScenesPrimitivesNeedStaticMeshElementUpdate
		|| CachedDefaultBasePassDepthStencilAccess != DefaultBasePassDepthStencilAccess)
	{
		// Mark all primitives as needing an update
		// Note: Only visible primitives will actually update their static mesh elements
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Primitives.Num(); PrimitiveIndex++)
		{
			Primitives[PrimitiveIndex]->BeginDeferredUpdateStaticMeshes();
		}

		bScenesPrimitivesNeedStaticMeshElementUpdate = false;
		CachedDefaultBasePassDepthStencilAccess = DefaultBasePassDepthStencilAccess;
	}
}

void FScene::DumpUnbuiltLightInteractions( FOutputDevice& Ar ) const
{
	FlushRenderingCommands();

	TSet<FString> LightsWithUnbuiltInteractions;
	TSet<FString> PrimitivesWithUnbuiltInteractions;

	// if want to print out all of the lights
	for( auto It = Lights.CreateConstIterator(); It; ++It )
	{
		const FLightSceneInfoCompact& LightCompactInfo = *It;
		FLightSceneInfo* LightSceneInfo = LightCompactInfo.LightSceneInfo;

		bool bLightHasUnbuiltInteractions = false;

		for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->GetDynamicInteractionOftenMovingPrimitiveList();
			Interaction;
			Interaction = Interaction->GetNextPrimitive())
		{
			if (Interaction->IsUncachedStaticLighting())
			{
				bLightHasUnbuiltInteractions = true;
				PrimitivesWithUnbuiltInteractions.Add(Interaction->GetPrimitiveSceneInfo()->ComponentForDebuggingOnly->GetFullName());
			}
		}

		for(FLightPrimitiveInteraction* Interaction = LightSceneInfo->GetDynamicInteractionStaticPrimitiveList();
			Interaction;
			Interaction = Interaction->GetNextPrimitive())
		{
			if (Interaction->IsUncachedStaticLighting())
			{
				bLightHasUnbuiltInteractions = true;
				PrimitivesWithUnbuiltInteractions.Add(Interaction->GetPrimitiveSceneInfo()->ComponentForDebuggingOnly->GetFullName());
			}
		}

		if (bLightHasUnbuiltInteractions)
		{
			LightsWithUnbuiltInteractions.Add(LightSceneInfo->Proxy->GetOwnerNameOrLabel());
		}
	}

	Ar.Logf( TEXT( "DumpUnbuiltLightIteractions" ) );
	Ar.Logf( TEXT( "Lights with unbuilt interactions: %d" ), LightsWithUnbuiltInteractions.Num() );
	for (auto &LightName : LightsWithUnbuiltInteractions)
	{
		Ar.Logf(TEXT("    Light %s"), *LightName);
	}

	Ar.Logf( TEXT( "" ) );
	Ar.Logf( TEXT( "Primitives with unbuilt interactions: %d" ), PrimitivesWithUnbuiltInteractions.Num() );
	for (auto &PrimitiveName : PrimitivesWithUnbuiltInteractions)
	{
		Ar.Logf(TEXT("    Primitive %s"), *PrimitiveName);
	}
}

/**
 * Exports the scene.
 *
 * @param	Ar		The Archive used for exporting.
 **/
void FScene::Export( FArchive& Ar ) const
{
	
}

void FScene::ApplyWorldOffset(const FVector& InOffset)
{
	// Send a command to the rendering thread to shift scene data
	FScene* Scene = this;
	FVector Offset = InOffset;
	ENQUEUE_RENDER_COMMAND(FApplyWorldOffset)(
		[Scene, Offset](FRHICommandListImmediate& RHICmdList)
		{
			Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);
			Scene->ApplyWorldOffset_RenderThread(Offset);
		});
}

void FScene::ApplyWorldOffset_RenderThread(const FVector& InOffset)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_SceneApplyWorldOffset);
	SCOPED_NAMED_EVENT(FScene_ApplyWorldOffset_RenderThread, FColor::Yellow);
	
	GPUScene.bUpdateAllPrimitives = true;

	// Primitives
	checkf(AddedPrimitiveSceneInfos.Num() == 0, TEXT("All primitives found in AddedPrimitiveSceneInfos must have been added to the scene before the world offset is applied"));
	for (int32 Idx = 0; Idx < Primitives.Num(); ++Idx)
	{
		Primitives[Idx]->ApplyWorldOffset(InOffset);
	}

	// Primitive transforms
	for (int32 Idx = 0; Idx < PrimitiveTransforms.Num(); ++Idx)
	{
		PrimitiveTransforms[Idx].SetOrigin(PrimitiveTransforms[Idx].GetOrigin() + InOffset);
	}

	// Primitive bounds
	for (int32 Idx = 0; Idx < PrimitiveBounds.Num(); ++Idx)
	{
		PrimitiveBounds[Idx].BoxSphereBounds.Origin+= InOffset;
	}

#if RHI_RAYTRACING
	for (auto& BoundsPair : PrimitiveRayTracingGroups)
	{
		BoundsPair.Value.Bounds.Origin += InOffset;
	}
#endif

	// Primitive occlusion bounds
	for (int32 Idx = 0; Idx < PrimitiveOcclusionBounds.Num(); ++Idx)
	{
		PrimitiveOcclusionBounds[Idx].Origin+= InOffset;
	}
	
	// Precomputed light volumes
	for (const FPrecomputedLightVolume* It : PrecomputedLightVolumes)
	{
		const_cast<FPrecomputedLightVolume*>(It)->ApplyWorldOffset(InOffset);
	}

	// Precomputed visibility
	if (PrecomputedVisibilityHandler)
	{
		const_cast<FPrecomputedVisibilityHandler*>(PrecomputedVisibilityHandler)->ApplyWorldOffset(InOffset);
	}
	
	// Invalidate indirect lighting cache
	IndirectLightingCache.SetLightingCacheDirty(this, NULL);

	// Primitives octree
	PrimitiveOctree.ApplyOffset(InOffset, /*bGlobalOctee*/ true);

	// Lights
	VectorRegister OffsetReg = VectorLoadFloat3_W0(&InOffset);
	for (auto It = Lights.CreateIterator(); It; ++It)
	{
		(*It).BoundingSphereVector = VectorAdd((*It).BoundingSphereVector, OffsetReg);
		(*It).LightSceneInfo->Proxy->ApplyWorldOffset(InOffset);
	}

	LocalShadowCastingLightOctree.ApplyOffset(InOffset, /*bGlobalOctee*/ true);

	// Cached preshadows
	for (auto It = CachedPreshadows.CreateIterator(); It; ++It)
	{
		(*It)->PreShadowTranslation-= InOffset;
		(*It)->ShadowBounds.Center+= InOffset;
	}

	// Decals
	for (auto It = Decals.CreateIterator(); It; ++It)
	{
		(*It)->ComponentTrans.AddToTranslation(InOffset);
	}

	// Wind sources
	for (auto It = WindSources.CreateIterator(); It; ++It)
	{
		(*It)->ApplyWorldOffset(InOffset);
	}

	// Reflection captures
	for (auto It = ReflectionSceneData.RegisteredReflectionCaptures.CreateIterator(); It; ++It)
	{
		FMatrix NewTransform = FMatrix((*It)->BoxTransform.Inverse().ConcatTranslation((FVector3f)InOffset));
		(*It)->SetTransform(NewTransform);
	}

	// Planar reflections
	for (auto It = PlanarReflections.CreateIterator(); It; ++It)
	{
		(*It)->ApplyWorldOffset(InOffset);
	}
	
	// Exponential Fog
	for (FExponentialHeightFogSceneInfo& FogInfo : ExponentialFogs)
	{
		for (FExponentialHeightFogSceneInfo::FExponentialHeightFogSceneData& FogData : FogInfo.FogData)
		{
			FogData.Height += InOffset.Z;
		}
	}

	// SkyAtmospheres
	for (FSkyAtmosphereSceneProxy* SkyAtmosphereProxy : SkyAtmosphereStack)
	{
		SkyAtmosphereProxy->ApplyWorldOffset((FVector3f)InOffset);
	}
	
	
	VelocityData.ApplyOffset(InOffset);
}

void FScene::OnLevelAddedToWorld(const FName& InLevelAddedName, UWorld* InWorld, bool bIsLightingScenario)
{
	if (bIsLightingScenario)
	{
		InWorld->PropagateLightingScenarioChange();
	}

	FScene* Scene = this;
	FName LevelAddedName = InLevelAddedName;
	ENQUEUE_RENDER_COMMAND(FLevelAddedToWorld)(
		[Scene, LevelAddedName](FRHICommandListImmediate& RHICmdList)
		{
			Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);
			Scene->OnLevelAddedToWorld_RenderThread(LevelAddedName);
		});
}

void FScene::OnLevelAddedToWorld_RenderThread(const FName& InLevelName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScene::OnLevelAddedToWorld_RenderThread);

	// Mark level primitives
	TArray<FPrimitiveSceneInfo*> PrimitivesToAdd;

	if (const TArray<FPrimitiveSceneInfo*>* LevelPrimitives = PrimitivesNeedingLevelUpdateNotification.Find(InLevelName))
	{
		for (FPrimitiveSceneInfo* Primitive : *LevelPrimitives)
		{
			// If the primitive proxy returns true, it needs it's static meshes added to the scene
			if (Primitive->Proxy->OnLevelAddedToWorld_RenderThread())
			{
				// Remove static meshes and cached commands for any primitives that need to be added
				Primitive->RemoveStaticMeshes();
				PrimitivesToAdd.Add(Primitive);
			}
			// Invalidate primitive proxy entry in GPU Scene. 
			// This is necessary for Nanite::FSceneProxy to be uploaded to GPU scene (see GetPrimitiveID in GPUScene.cpp)
			if (Primitive->Proxy->IsNaniteMesh())
			{
				Primitive->RequestGPUSceneUpdate();
			}
		}
	}

	FPrimitiveSceneInfo::AddStaticMeshes(this, PrimitivesToAdd);
}

void FScene::OnLevelRemovedFromWorld(const FName& InLevelRemovedName, UWorld* InWorld, bool bIsLightingScenario)
{
	if (bIsLightingScenario)
	{
		InWorld->PropagateLightingScenarioChange();
	}

	FScene* Scene = this;
	FName LevelRemovedName = InLevelRemovedName;
	ENQUEUE_RENDER_COMMAND(FLevelRemovedFromWorld)(
		[Scene, LevelRemovedName](FRHICommandListImmediate& RHICmdList)
		{
			Scene->UpdateAllPrimitiveSceneInfos(RHICmdList);
			Scene->OnLevelRemovedFromWorld_RenderThread(LevelRemovedName);
		});
}


void FScene::OnLevelRemovedFromWorld_RenderThread(const FName& InLevelName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FScene::OnLevelRemovedFromWorld_RenderThread);

	if (TArray<FPrimitiveSceneInfo*>* LevelPrimitives = PrimitivesNeedingLevelUpdateNotification.Find(InLevelName))
	{
		for (FPrimitiveSceneInfo* Primitive : *LevelPrimitives)
		{
			Primitive->Proxy->OnLevelRemovedFromWorld_RenderThread();
			// Invalidate primitive proxy entry in GPU Scene.
			// This is necessary for Nanite::FSceneProxy to be uploaded to GPU scene (see GetPrimitiveID in GPUScene.cpp)
			if (Primitive->Proxy->IsNaniteMesh())
			{
				Primitive->RequestGPUSceneUpdate();
			}
		}
	}
}


void FScene::ProcessAtmosphereLightAddition_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	if (LightSceneInfo->Proxy->IsUsedAsAtmosphereSunLight())
	{
		const uint8 Index = LightSceneInfo->Proxy->GetAtmosphereSunLightIndex();
		if (!AtmosphereLights[Index] ||																								// Set it if null
			LightSceneInfo->Proxy->GetColor().GetLuminance() > AtmosphereLights[Index]->Proxy->GetColor().GetLuminance())	// Or choose the brightest sun light
		{
			AtmosphereLights[Index] = LightSceneInfo;
		}
	}
}

void FScene::ProcessAtmosphereLightRemoval_RenderThread(FLightSceneInfo* LightSceneInfo)
{
	// When a light has its intensity or index changed, it will be removed first, then re-added. So we only need to check the index of the removed light.
	const uint8 Index = LightSceneInfo->Proxy->GetAtmosphereSunLightIndex();
	if (AtmosphereLights[Index] == LightSceneInfo)
	{
		AtmosphereLights[Index] = nullptr;
		float SelectedLightLuminance = 0.0f;

		for (auto It = Lights.CreateConstIterator(); It; ++It)
		{
			const FLightSceneInfoCompact& LightInfo = *It;
			float LightLuminance = LightInfo.LightSceneInfo->Proxy->GetColor().GetLuminance();

			if (LightInfo.LightSceneInfo != LightSceneInfo
				&& LightInfo.LightSceneInfo->Proxy->IsUsedAsAtmosphereSunLight() && LightInfo.LightSceneInfo->Proxy->GetAtmosphereSunLightIndex() == Index
				&& (!AtmosphereLights[Index] || SelectedLightLuminance < LightLuminance))
			{
				AtmosphereLights[Index] = LightInfo.LightSceneInfo;
				SelectedLightLuminance = LightLuminance;
			}
		}
	}
}

#if WITH_EDITOR
bool FScene::InitializePixelInspector(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDEF, int32 BufferIndex)
{
	//Initialize the buffers
	PixelInspectorData.InitializeBuffers(BufferFinalColor, BufferSceneColor, BufferDepth, BufferHDR, BufferA, BufferBCDEF, BufferIndex);
	//return true when the interface is implemented
	return true;
}

bool FScene::AddPixelInspectorRequest(FPixelInspectorRequest *PixelInspectorRequest)
{
	return PixelInspectorData.AddPixelInspectorRequest(PixelInspectorRequest);
}
#endif //WITH_EDITOR


struct FPrimitiveArraySortKey
{
	inline bool operator()(const FPrimitiveSceneInfo& A, const FPrimitiveSceneInfo& B) const
	{
		uint32 AHash = A.Proxy->GetTypeHash();
		uint32 BHash = B.Proxy->GetTypeHash();

		if (AHash == BHash) 
		{
			return A.RegistrationSerialNumber < B.RegistrationSerialNumber;
		}
		else
		{
			return AHash < BHash;
		}
	}
};

static bool ShouldPrimitiveOutputVelocity(const FPrimitiveSceneProxy* Proxy, const FStaticShaderPlatform ShaderPlatform)
{
	bool bShouldPrimitiveOutputVelocity = Proxy->HasDynamicTransform();

	bool bPlatformSupportsVelocityRendering = PlatformSupportsVelocityRendering(ShaderPlatform);

	return bPlatformSupportsVelocityRendering && bShouldPrimitiveOutputVelocity;
}

void FScene::UpdatePrimitiveVelocityState_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsBeingMoved)
{
	if (bIsBeingMoved)
	{
		if (ShouldPrimitiveOutputVelocity(PrimitiveSceneInfo->Proxy, GetShaderPlatform()))
		{
			if (PrimitiveSceneInfo->IsIndexValid())
			{
				PrimitiveSceneInfo->bRegisteredWithVelocityData = true;
				// We must register the initial LocalToWorld with the velocity state. 
				int32 PrimitiveIndex = PrimitiveSceneInfo->PackedIndex;
				VelocityData.UpdateTransform(PrimitiveSceneInfo, PrimitiveTransforms[PrimitiveIndex], PrimitiveTransforms[PrimitiveIndex]);
			}
		}
	}
	else if (PrimitiveSceneInfo->bRegisteredWithVelocityData)
	{
		PrimitiveSceneInfo->bRegisteredWithVelocityData = false;
		VelocityData.RemoveFromScene(PrimitiveSceneInfo->PrimitiveComponentId, true);
	}
}

#if RHI_RAYTRACING
void FScene::UpdateRayTracingGroupBounds_AddPrimitives(const Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*>& PrimitiveSceneInfos)
{
	for (FPrimitiveSceneInfo* const PrimitiveSceneInfo : PrimitiveSceneInfos)
	{
		const int32 GroupId = PrimitiveSceneInfo->Proxy->GetRayTracingGroupId();
		if (GroupId != -1)
		{
			bool bInMap = false;
			static const FRayTracingCullingGroup DefaultGroup;
			FRayTracingCullingGroup* const Group = PrimitiveRayTracingGroups.FindOrAdd(GroupId, DefaultGroup, bInMap);
			if (bInMap)
			{
				Group->Bounds = Group->Bounds + PrimitiveSceneInfo->Proxy->GetBounds();
				Group->MinDrawDistance = FMath::Max(Group->MinDrawDistance, PrimitiveSceneInfo->Proxy->GetMinDrawDistance());
			}
			else
			{
				Group->Bounds = PrimitiveSceneInfo->Proxy->GetBounds();
				Group->MinDrawDistance = PrimitiveSceneInfo->Proxy->GetMinDrawDistance();
			}
			Group->Primitives.Add(PrimitiveSceneInfo);
		}
	}
}

static void UpdateRayTracingGroupBounds(Experimental::TRobinHoodHashSet<FScene::FRayTracingCullingGroup*>& GroupsToUpdate)
{
	for (FScene::FRayTracingCullingGroup* const Group : GroupsToUpdate)
	{
		bool bFirstBounds = false;
		for (FPrimitiveSceneInfo* const Primitive : Group->Primitives)
		{
			if (!bFirstBounds)
			{
				Group->Bounds = Primitive->Proxy->GetBounds();
				bFirstBounds = true;
			}
			else
			{
				Group->Bounds = Group->Bounds + Primitive->Proxy->GetBounds();
			}
		}
	}
}

void FScene::UpdateRayTracingGroupBounds_RemovePrimitives(const Experimental::TRobinHoodHashSet<FPrimitiveSceneInfo*>& PrimitiveSceneInfos)
{
	Experimental::TRobinHoodHashSet<FRayTracingCullingGroup*> GroupsToUpdate;
	for (FPrimitiveSceneInfo* const PrimitiveSceneInfo : PrimitiveSceneInfos)
	{
		const int32 RayTracingGroupId = PrimitiveSceneInfo->Proxy->GetRayTracingGroupId();
		const Experimental::FHashElementId GroupId = (RayTracingGroupId != -1) ? PrimitiveRayTracingGroups.FindId(RayTracingGroupId) : Experimental::FHashElementId();
		if (GroupId.IsValid())
		{
			FRayTracingCullingGroup& Group = PrimitiveRayTracingGroups.GetByElementId(GroupId).Value;
			Group.Primitives.RemoveSingleSwap(PrimitiveSceneInfo);
			if (Group.Primitives.Num() == 0)
			{
				PrimitiveRayTracingGroups.RemoveByElementId(GroupId);
			}
			else
			{
				GroupsToUpdate.FindOrAdd(&Group);
			}
		}
	}

	UpdateRayTracingGroupBounds(GroupsToUpdate);
}

template<typename ValueType>
inline void FScene::UpdateRayTracingGroupBounds_UpdatePrimitives(const Experimental::TRobinHoodHashMap<FPrimitiveSceneProxy*, ValueType>& InUpdatedTransforms)
{
	Experimental::TRobinHoodHashSet<FRayTracingCullingGroup*> GroupsToUpdate;
	for (const auto& Transform : InUpdatedTransforms)
	{
		FPrimitiveSceneProxy* const PrimitiveSceneProxy = Transform.Key;
		const int32 RayTracingGroupId = PrimitiveSceneProxy->GetRayTracingGroupId();
		const Experimental::FHashElementId GroupId = (RayTracingGroupId != -1) ? PrimitiveRayTracingGroups.FindId(RayTracingGroupId) : Experimental::FHashElementId();
		if (GroupId.IsValid())
		{
			FRayTracingCullingGroup& Group = PrimitiveRayTracingGroups.GetByElementId(GroupId).Value;
			GroupsToUpdate.FindOrAdd(&Group);
		}
	}

	UpdateRayTracingGroupBounds(GroupsToUpdate);
}
#endif

static inline bool IsPrimitiveRelevantToPathTracing(FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
#if RHI_RAYTRACING
	bool bIsAffectsIndirectLightingWhileHidden = false;
	bool bCastsHiddenShadow = false;
	if (PrimitiveSceneInfo->Proxy)
	{
		bIsAffectsIndirectLightingWhileHidden = PrimitiveSceneInfo->Proxy->AffectsIndirectLightingWhileHidden();
		bCastsHiddenShadow = PrimitiveSceneInfo->Proxy->CastsHiddenShadow();
	}

	// returns true if the primitive is likely to impact the path traced image
	return PrimitiveSceneInfo->bIsRayTracingRelevant &&
		   PrimitiveSceneInfo->bIsVisibleInRayTracing &&
		   (PrimitiveSceneInfo->bDrawInGame || 
			   bIsAffectsIndirectLightingWhileHidden ||
			   bCastsHiddenShadow) &&
		   PrimitiveSceneInfo->bShouldRenderInMainPass;
#else
	return false;
#endif
}


FLightSceneChangeSet FScene::UpdateAllLightSceneInfos(FRDGBuilder& GraphBuilder)
{
	struct FFLightSceneChangeSetAllocation
	{
		TArray<int32, SceneRenderingAllocator> RemovedLightIds;
		TArray<int32, SceneRenderingAllocator> TransformUpdatedLightIds;
		TArray<int32, SceneRenderingAllocator> ColorUpdatedLightIds;
		TArray<int32, SceneRenderingAllocator> AddedLightIds;
	};
	// Allocate change set storage with graph builder lifetime such that we can safely pass it to async tasks.
	FFLightSceneChangeSetAllocation& ChangeSet = *GraphBuilder.AllocObject<FFLightSceneChangeSetAllocation>();

	// Conservative pre-size the arrays (some updates are covered by Adds).
	ChangeSet.RemovedLightIds.Reserve(SceneLightInfoUpdates->NumRemoves);
	ChangeSet.TransformUpdatedLightIds.Reserve(SceneLightInfoUpdates->NumUpdates);
	ChangeSet.ColorUpdatedLightIds.Reserve(SceneLightInfoUpdates->NumUpdates);
	ChangeSet.AddedLightIds.Reserve(SceneLightInfoUpdates->NumAdds);

	// Filter out removes & updates:
	for (const auto& Element : SceneLightInfoUpdates->Commands)
	{
		const FUpdateLightCommand& UpdateLightCommand = Element.Value;
		int32 Id = UpdateLightCommand.LightSceneInfo->Id;
		if (UpdateLightCommand.IsRemove())
		{
			ChangeSet.RemovedLightIds.Add(Id);
		}
		else if (!UpdateLightCommand.IsAdd())
		{
			check(Id != INDEX_NONE);
			if (UpdateLightCommand.bHasTransform)
			{
				ChangeSet.TransformUpdatedLightIds.Add(Id);
			}
			if (UpdateLightCommand.bHasColor)
			{
				ChangeSet.ColorUpdatedLightIds.Add(Id);
			}
		}
	}
	// This can't access the scene light data if done async since it happens before the actual removals.
	OnPreLigtSceneInfoUpdate.Broadcast(FLightSceneChangeSet{ ChangeSet.RemovedLightIds, TConstArrayView<int32>(), ChangeSet.TransformUpdatedLightIds, ChangeSet.ColorUpdatedLightIds });

	// Batch process all light removes
	for (int32 LightId : ChangeSet.RemovedLightIds)
	{
		FLightSceneInfo* LightSceneInfo = Lights[LightId].LightSceneInfo;
		FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());
		RemoveLightSceneInfo_RenderThread(LightSceneInfo);
	}

	// Process all light adds & updates
	for (const auto &Element : SceneLightInfoUpdates->Commands)
	{
		const FUpdateLightCommand& UpdateLightCommand = Element.Value;
		if (UpdateLightCommand.IsRemove())
		{
			continue;
		}

		FLightSceneInfo* LightSceneInfo = UpdateLightCommand.LightSceneInfo;
		FScopeCycleCounter Context(LightSceneInfo->Proxy->GetStatId());

		const int32 Id = LightSceneInfo->Id;
		const bool bHasId = Id != INDEX_NONE;
		check(bHasId == !UpdateLightCommand.IsAdd());
		// Directly process updates.
		if (UpdateLightCommand.bHasTransform)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateSceneLightTime);

			UpdateLightTransform_RenderThread(Id, LightSceneInfo, UpdateLightCommand.TransformParameters);
		}
		if (UpdateLightCommand.bHasColor)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateSceneLightTime);

			const FUpdateLightCommand::FColorParameters& NewParameters = UpdateLightCommand.ColorParameters;

			// Mobile renderer:
			// a light with no color/intensity can cause the light to be ignored when rendering.
			// thus, lights that change state in this way must update the draw lists.
			bScenesPrimitivesNeedStaticMeshElementUpdate =
				bScenesPrimitivesNeedStaticMeshElementUpdate ||
				(GetShadingPath() == EShadingPath::Mobile
					&& NewParameters.NewColor.IsAlmostBlack() != LightSceneInfo->Proxy->GetColor().IsAlmostBlack());

			// Path Tracing: something about the light has changed, restart path traced accumulation
			InvalidatePathTracedOutput();

			LightSceneInfo->Proxy->SetColor(NewParameters.NewColor);
			LightSceneInfo->Proxy->IndirectLightingScale = NewParameters.NewIndirectLightingScale;
			LightSceneInfo->Proxy->VolumetricScatteringIntensity = NewParameters.NewVolumetricScatteringIntensity;

			// Also update the LightSceneInfoCompact (if it does not have an ID, it is being added)
			if (bHasId)
			{
				Lights[Id].Color = NewParameters.NewColor;
			}
		}

		// Perform Add after update, since that reduces redundant processing (e.g., Add + Move)
		if (UpdateLightCommand.IsAdd())
		{
			AddLightSceneInfo_RenderThread(LightSceneInfo);
			// Note: Id is set in AddLightSceneInfo_RenderThread so we must fetch it again
			ChangeSet.AddedLightIds.Add(LightSceneInfo->Id);
		}
	}

	OnPostLigtSceneInfoUpdate.Broadcast(FLightSceneChangeSet{ ChangeSet.RemovedLightIds, ChangeSet.AddedLightIds, ChangeSet.TransformUpdatedLightIds, ChangeSet.ColorUpdatedLightIds });

	SceneLightInfoUpdates->Reset();

	return FLightSceneChangeSet{ ChangeSet.RemovedLightIds, ChangeSet.AddedLightIds, ChangeSet.TransformUpdatedLightIds, ChangeSet.ColorUpdatedLightIds };
}

void FScene::UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, EUpdateAllPrimitiveSceneInfosAsyncOps AsyncOps)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Scene::UpdateAllPrimitiveSceneInfos);
	SCOPED_NAMED_EVENT(FScene_UpdateAllPrimitiveSceneInfos, FColor::Orange);
	SCOPE_CYCLE_COUNTER(STAT_UpdateScenePrimitiveRenderThreadTime);

	check(IsInRenderingThread());
	FSceneRenderer::WaitForCleanUpTasks(GraphBuilder.RHICmdList);
	FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

	RDG_EVENT_SCOPE(GraphBuilder, "UpdateAllPrimitiveSceneInfos");

	UpdateAllLightSceneInfos(GraphBuilder);

#if RHI_RAYTRACING
	UpdateRayTracingGroupBounds_RemovePrimitives(RemovedPrimitiveSceneInfos);
	UpdateRayTracingGroupBounds_AddPrimitives(AddedPrimitiveSceneInfos);
#endif

	TArray<FPrimitiveSceneInfo*> RemovedLocalPrimitiveSceneInfos;
	RemovedLocalPrimitiveSceneInfos.Reserve(RemovedPrimitiveSceneInfos.Num());
	for (FPrimitiveSceneInfo* SceneInfo : RemovedPrimitiveSceneInfos)
	{
		RemovedLocalPrimitiveSceneInfos.Add(SceneInfo);
	}
	// NOTE: We clear this early because IsPrimitiveBeingRemoved gets called from the CreateLightPrimitiveInteraction (to make sure that old primitives are not accessed) 
	// we cannot safely kick off the AsyncCreateLightPrimitiveInteractionsTask before the RemovedPrimitiveSceneInfos has been cleared.
	RemovedPrimitiveSceneInfos.Empty();

	RemovedLocalPrimitiveSceneInfos.Sort(FPrimitiveArraySortKey());

	TArray<FVirtualShadowMapArrayCacheManager*, SceneRenderingAllocator> VirtualShadowCacheManagers;
	GetAllVirtualShadowMapCacheManagers(VirtualShadowCacheManagers);

	for (FVirtualShadowMapArrayCacheManager* CacheManager : VirtualShadowCacheManagers)
	{
		FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector InvalidatingPrimitiveCollector(CacheManager);

		// All removed primitives must invalidate their footprints in the VSM before leaving
		for (FPrimitiveSceneInfo* PrimitiveSceneInfo : RemovedLocalPrimitiveSceneInfos)
		{
			InvalidatingPrimitiveCollector.Removed(PrimitiveSceneInfo);
		}
		// All updated instances must also before moving or re-allocating (TODO: filter out only those actually updated)
		for (const auto& Instance : UpdatedInstances)
		{
			InvalidatingPrimitiveCollector.UpdatedInstances(Instance.Key->GetPrimitiveSceneInfo());
		}
		// As must all primitive updates, 
		for (const auto& Transform : UpdatedTransforms)
		{
			InvalidatingPrimitiveCollector.UpdatedTransform(Transform.Key->GetPrimitiveSceneInfo());
		}

		CacheManager->ProcessRemovedOrUpdatedPrimitives(GraphBuilder, GPUScene, InvalidatingPrimitiveCollector);
	}
	TArray<FPrimitiveSceneInfo*> AddedLocalPrimitiveSceneInfos;
	AddedLocalPrimitiveSceneInfos.Reserve(AddedPrimitiveSceneInfos.Num());
	for (FPrimitiveSceneInfo* SceneInfo : AddedPrimitiveSceneInfos)
	{
		AddedLocalPrimitiveSceneInfos.Add(SceneInfo);
	}

	AddedLocalPrimitiveSceneInfos.Sort(FPrimitiveArraySortKey());

	TSet<FPrimitiveSceneInfo*> DeletedSceneInfos;
	DeletedSceneInfos.Reserve(RemovedLocalPrimitiveSceneInfos.Num());

	TArray<int32> RemovedPrimitiveIndices;
	RemovedPrimitiveIndices.SetNumUninitialized(RemovedLocalPrimitiveSceneInfos.Num());

	GPUScene.ResizeDirtyState(Primitives.Num());
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(RemovePrimitiveSceneInfos);
		SCOPED_NAMED_EVENT(FScene_RemovePrimitiveSceneInfos, FColor::Red);
		SCOPE_CYCLE_COUNTER(STAT_RemoveScenePrimitiveTime);

		GPUScene.BeginDeferAllocatorMerges();

		for (FPrimitiveSceneInfo* PrimitiveSceneInfo : RemovedLocalPrimitiveSceneInfos)
		{
			// clear it up, parent is getting removed
			SceneLODHierarchy.UpdateNodeSceneInfo(PrimitiveSceneInfo->PrimitiveComponentId, nullptr);
		}

		while (RemovedLocalPrimitiveSceneInfos.Num())
		{
			int32 StartIndex = RemovedLocalPrimitiveSceneInfos.Num() - 1;
			SIZE_T InsertProxyHash = RemovedLocalPrimitiveSceneInfos[StartIndex]->Proxy->GetTypeHash();

			while (StartIndex > 0 && RemovedLocalPrimitiveSceneInfos[StartIndex - 1]->Proxy->GetTypeHash() == InsertProxyHash)
			{
				StartIndex--;
			}

			int32 BroadIndex = -1;
			//broad phase search for a matching type
			for (BroadIndex = TypeOffsetTable.Num() - 1; BroadIndex >= 0; BroadIndex--)
			{
				// example how the prefix sum of the tails could look like
				// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8]
				// TypeOffsetTable[3,8,12,15,16,17,18]

				if (TypeOffsetTable[BroadIndex].PrimitiveSceneProxyType == InsertProxyHash)
				{
					const int32 InsertionOffset = TypeOffsetTable[BroadIndex].Offset;
					const int32 PrevOffset = BroadIndex > 0 ? TypeOffsetTable[BroadIndex - 1].Offset : 0;
					for (int32 CheckIndex = StartIndex; CheckIndex < RemovedLocalPrimitiveSceneInfos.Num(); CheckIndex++)
					{
						int32 PrimitiveIndex = RemovedLocalPrimitiveSceneInfos[CheckIndex]->PackedIndex;
						checkfSlow(PrimitiveIndex >= PrevOffset && PrimitiveIndex < InsertionOffset, TEXT("PrimitiveIndex %d not in Bucket Range [%d, %d]"), PrimitiveIndex, PrevOffset, InsertionOffset);
					}
					break;
				}
			}

			{
				SCOPED_NAMED_EVENT(FScene_SwapPrimitiveSceneInfos, FColor::Turquoise);

				for (int32 CheckIndex = StartIndex; CheckIndex < RemovedLocalPrimitiveSceneInfos.Num(); CheckIndex++)
				{
					int32 SourceIndex = RemovedLocalPrimitiveSceneInfos[CheckIndex]->PackedIndex;

					for (int32 TypeIndex = BroadIndex; TypeIndex < TypeOffsetTable.Num(); TypeIndex++)
					{
						FTypeOffsetTableEntry& NextEntry = TypeOffsetTable[TypeIndex];
						int DestIndex = --NextEntry.Offset; //decrement and prepare swap 

						// example swap chain of removing X 
						// PrimitiveSceneProxies[0,0,0,6,X,6,6,6,2,2,2,2,1,1,1,7,4,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,X,2,2,2,1,1,1,7,4,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,X,1,1,1,7,4,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,1,1,1,X,7,4,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,1,1,1,7,X,4,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,1,1,1,7,4,X,8]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,1,1,1,7,4,8,X]

						if (DestIndex != SourceIndex)
						{
							checkfSlow(DestIndex > SourceIndex, TEXT("Corrupted Prefix Sum [%d, %d]"), DestIndex, SourceIndex);
							Primitives[DestIndex]->PackedIndex = SourceIndex;
							// Update (the dynamic/compacted) primitive ID for the swapped primitive (not moved), no need to do the other one since it is being removed.
							FPersistentPrimitiveIndex MovedPersisitentIndex = Primitives[DestIndex]->PersistentIndex;
							PersistentPrimitiveIdToIndexMap[MovedPersisitentIndex.Index] = SourceIndex;

							Primitives[SourceIndex]->PackedIndex = DestIndex;

							TArraySwapElements(Primitives, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveTransforms, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveSceneProxies, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveBounds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveFlagsCompact, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVisibilityIds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOctreeIndex, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOcclusionFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveComponentIds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVirtualTextureFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVirtualTextureLod, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOcclusionBounds, DestIndex, SourceIndex);
						#if WITH_EDITOR
							TBitArraySwapElements(PrimitivesSelected, DestIndex, SourceIndex);
						#endif
						#if RHI_RAYTRACING
							TArraySwapElements(PrimitiveRayTracingFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveRayTracingGroupIds, DestIndex, SourceIndex);
						#endif
							TBitArraySwapElements(PrimitivesNeedingStaticMeshUpdate, DestIndex, SourceIndex);

							for (TMap<int32, TArray<FCachedShadowMapData>>::TIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
							{
								TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

								for (auto& ShadowMapData : ShadowMapDatas)
								{
									if (ShadowMapData.StaticShadowSubjectMap.Num() > 0)
									{
										TBitArraySwapElements(ShadowMapData.StaticShadowSubjectMap, DestIndex, SourceIndex);
									}
								}
							}
							GPUScene.RecordPrimitiveIdSwap(DestIndex, SourceIndex);

						#if RHI_RAYTRACING
							// Update cached PrimitiveIndex after an index swap
							Primitives[SourceIndex]->CachedRayTracingInstance.DefaultUserData = SourceIndex;
							Primitives[DestIndex]->CachedRayTracingInstance.DefaultUserData = DestIndex;
						#endif

							SourceIndex = DestIndex;
						}
					}
				}
			}

			const int32 PreviousOffset = BroadIndex > 0 ? TypeOffsetTable[BroadIndex - 1].Offset : 0;
			const int32 CurrentOffset = TypeOffsetTable[BroadIndex].Offset;

			checkfSlow(PreviousOffset <= CurrentOffset, TEXT("Corrupted Bucket [%d, %d]"), PreviousOffset, CurrentOffset);
			if (CurrentOffset - PreviousOffset == 0)
			{
				// remove empty OffsetTable entries e.g.
				// TypeOffsetTable[3,8,12,15,15,17,18]
				// TypeOffsetTable[3,8,12,15,17,18]
				TypeOffsetTable.RemoveAt(BroadIndex);
			}

			checkfSlow((TypeOffsetTable.Num() == 0 && Primitives.Num() == (RemovedLocalPrimitiveSceneInfos.Num() - StartIndex)) || TypeOffsetTable[TypeOffsetTable.Num() - 1].Offset == Primitives.Num() - (RemovedLocalPrimitiveSceneInfos.Num() - StartIndex), TEXT("Corrupted Tail Offset [%d, %d]"), TypeOffsetTable[TypeOffsetTable.Num() - 1].Offset, Primitives.Num() - (RemovedLocalPrimitiveSceneInfos.Num() - StartIndex));

			for (int32 RemoveIndex = StartIndex; RemoveIndex < RemovedLocalPrimitiveSceneInfos.Num(); RemoveIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = RemovedLocalPrimitiveSceneInfos[RemoveIndex];
				checkf(RemovedLocalPrimitiveSceneInfos[RemoveIndex]->PackedIndex >= Primitives.Num() - RemovedLocalPrimitiveSceneInfos.Num(), TEXT("Removed item should be at the end"));

				// Store the previous index for use later, and set the PackedIndex member to invalid.
				// FPrimitiveOctreeSemantics::SetOctreeNodeIndex will attempt to remove the node index from the 
				// PrimitiveOctreeIndex.  Since the elements have already been swapped, this will cause an invalid change to PrimitiveOctreeIndex.
				// Setting the packed index to INDEX_NONE prevents this from happening, but we also need to keep track of the old
				// index for use below.
				RemovedPrimitiveIndices[RemoveIndex] = RemovedLocalPrimitiveSceneInfos[RemoveIndex]->PackedIndex;
				PrimitiveSceneInfo->PackedIndex = INDEX_NONE;
			}

			//Remove all items from the location of StartIndex to the end of the arrays.
			int RemoveCount = RemovedLocalPrimitiveSceneInfos.Num() - StartIndex;
			int SourceIndex = Primitives.Num() - RemoveCount;

			Primitives.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveTransforms.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveSceneProxies.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveBounds.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveFlagsCompact.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveVisibilityIds.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveOctreeIndex.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveOcclusionFlags.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveComponentIds.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveVirtualTextureFlags.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveVirtualTextureLod.RemoveAt(SourceIndex, RemoveCount, false);
			PrimitiveOcclusionBounds.RemoveAt(SourceIndex, RemoveCount, false);

			#if WITH_EDITOR
			PrimitivesSelected.RemoveAt(SourceIndex, RemoveCount);
			#endif
			#if RHI_RAYTRACING
			PrimitiveRayTracingFlags.RemoveAt(SourceIndex, RemoveCount);
			PrimitiveRayTracingGroupIds.RemoveAt(SourceIndex, RemoveCount);
			#endif
			PrimitivesNeedingStaticMeshUpdate.RemoveAt(SourceIndex, RemoveCount);

			CheckPrimitiveArrays();

			for (int32 RemoveIndex = StartIndex; RemoveIndex < RemovedLocalPrimitiveSceneInfos.Num(); RemoveIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = RemovedLocalPrimitiveSceneInfos[RemoveIndex];
				FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;
				FScopeCycleCounter Context(SceneProxy->GetStatId());

				// The removed items PrimitiveIndex has already been invalidated, but a backup is kept in RemovedPrimitiveIndices
				int32 PrimitiveIndex = RemovedPrimitiveIndices[RemoveIndex];

				if (PrimitiveSceneInfo->bRegisteredWithVelocityData)
				{
					// Remove primitive's motion blur information.
					VelocityData.RemoveFromScene(PrimitiveSceneInfo->PrimitiveComponentId, false);
				}

				// Unlink the primitive from its shadow parent.
				PrimitiveSceneInfo->UnlinkAttachmentGroup();

				// Unlink the LOD parent info if valid
				PrimitiveSceneInfo->UnlinkLODParentComponent();

				// Flush virtual textures touched by primitive
				PrimitiveSceneInfo->FlushRuntimeVirtualTexture();

				// Remove the primitive from the scene.
				PrimitiveSceneInfo->RemoveFromScene(true);

				PrimitiveSceneInfo->FreeGPUSceneInstances();

				// Update the primitive that was swapped to this index
				GPUScene.AddPrimitiveToUpdate(PrimitiveIndex, EPrimitiveDirtyState::Removed);

				DistanceFieldSceneData.RemovePrimitive(PrimitiveSceneInfo);
				LumenRemovePrimitive(PrimitiveSceneInfo, PrimitiveIndex);

#if RHI_RAYTRACING
				if (SceneProxy->IsNaniteMesh() && SceneProxy->HasRayTracingRepresentation())
				{
					Nanite::GRayTracingManager.Remove(PrimitiveSceneInfo);
				}
#endif

				DeletedSceneInfos.Add(PrimitiveSceneInfo);

				for (const FLightSceneInfo* LightSceneInfo : DirectionalLights)
				{
					TArray<FCachedShadowMapData>* CachedShadowMapDatas = GetCachedShadowMapDatas(LightSceneInfo->Id);

					if (CachedShadowMapDatas)
					{
						for (auto& CachedShadowMapData : *CachedShadowMapDatas)
						{
							if (CachedShadowMapData.StaticShadowSubjectMap[PrimitiveIndex] == true)
							{
								CachedShadowMapData.InvalidateCachedShadow();
							}
						}
					}
				}

				const int32 PersistentIndex = PrimitiveSceneInfo->PersistentIndex.Index;
				PersistentPrimitiveIdAllocator.Free(PersistentIndex);
				PersistentPrimitiveIdToIndexMap[PersistentIndex] = INDEX_NONE;
			}

			for (TMap<int32, TArray<FCachedShadowMapData>>::TIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
			{
				TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

				for (auto& ShadowMapData : ShadowMapDatas)
				{
					if (ShadowMapData.StaticShadowSubjectMap.Num() > 0)
					{
						ShadowMapData.StaticShadowSubjectMap.RemoveAt(SourceIndex, RemoveCount);
						check(Primitives.Num() == ShadowMapData.StaticShadowSubjectMap.Num());
					}
				}
			}
			RemovedLocalPrimitiveSceneInfos.RemoveAt(StartIndex, RemovedLocalPrimitiveSceneInfos.Num() - StartIndex, false);
		}
	
		GPUScene.EndDeferAllocatorMerges();
	}

	bool bNeedPathTracedInvalidation = false;

	const int32 SceneInfosContainerReservedSize = AddedPrimitiveSceneInfos.Num() + UpdatedTransforms.Num() + UpdatedInstances.Num();

	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> SceneInfosToFlushVirtualTexture;
	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& SceneInfosWithStaticDrawListUpdate = *GraphBuilder.AllocObject<TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>>();
	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> SceneInfosWithoutStaticDrawListUpdate;
	SceneInfosToFlushVirtualTexture.Reserve(SceneInfosContainerReservedSize);
	SceneInfosWithStaticDrawListUpdate.Reserve(SceneInfosContainerReservedSize);
	SceneInfosWithoutStaticDrawListUpdate.Reserve(SceneInfosContainerReservedSize);

	const auto QueueSceneInfoToFlushVirtualTexture = [&](FPrimitiveSceneInfo* SceneInfo)
	{
		if (!SceneInfo->bPendingFlushVirtualTexture)
		{
			SceneInfo->bPendingFlushVirtualTexture = true;
			SceneInfosToFlushVirtualTexture.Push(SceneInfo);
		}
	};

	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(AddPrimitiveSceneInfos);
		SCOPED_NAMED_EVENT(FScene_AddPrimitiveSceneInfos, FColor::Green);
		SCOPE_CYCLE_COUNTER(STAT_AddScenePrimitiveRenderThreadTime);
#if ENABLE_LOG_PRIMITIVE_INSTANCE_ID_STATS_TO_CSV
		int32 PersistentPrimitiveFreeListSizeBeforeConsolidate = PersistentPrimitiveIdAllocator.GetNumFreeSpans();
		int32 PersistentPrimitivePendingFreeListSizeBeforeConsolidate = PersistentPrimitiveIdAllocator.GetNumPendingFreeSpans();
#endif
		PersistentPrimitiveIdAllocator.Consolidate();

		if (AddedLocalPrimitiveSceneInfos.Num())
		{
			Primitives.Reserve(Primitives.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveTransforms.Reserve(PrimitiveTransforms.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveSceneProxies.Reserve(PrimitiveSceneProxies.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveBounds.Reserve(PrimitiveBounds.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveFlagsCompact.Reserve(PrimitiveFlagsCompact.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveVisibilityIds.Reserve(PrimitiveVisibilityIds.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveOcclusionFlags.Reserve(PrimitiveOcclusionFlags.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveComponentIds.Reserve(PrimitiveComponentIds.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveVirtualTextureFlags.Reserve(PrimitiveVirtualTextureFlags.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveVirtualTextureLod.Reserve(PrimitiveVirtualTextureLod.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveOcclusionBounds.Reserve(PrimitiveOcclusionBounds.Num() + AddedLocalPrimitiveSceneInfos.Num());
		#if WITH_EDITOR
			PrimitivesSelected.Reserve(PrimitivesSelected.Num() + AddedLocalPrimitiveSceneInfos.Num());
		#endif
		#if RHI_RAYTRACING
			PrimitiveRayTracingFlags.Reserve(PrimitiveRayTracingFlags.Num() + AddedLocalPrimitiveSceneInfos.Num());
			PrimitiveRayTracingGroupIds.Reserve(PrimitiveRayTracingGroupIds.Num() + AddedLocalPrimitiveSceneInfos.Num());
		#endif
			PrimitivesNeedingStaticMeshUpdate.Reserve(PrimitivesNeedingStaticMeshUpdate.Num() + AddedLocalPrimitiveSceneInfos.Num());

			for (TMap<int32, TArray<FCachedShadowMapData>>::TIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
			{
				TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

				for (auto& ShadowMapData : ShadowMapDatas)
				{
					if (ShadowMapData.StaticShadowSubjectMap.Num() > 0)
					{
						ShadowMapData.StaticShadowSubjectMap.Reserve(ShadowMapData.StaticShadowSubjectMap.Num() + AddedLocalPrimitiveSceneInfos.Num());
					}
				}
			}
		}

		while (AddedLocalPrimitiveSceneInfos.Num())
		{
			int32 StartIndex = AddedLocalPrimitiveSceneInfos.Num() - 1;
			SIZE_T InsertProxyHash = AddedLocalPrimitiveSceneInfos[StartIndex]->Proxy->GetTypeHash();

			while (StartIndex > 0 && AddedLocalPrimitiveSceneInfos[StartIndex - 1]->Proxy->GetTypeHash() == InsertProxyHash)
			{
				StartIndex--;
			}

			for (int32 AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = AddedLocalPrimitiveSceneInfos[AddIndex];
				Primitives.Add(PrimitiveSceneInfo);
				const FMatrix LocalToWorld = PrimitiveSceneInfo->Proxy->GetLocalToWorld();
				PrimitiveTransforms.Add(LocalToWorld);
				PrimitiveSceneProxies.Add(PrimitiveSceneInfo->Proxy);
				PrimitiveBounds.AddUninitialized();
				PrimitiveFlagsCompact.AddUninitialized();
				PrimitiveVisibilityIds.AddUninitialized();
				PrimitiveOctreeIndex.Add(0);
				PrimitiveOcclusionFlags.AddUninitialized();
				PrimitiveComponentIds.AddUninitialized();
				PrimitiveVirtualTextureFlags.AddUninitialized();
				PrimitiveVirtualTextureLod.AddUninitialized();
				PrimitiveOcclusionBounds.AddUninitialized();
			#if WITH_EDITOR
				PrimitivesSelected.Add(PrimitiveSceneInfo->Proxy->IsSelected());
			#endif
			#if RHI_RAYTRACING
				PrimitiveRayTracingFlags.AddZeroed();
				PrimitiveRayTracingGroupIds.Add(Experimental::FHashElementId());
			#endif
				PrimitivesNeedingStaticMeshUpdate.Add(false);

				for (TMap<int32, TArray<FCachedShadowMapData>>::TIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
				{
					TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

					for (auto& ShadowMapData : ShadowMapDatas)
					{
						if (ShadowMapData.StaticShadowSubjectMap.Num() > 0)
						{
							ShadowMapData.StaticShadowSubjectMap.Add(false);
						}
					}
				}

				const int32 SourceIndex = PrimitiveSceneProxies.Num() - 1;
				PrimitiveSceneInfo->PackedIndex = SourceIndex;
				checkSlow(PrimitiveSceneInfo->PersistentIndex.Index == INDEX_NONE);
				FPersistentPrimitiveIndex PersistentPrimitiveIndex{ PersistentPrimitiveIdAllocator.Allocate() };
				PrimitiveSceneInfo->PersistentIndex = PersistentPrimitiveIndex;
				// Ensure map is large enough
				if (PersistentPrimitiveIndex.Index >= PersistentPrimitiveIdToIndexMap.Num())
				{
					PersistentPrimitiveIdToIndexMap.SetNumUninitialized(PersistentPrimitiveIndex.Index + 1);
				}
				PersistentPrimitiveIdToIndexMap[PersistentPrimitiveIndex.Index] = SourceIndex;

				GPUScene.AddPrimitiveToUpdate(SourceIndex, EPrimitiveDirtyState::AddedMask);
			}

			bool EntryFound = false;
			int32 BroadIndex = -1;
			//broad phase search for a matching type
			for (BroadIndex = TypeOffsetTable.Num() - 1; BroadIndex >= 0; BroadIndex--)
			{
				// example how the prefix sum of the tails could look like
				// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8]
				// TypeOffsetTable[3,8,12,15,16,17,18]

				if (TypeOffsetTable[BroadIndex].PrimitiveSceneProxyType == InsertProxyHash)
				{
					EntryFound = true;
					break;
				}
			}

			//new type encountered
			if (EntryFound == false)
			{
				BroadIndex = TypeOffsetTable.Num();
				if (BroadIndex)
				{
					FTypeOffsetTableEntry Entry = TypeOffsetTable[BroadIndex - 1];
					//adding to the end of the list and offset of the tail (will will be incremented once during the while loop)
					TypeOffsetTable.Push(FTypeOffsetTableEntry(InsertProxyHash, Entry.Offset));
				}
				else
				{
					//starting with an empty list and offset zero (will will be incremented once during the while loop)
					TypeOffsetTable.Push(FTypeOffsetTableEntry(InsertProxyHash, 0));
				}
			}

			{
				SCOPED_NAMED_EVENT(FScene_SwapPrimitiveSceneInfos, FColor::Turquoise);

				for (int32 AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
				{
					int32 SourceIndex = AddedLocalPrimitiveSceneInfos[AddIndex]->PackedIndex;

					for (int32 TypeIndex = BroadIndex; TypeIndex < TypeOffsetTable.Num(); TypeIndex++)
					{
						FTypeOffsetTableEntry& NextEntry = TypeOffsetTable[TypeIndex];
						int32 DestIndex = NextEntry.Offset++; //prepare swap and increment

						// example swap chain of inserting a type of 6 at the end
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8,6]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,1,1,1,7,4,8,2]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,7,4,8,1]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,1,4,8,7]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,1,7,8,4]
						// PrimitiveSceneProxies[0,0,0,6,6,6,6,6,6,2,2,2,2,1,1,1,7,4,8]

						if (DestIndex != SourceIndex)
						{
							checkfSlow(SourceIndex > DestIndex, TEXT("Corrupted Prefix Sum [%d, %d]"), SourceIndex, DestIndex);
							Primitives[DestIndex]->PackedIndex = SourceIndex;
							Primitives[SourceIndex]->PackedIndex = DestIndex;

							// Update (the dynamic/compacted) primitive ID for the swapped primitives
							{
								FPersistentPrimitiveIndex PersisitentIndex = Primitives[DestIndex]->PersistentIndex;
								PersistentPrimitiveIdToIndexMap[PersisitentIndex.Index] = SourceIndex;
							}
							{
								FPersistentPrimitiveIndex PersisitentIndex = Primitives[SourceIndex]->PersistentIndex;
								PersistentPrimitiveIdToIndexMap[PersisitentIndex.Index] = DestIndex;
							}
							TArraySwapElements(Primitives, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveTransforms, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveSceneProxies, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveBounds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveFlagsCompact, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVisibilityIds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOctreeIndex, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOcclusionFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveComponentIds, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVirtualTextureFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveVirtualTextureLod, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveOcclusionBounds, DestIndex, SourceIndex);
						#if WITH_EDITOR
							TBitArraySwapElements(PrimitivesSelected, DestIndex, SourceIndex);
						#endif
						#if RHI_RAYTRACING
							TArraySwapElements(PrimitiveRayTracingFlags, DestIndex, SourceIndex);
							TArraySwapElements(PrimitiveRayTracingGroupIds, DestIndex, SourceIndex);
						#endif
							TBitArraySwapElements(PrimitivesNeedingStaticMeshUpdate, DestIndex, SourceIndex);

							for (TMap<int32, TArray<FCachedShadowMapData>>::TIterator CachedShadowMapIt(CachedShadowMaps); CachedShadowMapIt; ++CachedShadowMapIt)
							{
								TArray<FCachedShadowMapData>& ShadowMapDatas = CachedShadowMapIt.Value();

								for (auto& ShadowMapData : ShadowMapDatas)
								{
									if (ShadowMapData.StaticShadowSubjectMap.Num() > 0)
									{
										TBitArraySwapElements(ShadowMapData.StaticShadowSubjectMap, DestIndex, SourceIndex);
									}
								}
							}

							GPUScene.RecordPrimitiveIdSwap(DestIndex, SourceIndex);

						#if RHI_RAYTRACING
							// Update cached PrimitiveIndex after an index swap
							Primitives[SourceIndex]->CachedRayTracingInstance.DefaultUserData = SourceIndex;
							Primitives[DestIndex]->CachedRayTracingInstance.DefaultUserData = DestIndex;
						#endif
						}
					}
				}
			}

			CheckPrimitiveArrays();

#if ENABLE_LOG_PRIMITIVE_INSTANCE_ID_STATS_TO_CSV
			UpdatePrimitiveAllocatorStats(Primitives.Num(), PersistentPrimitiveIdAllocator.GetMaxSize(), PersistentPrimitiveIdAllocator.GetSparselyAllocatedSize(), PersistentPrimitiveIdAllocator.GetNumFreeSpans(), PersistentPrimitiveFreeListSizeBeforeConsolidate, PersistentPrimitivePendingFreeListSizeBeforeConsolidate, GPUScene.GetNumInstances(), GPUScene.GetInstanceSceneDataAllocator().GetSparselyAllocatedSize());
#endif

			for (int32 AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = AddedLocalPrimitiveSceneInfos[AddIndex];
				FScopeCycleCounter Context(PrimitiveSceneInfo->Proxy->GetStatId());
				int32 PrimitiveIndex = PrimitiveSceneInfo->PackedIndex;

				// Add the primitive to its shadow parent's linked list of children.
				// Note: must happen before AddToScene because AddToScene depends on LightingAttachmentRoot
				PrimitiveSceneInfo->LinkAttachmentGroup();
			}

			FPrimitiveSceneInfo::AllocateGPUSceneInstances(this, TArrayView<FPrimitiveSceneInfo*>(&AddedLocalPrimitiveSceneInfos[StartIndex], AddedLocalPrimitiveSceneInfos.Num() - StartIndex));

			for (int AddIndex = StartIndex; AddIndex < AddedLocalPrimitiveSceneInfos.Num(); AddIndex++)
			{
				FPrimitiveSceneInfo* PrimitiveSceneInfo = AddedLocalPrimitiveSceneInfos[AddIndex];
				int32 PrimitiveIndex = PrimitiveSceneInfo->PackedIndex;
				FPrimitiveSceneProxy* SceneProxy = PrimitiveSceneInfo->Proxy;

				SceneInfosWithStaticDrawListUpdate.Push(PrimitiveSceneInfo);
				PrimitiveSceneInfo->bPendingAddToScene = true;

				if (ShouldPrimitiveOutputVelocity(SceneProxy, GetShaderPlatform()))
				{
					PrimitiveSceneInfo->bRegisteredWithVelocityData = true;
					// We must register the initial LocalToWorld with the velocity state. 
					// In the case of a moving component with MarkRenderStateDirty() called every frame, UpdateTransform will never happen.
					VelocityData.UpdateTransform(PrimitiveSceneInfo, PrimitiveTransforms[PrimitiveIndex], PrimitiveTransforms[PrimitiveIndex]);
				}

				DistanceFieldSceneData.AddPrimitive(PrimitiveSceneInfo);
				LumenAddPrimitive(PrimitiveSceneInfo);

#if RHI_RAYTRACING
				if (SceneProxy->IsNaniteMesh() && SceneProxy->HasRayTracingRepresentation())
				{
					Nanite::GRayTracingManager.Add(PrimitiveSceneInfo);
				}
#endif

				// Flush virtual textures touched by primitive
				QueueSceneInfoToFlushVirtualTexture(PrimitiveSceneInfo);

				bNeedPathTracedInvalidation = bNeedPathTracedInvalidation || IsPrimitiveRelevantToPathTracing(PrimitiveSceneInfo);
			}
			AddedLocalPrimitiveSceneInfos.RemoveAt(StartIndex, AddedLocalPrimitiveSceneInfos.Num() - StartIndex, false);
		}
	}
	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UpdatePrimitiveTransform);
		SCOPED_NAMED_EVENT(FScene_AddPrimitiveSceneInfos, FColor::Yellow);
		SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveTransformRenderThreadTime);

		for (const auto& Transform : UpdatedTransforms)
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = Transform.Key;
			if (DeletedSceneInfos.Contains(PrimitiveSceneProxy->GetPrimitiveSceneInfo()))
			{
				continue;
			}
			check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex != INDEX_NONE);

			const FBoxSphereBounds& WorldBounds = Transform.Value.WorldBounds;
			const FBoxSphereBounds& LocalBounds = Transform.Value.LocalBounds;
			const FMatrix& LocalToWorld = Transform.Value.LocalToWorld;
			const FVector& AttachmentRootPosition = Transform.Value.AttachmentRootPosition;
			FScopeCycleCounter Context(PrimitiveSceneProxy->GetStatId());

			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
			const bool bUpdateStaticDrawLists = !PrimitiveSceneProxy->StaticElementsAlwaysUseProxyPrimitiveUniformBuffer();

			if (!PrimitiveSceneInfo->bPendingAddToScene)
			{
				PrimitiveSceneInfo->bPendingAddToScene = true;

				if (bUpdateStaticDrawLists)
				{
					SceneInfosWithStaticDrawListUpdate.Push(PrimitiveSceneInfo);
				}
				else
				{
					SceneInfosWithoutStaticDrawListUpdate.Push(PrimitiveSceneInfo);
				}

				// Remove the primitive from the scene at its old location
				// (note that the octree update relies on the bounds not being modified yet).
				PrimitiveSceneInfo->RemoveFromScene(bUpdateStaticDrawLists);
			}

			QueueSceneInfoToFlushVirtualTexture(PrimitiveSceneInfo);

			if (ShouldPrimitiveOutputVelocity(PrimitiveSceneInfo->Proxy, GetShaderPlatform()))
			{
				PrimitiveSceneInfo->bRegisteredWithVelocityData = true;
				VelocityData.UpdateTransform(PrimitiveSceneInfo, LocalToWorld, PrimitiveSceneProxy->GetLocalToWorld());
			}

			bNeedPathTracedInvalidation = bNeedPathTracedInvalidation || (IsPrimitiveRelevantToPathTracing(PrimitiveSceneInfo) &&
				!PrimitiveTransforms[PrimitiveSceneInfo->PackedIndex].Equals(LocalToWorld, SMALL_NUMBER));

			// Update the primitive transform.
			PrimitiveSceneProxy->SetTransform(LocalToWorld, WorldBounds, LocalBounds, AttachmentRootPosition);
			PrimitiveTransforms[PrimitiveSceneInfo->PackedIndex] = LocalToWorld;

			if (!RHISupportsVolumeTextures(GetFeatureLevel())
				&& (PrimitiveSceneProxy->IsMovable() || PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting() || PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
			{
				PrimitiveSceneInfo->MarkIndirectLightingCacheBufferDirty();
			}

			GPUScene.AddPrimitiveToUpdate(PrimitiveSceneInfo->PackedIndex, EPrimitiveDirtyState::ChangedTransform);

			DistanceFieldSceneData.UpdatePrimitive(PrimitiveSceneInfo);
			LumenUpdatePrimitive(PrimitiveSceneInfo);
		#if RHI_RAYTRACING
			PrimitiveSceneInfo->UpdateCachedRayTracingInstanceWorldBounds(LocalToWorld);
		#endif

			// If the primitive has static mesh elements, it should have returned true from ShouldRecreateProxyOnUpdateTransform!
			check(!(bUpdateStaticDrawLists && PrimitiveSceneInfo->StaticMeshes.Num()));
		}
#if RHI_RAYTRACING
		{
			UpdateRayTracingGroupBounds_UpdatePrimitives(UpdatedTransforms);
		}
#endif

		for (const auto& Transform : OverridenPreviousTransforms)
		{
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Transform.Key;
			VelocityData.OverridePreviousTransform(PrimitiveSceneInfo->PrimitiveComponentId, Transform.Value);
		}
	}

	// handle scene changes
	for (FVirtualShadowMapArrayCacheManager* CacheManager : VirtualShadowCacheManagers)
	{
		CacheManager->OnSceneChange();
	}

#if RHI_RAYTRACING
	TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator> RayTracingPrimitivesToUpdate;
	RayTracingPrimitivesToUpdate.Reserve(UpdatedInstances.Num());
	bool bUpdateCachedRayTracingInstances = false;
#endif

	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UpdatePrimitiveInstances);
		SCOPED_NAMED_EVENT(FScene_UpdatePrimitiveInstances, FColor::Emerald);
		SCOPE_CYCLE_COUNTER(STAT_UpdatePrimitiveInstanceRenderThreadTime);

		for (const auto& UpdateInstance : UpdatedInstances)
		{
			FPrimitiveSceneProxy* PrimitiveSceneProxy = UpdateInstance.Key;
			if (DeletedSceneInfos.Contains(PrimitiveSceneProxy->GetPrimitiveSceneInfo()))
			{
				continue;
			}

			check(PrimitiveSceneProxy->GetPrimitiveSceneInfo()->PackedIndex != INDEX_NONE);

			FScopeCycleCounter Context(PrimitiveSceneProxy->GetStatId());
			FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
			
			QueueSceneInfoToFlushVirtualTexture(PrimitiveSceneInfo);

			// If we recorded no adds or removes the instance count has stayed the same.  Therefore the cached mesh draw commands do not
			// need to be updated.  In situations where the instance count changes the mesh draw command stores the instance count which
			// would need to be updated.
			const bool bInstanceCountChanged = (UpdateInstance.Value.CmdBuffer.NumAdds > 0) || (UpdateInstance.Value.CmdBuffer.NumRemoves > 0);

			const bool bUpdateStaticDrawLists = !PrimitiveSceneProxy->StaticElementsAlwaysUseProxyPrimitiveUniformBuffer() || bInstanceCountChanged;

			if (!PrimitiveSceneInfo->bPendingAddToScene)
			{
				PrimitiveSceneInfo->bPendingAddToScene = true;

				if (bUpdateStaticDrawLists)
				{
					SceneInfosWithStaticDrawListUpdate.Push(PrimitiveSceneInfo);
				}
				else
				{
#if RHI_RAYTRACING
					RayTracingPrimitivesToUpdate.Add(PrimitiveSceneInfo);
					bUpdateCachedRayTracingInstances = true;
#endif
					SceneInfosWithoutStaticDrawListUpdate.Push(PrimitiveSceneInfo);
				}

				PrimitiveSceneInfo->RemoveFromScene(bUpdateStaticDrawLists);
			}

			// Update the Proxy's data.
			PrimitiveSceneProxy->UpdateInstances_RenderThread(UpdateInstance.Value.CmdBuffer, UpdateInstance.Value.WorldBounds, UpdateInstance.Value.LocalBounds, UpdateInstance.Value.StaticMeshBounds);

			if (!RHISupportsVolumeTextures(GetFeatureLevel())
				&& (PrimitiveSceneProxy->IsMovable() || PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting() || PrimitiveSceneProxy->GetLightmapType() == ELightmapType::ForceVolumetric))
			{
				PrimitiveSceneInfo->MarkIndirectLightingCacheBufferDirty();
			}

			if (UpdateInstance.Value.CmdBuffer.NumAdds > 0 || UpdateInstance.Value.CmdBuffer.NumRemoves > 0)
			{
				PrimitiveSceneInfo->FreeGPUSceneInstances();
				FPrimitiveSceneInfo::AllocateGPUSceneInstances(this, MakeArrayView(&PrimitiveSceneInfo, 1));

				DistanceFieldSceneData.RemovePrimitive(PrimitiveSceneInfo);
				DistanceFieldSceneData.AddPrimitive(PrimitiveSceneInfo);

				LumenRemovePrimitive(PrimitiveSceneInfo, PrimitiveSceneInfo->GetIndex());
				LumenAddPrimitive(PrimitiveSceneInfo);
			}
			else
			{
				GPUScene.AddPrimitiveToUpdate(PrimitiveSceneInfo->PackedIndex, EPrimitiveDirtyState::ChangedAll);

				DistanceFieldSceneData.UpdatePrimitive(PrimitiveSceneInfo);
				LumenUpdatePrimitive(PrimitiveSceneInfo);
			}

			bNeedPathTracedInvalidation = bNeedPathTracedInvalidation || IsPrimitiveRelevantToPathTracing(PrimitiveSceneInfo);
		}

#if RHI_RAYTRACING
		{
			UpdateRayTracingGroupBounds_UpdatePrimitives(UpdatedInstances);
		}
#endif
	}

	if (SceneInfosWithoutStaticDrawListUpdate.Num() > 0)
	{
		const EPrimitiveAddToSceneOps AddToSceneOps = EnumHasAnyFlags(AsyncOps, EUpdateAllPrimitiveSceneInfosAsyncOps::CreateLightPrimitiveInteractions)
			? EPrimitiveAddToSceneOps::None
			: EPrimitiveAddToSceneOps::CreateLightPrimitiveInteractions;

		FPrimitiveSceneInfo::AddToScene(this, SceneInfosWithoutStaticDrawListUpdate, AddToSceneOps);

#if RHI_RAYTRACING
		if (bUpdateCachedRayTracingInstances)
		{
			FPrimitiveSceneInfo::UpdateCachedRayTracingInstances(this, RayTracingPrimitivesToUpdate);
		}
#endif
	}

	// Re-add the primitive to the scene with the new transform.
	if (SceneInfosWithStaticDrawListUpdate.Num() > 0)
	{
		EPrimitiveAddToSceneOps AddToSceneOps = EPrimitiveAddToSceneOps::All;

		if (EnumHasAnyFlags(AsyncOps, EUpdateAllPrimitiveSceneInfosAsyncOps::CreateLightPrimitiveInteractions))
		{
			EnumRemoveFlags(AddToSceneOps, EPrimitiveAddToSceneOps::CreateLightPrimitiveInteractions);
		}

		if (EnumHasAnyFlags(AsyncOps, EUpdateAllPrimitiveSceneInfosAsyncOps::CacheMeshDrawCommands))
		{
			EnumRemoveFlags(AddToSceneOps, EPrimitiveAddToSceneOps::CacheMeshDrawCommands);
		}

		FPrimitiveSceneInfo::AddToScene(this, SceneInfosWithStaticDrawListUpdate, AddToSceneOps);

		if (EnumHasAnyFlags(AsyncOps, EUpdateAllPrimitiveSceneInfosAsyncOps::CacheMeshDrawCommands))
		{
			CacheMeshDrawCommandsTask = GraphBuilder.AddSetupTask([this, &SceneInfosWithStaticDrawListUpdate] () mutable
			{
				SCOPED_NAMED_EVENT(AsyncCacheMeshDrawCommands, FColor::Emerald);
				FPrimitiveSceneInfo::CacheMeshDrawCommands(this, SceneInfosWithStaticDrawListUpdate);
				FPrimitiveSceneInfo::CacheNaniteDrawCommands(this, SceneInfosWithStaticDrawListUpdate);
			#if RHI_RAYTRACING
				FPrimitiveSceneInfo::CacheRayTracingPrimitives(this, SceneInfosWithStaticDrawListUpdate);
			#endif
			});
		}
	}

	if (EnumHasAnyFlags(AsyncOps, EUpdateAllPrimitiveSceneInfosAsyncOps::CreateLightPrimitiveInteractions))
	{
		CreateLightPrimitiveInteractionsTask = GraphBuilder.AddSetupTask(
			[this, &SceneInfosWithStaticDrawListUpdate, SceneInfosWithoutStaticDrawListUpdate = MoveTemp(SceneInfosWithoutStaticDrawListUpdate)]
		{
			SCOPED_NAMED_EVENT(AsyncCreateLightPrimitiveInteractions, FColor::Emerald);

			for (FPrimitiveSceneInfo* SceneInfo : SceneInfosWithStaticDrawListUpdate)
			{
				CreateLightPrimitiveInteractionsForPrimitive(SceneInfo);
			}

			for (FPrimitiveSceneInfo* SceneInfo : SceneInfosWithoutStaticDrawListUpdate)
			{
				CreateLightPrimitiveInteractionsForPrimitive(SceneInfo);
			}
		});
	}

	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : AddedPrimitiveSceneInfos)
	{
		// Set LOD parent information if valid
		PrimitiveSceneInfo->LinkLODParentComponent();

		// Update scene LOD tree
		SceneLODHierarchy.UpdateNodeSceneInfo(PrimitiveSceneInfo->PrimitiveComponentId, PrimitiveSceneInfo);
	}

	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : SceneInfosToFlushVirtualTexture)
	{
		PrimitiveSceneInfo->FlushRuntimeVirtualTexture();
		PrimitiveSceneInfo->bPendingFlushVirtualTexture = false;
	}

	for (const auto& Attachments : UpdatedAttachmentRoots)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Attachments.Key;
		if (DeletedSceneInfos.Contains(PrimitiveSceneInfo))
		{
			continue;
		}

		PrimitiveSceneInfo->UnlinkAttachmentGroup();
		PrimitiveSceneInfo->LightingAttachmentRoot = Attachments.Value;
		PrimitiveSceneInfo->LinkAttachmentGroup();
	}

	for (const auto& CustomParams : UpdatedCustomPrimitiveParams)
	{
		FPrimitiveSceneProxy* PrimitiveSceneProxy = CustomParams.Key;
		if (DeletedSceneInfos.Contains(PrimitiveSceneProxy->GetPrimitiveSceneInfo()))
		{
			continue;
		}

		FScopeCycleCounter Context(PrimitiveSceneProxy->GetStatId());
		PrimitiveSceneProxy->CustomPrimitiveData = CustomParams.Value;

		// Ensure an update of primitive data before rendering
		PrimitiveSceneProxy->GetPrimitiveSceneInfo()->MarkGPUStateDirty(EPrimitiveDirtyState::ChangedOther);
	}

	for (FPrimitiveSceneInfo* PrimitiveSceneInfo : DistanceFieldSceneDataUpdates)
	{
		if (DeletedSceneInfos.Contains(PrimitiveSceneInfo))
		{
			continue;
		}

		DistanceFieldSceneData.UpdatePrimitive(PrimitiveSceneInfo);
	}

	for (const auto& OccSlackDelta : UpdatedOcclusionBoundsSlacks)
	{
		const FPrimitiveSceneProxy* SceneProxy = OccSlackDelta.Key;
		const FPrimitiveSceneInfo* SceneInfo = SceneProxy->GetPrimitiveSceneInfo();

		if (DeletedSceneInfos.Contains(SceneInfo))
		{
			continue;
		}

		FBoxSphereBounds NewOccBounds;
		if (SceneProxy->HasCustomOcclusionBounds())
		{
			NewOccBounds = SceneProxy->GetCustomOcclusionBounds();
		}
		else
		{
			NewOccBounds = SceneProxy->GetBounds();
		}

		PrimitiveOcclusionBounds[SceneInfo->PackedIndex] = NewOccBounds.ExpandBy(OCCLUSION_SLOP + OccSlackDelta.Value);
	}

	{
		SCOPED_NAMED_EVENT(FScene_DeletePrimitiveSceneInfo, FColor::Red);
		for (FPrimitiveSceneInfo* PrimitiveSceneInfo : DeletedSceneInfos)
		{
			bNeedPathTracedInvalidation = bNeedPathTracedInvalidation || IsPrimitiveRelevantToPathTracing(PrimitiveSceneInfo);

			// It is possible that the HitProxies list isn't empty if PrimitiveSceneInfo was Added/Removed in same frame
			// Delete the PrimitiveSceneInfo on the game thread after the rendering thread has processed its removal.
			// This must be done on the game thread because the hit proxy references (and possibly other members) need to be freed on the game thread.
			struct DeferDeleteHitProxies : FDeferredCleanupInterface
			{
				DeferDeleteHitProxies(TArray<TRefCountPtr<HHitProxy>>&& InHitProxies) : HitProxies(MoveTemp(InHitProxies)) {}
				TArray<TRefCountPtr<HHitProxy>> HitProxies;
			};

			BeginCleanup(new DeferDeleteHitProxies(MoveTemp(PrimitiveSceneInfo->HitProxies)));
			// free the primitive scene proxy.
			delete PrimitiveSceneInfo->Proxy;
			delete PrimitiveSceneInfo;
		}
	}
	if (bNeedPathTracedInvalidation)
	{
		InvalidatePathTracedOutput();
	}
	UpdatedAttachmentRoots.Empty();
	UpdatedTransforms.Empty();
	UpdatedInstances.Empty();
	UpdatedCustomPrimitiveParams.Empty();
	OverridenPreviousTransforms.Empty();
	UpdatedOcclusionBoundsSlacks.Empty();
	DistanceFieldSceneDataUpdates.Empty();
	AddedPrimitiveSceneInfos.Empty();

#if DO_GUARD_SLOW
	for (const FPrimitiveSceneInfo* PrimitiveSceneInfo : Primitives)
	{
		checkSlow(PrimitiveSceneInfo->PackedIndex != INDEX_NONE);
		checkSlow(PrimitiveSceneInfo->PackedIndex < Primitives.Num());
		checkSlow(PrimitiveSceneInfo->PersistentIndex.Index != INDEX_NONE);
		checkSlow(PersistentPrimitiveIdToIndexMap[PrimitiveSceneInfo->PersistentIndex.Index] == PrimitiveSceneInfo->PackedIndex);
	}
#endif
}

void FScene::CreateLightPrimitiveInteractionsForPrimitive(FPrimitiveSceneInfo* PrimitiveInfo)
{
	FPrimitiveSceneProxy* Proxy = PrimitiveInfo->Proxy;
	if (Proxy->GetLightingChannelMask() != 0)
	{
		const FBoxSphereBounds& Bounds = Proxy->GetBounds();
		const FPrimitiveSceneInfoCompact PrimitiveSceneInfoCompact(PrimitiveInfo);

		// Find local lights that affect the primitive in the light octree.
		LocalShadowCastingLightOctree.FindElementsWithBoundsTest(Bounds.GetBox(), [&PrimitiveSceneInfoCompact](const FLightSceneInfoCompact& LightSceneInfoCompact)
		{
			LightSceneInfoCompact.LightSceneInfo->CreateLightPrimitiveInteraction(LightSceneInfoCompact, PrimitiveSceneInfoCompact);
		});

		// Also loop through non-local (directional) shadow-casting lights
		for (int32 LightID : DirectionalShadowCastingLightIDs)
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = Lights[LightID];
			LightSceneInfoCompact.LightSceneInfo->CreateLightPrimitiveInteraction(LightSceneInfoCompact, PrimitiveSceneInfoCompact);
		}
	}
}

bool FScene::IsPrimitiveBeingRemoved(FPrimitiveSceneInfo* PrimitiveSceneInfo) const
{
	check(IsInParallelRenderingThread() || IsInRenderingThread());
	return RemovedPrimitiveSceneInfos.Find(PrimitiveSceneInfo) != nullptr;
}

bool FScene::ShouldRenderSkylightInBasePass(bool bIsTranslucent) const
{
	if (IsMobilePlatform(GetShaderPlatform()))
	{
		bool bRenderSkyLight = SkyLight && !SkyLight->bHasStaticLighting;
		const bool bIsForwardShading = bIsTranslucent || !IsMobileDeferredShadingEnabled(GetShaderPlatform());

		if (bIsForwardShading)
		{
			// Both stationary and movable skylights are applied in base pass for forward shading
			bRenderSkyLight = bRenderSkyLight && (ReadOnlyCVARCache.bEnableStationarySkylight || !SkyLight->bWantsStaticShadowing);
		}
		else
		{
			// Only stationary skylights are applied in base pass for deferred
			bRenderSkyLight = bRenderSkyLight && (ReadOnlyCVARCache.bEnableStationarySkylight && SkyLight->bWantsStaticShadowing);
		}

		return bRenderSkyLight;
	}
	else
	{
		bool bRenderSkyLight = SkyLight && !SkyLight->bHasStaticLighting && !(ShouldRenderRayTracingSkyLight(SkyLight) && !IsForwardShadingEnabled(GetShaderPlatform()));

		if (bIsTranslucent)
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
						&& IsForwardShadingEnabled(GetShaderPlatform())));
		}

		return bRenderSkyLight;
	}
}

/**
 * Dummy NULL scene interface used by dedicated servers.
 */
class FNULLSceneInterface : public FSceneInterface
{
public:
	FNULLSceneInterface(UWorld* InWorld, bool bCreateFXSystem )
		:	FSceneInterface(GMaxRHIFeatureLevel)
		,	World( InWorld )
		,	FXSystem( NULL )
	{
		World->Scene = this;

		if (bCreateFXSystem)
		{
			World->CreateFXSystem();
		}
		else
		{
			World->FXSystem = NULL;
			SetFXSystem(NULL);
		}
	}

	virtual void AddPrimitive(UPrimitiveComponent* Primitive) override {}
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) override {}
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) override {}
	virtual void UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, EUpdateAllPrimitiveSceneInfosAsyncOps = EUpdateAllPrimitiveSceneInfosAsyncOps::None) override {}
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimiteIndex) override { return nullptr; }
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(const FPersistentPrimitiveIndex& PersistentPrimitiveIndex) override { return nullptr; }

	/** Updates the transform of a primitive which has already been added to the scene. */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) override {}
	virtual void UpdatePrimitiveInstances(UInstancedStaticMeshComponent* Primitive) override {}
	virtual void UpdatePrimitiveOcclusionBoundsSlack(UPrimitiveComponent* Primitive, float NewSlack) override {}
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) override {}
	virtual void UpdateCustomPrimitiveData(UPrimitiveComponent* Primitive) override {}

	virtual void AddLight(ULightComponent* Light) override {}
	virtual void RemoveLight(ULightComponent* Light) override {}
	virtual void AddInvisibleLight(ULightComponent* Light) override {}
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) override {}
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) override {}
	virtual bool HasSkyLightRequiringLightingBuild() const { return false; }
	virtual bool HasAtmosphereLightRequiringLightingBuild() const { return false; }

	virtual void AddDecal(UDecalComponent*) override {}
	virtual void RemoveDecal(UDecalComponent*) override {}
	virtual void UpdateDecalTransform(UDecalComponent* Decal) override {}
	virtual void UpdateDecalFadeOutTime(UDecalComponent* Decal) override {};
	virtual void UpdateDecalFadeInTime(UDecalComponent* Decal) override {};
	virtual void BatchUpdateDecals(TArray<FDeferredDecalUpdateParams>&& UpdateParams) override {}

	/** Updates the transform of a light which has already been added to the scene. */
	virtual void UpdateLightTransform(ULightComponent* Light) override {}
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) override {}

	virtual void AddExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) override {}
	virtual void RemoveExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) override {}
	virtual bool HasAnyExponentialHeightFog() const override { return false; }

	virtual void AddSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy, bool bStaticLightingBuilt) override {}
	virtual void RemoveSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy) override {}
	virtual FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() override { return NULL; }
	virtual const FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() const override { return NULL; }

	virtual void AddSparseVolumeTextureViewer(FSparseVolumeTextureViewerSceneProxy* SVTV) override {}
	virtual void RemoveSparseVolumeTextureViewer(FSparseVolumeTextureViewerSceneProxy* SVTV) override {}

	virtual void AddHairStrands(FHairStrandsInstance* Proxy) override {}
	virtual void RemoveHairStrands(FHairStrandsInstance* Proxy) override {}
	virtual void GetLightIESAtlasSlot(const FLightSceneProxy* Proxy, FLightRenderParameters* Out) override {}
	virtual void GetRectLightAtlasSlot(const FRectLightSceneProxy* Proxy, FLightRenderParameters* Out) override {}

	virtual void SetPhysicsField(FPhysicsFieldSceneProxy* PhysicsFieldSceneProxy) override {}
	virtual void ResetPhysicsField() override {}
	virtual void ShowPhysicsField() override {}
	virtual void UpdatePhysicsField(FRDGBuilder& GraphBuilder, FViewInfo& View) override {}

	virtual void AddVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) override {}
	virtual void RemoveVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) override {}
	virtual FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() override { return NULL; }
	virtual const FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() const override { return NULL; }

	virtual void AddWindSource(class UWindDirectionalSourceComponent* WindComponent) override {}
	virtual void RemoveWindSource(class UWindDirectionalSourceComponent* WindComponent) override {}
	virtual void UpdateWindSource(class UWindDirectionalSourceComponent* WindComponent) override {}
	virtual const TArray<class FWindSourceSceneProxy*>& GetWindSources_RenderThread() const override
	{
		static TArray<class FWindSourceSceneProxy*> NullWindSources;
		return NullWindSources;
	}
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override { OutDirection = FVector(1.0f, 0.0f, 0.0f); OutSpeed = 0.0f; OutMinGustAmt = 0.0f; OutMaxGustAmt = 0.0f; }
	virtual void GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override { OutDirection = FVector(1.0f, 0.0f, 0.0f); OutSpeed = 0.0f; OutMinGustAmt = 0.0f; OutMaxGustAmt = 0.0f; }
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const override { OutDirection = FVector(1.0f, 0.0f, 0.0f); OutSpeed = 0.0f; OutMinGustAmt = 0.0f; OutMaxGustAmt = 0.0f; }
	virtual void AddSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) override {}
	virtual void RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) override {}
	virtual void UpdateSpeedTreeWind(double CurrentTime) override {}
	virtual FRHIUniformBuffer* GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) const override { return nullptr; }

	virtual void Release() override {}

	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const override {}

	/**
	 * @return		true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const override
	{
		return false;
	}

	// Accessors.
	virtual class UWorld* GetWorld() const override
	{
		return World;
	}

	/**
	* Return the scene to be used for rendering
	*/
	virtual class FScene* GetRenderScene() override
	{
		return NULL;
	}

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) override
	{
		FXSystem = InFXSystem;
	}

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() override
	{
		return FXSystem;
	}

	virtual bool HasAnyLights() const override { return false; }

private:
	UWorld* World;
	class FFXSystemInterface* FXSystem;
};

FSceneInterface* FRendererModule::AllocateScene(UWorld* World, bool bInRequiresHitProxies, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel)
{
	LLM_SCOPE(ELLMTag::SceneRender);
	check(IsInGameThread());

	// Create a full fledged scene if we have something to render.
	if (GIsClient && FApp::CanEverRender() && !GUsingNullRHI)
	{
		FScene* NewScene = new FScene(World, bInRequiresHitProxies, GIsEditor && (!World || !World->IsGameWorld()), bCreateFXSystem, InFeatureLevel);
		AllocatedScenes.Add(NewScene);
		return NewScene;
	}
	// And fall back to a dummy/ NULL implementation for commandlets and dedicated server.
	else
	{
		return new FNULLSceneInterface(World, bCreateFXSystem);
	}
}

void FRendererModule::RemoveScene(FSceneInterface* Scene)
{
	check(IsInGameThread());
	AllocatedScenes.Remove(Scene);
}

void FRendererModule::UpdateStaticDrawLists()
{
	// Update all static meshes in order to recache cached mesh draw commands.
	check(IsInGameThread()); // AllocatedScenes is managed by the game thread
	for (FSceneInterface* Scene : AllocatedScenes)
	{
		Scene->UpdateStaticDrawLists();
	}
}

void UpdateStaticMeshesForMaterials(const TArray<const FMaterial*>& MaterialResourcesToUpdate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateStaticMeshesForMaterials);

	TArray<UMaterialInterface*> UsedMaterials;
	TSet<UMaterialInterface*> UsedMaterialsDependencies;
	TMap<FScene*, TArray<FPrimitiveSceneInfo*>> UsedPrimitives;
	for (TObjectIterator<UPrimitiveComponent> PrimitiveIt; PrimitiveIt; ++PrimitiveIt)
	{
		UPrimitiveComponent* PrimitiveComponent = *PrimitiveIt;

		if (PrimitiveComponent->IsRenderStateCreated() && PrimitiveComponent->SceneProxy)
		{
			UsedMaterialsDependencies.Reset();
			UsedMaterials.Reset();

			// Note: relying on GetUsedMaterials to be accurate, or else we won't propagate to the right primitives and the renderer will crash later
			// FPrimitiveSceneProxy::VerifyUsedMaterial is used to make sure that all materials used for rendering are reported in GetUsedMaterials
			PrimitiveComponent->GetUsedMaterials(UsedMaterials);

			for (UMaterialInterface* UsedMaterial : UsedMaterials)
			{
				if (UsedMaterial)
				{
					UsedMaterial->GetDependencies(UsedMaterialsDependencies);
				}
			}

			if (UsedMaterialsDependencies.Num() > 0)
			{
				for (const FMaterial* MaterialResourceToUpdate : MaterialResourcesToUpdate)
				{
					UMaterialInterface* UpdatedMaterialInterface = MaterialResourceToUpdate->GetMaterialInterface();

					if (UpdatedMaterialInterface)
					{
						if (UsedMaterialsDependencies.Contains(UpdatedMaterialInterface))
						{
							FPrimitiveSceneProxy* SceneProxy = PrimitiveComponent->SceneProxy;
							FPrimitiveSceneInfo* SceneInfo = SceneProxy->GetPrimitiveSceneInfo();
							FScene* Scene = SceneInfo->Scene;
							TArray<FPrimitiveSceneInfo*>& SceneInfos = UsedPrimitives.FindOrAdd(Scene);
							SceneInfos.Add(SceneInfo);
							break;
						}
					}
				}
			}
		}
	}
	ENQUEUE_RENDER_COMMAND(FUpdateStaticMeshesForMaterials)(
		[UsedPrimitives = MoveTemp(UsedPrimitives)](FRHICommandListImmediate& RHICmdList) mutable
		{
			// Defer the caching until the next render tick, to make sure that all render components queued
			// for re-creation are processed. Otherwise, we may end up caching mesh commands from stale data.
			for (auto& SceneInfos: UsedPrimitives)
			{
				SceneInfos.Key->UpdateAllPrimitiveSceneInfos(RHICmdList);
			}
			for (auto& SceneInfos : UsedPrimitives)
			{
				TArray<FPrimitiveSceneInfo*>& SceneInfoArray = SceneInfos.Value;
				FPrimitiveSceneInfo::UpdateStaticMeshes(SceneInfos.Key, SceneInfoArray, EUpdateStaticMeshFlags::AllCommands, false);
			}
		});
}

void FRendererModule::UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials)
{
	// Update static meshes for a given set of materials in order to recache cached mesh draw commands.
	UpdateStaticMeshesForMaterials(Materials);
}

FSceneViewStateInterface* FRendererModule::AllocateViewState(ERHIFeatureLevel::Type FeatureLevel)
{
	return new FSceneViewState(FeatureLevel, nullptr);
}

FSceneViewStateInterface* FRendererModule::AllocateViewState(ERHIFeatureLevel::Type FeatureLevel, FSceneViewStateInterface* ShareOriginTarget)
{
	return new FSceneViewState(FeatureLevel, (FSceneViewState*)ShareOriginTarget);
}

void FRendererModule::InvalidatePathTracedOutput()
{
	// AllocatedScenes is managed by the game thread

	// #jira UE-130700:
	// Because material updates call this function and could happen in parallel, we also allow the parallel game thread here.
	// We assume that no changes will be made to AllocatedScene during this time, otherwise locking would need to
	// be introduced (which could have performance implications). 

	check(IsInGameThread() || IsInParallelGameThread());
	for (FSceneInterface* Scene : AllocatedScenes)
	{
		Scene->InvalidatePathTracedOutput();
	}
}

uint32 FScene::GetFrameNumber() const
{
	if (IsInGameThread())
	{
		return SceneFrameNumber;
	}
	else
	{
		return SceneFrameNumberRenderThread;
	}
}

void FScene::IncrementFrameNumber()
{
	// Increment game-tread version
	++SceneFrameNumber;
	ENQUEUE_RENDER_COMMAND(SceneStartFrame)([this,NewNumber = SceneFrameNumber](FRHICommandListImmediate& RHICmdList)
	{
		SceneFrameNumberRenderThread = NewNumber;
	});
}