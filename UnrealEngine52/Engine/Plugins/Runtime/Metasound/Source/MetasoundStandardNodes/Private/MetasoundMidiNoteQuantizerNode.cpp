// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "DSP/MidiNoteQuantizer.h"
#include "Internationalization/Text.h"

#include "MetasoundFacade.h"
#include "MetasoundPrimitives.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundParamHelper.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundDataTypeRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_MidiNoteQuantizerNode"

namespace Metasound
{
	// forward declarations
	// ...
		
#pragma region Parameter Names
	namespace MidiNoteQuantizerParameterNames
	{
		// inputs
		METASOUND_PARAM(ParamNoteIn, "Note In", "Midi Note to quantize");
		METASOUND_PARAM(ParamRootNote, "Root Note", "Midi note to treat as the Root (0 = C, 1 = D, etc.).  Octave does not matter. Values < 0 will clamp to 0");
		METASOUND_PARAM(ParamScaleDegrees, "Scale Degrees"
			, "Set of notes in ascending order, represeting half steps starting at 0.0f meaning the Root Note. The highest value MUST be the same as 0.0 one octave higher: 12.0 in most cases, but it doesn't have to be (i.e. could be 24.0 if you want to define a 2 octave range)");

		// outputs
		METASOUND_PARAM(ParamNoteOutput, "Note Out", "Quantized Note");

	} // namespace MidiNoteQuantizerParameterNames

	using namespace MidiNoteQuantizerParameterNames;

#pragma endregion // Parameter Names


#pragma region Operator Declaration
	class FMidiNoteQuantizerOperator: public TExecutableOperator < FMidiNoteQuantizerOperator >
	{
	public:
		using FArrayScaleDegreeReadRef = TDataReadReference<TArray<float>>;
		using ScaleDegreeArrayType = TArray<float>;

		// ctor
		FMidiNoteQuantizerOperator(
			  const FOperatorSettings& InSettings
			, const FFloatReadRef& InMidiNoteIn
			, const FFloatReadRef& InRootNote
			, const FArrayScaleDegreeReadRef& InScale
		);

		// node interface
		static const FNodeClassMetadata& GetNodeInfo();
		static FVertexInterface DeclareVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Execute();

	private: // members
		// input pins
		FFloatReadRef MidiNoteIn;
		FFloatReadRef RootNote;
		FArrayScaleDegreeReadRef Scale;

		// output pins
		FFloatWriteRef MidiNoteOut;

		// cached values
		float PreviousNoteIn = TNumericLimits<float>::Lowest();
		float PreviousRoot = -1.0f;
		TArray<float> PreviousScale;
		float PreviousNoteOut = 0.0f;

		// other
		FOperatorSettings Settings;

	}; // class FMidiNoteQuantizerOperator

#pragma endregion // Operator Declaration


#pragma region Operator Implementation

	// ctor
	FMidiNoteQuantizerOperator::FMidiNoteQuantizerOperator(
		  const FOperatorSettings& InSettings
		, const FFloatReadRef& InMidiNoteIn
		, const FFloatReadRef& InRootNote
		, const FArrayScaleDegreeReadRef& InScale
		)
		: MidiNoteIn(InMidiNoteIn)
		, RootNote(InRootNote)
		, Scale(InScale)
		, MidiNoteOut(FFloatWriteRef::CreateNew())
		, Settings(InSettings)
	{
	}


