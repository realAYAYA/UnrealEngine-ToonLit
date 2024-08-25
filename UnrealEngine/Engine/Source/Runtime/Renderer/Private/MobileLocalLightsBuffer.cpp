// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "BasePassRendering.h"
#include "PixelShaderUtils.h"
#include "MobileBasePassRendering.h"
#include "RendererPrivateUtils.h"
#include "GlobalRenderResources.h"
#include "ScenePrivate.h"
#include "LightRendering.h"
#include "LightFunctionRendering.h"
#include "Materials/MaterialRenderProxy.h"

static bool CompileShaderPermutationsForMobileLocalLightsBuffer(const FStaticShaderPlatform Platform)
{
	return !IsMobileDeferredShadingEnabled(Platform) && 
			IsMobilePlatform(Platform) && 
			MobileLocalLightsBufferEnabled(Platform) &&
			MobileUsesFullDepthPrepass(Platform);
}

const int32 GLocalLightPrepassTileSizeX = 8;
class FLocalLightBufferCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferCS);
public:
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWTileInfo)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
		SHADER_PARAMETER(FIntPoint, GroupSize)
	END_SHADER_PARAMETER_STRUCT()
 
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return CompileShaderPermutationsForMobileLocalLightsBuffer(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GLocalLightPrepassTileSizeX);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 1);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 0);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferCS, "/Engine/Private/MobileLocalLightsBuffer.usf", "MainCS", SF_Compute);

class FLocalLightBufferVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferVS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileInfo)
		SHADER_PARAMETER(int32, LightGridPixelSize)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return CompileShaderPermutationsForMobileLocalLightsBuffer(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferVS, "/Engine/Private/MobileLocalLightsBuffer.usf", "MainVS", SF_Vertex);

class FLocalLightBufferPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLocalLightBufferPS);
	SHADER_USE_PARAMETER_STRUCT(FLocalLightBufferPS, FGlobalShader)

	class FGenerateLightFunctionDepthStencil : SHADER_PERMUTATION_BOOL("GENERATE_LIGHT_FUNCTION_DEPTH_STENCIL");
	using FPermutationDomain = TShaderPermutationDomain<FGenerateLightFunctionDepthStencil>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FForwardLightData, ForwardLightData)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return CompileShaderPermutationsForMobileLocalLightsBuffer(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 0);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatR11G11B10);
		OutEnvironment.SetRenderTargetOutputFormat(1, PF_B8G8R8A8);
	}
};

IMPLEMENT_GLOBAL_SHADER(FLocalLightBufferPS, "/Engine/Private/MobileLocalLightsBuffer.usf", "Main", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalLightBufferPrepassParameters, )
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightBufferVS::FParameters, VS)
SHADER_PARAMETER_STRUCT_INCLUDE(FLocalLightBufferPS::FParameters, PS)
SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

/**
 * A pixel shader for projecting a light function onto the scene and blending with the color from the previously calculated lights in the prepass. 
 */
class FMobileLocalLightFunctionPS : public FMaterialShader
{
	DECLARE_SHADER_TYPE(FMobileLocalLightFunctionPS, Material);
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FMobileLocalLightFunctionPS, FMaterialShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, SvPositionToLight)
		SHADER_PARAMETER(FVector4f, LightFunctionParameters)
		SHADER_PARAMETER(FVector2f, LightFunctionParameters2)
		SHADER_PARAMETER_STRUCT_REF(FDeferredLightUniformStruct, DeferredLightUniforms)
	END_SHADER_PARAMETER_STRUCT()

