// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "Misc/EnumClassFlags.h"
#include "Containers/BitArray.h"
#include "Shader/ShaderTypes.h"

class UTexture;

namespace UE::Shader
{
enum class EPreshaderOpcode : uint8;
}

namespace UE::HLSLTree
{

class FNode;
class FStructType;
class FExpression;
class FStatement;
class FScope;
class FTree;

/**
 * Describes how a given expression needs to be evaluated */
enum class EExpressionEvaluation : uint8
{
	/** Invalid/uninitialized */
	None,

	/** Valid, but not yet known */
	Unknown,

	/** The expression outputs HLSL code (via FExpressionEmitResult::Writer) */
	Shader,

	PreshaderLoop,

	/** The expression outputs preshader code evaluated at runtime (via FExpressionEmitResult::Preshader) */
	Preshader,

	ConstantLoop,

	/** The expression outputs constant preshader code evaluated at compile time (via FExpressionEmitResult::Preshader) */
	Constant,

	/** The expression evaluates to 0 */
	ConstantZero,
};

EExpressionEvaluation CombineEvaluations(EExpressionEvaluation Lhs, EExpressionEvaluation Rhs);
EExpressionEvaluation MakeLoopEvaluation(EExpressionEvaluation Evaluation);
EExpressionEvaluation MakeNonLoopEvaluation(EExpressionEvaluation Evaluation);

inline bool IsConstantEvaluation(EExpressionEvaluation Evaluation)
{
	return Evaluation == EExpressionEvaluation::Constant || Evaluation == EExpressionEvaluation::ConstantZero;
}

inline bool IsLoopEvaluation(EExpressionEvaluation Evaluation)
{
	return Evaluation == EExpressionEvaluation::PreshaderLoop || Evaluation == EExpressionEvaluation::ConstantLoop;
}

inline bool IsRequestedEvaluation(EExpressionEvaluation Evaluation)
{
	return Evaluation != EExpressionEvaluation::None && Evaluation != EExpressionEvaluation::ConstantZero;
}

enum class EOperation : uint8
{
	None,

	// Unary Ops
	Abs,
	Neg,
	Rcp,
	Sqrt,
	Rsqrt,
	Log,
	Log2,
	Exp,
	Exp2,
	Frac,
	Floor,
	Ceil,
	Round,
	Trunc,
	Saturate,
	Sign,
	Length,
	Normalize,
	Sum,
	Sin,
	Cos,
	Tan,
	Asin,
	AsinFast,
	Acos,
	AcosFast,
	Atan,
	AtanFast,

	TruncateLWC,

	// Binary Ops
	Add,
	Sub,
	Mul,
	Div,
	Fmod,
	Step,
	PowPositiveClamped,
	Atan2,
	Atan2Fast,
	Min,
	Max,
	Less,
	Greater,
	LessEqual,
	GreaterEqual,

	VecMulMatrix3,
	VecMulMatrix4,
	Matrix3MulVec,
	Matrix4MulVec,

	// Ternary Ops
	SmoothStep,
};

struct FOperationDescription
{
	FOperationDescription();
	FOperationDescription(const TCHAR* InName, const TCHAR* InOperator, int8 InNumInputs, Shader::EPreshaderOpcode InOpcode);

	const TCHAR* Name;
	const TCHAR* Operator;
	int8 NumInputs;
	Shader::EPreshaderOpcode PreshaderOpcode;
};

FOperationDescription GetOperationDescription(EOperation Op);

struct FSwizzleParameters
{
	FSwizzleParameters() : NumComponents(0), bHasSwizzle(false) { SwizzleComponentIndex[0] = SwizzleComponentIndex[1] = SwizzleComponentIndex[2] = SwizzleComponentIndex[3] = INDEX_NONE; }
	ENGINE_API explicit FSwizzleParameters(int8 IndexR, int8 IndexG = INDEX_NONE, int8 IndexB = INDEX_NONE, int8 IndexA = INDEX_NONE);

	inline int32 GetSwizzleComponentIndex(int32 Index) const
	{
		const int32 ComponentIndex = (NumComponents == 1) ? 0 : Index;
		return SwizzleComponentIndex[ComponentIndex];
	}

	inline int32 GetNumInputComponents() const
	{
		int32 MaxComponentIndex = INDEX_NONE;
		for (int32 i = 0; i < NumComponents; ++i)
		{
			MaxComponentIndex = FMath::Max<int32>(MaxComponentIndex, SwizzleComponentIndex[i]);
		}
		return (MaxComponentIndex != INDEX_NONE) ? (MaxComponentIndex + 1) : 0;
	}

	int8 SwizzleComponentIndex[4];
	int8 NumComponents;
	bool bHasSwizzle;
};

struct FCustomHLSLInput
{
	FCustomHLSLInput() = default;
	FCustomHLSLInput(FStringView InName, const FExpression* InExpression) : Name(InName), Expression(InExpression) {}

	FStringView Name;
	const FExpression* Expression = nullptr;
};

typedef TArray<const Shader::FStructField*> FActiveStructFieldStack;

class FScopedActiveStructField
{
	FActiveStructFieldStack& Stack;

public:
	FScopedActiveStructField(FActiveStructFieldStack& InStack, const Shader::FStructField* Field)
		: Stack(InStack)
	{
		Stack.Push(Field);
	}

	~FScopedActiveStructField()
	{
		Stack.Pop(EAllowShrinking::No);
	}
};

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
