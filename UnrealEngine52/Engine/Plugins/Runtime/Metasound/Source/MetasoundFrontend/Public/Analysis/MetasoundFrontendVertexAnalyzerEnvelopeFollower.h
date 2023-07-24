// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "Containers/Array.h"
#include "DSP/EnvelopeFollower.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundRouter.h"


namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVertexAnalyzerEnvelopeFollower : public FVertexAnalyzerBase
		{
		public:
			static const FName& GetAnalyzerName();
			static const FName& GetDataType();

			struct METASOUNDFRONTEND_API FOutputs
			{
				static const FAnalyzerOutput& GetValue();
			};

			class METASOUNDFRONTEND_API FFactory : public TVertexAnalyzerFactory<FVertexAnalyzerEnvelopeFollower>
			{
			public:
				virtual const TArray<FAnalyzerOutput>& GetAnalyzerOutputs() const override
				{
					static const TArray<FAnalyzerOutput> Outputs { FOutputs::GetValue() };
					return Outputs;
				}
			};

			FVertexAnalyzerEnvelopeFollower(const FCreateAnalyzerParams& InParams);
			virtual ~FVertexAnalyzerEnvelopeFollower() = default;

			virtual void Execute() override;

		private:
			Audio::FEnvelopeFollower EnvelopeFollower;
			TDataWriteReference<float> EnvelopeValue;
		};
	} // namespace Frontend
} // namespace Metasound
