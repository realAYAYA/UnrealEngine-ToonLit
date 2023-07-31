// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineAppleProResOutput.h"
#include "MoviePipeline.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineMasterConfig.h"
#include "ImagePixelData.h"
#include "MoviePipelineImageQuantization.h"
#include "SampleBuffer.h"
#include "MovieRenderPipelineCoreModule.h"
#include "ImageWriteTask.h"
#include "MovieRenderPipelineDataTypes.h"

// For logs
#include "MovieRenderPipelineCoreModule.h"

TUniquePtr<MovieRenderPipeline::IVideoCodecWriter> UMoviePipelineAppleProResOutput::Initialize_GameThread(const FString& InFileName, FIntPoint InResolution, EImagePixelType InPixelType, ERGBFormat InPixelFormat, uint8 InBitDepth, uint8 InNumChannels)
{
	const UMoviePipelineOutputSetting* OutputSettings = GetPipeline()->GetPipelineMasterConfig()->FindSetting<UMoviePipelineOutputSetting>();
	if (!OutputSettings)
	{
		return nullptr;
	}
	 
	FAppleProResEncoderOptions Options;
	Options.OutputFilename = InFileName;
	Options.Width = InResolution.X;
	Options.Height = InResolution.Y;
	Options.FrameRate = GetPipeline()->GetPipelineMasterConfig()->GetEffectiveFrameRate(GetPipeline()->GetTargetSequence());
	Options.MaxNumberOfEncodingThreads = bOverrideMaximumEncodingThreads ? MaxNumberOfEncodingThreads : 0; // Hardware Determine
	Options.Codec = Codec;
	Options.ColorPrimaries = EAppleProResEncoderColorPrimaries::CD_HDREC709; // Force Rec 709 for now
	Options.ScanMode = EAppleProResEncoderScanMode::IM_PROGRESSIVE_SCAN; // No interlace sources.
	Options.bWriteAlpha = true;

	TUniquePtr<FAppleProResEncoder> Encoder = MakeUnique<FAppleProResEncoder>(Options);
	
	TUniquePtr<FProResWriter> OutWriter = MakeUnique<FProResWriter>();
	OutWriter->Writer = MoveTemp(Encoder);
	OutWriter->FileName = InFileName;
	
	return OutWriter;
}

bool UMoviePipelineAppleProResOutput::Initialize_EncodeThread(MovieRenderPipeline::IVideoCodecWriter* InWriter)
{
	FProResWriter* CodecWriter = static_cast<FProResWriter*>(InWriter);
	if(!CodecWriter->Writer->Initialize())
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("Failed to initialize Apple Pro Res Writer."));
		return false;
	}

	return true;
}

