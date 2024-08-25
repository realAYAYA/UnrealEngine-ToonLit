// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzer.h"
#include "Containers/Array.h"
#include "MetasoundDataReferenceCollection.h"


namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVertexAnalyzerAudioBuffer : public FVertexAnalyzerBase
		{
		public:
			static const FName& GetAnalyzerName();
			static const FName& GetDataType();

			struct METASOUNDFRONTEND_API FOutputs
			{
				static const FAnalyzerOutput& GetValue();
			};

			class METASOUNDFRONTEND_API FFactory : public TVertexAnalyzerFactory<FVertexAnalyzerAudioBuffer>
			{
			public:
				virtual const TArray<FAnalyzerOutput>& GetAnalyzerOutputs() const override
				{
					static const TArray<FAnalyzerOutput> Outputs { FOutputs::GetValue() };
					return Outputs;
				}
			};

			FVertexAnalyzerAudioBuffer(const FCreateAnalyzerParams& InParams);
			virtual ~FVertexAnalyzerAudioBuffer() = default;

			virtual void Execute() override;

		private:
			FAudioBufferWriteRef AudioBuffer;
		};
	} // namespace Frontend
} // namespace Metasound
