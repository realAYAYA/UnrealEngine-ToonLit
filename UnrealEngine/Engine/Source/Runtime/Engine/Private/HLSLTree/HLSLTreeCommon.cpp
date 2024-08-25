// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "HLSLTree/HLSLTreeCommon.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "RenderUtils.h"
#include "Shader/Preshader.h"
#include "SceneTypes.h"
#include "MaterialShared.h"
#include "MaterialHLSLTree.h"
#include "MaterialCachedData.h"
#include "Materials/MaterialInterface.h"
#include "DataDrivenShaderPlatformInfo.h"

namespace UE::HLSLTree
{

struct FPreshaderLoopScope
{
	const FStatement* BreakStatement = nullptr;
	Shader::FPreshaderLabel BreakLabel;
};

FSwizzleParameters::FSwizzleParameters(int8 InR, int8 InG, int8 InB, int8 InA) : NumComponents(0), bHasSwizzle(true)
{
	SwizzleComponentIndex[0] = InR;
	SwizzleComponentIndex[1] = InG;
	SwizzleComponentIndex[2] = InB;
	SwizzleComponentIndex[3] = InA;

	if (InA >= 0)
	{
		check(InA <= 3);
		++NumComponents;
		check(InB >= 0);
	}
	if (InB >= 0)
	{
		check(InB <= 3);
		++NumComponents;
		check(InG >= 0);
	}

	if (InG >= 0)
	{
		check(InG <= 3);
		++NumComponents;
		check(InR >= 0);
	}

	if (InR >= 0)
	{
		check(InR <= 3);
		++NumComponents;
	}
}

FSwizzleParameters MakeSwizzleMask(bool bInR, bool bInG, bool bInB, bool bInA)
{
	int8 ComponentIndex[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
	int8 CurrentComponent = 0;
	if (bInR)
	{
		ComponentIndex[CurrentComponent++] = 0;
	}
	if (bInG)
	{
		ComponentIndex[CurrentComponent++] = 1;
	}
	if (bInB)
	{
		ComponentIndex[CurrentComponent++] = 2;
	}
	if (bInA)
	{
		ComponentIndex[CurrentComponent++] = 3;
	}
	return FSwizzleParameters(ComponentIndex[0], ComponentIndex[1], ComponentIndex[2], ComponentIndex[3]);
}

bool FExpressionError::PrepareValue(FEmitContext& Context, FEmitScope&, const FRequestedType&, FPrepareValueResult&) const
{
	return Context.Error(ErrorMessage);
}

void FExpressionForward::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	Expression->ComputeAnalyticDerivatives(Tree, OutResult);
}

const FExpression* FExpressionForward::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Expression->ComputePreviousFrame(Tree, RequestedType);
}

bool FExpressionForward::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& PreparedType = Context.PrepareExpression(Expression, Scope, RequestedType);
	return OutResult.SetType(Context, RequestedType, PreparedType);
}

void FExpressionForward::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	Expression->EmitValueShader(Context, Scope, RequestedType, OutResult);
}

void FExpressionForward::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	Expression->EmitValuePreshader(Context, Scope, RequestedType, OutResult);
}

bool FExpressionForward::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	return Expression->EmitValueObject(Context, Scope, ObjectTypeName, OutObjectBase);
}

const FExpression* FExpressionPreviousFrameSwitch::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return PreviousFrameExpression->ComputePreviousFrame(Tree, RequestedType);
}

const FExpression* FTree::NewConstant(const Shader::FValue& Value)
{
	return NewExpression<FExpressionConstant>(Value);
}

void FExpressionConstant::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const Shader::FType DerivativeType = Value.Type.GetDerivativeType();
	if (!DerivativeType.IsVoid())
	{
		const Shader::FValue ZeroValue(DerivativeType);
		OutResult.ExpressionDdx = Tree.NewConstant(ZeroValue);
		OutResult.ExpressionDdy = OutResult.ExpressionDdx;
	}
}

namespace Private
{
FPreparedType PrepareConstant(const Shader::FValue& Value)
{
	const int32 NumComponents = Value.Type.GetNumComponents();
	FPreparedType ResultType(Value.Type);
	ResultType.PreparedComponents.Reserve(NumComponents);
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		const Shader::FValueComponent Component = Value.GetComponent(Index);
		ResultType.PreparedComponents.Add(Component.Packed ? EExpressionEvaluation::Constant : EExpressionEvaluation::ConstantZero);
	}
	return ResultType;
}
} // namespace Private

bool FExpressionConstant::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	return OutResult.SetType(Context, RequestedType, Private::PrepareConstant(Value));
}

void FExpressionConstant::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	Context.PreshaderStackPosition++;
	OutResult.Type = Value.Type;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(Value);
}

void FExpressionDefaultValue::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const Shader::FType DerivativeType = DefaultValue.Type.GetDerivativeType();
	if (!DerivativeType.IsVoid())
	{
		const FExpressionDerivatives ExpressionDerivative = Tree.GetAnalyticDerivatives(Expression);
		if (ExpressionDerivative.IsValid())
		{
			OutResult.ExpressionDdx = Tree.NewExpression<FExpressionDefaultValue>(ExpressionDerivative.ExpressionDdx, Shader::FValue(DerivativeType));
			OutResult.ExpressionDdy = Tree.NewExpression<FExpressionDefaultValue>(ExpressionDerivative.ExpressionDdy, Shader::FValue(DerivativeType));
		}
	}
}

const FExpression* FExpressionDefaultValue::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionDefaultValue>(Tree.GetPreviousFrame(Expression, RequestedType), DefaultValue);
}

bool FExpressionDefaultValue::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FPreparedType ResultType = Context.PrepareExpression(Expression, Scope, RequestedType);
	if (ResultType.IsVoid())
	{
		ResultType = Private::PrepareConstant(DefaultValue);
	}
	return OutResult.SetType(Context, RequestedType, ResultType);
}

void FExpressionDefaultValue::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(Expression, RequestedType);
	if (!PreparedType.IsVoid())
	{
		// If 'Exrpession' is valid, use that
		return FExpressionForward::EmitValuePreshader(Context, Scope, RequestedType, OutResult);
	}

	// Otherwise emit the default value
	Context.PreshaderStackPosition++;
	OutResult.Type = DefaultValue.Type;
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(DefaultValue);
}

void FExpressionGetStructField::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives StructDerivatives = Tree.GetAnalyticDerivatives(StructExpression);
	if (StructDerivatives.IsValid())
	{
		const Shader::FStructType* DerivativeStructType = StructType->DerivativeType;
		check(DerivativeStructType);
		const Shader::FStructField* DerivativeField = DerivativeStructType->FindFieldByName(Field->Name);
		check(DerivativeField);

		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionGetStructField>(DerivativeStructType, DerivativeField, StructDerivatives.ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionGetStructField>(DerivativeStructType, DerivativeField, StructDerivatives.ExpressionDdy);
	}
}

const FExpression* FExpressionGetStructField::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	FRequestedType RequestedStructType(StructType, false);
	RequestedStructType.SetFieldRequested(Field, RequestedType);
	return Tree.NewExpression<FExpressionGetStructField>(StructType, Field, Tree.GetPreviousFrame(StructExpression, RequestedStructType));
}

