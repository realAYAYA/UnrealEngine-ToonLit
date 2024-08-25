// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskPostProcess_DistanceField.h"

#include "GeometryMaskModule.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "UnrealClient.h"

DECLARE_GPU_STAT_NAMED(GeometryMaskJFInit, TEXT("GeometryMaskJFInit"));
DECLARE_GPU_STAT_NAMED(GeometryMaskJFStep, TEXT("GeometryMaskJFStep"));
DECLARE_GPU_STAT_NAMED(GeometryMaskJFtoDF, TEXT("GeometryMaskJFtoDF"));

namespace UE::GeometryMask::Private
{
	static constexpr int32 MaxNumChannels = 4;
	static constexpr int32 NumNeighbors = 8;
	static constexpr int32 MaxSteps = 13;

	static FIntVector4 CalculateStepCount(const FIntVector4& InRadiusSizes)
	{
		return FIntVector4(
			InRadiusSizes[0] == 0 ? 0 : FMath::CeilToInt32(FMath::Log2(static_cast<double>(InRadiusSizes[0]))) - 1,
			InRadiusSizes[1] == 0 ? 0 : FMath::CeilToInt32(FMath::Log2(static_cast<double>(InRadiusSizes[1]))) - 1,
			InRadiusSizes[2] == 0 ? 0 : FMath::CeilToInt32(FMath::Log2(static_cast<double>(InRadiusSizes[2]))) - 1,
			InRadiusSizes[3] == 0 ? 0 : FMath::CeilToInt32(FMath::Log2(static_cast<double>(InRadiusSizes[3]))) - 1);
	}

	static FIntVector4 CalculateStepSize(int32 InStepIdx, const FIntVector4& InMaxSteps, FIntVector4& OutMask)
	{
		FIntVector4 StepDelta = InMaxSteps - FIntVector4(InStepIdx, InStepIdx, InStepIdx, InStepIdx);
		StepDelta.X = FMath::Min(InMaxSteps.X, StepDelta.X);
		StepDelta.Y = FMath::Min(InMaxSteps.Y, StepDelta.Y);
		StepDelta.Z = FMath::Min(InMaxSteps.Z, StepDelta.Z);
		StepDelta.W = FMath::Min(InMaxSteps.W, StepDelta.W);

		FIntVector4 StepSize(
			FMath::CeilToInt32(FMath::Pow<double>(2, StepDelta.X)),
			FMath::CeilToInt32(FMath::Pow<double>(2, StepDelta.Y)),
			FMath::CeilToInt32(FMath::Pow<double>(2, StepDelta.Z)),
			FMath::CeilToInt32(FMath::Pow<double>(2, StepDelta.W)));

		OutMask[0] = StepSize.X > 0;
		OutMask[1] = StepSize.Y > 0;
		OutMask[2] = StepSize.Z > 0;
		OutMask[3] = StepSize.W > 0;

		return StepSize;
	}
}

class FGeometryMaskJFInitCSBase : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FGeometryMaskJFInitCSBase, NonVirtual);
	SHADER_USE_PARAMETER_STRUCT(FGeometryMaskJFInitCSBase, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_ARRAY(FVector4f, Offsets, [UE::GeometryMask::Private::MaxNumChannels * (UE::GeometryMask::Private::NumNeighbors + 1)])
		SHADER_PARAMETER(FIntPoint, InputDimensions)
		SHADER_PARAMETER(FVector2f, OneOverInputDimensions)
		SHADER_PARAMETER(FVector2f, UVRatioAdjustment)
		SHADER_PARAMETER(FIntVector4, ChannelMap)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, OutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), FComputeShaderUtils::kGolden2DGroupSize);
	}
};

template <int32 NumChannels>
class TGeometryMaskJFInitCS : public FGeometryMaskJFInitCSBase
{
	DECLARE_GLOBAL_SHADER(TGeometryMaskJFInitCS);
	SHADER_USE_PARAMETER_STRUCT(TGeometryMaskJFInitCS, FGeometryMaskJFInitCSBase);

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("NUM_CHANNELS"), NumChannels);
		OutEnvironment.SetDefine(TEXT("KERNEL_SIZE"), 3);
	}
};

