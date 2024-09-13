#include "ToonOutlineRendering.h"

#include "SceneRendering.h"

#include "ScenePrivate.h"
#include "MeshPassProcessor.inl"
#include "SimpleMeshDrawCommandPass.h"

//IMPLEMENT_SHADERPIPELINE_TYPE_VSPS(BackfaceOutlinePipeline, FToonOutlineVS, FToonOutlinePS, true);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlineVS, TEXT("/Engine/Private/ToonLit/ToonLitOutLine.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FToonOutlinePS, TEXT("/Engine/Private/ToonLit/ToonLitOutLine.usf"), TEXT("MainPS"), SF_Pixel);

/**
 * Mesh Pass Processor
 * 
 */
FToonOutlineMeshPassProcessor::FToonOutlineMeshPassProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	const FMeshPassProcessorRenderState& InPassDrawRenderState,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(Scene, Scene->GetFeatureLevel(), InViewIfDynamicMeshCommand, InDrawListContext),
	  PassDrawRenderState(InPassDrawRenderState)
{
	//PassDrawRenderState.SetViewUniformBuffer(Scene->UniformBuffers.ViewUniformBuffer);
	if (PassDrawRenderState.GetDepthStencilState() == nullptr)
		PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_NotEqual>().GetRHI());
	if (PassDrawRenderState.GetBlendState() == nullptr)
		PassDrawRenderState.SetBlendState(TStaticBlendState<>().GetRHI());
}

void FToonOutlineMeshPassProcessor::AddMeshBatch(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	const FMaterialRenderProxy* FallBackMaterialRenderProxyPtr = nullptr;
	const FMaterial& Material = MaterialRenderProxy->GetMaterialWithFallback(Scene->GetFeatureLevel(), FallBackMaterialRenderProxyPtr);

	const UMaterialInterface* MaterialInterface = Material.GetMaterialInterface()->GetOutlineMaterial();
	if (!MaterialInterface)
		return;
	
	const FMaterial* OutlineMaterial = MaterialInterface->GetMaterialResource(Scene->GetFeatureLevel());
	const FMaterialRenderProxy* OutlineMaterialRenderProxy = MaterialInterface->GetRenderProxy();
	if (!OutlineMaterial || !OutlineMaterialRenderProxy)
		return;

	if (OutlineMaterial->GetRenderingThreadShaderMap())
	{
		// Determine the mesh's material and blend mode.
		if (Material.GetBlendMode() == BLEND_Opaque || Material.GetBlendMode() == BLEND_Masked)
		{
			Process<false, false>(
				MeshBatch,
				BatchElementMask,
				StaticMeshId,
				PrimitiveSceneProxy,
				*OutlineMaterialRenderProxy,
				*OutlineMaterial,
				FM_Solid,
				CM_CCW);
		}
	}
}

template <bool bPositionOnly, bool bUsesMobileColorValue>
bool FToonOutlineMeshPassProcessor::Process(
	const FMeshBatch& MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	TMeshProcessorShaders<
		FToonOutlineVS,
		FToonOutlinePS> ToonOutlineShaders;

	// Try Get Shader.
	{
		FMaterialShaderTypes ShaderTypes;
		ShaderTypes.AddShaderType<FToonOutlineVS>();
		ShaderTypes.AddShaderType<FToonOutlinePS>();

		const FVertexFactoryType* VertexFactoryType = VertexFactory->GetType();

		FMaterialShaders Shaders;
		if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactoryType, Shaders))
		{
			//UE_LOG(LogShaders, Warning, TEXT("**********************!Shader Not Found!*************************"));
			return false;
		}

		Shaders.TryGetVertexShader(ToonOutlineShaders.VertexShader);
		Shaders.TryGetPixelShader(ToonOutlineShaders.PixelShader);
	}

	FToonOutlinePassShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId,
	                                             false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(ToonOutlineShaders.VertexShader, ToonOutlineShaders.PixelShader);

	// !
	PassDrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
			true, CF_GreaterEqual, // Enable DepthTest, It reverse about OpenGL(which is less)
			false, CF_Never, SO_Keep, SO_Keep, SO_Keep,
			false, CF_Never, SO_Keep, SO_Keep, SO_Keep, // enable stencil test when cull back
			0x00, // disable stencil read
			0x00> // disable stencil write
		::GetRHI());
	PassDrawRenderState.SetStencilRef(0);

	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		ToonOutlineShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData
	);

	return true;
}

FInt32Range GetDynamicMeshElementRange(const FViewInfo& View, uint32 PrimitiveIndex)
{
	// DynamicMeshEndIndices contains valid values only for visible primitives with bDynamicRelevance.
	if (View.PrimitiveVisibilityMap[PrimitiveIndex])
	{
		const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveIndex];
		if (ViewRelevance.bDynamicRelevance)
		{

			return FInt32Range(View.DynamicMeshElementRanges[PrimitiveIndex].X, View.DynamicMeshElementRanges[PrimitiveIndex].Y);
		}
	}

	return FInt32Range::Empty();
}

BEGIN_SHADER_PARAMETER_STRUCT(FToonOutlineMeshPassParameters,)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * Render()
 */
