// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Math/RandomStream.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectIterator.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "ShowFlags.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "Shader.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ShadowMap.h"
#include "LandscapeComponent.h"
#include "LandscapeVersion.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "Materials/Material.h"
#include "LandscapeGrassType.h"
#include "Materials/MaterialExpressionLandscapeGrassOutput.h"
#include "Engine/TextureRenderTarget2D.h"
#include "LandscapeDataAccess.h"
#include "StaticMeshResources.h"
#include "LandscapeLight.h"
#include "GrassInstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ShaderParameterUtils.h"
#include "EngineModule.h"
#include "LandscapeRender.h"
#include "MaterialCompiler.h"
#include "Algo/Accumulate.h"
#include "UObject/Package.h"
#include "Engine/StaticMesh.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/InstancedStaticMesh.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "Math/Halton.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "TextureCompiler.h"
#include "RenderCaptureInterface.h"
#include "SimpleMeshDrawCommandPass.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "MaterialCachedData.h"
#include "SceneRenderTargetParameters.h"

#define LOCTEXT_NAMESPACE "Landscape"

DEFINE_LOG_CATEGORY_STATIC(LogGrass, Log, All);

static float GGuardBandMultiplier = 1.3f;
static FAutoConsoleVariableRef CVarGuardBandMultiplier(
	TEXT("grass.GuardBandMultiplier"),
	GGuardBandMultiplier,
	TEXT("Used to control discarding in the grass system. Approximate range, 1-4. Multiplied by the cull distance to control when we add grass components."));

static float GGuardBandDiscardMultiplier = 1.4f;
static FAutoConsoleVariableRef CVarGuardBandDiscardMultiplier(
	TEXT("grass.GuardBandDiscardMultiplier"),
	GGuardBandDiscardMultiplier,
	TEXT("Used to control discarding in the grass system. Approximate range, 1-4. Multiplied by the cull distance to control when we discard grass components."));

static int32 GMinFramesToKeepGrass = 30;
static FAutoConsoleVariableRef CVarMinFramesToKeepGrass(
	TEXT("grass.MinFramesToKeepGrass"),
	GMinFramesToKeepGrass,
	TEXT("Minimum number of frames before cached grass can be discarded; used to prevent thrashing."));

static int32 GGrassTickInterval = 1;
static FAutoConsoleVariableRef CVarGrassTickInterval(
	TEXT("grass.TickInterval"),
	GGrassTickInterval,
	TEXT("Number of frames between grass ticks."));

static float GMinTimeToKeepGrass = 5.0f;
static FAutoConsoleVariableRef CVarMinTimeToKeepGrass(
	TEXT("grass.MinTimeToKeepGrass"),
	GMinTimeToKeepGrass,
	TEXT("Minimum number of seconds before cached grass can be discarded; used to prevent thrashing."));

static int32 GMaxInstancesPerComponent = 65536;
static FAutoConsoleVariableRef CVarMaxInstancesPerComponent(
	TEXT("grass.MaxInstancesPerComponent"),
	GMaxInstancesPerComponent,
	TEXT("Used to control the number of grass components created. More can be more efficient, but can be hitchy as new components come into range"));

static int32 GMaxAsyncTasks = 4;
static FAutoConsoleVariableRef CVarMaxAsyncTasks(
	TEXT("grass.MaxAsyncTasks"),
	GMaxAsyncTasks,
	TEXT("Used to control the number of grass components created at a time."));

static int32 GUseHaltonDistribution = 0;
static FAutoConsoleVariableRef CVarUseHaltonDistribution(
	TEXT("grass.UseHaltonDistribution"),
	GUseHaltonDistribution,
	TEXT("Used to control the distribution of grass instances. If non-zero, use a halton sequence."));

static float GGrassDensityScale = 1;
static FAutoConsoleVariableRef CVarGrassDensityScale(
	TEXT("grass.densityScale"),
	GGrassDensityScale,
	TEXT("Multiplier on all grass densities."),
	ECVF_Scalability);

static float GGrassCullDistanceScale = 1;
static FAutoConsoleVariableRef CVarGrassCullDistanceScale(
	TEXT("grass.CullDistanceScale"),
	GGrassCullDistanceScale,
	TEXT("Multiplier on all grass cull distances."),
	ECVF_Scalability);

static int32 GGrassEnable = 1;
static FAutoConsoleVariableRef CVarGrassEnable(
	TEXT("grass.Enable"),
	GGrassEnable,
	TEXT("1: Enable Grass; 0: Disable Grass"));

static int32 GGrassDiscardDataOnLoad = 0;
static FAutoConsoleVariableRef CVarGrassDiscardDataOnLoad(
	TEXT("grass.DiscardDataOnLoad"),
	GGrassDiscardDataOnLoad,
	TEXT("1: Discard grass data on load (disables grass); 0: Keep grass data (requires reloading level)"),
	ECVF_Scalability);

static int32 GCullSubsections = 1;
static FAutoConsoleVariableRef CVarCullSubsections(
	TEXT("grass.CullSubsections"),
	GCullSubsections,
	TEXT("1: Cull each foliage component; 0: Cull only based on the landscape component."));

static int32 GDisableGPUCull = 0;
static FAutoConsoleVariableRef CVarDisableGPUCull(
	TEXT("grass.DisableGPUCull"),
	GDisableGPUCull,
	TEXT("For debugging. Set this to zero to see where the grass is generated. Useful for tweaking the guard bands."));

static int32 GPrerenderGrassmaps = 1;
static FAutoConsoleVariableRef CVarPrerenderGrassmaps(
	TEXT("grass.PrerenderGrassmaps"),
	GPrerenderGrassmaps,
	TEXT("1: Pre-render grass maps for all components in the editor; 0: Generate grass maps on demand while moving through the editor"));

static int32 GDisableDynamicShadows = 0;
static FAutoConsoleVariableRef CVarDisableDynamicShadows(
	TEXT("grass.DisableDynamicShadows"),
	GDisableDynamicShadows,
	TEXT("0: Dynamic shadows from grass follow the grass type bCastDynamicShadow flag; 1: Dynamic shadows are disabled for all grass"));

static int32 GIgnoreExcludeBoxes = 0;
static FAutoConsoleVariableRef CVarIgnoreExcludeBoxes(
	TEXT("grass.IgnoreExcludeBoxes"),
	GIgnoreExcludeBoxes,
	TEXT("For debugging. Ignores any exclusion boxes."));

static int32 GGrassMaxCreatePerFrame = 1;
static FAutoConsoleVariableRef CVarGrassMaxCreatePerFrame(
	TEXT("grass.MaxCreatePerFrame"),
	GGrassMaxCreatePerFrame,
	TEXT("Maximum number of Grass components to create per frame"));

static int32 GGrassUpdateAllOnRebuild = 0;
static FAutoConsoleVariableRef CVarUpdateAllOnRebuild(
	TEXT("grass.UpdateAllOnRebuild"),
	GGrassUpdateAllOnRebuild,
	TEXT(""));


static int32 GCaptureNextGrassUpdate = 0;
static FAutoConsoleVariableRef CVarCaptureNextGrassUpdate(
	TEXT("grass.CaptureNextGrassUpdate"),
	GCaptureNextGrassUpdate,
	TEXT("Trigger a renderdoc capture for the next X grass updates (calls to RenderGrassMap or RenderGrassMaps"));

#if RHI_RAYTRACING
static TAutoConsoleVariable<int32> CVarRayTracingLandscapeGrass(
	TEXT("r.RayTracing.Geometry.LandscapeGrass"),
	0,
	TEXT("Include landscapes grass in ray tracing effects (default = 1)"));
#endif

DECLARE_CYCLE_STAT(TEXT("Grass Async Build Time"), STAT_FoliageGrassAsyncBuildTime, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Grass Start Comp"), STAT_FoliageGrassStartComp, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Grass End Comp"), STAT_FoliageGrassEndComp, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Grass Destroy Comps"), STAT_FoliageGrassDestoryComp, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Grass Update"), STAT_GrassUpdate, STATGROUP_Foliage);

int32 ALandscapeProxy::GrassUpdateInterval = 1;

static void GrassCVarSinkFunction()
{
	static float CachedGrassDensityScale = 1.0f;
	float GrassDensityScale = GGrassDensityScale;

	if (FApp::IsGame())
	{
		ALandscapeProxy::SetGrassUpdateInterval(FMath::Clamp<int32>(GGrassTickInterval, 1, 60));
	}

	static float CachedGrassCullDistanceScale = 1.0f;
	float GrassCullDistanceScale = GGrassCullDistanceScale;

	static const IConsoleVariable* DetailModeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DetailMode"));
	static int32 CachedDetailMode = DetailModeCVar ? DetailModeCVar->GetInt() : 0;
	int32 DetailMode = DetailModeCVar ? DetailModeCVar->GetInt() : 0;

	if (DetailMode != CachedDetailMode || 
		GrassDensityScale != CachedGrassDensityScale || 
		GrassCullDistanceScale != CachedGrassCullDistanceScale)
	{
		CachedGrassDensityScale = GrassDensityScale;
		CachedGrassCullDistanceScale = GrassCullDistanceScale;
		CachedDetailMode = DetailMode;

		for (auto* Landscape : TObjectRange<ALandscapeProxy>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
		{
			Landscape->FlushGrassComponents(nullptr, false);
		}
	}
}

static FAutoConsoleVariableSink CVarGrassSink(FConsoleCommandDelegate::CreateStatic(&GrassCVarSinkFunction));

//
// Grass weightmap rendering
//

#if WITH_EDITOR
static bool ShouldCacheLandscapeGrassShaders(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	// We only need grass weight shaders for Landscape vertex factories on desktop platforms
	return (Parameters.MaterialParameters.bIsUsedWithLandscape || Parameters.MaterialParameters.bIsSpecialEngineMaterial) &&
		IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
		Parameters.VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLandscapeFixedGridVertexFactory"), FNAME_Find)) &&
		EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);
}

class FLandscapeGrassWeightShaderElementData : public FMeshMaterialShaderElementData
{
public:

	int32 OutputPass;
	FVector2f RenderOffset;
};

class FLandscapeGrassWeightVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLandscapeGrassWeightVS, MeshMaterial);

	LAYOUT_FIELD(FShaderParameter, RenderOffsetParameter);

protected:

	FLandscapeGrassWeightVS()
	{}

	FLandscapeGrassWeightVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer)
	: FMeshMaterialShader(Initializer)
	{
		RenderOffsetParameter.Bind(Initializer.ParameterMap, TEXT("RenderOffset"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return ShouldCacheLandscapeGrassShaders(Parameters);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(RenderOffsetParameter, ShaderElementData.RenderOffset);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLandscapeGrassWeightVS, TEXT("/Engine/Private/LandscapeGrassWeight.usf"), TEXT("VSMain"), SF_Vertex);

class FLandscapeGrassWeightPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FLandscapeGrassWeightPS, MeshMaterial);
	LAYOUT_FIELD(FShaderParameter, OutputPassParameter);
public:

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return ShouldCacheLandscapeGrassShaders(Parameters);
	}

	FLandscapeGrassWeightPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FMeshMaterialShader(Initializer)
	{
		OutputPassParameter.Bind(Initializer.ParameterMap, TEXT("OutputPass"));
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::StaticStructMetadata.GetShaderVariableName());
	}

	FLandscapeGrassWeightPS()
	{}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshPassProcessorRenderState& DrawRenderState,
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, DrawRenderState, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(OutputPassParameter, ShaderElementData.OutputPass);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLandscapeGrassWeightPS, TEXT("/Engine/Private/LandscapeGrassWeight.usf"), TEXT("PSMain"), SF_Pixel);

class FLandscapeGrassWeightMeshProcessor : public FMeshPassProcessor
{
public:
	FLandscapeGrassWeightMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex, 
		const TArray<int32>& HeightMips,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy);

	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) override final
	{
		checkf(false, TEXT("Default AddMeshBatch can't be used as rendering requires extra parameters per pass."));
	}


private:
	bool TryAddMeshBatch(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		int32 StaticMeshId,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& MaterialResource,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex,
		const TArray<int32>& HeightMips);

	bool Process(
		const FMeshBatch& MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		int32 NumPasses,
		FVector2D ViewOffset,
		float PassOffsetX,
		int32 FirstHeightMipsPassIndex,
		const TArray<int32>& HeightMips);

	FMeshPassProcessorRenderState PassDrawRenderState;
};

FLandscapeGrassWeightMeshProcessor::FLandscapeGrassWeightMeshProcessor(const FScene* Scene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::Num, Scene, InFeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());
}

void FLandscapeGrassWeightMeshProcessor::AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex, 
	const TArray<int32>& HeightMips, 
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material)
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, -1, *MaterialRenderProxy, *Material, NumPasses, ViewOffset, PassOffsetX, FirstHeightMipsPassIndex, HeightMips))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}
}

bool FLandscapeGrassWeightMeshProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& MaterialResource,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex,
	const TArray<int32>& HeightMips)
{
	check(MeshBatch.VertexFactory != nullptr);
	return Process(MeshBatch, BatchElementMask, PrimitiveSceneProxy, MaterialRenderProxy, MaterialResource, NumPasses, ViewOffset, PassOffsetX, FirstHeightMipsPassIndex, HeightMips);
}

bool FLandscapeGrassWeightMeshProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	int32 NumPasses,
	FVector2D ViewOffset,
	float PassOffsetX,
	int32 FirstHeightMipsPassIndex,
	const TArray<int32>& HeightMips)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FLandscapeGrassWeightVS,
		FLandscapeGrassWeightPS> PassShaders;

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	ShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	Shaders.TryGetVertexShader(PassShaders.VertexShader);
	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(MaterialResource, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = CM_None;

	FLandscapeGrassWeightShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, -1, true);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	for (int32 PassIndex = 0; PassIndex < NumPasses; ++PassIndex)
	{
		ShaderElementData.OutputPass = (PassIndex >= FirstHeightMipsPassIndex) ? 0 : PassIndex;
		ShaderElementData.RenderOffset = FVector2f(ViewOffset) + FVector2f(PassOffsetX * PassIndex, 0);	// LWC_TODO: Precision loss

		uint64 Mask = (PassIndex >= FirstHeightMipsPassIndex) ? HeightMips[PassIndex - FirstHeightMipsPassIndex] : BatchElementMask;

		BuildMeshDrawCommands(
			MeshBatch,
			Mask,
			PrimitiveSceneProxy,
			MaterialRenderProxy,
			MaterialResource,
			PassDrawRenderState,
			PassShaders,
			MeshFillMode,
			MeshCullMode,
			SortKey,
			EMeshPassFeatures::Default,
			ShaderElementData);
	}

	return true;
}

// data also accessible by render thread
class FLandscapeGrassWeightExporter_RenderThread
{
	FSceneInterface* SceneInterface = nullptr;

	FLandscapeGrassWeightExporter_RenderThread(int32 InNumGrassMaps, bool InbNeedsHeightmap, TArray<int32> InHeightMips)
		: RenderTargetResource(nullptr)
		, NumPasses(0)
		, HeightMips(MoveTemp(InHeightMips))
		, FirstHeightMipsPassIndex(MAX_int32)
	{
		if (InbNeedsHeightmap || InNumGrassMaps > 0)
		{
			NumPasses += FMath::DivideAndRoundUp(2 /* heightmap */ + InNumGrassMaps, 4);
		}
		if (HeightMips.Num() > 0)
		{
			FirstHeightMipsPassIndex = NumPasses;
			NumPasses += HeightMips.Num();
		}
	}

	friend class FLandscapeGrassWeightExporter;

public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread()
	{}

	struct FComponentInfo
	{
		ULandscapeComponent* Component;
		FVector2D ViewOffset;
		int32 PixelOffsetX;
		FLandscapeComponentSceneProxy* SceneProxy;

		FComponentInfo(ULandscapeComponent* InComponent, FVector2D& InViewOffset, int32 InPixelOffsetX)
			: Component(InComponent)
			, ViewOffset(InViewOffset)
			, PixelOffsetX(InPixelOffsetX)
			, SceneProxy((FLandscapeComponentSceneProxy*)InComponent->SceneProxy)
		{}
	};

	FTextureRenderTarget2DResource* RenderTargetResource;
	TArray<FComponentInfo, TInlineAllocator<1>> ComponentInfos;
	FIntPoint TargetSize;
	int32 NumPasses;
	TArray<int32> HeightMips;
	int32 FirstHeightMipsPassIndex;
	float PassOffsetX;
	FVector ViewOrigin;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;

	BEGIN_SHADER_PARAMETER_STRUCT(FLandscapeGrassPassParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	void RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, SceneInterface, FEngineShowFlags(ESFIM_Game))
			.SetTime(FGameTime::GetTimeSinceAppStart()));

		ViewFamily.LandscapeLODOverride = 0; // Force LOD render

		// Ensure scene primitive rendering is valid (added primitives comitted, GPU-Scene updated, push/pop dynamic culling context).
		FScenePrimitiveRenderingContextScopeHelper ScenePrimitiveRenderingContextScopeHelper(GetRendererModule().BeginScenePrimitiveRendering(GraphBuilder, &ViewFamily));
		
		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TargetSize.X, TargetSize.Y));
		ViewInitOptions.ViewOrigin = ViewOrigin;
		ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		ViewInitOptions.ViewFamily = &ViewFamily;

		GetRendererModule().CreateAndInitSingleView(RHICmdList, &ViewFamily, &ViewInitOptions);
		
		const FSceneView* View = ViewFamily.Views[0];

		FRDGTextureRef OutputTexture = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(RenderTargetResource->GetTextureRHI(), TEXT("LandscapeGrass")));

		auto* PassParameters = GraphBuilder.AllocParameters<FLandscapeGrassPassParameters>();
		PassParameters->View = View->ViewUniformBuffer;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(OutputTexture, ERenderTargetLoadAction::EClear);

		AddSimpleMeshPass(GraphBuilder, PassParameters, SceneInterface->GetRenderScene(), *View, nullptr, RDG_EVENT_NAME("LandscapeGrass"), View->UnscaledViewRect,
			[&View, this](FDynamicPassMeshDrawListContext* DynamicMeshPassContext)
			{
				FLandscapeGrassWeightMeshProcessor PassMeshProcessor(
					nullptr,
					View->GetFeatureLevel(),
					View,
					DynamicMeshPassContext);

				const uint64 DefaultBatchElementMask = 1 << 0; // LOD 0 only

				for (auto& ComponentInfo : ComponentInfos)
				{
					if (ensure(ComponentInfo.SceneProxy))
					{
						const FMeshBatch& Mesh = ComponentInfo.SceneProxy->GetGrassMeshBatch();
						Mesh.MaterialRenderProxy->UpdateUniformExpressionCacheIfNeeded(View->GetFeatureLevel());

						PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, NumPasses, ComponentInfo.ViewOffset, PassOffsetX, FirstHeightMipsPassIndex, HeightMips, ComponentInfo.SceneProxy);
					}
				}
			});

		GraphBuilder.Execute();
	}
};

class FLandscapeGrassWeightExporter : public FLandscapeGrassWeightExporter_RenderThread
{
	ALandscapeProxy* LandscapeProxy;
	int32 ComponentSizeVerts;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	TArray<ULandscapeGrassType*> GrassTypes;
	UTextureRenderTarget2D* RenderTargetTexture;

public:
	FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, const TArray<ULandscapeComponent*>& InLandscapeComponents, TArray<ULandscapeGrassType*> InGrassTypes, bool InbNeedsHeightmap = true, TArray<int32> InHeightMips = {})
		: FLandscapeGrassWeightExporter_RenderThread(
			InGrassTypes.Num(),
			InbNeedsHeightmap,
			MoveTemp(InHeightMips))
		, LandscapeProxy(InLandscapeProxy)
		, ComponentSizeVerts(InLandscapeProxy->ComponentSizeQuads + 1)
		, SubsectionSizeQuads(InLandscapeProxy->SubsectionSizeQuads)
		, NumSubsections(InLandscapeProxy->NumSubsections)
		, GrassTypes(MoveTemp(InGrassTypes))
		, RenderTargetTexture(nullptr)
	{
		check(InLandscapeComponents.Num() > 0);
		SceneInterface = InLandscapeComponents[0]->GetScene();

		// todo: use a 2d target?
		TargetSize = FIntPoint(ComponentSizeVerts * NumPasses * InLandscapeComponents.Num(), ComponentSizeVerts);
		FIntPoint TargetSizeMinusOne(TargetSize - FIntPoint(1, 1));
		PassOffsetX = 2.0f * (float)ComponentSizeVerts / (float)TargetSize.X;

		for (int32 Idx = 0; Idx < InLandscapeComponents.Num(); Idx++)
		{
			ULandscapeComponent* Component = InLandscapeComponents[Idx];

			FIntPoint ComponentOffset = (Component->GetSectionBase() - LandscapeProxy->LandscapeSectionOffset);
			int32 PixelOffsetX = Idx * NumPasses * ComponentSizeVerts;

			FVector2D ViewOffset(-ComponentOffset.X, ComponentOffset.Y);
			ViewOffset.X += PixelOffsetX;
			ViewOffset /= (FVector2D(TargetSize) * 0.5f);

			ensure(Component->SceneProxy);
			ComponentInfos.Add(FComponentInfo(Component, ViewOffset, PixelOffsetX));
		}

		// center of target area in world
		FVector TargetCenter = LandscapeProxy->GetTransform().TransformPosition(FVector(TargetSizeMinusOne, 0.f)*0.5f);

		// extent of target in world space
		FVector TargetExtent = FVector(TargetSize, 0.0f)*LandscapeProxy->GetActorScale()*0.5f;

		ViewOrigin = TargetCenter;
		ViewRotationMatrix = FInverseRotationMatrix(LandscapeProxy->GetActorRotation());
		ViewRotationMatrix *= FMatrix(FPlane(1, 0, 0, 0),
		                              FPlane(0,-1, 0, 0),
		                              FPlane(0, 0,-1, 0),
		                              FPlane(0, 0, 0, 1));

		const FMatrix::FReal ZOffset = UE_OLD_WORLD_MAX;
		ProjectionMatrix = FReversedZOrthoMatrix(
			TargetExtent.X,
			TargetExtent.Y,
			0.5f / ZOffset,
			ZOffset);

		RenderTargetTexture = NewObject<UTextureRenderTarget2D>();
		check(RenderTargetTexture);
		RenderTargetTexture->ClearColor = FLinearColor::White;
		RenderTargetTexture->TargetGamma = 1.0f;
		const bool bForceLinearGamma = true;
		RenderTargetTexture->InitCustomFormat(TargetSize.X, TargetSize.Y, PF_B8G8R8A8, bForceLinearGamma);
		RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource()->GetTextureRenderTarget2DResource();

		// render
		FLandscapeGrassWeightExporter_RenderThread* Exporter = this;
		ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
			[Exporter](FRHICommandListImmediate& RHICmdList)
			{
				Exporter->RenderLandscapeComponentToTexture_RenderThread(RHICmdList);
				FlushPendingDeleteRHIResources_RenderThread();
			});
	}

	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>>
		FetchResults()
	{
		TArray<FColor> Samples;
		Samples.SetNumUninitialized(TargetSize.X*TargetSize.Y);

		// Copy the contents of the remote texture to system memory
		FReadSurfaceDataFlags ReadSurfaceDataFlags;
		ReadSurfaceDataFlags.SetLinearToGamma(false);
		RenderTargetResource->ReadPixels(Samples, ReadSurfaceDataFlags, FIntRect(0, 0, TargetSize.X, TargetSize.Y));

		TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> Results;
		Results.Reserve(ComponentInfos.Num());
		
		// Local data will be moved in contiguous array at the end of export (to minimize slack waste)
		TArray<uint16> HeightData;
		TMap<ULandscapeGrassType*, TArray<uint8>> WeightData;

		for (auto& ComponentInfo : ComponentInfos)
		{
			ULandscapeComponent* Component = ComponentInfo.Component;
			ALandscapeProxy* Proxy = Component->GetLandscapeProxy();

			TUniquePtr<FLandscapeComponentGrassData> NewGrassData = MakeUnique<FLandscapeComponentGrassData>(Component);

			if (FirstHeightMipsPassIndex > 0)
			{
				HeightData.Empty(FMath::Square(ComponentSizeVerts));
			}
			else
			{
				HeightData.Empty(0);
			}
			NewGrassData->HeightMipData.Empty(HeightMips.Num());

			WeightData.Empty();
			TArray<TArray<uint8>*> GrassWeightArrays;
			GrassWeightArrays.Empty(GrassTypes.Num());
			for (auto GrassType : GrassTypes)
			{
				WeightData.Add(GrassType);
			}

			// need a second loop because the WeightData map will reallocate its arrays as grass types are added
			for (auto GrassType : GrassTypes)
			{
				TArray<uint8>* DataArray = WeightData.Find(GrassType);
				check(DataArray);
				DataArray->Empty(FMath::Square(ComponentSizeVerts));
				GrassWeightArrays.Add(DataArray);
			}

			// output debug bitmap
#if UE_BUILD_DEBUG
			static bool bOutputGrassBitmap = false;
			if (bOutputGrassBitmap)
			{
				FString TempPath = FPaths::ScreenShotDir();
				TempPath += TEXT("/GrassDebug");
				IFileManager::Get().MakeDirectory(*TempPath, true);
				FFileHelper::CreateBitmap(*(TempPath / "Grass"), TargetSize.X, TargetSize.Y, Samples.GetData(), nullptr, &IFileManager::Get(), nullptr, GrassTypes.Num() >= 2);
			}
#endif

			for (int32 PassIdx = 0; PassIdx < NumPasses; PassIdx++)
			{
				FColor* SampleData = &Samples[ComponentInfo.PixelOffsetX + PassIdx*ComponentSizeVerts];
				if (PassIdx < FirstHeightMipsPassIndex)
				{
					if (PassIdx == 0)
					{
						for (int32 y = 0; y < ComponentSizeVerts; y++)
						{
							for (int32 x = 0; x < ComponentSizeVerts; x++)
							{
								FColor& Sample = SampleData[x + y * TargetSize.X];
								uint16 Height = (((uint16)Sample.R) << 8) + (uint16)(Sample.G);
								HeightData.Add(Height);
								if (GrassTypes.Num() > 0)
								{
									GrassWeightArrays[0]->Add(Sample.B);
									if (GrassTypes.Num() > 1)
									{
										GrassWeightArrays[1]->Add(Sample.A);
									}
								}
							}
						}
					}
					else
					{
						for (int32 y = 0; y < ComponentSizeVerts; y++)
						{
							for (int32 x = 0; x < ComponentSizeVerts; x++)
							{
								FColor& Sample = SampleData[x + y * TargetSize.X];

								int32 TypeIdx = PassIdx * 4 - 2;
								GrassWeightArrays[TypeIdx++]->Add(Sample.R);
								if (TypeIdx < GrassTypes.Num())
								{
									GrassWeightArrays[TypeIdx++]->Add(Sample.G);
									if (TypeIdx < GrassTypes.Num())
									{
										GrassWeightArrays[TypeIdx++]->Add(Sample.B);
										if (TypeIdx < GrassTypes.Num())
										{
											GrassWeightArrays[TypeIdx++]->Add(Sample.A);
										}
									}
								}
							}
						}
					}
				}
				else // PassIdx >= FirstHeightMipsPassIndex
				{
					const int32 Mip = HeightMips[PassIdx - FirstHeightMipsPassIndex];
					int32 MipSizeVerts = NumSubsections * (SubsectionSizeQuads >> Mip);
					TArray<uint16>& MipHeightData = NewGrassData->HeightMipData.Add(Mip);
					for (int32 y = 0; y < MipSizeVerts; y++)
					{
						for (int32 x = 0; x < MipSizeVerts; x++)
						{
							FColor& Sample = SampleData[x + y * TargetSize.X];
							uint16 Height = (((uint16)Sample.R) << 8) + (uint16)(Sample.G);
							MipHeightData.Add(Height);
						}
					}
				}
			}

			// remove null grass type if we had one (can occur if the node has null entries)
			WeightData.Remove(nullptr);

			// Remove any grass data that is entirely weight 0
			for (auto Iter(WeightData.CreateIterator()); Iter; ++Iter)
			{
				if (Iter->Value.IndexOfByPredicate([&](const int8& Weight) { return Weight != 0; }) == INDEX_NONE)
				{
					Iter.RemoveCurrent();
				}
			}

			NewGrassData->InitializeFrom(HeightData, WeightData);
			Results.Add(Component, MoveTemp(NewGrassData));
		}

		return Results;
	}

	void ApplyResults()
	{
		TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> NewGrassData = FetchResults();

		for (auto&& GrassDataPair : NewGrassData)
		{
			ULandscapeComponent* Component = GrassDataPair.Key;
			FLandscapeComponentGrassData* ComponentGrassData = GrassDataPair.Value.Release();
			ALandscapeProxy* Proxy = Component->GetLandscapeProxy();

			// Assign the new data (thread-safe)
			Component->GrassData = MakeShareable(ComponentGrassData);
			Component->GrassData->bIsDirty = true;

			if (Proxy->bBakeMaterialPositionOffsetIntoCollision)
			{
				Component->DestroyCollisionData();
				Component->UpdateCollisionData();
			}
		}
	}

	void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
	{
		if (RenderTargetTexture)
		{
			Collector.AddReferencedObject(RenderTargetTexture);
		}

		if (LandscapeProxy)
		{
			Collector.AddReferencedObject(LandscapeProxy);
		}

		for (auto& Info : ComponentInfos)
		{
			if (Info.Component)
			{
				Collector.AddReferencedObject(Info.Component);
			}
		}

		for (auto GrassType : GrassTypes)
		{
			if (GrassType)
			{
				Collector.AddReferencedObject(GrassType);
			}
		}
	}
};

FLandscapeComponentGrassData::FLandscapeComponentGrassData(ULandscapeComponent* Component)
	: RotationForWPO(Component->GetLandscapeMaterial()->GetMaterial()->IsPropertyConnected(MP_WorldPositionOffset) ? Component->GetComponentTransform().GetRotation() : FQuat(0, 0, 0, 0))
	, bIsDirty(false)
{
	UMaterialInterface* Material = Component->GetLandscapeMaterial();
	for (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Material); MIC; MIC = Cast<UMaterialInstanceConstant>(Material))
	{
		MaterialStateIds.Add(MIC->ParameterStateId);
		Material = MIC->Parent;
	}
	MaterialStateIds.Add(CastChecked<UMaterial>(Material)->StateId);
}

bool ULandscapeComponent::MaterialHasGrass() const
{
	UMaterialInterface* Material = GetLandscapeMaterial();
	TArray<const UMaterialExpressionLandscapeGrassOutput*> GrassExpressions;
	Material->GetMaterial()->GetAllExpressionsOfType<UMaterialExpressionLandscapeGrassOutput>(GrassExpressions);
	if (GrassExpressions.Num() > 0 &&
		GrassExpressions[0]->GrassTypes.Num() > 0)
	{
		return GrassExpressions[0]->GrassTypes.ContainsByPredicate([](FGrassInput& GrassInput) { return (GrassInput.Input.IsConnected() && GrassInput.GrassType); });
	}

	return false;
}

bool ULandscapeComponent::IsGrassMapOutdated() const
{
	if (GrassData->HasValidData())
	{
		// check material / instances haven't changed
		const auto& MaterialStateIds = GrassData->MaterialStateIds;
		UMaterialInterface* Material = GetLandscapeMaterial();
		int32 TestIndex = 0;
		for (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Material); MIC; MIC = Cast<UMaterialInstanceConstant>(Material))
		{
			if (!MaterialStateIds.IsValidIndex(TestIndex) || MaterialStateIds[TestIndex] != MIC->ParameterStateId)
			{
				return true;
			}
			Material = MIC->Parent;
			++TestIndex;
		}

		UMaterial* MaterialBase = Cast<UMaterial>(Material);

		// last one should be a UMaterial
		if (TestIndex != MaterialStateIds.Num() - 1 || (MaterialBase != nullptr && MaterialStateIds[TestIndex] != MaterialBase->StateId))
		{
			return true;
		}

		FQuat RotationForWPO = GetLandscapeMaterial()->GetMaterial()->IsPropertyConnected(MP_WorldPositionOffset) ? GetComponentTransform().GetRotation() : FQuat(0, 0, 0, 0);
		if (GrassData->RotationForWPO != RotationForWPO)
		{
			return true;
		}
	}
	return false;
}

