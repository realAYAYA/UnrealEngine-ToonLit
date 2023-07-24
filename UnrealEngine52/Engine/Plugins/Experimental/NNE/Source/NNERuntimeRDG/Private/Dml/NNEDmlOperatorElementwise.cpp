// Copyright Epic Games, Inc. All Rights Reserved.
#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{


/**
 * Element-wise unary ML operator implementation
 */
template
<
	typename DmlElementWiseOpDescType, 
	DML_OPERATOR_TYPE DmlElementWiseOpType
>
class FOperatorDmlElementWiseUnary : public FOperatorDml
{
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlElementWiseUnary();
	}

	virtual ~FOperatorDmlElementWiseUnary() = default;

private:

	FOperatorDmlElementWiseUnary() : Min(TNumericLimits<float>::Min()), Max(TNumericLimits<float>::Max()) {}
	float Min;
	float Max;

public:

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{

		const NNECore::Internal::FTensor& InputTensorDesc = InputTensors[0];
		const NNECore::Internal::FTensor& OutputTensorDesc = OutputTensors[0];


		if constexpr (std::is_same_v<DmlElementWiseOpDescType, DML_ELEMENT_WISE_CLIP_OPERATOR_DESC>)
		{
			const FNNEAttributeValue* MinAttr = Attributes.GetAttributeValue(TEXT("min"));
			if(MinAttr)
			{
				if(MinAttr->GetType() != ENNEAttributeDataType::Float)
				{
					UE_LOG(LogNNE, Error, TEXT("Min attribute of clip must be float for DML inference"));
					return false;
				}
				
				Min = MinAttr->GetValue<float>();
			}
			const FNNEAttributeValue* MaxAttr = Attributes.GetAttributeValue(TEXT("max"));
			if(MaxAttr)
			{
				if(MaxAttr->GetType() != ENNEAttributeDataType::Float)
				{
					UE_LOG(LogNNE, Error, TEXT("Max attribute of clip must be float for DML inference"));
					return false;
				}
				Max = MaxAttr->GetValue<float>();
			}
		}

		// Initialize tensor descriptor (it's same for both input and output)
		DmlUtil::FTensorDesc	DmlTensorDesc{};

		if (!InitDmlTensorDesc(DmlTensorDesc, InputTensorDesc))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DmlElementWiseOpDescType	DmlElemWiseOpDesc{};

		InitDmlOpDesc(DmlElemWiseOpDesc, DmlTensorDesc);

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlElementWiseOpType;
		DmlOpDesc.Desc = &DmlElemWiseOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}

private:

	template<typename OpDesc>
	void InitDmlOpDesc(OpDesc& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
	}

	void InitDmlOpDesc(DML_ELEMENT_WISE_CLIP_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& TensorDesc)
	{
		Desc.InputTensor = &TensorDesc.Desc;
		Desc.OutputTensor = &TensorDesc.Desc;
		Desc.Min = Min;
		Desc.Max = Max;
	}
};


/**
 * Element-wise binary ML operator implementation
 */
template
<
	typename DmlElementWiseOpDescType, 
	DML_OPERATOR_TYPE DmlElementWiseOpType
>
class FOperatorDmlElementWiseBinary : public FOperatorDml
{

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlElementWiseBinary();
	}

private:

	FOperatorDmlElementWiseBinary() = default;

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

		DmlElementWiseOpDescType	DmlElemWiseOpDesc{};

		InitDmlOpDesc(DmlElemWiseOpDesc, DmlInputATensorDesc, DmlInputBTensorDesc, DmlOutputTensorDesc);

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DmlElementWiseOpType;
		DmlOpDesc.Desc = &DmlElemWiseOpDesc;

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

	void InitDmlOpDesc(DML_ELEMENT_WISE_POW_OPERATOR_DESC& Desc, DmlUtil::FTensorDesc& LHSTensor, DmlUtil::FTensorDesc& RHSTensor, DmlUtil::FTensorDesc& OutputTensor)
	{
		Desc.InputTensor = &LHSTensor.Desc;
		Desc.ExponentTensor = &RHSTensor.Desc;
		Desc.OutputTensor = &OutputTensor.Desc;
	}
};

#define REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, OpClass) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), OpClass<DML_ELEMENT_WISE_##DmlOpName##_OPERATOR_DESC, DML_OPERATOR_ELEMENT_WISE_##DmlOpName>::Create); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;


#define REGISTER_OP_ELEMENT_WISE_UNARY(OpName, DmlOpName) REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, FOperatorDmlElementWiseUnary)

#define REGISTER_OP_ELEMENT_WISE_BINARY(OpName, DmlOpName) REGISTER_OP_ELEMENT_WISE(OpName, DmlOpName, FOperatorDmlElementWiseBinary)

// 	REGISTER_OP_ELEMENT_WISE_UNARY(	OpName, 			DmlOpName )
	REGISTER_OP_ELEMENT_WISE_UNARY( Abs,				ABS )
	REGISTER_OP_ELEMENT_WISE_UNARY( Acos, 				ACOS )
	REGISTER_OP_ELEMENT_WISE_UNARY( Acosh, 				ACOSH )
	REGISTER_OP_ELEMENT_WISE_UNARY( Asin, 				ASIN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Asinh, 				ASINH )
	REGISTER_OP_ELEMENT_WISE_UNARY( Atan,				ATAN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Atanh, 				ATANH )
	REGISTER_OP_ELEMENT_WISE_UNARY( Ceil,				CEIL )
	REGISTER_OP_ELEMENT_WISE_UNARY( Clip, 				CLIP )
	REGISTER_OP_ELEMENT_WISE_UNARY( Cos,				COS )
	REGISTER_OP_ELEMENT_WISE_UNARY( Cosh,				COSH )
	REGISTER_OP_ELEMENT_WISE_UNARY( Erf,				ERF )
	REGISTER_OP_ELEMENT_WISE_UNARY( Exp, 				EXP )
	REGISTER_OP_ELEMENT_WISE_UNARY( Floor,				FLOOR )
	REGISTER_OP_ELEMENT_WISE_UNARY( IsInf, 				IS_INFINITY )
	REGISTER_OP_ELEMENT_WISE_UNARY( IsNan, 				IS_NAN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Log,				LOG )
	REGISTER_OP_ELEMENT_WISE_UNARY( Neg,				NEGATE )
	REGISTER_OP_ELEMENT_WISE_UNARY( Reciprocal, 		RECIP )
	REGISTER_OP_ELEMENT_WISE_UNARY( Round,				ROUND )
	REGISTER_OP_ELEMENT_WISE_UNARY( Sign,				SIGN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Sin,				SIN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Sinh,				SINH )
	REGISTER_OP_ELEMENT_WISE_UNARY( Sqrt,				SQRT )
	REGISTER_OP_ELEMENT_WISE_UNARY( Tan,				TAN )
	REGISTER_OP_ELEMENT_WISE_UNARY( Tanh,				TANH )

// 	REGISTER_OP_ELEMENT_WISE_BINARY( OpName, 			DmlOpName )
	REGISTER_OP_ELEMENT_WISE_BINARY( Add,				ADD )
	REGISTER_OP_ELEMENT_WISE_BINARY( Div,				DIVIDE )
	REGISTER_OP_ELEMENT_WISE_BINARY( Mul,				MULTIPLY )
	REGISTER_OP_ELEMENT_WISE_BINARY( Pow,				POW )
	REGISTER_OP_ELEMENT_WISE_BINARY( Sub,				SUBTRACT )



} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML