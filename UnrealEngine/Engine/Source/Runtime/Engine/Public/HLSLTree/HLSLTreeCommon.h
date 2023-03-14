// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "HLSLTree/HLSLTree.h"
#include "RHIDefinitions.h"

namespace UE::HLSLTree
{

class FExpressionError : public FExpression
{
public:
	explicit FExpressionError(FStringView InErrorMessage)
		: ErrorMessage(InErrorMessage)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;

	FStringView ErrorMessage;
};

/**
 * Forwards all calls to the owned expression
 * Intended to be used as a baseclass, where derived classes may hook certain method overrides
 */
class FExpressionForward : public FExpression
{
public:
	explicit FExpressionForward(const FExpression* InExpression) : Expression(InExpression) {}

	const FExpression* Expression;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionConstant : public FExpression
{
public:
	explicit FExpressionConstant(const Shader::FValue& InValue)
		: Value(InValue)
	{}

	Shader::FValue Value;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

/** Forwards to another expression, but will fallback to the 'DefaultValue' constant if the forwarded expression fails */
class FExpressionDefaultValue : public FExpressionForward
{
public:
	explicit FExpressionDefaultValue(const FExpression* InExpression, const Shader::FValue& InDefaultValue)
		: FExpressionForward(InExpression)
		, DefaultValue(InDefaultValue)
	{}

	Shader::FValue DefaultValue;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionGetStructField : public FExpression
{
public:
	FExpressionGetStructField(const Shader::FStructType* InStructType, const Shader::FStructField* InField, const FExpression* InStructExpression)
		: StructType(InStructType)
		, Field(InField)
		, StructExpression(InStructExpression)
	{
	}

	const Shader::FStructType* StructType;
	const Shader::FStructField* Field;
	const FExpression* StructExpression;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionSetStructField : public FExpression
{
public:
	FExpressionSetStructField(const Shader::FStructType* InStructType, const Shader::FStructField* InField, const FExpression* InStructExpression, const FExpression* InFieldExpression)
		: StructType(InStructType)
		, Field(InField)
		, StructExpression(InStructExpression)
		, FieldExpression(InFieldExpression)
	{
		check(InStructType);
		check(InField);
	}

	const Shader::FStructType* StructType;
	const Shader::FStructField* Field;
	const FExpression* StructExpression;
	const FExpression* FieldExpression;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionSelect : public FExpression
{
public:
	FExpressionSelect(const FExpression* InCondition, const FExpression* InTrue, const FExpression* InFalse)
		: ConditionExpression(InCondition)
		, TrueExpression(InTrue)
		, FalseExpression(InFalse)
	{
		check(ConditionExpression);
	}

	const FExpression* ConditionExpression;
	const FExpression* TrueExpression;
	const FExpression* FalseExpression;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionDerivative : public FExpression
{
public:
	FExpressionDerivative(EDerivativeCoordinate InCoord, const FExpression* InInput) : Input(InInput), Coord(InCoord) {}

	const FExpression* Input;
	EDerivativeCoordinate Coord;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

FSwizzleParameters MakeSwizzleMask(bool bInR, bool bInG, bool bInB, bool bInA);

class FExpressionSwizzle : public FExpression
{
public:
	FExpressionSwizzle(const FSwizzleParameters& InParams, const FExpression* InInput)
		: Parameters(InParams)
		, Input(InInput)
	{}

	FSwizzleParameters Parameters;
	const FExpression* Input;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

/**
 * Similar to FExpressionSwizzle, except swizzle parameters are extracted from a 'Mask' expression, which is expected to generate a 'bool4' component mask (which is then used to initialize a FSwizzleParameters)
 * The 'Mask' input is required to be constant (otherwise an error is generated)
 * This is used to support material StaticParameterMask expressions, in most cases FExpressionSwizzle is a better choice
 */
class FExpressionComponentMask : public FExpression
{
public:
	FExpressionComponentMask(const FExpression* InInput, const FExpression* InMask)
		: Input(InInput)
		, Mask(InMask)
	{}

	const FExpression* Input;
	const FExpression* Mask;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionAppend : public FExpression
{
public:
	FExpressionAppend(const FExpression* InLhs, const FExpression* InRhs)
		: Lhs(InLhs)
		, Rhs(InRhs)
	{}

	const FExpression* Lhs;
	const FExpression* Rhs;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionSwitchBase : public FExpression
{
public:
	static constexpr int8 MaxInputs = 8;

	FExpressionSwitchBase(TConstArrayView<const FExpression*> InInputs) : NumInputs(static_cast<int8>(InInputs.Num()))
	{
		check(InInputs.Num() <= MaxInputs);
		for (int32 i = 0; i < InInputs.Num(); ++i)
		{
			Input[i] = InInputs[i];
		}
	}

	const FExpression* Input[MaxInputs] = { nullptr };
	int8 NumInputs = 0;

	virtual const FExpression* NewSwitch(FTree& Tree, TConstArrayView<const FExpression*> InInputs) const = 0;
	virtual bool IsInputActive(const FEmitContext& Context, int32 Index) const = 0;

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
};

class FExpressionFeatureLevelSwitch : public FExpressionSwitchBase
{
public:
	static_assert(MaxInputs >= (int32)ERHIFeatureLevel::Num, "FExpressionSwitchBase is too small for FExpressionFeatureLevelSwitch");
	FExpressionFeatureLevelSwitch(TConstArrayView<const FExpression*> InInputs) : FExpressionSwitchBase(InInputs)
	{
		check(InInputs.Num() == (int32)ERHIFeatureLevel::Num);
	}

	virtual const FExpression* NewSwitch(FTree& Tree, TConstArrayView<const FExpression*> InInputs) const override { return Tree.NewExpression<FExpressionFeatureLevelSwitch>(InInputs); }
	virtual bool IsInputActive(const FEmitContext& Context, int32 Index) const override;
};

class FExpressionShadingPathSwitch : public FExpressionSwitchBase
{
public:
	static_assert(MaxInputs >= (int32)ERHIShadingPath::Num, "FExpressionSwitchBase is too small for FExpressionShadingPathSwitch");
	FExpressionShadingPathSwitch(TConstArrayView<const FExpression*> InInputs) : FExpressionSwitchBase(InInputs)
	{
		check(InInputs.Num() == (int32)ERHIShadingPath::Num);
	}

	virtual const FExpression* NewSwitch(FTree& Tree, TConstArrayView<const FExpression*> InInputs) const override { return Tree.NewExpression<FExpressionShadingPathSwitch>(InInputs); }
	virtual bool IsInputActive(const FEmitContext& Context, int32 Index) const override;
};

/** Can be used to emit HLSL chunks with no inputs, where it's not worth the trouble of defining a new expression type */
class FExpressionInlineCustomHLSL : public FExpression
{
public:
	FExpressionInlineCustomHLSL(Shader::EValueType InType, FStringView InCode) : Code(InCode), ResultType(InType) {}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;

	FStringView Code;
	Shader::EValueType ResultType;
};

class FExpressionCustomHLSL : public FExpression
{
public:
	FExpressionCustomHLSL(FStringView InDeclarationCode, FStringView InFunctionCode, TArrayView<FCustomHLSLInput> InInputs, const Shader::FStructType* InOutputStructType)
		: DeclarationCode(InDeclarationCode)
		, FunctionCode(InFunctionCode)
		, Inputs(InInputs)
		, OutputStructType(InOutputStructType)
	{}

	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;

	FStringView DeclarationCode;
	FStringView FunctionCode;
	TArray<FCustomHLSLInput, TInlineAllocator<8>> Inputs;
	const Shader::FStructType* OutputStructType = nullptr;
};

class FStatementError : public FStatement
{
public:
	explicit FStatementError(FStringView InErrorMessage)
		: ErrorMessage(InErrorMessage)
	{}

	virtual bool Prepare(FEmitContext& Context, FEmitScope& Scope) const override;

	FStringView ErrorMessage;
};

class FStatementBreak : public FStatement
{
public:
	virtual bool Prepare(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitShader(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const override;
};

class FStatementIf : public FStatement
{
public:
	const FExpression* ConditionExpression;
	FScope* ThenScope;
	FScope* ElseScope;
	FScope* NextScope;

	virtual bool Prepare(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitShader(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const override;
};

class FStatementLoop : public FStatement
{
public:
	FStatement* BreakStatement;
	FScope* LoopScope;
	FScope* NextScope;

	virtual bool IsLoop() const override { return true; }
	virtual bool Prepare(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitShader(FEmitContext& Context, FEmitScope& Scope) const override;
	virtual void EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const override;
};

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
