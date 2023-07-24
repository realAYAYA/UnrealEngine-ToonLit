// Copyright Epic Games, Inc. All Rights Reserved.
#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 * Activation unary ML operator implementation
 */
template
<
	typename DmlActivationOpDescType,
	DML_OPERATOR_TYPE DmlActivationOpType
>
class FOperatorDmlActivationUnary : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlActivationUnary();
	}

	virtual ~FOperatorDmlActivationUnary() = default;

private:

	FOperatorDmlActivationUnary() : Alpha(0.0f), Beta(0.0f), Gamma(0.0f), Axis(-1) {}
	float Alpha;
	float Beta;
	float Gamma;
	int Axis;

public:

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		const NNECore::Internal::FTensor& InputTensorDesc = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensorDesc = OutputTensors[0];

		if constexpr (std::is_same_v<DmlActivationOpDescType, DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC> ||
					  std::is_same_v<DmlActivationOpDescType, DML_ACTIVATION_LOG_SOFTMAX1_OPERATOR_DESC>)
		{
			Axis = Attributes.GetValueOrDefault(TEXT("axis"), -1);
		}
		Axis = HandleNegativeAxis(Axis, InputTensorDesc.GetShape().Rank());

		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		Gamma = Attributes.GetValueOrDefault(TEXT("gamma"), Gamma);


		// Initialize tensor descriptor (it's same for both input and output)
		DmlUtil::FTensorDesc	DmlTensorDesc{};

		if (!InitDmlTensorDesc(DmlTensorDesc, InputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlActivationOpDescType	DmlActivationOpDesc{};

		InitDmlOpDesc(DmlActivationOpDesc, DmlTensorDesc);

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlActivationOpType;
		DmlOpDesc.Desc = &DmlActivationOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
	}

	void InitDmlOpDesc(DML_ACTIVATION_LOG_SOFTMAX1_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.AxisCount = 1;
		Desc.Axes = (UINT*) &Axis;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.AxisCount = 1;
		Desc.Axes = (UINT*) &Axis;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Steepness = 1.0f;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SCALED_ELU_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Alpha = Alpha;
		Desc.Gamma = Gamma;
	}

	void InitDmlOpDesc(DML_ACTIVATION_ELU_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Alpha = Alpha;
	}

	void InitDmlOpDesc(DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Alpha = Alpha;
		Desc.Beta = Beta;
	}

	void InitDmlOpDesc(DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Alpha = Alpha;
	}
};

/**
 * Activation binary ML operator implementation
 */
template
<
	typename DmlActivationOpDescType,
	DML_OPERATOR_TYPE DmlActivationOpType
>
class FOperatorDmlActivationBinary : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlActivationBinary();
	}

private:

	FOperatorDmlActivationBinary() = default;

public:

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		const NNECore::Internal::FTensor& InputATensorDesc = InputTensors[0];
		const NNECore::Internal::FTensor& InputBTensorDesc = InputTensors[1];
		const NNECore::Internal::FTensor& OutputTensorDesc = OutputTensors[0];

		// Initialize tensor descriptors
		DmlUtil::FTensorDesc	DmlInputATensorDesc{};
		DmlUtil::FTensorDesc	DmlInputBTensorDesc{};
		DmlUtil::FTensorDesc	DmlOutputTensorDesc{};

		if (!InitDmlTensorDesc(DmlInputATensorDesc, InputATensorDesc, OutputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!InitDmlTensorDesc(DmlInputBTensorDesc, InputBTensorDesc, OutputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!InitDmlTensorDesc(DmlOutputTensorDesc, OutputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlActivationOpDescType	DmlActivationOpDesc{};

		InitDmlOpDesc(DmlActivationOpDesc, DmlInputATensorDesc, DmlInputBTensorDesc, DmlOutputTensorDesc);

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlActivationOpType;
		DmlOpDesc.Desc = &DmlActivationOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, DmlUtil::FTensorDesc& LHSTensor, DmlUtil::FTensorDesc& RHSTensor, DmlUtil::FTensorDesc& OutputTensor)
	{
		Desc.ATensor = &LHSTensor.Desc;
		Desc.BTensor = &RHSTensor.Desc;
		Desc.OutputTensor = &OutputTensor.Desc;
	}

	void InitDmlOpDesc(DML_ACTIVATION_PARAMETERIZED_RELU_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& LHSTensor, DmlUtil::FTensorDesc& RHSTensor, DmlUtil::FTensorDesc& OutputTensor)
	{
		Desc.InputTensor = &LHSTensor.Desc;
		Desc.SlopeTensor = &RHSTensor.Desc;
		Desc.OutputTensor = &OutputTensor.Desc;
	}
};

#define REGISTER_OP_ACTIVATION(OpName, DmlOpName, OpClass) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), OpClass<DML_ACTIVATION_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ACTIVATION_##DmlOpName>::Create); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;


#define REGISTER_OP_ACTIVATION_UNARY(OpName, DmlOpName) REGISTER_OP_ACTIVATION(OpName, DmlOpName, FOperatorDmlActivationUnary)

#define REGISTER_OP_ACTIVATION_UNARY_PARAMS(OpName, DmlOpName, InAlpha, InBeta, InGamma) \
template<> FOperatorDmlActivationUnary<DML_ACTIVATION_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ACTIVATION_##DmlOpName>::FOperatorDmlActivationUnary() \
	: Alpha(InAlpha), Beta(InBeta), Gamma(InGamma) \
{ \
} \
REGISTER_OP_ACTIVATION_UNARY(OpName, DmlOpName)


#define REGISTER_OP_ACTIVATION_BINARY(OpName, DmlOpName) REGISTER_OP_ACTIVATION(OpName, DmlOpName, FOperatorDmlActivationBinary)



// 	REGISTER_OP_ACTIVATION_UNARY(			OpName, 			DmlOpName )
// 	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	OpName, 			DmlOpName, 				InAlpha, 							InBeta, 	InGamma )

	REGISTER_OP_ACTIVATION_UNARY(			Dropout,			IDENTITY )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	Elu, 				ELU, 					1.0f, 								0.0f, 		1.05070102214813232421875f )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	HardSigmoid, 		HARD_SIGMOID, 			0.2f, 								0.5f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	LeakyRelu, 			LEAKY_RELU, 			0.01f, 								0.0f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY(			LogSoftmax, 		LOG_SOFTMAX1 )
	REGISTER_OP_ACTIVATION_UNARY(			Relu, 				RELU )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	Selu, 				SCALED_ELU, 			1.67326319217681884765625f, 		0.0f, 		1.05070102214813232421875f )
	REGISTER_OP_ACTIVATION_UNARY(			Sigmoid, 			SIGMOID	)
	REGISTER_OP_ACTIVATION_UNARY(			Softmax, 			SOFTMAX1 )
	REGISTER_OP_ACTIVATION_UNARY(			Softplus, 			SOFTPLUS )
	REGISTER_OP_ACTIVATION_UNARY(			Softsign, 			SOFTSIGN )

// 	REGISTER_OP_ACTIVATION_BINARY(	OpName, 			DmlOpName )

	REGISTER_OP_ACTIVATION_BINARY(	Prelu, 				PARAMETERIZED_RELU )



} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML