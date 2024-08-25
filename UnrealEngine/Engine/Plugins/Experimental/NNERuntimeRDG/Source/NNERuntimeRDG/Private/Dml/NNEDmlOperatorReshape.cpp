// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"
#include "Algo/Transform.h"
#include "Algo/Count.h"
#include "Misc/EnumerateRange.h"
#include "Helper/NNERuntimeRDGHelperReshape.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

class FOperatorDmlReshape : public FOperatorDml
{
	mutable Util::FSmallUIntArray	OutputShape;
	bool							bAllowZero;
	static constexpr uint32 NumAllowedInputTensors = 2, NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = GMaxTensorRank;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlReshape();
	}

    static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = TEXT("Reshape");

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	ENNETensorDataType::Double, ENNETensorDataType::Float, ENNETensorDataType::Half, 
				ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
				ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, 
				ENNETensorDataType::UInt16, ENNETensorDataType::UInt8
			},
			MinTensorRank, MaxTensorRank
		  	))
		{
			return false;
		}

		if(InputShapes.Num() == 2) //-V547
		{
			if (!CheckGenericTensor1D(OpName, InputTypes[1], InputShapes[1], 
				{ 	ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::UInt32
				}
				))
			{
				return false;
			}
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() == NumAllowedInputTensors);
		check(Outputs.Num() == NumAllowedOutputTensors);

		ConstantCPUInputs.Add(1);

		bAllowZero = Attributes.GetValueOrDefault<int32>(TEXT("allowzero"), 0) != 0;

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		// Shape tensor must be constant!
		check(InputTensors[1]->HasPreparedData());
		
		OutputShape.Reset();

		switch (InputTensors[1]->GetDataType())
		{
			case ENNETensorDataType::Int32:
				if (!ShapeHelper::Reshape::ReshapeTensor<int32>(InputTensors[0]->GetShape(), *InputTensors[1], bAllowZero, OutputShape))
				{
					return false;
				}
				break;

			case ENNETensorDataType::Int64:
				if (!ShapeHelper::Reshape::ReshapeTensor<int64>(InputTensors[0]->GetShape(), *InputTensors[1], bAllowZero, OutputShape))
				{
					return false;
				}
				break;

			case ENNETensorDataType::UInt32:
				if (!ShapeHelper::Reshape::ReshapeTensor<uint32>(InputTensors[0]->GetShape(), *InputTensors[1], bAllowZero, OutputShape))
				{
					return false;
				}
				break;

			default:
				UE_LOG(LogNNE, Warning, TEXT("Shape tensor has invalid data type"));
				return false;
		};

		OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
        FTensorDescDml DmlInputTensorDesc;

        if (!DmlInputTensorDesc
				.SetFromTensor(*InputTensors[0])
				.SetShape(OutputShape)
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reshape's input tensor for DML inference"));
			return false;
		}

        FTensorDescDml DmlOutputTensorDesc;

        if (!DmlOutputTensorDesc
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize Reshape's output tensor for DML inference"));
			return false;
		}
        
		DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC DmlIdentityOpDesc{};

        DmlIdentityOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
        DmlIdentityOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_ELEMENT_WISE_IDENTITY, &DmlIdentityOpDesc} );
	}
};

// Register Reshape operator on Module startup
NNE_DML_REGISTER_OP_VERSION(Reshape, 5)
NNE_DML_REGISTER_OP_VERSION(Reshape, 13)
NNE_DML_REGISTER_OP_VERSION(Reshape, 14)
NNE_DML_REGISTER_OP_VERSION(Reshape, 19)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