bool ULandscapeComponent::CanRenderGrassMap() const
{
	// Check we can render
	UWorld* ComponentWorld = GetWorld();
	if (!GIsEditor || GUsingNullRHI || !ComponentWorld || ComponentWorld->IsGameWorld() || ComponentWorld->FeatureLevel < ERHIFeatureLevel::SM5 || !SceneProxy)
	{
		return false;
	}

	UMaterialInstance* MaterialInstance = GetMaterialInstanceCount(false) > 0 ? GetMaterialInstance(0) : nullptr;
	FMaterialResource* MaterialResource = MaterialInstance != nullptr ? MaterialInstance->GetMaterialResource(ComponentWorld->FeatureLevel) : nullptr;

	// Check we can render the material
	if (MaterialResource == nullptr)
	{
		return false;
	}

	// We only need the GrassWeight shaders on the fixed grid vertex factory to render grass maps : 
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	ShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();

	FVertexFactoryType* LandscapeGrassVF = FindVertexFactoryType(FName(TEXT("FLandscapeFixedGridVertexFactory"), FNAME_Find));
	if (!MaterialResource->HasShaders(ShaderTypes, LandscapeGrassVF))
	{
		return false;
	}

	return true;
}

static bool IsTextureStreamedForGrassMapRender(UTexture2D* InTexture)
{
	return InTexture && !InTexture->IsDefaultTexture() && !InTexture->HasPendingInitOrStreaming() && InTexture->IsFullyStreamedIn();
}

bool ULandscapeComponent::AreTexturesStreamedForGrassMapRender() const
{
	// Check for valid heightmap that is fully streamed in
	if (!IsTextureStreamedForGrassMapRender(HeightmapTexture))
	{
		return false;
	}
	
	// Check for valid weightmaps that is fully streamed in
	for (auto WeightmapTexture : WeightmapTextures)
	{
		if (!IsTextureStreamedForGrassMapRender(WeightmapTexture))
		{
			return false;
		}
	}

	return true;
}

void ULandscapeComponent::RenderGrassMap()
{
	UMaterialInterface* Material = GetLandscapeMaterial();
	if (ensure(CanRenderGrassMap()))
	{
		TArray<ULandscapeGrassType*> GrassTypes;

		TArray<const UMaterialExpressionLandscapeGrassOutput*> GrassExpressions;
		Material->GetMaterial()->GetAllExpressionsOfType<UMaterialExpressionLandscapeGrassOutput>(GrassExpressions);
		if (GrassExpressions.Num() > 0)
		{
			GrassTypes.Empty(GrassExpressions[0]->GrassTypes.Num());
			for (auto& GrassTypeInput : GrassExpressions[0]->GrassTypes)
			{
				GrassTypes.Add(GrassTypeInput.GrassType);
			}
		}

		const bool bBakeMaterialPositionOffsetIntoCollision = (GetLandscapeProxy() && GetLandscapeProxy()->bBakeMaterialPositionOffsetIntoCollision);

		TArray<int32> HeightMips;
		if (bBakeMaterialPositionOffsetIntoCollision)
		{
			if (CollisionMipLevel > 0)
			{
				HeightMips.Add(CollisionMipLevel);
			}
			if (SimpleCollisionMipLevel > CollisionMipLevel)
			{
				HeightMips.Add(SimpleCollisionMipLevel);
			}
		}

		if (GrassTypes.Num() > 0 || bBakeMaterialPositionOffsetIntoCollision)
		{
			TArray<ULandscapeComponent*> LandscapeComponents;
			LandscapeComponents.Add(this);

			FLandscapeGrassWeightExporter Exporter(GetLandscapeProxy(), MoveTemp(LandscapeComponents), MoveTemp(GrassTypes), true, MoveTemp(HeightMips));
			Exporter.ApplyResults();
		}
	}
}

TArray<uint16> ULandscapeComponent::RenderWPOHeightmap(int32 LOD)
{
	TArray<uint16> Results;

	if (!CanRenderGrassMap())
	{
		GetMaterialInstance(0)->GetMaterialResource(GetWorld()->FeatureLevel)->FinishCompilation();
	}

	if (ensure(SceneProxy))
	{
		TArray<ULandscapeGrassType*> GrassTypes;
		TArray<ULandscapeComponent*> LandscapeComponents;
		LandscapeComponents.Add(this);

		if (LOD == 0)
		{
			FLandscapeGrassWeightExporter Exporter(GetLandscapeProxy(), MoveTemp(LandscapeComponents), MoveTemp(GrassTypes), true, {});
			TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> TempGrassData;
			TempGrassData = Exporter.FetchResults();
			Results = TArray<uint16>(TempGrassData[this]->GetHeightData());
		}
		else
		{
			TArray<int32> HeightMips;
			HeightMips.Add(LOD);
			FLandscapeGrassWeightExporter Exporter(GetLandscapeProxy(), MoveTemp(LandscapeComponents), MoveTemp(GrassTypes), false, MoveTemp(HeightMips));
			TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> TempGrassData;
			TempGrassData = Exporter.FetchResults();
			Results = MoveTemp(TempGrassData[this]->HeightMipData[LOD]);
		}
	}

	return Results;
}

void ULandscapeComponent::RemoveGrassMap()
{
	*GrassData = FLandscapeComponentGrassData();

	GrassData->bIsDirty = true;
	if (!MaterialHasGrass())
	{
		// Mark grass data as valid but empty if it just doesn't support grass because nothing will ever trigger a RenderGrassMaps on it, which would leave NumElements unset (which is considered as invalid data) :
		GrassData->NumElements = 0;
	}
}

void ALandscapeProxy::RenderGrassMaps(const TArray<ULandscapeComponent*>& InLandscapeComponents, const TArray<ULandscapeGrassType*>& GrassTypes)
{
	TArray<int32> HeightMips;
	if (CollisionMipLevel > 0)
	{
		HeightMips.Add(CollisionMipLevel);
	}
	if (SimpleCollisionMipLevel > CollisionMipLevel)
	{
		HeightMips.Add(SimpleCollisionMipLevel);
	}

	FLandscapeGrassWeightExporter Exporter(this, InLandscapeComponents, GrassTypes, true, MoveTemp(HeightMips));
	Exporter.ApplyResults();
}

#endif //WITH_EDITOR

// the purpose of this class is to copy the lightmap from the terrain, and set the CoordinateScale and CoordinateBias to zero.
// we re-use the same texture references, so the memory cost is relatively minimal.
class FLandscapeGrassLightMap : public FLightMap2D
{
public:
	FLandscapeGrassLightMap(const FLightMap2D& InLightMap)
		: FLightMap2D(InLightMap)
	{
		CoordinateScale = FVector2D::ZeroVector;
		CoordinateBias = FVector2D::ZeroVector;
	}
};

// the purpose of this class is to copy the shadowmap from the terrain, and set the CoordinateScale and CoordinateBias to zero.
// we re-use the same texture references, so the memory cost is relatively minimal.
class FLandscapeGrassShadowMap : public FShadowMap2D
{
public:
	FLandscapeGrassShadowMap(const FShadowMap2D& InShadowMap)
		: FShadowMap2D(InShadowMap)
	{
		CoordinateScale = FVector2D::ZeroVector;
		CoordinateBias = FVector2D::ZeroVector;
	}
};

//
// UMaterialExpressionLandscapeGrassOutput
//
FName UMaterialExpressionLandscapeGrassOutput::PinDefaultName = TEXT("Input");

UMaterialExpressionLandscapeGrassOutput::UMaterialExpressionLandscapeGrassOutput(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		FText STRING_Landscape;
		FName NAME_Grass;
		FConstructorStatics()
			: STRING_Landscape(LOCTEXT("Landscape", "Landscape"))
			, NAME_Grass("Grass")
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

#if WITH_EDITORONLY_DATA
	MenuCategories.Add(ConstructorStatics.STRING_Landscape);

	// No outputs
	Outputs.Reset();
#endif

	// Default input
	new(GrassTypes)FGrassInput(ConstructorStatics.NAME_Grass);
}

#if WITH_EDITOR
int32 UMaterialExpressionLandscapeGrassOutput::Compile(class FMaterialCompiler* Compiler, int32 OutputIndex)
{
	if (GrassTypes.IsValidIndex(OutputIndex))
	{
		// Default input to 0 if not connected.
		int32 CodeInput = GrassTypes[OutputIndex].Input.IsConnected() ? GrassTypes[OutputIndex].Input.Compile(Compiler) : Compiler->Constant(0.f);
		return Compiler->CustomOutput(this, OutputIndex, CodeInput);
	}

	return INDEX_NONE;
}

void UMaterialExpressionLandscapeGrassOutput::GetCaption(TArray<FString>& OutCaptions) const
{
	OutCaptions.Add(TEXT("Grass"));
}

const TArray<FExpressionInput*> UMaterialExpressionLandscapeGrassOutput::GetInputs()
{
	TArray<FExpressionInput*> OutInputs;
	for (auto& GrassType : GrassTypes)
	{
		OutInputs.Add(&GrassType.Input);
	}
	return OutInputs;
}

FExpressionInput* UMaterialExpressionLandscapeGrassOutput::GetInput(int32 InputIndex)
{
	return &GrassTypes[InputIndex].Input;
}

FName UMaterialExpressionLandscapeGrassOutput::GetInputName(int32 InputIndex) const
{
	return GrassTypes[InputIndex].Name;
}
#endif // WITH_EDITOR


#if WITH_EDITOR
void UMaterialExpressionLandscapeGrassOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty)
	{
		const FName PropertyName = PropertyChangedEvent.MemberProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UMaterialExpressionLandscapeGrassOutput, GrassTypes))
		{
			for (FGrassInput& Input : GrassTypes)
			{
				ValidateInputName(Input);
			}

			if (GraphNode)
			{
				GraphNode->ReconstructNode();
			}
		}
	}
}

void UMaterialExpressionLandscapeGrassOutput::ValidateInputName(FGrassInput& InInput) const
{
	if (Material != nullptr)
	{
		int32 NameIndex = 1;
		bool bFoundValidName = false;

		// Parameters cannot be named Name_None, use the default name instead
		FName PotentialName = (InInput.Name == NAME_None) ? UMaterialExpressionLandscapeGrassOutput::PinDefaultName : InInput.Name;

		// Find an available unique name
		while (!bFoundValidName)
		{
			if (NameIndex != 1)
			{
				PotentialName.SetNumber(NameIndex);
			}

			bFoundValidName = true;

			// Make sure the name is unique among others pins of this node
			for (const FGrassInput& OtherInput : GrassTypes)
			{
				if (&OtherInput != &InInput)
				{
					if (OtherInput.Name == PotentialName)
					{
						bFoundValidName = false;
						break;
					}
				}
			}

			++NameIndex;
		}

		InInput.Name = PotentialName;
	}
}
#endif

//
// ULandscapeGrassType
//

ULandscapeGrassType::ULandscapeGrassType(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	GrassDensity_DEPRECATED = 400;
	StartCullDistance_DEPRECATED = 10000.0f;
	EndCullDistance_DEPRECATED = 10000.0f;
	PlacementJitter_DEPRECATED = 1.0f;
	RandomRotation_DEPRECATED = true;
	AlignToSurface_DEPRECATED = true;
	bEnableDensityScaling = true;
}

void ULandscapeGrassType::PostLoad()
{
	Super::PostLoad();
	if (GrassMesh_DEPRECATED && !GrassVarieties.Num())
	{
		FGrassVariety Grass;
		Grass.GrassMesh = GrassMesh_DEPRECATED;
		Grass.GrassDensity = GrassDensity_DEPRECATED;
		Grass.StartCullDistance = StartCullDistance_DEPRECATED;
		Grass.EndCullDistance = EndCullDistance_DEPRECATED;
		Grass.PlacementJitter = PlacementJitter_DEPRECATED;
		Grass.RandomRotation = RandomRotation_DEPRECATED;
		Grass.AlignToSurface = AlignToSurface_DEPRECATED;

		GrassVarieties.Add(Grass);
		GrassMesh_DEPRECATED = nullptr;
	}
}


#if WITH_EDITOR
void ULandscapeGrassType::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (GIsEditor)
	{
		for (TObjectIterator<ALandscapeProxy> It; It; ++It)
		{
			ALandscapeProxy* Proxy = *It;
			if (Proxy->GetWorld() && !Proxy->GetWorld()->IsPlayInEditor())
			{
				const UMaterialInterface* MaterialInterface = Proxy->LandscapeMaterial;
				if (MaterialInterface)
				{
					TArray<const UMaterialExpressionLandscapeGrassOutput*> GrassExpressions;
					MaterialInterface->GetMaterial()->GetAllExpressionsOfType<UMaterialExpressionLandscapeGrassOutput>(GrassExpressions);

					// Should only be one grass type node
					if (GrassExpressions.Num() > 0)
					{
						for (auto& Output : GrassExpressions[0]->GrassTypes)
						{
							if (Output.GrassType == this)
							{
								Proxy->FlushGrassComponents();
								break;
							}
						}
					}
				}
			}
		}
	}
}
#endif

//
// FLandscapeComponentGrassData
//
SIZE_T FLandscapeComponentGrassData::GetAllocatedSize() const
{
	return sizeof(*this) + HeightWeightData.GetAllocatedSize() + WeightOffsets.GetAllocatedSize();
}

bool FLandscapeComponentGrassData::HasWeightData() const
{
	return !!WeightOffsets.Num();
}

bool FLandscapeComponentGrassData::HasValidData() const
{
	// If NumElements < 0, its data wasn't computed. 
	// If == 0, its data was computed (i.e. is valid), but removed (for saving space) because its weight data is all zero : 
	// If > 0, its data was computed and contains non-zero weight data
	return (NumElements >= 0);
}

bool FLandscapeComponentGrassData::HasData() const
{
	if (HasValidData())
	{
		int32 LocalNumElements = HeightWeightData.Num();
#if WITH_EDITORONLY_DATA
		if (LocalNumElements == 0)
		{
			LocalNumElements = HeightMipData.Num();
		}
#endif
		return LocalNumElements > 0;
	}

	return false;
}

TArrayView<uint8> FLandscapeComponentGrassData::GetWeightData(const ULandscapeGrassType* GrassType)
{
	if (!HeightWeightData.IsEmpty())
	{
		if (int32* OffsetPtr = WeightOffsets.Find(GrassType))
		{
			int32 Offset = *OffsetPtr;
			check(Offset + NumElements <= HeightWeightData.Num());
			check(NumElements);
			return MakeArrayView<uint8>(&HeightWeightData[Offset], NumElements);
		}
	}

	return TArrayView<uint8>();
}

bool FLandscapeComponentGrassData::Contains(ULandscapeGrassType* GrassType) const
{
	return WeightOffsets.Contains(GrassType);
}

TArrayView<uint16> FLandscapeComponentGrassData::GetHeightData()
{
	if (HeightWeightData.IsEmpty())
	{
		return TArrayView<uint16>();
	}

	check(NumElements <= HeightWeightData.Num());
	return MakeArrayView<uint16>((uint16*)&HeightWeightData[0], NumElements);
}

void FLandscapeComponentGrassData::InitializeFrom(const TArray<uint16>& HeightData, const TMap<ULandscapeGrassType*, TArray<uint8>>& WeightData)
{
#if WITH_EDITORONLY_DATA
	WeightOffsets.Empty(WeightData.Num());

	// If weight data is empty make sure we don't have any memory allocated to grass
	if (WeightData.Num() == 0)
	{
		NumElements = 0;
		HeightWeightData.Empty();
		return;
	}

	NumElements = HeightData.Num();
	HeightWeightData.SetNumUninitialized(NumElements * sizeof(uint16) + NumElements * WeightData.Num() * sizeof(uint8));

	uint8* CopyDest = HeightWeightData.GetData();
	int32 CopyOffset = 0;
	int32 CopySize = HeightData.Num() * sizeof(uint16);

	check((CopyOffset + CopySize) <= HeightWeightData.Num());

	FMemory::Memcpy(&CopyDest[CopyOffset], HeightData.GetData(), CopySize);

	CopyOffset += CopySize;
	CopySize = NumElements * sizeof(uint8);
		
	for (const TPair<ULandscapeGrassType*,TArray<uint8>>& Pair : WeightData)
	{
		WeightOffsets.Add(Pair.Key, CopyOffset);
		check(Pair.Value.Num() == NumElements);
		check((CopyOffset + CopySize) <= HeightWeightData.Num());

		FMemory::Memcpy(&CopyDest[CopyOffset], Pair.Value.GetData(), CopySize);
		CopyOffset += CopySize;
	}
#endif
}


