// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interfaces/MetasoundFrontendSourceInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundEngineNodesNames.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundPrimitives.h"
#include "MetasoundParamHelper.h"
#include "MetasoundStandardNodesCategories.h"
#include "MetasoundWaveTable.h"
#include "WaveTableSampler.h"

#define LOCTEXT_NAMESPACE "MetasoundStandardNodes"

namespace Metasound
{
	namespace WaveTableEvaluateNode
	{
		METASOUND_PARAM(WaveTableParam, "WaveTable", "The Wavetable to evaluate");
		METASOUND_PARAM(InputParam, "Input", "[0, 1] the X input with which to evaluate the wavetable");
		METASOUND_PARAM(OutParam, "Output", "The value of the wavetable at the specified point");
	}
	
	class FMetasoundWaveTableEvaluateNodeOperator : public TExecutableOperator<FMetasoundWaveTableEvaluateNodeOperator>
	{
	public:
		static const FVertexInterface& GetDefaultInterface()
		{
			using namespace WaveTable;
			using namespace WaveTableEvaluateNode;

			static const FVertexInterface DefaultInterface(
				FInputVertexInterface(
					TInputDataVertex<FWaveTable>(METASOUND_GET_PARAM_NAME_AND_METADATA(WaveTableParam)),
					TInputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputParam), 0.0f),
					TInputDataVertex<FEnumWaveTableInterpolationMode>("Interpolation", FDataVertexMetadata
					{
						LOCTEXT("MetasoundWaveTableEvaluateNode_InterpDescription", "How the Evaluate interpolates between WaveTable values."),
						LOCTEXT("MetasoundWaveTableEvaluateNode_Interp", "Interpolation"),
						true /* bIsAdvancedDisplay */
					}, static_cast<int32>(FWaveTableSampler::EInterpolationMode::Linear))
				),
				FOutputVertexInterface(
					TOutputDataVertex<float>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutParam))
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
					{ EngineNodes::Namespace, "WaveTableEvaluate", "" },
					1, // Major Version
					0, // Minor Version
					LOCTEXT("MetasoundWaveTableEvaluateNode_Name", "Evaluate WaveTable"),
					LOCTEXT("MetasoundWaveTableEvaluateNode_Description", "Evaluates a wavetable for a given input value."),
					PluginAuthor,
					PluginNodeMissingPrompt,
					GetDefaultInterface(),
					{ NodeCategories::Envelopes },
					{ METASOUND_LOCTEXT("WaveTableEvaluateEnvelopeKeyword", "Envelope"), METASOUND_LOCTEXT("WaveTableEvaluateCurveKeyword", "Curve")},
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
			FFloatReadRef InInputReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<float>(InputInterface, "Input", InParams.OperatorSettings);
			FEnumWaveTableInterpModeReadRef InInterpReadRef = InputCollection.GetDataReadReferenceOrConstructWithVertexDefault<FEnumWaveTableInterpolationMode>(InputInterface, "Interpolation", InParams.OperatorSettings);

			return MakeUnique<FMetasoundWaveTableEvaluateNodeOperator>(InParams, InWaveTableReadRef, InInputReadRef, InInterpReadRef);
		}

		FMetasoundWaveTableEvaluateNodeOperator(
			const FCreateOperatorParams& InParams,
			const FWaveTableReadRef& InWaveTableReadRef,
			const FFloatReadRef& InInputReadRef,
			const FEnumWaveTableInterpModeReadRef& InInterpModeReadRef)
			: WaveTableReadRef(InWaveTableReadRef)
			, InputReadRef(InInputReadRef)
			, InterpModeReadRef(InInterpModeReadRef)
			, OutWriteRef(TDataWriteReferenceFactory<float>::CreateAny(InParams.OperatorSettings))
		{
			using namespace WaveTable;
			FWaveTableSampler::FSettings Settings;
			Settings.Freq = 0.0f; // Sampler phase is manually progressed via this node
			Sampler = FWaveTableSampler(MoveTemp(Settings));
		}

		virtual ~FMetasoundWaveTableEvaluateNodeOperator() = default;

		virtual void Bind(FVertexInterfaceData& InVertexData) const override
		{
			using namespace WaveTableEvaluateNode;
			
			FInputVertexInterfaceData& Inputs = InVertexData.GetInputs();
			Inputs.BindReadVertex(METASOUND_GET_PARAM_NAME(WaveTableParam), WaveTableReadRef);
			Inputs.BindReadVertex(METASOUND_GET_PARAM_NAME(InputParam), InputReadRef);
			Inputs.BindReadVertex("Interpolation", InterpModeReadRef);

			FOutputVertexInterfaceData& Outputs = InVertexData.GetOutputs();
			Outputs.BindReadVertex(METASOUND_GET_PARAM_NAME(OutParam), OutWriteRef);
		}
		
		virtual FDataReferenceCollection GetInputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed. 
			checkNoEntry();

			FDataReferenceCollection InputDataReferences;
			return InputDataReferences;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			// This should never be called. Bind(...) is called instead. This method
			// exists as a stop-gap until the API can be deprecated and removed. 
			checkNoEntry();

			FDataReferenceCollection OutputDataReferences;
			return OutputDataReferences;
		}

		void Execute()
		{
			using namespace WaveTable;
			const float Input = *InputReadRef;

			float NextValue = 0.f;
			
			const FWaveTableView& WaveTableView = WaveTableReadRef->GetView();

			constexpr auto SampleMode =  FWaveTableSampler::ESingleSampleMode::Hold;
			
			Sampler.Reset();
			Sampler.SetInterpolationMode(*InterpModeReadRef);
			Sampler.SetPhase(FMath::Clamp(Input, 0.0f, 1.0f));
			Sampler.Process(WaveTableView, NextValue, SampleMode);

			*OutWriteRef = NextValue;
		}

	private:
		FWaveTableReadRef WaveTableReadRef;
		FFloatReadRef InputReadRef;
		FEnumWaveTableInterpModeReadRef InterpModeReadRef;

		WaveTable::FWaveTableSampler Sampler;

		FFloatWriteRef OutWriteRef;
	};

	class FMetasoundWaveTableEvaluateNode : public FNodeFacade
	{
	public:
		FMetasoundWaveTableEvaluateNode(const FNodeInitData& InInitData)
			: FNodeFacade(InInitData.InstanceName, InInitData.InstanceID, TFacadeOperatorClass<FMetasoundWaveTableEvaluateNodeOperator>())
		{
		}

		virtual ~FMetasoundWaveTableEvaluateNode() = default;
	};

	METASOUND_REGISTER_NODE(FMetasoundWaveTableEvaluateNode)
} // namespace Metasound

#undef LOCTEXT_NAMESPACE // MetasoundStandardNodes
