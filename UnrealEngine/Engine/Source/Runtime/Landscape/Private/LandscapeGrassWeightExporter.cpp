// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeGrassWeightExporter.h"
#include "SceneRendererInterface.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Materials/Material.h"
#include "LandscapeGrassType.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "LandscapeRender.h"
#include "MeshPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "SimpleMeshDrawCommandPass.h"
#include "TextureResource.h"
#include "RenderCaptureInterface.h"
#include "ShaderPlatformCachedIniValue.h"
#include "LandscapePrivate.h"

#if UE_BUILD_DEBUG
#include "Misc/FileHelper.h"
#endif

class FLandscapeGrassWeightVS;
class FLandscapeGrassWeightPS;

int32 GRenderCaptureNextGrassmapDraws = 0;
static FAutoConsoleVariableRef CVarRenderCaptureNextGrassmapDraws(
	TEXT("grass.GrassMap.RenderCaptureNextDraws"),
	GRenderCaptureNextGrassmapDraws,
	TEXT("Trigger render captures during the next N grassmap draw calls."));

extern int32 GGrassMapAlwaysBuildRuntimeGenerationResources;
extern int32 GGrassMapUseRuntimeGeneration;

BEGIN_SHADER_PARAMETER_STRUCT(FLandscapeGrassPassParameters, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FLandscapeGrassWeightShaderElementData : public FMeshMaterialShaderElementData
{
public:
	int32 OutputPass;
	FVector2f RenderOffset;
};

static bool ShouldCacheLandscapeGrassShaders(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	const bool bIsEditorPlatform = 
		IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
		EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData);

#if WITH_EDITOR
	static FShaderPlatformCachedIniValue<int32> GrassMapsUseRuntimeGenerationPerPlatform(TEXT("grass.GrassMap.UseRuntimeGeneration"));
	const bool bPlatformUsesRuntimeGen = (GrassMapsUseRuntimeGenerationPerPlatform.Get(Parameters.Platform) != 0);
#else
	const bool bPlatformUsesRuntimeGen = (GGrassMapUseRuntimeGeneration != 0);
#endif // WITH_EDITOR

	const bool bShouldBuildForPlatform = 
		bIsEditorPlatform ||
		GGrassMapAlwaysBuildRuntimeGenerationResources ||
		bPlatformUsesRuntimeGen;

	const bool bIsFixedGridVertexFactory =
		Parameters.VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLandscapeFixedGridVertexFactory"), FNAME_Find));

	// We only need grass weight shaders for Landscape fixed grid vertex factories
	// And only for platforms that have runtime generation enabled or are editor platforms (or if we are always building resources)
	const bool bIsLandscapeRelated = (Parameters.MaterialParameters.bIsUsedWithLandscape || Parameters.MaterialParameters.bIsSpecialEngineMaterial);

	const bool bShouldCache =
		bIsLandscapeRelated &&
		bIsFixedGridVertexFactory &&
		bShouldBuildForPlatform;

	return bShouldCache;
}

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
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
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
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

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
		PassUniformBuffer.Bind(Initializer.ParameterMap, FSceneTextureUniformParameters::FTypeInfo::GetStructMetadata()->GetShaderVariableName());
	}

	FLandscapeGrassWeightPS()
	{}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FLandscapeGrassWeightShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		ShaderBindings.Add(OutputPassParameter, ShaderElementData.OutputPass);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FLandscapeGrassWeightPS, TEXT("/Engine/Private/LandscapeGrassWeight.usf"), TEXT("PSMain"), SF_Pixel);

void UE::Landscape::Grass::AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes)
{
	InOutShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	InOutShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();
}

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

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FLandscapeGrassWeightVS>();
	ShaderTypes.AddShaderType<FLandscapeGrassWeightPS>();

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	TMeshProcessorShaders<
		FLandscapeGrassWeightVS,
		FLandscapeGrassWeightPS> PassShaders;
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