FArchive& operator<<(FArchive& Ar, FLandscapeComponentGrassData& Data)
{
	Ar.UsingCustomVersion(FLandscapeCustomVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsFilterEditorOnly())
	{
		if (Ar.CustomVer(FLandscapeCustomVersion::GUID) >= FLandscapeCustomVersion::GrassMaterialInstanceFix)
		{
			Ar << Data.MaterialStateIds;
		}
		else
		{
			Data.MaterialStateIds.Empty(1);
			if (Ar.UEVer() >= VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA_MATERIAL_GUID)
			{
				FGuid MaterialStateId;
				Ar << MaterialStateId;
				Data.MaterialStateIds.Add(MaterialStateId);
			}
		}

		if (Ar.CustomVer(FLandscapeCustomVersion::GUID) >= FLandscapeCustomVersion::GrassMaterialWPO)
		{
			Ar << Data.RotationForWPO;
		}
	}

	TArray<uint16> DeprecatedHeightData;
	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::LandscapeGrassSingleArray)
	{
		DeprecatedHeightData.BulkSerialize(Ar);
	}

	if (!Ar.IsFilterEditorOnly())
	{
		if (Ar.CustomVer(FLandscapeCustomVersion::GUID) >= FLandscapeCustomVersion::CollisionMaterialWPO)
		{
			if (Ar.CustomVer(FLandscapeCustomVersion::GUID) >= FLandscapeCustomVersion::LightmassMaterialWPO)
			{
				// todo - BulkSerialize each mip?
				Ar << Data.HeightMipData;
			}
			else
			{
				checkSlow(Ar.IsLoading());

				TArray<uint16> CollisionHeightData;
				CollisionHeightData.BulkSerialize(Ar);
				if (CollisionHeightData.Num())
				{
					const int32 ComponentSizeQuads = FMath::Sqrt(static_cast<float>(Data.GetHeightData().Num())) - 1;
					const int32 CollisionSizeQuads = FMath::Sqrt(static_cast<float>(CollisionHeightData.Num())) - 1;
					const int32 CollisionMip = FMath::FloorLog2(ComponentSizeQuads / CollisionSizeQuads);
					Data.HeightMipData.Add(CollisionMip, MoveTemp(CollisionHeightData));
				}

				TArray<uint16> SimpleCollisionHeightData;
				SimpleCollisionHeightData.BulkSerialize(Ar);
				if (SimpleCollisionHeightData.Num())
				{
					const int32 ComponentSizeQuads = FMath::Sqrt(static_cast<float>(Data.GetHeightData().Num())) - 1;
					const int32 SimpleCollisionSizeQuads = FMath::Sqrt(static_cast<float>(SimpleCollisionHeightData.Num())) - 1;
					const int32 SimpleCollisionMip = FMath::FloorLog2(ComponentSizeQuads / SimpleCollisionSizeQuads);
					Data.HeightMipData.Add(SimpleCollisionMip, MoveTemp(SimpleCollisionHeightData));
				}
			}
		}
	}

	TMap<ULandscapeGrassType*, TArray<uint8>> DeprecatedWeightData;
	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::LandscapeGrassSingleArray)
	{
		Ar << DeprecatedWeightData;

		Data.InitializeFrom(DeprecatedHeightData, DeprecatedWeightData);
	}
	else
#endif
	{
		Ar << Data.NumElements;
		Ar << Data.WeightOffsets;
		Ar << Data.HeightWeightData;
	}

	return Ar;
}

void FLandscapeComponentGrassData::ConditionalDiscardDataOnLoad()
{
	//check(HasValidData());
	if (!GIsEditor && GGrassDiscardDataOnLoad)
	{
		bool bRemoved = false;
		// Remove data for grass types which have scalability enabled
		for (auto GrassTypeIt = WeightOffsets.CreateIterator(); GrassTypeIt; ++GrassTypeIt)
		{
			if (!GrassTypeIt.Key() || GrassTypeIt.Key()->bEnableDensityScaling)
			{
				GrassTypeIt.RemoveCurrent();
				bRemoved = true;
			}
		}

		// If all grass types have been removed, discard the height data too.
		if (WeightOffsets.Num() == 0)
		{
			*this = FLandscapeComponentGrassData();
			NumElements = 0;
		}
		else if (bRemoved) 
		{
			TMap<ULandscapeGrassType*, int32> PreviousOffsets(MoveTemp(WeightOffsets));
			TArray<uint8> PreviousHeightWeightData(MoveTemp(HeightWeightData));
			HeightWeightData.SetNumUninitialized(NumElements * sizeof(uint16) + NumElements * PreviousOffsets.Num() * sizeof(uint8), /*bAllowShrinking*/ true);

			uint8* CopyDest = HeightWeightData.GetData();
			uint8* CopySrc = PreviousHeightWeightData.GetData();
			int32 CopyOffset = 0;
			int32 CopySize = NumElements * sizeof(uint16);

			check((CopyOffset + CopySize) <= HeightWeightData.Num());

			FMemory::Memcpy(&CopyDest[CopyOffset], PreviousHeightWeightData.GetData(), CopySize);

			CopyOffset += CopySize;
			CopySize = NumElements * sizeof(uint8);

			for (const TPair<ULandscapeGrassType*, int32>& Pair : PreviousOffsets)
			{
				int32 PreviousOffset = Pair.Value;
				WeightOffsets.Add(Pair.Key, CopyOffset);
				check((CopyOffset + CopySize) <= HeightWeightData.Num());
				check((PreviousOffset + CopySize) <= PreviousHeightWeightData.Num());

				FMemory::Memcpy(&CopyDest[CopyOffset], &CopySrc[PreviousOffset], CopySize);
				CopyOffset += CopySize;
	}
}
	}
}

//
// ALandscapeProxy grass-related functions
//

void ALandscapeProxy::TickGrass(const TArray<FVector>& Cameras, int32& InOutNumCompsCreated)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ALandscapeProxy::TickGrass);
#if WITH_EDITORONLY_DATA
	if (ALandscape* Landscape = GetLandscapeActor())
	{
		if (!Landscape->IsUpToDate() || !Landscape->bGrassUpdateEnabled)
		{
			return;
		}
	}
#endif

	UpdateGrass(Cameras, InOutNumCompsCreated);
}

struct FGrassBuilderBase
{
	bool bHaveValidData;
	float GrassDensity;
	FVector DrawScale;
	FVector DrawLoc;
	FMatrix LandscapeToWorld;

	FIntPoint SectionBase;
	FIntPoint LandscapeSectionOffset;
	int32 ComponentSizeQuads;
	FVector Origin;
	FVector Extent;
	FVector ComponentOrigin;

	int32 SqrtMaxInstances;

	FGrassBuilderBase(ALandscapeProxy* Landscape, ULandscapeComponent* Component, const FGrassVariety& GrassVariety, ERHIFeatureLevel::Type FeatureLevel, int32 SqrtSubsections = 1, int32 SubX = 0, int32 SubY = 0, bool bEnableDensityScaling = true)
	{
		const float DensityScale = bEnableDensityScaling ? GGrassDensityScale : 1.0f;
		GrassDensity = GrassVariety.GrassDensity.GetValue() * DensityScale;

		DrawScale = Landscape->GetRootComponent()->GetRelativeScale3D();
		DrawLoc = Landscape->GetActorLocation();
		LandscapeSectionOffset = Landscape->LandscapeSectionOffset;

		SectionBase = Component->GetSectionBase();
		ComponentSizeQuads = Component->ComponentSizeQuads;

		Origin.X = DrawScale.X * SectionBase.X;
		Origin.Y = DrawScale.Y * SectionBase.Y;
		Origin.Z = 0.0f;

		Extent.X = DrawScale.X * ComponentSizeQuads;
		Extent.Y = DrawScale.Y * ComponentSizeQuads;
		Extent.Z = 0.0f;

		ComponentOrigin.X = DrawScale.X * (SectionBase.X - LandscapeSectionOffset.X);
		ComponentOrigin.Y = DrawScale.Y * (SectionBase.Y - LandscapeSectionOffset.Y);
		ComponentOrigin.Z = 0.0f;

		SqrtMaxInstances = FMath::CeilToInt(FMath::Sqrt(FMath::Abs(Extent.X * Extent.Y * GrassDensity / 1000.0f / 1000.0f)));

		bHaveValidData = SqrtMaxInstances != 0;

		LandscapeToWorld = Landscape->GetRootComponent()->GetComponentTransform().ToMatrixNoScale();

		if (bHaveValidData && SqrtSubsections != 1)
		{
			check(SqrtMaxInstances > 2 * SqrtSubsections);
			SqrtMaxInstances /= SqrtSubsections;
			check(SqrtMaxInstances > 0);

			Extent.X /= SqrtSubsections;
			Extent.Y /= SqrtSubsections;

			Origin.X += Extent.X * SubX;
			Origin.Y += Extent.Y * SubY;
		}
	}
};

// FLandscapeComponentGrassAccess - accessor wrapper for data for one GrassType from one Component
struct FLandscapeComponentGrassAccess
{
	FLandscapeComponentGrassAccess(const ULandscapeComponent* InComponent, const ULandscapeGrassType* GrassType)
	: GrassData(InComponent->GrassData)
	, HeightData(InComponent->GrassData->GetHeightData())
	, WeightData(InComponent->GrassData->GetWeightData(GrassType))
	, Stride(InComponent->ComponentSizeQuads + 1)
	{}

	bool IsValid()
	{
		return WeightData.Num() == FMath::Square(Stride) && HeightData.Num() == FMath::Square(Stride);
	}

	FORCEINLINE float GetHeight(int32 IdxX, int32 IdxY)
	{
		return LandscapeDataAccess::GetLocalHeight(HeightData[IdxX + Stride*IdxY]);
	}
	FORCEINLINE float GetWeight(int32 IdxX, int32 IdxY)
	{
		return ((float)WeightData[IdxX + Stride*IdxY]) / 255.f;
	}

	FORCEINLINE int32 GetStride()
	{
		return Stride;
	}

private:
	TSharedRef<FLandscapeComponentGrassData, ESPMode::ThreadSafe> GrassData;
	TArrayView<uint16> HeightData;
	TArrayView<uint8> WeightData;
	int32 Stride;
};

struct FAsyncGrassBuilder : public FGrassBuilderBase
{
	FLandscapeComponentGrassAccess GrassData;
	EGrassScaling Scaling;
	FFloatInterval ScaleX;
	FFloatInterval ScaleY;
	FFloatInterval ScaleZ;
	bool RandomRotation;
	bool RandomScale;
	bool AlignToSurface;
	float PlacementJitter;
	FRandomStream RandomStream;
	FMatrix XForm;
	FBox MeshBox;
	int32 DesiredInstancesPerLeaf;

	double BuildTime;
	int32 TotalInstances;
	uint32 HaltonBaseIndex;

	bool UseLandscapeLightmap;
	FVector2D LightmapBaseBias;
	FVector2D LightmapBaseScale;
	FVector2D ShadowmapBaseBias;
	FVector2D ShadowmapBaseScale;
	FVector2D LightMapComponentBias;
	FVector2D LightMapComponentScale;
	bool RequireCPUAccess;

	TArray<FBox> ExcludedBoxes;

	// output
	TArray<FInstancedStaticMeshInstanceData> InstanceData;
	FStaticMeshInstanceData InstanceBuffer;
	TArray<FClusterNode> ClusterTree;
	int32 OutOcclusionLayerNum;

	FAsyncGrassBuilder(ALandscapeProxy* Landscape, ULandscapeComponent* Component, const ULandscapeGrassType* GrassType, const FGrassVariety& GrassVariety, ERHIFeatureLevel::Type FeatureLevel, UGrassInstancedStaticMeshComponent* GrassInstancedStaticMeshComponent, int32 SqrtSubsections, int32 SubX, int32 SubY, uint32 InHaltonBaseIndex, TArray<FBox>& InExcludedBoxes)
		: FGrassBuilderBase(Landscape, Component, GrassVariety, FeatureLevel, SqrtSubsections, SubX, SubY, GrassType->bEnableDensityScaling)
		, GrassData(Component, GrassType)
		, Scaling(GrassVariety.Scaling)
		, ScaleX(GrassVariety.ScaleX)
		, ScaleY(GrassVariety.ScaleY)
		, ScaleZ(GrassVariety.ScaleZ)
		, RandomRotation(GrassVariety.RandomRotation)
		, RandomScale(false)
		, AlignToSurface(GrassVariety.AlignToSurface)
		, PlacementJitter(GrassVariety.PlacementJitter)
		, RandomStream(GrassInstancedStaticMeshComponent->InstancingRandomSeed)
		, XForm(LandscapeToWorld * GrassInstancedStaticMeshComponent->GetComponentTransform().ToMatrixWithScale().Inverse())
		, MeshBox(GrassVariety.GrassMesh->GetBounds().GetBox())
		, DesiredInstancesPerLeaf(GrassInstancedStaticMeshComponent->DesiredInstancesPerLeaf())

		, BuildTime(0)
		, TotalInstances(0)
		, HaltonBaseIndex(InHaltonBaseIndex)

		, UseLandscapeLightmap(GrassVariety.bUseLandscapeLightmap)
		, LightmapBaseBias(FVector2D::ZeroVector)
		, LightmapBaseScale(FVector2D::UnitVector)
		, ShadowmapBaseBias(FVector2D::ZeroVector)
		, ShadowmapBaseScale(FVector2D::UnitVector)
		, LightMapComponentBias(FVector2D::ZeroVector)
		, LightMapComponentScale(FVector2D::UnitVector)
		, RequireCPUAccess(GrassVariety.bKeepInstanceBufferCPUCopy)

		// output
		, InstanceBuffer(/*bSupportsVertexHalfFloat*/ GVertexElementTypeSupport.IsSupported(VET_Half2))
		, ClusterTree()
		, OutOcclusionLayerNum(0)
	{		

		switch (Scaling)
		{
		case EGrassScaling::Uniform:
			RandomScale = ScaleX.Size() > 0;
			break;
		case EGrassScaling::Free:
			RandomScale = ScaleX.Size() > 0 || ScaleY.Size() > 0 || ScaleZ.Size() > 0;
			break;
		case EGrassScaling::LockXY:
			RandomScale = ScaleX.Size() > 0 || ScaleZ.Size() > 0;
			break;
		default:
			check(0);
		}

		if (InExcludedBoxes.Num())
		{
			FMatrix BoxXForm = GrassInstancedStaticMeshComponent->GetComponentToWorld().ToMatrixWithScale().Inverse() * XForm.Inverse();
			for (const FBox& Box : InExcludedBoxes)
			{
				ExcludedBoxes.Add(Box.TransformBy(BoxXForm));
			}
		}

		bHaveValidData = bHaveValidData && GrassData.IsValid();

		InstanceBuffer.SetAllowCPUAccess(RequireCPUAccess);

		check(DesiredInstancesPerLeaf > 0);

		if (UseLandscapeLightmap)
		{
			InitLandscapeLightmap(Component);
		}
	}

