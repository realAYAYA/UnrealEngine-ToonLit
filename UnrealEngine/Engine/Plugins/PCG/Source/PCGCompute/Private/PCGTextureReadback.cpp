// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGTextureReadback.h"

#include "GlobalShader.h"
#include "RHIGPUReadback.h"
#include "RenderGraphUtils.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "PCGCompute"
#define PCG_NUM_THREADS_PER_GROUP_DIMENSION 8

class PCGCOMPUTE_API FPCGTextureReadbackCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FPCGTextureReadbackCS);
	SHADER_USE_PARAMETER_STRUCT(FPCGTextureReadbackCS, FGlobalShader);
 
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2DArray<float4>, SourceTextureArray)
		SHADER_PARAMETER_SAMPLER(SamplerState, SourceSampler)
		SHADER_PARAMETER(FVector2f, SourceDimensions)
		SHADER_PARAMETER(uint32, SourceTextureIndex)
		SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()
 
public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), PCG_NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), PCG_NUM_THREADS_PER_GROUP_DIMENSION);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FPCGTextureReadbackCS, "/PCGComputeShaders/PCGTextureReadback.usf", "PCGTextureReadback_CS", SF_Compute);

void FPCGTextureReadbackInterface::Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback)
{
	check(Params.SourceTexture && Params.SourceSampler);

	FPCGTextureReadbackCS::FParameters PassParameters;
	PassParameters.SourceTextureArray = Params.SourceTexture;
	PassParameters.SourceSampler = Params.SourceSampler;
	PassParameters.SourceDimensions = { (float)Params.SourceDimensions.X, (float)Params.SourceDimensions.Y };
	PassParameters.SourceTextureIndex = Params.SourceTextureIndex;

	FRHITextureCreateDesc TargetTextureDesc =
		FRHITextureCreateDesc::Create2D(TEXT("PCGTexture Readback Compute Target"), Params.SourceDimensions.X, Params.SourceDimensions.Y, EPixelFormat::PF_B8G8R8A8)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::UAV |
				ETextureCreateFlags::RenderTargetable |
				ETextureCreateFlags::ShaderResource |
				ETextureCreateFlags::NoTiling
			)
			.SetInitialState(ERHIAccess::UAVCompute)
			.DetermineInititialState();
	check(TargetTextureDesc.IsValid());
	
	// Create temporary output texture
	FTextureRHIRef OutputTexture = RHICreateTexture(TargetTextureDesc);
	PassParameters.OutputTexture = RHICmdList.CreateUnorderedAccessView(OutputTexture,
		FRHIViewDesc::CreateTextureUAV()
			.SetDimensionFromTexture(OutputTexture)
			.SetMipLevel(0)
			.SetFormat(TargetTextureDesc.Format)
			.SetArrayRange(0, 0)
		);

	TShaderMapRef<FPCGTextureReadbackCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters,
		FIntVector(FMath::DivideAndRoundUp(Params.SourceDimensions.X, PCG_NUM_THREADS_PER_GROUP_DIMENSION),
			FMath::DivideAndRoundUp(Params.SourceDimensions.Y, PCG_NUM_THREADS_PER_GROUP_DIMENSION), 1));

	// Prepare OutputTexture to be copied
	RHICmdList.Transition(FRHITransitionInfo(OutputTexture, ERHIAccess::UAVCompute, ERHIAccess::CopySrc));

	FRHIGPUTextureReadback* GPUTextureReadback = new FRHIGPUTextureReadback(TEXT("PCGTextureReadbackCopy"));
	GPUTextureReadback->EnqueueCopy(RHICmdList, PassParameters.OutputTexture->GetTexture());

	auto RunnerFunc = [GPUTextureReadback, AsyncCallback](auto&& RunnerFunc) -> void
	{
		if (GPUTextureReadback->IsReady())
		{
			int32 ReadbackWidth = 0, ReadbackHeight = 0;
			void* OutBuffer = GPUTextureReadback->Lock(ReadbackWidth, &ReadbackHeight);

			AsyncCallback(OutBuffer, ReadbackWidth, ReadbackHeight);

			GPUTextureReadback->Unlock();
			delete GPUTextureReadback;
		}
		else
		{
			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
			{
				RunnerFunc(RunnerFunc);
			});
		}
	};

	AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]()
	{
		RunnerFunc(RunnerFunc);
	});
}

void FPCGTextureReadbackInterface::Dispatch_GameThread(const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
		{
			Dispatch_RenderThread(RHICmdList, Params, AsyncCallback);
		});
}

void FPCGTextureReadbackInterface::Dispatch(const FPCGTextureReadbackDispatchParams& Params, const TFunction<void(void* OutBuffer, int32 ReadbackWidth, int32 ReadbackHeight)>& AsyncCallback)
{
	if (IsInRenderingThread())
	{
		Dispatch_RenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
	}
	else
	{
		Dispatch_GameThread(Params, AsyncCallback);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCG_NUM_THREADS_PER_GROUP_DIMENSION