void FLandscapeGrassWeightExporter_RenderThread::RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList)
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

	FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(
		TargetSize,
		PF_B8G8R8A8,
		FClearValueBinding(),
		ETextureCreateFlags::RenderTargetable);

	FRDGTextureRef OutputTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("LandscapeGrassMapRenderTarget"), ERDGTextureFlags::None);

	auto* PassParameters = GraphBuilder.AllocParameters<FLandscapeGrassPassParameters>();
	PassParameters->View = View->ViewUniformBuffer;
	PassParameters->Scene = GetSceneUniformBufferRef(GraphBuilder, *View);
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

					PassMeshProcessor.AddMeshBatch(Mesh, DefaultBatchElementMask, ComponentInfo.NumPasses, ComponentInfo.ViewOffset, PassOffsetX, ComponentInfo.FirstHeightMipsPassIndex, HeightMips, ComponentInfo.SceneProxy);
				}
			}
		});

	if (AsyncReadbackPtr != nullptr)
	{
		AsyncReadbackPtr->StartReadback_RenderThread(GraphBuilder, OutputTexture);
	}

	GraphBuilder.Execute();
}


FLandscapeGrassWeightExporter::FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap, bool bInNeedsHeightmap, const TArray<int32>& InHeightMips)
	: FLandscapeGrassWeightExporter_RenderThread(InHeightMips)
	, LandscapeProxy(InLandscapeProxy)
	, ComponentSizeVerts(InLandscapeProxy->ComponentSizeQuads + 1)
	, SubsectionSizeQuads(InLandscapeProxy->SubsectionSizeQuads)
	, NumSubsections(InLandscapeProxy->NumSubsections)
{
	check(InLandscapeComponents.Num() > 0);
	SceneInterface = InLandscapeComponents[0]->GetScene();

	// todo: use a 2d target?
	const int32 SingleTileWidth = ComponentSizeVerts;
	TargetSize = FIntPoint(0, ComponentSizeVerts);

	// First compute the total render target size and prepare ComponentInfos (each component has its own number of needed passes because some might have different materials, thus different grass types) :
	ComponentInfos.Reserve(InLandscapeComponents.Num());
	int32 CurrentPixelOffsetX = 0;
	for (ULandscapeComponent* Component : InLandscapeComponents)
	{
		ensure(Component->SceneProxy);

		FComponentInfo& ComponentInfo = ComponentInfos.Add_GetRef(FComponentInfo(Component, bInNeedsGrassmap, bInNeedsHeightmap, InHeightMips));
		ComponentInfo.PixelOffsetX = CurrentPixelOffsetX;

		CurrentPixelOffsetX += ComponentInfo.NumPasses * SingleTileWidth;
	}
	TargetSize.X = CurrentPixelOffsetX;

	FIntPoint TargetSizeMinusOne(TargetSize - FIntPoint(1, 1));
	PassOffsetX = 2.0f * (float)SingleTileWidth / (float)TargetSize.X;

	// Then compute FComponentInfo's ViewOffset with the knowledge of the total render target size : 
	for (FComponentInfo& ComponentInfo : ComponentInfos)
	{
		FIntPoint ComponentOffset = (ComponentInfo.Component->GetSectionBase() - LandscapeProxy->LandscapeSectionOffset);
		FVector2D ViewOffset(-ComponentOffset.X, ComponentOffset.Y);
		ViewOffset.X += ComponentInfo.PixelOffsetX;
		ViewOffset /= (FVector2D(TargetSize) * 0.5f);
		ComponentInfo.ViewOffset = ViewOffset;
	}

	// center of target area in world
	FVector TargetCenter = LandscapeProxy->GetTransform().TransformPosition(FVector(TargetSizeMinusOne, 0.f) * 0.5f);

	// extent of target in world space
	FVector TargetExtent = FVector(TargetSize, 0.0f) * LandscapeProxy->GetActorScale() * 0.5f;

	ViewOrigin = TargetCenter;
	ViewRotationMatrix = FInverseRotationMatrix(LandscapeProxy->GetActorRotation());
	ViewRotationMatrix *= FMatrix(FPlane(1.0f, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, -1.0f, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, -1.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f));

	const float ZOffset = UE_OLD_WORLD_MAX;
	ProjectionMatrix = FReversedZOrthoMatrix(
		TargetExtent.X,
		TargetExtent.Y,
		0.5f / ZOffset,
		ZOffset);

	UE::RenderCommandPipe::FSyncScope SyncScope;

	RenderCaptureInterface::FScopedCapture RenderCapture((GRenderCaptureNextGrassmapDraws != 0), TEXT("LandscapeGrassmapCapture"));
	GRenderCaptureNextGrassmapDraws = FMath::Max(0, GRenderCaptureNextGrassmapDraws - 1);

	// render
	FLandscapeGrassWeightExporter_RenderThread* Exporter = this;
	ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
		[Exporter](FRHICommandListImmediate& RHICmdList)
		{
			Exporter->RenderLandscapeComponentToTexture_RenderThread(RHICmdList);
			FlushPendingDeleteRHIResources_RenderThread();
		});
}