	void InitLandscapeLightmap(ULandscapeComponent* Component)
	{
		const int32 SubsectionSizeQuads = Component->SubsectionSizeQuads;
		const int32 NumSubsections = Component->NumSubsections;
		const int32 LandscapeComponentSizeQuads = Component->ComponentSizeQuads;
	
		const int32 StaticLightingLOD = Component->GetLandscapeProxy()->StaticLightingLOD;
		const int32 ComponentSizeVerts = LandscapeComponentSizeQuads + 1;
		const float LightMapRes = Component->StaticLightingResolution > 0.0f ? Component->StaticLightingResolution : Component->GetLandscapeProxy()->StaticLightingResolution;
		const int32 LightingLOD = Component->GetLandscapeProxy()->StaticLightingLOD;

		// Calculate mapping from landscape to lightmap space for mapping landscape grass to the landscape lightmap
		// Copied from the calculation of FLandscapeUniformShaderParameters::LandscapeLightmapScaleBias in FLandscapeComponentSceneProxy::OnTransformChanged()
		int32 PatchExpandCountX = 0;
		int32 PatchExpandCountY = 0;
		int32 DesiredSize = 1;
		const float LightMapRatio = ::GetTerrainExpandPatchCount(LightMapRes, PatchExpandCountX, PatchExpandCountY, LandscapeComponentSizeQuads, (NumSubsections * (SubsectionSizeQuads + 1)), DesiredSize, LightingLOD);
		const float LightmapLODScaleX = LightMapRatio / ((ComponentSizeVerts >> StaticLightingLOD) + 2 * PatchExpandCountX);
		const float LightmapLODScaleY = LightMapRatio / ((ComponentSizeVerts >> StaticLightingLOD) + 2 * PatchExpandCountY);
		const float LightmapBiasX = PatchExpandCountX * LightmapLODScaleX;
		const float LightmapBiasY = PatchExpandCountY * LightmapLODScaleY;
		const float LightmapScaleX = LightmapLODScaleX * (float)((ComponentSizeVerts >> StaticLightingLOD) - 1) / LandscapeComponentSizeQuads;
		const float LightmapScaleY = LightmapLODScaleY * (float)((ComponentSizeVerts >> StaticLightingLOD) - 1) / LandscapeComponentSizeQuads;

		LightMapComponentScale = FVector2D(LightmapScaleX, LightmapScaleY) / FVector2D(DrawScale);
		LightMapComponentBias = FVector2D(LightmapBiasX, LightmapBiasY);

		const FMeshMapBuildData* MeshMapBuildData = Component->GetMeshMapBuildData();

		if (MeshMapBuildData != nullptr)
		{
			if (MeshMapBuildData->LightMap.IsValid())
			{
				LightmapBaseBias = MeshMapBuildData->LightMap->GetLightMap2D()->GetCoordinateBias();
				LightmapBaseScale = MeshMapBuildData->LightMap->GetLightMap2D()->GetCoordinateScale();
			}

			if (MeshMapBuildData->ShadowMap.IsValid())
			{
				ShadowmapBaseBias = MeshMapBuildData->ShadowMap->GetShadowMap2D()->GetCoordinateBias();
				ShadowmapBaseScale = MeshMapBuildData->ShadowMap->GetShadowMap2D()->GetCoordinateScale();
			}
		}
	}

	void SetInstance(int32 InstanceIndex, const FMatrix& InXForm, float RandomFraction)
	{
		if (UseLandscapeLightmap)
		{
			FMatrix::FReal InstanceX = InXForm.M[3][0];
			FMatrix::FReal InstanceY = InXForm.M[3][1];

			FVector2D NormalizedGrassCoordinate;
			NormalizedGrassCoordinate.X = (InstanceX - ComponentOrigin.X) * LightMapComponentScale.X + LightMapComponentBias.X;
			NormalizedGrassCoordinate.Y = (InstanceY - ComponentOrigin.Y) * LightMapComponentScale.Y + LightMapComponentBias.Y;

			FVector2D LightMapCoordinate = NormalizedGrassCoordinate * LightmapBaseScale + LightmapBaseBias;
			FVector2D ShadowMapCoordinate = NormalizedGrassCoordinate * ShadowmapBaseScale + ShadowmapBaseBias;

			InstanceBuffer.SetInstance(InstanceIndex, FMatrix44f(InXForm), RandomStream.GetFraction(), LightMapCoordinate, ShadowMapCoordinate);
		}
		else
		{
			InstanceBuffer.SetInstance(InstanceIndex, FMatrix44f(InXForm), RandomStream.GetFraction());
		}
	}

	FVector GetDefaultScale() const
	{
		FVector Result( ScaleX.Min > 0.0f && FMath::IsNearlyZero(ScaleX.Size()) ? ScaleX.Min : 1.0f,
						ScaleY.Min > 0.0f && FMath::IsNearlyZero(ScaleY.Size()) ? ScaleY.Min : 1.0f,
						ScaleZ.Min > 0.0f && FMath::IsNearlyZero(ScaleZ.Size()) ? ScaleZ.Min : 1.0f);
		switch (Scaling)
		{
		case EGrassScaling::Uniform:
			Result.Y = Result.X;
			Result.Z = Result.X;
			break;
		case EGrassScaling::Free:
			break;
		case EGrassScaling::LockXY:
			Result.Y = Result.X;
			break;
		default:
			check(0);
		}
		return Result;
	}

	FVector GetRandomScale() const
	{
		FVector Result(1.0f);

		switch (Scaling)
		{
		case EGrassScaling::Uniform:
			Result.X = ScaleX.Interpolate(RandomStream.GetFraction());
			Result.Y = Result.X;
			Result.Z = Result.X;
			break;
		case EGrassScaling::Free:
			Result.X = ScaleX.Interpolate(RandomStream.GetFraction());
			Result.Y = ScaleY.Interpolate(RandomStream.GetFraction());
			Result.Z = ScaleZ.Interpolate(RandomStream.GetFraction());
			break;
		case EGrassScaling::LockXY:
			Result.X = ScaleX.Interpolate(RandomStream.GetFraction());
			Result.Y = Result.X;
			Result.Z = ScaleZ.Interpolate(RandomStream.GetFraction());
			break;
		default:
			check(0);
		}
	
		return Result;
	}

	bool IsExcluded(const FVector& LocationWithHeight)
	{
		for (const FBox& Box : ExcludedBoxes)
		{
			if (Box.IsInside(LocationWithHeight))
			{
				return true;
			}
		}
		return false;
	}

	void Build()
	{
		SCOPE_CYCLE_COUNTER(STAT_FoliageGrassAsyncBuildTime);
		check(bHaveValidData);
		double StartTime = FPlatformTime::Seconds();
		const FVector DefaultScale = GetDefaultScale();
		float Div = 1.0f / float(SqrtMaxInstances);
		TArray<FMatrix> InstanceTransforms;
		if (HaltonBaseIndex)
		{
			if (Extent.X < 0)
			{
				Origin.X += Extent.X;
				Extent.X *= -1.0f;
			}
			if (Extent.Y < 0)
			{
				Origin.Y += Extent.Y;
				Extent.Y *= -1.0f;
			}
			int32 MaxNum = SqrtMaxInstances * SqrtMaxInstances;
			InstanceTransforms.Reserve(MaxNum);
			FVector DivExtent(Extent * Div);
			for (int32 InstanceIndex = 0; InstanceIndex < MaxNum; InstanceIndex++)
			{
				float HaltonX = Halton(InstanceIndex + HaltonBaseIndex, 2);
				float HaltonY = Halton(InstanceIndex + HaltonBaseIndex, 3);
				FVector Location(Origin.X + HaltonX * Extent.X, Origin.Y + HaltonY * Extent.Y, 0.0f);
				FVector LocationWithHeight;
				FVector ComputedNormal;
				float Weight = 0.f;
				SampleLandscapeAtLocationLocal(Location, LocationWithHeight, Weight, AlignToSurface ? &ComputedNormal : nullptr);
				bool bKeep = Weight > 0.0f && Weight >= RandomStream.GetFraction() && !IsExcluded(LocationWithHeight);
				if (bKeep)
				{
					const FVector Scale = RandomScale ? GetRandomScale() : DefaultScale;
					const float Rot = RandomRotation ? RandomStream.GetFraction() * 360.0f : 0.0f;
					const FMatrix BaseXForm = FScaleRotationTranslationMatrix(Scale, FRotator(0.0f, Rot, 0.0f), FVector::ZeroVector);
					FMatrix OutXForm;
					if (AlignToSurface && !ComputedNormal.IsNearlyZero())
					{
						const FVector NewZ = ComputedNormal * FMath::Sign(ComputedNormal.Z);
						const FVector NewX = (FVector(0, -1, 0) ^ NewZ).GetSafeNormal();
						const FVector NewY = NewZ ^ NewX;
						const FMatrix Align = FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
						OutXForm = (BaseXForm * Align).ConcatTranslation(LocationWithHeight) * XForm;
					}
					else
					{
						OutXForm = BaseXForm.ConcatTranslation(LocationWithHeight) * XForm;
					}
					InstanceTransforms.Add(OutXForm);
				}
			}
			if (InstanceTransforms.Num())
			{
				TotalInstances += InstanceTransforms.Num();
				InstanceBuffer.AllocateInstances(InstanceTransforms.Num(), 0, EResizeBufferFlags::AllowSlackOnGrow | EResizeBufferFlags::AllowSlackOnReduce, true);
				for (int32 InstanceIndex = 0; InstanceIndex < InstanceTransforms.Num(); InstanceIndex++)
				{
					const FMatrix& OutXForm = InstanceTransforms[InstanceIndex];
					SetInstance(InstanceIndex, OutXForm, RandomStream.GetFraction());
				}
			}
		}
		else
		{
			int32 NumKept = 0;
			float MaxJitter1D = FMath::Clamp<float>(PlacementJitter, 0.0f, .99f) * Div * .5f;
			FVector MaxJitter(MaxJitter1D, MaxJitter1D, 0.0f);
			MaxJitter *= Extent;
			Origin += Extent * (Div * 0.5f);
			struct FInstanceLocal
			{
				FVector Pos;
				bool bKeep;
			};
			TArray<FInstanceLocal> Instances;
			Instances.AddUninitialized(SqrtMaxInstances * SqrtMaxInstances);
			{
				int32 InstanceIndex = 0;
				for (int32 xStart = 0; xStart < SqrtMaxInstances; xStart++)
				{
					for (int32 yStart = 0; yStart < SqrtMaxInstances; yStart++)
					{
						FVector Location(Origin.X + float(xStart) * Div * Extent.X, Origin.Y + float(yStart) * Div * Extent.Y, 0.0f);

						// NOTE: We evaluate the random numbers on the stack and store them in locals rather than inline within the FVector() constructor below, because 
						// the order of evaluation of function arguments in C++ is unspecified.  We really want this to behave consistently on all sorts of
						// different platforms!
						const float FirstRandom = RandomStream.GetFraction();
						const float SecondRandom = RandomStream.GetFraction();
						Location += FVector(FirstRandom * 2.0f - 1.0f, SecondRandom * 2.0f - 1.0f, 0.0f) * MaxJitter;

						FInstanceLocal& Instance = Instances[InstanceIndex];
						float Weight = 0.f;
						SampleLandscapeAtLocationLocal(Location, Instance.Pos, Weight);
						Instance.bKeep = Weight > 0.0f && Weight >= RandomStream.GetFraction() && !IsExcluded(Instance.Pos);
						if (Instance.bKeep)
						{
							NumKept++;
						}
						InstanceIndex++;
					}
				}
			}
			if (NumKept)
			{
				InstanceTransforms.AddUninitialized(NumKept);
				TotalInstances += NumKept;
				{
					InstanceBuffer.AllocateInstances(NumKept, 0, EResizeBufferFlags::AllowSlackOnGrow | EResizeBufferFlags::AllowSlackOnReduce, true);
					int32 InstanceIndex = 0;
					int32 OutInstanceIndex = 0;
					for (int32 xStart = 0; xStart < SqrtMaxInstances; xStart++)
					{
						for (int32 yStart = 0; yStart < SqrtMaxInstances; yStart++)
						{
							const FInstanceLocal& Instance = Instances[InstanceIndex];
							if (Instance.bKeep)
							{
								const FVector Scale = RandomScale ? GetRandomScale() : DefaultScale;
								const float Rot = RandomRotation ? RandomStream.GetFraction() * 360.0f : 0.0f;
								const FMatrix BaseXForm = FScaleRotationTranslationMatrix(Scale, FRotator(0.0f, Rot, 0.0f), FVector::ZeroVector);
								FMatrix OutXForm;
								if (AlignToSurface)
								{
									FVector PosX1 = xStart ? Instances[InstanceIndex - SqrtMaxInstances].Pos : Instance.Pos;
									FVector PosX2 = (xStart + 1 < SqrtMaxInstances) ? Instances[InstanceIndex + SqrtMaxInstances].Pos : Instance.Pos;
									FVector PosY1 = yStart ? Instances[InstanceIndex - 1].Pos : Instance.Pos;
									FVector PosY2 = (yStart + 1 < SqrtMaxInstances) ? Instances[InstanceIndex + 1].Pos : Instance.Pos;

									if (PosX1 != PosX2 && PosY1 != PosY2)
									{
										FVector NewZ = ((PosX1 - PosX2) ^ (PosY1 - PosY2)).GetSafeNormal();
										NewZ *= FMath::Sign(NewZ.Z);

										const FVector NewX = (FVector(0, -1, 0) ^ NewZ).GetSafeNormal();
										const FVector NewY = NewZ ^ NewX;

										FMatrix Align = FMatrix(NewX, NewY, NewZ, FVector::ZeroVector);
										OutXForm = (BaseXForm * Align).ConcatTranslation(Instance.Pos) * XForm;
									}
									else
									{
										OutXForm = BaseXForm.ConcatTranslation(Instance.Pos) * XForm;
									}
								}
								else
								{
									OutXForm = BaseXForm.ConcatTranslation(Instance.Pos) * XForm;
								}
								InstanceTransforms[OutInstanceIndex] = OutXForm;
								SetInstance(OutInstanceIndex++, OutXForm, RandomStream.GetFraction());
							}
							InstanceIndex++;
						}
					}
				}
			}
		}

		int32 NumInstances = InstanceTransforms.Num();
		if (NumInstances)
		{
			TArray<int32> SortedInstances;
			TArray<int32> InstanceReorderTable;
			TArray<float> InstanceCustomDataDummy;
			UGrassInstancedStaticMeshComponent::BuildTreeAnyThread(InstanceTransforms, InstanceCustomDataDummy, 0, MeshBox, ClusterTree, SortedInstances, InstanceReorderTable, OutOcclusionLayerNum, DesiredInstancesPerLeaf, false);

			InstanceData.Reset(NumInstances);
			for (const FMatrix& Transform : InstanceTransforms)
			{
				InstanceData.Emplace(Transform);
			}
			
			// in-place sort the instances and generate the sorted instance data
			for (int32 FirstUnfixedIndex = 0; FirstUnfixedIndex < NumInstances; FirstUnfixedIndex++)
			{
				int32 LoadFrom = SortedInstances[FirstUnfixedIndex];				

				if (LoadFrom != FirstUnfixedIndex)
				{
					check(LoadFrom > FirstUnfixedIndex);
					InstanceBuffer.SwapInstance(FirstUnfixedIndex, LoadFrom);
					InstanceData.Swap(FirstUnfixedIndex, LoadFrom);

					int32 SwapGoesTo = InstanceReorderTable[FirstUnfixedIndex];
					check(SwapGoesTo > FirstUnfixedIndex);
					check(SortedInstances[SwapGoesTo] == FirstUnfixedIndex);
					SortedInstances[SwapGoesTo] = LoadFrom;
					InstanceReorderTable[LoadFrom] = SwapGoesTo;

					InstanceReorderTable[FirstUnfixedIndex] = FirstUnfixedIndex;
					SortedInstances[FirstUnfixedIndex] = FirstUnfixedIndex;
				}
			}
		}
		BuildTime = FPlatformTime::Seconds() - StartTime;
	}