bool FExpressionGetStructField::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FRequestedType RequestedStructType(StructType, false);
	RequestedStructType.SetFieldRequested(Field, RequestedType);

	FPreparedType StructPreparedType = Context.PrepareExpression(StructExpression, Scope, RequestedStructType);
	if (!StructPreparedType.IsVoid() && StructPreparedType.Type.StructType != StructType)
	{
		return Context.Errorf(TEXT("Expected type %s"), StructType->Name);
	}

	return OutResult.SetType(Context, RequestedType, StructPreparedType.GetFieldType(Field));
}

void FExpressionGetStructField::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FRequestedType RequestedStructType(StructType, false);
	RequestedStructType.SetFieldRequested(Field, RequestedType);

	FEmitShaderExpression* StructValue = StructExpression->GetValueShader(Context, Scope, RequestedStructType);

	OutResult.Code = Context.EmitInlineExpression(Scope, Field->Type, TEXT("%.%"),
		StructValue,
		Field->Name);
}

void FExpressionGetStructField::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	FRequestedType RequestedStructType(StructType, false);
	RequestedStructType.SetFieldRequested(Field, RequestedType);

	StructExpression->GetValuePreshader(Context, Scope, RequestedStructType, OutResult.Preshader);
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::GetField).Write(Field->Type).Write(Field->ComponentIndex);
	OutResult.Type = Field->Type;
}

void FExpressionSetStructField::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives StructDerivatives = Tree.GetAnalyticDerivatives(StructExpression);
	const FExpressionDerivatives FieldDerivatives = Tree.GetAnalyticDerivatives(FieldExpression);

	if (StructDerivatives.IsValid() && FieldDerivatives.IsValid())
	{
		const Shader::FStructType* DerivativeStructType = StructType->DerivativeType;
		check(DerivativeStructType);
		const Shader::FStructField* DerivativeField = DerivativeStructType->FindFieldByName(Field->Name);
		check(DerivativeField);

		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSetStructField>(DerivativeStructType, DerivativeField, StructDerivatives.ExpressionDdx, FieldDerivatives.ExpressionDdx, TestMaterialProperty);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSetStructField>(DerivativeStructType, DerivativeField, StructDerivatives.ExpressionDdy, FieldDerivatives.ExpressionDdy, TestMaterialProperty);
	}
}

const FExpression* FExpressionSetStructField::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	FRequestedType RequestedStructType = MakeRequestedStructType(Tree.ActiveStructFieldStack, RequestedType);
	const FExpression* PrevStructExpression = Tree.GetPreviousFrame(StructExpression, RequestedStructType);

	const FRequestedType RequestedFieldType = MakeRequestedFieldType(Tree.ActiveStructFieldStack, RequestedType);
	if (RequestedFieldType.IsEmpty())
	{
		// Ignore the field subtree if not requested
		return PrevStructExpression;
	}

	const FExpression* PrevFieldExpression;
	{
		FScopedActiveStructField ScopedActiveField(Tree.ActiveStructFieldStack, Field);
		PrevFieldExpression = Tree.GetPreviousFrame(FieldExpression, RequestedFieldType);
	}

	return Tree.NewExpression<FExpressionSetStructField>(StructType, Field, PrevStructExpression, PrevFieldExpression, TestMaterialProperty);
}

bool FExpressionSetStructField::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	if (!IsStructFieldActive(Context))
	{
		// The struct field isn't active so don't touch it and just forward
		const FPreparedType& PreparedType = Context.PrepareExpression(StructExpression, Scope, RequestedType);
		return OutResult.SetType(Context, RequestedType, PreparedType);
	}

	FRequestedType RequestedStructType = MakeRequestedStructType(Context.ActiveStructFieldStack, RequestedType);

	const FPreparedType StructPreparedType = Context.PrepareExpression(StructExpression, Scope, RequestedStructType);
	if (!StructPreparedType.IsVoid() && StructPreparedType.Type.StructType != StructType)
	{
		return Context.Errorf(TEXT("Expected type %s"), StructType->Name);
	}

	FPreparedType ResultType(StructPreparedType);
	if (ResultType.IsVoid())
	{
		ResultType = StructType;
	}

	const FRequestedType RequestedFieldType = MakeRequestedFieldType(Context.ActiveStructFieldStack, RequestedType);
	if (!RequestedFieldType.IsEmpty())
	{
		FScopedActiveStructField ActiveFieldScope(Context.ActiveStructFieldStack, Field);

		const FPreparedType FieldPreparedType = Context.PrepareExpression(FieldExpression, Scope, RequestedFieldType);
		ResultType.SetField(Field, FieldPreparedType);
	}

	if (!RequestedType.IsStruct())
	{
		const Shader::FStructField* ActiveField = Context.ActiveStructFieldStack.Last();
		ResultType = ResultType.GetFieldType(ActiveField);
	}

	return OutResult.SetType(Context, RequestedType, ResultType);
}

void FExpressionSetStructField::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	if (!IsStructFieldActive(Context))
	{
		StructExpression->EmitValueShader(Context, Scope, RequestedType, OutResult);
		return;
	}

	FRequestedType RequestedStructType = MakeRequestedStructType(Context.ActiveStructFieldStack, RequestedType);
	const EExpressionEvaluation StructEvaluation = Context.GetEvaluation(StructExpression, Scope, RequestedStructType);
	check(StructEvaluation != EExpressionEvaluation::None);

	const FRequestedType RequestedFieldType = MakeRequestedFieldType(Context.ActiveStructFieldStack, RequestedType);
	const EExpressionEvaluation FieldEvaluation = Context.GetEvaluation(FieldExpression, Scope, RequestedFieldType);
	check(FieldEvaluation != EExpressionEvaluation::None);

	if (StructEvaluation == EExpressionEvaluation::ConstantZero && FieldEvaluation == EExpressionEvaluation::ConstantZero)
	{
		OutResult.Code = Context.EmitConstantZero(Scope, StructType);
	}
	else
	{
		FEmitShaderExpression* StructValue = StructExpression->GetValueShader(Context, Scope, RequestedStructType, StructType);

		// Do not emit code to set the field if not requested
		if (RequestedFieldType.IsEmpty())
		{
			OutResult.Code = StructValue;
		}
		else
		{
			FScopedActiveStructField ActiveFieldScope(Context.ActiveStructFieldStack, Field);

			FEmitShaderExpression* FieldValue = FieldExpression->GetValueShader(Context, Scope, RequestedFieldType, Field->Type);
			OutResult.Code = Context.EmitExpression(Scope, StructType, TEXT("%_Set%(%, %)"),
				StructType->Name,
				Field->Name,
				StructValue,
				FieldValue);
		}
	}

	if (!RequestedType.IsStruct())
	{
		const Shader::FStructField* ActiveField = Context.ActiveStructFieldStack.Last();
		OutResult.Code = Context.EmitInlineExpression(Scope, ActiveField->Type, TEXT("%.%"), OutResult.Code, ActiveField->Name);
	}
}

void FExpressionSetStructField::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	if (!IsStructFieldActive(Context))
	{
		StructExpression->EmitValuePreshader(Context, Scope, RequestedType, OutResult);
		return;
	}
	
	FRequestedType RequestedStructType = MakeRequestedStructType(Context.ActiveStructFieldStack, RequestedType);
	const EExpressionEvaluation StructEvaluation = Context.GetEvaluation(StructExpression, Scope, RequestedStructType);

	const FRequestedType RequestedFieldType = MakeRequestedFieldType(Context.ActiveStructFieldStack, RequestedType);
	const EExpressionEvaluation FieldEvaluation = Context.GetEvaluation(FieldExpression, Scope, RequestedFieldType);

	OutResult.Type = StructType;
	if (StructEvaluation == EExpressionEvaluation::ConstantZero && FieldEvaluation == EExpressionEvaluation::ConstantZero)
	{
		Context.PreshaderStackPosition++;
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(Shader::FType(StructType));
	}
	else
	{
		StructExpression->GetValuePreshader(Context, Scope, RequestedStructType, StructType, OutResult.Preshader);

		FScopedActiveStructField ActiveFieldScope(Context.ActiveStructFieldStack, Field);

		FieldExpression->GetValuePreshader(Context, Scope, RequestedFieldType, Field->Type, OutResult.Preshader);

		check(Context.PreshaderStackPosition > 0);
		Context.PreshaderStackPosition--;

		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::SetField).Write(Field->ComponentIndex).Write(Field->GetNumComponents());
	}

	if (!RequestedType.IsStruct())
	{
		const Shader::FStructField* ActiveField = Context.ActiveStructFieldStack.Last();
		OutResult.Type = ActiveField->Type;
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::GetField).Write(ActiveField->Type).Write(ActiveField->ComponentIndex);
	}
}

FRequestedType FExpressionSetStructField::MakeRequestedStructType(const FActiveStructFieldStack& ActiveFieldStack, const FRequestedType& RequestedType) const
{
	FRequestedType Result;
	if (RequestedType.IsStruct())
	{
		Result = RequestedType;
	}
	else
	{
		check(!RequestedType.IsObject() && RequestedType.Type.IsNumericVector());
		// Due to legacy, it is possible for RequestedType to be numeric instead of a struct type. It implies requesting the material attribute being processed
		const Shader::FStructField* ActiveField = ActiveFieldStack.Last();
		Result = FRequestedType(StructType, false);
		Result.SetFieldRequested(ActiveField, RequestedType);
	}
	Result.ClearFieldRequested(Field);
	return Result;
}

FRequestedType FExpressionSetStructField::MakeRequestedFieldType(const FActiveStructFieldStack& ActiveFieldStack, const FRequestedType& RequestedType) const
{
	if (RequestedType.IsStruct())
	{
		return RequestedType.GetField(Field);
	}
	else
	{
		check(!RequestedType.IsObject() && RequestedType.Type.IsNumericVector());
		// Due to legacy, it is possible for RequestedType to be numeric instead of a struct type. It implies requesting the material attribute being processed
		const Shader::FStructField* ActiveField = ActiveFieldStack.Last();
		return ActiveField == Field ? RequestedType : FRequestedType(RequestedType.Type, false);
	}
}

bool FExpressionSetStructField::IsStructFieldActive(FEmitContext& Context) const
{
	if (TestMaterialProperty != MP_MAX)
	{
		if (Context.MaterialInterface)
		{
			return Context.MaterialInterface->IsPropertyActive(TestMaterialProperty);
		}
		else if (Context.Material && Context.Material->GetMaterialInterface())
		{
			return Context.Material->GetMaterialInterface()->IsPropertyActive(TestMaterialProperty);
		}
		else
		{
			checkNoEntry();
			return false;
		}
	}
	return true;
}

void FExpressionSelect::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives TrueDerivatives = Tree.GetAnalyticDerivatives(TrueExpression);
	const FExpressionDerivatives FalseDerivatives = Tree.GetAnalyticDerivatives(FalseExpression);
	if (TrueDerivatives.IsValid() && FalseDerivatives.IsValid())
	{
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionSelect>(ConditionExpression, TrueDerivatives.ExpressionDdx, FalseDerivatives.ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionSelect>(ConditionExpression, TrueDerivatives.ExpressionDdy, FalseDerivatives.ExpressionDdy);
	}
}

const FExpression* FExpressionSelect::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionSelect>(
		Tree.GetPreviousFrame(ConditionExpression, Shader::EValueType::Bool1),
		Tree.GetPreviousFrame(TrueExpression, RequestedType),
		Tree.GetPreviousFrame(FalseExpression, RequestedType));
}

bool FExpressionSelect::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& ConditionType = Context.PrepareExpression(ConditionExpression, Scope, Shader::EValueType::Bool1);
	if (ConditionType.IsVoid())
	{
		return false;
	}

	const EExpressionEvaluation ConditionEvaluation = ConditionType.GetEvaluation(Scope, Shader::EValueType::Bool1);
	if (IsConstantEvaluation(ConditionEvaluation))
	{
		const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, ConditionType, Shader::EValueType::Bool1).AsBoolScalar();
		FPreparedType ResultType = Context.PrepareExpression(bCondition ? TrueExpression : FalseExpression, Scope, RequestedType);
		return OutResult.SetType(Context, RequestedType, ResultType);
	}

	const FPreparedType& LhsType = Context.PrepareExpression(FalseExpression, Scope, RequestedType);
	const FPreparedType& RhsType = Context.PrepareExpression(TrueExpression, Scope, RequestedType);
	if (LhsType.IsVoid() || RhsType.IsVoid())
	{
		return false;
	}
	
	FPreparedType ResultType = MergePreparedTypes(LhsType, RhsType);
	if (ResultType.IsVoid())
	{
		return Context.Error(TEXT("Type mismatch"));
	}
	if (LhsType.Type != RhsType.Type)
	{
		check(LhsType.Type.IsNumericVector() && RhsType.Type.IsNumericVector());
		if (LhsType.Type.GetNumComponents() != RhsType.Type.GetNumComponents() && !LhsType.IsNumericScalar() && !RhsType.IsNumericScalar())
		{
			return Context.Error(TEXT("Cannot mix numeric vectors with different number of components"));
		}
	}
	else if (LhsType.IsObject() && !IsConstantEvaluation(ConditionEvaluation))
	{
		return Context.Error(TEXT("Condition must be constant when selecting between objects"));
	}

	ResultType.MergeEvaluation(ConditionEvaluation);
	return OutResult.SetType(Context, RequestedType, ResultType);
}

void FExpressionSelect::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FPreparedType& ConditionType = Context.GetPreparedType(ConditionExpression, Shader::EValueType::Bool1);
	const EExpressionEvaluation ConditionEvaluation = ConditionType.GetEvaluation(Scope, Shader::EValueType::Bool1);
	if (IsConstantEvaluation(ConditionEvaluation))
	{
		const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, ConditionType, Shader::EValueType::Bool1).AsBoolScalar();
		const FExpression* InputExpression = bCondition ? TrueExpression : FalseExpression;
		OutResult.Code = InputExpression->GetValueShader(Context, Scope, RequestedType);
	}
	else
	{
		const Shader::FType LocalType = Context.GetResultType(this, RequestedType);
		const bool bIsLWC = LocalType.IsNumericLWC();

		FEmitShaderExpression* TrueValue = TrueExpression->GetValueShader(Context, Scope, RequestedType, LocalType);
		FEmitShaderExpression* FalseValue = FalseExpression->GetValueShader(Context, Scope, RequestedType, LocalType);

		OutResult.Code = Context.EmitExpression(Scope, LocalType, bIsLWC ? TEXT("LWCSelect(%, %, %)") : TEXT("(% ? % : %)"),
			ConditionExpression->GetValueShader(Context, Scope, Shader::EValueType::Bool1),
			TrueValue,
			FalseValue);
	}
}