struct FByteBuffer2DView : public IBuffer2DView<uint8>
{
	uint8* BufferStart = nullptr;
	int32 ByteStrideY = 0;
	int32 ByteStrideX = 0;
	int32 NumX = 0;
	int32 NumY = 0;

	// copy elements from buffer to Dest, in X then Y order
	virtual void CopyTo(uint8* Dest, int32 SizeInBytes) const override
	{
		for (int Y = 0; SizeInBytes > 0 && Y < NumY; Y++)
		{
			uint8* Src = BufferStart + Y * ByteStrideY;
			int32 CopyCountX = FMath::Min(SizeInBytes, NumX);
			// we can't use memcpy because of the ByteStride
			while (CopyCountX--)
			{
				*Dest = *Src;
				Dest++;
				Src += ByteStrideX;
			}
			SizeInBytes -= NumX;
		}
	}

	// copy elements from buffer to Dest, in X then Y order, return true if the copied data is all zero
	virtual bool CopyToAndCalcIsAllZero(uint8* Dest, int32 SizeInBytes) const override
	{
		uint8 MaxBits = 0;
		for (int Y = 0; SizeInBytes > 0 && Y < NumY; Y++)
		{
			uint8* Src = BufferStart + Y * ByteStrideY;
			int32 CopyCountX = FMath::Min(SizeInBytes, NumX);
			// we can't use memcpy because of the ByteStride
			while (CopyCountX--)
			{
				uint8 Value = *Src;
				MaxBits = MaxBits | Value;
				*Dest = Value;
				Dest++;
				Src += ByteStrideX;
			}
			SizeInBytes -= NumX;
		}
		return (MaxBits == 0);
	}

	virtual int32 Num() const override { return NumX * NumY; }
};

// FColor memory layout matches our BGRA GPU layout only on little endian CPUs!
#if PLATFORM_LITTLE_ENDIAN
	#define BGRA_AS_FCOLOR_BLUE B
	#define BGRA_AS_FCOLOR_GREEN G
	#define BGRA_AS_FCOLOR_RED R
	#define BGRA_AS_FCOLOR_ALPHA A
#else
	#define BGRA_AS_FCOLOR_BLUE A
	#define BGRA_AS_FCOLOR_GREEN R
	#define BGRA_AS_FCOLOR_RED G
	#define BGRA_AS_FCOLOR_ALPHA B
#endif // PLATFORM_LITTLE_ENDIAN

struct FHeightBuffer2DView : IBuffer2DView<uint16>
{
	FColor* BufferStart = nullptr;
	int32 StrideY = 0;
	int32 NumX = 0;
	int32 NumY = 0;

	// copy elements from buffer to Dest, in X then Y order
	virtual void CopyTo(uint16* Dest, int32 Count) const override
	{
		for (int y = 0; Count > 0 && y < NumY; y++)
		{
			FColor* Src = BufferStart + y * StrideY;
			int32 CopyCountX = FMath::Min(Count, NumX);
			while (CopyCountX--)
			{
				*Dest = (((uint16) Src->BGRA_AS_FCOLOR_RED) << 8) + (uint16)(Src->BGRA_AS_FCOLOR_GREEN);
				Dest++;
				Src++;
			}
			Count -= NumX;
		}
	}

	// copy elements from buffer to Dest, in X then Y order
	virtual bool CopyToAndCalcIsAllZero(uint16* Dest, int32 Count) const override
	{
		unimplemented()
		return true;
	}

	virtual int32 Num() const override { return NumX * NumY; }
};


