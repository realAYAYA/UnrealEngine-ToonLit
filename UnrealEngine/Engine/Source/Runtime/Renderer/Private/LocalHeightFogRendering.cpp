// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalHeightFogRendering.h"
#include "ScenePrivate.h"
#include "RendererUtils.h"
#include "ScreenPass.h"
#include "LocalHeightFogSceneProxy.h"
#include "MobileBasePassRendering.h"


// The runtime ON/OFF toggle
static TAutoConsoleVariable<int32> CVarLocalHeightFog(
	TEXT("r.LocalHeightFog"), 1,
	TEXT("LocalHeightFog components are rendered when this is not 0, otherwise ignored.\n"),
	ECVF_RenderThreadSafe);

bool ShouldRenderLocalHeightFog(const FScene* Scene, const FSceneViewFamily& Family)
{
	const FEngineShowFlags EngineShowFlags = Family.EngineShowFlags;
	if (Scene && Scene->HasAnyLocalHeightFog() && EngineShowFlags.Fog && !Family.UseDebugViewPS())
	{
		return CVarLocalHeightFog.GetValueOnRenderThread() > 0;
	}
	return false;
}

DECLARE_GPU_STAT(LocalHeightFogVolumes);

/*=============================================================================
	FScene functions
=============================================================================*/

void FScene::AddLocalHeightFog(class FLocalHeightFogSceneProxy* FogProxy)
{
	check(FogProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FAddLocalHeightFogCommand)(
		[Scene, FogProxy](FRHICommandListImmediate& RHICmdList)
		{
			check(!Scene->LocalHeightFogs.Contains(FogProxy));
			Scene->LocalHeightFogs.Push(FogProxy);
		} );
}

void FScene::RemoveLocalHeightFog(class FLocalHeightFogSceneProxy* FogProxy)
{
	check(FogProxy);
	FScene* Scene = this;

	ENQUEUE_RENDER_COMMAND(FRemoveLocalHeightFogCommand)(
		[Scene, FogProxy](FRHICommandListImmediate& RHICmdList)
		{
			Scene->LocalHeightFogs.RemoveSingle(FogProxy);
		} );
}

bool FScene::HasAnyLocalHeightFog() const
{ 
	return LocalHeightFogs.Num() > 0;
}

/*=============================================================================
	Local height fog rendering common function
=============================================================================*/

void GetLocalFogVolumeSortingData(const FScene* Scene, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& Out)
{
	// No culling as of today
	Out.LocalHeightFogInstanceCount = Scene->LocalHeightFogs.Num();
	Out.LocalHeightFogInstanceCountFinal = 0;
	Out.LocalHeightFogGPUInstanceData = (FLocalHeightFogGPUInstanceData*)GraphBuilder.Alloc(sizeof(FLocalHeightFogGPUInstanceData) * Out.LocalHeightFogInstanceCount, 16);
	Out.LocalHeightFogCenterPos = (FVector*)GraphBuilder.Alloc(sizeof(FVector3f) * Out.LocalHeightFogInstanceCount, 16);
	Out.LocalHeightFogSortKeys.SetNumUninitialized(Out.LocalHeightFogInstanceCount);
	for (FLocalHeightFogSceneProxy* LHF : Scene->LocalHeightFogs)
	{
		if (LHF->FogDensity <= 0.0f)
		{
			continue; // this volume will never be visible
		}

		FTransform TransformScaleOnly;
		TransformScaleOnly.SetScale3D(LHF->FogTransform.GetScale3D());

		FLocalHeightFogGPUInstanceData* LocalHeightFogGPUInstanceDataIt = &Out.LocalHeightFogGPUInstanceData[Out.LocalHeightFogInstanceCountFinal];
		LocalHeightFogGPUInstanceDataIt->Transform = FMatrix44f(LHF->FogTransform.ToMatrixWithScale());
		LocalHeightFogGPUInstanceDataIt->InvTransform = LocalHeightFogGPUInstanceDataIt->Transform.Inverse();
		LocalHeightFogGPUInstanceDataIt->InvTranformNoScale = FMatrix44f(LHF->FogTransform.ToMatrixNoScale()).Inverse();
		LocalHeightFogGPUInstanceDataIt->TransformScaleOnly = FMatrix44f(TransformScaleOnly.ToMatrixWithScale());

		LocalHeightFogGPUInstanceDataIt->Density = LHF->FogDensity;
		LocalHeightFogGPUInstanceDataIt->HeightFalloff = LHF->FogHeightFalloff * 0.01f;	// This scale is used to have artist author reasonable range.
		LocalHeightFogGPUInstanceDataIt->HeightOffset = LHF->FogHeightOffset;
		LocalHeightFogGPUInstanceDataIt->RadialAttenuation = LHF->FogRadialAttenuation;

		LocalHeightFogGPUInstanceDataIt->FogMode = float(LHF->FogMode);

		LocalHeightFogGPUInstanceDataIt->Albedo = FVector3f(LHF->FogAlbedo);
		LocalHeightFogGPUInstanceDataIt->PhaseG = LHF->FogPhaseG;
		LocalHeightFogGPUInstanceDataIt->Emissive = FVector3f(LHF->FogEmissive);

		Out.LocalHeightFogCenterPos[Out.LocalHeightFogInstanceCountFinal] = LHF->FogTransform.GetTranslation();

		FLocalFogVolumeSortKey* LocalHeightFogSortKeysIt = &Out.LocalHeightFogSortKeys[Out.LocalHeightFogInstanceCountFinal];
		LocalHeightFogSortKeysIt->FogVolume.Index = Out.LocalHeightFogInstanceCountFinal;
		LocalHeightFogSortKeysIt->FogVolume.Distance = 0;	// Filled up right before sorting according to a view
		LocalHeightFogSortKeysIt->FogVolume.Priority = LHF->FogSortPriority;

		Out.LocalHeightFogInstanceCountFinal++;
	}
	// Shrink the array to only what is needed in order for the sort to correctly work on only what is needed.
	Out.LocalHeightFogSortKeys.SetNum(Out.LocalHeightFogInstanceCountFinal, false/*bAllowShrinking*/);
}

