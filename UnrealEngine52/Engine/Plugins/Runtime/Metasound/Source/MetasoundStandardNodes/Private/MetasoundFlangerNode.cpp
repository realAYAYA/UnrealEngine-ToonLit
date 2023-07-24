// Copyright Epic Games, Inc. All Rights Reserved.

#include "Internationalization/Text.h"
#include "MetasoundFacade.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundStandardNodesCategories.h"
#include "DSP/BufferVectorOperations.h"
#include "DSP/Flanger.h"
#include "MetasoundParamHelper.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_FlangerNode"

namespace Metasound
{
	namespace FlangerVertexNames
	{
		METASOUND_PARAM(InputAudio, "In Audio", "The audio input.");
		METASOUND_PARAM(InputModulationRate, "Modulation Rate", "The LFO frequency (rate) that varies the delay time, in Hz. Clamped at blockrate.");
		METASOUND_PARAM(InputModulationDepth, "Modulation Depth", "The LFO amplitude (strength) that scales the delay time.");
		METASOUND_PARAM(InputCenterDelay, "Center Delay", "The center delay amount (in milliseconds).");
		METASOUND_PARAM(InputMixLevel, "Mix Level", "Balance between original and delayed signal (Should be between 0 and 1.0; 0.5 is equal amounts of each and > 0.5 is more delayed signal than non-delayed signal).");
		
		METASOUND_PARAM(OutputAudio, "Out Audio", "Output audio with flanger effect applied.");
	}

	class FFlangerOperator : public TExecutableOperator<FFlangerOperator>
	{
	public:

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto InitNodeInfo = []() -> FNodeClassMetadata
			{
				const FName OperatorName = TEXT("Flanger");
				const FText NodeDisplayName = METASOUND_LOCTEXT("Metasound_FlangerNodeDisplayName", "Flanger");
				const FText NodeDescription = METASOUND_LOCTEXT("Metasound_FlangerNodeDescription", "Applies a flanger effect to input audio.");

				FNodeClassMetadata Info;
				Info.ClassName = { StandardNodes::Namespace, OperatorName, TEXT("") };
				Info.MajorVersion = 1;
				Info.MinorVersion = 0;
				Info.DisplayName = NodeDisplayName;
				Info.Description = NodeDescription;
				Info.Author = PluginAuthor;
				Info.PromptIfMissing = PluginNodeMissingPrompt;
				Info.DefaultInterface = GetVertexInterface();

				return Info;
			};

			static const FNodeClassMetadata Info = InitNodeInfo();
			return Info;
		}

		static const FVertexInterface& GetVertexInterface()
		{
			using namespace FlangerVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputAudio)), 
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputModulationRate), 0.5f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputModulationDepth), 0.5f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputCenterDelay), 0.5f),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputMixLevel), 0.5f)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
				)
			);

			return Interface;
		}

		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
		{
			using namespace FlangerVertexNames;

			const FDataReferenceCollection& InputCollection = InParams.InputDataReferences;
			const FInputVertexInterface& InputInterface = GetVertexInterface().GetInputInterface();

			FAudioBufferReadRef AudioInput = InputCollection.GetDataReadReferenceOrConstruct<FAudioBuffer>(METASOUND_GET_PARAM_NAME(InputAudio), InParams.OperatorSettings);
			FFloatReadRef ModulationRate = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputModulationRate), InParams.OperatorSettings);
			FFloatReadRef ModulationDepth = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputModulationDepth), InParams.OperatorSettings);
			FFloatReadRef CenterDelay = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputCenterDelay), InParams.OperatorSettings);
			FFloatReadRef MixLevel = InParams.InputDataReferences.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, METASOUND_GET_PARAM_NAME(InputMixLevel), InParams.OperatorSettings);

			return MakeUnique<FFlangerOperator>(InParams.OperatorSettings, AudioInput, ModulationRate, ModulationDepth, CenterDelay, MixLevel);
		}

		FFlangerOperator(const FOperatorSettings& InSettings,
			const FAudioBufferReadRef& InAudioInput,
			const FFloatReadRef& InputModulationRate,
			const FFloatReadRef& InputModulationDepth,
			const FFloatReadRef& InputCenterDelay,
			const FFloatReadRef& InMixLevel)

			: AudioIn(InAudioInput)
			, ModulationRate(InputModulationRate)
			, ModulationDepth(InputModulationDepth)
			, CenterDelay(InputCenterDelay)
			, MixLevel(InMixLevel)
			, AudioOut(FAudioBufferWriteRef::CreateNew(InSettings))
			, Flanger()
		{
			Flanger.Init(InSettings.GetSampleRate());
		}

		FDataReferenceCollection GetInputs() const
		{
			using namespace FlangerVertexNames;

			FDataReferenceCollection InputDataReferences;
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputAudio), AudioIn);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputModulationRate), ModulationRate);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputModulationDepth), ModulationDepth);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputCenterDelay), CenterDelay);
			InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(InputMixLevel), MixLevel);

			return InputDataReferences;
		}

		virtual FDataReferenceCollection GetOutputs() const
		{
			using namespace FlangerVertexNames;

			FDataReferenceCollection OutputDataReferences;
			OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOut);
			return OutputDataReferences;
		}

		void Execute()
		{
			// Update flanger parameters if necessary 
			Flanger.SetModulationRate(*ModulationRate);
			Flanger.SetCenterDelay(*CenterDelay);
			Flanger.SetModulationDepth(*ModulationDepth);
			Flanger.SetMixLevel(*MixLevel);

			const float* InputAudio = AudioIn->GetData();
			float* OutputAudio = AudioOut->GetData();
			int32 NumFrames = AudioIn->Num();

			// Copy input audio into aligned buffer
			AlignedAudioIn.Reset();
			AlignedAudioIn.Append(InputAudio, NumFrames);

			AlignedAudioOut.Reset();
			AlignedAudioOut.AddUninitialized(NumFrames);

			Flanger.ProcessAudio(AlignedAudioIn, NumFrames, AlignedAudioOut);
			FMemory::Memcpy(OutputAudio, AlignedAudioOut.GetData(), sizeof(float) * NumFrames);
		}

	private:
		// The input audio buffer
		FAudioBufferReadRef AudioIn;

		// LFO parameters
		FFloatReadRef ModulationRate;
		FFloatReadRef ModulationDepth;
		FFloatReadRef CenterDelay;

		// Balance between original and delayed signal 
		// (Should be between 0 and 1.0; 
		// 0.5 is equal amounts of each and 
		// > 0.5 is more delayed signal than non-delayed signal)
		FFloatReadRef MixLevel;

		// Audio output
		FAudioBufferWriteRef AudioOut;

		// Flanger DSP object
		Audio::FFlanger Flanger;

		// Input/output aligned buffers
		Audio::FAlignedFloatBuffer AlignedAudioIn;
		Audio::FAlignedFloatBuffer AlignedAudioOut;
	};

	class METASOUNDSTANDARDNODES_API FFlangerNode : public FNodeFacade
	{
	public:
		/**
		 * Constructor used by the Metasound Frontend.
		 */
		FFlangerNode(const FNodeInitData& InitData)
			: FNodeFacade(InitData.InstanceName, InitData.InstanceID, TFacadeOperatorClass<FFlangerOperator>())
		{
		}
	};

	METASOUND_REGISTER_NODE(FFlangerNode)
}

#undef LOCTEXT_NAMESPACE
