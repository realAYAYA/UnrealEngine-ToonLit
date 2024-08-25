// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentLighting.cpp: Translucent lighting implementation.
=============================================================================*/

#include "TranslucentLighting.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "EngineDefines.h"
#include "RHI.h"
#include "RenderResource.h"
#include "HitProxies.h"
#include "FinalPostProcessSettings.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "Engine/MapBuildDataRegistry.h"
#include "Components/LightComponent.h"
#include "Materials/Material.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightSceneInfo.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "TranslucentRendering.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "OneColorShader.h"
#include "LightRendering.h"
#include "ScreenRendering.h"
#include "AmbientCubemapParameters.h"
#include "VolumeRendering.h"
#include "VolumeLighting.h"
#include "PipelineStateCache.h"
#include "VisualizeTexture.h"
#include "MeshPassProcessor.inl"
#include "SkyAtmosphereRendering.h"
#include "VolumetricCloudRendering.h"
#include "RenderCore.h"
#include "StaticMeshBatch.h"
#include "LightFunctionAtlas.h"
#include "HeterogeneousVolumes/HeterogeneousVolumes.h"

class FMaterial;

/** Whether to allow rendering translucency shadow depths. */
bool GUseTranslucencyShadowDepths = true;

DECLARE_GPU_STAT_NAMED(TranslucentLighting, TEXT("Translucent Lighting"));
 
