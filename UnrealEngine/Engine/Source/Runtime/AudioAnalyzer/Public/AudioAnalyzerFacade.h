// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "IAudioAnalyzerInterface.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

namespace Audio
{

	IAnalyzerFactory* GetAnalyzerFactory(FName InFactoryName);

	/** FAnalyzer
	 *
	 * FAnalyzer provides a simplified interface for performing analysis on audio buffers from the same AudioAnalyzer.
	 */
	class AUDIOANALYZER_API FAnalyzerFacade
	{
		public:
			/**
			 * Create an FAnalyzerBatch with the analyzer settings and factory name.
			 */
			FAnalyzerFacade(TUniquePtr<IAnalyzerSettings> InSettings, IAnalyzerFactory* InFactory);

			/**
			 * Analyze the audio buffer.
			 */
			TUniquePtr<IAnalyzerResult> AnalyzeAudioBuffer(const TArray<float>& InAudioBuffer, int32 InNumChannels, float InSampleRate);

		private:

			TUniquePtr<IAnalyzerSettings> Settings;
			TUniquePtr<IAnalyzerWorker> Worker;

			IAnalyzerFactory* Factory;
			FName FactoryName;
	};
}
