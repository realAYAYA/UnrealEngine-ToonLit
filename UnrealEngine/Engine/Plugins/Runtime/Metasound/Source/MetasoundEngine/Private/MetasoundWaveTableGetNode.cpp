// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/FloatArrayMath.h"
#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundTrace.h"
#include "MetasoundWaveTable.h"
#include "WaveTable.h"

#define LOCTEXT_NAMESPACE "MetasoundEngine"


namespace Metasound
{
	class FMetasoundWaveTableGetNodeOperator : public TExecutableOperator<FMetasoundWaveTableGetNodeOperator>
	{
	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace WaveTable;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FWaveTableBankAsset>("WaveTableBank", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableGetNode_InputWaveTableBank", "WaveTableBank") }),
					TInputDataVertex<float>("TableIndex", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableGetNode_InputTableIndex", "Index") }, 0.0f)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FWaveTable>("Out", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableGetNode_OutputWaveTable", "Out") })
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata
				{
					FNodeClassName { EngineNodes::Namespace, "MetasoundWaveTableGet", "" },
					1, // Major Version
					0, // Minor Version
					LOCTEXT("MetasoundWaveTableGetNode_Name", "Get WaveTable From Bank"),
					LOCTEXT("MetasoundWaveTableGetNode_Description", "Gets or generates interpolated WaveTable from provided WaveTableBank asset."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					GetDefaultInterface(),
					{ },
					{ },
					{ }
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			using namespace Metasound;

			const FInputVertexInterface& InputInterface = InParams.Node.GetVertexInterface().GetInputInterface();
			const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;

			FWaveTableBankAssetReadRef WaveTableBankReadRef = InputCollection.GetDataReadReferenceOrConstruct<FWaveTableBankAsset>("WaveTableBank");
			FFloatReadRef TableIndexReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, "TableIndex", InParams.OperatorSettings);

			return MakeUnique<FMetasoundWaveTableGetNodeOperator>(InParams, WaveTableBankReadRef, TableIndexReadRef);
		}

		FMetasoundWaveTableGetNodeOperator(
			const FCreateOperatorParams& InParams,
			const FWaveTableBankAssetReadRef& InWaveTableBankReadRef,
			const FFloatReadRef& InTableIndexReadRef)
			: WaveTableBankReadRef(InWaveTableBankReadRef)
			, TableIndexReadRef(InTableIndexReadRef)
			, OutTable(TDataWriteReferenceFactory<WaveTable::FWaveTable>::CreateAny(InParams.OperatorSettings))
		{
		}

		virtual ~FMetasoundWaveTableGetNodeOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace Metasound;

			FDataReferenceCollection Inputs;
			Inputs.AddDataReadReference("WaveTableBank", WaveTableBankReadRef);
			Inputs.AddDataReadReference("TableIndex", TableIndexReadRef);

			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace Metasound;

			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference("Out", FWaveTableWriteRef(OutTable));

			return Outputs;
		}

		void Execute()
		{
			using namespace WaveTable;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetasoundWaveTableGetNodeOperator::Execute);

			const FWaveTableBankAsset& WaveTableBankAsset = *WaveTableBankReadRef;

			FWaveTableBankAssetProxyPtr Proxy = WaveTableBankAsset.GetProxy();
			if (Proxy.IsValid())
			{
				const TArray<FWaveTable>& WaveTables = Proxy->GetWaveTables();
				if (!WaveTables.IsEmpty())
				{
					int32 NumSamples = WaveTables.Last().Num();
					FWaveTable& OutputWaveTable = *OutTable;
					OutputWaveTable.SetNum(NumSamples);
					OutputWaveTable.Zero();

					if (NumSamples > 0)
					{
						auto WrapIndex = [Max = WaveTables.Num()](float& InOutIndex)
						{
							InOutIndex = FMath::Abs(InOutIndex); // Avoids remainder offset flip at 0 crossing
							const int32 WrapIndex = FMath::TruncToInt32(InOutIndex) % Max;
							const float Remainder = FMath::Frac(InOutIndex);
							InOutIndex = WrapIndex + Remainder;
						};

						float NextTableIndex = *TableIndexReadRef;
						WrapIndex(NextTableIndex);

						if (WaveTables.Num() == 1)
						{
							OutputWaveTable = WaveTables.Last();
						}
						else
						{
							if (LastTableIndex < 0.0f)
							{
								LastTableIndex = NextTableIndex;
							}
							else
							{
								WrapIndex(LastTableIndex);
							}

							const int32 StartHighIndex = FMath::CeilToInt32(LastTableIndex) % WaveTables.Num();
							const int32 StartLowIndex = FMath::FloorToInt32(LastTableIndex);

							const float StartHighGain = LastTableIndex - StartLowIndex;
							const float StartLowGain = 1.0f - StartHighGain;

							const int32 EndHighIndex = FMath::CeilToInt32(NextTableIndex) % WaveTables.Num();
							const int32 EndLowIndex = FMath::FloorToInt32(NextTableIndex);

							const float EndHighGain = NextTableIndex - EndLowIndex;
							const float EndLowGain = 1.0f - EndHighGain;

							for (int32 TableIndex = 0; TableIndex < WaveTables.Num(); ++TableIndex)
							{
								float Gain = 0.0f;
								float GainDelta = 0.0f;

								if (TableIndex == StartLowIndex)
								{
									Gain = StartLowGain;
								}
								else if (TableIndex == StartHighIndex)
								{
									Gain = StartHighGain;
								}

								if (TableIndex == EndLowIndex)
								{
									GainDelta = (EndLowGain - Gain) / NumSamples;
								}
								else if (TableIndex == EndHighIndex)
								{
									GainDelta = (EndHighGain - Gain) / NumSamples;
								}

								if (Gain > 0.0f || GainDelta > 0.0f)
								{
									const FWaveTable& InputWaveTable = WaveTables[TableIndex];
									Audio::ArrayMixIn(InputWaveTable.GetView(), OutputWaveTable.GetView(), Gain);
								}
							}
						}

						LastTableIndex = NextTableIndex;
					}
					else
					{
						LastTableIndex = -1.0f;
					}
				}
				else
				{
					LastTableIndex = -1.0f;
				}
			}
			else
			{
				LastTableIndex = -1.0f;
			}
		}

	private:
		FWaveTableBankAssetReadRef WaveTableBankReadRef;
		FFloatReadRef TableIndexReadRef;

		float LastTableIndex = -1.0f;

		FWaveTableWriteRef OutTable;
	};

	class FMetasoundWaveTableGetNode : public FNodeFacade
	{
	public:
		FMetasoundWaveTableGetNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMetasoundWaveTableGetNodeOperator>())
		{
		}

		virtual ~FMetasoundWaveTableGetNode() = default;
	};

	METASOUND_REGISTER_NODE(FMetasoundWaveTableGetNode)
} // namespace Metasound

#undef LOCTEXT_NAMESPACE