	FORCEINLINE_DEBUGGABLE void SampleLandscapeAtLocationLocal(const FVector& InLocation, FVector& OutLocation, float& OutLayerWeight, FVector* OutNormal = nullptr)
	{
		// Find location
		const float TestX = InLocation.X / DrawScale.X - (float)SectionBase.X;
		const float TestY = InLocation.Y / DrawScale.Y - (float)SectionBase.Y;

		// Find data
		const int32 X1 = FMath::FloorToInt(TestX);
		const int32 Y1 = FMath::FloorToInt(TestY);
		const int32 X2 = FMath::CeilToInt(TestX);
		const int32 Y2 = FMath::CeilToInt(TestY);

		// Clamp to prevent the sampling of the final columns from overflowing
		const int32 IdxX1 = FMath::Clamp<int32>(X1, 0, GrassData.GetStride() - 1);
		const int32 IdxY1 = FMath::Clamp<int32>(Y1, 0, GrassData.GetStride() - 1);
		const int32 IdxX2 = FMath::Clamp<int32>(X2, 0, GrassData.GetStride() - 1);
		const int32 IdxY2 = FMath::Clamp<int32>(Y2, 0, GrassData.GetStride() - 1);

		const float LerpX = FMath::Fractional(TestX);
		const float LerpY = FMath::Fractional(TestY);

		// Bilinear interpolate sampled weights
		const float SampleWeight11 = GrassData.GetWeight(IdxX1, IdxY1);
		const float SampleWeight21 = GrassData.GetWeight(IdxX2, IdxY1);
		const float SampleWeight12 = GrassData.GetWeight(IdxX1, IdxY2);
		const float SampleWeight22 = GrassData.GetWeight(IdxX2, IdxY2);

		OutLayerWeight = FMath::Lerp(
			FMath::Lerp(SampleWeight11, SampleWeight21, LerpX),
			FMath::Lerp(SampleWeight12, SampleWeight22, LerpX),
			LerpY);

		// Bilinear interpolate sampled heights
		const float SampleHeight11 = GrassData.GetHeight(IdxX1, IdxY1);
		const float SampleHeight21 = GrassData.GetHeight(IdxX2, IdxY1);
		const float SampleHeight12 = GrassData.GetHeight(IdxX1, IdxY2);
		const float SampleHeight22 = GrassData.GetHeight(IdxX2, IdxY2);

		OutLocation.X = InLocation.X - DrawScale.X * float(LandscapeSectionOffset.X);
		OutLocation.Y = InLocation.Y - DrawScale.Y * float(LandscapeSectionOffset.Y);
		OutLocation.Z = DrawScale.Z * FMath::Lerp(
			FMath::Lerp(SampleHeight11, SampleHeight21, LerpX),
			FMath::Lerp(SampleHeight12, SampleHeight22, LerpX),
			LerpY);
		
		// Compute normal
		if (OutNormal)
		{
			const FVector P11((float)X1, (float)Y1, SampleHeight11);
			const FVector P22((float)X2, (float)Y2, SampleHeight22);
			
			// Choose triangle and compute normal
			if (LerpX > LerpY)
			{
				const FVector P21((float)X2, (float)Y1, SampleHeight21);
				*OutNormal = (P11 != P21 && P22 != P21) ? FVector(((P22 - P21) ^ (P11 - P21)).GetSafeNormal()) : FVector::ZeroVector;
			}
			else 
			{
				const FVector P12((float)X1, (float)Y2, SampleHeight12);
				*OutNormal = (P11 != P12 && P22 != P12) ? FVector(((P11 - P12) ^ (P22 - P12)).GetSafeNormal()) : FVector::ZeroVector;
			}
		}
	}
};

void ALandscapeProxy::FlushGrassComponents(const TSet<ULandscapeComponent*>* OnlyForComponents, bool bFlushGrassMaps)
{
	if (OnlyForComponents)
	{
		for (FCachedLandscapeFoliage::TGrassSet::TIterator Iter(FoliageCache.CachedGrassComps); Iter; ++Iter)
		{
			ULandscapeComponent* Component = (*Iter).Key.BasedOn.Get();
			// if the weak pointer in the cache is invalid, we should kill them anyway
			if (Component == nullptr || OnlyForComponents->Contains(Component))
			{
				UHierarchicalInstancedStaticMeshComponent *Used = (*Iter).Foliage.Get();
				if (Used)
				{
					SCOPE_CYCLE_COUNTER(STAT_FoliageGrassDestoryComp);
					Used->ClearInstances();
					Used->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
					Used->DestroyComponent();
				}
				Iter.RemoveCurrent();
			}
		}
#if WITH_EDITOR
		if (bFlushGrassMaps)
		{
			for (ULandscapeComponent* Component : *OnlyForComponents)
			{
				if (Component->CanRenderGrassMap())
				{
					Component->RemoveGrassMap();
				}
			}
		}
#endif
	}
	else
	{
		// Clear old foliage component containers
		FoliageComponents.Empty();

		// Might as well clear the cache...
		FoliageCache.ClearCache();
		// Destroy any owned foliage components
		TInlineComponentArray<UHierarchicalInstancedStaticMeshComponent*> FoliageComps;
		GetComponents(FoliageComps);
		for (UHierarchicalInstancedStaticMeshComponent* Component : FoliageComps)
		{
			SCOPE_CYCLE_COUNTER(STAT_FoliageGrassDestoryComp);
			Component->ClearInstances();
			Component->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
			Component->DestroyComponent();
		}

		TArray<USceneComponent*> AttachedFoliageComponents = RootComponent->GetAttachChildren().FilterByPredicate(
			[](USceneComponent* Component)
		{
			return Cast<UHierarchicalInstancedStaticMeshComponent>(Component);
		});

		// Destroy any attached but un-owned foliage components
		for (USceneComponent* Component : AttachedFoliageComponents)
		{
			SCOPE_CYCLE_COUNTER(STAT_FoliageGrassDestoryComp);
			CastChecked<UHierarchicalInstancedStaticMeshComponent>(Component)->ClearInstances();
			Component->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
			Component->DestroyComponent();
		}

#if WITH_EDITOR
		UWorld* World = GetWorld();

		if (bFlushGrassMaps)
		{
			// Clear GrassMaps
			for (UActorComponent* Component : GetComponents())
			{
				if (ULandscapeComponent* LandscapeComp = Cast<ULandscapeComponent>(Component))
				{
					if (LandscapeComp->CanRenderGrassMap())
					{
						LandscapeComp->RemoveGrassMap();
					}
				}
			}
		}
#endif
	}
}

void ALandscapeProxy::GetGrassTypes(const UWorld* World, UMaterialInterface* LandscapeMat, TArray<ULandscapeGrassType*>& GrassTypesOut, float& OutMaxDiscardDistance)
{
	OutMaxDiscardDistance = 0.0f;
	if (LandscapeMat)
	{
		GrassTypesOut.Append(LandscapeMat->GetCachedExpressionData().GrassTypes);

		for (const ULandscapeGrassType* GrassType : LandscapeMat->GetCachedExpressionData().GrassTypes)
		{
			if (GrassType != nullptr)
			{
				for (auto& GrassVariety : GrassType->GrassVarieties)
				{
					const int32 EndCullDistance = GrassVariety.EndCullDistance.GetValue();
					if (EndCullDistance > OutMaxDiscardDistance)
					{
						OutMaxDiscardDistance = EndCullDistance;
					}
				}
			}
		}
	}
}

static uint32 GGrassExclusionChangeTag = 1;
static uint32 GFrameNumberLastStaleCheck = 0;
static TMap<FWeakObjectPtr, FBox> GGrassExclusionBoxes;

void ALandscapeProxy::AddExclusionBox(FWeakObjectPtr Owner, const FBox& BoxToRemove)
{
	GGrassExclusionBoxes.Add(Owner, BoxToRemove);
	GGrassExclusionChangeTag++;
}
void ALandscapeProxy::RemoveExclusionBox(FWeakObjectPtr Owner)
{
	GGrassExclusionBoxes.Remove(Owner);
	GGrassExclusionChangeTag++;
}
void ALandscapeProxy::RemoveAllExclusionBoxes()
{
	if (GGrassExclusionBoxes.Num())
	{
		GGrassExclusionBoxes.Empty();
		GGrassExclusionChangeTag++;
	}
}


#if WITH_EDITOR

FLandscapeGrassMapsBuilder::FLandscapeGrassMapsBuilder(UWorld* InOwner)
	: World(InOwner)
	, OutdatedGrassMapCount(0)
	, GrassMapsLastCheckTime(0)
{}


void FLandscapeGrassMapsBuilder::Build()
{
	if (World)
	{
		int32 Count = GetOutdatedGrassMapCount();
		FScopedSlowTask SlowTask(Count, (LOCTEXT("GrassMaps_BuildGrassMaps", "Building Grass maps")));
		SlowTask.MakeDialog();

		for (TActorIterator<ALandscapeProxy> ProxyIt(World); ProxyIt; ++ProxyIt)
		{
			ProxyIt->BuildGrassMaps(&SlowTask);
		}
	}
}

int32 FLandscapeGrassMapsBuilder::GetOutdatedGrassMapCount(bool bInForceUpdate) const
{
	if (World)
	{
		bool bUpdate = bInForceUpdate || GLandscapeEditModeActive;
		if (!bUpdate)
		{
			double GrassMapsTimeNow = FPlatformTime::Seconds();
			// Recheck every 20 secs to handle the case where levels may have been Streamed in/out
			if ((GrassMapsTimeNow - GrassMapsLastCheckTime) > 20)
			{
				GrassMapsLastCheckTime = GrassMapsTimeNow;
				bUpdate = true;
			}
		}

		if (bUpdate)
		{
			OutdatedGrassMapCount = 0;
			for (TActorIterator<ALandscapeProxy> ProxyIt(World); ProxyIt; ++ProxyIt)
			{
				OutdatedGrassMapCount += ProxyIt->GetOutdatedGrassMapCount();
			}
		}
	}
	return OutdatedGrassMapCount;
}

void ALandscapeProxy::BuildGrassMaps(FScopedSlowTask* InSlowTask)
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		bool bNewHasLandscapeGrass = LandscapeComponents.ContainsByPredicate([](ULandscapeComponent* Component) { return Component->MaterialHasGrass(); });
		if (bHasLandscapeGrass != bNewHasLandscapeGrass)
		{
			bHasLandscapeGrass = bNewHasLandscapeGrass;
			MarkPackageDirty();
		}

		// Generate an mark dirty if changed
		UpdateGrassData(true, InSlowTask);
	}
}

int32 ALandscapeProxy::GetOutdatedGrassMapCount() const
{
	int32 OutdatedGrassMaps = 0;
	if (GGrassEnable > 0)
	{
		UpdateGrassDataStatus(nullptr, nullptr, nullptr, nullptr, false, &OutdatedGrassMaps);
	}
	return OutdatedGrassMaps;
}

int32 ALandscapeProxy::TotalComponentsNeedingGrassMapRender = 0;
int32 ALandscapeProxy::TotalTexturesToStreamForVisibleGrassMapRender = 0;
int32 ALandscapeProxy::TotalComponentsNeedingTextureBaking = 0;

void ALandscapeProxy::UpdateGrassDataStatus(TSet<UTexture2D*>* OutCurrentForcedStreamedTextures, TSet<UTexture2D*>* OutDesiredForcedStreamedTextures, TSet<ULandscapeComponent*>* OutComponentsNeedingGrassMapRender, TSet<ULandscapeComponent*>* OutOutdatedComponents, bool bInEnableForceResidentFlag, int32* OutOutdatedGrassMaps) const
{
	if (OutCurrentForcedStreamedTextures)
	{
		OutCurrentForcedStreamedTextures->Empty();
	}
	if (OutDesiredForcedStreamedTextures)
	{
		OutDesiredForcedStreamedTextures->Empty();
	}

	if (OutComponentsNeedingGrassMapRender)
	{
		OutComponentsNeedingGrassMapRender->Empty();
	}

	if (OutOutdatedComponents)
	{
		OutOutdatedComponents->Empty();
	}

	if (OutOutdatedGrassMaps)
	{
		*OutOutdatedGrassMaps = 0;
	}
		
	const UWorld* World = GetWorld();
	if (!World || World->IsGameWorld())
	{
		return;
	}

	// In either case we want to check the Grass textures stream state
	const bool bCheckStreamingState = OutDesiredForcedStreamedTextures || bInEnableForceResidentFlag;

	TArray<ULandscapeGrassType*> GrassTypes;
	float OutMaxDiscardDistance = 0.0f;
	GetGrassTypes(World, LandscapeMaterial, GrassTypes, OutMaxDiscardDistance);
	const bool bHasGrassTypes = GrassTypes.Num() > 0;

	const bool bIsOutermostPackageDirty = GetOutermost()->IsDirty();

	int32 OutdatedGrassMaps = 0;
	for (auto Component : LandscapeComponents)
	{
		if (Component != nullptr)
		{
			UTexture2D* Heightmap = Component->GetHeightmap();
			const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures();

			if (OutCurrentForcedStreamedTextures)
			{
				// check textures currently needing force streaming
				if (Heightmap->bForceMiplevelsToBeResident)
				{
					OutCurrentForcedStreamedTextures->Add(Heightmap);
				}

				for (auto WeightmapTexture : ComponentWeightmapTextures)
				{
					if (WeightmapTexture->bForceMiplevelsToBeResident)
					{
						OutCurrentForcedStreamedTextures->Add(WeightmapTexture);
					}
				}
			}

			if (OutOutdatedComponents && Component->IsGrassMapOutdated())
			{
				OutOutdatedComponents->Add(Component);
			}

			if (bHasGrassTypes || bBakeMaterialPositionOffsetIntoCollision)
			{
				if (Component->IsGrassMapOutdated() || !Component->GrassData->HasValidData() || GGrassUpdateAllOnRebuild != 0)
				{
					if (OutComponentsNeedingGrassMapRender)
					{
						OutComponentsNeedingGrassMapRender->Add(Component);
					}
					++OutdatedGrassMaps;

					if (bCheckStreamingState && !Component->AreTexturesStreamedForGrassMapRender())
					{
						if (OutDesiredForcedStreamedTextures)
						{
							OutDesiredForcedStreamedTextures->Add(Heightmap);
						}

						if (bInEnableForceResidentFlag)
						{
							Heightmap->bForceMiplevelsToBeResident = true;
						}

						for (auto WeightmapTexture : ComponentWeightmapTextures)
						{
							if (OutDesiredForcedStreamedTextures)
							{
								OutDesiredForcedStreamedTextures->Add(WeightmapTexture);
							}

							if (bInEnableForceResidentFlag)
							{
								WeightmapTexture->bForceMiplevelsToBeResident = true;
							}
						}
					}
				}
				// Don't count dirty component's if package is already dirty. 
				// Saving the dirty package will also save the updated grass maps.
				// This counter is used to know if grass maps need to be rebuilt.
				else if (Component->GrassData->bIsDirty && !bIsOutermostPackageDirty)
				{
					++OutdatedGrassMaps;
				}
			}
		}
	}

	if (OutOutdatedGrassMaps)
	{
		*OutOutdatedGrassMaps = OutdatedGrassMaps;
	}
}

void ALandscapeProxy::UpdateGrassData(bool bInShouldMarkDirty, FScopedSlowTask* InSlowTask)
{
	if (!GGrassEnable || !GetWorld() || GetWorld()->IsGameWorld())
	{
		return;
	}

	// see if we need to flush grass for any components
	TSet<UTexture2D*> DesiredForcedStreamedTextures;
	TSet<UTexture2D*> CurrentForcedStreamedTextures;
	TSet<ULandscapeComponent*> ComponentsNeedingGrassMapRender;
	TSet<ULandscapeComponent*> OutdatedComponents;
	int32 TotalOutdatedGrassMaps = 0;
	const bool bEnableForceResidentFlag = true;
	UpdateGrassDataStatus(&CurrentForcedStreamedTextures, &DesiredForcedStreamedTextures, &ComponentsNeedingGrassMapRender, &OutdatedComponents, bEnableForceResidentFlag, &TotalOutdatedGrassMaps);

	if (OutdatedComponents.Num())
	{
		FlushGrassComponents(&OutdatedComponents);
	}

	// Remove local count from global count
	TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
	NumTexturesToStreamForVisibleGrassMapRender = 0;

	// Remove local count from global count
	NumComponentsNeedingGrassMapRender = 0;

	// Wait for Texture Streaming
	for (UTexture2D* TextureToStream : DesiredForcedStreamedTextures)
	{
		FTextureCompilingManager::Get().FinishCompilation({TextureToStream});
		TextureToStream->WaitForStreaming();
		CurrentForcedStreamedTextures.Add(TextureToStream);
	}
	
	auto UpdateProgress = [InSlowTask](int Increment)
	{
		if (InSlowTask && Increment && ((InSlowTask->CompletedWork + Increment) <= InSlowTask->TotalAmountOfWork))
		{
			InSlowTask->EnterProgressFrame(Increment, FText::Format(LOCTEXT("GrassMaps_BuildGrassMapsProgress", "Building Grass Map {0} of {1})"), FText::AsNumber(InSlowTask->CompletedWork), FText::AsNumber(InSlowTask->TotalAmountOfWork)));
		}
	};

	// Take into consideration already dirty/rendered grass maps to update progress accordingly
	int32 AlreadyRenderedGrassMap = TotalOutdatedGrassMaps - ComponentsNeedingGrassMapRender.Num();
	UpdateProgress(AlreadyRenderedGrassMap);

	// Render grass data
	for(ULandscapeComponent* Component : ComponentsNeedingGrassMapRender)
	{
		if (Component->CanRenderGrassMap())
		{
			Component->RenderGrassMap();
			UpdateProgress(1);
		}
	}

	if (bInShouldMarkDirty && GetOutdatedGrassMapCount() > 0)
	{
		MarkPackageDirty();
	}

	// In Edit Layers, bForceMiplevelsToBeResident needs to remain true
	if (!HasLayersContent())
	{
		for (UTexture2D* TextureToStream : CurrentForcedStreamedTextures)
		{
			TextureToStream->bForceMiplevelsToBeResident = false;
		}
	}
}
#endif

