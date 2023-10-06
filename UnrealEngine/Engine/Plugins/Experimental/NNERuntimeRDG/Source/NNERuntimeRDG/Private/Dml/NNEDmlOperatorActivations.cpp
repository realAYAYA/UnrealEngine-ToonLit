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

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() != 1)
		{
			UE_LOG(LogNNE, Warning, TEXT("Invalid number of input tensors"));
			return false;
		}
		return CheckElementwiseTensor(InputTypes[0], InputShapes[0]);
	}

	virtual ~FOperatorDmlActivationUnary() = default;

private:

	FOperatorDmlActivationUnary() : Alpha(0.0f), Beta(0.0f), Gamma(0.0f), Axis(-1) {}
	float Alpha;
	float Beta;
	float Gamma;
	float Bias;
	float Treshold;
	int Axis;

public:

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
		const NNE::Internal::FTensor& InputTensor = InputTensors[0];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		if constexpr (std::is_same_v<DmlActivationOpDescType, DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC> ||
					  std::is_same_v<DmlActivationOpDescType, DML_ACTIVATION_LOG_SOFTMAX1_OPERATOR_DESC>)
		{
			Axis = Attributes.GetValueOrDefault(TEXT("axis"), -1);
		}
		Axis = HandleNegativeAxis(Axis, InputTensor.GetShape().Rank());

		Alpha = Attributes.GetValueOrDefault(TEXT("alpha"), Alpha);
		Beta = Attributes.GetValueOrDefault(TEXT("beta"), Beta);
		Gamma = Attributes.GetValueOrDefault(TEXT("gamma"), Gamma);

		if constexpr (DmlActivationOpType == DML_OPERATOR_ACTIVATION_SHRINK)
		{
			Bias = Attributes.GetValueOrDefault(TEXT("bias"), 0.0f);
			Treshold = Attributes.GetValueOrDefault(TEXT("lambd"), 0.5f);
		}

		// Initialize tensor descriptor (it's same for both input and output)
		FTensorDescDml	DmlTensorDesc;

		if (!DmlTensorDesc
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlActivationOpDescType	DmlActivationOpDesc{};

		InitDmlOpDesc(DmlActivationOpDesc, *DmlTensorDesc.GetDmlDesc());

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlActivationOpType;
		DmlOpDesc.Desc = &DmlActivationOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
	}

	void InitDmlOpDesc(DML_ACTIVATION_LOG_SOFTMAX1_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.AxisCount = 1;
		Desc.Axes = (UINT*) &Axis;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SOFTMAX1_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.AxisCount = 1;
		Desc.Axes = (UINT*) &Axis;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SOFTPLUS_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Steepness = 1.0f;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SCALED_ELU_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Alpha = Alpha;
		Desc.Gamma = Gamma;
	}

	void InitDmlOpDesc(DML_ACTIVATION_ELU_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Alpha = Alpha;
	}

	void InitDmlOpDesc(DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Alpha = Alpha;
		Desc.Beta = Beta;
	}

	void InitDmlOpDesc(DML_ACTIVATION_LEAKY_RELU_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Alpha = Alpha;
	}

	void InitDmlOpDesc(DML_ACTIVATION_CELU_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Alpha = Alpha;
	}

	void InitDmlOpDesc(DML_ACTIVATION_SHRINK_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc;
		Desc.OutputTensor = &TensorDesc;
		Desc.Bias = Bias;
		Desc.Threshold = Treshold;
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

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		if(InputShapes.Num() != 2)
		{
			UE_LOG(LogNNE, Warning, TEXT("Invalid number of input tensors"));
			return false;
		}

		if(!CheckElementwiseTensor(InputTypes[0], InputShapes[0]))
		{
			return false;
		}

		if(!CheckElementwiseTensor(InputTypes[1], InputShapes[1]))
		{
			return false;
		}

		return true;
	}

private:

	FOperatorDmlActivationBinary() = default;

public:

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) override
	{
		const NNE::Internal::FTensor& InputATensor = InputTensors[0];
		const NNE::Internal::FTensor& InputBTensor = InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = OutputTensors[0];

		// Initialize tensor descriptors
		FTensorDescDml	DmlInputATensorDesc;
		FTensorDescDml	DmlInputBTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputATensorDesc
				.SetFromTensorBroadcast(InputATensor, OutputTensor.GetShape())
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlInputBTensorDesc
				.SetFromTensorBroadcast(InputBTensor, OutputTensor.GetShape())
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlActivationOpDescType	DmlActivationOpDesc{};

		InitDmlOpDesc(DmlActivationOpDesc, *DmlInputATensorDesc.GetDmlDesc(), *DmlInputBTensorDesc.GetDmlDesc(), *DmlOutputTensorDesc.GetDmlDesc());

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlActivationOpType;
		DmlOpDesc.Desc = &DmlActivationOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, DML_TENSOR_DESC& LHSTensorDesc, const DML_TENSOR_DESC& RHSTensorDesc, const DML_TENSOR_DESC& OutputTensorDesc)
	{
		Desc.ATensor = &LHSTensorDesc;
		Desc.BTensor = &RHSTensorDesc;
		Desc.OutputTensor = &OutputTensorDesc;
	}

	void InitDmlOpDesc(DML_ACTIVATION_PARAMETERIZED_RELU_OPERATOR_DESC& Desc, const DML_TENSOR_DESC& LHSTensorDesc, const DML_TENSOR_DESC& RHSTensorDesc, const DML_TENSOR_DESC& OutputTensorDesc)
	{
		Desc.InputTensor = &LHSTensorDesc;
		Desc.SlopeTensor = &RHSTensorDesc;
		Desc.OutputTensor = &OutputTensorDesc;
	}
};

#define REGISTER_OP_ACTIVATION(OpName, DmlOpName, OpClass) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), OpClass<DML_ACTIVATION_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ACTIVATION_##DmlOpName>::Create, OpClass<DML_ACTIVATION_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ACTIVATION_##DmlOpName>::Validate); \
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
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	Celu, 				CELU, 					1.0f, 								0.0f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	Elu, 				ELU, 					1.0f, 								0.0f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	HardSigmoid, 		HARD_SIGMOID, 			0.2f, 								0.5f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	LeakyRelu, 			LEAKY_RELU, 			0.01f, 								0.0f, 		0.0f )
	REGISTER_OP_ACTIVATION_UNARY(			LogSoftmax, 		LOG_SOFTMAX1 )
	REGISTER_OP_ACTIVATION_UNARY(			Relu, 				RELU )
	REGISTER_OP_ACTIVATION_UNARY_PARAMS(	Selu, 				SCALED_ELU, 			1.67326319217681884765625f, 		0.0f, 		1.05070102214813232421875f )
	REGISTER_OP_ACTIVATION_UNARY(			Shrink, 			SHRINK	)
	REGISTER_OP_ACTIVATION_UNARY(			Sigmoid, 			SIGMOID	)
	REGISTER_OP_ACTIVATION_UNARY(			Softmax, 			SOFTMAX1 )
	REGISTER_OP_ACTIVATION_UNARY(			Softplus, 			SOFTPLUS )
	REGISTER_OP_ACTIVATION_UNARY(			Softsign, 			SOFTSIGN )

// 	REGISTER_OP_ACTIVATION_BINARY(	OpName, 			DmlOpName )

	REGISTER_OP_ACTIVATION_BINARY(	Prelu, 				PARAMETERIZED_RELU )



} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML