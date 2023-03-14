// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaTextureSampleConverter.h"

#include "RenderGraphBuilder.h"
#include "RivermaxShaders.h"
#include "RivermaxMediaTextureSample.h"


DECLARE_GPU_STAT(RivermaxSource_SampleConversion);


void FRivermaxMediaTextureSampleConverter::Setup(ERivermaxMediaSourcePixelFormat InPixelFormat, TWeakPtr<FRivermaxMediaTextureSample> InSample, bool bInDoSRGBToLinear)
{
	InputPixelFormat = InPixelFormat;
	Sample = InSample;
	bDoSRGBToLinear = bInDoSRGBToLinear;
}

bool FRivermaxMediaTextureSampleConverter::Convert(FTexture2DRHIRef& InDestinationTexture, const FConversionHints& Hints)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::Convert);

	using namespace UE::RivermaxShaders;

	TSharedPtr<FRivermaxMediaTextureSample> SamplePtr = Sample.Pin();
	if (SamplePtr.IsValid() == false)
	{
		return false;
	}

	FIntVector GroupCount;
	
	FRDGBuilder GraphBuilder(FRHICommandListExecutor::GetImmediateCommandList());
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RivermaxSource_SampleConversion)
		SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, Rivermax_SampleConverter);

		FRDGTextureRef OutputResource = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(InDestinationTexture, TEXT("RivermaxMediaTextureOutputResource")));

		//Configure shader and add conversion pass based on desired pixel format
		switch (InputPixelFormat)
		{
		case ERivermaxMediaSourcePixelFormat::YUV422_8bit:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::YUV8ShaderSetup);
			const int32 BytesPerElement = sizeof(FYUV8Bit422ToRGBACS::FYUV8Bit422Buffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FYUV8Bit422ToRGBACS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FYUV8Bit422ToRGBACS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputYUVBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 2;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput, InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FYUV8Bit422ToRGBACS> ComputeShader(GlobalShaderMap, PermutationVector);
			FMatrix YUVToRGBMatrix = SamplePtr->GetYUVToRGBMatrix();
			FVector YUVOffset(MediaShaders::YUVOffset8bits);
			FYUV8Bit422ToRGBACS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputYUVBuffer, OutputResource, YUVToRGBMatrix, YUVOffset, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("YUV8Bit422ToRGBA")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		case ERivermaxMediaSourcePixelFormat::YUV422_10bit:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::YUV10ShaderSetup);
			const int32 BytesPerElement = sizeof(FYUV10Bit422ToRGBACS::FYUV10Bit422LEBuffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FYUV10Bit422ToRGBACS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FYUV10Bit422ToRGBACS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputYUVBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 8;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput, InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FYUV10Bit422ToRGBACS> ComputeShader(GlobalShaderMap, PermutationVector);
			FMatrix YUVToRGBMatrix = SamplePtr->GetYUVToRGBMatrix();
			FVector YUVOffset(MediaShaders::YUVOffset10bits);
			FYUV10Bit422ToRGBACS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputYUVBuffer, OutputResource, YUVToRGBMatrix, YUVOffset, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("YUV10Bit422ToRGBA")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_8bit:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::RGB8ShaderSetup);
			const int32 BytesPerElement = sizeof(FRGB8BitToRGBA8CS::FRGB8BitBuffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FRGB8BitToRGBA8CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRGB8BitToRGBA8CS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputRGGBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 4;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput,InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FRGB8BitToRGBA8CS> ComputeShader(GlobalShaderMap, PermutationVector);
			FRGB8BitToRGBA8CS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputRGGBuffer, OutputResource, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("RGB8BitToRGBA8")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_10bit:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::RGB10ShaderSetup);

			const int32 BytesPerElement = sizeof(FRGB10BitToRGBA10CS::FRGB10BitBuffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FRGB10BitToRGBA10CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRGB10BitToRGBA10CS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputRGGBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 16;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput,InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FRGB10BitToRGBA10CS> ComputeShader(GlobalShaderMap, PermutationVector);
			FRGB10BitToRGBA10CS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputRGGBuffer, OutputResource, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("RGB10BitToRGBA")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_12bit:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::RGB12ShaderSetup);

			const int32 BytesPerElement = sizeof(FRGB12BitToRGBA12CS::FRGB12BitBuffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FRGB12BitToRGBA12CS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRGB12BitToRGBA12CS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputRGGBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 8;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput,InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FRGB12BitToRGBA12CS> ComputeShader(GlobalShaderMap, PermutationVector);
			FRGB12BitToRGBA12CS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputRGGBuffer, OutputResource, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("RGB12BitToRGBA")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		case ERivermaxMediaSourcePixelFormat::RGB_16bit_Float:
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(RivermaxSampleConverter::RGB16FloatShaderSetup);

			const int32 BytesPerElement = sizeof(FRGB16fBitToRGBA16fCS::FRGB16fBuffer);
			const uint32 Stride = SamplePtr->GetStride();
			const int32 ElementsPerRow = (Stride / BytesPerElement) + ((Stride % BytesPerElement > 0) ? 1 : 0);
			const int32 ElementCount = ElementsPerRow * InDestinationTexture->GetDesc().Extent.Y;

			FRGB16fBitToRGBA16fCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRGB16fBitToRGBA16fCS::FSRGBToLinear>(bDoSRGBToLinear);

			FRDGBufferRef InputRGGBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("RivermaxInputBuffer"), BytesPerElement, ElementCount, SamplePtr->GetBuffer(), BytesPerElement * ElementCount);
			constexpr int32 PixelsPerInput = 2;
			const FIntPoint ProcessedOutputDimension = { InDestinationTexture->GetDesc().Extent.X / PixelsPerInput,InDestinationTexture->GetDesc().Extent.Y };
			GroupCount = FComputeShaderUtils::GetGroupCount(ProcessedOutputDimension, FComputeShaderUtils::kGolden2DGroupSize);
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FRGB16fBitToRGBA16fCS> ComputeShader(GlobalShaderMap, PermutationVector);
			FRGB16fBitToRGBA16fCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InputRGGBuffer, OutputResource, ElementsPerRow, ProcessedOutputDimension.Y);


			FComputeShaderUtils::AddPass(
				GraphBuilder
				, RDG_EVENT_NAME("RGB16fBitToRGBA")
				, ComputeShader
				, Parameters
				, GroupCount);
			break;
		}
		default:
		{
			ensureMsgf(false, TEXT("Unhandled pixel format (%d) given to Rivermax MediaSample converter"), InputPixelFormat);
			return false;
		}
		}
	}

	GraphBuilder.Execute();

	return true;
}

uint32 FRivermaxMediaTextureSampleConverter::GetConverterInfoFlags() const
{
	return IMediaTextureSampleConverter::ConverterInfoFlags_NeedUAVOutputTexture;
}
