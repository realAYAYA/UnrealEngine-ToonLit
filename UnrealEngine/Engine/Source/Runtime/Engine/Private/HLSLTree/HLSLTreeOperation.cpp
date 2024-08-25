// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "HLSLTree/HLSLTreeCommon.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "Shader/Preshader.h"

namespace UE::HLSLTree
{

class FExpressionOperation : public FExpression
{
public:
	FExpressionOperation(EOperation InOp, TConstArrayView<const FExpression*> InInputs);

	static constexpr int8 MaxInputs = 3;

	EOperation Op;
	const FExpression* Inputs[MaxInputs];

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

FOperationDescription::FOperationDescription(const TCHAR* InName, const TCHAR* InOperator, int8 InNumInputs, Shader::EPreshaderOpcode InOpcode)
	: Name(InName), Operator(InOperator), NumInputs(InNumInputs), PreshaderOpcode(InOpcode)
{}

FOperationDescription::FOperationDescription()
	: Name(nullptr), Operator(nullptr), PreshaderOpcode(Shader::EPreshaderOpcode::Nop)
{}


FOperationDescription GetOperationDescription(EOperation Op)
{
	switch (Op)
	{
	case EOperation::None: return FOperationDescription(TEXT("None"), TEXT(""), 0, Shader::EPreshaderOpcode::Nop); break;

	// Unary
	case EOperation::Abs: return FOperationDescription(TEXT("Abs"), TEXT("abs"), 1, Shader::EPreshaderOpcode::Abs); break;
	case EOperation::Neg: return FOperationDescription(TEXT("Neg"), TEXT("-"), 1, Shader::EPreshaderOpcode::Neg); break;
	case EOperation::Rcp: return FOperationDescription(TEXT("Rcp"), TEXT("/"), 1, Shader::EPreshaderOpcode::Rcp); break;
	case EOperation::Sqrt: return FOperationDescription(TEXT("Sqrt"), TEXT("sqrt"), 1, Shader::EPreshaderOpcode::Sqrt); break;
	case EOperation::Rsqrt: return FOperationDescription(TEXT("Rsqrt"), TEXT("rsqrt"), 1, Shader::EPreshaderOpcode::Nop); break; // TODO
	case EOperation::Log: return FOperationDescription(TEXT("Log"), TEXT("log"), 1, Shader::EPreshaderOpcode::Log); break;
	case EOperation::Log2: return FOperationDescription(TEXT("Log2"), TEXT("log2"), 1, Shader::EPreshaderOpcode::Log2); break;
	case EOperation::Exp: return FOperationDescription(TEXT("Exp"), TEXT("exp"), 1, Shader::EPreshaderOpcode::Exp); break;
	case EOperation::Exp2: return FOperationDescription(TEXT("Exp2"), TEXT("exp2"), 1, Shader::EPreshaderOpcode::Exp2); break; // TODO
	case EOperation::Frac: return FOperationDescription(TEXT("Frac"), TEXT("frac"), 1, Shader::EPreshaderOpcode::Frac); break;
	case EOperation::Floor: return FOperationDescription(TEXT("Floor"), TEXT("floor"), 1, Shader::EPreshaderOpcode::Floor); break;
	case EOperation::Ceil: return FOperationDescription(TEXT("Ceil"), TEXT("ceil"), 1, Shader::EPreshaderOpcode::Ceil); break;
	case EOperation::Round: return FOperationDescription(TEXT("Round"), TEXT("round"), 1, Shader::EPreshaderOpcode::Round); break;
	case EOperation::Trunc: return FOperationDescription(TEXT("Trunc"), TEXT("trunc"), 1, Shader::EPreshaderOpcode::Trunc); break;
	case EOperation::Saturate: return FOperationDescription(TEXT("Saturate"), TEXT("saturate"), 1, Shader::EPreshaderOpcode::Saturate); break;
	case EOperation::Sign: return FOperationDescription(TEXT("Sign"), TEXT("sign"), 1, Shader::EPreshaderOpcode::Sign); break;
	case EOperation::Length: return FOperationDescription(TEXT("Length"), TEXT("length"), 1, Shader::EPreshaderOpcode::Length); break;
	case EOperation::Normalize: return FOperationDescription(TEXT("Normalize"), TEXT("normalize"), 1, Shader::EPreshaderOpcode::Normalize); break;
	case EOperation::Sum: return FOperationDescription(TEXT("Sum"), TEXT("sum"), 1, Shader::EPreshaderOpcode::Nop); break; // TODO
	case EOperation::Sin: return FOperationDescription(TEXT("Sin"), TEXT("sin"), 1, Shader::EPreshaderOpcode::Sin); break;
	case EOperation::Cos: return FOperationDescription(TEXT("Cos"), TEXT("cos"), 1, Shader::EPreshaderOpcode::Cos); break;
	case EOperation::Tan: return FOperationDescription(TEXT("Tan"), TEXT("tan"), 1, Shader::EPreshaderOpcode::Tan); break;
	case EOperation::Asin: return FOperationDescription(TEXT("Asin"), TEXT("asin"), 1, Shader::EPreshaderOpcode::Asin); break;
	case EOperation::AsinFast: return FOperationDescription(TEXT("AsinFast"), TEXT("asinFast"), 1, Shader::EPreshaderOpcode::Asin); break;
	case EOperation::Acos: return FOperationDescription(TEXT("Acos"), TEXT("acos"), 1, Shader::EPreshaderOpcode::Acos); break;
	case EOperation::AcosFast: return FOperationDescription(TEXT("AcosFast"), TEXT("acosFast"), 1, Shader::EPreshaderOpcode::Acos); break;
	case EOperation::Atan: return FOperationDescription(TEXT("Atan"), TEXT("atan"), 1, Shader::EPreshaderOpcode::Atan); break;
	case EOperation::AtanFast: return FOperationDescription(TEXT("AtanFast"), TEXT("atanFast"), 1, Shader::EPreshaderOpcode::Atan); break;

	// Binary
	case EOperation::Add: return FOperationDescription(TEXT("Add"), TEXT("+"), 2, Shader::EPreshaderOpcode::Add); break;
	case EOperation::Sub: return FOperationDescription(TEXT("Subtract"), TEXT("-"), 2, Shader::EPreshaderOpcode::Sub); break;
	case EOperation::Mul: return FOperationDescription(TEXT("Multiply"), TEXT("*"), 2, Shader::EPreshaderOpcode::Mul); break;
	case EOperation::Div: return FOperationDescription(TEXT("Divide"), TEXT("/"), 2, Shader::EPreshaderOpcode::Div); break;
	case EOperation::Fmod: return FOperationDescription(TEXT("Fmod"), TEXT("%"), 2, Shader::EPreshaderOpcode::Fmod); break;
	case EOperation::Step: return FOperationDescription(TEXT("Step"), TEXT("step"), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::PowPositiveClamped: return FOperationDescription(TEXT("PowPositiveClamped"), TEXT("PowPositiveClamped"), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::Atan2: return FOperationDescription(TEXT("Atan2"), TEXT("atan2"), 2, Shader::EPreshaderOpcode::Atan2); break;
	case EOperation::Atan2Fast: return FOperationDescription(TEXT("Atan2Fast"), TEXT("atan2Fast"), 2, Shader::EPreshaderOpcode::Atan2); break;
	case EOperation::Min: return FOperationDescription(TEXT("Min"), TEXT("min"), 2, Shader::EPreshaderOpcode::Min); break;
	case EOperation::Max: return FOperationDescription(TEXT("Max"), TEXT("max"), 2, Shader::EPreshaderOpcode::Max); break;
	case EOperation::Less: return FOperationDescription(TEXT("Less"), TEXT("<"), 2, Shader::EPreshaderOpcode::Less); break;
	case EOperation::Greater: return FOperationDescription(TEXT("Greater"), TEXT(">"), 2, Shader::EPreshaderOpcode::Greater); break;
	case EOperation::LessEqual: return FOperationDescription(TEXT("LessEqual"), TEXT("<="), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::GreaterEqual: return FOperationDescription(TEXT("GreaterEqual"), TEXT(">="), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::VecMulMatrix3: return FOperationDescription(TEXT("VecMulMatrix3"), TEXT("mul"), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::VecMulMatrix4: return FOperationDescription(TEXT("VecMulMatrix4"), TEXT("mul"), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::Matrix3MulVec: return FOperationDescription(TEXT("Matrix3MulVec"), TEXT("mul"), 2, Shader::EPreshaderOpcode::Nop); break;
	case EOperation::Matrix4MulVec: return FOperationDescription(TEXT("Matrix4MulVec"), TEXT("mul"), 2, Shader::EPreshaderOpcode::Nop); break;

	// Ternary
	case EOperation::SmoothStep: return FOperationDescription(TEXT("SmoothStep"), TEXT("smoothstep"), 3, Shader::EPreshaderOpcode::Nop); break;

	default: checkNoEntry(); return FOperationDescription();
	}
}

const FExpression* FTree::NewUnaryOp(EOperation Op, const FExpression* Input)
{
	const FExpression* Inputs[1] = { Input };
	return NewExpression<FExpressionOperation>(Op, Inputs);
}

const FExpression* FTree::NewBinaryOp(EOperation Op, const FExpression* Lhs, const FExpression* Rhs)
{
	const FExpression* Inputs[2] = { Lhs, Rhs };
	return NewExpression<FExpressionOperation>(Op, Inputs);
}

const FExpression* FTree::NewTernaryOp(EOperation Op, const FExpression* Input0, const FExpression* Input1, const FExpression* Input2)
{
	const FExpression* Inputs[3] = { Input0, Input1, Input2 };
	return NewExpression<FExpressionOperation>(Op, Inputs);
}

const FExpression* FTree::NewLog(const FExpression* Input)
{
	const float Log2ToLog = 1.0f / FMath::Log2(UE_EULERS_NUMBER);
	return NewMul(NewLog2(Input), NewConstant(Log2ToLog));
}

const FExpression* FTree::NewCross(const FExpression* Lhs, const FExpression* Rhs)
{
	//c_P[0] = v_A[1] * v_B[2] - v_A[2] * v_B[1];
	//c_P[1] = -(v_A[0] * v_B[2] - v_A[2] * v_B[0]);
	//c_P[2] = v_A[0] * v_B[1] - v_A[1] * v_B[0];
	const FExpression* Lhs0 = NewSwizzle(FSwizzleParameters(1, 2, 0), Lhs);
	const FExpression* Lhs1 = NewSwizzle(FSwizzleParameters(2, 0, 1), Lhs);
	const FExpression* Rhs0 = NewSwizzle(FSwizzleParameters(2, 0, 1), Rhs);
	const FExpression* Rhs1 = NewSwizzle(FSwizzleParameters(1, 2, 0), Rhs);
	return NewSub(NewMul(Lhs0, Rhs0), NewMul(Lhs1, Rhs1));
}

FExpressionOperation::FExpressionOperation(EOperation InOp, TConstArrayView<const FExpression*> InInputs) : Op(InOp)
{
	const FOperationDescription OpDesc = GetOperationDescription(InOp);
	check(OpDesc.NumInputs == InInputs.Num());
	check(InInputs.Num() <= MaxInputs);

	for (int32 i = 0; i < OpDesc.NumInputs; ++i)
	{
		Inputs[i] = InInputs[i];
		check(Inputs[i]);
	}
}

namespace Private
{
struct FOperationRequestedTypes
{
	FRequestedType InputType[FExpressionOperation::MaxInputs];
	bool bIsMatrixOperation = false;
};
struct FOperationTypes
{
	Shader::EValueType InputType[FExpressionOperation::MaxInputs];
	FPreparedType ResultType;
	bool bIsLWC = false;
};

FOperationRequestedTypes GetOperationRequestedTypes(EOperation Op, const FRequestedType& RequestedType)
{
	const FOperationDescription OpDesc = GetOperationDescription(Op);
	FOperationRequestedTypes Types;
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		Types.InputType[Index] = RequestedType;
	}
	switch (Op)
	{
	case EOperation::Length:
	case EOperation::Normalize:
	case EOperation::Sum:
		Types.InputType[0] = Shader::MakeValueType(RequestedType.GetValueComponentType(), 4);
		break;
	case EOperation::VecMulMatrix3:
		Types.bIsMatrixOperation = true;
		Types.InputType[0] = Shader::EValueType::Float3;
		Types.InputType[1] = Shader::EValueType::Float4x4;
		break;
	case EOperation::VecMulMatrix4:
		// float3 * FLWCMatrix -> FLWCVector3
		// Note that we'll never explicitly request the 'FLWCVector3 * FLWCInverseMatrix -> float3' case
		Types.bIsMatrixOperation = true;
		Types.InputType[0] = Shader::EValueType::Float3;
		Types.InputType[1] = Shader::MakeValueType(RequestedType.GetValueComponentType(), 16);
		break;
	case EOperation::Matrix3MulVec:
	case EOperation::Matrix4MulVec:
		// No LWC for transpose matrices
		Types.bIsMatrixOperation = true;
		Types.InputType[0] = Shader::EValueType::Float4x4;
		Types.InputType[1] = Shader::EValueType::Float3;
		break;
	case EOperation::Less:
	case EOperation::Greater:
	case EOperation::LessEqual:
	case EOperation::GreaterEqual:
		// inputs for comparisons are some numeric type
		Types.InputType[0] = Shader::EValueType::Numeric1;
		Types.InputType[1] = Shader::EValueType::Numeric1;
		break;
	default:
		break;
	}
	return Types;
}

FOperationTypes GetOperationTypes(EOperation Op, TConstArrayView<FPreparedType> InputPreparedType)
{
	FOperationTypes Types;
	if (Op == EOperation::VecMulMatrix3 ||
		Op == EOperation::VecMulMatrix4 ||
		Op == EOperation::Matrix3MulVec ||
		Op == EOperation::Matrix4MulVec)
	{
		FPreparedComponent IntermediateComponent;
		for (int32 Index = 0; Index < InputPreparedType.Num(); ++Index)
		{
			IntermediateComponent = CombineComponents(IntermediateComponent, InputPreparedType[Index].GetMergedComponent());
		}

		switch (Op)
		{
		case EOperation::VecMulMatrix3:
			// No LWC for matrix3
			Types.InputType[0] = Shader::EValueType::Float3;
			Types.InputType[1] = Shader::EValueType::Float4x4;
			Types.ResultType = FPreparedType(Shader::EValueType::Float3, IntermediateComponent);
			break;
		case EOperation::VecMulMatrix4:
			switch (InputPreparedType[1].Type.ValueType)
			{
			case Shader::EValueType::Double4x4:
				// float3 * FLWCMatrix -> FLWCVector3
				Types.InputType[0] = Shader::EValueType::Float3;
				Types.InputType[1] = Shader::EValueType::Double4x4;
				Types.ResultType = FPreparedType(Shader::EValueType::Double3, IntermediateComponent);
				Types.bIsLWC = true;
				break;
			case Shader::EValueType::DoubleInverse4x4:
				// FLWCVector3 * FLWCInverseMatrix -> float3
				Types.InputType[0] = Shader::EValueType::Double3;
				Types.InputType[1] = Shader::EValueType::DoubleInverse4x4;
				Types.ResultType = FPreparedType(Shader::EValueType::Float3, IntermediateComponent);
				Types.bIsLWC = true;
				break;
			default:
				// float3 * float4x4 -> float3
				Types.InputType[0] = Shader::EValueType::Float3;
				Types.InputType[1] = Shader::EValueType::Float4x4;
				Types.ResultType = FPreparedType(Shader::EValueType::Float3, IntermediateComponent);
				break;
			}
			break;
		case EOperation::Matrix3MulVec:
		case EOperation::Matrix4MulVec:
			// No LWC for transpose matrices
			Types.InputType[0] = Shader::EValueType::Float4x4;
			Types.InputType[1] = Shader::EValueType::Float3;
			Types.ResultType = FPreparedType(Shader::EValueType::Float3, IntermediateComponent);
			break;
		}
	}
	else
	{
		FPreparedType IntermediateType;
		for (int32 Index = 0; Index < InputPreparedType.Num(); ++Index)
		{
			IntermediateType = MergePreparedTypes(IntermediateType, InputPreparedType[Index]);
		}

		const Shader::EValueComponentType IntermediateComponentType = IntermediateType.GetValueComponentType();
		const int32 NumIntermediateComponents = IntermediateType.Type.GetNumComponents();
		for (int32 Index = 0; Index < InputPreparedType.Num(); ++Index)
		{
			Shader::EValueComponentType InputComponentType = InputPreparedType[Index].GetValueComponentType();
			if (InputComponentType != IntermediateComponentType)
			{
				switch (IntermediateComponentType)
				{
				case Shader::EValueComponentType::Float:
				case Shader::EValueComponentType::Double:
					// If the result type is either float or double, we promote input types to float
					// Nothing should promote to double, unless it's already at double precision
					InputComponentType = Shader::EValueComponentType::Float;
					break;
				default:
					// Otherwise use ints
					InputComponentType = Shader::EValueComponentType::Int;
					break;
				}
			}

			// No implicit conversion from FLWCScalar to FLWCVector types
			// Could potentially add overloads to LWCOperations.ush to allow this
			const bool bInputScalar = (InputPreparedType[Index].IsNumericScalar() && InputComponentType != Shader::EValueComponentType::Double);
			Types.InputType[Index] = Shader::MakeValueType(InputComponentType, bInputScalar ? 1 : NumIntermediateComponents);
		}
		Types.ResultType = IntermediateType;
		Types.bIsLWC = (IntermediateComponentType == Shader::EValueComponentType::Double);

		switch (Op)
		{
		case EOperation::Length:
		case EOperation::Sum:
			Types.ResultType = FPreparedType(Shader::MakeValueType(IntermediateComponentType, 1), IntermediateType.GetMergedComponent());
			break;
		case EOperation::Normalize:
		case EOperation::Rcp:
		case EOperation::Sqrt:
		case EOperation::Rsqrt:
		case EOperation::Sign:
		case EOperation::Tan:
		case EOperation::Asin:
		case EOperation::AsinFast:
		case EOperation::Acos:
		case EOperation::AcosFast:
		case EOperation::Atan:
		case EOperation::AtanFast:
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::Saturate:
		case EOperation::Frac:
		case EOperation::Step:
		case EOperation::SmoothStep:
			if (Types.bIsLWC)
			{
				// LWCSmoothStep requires all inputs have the same type
				for (int32 InputIndex = 0; InputIndex < InputPreparedType.Num(); ++InputIndex)
				{
					Types.InputType[InputIndex] = IntermediateType.Type.ValueType;
				}
			}
			for (int32 Index = 0; Index < Types.ResultType.PreparedComponents.Num(); ++Index)
			{
				Types.ResultType.SetComponentBounds(Index, Shader::FComponentBounds(Shader::EComponentBound::Zero, Shader::EComponentBound::One));
			}
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::Sin:
		case EOperation::Cos:
			for (int32 Index = 0; Index < Types.ResultType.PreparedComponents.Num(); ++Index)
			{
				Types.ResultType.SetComponentBounds(Index, Shader::FComponentBounds(Shader::EComponentBound::NegOne, Shader::EComponentBound::One));
			}
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::Log:
		case EOperation::Log2:
		case EOperation::Exp:
		case EOperation::Exp2:
			// No LWC support yet
			Types.InputType[0] = Shader::MakeNonLWCType(Types.InputType[0]);
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::Less:
		case EOperation::Greater:
		case EOperation::LessEqual:
		case EOperation::GreaterEqual:
			Types.ResultType.Type = Shader::MakeValueType(Shader::EValueComponentType::Bool, NumIntermediateComponents);
			break;
		case EOperation::Fmod:
			Types.InputType[1] = Shader::MakeNonLWCType(Types.InputType[1]);
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::PowPositiveClamped:
		case EOperation::Atan2:
		case EOperation::Atan2Fast:
			// No LWC support yet
			Types.InputType[0] = Shader::MakeNonLWCType(Types.InputType[0]);
			Types.InputType[1] = Shader::MakeNonLWCType(Types.InputType[1]);
			Types.ResultType = MakeNonLWCType(IntermediateType);
			break;
		case EOperation::Min:
			for (int32 Index = 0; Index < Types.ResultType.PreparedComponents.Num(); ++Index)
			{
				Types.ResultType.SetComponentBounds(Index, Shader::MinBound(InputPreparedType[0].GetComponentBounds(Index), InputPreparedType[1].GetComponentBounds(Index)));
			}
			break;
		case EOperation::Max:
			for (int32 Index = 0; Index < Types.ResultType.PreparedComponents.Num(); ++Index)
			{
				Types.ResultType.SetComponentBounds(Index, Shader::MaxBound(InputPreparedType[0].GetComponentBounds(Index), InputPreparedType[1].GetComponentBounds(Index)));
			}
			break;
		default:
			break;
		}
	}
	return Types;
}
} // namespace Private

void FExpressionOperation::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	// Operations with constant derivatives
	switch (Op)
	{
	case EOperation::Less:
	case EOperation::Greater:
	case EOperation::LessEqual:
	case EOperation::GreaterEqual:
	case EOperation::Floor:
	case EOperation::Ceil:
	case EOperation::Round:
	case EOperation::Trunc:
	case EOperation::Sign:
	case EOperation::Step:
		OutResult.ExpressionDdx = OutResult.ExpressionDdy = Tree.NewConstant(0.0f);
		break;
	default:
		break;
	}

	if (OutResult.IsValid())
	{
		return;
	}

	const FOperationDescription OpDesc = GetOperationDescription(Op);
	FExpressionDerivatives InputDerivatives[MaxInputs];
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		InputDerivatives[Index] = Tree.GetAnalyticDerivatives(Inputs[Index]);
		if (!InputDerivatives[Index].IsValid())
		{
			return;
		}
	}

	switch (Op)
	{
	case EOperation::Neg:
		OutResult.ExpressionDdx = Tree.NewNeg(InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewNeg(InputDerivatives[0].ExpressionDdy);
		break;
	case EOperation::Rcp:
	{
		const FExpression* Result = Tree.NewRcp(Inputs[0]);
		const FExpression* dFdA = Tree.NewNeg(Tree.NewMul(Result, Result));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Sqrt:
	{
		const FExpression* dFdA = Tree.NewMul(Tree.NewRsqrt(Tree.NewMax(Inputs[0], Tree.NewConstant(0.00001f))), Tree.NewConstant(0.5f));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Rsqrt:
	{
		const FExpression* dFdA = Tree.NewMul(Tree.NewMul(Tree.NewRsqrt(Inputs[0]), Tree.NewRcp(Inputs[0])), Tree.NewConstant(-0.5f));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Sum:
		OutResult.ExpressionDdx = Tree.NewSum(InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewSum(InputDerivatives[0].ExpressionDdy);
		break;
	case EOperation::Frac:
		OutResult = InputDerivatives[0];
		break;
	case EOperation::Sin:
	{
		const FExpression* dFdA = Tree.NewCos(Inputs[0]);
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Cos:
	{
		const FExpression* dFdA = Tree.NewNeg(Tree.NewSin(Inputs[0]));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Tan:
	{
		const FExpression* dFdA = Tree.NewRcp(Tree.NewPow2(Tree.NewCos(Inputs[0])));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Asin:
	case EOperation::AsinFast:
	{
		const FExpression* dFdA = Tree.NewRsqrt(Tree.NewMax(Tree.NewSub(Tree.NewConstant(1.0f), Tree.NewPow2(Inputs[0])), Tree.NewConstant(0.00001f)));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Acos:
	case EOperation::AcosFast:
	{
		const FExpression* dFdA = Tree.NewNeg(Tree.NewRsqrt(Tree.NewMax(Tree.NewSub(Tree.NewConstant(1.0f), Tree.NewPow2(Inputs[0])), Tree.NewConstant(0.00001f))));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Atan:
	case EOperation::AtanFast:
	{
		const FExpression* dFdA = Tree.NewRcp(Tree.NewAdd(Tree.NewPow2(Inputs[0]), Tree.NewConstant(1.0f)));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Atan2:
	case EOperation::Atan2Fast:
	{
		const FExpression* Denom = Tree.NewRcp(Tree.NewAdd(Tree.NewPow2(Inputs[0]), Tree.NewPow2(Inputs[1])));
		const FExpression* dFdA = Tree.NewMul(Inputs[1], Denom);
		const FExpression* dFdB = Tree.NewMul(Tree.NewNeg(Inputs[0]), Denom);
		OutResult.ExpressionDdx = Tree.NewAdd(Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx), Tree.NewMul(dFdB, InputDerivatives[1].ExpressionDdx));
		OutResult.ExpressionDdy = Tree.NewAdd(Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy), Tree.NewMul(dFdB, InputDerivatives[1].ExpressionDdy));
		break;
	}
	case EOperation::Abs:
	{
		const FExpression* ConstantZero = Tree.NewConstant(0.f);
		const FExpression* ConstantOne = Tree.NewConstant(1.f);
		const FExpression* Cond = Tree.NewGreaterEqual(Inputs[0], ConstantZero);
		const FExpression* Sign = Tree.NewExpression<FExpressionSelect>(Cond, ConstantOne, Tree.NewNeg(ConstantOne));
		OutResult.ExpressionDdx = Tree.NewMul(Sign, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(Sign, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Saturate:
	{
		const FExpression* ConstantZero = Tree.NewConstant(0.f);
		const FExpression* ConstantOne = Tree.NewConstant(1.f);
		const FExpression* Cond0 = Tree.NewGreater(Inputs[0], ConstantZero);
		const FExpression* Cond1 = Tree.NewLess(Inputs[0], ConstantOne);
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSelect>(Cond0, Tree.NewExpression<FExpressionSelect>(Cond1, InputDerivatives[0].ExpressionDdx, ConstantOne), ConstantZero);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSelect>(Cond0, Tree.NewExpression<FExpressionSelect>(Cond1, InputDerivatives[0].ExpressionDdy, ConstantOne), ConstantZero);
		break;
	}
	case EOperation::Log:
	{
		const FExpression* dFdA = Tree.NewRcp(Inputs[0]);
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Log2:
	{
		const FExpression* dFdA = Tree.NewMul(Tree.NewRcp(Inputs[0]), Tree.NewConstant(1.442695f));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Exp:
	{
		const FExpression* dFdA = Tree.NewExp(Inputs[0]);
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Exp2:
	{
		const FExpression* dFdA = Tree.NewMul(Tree.NewExp2(Inputs[0]), Tree.NewConstant(0.693147f));
		OutResult.ExpressionDdx = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy);
		break;
	}
	case EOperation::Length:
	{
		const FExpression* LengthExpression = Tree.NewSqrt(Tree.NewSum(Tree.NewMul(Inputs[0], Inputs[0])));
		LengthExpression->ComputeAnalyticDerivatives(Tree, OutResult);
		break;
	}
	case EOperation::Normalize:
	{
		const FExpression* NormalizeExpression = Tree.NewMul(Inputs[0], Tree.NewRsqrt(Tree.NewSum(Tree.NewMul(Inputs[0], Inputs[0]))));
		NormalizeExpression->ComputeAnalyticDerivatives(Tree, OutResult);
		break;
	}
	case EOperation::PowPositiveClamped:
	{
		const FExpression* ConstantZero = Tree.NewConstant(0.f);
		const FExpression* Cond = Tree.NewLess(ConstantZero, Inputs[1]); // should we check for A as well?
		const FExpression* F = Tree.NewPowClamped(Inputs[0], Inputs[1]);
		const FExpression* LnA = Tree.NewLog(Inputs[0]);
		const FExpression* BByA = Tree.NewDiv(Inputs[1], Inputs[0]);
		const FExpression* InRangeDdx = Tree.NewMul(F, Tree.NewAdd(Tree.NewMul(InputDerivatives[1].ExpressionDdx, LnA), Tree.NewMul(BByA, InputDerivatives[0].ExpressionDdx)));
		const FExpression* InRangeDdy = Tree.NewMul(F, Tree.NewAdd(Tree.NewMul(InputDerivatives[1].ExpressionDdy, LnA), Tree.NewMul(BByA, InputDerivatives[0].ExpressionDdy)));
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSelect>(Cond, InRangeDdx, ConstantZero);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSelect>(Cond, InRangeDdy, ConstantZero);
		break;
	}
	case EOperation::Add:
		OutResult.ExpressionDdx = Tree.NewAdd(InputDerivatives[0].ExpressionDdx, InputDerivatives[1].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewAdd(InputDerivatives[0].ExpressionDdy, InputDerivatives[1].ExpressionDdy);
		break;
	case EOperation::Sub:
		OutResult.ExpressionDdx = Tree.NewSub(InputDerivatives[0].ExpressionDdx, InputDerivatives[1].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewSub(InputDerivatives[0].ExpressionDdy, InputDerivatives[1].ExpressionDdy);
		break;
	case EOperation::Mul:
		OutResult.ExpressionDdx = Tree.NewAdd(Tree.NewMul(InputDerivatives[0].ExpressionDdx, Inputs[1]), Tree.NewMul(InputDerivatives[1].ExpressionDdx, Inputs[0]));
		OutResult.ExpressionDdy = Tree.NewAdd(Tree.NewMul(InputDerivatives[0].ExpressionDdy, Inputs[1]), Tree.NewMul(InputDerivatives[1].ExpressionDdy, Inputs[0]));
		break;
	case EOperation::Div:
	{
		const FExpression* Denom = Tree.NewRcp(Tree.NewMul(Inputs[1], Inputs[1]));
		const FExpression* dFdA = Tree.NewMul(Inputs[1], Denom);
		const FExpression* dFdB = Tree.NewNeg(Tree.NewMul(Inputs[0], Denom));
		OutResult.ExpressionDdx = Tree.NewAdd(Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdx), Tree.NewMul(dFdB, InputDerivatives[1].ExpressionDdx));
		OutResult.ExpressionDdy = Tree.NewAdd(Tree.NewMul(dFdA, InputDerivatives[0].ExpressionDdy), Tree.NewMul(dFdB, InputDerivatives[1].ExpressionDdy));
		break;
	}
	case EOperation::Fmod:
		// Only valid when B derivatives are zero.
		// We can't really do anything meaningful in the non-zero case.
		OutResult = InputDerivatives[0];
		break;
	case EOperation::Min:
	{
		const FExpression* Cond = Tree.NewLess(Inputs[0], Inputs[1]);
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSelect>(Cond, InputDerivatives[0].ExpressionDdx, InputDerivatives[1].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSelect>(Cond, InputDerivatives[0].ExpressionDdy, InputDerivatives[1].ExpressionDdy);
		break;
	}
	case EOperation::Max:
	{
		const FExpression* Cond = Tree.NewGreater(Inputs[0], Inputs[1]);
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSelect>(Cond, InputDerivatives[0].ExpressionDdx, InputDerivatives[1].ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSelect>(Cond, InputDerivatives[0].ExpressionDdy, InputDerivatives[1].ExpressionDdy);
		break;
	}
	case EOperation::VecMulMatrix3:
	case EOperation::VecMulMatrix4:
	case EOperation::Matrix3MulVec:
	case EOperation::Matrix4MulVec:
		// TODO
		OutResult.ExpressionDdx = OutResult.ExpressionDdy = Tree.NewConstant(FVector3f(0.0f, 0.0f, 0.0f));
		break;
	case EOperation::SmoothStep:
		// TODO
		break;
	default:
		checkNoEntry();
		break;
	}
}

const FExpression* FExpressionOperation::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	const Private::FOperationRequestedTypes RequestedTypes = Private::GetOperationRequestedTypes(Op, RequestedType);
	const FOperationDescription OpDesc = GetOperationDescription(Op);
	const FExpression* PrevFrameInputs[MaxInputs];
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		PrevFrameInputs[Index] = Tree.GetPreviousFrame(Inputs[Index], RequestedTypes.InputType[Index]);
	}

	return Tree.NewExpression<FExpressionOperation>(Op, MakeArrayView(PrevFrameInputs, OpDesc.NumInputs));
}

bool FExpressionOperation::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FOperationDescription OpDesc = GetOperationDescription(Op);
	const Private::FOperationRequestedTypes RequestedTypes = Private::GetOperationRequestedTypes(Op, RequestedType);

	FPreparedType InputPreparedType[MaxInputs];
	Shader::FValue ConstantInput[MaxInputs];
	bool bConstantZeroInput[MaxInputs] = { false };
	bool bMarkLiveInput[MaxInputs] = { false };
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		if (Context.bMarkLiveValues)
		{
			InputPreparedType[Index] = Context.GetPreparedType(Inputs[Index], RequestedTypes.InputType[Index]);
			bMarkLiveInput[Index] = true;
		}
		else
		{
			InputPreparedType[Index] = Context.PrepareExpression(Inputs[Index], Scope, RequestedTypes.InputType[Index]);
		}

		if (InputPreparedType[Index].IsVoid())
		{
			return false;
		}

		if (!InputPreparedType[Index].IsNumeric())
		{
			return Context.Error(TEXT("Invalid arithmetic between non-numeric types"));
		}
		
		const EExpressionEvaluation InputEvaluation = InputPreparedType[Index].GetEvaluation(Scope, RequestedType);
		if (IsConstantEvaluation(InputEvaluation))
		{
			ConstantInput[Index] = Inputs[Index]->GetValueConstant(Context, Scope, RequestedType, InputPreparedType[Index]);
			bConstantZeroInput[Index] = ConstantInput[Index].IsZero();
		}
	}

	Private::FOperationTypes Types = Private::GetOperationTypes(Op, MakeArrayView(InputPreparedType, OpDesc.NumInputs));
	if (OpDesc.PreshaderOpcode == Shader::EPreshaderOpcode::Nop)
	{
		// No preshader support
		Types.ResultType.SetEvaluation(EExpressionEvaluation::Shader);
	}

	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		Context.MarkInputType(Inputs[Index], Types.InputType[Index]);
	}

	switch (Op)
	{
	case EOperation::Mul:
		if (bConstantZeroInput[0] || bConstantZeroInput[1])
		{
			// X * 0 == 0
			Types.ResultType.SetEvaluation(EExpressionEvaluation::ConstantZero);
			// If one input is 0, we can avoid marking the other input as live (since we don't care what it is)
			// In the case 'both' inputs are 0, we still need to mark one of them live...in this case we could potentially somehow choose the simplest input to mark live, but for now just pick one
			if (bConstantZeroInput[0])
			{
				bMarkLiveInput[1] = false;
			}
			else
			{
				bMarkLiveInput[0] = false;
			}
		}
		break;
	default:
		break;
	}

	if (Context.bMarkLiveValues)
	{
		for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
		{
			if (bMarkLiveInput[Index])
			{
				Context.PrepareExpression(Inputs[Index], Scope, RequestedTypes.InputType[Index]);
			}
		}
	}

	return OutResult.SetType(Context, RequestedType, Types.ResultType);
}

void FExpressionOperation::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FOperationDescription OpDesc = GetOperationDescription(Op);
	const Private::FOperationRequestedTypes RequestedTypes = Private::GetOperationRequestedTypes(Op, RequestedType);

	FPreparedType InputPreparedType[MaxInputs];
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		InputPreparedType[Index] = Context.GetPreparedType(Inputs[Index], RequestedTypes.InputType[Index]);
	}

	const Private::FOperationTypes Types = Private::GetOperationTypes(Op, MakeArrayView(InputPreparedType, OpDesc.NumInputs));
	FEmitShaderExpression* InputValue[MaxInputs] = { nullptr };
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		InputValue[Index] = Inputs[Index]->GetValueShader(Context, Scope, RequestedTypes.InputType[Index], Types.InputType[Index]);
	}

	const Shader::EValueType ResultType = Types.ResultType.GetResultType();
	check(Shader::IsNumericType(ResultType));

	switch (Op)
	{
	// Unary Ops
	case EOperation::Abs: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAbs(%)") : TEXT("abs(%)"), InputValue[0]); break;
	case EOperation::Neg:
		if (Types.bIsLWC)
		{
			OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("WSNegate(%)"), InputValue[0]);
		}
		else
		{
			OutResult.Code = Context.EmitInlineExpression(Scope, ResultType, TEXT("(-%)"), InputValue[0]);
		}
		break;
	case EOperation::Rcp: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSRcpDemote(%)") : TEXT("rcp(%)"), InputValue[0]); break;
	case EOperation::Sqrt: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSqrtDemote(%)") : TEXT("sqrt(%)"), InputValue[0]); break;
	case EOperation::Rsqrt: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSRsqrtDemote(%)") : TEXT("rsqrt(%)"), InputValue[0]); break;
	case EOperation::Log: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("log(%)"), InputValue[0]); break;
	case EOperation::Log2: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("log2(%)"), InputValue[0]); break;
	case EOperation::Exp: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("exp(%)"), InputValue[0]); break;
	case EOperation::Exp2: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("exp2(%)"), InputValue[0]); break;
	case EOperation::Frac: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSFracDemote(%)") : TEXT("frac(%)"), InputValue[0]); break;
	case EOperation::Floor: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSFloor(%)") : TEXT("floor(%)"), InputValue[0]); break;
	case EOperation::Ceil: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSCeil(%)") : TEXT("ceil(%)"), InputValue[0]); break;
	case EOperation::Round: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSRound(%)") : TEXT("round(%)"), InputValue[0]); break;
	case EOperation::Trunc: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSTrunc(%)") : TEXT("trunc(%)"), InputValue[0]); break;
	case EOperation::Saturate: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSaturateDemote(%)") : TEXT("saturate(%)"), InputValue[0]); break;
	case EOperation::Sign: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSign(%)") : TEXT("sign(%)"), InputValue[0]); break;
	case EOperation::Length: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSLength(%)") : TEXT("length(%)"), InputValue[0]); break;
	case EOperation::Normalize: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSNormalizeDemote(%)") : TEXT("normalize(%)"), InputValue[0]); break;
	case EOperation::Sum: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSVectorSum(%)") : TEXT("VectorSum(%)"), InputValue[0]); break;
	case EOperation::Sin: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSin(%)") : TEXT("sin(%)"), InputValue[0]); break;
	case EOperation::Cos: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSCos(%)") : TEXT("cos(%)"), InputValue[0]); break;
	case EOperation::Tan: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSTan(%)") : TEXT("tan(%)"), InputValue[0]); break;
	case EOperation::Asin: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAsin(%)") : TEXT("asin(%)"), InputValue[0]); break;
	case EOperation::AsinFast: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAsin(%)") : TEXT("asinFast(%)"), InputValue[0]); break;
	case EOperation::Acos: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAcos(%)") : TEXT("acos(%)"), InputValue[0]); break;
	case EOperation::AcosFast: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAcos(%)") : TEXT("acosFast(%)"), InputValue[0]); break;
	case EOperation::Atan: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAtan(%)") : TEXT("atan(%)"), InputValue[0]); break;
	case EOperation::AtanFast: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAtan(%)") : TEXT("atanFast(%)"), InputValue[0]); break;
	
	// Binary Ops
	case EOperation::Add: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSAdd(%, %)") : TEXT("(% + %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Sub: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSubtract(%, %)") : TEXT("(% - %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Mul: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSMultiply(%, %)") : TEXT("(% * %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Div: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSDivide(%, %)") : TEXT("(% / %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Fmod: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSFmodDemote(%, %)") : TEXT("fmod(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Step: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSStep(%, %)") : TEXT("step(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::PowPositiveClamped: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("PositiveClampedPow(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Atan2: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("atan2(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Atan2Fast: OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("atan2Fast(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Min: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSMin(%, %)") : TEXT("min(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Max: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSMax(%, %)") : TEXT("max(%, %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Less: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSLess(%, %)") : TEXT("(% < %)"), InputValue[0], InputValue[1]); break;
	case EOperation::Greater: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSGreater(%, %)") : TEXT("(% > %)"), InputValue[0], InputValue[1]); break;
	case EOperation::LessEqual: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSLessEqual(%, %)") : TEXT("(% <= %)"), InputValue[0], InputValue[1]); break;
	case EOperation::GreaterEqual: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSGreaterEqual(%, %)") : TEXT("(% >= %)"), InputValue[0], InputValue[1]); break;
	case EOperation::VecMulMatrix3:
		if (Types.bIsLWC)
		{
			if (RequestedType.GetValueComponentType() == Shader::EValueComponentType::Double)
			{
				OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("WSMultiply(%, %)"), InputValue[0], InputValue[1]);
			}
			else
			{
				OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("WSMultiplyDemote(%, %)"), InputValue[0], InputValue[1]);
			}
		}
		else
		{
			OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("mul(%, (float3x3)%)"), InputValue[0], InputValue[1]);
		}
		break;
	case EOperation::VecMulMatrix4:
		if (Types.bIsLWC)
		{
			if (RequestedType.GetValueComponentType() == Shader::EValueComponentType::Double)
			{
				OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("WSMultiply(%, %)"), InputValue[0], InputValue[1]);
			}
			else
			{
				OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("WSMultiplyDemote(%, %)"), InputValue[0], InputValue[1]);
			}
		}
		else
		{
			// Append 1.f because VecMulMatrix4 implies position but we request float3 for the vector
			OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("mul(float4(%, 1.f), %).xyz"), InputValue[0], InputValue[1]);
		}
		break;
	case EOperation::Matrix3MulVec:
		OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("mul((float3x3)%, %)"), InputValue[0], InputValue[1]);
		break;
	case EOperation::Matrix4MulVec:
		OutResult.Code = Context.EmitExpression(Scope, ResultType, TEXT("mul(%, %)"), InputValue[0], InputValue[1]);
		break;

	// Ternary Ops
	case EOperation::SmoothStep: OutResult.Code = Context.EmitExpression(Scope, ResultType, Types.bIsLWC ? TEXT("WSSmoothStep(%, %, %)") : TEXT("smoothstep(%, %, %)"), InputValue[0], InputValue[1], InputValue[2]); break;

	default:
		checkNoEntry();
		break;
	}
}

void FExpressionOperation::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const FOperationDescription OpDesc = GetOperationDescription(Op);
	const Private::FOperationRequestedTypes RequestedTypes = Private::GetOperationRequestedTypes(Op, RequestedType);

	FPreparedType InputPreparedType[MaxInputs];
	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		InputPreparedType[Index] = Context.GetPreparedType(Inputs[Index], RequestedTypes.InputType[Index]);
	}

	const Private::FOperationTypes Types = Private::GetOperationTypes(Op, MakeArrayView(InputPreparedType, OpDesc.NumInputs));
	check(OpDesc.PreshaderOpcode != Shader::EPreshaderOpcode::Nop);

	for (int32 Index = 0; Index < OpDesc.NumInputs; ++Index)
	{
		Inputs[Index]->GetValuePreshader(Context, Scope, RequestedTypes.InputType[Index], OutResult.Preshader);
	}

	const int32 NumInputsToPop = OpDesc.NumInputs - 1;
	if (NumInputsToPop > 0)
	{
		check(Context.PreshaderStackPosition >= NumInputsToPop);
		Context.PreshaderStackPosition -= NumInputsToPop;
	}

	OutResult.Preshader.WriteOpcode(OpDesc.PreshaderOpcode);
	OutResult.Type = Types.ResultType.GetResultType();
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
