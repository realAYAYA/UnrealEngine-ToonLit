// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeterogeneousVolumes.h"
#include "HeterogeneousVolumeInterface.h"

#include "PixelShaderUtils.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "RayTracingInstanceBufferUtil.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneManagement.h"

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumes(
	TEXT("r.HeterogeneousVolumes"),
	1,
	TEXT("Enables the Heterogeneous volume integrator (Default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesDebug(
	TEXT("r.HeterogeneousVolumes.Debug"),
	0,
	TEXT("Creates auxillary output buffers for debugging (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesHardwareRayTracing(
	TEXT("r.HeterogeneousVolumes.HardwareRayTracing"),
	0,
	TEXT("Enables hardware ray tracing acceleration (Default = 0)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesIndirectLighting(
	TEXT("r.HeterogeneousVolumes.IndirectLighting"),
	0,
	TEXT("Enables indirect lighting (Default = 0)"),
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
	512,
	TEXT("The maximum ray-marching step count (Default = 512)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesMaxTraceDistance(
	TEXT("r.HeterogeneousVolumes.MaxTraceDistance"),
	30000.0,
	TEXT("The maximum trace view-distance for direct volume rendering (Default = 30000)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesMaxShadowTraceDistance(
	TEXT("r.HeterogeneousVolumes.MaxShadowTraceDistance"),
	30000.0,
	TEXT("The maximum shadow-trace distance (Default = 30000)"),
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
	TEXT("r.HeterogeneousVolumes.VolumeResolution.X"),
	0,
	TEXT("Overrides the preshading and lighting volume resolution in X (Default = 0)")
	TEXT("0: Disabled, uses per-volume attribute\n")
	TEXT(">0: Overrides resolution in X\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingVolumeResolutionY(
	TEXT("r.HeterogeneousVolumes.VolumeResolution.Y"),
	0,
	TEXT("Overrides the preshading and lighting volume resolution in X (Default = 0)")
	TEXT("0: Disabled, uses per-volume attribute\n")
	TEXT(">0: Overrides resolution in Y\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesPreshadingVolumeResolutionZ(
	TEXT("r.HeterogeneousVolumes.VolumeResolution.Z"),
	0,
	TEXT("Overrides the preshading and lighting volume resolution in X (Default = 0)")
	TEXT("0: Disabled, uses per-volume attribute\n")
	TEXT(">0: Overrides resolution in Z\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarHeterogeneousVolumesShadowStepSize(
	TEXT("r.HeterogeneousVolumes.ShadowStepSize"),
	-1.0,
	TEXT("The ray-marching step-size override for shadow rays (Default = -1.0, disabled)"),
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
	-1.0,
	TEXT("The ray-marching step-size override (Default = -1.0, disabled)"),
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
	0,
	TEXT("Overrides the lighting-cache downsample factor, relative to the preshading volume resolution (Default = 0)\n")
	TEXT("0: Disabled, uses per-volume attribute\n")
	TEXT(">0: Overrides the lighting-cache downsample factor"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarHeterogeneousVolumesDepthSort(
	TEXT("r.HeterogeneousVolumes.DepthSort"),
	1,
	TEXT("Iterates over volumes in depth-sorted order, based on its centroid (Default = 1)"),
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
		&& Scene != nullptr
		&& DoesPlatformSupportHeterogeneousVolumes(Scene->GetShaderPlatform());
}

bool ShouldRenderHeterogeneousVolumesForView(
	const FViewInfo& View
)
{
	return IsHeterogeneousVolumesEnabled()
		&& !View.HeterogeneousVolumesMeshBatches.IsEmpty()
		&& View.Family
		&& !View.bIsReflectionCapture;
}

bool DoesPlatformSupportHeterogeneousVolumes(EShaderPlatform Platform)
{
	return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5)
		// TODO:
		// && FDataDrivenShaderPlatformInfo::GetSupportsHeterogeneousVolumes(Platform)
		&& !IsForwardShadingEnabled(Platform);
}

bool DoesMaterialShaderSupportHeterogeneousVolumes(const FMaterialShaderParameters& MaterialShaderParameters)
{
	return (MaterialShaderParameters.MaterialDomain == MD_Volume)
		&& MaterialShaderParameters.bIsUsedWithHeterogeneousVolumes;
}

bool DoesMaterialShaderSupportHeterogeneousVolumes(const FMaterial& Material)
{
	return (Material.GetMaterialDomain() == MD_Volume)
		&& Material.IsUsedWithHeterogeneousVolumes();
}

bool ShouldRenderMeshBatchWithHeterogeneousVolumes(
	const FMeshBatch* Mesh,
	const FPrimitiveSceneProxy* Proxy,
	ERHIFeatureLevel::Type FeatureLevel
)
{
	check(Mesh);
	check(Proxy);
	check(Mesh->MaterialRenderProxy);

	const FMaterialRenderProxy* MaterialRenderProxy = Mesh->MaterialRenderProxy;
	const FMaterial& Material = MaterialRenderProxy->GetMaterialWithFallback(FeatureLevel, MaterialRenderProxy);
	return IsHeterogeneousVolumesEnabled()
		&& Proxy->IsHeterogeneousVolume()
		&& DoesMaterialShaderSupportHeterogeneousVolumes(Material);
}

namespace HeterogeneousVolumes
{
	// CVars
	FIntVector GetVolumeResolution(const IHeterogeneousVolumeInterface* Interface)
	{
		FIntVector VolumeResolution = Interface->GetVoxelResolution();

		FIntVector OverrideVolumeResolution = FIntVector(
			CVarHeterogeneousVolumesPreshadingVolumeResolutionX.GetValueOnRenderThread(),
			CVarHeterogeneousVolumesPreshadingVolumeResolutionY.GetValueOnRenderThread(),
			CVarHeterogeneousVolumesPreshadingVolumeResolutionZ.GetValueOnRenderThread());

		VolumeResolution.X = OverrideVolumeResolution.X > 0 ? OverrideVolumeResolution.X : VolumeResolution.X;
		VolumeResolution.Y = OverrideVolumeResolution.Y > 0 ? OverrideVolumeResolution.Y : VolumeResolution.Y;
		VolumeResolution.Z = OverrideVolumeResolution.Z > 0 ? OverrideVolumeResolution.Z : VolumeResolution.Z;

		// Clamp each dimension to [1, 1024]
		VolumeResolution.X = FMath::Clamp(VolumeResolution.X, 1, 1024);
		VolumeResolution.Y = FMath::Clamp(VolumeResolution.Y, 1, 1024);
		VolumeResolution.Z = FMath::Clamp(VolumeResolution.Z, 1, 1024);
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
		return IsRayTracingEnabled()
			&& (CVarHeterogeneousVolumesHardwareRayTracing.GetValueOnRenderThread() != 0);
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

	int GetVoxelCount(const FRDGTextureDesc& TextureDesc)
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
	FIntVector GetLightingCacheResolution(const IHeterogeneousVolumeInterface* RenderInterface)
	{
		float OverrideDownsampleFactor = CVarHeterogeneousVolumesLightingCacheDownsampleFactor.GetValueOnRenderThread();
		float DownsampleFactor = OverrideDownsampleFactor > 0.0 ? OverrideDownsampleFactor : RenderInterface->GetLightingDownsampleFactor();
		DownsampleFactor = FMath::Max(DownsampleFactor, 0.125);

		FVector VolumeResolution = FVector(GetVolumeResolution(RenderInterface));
		FIntVector LightingCacheResolution = FIntVector(VolumeResolution / DownsampleFactor);
		LightingCacheResolution.X = FMath::Clamp(LightingCacheResolution.X, 1, 1024);
		LightingCacheResolution.Y = FMath::Clamp(LightingCacheResolution.Y, 1, 1024);
		LightingCacheResolution.Z = FMath::Clamp(LightingCacheResolution.Z, 1, 512);
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
	SCOPED_NAMED_EVENT(HeterogeneousVolumes, FColor::Emerald);

	// Build global transmittance structure
	TRDGUniformBufferRef<FOrthoVoxelGridUniformBufferParameters> OrthoGridUniformBuffer;
	TRDGUniformBufferRef<FFrustumVoxelGridUniformBufferParameters> FrustumGridUniformBuffer;

	if (HeterogeneousVolumes::GetDebugMode() != 0)
	{
		BuildOrthoVoxelGrid(GraphBuilder, Scene, Views, OrthoGridUniformBuffer);
		BuildFrustumVoxelGrid(GraphBuilder, Scene, Views[0], FrustumGridUniformBuffer);
	}

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
			if (HeterogeneousVolumes::GetDebugMode() != 0)
			{
				RenderTransmittanceWithVoxelGrid(
					GraphBuilder,
					SceneTextures,
					Scene,
					ViewFamily,
					Views[ViewIndex],
					OrthoGridUniformBuffer,
					FrustumGridUniformBuffer,
					HeterogeneousVolumeRadiance
				);
			}
			else
			{
				// Collect volume interfaces
				struct VolumeMesh
				{
					const IHeterogeneousVolumeInterface* Volume;
					const FMaterialRenderProxy* MaterialRenderProxy;

					VolumeMesh(const IHeterogeneousVolumeInterface* V, const FMaterialRenderProxy* M) : 
						Volume(V),
						MaterialRenderProxy(M)
					{}
				};

				TArray<VolumeMesh> VolumeMeshes;
				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.HeterogeneousVolumesMeshBatches.Num(); ++MeshBatchIndex)
				{
					const FMeshBatch* Mesh = View.HeterogeneousVolumesMeshBatches[MeshBatchIndex].Mesh;
					const FPrimitiveSceneProxy* PrimitiveSceneProxy = View.HeterogeneousVolumesMeshBatches[MeshBatchIndex].Proxy;
					if (!ShouldRenderMeshBatchWithHeterogeneousVolumes(Mesh, PrimitiveSceneProxy, View.GetFeatureLevel()))
					{
						continue;
					}

					const FMaterialRenderProxy* MaterialRenderProxy = Mesh->MaterialRenderProxy;
					for (int32 VolumeIndex = 0; VolumeIndex < Mesh->Elements.Num(); ++VolumeIndex)
					{
						const IHeterogeneousVolumeInterface* HeterogeneousVolume = (IHeterogeneousVolumeInterface*)Mesh->Elements[VolumeIndex].UserData;
						//check(HeterogeneousVolume != nullptr);
						if (HeterogeneousVolume == nullptr)
						{
							continue;
						}

						VolumeMeshes.Add(VolumeMesh(HeterogeneousVolume, MaterialRenderProxy));
					}
				}

				// Provide coarse depth-sorting, based on camera-distance to world centroid
				bool bDepthSort = CVarHeterogeneousVolumesDepthSort.GetValueOnRenderThread() == 1;
				if (bDepthSort)
				{
					struct FDepthCompareHeterogeneousVolumes
					{
						FVector WorldCameraOrigin;
						FDepthCompareHeterogeneousVolumes(FViewInfo& View) : WorldCameraOrigin(View.ViewMatrices.GetViewOrigin()) {}

						FORCEINLINE bool operator()(const VolumeMesh& A, const VolumeMesh& B) const
						{
							FVector CameraToA = A.Volume->GetBounds().Origin - WorldCameraOrigin;
							float SquaredDistanceToA = FVector::DotProduct(CameraToA, CameraToA);

							FVector CameraToB = B.Volume->GetBounds().Origin - WorldCameraOrigin;
							float SquaredDistanceToB = FVector::DotProduct(CameraToB, CameraToB);

							return SquaredDistanceToA < SquaredDistanceToB;
						}
					};

					VolumeMeshes.Sort(FDepthCompareHeterogeneousVolumes(View));
				}

				for (int32 VolumeIndex = 0; VolumeIndex < VolumeMeshes.Num(); ++VolumeIndex)
				{
					{
						const IHeterogeneousVolumeInterface* HeterogeneousVolume = VolumeMeshes[VolumeIndex].Volume;
						const FMaterialRenderProxy* MaterialRenderProxy = VolumeMeshes[VolumeIndex].MaterialRenderProxy;
						const FPrimitiveSceneProxy* PrimitiveSceneProxy = HeterogeneousVolume->GetPrimitiveSceneProxy();
						const FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveSceneProxy->GetPrimitiveSceneInfo();
						const int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
						const FBoxSphereBounds LocalBoxSphereBounds = HeterogeneousVolume->GetLocalBounds();

						RDG_EVENT_SCOPE(GraphBuilder, "%s [%d]", *PrimitiveSceneProxy->GetResourceName().ToString(), VolumeIndex);

						// Allocate transmittance volume
						// TODO: Allow option for scalar transmittance to conserve bandwidth
						FIntVector LightingCacheResolution = HeterogeneousVolumes::GetLightingCacheResolution(HeterogeneousVolume);
						uint32 NumMips = FMath::Log2(float(FMath::Min(FMath::Min(LightingCacheResolution.X, LightingCacheResolution.Y), LightingCacheResolution.Z))) + 1;
						FRDGTextureDesc LightingCacheDesc = FRDGTextureDesc::Create3D(
							LightingCacheResolution,
                            !IsMetalPlatform(GShaderPlatformForFeatureLevel[View.FeatureLevel]) ? PF_FloatR11G11B10 : PF_FloatRGBA,
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
								HeterogeneousVolume,
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
								HeterogeneousVolume,
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

			}
			View.HeterogeneousVolumeRadiance = HeterogeneousVolumeRadiance;
		}
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