void CreateViewLocalFogVolumeBufferSRV(FViewInfo& View, FRDGBuilder& GraphBuilder, FLocalFogVolumeSortingData& SortingData)
{
	static const uint32 SizeOfFloat4 = sizeof(float) * 4;
	static const uint32 Float4CountInLocalHeightFogGPUInstanceData = sizeof(FLocalHeightFogGPUInstanceData) / SizeOfFloat4;
	static_assert(sizeof(FLocalHeightFogGPUInstanceData) == Float4CountInLocalHeightFogGPUInstanceData * SizeOfFloat4); // The size of the structure must be a multiple of FVector4.

	if (SortingData.LocalHeightFogInstanceCountFinal == 0)
	{
		View.LocalHeightFogGPUInstanceCount = 0;

		static FLocalHeightFogGPUInstanceData DummyData;
		View.LocalHeightFogGPUInstanceDataBufferSRV = GraphBuilder.CreateSRV(
			CreateVertexBuffer(GraphBuilder, TEXT("LocalHeightFogGPUInstanceDataBuffer"), 
			FRDGBufferDesc::CreateBufferDesc(SizeOfFloat4, Float4CountInLocalHeightFogGPUInstanceData), &DummyData, sizeof(FLocalHeightFogGPUInstanceData) * 1, ERDGInitialDataFlags::NoCopy),
			PF_A32B32G32R32F);
		return;
	}

	// 1. Sort all the volumes
	const FVector ViewOrigin = View.ViewMatrices.GetViewOrigin();
	for (uint32 i = 0; i < SortingData.LocalHeightFogInstanceCountFinal; ++i)
	{
		FVector FogCenterPos = SortingData.LocalHeightFogCenterPos[SortingData.LocalHeightFogSortKeys[i].FogVolume.Index];	// Recovered form the original array via index because the sorting of the previous view might have changed the order.
		float DistancetoView = float((FogCenterPos - ViewOrigin).Size());
		SortingData.LocalHeightFogSortKeys[i].FogVolume.Distance = *reinterpret_cast<uint32*>(&DistancetoView);
	}
	SortingData.LocalHeightFogSortKeys.Sort();

	// 2. Create the buffer containing all the fog volume data instance sorted according to their key for the current view.
	FLocalHeightFogGPUInstanceData* LocalHeightFogGPUSortedInstanceData = (FLocalHeightFogGPUInstanceData*)GraphBuilder.Alloc(sizeof(FLocalHeightFogGPUInstanceData) * SortingData.LocalHeightFogInstanceCountFinal, 16);
	for (uint32 i = 0; i < SortingData.LocalHeightFogInstanceCountFinal; ++i)
	{
		// We could also have an indirection buffer on GPU but choosing to go with the sorting + copy on CPU since it is expected to not have many local height fog volumes.
		LocalHeightFogGPUSortedInstanceData[i] = SortingData.LocalHeightFogGPUInstanceData[SortingData.LocalHeightFogSortKeys[i].FogVolume.Index];
	}

	// 3. Allocate buffer and initialize with sorted data to upload to GPU
	const uint32 AllLocalHeightFogInstanceBytesFinal = sizeof(FLocalHeightFogGPUInstanceData) * SortingData.LocalHeightFogInstanceCountFinal;
	FRDGBufferRef LocalHeightFogGPUInstanceDataBuffer = CreateVertexBuffer(
		GraphBuilder, TEXT("LocalHeightFogGPUInstanceDataBuffer"),
		FRDGBufferDesc::CreateBufferDesc(SizeOfFloat4, SortingData.LocalHeightFogInstanceCountFinal * Float4CountInLocalHeightFogGPUInstanceData), 
		LocalHeightFogGPUSortedInstanceData, AllLocalHeightFogInstanceBytesFinal, ERDGInitialDataFlags::NoCopy);

	View.LocalHeightFogGPUInstanceCount = SortingData.LocalHeightFogInstanceCountFinal;
	View.LocalHeightFogGPUInstanceDataBufferSRV = GraphBuilder.CreateSRV(LocalHeightFogGPUInstanceDataBuffer, PF_A32B32G32R32F);
}