int32 GUseTranslucentLightingVolumes = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumes(
	TEXT("r.TranslucentLightingVolume"),
	GUseTranslucentLightingVolumes,
	TEXT("Whether to allow updating the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

float GTranslucentVolumeMinFOV = 45;
static FAutoConsoleVariableRef CVarTranslucentVolumeMinFOV(
	TEXT("r.TranslucentVolumeMinFOV"),
	GTranslucentVolumeMinFOV,
	TEXT("Minimum FOV for translucent lighting volume.  Prevents popping in lighting when zooming in."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

float GTranslucentVolumeFOVSnapFactor = 10;
static FAutoConsoleVariableRef CTranslucentVolumeFOVSnapFactor(
	TEXT("r.TranslucentVolumeFOVSnapFactor"),
	GTranslucentVolumeFOVSnapFactor,
	TEXT("FOV will be snapped to a factor of this before computing volume bounds."),
	ECVF_RenderThreadSafe | ECVF_Scalability
	);

int32 GUseTranslucencyVolumeBlur = 1;
FAutoConsoleVariableRef CVarUseTranslucentLightingVolumeBlur(
	TEXT("r.TranslucencyVolumeBlur"),
	GUseTranslucencyVolumeBlur,
	TEXT("Whether to blur the translucent lighting volumes.\n")
	TEXT("0:off, otherwise on, default is 1"),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

int32 GTranslucencyLightingVolumeDim = 64;
FAutoConsoleVariableRef CVarTranslucencyLightingVolumeDim(
	TEXT("r.TranslucencyLightingVolumeDim"),
	GTranslucencyLightingVolumeDim,
	TEXT("Dimensions of the volume textures used for translucency lighting.  Larger textures result in higher resolution but lower performance."),
	ECVF_Scalability | ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeInnerDistance(
	TEXT("r.TranslucencyLightingVolumeInnerDistance"),
	1500.0f,
	TEXT("Distance from the camera that the first volume cascade should end"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<float> CVarTranslucencyLightingVolumeOuterDistance(
	TEXT("r.TranslucencyLightingVolumeOuterDistance"),
	5000.0f,
	TEXT("Distance from the camera that the second volume cascade should end"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

/** Function returning current translucency lighting volume dimensions. */
int32 GetTranslucencyLightingVolumeDim()
{
	extern int32 GTranslucencyLightingVolumeDim;
	return FMath::Clamp(GTranslucencyLightingVolumeDim, 4, 2048);
}

void FViewInfo::CalcTranslucencyLightingVolumeBounds(FBox* InOutCascadeBoundsArray, int32 NumCascades) const
{
	for (int32 CascadeIndex = 0; CascadeIndex < NumCascades; CascadeIndex++)
	{
		double InnerDistance = CVarTranslucencyLightingVolumeInnerDistance.GetValueOnRenderThread();
		double OuterDistance = CVarTranslucencyLightingVolumeOuterDistance.GetValueOnRenderThread();

		const double FrustumStartDistance = CascadeIndex == 0 ? 0 : InnerDistance;
		const double FrustumEndDistance = CascadeIndex == 0 ? InnerDistance : OuterDistance;

		double FieldOfView = DOUBLE_PI / 4.0;
		double AspectRatio = 1.0;

		if (IsPerspectiveProjection())
		{
			// Derive FOV and aspect ratio from the perspective projection matrix
			FieldOfView = FMath::Atan(1.0 / ShadowViewMatrices.GetProjectionMatrix().M[0][0]);
			// Clamp to prevent shimmering when zooming in
			FieldOfView = FMath::Max(FieldOfView, GTranslucentVolumeMinFOV * DOUBLE_PI / 180.0);
			const double RoundFactorRadians = GTranslucentVolumeFOVSnapFactor * DOUBLE_PI / 180.0;
			// Round up to a fixed factor
			// This causes the volume lighting to make discreet jumps as the FOV animates, instead of slowly crawling over a long period
			FieldOfView = FieldOfView + RoundFactorRadians - FMath::Fmod(FieldOfView, RoundFactorRadians);
			AspectRatio = ShadowViewMatrices.GetProjectionMatrix().M[1][1] / ShadowViewMatrices.GetProjectionMatrix().M[0][0];
		}

		const double StartHorizontalLength = FrustumStartDistance * FMath::Tan(FieldOfView);
		const FVector StartCameraRightOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(0) * StartHorizontalLength;
		const double StartVerticalLength = StartHorizontalLength / AspectRatio;
		const FVector StartCameraUpOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(1) * StartVerticalLength;

		const double EndHorizontalLength = FrustumEndDistance * FMath::Tan(FieldOfView);
		const FVector EndCameraRightOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(0) * EndHorizontalLength;
		const double EndVerticalLength = EndHorizontalLength / AspectRatio;
		const FVector EndCameraUpOffset = ShadowViewMatrices.GetViewMatrix().GetColumn(1) * EndVerticalLength;

		FVector SplitVertices[8];
		const FVector ShadowViewOrigin = ShadowViewMatrices.GetViewOrigin();

		SplitVertices[0] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[1] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance + StartCameraRightOffset - StartCameraUpOffset;
		SplitVertices[2] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset + StartCameraUpOffset;
		SplitVertices[3] = ShadowViewOrigin + GetViewDirection() * FrustumStartDistance - StartCameraRightOffset - StartCameraUpOffset;

		SplitVertices[4] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[5] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance + EndCameraRightOffset - EndCameraUpOffset;
		SplitVertices[6] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset + EndCameraUpOffset;
		SplitVertices[7] = ShadowViewOrigin + GetViewDirection() * FrustumEndDistance - EndCameraRightOffset - EndCameraUpOffset;

		FVector Center(0,0,0);
		// Weight the far vertices more so that the bounding sphere will be further from the camera
		// This minimizes wasted shadowmap space behind the viewer
		const double FarVertexWeightScale = 10.0;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			const double Weight = VertexIndex > 3 ? 1 / (4.0 + 4.0 / FarVertexWeightScale) : 1 / (4.0 + 4.0 * FarVertexWeightScale);
			Center += SplitVertices[VertexIndex] * Weight;
		}

		double RadiusSquared = 0;
		for (int32 VertexIndex = 0; VertexIndex < 8; VertexIndex++)
		{
			RadiusSquared = FMath::Max(RadiusSquared, (Center - SplitVertices[VertexIndex]).SizeSquared());
		}

		if (RadiusSquared > 0) // Avoid issues with bad cvar usage, e.g. r.TranslucencyLightingVolumeInnerDistance.
		{
			FSphere SphereBounds(Center, FMath::Sqrt(RadiusSquared));

			// Snap the center to a multiple of the volume dimension for stability
			const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
			SphereBounds.Center.X = SphereBounds.Center.X - FMath::Fmod(SphereBounds.Center.X, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
			SphereBounds.Center.Y = SphereBounds.Center.Y - FMath::Fmod(SphereBounds.Center.Y, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);
			SphereBounds.Center.Z = SphereBounds.Center.Z - FMath::Fmod(SphereBounds.Center.Z, SphereBounds.W * 2 / TranslucencyLightingVolumeDim);

			InOutCascadeBoundsArray[CascadeIndex] = FBox(SphereBounds.Center - SphereBounds.W, SphereBounds.Center + SphereBounds.W);
		}
		else
		{
			InOutCascadeBoundsArray[CascadeIndex] = FBox(Center, Center);
		}
	}
}

class FTranslucencyDepthShaderElementData : public FMeshMaterialShaderElementData
{
public:
	float TranslucentShadowStartOffset;
};

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassUniformParameters,)
	SHADER_PARAMETER_STRUCT(FSceneTextureUniformParameters, SceneTextures)
	SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
	SHADER_PARAMETER(float, bClampToNearPlane)
	SHADER_PARAMETER(float, InvMaxSubjectDepth)
	SHADER_PARAMETER_STRUCT(FTranslucentSelfShadowUniformParameters, TranslucentSelfShadow)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_STATIC_UNIFORM_BUFFER_STRUCT(FTranslucencyDepthPassUniformParameters, "TranslucentDepthPass", SceneTextures);

void SetupTranslucencyDepthPassUniformBuffer(
	const FProjectedShadowInfo* ShadowInfo,
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	FTranslucencyDepthPassUniformParameters& TranslucencyDepthPassParameters)
{
	// Note - scene depth can be bound by the material for use in depth fades
	// This is incorrect when rendering a shadowmap as it's not from the camera's POV
	// Set the scene depth texture to something safe when rendering shadow depths
	SetupSceneTextureUniformParameters(GraphBuilder, View.GetSceneTexturesChecked(), View.FeatureLevel, ESceneTextureSetupMode::None, TranslucencyDepthPassParameters.SceneTextures);

	TranslucencyDepthPassParameters.ProjectionMatrix = FTranslationMatrix44f(FVector3f(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation())) * ShadowInfo->TranslatedWorldToClipInnerMatrix;

	// Only clamp vertices to the near plane when rendering whole scene directional light shadow depths or preshadows from directional lights
	const bool bClampToNearPlaneValue = ShadowInfo->IsWholeSceneDirectionalShadow() || (ShadowInfo->bPreShadow && ShadowInfo->bDirectionalLight);
	TranslucencyDepthPassParameters.bClampToNearPlane = bClampToNearPlaneValue ? 1.0f : 0.0f;

	TranslucencyDepthPassParameters.InvMaxSubjectDepth = ShadowInfo->InvMaxSubjectDepth;

	SetupTranslucentSelfShadowUniformParameters(ShadowInfo, TranslucencyDepthPassParameters.TranslucentSelfShadow);
}

/**
* Vertex shader used to render shadow maps for translucency.
*/
class FTranslucencyShadowDepthVS : public FMeshMaterialShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FTranslucencyShadowDepthVS, NonVirtual);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return AllowTranslucencyPerObjectShadows(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters);
	}

	FTranslucencyShadowDepthVS() {}
	FTranslucencyShadowDepthVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{}
};

enum ETranslucencyShadowDepthShaderMode
{
	TranslucencyShadowDepth_PerspectiveCorrect,
	TranslucencyShadowDepth_Standard,
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthVS : public FTranslucencyShadowDepthVS
{
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthVS, MeshMaterial);
public:

	TTranslucencyShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FTranslucencyShadowDepthVS(Initializer)
	{}

	TTranslucencyShadowDepthVS() {}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTranslucencyShadowDepthVS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthVS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainVS"),SF_Vertex);

/**
 * Pixel shader used for accumulating translucency layer densities
 */
class FTranslucencyShadowDepthPS : public FMeshMaterialShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FTranslucencyShadowDepthPS, NonVirtual);
public:
	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return AllowTranslucencyPerObjectShadows(Parameters.Platform) && IsTranslucentBlendMode(Parameters.MaterialParameters);
	}

	FTranslucencyShadowDepthPS() = default;
	FTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FMeshMaterialShader(Initializer)
	{
		TranslucentShadowStartOffset.Bind(Initializer.ParameterMap, TEXT("TranslucentShadowStartOffset"));
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FTranslucencyDepthShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(TranslucentShadowStartOffset, ShaderElementData.TranslucentShadowStartOffset);
	}

private:
	LAYOUT_FIELD(FShaderParameter, TranslucentShadowStartOffset);
};

template <ETranslucencyShadowDepthShaderMode ShaderMode>
class TTranslucencyShadowDepthPS : public FTranslucencyShadowDepthPS
{
public:
	DECLARE_SHADER_TYPE(TTranslucencyShadowDepthPS, MeshMaterial);

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FTranslucencyShadowDepthPS::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == TranslucencyShadowDepth_PerspectiveCorrect ? 1 : 0));
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
	}

	TTranslucencyShadowDepthPS() = default;
	TTranslucencyShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType & Initializer) :
		FTranslucencyShadowDepthPS(Initializer)
	{}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_PerspectiveCorrect>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TTranslucencyShadowDepthPS<TranslucencyShadowDepth_Standard>,TEXT("/Engine/Private/TranslucentShadowDepthShaders.usf"),TEXT("MainOpacityPS"),SF_Pixel);

class FTranslucencyDepthPassMeshProcessor : public FMeshPassProcessor
{
public:
	FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
		const FSceneView* InViewIfDynamicMeshCommand,
		const FMeshPassProcessorRenderState& InPassDrawRenderState,
		const FProjectedShadowInfo* InShadowInfo,
		FMeshPassDrawListContext* InDrawListContext);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final;

private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch, 
		uint64 BatchElementMask, 
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, 
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material);

	template<ETranslucencyShadowDepthShaderMode ShaderMode>
	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		int32 StaticMeshId,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		float MaterialTranslucentShadowStartOffset,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode);

	FMeshPassProcessorRenderState PassDrawRenderState;
	const FProjectedShadowInfo* ShadowInfo;
	FShadowDepthType ShadowDepthType;
	const bool bDirectionalLight;
};

FTranslucencyDepthPassMeshProcessor::FTranslucencyDepthPassMeshProcessor(const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	const FProjectedShadowInfo* InShadowInfo,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Num, Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext)
	, PassDrawRenderState(InPassDrawRenderState)
	, ShadowInfo(InShadowInfo)
	, ShadowDepthType(InShadowInfo->GetShadowDepthType())
	, bDirectionalLight(InShadowInfo->bDirectionalLight)
{
}

bool FTranslucencyDepthPassMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch, 
	uint64 BatchElementMask, 
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, 
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// Determine the mesh's material and blend mode.
	const float MaterialTranslucentShadowStartOffset = Material.GetTranslucentShadowStartOffset();
	const bool MaterialCastDynamicShadowAsMasked = Material.GetCastDynamicShadowAsMasked();
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(Material, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(Material, OverrideSettings);
	const bool bIsTranslucent = IsTranslucentBlendMode(Material);

	// Only render translucent meshes into the Fourier opacity maps
	if (bIsTranslucent && ShouldIncludeDomainInMeshPass(Material.GetMaterialDomain()) && !MaterialCastDynamicShadowAsMasked)
	{
		if (bDirectionalLight)
		{
			return Process<TranslucencyShadowDepth_Standard>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
		}
		else
		{
			return Process<TranslucencyShadowDepth_PerspectiveCorrect>(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy, MaterialRenderProxy, Material, MaterialTranslucentShadowStartOffset, MeshFillMode, MeshCullMode);
		}
	}

	return true;
}

template<ETranslucencyShadowDepthShaderMode ShaderMode>
bool FTranslucencyDepthPassMeshProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	float MaterialTranslucentShadowStartOffset,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		TTranslucencyShadowDepthVS<ShaderMode>,
		TTranslucencyShadowDepthPS<ShaderMode>> PassShaders;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<TTranslucencyShadowDepthVS<ShaderMode>>();
	ShaderTypes.AddShaderType<TTranslucencyShadowDepthPS<ShaderMode>>();

	FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(PassShaders.VertexShader);
	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	FMeshPassProcessorRenderState DrawRenderState(PassDrawRenderState);

	FTranslucencyDepthShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const float LocalToWorldScale = ShadowInfo->GetParentSceneInfo()->Proxy->GetLocalToWorld().GetScaleVector().GetMax();
	const float TranslucentShadowStartOffsetValue = MaterialTranslucentShadowStartOffset * LocalToWorldScale;
	ShaderElementData.TranslucentShadowStartOffset = TranslucentShadowStartOffsetValue / (ShadowInfo->MaxSubjectZ - ShadowInfo->MinSubjectZ);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		DrawRenderState,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

void FTranslucencyDepthPassMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId)
{
	if (MeshBatch.CastShadow)
	{
		const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
		while (MaterialRenderProxy)
		{
			const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
			if (Material && Material->GetRenderingThreadShaderMap())
			{
				if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
				{
					break;
				}
			}

			MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
		}
	}
}

BEGIN_SHADER_PARAMETER_STRUCT(FTranslucencyDepthPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FTranslucencyDepthPassUniformParameters, PassUniformBuffer)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void FProjectedShadowInfo::RenderTranslucencyDepths(FRDGBuilder& GraphBuilder, FSceneRenderer* SceneRenderer, const FRenderTargetBindingSlots& InRenderTargets, FInstanceCullingManager& InstanceCullingManager)
{
	check(IsInRenderingThread());
	checkSlow(!bWholeSceneShadow);
	SCOPE_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime);

	BeginRenderView(GraphBuilder, SceneRenderer->Scene);

	auto* TranslucencyDepthPassParameters = GraphBuilder.AllocParameters<FTranslucencyDepthPassUniformParameters>();
	SetupTranslucencyDepthPassUniformBuffer(this, GraphBuilder, *ShadowDepthView, *TranslucencyDepthPassParameters);
	TRDGUniformBufferRef<FTranslucencyDepthPassUniformParameters> PassUniformBuffer = GraphBuilder.CreateUniformBuffer(TranslucencyDepthPassParameters);

	auto* PassParameters = GraphBuilder.AllocParameters<FTranslucencyDepthPassParameters>();
	PassParameters->View = ShadowDepthView->ViewUniformBuffer;
	PassParameters->PassUniformBuffer = PassUniformBuffer;
	PassParameters->RenderTargets = InRenderTargets;

	FSimpleMeshDrawCommandPass* SimpleMeshDrawCommandPass = GraphBuilder.AllocObject<FSimpleMeshDrawCommandPass>(*ShadowDepthView, &InstanceCullingManager);

	FMeshPassProcessorRenderState DrawRenderState;
	DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
	DrawRenderState.SetBlendState(TStaticBlendState<
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
		CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI());


	FTranslucencyDepthPassMeshProcessor TranslucencyDepthPassMeshProcessor(
		SceneRenderer->Scene,
		ShadowDepthView,
		DrawRenderState,
		this,
		SimpleMeshDrawCommandPass->GetDynamicPassMeshDrawListContext());

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectTranslucentMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshAndRelevance = DynamicSubjectTranslucentMeshElements[MeshBatchIndex];
		const uint64 BatchElementMask = ~0ull;
		TranslucencyDepthPassMeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, BatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
	}

	for (int32 PrimitiveIndex = 0; PrimitiveIndex < SubjectTranslucentPrimitives.Num(); PrimitiveIndex++)
	{
		const FPrimitiveSceneInfo* PrimitiveSceneInfo = SubjectTranslucentPrimitives[PrimitiveIndex];
		int32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
		FPrimitiveViewRelevance ViewRelevance = ShadowDepthView->PrimitiveViewRelevanceMap[PrimitiveId];

		if (!ViewRelevance.bInitializedThisFrame)
		{
			// Compute the subject primitive's view relevance since it wasn't cached
			ViewRelevance = PrimitiveSceneInfo->Proxy->GetViewRelevance(ShadowDepthView);
		}

		if (ViewRelevance.bDrawRelevance && ViewRelevance.bStaticRelevance)
		{
			int8 MinLOD, MaxLOD;
			PrimitiveSceneInfo->GetStaticMeshesLODRange(MinLOD, MaxLOD);
			// For any primitive, we only render LOD0 meshes since we do not have FSceneView available to use ComputeLODForMeshes.
			for (int32 MeshIndex = 0; MeshIndex < PrimitiveSceneInfo->StaticMeshes.Num(); MeshIndex++)
			{
				const FStaticMeshBatch& StaticMeshBatch = PrimitiveSceneInfo->StaticMeshes[MeshIndex];
				if (StaticMeshBatch.LODIndex != MinLOD)
				{
					continue;
				}
				const uint64 DefaultBatchElementMask = ~0ul;
				TranslucencyDepthPassMeshProcessor.AddMeshBatch(StaticMeshBatch, DefaultBatchElementMask, StaticMeshBatch.PrimitiveSceneInfo->Proxy, StaticMeshBatch.Id);
			}
		}
	}

	SimpleMeshDrawCommandPass->BuildRenderingCommands(GraphBuilder, *ShadowDepthView, SceneRenderer->Scene->GPUScene, PassParameters->InstanceCullingDrawParams);


	FString EventName;
#if WANTS_DRAW_MESH_EVENTS
	if (GetEmitDrawEvents())
	{
		GetShadowTypeNameForDrawEvent(EventName);
	}