public:
	/**
	  * Makes sure only shaders for materials that are explicitly flagged
	  * as 'UsedAsLightFunction' in the Material Editor gets compiled into
	  * the shader cache.
	**/  
	static bool ShouldCompilePermutation(const FMaterialShaderPermutationParameters& Parameters)
	{
		return Parameters.MaterialParameters.MaterialDomain == MD_LightFunction && CompileShaderPermutationsForMobileLocalLightsBuffer(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("LIGHT_FUNCTION"), 1);
		OutEnvironment.SetDefine(TEXT("COMPUTE_SHADER"), 0);
		OutEnvironment.SetDefine(TEXT("SUBSTRATE_INLINE_SHADING"), 1);
		OutEnvironment.SetRenderTargetOutputFormat(0, PF_FloatR11G11B10);
		OutEnvironment.SetRenderTargetOutputFormat(1, PF_B8G8R8A8);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FMaterialRenderProxy* MaterialProxy, const FMaterial& Material)
	{
		FMaterialShader::SetViewParameters(BatchedParameters, View, View.ViewUniformBuffer);
		FMaterialShader::SetParameters(BatchedParameters, MaterialProxy, Material, View);
	}

	FParameters GetParameters(const FViewInfo& View, const FLightSceneInfo* LightSceneInfo, const float FadeAlpha)
	{
		FParameters PS;
		
		LightFunctionSvPositionToLightTransform(PS.SvPositionToLight, View, *LightSceneInfo);
		PS.LightFunctionParameters = FLightFunctionSharedParameters::GetLightFunctionSharedParameters(LightSceneInfo, FadeAlpha);
		PS.LightFunctionParameters2 = FVector2f(LightSceneInfo->Proxy->GetLightFunctionFadeDistance(), LightSceneInfo->Proxy->GetLightFunctionDisabledBrightness());
		PS.DeferredLightUniforms = TUniformBufferRef<FDeferredLightUniformStruct>::CreateUniformBufferImmediate(GetDeferredLightParameters(View, *LightSceneInfo), EUniformBufferUsage::UniformBuffer_SingleFrame);

		return PS;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(, FMobileLocalLightFunctionPS, TEXT("/Engine/Private/MobileLocalLightsBuffer.usf"), TEXT("MainLightFunction"), SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FLocalLightFunctionParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, SceneTextures)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static bool TryGetLightFunctionShaders(FMaterialRenderProxy const*& OutMaterialProxy, FMaterial const*& OutMaterial, FMaterialShaders& OutShaders)
{
	while (OutMaterialProxy)
	{
		OutMaterial = OutMaterialProxy->GetMaterialNoFallback(ERHIFeatureLevel::ES3_1);
		if (OutMaterial && OutMaterial->IsLightFunction())
		{
			FMaterialShaderTypes ShaderTypes;
			ShaderTypes.AddShaderType<FMobileLocalLightFunctionPS>();
			if (OutMaterial->TryGetShaders(ShaderTypes, nullptr, OutShaders))
			{
				return true;
			}
		}
		OutMaterialProxy = OutMaterialProxy->GetFallback(ERHIFeatureLevel::ES3_1);
	}
	return false;
}

template <bool DepthWrite>
using LightFunctionMainPassDepthStencilState = TStaticDepthStencilState<
	DepthWrite, CF_Always,
	true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
	true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
	0, STENCIL_MOBILE_LIGHTFUNCTION_MASK>;

template <ECompareFunction CompareFunction>
using LightFunctionMaterialPassDepthStencilState = TStaticDepthStencilState<
	false, CompareFunction,
	true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
	true, CF_Equal, SO_Keep, SO_Keep, SO_Keep,
	STENCIL_MOBILE_LIGHTFUNCTION_MASK, 0>;

void FMobileSceneRenderer::RenderMobileLocalLightsBuffer(FRDGBuilder& GraphBuilder, FSceneTextures& SceneTextures, const FSortedLightSetSceneInfo& SortedLights)
{
	if (!CompileShaderPermutationsForMobileLocalLightsBuffer(ShaderPlatform) ||
		IsMobileDeferredShadingEnabled(ShaderPlatform))
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "RenderMobileLocalLightsBuffer");
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RenderMobileLocalLightsBuffer);

	static const auto LightGridPixelSizeCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Forward.LightGridPixelSize"));
	check(LightGridPixelSizeCVar != nullptr);
	int32 LightGridPixelSize = LightGridPixelSizeCVar->GetInt();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		FViewInfo& View = Views[ViewIndex];
		bool bHasNoLocalLights = (!View.ForwardLightingResources.ForwardLightData) || (View.ForwardLightingResources.ForwardLightData->NumLocalLights == 0);
		if (!View.ShouldRenderView() || bHasNoLocalLights)
		{
			continue;
		}

		const FIntPoint GroupSize(
			FMath::DivideAndRoundUp(View.ViewRect.Size().X, LightGridPixelSize),
			FMath::DivideAndRoundUp(View.ViewRect.Size().Y, LightGridPixelSize));

		FRDGBufferRef TileInfoBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 2 * GroupSize.X * GroupSize.Y), TEXT("TileInfoBuffer"));
		FRDGBufferUAVRef TileInfoBufferUAV = GraphBuilder.CreateUAV(TileInfoBuffer, PF_R32_UINT);
		FRDGBufferSRVRef TileInfoBufferSRV = GraphBuilder.CreateSRV(TileInfoBuffer, PF_R32_UINT);

		{
			auto* PassParameters = GraphBuilder.AllocParameters<FLocalLightBufferCS::FParameters>();
			PassParameters->RWTileInfo = TileInfoBufferUAV;
			PassParameters->ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->GroupSize = GroupSize;
			auto ComputeShader = View.ShaderMap->GetShader<FLocalLightBufferCS>();

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("RenderMobileLocalLights_TiledInfoCS"),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				FIntVector(FMath::DivideAndRoundUp<uint32>(GroupSize.Y * GroupSize.X, GLocalLightPrepassTileSizeX), 1, 1));
		}

		bool bRenderLightFunctions = false;
		{
			// Check if there are any light functions
			for (const FSortedLightSceneInfo& SortedLightSceneInfo : SortedLights.SortedLights)
			{
				// Directional lights are currently not supported
				if (SortedLightSceneInfo.SortKey.Fields.LightType == LightType_Directional)
				{
					continue;
				}

				if (!SortedLightSceneInfo.SortKey.Fields.bLightFunction)
				{
					continue;
				}

				bRenderLightFunctions = true;
				break;
			}
		}

		{
			FLocalLightBufferPrepassParameters* PassParameters = GraphBuilder.AllocParameters<FLocalLightBufferPrepassParameters>();
			PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureA, ERenderTargetLoadAction::EClear);
			PassParameters->RenderTargets[1] = FRenderTargetBinding(SceneTextures.MobileLocalLightTextureB, ERenderTargetLoadAction::EClear);


			bool bRestoreDepthBuffer = false;
			if (bRenderLightFunctions)
			{
				if (MobileRequiresSceneDepthAux(ShaderPlatform))
				{
					// In this case the depth buffer is typically memoryless and not preserved, so we restore it from SceneDepthAux
					PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Resolve, ERenderTargetLoadAction::EClear, ERenderTargetLoadAction::EClear, FExclusiveDepthStencil::DepthWrite_StencilWrite);
					bRestoreDepthBuffer = true;
				}
				else
				{
					// If the main depth buffer is not memoryless we rely on STENCIL_MOBILE_LIGHTFUNCTION_MASK being cleared to 0 already
					const ERenderTargetLoadAction LoadAction = EnumHasAnyFlags(SceneTextures.Depth.Resolve->Desc.Flags, TexCreate_Memoryless) ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad;
					PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneTextures.Depth.Resolve, LoadAction, LoadAction, FExclusiveDepthStencil::DepthRead_StencilWrite);
				}
			}
		
			PassParameters->SceneTextures = SceneTextures.GetSceneTextureShaderParameters(View.FeatureLevel);

			PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);
			PassParameters->VS.TileInfo = TileInfoBufferSRV;
			PassParameters->VS.LightGridPixelSize = LightGridPixelSize;
			PassParameters->PS.ForwardLightData = View.ForwardLightingResources.ForwardLightUniformBuffer;
			PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);

			auto VertexShader = View.ShaderMap->GetShader<FLocalLightBufferVS>();

			FLocalLightBufferPS::FPermutationDomain PermutationVectorPS;
			PermutationVectorPS.Set<FLocalLightBufferPS::FGenerateLightFunctionDepthStencil>(bRenderLightFunctions);
			auto PixelShader = View.ShaderMap->GetShader<FLocalLightBufferPS>(PermutationVectorPS);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("RenderMobileLocalLightsBuffer Prepass"),
				PassParameters,
				ERDGPassFlags::Raster,
				[PassParameters, VertexShader, PixelShader, &View, GroupSize, &SortedLights, bRenderLightFunctions, bRestoreDepthBuffer](FRHICommandList& RHICmdList)
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0.0f, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1.0f);
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
					GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
					
					// The main non-lightfunction pass creates a stencil mask of the lit area
					// Later lightfunction passes use it for stencil test to avoid redundant PS execution
					uint32 StencilRef = 0;
					if (bRenderLightFunctions)
					{
						StencilRef = STENCIL_MOBILE_LIGHTFUNCTION_MASK;
						if (bRestoreDepthBuffer)
						{
							GraphicsPSOInit.DepthStencilState = LightFunctionMainPassDepthStencilState<true>::GetRHI();
						}
						else
						{
							GraphicsPSOInit.DepthStencilState = LightFunctionMainPassDepthStencilState<false>::GetRHI();
						}
					}
					else
					{
						GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					}

					GraphicsPSOInit.PrimitiveType = PT_TriangleList;
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTileVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

					SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
					SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

					RHICmdList.SetStreamSource(0, GetOneTileQuadVertexBuffer(), 0);
					RHICmdList.DrawIndexedPrimitive(GetOneTileQuadIndexBuffer(),
						0,
						0,
						4,
						0,
						2,
						GroupSize.X * GroupSize.Y);

					if (!bRenderLightFunctions)
					{
						return;
					}

					// Draws a pass for each visible light with a light function and blends on top of the color texture 
					// generated by last pass
					for (const FSortedLightSceneInfo& SortedLightSceneInfo : SortedLights.SortedLights)
					{
						 // Directional lights are currently not supported
						if (SortedLightSceneInfo.SortKey.Fields.LightType == LightType_Directional)
						{
							continue;
						}

						if (!SortedLightSceneInfo.SortKey.Fields.bLightFunction)
						{
							continue;
						}

						const FLightSceneInfo* const LightSceneInfo = SortedLightSceneInfo.LightSceneInfo;
						const FSphere LightBounds = LightSceneInfo->Proxy->GetBoundingSphere();

						const float FadeAlpha = GetLightFunctionFadeFraction(View, LightBounds);
						// Don't draw the light function if it has completely faded out
						if (FadeAlpha < 1.0f / 256.0f)
						{
							continue;
						}

						FMaterialShaders MaterialShaders;
						const FMaterialRenderProxy* MaterialProxyForRendering = LightSceneInfo->Proxy->GetLightFunctionMaterial();
						const FMaterial* MaterialForRendering = nullptr;
						if (!TryGetLightFunctionShaders(MaterialProxyForRendering, MaterialForRendering, MaterialShaders))
						{
							UE_LOG(LogTemp, Error, TEXT("Light function shader for light %d not found."), LightSceneInfo->Id);
							continue;
						}

						FDeferredLightVS::FPermutationDomain PermutationVectorVS;
						PermutationVectorVS.Set<FDeferredLightVS::FRadialLight>(true);
						TShaderMapRef<FDeferredLightVS> LightFunctionVertexShader(View.ShaderMap, PermutationVectorVS);
						FDeferredLightVS::FParameters ParametersVS = FDeferredLightVS::GetParameters(View, LightSceneInfo, false);
						ParametersVS.View = TUniformBufferBinding(View.ViewUniformBuffer, EUniformBufferBindingFlags::Shader);
						TShaderRef<FMobileLocalLightFunctionPS> LightFunctionPixelShader;
						MaterialShaders.TryGetPixelShader(LightFunctionPixelShader);
						FMobileLocalLightFunctionPS::FParameters ParametersPS = LightFunctionPixelShader->GetParameters(View, LightSceneInfo, FadeAlpha);

						// Directional RT was already generated in main pass
						GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_One, BF_Zero, CW_NONE>::GetRHI(); 

						GraphicsPSOInit.PrimitiveType = PT_TriangleList;
						GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = LightFunctionVertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = LightFunctionPixelShader.GetPixelShader();

						if (((FVector)View.ViewMatrices.GetViewOrigin() - LightBounds.Center).SizeSquared() < FMath::Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f))
						{
							// Render backfaces with depth tests disabled since the camera is inside (or close to inside) the light function geometry
							GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
							GraphicsPSOInit.DepthStencilState = LightFunctionMaterialPassDepthStencilState<CF_Always>::GetRHI();
						}
						else
						{
							// Render frontfaces with depth test so that less pixel are shaded
							GraphicsPSOInit.RasterizerState = View.bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
							GraphicsPSOInit.DepthStencilState = LightFunctionMaterialPassDepthStencilState<CF_DepthNearOrEqual>::GetRHI();
						}

						// Set the light's scissor rectangle.
						LightSceneInfo->Proxy->SetScissorRect(RHICmdList, View, View.ViewRect);

						SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);

						SetShaderParameters(RHICmdList, LightFunctionVertexShader, LightFunctionVertexShader.GetVertexShader(), ParametersVS);
						SetShaderParametersMixedPS(RHICmdList, LightFunctionPixelShader, ParametersPS, View, MaterialProxyForRendering, *MaterialForRendering);

						// Project the light function using a sphere around the light
						if (SortedLightSceneInfo.SortKey.Fields.LightType == LightType_Spot)
						{
							StencilingGeometry::DrawCone(RHICmdList);
						}
						else
						{
							StencilingGeometry::DrawSphere(RHICmdList);
						}
					}
				});
		}
	}
}
