// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeterogeneousVolumes.h"

#include "PixelShaderUtils.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneManagement.h"

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumes(
	TEXT("r.HeterogeneousVolumes"),
	0,
	TEXT("Enables the Heterogeneous volume integrator (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesDebug(
	TEXT("r.HeterogeneousVolumes.Debug"),
	0,
	TEXT("Creates auxillary output buffers for debugging (Default = 0)"),
	ECVF_RenderThreadSafe
);

#if 0
// Currently disabled for 5.1
static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesHardwareRayTracing(
	TEXT("r.HeterogeneousVolumes.HardwareRayTracing"),
	0,
	TEXT("Enables hardware ray tracing acceleration (Default = 0)"),
	ECVF_RenderThreadSafe
);
#endif

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesIndirectLighting(
	TEXT("r.HeterogeneousVolumes.IndirectLighting"),
	1,
	TEXT("Enables indirect lighting (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesJitter(
	TEXT("r.HeterogeneousVolumes.Jitter"),
	1,
	TEXT("Enables jitter when ray marching (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesMaxStepCount(
	TEXT("r.HeterogeneousVolumes.MaxStepCount"),
	128,
	TEXT("The maximum ray-marching step count (Default = 128)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesMaxTraceDistance(
	TEXT("r.HeterogeneousVolumes.MaxTraceDistance"),
	10000.0,
	TEXT("The maximum trace view-distance for direct volume rendering (Default = 10000)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesMaxShadowTraceDistance(
	TEXT("r.HeterogeneousVolumes.MaxShadowTraceDistance"),
	10000.0,
	TEXT("The maximum shadow-trace distance (Default = 10000)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshading(
	TEXT("r.HeterogeneousVolumes.Preshading"),
	0,
	TEXT("Evaluates the material into a canonical preshaded volume before rendering the result (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingMipLevel(
	TEXT("r.HeterogeneousVolumes.Preshading.MipLevel"),
	0,
	TEXT("Statically determines the MIP-level when evaluating preshaded volume data (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingVolumeResolutionX(
	TEXT("r.HeterogeneousVolumes.Preshading.VolumeResolution.X"),
	256,
	TEXT("Determines the preshading volume resolution in X (Default = 256)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingVolumeResolutionY(
	TEXT("r.HeterogeneousVolumes.Preshading.VolumeResolution.Y"),
	256,
	TEXT("Determines the preshading volume resolution in Y (Default = 256)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingVolumeResolutionZ(
	TEXT("r.HeterogeneousVolumes.Preshading.VolumeResolution.Z"),
	256,
	TEXT("Determines the preshading volume resolution in Z (Default = 256)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesShadowStepSize(
	TEXT("r.HeterogeneousVolumes.ShadowStepSize"),
	8.0,
	TEXT("The ray-marching step-size for shadow rays (Default = 8.0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesSparseVoxel(
	TEXT("r.HeterogeneousVolumes.SparseVoxel"),
	0,
	TEXT("Uses sparse-voxel rendering algorithms (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesSparseVoxelGenerationMipBias(
	TEXT("r.HeterogeneousVolumes.SparseVoxel.GenerationMipBias"),
	3,
	TEXT("Determines MIP bias for sparse voxel generation (Default = 3)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesSparseVoxelPerTileCulling(
	TEXT("r.HeterogeneousVolumes.SparseVoxel.PerTileCulling"),
	1,
	TEXT("Enables sparse-voxel culling when using tiled rendering (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesSparseVoxelRefinement(
	TEXT("r.HeterogeneousVolumes.SparseVoxel.Refinement"),
	1,
	TEXT("Uses hierarchical refinement to coalesce neighboring sparse-voxels (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesStepSize(
	TEXT("r.HeterogeneousVolumes.StepSize"),
	1.0,
	TEXT("The ray-marching step-size (Default = 1.0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesLightingCache(
	TEXT("r.HeterogeneousVolumes.LightingCache"),
	2,
	TEXT("Enables an optimized pre-pass, caching certain volumetric rendering lighting quantities (Default = 2)\n")
	TEXT("0: Disabled\n")
	TEXT("1: Cache transmittance\n")
	TEXT("2: Cache in-scattering\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesLightingCacheDownsampleFactor(
	TEXT("r.HeterogeneousVolumes.LightingCache.DownsampleFactor"),
	1,
	TEXT("Determines the downsample factor, relative to the preshading volume resolution (Default = 1)"),
	ECVF_RenderThreadSafe
);

DECLARE_GPU_STAT_NAMED(HeterogeneousVolumesStat, TEXT("HeterogeneousVolumes"));

static bool IsHeterogeneousVolumesEnabled()
{
	return CVarHeterogeneousVolumes.GetValueOnRenderThread() != 0;
}

bool ShouldRenderHeterogeneousVolumes(
	const FScene* Scene
)
{
	return IsHeterogeneousVolumesEnabled()
		&& Scene != nullptr;
}

bool ShouldRenderHeterogeneousVolumesForView(
	const FSceneView& View
)
{
	return IsHeterogeneousVolumesEnabled()
		&& View.Family
		&& !View.bIsPlanarReflection
		&& !View.bIsSceneCapture
		&& !View.bIsReflectionCapture
		&& View.State;
}

bool DoesPlatformSupportHeterogeneousVolumes(EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5)
		// TODO:
		// && FDataDrivenShaderPlatformInfo::GetSupportsHeterogeneousVolumes(Platform)
		&& !IsForwardShadingEnabled(Platform);
}

namespace HeterogeneousVolumes
{
	// CVars

	FIntVector GetVolumeResolution()
	{
		FIntVector VolumeResolution;
		VolumeResolution.X = FMath::Clamp(CVarHeterogeneousVolumesPreshadingVolumeResolutionX.GetValueOnRenderThread(), 64, 1024);
		VolumeResolution.Y = FMath::Clamp(CVarHeterogeneousVolumesPreshadingVolumeResolutionY.GetValueOnRenderThread(), 64, 1024);
		VolumeResolution.Z = FMath::Clamp(CVarHeterogeneousVolumesPreshadingVolumeResolutionZ.GetValueOnRenderThread(), 64, 1024);
		return VolumeResolution;
	}

	float GetShadowStepSize()
	{
		return CVarHeterogeneousVolumesShadowStepSize.GetValueOnRenderThread();
	}

	float GetMaxTraceDistance()
	{
		return CVarHeterogeneousVolumesMaxTraceDistance.GetValueOnRenderThread();
	}

	float GetMaxShadowTraceDistance()
	{
		return CVarHeterogeneousVolumesMaxShadowTraceDistance.GetValueOnRenderThread();
	}

	float GetStepSize()
	{
		return CVarHeterogeneousVolumesStepSize.GetValueOnRenderThread();
	}

	float GetMaxStepCount()
	{
		return CVarHeterogeneousVolumesMaxStepCount.GetValueOnRenderThread();
	}

	int32 GetMipLevel()
	{
		return CVarHeterogeneousVolumesPreshadingMipLevel.GetValueOnRenderThread();
	}

	uint32 GetSparseVoxelMipBias()
	{
		// TODO: Clamp based on texture dimension..
		return FMath::Clamp(CVarHeterogeneousVolumesSparseVoxelGenerationMipBias.GetValueOnRenderThread(), 0, 10);
	}

	int32 GetDebugMode()
	{
		return CVarHeterogeneousVolumesDebug.GetValueOnRenderThread();
	}

	bool UseSparseVoxelPipeline()
	{
		return CVarHeterogeneousVolumesSparseVoxel.GetValueOnRenderThread() != 0;
	}

	bool ShouldRefineSparseVoxels()
	{
		return CVarHeterogeneousVolumesSparseVoxelRefinement.GetValueOnRenderThread() != 0;
	}

	bool UseSparseVoxelPerTileCulling()
	{
		return CVarHeterogeneousVolumesSparseVoxelPerTileCulling.GetValueOnRenderThread() != 0;
	}

	int32 GetLightingCacheMode()
	{
		return CVarHeterogeneousVolumesLightingCache.GetValueOnRenderThread();
	}

	bool UseLightingCacheForInscattering()
	{
		return CVarHeterogeneousVolumesLightingCache.GetValueOnRenderThread() == 2;
	}

	bool UseLightingCacheForTransmittance()
	{
		return CVarHeterogeneousVolumesLightingCache.GetValueOnRenderThread() == 1;
	}

	bool ShouldJitter()
	{
		return CVarHeterogeneousVolumesJitter.GetValueOnRenderThread() != 0;
	}

	bool UseHardwareRayTracing()
	{
#if 0
		// Currently disabled for 5.1
		return IsRayTracingEnabled()
			&& (CVarHeterogeneousVolumesHardwareRayTracing.GetValueOnRenderThread() != 0);
#else
		return false;
#endif
	}

	bool UseIndirectLighting()
	{
		return CVarHeterogeneousVolumesIndirectLighting.GetValueOnRenderThread() != 0;
	}

	// Convenience Utils

	int GetVoxelCount(FIntVector VolumeResolution)
	{
		return VolumeResolution.X * VolumeResolution.Y * VolumeResolution.Z;
	}

	int GetVoxelCount(FRDGTextureDesc TextureDesc)
	{
		return TextureDesc.Extent.X * TextureDesc.Extent.Y * TextureDesc.Depth;
	}

	FIntVector GetMipVolumeResolution(FIntVector VolumeResolution, uint32 MipLevel)
	{
		return FIntVector(
			FMath::Max(VolumeResolution.X >> MipLevel, 1),
			FMath::Max(VolumeResolution.Y >> MipLevel, 1),
			FMath::Max(VolumeResolution.Z >> MipLevel, 1)
		);
	}

	FIntVector GetLightingCacheResolution()
	{
		float DownsampleFactor = CVarHeterogeneousVolumesLightingCacheDownsampleFactor.GetValueOnRenderThread();
		FIntVector LightingCacheResolution = GetVolumeResolution() / DownsampleFactor;

		return LightingCacheResolution;
	}
}

void FDeferredShadingSceneRenderer::RenderHeterogeneousVolumes(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures
)
{
	RDG_EVENT_SCOPE(GraphBuilder, "HeterogeneousVolumes");
	RDG_GPU_STAT_SCOPE(GraphBuilder, HeterogeneousVolumesStat);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		// Per-view??
		FRDGTextureDesc Desc = SceneTextures.Color.Target->Desc;
		Desc.Format = PF_FloatRGBA;
		Desc.Flags &= ~(TexCreate_FastVRAM);
		FRDGTextureRef HeterogeneousVolumeRadiance = GraphBuilder.CreateTexture(Desc, TEXT("HeterogeneousVolumes"));
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(HeterogeneousVolumeRadiance), FLinearColor::Transparent);

		if (ShouldRenderHeterogeneousVolumesForView(View))
		{
			for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.VolumetricMeshBatches.Num(); ++MeshBatchIndex)
			{
				const FMeshBatch* Mesh = View.VolumetricMeshBatches[MeshBatchIndex].Mesh;
				const FMaterialRenderProxy* MaterialRenderProxy = Mesh->MaterialRenderProxy;
				const FMaterial& Material = MaterialRenderProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialRenderProxy);
				// Only Niagara mesh particles bound to volume materials
				if (Material.GetMaterialDomain() != MD_Volume || !Material.IsUsedWithNiagaraMeshParticles())
				{
					continue;
				}

				const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.VolumetricMeshBatches[MeshBatchIndex].Proxy;
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
				const int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
				const FBoxSphereBounds LocalBoxSphereBounds = PrimitiveSceneProxy->GetLocalBounds();

				RDG_EVENT_SCOPE(GraphBuilder, "%s", *PrimitiveSceneProxy->GetResourceName().ToString());

				// Allocate transmittance volume
				// TODO: Allow option for scalar transmittance to conserve bandwidth
				FIntVector LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution();
				uint32 NumMips = FMath::Log2(float(FMath::Min(FMath::Min(LightingCacheResolution.X, LightingCacheResolution.Y), LightingCacheResolution.Z))) + 1;
				FRDGTextureDesc LightingCacheDesc = FRDGTextureDesc::Create3D(
					LightingCacheResolution,
					PF_FloatR11G11B10,
					FClearValueBinding::Black,
					TexCreate_ShaderResource | TexCreate_UAV | TexCreate_3DTiling,
					NumMips
				);
				FRDGTextureRef LightingCacheTexture = GraphBuilder.CreateTexture(LightingCacheDesc, TEXT("HeterogeneousVolumes.LightingCacheTexture"));
				AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(LightingCacheTexture), FLinearColor::Black);

				// Material baking executes a pre-shading pipeline
				if (CVarHeterogeneousVolumesPreshading.GetValueOnRenderThread())
				{
					RenderWithPreshading(
						GraphBuilder,
						SceneTextures,
						Scene,
						ViewFamily,
						View,
						// Shadow Data
						VisibleLightInfos,
						VirtualShadowMapArray,
						// Object data
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						PrimitiveId,
						LocalBoxSphereBounds,
						// Transmittance accleration
						LightingCacheTexture,
						// Output
						HeterogeneousVolumeRadiance
					);
				}
				// Otherwise execute a live-shading pipeline
				else
				{
					RenderWithLiveShading(
						GraphBuilder,
						SceneTextures,
						Scene,
						View,
						// Shadow Data
						VisibleLightInfos,
						VirtualShadowMapArray,
						// Object Data
						PrimitiveSceneProxy,
						MaterialRenderProxy,
						PrimitiveId,
						LocalBoxSphereBounds,
						// Transmittance accleration
						LightingCacheTexture,
						// Output
						HeterogeneousVolumeRadiance
					);
				}
			}
		}

		View.HeterogeneousVolumeRadiance = HeterogeneousVolumeRadiance;
	}
}

class FHeterogeneousVolumesCompositeCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHeterogeneousVolumesCompositeCS);
	SHADER_USE_PARAMETER_STRUCT(FHeterogeneousVolumesCompositeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Scene data
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)

		// Volume data
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, HeterogeneousVolumeRadiance)

		// Dispatch data
		SHADER_PARAMETER(FIntVector, GroupCount)

		// Output
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWColorTexture)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(
		const FGlobalShaderPermutationParameters& Parameters
	)
	{
		// Apply conditional project settings for Heterogeneous volumes?
		return DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(
		const FGlobalShaderPermutationParameters& Parameters,
		FShaderCompilerEnvironment& OutEnvironment
	)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_1D"), GetThreadGroupSize1D());
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE_2D"), GetThreadGroupSize2D());

		// This shader takes a very long time to compile with FXC, so we pre-compile it with DXC first and then forward the optimized HLSL to FXC.
		//OutEnvironment.CompilerFlags.Add(CFLAG_PrecompileWithDXC);
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		OutEnvironment.SetDefine(TEXT("GET_PRIMITIVE_DATA_OVERRIDE"), 1);
	}

	static int32 GetThreadGroupSize1D() { return GetThreadGroupSize2D() * GetThreadGroupSize2D(); }
	static int32 GetThreadGroupSize2D() { return 8; }
};

IMPLEMENT_GLOBAL_SHADER(FHeterogeneousVolumesCompositeCS, "/Engine/Private/HeterogeneousVolumes/HeterogeneousVolumesComposite.usf", "HeterogeneousVolumesCompositeCS", SF_Compute);


void FDeferredShadingSceneRenderer::CompositeHeterogeneousVolumes(
	FRDGBuilder& GraphBuilder,
	const FSceneTextures& SceneTextures
)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];

		if (ShouldRenderHeterogeneousVolumesForView(View))
		{
			uint32 GroupCountX = FMath::DivideAndRoundUp(View.ViewRect.Size().X, FHeterogeneousVolumesCompositeCS::GetThreadGroupSize2D());
			uint32 GroupCountY = FMath::DivideAndRoundUp(View.ViewRect.Size().Y, FHeterogeneousVolumesCompositeCS::GetThreadGroupSize2D());
			FIntVector GroupCount = FIntVector(GroupCountX, GroupCountY, 1);

			FHeterogeneousVolumesCompositeCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHeterogeneousVolumesCompositeCS::FParameters>();
			{
				// Scene data
				PassParameters->View = View.ViewUniformBuffer;
				// Volume data
				PassParameters->HeterogeneousVolumeRadiance = View.HeterogeneousVolumeRadiance;
				// Dispatch data
				PassParameters->GroupCount = GroupCount;
				// Output
				PassParameters->RWColorTexture = GraphBuilder.CreateUAV(SceneTextures.Color.Target);
			}

			TShaderRef<FHeterogeneousVolumesCompositeCS> ComputeShader = View.ShaderMap->GetShader<FHeterogeneousVolumesCompositeCS>();
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("FHeterogeneousVolumesCompositeCS"),
				ComputeShader,
				PassParameters,
				GroupCount);
		}
	}
}