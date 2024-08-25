// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundDataReference.h"
#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"

#include "HarmonixMetasound/DataTypes/MidiEventInfo.h"

namespace HarmonixMetasound::Analysis
{
	class HARMONIXMETASOUND_API FMidiStreamVertexAnalyzer final : public Metasound::Frontend::FVertexAnalyzerBase
	{
	public:
		static const FName& GetAnalyzerName();
		static const FName& GetDataType();

		struct HARMONIXMETASOUND_API FOutputs
		{
			static const Metasound::Frontend::FAnalyzerOutput& GetValue();
		};

		class HARMONIXMETASOUND_API FFactory final : public Metasound::Frontend::TVertexAnalyzerFactory<FMidiStreamVertexAnalyzer>
		{
		public:
			virtual const TArray<Metasound::Frontend::FAnalyzerOutput>& GetAnalyzerOutputs() const override;
		};

		explicit FMidiStreamVertexAnalyzer(const Metasound::Frontend::FCreateAnalyzerParams& InParams);
		virtual ~FMidiStreamVertexAnalyzer() override = default;

		virtual void Execute() override;

	private:
		FMidiEventInfoWriteRef LastMidiEvent;
	};
}