void ALandscapeProxy::UpdateGrass(const TArray<FVector>& Cameras, bool bForceSync /* = false */)
{
	int32 InOutNumCompsCreated = 0;
	UpdateGrass(Cameras, InOutNumCompsCreated, bForceSync);
}

void ALandscapeProxy::UpdateGrass(const TArray<FVector>& Cameras, int32& InOutNumCompsCreated, bool bForceSync /* = false */)
{
	SCOPE_CYCLE_COUNTER(STAT_GrassUpdate);
	TRACE_CPUPROFILER_EVENT_SCOPE(ALandscapeProxy::UpdateGrass);

	// don't let it create grass actors inside a transaction: 
	if (GUndo != nullptr)
	{
		return;
	}

	if (GFrameNumberLastStaleCheck != GFrameNumber && GIgnoreExcludeBoxes == 0)
	{
		GFrameNumberLastStaleCheck = GFrameNumber;
		for (auto Iter = GGrassExclusionBoxes.CreateIterator(); Iter; ++Iter)
		{
			if (!Iter->Key.IsValid())
			{
				Iter.RemoveCurrent();
				GGrassExclusionChangeTag++;
			}
		}
	}

	if (GGrassEnable > 0)
	{
		UWorld* World = GetWorld();

		float GuardBand = GGuardBandMultiplier;
		float DiscardGuardBand = GGuardBandDiscardMultiplier;
		bool bCullSubsections = GCullSubsections > 0;
		bool bDisableGPUCull = GDisableGPUCull > 0;
		bool bDisableDynamicShadows = GDisableDynamicShadows > 0;
		int32 MaxInstancesPerComponent = FMath::Max<int32>(1024, GMaxInstancesPerComponent);
		int32 MaxTasks = GMaxAsyncTasks;
		const float CullDistanceScale = GGrassCullDistanceScale;

		if (World)
		{
#if WITH_EDITOR
			TArray<ULandscapeGrassType*> LandscapeGrassTypes;
			float GrassMaxDiscardDistance = 0.0f;
			GetGrassTypes(World, LandscapeMaterial, LandscapeGrassTypes, GrassMaxDiscardDistance);
#else
			// In non editor builds, cache grass types for performance.
			if (LandscapeMaterial != LandscapeMaterialCached)
			{
				LandscapeMaterialCached = LandscapeMaterial;
				LandscapeGrassTypes.Reset();
				GrassMaxDiscardDistance = 0.0f;
				GetGrassTypes(World, LandscapeMaterial, LandscapeGrassTypes, GrassMaxDiscardDistance);
			}
#endif
			// Cull grass max distance based on Cull Distance scale factor and on max GuardBand factor.
			float GrassMaxCulledDiscardDistance = GrassMaxDiscardDistance * GGrassCullDistanceScale *
			FMath::Max(GGuardBandDiscardMultiplier, GGuardBandMultiplier);
			float GrassMaxSquareDiscardDistance = GrassMaxCulledDiscardDistance * GrassMaxCulledDiscardDistance;
#if WITH_EDITOR


			int32 RequiredTexturesNotStreamedIn = 0;
			TSet<ULandscapeComponent*> ComponentsNeedingGrassMapRender;
			TSet<ULandscapeComponent*> OutdatedComponents;
			TSet<UTexture2D*> CurrentForcedStreamedTextures;
			TSet<UTexture2D*> DesiredForceStreamedTextures;
			const bool bEnableForceResidentFlag = false;
			
			// Do not pass in DesiredForceStreamedTextures because we build our own list
			UpdateGrassDataStatus(&CurrentForcedStreamedTextures, nullptr, &ComponentsNeedingGrassMapRender, &OutdatedComponents, bEnableForceResidentFlag);
			
			if(OutdatedComponents.Num())
			{
				FlushGrassComponents(&OutdatedComponents);
			}
#endif
			ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

			struct SortedLandscapeElement
			{
				SortedLandscapeElement(ULandscapeComponent* InComponent, float InMinDistance, const FBox& InBoundsBox)
				: Component(InComponent)
				, MinDistance(InMinDistance)
				, BoundsBox(InBoundsBox)
				{}

				ULandscapeComponent* Component;
				float MinDistance;
				FBox BoundsBox;
			};

			static TArray<SortedLandscapeElement> SortedLandscapeComponents;
			SortedLandscapeComponents.Reset(LandscapeComponents.Num());
			for (ULandscapeComponent* Component : LandscapeComponents)
			{
				// skip if we have no data and no way to generate it
				if (Component == nullptr || (World->IsGameWorld() && !Component->GrassData->HasWeightData()))
				{
					continue;
				}
				FBoxSphereBounds WorldBounds = Component->CalcBounds(Component->GetComponentTransform());
				float MinSqrDistanceToComponent = Cameras.Num() ? MAX_flt : 0.0f;
				for (const FVector& CameraPos : Cameras)
				{
					MinSqrDistanceToComponent = FMath::Min<float>(MinSqrDistanceToComponent, WorldBounds.ComputeSquaredDistanceFromBoxToPoint(CameraPos));
				}

				if (MinSqrDistanceToComponent > GrassMaxSquareDiscardDistance)
				{
					continue;
				}

				SortedLandscapeComponents.Emplace(Component, FMath::Sqrt(MinSqrDistanceToComponent), WorldBounds.GetBox());
			}
			
#if WITH_EDITOR
			// When editing landscape, prioritize components that are closer to camera for a more reactive update
			if (GLandscapeEditModeActive)
			{
				Algo::Sort(SortedLandscapeComponents, [](const SortedLandscapeElement& A, const SortedLandscapeElement& B) { return A.MinDistance < B.MinDistance; });
			}
#endif

			for (const SortedLandscapeElement& SortedLandscapeComponent : SortedLandscapeComponents)
			{
				ULandscapeComponent* Component = SortedLandscapeComponent.Component;
				float MinDistanceToComp = SortedLandscapeComponent.MinDistance;

				if (Component->ChangeTag != GGrassExclusionChangeTag)
				{
					Component->ActiveExcludedBoxes.Empty();
					if (GGrassExclusionBoxes.Num() && GIgnoreExcludeBoxes == 0)
					{
						const FBox& WorldBox = SortedLandscapeComponent.BoundsBox;

						for (const TPair<FWeakObjectPtr, FBox>& Pair : GGrassExclusionBoxes)
						{
							if (Pair.Value.Intersect(WorldBox))
							{
								Component->ActiveExcludedBoxes.AddUnique(Pair.Value);
							}
						}
					}
					Component->ChangeTag = GGrassExclusionChangeTag;
				}

				for (ULandscapeGrassType* GrassType : LandscapeGrassTypes)
				{
					if (GrassType)
					{
						if (World->IsGameWorld() && !Component->GrassData->Contains(GrassType))
						{
							continue;
						}

						int32 GrassVarietyIndex = -1;
						uint32 HaltonBaseIndex = 1;
						for (auto& GrassVariety : GrassType->GrassVarieties)
						{
							GrassVarietyIndex++;
							int32 EndCullDistance = GrassVariety.EndCullDistance.GetValue();
							if (GrassVariety.GrassMesh && GrassVariety.GrassDensity.GetValue() > 0.0f && EndCullDistance > 0)
							{
								float MustHaveDistance = GuardBand * (float)EndCullDistance * CullDistanceScale;
								float DiscardDistance = DiscardGuardBand * (float)EndCullDistance * CullDistanceScale;

								bool bUseHalton = !GrassVariety.bUseGrid;

								if (!bUseHalton && MinDistanceToComp > DiscardDistance)
								{
									continue;
								}

								FGrassBuilderBase ForSubsectionMath(this, Component, GrassVariety, FeatureLevel);

								int32 SqrtSubsections = 1;

								if (ForSubsectionMath.bHaveValidData && ForSubsectionMath.SqrtMaxInstances > 0)
								{
									SqrtSubsections = FMath::Clamp<int32>(FMath::CeilToInt(float(ForSubsectionMath.SqrtMaxInstances) / FMath::Sqrt((float)MaxInstancesPerComponent)), 1, 16);
								}
								int32 MaxInstancesSub = FMath::Square(ForSubsectionMath.SqrtMaxInstances / SqrtSubsections);

								if (bUseHalton && MinDistanceToComp > DiscardDistance)
								{
									HaltonBaseIndex += MaxInstancesSub * SqrtSubsections * SqrtSubsections;
									continue;
								}

								FBox LocalBox = Component->CachedLocalBox;
								FVector LocalExtentDiv = (LocalBox.Max - LocalBox.Min) * FVector(1.0f / float(SqrtSubsections), 1.0f / float(SqrtSubsections), 1.0f);
								for (int32 SubX = 0; SubX < SqrtSubsections; SubX++)
								{
									for (int32 SubY = 0; SubY < SqrtSubsections; SubY++)
									{
										float MinDistanceToSubComp = MinDistanceToComp;

										FBox WorldSubBox;

										if ((bCullSubsections && SqrtSubsections > 1) || Component->ActiveExcludedBoxes.Num())
										{
											FVector BoxMin;
											BoxMin.X = LocalBox.Min.X + LocalExtentDiv.X * float(SubX);
											BoxMin.Y = LocalBox.Min.Y + LocalExtentDiv.Y * float(SubY);
											BoxMin.Z = LocalBox.Min.Z;

											FVector BoxMax;
											BoxMax.X = LocalBox.Min.X + LocalExtentDiv.X * float(SubX + 1);
											BoxMax.Y = LocalBox.Min.Y + LocalExtentDiv.Y * float(SubY + 1);
											BoxMax.Z = LocalBox.Max.Z;

											FBox LocalSubBox(BoxMin, BoxMax);
											WorldSubBox = LocalSubBox.TransformBy(Component->GetComponentTransform());

											if (bCullSubsections && SqrtSubsections > 1)
											{
												MinDistanceToSubComp = Cameras.Num() ? MAX_flt : 0.0f;
												for (auto& Pos : Cameras)
												{
													MinDistanceToSubComp = FMath::Min<float>(MinDistanceToSubComp, ComputeSquaredDistanceFromBoxToPoint(WorldSubBox.Min, WorldSubBox.Max, Pos));
												}
												MinDistanceToSubComp = FMath::Sqrt(MinDistanceToSubComp);
											}
										}

										if (bUseHalton)
										{
											HaltonBaseIndex += MaxInstancesSub;  // we are going to pre-increment this for all of the continues...however we need to subtract later if we actually do this sub
										}

										if (MinDistanceToSubComp > DiscardDistance)
										{
											continue;
										}

										FCachedLandscapeFoliage::FGrassComp NewComp;
										NewComp.Key.BasedOn = Component;
										NewComp.Key.GrassType = GrassType;
										NewComp.Key.SqrtSubsections = SqrtSubsections;
										NewComp.Key.CachedMaxInstancesPerComponent = MaxInstancesPerComponent;
										NewComp.Key.SubsectionX = SubX;
										NewComp.Key.SubsectionY = SubY;
										NewComp.Key.NumVarieties = GrassType->GrassVarieties.Num();
										NewComp.Key.VarietyIndex = GrassVarietyIndex;

										bool bRebuildForBoxes = false;

										{
											FCachedLandscapeFoliage::FGrassComp* Existing = FoliageCache.CachedGrassComps.Find(NewComp.Key);
											if (Existing && !Existing->PreviousFoliage.IsValid() && Existing->ExclusionChangeTag != GGrassExclusionChangeTag && !Existing->PendingRemovalRebuild && !Existing->Pending)
											{
												for (const FBox& Box : Component->ActiveExcludedBoxes)
												{
													if (Box.Intersect(WorldSubBox))
													{
														NewComp.ExcludedBoxes.Add(Box);
													}
												}
												if (NewComp.ExcludedBoxes != Existing->ExcludedBoxes)
												{
													bRebuildForBoxes = true;
													NewComp.PreviousFoliage = Existing->Foliage;
													Existing->PendingRemovalRebuild = true;
												}
												else
												{
													Existing->ExclusionChangeTag = GGrassExclusionChangeTag;
												}
											}

											if (Existing || MinDistanceToSubComp > MustHaveDistance)
											{
												if (Existing)
												{
													Existing->Touch();
												}
												if (!bRebuildForBoxes)
												{
													continue;
												}
											}
										}

										if (!bRebuildForBoxes && !bForceSync && (InOutNumCompsCreated >= GGrassMaxCreatePerFrame || AsyncFoliageTasks.Num() >= MaxTasks))
										{
											continue; // one per frame, but we still want to touch the existing ones and we must do the rebuilds because we changed the tag
										}
										if (!bRebuildForBoxes)
										{
											for (const FBox& Box : Component->ActiveExcludedBoxes)
											{
												if (Box.Intersect(WorldSubBox))
												{
													NewComp.ExcludedBoxes.Add(Box);
												}
											}
										}
										NewComp.ExclusionChangeTag = GGrassExclusionChangeTag;

#if WITH_EDITOR
										// render grass data if we don't have any
										if (!Component->GrassData->HasValidData())
										{
											if (!Component->CanRenderGrassMap())
											{
												// we can't currently render grassmaps (eg shaders not compiled)
												continue;
											}
											else if (!Component->AreTexturesStreamedForGrassMapRender())
											{
												// we're ready to generate but our textures need streaming in
												DesiredForceStreamedTextures.Add(Component->GetHeightmap());
												const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures();
												
												for (UTexture2D* WeightmapTexture : ComponentWeightmapTextures)
												{
													DesiredForceStreamedTextures.Add(WeightmapTexture);
												}
												RequiredTexturesNotStreamedIn++;
												continue;
											}

											RenderCaptureInterface::FScopedCapture RenderCapture((GCaptureNextGrassUpdate != 0), TEXT("RenderGrassMap"));
											GCaptureNextGrassUpdate = FMath::Max(GCaptureNextGrassUpdate - 1, 0);

											QUICK_SCOPE_CYCLE_COUNTER(STAT_GrassRenderToTexture);
											Component->RenderGrassMap();
											ComponentsNeedingGrassMapRender.Remove(Component);
										}
#endif

										InOutNumCompsCreated++;

										SCOPE_CYCLE_COUNTER(STAT_FoliageGrassStartComp);

										// To guarantee consistency across platforms, we force the string to be lowercase and always treat it as an ANSI string.
										int32 FolSeed = FCrc::StrCrc32( StringCast<ANSICHAR>( *FString::Printf( TEXT("%s%s%d %d %d"), *GrassType->GetName().ToLower(), *Component->GetName().ToLower(), SubX, SubY, GrassVarietyIndex)).Get() );
										if (FolSeed == 0)
										{
											FolSeed++;
										}

										// Do not record the transaction of creating temp component for visualizations
										ClearFlags(RF_Transactional);
										bool PreviousPackageDirtyFlag = GetOutermost()->IsDirty();

										UGrassInstancedStaticMeshComponent* GrassInstancedStaticMeshComponent;
										{
											QUICK_SCOPE_CYCLE_COUNTER(STAT_GrassCreateComp);
											GrassInstancedStaticMeshComponent = NewObject<UGrassInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
										}
										NewComp.Foliage = GrassInstancedStaticMeshComponent;
										FoliageCache.CachedGrassComps.Add(NewComp);

										GrassInstancedStaticMeshComponent->Mobility = EComponentMobility::Static;
										GrassInstancedStaticMeshComponent->SetStaticMesh(GrassVariety.GrassMesh);
										GrassInstancedStaticMeshComponent->MinLOD = GrassVariety.MinLOD;
										GrassInstancedStaticMeshComponent->bSelectable = false;
										GrassInstancedStaticMeshComponent->bHasPerInstanceHitProxies = false;
										GrassInstancedStaticMeshComponent->bReceivesDecals = GrassVariety.bReceivesDecals;
										static FName NoCollision(TEXT("NoCollision"));
										GrassInstancedStaticMeshComponent->SetCollisionProfileName(NoCollision);
										GrassInstancedStaticMeshComponent->bDisableCollision = true;
										GrassInstancedStaticMeshComponent->SetCanEverAffectNavigation(false);
										GrassInstancedStaticMeshComponent->InstancingRandomSeed = FolSeed;
										GrassInstancedStaticMeshComponent->LightingChannels = GrassVariety.LightingChannels;
										GrassInstancedStaticMeshComponent->bCastStaticShadow = false;
										GrassInstancedStaticMeshComponent->CastShadow = (GrassVariety.bCastDynamicShadow || GrassVariety.bCastContactShadow) && !bDisableDynamicShadows;
										GrassInstancedStaticMeshComponent->bCastDynamicShadow = GrassVariety.bCastDynamicShadow && !bDisableDynamicShadows;
										GrassInstancedStaticMeshComponent->bCastContactShadow = GrassVariety.bCastContactShadow && !bDisableDynamicShadows;
										GrassInstancedStaticMeshComponent->OverrideMaterials = GrassVariety.OverrideMaterials;
										GrassInstancedStaticMeshComponent->bEvaluateWorldPositionOffset = true;
										GrassInstancedStaticMeshComponent->WorldPositionOffsetDisableDistance = GrassVariety.InstanceWorldPositionOffsetDisableDistance;

										GrassInstancedStaticMeshComponent->PrecachePSOs();

										const FMeshMapBuildData* MeshMapBuildData = Component->GetMeshMapBuildData();

										if (GrassVariety.bUseLandscapeLightmap
											&& GrassVariety.GrassMesh->GetNumLODs() > 0
											&& MeshMapBuildData
											&& MeshMapBuildData->LightMap)
										{
											GrassInstancedStaticMeshComponent->SetLODDataCount(GrassVariety.GrassMesh->GetNumLODs(), GrassVariety.GrassMesh->GetNumLODs());

											FLightMapRef GrassLightMap = new FLandscapeGrassLightMap(*MeshMapBuildData->LightMap->GetLightMap2D());
											FShadowMapRef GrassShadowMap = MeshMapBuildData->ShadowMap ? new FLandscapeGrassShadowMap(*MeshMapBuildData->ShadowMap->GetShadowMap2D()) : nullptr;

											for (auto& LOD : GrassInstancedStaticMeshComponent->LODData)
											{
												// This trasient OverrideMapBuildData will be cleaned up by UMapBuildDataRegistry::CleanupTransientOverrideMapBuildData() if the underlying MeshMapBuildData is gone
												LOD.OverrideMapBuildData = MakeUnique<FMeshMapBuildData>();
												LOD.OverrideMapBuildData->LightMap = GrassLightMap;
												LOD.OverrideMapBuildData->ShadowMap = GrassShadowMap;
												LOD.OverrideMapBuildData->ResourceCluster = MeshMapBuildData->ResourceCluster;
											}
										}

										if (!Cameras.Num() || bDisableGPUCull)
										{
											// if we don't have any cameras, then we are rendering landscape LOD materials or somesuch and we want to disable culling
											GrassInstancedStaticMeshComponent->InstanceStartCullDistance = 0;
											GrassInstancedStaticMeshComponent->InstanceEndCullDistance = 0;
										}
										else
										{
											GrassInstancedStaticMeshComponent->InstanceStartCullDistance = GrassVariety.StartCullDistance.GetValue() * CullDistanceScale;
											GrassInstancedStaticMeshComponent->InstanceEndCullDistance = GrassVariety.EndCullDistance.GetValue() * CullDistanceScale;
										}

										//@todo - take the settings from a UFoliageType object.  For now, disable distance field lighting on grass so we don't hitch.
										GrassInstancedStaticMeshComponent->bAffectDistanceFieldLighting = false;

										{
											QUICK_SCOPE_CYCLE_COUNTER(STAT_GrassAttachComp);

											GrassInstancedStaticMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
											FTransform DesiredTransform = GetRootComponent()->GetComponentTransform();
											DesiredTransform.RemoveScaling();
											GrassInstancedStaticMeshComponent->SetWorldTransform(DesiredTransform);

											FoliageComponents.Add(GrassInstancedStaticMeshComponent);
										}

										FAsyncGrassBuilder* Builder;

										{
											QUICK_SCOPE_CYCLE_COUNTER(STAT_GrassCreateBuilder);

											uint32 HaltonIndexForSub = 0;
											if (bUseHalton)
											{
												check(HaltonBaseIndex > (uint32)MaxInstancesSub);
												HaltonIndexForSub = HaltonBaseIndex - (uint32)MaxInstancesSub;
											}
											Builder = new FAsyncGrassBuilder(this, Component, GrassType, GrassVariety, FeatureLevel, GrassInstancedStaticMeshComponent, SqrtSubsections, SubX, SubY, HaltonIndexForSub, NewComp.ExcludedBoxes);
										}

										if (Builder->bHaveValidData)
										{
											FAsyncTask<FAsyncGrassTask>* Task = new FAsyncTask<FAsyncGrassTask>(Builder, NewComp.Key, GrassInstancedStaticMeshComponent);

											Task->StartBackgroundTask();

											AsyncFoliageTasks.Add(Task);
										}
										else
										{
											delete Builder;
										}
										{
											QUICK_SCOPE_CYCLE_COUNTER(STAT_GrassRegisterComp);

											GrassInstancedStaticMeshComponent->RegisterComponent();
										}

										SetFlags(RF_Transactional);
										GetOutermost()->SetDirtyFlag(PreviousPackageDirtyFlag);
									}
								}
							}
						}
					}
				}
			}

#if WITH_EDITOR

			TotalTexturesToStreamForVisibleGrassMapRender -= NumTexturesToStreamForVisibleGrassMapRender;
			NumTexturesToStreamForVisibleGrassMapRender = RequiredTexturesNotStreamedIn;
			TotalTexturesToStreamForVisibleGrassMapRender += NumTexturesToStreamForVisibleGrassMapRender;

			{
				int32 NumComponentsRendered = 0;
				int32 NumComponentsUnableToRender = 0;
				if ((LandscapeGrassTypes.Num() > 0 && GPrerenderGrassmaps > 0) || bBakeMaterialPositionOffsetIntoCollision)
				{
					// try to render some grassmaps
					TArray<ULandscapeComponent*> ComponentsToRender;
					for (auto Component : ComponentsNeedingGrassMapRender)
					{
						if (Component->CanRenderGrassMap())
						{
							if (Component->AreTexturesStreamedForGrassMapRender())
							{
								// We really want to throttle the number based on component size.
								if (NumComponentsRendered <= 4)
								{
									ComponentsToRender.Add(Component);
									NumComponentsRendered++;
								}
							}
							else
							if (TotalTexturesToStreamForVisibleGrassMapRender == 0)
							{
								// Force stream in other heightmaps but only if we're not waiting for the textures 
								// near the camera to stream in
								DesiredForceStreamedTextures.Add(Component->GetHeightmap());
								const TArray<UTexture2D*>& ComponentWeightmapTextures = Component->GetWeightmapTextures();
								
								for (UTexture2D* WeightmapTexture : ComponentWeightmapTextures)
								{
									DesiredForceStreamedTextures.Add(WeightmapTexture);
								}
							}
						}
						else
						{
							NumComponentsUnableToRender++;
						}
					}
					if (ComponentsToRender.Num())
					{
						RenderCaptureInterface::FScopedCapture RenderCapture((GCaptureNextGrassUpdate != 0), TEXT("RenderGrassMaps"));
						GCaptureNextGrassUpdate = FMath::Max(GCaptureNextGrassUpdate - 1, 0);

						RenderGrassMaps(ComponentsToRender, LandscapeGrassTypes);
					}
				}

				NumComponentsNeedingGrassMapRender = ComponentsNeedingGrassMapRender.Num() - NumComponentsRendered - NumComponentsUnableToRender;

				// Update resident flags
				for (auto Texture : DesiredForceStreamedTextures.Difference(CurrentForcedStreamedTextures))
				{
					Texture->bForceMiplevelsToBeResident = true;
				}

				// In Edit Layers, bForceMiplevelsToBeResident needs to remain true
				if (!HasLayersContent())
				{
					for (auto Texture : CurrentForcedStreamedTextures.Difference(DesiredForceStreamedTextures))
					{
						Texture->bForceMiplevelsToBeResident = false;
					}
				}
			}
#endif
		}
	}

	TSet<UHierarchicalInstancedStaticMeshComponent *> StillUsed;
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Grass_StillUsed);

		// trim cached items based on time, pending and emptiness
		double OldestToKeepTime = FPlatformTime::Seconds() - GMinTimeToKeepGrass;
		uint32 OldestToKeepFrame = GFrameNumber - GMinFramesToKeepGrass * GetGrassUpdateInterval();
		for (FCachedLandscapeFoliage::TGrassSet::TIterator Iter(FoliageCache.CachedGrassComps); Iter; ++Iter)
		{
			const FCachedLandscapeFoliage::FGrassComp& GrassItem = *Iter;
			UHierarchicalInstancedStaticMeshComponent *Used = GrassItem.Foliage.Get();
			UHierarchicalInstancedStaticMeshComponent *UsedPrev = GrassItem.PreviousFoliage.Get();
			bool bOld =
				!GrassItem.Pending &&
				(
					!GrassItem.Key.BasedOn.Get() ||
					!GrassItem.Key.GrassType.Get() ||
					!Used ||
				(GrassItem.LastUsedFrameNumber < OldestToKeepFrame && GrassItem.LastUsedTime < OldestToKeepTime)
				);
			if (bOld)
			{
				Iter.RemoveCurrent();
			}
			else if (Used || UsedPrev)
			{
				if (!StillUsed.Num())
				{
					StillUsed.Reserve(FoliageCache.CachedGrassComps.Num());
				}
				if (Used)
				{
					StillUsed.Add(Used);
				}
				if (UsedPrev)
				{
					StillUsed.Add(UsedPrev);
				}
			}
		}
	}
	if (StillUsed.Num() < FoliageComponents.Num())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Grass_DelComps);

		// delete components that are no longer used
		for (int32 Index = 0; Index < FoliageComponents.Num(); Index++)
		{
			UHierarchicalInstancedStaticMeshComponent* HComponent = FoliageComponents[Index];
			if (!StillUsed.Contains(HComponent))
			{
				{
					SCOPE_CYCLE_COUNTER(STAT_FoliageGrassDestoryComp);
					if (HComponent)
					{
						HComponent->ClearInstances();
						HComponent->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
						HComponent->DestroyComponent();
					}
					FoliageComponents.RemoveAtSwap(Index--);
				}
				if (!bForceSync)
				{
					break; // one per frame is fine
				}
			}
		}
	}
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_Grass_FinishAsync);
		// finish async tasks
		for (int32 Index = 0; Index < AsyncFoliageTasks.Num(); Index++)
		{
			FAsyncTask<FAsyncGrassTask>* Task = AsyncFoliageTasks[Index];
			if (bForceSync)
			{
				Task->EnsureCompletion();
			}
			if (Task->IsDone())
			{
				SCOPE_CYCLE_COUNTER(STAT_FoliageGrassEndComp);
				FAsyncGrassTask& Inner = Task->GetTask();
				AsyncFoliageTasks.RemoveAtSwap(Index--);
				UGrassInstancedStaticMeshComponent* GrassISMComponent = Cast<UGrassInstancedStaticMeshComponent>(Inner.Foliage.Get());
				int32 NumBuiltRenderInstances = Inner.Builder->InstanceBuffer.GetNumInstances();
				//UE_LOG(LogCore, Display, TEXT("%d instances in %4.0fms     %6.0f instances / sec"), NumBuiltRenderInstances, 1000.0f * float(Inner.Builder->BuildTime), float(NumBuiltRenderInstances) / float(Inner.Builder->BuildTime));

				if (GrassISMComponent && StillUsed.Contains(GrassISMComponent))
				{
					if (NumBuiltRenderInstances > 0)
					{
						QUICK_SCOPE_CYCLE_COUNTER(STAT_FoliageGrassEndComp_AcceptPrebuiltTree);

						if (!GrassISMComponent->PerInstanceRenderData.IsValid())
						{
							GrassISMComponent->InitPerInstanceRenderData(true, &Inner.Builder->InstanceBuffer, Inner.Builder->RequireCPUAccess);
						}
						else
						{
							GrassISMComponent->PerInstanceRenderData->UpdateFromPreallocatedData(Inner.Builder->InstanceBuffer);
						}

						GrassISMComponent->AcceptPrebuiltTree(Inner.Builder->InstanceData, Inner.Builder->ClusterTree, Inner.Builder->OutOcclusionLayerNum, NumBuiltRenderInstances);
						if (bForceSync && GetWorld())
						{
							QUICK_SCOPE_CYCLE_COUNTER(STAT_FoliageGrassEndComp_SyncUpdate);
							GrassISMComponent->RecreateRenderState_Concurrent();
						}
					}
				}
				FCachedLandscapeFoliage::FGrassComp* Existing = FoliageCache.CachedGrassComps.Find(Inner.Key);
				if (Existing)
				{
					Existing->Pending = false;
					if (Existing->PreviousFoliage.IsValid())
					{
						SCOPE_CYCLE_COUNTER(STAT_FoliageGrassDestoryComp);
						UHierarchicalInstancedStaticMeshComponent* HComponent = Existing->PreviousFoliage.Get();
						if (HComponent)
						{
							HComponent->ClearInstances();
							HComponent->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepRelative, false));
							HComponent->DestroyComponent();
						}
						FoliageComponents.RemoveSwap(HComponent);
						Existing->PreviousFoliage = nullptr;
					}

					Existing->Touch();
				}
				delete Task;
				if (!bForceSync)
				{
					break; // one per frame is fine
				}
			}
		}
	}
}

