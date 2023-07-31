// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundTrigger.h"
#include "MetasoundTime.h"
#include "MetasoundAudioBuffer.h"
#include "Internationalization/Text.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Dsp.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_ADEnvelopeNode"

namespace Metasound
{
	namespace ADEnvelopeVertexNames
	{
		METASOUND_PARAM(InputTrigger, "Trigger", "Trigger to start the attack phase of the envelope generator.");
		METASOUND_PARAM(InputAttackTime, "Attack Time", "Attack time of the envelope.");
		METASOUND_PARAM(InputDecayTime, "Decay Time", "Decay time of the envelope.");
		METASOUND_PARAM(InputAttackCurve, "Attack Curve", "The exponential curve factor of the attack. 1.0 = linear growth, < 1.0 logorithmic growth, > 1.0 exponential growth.");
		METASOUND_PARAM(InputDecayCurve, "Decay Curve", "The exponential curve factor of the decay. 1.0 = linear decay, < 1.0 exponential decay, > 1.0 logarithmic decay.");
		METASOUND_PARAM(InputLooping, "Looping", "Set to true to enable looping of the envelope. This will allow the envelope to be an LFO or wave generator.");

		METASOUND_PARAM(OutputOnTrigger, "On Trigger", "Triggers when the envelope is triggered.");
		METASOUND_PARAM(OutputOnDone, "On Done", "Triggers when the envelope finishes or loops back if looping is enabled.");
		METASOUND_PARAM(OutputEnvelopeValue, "Out Envelope", "The output value of the envelope.");
	}

	namespace ADEnvelopeNodePrivate
	{
		struct FEnvState
		{
			// Where the envelope is. If INDEX_NONE, then the envelope is not triggered
			int32 CurrentSampleIndex = INDEX_NONE;

			// Number of samples for attack
			int32 AttackSampleCount = 1;

			// Number of samples for Decay
			int32 DecaySampleCount = 1;

			// Curve factors for attack/Decay
			float AttackCurveFactor = 0.0f;
			float DecayCurveFactor = 0.0f;

			Audio::FExponentialEase EnvEase;

			// Where the envelope value was when it was triggered
			float StartingEnvelopeValue = 0.0f;
			float CurrentEnvelopeValue = 0.0f;

			bool bLooping = false;
		};

		template<typename ValueType>
		struct TADEnvelope
		{
		};

		template<>
		struct TADEnvelope<float>
		{
			static void GetNextEnvelopeOutput(FEnvState& InState, int32 StartFrame, int32 EndFrame, TArray<int32>& OutFinishedFrames, float& OutEnvelopeValue)
			{
				// Don't need to do anything if we're not generating the envelope at the top of the block since this is a block-rate envelope
				if (StartFrame > 0 || InState.CurrentSampleIndex == INDEX_NONE)
				{
					OutEnvelopeValue = 0.0f;
					return;
				}

				// We are in attack
				if (InState.CurrentSampleIndex < InState.AttackSampleCount)
				{
					if (InState.AttackSampleCount > 1)
					{
						float AttackFraction = (float)InState.CurrentSampleIndex++ / InState.AttackSampleCount;
						OutEnvelopeValue = InState.StartingEnvelopeValue + (1.0f - InState.StartingEnvelopeValue) * FMath::Pow(AttackFraction, InState.AttackCurveFactor);
					}
					else
					{
						// Attack is effectively 0, skip Attack fade-in
						InState.CurrentSampleIndex++;
						OutEnvelopeValue = 1.f;
					}
				}
				else
				{
					int32 TotalEnvSampleCount = (InState.AttackSampleCount + InState.DecaySampleCount);
					
					// We are in Decay
					if (InState.CurrentSampleIndex < TotalEnvSampleCount)
					{
						int32 SampleCountInDecayState = InState.CurrentSampleIndex++ - InState.AttackSampleCount;
						float DecayFraction = (float)SampleCountInDecayState / InState.DecaySampleCount;
						OutEnvelopeValue = 1.0f - FMath::Pow(DecayFraction, InState.DecayCurveFactor);
					}
					// We are looping so reset the sample index
					else if (InState.bLooping)
					{
						InState.CurrentSampleIndex = 0;
						OutFinishedFrames.Add(0);
					}
					else
					{
						// Envelope is done
						InState.CurrentSampleIndex = INDEX_NONE;
						OutEnvelopeValue = 0.0f;
						OutFinishedFrames.Add(0);
					}
				}
			}