/*=============================================================================
	Local height fog rendering - non mobile
=============================================================================*/

class FLocalHeightFogVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalHeightFogVS);
	SHADER_USE_PARAMETER_STRUCT(FLocalHeightFogVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalHeightFogInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalHeightFogVS, "/Engine/Private/LocalHeightFog.usf", "LocalHeightFogSplatVS", SF_Vertex);

class FLocalHeightFogPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalHeightFogPS);
	SHADER_USE_PARAMETER_STRUCT(FLocalHeightFogPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalHeightFogInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalHeightFogPS, "/Engine/Private/LocalHeightFog.usf", "LocalHeightFogSplatPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalHeightFogPassParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FLocalHeightFogVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLocalHeightFogPS::FParameters, PS)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()
 
void RenderLocalHeightFog(
	const FScene* Scene,
	TArray<FViewInfo>& Views,
	FRDGBuilder& GraphBuilder,
	const FMinimalSceneTextures& SceneTextures,
	FRDGTextureRef LightShaftOcclusionTexture)
{
	uint32 LocalHeightFogInstanceCount = Scene->LocalHeightFogs.Num();
	if (LocalHeightFogInstanceCount > 0)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, LocalHeightFogVolumes);

		FLocalFogVolumeSortingData SortingData;
		GetLocalFogVolumeSortingData(Scene, GraphBuilder, SortingData);

		if (SortingData.LocalHeightFogInstanceCountFinal > 0)
		{
			FRDGTextureRef SceneColorTexture = SceneTextures.Color.Resolve;

			for (FViewInfo& View : Views)
			{
				CreateViewLocalFogVolumeBufferSRV(View, GraphBuilder, SortingData);
				if (View.LocalHeightFogGPUInstanceCount == 0)
				{
					continue;
				}

				FLocalHeightFogPassParameters* PassParameters = GraphBuilder.AllocParameters<FLocalHeightFogPassParameters>();

				PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
				PassParameters->VS.LocalHeightFogInstances = View.LocalHeightFogGPUInstanceDataBufferSRV;

				PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);
				PassParameters->PS.LocalHeightFogInstances = View.LocalHeightFogGPUInstanceDataBufferSRV;

				PassParameters->SceneTextures = SceneTextures.UniformBuffer;
				PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ENoAction);

				FLocalHeightFogVS::FPermutationDomain VSPermutationVector;
				auto VertexShader = View.ShaderMap->GetShader< FLocalHeightFogVS >(VSPermutationVector);

				FLocalHeightFogPS::FPermutationDomain PsPermutationVector;
				auto PixelShader = View.ShaderMap->GetShader< FLocalHeightFogPS >(PsPermutationVector);

				const FIntRect ViewRect = View.ViewRect;

				ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
				ClearUnusedGraphResources(PixelShader, &PassParameters->PS);

				uint32 LocalHeightFogGPUInstanceCount = View.LocalHeightFogGPUInstanceCount;
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("RenderLocalHeightFog %u inst.", LocalHeightFogGPUInstanceCount),
					PassParameters,
					ERDGPassFlags::Raster,
					[VertexShader, PixelShader, PassParameters, LocalHeightFogGPUInstanceCount, ViewRect](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

					// Render back faces only since camera may intersect
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

					RHICmdList.DrawIndexedPrimitive(GetUnitCubeIndexBuffer()
						, 0									//BaseVertexIndex
						, 0									//FirstInstance
						, 8									//uint32 NumVertices
						, 0									//uint32 StartIndex
						, UE_ARRAY_COUNT(GCubeIndices) / 3	//uint32 NumPrimitives
						, LocalHeightFogGPUInstanceCount	//uint32 NumInstances
					);
				});
			}
		}
	}
}