void FExpressionSelect::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const FPreparedType& ConditionType = Context.GetPreparedType(ConditionExpression, Shader::EValueType::Bool1);
	const EExpressionEvaluation ConditionEvaluation = ConditionType.GetEvaluation(Scope, Shader::EValueType::Bool1);
	if (IsConstantEvaluation(ConditionEvaluation))
	{
		const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, ConditionType, Shader::EValueType::Bool1).AsBoolScalar();
		const FExpression* InputExpression = bCondition ? TrueExpression : FalseExpression;
		OutResult.Type = InputExpression->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
	}
	else
	{
		const Shader::FType ResultType = Context.GetResultType(this, RequestedType);
		auto ConvertToResultType = [&ResultType](const Shader::FType& Type, Shader::FPreshaderData& Preshader)
		{
			if (Type == ResultType)
			{
				return;
			}

			check(Type.IsNumericVector() && ResultType.IsNumericVector());
			const Shader::EValueComponentType ComponentType = Type.GetComponentType(0);
			const Shader::EValueComponentType ResultComponentType = ResultType.GetComponentType(0);
			const int32 NumComponents = Type.GetNumComponents();
			const int32 NumResultComponents = ResultType.GetNumComponents();
			check(NumComponents == NumResultComponents || NumComponents == 1);

			if (ComponentType != ResultComponentType || NumComponents != NumResultComponents)
			{
				Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(ResultType);
				Preshader.WriteOpcode(Shader::EPreshaderOpcode::Add);
			}
		};

		check(ConditionEvaluation == EExpressionEvaluation::Preshader);
		ConditionExpression->GetValuePreshader(Context, Scope, Shader::EValueType::Bool1, OutResult.Preshader);

		check(Context.PreshaderStackPosition > 0);
		// -1 due to JumpIfFalse consuming condition value. Another -1 due to only one branch is evaluated
		Context.PreshaderStackPosition -= 2;
		const Shader::FPreshaderLabel Label0 = OutResult.Preshader.WriteJump(Shader::EPreshaderOpcode::JumpIfFalse);

		const Shader::FType TrueType = TrueExpression->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
		ConvertToResultType(TrueType, OutResult.Preshader);

		const Shader::FPreshaderLabel Label1 = OutResult.Preshader.WriteJump(Shader::EPreshaderOpcode::Jump);
		OutResult.Preshader.SetLabel(Label0);

		const Shader::FType FalseType = FalseExpression->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
		ConvertToResultType(FalseType, OutResult.Preshader);

		OutResult.Preshader.SetLabel(Label1);
		OutResult.Type = ResultType;
	}
}

bool FExpressionSelect::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	// We have already checked that the condition is constant during PrepareValue
	const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, Shader::EValueType::Bool1).AsBoolScalar();
	const FExpression* SelectedExpression = bCondition ? TrueExpression : FalseExpression;
	return SelectedExpression->GetValueObject(Context, Scope, ObjectTypeName, OutObjectBase);
}

void FExpressionDerivative::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	// TODO
}

const FExpression* FExpressionDerivative::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionDerivative>(Coord, Tree.GetPreviousFrame(Input, RequestedType));
}

bool FExpressionDerivative::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FPreparedType ResultType = Context.PrepareExpression(Input, Scope, RequestedType);
	if (ResultType.IsVoid())
	{
		return false;
	}

	ResultType.Type = Shader::MakeNonLWCType(ResultType.Type);

	const EExpressionEvaluation InputEvaluation = ResultType.GetEvaluation(Scope, RequestedType);
	if (InputEvaluation != EExpressionEvaluation::Shader)
	{
		ResultType.SetEvaluation(EExpressionEvaluation::Constant);
	}
	return OutResult.SetType(Context, RequestedType, ResultType);
}

void FExpressionDerivative::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderExpression* EmitInput = Input->GetValueShader(Context, Scope, RequestedType);
	const bool bIsLWC = Shader::IsLWCType(EmitInput->Type);
	const TCHAR* FunctionName = nullptr;
	switch (Coord)
	{
	case EDerivativeCoordinate::Ddx: FunctionName = bIsLWC ? TEXT("LWCDdx") : TEXT("ddx"); break;
	case EDerivativeCoordinate::Ddy: FunctionName = bIsLWC ? TEXT("LWCDdy") : TEXT("ddy"); break;
	default: checkNoEntry(); break;
	}
	OutResult.Code = Context.EmitExpression(Scope, Shader::MakeNonLWCType(EmitInput->Type), TEXT("%(%)"), FunctionName, EmitInput);
}

void FExpressionDerivative::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	// Derivative of a constant is 0
	Context.PreshaderStackPosition++;
	OutResult.Type = Context.GetResultType(this, RequestedType);
	OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(OutResult.Type);
}

namespace Private
{
FRequestedType GetRequestedSwizzleType(const FSwizzleParameters& Params, const FRequestedType& RequestedType)
{
	const Shader::EValueType InputType = Shader::MakeValueType(RequestedType.GetValueComponentType(), Params.GetNumInputComponents());
	FRequestedType RequestedInputType(InputType, false);
	if (Params.NumComponents == 1)
	{
		if (RequestedType.IsNumericVectorRequested())
		{
			RequestedInputType.SetComponentRequest(Params.GetSwizzleComponentIndex(0));
		}
	}
	else
	{
		for (int32 Index = 0; Index < Params.NumComponents; ++Index)
		{
			if (RequestedType.IsComponentRequested(Index))
			{
				const int32 SwizzledComponentIndex = Params.GetSwizzleComponentIndex(Index);
				RequestedInputType.SetComponentRequest(SwizzledComponentIndex);
			}
		}
	}
	return RequestedInputType;
}
} // namespace Private

const FExpression* FTree::NewSwizzle(const FSwizzleParameters& Params, const FExpression* Input)
{
	if (Params.bHasSwizzle)
	{
		return NewExpression<FExpressionSwizzle>(Params, Input);
	}
	return Input;
}

void FExpressionSwizzle::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives InputDerivatives = Tree.GetAnalyticDerivatives(Input);
	if (InputDerivatives.IsValid())
	{
		OutResult.ExpressionDdx = Tree.NewSwizzle(Parameters, InputDerivatives.ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewSwizzle(Parameters, InputDerivatives.ExpressionDdy);
	}
}

const FExpression* FExpressionSwizzle::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	const FRequestedType RequestedInputType = Private::GetRequestedSwizzleType(Parameters, RequestedType);
	return Tree.NewSwizzle(Parameters, Tree.GetPreviousFrame(Input, RequestedInputType));
}