using FGeometryMaskJFInitCS1 = TGeometryMaskJFInitCS<1>;
using FGeometryMaskJFInitCS2 = TGeometryMaskJFInitCS<2>;
using FGeometryMaskJFInitCS3 = TGeometryMaskJFInitCS<3>;
using FGeometryMaskJFInitCS4 = TGeometryMaskJFInitCS<4>;

IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFInitCS1, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFInitCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFInitCS2, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFInitCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFInitCS3, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFInitCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFInitCS4, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFInitCS.usf"), TEXT("MainCS"), SF_Compute);

class FGeometryMaskJFStepCSBase : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FGeometryMaskJFStepCSBase, NonVirtual);
	SHADER_USE_PARAMETER_STRUCT(FGeometryMaskJFStepCSBase, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntVector4, StepSize)
		SHADER_PARAMETER(FIntVector4, ChannelMask)
		SHADER_PARAMETER(FIntPoint, InputDimensions)
		SHADER_PARAMETER(FVector2f, OneOverInputDimensions)
		SHADER_PARAMETER(FVector2f, UVRatioAdjustment)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, InputBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, OutputBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("MAX_STEPS"), UE::GeometryMask::Private::MaxSteps);
		OutEnvironment.SetDefine(TEXT("KERNEL_SIZE"), 3);
	}
};

template <int32 NumChannels>
class TGeometryMaskJFStepCS : public FGeometryMaskJFStepCSBase
{
	DECLARE_GLOBAL_SHADER(TGeometryMaskJFStepCS);
	SHADER_USE_PARAMETER_STRUCT(TGeometryMaskJFStepCS, FGeometryMaskJFStepCSBase);

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGeometryMaskJFStepCSBase::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NUM_CHANNELS"), NumChannels);
	}
};

using FGeometryMaskJFStepCS1 = TGeometryMaskJFStepCS<1>;
using FGeometryMaskJFStepCS2 = TGeometryMaskJFStepCS<2>;
using FGeometryMaskJFStepCS3 = TGeometryMaskJFStepCS<3>;
using FGeometryMaskJFStepCS4 = TGeometryMaskJFStepCS<4>;

IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFStepCS1, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFStepCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFStepCS2, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFStepCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFStepCS3, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFStepCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFStepCS4, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFStepCS.usf"), TEXT("MainCS"), SF_Compute);

class FGeometryMaskJFtoDFCSBase : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FGeometryMaskJFtoDFCSBase, NonVirtual);
	SHADER_USE_PARAMETER_STRUCT(FGeometryMaskJFtoDFCSBase, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, InputDimensions)
		SHADER_PARAMETER(FVector2f, OneOverInputDimensions)
		SHADER_PARAMETER(FVector2f, UVRatioAdjustment)
		SHADER_PARAMETER(FIntVector4, ChannelMap)
		SHADER_PARAMETER(FVector4f, StepDistanceMultipliers)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, OriginalInputTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, OriginalInputSampler)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<float4>, InputBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), FComputeShaderUtils::kGolden2DGroupSize);
	}
};

template <int32 NumChannels>
class TGeometryMaskJFtoDFCS : public FGeometryMaskJFtoDFCSBase
{
	DECLARE_GLOBAL_SHADER(TGeometryMaskJFtoDFCS);
	SHADER_USE_PARAMETER_STRUCT(TGeometryMaskJFtoDFCS, FGeometryMaskJFtoDFCSBase);

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGeometryMaskJFtoDFCSBase::ModifyCompilationEnvironment(InParameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("NUM_CHANNELS"), NumChannels);
	}
};

using FGeometryMaskJFtoDFCS1 = TGeometryMaskJFtoDFCS<1>;
using FGeometryMaskJFtoDFCS2 = TGeometryMaskJFtoDFCS<2>;
using FGeometryMaskJFtoDFCS3 = TGeometryMaskJFtoDFCS<3>;
using FGeometryMaskJFtoDFCS4 = TGeometryMaskJFtoDFCS<4>;

IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFtoDFCS1, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFtoDFCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFtoDFCS2, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFtoDFCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFtoDFCS3, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFtoDFCS.usf"), TEXT("MainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, FGeometryMaskJFtoDFCS4, TEXT("/Plugin/GeometryMask/Private/GeometryMaskJFtoDFCS.usf"), TEXT("MainCS"), SF_Compute);

FGeometryMaskPostProcessParameters_DistanceField::FGeometryMaskPostProcessParameters_DistanceField()
{
	bPerChannelCalculateDF.Init(false, 4);
	PerChannelRadius.Init(0.0, 4);
}

FGeometryMaskPostProcess_DistanceField::FGeometryMaskPostProcess_DistanceField(const FGeometryMaskPostProcessParameters_DistanceField& InParameters)
	: Super(InParameters)
{
}

void FGeometryMaskPostProcess_DistanceField::Execute(FRenderTarget* InTexture)
{
	if (!InTexture)
	{
		return;
	}

	// If blur NOT applied to any channel, early-out 
	if (!Parameters.bPerChannelCalculateDF.Contains(true))
	{
		return;
	}
	
	ENQUEUE_RENDER_COMMAND(CanvasRenderTargetMakeCurrentCommand)(
	[Self = AsShared(), InTexture](FRHICommandListImmediate& InRHICmdList)
	{
		Self->Execute_RenderThread(InRHICmdList, InTexture);
	});
}

