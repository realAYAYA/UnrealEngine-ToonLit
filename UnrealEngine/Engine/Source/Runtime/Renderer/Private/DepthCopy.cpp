// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DepthCopy.cpp: Depth rendering implementation.
=============================================================================*/

#include "DepthCopy.h"
#include "ScenePrivate.h"
#include "DataDrivenShaderPlatformInfo.h"

class FViewDepthCopyCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FViewDepthCopyCS)
		SHADER_USE_PARAMETER_STRUCT(FViewDepthCopyCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, RWDepthTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		END_SHADER_PARAMETER_STRUCT()

		using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static int32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FViewDepthCopyCS, "/Engine/Private/CopyDepthTextureCS.usf", "CopyDepthCS", SF_Compute);

void AddViewDepthCopyCSPass(FRDGBuilder& GraphBuilder, FViewInfo& View, FRDGTextureRef SourceSceneDepthTexture, FRDGTextureRef DestinationDepthTexture)
{
	FViewDepthCopyCS::FPermutationDomain PermutationVector;
	auto ComputeShader = View.ShaderMap->GetShader<FViewDepthCopyCS>(PermutationVector);

	FViewDepthCopyCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FViewDepthCopyCS::FParameters>();
	PassParameters->RWDepthTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DestinationDepthTexture));
	PassParameters->View = View.ViewUniformBuffer;
	PassParameters->SceneDepthTexture = SourceSceneDepthTexture;

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CopyViewDepthCS"),
		ComputeShader,
		PassParameters,
		FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FViewDepthCopyCS::GetGroupSize()));
}