void FSceneRenderer::RenderToonOutlinePass(FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture)
{
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		FViewInfo& View = Views[ViewIndex];

		if (View.Family->Scene == nullptr)
		{
			UE_LOG(LogShaders, Log, TEXT("%s - View.Family->Scene is NULL!"), *FString(__FUNCTION__));
			continue;
		}

		FSimpleMeshDrawCommandPass* SimpleMeshPass = GraphBuilder.AllocObject<FSimpleMeshDrawCommandPass>(View, nullptr);

		FMeshPassProcessorRenderState DrawRenderState;
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>().GetRHI());

		FToonOutlineMeshPassProcessor MeshProcessor(
			Scene,
			&View,
			DrawRenderState,
			SimpleMeshPass->GetDynamicPassMeshDrawListContext());

		// Gather & Flitter MeshBatch from Scene->Primitives.
		for (int32 PrimitiveIndex = 0; PrimitiveIndex < Scene->Primitives.Num(); PrimitiveIndex++)
		{
			const FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->Primitives[PrimitiveIndex];

			if (!View.PrimitiveVisibilityMap[PrimitiveSceneInfo->GetIndex()])
				continue;

			const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveSceneInfo->GetIndex()];

			if (ViewRelevance.bRenderInMainPass && ViewRelevance.bStaticRelevance)
			{
				for (int32 StaticMeshIdx = 0; StaticMeshIdx < PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx++)
				{
					const FStaticMeshBatch& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];
					if (View.StaticMeshVisibilityMap[StaticMesh.Id])
					{
						constexpr uint64 DefaultBatchElementMask = ~0ul;
						MeshProcessor.AddMeshBatch(StaticMesh, DefaultBatchElementMask, StaticMesh.PrimitiveSceneInfo->Proxy);
					}
				}
			}

			if (ViewRelevance.bRenderInMainPass && ViewRelevance.bDynamicRelevance)
			{
				const FInt32Range MeshBatchRange = GetDynamicMeshElementRange(View, PrimitiveSceneInfo->GetIndex());
				for (int32 MeshBatchIndex = MeshBatchRange.GetLowerBoundValue(); MeshBatchIndex < MeshBatchRange.GetUpperBoundValue(); ++MeshBatchIndex)
				{
					const FMeshBatchAndRelevance& MeshAndRelevance = View.DynamicMeshElements[MeshBatchIndex];
					constexpr uint64 BatchElementMask = ~0ull;
					MeshProcessor.AddMeshBatch(*MeshAndRelevance.Mesh, BatchElementMask, MeshAndRelevance.PrimitiveSceneProxy);
				}
			}
		}// for PrimitiveIndex

		const FSceneTextures& SceneTextures = GetActiveSceneTextures();
		
		// Render targets bindings should remain constant at this point.
		FRenderTargetBindingSlots BindingSlots;
		BindingSlots[0] = FRenderTargetBinding(SceneTextures.Color.Target, ERenderTargetLoadAction::ELoad);
		BindingSlots.DepthStencil = FDepthStencilBinding(
			SceneTextures.Depth.Target,
			ERenderTargetLoadAction::ENoAction,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthWrite_StencilNop);

		FToonOutlineMeshPassParameters* PassParameters = GraphBuilder.AllocParameters<FToonOutlineMeshPassParameters>();
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->RenderTargets = BindingSlots;
		PassParameters->Scene = GetSceneUniforms().GetBuffer(GraphBuilder);

		SimpleMeshPass->BuildRenderingCommands(GraphBuilder, View, Scene->GPUScene, PassParameters->InstanceCullingDrawParams);

		FIntRect ViewportRect = View.ViewRect;
		FIntRect ScissorRect = FIntRect(FIntPoint(EForceInit::ForceInitToZero), SceneColorTexture->Desc.Extent);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("ToonOutlinePass"),
			PassParameters,
			ERDGPassFlags::Raster,
			[this, ViewportRect, ScissorRect, SimpleMeshPass, PassParameters](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(
					ViewportRect.Min.X, ViewportRect.Min.Y, 0.0f,
					ViewportRect.Max.X, ViewportRect.Max.Y, 1.0f);

				RHICmdList.SetScissorRect(
					true,
					ScissorRect.Min.X >= ViewportRect.Min.X ? ScissorRect.Min.X : ViewportRect.Min.X,
					ScissorRect.Min.Y >= ViewportRect.Min.Y ? ScissorRect.Min.Y : ViewportRect.Min.Y,
					ScissorRect.Max.X <= ViewportRect.Max.X ? ScissorRect.Max.X : ViewportRect.Max.X,
					ScissorRect.Max.Y <= ViewportRect.Max.Y ? ScissorRect.Max.Y : ViewportRect.Max.Y);

				SimpleMeshPass->SubmitDraw(RHICmdList, PassParameters->InstanceCullingDrawParams);
			});
	} // for View
}

/*// Register Pass to Global Manager
void SetupToonOutLinePassState(FMeshPassProcessorRenderState& DrawRenderState)
{
	DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());

	// !
	PassDrawRenderState.SetDepthStencilState(
		TStaticDepthStencilState<
		true, CF_GreaterEqual,// Enable DepthTest, It reverse about OpenGL(which is less)
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,
		false, CF_Never, SO_Keep, SO_Keep, SO_Keep,// enable stencil test when cull back
		0x00,// disable stencil read
		0x00>// disable stencil write
		::GetRHI());
	PassDrawRenderState.SetStencilRef(0);
}

FMeshPassProcessor* CreateToonOutLinePassProcessor(
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext
)
{
	FMeshPassProcessorRenderState ToonOutLinePassState;
	SetupToonOutLinePassState(ToonOutLinePassState);

	return new(FMemStack::Get()) FToonOutlineMeshPassProcessor(
		Scene,
		InViewIfDynamicMeshCommand,
		ToonOutLinePassState,
		InDrawListContext
	);
}

FRegisterPassProcessorCreateFunction RegisteToonOutLineMeshPass(
	&CreateToonOutLinePassProcessor,
	EShadingPath::Deferred,
	EMeshPass::BackfaceOutLinePass,
	EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView
);*/