void FGeometryMaskPostProcess_DistanceField::Execute_RenderThread(
	FRHICommandListImmediate& InRHICmdList,
	FRenderTarget* InTexture)
{
	ensure(IsInRenderingThread());

	uint8 DebugPass = 0; // 0 = no debug
	
#if WITH_EDITORONLY_DATA
	DebugPass = GetDefault<UGeometryMaskSettings>()->DebugDF;
#endif

	FRDGBuilder GraphBuilder(InRHICmdList);
	{
		FIntPoint InputSize = InTexture->GetSizeXY();
		FVector2f OneOverInputSize = FVector2f::One() / InputSize;

		bool bInputSizeChanged = LastInputSize != InputSize;
		LastInputSize = InputSize;

		// Scales UV's such that the X axis is always 1.0
		FVector2f InputHeightOverWidth = FVector2f(1.0f, static_cast<float>(InputSize.Y) / static_cast<float>(InputSize.X));

		FIntVector NumGroups = FComputeShaderUtils::GetGroupCount(InputSize, FIntPoint(8, 8));
		int32 NumActiveChannels = Parameters.bPerChannelCalculateDF.CountSetBits();

		FIntVector4 ChannelMap(ForceInitToZero);
		{
			int32 ActiveChannelIdx = 0;
			for (int32 ChannelIdx = 0; ChannelIdx < UE::GeometryMask::Private::MaxNumChannels; ++ChannelIdx)
			{
				for (int32 ChannelMaskIdx = ActiveChannelIdx; ChannelMaskIdx < Parameters.bPerChannelCalculateDF.Num(); ++ChannelMaskIdx)
				{
					if (Parameters.bPerChannelCalculateDF[ChannelMaskIdx])
					{
						ActiveChannelIdx = ChannelMaskIdx;
						break;
					}
				}

				ChannelMap[ChannelIdx] = ActiveChannelIdx;
				++ActiveChannelIdx;
			}
		}

		// Used to calculate step size per-channel
		FIntVector4 SizeFromRadius;
		for (int32 ChannelIdx = 0; ChannelIdx < UE::GeometryMask::Private::MaxNumChannels; ++ChannelIdx)
		{
			SizeFromRadius[ChannelIdx] = Parameters.PerChannelRadius[ChannelIdx];
		}

		bool bNumActiveChannelsChanged = LastNumActiveChannels != NumActiveChannels;
		LastNumActiveChannels = NumActiveChannels;

		FIntPoint BufferSize = InputSize;
		BufferSize.X *= NumActiveChannels;
		
		const uint32 BufferLength = BufferSize.X * BufferSize.Y;

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

		TShaderRef<FGeometryMaskJFInitCSBase> InitComputeShader;
		TShaderRef<FGeometryMaskJFStepCSBase> StepComputeShader;
		TShaderRef<FGeometryMaskJFtoDFCSBase> OutputComputeShader;
		
		{
			TShaderMapRef<FGeometryMaskJFInitCS1> InitComputeShader1(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFInitCS2> InitComputeShader2(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFInitCS3> InitComputeShader3(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFInitCS4> InitComputeShader4(GlobalShaderMap);

			TShaderMapRef<FGeometryMaskJFStepCS1> StepComputeShader1(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFStepCS2> StepComputeShader2(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFStepCS3> StepComputeShader3(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFStepCS4> StepComputeShader4(GlobalShaderMap);

			TShaderMapRef<FGeometryMaskJFtoDFCS1> OutputComputeShader1(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFtoDFCS2> OutputComputeShader2(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFtoDFCS3> OutputComputeShader3(GlobalShaderMap);
			TShaderMapRef<FGeometryMaskJFtoDFCS4> OutputComputeShader4(GlobalShaderMap);

			if (NumActiveChannels == 1)
			{
				InitComputeShader = InitComputeShader1;
				StepComputeShader = StepComputeShader1;
				OutputComputeShader = OutputComputeShader1;
			}
			else if (NumActiveChannels == 2)
			{
				InitComputeShader = InitComputeShader2;
				StepComputeShader = StepComputeShader2;
				OutputComputeShader = OutputComputeShader2;
			}
			else if (NumActiveChannels == 3)
			{
				InitComputeShader = InitComputeShader3;
				StepComputeShader = StepComputeShader3;
				OutputComputeShader = OutputComputeShader3;
			}
			else if(NumActiveChannels == 4)
			{
				InitComputeShader = InitComputeShader4;
				StepComputeShader = StepComputeShader4;
				OutputComputeShader = OutputComputeShader4;
			}
		}

		FRDGTextureRef InputTexture = InTexture->GetRenderTargetTexture(GraphBuilder);
		FRDGTextureSRVRef InputTexture_SRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(InputTexture));
		FRDGTextureUAVRef InputTexture_UAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(InputTexture));

#if WITH_EDITOR
		bInputSizeChanged = DebugPass > 0;
#endif

		FVector4f* BufferData = nullptr;
		if (bInputSizeChanged || bNumActiveChannelsChanged)
		{
			BufferData = GraphBuilder.AllocPODArray<FVector4f>(BufferLength);
		}

		FRDGBufferRef InitOutputBuffer = nullptr;
		if (bInputSizeChanged || bNumActiveChannelsChanged)
		{
			FRDGBufferDesc InitOutputBufferDesc =
				FRDGBufferDesc::CreateBufferDesc(
					sizeof(FVector4f),
					BufferLength);
			InitOutputBufferDesc.Usage = EBufferUsageFlags::UnorderedAccess;

			InitOutputBuffer = GraphBuilder.CreateBuffer(InitOutputBufferDesc, TEXT("GeometryMaskJFInit.OutputBuffer"));
			StoredInitOutputBuffer = GraphBuilder.ConvertToExternalBuffer(InitOutputBuffer);

			GraphBuilder.QueueBufferUpload(InitOutputBuffer, BufferData, sizeof(FVector4f) * BufferLength, ERDGInitialDataFlags::NoCopy);
		}
		else
		{
			InitOutputBuffer = GraphBuilder.RegisterExternalBuffer(StoredInitOutputBuffer, TEXT("GeometryMaskJFInit.OutputBuffer"));
		}

		// 1. Init from input binary-ish texture
		{
			FRDGBufferUAVRef OutputBuffer_UAV = GraphBuilder.CreateUAV(InitOutputBuffer, EPixelFormat::PF_FloatRGBA);

			{
				RDG_GPU_STAT_SCOPE(GraphBuilder, GeometryMaskJFInit);
				RDG_EVENT_SCOPE(GraphBuilder, "GeometryMaskJFInit");
				TRACE_CPUPROFILER_EVENT_SCOPE(GeometryMaskJFInit);
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FGeometryMaskPostProcess_DistanceField::GeometryMaskJFInit"), STAT_GeometryMask_GeometryMaskJFInit, STATGROUP_GeometryMask);

				FGeometryMaskJFInitCSBase::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeometryMaskJFInitCSBase::FParameters>();
				{
					PassParameters->InputDimensions = InputSize;
					PassParameters->OneOverInputDimensions = OneOverInputSize;
					PassParameters->UVRatioAdjustment = InputHeightOverWidth;
					PassParameters->ChannelMap = ChannelMap;
					PassParameters->InputTexture = InputTexture_SRV;
					PassParameters->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
					PassParameters->OutputBuffer = OutputBuffer_UAV;
				}

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("Init"),
					ERDGPassFlags::Compute,
					InitComputeShader,
					PassParameters,
					NumGroups);
			}
		}

		FRDGBufferSRVRef StepOutputBuffer_SRV = nullptr;

		
#if WITH_EDITOR
		// DebugPass == 1 - Only sobel
		if (DebugPass == 1)
		{
			StepOutputBuffer_SRV = GraphBuilder.CreateSRV(InitOutputBuffer, EPixelFormat::PF_FloatRGBA);
		}
		else
#endif
		// 2. JumpFlood steps
		{
			FRDGBufferRef StepIntermediateBufferB = nullptr;
			if (bInputSizeChanged || bNumActiveChannelsChanged)
			{
				FRDGBufferDesc StepIntermediateBufferDescB =
					FRDGBufferDesc::CreateBufferDesc(
						sizeof(FVector4f),
						BufferLength);
				StepIntermediateBufferDescB.Usage = EBufferUsageFlags::UnorderedAccess;

				StepIntermediateBufferB = GraphBuilder.CreateBuffer(StepIntermediateBufferDescB, TEXT("GeometryMaskJFStep.IntermediateBufferB"));
				StoredStepIntermediateBufferB = GraphBuilder.ConvertToExternalBuffer(StepIntermediateBufferB);

				GraphBuilder.QueueBufferUpload(StepIntermediateBufferB, BufferData, sizeof(FVector4f) * BufferLength, ERDGInitialDataFlags::NoCopy);
			}
			else
			{
				StepIntermediateBufferB = GraphBuilder.RegisterExternalBuffer(StoredInitOutputBuffer, TEXT("GeometryMaskJFInit.IntermediateBufferB"));
			}

			FRDGBufferUAVRef StepIntermediateBufferA_UAV = GraphBuilder.CreateUAV(InitOutputBuffer, EPixelFormat::PF_FloatRGBA);
			FRDGBufferUAVRef StepIntermediateBufferB_UAV = GraphBuilder.CreateUAV(StepIntermediateBufferB, EPixelFormat::PF_FloatRGBA);

			FRDGBufferSRVRef StepIntermediateBufferA_SRV = GraphBuilder.CreateSRV(InitOutputBuffer, EPixelFormat::PF_FloatRGBA);
			FRDGBufferSRVRef StepIntermediateBufferB_SRV = GraphBuilder.CreateSRV(StepIntermediateBufferB, EPixelFormat::PF_FloatRGBA);

			StepOutputBuffer_SRV = StepIntermediateBufferA_SRV;

			{
				FIntVector4 StepCount = UE::GeometryMask::Private::CalculateStepCount(SizeFromRadius);
				FIntVector4 ChannelMask(ForceInitToZero);
				int32 MaxStepCount = FMath::Max(FMath::Max(FMath::Max(StepCount.X, StepCount.Y), StepCount.Z), StepCount.W);
				int32 BufferIdx = 0;

#if WITH_EDITOR
				if (MaxStepCount > UE::GeometryMask::Private::MaxSteps)
				{
					UE_LOG(LogGeometryMask, Warning, TEXT("JumpFlood requires too many steps (%u), maximum is %u."), MaxStepCount, UE::GeometryMask::Private::MaxSteps);
				}
				
				if (DebugPass > 1)
				{
					MaxStepCount = FMath::Min(MaxStepCount, DebugPass - 1);
				}
#endif

				for (int32 StepIdx = 0; StepIdx < MaxStepCount; ++StepIdx)
				{
					RDG_GPU_STAT_SCOPE(GraphBuilder, GeometryMaskJFStep);
					RDG_EVENT_SCOPE(GraphBuilder, "GeometryMaskJFStep");
					TRACE_CPUPROFILER_EVENT_SCOPE(GeometryMaskJFStep);
					DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FGeometryMaskPostProcess_DistanceField::GeometryMaskJFStep"), STAT_GeometryMask_GeometryMaskJFStep, STATGROUP_GeometryMask);

					FGeometryMaskJFStepCSBase::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeometryMaskJFStepCSBase::FParameters>();
					{
						PassParameters->InputDimensions = InputSize;
						PassParameters->OneOverInputDimensions = OneOverInputSize;
						PassParameters->UVRatioAdjustment = InputHeightOverWidth;

						PassParameters->StepSize = UE::GeometryMask::Private::CalculateStepSize(StepIdx, StepCount, ChannelMask);
						PassParameters->ChannelMask = ChannelMask;
						
						PassParameters->InputBuffer = BufferIdx > 0
							? StepIntermediateBufferB_SRV
							: StepIntermediateBufferA_SRV;

						PassParameters->OutputBuffer = BufferIdx > 0
							? StepIntermediateBufferA_UAV
							: StepIntermediateBufferB_UAV;
					}

					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("Step"),
						ERDGPassFlags::Compute,
						StepComputeShader,
						PassParameters,
						NumGroups);

					BufferIdx = (BufferIdx + 1) % 2;
				}

				StepOutputBuffer_SRV = BufferIdx > 0
					? StepIntermediateBufferB_SRV
					: StepIntermediateBufferA_SRV;
			}
		}

		// 3. JF to DF
		{
			{
				RDG_GPU_STAT_SCOPE(GraphBuilder, GeometryMaskJFtoDF);
				RDG_EVENT_SCOPE(GraphBuilder, "GeometryMaskJFtoDF");
				TRACE_CPUPROFILER_EVENT_SCOPE(GeometryMaskJFtoDF);
				DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FGeometryMaskPostProcess_DistanceField::GeometryMaskJFtoDF"), STAT_GeometryMask_GeometryMaskJFtoDF, STATGROUP_GeometryMask);

				FGeometryMaskJFtoDFCSBase::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeometryMaskJFtoDFCSBase::FParameters>();
				{
					FVector4f StepDistanceMultipliers;
					for (int32 ChannelIdx = 0; ChannelIdx < UE::GeometryMask::Private::MaxNumChannels; ++ChannelIdx)
					{
						const float Radius = Parameters.bPerChannelCalculateDF[ChannelIdx] ? FMath::Max(1, Parameters.PerChannelRadius[ChannelIdx]) : InputSize.GetMax();
						StepDistanceMultipliers[ChannelIdx] = static_cast<float>(InputSize.GetMax()) / Radius; 
					}
					
					PassParameters->InputDimensions = InputSize;
					PassParameters->OneOverInputDimensions = OneOverInputSize;
					PassParameters->UVRatioAdjustment = InputHeightOverWidth;
					PassParameters->ChannelMap = ChannelMap;
					PassParameters->StepDistanceMultipliers = StepDistanceMultipliers;
					PassParameters->OriginalInputTexture = InputTexture_SRV;
					PassParameters->OriginalInputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
					PassParameters->InputBuffer = StepOutputBuffer_SRV;
					PassParameters->OutputTexture = InputTexture_UAV;
				}

				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("CopyToDF"),
					ERDGPassFlags::Compute,
					OutputComputeShader,
					PassParameters,
					NumGroups);
			}
		}
	}

	GraphBuilder.Execute();
}
