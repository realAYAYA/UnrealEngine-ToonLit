// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationRenderLayerFactory.h"

#include "IWaveformTransformation.h"
#include "Styling/AppStyle.h"
#include "WaveformEditorTransportCoordinator.h"
#include "WaveformTransformationDurationRenderer.h"
#include "WaveformTransformationTrimFade.h"
#include "WaveformTransformationTrimFadeRenderer.h"

FWaveformTransformationRenderLayerFactory::FWaveformTransformationRenderLayerFactory(TSharedRef<FWaveformEditorRenderData> InWaveformRenderData, 
	TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)> InTransformationChangeNotifier)
	: WaveformRenderData(InWaveformRenderData)
	, TransformationChangeNotifier(InTransformationChangeNotifier)
{
}


TSharedPtr<IWaveformTransformationRenderer> FWaveformTransformationRenderLayerFactory::Create(TObjectPtr<UWaveformTransformationBase> InTransformationToRender) const
{
	TSharedPtr<IWaveformTransformationRenderer> OutRenderer = nullptr;
	UClass* TransformationClass = InTransformationToRender->GetClass();

	if (TransformationClass == UWaveformTransformationTrimFade::StaticClass())
	{
		TSharedPtr<IWaveformTransformationRenderer> TrimFadeLayer = MakeShared<FWaveformTransformationTrimFadeRenderer>(Cast<UWaveformTransformationTrimFade>(InTransformationToRender));
		TrimFadeLayer->SetTransformationNotifier(TransformationChangeNotifier);
		OutRenderer = TrimFadeLayer;
	}

	return OutRenderer;
}

TSharedPtr<IWaveformTransformationRenderer> FWaveformTransformationRenderLayerFactory::CreateDurationHiglightLayer() const
{
	TSharedPtr<IWaveformTransformationRenderer> DurationHiglightLayer = MakeShared<FWaveformTransformationDurationRenderer>(WaveformRenderData.ToSharedRef());
	return DurationHiglightLayer;
}