/*=============================================================================
	Local height fog rendering - mobile
=============================================================================*/
	

class FMobileLocalHeightFogVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileLocalHeightFogVS);
	SHADER_USE_PARAMETER_STRUCT(FMobileLocalHeightFogVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalHeightFogInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEPTHREAD_SUBPASS"), bMobileForceDepthRead ? 0u : 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileLocalHeightFogVS, "/Engine/Private/LocalHeightFog.usf", "LocalHeightFogSplatVS", SF_Vertex);

class FMobileLocalHeightFogPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMobileLocalHeightFogPS);
	SHADER_USE_PARAMETER_STRUCT(FMobileLocalHeightFogPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FViewShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FMobileBasePassUniformParameters, MobileBasePass)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, LocalHeightFogInstances)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!IsMobilePlatform(Parameters.Platform))
		{
			return false;
		}
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		const bool bMobileForceDepthRead = MobileUsesFullDepthPrepass(Parameters.Platform);
		OutEnvironment.SetDefine(TEXT("IS_MOBILE_DEPTHREAD_SUBPASS"), bMobileForceDepthRead ? 0u : 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMobileLocalHeightFogPS, "/Engine/Private/LocalHeightFog.usf", "LocalHeightFogSplatPS", SF_Pixel);

void RenderLocalHeightFogMobile(
	FRHICommandList& RHICmdList,
	const FViewInfo& View)
{
	if (View.LocalHeightFogGPUInstanceCount == 0)
	{
		return;
	}

	SCOPED_DRAW_EVENT(RHICmdList, LocalHeightFogVolumes);

	FMobileLocalHeightFogVS::FPermutationDomain VSPermutationVector;
	auto VertexShader = View.ShaderMap->GetShader< FMobileLocalHeightFogVS >(VSPermutationVector);

	FMobileLocalHeightFogPS::FPermutationDomain PsPermutationVector;
	auto PixelShader = View.ShaderMap->GetShader< FMobileLocalHeightFogPS >(PsPermutationVector);

	const FIntRect ViewRect = View.ViewRect;

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

	// Render back faces only since camera may intersect
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_SourceAlpha, BO_Add, BF_Zero, BF_SourceAlpha>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	FMobileLocalHeightFogVS::FParameters VSParameters;
	VSParameters.View = View.GetShaderParameters();
	VSParameters.LocalHeightFogInstances = View.LocalHeightFogGPUInstanceDataBufferSRV;
	SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), VSParameters);

	FMobileLocalHeightFogPS::FParameters PSParameters;
	PSParameters.View = View.GetShaderParameters();
	PSParameters.LocalHeightFogInstances = View.LocalHeightFogGPUInstanceDataBufferSRV;
	// PSParameters.MobileBasePass filled up by the RDG pass parameters.
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParameters);

	RHICmdList.SetStreamSource(0, GetUnitCubeVertexBuffer(), 0);

	RHICmdList.DrawIndexedPrimitive(
		GetUnitCubeIndexBuffer()
		, 0										//BaseVertexIndex
		, 0										//FirstInstance
		, 8										//uint32 NumVertices
		, 0										//uint32 StartIndex
		, UE_ARRAY_COUNT(GCubeIndices) / 3		//uint32 NumPrimitives
		, View.LocalHeightFogGPUInstanceCount	//uint32 NumInstances
	);
}
