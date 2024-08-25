// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorSequenceDataProvider.h"

#include "Audio.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/FloatArrayMath.h"
#include "IWaveformTransformationRenderer.h"
#include "Sound/SoundWave.h"
#include "WaveformTransformationDurationRenderer.h"

FWaveformEditorSequenceDataProvider::FWaveformEditorSequenceDataProvider(
	TObjectPtr<USoundWave> InSoundWave, 
	FOnTransformationsPropertiesRequired InTransformationChainPropertiesHandler)
	: SoundWaveToRender(InSoundWave)
	, LayersFactory(MakeUnique<FWaveformTransformationRenderLayerFactory>())
	, ChainPropertiesHandler(InTransformationChainPropertiesHandler)
{
	GenerateLayersChain();	

	if (!SoundWaveToRender->GetImportedSoundWaveData(ImportedRawPCMData, ImportedSampleRate, ImportedNumChannels))
	{
		UE_LOG(LogAudio, Warning, TEXT("Failed to get transformations render data for: %s"), *SoundWaveToRender->GetPathName());
		return;
	}
}

FFixedSampledSequenceView FWaveformEditorSequenceDataProvider::RequestSequenceView(const TRange<double> DataRatioRange)
{
	check(DataRatioRange.GetLowerBoundValue() >= 0)
	check(DataRatioRange.GetUpperBoundValue() <= 1)
	check(DataRatioRange.GetLowerBoundValue() != DataRatioRange.GetUpperBoundValue())

	const int32 NumChannels = ImportedNumChannels;
	const uint8 MinFramesToDisplay = 2;
	const uint32 MinSamplesToDisplay = MinFramesToDisplay * NumChannels;
	const uint32 NumOriginalSamples = TransformedPCMData.Num();
	const uint32 NumOriginalFrames = NumOriginalSamples / NumChannels;

	const uint32 FirstRenderedSample = FMath::Clamp(FMath::RoundToInt32(NumOriginalFrames * DataRatioRange.GetLowerBoundValue()), 0, NumOriginalFrames - MinFramesToDisplay) * NumChannels;
	const uint32 NumFramesToRender = FMath::RoundToInt32(NumOriginalFrames * DataRatioRange.Size<double>());
	const uint32 NumSamplesToRender = FMath::Clamp(NumFramesToRender * NumChannels, MinSamplesToDisplay, NumOriginalSamples - FirstRenderedSample);

	check(NumSamplesToRender % NumChannels == 0 && FirstRenderedSample % NumChannels == 0);
;
	TArrayView<const float> SampleData = MakeArrayView(TransformedPCMData.GetData(), TransformedPCMData.Num()).Slice(FirstRenderedSample, NumSamplesToRender);

	FFixedSampledSequenceView DataView{ SampleData , NumChannels, ImportedSampleRate};
	OnDataViewGenerated.Broadcast(DataView, FirstRenderedSample);

	return DataView;
}

void FWaveformEditorSequenceDataProvider::GenerateLayersChain()
{
	check(SoundWaveToRender);
		
	TransformationsToRender.Empty();
	RenderLayers.Empty();

	if (SoundWaveToRender->Transformations.Num() > 0)
	{
		FTransformationsToPropertiesArray TransformPropertiesMap;

		for (const TObjectPtr<UWaveformTransformationBase>& Transformation : SoundWaveToRender->Transformations)
		{
			if (Transformation != nullptr)
			{
				TArray<TSharedRef<IPropertyHandle>> TransformationPropertiesArray;
				TransformPropertiesMap.Emplace(Transformation, TransformationPropertiesArray);
			}
		}

		ChainPropertiesHandler.Execute(TransformPropertiesMap);
		

		for (FTransformationToPropertiesPair& ObjPropsPair : TransformPropertiesMap)
		{
			TransformationsToRender.Add(ObjPropsPair.Key);
			TSharedPtr<IWaveformTransformationRenderer> TransformationUI = LayersFactory->Create(ObjPropsPair.Key, ObjPropsPair.Value);
			FTransformationRenderLayerInfo RenderLayerInfo = FTransformationRenderLayerInfo(TransformationUI, FTransformationLayerConstraints(0.f, 1.f));
			RenderLayers.Add(RenderLayerInfo);
		}

		CreateDurationHighlightLayer();
	}

	OnLayersChainGenerated.Broadcast(RenderLayers.GetData(), RenderLayers.Num());
}

