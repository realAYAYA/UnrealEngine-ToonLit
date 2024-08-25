// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEDenoiserIOProcessOidn.h"
#include "NNEDenoiserLog.h"
#include "NNEDenoiserParameters.h"
#include "NNEDenoiserShadersOidnCS.h"
#include "NNEDenoiserTransferFunction.h"
#include "NNEDenoiserUtils.h"
#include "RHIStaticStates.h"

DECLARE_GPU_STAT_NAMED(FNNEDenoiserOidnPreOrPostprocessing, TEXT("NNEDenoiser.OidnPreOrPostprocessing"));

namespace UE::NNEDenoiser::Private
{

namespace IOProcessOidnHelper
{

void AddPreOrPostProcess(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef InputTexture,
	EResourceName TensorName,
	int32 FrameIdx,
	FRDGTextureRef OutputTexture)
{
	using namespace UE::NNEDenoiserShaders::Internal;

	FHDRTransferFunction TransferFunction{HDRMax};

	const FIntVector InputTextureSize = InputTexture->Desc.GetSize();
	const FIntVector OutputTextureSize = OutputTexture->Desc.GetSize();

	FNNEDenoiserOidnCS::FParameters *ShaderParameters = GraphBuilder.AllocParameters<FNNEDenoiserOidnCS::FParameters>();
	ShaderParameters->InputTextureWidth = InputTextureSize.X;
	ShaderParameters->InputTextureHeight = InputTextureSize.Y;
	ShaderParameters->InputTexture = InputTexture;
	ShaderParameters->OutputTextureWidth = OutputTextureSize.X;
	ShaderParameters->OutputTextureHeight = OutputTextureSize.Y;
	ShaderParameters->OutputTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutputTexture));
	ShaderParameters->NormScale = TransferFunction.GetNormScale();
	ShaderParameters->InvNormScale = TransferFunction.GetInvNormScale();

	FNNEDenoiserOidnCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FNNEDenoiserOidnCS::FNNEDenoiserInputKind>(GetInputKind(TensorName));

	FIntVector ThreadGroupCount = FIntVector(
		FMath::DivideAndRoundUp(OutputTextureSize.X, FNNEDenoiserOidnConstants::THREAD_GROUP_SIZE),
		FMath::DivideAndRoundUp(OutputTextureSize.Y, FNNEDenoiserOidnConstants::THREAD_GROUP_SIZE),
		1);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FNNEDenoiserOidnCS> Shader(GlobalShaderMap, PermutationVector);

	RDG_EVENT_SCOPE(GraphBuilder, "NNEDenoiser.OidnPreOrPostprocessing");
	RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEDenoiserOidnPreOrPostprocessing);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("NNEDenoiser.OidnPreOrPostprocessing"),
		ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
		Shader,
		ShaderParameters,
		ThreadGroupCount);
}

} // namespace IOProcessOidnHelper

bool FInputProcessOidn::HasPreprocessInput(EResourceName TensorName, int32 FrameIdx) const
{
	check(FrameIdx == 0);

	switch(TensorName)
	{
		case EResourceName::Color:
		case EResourceName::Albedo:
		case EResourceName::Normal:
			return true;
	}
	return false;
}

void FInputProcessOidn::PreprocessInput(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef Texture,
	EResourceName TensorName,
	int32 FrameIdx,
	FRDGTextureRef PreprocessedTexture) const
{
	IOProcessOidnHelper::AddPreOrPostProcess(GraphBuilder, Texture, TensorName, FrameIdx, PreprocessedTexture);
}

bool FOutputProcessOidn::HasPostprocessOutput(EResourceName TensorName, int32 FrameIdx) const
{
	check(FrameIdx == 0);

	return TensorName == EResourceName::Output;
}

void FOutputProcessOidn::PostprocessOutput(
	FRDGBuilder& GraphBuilder,
	FRDGTextureRef Texture,
	FRDGTextureRef PostprocessedTexture) const
{
	IOProcessOidnHelper::AddPreOrPostProcess(GraphBuilder, Texture, EResourceName::Output, 0, PostprocessedTexture);
}

} // namespace UE::NNEDenoiser::Private