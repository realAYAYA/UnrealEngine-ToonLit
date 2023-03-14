// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShadowRendering.h"
#include "Engine/Engine.h"

class FProjectedShadowInfo;
class FPlanarReflectionSceneProxy;
class FRHIRenderQuery;

DECLARE_GPU_STAT_NAMED_EXTERN(HZB, TEXT("HZB"));

/*=============================================================================
	SceneOcclusion.h
=============================================================================*/

struct FViewOcclusionQueries
{
	using FProjectedShadowArray = TArray<FProjectedShadowInfo const*, SceneRenderingAllocator>;
	using FPlanarReflectionArray = TArray<FPlanarReflectionSceneProxy const*, SceneRenderingAllocator>;
	using FRenderQueryArray = TArray<FRHIRenderQuery*, SceneRenderingAllocator>;

	FProjectedShadowArray LocalLightQueryInfos;
	FProjectedShadowArray CSMQueryInfos;
	FProjectedShadowArray ShadowQueryInfos;
	FPlanarReflectionArray ReflectionQueryInfos;

	FRenderQueryArray LocalLightQueries;
	FRenderQueryArray CSMQueries;
	FRenderQueryArray ShadowQueries;
	FRenderQueryArray ReflectionQueries;

	bool bFlushQueries = true;
};

using FViewOcclusionQueriesPerView = TArray<FViewOcclusionQueries, TInlineAllocator<1, SceneRenderingAllocator>>;

/**
* A vertex shader for rendering a texture on a simple element.
*/
class FOcclusionQueryVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOcclusionQueryVS,Global);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1); }

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		static auto* MobileUseHWsRGBEncodingCVAR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Mobile.UseHWsRGBEncoding"));
		const bool bMobileUseHWsRGBEncoding = (MobileUseHWsRGBEncodingCVAR && MobileUseHWsRGBEncodingCVAR->GetValueOnAnyThread() == 1);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("OUTPUT_GAMMA_SPACE"), IsMobileHDR() == false && !bMobileUseHWsRGBEncoding);
		OutEnvironment.SetDefine(TEXT("OUTPUT_MOBILE_HDR"), IsMobileHDR() == true);
	}

	FOcclusionQueryVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
			StencilingGeometryParameters.Bind(Initializer.ParameterMap);
			ViewId.Bind(Initializer.ParameterMap, TEXT("ViewId"));
	}

	FOcclusionQueryVS() {}

	void SetParametersWithBoundingSphere(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& BoundingSphere)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, RHICmdList.GetBoundVertexShader(), View.ViewUniformBuffer);

		FVector4f StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, BoundingSphere, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);

		if (GEngine && GEngine->StereoRenderingDevice)
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ViewId, View.StereoViewIndex);
		}
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, RHICmdList.GetBoundVertexShader(),View.ViewUniformBuffer);

		// Don't transform if rendering frustum
		StencilingGeometryParameters.Set(RHICmdList, this, FVector4f(0,0,0,1));

		if (GEngine && GEngine->StereoRenderingDevice)
		{
			SetShaderValue(RHICmdList, RHICmdList.GetBoundVertexShader(), ViewId, View.StereoViewIndex);
		}
	}

private:
	LAYOUT_FIELD(FStencilingGeometryShaderParameters, StencilingGeometryParameters)
	LAYOUT_FIELD(FShaderParameter, ViewId)
};

/**
 * A pixel shader for rendering a texture on a simple element.
 */
class FOcclusionQueryPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOcclusionQueryPS, Global);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1); }

	FOcclusionQueryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer) {}
	FOcclusionQueryPS() {}
};

// Returns whether occlusion queries should be downsampled.
extern RENDERER_API bool UseDownsampledOcclusionQueries();