			static bool IsAudio() { return false; }
		};

		template<>
		struct TADEnvelope<FAudioBuffer>
		{
			static void GetNextEnvelopeOutput(FEnvState& InState, int32 StartFrame, int32 EndFrame, TArray<int32>& OutFinishedFrames, FAudioBuffer& OutEnvelopeValue)
			{
				// If we are not active zero the buffer and early exit
				if (InState.CurrentSampleIndex == INDEX_NONE)
				{
					OutEnvelopeValue.Zero();
					return;
				}

				float* OutEnvPtr = OutEnvelopeValue.GetData();
				for (int32 i = StartFrame; i < EndFrame; ++i)
				{
					// We are in attack
					if (InState.CurrentSampleIndex <= InState.AttackSampleCount)
					{
						float AttackFraction = (float)InState.CurrentSampleIndex++ / InState.AttackSampleCount;
						float EnvValue = FMath::Pow(AttackFraction, InState.AttackCurveFactor);
						float TargetEnvelopeValue = InState.StartingEnvelopeValue + (1.0f - InState.StartingEnvelopeValue) * EnvValue;
						InState.EnvEase.SetValue(TargetEnvelopeValue);
						InState.CurrentEnvelopeValue = InState.EnvEase.GetNextValue();
						OutEnvPtr[i] = InState.CurrentEnvelopeValue;
					}
					else 
					{
						int32 TotalEnvSampleCount = (InState.AttackSampleCount + InState.DecaySampleCount);

						// We are in Decay
						if (InState.CurrentSampleIndex < TotalEnvSampleCount)
						{
							int32 SampleCountInDecayState = InState.CurrentSampleIndex++ - InState.AttackSampleCount;
							float DecayFracton = (float)SampleCountInDecayState / InState.DecaySampleCount;
							float TargetEnvelopeValue = 1.0f - FMath::Pow(DecayFracton, InState.DecayCurveFactor);
							InState.EnvEase.SetValue(TargetEnvelopeValue);
							InState.CurrentEnvelopeValue = InState.EnvEase.GetNextValue();
							OutEnvPtr[i] = InState.CurrentEnvelopeValue;
						}
						// We are looping so reset the sample index
						else if (InState.bLooping)
						{
							InState.StartingEnvelopeValue = 0.0f;
							InState.CurrentEnvelopeValue = 0.0f;
							InState.CurrentSampleIndex = 0;
							OutFinishedFrames.Add(i);
						}
						else
						{
							InState.CurrentEnvelopeValue = InState.EnvEase.GetNextValue();
							OutEnvPtr[i] = InState.CurrentEnvelopeValue;

							// Envelope is done
							if (InState.EnvEase.IsDone())
							{
								// Zero out the rest of the envelope
								int32 NumSamplesLeft = EndFrame - i - 1;
								if (NumSamplesLeft > 0)
								{
									FMemory::Memzero(&OutEnvPtr[i + 1], sizeof(float) * NumSamplesLeft);
								}
								InState.CurrentSampleIndex = INDEX_NONE;
								OutFinishedFrames.Add(i);
								break;
							}
						}
					}
				}
			}

			static bool IsAudio() { return true; }
		};
	}

