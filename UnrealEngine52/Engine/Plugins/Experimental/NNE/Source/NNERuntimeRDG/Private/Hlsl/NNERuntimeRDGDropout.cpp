// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGDropout.h"
#include "NNECoreTensor.h"
#include "NNECoreTypes.h"
#include "RenderGraphUtils.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorDropout, TEXT("NNE.Operator.Hlsl.Dropout"));

	/**
	 * Dropout operator implementation
	 */
	class FDropout : public FOperatorHlsl
	{
	public:

		FDropout() {}
		virtual ~FDropout() = default;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNECore::Internal::FTensorRef> InputTensors, TArrayView<NNECore::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);

			const NNECore::Internal::FTensor& X = *InputTensors[0];

			OutputTensors[0]->SetShape(X.GetShape());
			if (X.HasPreparedData())
			{
				OutputTensors[0]->SetPreparedData<uint8>(X.GetPreparedData<uint8>());
			}

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNECore::FTensorDesc> InputTensorDescs, TConstArrayView<NNECore::FTensorDesc> OutputTensorDescs, const NNECore::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1 || OutputTensorDescs.Num() == 2);

			if (OutputTensorDescs.Num() == 2)
			{
				UE_LOG(LogNNE, Warning, TEXT("Dropout is only supported in inference mode at the moment, without the ability to output a 'mask'."));
				return false;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1 || OutputTensors.Num() == 2);
			check(InputTensors[0] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Data = *InputTensors[0];
			const FTensorRDG& Output = *OutputTensors[0];

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Dropout");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorDropout);

			AddCopyBufferPass(GraphBuilder, Output.GetBuffer(), Data.GetBuffer());
		}
	};

	bool ValidateDropoutOperator(const NNECore::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNECore::FSymbolicTensorShape> InputDropouts)
	{
		bool bIsValid = true;

		//This match version 13 of the Dropout operator
		//https://github.com/onnx/onnx/blob/main/docs/Operators.md#Dropout
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("seed"), ENNEAttributeDataType::Int32); //Will be ignored, only useful in training mode.
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Half);
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddSupportedType(ENNETensorDataType::Double);
		InputValidator.AddRequired();

		if (InputTypes.Num() > 1)
		{
			UE_LOG(LogNNE, Warning, TEXT("Dropout is only supported in inference mode at the moment, 'ratio' and 'training_mode' will be ignored."));
		}

		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateDropoutOperator()
	{
		return new FDropout();
	}

	bool RegisterDropoutOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Dropout"), CreateDropoutOperator, ValidateDropoutOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