#endif

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("%s", *EventName),
		PassParameters,
		ERDGPassFlags::Raster,
		[this, SimpleMeshDrawCommandPass, PassParameters](FRHICommandList& RHICmdList)
	{
		FMeshPassProcessorRenderState DrawRenderState;

		// Clear the shadow and its border
		RHICmdList.SetViewport(
			X,
			Y,
			0.0f,
			(X + BorderSize * 2 + ResolutionX),
			(Y + BorderSize * 2 + ResolutionY),
			1.0f
		);

		FLinearColor ClearColors[2] = { FLinearColor(0,0,0,0), FLinearColor(0,0,0,0) };
		DrawClearQuadMRT(RHICmdList, true, UE_ARRAY_COUNT(ClearColors), ClearColors, false, 1.0f, false, 0);

		// Set the viewport for the shadow.
		RHICmdList.SetViewport(
			(X + BorderSize),
			(Y + BorderSize),
			0.0f,
			(X + BorderSize + ResolutionX),
			(Y + BorderSize + ResolutionY),
			1.0f
		);
		SimpleMeshDrawCommandPass->SubmitDraw(RHICmdList, PassParameters->InstanceCullingDrawParams);
	});
}

/** Pixel shader used to filter a single volume lighting cascade. */
class FFilterTranslucentVolumePS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFilterTranslucentVolumePS);
	SHADER_USE_PARAMETER_STRUCT(FFilterTranslucentVolumePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeAmbient)
		SHADER_PARAMETER_RDG_TEXTURE(Texture3D, TranslucencyLightingVolumeDirectional)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyLightingVolumeAmbientSampler)
		SHADER_PARAMETER_SAMPLER(SamplerState, TranslucencyLightingVolumeDirectionalSampler)
		SHADER_PARAMETER(float, TexelSize)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}
};

IMPLEMENT_GLOBAL_SHADER(FFilterTranslucentVolumePS, "/Engine/Private/TranslucentLightingShaders.usf", "FilterMainPS", SF_Pixel);

/** Shader that adds direct lighting contribution from the given light to the current volume lighting cascade. */
class FTranslucentLightingInjectPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FTranslucentLightingInjectPS, Material);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDeferredLightUniformStruct, DeferredLight)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVolumeShadowingShaderParameters, VolumeShadowingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FVirtualShadowMapSamplingParameters, VirtualShadowMapSamplingParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FLightCloudTransmittanceParameters, LightCloudTransmittanceParameters)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FAdaptiveVolumetricShadowMapUniformBufferParameters, AVSM)
		SHADER_PARAMETER(FMatrix44f, LightFunctionTranslatedWorldToLight)
		SHADER_PARAMETER(FVector4f, LightFunctionParameters)
		SHADER_PARAMETER(float, SpotlightMask)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)
		SHADER_PARAMETER(int32, VirtualShadowMapId)
		SHADER_PARAMETER(uint32, AtmospherePerPixelTransmittanceEnabled)
		SHADER_PARAMETER(uint32, VolumetricCloudShadowEnabled)
	END_SHADER_PARAMETER_STRUCT()

	class FRadialAttenuation	: SHADER_PERMUTATION_BOOL("RADIAL_ATTENUATION");
	class FDynamicallyShadowed	: SHADER_PERMUTATION_BOOL("DYNAMICALLY_SHADOWED");
	class FLightFunction		: SHADER_PERMUTATION_BOOL("APPLY_LIGHT_FUNCTION");
	class FVirtualShadowMap		: SHADER_PERMUTATION_BOOL("VIRTUAL_SHADOW_MAP");
	class FAdaptiveVolumetricShadowMap : SHADER_PERMUTATION_BOOL("ADAPTIVE_VOLUMETRIC_SHADOW_MAP");

	using FPermutationDomain = TShaderPermutationDomain<
		FRadialAttenuation,
		FDynamicallyShadowed,
		FLightFunction,
		FVirtualShadowMap,
		FAdaptiveVolumetricShadowMap >;

public:

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INJECTION_PIXEL_SHADER"), 1);
	}

	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	  */
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (!DoesPlatformSupportVirtualShadowMaps(Parameters.Platform) && PermutationVector.Get<FVirtualShadowMap>() != 0)
		{
			return false;
		}

		if (!DoesPlatformSupportHeterogeneousVolumes(Parameters.Platform) && PermutationVector.Get<FAdaptiveVolumetricShadowMap>() != 0)
		{
			return false;
		}

		return (Parameters.MaterialParameters.MaterialDomain == MD_LightFunction || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
			(IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			(RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform)));
	}

	FTranslucentLightingInjectPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMaterialShader(Initializer)
	{
		Bindings.BindForLegacyShaderParameters( 
			this, 
			Initializer.PermutationId, 
			Initializer.ParameterMap,
			*FParameters::FTypeInfo::GetStructMetadata(), 
			// Don't require full bindings, we use FMaterialShader::SetParameters
			false); 
	}

	FTranslucentLightingInjectPS() {}

	void SetParameters(
		FRHIBatchedShaderParameters& BatchedParameters,
		const FViewInfo& View, 
		const FMaterialRenderProxy* MaterialProxy)
	{
		const FMaterial& Material = MaterialProxy->GetMaterialWithFallback(View.GetFeatureLevel(), MaterialProxy);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FTranslucentLightingInjectPS, TEXT("/Engine/Private/TranslucentLightInjectionShaders.usf"), TEXT("InjectMainPS"), SF_Pixel);

class FClearTranslucentLightingVolumeCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearTranslucentLightingVolumeCS);
	SHADER_USE_PARAMETER_STRUCT(FClearTranslucentLightingVolumeCS, FGlobalShader)

	static const int32 CLEAR_BLOCK_SIZE = 4;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWAmbient0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWDirectional0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWAmbient1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float4>, RWDirectional1)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CLEAR_COMPUTE_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("CLEAR_BLOCK_SIZE"), CLEAR_BLOCK_SIZE);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearTranslucentLightingVolumeCS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "ClearTranslucentLightingVolumeCS", SF_Compute);

int32 FTranslucencyLightingVolumeTextures::GetIndex(const FViewInfo& View, int32 CascadeIndex) const
{
	// if we only have one view or one stereo pair we can just use primary index
	if (Directional.Num() == TVC_MAX)
	{
		return (View.PrimaryViewIndex * TVC_MAX) + CascadeIndex;
	}
	else
	{
		// support uncommon but possible (in theory) situations, like a stereo pair and also multiple views
		return (ViewsToTexturePairs[View.PrimaryViewIndex] * TVC_MAX) + CascadeIndex;
	}
}

