// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationsRenderManager.h"

#include "DSP/BufferVectorOperations.h"
#include "DSP/FloatArrayMath.h"
#include "IWaveformTransformationRenderer.h"
#include "Sound/SoundWave.h"

FWaveformTransformationsRenderManager::FWaveformTransformationsRenderManager(TObjectPtr<USoundWave> InSoundWave, 
	TSharedRef<FWaveformEditorRenderData> InWaveformRenderData, 
	TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)> InPropertyChangeNotifier)
	: SoundWaveToRender(InSoundWave)
	, LayersFactory(MakeUnique<FWaveformTransformationRenderLayerFactory>(InWaveformRenderData, InPropertyChangeNotifier))
{
	GenerateLayersChain();	
}

void FWaveformTransformationsRenderManager::GenerateLayersChain()
{
	check(SoundWaveToRender);

	TransformationsToRender.Empty();
	RenderLayers.Empty();

	if (SoundWaveToRender->Transformations.Num() > 0)
	{
		for (const TObjectPtr<UWaveformTransformationBase> Transformation : SoundWaveToRender->Transformations)
		{
			if (Transformation != nullptr)
			{
				TransformationsToRender.Add(Transformation);
				
				TSharedPtr<IWaveformTransformationRenderer> TransformationUI = LayersFactory->Create(Transformation);
				FTransformationRenderLayerInfo RenderLayerInfo = FTransformationRenderLayerInfo(TransformationUI, FTransformationLayerConstraints(0.f, 1.f));
				RenderLayers.Add(RenderLayerInfo);
				
			}
		}

		CreateDurationHighlightLayer();

	}

	OnLayersChainGenerated.Broadcast(RenderLayers.GetData(), RenderLayers.Num());
}

void FWaveformTransformationsRenderManager::UpdateRenderElements()
{
	GenerateRenderDataInternal();
}

void FWaveformTransformationsRenderManager::GenerateRenderDataInternal()
{
	check(SoundWaveToRender);

	RawPCMData.Empty();
	uint16 NumChannels = 0;
	uint32 SampleRate = 0;

	if (!SoundWaveToRender->GetImportedSoundWaveData(RawPCMData, SampleRate, NumChannels))
	{
		UE_LOG(LogAudio, Warning, TEXT("Failed to get transformations render data for: %s"), *SoundWaveToRender->GetPathName());
		return;
	}

	uint32 NumOriginalSamples = RawPCMData.Num() * sizeof(uint8) / sizeof(int16);
	uint32 FirstEditedSample = 0;
	uint32 LastEditedSample = NumOriginalSamples;

	if (TransformationsToRender.Num() > 0)
	{
		Audio::FWaveformTransformationWaveInfo TransformationInfo;


		Audio::FAlignedFloatBuffer TransformationsBuffer;
		Audio::FAlignedFloatBuffer OutputBuffer;
		TransformationsBuffer.SetNumUninitialized(NumOriginalSamples);

		Audio::ArrayPcm16ToFloat(MakeArrayView((int16*)RawPCMData.GetData(), NumOriginalSamples), TransformationsBuffer);
		OutputBuffer = TransformationsBuffer;

		TransformationInfo.Audio = &TransformationsBuffer;
		TransformationInfo.NumChannels = NumChannels;
		TransformationInfo.SampleRate = SampleRate;

		TArray<Audio::FTransformationPtr> Transformations = CreateTransformations();

		const bool bChainChangesFileLength = CanChainChangeFileLength(Transformations);

		for (int32 i = 0; i < Transformations.Num(); ++i)
		{
			Transformations[i]->ProcessAudio(TransformationInfo);

			if (RenderLayers[i].Key)
			{
				FWaveformTransformationRenderInfo RenderLayerInfo{ TransformationInfo.SampleRate, TransformationInfo.NumChannels, FirstEditedSample, LastEditedSample - FirstEditedSample };
				RenderLayers[i].Key->SetTransformationWaveInfo(MoveTemp(RenderLayerInfo));

				float LeftWidgetConstraint = FirstEditedSample / (float) NumOriginalSamples;
				float RightWidgetConstraint = LastEditedSample / (float) NumOriginalSamples;

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
			
		if (!bChainChangesFileLength)
		{
			OutputBuffer = TransformationsBuffer;
		}
		
		const float MaxValue = Audio::ArrayMaxAbsValue(OutputBuffer);

		if (MaxValue > 1.f)
		{
			Audio::ArrayMultiplyByConstantInPlace(OutputBuffer, 1.f / MaxValue);
		}

		SampleRate = TransformationInfo.SampleRate;
		NumChannels = TransformationInfo.NumChannels;
		NumOriginalSamples = OutputBuffer.Num();
		
		check(NumChannels > 0);
		check(SampleRate > 0);

		Audio::ArrayFloatToPcm16(OutputBuffer, MakeArrayView((int16*)RawPCMData.GetData(), NumOriginalSamples));
	}

	OnRenderDataGenerated.Broadcast(RawPCMData.GetData(), NumOriginalSamples, FirstEditedSample, LastEditedSample, SampleRate, NumChannels);
	OnRenderElementsUpdated.Broadcast();
}

TArray<Audio::FTransformationPtr> FWaveformTransformationsRenderManager::CreateTransformations() const
{
	TArray<Audio::FTransformationPtr> TransformationPtrs;

	for (const TObjectPtr<UWaveformTransformationBase> TransformationBase : TransformationsToRender)
	{
		TransformationPtrs.Add(TransformationBase->CreateTransformation());
	}

	return TransformationPtrs;
}

const bool FWaveformTransformationsRenderManager::CanChainChangeFileLength(const TArray<Audio::FTransformationPtr>& TransformationChain) const
{
	bool bCanChainChangeFileLength = false;

	for (const Audio::FTransformationPtr& Transformation : TransformationChain)
	{
		bCanChainChangeFileLength |= Transformation->CanChangeFileLength();
	}

	return bCanChainChangeFileLength;
}

TArrayView<const FTransformationRenderLayerInfo> FWaveformTransformationsRenderManager::GetTransformLayers() const
{
	return MakeArrayView(RenderLayers.GetData(), RenderLayers.Num());
}

void FWaveformTransformationsRenderManager::CreateDurationHighlightLayer()
{
	if (!DurationHiglightLayer)
	{
		DurationHiglightLayer = LayersFactory->CreateDurationHiglightLayer();
	}

	DurationHiglightLayer->SetTransformationWaveInfo(FWaveformTransformationRenderInfo());
	RenderLayers.Emplace(DurationHiglightLayer, FTransformationLayerConstraints(0.f,1.f));
}