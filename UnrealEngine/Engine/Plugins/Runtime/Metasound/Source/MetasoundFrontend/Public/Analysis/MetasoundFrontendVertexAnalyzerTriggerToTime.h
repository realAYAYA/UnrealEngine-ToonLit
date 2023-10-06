// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundDataReference.h"
#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"

namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVertexAnalyzerTriggerToTime final : public FVertexAnalyzerBase
		{
		public:
			static const FName& GetAnalyzerName();
			static const FName& GetDataType();

			struct METASOUNDFRONTEND_API FOutputs
			{
				static const FAnalyzerOutput& GetValue();
			};

			class METASOUNDFRONTEND_API FFactory final : public TVertexAnalyzerFactory<FVertexAnalyzerTriggerToTime>
			{
			public:
				virtual const TArray<FAnalyzerOutput>& GetAnalyzerOutputs() const override;
			};

			explicit FVertexAnalyzerTriggerToTime(const FCreateAnalyzerParams& InParams);
			virtual ~FVertexAnalyzerTriggerToTime() override = default;

			virtual void Execute() override;

		private:
			const FSampleRate SampleRate;
			const int32 FramesPerBlock;
			const double SecondsPerBlock;
			FTime BlockStartTime{ 0.0f };
			TDataWriteReference<FTime> LastTriggerTime;
		};
	} // namespace Frontend
} // namespace Metasound