	template<typename ValueType>
	class TADEnvelopeNodeOperator : public TExecutableOperator<TADEnvelopeNodeOperator<ValueType>>
	{
	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace ADEnvelopeVertexNames;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputTrigger)),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAttackTime), 0.01f),
					TInputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDecayTime), 1.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAttackCurve), 1.0f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputDecayCurve), 1.0f),
					TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputLooping), false)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnTrigger)),
					TOutputDataVertex<FTrigger>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputOnDone)),
					TOutputDataVertex<ValueType>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputEnvelopeValue))
				)
			);

			return DefaultInterface;
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				const FName DataTypeName = GetMetasoundDataTypeName<ValueType>();
				const FName OperatorName = "AD Envelope";
				const FText NodeDisplayName = METASOUND_LOCTEXT_FORMAT("ADEnvelopeDisplayNamePattern", "AD Envelope ({0})", GetMetasoundDataTypeDisplayText<ValueType>());
				const FText NodeDescription = METASOUND_LOCTEXT("ADEnevelopeDesc", "Generates an attack-decay envelope value output when triggered.");
				const FVertexInterface NodeInterface = GetDefaultInterface();

				const FNodeClassMetadata Metadata
				{
					FNodeClassName { "AD Envelope", OperatorName, DataTypeName },
					1, // Major Version
					0, // Minor Version
					NodeDisplayName,
					NodeDescription,
					PluginAuthor,
					PluginNodeMissingPrompt,
					NodeInterface,
					{ NodeCategories::Envelopes },
					{ },
					FNodeDisplayStyle()
				};

				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, TArray<TUniquePtr<IOperatorBuildError>>& OutErrors)
		{
			using namespace ADEnvelopeVertexNames;

			const FInputVertexInterface& InputInterface = GetDefaultInterface().GetInputInterface();

			FTriggerReadRef TriggerIn = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<FTrigger>(METASOUND_GET_PARAM_NAME(InputTrigger), InParams.OperatorSettings);
			FTimeReadRef AttackTime = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputAttackTime), InParams.OperatorSettings);
			FTimeReadRef DecayTime = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<FTime>(InputInterface, METASOUND_GET_PARAM_NAME(InputDecayTime), InParams.OperatorSettings);
			FFloatReadRef AttackCurveFactor = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputAttackCurve), InParams.OperatorSettings);
			FFloatReadRef DecayCurveFactor = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputDecayCurve), InParams.OperatorSettings);
			FBoolReadRef bLooping = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<bool>(InputInterface, METASOUND_GET_PARAM_NAME(InputLooping), InParams.OperatorSettings);

			return MakeUnique<TADEnvelopeNodeOperator<ValueType>>(InParams.OperatorSettings, TriggerIn, AttackTime, DecayTime, AttackCurveFactor, DecayCurveFactor, bLooping);
		}

		TADEnvelopeNodeOperator(const FOperatorSettings& InSettings,
			const FTriggerReadRef& InTriggerIn,
			const FTimeReadRef& InAttackTime,
			const FTimeReadRef& InDecayTime,
			const FFloatReadRef& InAttackCurveFactor,
			const FFloatReadRef& InDecayeCurveFactor,
			const FBoolReadRef& bInLooping)
			: TriggerAttackIn(InTriggerIn)
			, AttackTime(InAttackTime)
			, DecayTime(InDecayTime)
			, AttackCurveFactor(InAttackCurveFactor)
			, DecayCurveFactor(InDecayeCurveFactor)
			, bLooping(bInLooping)
			, OnAttackTrigger(TDataWriteReferenceFactory<FTrigger>::CreateAny(InSettings))
			, OnDone(TDataWriteReferenceFactory<FTrigger>::CreateAny(InSettings))
			, OutputEnvelope(TDataWriteReferenceFactory<ValueType>::CreateAny(InSettings))
		{
			NumFramesPerBlock = InSettings.GetNumFramesPerBlock();

			EnvState.EnvEase.SetEaseFactor(0.01f);

			if (ADEnvelopeNodePrivate::TADEnvelope<ValueType>::IsAudio())
			{
				SampleRate = InSettings.GetSampleRate();
			}
			else
			{
				SampleRate = InSettings.GetActualBlockRate();
			}
		}

		virtual ~TADEnvelopeNodeOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override
		{
			using namespace ADEnvelopeVertexNames;

			FDataReferenceCollection Inputs;
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputTrigger), TriggerAttackIn);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAttackTime), AttackTime);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDecayTime), DecayTime);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAttackCurve), AttackCurveFactor);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputDecayCurve), DecayCurveFactor);
			Inputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputLooping), bLooping);

			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace ADEnvelopeVertexNames;

			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputOnTrigger), OnAttackTrigger);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputOnDone), OnDone);
			Outputs.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputEnvelopeValue), OutputEnvelope);

			return Outputs;
		}

		void UpdateParams()
		{
			float AttackTimeSeconds = AttackTime->GetSeconds();
			float DecayTimeSeconds = DecayTime->GetSeconds();
			EnvState.AttackSampleCount = FMath::Max(1, SampleRate * AttackTimeSeconds);
			EnvState.DecaySampleCount = FMath::Max(1, SampleRate * DecayTimeSeconds);
			EnvState.AttackCurveFactor = FMath::Max(KINDA_SMALL_NUMBER, *AttackCurveFactor);
			EnvState.DecayCurveFactor = FMath::Max(KINDA_SMALL_NUMBER, *DecayCurveFactor);
			EnvState.bLooping = *bLooping;
		}


		void Execute()
		{
			using namespace ADEnvelopeNodePrivate;

			OnAttackTrigger->AdvanceBlock();
			OnDone->AdvanceBlock();

			// check for any updates to input params
			UpdateParams();

			TriggerAttackIn->ExecuteBlock(
				// OnPreTrigger
				[&](int32 StartFrame, int32 EndFrame)
				{
					TArray<int32> FinishedFrames;
					TADEnvelope<ValueType>::GetNextEnvelopeOutput(EnvState, StartFrame, EndFrame, FinishedFrames, *OutputEnvelope);

					for (int32 FrameFinished : FinishedFrames)
					{
						OnDone->TriggerFrame(FrameFinished);
					}
				},
				// OnTrigger
					[&](int32 StartFrame, int32 EndFrame)
				{
					// Get latest params
					UpdateParams();

					// Set the sample index to the top of the envelope
					EnvState.CurrentSampleIndex = 0;
					EnvState.StartingEnvelopeValue = EnvState.CurrentEnvelopeValue;

					// Generate the output (this will no-op if we're block rate)
					TArray<int32> FinishedFrames;
					TADEnvelope<ValueType>::GetNextEnvelopeOutput(EnvState, StartFrame, EndFrame, FinishedFrames, *OutputEnvelope);
					for (int32 FrameFinished : FinishedFrames)
					{
						OnDone->TriggerFrame(FrameFinished);
					}

					// Forward the trigger
					OnAttackTrigger->TriggerFrame(StartFrame);
				}
			);
		}

	private:

		FTriggerReadRef TriggerAttackIn;
		FTimeReadRef AttackTime;
		FTimeReadRef DecayTime;
		FFloatReadRef AttackCurveFactor;
		FFloatReadRef DecayCurveFactor;
		FBoolReadRef bLooping;

		FTriggerWriteRef OnAttackTrigger;
		FTriggerWriteRef OnDone;
		TDataWriteReference<ValueType> OutputEnvelope;

		// This will either be the block rate or sample rate depending on if this is block-rate or audio-rate envelope
		float SampleRate = 0.0f;
		int32 NumFramesPerBlock = 0;

		ADEnvelopeNodePrivate::FEnvState EnvState;
	};

	/** TADEnvelopeNode
	 *
	 *  Creates an Attack/Decay envelope node.
	 */
	template<typename ValueType>
	class METASOUNDSTANDARDNODES_API TADEnvelopeNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		TADEnvelopeNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<TADEnvelopeNodeOperator<ValueType>>())
		{}

		virtual ~TADEnvelopeNode() = default;
	};

	using FADEnvelopeNodeFloat = TADEnvelopeNode<float>;
	METASOUND_REGISTER_NODE(FADEnvelopeNodeFloat)

	using FADEnvelopeAudioBuffer = TADEnvelopeNode<FAudioBuffer>;
	METASOUND_REGISTER_NODE(FADEnvelopeAudioBuffer)
}

#undef LOCTEXT_NAMESPACE