namespace Private
{
bool SwizzlePrepareValue(FEmitContext& Context,
	FEmitScope& Scope,
	const FRequestedType& RequestedType,
	const FSwizzleParameters& Parameters,
	const FExpression* Input,
	FPrepareValueResult& OutResult)
{
	const FRequestedType RequestedInputType = Private::GetRequestedSwizzleType(Parameters, RequestedType);
	const FPreparedType& InputType = Context.PrepareExpression(Input, Scope, RequestedInputType);
	if (InputType.IsVoid())
	{
		return false;
	}

	FPreparedType ResultType;
	ResultType.Type = Shader::MakeValueType(InputType.GetValueComponentType(), Parameters.NumComponents);
	for (int32 ComponentIndex = 0; ComponentIndex < Parameters.NumComponents; ++ComponentIndex)
	{
		const int32 SwizzledComponentIndex = Parameters.GetSwizzleComponentIndex(ComponentIndex);
		ResultType.SetComponent(ComponentIndex, InputType.GetComponent(SwizzledComponentIndex));
	}
	return OutResult.SetType(Context, RequestedType, ResultType);
}

void SwizzleEmitValueShader(FEmitContext& Context,
	FEmitScope& Scope,
	const FRequestedType& RequestedType,
	const FSwizzleParameters& Parameters,
	const FExpression* Input,
	FEmitValueShaderResult& OutResult)
{
	const FRequestedType RequestedInputType = Private::GetRequestedSwizzleType(Parameters, RequestedType);

	// Make sure the input is cast to the explicit requested type, this ensures it will have enough components for the swizzle
	// Alternately, we could avoid the cast, and update the logic to insert 0s for swizzle access to invalid components or
	// replicate the first component in case the input is scalar
	FEmitShaderExpression* EmitInput = Input->GetValueShader(Context, Scope, RequestedInputType, RequestedInputType.Type.GetConcreteType());

	const Shader::FValueTypeDescription InputTypeDesc = Shader::GetValueTypeDescription(EmitInput->Type);
	bool bSkipSwizzle = (InputTypeDesc.NumComponents == Parameters.NumComponents);
	if (bSkipSwizzle)
	{
		for (int32 ComponentIndex = 0; ComponentIndex < Parameters.NumComponents; ++ComponentIndex)
		{
			if (Parameters.GetSwizzleComponentIndex(ComponentIndex) != ComponentIndex)
			{
				bSkipSwizzle = false;
				break;
			}
		}
	}

	if (bSkipSwizzle)
	{
		OutResult.Code = EmitInput;
	}
	else
	{
		const bool bIsLWC = (InputTypeDesc.ComponentType == Shader::EValueComponentType::Double);
		TStringBuilder<256> FormattedCode;
		if (bIsLWC)
		{
			FormattedCode.Appendf(TEXT("WSSwizzle(%s"), EmitInput->Reference);
		}
		else
		{
			FormattedCode.Appendf(TEXT("%s."), EmitInput->Reference);
		}

		static const TCHAR ComponentName[] = { 'x', 'y', 'z', 'w' };
		for (int32 ComponentIndex = 0; ComponentIndex < Parameters.NumComponents; ++ComponentIndex)
		{
			const int32 SwizzledComponentIndex = (InputTypeDesc.NumComponents > 1) ? Parameters.GetSwizzleComponentIndex(ComponentIndex) : 0;
			if (bIsLWC)
			{
				FormattedCode.Appendf(TEXT(", %d"), SwizzledComponentIndex);
			}
			else
			{
				FormattedCode.AppendChar(ComponentName[SwizzledComponentIndex]);
			}
		}

		if (bIsLWC)
		{
			FormattedCode.Append(TEXT(")"));
		}

		const Shader::FType ResultType = Shader::MakeValueType(InputTypeDesc.ComponentType, Parameters.NumComponents);
		OutResult.Code = Context.EmitInlineExpressionWithDependency(Scope, EmitInput, ResultType, FormattedCode.ToView());
	}
}

void SwizzleEmitValuePreshader(FEmitContext& Context,
	FEmitScope& Scope,
	const FRequestedType& RequestedType,
	const FSwizzleParameters& Parameters,
	const FExpression* Input,
	FEmitValuePreshaderResult& OutResult)
{
	const FRequestedType RequestedInputType = Private::GetRequestedSwizzleType(Parameters, RequestedType);
	if (RequestedInputType.IsVoid())
	{
		Context.PreshaderStackPosition++;
		OutResult.Type = Shader::MakeValueType(Shader::EValueComponentType::Float, Parameters.NumComponents);
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(OutResult.Type);
	}
	else
	{
		const Shader::FType InputType = Input->GetValuePreshader(Context, Scope, RequestedInputType, OutResult.Preshader);
		const Shader::FValueTypeDescription InputTypeDesc = Shader::GetValueTypeDescription(InputType);

		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ComponentSwizzle)
			.Write((uint8)Parameters.NumComponents)
			.Write((uint8)Parameters.GetSwizzleComponentIndex(0))
			.Write((uint8)Parameters.GetSwizzleComponentIndex(1))
			.Write((uint8)Parameters.GetSwizzleComponentIndex(2))
			.Write((uint8)Parameters.GetSwizzleComponentIndex(3));
		OutResult.Type = Shader::MakeValueType(InputTypeDesc.ComponentType, Parameters.NumComponents);
	}
}

} // namespace Private

bool FExpressionSwizzle::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	return Private::SwizzlePrepareValue(Context, Scope, RequestedType, Parameters, Input, OutResult);
}

void FExpressionSwizzle::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	return Private::SwizzleEmitValueShader(Context, Scope, RequestedType, Parameters, Input, OutResult);
}

void FExpressionSwizzle::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	return Private::SwizzleEmitValuePreshader(Context, Scope, RequestedType, Parameters, Input, OutResult);
}

void FExpressionComponentMask::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives InputDerivatives = Tree.GetAnalyticDerivatives(Input);
	if (InputDerivatives.IsValid())
	{
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionComponentMask>(InputDerivatives.ExpressionDdx, Mask);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionComponentMask>(InputDerivatives.ExpressionDdy, Mask);
	}
}

const FExpression* FExpressionComponentMask::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionComponentMask>(Tree.GetPreviousFrame(Input, RequestedType), Mask);
}

namespace Private
{
FSwizzleParameters GetComponentMaskSwizzle(FEmitContext& Context, FEmitScope& Scope, const FExpression* Mask)
{
	const Shader::FBoolValue MaskValue = Mask->GetValueConstant(Context, Scope, Shader::EValueType::Bool4).AsBool();
	return MakeSwizzleMask(MaskValue[0], MaskValue[1], MaskValue[2], MaskValue[3]);
}
} // namespace Private

bool FExpressionComponentMask::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& PreparedMaskType = Context.PrepareExpression(Mask, Scope, Shader::EValueType::Bool4);
	const EExpressionEvaluation MaskEvaluation = PreparedMaskType.GetEvaluation(Scope, Shader::EValueType::Bool4);
	if (!IsConstantEvaluation(MaskEvaluation))
	{
		return Context.Error(TEXT("Mask must be constant"));
	}

	const FSwizzleParameters Swizzle = Private::GetComponentMaskSwizzle(Context, Scope, Mask);
	return Private::SwizzlePrepareValue(Context, Scope, RequestedType, Swizzle, Input, OutResult);
}

