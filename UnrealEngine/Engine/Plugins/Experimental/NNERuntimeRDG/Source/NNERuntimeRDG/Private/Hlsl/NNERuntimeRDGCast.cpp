// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGCast.h"

#include "Helper/NNERuntimeRDGLogHelper.h"
#include "NNERuntimeRDGHelperCast.h"
#include "NNETensor.h"
#include "NNETypes.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	/**
	 * Cast operator implementation
	 */
	class FCast : public FOperatorHlsl
	{
	public:

		FCast() {}
		virtual ~FCast() = default;

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			OutputTensors[0]->SetShape(InputTensors[0]->GetShape());

			const NNE::Internal::FTensor& X = *InputTensors[0];

			Internal::CPUHelper::Cast::Apply(X, *OutputTensors[0]);

			if (!OutputTensors[0]->HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Cast: Output could not be computed as a constant tensor, however Cast is not implemented on GPU at the moment."));
				return -1;
			}

			return 0;
		}

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			ENNETensorDataType ToFromAttribute = (ENNETensorDataType)Attributes.GetValue<int32>(TEXT("to"));
			ENNETensorDataType ToFromTensor = OutputTensorDescs[0].GetDataType();

			if (ToFromAttribute != ToFromTensor)
			{
				UE_LOG(LogNNE, Warning, TEXT("Cast should output a tensor of type %d but was of type %d."), int(ToFromAttribute), int(ToFromTensor));
				return false;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			UE_LOG(LogNNE, Warning, TEXT("Cast: Output should be constant and already uploaded to GPU memory. Dispatch should not need to be called."));
		}
	};

	bool ValidateCastOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddRequired(TEXT("to"), ENNEAttributeDataType::Int32);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		if (bIsValid)
		{
			ENNETensorDataType To = (ENNETensorDataType)AttributeMap.GetValue<int32>(TEXT("to"));
			switch (To)
			{
				case ENNETensorDataType::Float:
					break;
				case ENNETensorDataType::Int32:
					break;
				case ENNETensorDataType::Int64:
					break;
				default:
					FString TargetType = LogHelper::GetTensorDataTypeName(To);
					UE_LOG(LogNNE, Warning, TEXT("Cast: Target tensor data type %s not supported."), *TargetType);
					bIsValid = false;
			}
		}
		
		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32);
		InputValidator.AddSupportedType(ENNETensorDataType::Int64);
		InputValidator.AddRequired();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateCastOperator()
	{
		return new FCast();
	}

	bool RegisterCastOperator(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
		Registry.OpAdd({{TEXT("Cast"), TEXT("Onnx")}, 6}, CreateCastOperator, ValidateCastOperator);
		Registry.OpAdd({{TEXT("Cast"), TEXT("Onnx")}, 9}, CreateCastOperator, ValidateCastOperator);
		Registry.OpAdd({{TEXT("Cast"), TEXT("Onnx")}, 13}, CreateCastOperator, ValidateCastOperator);
		// Next version: 19
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
