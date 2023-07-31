// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "DSP/MidiNoteQuantizer.h"
#include "Internationalization/Text.h"

#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundFacade.h"
#include "MetasoundPrimitives.h"
#include "MetasoundScaleEnums.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundParamHelper.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundStandardNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundDataTypeRegistrationMacro.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes_MidiScaleToArray"

namespace Metasound
{
#pragma region Parameter Names
	namespace MusicalScaleToNoteArrayParameterNames
	{
		// inputs
		METASOUND_PARAM(ParamScaleDegreesPreset, "Scale Degrees", "Select scale preset");
		METASOUND_PARAM(ParamChordTonesOnly, "Chord Tones Only", "If true, will only return a subset of the scale represeting chord tones. (i.e. scale degrees 1,3,5,7)");

		// outputs
		METASOUND_PARAM(ParamNoteArrayOutput, "Scale Array Out", "Array represeting the scale as half steps above the root. The set is inclusive at both ends: (starting at 0.0f and ending with 12.0f)");

	} // namespace MusicalScaleToNoteArrayParameterNames

	using namespace MusicalScaleToNoteArrayParameterNames;

#pragma endregion // Parameter Names


#pragma region Operator Declaration
	class FMusicalScaleToNoteArrayOperator: public TExecutableOperator < FMusicalScaleToNoteArrayOperator >
	{
	public:
		using FArrayScaleDegreeWriteRef = TDataWriteReference<TArray<float>>;
		using ScaleDegreeArrayType = TArray<float>;

		// ctor
		FMusicalScaleToNoteArrayOperator(
			  const FOperatorSettings& InSettings
			, const FEnumMusicalScaleReadRef& InScale
			, const FBoolReadRef& InChordTonesOnly
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
		FEnumMusicalScaleReadRef Scale;
		FBoolReadRef bChordTonesOnly;

		// output pins
		FArrayScaleDegreeWriteRef ScaleArrayOutput;

		// cached values
		Audio::EMusicalScale::Scale PreviousScale = Audio::EMusicalScale::Scale::Count;
		bool bPreviousChordTones = false;
		TArray<float> PreviousNoteOut;

		// other
		FOperatorSettings Settings;

	}; // class FMusicalScaleToNoteArrayOperator

#pragma endregion // Operator Declaration


#pragma region Operator Implementation

	// ctor
	FMusicalScaleToNoteArrayOperator::FMusicalScaleToNoteArrayOperator(
		  const FOperatorSettings& InSettings
		, const FEnumMusicalScaleReadRef& InScale
		, const FBoolReadRef& InChordTonesOnly
		)
		: Scale(InScale)
		, bChordTonesOnly(InChordTonesOnly)
		, ScaleArrayOutput(FArrayScaleDegreeWriteRef::CreateNew())
		, Settings(InSettings)
	{
		// prime our output array
		Execute();
	}


	const FNodeClassMetadata& FMusicalScaleToNoteArrayOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { StandardNodes::Namespace, TEXT("Musical Scale To Note Array"), StandardNodes::AudioVariant };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("Metasound_ScaleToNoteArray_NodeDisplayName", "Scale to Note Array");
			Info.Description = METASOUND_LOCTEXT("MusicalScaleToNoteArray_NodeDescription", "Returns an array represeting the selected scale");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();
			Info.CategoryHierarchy.Emplace(NodeCategories::Music);
			Info.Keywords.Add(METASOUND_LOCTEXT("ScaleToArrayPitchKeyword", "Pitch"));
			Info.Keywords.Add(METASOUND_LOCTEXT("ScaleToArrayMIDIKeyword", "MIDI"));

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}


	FVertexInterface FMusicalScaleToNoteArrayOperator::DeclareVertexInterface()
	{
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FEnumEMusicalScale>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamScaleDegreesPreset)),
				TInputDataVertex<bool>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamChordTonesOnly))
			),
			FOutputVertexInterface(
				TOutputDataVertex<ScaleDegreeArrayType>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamNoteArrayOutput))
				)
			);

		return Interface;
	}


	TUniquePtr<IOperator> FMusicalScaleToNoteArrayOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		const FDataReferenceCollection& InputDataRefs = InParams.InputDataReferences;

		// inputs
		FEnumMusicalScaleReadRef Scale = InputDataRefs.GetDataReadReferenceOrConstruct<FEnumEMusicalScale>(METASOUND_GET_PARAM_NAME(ParamScaleDegreesPreset));
		FBoolReadRef ChordTonesOnly = InputDataRefs.GetDataReadReferenceOrConstruct<bool>(METASOUND_GET_PARAM_NAME(ParamChordTonesOnly));

		return MakeUnique <FMusicalScaleToNoteArrayOperator>(
			  InParams.OperatorSettings
			, Scale
			, ChordTonesOnly
			);
	}

	FDataReferenceCollection FMusicalScaleToNoteArrayOperator::GetInputs() const
	{
		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamScaleDegreesPreset), FEnumMusicalScaleReadRef(Scale));
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamChordTonesOnly), FBoolReadRef(bChordTonesOnly));

		return InputDataReferences;
	}

	FDataReferenceCollection FMusicalScaleToNoteArrayOperator::GetOutputs() const
	{
		// expose read access to our output buffer for other processors in the graph
		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamNoteArrayOutput), FArrayScaleDegreeWriteRef(ScaleArrayOutput));

		return OutputDataReferences;
	}

	void FMusicalScaleToNoteArrayOperator::Execute()
	{
		// calculate new output and cache values if needed
		if ((PreviousScale != *Scale) || (bPreviousChordTones != *bChordTonesOnly))
		{
			// cache values
			PreviousScale = *Scale;
			bPreviousChordTones = *bChordTonesOnly;

			PreviousNoteOut = *ScaleArrayOutput = Audio::FMidiNoteQuantizer::ScaleDegreeSetMap[PreviousScale].GetScaleDegreeSet(bPreviousChordTones);
		}
	}

#pragma endregion // Operator Implementation


#pragma region Node Declaration
	class METASOUNDSTANDARDNODES_API FMusicalScaleToNoteArrayNode: public FNodeFacade
	{
	public:
		// public node api needs to define two conversion constructors:
		// (1: from FString)
		FMusicalScaleToNoteArrayNode(const FVertexName& InInstanceName, const FGuid & InInstanceID)
			: FNodeFacade(InInstanceName, InInstanceID, TFacadeOperatorClass <FMusicalScaleToNoteArrayOperator>())
		{ }

		// (2: From an NodeInitData struct)
		FMusicalScaleToNoteArrayNode(const FNodeInitData & InInitData)
			: FMusicalScaleToNoteArrayNode(InInitData.InstanceName, InInitData.InstanceID)
		{ }

	};
#pragma endregion // Node Declaration


#pragma region Node Registration
	METASOUND_REGISTER_NODE(FMusicalScaleToNoteArrayNode);
#pragma endregion // Node Registration

} // namespace Metasound

#undef LOCTEXT_NAMESPACE //MetasoundBasicFilterNodes
