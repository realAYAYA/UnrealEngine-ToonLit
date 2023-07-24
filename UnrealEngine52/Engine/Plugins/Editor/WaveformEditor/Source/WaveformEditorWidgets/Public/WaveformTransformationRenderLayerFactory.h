// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectPtr.h"
#include "Templates/SharedPointer.h"

class IWaveformTransformationRenderer;
class UWaveformTransformationBase;
class IPropertyHandle;

class WAVEFORMEDITORWIDGETS_API FWaveformTransformationRenderLayerFactory
{
public:
	~FWaveformTransformationRenderLayerFactory() = default;

	TSharedPtr<IWaveformTransformationRenderer> Create(TObjectPtr<UWaveformTransformationBase> InTransformationToRender);
	TSharedPtr<IWaveformTransformationRenderer> Create(TObjectPtr<UWaveformTransformationBase> InTransformationToRender, TArray<TSharedRef<IPropertyHandle>>& TransformationsProperties);

};