// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"

#include "HarmonixMetasound/DataTypes/FFTAnalyzerResult.h"
#include "MetasoundDataReference.h"

namespace HarmonixMetasound::Analysis
{
	class HARMONIXMETASOUND_API FFFTAnalyzerResultVertexAnalyzer final : public Metasound::Frontend::FVertexAnalyzerBase
	{
	public:
		static const FName& GetAnalyzerName();
		static const FName& GetDataType();

		struct HARMONIXMETASOUND_API FOutputs
		{
			static const Metasound::Frontend::FAnalyzerOutput& GetValue();
		};

		class HARMONIXMETASOUND_API FFactory final : public Metasound::Frontend::TVertexAnalyzerFactory<FFFTAnalyzerResultVertexAnalyzer>
		{
		public:
			virtual const TArray<Metasound::Frontend::FAnalyzerOutput>& GetAnalyzerOutputs() const override;
		};

		explicit FFFTAnalyzerResultVertexAnalyzer(const Metasound::Frontend::FCreateAnalyzerParams& InParams);
		virtual ~FFFTAnalyzerResultVertexAnalyzer() override = default;

		virtual void Execute() override;

	private:
		FHarmonixFFTAnalyzerResultsWriteRef LastAnalyzerResult;
	};
}