void FExpressionComponentMask::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const FSwizzleParameters Swizzle = Private::GetComponentMaskSwizzle(Context, Scope, Mask);
	return Private::SwizzleEmitValueShader(Context, Scope, RequestedType, Swizzle, Input, OutResult);
}

void FExpressionComponentMask::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const FSwizzleParameters Swizzle = Private::GetComponentMaskSwizzle(Context, Scope, Mask);
	return Private::SwizzleEmitValuePreshader(Context, Scope, RequestedType, Swizzle, Input, OutResult);
}

namespace Private
{
struct FAppendTypes
{
	FPreparedType ResultType;
	Shader::EValueType LhsType;
	Shader::EValueType RhsType;
	FRequestedType LhsRequestedType;
	FRequestedType RhsRequestedType;
	bool bIsLWC;
};
FAppendTypes GetAppendTypes(const FRequestedType& RequestedType, const FPreparedType& LhsType, const FPreparedType& RhsType)
{
	const Shader::FValueTypeDescription LhsTypeDesc = Shader::GetValueTypeDescription(LhsType.Type);
	const Shader::FValueTypeDescription RhsTypeDesc = Shader::GetValueTypeDescription(RhsType.Type);
	const Shader::EValueComponentType ComponentType = Shader::CombineComponentTypes(LhsTypeDesc.ComponentType, RhsTypeDesc.ComponentType);
	const int32 NumLhsComponents = LhsType.Type.GetNumComponents();
	const int32 NumRhsComponents = RhsType.Type.GetNumComponents();
	const int32 NumComponents = FMath::Min(NumLhsComponents + NumRhsComponents, 4);

	FAppendTypes Types;
	Types.ResultType.Type = Shader::MakeValueType(ComponentType, NumComponents);
	Types.LhsType = Shader::MakeValueType(ComponentType, NumLhsComponents);
	Types.RhsType = Shader::MakeValueType(ComponentType, NumComponents - NumLhsComponents);
	Types.LhsRequestedType.Type = Types.LhsType;
	Types.RhsRequestedType.Type = Types.RhsType;
	Types.bIsLWC = ComponentType == Shader::EValueComponentType::Double;

	for (int32 Index = 0; Index < NumLhsComponents; ++Index)
	{
		Types.ResultType.SetComponent(Index, LhsType.GetComponent(Index));
		if (RequestedType.IsComponentRequested(Index))
		{
			Types.LhsRequestedType.SetComponentRequest(Index);
		}
	}
	for (int32 Index = NumLhsComponents; Index < NumComponents; ++Index)
	{
		Types.ResultType.SetComponent(Index, RhsType.GetComponent(Index - NumLhsComponents));
		if (RequestedType.IsComponentRequested(Index))
		{
			Types.RhsRequestedType.SetComponentRequest(Index - NumLhsComponents);
		}
	}

	return Types;
}
} // namespace Private

void FExpressionAppend::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpressionDerivatives LhsDerivatives = Tree.GetAnalyticDerivatives(Lhs);
	const FExpressionDerivatives RhsDerivatives = Tree.GetAnalyticDerivatives(Rhs);
	if (LhsDerivatives.IsValid() && RhsDerivatives.IsValid())
	{
		OutResult.ExpressionDdx = Tree.NewExpression<FExpressionAppend>(LhsDerivatives.ExpressionDdx, RhsDerivatives.ExpressionDdx);
		OutResult.ExpressionDdy = Tree.NewExpression<FExpressionAppend>(LhsDerivatives.ExpressionDdy, RhsDerivatives.ExpressionDdy);
	}
}

const FExpression* FExpressionAppend::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	// TODO - requested type?
	return Tree.NewExpression<FExpressionAppend>(Tree.GetPreviousFrame(Lhs, RequestedType), Tree.GetPreviousFrame(Rhs, RequestedType));
}

bool FExpressionAppend::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	const FPreparedType& LhsType = Context.PrepareExpression(Lhs, Scope, RequestedType);
	if (LhsType.IsVoid())
	{
		return false;
	}

	const int32 NumRequestedComponents = RequestedType.Type.GetNumComponents();
	const int32 NumLhsComponents = LhsType.Type.GetNumComponents();
	const int32 NumRhsComponents = FMath::Max(NumRequestedComponents - NumLhsComponents, 1);

	FRequestedType RhsRequestedType;
	if (RequestedType.Type.IsAny())
	{
		RhsRequestedType = Shader::EValueType::Any;
	}
	else
	{
		RhsRequestedType.Type = Shader::MakeValueType(RequestedType.GetValueComponentType(), NumRhsComponents);
		for (int32 Index = NumLhsComponents; Index < NumRequestedComponents; ++Index)
		{
			if (RequestedType.IsComponentRequested(Index))
			{
				RhsRequestedType.SetComponentRequest(Index - NumLhsComponents);
			}
		}
	}

	const FPreparedType& RhsType = Context.PrepareExpression(Rhs, Scope, RhsRequestedType);
	if (RhsType.IsVoid())
	{
		return false;
	}

	const Private::FAppendTypes Types = Private::GetAppendTypes(RequestedType, LhsType, RhsType);
	check(!Types.ResultType.IsNumericScalar()); // Appending 2 values should never result in a scalar
	if (Types.ResultType.IsVoid())
	{
		return false;
	}

	Context.MarkInputType(Lhs, Types.LhsType);
	Context.MarkInputType(Rhs, Types.RhsType);

	return OutResult.SetType(Context, RequestedType, Types.ResultType);
}

void FExpressionAppend::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	const Private::FAppendTypes Types = Private::GetAppendTypes(RequestedType, Context.GetPreparedType(Lhs, RequestedType), Context.GetPreparedType(Rhs, RequestedType));
	const Shader::FType ResultType = Types.ResultType.GetResultType();
	FEmitShaderExpression* LhsValue = Lhs->GetValueShader(Context, Scope, Types.LhsRequestedType, Types.LhsType);

	if (Types.RhsType == Shader::EValueType::Void)
	{
		OutResult.Code = LhsValue;
	}
	else
	{
		FEmitShaderExpression* RhsValue = Rhs->GetValueShader(Context, Scope, Types.RhsRequestedType, Types.RhsType);
		if (Types.bIsLWC)
		{
			OutResult.Code = Context.EmitInlineExpression(Scope, ResultType, TEXT("MakeWSVector(%, %)"),
				LhsValue,
				RhsValue);
		}
		else
		{
			OutResult.Code = Context.EmitInlineExpression(Scope, ResultType, TEXT("%(%, %)"),
				Shader::GetValueTypeDescription(ResultType).Name,
				LhsValue,
				RhsValue);
		}
	}
}

void FExpressionAppend::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	const Private::FAppendTypes Types = Private::GetAppendTypes(RequestedType, Context.GetPreparedType(Lhs, RequestedType), Context.GetPreparedType(Rhs, RequestedType));
	Lhs->GetValuePreshader(Context, Scope, Types.LhsRequestedType, OutResult.Preshader);
	if (Types.RhsType != Shader::EValueType::Void)
	{
		Rhs->GetValuePreshader(Context, Scope, Types.RhsRequestedType, OutResult.Preshader);

		check(Context.PreshaderStackPosition > 0);
		Context.PreshaderStackPosition--;

		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::AppendVector);
	}
	OutResult.Type = Types.ResultType.GetResultType();
}