void FTranslucencyLightingVolumeTextures::Init(FRDGBuilder& GraphBuilder, TArrayView<const FViewInfo> Views, ERDGPassFlags PassFlags)
{
	check(PassFlags == ERDGPassFlags::Compute || PassFlags == ERDGPassFlags::AsyncCompute);

	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	VolumeDim = GetTranslucencyLightingVolumeDim();
	const FIntVector TranslucencyLightingVolumeDim(VolumeDim);

	// calculate the number of textures needed given that for each stereo pair the primary view's textures will be shared between the "eyes"
	const int32 ViewCount = Views.Num();
	uint32 NumViewsWithTextures = 0;
	ViewsToTexturePairs.SetNumZeroed(Views.Num());
	for (int32 ViewIndex = 0, NumViews = Views.Num(); ViewIndex < NumViews; ++ViewIndex)
	{
		ViewsToTexturePairs[ViewIndex] = NumViewsWithTextures;
		NumViewsWithTextures += (ViewIndex == Views[ViewIndex].PrimaryViewIndex) ? 1 : 0;	// this will add 0 for those views who aren't primary
	}
	check(NumViewsWithTextures > 0);
	{
		// TODO: We can skip the and TLV allocations when rendering in forward shading mode
		const ETextureCreateFlags TranslucencyTargetFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_ReduceMemoryWithTilingMode | TexCreate_UAV;

		Ambient.SetNum(NumViewsWithTextures * TVC_MAX);
		Directional.SetNum(NumViewsWithTextures * TVC_MAX);

		for (int32 ViewIndex = 0; ViewIndex < ViewCount; ++ViewIndex)
		{
			for (int32 CascadeIndex = 0; CascadeIndex < TVC_MAX; ++CascadeIndex)
			{
				const uint32 TextureIndex = FTranslucencyLightingVolumeTextures::GetIndex(Views[ViewIndex], CascadeIndex);
				check(TextureIndex <= NumViewsWithTextures * TVC_MAX);

				const FRDGEventName& AmbientName = *GraphBuilder.AllocObject<FRDGEventName>(RDG_EVENT_NAME("TranslucentVolumeAmbient%d", TextureIndex));
				const FRDGEventName& DirectionalName = *GraphBuilder.AllocObject<FRDGEventName>(RDG_EVENT_NAME("TranslucentVolumeDirectional%d", TextureIndex));

				FRDGTextureRef AmbientTexture = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(
						TranslucencyLightingVolumeDim,
						PF_FloatRGBA,
						FClearValueBinding::Transparent,
						TranslucencyTargetFlags),
					AmbientName.GetTCHAR());

				FRDGTextureRef DirectionalTexture = GraphBuilder.CreateTexture(
					FRDGTextureDesc::Create3D(
						TranslucencyLightingVolumeDim,
						PF_FloatRGBA,
						FClearValueBinding::Transparent,
						TranslucencyTargetFlags),
					DirectionalName.GetTCHAR());

				Ambient[TextureIndex] = AmbientTexture;
				Directional[TextureIndex] = DirectionalTexture;
			}
		}
	}

	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(TranslucencyLightingVolumeDim, FClearTranslucentLightingVolumeCS::CLEAR_BLOCK_SIZE);

	TShaderMapRef<FClearTranslucentLightingVolumeCS> ComputeShader(Views[0].ShaderMap);

	for (uint32 TexturePairIndex = 0; TexturePairIndex < NumViewsWithTextures; ++TexturePairIndex)
	{
		auto* PassParameters = GraphBuilder.AllocParameters<FClearTranslucentLightingVolumeCS::FParameters>();
		PassParameters->RWAmbient0 = GraphBuilder.CreateUAV(Ambient[TexturePairIndex * TVC_MAX]);
		PassParameters->RWAmbient1 = GraphBuilder.CreateUAV(Ambient[TexturePairIndex * TVC_MAX + 1]);
		PassParameters->RWDirectional0 = GraphBuilder.CreateUAV(Directional[TexturePairIndex * TVC_MAX]);
		PassParameters->RWDirectional1 = GraphBuilder.CreateUAV(Directional[TexturePairIndex * TVC_MAX + 1]);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClearTranslucencyLightingVolumeCompute %d", VolumeDim),
			PassFlags,
			ComputeShader,
			PassParameters,
			GroupCount);
	}
}

FTranslucencyLightingVolumeParameters GetTranslucencyLightingVolumeParameters(FRDGBuilder& GraphBuilder, const FTranslucencyLightingVolumeTextures& Textures, const FViewInfo& View)
{
	FTranslucencyLightingVolumeParameters Parameters;
	if (Textures.IsValid())
	{
		const uint32 InnerIndex = Textures.GetIndex(View, TVC_Inner);
		const uint32 OuterIndex = Textures.GetIndex(View, TVC_Outer);

		Parameters.TranslucencyLightingVolumeAmbientInner = Textures.Ambient[InnerIndex];
		Parameters.TranslucencyLightingVolumeAmbientOuter = Textures.Ambient[OuterIndex];
		Parameters.TranslucencyLightingVolumeDirectionalInner = Textures.Directional[InnerIndex];
		Parameters.TranslucencyLightingVolumeDirectionalOuter = Textures.Directional[OuterIndex];
	}
	else
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		Parameters.TranslucencyLightingVolumeAmbientInner = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeAmbientOuter = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeDirectionalInner = SystemTextures.VolumetricBlack;
		Parameters.TranslucencyLightingVolumeDirectionalOuter = SystemTextures.VolumetricBlack;
	}
	return Parameters;
}

class FInjectAmbientCubemapPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FInjectAmbientCubemapPS);
	SHADER_USE_PARAMETER_STRUCT(FInjectAmbientCubemapPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FAmbientCubemapParameters, AmbientCubemap)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FInjectAmbientCubemapPS, "/Engine/Private/TranslucentLightingShaders.usf", "InjectAmbientCubemapMainPS", SF_Pixel);

void InjectTranslucencyLightingVolumeAmbientCubemap(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views,
	const FTranslucencyLightingVolumeTextures& Textures)
{
	if (!GUseTranslucentLightingVolumes || !GSupportsVolumeTextureRendering)
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "InjectAmbientCubemapTranslucentVolumeLighting");
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	const int32 TranslucencyLightingVolumeDim = Textures.VolumeDim;
	const FVolumeBounds VolumeBounds(TranslucencyLightingVolumeDim);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; ++VolumeCascadeIndex)
		{
			FRDGTextureRef VolumeAmbientTexture = Textures.GetAmbientTexture(View, VolumeCascadeIndex);

			for (const FFinalPostProcessSettings::FCubemapEntry& CubemapEntry : View.FinalPostProcessSettings.ContributingCubemaps)
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FInjectAmbientCubemapPS::FParameters>();
				SetupAmbientCubemapParameters(CubemapEntry, &PassParameters->AmbientCubemap);
				PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
				PassParameters->View = View.ViewUniformBuffer;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Cascade %d", VolumeCascadeIndex),
					PassParameters,
					ERDGPassFlags::Raster,
					[&View, PassParameters, VolumeBounds, TranslucencyLightingVolumeDim](FRHICommandList& RHICmdList)
				{
					TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
					TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
					TShaderMapRef<FInjectAmbientCubemapPS> PixelShader(View.ShaderMap);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if (GeometryShader.IsValid())
					{
						SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
					}
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
				});
			}
		}
	}
}