void FWaveformEditorSequenceDataProvider::UpdateRenderElements()
{
	GenerateSequenceDataInternal();
	OnRenderElementsUpdated.Broadcast();
}

void FWaveformEditorSequenceDataProvider::GenerateSequenceDataInternal()
{
	check(SoundWaveToRender);
	
	uint32 NumWaveformSamples = ImportedRawPCMData.Num() * sizeof(uint8) / sizeof(int16);
	NumOriginalWaveformFrames = NumWaveformSamples / ImportedNumChannels;
	uint32 FirstEditedSample = 0;
	uint32 LastEditedSample = NumWaveformSamples;

	if (TransformationsToRender.Num() > 0)
	{
		Audio::FWaveformTransformationWaveInfo TransformationInfo;

		Audio::FAlignedFloatBuffer TransformationsBuffer;
		Audio::FAlignedFloatBuffer OutputBuffer;

		TransformationsBuffer.SetNumUninitialized(NumWaveformSamples);

		Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)ImportedRawPCMData.GetData(), NumWaveformSamples), TransformationsBuffer);
		OutputBuffer = TransformationsBuffer;

		TransformationInfo.Audio = &TransformationsBuffer;
		TransformationInfo.NumChannels = ImportedNumChannels;
		TransformationInfo.SampleRate = ImportedSampleRate;

		TArray<Audio::FTransformationPtr> Transformations = CreateTransformations();

		const bool bChainChangesFileLength = CanChainChangeFileLength(Transformations);

		for (int32 i = 0; i < Transformations.Num(); ++i)
		{
			Transformations[i]->ProcessAudio(TransformationInfo);

			if (RenderLayers[i].Key)
			{
				FWaveformTransformationRenderInfo RenderLayerInfo{ TransformationInfo.SampleRate, TransformationInfo.NumChannels, FirstEditedSample, LastEditedSample - FirstEditedSample };
				RenderLayers[i].Key->SetTransformationWaveInfo(MoveTemp(RenderLayerInfo));

				float LeftWidgetConstraint = FirstEditedSample / (float) NumWaveformSamples;
				float RightWidgetConstraint = LastEditedSample / (float) NumWaveformSamples;

				RenderLayers[i].Value = FTransformationLayerConstraints(LeftWidgetConstraint, RightWidgetConstraint);

			}

			FirstEditedSample += TransformationInfo.StartFrameOffset;
			LastEditedSample = TransformationInfo.NumEditedSamples <= 0 ? LastEditedSample : FirstEditedSample + TransformationInfo.NumEditedSamples;

			if (bChainChangesFileLength)
			{
				check(LastEditedSample > FirstEditedSample);
				const int32 EditedBufferSize = LastEditedSample - FirstEditedSample;
				FMemory::Memcpy(&OutputBuffer.GetData()[FirstEditedSample], TransformationsBuffer.GetData(), EditedBufferSize * sizeof(float));
			}

			TransformationInfo.StartFrameOffset = 0;
			TransformationInfo.NumEditedSamples = 0;
		}

		check(DurationHiglightLayer)
		FWaveformTransformationRenderInfo DurationLayerInfo{ TransformationInfo.SampleRate, TransformationInfo.NumChannels, FirstEditedSample, LastEditedSample - FirstEditedSample };
		DurationHiglightLayer->SetTransformationWaveInfo(MoveTemp(DurationLayerInfo));
		DurationHiglightLayer->SetOriginalWaveformFrames(NumOriginalWaveformFrames);
		

		UpdateTransformedWaveformBounds(FirstEditedSample, LastEditedSample, NumWaveformSamples);

		if (!bChainChangesFileLength)
		{
			if (TransformedPCMData.Num() != TransformationsBuffer.Num())
			{
				TransformedPCMData.SetNumUninitialized(TransformationsBuffer.Num());
			}
			FMemory::Memcpy(TransformedPCMData.GetData(), TransformationsBuffer.GetData(), TransformationsBuffer.GetAllocatedSize());
		}
		else
		{
			if (TransformedPCMData.Num() != OutputBuffer.Num())
			{
				TransformedPCMData.SetNumUninitialized(OutputBuffer.Num());
			}
			FMemory::Memcpy(TransformedPCMData.GetData(), OutputBuffer.GetData(), OutputBuffer.GetAllocatedSize());
		}
		
		const float MaxValue = Audio::ArrayMaxAbsValue(TransformedPCMData);

		if (MaxValue > 1.f)
		{
			Audio::ArrayMultiplyByConstantInPlace(TransformedPCMData, 1.f / MaxValue);
		}
		
		check(TransformationInfo.NumChannels > 0);
		check(TransformationInfo.SampleRate > 0);
	}
	else
	{
		if (TransformedPCMData.Num() != NumWaveformSamples)
		{
			TransformedPCMData.SetNumUninitialized(NumWaveformSamples);

		}
		Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)ImportedRawPCMData.GetData(), NumWaveformSamples), TransformedPCMData);
	}
}

