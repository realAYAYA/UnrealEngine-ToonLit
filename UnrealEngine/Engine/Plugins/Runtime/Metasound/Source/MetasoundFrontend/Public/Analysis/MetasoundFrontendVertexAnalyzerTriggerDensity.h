// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Containers/Array.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/EnvelopeFollower.h"
#include "MetasoundTrigger.h"


namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVertexAnalyzerTriggerDensity : public FVertexAnalyzerBase
		{
		public:
			static const FName& GetAnalyzerName();
			static const FName& GetDataType();

			struct METASOUNDFRONTEND_API FOutputs
			{
				static const FAnalyzerOutput& GetValue();

			};

			class METASOUNDFRONTEND_API FFactory : public TVertexAnalyzerFactory<FVertexAnalyzerTriggerDensity>
			{
			public:
				virtual const TArray<FAnalyzerOutput>& GetAnalyzerOutputs() const override
				{
					static const TArray<FAnalyzerOutput> Outputs { FOutputs::GetValue() };
					return Outputs;
				}
			};

			FVertexAnalyzerTriggerDensity(const FCreateAnalyzerParams& InParams);
			virtual ~FVertexAnalyzerTriggerDensity() = default;

			virtual void Execute() override;

		private:

			Audio::FEnvelopeFollower EnvelopeFollower;
			TDataWriteReference<float> EnvelopeValue;
			int32 NumFramesPerBlock = 0;
			Audio::FAlignedFloatBuffer ScratchBuffer;
		};
	} // namespace Frontend
} // namespace Metasound