	const FNodeClassMetadata& FMidiNoteQuantizerOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("MIDI Note Quantizer"), StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_MIDI_Note_Quantizer_NodeDisplayName", "MIDI Note Quantizer");
			Info.Description = METASOUND_LOCTEXT("MidiNoteQuantizer_NodeDescription", "Quantizes a MIDI note to the nearset note that matches provided criteria");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			Info.Keywords.Add(METASOUND_LOCTEXT("QuantizerPitchKeyword", "Pitch"));

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}


	FVertexInterface FMidiNoteQuantizerOperator::DeclareVertexInterface()
	{
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamNoteIn), 60.0f),
				TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamRootNote), 0.0f),
				TInputDataVertex<ScaleDegreeArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamScaleDegrees))
			),
			FOutputVertexInterface(
				TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamNoteOutput))
				)
			);

		return Interface;
	}


	TUniquePtr<IOperator> FMidiNoteQuantizerOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		const FDataReferenceCollection& InputDataRefs = InParams.InputDataReferences;

		// inputs
		FFloatReadRef MidiNoteIn = InputDataRefs.GetDataReadReferenceOrConstruct<float>(METASOUND_GET_PARAM_NAME(ParamNoteIn));
		FFloatReadRef RootNoteIn = InputDataRefs.GetDataReadReferenceOrConstruct<float>(METASOUND_GET_PARAM_NAME(ParamRootNote));
		FArrayScaleDegreeReadRef InScaleArray = InParams.InputDataReferences.GetDataReadReferenceOrConstruct<ScaleDegreeArrayType>(METASOUND_GET_PARAM_NAME(ParamScaleDegrees));

		return MakeUnique <FMidiNoteQuantizerOperator>(
			  InParams.OperatorSettings
			, MidiNoteIn
			, RootNoteIn
			, InScaleArray
			);
	}

	FDataReferenceCollection FMidiNoteQuantizerOperator::GetInputs() const
	{
		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamNoteIn), FFloatReadRef(MidiNoteIn));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamRootNote), FFloatReadRef(RootNote));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamScaleDegrees), FArrayScaleDegreeReadRef(Scale));

		return InputDataReferences;
	}

	FDataReferenceCollection FMidiNoteQuantizerOperator::GetOutputs() const
	{
		// expose read access to our output buffer for other processors in the graph
		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamNoteOutput), FFloatReadRef(MidiNoteOut));

		return OutputDataReferences;
	}

	void FMidiNoteQuantizerOperator::Execute()
	{
		if ((*Scale).IsEmpty())
		{
			*MidiNoteOut = *MidiNoteIn;
			return;
		}

		// calculate new output and cache values if needed
		const float CurrentRoot = FMath::Max(*RootNote, 0.0f);
		if (!FMath::IsNearlyEqual(PreviousNoteIn, *MidiNoteIn)
			|| !FMath::IsNearlyEqual(PreviousRoot, CurrentRoot)
			|| PreviousScale !=  *Scale
			)
		{
			// cache values
			PreviousNoteIn = *MidiNoteIn;
			PreviousRoot = *RootNote;
			PreviousScale = *Scale;

			PreviousScale.Sort();
			PreviousNoteOut = Audio::FMidiNoteQuantizer::QuantizeMidiNote(PreviousNoteIn, PreviousRoot, PreviousScale);
		}

		// set the output value
		*MidiNoteOut = PreviousNoteOut;
	}

#pragma endregion // Operator Implementation


#pragma region Node Declaration
	class METASOUNDSTANDARDNODES_API FMidiNoteQuantizerNode: public FNodeFacade
	{
	public:
		// public node api needs to define two conversion constructors:
		// (1: from FString)
		FMidiNoteQuantizerNode(const FVertexName& InInstanceName, const FGuid & InInstanceID)
			: FNodeFacade(InInstanceName, InInstanceID, TFacadeOperatorClass <FMidiNoteQuantizerOperator>())
		{ }

		// (2: From an NodeInitData struct)
		FMidiNoteQuantizerNode(const FNodeInitData & InInitData)
			: FMidiNoteQuantizerNode(InInitData.InstanceName, InInitData.InstanceID)
		{ }

	};
#pragma endregion // Node Declaration


#pragma region Node Registration
	METASOUND_REGISTER_NODE(FMidiNoteQuantizerNode);
#pragma endregion // Node Registration

} // namespace Metasound

#undef LOCTEXT_NAMESPACE //MetasoundBasicFilterNodes