void FWaveformEditorSequenceDataProvider::UpdateTransformedWaveformBounds(const uint32 FirstEditedSample, const uint32 LastEditedSample, const uint32 NumOriginalSamples)
{
	const double FirstEditedSampleOffset = FirstEditedSample / (double) NumOriginalSamples;
	const double LastEditedSampleOffset = (NumOriginalSamples - LastEditedSample) / (double) NumOriginalSamples;

	TransformedWaveformBounds.SetLowerBoundValue(FirstEditedSampleOffset);
	TransformedWaveformBounds.SetUpperBoundValue(1 - LastEditedSampleOffset);
}

TArray<Audio::FTransformationPtr> FWaveformEditorSequenceDataProvider::CreateTransformations() const
{
	TArray<Audio::FTransformationPtr> TransformationPtrs;

	for (const TObjectPtr<UWaveformTransformationBase>& TransformationBase : TransformationsToRender)
	{
		TransformationPtrs.Add(TransformationBase->CreateTransformation());
	}

	return TransformationPtrs;
}

const bool FWaveformEditorSequenceDataProvider::CanChainChangeFileLength(const TArray<Audio::FTransformationPtr>& TransformationChain) const
{
	bool bCanChainChangeFileLength = false;

	for (const Audio::FTransformationPtr& Transformation : TransformationChain)
	{
		bCanChainChangeFileLength |= Transformation->CanChangeFileLength();
	}

	return bCanChainChangeFileLength;
}

TArrayView<const FTransformationRenderLayerInfo> FWaveformEditorSequenceDataProvider::GetTransformLayers() const
{
	return MakeArrayView(RenderLayers.GetData(), RenderLayers.Num());
}

const TRange<double> FWaveformEditorSequenceDataProvider::GetTransformedWaveformBounds() const
{
	return TransformedWaveformBounds;
}

void FWaveformEditorSequenceDataProvider::CreateDurationHighlightLayer()
{
	if (!DurationHiglightLayer)
	{
		DurationHiglightLayer = MakeShared<FWaveformTransformationDurationRenderer>(NumOriginalWaveformFrames);
	}

	DurationHiglightLayer->SetTransformationWaveInfo(FWaveformTransformationRenderInfo());
	RenderLayers.Emplace(DurationHiglightLayer, FTransformationLayerConstraints(0.f,1.f));
}