/** Calculates volume texture bounds for the given light in the given translucent lighting volume cascade. */
FVolumeBounds CalculateLightVolumeBounds(const FSphere& LightBounds, const FViewInfo& View, uint32 VolumeCascadeIndex, bool bDirectionalLight)
{
	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

	FVolumeBounds VolumeBounds;

	if (bDirectionalLight)
	{
		VolumeBounds = FVolumeBounds(TranslucencyLightingVolumeDim);
	}
	else
	{
		// Determine extents in the volume texture
		const FVector MinPosition = (LightBounds.Center - LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];
		const FVector MaxPosition = (LightBounds.Center + LightBounds.W - View.TranslucencyLightingVolumeMin[VolumeCascadeIndex]) / View.TranslucencyVolumeVoxelSize[VolumeCascadeIndex];

		VolumeBounds.MinX = FMath::Max(FMath::TruncToInt32(MinPosition.X), 0);
		VolumeBounds.MinY = FMath::Max(FMath::TruncToInt32(MinPosition.Y), 0);
		VolumeBounds.MinZ = FMath::Max(FMath::TruncToInt32(MinPosition.Z), 0);

		VolumeBounds.MaxX = FMath::Min(FMath::TruncToInt32(MaxPosition.X) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxY = FMath::Min(FMath::TruncToInt32(MaxPosition.Y) + 1, TranslucencyLightingVolumeDim);
		VolumeBounds.MaxZ = FMath::Min(FMath::TruncToInt32(MaxPosition.Z) + 1, TranslucencyLightingVolumeDim);
	}

	return VolumeBounds;
}

FTranslucentLightInjectionCollector::FTranslucentLightInjectionCollector(
	FRDGBuilder& GraphBuilder,
	TArrayView<const FViewInfo> Views)
	// NOTE: This data is directly referenced inside the render pass lamba, so must be allocated in the graph
	: InjectionDataPerView(*GraphBuilder.AllocObject<TArray<FInjectionDataArray, SceneRenderingAllocator>>())
{
	InjectionDataPerView.SetNum(Views.Num());
}

/**
 * Adds a light to LightInjectionData if it should be injected into the translucent volume, and caches relevant information in a FTranslucentLightInjectionData.
 * @param InProjectedShadowInfo is 0 for unshadowed lights
 */
void FTranslucentLightInjectionCollector::AddLightForInjection(
	const FViewInfo& View,
	const uint32 ViewIndex,
	TArrayView<const FVisibleLightInfo> VisibleLightInfos,
	const FLightSceneInfo& LightSceneInfo,
	const FProjectedShadowInfo* InProjectedShadowInfo)
{
	if (LightSceneInfo.Proxy->AffectsTranslucentLighting())
	{
		const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos[LightSceneInfo.Id];
		const ERHIFeatureLevel::Type FeatureLevel = View.FeatureLevel;

		const bool bApplyLightFunction = (View.Family->EngineShowFlags.LightFunctions &&
			LightSceneInfo.Proxy->GetLightFunctionMaterial() && 
			LightSceneInfo.Proxy->GetLightFunctionMaterial()->GetIncompleteMaterialWithFallback(FeatureLevel).IsLightFunction());

		const FMaterialRenderProxy* MaterialProxy = bApplyLightFunction ? 
			LightSceneInfo.Proxy->GetLightFunctionMaterial() : 
			UMaterial::GetDefaultMaterial(MD_LightFunction)->GetRenderProxy();

		// Skip rendering if the DefaultLightFunctionMaterial isn't compiled yet
		if (MaterialProxy->GetIncompleteMaterialWithFallback(FeatureLevel).IsLightFunction())
		{
			FTranslucentLightInjectionCollector::FInjectionData Data;
			Data.LightSceneInfo = &LightSceneInfo;
			Data.ProjectedShadowInfo = InProjectedShadowInfo;
			Data.bApplyLightFunction = bApplyLightFunction;
			Data.LightFunctionMaterialProxy = MaterialProxy;
			InjectionDataPerView[ViewIndex].Add(Data);
		}
	}
}

static FRDGTextureRef GetSkyTransmittanceLutTexture(FRDGBuilder& GraphBuilder, const FScene* Scene, const FViewInfo& View)
{
	FRDGTextureRef TransmittanceLutTexture = nullptr;
	if (ShouldRenderSkyAtmosphere(Scene, View.Family->EngineShowFlags))
	{
		if (const FSkyAtmosphereRenderSceneInfo* SkyInfo = Scene->GetSkyAtmosphereSceneInfo())
		{
			TransmittanceLutTexture = SkyInfo->GetTransmittanceLutTexture(GraphBuilder);
		}
	}
	return TransmittanceLutTexture;
}

BEGIN_SHADER_PARAMETER_STRUCT(FInjectTranslucentLightArrayParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FTranslucentLightingInjectPS::FParameters, PS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FVolumetricCloudShadowAOParameters, CloudShadowAO)
	RDG_TEXTURE_ACCESS(TransmittanceLutTexture, ERHIAccess::SRVGraphics)
	RDG_TEXTURE_ACCESS(ShadowDepthTexture, ERHIAccess::SRVGraphics)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/** Injects all the lights in LightInjectionData into the translucent lighting volume textures. */
void InjectTranslucencyLightingVolume(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const uint32 ViewIndex,
	const FScene* Scene,
	const FSceneRenderer& Renderer,
	const FTranslucencyLightingVolumeTextures& Textures,
	const FTranslucentLightInjectionCollector& Collector)
{
	if (!GUseTranslucentLightingVolumes || !GSupportsVolumeTextureRendering)
	{
		return;
	}

	const FTranslucentLightInjectionCollector::FInjectionDataArray& LightInjectionData = Collector.InjectionDataPerView[ViewIndex];

	SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);
	INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, LightInjectionData.Num());

	const FVolumetricCloudShadowAOParameters CloudShadowAOParameters = GetCloudShadowAOParameters(GraphBuilder, View, Scene->GetVolumetricCloudSceneInfo());
	const bool bUseLightFunctionAtlas = View.LightFunctionAtlasViewData.UsesLightFunctionAtlas(LightFunctionAtlas::ELightFunctionAtlasSystem::DeferredLighting);

	FRDGTextureRef TransmittanceLutTexture = GetSkyTransmittanceLutTexture(GraphBuilder, Scene, View);

	// Inject into each volume cascade. Operate on one cascade at a time to reduce render target switches.
	for (uint32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
	{
		// for stereo case, using PrimaryViewIndex essentially shares the lighting volume textures
		const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);
		FRDGTextureRef VolumeAmbientTexture = Textures.Ambient[TextureIndex];
		FRDGTextureRef VolumeDirectionalTexture = Textures.Directional[TextureIndex];

		for (int32 LightIndex = 0; LightIndex < LightInjectionData.Num(); LightIndex++)
		{
			const FTranslucentLightInjectionCollector::FInjectionData& InjectionData = LightInjectionData[LightIndex];
			const FLightSceneInfo* const LightSceneInfo = InjectionData.LightSceneInfo;
			const FVisibleLightInfo& VisibleLightInfo = Renderer.VisibleLightInfos[LightSceneInfo->Id];
			const bool bInverseSquared = LightSceneInfo->Proxy->IsInverseSquared();
			const bool bDirectionalLight = LightSceneInfo->Proxy->GetLightType() == LightType_Directional;
			bool bUseVSM = Renderer.VirtualShadowMapArray.IsAllocated();
			const bool bUseAdaptiveVolumetricShadowMap = LightSceneInfo->Proxy->CastsVolumetricShadow() && ShouldRenderHeterogeneousVolumes(Scene) && ShouldHeterogeneousVolumesCastShadows();

			const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightSceneInfo->Proxy->GetBoundingSphere(), View, VolumeCascadeIndex, bDirectionalLight);
			if (VolumeBounds.IsValid())
			{
				TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
				TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);

				FRDGTextureRef ShadowDepthTexture = nullptr;

				if (InjectionData.ProjectedShadowInfo)
				{
					ShadowDepthTexture = TryRegisterExternalTexture(GraphBuilder, InjectionData.ProjectedShadowInfo->RenderTargets.DepthTarget);
				}

				auto* PassParameters = GraphBuilder.AllocParameters< FInjectTranslucentLightArrayParameters >();
				PassParameters->TransmittanceLutTexture = TransmittanceLutTexture;
				PassParameters->ShadowDepthTexture = ShadowDepthTexture;
				PassParameters->CloudShadowAO = CloudShadowAOParameters;
				PassParameters->PS.VirtualShadowMapSamplingParameters = Renderer.VirtualShadowMapArray.GetSamplingParameters(GraphBuilder);
				PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
				PassParameters->RenderTargets[1] = FRenderTargetBinding(VolumeDirectionalTexture, ERenderTargetLoadAction::ELoad);

				PassParameters->PS.ViewUniformBuffer = View.ViewUniformBuffer;

				FDeferredLightUniformStruct* DeferredLightStruct = GraphBuilder.AllocParameters<FDeferredLightUniformStruct>();
				*DeferredLightStruct = GetDeferredLightParameters(View, *LightSceneInfo, bUseLightFunctionAtlas, ELightShaderParameterFlags::RectAsSpotLight);
				PassParameters->PS.DeferredLight = GraphBuilder.CreateUniformBuffer(DeferredLightStruct);

				GetVolumeShadowingShaderParameters(GraphBuilder, View, LightSceneInfo, InjectionData.ProjectedShadowInfo, PassParameters->PS.VolumeShadowingParameters);

				const int32 VirtualShadowMapId = bUseVSM ? Renderer.VisibleLightInfos[LightSceneInfo->Id].GetVirtualShadowMapId(&View) : INDEX_NONE;
				
				// Switch it back off if there's no ID to avoid the FVirtualShadowMap permutation if we don't need it
				bUseVSM = (VirtualShadowMapId != INDEX_NONE);

				PassParameters->PS.VirtualShadowMapId = VirtualShadowMapId;
				PassParameters->PS.LightFunctionParameters = FLightFunctionSharedParameters::GetLightFunctionSharedParameters(LightSceneInfo, 1.0f);
				PassParameters->PS.VolumeCascadeIndex = VolumeCascadeIndex;
				PassParameters->PS.AVSM = HeterogeneousVolumes::GetAdaptiveVolumetricShadowMapUniformBuffer(GraphBuilder, View.ViewState, LightSceneInfo);

				bool bIsSpotlight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
				PassParameters->PS.SpotlightMask = bIsSpotlight ? 1.0f : 0.0f; //@todo - needs to be a permutation to reduce shadow filtering work

				{
					const FVector Scale = LightSceneInfo->Proxy->GetLightFunctionScale();
					// Switch x and z so that z of the user specified scale affects the distance along the light direction
					const FVector InverseScale = FVector(1.f / Scale.Z, 1.f / Scale.Y, 1.f / Scale.X);
					const FMatrix WorldToLight = LightSceneInfo->Proxy->GetWorldToLight() * FScaleMatrix(InverseScale);
					const FMatrix TranslatedWorldToWorld = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());

					PassParameters->PS.LightFunctionTranslatedWorldToLight = FMatrix44f(TranslatedWorldToWorld * WorldToLight);
				}

				const bool bCloudShadowEnabled = SetupLightCloudTransmittanceParameters(GraphBuilder, Scene, View, LightSceneInfo, PassParameters->PS.LightCloudTransmittanceParameters);
				PassParameters->PS.VolumetricCloudShadowEnabled = bCloudShadowEnabled ? 1 : 0;

				PassParameters->PS.AtmospherePerPixelTransmittanceEnabled = IsLightAtmospherePerPixelTransmittanceEnabled(Scene, View, LightSceneInfo);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("InjectTranslucencyLightingVolume(VolumeCascade=%d%s%s%s)",
						VolumeCascadeIndex,
						VirtualShadowMapId != INDEX_NONE ? TEXT(",VirtualShadowMap") : TEXT(""),
						InjectionData.ProjectedShadowInfo != nullptr ? TEXT(",ShadowMap") : TEXT(""),
						InjectionData.bApplyLightFunction ? TEXT(",LightFunction") : TEXT("")),
					PassParameters,
					ERDGPassFlags::Raster,
					[PassParameters, VertexShader, GeometryShader, &View, &Renderer, &InjectionData, LightSceneInfo, bDirectionalLight, bUseVSM, VolumeBounds, VolumeCascadeIndex, bUseAdaptiveVolumetricShadowMap](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

					if (bDirectionalLight)
					{
						// Accumulate the contribution of multiple lights
						// Directional lights write their shadowing into alpha of the ambient texture
						GraphicsPSOInit.BlendState = TStaticBlendState<
							CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One,
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_One>::GetRHI();
					}
					else
					{
						// Accumulate the contribution of multiple lights
						GraphicsPSOInit.BlendState = TStaticBlendState<
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One,
							CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
					}

					const FMaterialRenderProxy* MaterialProxy = InjectionData.LightFunctionMaterialProxy;
					const FMaterial& Material = MaterialProxy->GetMaterialWithFallback( View.GetFeatureLevel(), MaterialProxy );
					const FMaterialShaderMap* MaterialShaderMap = Material.GetRenderingThreadShaderMap();

					FTranslucentLightingInjectPS::FPermutationDomain PermutationVector;
					PermutationVector.Set< FTranslucentLightingInjectPS::FRadialAttenuation >( !bDirectionalLight );
					PermutationVector.Set< FTranslucentLightingInjectPS::FDynamicallyShadowed >( InjectionData.ProjectedShadowInfo != nullptr );
					PermutationVector.Set< FTranslucentLightingInjectPS::FLightFunction >( InjectionData.bApplyLightFunction );
					PermutationVector.Set< FTranslucentLightingInjectPS::FVirtualShadowMap >( bUseVSM );
					PermutationVector.Set< FTranslucentLightingInjectPS::FAdaptiveVolumetricShadowMap >(bUseAdaptiveVolumetricShadowMap);

					auto PixelShader = MaterialShaderMap->GetShader< FTranslucentLightingInjectPS >( PermutationVector );
	
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

					SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
					if (GeometryShader.IsValid())
					{
						SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
					}

					SetShaderParametersLegacyPS(RHICmdList, PixelShader, View, InjectionData.LightFunctionMaterialProxy);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
				});
			}
		}
	}
}

class FSimpleLightTranslucentLightingInjectPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FSimpleLightTranslucentLightingInjectPS);
	SHADER_USE_PARAMETER_STRUCT(FSimpleLightTranslucentLightingInjectPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(FVector4f, SimpleLightPositionAndRadius)
		SHADER_PARAMETER(FVector4f, SimpleLightColorAndExponent)
		SHADER_PARAMETER(uint32, VolumeCascadeIndex)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) 
	{ 
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) && (RHISupportsGeometryShaders(Parameters.Platform) || RHISupportsVertexShaderLayer(Parameters.Platform));
	}
};

IMPLEMENT_GLOBAL_SHADER(FSimpleLightTranslucentLightingInjectPS, "/Engine/Private/TranslucentLightInjectionShaders.usf", "SimpleLightInjectMainPS", SF_Pixel);

void InjectSimpleTranslucencyLightingVolumeArray(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const uint32 ViewIndex,
	const uint32 ViewCount,
	const FTranslucencyLightingVolumeTextures& Textures,
	const FSimpleLightArray& SimpleLights)
{
	SCOPE_CYCLE_COUNTER(STAT_TranslucentInjectTime);

	int32 NumLightsToInject = 0;

	for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
	{
		if (SimpleLights.InstanceData[LightIndex].bAffectTranslucency)
		{
			NumLightsToInject++;
		}
	}

	if (NumLightsToInject > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "InjectSimpleTranslucentLightArray");

		INC_DWORD_STAT_BY(STAT_NumLightsInjectedIntoTranslucency, NumLightsToInject);

		const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();

		const float Exposure = View.GetLastEyeAdaptationExposure();

		// Inject into each volume cascade
		// Operate on one cascade at a time to reduce render target switches
		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);

			RDG_EVENT_SCOPE(GraphBuilder, "Cascade%d", VolumeCascadeIndex);
			FRDGTextureRef VolumeAmbientTexture = Textures.Ambient[TextureIndex];
			FRDGTextureRef VolumeDirectionalTexture = Textures.Directional[TextureIndex];

			for (int32 LightIndex = 0; LightIndex < SimpleLights.InstanceData.Num(); LightIndex++)
			{
				const FSimpleLightEntry& SimpleLight = SimpleLights.InstanceData[LightIndex];
				const FSimpleLightPerViewEntry& SimpleLightPerViewData = SimpleLights.GetViewDependentData(LightIndex, ViewIndex, ViewCount);

				if (SimpleLight.bAffectTranslucency)
				{
					const FSphere LightBounds(SimpleLightPerViewData.Position, SimpleLight.Radius);
					const FVolumeBounds VolumeBounds = CalculateLightVolumeBounds(LightBounds, View, VolumeCascadeIndex, false);

					if (VolumeBounds.IsValid())
					{
						const FVector3f TranslatedLightPosition = FVector3f(SimpleLightPerViewData.Position + View.ViewMatrices.GetPreViewTranslation());

						auto* PassParameters = GraphBuilder.AllocParameters<FSimpleLightTranslucentLightingInjectPS::FParameters>();
						PassParameters->View = View.ViewUniformBuffer;
						PassParameters->VolumeCascadeIndex = VolumeCascadeIndex;
						PassParameters->SimpleLightPositionAndRadius = FVector4f(TranslatedLightPosition, SimpleLight.Radius);
						PassParameters->SimpleLightColorAndExponent = FVector4f((FVector3f)SimpleLight.Color * FLightRenderParameters::GetLightExposureScale(Exposure, SimpleLight.InverseExposureBlend), SimpleLight.Exponent);
						PassParameters->RenderTargets[0] = FRenderTargetBinding(VolumeAmbientTexture, ERenderTargetLoadAction::ELoad);
						PassParameters->RenderTargets[1] = FRenderTargetBinding(VolumeDirectionalTexture, ERenderTargetLoadAction::ELoad);

						TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
						TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
						TShaderMapRef<FSimpleLightTranslucentLightingInjectPS> PixelShader(View.ShaderMap);

						GraphBuilder.AddPass(
							{},
							PassParameters,
							ERDGPassFlags::Raster,
							[VertexShader, GeometryShader, PixelShader, PassParameters, VolumeBounds, TranslucencyLightingVolumeDim](FRHICommandList& RHICmdList)
						{
							FGraphicsPipelineStateInitializer GraphicsPSOInit;
							RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

							GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
							GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
							// Accumulate the contribution of multiple lights
							GraphicsPSOInit.BlendState = TStaticBlendState<
								CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One,
								CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI();
							GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

							GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
							GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
							GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
							GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
							SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

							SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
							if (GeometryShader.IsValid())
							{
								SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
							}
							SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
							RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
						});
					}
				}
			}
		}
	}
}

void FilterTranslucencyLightingVolume(
	FRDGBuilder& GraphBuilder,
	const TArrayView<const FViewInfo> Views,
	FTranslucencyLightingVolumeTextures& Textures)
{
	if (!GUseTranslucentLightingVolumes || !GSupportsVolumeTextureRendering || !GUseTranslucencyVolumeBlur)
	{
		return;
	}

	FRHISamplerState* SamplerStateRHI = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const int32 TranslucencyLightingVolumeDim = GetTranslucencyLightingVolumeDim();
	RDG_EVENT_SCOPE(GraphBuilder, "FilterTranslucentVolume %dx%dx%d Cascades:%d", TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TranslucencyLightingVolumeDim, TVC_MAX);
	RDG_GPU_STAT_SCOPE(GraphBuilder, TranslucentLighting);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const FViewInfo& View = Views[ViewIndex];
		RDG_GPU_MASK_SCOPE(GraphBuilder, View.GPUMask);
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", ViewIndex);

		for (int32 VolumeCascadeIndex = 0; VolumeCascadeIndex < TVC_MAX; VolumeCascadeIndex++)
		{
			const uint32 TextureIndex = Textures.GetIndex(View, VolumeCascadeIndex);

			FRDGTextureRef InputVolumeAmbientTexture = Textures.Ambient[TextureIndex];
			FRDGTextureRef InputVolumeDirectionalTexture = Textures.Directional[TextureIndex];

			FRDGTextureRef OutputVolumeAmbientTexture = GraphBuilder.CreateTexture(InputVolumeAmbientTexture->Desc, InputVolumeAmbientTexture->Name);
			FRDGTextureRef OutputVolumeDirectionalTexture = GraphBuilder.CreateTexture(InputVolumeDirectionalTexture->Desc, InputVolumeDirectionalTexture->Name);

			Textures.Ambient[TextureIndex] = OutputVolumeAmbientTexture;
			Textures.Directional[TextureIndex] = OutputVolumeDirectionalTexture;

			auto* PassParameters = GraphBuilder.AllocParameters<FFilterTranslucentVolumePS::FParameters>();
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->TexelSize = 1.0f / TranslucencyLightingVolumeDim;
			PassParameters->TranslucencyLightingVolumeAmbient = InputVolumeAmbientTexture;
			PassParameters->TranslucencyLightingVolumeDirectional = InputVolumeDirectionalTexture;
			PassParameters->TranslucencyLightingVolumeAmbientSampler = SamplerStateRHI;
			PassParameters->TranslucencyLightingVolumeDirectionalSampler = SamplerStateRHI;
			PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputVolumeAmbientTexture, ERenderTargetLoadAction::ENoAction);
			PassParameters->RenderTargets[1] = FRenderTargetBinding(OutputVolumeDirectionalTexture, ERenderTargetLoadAction::ENoAction);

			const FVolumeBounds VolumeBounds(TranslucencyLightingVolumeDim);
			TShaderMapRef<FWriteToSliceVS> VertexShader(View.ShaderMap);
			TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(View.ShaderMap);
			TShaderMapRef<FFilterTranslucentVolumePS> PixelShader(View.ShaderMap);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Cascade%d", VolumeCascadeIndex),
				PassParameters,
				ERDGPassFlags::Raster,
				[VertexShader, GeometryShader, PixelShader, PassParameters, VolumeBounds, TranslucencyLightingVolumeDim](FRHICommandList& RHICmdList)
			{
				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.SetGeometryShader(GeometryShader.GetGeometryShader());
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

				SetShaderParametersLegacyVS(RHICmdList, VertexShader, VolumeBounds, FIntVector(TranslucencyLightingVolumeDim));
				if (GeometryShader.IsValid())
				{
					SetShaderParametersLegacyGS(RHICmdList, GeometryShader, VolumeBounds.MinZ);
				}
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
				RasterizeToVolumeTexture(RHICmdList, VolumeBounds);
			});
		}
	}
}
