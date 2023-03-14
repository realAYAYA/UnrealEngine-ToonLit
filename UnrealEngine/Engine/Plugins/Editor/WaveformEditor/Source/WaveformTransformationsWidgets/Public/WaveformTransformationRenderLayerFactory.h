// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"

class FWaveformEditorRenderData;
class FWaveformEditorTransportCoordinator;
class IWaveformTransformationRenderer;
class UWaveformTransformationBase;

class FWaveformTransformationRenderLayerFactory
{
public:
	explicit FWaveformTransformationRenderLayerFactory(
		TSharedRef<FWaveformEditorRenderData> InWaveformRenderData, 
		TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)> InTransformationChangeNotifier);

	~FWaveformTransformationRenderLayerFactory() = default;

	TSharedPtr<IWaveformTransformationRenderer> Create(TObjectPtr<UWaveformTransformationBase> InTransformationToRender) const;
	TSharedPtr<IWaveformTransformationRenderer> CreateDurationHiglightLayer () const;

private:
	TSharedPtr<FWaveformEditorRenderData> WaveformRenderData = nullptr;
	TSharedPtr<FWaveformEditorTransportCoordinator> TransportCoordinator = nullptr;
	TFunction<void(FPropertyChangedEvent&, FEditPropertyChain*)> TransformationChangeNotifier;
};