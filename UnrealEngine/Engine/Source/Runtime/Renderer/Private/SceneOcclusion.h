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

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FSphere& BoundingSphere)
	{
		FVector4f StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, BoundingSphere, View.ViewMatrices.GetPreViewTranslation());
		SetParametersInternal(BatchedParameters, View, StencilingSpherePosAndScale);
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View)
	{
		SetParametersInternal(BatchedParameters, View, FVector4f(0, 0, 0, 1));
	}

private:
	void SetParametersInternal(FRHIBatchedShaderParameters& BatchedParameters, const FViewInfo& View, const FVector4f& StencilingSpherePosAndScale)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(BatchedParameters, View.ViewUniformBuffer);

		// Don't transform if rendering frustum
		StencilingGeometryParameters.Set(BatchedParameters, StencilingSpherePosAndScale);

		if (ViewId.IsBound())
		{
			SetShaderValue(BatchedParameters, ViewId, (View.StereoPass == EStereoscopicPass::eSSP_FULL) ? 0 : View.StereoViewIndex);
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
	FOcclusionQueryPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer) {}
	FOcclusionQueryPS() {}
};

// Returns whether occlusion queries should be downsampled.
extern RENDERER_API bool UseDownsampledOcclusionQueries();
