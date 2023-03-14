// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

#include "MetasoundWave.h"
#include "MetasoundFacade.h"
#include "MetasoundPrimitives.h"
#include "MetasoundAudioBuffer.h"
#include "MetasoundParamHelper.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundVertex.h"

#define LOCTEXT_NAMESPACE "MetasoundWaveInfo"

namespace Metasound
{
	// forward declarations
	// ...

	namespace WaveInfoNodeParameterNames
	{
		// inputs
		METASOUND_PARAM(ParamWaveAsset, "Wave", "Input Wave Asset");

		// outputs
		METASOUND_PARAM(ParamDurationSeconds, "Duration", "Duration of the wave asset in seconds");

	} // namespace WaveInfoNodeParameterNames

	using namespace WaveInfoNodeParameterNames;



	class FWaveInfoNodeOperator : public TExecutableOperator < FWaveInfoNodeOperator >
	{
	public:
		// ctor
		FWaveInfoNodeOperator(const FOperatorSettings& InSettings, const FWaveAssetReadRef& InWaveAsset);

		// node interface
		static const FNodeClassMetadata& GetNodeInfo();
		static FVertexInterface DeclareVertexInterface();
		static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		void Execute();

	private: // members
		// input pins
		FWaveAssetReadRef WaveAsset;

		// output pins
		FTimeWriteRef DurationSeconds;

		// other
		FOperatorSettings Settings;

	}; // class FWaveInfoNodeOperator




	// ctor
	FWaveInfoNodeOperator::FWaveInfoNodeOperator(const FOperatorSettings& InSettings, const FWaveAssetReadRef& InWaveAsset)
		: WaveAsset(InWaveAsset)
		, DurationSeconds(FTimeWriteRef::CreateNew(0.0f))
		, Settings(InSettings)
	{
		Execute();
	}


	const FNodeClassMetadata& FWaveInfoNodeOperator::GetNodeInfo()
	{
		auto InitNodeInfo = []() -> FNodeClassMetadata
		{
			FNodeClassMetadata Info;
			Info.ClassName = { Metasound::EngineNodes::Namespace, TEXT("Get Wave Duration"), TEXT(" ") };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.DisplayName = METASOUND_LOCTEXT("MetasoundGetWaveDuration_ClassNodeDisplayName", "Get Wave Duration");
			Info.Description = METASOUND_LOCTEXT("GetWaveDuration_NodeDescription", "Returns the duration of the input Wave asset (in seconds)"),
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		};

		static const FNodeClassMetadata Info = InitNodeInfo();

		return Info;
	}


	FVertexInterface FWaveInfoNodeOperator::DeclareVertexInterface()
	{
		using namespace WaveInfoNodeParameterNames;
		static const FVertexInterface Interface(
			FInputVertexInterface(
				TInputDataVertex<FWaveAsset>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamWaveAsset))
			),
			FOutputVertexInterface(
				TOutputDataVertex<FTime>(METASOUND_GET_PARAM_NAME_AND_METADATA(ParamDurationSeconds))
			)
		);

		return Interface;
	}


	TUniquePtr<IOperator> FWaveInfoNodeOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
	{
		using namespace WaveInfoNodeParameterNames;
		
		const FDataReferenceCollection& InputDataRefs = InParams.InputDataReferences;

		// inputs
		FWaveAssetReadRef WaveAssetIn = InputDataRefs.GetDataReadReferenceOrConstruct<FWaveAsset>(METASOUND_GET_PARAM_NAME(ParamWaveAsset));

		return MakeUnique < FWaveInfoNodeOperator >(InParams.OperatorSettings, WaveAssetIn);
	}

	FDataReferenceCollection FWaveInfoNodeOperator::GetInputs() const
	{
		using namespace WaveInfoNodeParameterNames;
		FDataReferenceCollection InputDataReferences;
		InputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamWaveAsset), FWaveAssetReadRef(WaveAsset));

		return InputDataReferences;
	}

	FDataReferenceCollection FWaveInfoNodeOperator::GetOutputs() const
	{
		// expose read access to our output buffer for other processors in the graph
		using namespace WaveInfoNodeParameterNames;
		FDataReferenceCollection OutputDataReferences;
		OutputDataReferences.AddDataReadReference(METASOUND_GET_PARAM_NAME(ParamDurationSeconds), DurationSeconds);

		return OutputDataReferences;
	}

	void FWaveInfoNodeOperator::Execute()
	{
		if ((*WaveAsset).IsSoundWaveValid())
		{
			*DurationSeconds = FTime::FromSeconds((*WaveAsset)->GetDuration());
		}
		else
		{
			*DurationSeconds = FTime::FromSeconds(0.0f);
		}
	}



	class FWaveInfoNode : public FNodeFacade
	{
	public:
		// public node api needs to define two conversion constructors:
		// (1: from FString)
		FWaveInfoNode(const Metasound::FVertexName& InInstanceName, const FGuid& InInstanceID)
			: FNodeFacade(InInstanceName, InInstanceID, TFacadeOperatorClass < FWaveInfoNodeOperator >())
		{ }

		// (2: From an NodeInitData struct)
		FWaveInfoNode(const FNodeInitData& InInitData)
			: FWaveInfoNode(InInitData.InstanceName, InInitData.InstanceID)
		{ }

	};


	METASOUND_REGISTER_NODE(FWaveInfoNode);

} // namespace Metasound

#undef LOCTEXT_NAMESPACE //MetasoundWaveInfo
