// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWaveformTransformationRenderer.h"

using FWaveformTransformRendererInstantiator = TFunction<TSharedPtr<IWaveformTransformationRenderer>()>;

class WAVEFORMTRANSFORMATIONSWIDGETS_API FWaveformTransformationRendererMapper
{
public:
	/** Access the singleton instance for mapper */
	static FWaveformTransformationRendererMapper& Get();
	static void Init();

	template<typename ConcreteRendererType>
	bool RegisterRenderer(const UClass* SupportedTransformation)
	{
		FWaveformTransformRendererInstantiator Instantiator = []()
		{
			return MakeShared<ConcreteRendererType>();
		};

		check(Instance)
		Instance->TransformationsTypeMap.Add(SupportedTransformation, Instantiator);
		return true;
	}

	void UnregisterRenderer(const UClass* SupportedTransformation);

	FWaveformTransformRendererInstantiator* GetRenderer(const UClass* WaveformTransformationClass);

private:
	static TUniquePtr<FWaveformTransformationRendererMapper> Instance;

	TMap<const UClass*, FWaveformTransformRendererInstantiator> TransformationsTypeMap;
};