void FLandscapeGrassWeightExporter::FreeAsyncReadback()
{
	check(AsyncReadbackPtr != nullptr);
	AsyncReadbackPtr->QueueDeletionFromGameThread();
	AsyncReadbackPtr = nullptr;
}

void FLandscapeGrassWeightExporter::CancelAndSelfDestruct()
{
	check(AsyncReadbackPtr != nullptr);

	// Cancel the readback, and queue destruction on the render thread
	AsyncReadbackPtr->CancelAndSelfDestruct();
	AsyncReadbackPtr = nullptr;

	// Queue destruction of FLandscapeGrassWeightExporter, also on the render thread
	FLandscapeGrassWeightExporter* Exporter = this;
	ENQUEUE_RENDER_COMMAND(FCancelAndDestructCommand)(
		[Exporter](FRHICommandListImmediate& RHICmdList)
		{
			check(Exporter->AsyncReadbackPtr == nullptr);
			delete Exporter;
		});
}

TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FLandscapeGrassWeightExporter::FetchResults(bool bFreeAsyncReadback)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FetchResults);
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> Results;
	TArray<FColor> Samples;

	check(AsyncReadbackPtr != nullptr);

	{
		FIntPoint Size;
		Samples = AsyncReadbackPtr->TakeResults(&Size);
		if (bFreeAsyncReadback)
		{
			FreeAsyncReadback();
		}
		check(Size == TargetSize);
	}

	Results.Reserve(ComponentInfos.Num());
	FHeightBuffer2DView HeightData;
	TMap<ULandscapeGrassType*, IBuffer2DView<uint8>*> WeightData;

	for (auto& ComponentInfo : ComponentInfos)
	{
		ULandscapeComponent* Component = ComponentInfo.Component;

		TUniquePtr<FLandscapeComponentGrassData> NewGrassData = MakeUnique<FLandscapeComponentGrassData>(Component);

		int32 ComponentSizeVerts2 = FMath::Square(ComponentSizeVerts);

		HeightData.NumX = ComponentSizeVerts;
		HeightData.NumY = ComponentSizeVerts;
		HeightData.StrideY = TargetSize.X;

#if WITH_EDITORONLY_DATA
		NewGrassData->HeightMipData.Empty(HeightMips.Num());
#endif // WITH_EDITORONLY_DATA

		WeightData.Empty();

		// this array is in 1:1 correspondence with ComponentInfo.RequestedGrassTypes
		TArray<FByteBuffer2DView> GrassWeightArrays;
		GrassWeightArrays.SetNum(ComponentInfo.RequestedGrassTypes.Num());

		for (int32 Index = 0; Index < GrassWeightArrays.Num(); Index++)
		{
			FByteBuffer2DView* WeightView = &GrassWeightArrays[Index];
			ULandscapeGrassType* GrassType = ComponentInfo.RequestedGrassTypes[Index];

			WeightView->NumX = ComponentSizeVerts;
			WeightView->NumY = ComponentSizeVerts;
			WeightView->ByteStrideX = 4;
			WeightView->ByteStrideY = TargetSize.X * 4;
			WeightView->BufferStart = nullptr;

			// Note: WeightData points directly at the elements of GrassWeightArrays (DO NOT REALLOCATE GRASSWEIGHTARRAYS)
			WeightData.Add(GrassType, WeightView);
		}

		// output debug bitmap
#if UE_BUILD_DEBUG
		static bool bOutputGrassBitmap = false;
		if (bOutputGrassBitmap)
		{
			FString TempPath = FPaths::ScreenShotDir();
			TempPath += TEXT("/GrassDebug");
			IFileManager::Get().MakeDirectory(*TempPath, true);
			FFileHelper::CreateBitmap(*(TempPath / "Grass"), TargetSize.X, TargetSize.Y, Samples.GetData(), nullptr, &IFileManager::Get(), nullptr, ComponentInfo.RequestedGrassTypes.Num() >= 2);
		}
#endif

		int32 GrassTypeCount = ComponentInfo.RequestedGrassTypes.Num();
		for (int32 PassIdx = 0; PassIdx < ComponentInfo.NumPasses; PassIdx++)
		{
			FColor* SampleData = &Samples[ComponentInfo.PixelOffsetX + PassIdx * ComponentSizeVerts];

			if (PassIdx < ComponentInfo.FirstHeightMipsPassIndex)
			{
				if (PassIdx == 0)	// height in RG, grass weights in BA
				{
					HeightData.BufferStart = SampleData;
					if (GrassTypeCount > 0)
					{
						GrassWeightArrays[0].BufferStart = &SampleData->BGRA_AS_FCOLOR_BLUE;
						if (GrassTypeCount > 1)
						{
							GrassWeightArrays[1].BufferStart = &SampleData->BGRA_AS_FCOLOR_ALPHA;
						}
					}
				}
				else
				{
					int32 TypeIdx = PassIdx * 4 - 2;

					GrassWeightArrays[TypeIdx+0].BufferStart = &SampleData->BGRA_AS_FCOLOR_RED;
					if (GrassTypeCount > TypeIdx+1)
					{
						GrassWeightArrays[TypeIdx+1].BufferStart = &SampleData->BGRA_AS_FCOLOR_GREEN;
						if (GrassTypeCount > TypeIdx + 2)
						{
							GrassWeightArrays[TypeIdx + 2].BufferStart = &SampleData->BGRA_AS_FCOLOR_BLUE;
							if (GrassTypeCount > TypeIdx + 3)
							{
								GrassWeightArrays[TypeIdx + 3].BufferStart = &SampleData->BGRA_AS_FCOLOR_ALPHA;
							}
						}
					}
				}
			}
			else // PassIdx >= FirstHeightMipsPassIndex
			{
#if WITH_EDITORONLY_DATA
				const int32 Mip = HeightMips[PassIdx - ComponentInfo.FirstHeightMipsPassIndex];
				int32 MipSizeVerts = NumSubsections * (SubsectionSizeQuads >> Mip);
				TArray<uint16>& MipHeightData = NewGrassData->HeightMipData.Add(Mip);
				MipHeightData.SetNumUninitialized(MipSizeVerts* MipSizeVerts);
				uint16* DstMipHeight = MipHeightData.GetData();
				for (int32 y = 0; y < MipSizeVerts; y++)
				{
					FColor* SrcSample = &SampleData[y * TargetSize.X];
					for (int32 x = 0; x < MipSizeVerts; x++)
					{
						*DstMipHeight++ = (((uint16)SrcSample->BGRA_AS_FCOLOR_RED) << 8) + (uint16)(SrcSample->BGRA_AS_FCOLOR_GREEN);
						SrcSample++;
					}
				}
#endif // WITH_EDITORONLY_DATA
			}
		}

		#undef BGRA_AS_FCOLOR_BLUE
		#undef BGRA_AS_FCOLOR_GREEN
		#undef BGRA_AS_FCOLOR_RED
		#undef BGRA_AS_FCOLOR_ALPHA
		
		NewGrassData->InitializeFrom(&HeightData, WeightData, /* bStripEmptyWeights = */ true);
		Results.Add(Component, MoveTemp(NewGrassData));
	}

	return Results;
}

void FLandscapeGrassWeightExporter::ApplyResults()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ApplyResults);

	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> NewGrassData = FetchResults(/* bFreeAsyncReadback = */ true);
	ApplyResults(NewGrassData);
}

void FLandscapeGrassWeightExporter::ApplyResults(TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>>& Results)
{
	for (auto&& GrassDataPair : Results)
	{
		ULandscapeComponent* Component = GrassDataPair.Key;
		FLandscapeComponentGrassData* ComponentGrassData = GrassDataPair.Value.Release();
		ALandscapeProxy* Proxy = Component->GetLandscapeProxy();

		UE_LOG(LogGrass, Verbose, TEXT("Populating component %s with grass data, size: %d"), *Component->GetName(), ComponentGrassData->NumElements);

		// Assign the new data (thread-safe)
		Component->GrassData = MakeShareable(ComponentGrassData);

#if WITH_EDITORONLY_DATA
		Component->GrassData->bIsDirty = true;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
		if (Proxy->bBakeMaterialPositionOffsetIntoCollision)
		{
			Component->DestroyCollisionData();
			Component->UpdateCollisionData();
		}
#endif // WITH_EDITOR
	}
}
