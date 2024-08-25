// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"

#include "HarmonixMetasound/DataTypes/MusicTimestamp.h"
#include "HarmonixMetasound/DataTypes/TimeSignature.h"

namespace HarmonixMetasound::Analysis
{
	class HARMONIXMETASOUND_API FMidiClockVertexAnalyzer final : public Metasound::Frontend::FVertexAnalyzerBase
	{
	public:
		struct HARMONIXMETASOUND_API FOutputs
		{
			/**
			 * @brief Get the default output for this analyzer
			 * @return The default output
			 */
			static const Metasound::Frontend::FAnalyzerOutput& GetValue();
			
			static const Metasound::Frontend::FAnalyzerOutput Timestamp;
			static const Metasound::Frontend::FAnalyzerOutput Tempo;
			static const Metasound::Frontend::FAnalyzerOutput TimeSignature;
			static const Metasound::Frontend::FAnalyzerOutput Speed;
		};

		class HARMONIXMETASOUND_API FFactory final : public Metasound::Frontend::TVertexAnalyzerFactory<FMidiClockVertexAnalyzer>
		{
		public:
			virtual const TArray<Metasound::Frontend::FAnalyzerOutput>& GetAnalyzerOutputs() const override;
		};

		static const FName& GetAnalyzerName();
		static const FName& GetDataType();

		explicit FMidiClockVertexAnalyzer(const Metasound::Frontend::FCreateAnalyzerParams& InParams);

		virtual void Execute() override;

	private:
		FMusicTimestampWriteRef Timestamp;
		Metasound::FFloatWriteRef Tempo;
		FTimeSignatureWriteRef TimeSignature;
		Metasound::FFloatWriteRef Speed;
	};
}
