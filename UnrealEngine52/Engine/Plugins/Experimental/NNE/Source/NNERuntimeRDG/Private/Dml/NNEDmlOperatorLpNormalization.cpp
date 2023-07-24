// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

//
//
//
class FOperatorDmlLpNormalization : public FOperatorDml
{
	static constexpr float DefaultEpsilon = 1e-5f;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlLpNormalization();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() == 1);
		check(OutputTensors.Num() == 1);

		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensor.GetShape().Rank() > 8)
		{
			UE_LOG(LogNNE, Warning, TEXT("InputTensor rank should be between 1 and 8, got:%d"), InputTensor.GetShape().Rank());
			return false;
		}

		// Read attributes
		const int32 P = Attributes.GetValueOrDefault<int32>(TEXT("p"), 1);
		check(P >= 1 && P <= 2);

		int32 OnnxAxis = Attributes.GetValueOrDefault<int32>(TEXT("axis"), 0);

		DmlUtil::FTensorDesc	DmlInputTensor{};

		if (!DmlInputTensor.InitFromTensor(InputTensor, InputTensor.GetShape().Rank()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}
		
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, DmlInputTensor.Sizes.Num()))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		uint32 DmlAxis = GetDmlAxis(OnnxAxis, InputTensor.GetShape().Rank(), DmlInputTensor.Sizes.Num());

		DML_LP_NORMALIZATION_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensor.Desc;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
		OpDesc.Axis = DmlAxis;
		OpDesc.Epsilon = DefaultEpsilon;
		OpDesc.P = P;

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_LP_NORMALIZATION, &OpDesc });
	}
};

// Register operator on Module startup
NNE_DML_REGISTER_OP(LpNormalization)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
