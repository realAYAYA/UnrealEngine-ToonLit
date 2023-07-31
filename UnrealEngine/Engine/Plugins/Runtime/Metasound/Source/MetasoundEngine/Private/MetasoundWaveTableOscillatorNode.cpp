// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrace.h"
#include "MetasoundTrigger.h"
#include "MetasoundWaveTable.h"
#include "WaveTableSampler.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"


namespace Metasound
{
	class FMetasoundWaveTableOscillatorNodeOperator : public TExecutableOperator<FMetasoundWaveTableOscillatorNodeOperator>
	{
	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace WaveTable;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>("Play", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableOscillatorNode_InputPlayDesc", "Plays the oscillator (block rate)") }),
					TInputDataVertex<FTrigger>("Stop", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableOscillatorNode_InputStopDesc", "Stops the oscillator (block rate)") }),
					TInputDataVertex<FWaveTable>("WaveTable", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableOscillatorNode_InputWaveTableDesc", "WaveTable") }),
					TInputDataVertex<FTrigger>("Sync", FDataVertexMetadata
					{
						LOCTEXT("MetasoundWaveTableOscillatorNode_InputSyncDesc", "Restarts playing the WaveTable on the trigger boundary (sample rate)"),
						LOCTEXT("MetasoundWaveTableOscillatorNode_InputSyncName", "Sync"),
						true /* bIsAdvancedDisplay */
					}),
					TInputDataVertex<float>("Freq", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableOscillatorNode_FreqDesc", "Frequency (number of times to sample one period of wavetable per second) [-20000Hz, 20000Hz]") }, 440.0f),
					TInputDataVertex<FAudioBuffer>("PhaseMod", FDataVertexMetadata
					{
						LOCTEXT("MetasoundWaveTableOscillatorNode_PhaseModDescription", "Modulation audio source for modulating oscillation phase of provided table. A value of 0 is no phase modulation and 1 a full table length (360 degrees) of phase shift."),
						LOCTEXT("MetasoundWaveTableOscillatorNode_PhaseMod", "Phase Modulator"),
						true /* bIsAdvancedDisplay */
					})
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>("Out", FDataVertexMetadata { LOCTEXT("MetasoundWaveTableOscillatorNode_Output", "Out") })
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
					{ EngineNodes::Namespace, "WaveTableOscillator", "" },
					1, // Major Version
					0, // Minor Version
					LOCTEXT("MetasoundWaveTableOscillatorNode_Name", "WaveTable Oscillator"),
					LOCTEXT("MetasoundWaveTableOscillatorNode_Description", "Reads through the given WaveTable at the provided frequency."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					GetDefaultInterface(),
					{ NodeCategories::Generators },
					{ METASOUND_LOCTEXT("WaveTableOscillatorSynthesisKeyword", "Synthesis")},
					{ }
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			using namespace WaveTable;

			const FInputVertexInterface& InputInterface = InParams.Node.GetVertexInterface().GetInputInterface();
			const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;

			FWaveTableReadRef InWaveTableReadRef = InputCollection.GetDataReadReferenceOrConstruct<FWaveTable>("WaveTable");
			FTriggerReadRef InPlayReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(InputInterface, "Play", InParams.OperatorSettings);
			FTriggerReadRef InStopReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(InputInterface, "Stop", InParams.OperatorSettings);
			FTriggerReadRef InSyncReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FTrigger>(InputInterface, "Sync", InParams.OperatorSettings);
			FFloatReadRef InFreqReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, "Freq", InParams.OperatorSettings);

			TOptional<FAudioBufferReadRef> InPhaseModReadRef;
			if (InputCollection.ContainsDataReadReference<FAudioBuffer>("PhaseMod"))
			{
				InPhaseModReadRef = InputCollection.GetDataReadReference<FAudioBuffer>("PhaseMod");
			}

			return MakeUnique<FMetasoundWaveTableOscillatorNodeOperator>(InParams, InWaveTableReadRef, InPlayReadRef, InStopReadRef, InSyncReadRef, InFreqReadRef, MoveTemp(InPhaseModReadRef));
		}

		FMetasoundWaveTableOscillatorNodeOperator(
			const FCreateOperatorParams& InParams,
			const FWaveTableReadRef& InWaveTableReadRef,
			const FTriggerReadRef& InPlayReadRef,
			const FTriggerReadRef& InStopReadRef,
			const FTriggerReadRef& InSyncReadRef,
			const FFloatReadRef& InFreqReadRef,
			TOptional<FAudioBufferReadRef>&& InPhaseModReadRef
		)
			: WaveTableReadRef(InWaveTableReadRef)
			, PlayReadRef(InPlayReadRef)
			, StopReadRef(InStopReadRef)
			, SyncReadRef(InSyncReadRef)
			, FreqReadRef(InFreqReadRef)
			, PhaseModReadRef(MoveTemp(InPhaseModReadRef))
			, OutBufferWriteRef(TDataWriteReferenceFactory<FAudioBuffer>::CreateAny(InParams.OperatorSettings))
		{
			const float BlockRate = InParams.OperatorSettings.GetActualBlockRate();
			if (BlockRate > 0.0f)
			{
				BlockPeriod = 1.0f / BlockRate;
			}
		}

		virtual ~FMetasoundWaveTableOscillatorNodeOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			FDataReferenceCollection Inputs;

			Inputs.AddDataReadReference("WaveTable", WaveTableReadRef);
			Inputs.AddDataReadReference("Sync", SyncReadRef);
			Inputs.AddDataReadReference("Freq", FreqReadRef);

			if (PhaseModReadRef.IsSet())
			{
				Inputs.AddDataReadReference("PhaseMod", *PhaseModReadRef);
			}

			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference("Out", TDataReadReference<FAudioBuffer>(OutBufferWriteRef));

			return Outputs;
		}

		void Execute()
		{
			using namespace WaveTable;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(FMetasoundWaveTableOscillatorNodeOperator::Execute);

			FAudioBuffer& OutBuffer = *OutBufferWriteRef;
			OutBuffer.Zero();

			auto GetLastIndex = [](const FTriggerReadRef& Trigger)
			{
				int32 LastIndex = -1;
				Trigger->ExecuteBlock([](int32, int32) {}, [&LastIndex](int32 StartFrame, int32 EndFrame)
				{
					LastIndex = FMath::Max(LastIndex, StartFrame);
				});
				return LastIndex;
			};

			const int32 LastPlayIndex = GetLastIndex(PlayReadRef);
			const int32 LastStopIndex = GetLastIndex(StopReadRef);
			if (LastPlayIndex >= 0 || LastStopIndex >= 0)
			{
				bPlaying = LastPlayIndex > LastStopIndex;
			}

			if (bPlaying)
			{
				const FTrigger& SyncTrigger = *SyncReadRef;
				TArrayView<float> SyncBufferView;
				if (SyncTrigger.IsTriggered())
				{
					SyncBuffer.SetNum(OutBuffer.Num());
					FMemory::Memset(SyncBuffer.GetData(), 0, sizeof(float) * SyncBuffer.Num());
					SyncTrigger.ExecuteBlock(
						[](int32 StartFrame, int32 EndFrame) {},
						[this](int32 StartFrame, int32 EndFrame)
						{
							SyncBuffer[StartFrame] = 1.0f;
						}
					);
					SyncBufferView = SyncBuffer;
				}

				TArrayView<const float> PhaseMod;
				if (PhaseModReadRef.IsSet())
				{
					const FAudioBuffer& Buffer = *(*PhaseModReadRef);
					PhaseMod = { Buffer.GetData(), Buffer.Num() };
				}

				// Limit wrap operations running off toward infinity while allowing sampler to play in reverse
				Sampler.SetFreq(FMath::Clamp(*FreqReadRef, -20000.f, 20000.f) * BlockPeriod);

				const FWaveTable& InputTable = *WaveTableReadRef;
				Sampler.Process(InputTable.GetView(), { }, PhaseMod, SyncBufferView, OutBuffer);
			}
		}

	private:
		float BlockPeriod = 0.0f;
		bool bPlaying = false;

		FWaveTableReadRef WaveTableReadRef;
		FTriggerReadRef PlayReadRef;
		FTriggerReadRef StopReadRef;
		FTriggerReadRef SyncReadRef;
		FFloatReadRef FreqReadRef;

		Audio::FAlignedFloatBuffer SyncBuffer;
		TOptional<FAudioBufferReadRef> PhaseModReadRef;

		WaveTable::FWaveTableSampler Sampler;

		TDataWriteReference<FAudioBuffer> OutBufferWriteRef;
	};

	class FMetasoundWaveTableOscillatorNode : public FNodeFacade
	{
	public:
		FMetasoundWaveTableOscillatorNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMetasoundWaveTableOscillatorNodeOperator>())
		{
		}

		virtual ~FMetasoundWaveTableOscillatorNode() = default;
	};

	METASOUND_REGISTER_NODE(FMetasoundWaveTableOscillatorNode)
} // namespace Metasound

#undef LOCTEXT_NAMESPACE // MetasoundStandardNodes