void UMoviePipelineAppleProResOutput::WriteFrame_EncodeThread(MovieRenderPipeline::IVideoCodecWriter* InWriter, FImagePixelData* InPixelData, TArray<MoviePipeline::FCompositePassInfo>&& InCompositePasses)
{
	FProResWriter* CodecWriter = static_cast<FProResWriter*>(InWriter);
	FImagePixelDataPayload* PipelinePayload = InPixelData->GetPayload<FImagePixelDataPayload>();
	
	// Translate our Movie Pipeline specific payload to a ProRes Encoder specific payload.
	TSharedPtr<FAppleProResEncoder::FTimecodePayload, ESPMode::ThreadSafe> ProResPayload = MakeShared<FAppleProResEncoder::FTimecodePayload, ESPMode::ThreadSafe>();

	// This is the frame number on the global time, can have overlaps (between encoders) or repeats when using handle frames/slowmo.
	ProResPayload->ReferenceFrameNumber = PipelinePayload->SampleState.OutputState.SourceFrameNumber;

	// ProRes can handle quantization internally but expects sRGB to be applied to the incoming data.
	TUniquePtr<FImagePixelData> QuantizedPixelData = UE::MoviePipeline::QuantizeImagePixelDataToBitDepth(InPixelData, 16, ProResPayload, InWriter->bConvertToSrgb);

	// Do a quick composite of renders/burn-ins.
	TArray<FPixelPreProcessor> PixelPreProcessors;
	for (const MoviePipeline::FCompositePassInfo& CompositePass : InCompositePasses)
	{
		// We don't need to copy the data here (even though it's being passed to a async system) because we already made a unique copy of the
		// burn in/widget data when we decided to composite it.
		switch (QuantizedPixelData->GetType())
		{
		case EImagePixelType::Color:
			PixelPreProcessors.Add(TAsyncCompositeImage<FColor>(CompositePass.PixelData->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float16:
			PixelPreProcessors.Add(TAsyncCompositeImage<FFloat16Color>(CompositePass.PixelData->MoveImageDataToNew()));
			break;
		case EImagePixelType::Float32:
			PixelPreProcessors.Add(TAsyncCompositeImage<FLinearColor>(CompositePass.PixelData->MoveImageDataToNew()));
			break;
		}
	}

	// This is done on the main thread for simplicity but the composite itself is parallaleized.
	FImagePixelData* PixelData = QuantizedPixelData.Get();
	for (const FPixelPreProcessor& PreProcessor : PixelPreProcessors)
	{
		// PreProcessors are assumed to be valid.
		PreProcessor(PixelData);
	}

	CodecWriter->Writer->WriteFrame(QuantizedPixelData.Get());
}

void UMoviePipelineAppleProResOutput::BeginFinalize_EncodeThread(MovieRenderPipeline::IVideoCodecWriter* InWriter)
{
	const MoviePipeline::FAudioState& AudioData = GetPipeline()->GetAudioState();
	FProResWriter* CodecWriter = static_cast<FProResWriter*>(InWriter);

	// The AppleProResEncoder implementation does not currently support encoding audio.
	/*for (const MoviePipeline::FAudioState::FAudioSegment& AudioSegment : AudioData.FinishedSegments)
	{
		double StartTime = FPlatformTime::Seconds();
		Audio::TSampleBuffer<int16> SampleBuffer = Audio::TSampleBuffer<int16>(AudioSegment.SegmentData.GetData(), AudioSegment.SegmentData.Num(), AudioSegment.NumChannels, AudioSegment.SampleRate);
		UE_LOG(LogMovieRenderPipeline, Log, TEXT("Audio Segment took %f seconds to convert to a sample buffer."), (FPlatformTime::Seconds() - StartTime));

		const TArrayView<int16> SampleData = SampleBuffer.GetArrayView(); // MakeArrayView<int16>(SampleBuffer.GetData(), SampleBuffer.GetNumSamples());
		CodecWriter->Writer->WriteAudioSample(SampleData);
	}*/
	
}

void UMoviePipelineAppleProResOutput::Finalize_EncodeThread(MovieRenderPipeline::IVideoCodecWriter* InWriter)
{
	// Commit this to disk.
	FProResWriter* CodecWriter = static_cast<FProResWriter*>(InWriter);
	CodecWriter->Writer->Finalize();
}

#if WITH_EDITOR
FText UMoviePipelineAppleProResOutput::GetDisplayText() const
{
	// When it's called from the CDO it's in the drop down menu so they haven't selected a setting yet.
	if(HasAnyFlags(RF_ArchetypeObject))
	{
		 return NSLOCTEXT("MovieRenderPipeline", "AppleProRes_DisplayNameVariedBits", "Apple ProRes [10-12bit]");
	}
	
	if(Codec == EAppleProResEncoderCodec::ProRes_4444XQ || Codec == EAppleProResEncoderCodec::ProRes_4444)
	{
		return NSLOCTEXT("MovieRenderPipeline", "AppleProRes_DisplayName12Bit", "Apple ProRes [12bit]");
	}
	else
	{
		return NSLOCTEXT("MovieRenderPipeline", "AppleProRes_DisplayName10Bit", "Apple ProRes [10bit]");
	}
}
#endif