const FExpression* FTree::NewAppend(const FExpression* Lhs, const FExpression* Rhs)
{
	return NewExpression<FExpressionAppend>(Lhs, Rhs);
}

void FExpressionSwitchBase::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	const FExpression* InputDdx[MaxInputs];
	const FExpression* InputDdy[MaxInputs];
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		const FExpressionDerivatives InputDerivative = Tree.GetAnalyticDerivatives(Input[Index]);
		InputDdx[Index] = InputDerivative.ExpressionDdx;
		InputDdy[Index] = InputDerivative.ExpressionDdy;
	}
	OutResult.ExpressionDdx = NewSwitch(Tree, MakeArrayView(InputDdx, NumInputs));
	OutResult.ExpressionDdy = NewSwitch(Tree, MakeArrayView(InputDdy, NumInputs));
}

const FExpression* FExpressionSwitchBase::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	const FExpression* InputPrevFrame[MaxInputs];
	for (int32 Index = 0; Index < NumInputs; ++Index)
	{
		InputPrevFrame[Index] = Tree.GetPreviousFrame(Input[Index], RequestedType);
	}
	return NewSwitch(Tree, MakeArrayView(InputPrevFrame, NumInputs));
}

bool FExpressionSwitchBase::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FPreparedType ResultType;
	for (int32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		if (IsInputActive(Context, InputIndex))
		{
			const FPreparedType& InputType = Context.PrepareExpression(Input[InputIndex], Scope, RequestedType);
			ResultType = MergePreparedTypes(ResultType, InputType);
		}
	}

	if (ResultType.IsVoid())
	{
		return false;
	}

	return OutResult.SetType(Context, RequestedType, ResultType);
}

void FExpressionSwitchBase::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	for (int32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		if (IsInputActive(Context, InputIndex))
		{
			OutResult.Code = Input[InputIndex]->GetValueShader(Context, Scope, RequestedType);
			break;
		}
	}
}

void FExpressionSwitchBase::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	for (int32 InputIndex = 0; InputIndex < NumInputs; ++InputIndex)
	{
		if (IsInputActive(Context, InputIndex))
		{
			OutResult.Type = Input[InputIndex]->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
			break;
		}
	}
}

FExpressionFeatureLevelSwitch::FExpressionFeatureLevelSwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= (int32)ERHIFeatureLevel::Num, "FExpressionSwitchBase is too small for FExpressionFeatureLevelSwitch");
	check(InInputs.Num() == (int32)ERHIFeatureLevel::Num);
}

bool FExpressionFeatureLevelSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	return Context.TargetParameters.IsGenericTarget() || (Index == (int32)Context.TargetParameters.FeatureLevel);
}

FExpressionShadingPathSwitch::FExpressionShadingPathSwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= (int32)ERHIShadingPath::Num, "FExpressionSwitchBase is too small for FExpressionShadingPathSwitch");
	check(InInputs.Num() == (int32)ERHIShadingPath::Num);
}

bool FExpressionShadingPathSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	if (Context.TargetParameters.IsGenericTarget())
	{
		return true;
	}

	const EShaderPlatform ShaderPlatform = Context.TargetParameters.ShaderPlatform;
	ERHIShadingPath::Type ShadingPathToCompile = ERHIShadingPath::Deferred;
	if (IsForwardShadingEnabled(ShaderPlatform))
	{
		ShadingPathToCompile = ERHIShadingPath::Forward;
	}
	else if (Context.TargetParameters.FeatureLevel < ERHIFeatureLevel::SM5)
	{
		ShadingPathToCompile = ERHIShadingPath::Mobile;
	}
	return Index == (int32)ShadingPathToCompile;
}

FExpressionQualitySwitch::FExpressionQualitySwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= (int32)EMaterialQualityLevel::Num + 1, "FExpressionSwitchBase is too small for FExpressionQualitySwitch");
	check(InInputs.Num() == (int32)EMaterialQualityLevel::Num + 1);
}

bool FExpressionQualitySwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	constexpr int32 DefaultInputIndex = (int32)EMaterialQualityLevel::Num;

	if (Context.TargetParameters.IsGenericTarget())
	{
		return Index == DefaultInputIndex || Input[Index] != Input[DefaultInputIndex];
	}
	else
	{
		return Index == (int32)Context.Material->GetQualityLevel();
	}
}

bool FExpressionQualitySwitch::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FMaterialCachedExpressionData* CachedExpressionData = Context.FindData<Material::FEmitData>().CachedExpressionData;
	if (CachedExpressionData)
	{
		constexpr int32 DefaultInputIndex = (int32)EMaterialQualityLevel::Num;

		for (int32 Index = 0; Index < DefaultInputIndex; ++Index)
		{
			if (Input[Index] != Input[DefaultInputIndex])
			{
				CachedExpressionData->QualityLevelsUsed[Index] = true;
			}
		}
	}

	return FExpressionSwitchBase::PrepareValue(Context, Scope, RequestedType, OutResult);
}

FExpressionShaderStageSwitch::FExpressionShaderStageSwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= 2, "FExpressionSwitchBase is too small for FExpressionShaderStageSwitch");
	check(InInputs.Num() == 2);
}

bool FExpressionShaderStageSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	check(Context.ShaderFrequency == SF_Pixel || Context.ShaderFrequency == SF_Vertex);
	return (Context.ShaderFrequency == SF_Pixel && Index == 0) || (Context.ShaderFrequency == SF_Vertex && Index == 1);
}

FExpressionVirtualTextureFeatureSwitch::FExpressionVirtualTextureFeatureSwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= 2, "FExpressionSwitchBase is too small for FExpressionVirtualTextureFeatureSwitch");
	check(InInputs.Num() == 2);
}

bool FExpressionVirtualTextureFeatureSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	if (Context.TargetParameters.IsGenericTarget())
	{
		return true;
	}

	if (UseVirtualTexturing(Context.TargetParameters.ShaderPlatform))
	{
		return Index == 0;
	}
	else
	{
		return Index == 1;
	}
}

FExpressionDistanceFieldsRenderingSwitch::FExpressionDistanceFieldsRenderingSwitch(TConstArrayView<const FExpression*> InInputs)
	: FExpressionSwitchBase(InInputs)
{
	static_assert(MaxInputs >= 2, "FExpressionSwitchBase is too small for FExpressionDistanceFieldsRenderingSwitch");
	check(InInputs.Num() == 2);
}

bool FExpressionDistanceFieldsRenderingSwitch::IsInputActive(const FEmitContext& Context, int32 Index) const
{
	if (Context.TargetParameters.IsGenericTarget())
	{
		return true;
	}

	if (IsMobilePlatform(Context.TargetParameters.ShaderPlatform))
	{
		if (IsMobileDistanceFieldEnabled(Context.TargetParameters.ShaderPlatform))
		{
			return Index == 0;
		}
		else
		{
			return Index == 1;
		}
	}
	else
	{
		if (IsUsingDistanceFields(Context.TargetParameters.ShaderPlatform))
		{
			return Index == 0;
		}
		else
		{
			return Index == 1;
		}
	}
}