FAsyncGrassTask::FAsyncGrassTask(FAsyncGrassBuilder* InBuilder, const FCachedLandscapeFoliage::FGrassCompKey& InKey, UHierarchicalInstancedStaticMeshComponent* InFoliage)
	: Builder(InBuilder)
	, Key(InKey)
	, Foliage(InFoliage)
{
}

void FAsyncGrassTask::DoWork()
{
	Builder->Build();
}

FAsyncGrassTask::~FAsyncGrassTask()
{
	delete Builder;
}

static void FlushGrass(const TArray<FString>& Args)
{
	for (ALandscapeProxy* Landscape : TObjectRange<ALandscapeProxy>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
	{
		Landscape->FlushGrassComponents();
	}
}

static void FlushGrassPIE(const TArray<FString>& Args)
{
	for (ALandscapeProxy* Landscape : TObjectRange<ALandscapeProxy>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::Garbage))
	{
		Landscape->FlushGrassComponents(nullptr, false);
	}
}

static void DumpExclusionBoxes(const TArray<FString>& Args)
{
	for (const TPair<FWeakObjectPtr, FBox>& Pair : GGrassExclusionBoxes)
	{
		UObject* Owner = Pair.Key.Get();
		UE_LOG(LogCore, Warning, TEXT("%f %f %f   %f %f %f   %s"),
			Pair.Value.Min.X,
			Pair.Value.Min.Y,
			Pair.Value.Min.Z,
			Pair.Value.Max.X,
			Pair.Value.Max.Y,
			Pair.Value.Max.Z,
			Owner ? *Owner->GetFullName() : TEXT("[stale]")
		);
	}
}

static FAutoConsoleCommand FlushGrassCmd(
	TEXT("grass.FlushCache"),
	TEXT("Flush the grass cache, debugging."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FlushGrass)
	);

static FAutoConsoleCommand FlushGrassCmdPIE(
	TEXT("grass.FlushCachePIE"),
	TEXT("Flush the grass cache, debugging."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FlushGrassPIE)
	);

static FAutoConsoleCommand DumpExclusionBoxesCmd(
	TEXT("grass.DumpExclusionBoxes"),
	TEXT("Print the exclusion boxes, debugging."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpExclusionBoxes)
);


#undef LOCTEXT_NAMESPACE