bool FExpressionInlineCustomHLSL::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::FType(ResultType));
}

void FExpressionInlineCustomHLSL::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	OutResult.Code = Context.EmitExpression(Scope, ResultType, Code);
}

bool FExpressionCustomHLSL::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	for (const FCustomHLSLInput& Input : Inputs)
	{
		const FPreparedType& InputType = Context.PrepareExpression(Input.Expression, Scope, Shader::EValueType::Any);
		if (InputType.IsVoid())
		{
			return false;
		}

		if (InputType.IsObject())
		{
			if (!Input.Expression->CheckObjectSupportsCustomHLSL(Context, Scope, InputType.Type.ObjectType))
			{
				return Context.Error(TEXT("Object type not supported for custom HLSL"));
			}
		}

		Context.MarkInputType(Input.Expression, InputType.Type);
	}

	FMaterialCachedExpressionData* CachedExpressionData = Context.FindData<Material::FEmitData>().CachedExpressionData;
	if (CachedExpressionData && CachedExpressionData->EditorOnlyData.IsValid())
	{
		for (const FString& IncludeFilePath : IncludeFilePaths)
		{
			CachedExpressionData->EditorOnlyData->ExpressionIncludeFilePaths.Add(IncludeFilePath);
		}
	}

	return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::Shader, Shader::FType(OutputStructType));
}

void FExpressionCustomHLSL::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	OutResult.Code = Context.EmitCustomHLSL(Scope, DeclarationCode, FunctionCode, Inputs, OutputStructType);
}

bool FStatementError::Prepare(FEmitContext& Context, FEmitScope& Scope) const
{
	return Context.Error(ErrorMessage);
}

bool FStatementBreak::Prepare(FEmitContext& Context, FEmitScope& Scope) const
{
	return true;
}

void FStatementBreak::EmitShader(FEmitContext& Context, FEmitScope& Scope) const
{
	Context.EmitStatement(Scope, TEXT("break;"));
}

void FStatementBreak::EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const
{
	FPreshaderLoopScope* LoopScope = Context.PreshaderLoopScopes.Last();
	check(!LoopScope->BreakStatement);
	LoopScope->BreakStatement = this;
	LoopScope->BreakLabel = OutPreshader.WriteJump(Shader::EPreshaderOpcode::Jump);
}

bool FStatementIf::Prepare(FEmitContext& Context, FEmitScope& Scope) const
{
	const FPreparedType& ConditionType = Context.PrepareExpression(ConditionExpression, Scope, Shader::EValueType::Bool1);
	if (ConditionType.IsVoid())
	{
		return false;
	}

	const EExpressionEvaluation ConditionEvaluation = ConditionType.GetEvaluation(Scope, Shader::EValueType::Bool1);
	check(ConditionEvaluation != EExpressionEvaluation::None);
	if (IsConstantEvaluation(ConditionEvaluation))
	{
		const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, ConditionType, Shader::EValueType::Bool1).AsBoolScalar();
		if (bCondition)
		{
			Context.MarkScopeEvaluation(Scope, ThenScope, ConditionEvaluation);
			Context.MarkScopeDead(Scope, ElseScope);
		}
		else
		{
			Context.MarkScopeDead(Scope, ThenScope);
			Context.MarkScopeEvaluation(Scope, ElseScope, ConditionEvaluation);
		}
	}
	else
	{
		Context.MarkScopeEvaluation(Scope, ThenScope, ConditionEvaluation);
		Context.MarkScopeEvaluation(Scope, ElseScope, ConditionEvaluation);
	}

	return true;
}

void FStatementIf::EmitShader(FEmitContext& Context, FEmitScope& Scope) const
{
	FEmitShaderNode* Dependency = nullptr;
	const FPreparedType& ConditionType = Context.GetPreparedType(ConditionExpression, Shader::EValueType::Bool1);
	const EExpressionEvaluation ConditionEvaluation = ConditionType.GetEvaluation(Scope, Shader::EValueType::Bool1);
	if (IsConstantEvaluation(ConditionEvaluation))
	{
		const bool bCondition = ConditionExpression->GetValueConstant(Context, Scope, ConditionType, Shader::EValueType::Bool1).AsBoolScalar();
		if (bCondition)
		{
			Dependency = Context.EmitNextScope(Scope, ThenScope);
		}
		else if(!bCondition)
		{
			Dependency = Context.EmitNextScope(Scope, ElseScope);
		}
	}
	else if(ConditionEvaluation != EExpressionEvaluation::None)
	{
		FEmitShaderExpression* ConditionValue = ConditionExpression->GetValueShader(Context, Scope, Shader::EValueType::Bool1);
		Dependency = Context.EmitNestedScopes(Scope, ThenScope, ElseScope, TEXT("if (%)"), TEXT("else"), ConditionValue);
	}

	Context.EmitNextScopeWithDependency(Scope, Dependency, NextScope);
}

void FStatementIf::EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const
{
	ConditionExpression->GetValuePreshader(Context, Scope, Shader::EValueType::Bool1, OutPreshader);

	check(Context.PreshaderStackPosition > 0);
	Context.PreshaderStackPosition--;
	const Shader::FPreshaderLabel Label0 = OutPreshader.WriteJump(Shader::EPreshaderOpcode::JumpIfFalse);

	Context.EmitPreshaderScope(ThenScope, RequestedType, Scopes, OutPreshader);

	const Shader::FPreshaderLabel Label1 = OutPreshader.WriteJump(Shader::EPreshaderOpcode::Jump);
	OutPreshader.SetLabel(Label0);
	
	Context.EmitPreshaderScope(ElseScope, RequestedType, Scopes, OutPreshader);

	OutPreshader.SetLabel(Label1);
}

bool FStatementLoop::Prepare(FEmitContext& Context, FEmitScope& Scope) const
{
	FEmitScope* BreakScope = Context.PrepareScope(&BreakStatement->GetParentScope());
	if (!BreakScope)
	{
		return false;
	}

	Context.MarkScopeEvaluation(Scope, LoopScope, BreakScope->Evaluation);
	return true;
}

void FStatementLoop::EmitShader(FEmitContext& Context, FEmitScope& Scope) const
{
	FEmitShaderNode* Dependency = Context.EmitNestedScope(Scope, LoopScope, TEXT("while (true)"));
	Context.EmitNextScopeWithDependency(Scope, Dependency, NextScope);
}

void FStatementLoop::EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const
{
	FPreshaderLoopScope PreshaderLoopScope;
	Context.PreshaderLoopScopes.Add(&PreshaderLoopScope);

	const Shader::FPreshaderLabel Label = OutPreshader.GetLabel();
	Context.EmitPreshaderScope(LoopScope, RequestedType, Scopes, OutPreshader);
	OutPreshader.WriteJump(Shader::EPreshaderOpcode::Jump, Label);

	verify(Context.PreshaderLoopScopes.Pop() == &PreshaderLoopScope);
	check(PreshaderLoopScope.BreakStatement == BreakStatement);
	OutPreshader.SetLabel(PreshaderLoopScope.BreakLabel);
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
