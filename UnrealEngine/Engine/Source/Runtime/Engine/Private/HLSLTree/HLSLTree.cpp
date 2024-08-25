// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "HLSLTree/HLSLTree.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "HLSLTree/HLSLTreeCommon.h"
#include "Misc/MemStackUtility.h"
#include "Shader/Preshader.h"
#include "Shader/PreshaderTypes.h"

namespace UE::HLSLTree
{

enum ELocalPHIChainType : uint8
{
	Ddx,
	Ddy,
	PreviousFrame,
};

struct FLocalPHIChainEntry
{
	FLocalPHIChainEntry() = default;
	FLocalPHIChainEntry(const ELocalPHIChainType& InType, const FRequestedType& InRequestedType) : Type(InType), RequestedType(InRequestedType) {}

	ELocalPHIChainType Type;
	FRequestedType RequestedType;
};

/**
 * Represents a phi node (see various topics on single static assignment)
 * A phi node takes on a value based on the previous scope that was executed.
 * In practice, this means the generated HLSL code will declare a local variable before all the previous scopes, then assign that variable the proper value from within each scope
 */
class FExpressionLocalPHI final : public FExpression
{
public:
	FExpressionLocalPHI(const FName& InLocalName, TArrayView<FScope*> InPreviousScopes) : LocalName(InLocalName), NumValues(InPreviousScopes.Num())
	{
		for (int32 i = 0; i < InPreviousScopes.Num(); ++i)
		{
			Scopes[i] = InPreviousScopes[i];
			Values[i] = nullptr;
		}
	}

	FExpressionLocalPHI(const FExpressionLocalPHI* Source, ELocalPHIChainType Type, const FRequestedType& RequestedType = FRequestedType());

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual const FExpression* ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
	virtual bool EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const override;

	TArray<FLocalPHIChainEntry, TInlineAllocator<8>> Chain;
	FName LocalName;
	FScope* Scopes[MaxNumPreviousScopes];
	const FExpression* Values[MaxNumPreviousScopes];
	int32 NumValues = 0;
};

/**
 * Represents a call to a function that includes its own scope/control-flow
 * Scope for the function will be linked into the generated material
 */
class FExpressionFunctionCall final : public FExpression
{
public:
	FExpressionFunctionCall(FFunction* InFunction, int32 InOutputIndex) : Function(InFunction), OutputIndex(InOutputIndex) {}

	virtual void ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const override;
	virtual bool PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const override;
	virtual void EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const override;
	virtual void EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const override;
	virtual bool EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const override;

	FFunction* Function;
	int32 OutputIndex;
};

EExpressionEvaluation CombineEvaluations(EExpressionEvaluation Lhs, EExpressionEvaluation Rhs)
{
	if (Lhs == EExpressionEvaluation::None)
	{
		// If either is 'None', return the other
		return Rhs;
	}
	else if (Rhs == EExpressionEvaluation::None)
	{
		return Lhs;
	}
	else if (Lhs == EExpressionEvaluation::Unknown)
	{
		return Rhs;
	}
	else if (Rhs == EExpressionEvaluation::Unknown)
	{
		return Lhs;
	}
	else if (Lhs == EExpressionEvaluation::Shader || Rhs == EExpressionEvaluation::Shader)
	{
		// If either requires shader, shader is required
		return EExpressionEvaluation::Shader;
	}
	else if (Lhs == EExpressionEvaluation::PreshaderLoop || Rhs == EExpressionEvaluation::PreshaderLoop)
	{
		// Otherwise if either requires preshader, preshader is required
		return EExpressionEvaluation::PreshaderLoop;
	}
	else if (Lhs == EExpressionEvaluation::Preshader || Rhs == EExpressionEvaluation::Preshader)
	{
		// Otherwise if either requires preshader, preshader is required
		return EExpressionEvaluation::Preshader;
	}
	else if (Lhs == EExpressionEvaluation::ConstantLoop || Rhs == EExpressionEvaluation::ConstantLoop)
	{
		return EExpressionEvaluation::ConstantLoop;
	}
	else if (Lhs == EExpressionEvaluation::Constant || Rhs == EExpressionEvaluation::Constant)
	{
		return EExpressionEvaluation::Constant;
	}

	// Otherwise must be constant
	check(Lhs == EExpressionEvaluation::ConstantZero);
	check(Rhs == EExpressionEvaluation::ConstantZero);
	return EExpressionEvaluation::ConstantZero;
}

EExpressionEvaluation MakeLoopEvaluation(EExpressionEvaluation Evaluation)
{
	if (Evaluation == EExpressionEvaluation::Preshader)
	{
		return EExpressionEvaluation::PreshaderLoop;
	}
	else if (Evaluation == EExpressionEvaluation::Constant)
	{
		return EExpressionEvaluation::ConstantLoop;
	}
	return Evaluation;
}

EExpressionEvaluation MakeNonLoopEvaluation(EExpressionEvaluation Evaluation)
{
	if (Evaluation == EExpressionEvaluation::PreshaderLoop)
	{
		return EExpressionEvaluation::Preshader;
	}
	else if (Evaluation == EExpressionEvaluation::ConstantLoop)
	{
		return EExpressionEvaluation::Constant;
	}
	return Evaluation;
}

FScope* FScope::FindSharedParent(FScope* Lhs, FScope* Rhs)
{
	FScope* Scope0 = Lhs;
	FScope* Scope1 = Rhs;
	if (Scope1)
	{
		while (Scope0 != Scope1)
		{
			if (Scope0->NestedLevel > Scope1->NestedLevel)
			{
				check(Scope0->ParentScope);
				Scope0 = Scope0->ParentScope;
			}
			else
			{
				check(Scope1->ParentScope);
				Scope1 = Scope1->ParentScope;
			}
		}
	}
	return Scope0;
}

FExpressionLocalPHI::FExpressionLocalPHI(const FExpressionLocalPHI* Source, ELocalPHIChainType Type, const FRequestedType& RequestedType)
	: Chain(Source->Chain)
	, LocalName(Source->LocalName)
	, NumValues(Source->NumValues)
{
	Chain.Emplace(Type, RequestedType);
	for (int32 i = 0; i < NumValues; ++i)
	{
		Scopes[i] = Source->Scopes[i];
	}
}

void FExpressionLocalPHI::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	// We don't have values assigned at the time analytic derivatives are computed
	// It's possible the derivatives will be end up being invalid, but that case will need to be detected later, during PrepareValue
	OutResult.ExpressionDdx = Tree.NewExpression<FExpressionLocalPHI>(this, ELocalPHIChainType::Ddx);
	OutResult.ExpressionDdy = Tree.NewExpression<FExpressionLocalPHI>(this, ELocalPHIChainType::Ddy);
}

const FExpression* FExpressionLocalPHI::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return Tree.NewExpression<FExpressionLocalPHI>(this, ELocalPHIChainType::PreviousFrame, RequestedType);
}

bool FExpressionLocalPHI::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	check(NumValues <= MaxNumPreviousScopes);

	FEmitScope* EmitValueScopes[MaxNumPreviousScopes];
	FScope* LiveScopes[MaxNumPreviousScopes];
	const FExpression* LiveValues[MaxNumPreviousScopes];
	int32 NumLiveScopes = 0;

	for (int32 i = 0; i < NumValues; ++i)
	{
		FEmitScope* EmitValueScope = Context.PrepareScope(Scopes[i]);
		if (EmitValueScope)
		{
			const int32 LiveScopeIndex = NumLiveScopes++;
			EmitValueScopes[LiveScopeIndex] = EmitValueScope;
			LiveScopes[LiveScopeIndex] = Scopes[i];
			LiveValues[LiveScopeIndex] = Values[i];
		}
	}

	if (NumLiveScopes == 0)
	{
		return OutResult.SetType(Context, RequestedType, EExpressionEvaluation::ConstantZero, Shader::EValueType::Float1);
	}

	FPreparedType TypePerValue[MaxNumPreviousScopes];
	int32 NumValidTypes = 0;
	FPreparedType CurrentType;

	auto UpdateValueTypes = [&]()
	{
		for (int32 i = 0; i < NumLiveScopes; ++i)
		{
			if (EmitValueScopes[i]->Evaluation == EExpressionEvaluation::Unknown)
			{
				EmitValueScopes[i] = Context.PrepareScope(LiveScopes[i]);
			}
			if (TypePerValue[i].IsVoid())
			{
				const FPreparedType& ValueType = Context.PrepareExpression(LiveValues[i], *EmitValueScopes[i], RequestedType);
				if (!ValueType.IsVoid())
				{
					TypePerValue[i] = ValueType;
					const FPreparedType MergedType = MergePreparedTypes(CurrentType, ValueType);
					if (MergedType.IsVoid())
					{
						return Context.Errorf(TEXT("Mismatched types for local variable %s and %s"),
							CurrentType.GetName(),
							ValueType.GetName());
					}
					CurrentType = MergedType;
					check(NumValidTypes < NumLiveScopes);
					NumValidTypes++;
				}
			}
		}

		return true;
	};

	// First try to assign all the values we can
	if (!UpdateValueTypes())
	{
		return false;
	}

	// Assuming we have at least one value with a valid type, we use that to initialize our type
	const FPreparedType InitialType(CurrentType);
	if (!OutResult.SetType(Context, RequestedType, InitialType))
	{
		return false;
	}

	if (NumValidTypes < NumLiveScopes)
	{
		// Now try to assign remaining types that failed the first iteration 
		if (!UpdateValueTypes())
		{
			return false;
		}
		if (NumValidTypes < NumLiveScopes)
		{
			if (Chain.Last().Type == ELocalPHIChainType::Ddx || Chain.Last().Type == ELocalPHIChainType::Ddy)
			{
				// Don't treat failing analytic derivatives as errors. Just fall back to HW ones
				return false;
			}
			else
			{
				return Context.Error(TEXT("Failed to compute all types for LocalPHI"));
			}
		}

		if (CurrentType != InitialType)
		{
			// Update type again based on computing remaining values
			if (!OutResult.SetType(Context, RequestedType, CurrentType))
			{
				return false;
			}

			// Since we changed our type, need to update any dependant values again
			for (int32 i = 0; i < NumLiveScopes; ++i)
			{
				const FPreparedType& ValueType = Context.PrepareExpression(LiveValues[i], *EmitValueScopes[i], RequestedType);
				// Don't expect types to change *again*
				if (ValueType.IsVoid() || MergePreparedTypes(CurrentType, ValueType) != CurrentType)
				{
					return Context.Errorf(TEXT("Mismatched types for local variable %s and %s"),
						CurrentType.GetName(),
						ValueType.GetName());
				}
			}
		}
	}

	// Only incorporate scope evaluations if we are not sure whether all paths produce the same value
	bool bMergeScopeEvaluations = false;

	for (int32 Index = 1; Index < NumLiveScopes; ++Index)
	{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6385) // The first NumLiveScopes entries in LiveValues are valid
#endif
		if (LiveValues[Index] != LiveValues[Index - 1])
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		{
			bMergeScopeEvaluations = true;
			break;
		}
	}

	if (bMergeScopeEvaluations && IsConstantEvaluation(CurrentType.GetEvaluation(Scope, RequestedType)))
	{
		bool bFirst = true;
		bool bAllValuesSame = true;
		Shader::FValue TestValue;

		for (int32 Index = 0; Index < NumLiveScopes; ++Index)
		{
			const Shader::FValue ValueConst = LiveValues[Index]->GetValueConstant(Context, *EmitValueScopes[Index], RequestedType, TypePerValue[Index]);
			if (bFirst)
			{
				TestValue = ValueConst;
				bFirst = false;
			}
			else if (ValueConst != TestValue)
			{
				bAllValuesSame = false;
				break;
			}
		}

		bMergeScopeEvaluations = !bAllValuesSame;
	}

	if (bMergeScopeEvaluations)
	{
		for (int32 Index = 0; Index < NumLiveScopes; ++Index)
		{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 28182) // The first NumLiveScopes entries in EmitValueScopes are not null
#endif
			CurrentType.MergeEvaluation(EmitValueScopes[Index]->Evaluation);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		}
		verify(OutResult.SetType(Context, RequestedType, CurrentType));
	}

	return true;
}

namespace Private
{
struct FLocalPHILiveScopes
{
	FEmitScope* EmitDeclarationScope = nullptr;
	FEmitScope* EmitValueScopes[MaxNumPreviousScopes];
	const FExpression* LiveValues[MaxNumPreviousScopes];
	int32 NumScopes = 0;
	bool bCanForwardValue = true;
};

bool GetLiveScopes(FEmitContext& Context, const FExpressionLocalPHI& Expression, FLocalPHILiveScopes& OutLiveScopes, bool bPreparedTypeConstant)
{
	for (int32 ScopeIndex = 0; ScopeIndex < Expression.NumValues; ++ScopeIndex)
	{
		FEmitScope* EmitValueScope = Context.AcquireEmitScope(Expression.Scopes[ScopeIndex]);
		if (!IsScopeDead(EmitValueScope))
		{
			if (OutLiveScopes.bCanForwardValue)
			{
				for (int32 PrevScopeIndex = 0; PrevScopeIndex < OutLiveScopes.NumScopes; ++PrevScopeIndex)
				{
					if (Expression.Values[ScopeIndex] != OutLiveScopes.LiveValues[PrevScopeIndex])
					{
						OutLiveScopes.bCanForwardValue = false;
					}
				}
			}

			const int32 LiveScopeIndex = OutLiveScopes.NumScopes++;
			check(LiveScopeIndex < MaxNumPreviousScopes);
			OutLiveScopes.LiveValues[LiveScopeIndex] = Expression.Values[ScopeIndex];
			OutLiveScopes.EmitValueScopes[LiveScopeIndex] = EmitValueScope;
			OutLiveScopes.EmitDeclarationScope = FEmitScope::FindSharedParent(OutLiveScopes.EmitDeclarationScope, EmitValueScope);
			if (!OutLiveScopes.EmitDeclarationScope)
			{
				return Context.Error(TEXT("Invalid LocalPHI"));
			}
		}
	}

	// If this LocalPHI is constant, then either only one value scope is live and it is constant or all live scopes produce the same constant value
	OutLiveScopes.bCanForwardValue |= bPreparedTypeConstant;

	return OutLiveScopes.NumScopes > 0;
}
}

void FExpressionLocalPHI::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FXxHash64 Hash;
	{
		FHasher Hasher;
		AppendHash(Hasher, this);
		AppendHash(Hasher, &Scope);
		AppendHash(Hasher, RequestedType);
		Hash = Hasher.Finalize();
	}

	FEmitShaderExpression* const* PrevEmitExpression = Context.EmitLocalPHIMap.Find(Hash);
	FEmitShaderExpression* EmitExpression = PrevEmitExpression ? *PrevEmitExpression : nullptr;
	if (!EmitExpression)
	{
		// Find the outermost scope to declare our local variable
		Private::FLocalPHILiveScopes LiveScopes;
		if (!Private::GetLiveScopes(Context, *this, LiveScopes, false))
		{
			return;
		}

		if (LiveScopes.bCanForwardValue)
		{
			EmitExpression = LiveScopes.LiveValues[0]->GetValueShader(Context, Scope, RequestedType);
			Context.EmitLocalPHIMap.Add(Hash, EmitExpression);
		}
		else
		{
			// This is the first time we've emitted shader code for this PHI
			// Create an expression and add it to the map first, so if this is called recursively this path will only be taken the first time
			const int32 LocalPHIIndex = Context.NumExpressionLocalPHIs++;
			const Shader::FType LocalType = Context.GetResultType(this, RequestedType);

			EmitExpression = OutResult.Code = Context.EmitInlineExpression(Scope,
				LocalType,
				TEXT("LocalPHI%"), LocalPHIIndex);
			Context.EmitLocalPHIMap.Add(Hash, EmitExpression);

			FEmitShaderStatement* EmitDeclaration = nullptr;
			for (int32 i = 0; i < LiveScopes.NumScopes; ++i)
			{
				FEmitScope* EmitValueScope = LiveScopes.EmitValueScopes[i];
				if (EmitValueScope == LiveScopes.EmitDeclarationScope)
				{
					FEmitShaderExpression* ShaderValue = LiveScopes.LiveValues[i]->GetValueShader(Context, *EmitValueScope, LocalType);
					EmitDeclaration = Context.EmitStatement(*EmitValueScope, TEXT("% LocalPHI% = %;"),
						LocalType.GetName(),
						LocalPHIIndex,
						ShaderValue);
					break;
				}
			}
			if (!EmitDeclaration)
			{
				EmitDeclaration = Context.EmitStatement(*LiveScopes.EmitDeclarationScope, TEXT("% LocalPHI%;"),
					LocalType.GetName(),
					LocalPHIIndex);
			}

			FEmitShaderNode* Dependencies[MaxNumPreviousScopes] = { nullptr };
			int32 NumDependencies = 0;
			for (int32 i = 0; i < LiveScopes.NumScopes; ++i)
			{
				FEmitScope* EmitValueScope = LiveScopes.EmitValueScopes[i];
				if (EmitValueScope != LiveScopes.EmitDeclarationScope)
				{
					FEmitShaderExpression* ShaderValue = LiveScopes.LiveValues[i]->GetValueShader(Context, *EmitValueScope, LocalType);
					FEmitShaderStatement* EmitAssignment = Context.EmitStatementWithDependency(*EmitValueScope, EmitDeclaration, TEXT("LocalPHI% = %;"),
						LocalPHIIndex,
						ShaderValue);
					Dependencies[NumDependencies++] = EmitAssignment;
				}
			}

			// Fill in the expression's dependencies
			EmitExpression->Dependencies = MemStack::AllocateArrayView(*Context.Allocator, MakeArrayView(Dependencies, NumDependencies));
		}
	}

	OutResult.Code = EmitExpression;
}

void FExpressionLocalPHI::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	int32 ValueStackPosition = INDEX_NONE;
	for (int32 Index = Context.PreshaderLocalPHIScopes.Num() - 1; Index >= 0; --Index)
	{
		const FPreshaderLocalPHIScope* LocalPHIScope = Context.PreshaderLocalPHIScopes[Index];
		if (LocalPHIScope->ExpressionLocalPHI == this)
		{
			ValueStackPosition = LocalPHIScope->ValueStackPosition;
			break;
		}
	}

	const FPreparedType& PreparedType = Context.GetPreparedType(this, RequestedType);
	OutResult.Type = PreparedType.GetResultType();

	if (ValueStackPosition == INDEX_NONE)
	{
		Private::FLocalPHILiveScopes LiveScopes;
		if (!Private::GetLiveScopes(Context, *this, LiveScopes, IsConstantEvaluation(PreparedType.GetEvaluation(Scope, RequestedType))))
		{
			return;
		}

		if (LiveScopes.bCanForwardValue)
		{
			OutResult.Type = LiveScopes.LiveValues[0]->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
		}
		else
		{
			// Assign the initial value
			Context.PreshaderStackPosition++;
			OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(OutResult.Type);

			ValueStackPosition = Context.PreshaderStackPosition;
			const FPreshaderLocalPHIScope LocalPHIScope(this, ValueStackPosition);
			Context.PreshaderLocalPHIScopes.Add(&LocalPHIScope);

			FEmitPreshaderScope PreshaderScopes[MaxNumPreviousScopes];
			for (int32 i = 0; i < LiveScopes.NumScopes; ++i)
			{
				FEmitPreshaderScope& PreshaderScope = PreshaderScopes[i];
				PreshaderScope.Scope = LiveScopes.EmitValueScopes[i];
				PreshaderScope.Value = LiveScopes.LiveValues[i];
			}

			Context.EmitPreshaderScope(*LiveScopes.EmitDeclarationScope, RequestedType, MakeArrayView(PreshaderScopes, LiveScopes.NumScopes), OutResult.Preshader);
			verify(Context.PreshaderLocalPHIScopes.Pop(EAllowShrinking::No) == &LocalPHIScope);
			check(Context.PreshaderStackPosition == ValueStackPosition);
		}
	}
	else
	{
		const int32 PreshaderStackOffset = Context.PreshaderStackPosition - ValueStackPosition;
		check(PreshaderStackOffset >= 0 && PreshaderStackOffset <= 0xffff);

		Context.PreshaderStackPosition++;
		OutResult.Preshader.WriteOpcode(Shader::EPreshaderOpcode::PushValue).Write((uint16)PreshaderStackOffset);
	}
}

bool FExpressionLocalPHI::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	Private::FLocalPHILiveScopes LiveScopes;
	if (!Private::GetLiveScopes(Context, *this, LiveScopes, false) || !LiveScopes.bCanForwardValue)
	{
		// Cannot get if no live scope. Don't know which scope to use if there is more than one live
		return false;
	}

	return LiveScopes.LiveValues[0]->GetValueObject(Context, Scope, ObjectTypeName, OutObjectBase);
}

void FExpressionFunctionCall::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	check(Function->OutputExpressions.IsValidIndex(OutputIndex));

	Function->OutputExpressions[OutputIndex]->ComputeAnalyticDerivatives(Tree, OutResult);
}

bool FExpressionFunctionCall::PrepareValue(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FPrepareValueResult& OutResult) const
{
	FEmitScope* EmitFunctionScope = Context.PrepareScopeWithParent(Function->RootScope, Function->CalledScope);
	if (!EmitFunctionScope)
	{
		return false;
	}

	FPreparedType OutputType = Context.PrepareExpression(Function->OutputExpressions[OutputIndex], Scope, RequestedType);
	return OutResult.SetType(Context, RequestedType, OutputType);
}

void FExpressionFunctionCall::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	FEmitShaderNode* const* PrevDependency = Context.EmitFunctionMap.Find(Function);
	FEmitShaderNode* Dependency = PrevDependency ? *PrevDependency : nullptr;
	if (!Dependency)
	{
		// Inject the function's root scope at scope where it's called
		FEmitScope* EmitCalledScope = Context.AcquireEmitScope(Function->CalledScope);
		Dependency = Context.EmitNextScope(*EmitCalledScope, Function->RootScope);
		Context.EmitFunctionMap.Add(Function, Dependency);
	}

	FEmitShaderExpression* EmitFunctionOutput = Function->OutputExpressions[OutputIndex]->GetValueShader(Context, Scope, RequestedType);
	OutResult.Code = Context.EmitInlineExpressionWithDependency(Scope, Dependency, EmitFunctionOutput->Type, TEXT("%"), EmitFunctionOutput);
}

void FExpressionFunctionCall::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	OutResult.Type = Function->OutputExpressions[OutputIndex]->GetValuePreshader(Context, Scope, RequestedType, OutResult.Preshader);
}

bool FExpressionFunctionCall::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	return Function->OutputExpressions[OutputIndex]->GetValueObject(Context, Scope, ObjectTypeName, OutObjectBase);
}

FRequestedType::FRequestedType(const Shader::FType& InType, bool bDefaultRequest) : Type(InType)
{
	if (bDefaultRequest)
	{
		RequestedComponents.Init(true, InType.GetNumComponents());
	}
}

FRequestedType::FRequestedType(const FRequestedType& InType, bool bDefaultRequest)
	: Type(InType.Type)
{
	if (bDefaultRequest)
	{
		RequestedComponents = InType.RequestedComponents;
	}
}

Shader::EValueComponentType FRequestedType::GetValueComponentType() const
{
	if (Type.IsAny())
	{
		return Shader::EValueComponentType::Numeric;
	}
	else if (Type.IsNumeric())
	{
		return Shader::GetValueTypeDescription(Type).ComponentType;
	}
	return Shader::EValueComponentType::Void;
}

void FRequestedType::SetComponentRequest(int32 Index, bool bRequested)
{
	if (!Type.IsAny())
	{
		check(FMath::IsWithin(Index, 0, Type.GetNumComponents()));
		if (bRequested)
		{
			RequestedComponents.PadToNum(Index + 1, false);
		}
		if (RequestedComponents.IsValidIndex(Index))
		{
			RequestedComponents[Index] = bRequested;
		}
	}
}

void FRequestedType::SetFieldRequested(const Shader::FStructField* Field, bool bRequested)
{
	const int32 NumComponents = Field->GetNumComponents();
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		SetComponentRequest(Field->ComponentIndex + Index, bRequested);
	}
}

void FRequestedType::SetFieldRequested(const Shader::FStructField* Field, const FRequestedType& InRequest)
{
	if (Field->Type.IsNumericScalar())
	{
		// Scalar fields can replicate, so field is requested if we request any of xyzw
		if (InRequest.IsNumericVectorRequested())
		{
			SetComponentRequest(Field->ComponentIndex, true);
		}
	}
	else
	{
		const int32 NumFieldComponents = Field->GetNumComponents();
		for (int32 Index = 0; Index < NumFieldComponents; ++Index)
		{
			SetComponentRequest(Field->ComponentIndex + Index, InRequest.IsComponentRequested(Index));
		}
	}
}

FRequestedType FRequestedType::GetField(const Shader::FStructField* Field) const
{
	FRequestedType Result(Field->Type, false);
	const int32 NumComponents = Field->GetNumComponents();
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		Result.SetComponentRequest(Index, IsComponentRequested(Field->ComponentIndex + Index));
	}
	return Result;
}

EExpressionEvaluation FPreparedComponent::GetEvaluation(const FEmitScope& Scope) const
{
	EExpressionEvaluation Result = Evaluation;
	if (IsLoopEvaluation(Result))
	{
		// We only want to return a 'Loop' evaluation if we're within the loop's scope
		if (!Scope.HasParent(LoopScope))
		{
			switch (Result)
			{
			case EExpressionEvaluation::ConstantLoop: Result = EExpressionEvaluation::Constant; break;
			case EExpressionEvaluation::PreshaderLoop: Result = EExpressionEvaluation::Preshader; break;
			default: checkNoEntry(); break;
			}
		}
	}
	return Result;
}

void FPreparedComponent::SetLoopEvaluation(FEmitScope& Scope)
{
	Evaluation = MakeLoopEvaluation(Evaluation);
	if (IsLoopEvaluation(Evaluation))
	{
		LoopScope = FEmitScope::FindSharedParent(&Scope, LoopScope);
	}
}

FPreparedComponent CombineComponents(const FPreparedComponent& Lhs, const FPreparedComponent& Rhs)
{
	if (Lhs.IsNone())
	{
		return Rhs;
	}
	else if (Rhs.IsNone())
	{
		return Lhs;
	}
	else
	{
		FPreparedComponent Result;
		Result.Evaluation = CombineEvaluations(Lhs.Evaluation, Rhs.Evaluation);
		if (IsLoopEvaluation(Result.Evaluation))
		{
			Result.LoopScope = FEmitScope::FindSharedParent(Lhs.LoopScope, Rhs.LoopScope);
		}
		if (Lhs.Bounds.Min == Rhs.Bounds.Min)
		{
			Result.Bounds.Min = Lhs.Bounds.Min;
		}
		if (Lhs.Bounds.Max == Rhs.Bounds.Max)
		{
			Result.Bounds.Max = Lhs.Bounds.Max;
		}
		return Result;
	}
}

FPreparedType::FPreparedType(const Shader::FType& InType, const FPreparedComponent& InComponent) : Type(InType)
{
	if (!InComponent.IsNone())
	{
		const int32 NumComponents = InType.GetNumComponents();
		PreparedComponents.Reserve(NumComponents);
		for (int32 Index = 0; Index < NumComponents; ++Index)
		{
			SetComponent(Index, InComponent);
		}
	}
}

int32 FPreparedType::GetNumPreparedComponents() const
{
	auto Predicate = [](const FPreparedComponent& InComponent) { return !InComponent.IsNone(); };

	if (Type.IsVoid())
	{
		check(!PreparedComponents.ContainsByPredicate(Predicate));
		return 0;
	}

	const int32 MaxComponentIndex = PreparedComponents.FindLastByPredicate(Predicate);
	return (MaxComponentIndex != INDEX_NONE) ? (MaxComponentIndex + 1) : 0;
}

bool FPreparedType::IsEmpty() const
{
	return GetNumPreparedComponents() == 0;
}

Shader::FType FPreparedType::GetResultType() const
{
	// Only numeric vectors will adjust the number of components
	if (!Type.IsNumericVector())
	{
		return Type;
	}

	const int32 NumPreparedComponents = GetNumPreparedComponents();
	const Shader::FValueTypeDescription TypeDesc = Shader::GetValueTypeDescription(Type);
	return Shader::MakeValueType(TypeDesc.ComponentType, FMath::Max(NumPreparedComponents, 1));
}

Shader::EValueComponentType FPreparedType::GetValueComponentType() const
{
	check(!Type.IsAny());
	if (Type.IsNumeric())
	{
		return Shader::GetValueTypeDescription(Type).ComponentType;
	}
	return Shader::EValueComponentType::Void;
}

FRequestedType FPreparedType::GetRequestedType() const
{
	const int32 NumComponents = PreparedComponents.Num();
	FRequestedType Result;
	Result.Type = Type;
	Result.RequestedComponents.Init(false, NumComponents);
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		const FPreparedComponent& Component = PreparedComponents[Index];
		if (Component.IsRequested())
		{
			Result.SetComponentRequest(Index);
		}
	}
	return Result;
}

EExpressionEvaluation FPreparedType::GetEvaluation(const FEmitScope& Scope) const
{
	EExpressionEvaluation Result = EExpressionEvaluation::ConstantZero;
	for (int32 Index = 0; Index < PreparedComponents.Num(); ++Index)
	{
		Result = CombineEvaluations(Result, PreparedComponents[Index].GetEvaluation(Scope));
	}
	return Result;
}

EExpressionEvaluation FPreparedType::GetEvaluation(const FEmitScope& Scope, const FRequestedType& RequestedType) const
{
	EExpressionEvaluation Result = EExpressionEvaluation::ConstantZero;
	if (IsNumericScalar())
	{
		if (RequestedType.IsNumericVectorRequested())
		{
			Result = GetComponent(0).GetEvaluation(Scope);
		}
	}
	else
	{
		for (int32 Index = 0; Index < Type.GetNumComponents(); ++Index)
		{
			if (RequestedType.IsComponentRequested(Index))
			{
				Result = CombineEvaluations(Result, GetComponent(Index).GetEvaluation(Scope));
			}
		}
	}
	return Result;
}

EExpressionEvaluation FPreparedType::GetFieldEvaluation(const FEmitScope& Scope, const FRequestedType& RequestedType, int32 ComponentIndex, int32 NumComponents) const
{
	EExpressionEvaluation Result = EExpressionEvaluation::ConstantZero;
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		const FPreparedComponent Component = GetComponent(ComponentIndex + Index);
		if (RequestedType.IsComponentRequested(ComponentIndex + Index))
		{
			Result = CombineEvaluations(Result, Component.GetEvaluation(Scope));
		}
	}
	return Result;
}

FPreparedComponent FPreparedType::GetMergedComponent() const
{
	FPreparedComponent Result;
	for (const FPreparedComponent& Component : PreparedComponents)
	{
		Result = CombineComponents(Result, Component);
	}
	return Result;
}

FPreparedComponent FPreparedType::GetComponent(int32 Index) const
{
	if (Index >= 0)
	{
		// Scalar replicated into xyzw
		const int32 ComponentIndex = (Type.IsNumericScalar() && Index < 4) ? 0 : Index;
		if (ComponentIndex < PreparedComponents.Num())
		{
			return PreparedComponents[ComponentIndex];
		}

		if (ComponentIndex < Type.GetNumComponents())
		{
			return FPreparedComponent(EExpressionEvaluation::ConstantZero);
		}
	}

	return FPreparedComponent(EExpressionEvaluation::None);
}

void FPreparedType::EnsureNumComponents(int32 NumComponents)
{
	if (NumComponents > PreparedComponents.Num())
	{
		static_assert((uint8)EExpressionEvaluation::None == 0u, "Assume zero initializes to None");
		PreparedComponents.AddZeroed(NumComponents - PreparedComponents.Num());
	}
}

void FPreparedType::SetComponent(int32 Index, const FPreparedComponent& InComponent)
{
	check(FMath::IsWithin(Index, 0, Type.GetNumComponents()));
	if (InComponent.Evaluation != EExpressionEvaluation::None)
	{
		EnsureNumComponents(Index + 1);
	}
	if (PreparedComponents.IsValidIndex(Index))
	{
		PreparedComponents[Index] = InComponent;
	}
}

void FPreparedType::SetComponentBounds(int32 Index, const Shader::FComponentBounds Bounds)
{
	check(FMath::IsWithin(Index, 0, Type.GetNumComponents()));
	if (PreparedComponents.IsValidIndex(Index))
	{
		FPreparedComponent& Component = PreparedComponents[Index];
		if (!Component.IsNone())
		{
			Component.Bounds = Bounds;
		}
	}
}

void FPreparedType::MergeComponent(int32 Index, const FPreparedComponent& InComponent)
{
	check(FMath::IsWithin(Index, 0, Type.GetNumComponents()));
	if (InComponent.Evaluation != EExpressionEvaluation::None)
	{
		EnsureNumComponents(Index + 1);
	}
	if (PreparedComponents.IsValidIndex(Index))
	{
		PreparedComponents[Index] = CombineComponents(PreparedComponents[Index], InComponent);
	}
}

void FPreparedType::SetEvaluation(EExpressionEvaluation Evaluation)
{
	check(!IsLoopEvaluation(Evaluation));
	for (int32 Index = 0; Index < PreparedComponents.Num(); ++Index)
	{
		if (!PreparedComponents[Index].IsNone())
		{
			PreparedComponents[Index] = Evaluation;
		}
	}
}

void FPreparedType::MergeEvaluation(EExpressionEvaluation Evaluation)
{
	check(!IsLoopEvaluation(Evaluation));
	for (int32 Index = 0; Index < PreparedComponents.Num(); ++Index)
	{
		if (!PreparedComponents[Index].IsNone())
		{
			FPreparedComponent& Component = PreparedComponents[Index];
			Component = CombineComponents(Component, Evaluation);
		}
	}
}

void FPreparedType::SetLoopEvaluation(FEmitScope& Scope, const FRequestedType& RequestedType)
{
	if (Type.IsNumericScalar())
	{
		// If we're a scalar type, set loop evaluation if any of xyzw are requested
		if (RequestedType.IsNumericVectorRequested())
		{
			PreparedComponents[0].SetLoopEvaluation(Scope);
		}
	}
	else
	{
		for (int32 Index = 0; Index < PreparedComponents.Num(); ++Index)
		{
			if (RequestedType.IsComponentRequested(Index))
			{
				PreparedComponents[Index].SetLoopEvaluation(Scope);
			}
		}
	}
}

Shader::FComponentBounds FPreparedType::GetComponentBounds(int32 Index) const
{
	const FPreparedComponent Component = GetComponent(Index);
	if (Component.Evaluation == EExpressionEvaluation::None)
	{
		return Shader::FComponentBounds();
	}
	else if (Component.Evaluation == EExpressionEvaluation::ConstantZero)
	{
		return Shader::FComponentBounds(Shader::EComponentBound::Zero, Shader::EComponentBound::Zero);
	}

	const Shader::EValueComponentType ComponentType = Type.GetComponentType(Index);
	check(ComponentType != Shader::EValueComponentType::Void);

	const Shader::FValueComponentTypeDescription ComponentTypeDesc = Shader::GetValueComponentTypeDescription(ComponentType);
	const Shader::EComponentBound MinBound = Shader::MaxBound(Component.Bounds.Min, ComponentTypeDesc.Bounds.Min);
	const Shader::EComponentBound MaxBound = Shader::MinBound(Component.Bounds.Max, ComponentTypeDesc.Bounds.Max);
	return Shader::FComponentBounds(MinBound, MaxBound);
}

Shader::FComponentBounds FPreparedType::GetBounds(const FRequestedType& RequestedType) const
{
	Shader::FComponentBounds Result(Shader::EComponentBound::DoubleMax, Shader::EComponentBound::NegDoubleMax);
	if (IsNumericScalar())
	{
		if (RequestedType.IsNumericVectorRequested())
		{
			Result = GetComponentBounds(0);
		}
	}
	else
	{
		for (int32 Index = 0; Index < PreparedComponents.Num(); ++Index)
		{
			if (RequestedType.IsComponentRequested(Index))
			{
				const Shader::FComponentBounds ComponentBounds = GetComponentBounds(Index);
				Result.Min = Shader::MinBound(Result.Min, ComponentBounds.Min);
				Result.Max = Shader::MaxBound(Result.Max, ComponentBounds.Max);
			}
		}
	}
	return Result;
}

void FPreparedType::SetField(const Shader::FStructField* Field, const FPreparedType& FieldType)
{
	const int32 NumFieldComponents = Field->GetNumComponents();
	for (int32 Index = 0; Index < NumFieldComponents; ++Index)
	{
		SetComponent(Field->ComponentIndex + Index, FieldType.GetComponent(Index));
	}
}

FPreparedType FPreparedType::GetFieldType(const Shader::FStructField* Field) const
{
	FPreparedType Result(Field->Type);
	for (int32 Index = 0; Index < Field->GetNumComponents(); ++Index)
	{
		Result.SetComponent(Index, GetComponent(Field->ComponentIndex + Index));
	}
	return Result;
}

FPreparedType MergePreparedTypes(const FPreparedType& Lhs, const FPreparedType& Rhs)
{
	const Shader::FType UpdatedType = Shader::CombineTypes(Lhs.Type, Rhs.Type);
	if (UpdatedType.IsVoid())
	{
		// Mismatched types
		return FPreparedType();
	}

	const int32 NumComponents = UpdatedType.GetNumComponents();

	FPreparedType Result;
	Result.Type = UpdatedType;
	Result.PreparedComponents.Reset(NumComponents);
	for (int32 Index = 0; Index < NumComponents; ++Index)
	{
		const FPreparedComponent LhsComponent = Lhs.GetComponent(Index);
		const FPreparedComponent RhsComponent = Rhs.GetComponent(Index);
		Result.SetComponent(Index, CombineComponents(LhsComponent, RhsComponent));
	}

	return Result;
}

FPreparedType MakeNonLWCType(const FPreparedType& Type)
{
	FPreparedType Result(Type);
	if (Result.IsNumeric())
	{
		Result.Type = Shader::MakeNonLWCType(Result.Type);
	}
	return Result;
}

bool FPrepareValueResult::TryMergePreparedType(FEmitContext& Context, const Shader::FType& Type)
{
	if (Type.IsVoid())
	{
		return false;
	}

	check(!Type.IsGeneric());

	if (PreparedType.IsVoid())
	{
		PreparedType.PreparedComponents.Reset();
		PreparedType.Type = Type;
		return true;
	}

	const Shader::FType UpdatedType = Shader::CombineTypes(PreparedType.Type, Type);
	if (UpdatedType.IsVoid())
	{
		return Context.Errorf(TEXT("Expression was previously prepared as %s, now as %s, types are not compatible"),
			PreparedType.Type.GetName(), Type.GetName());
	}

	if (PreparedType.Type.GetNumComponents() != UpdatedType.GetNumComponents())
	{
		// Don't allow the number of components to change
		// Other expressions may depend on the previous number of components (especially scalar replication)
		return Context.Errorf(TEXT("Expression was previously prepared as %s, now as %s, number of components don't match"),
			PreparedType.Type.GetName(), Type.GetName());
	}

	PreparedType.Type = UpdatedType;
	return true;
}

bool FPrepareValueResult::SetTypeVoid()
{
	PreparedType.PreparedComponents.Reset();
	PreparedType.Type = Shader::EValueType::Void;
	return false;
}

bool FPrepareValueResult::SetType(FEmitContext& Context, const FRequestedType& RequestedType, EExpressionEvaluation Evaluation, const Shader::FType& InType)
{
	Shader::FType Type = InType.GetConcreteType();
	if (TryMergePreparedType(Context, Type))
	{
		if (Evaluation != EExpressionEvaluation::None)
		{
			if (Type.IsNumericScalar())
			{
				if (RequestedType.IsNumericVectorRequested())
				{
					PreparedType.MergeComponent(0, Evaluation);
				}
			}
			else
			{
				const int32 NumComponents = Type.GetNumComponents();
				for (int32 Index = 0; Index < NumComponents; ++Index)
				{
					if (RequestedType.IsComponentRequested(Index))
					{
						PreparedType.MergeComponent(Index, Evaluation);
					}
				}
			}
		}
		return true;
	}
	return false;
}

bool FPrepareValueResult::SetType(FEmitContext& Context, const FRequestedType& RequestedType, const FPreparedType& InPreparedType)
{
	Shader::FType Type = InPreparedType.Type.GetConcreteType();
	if (Type.IsNumericLWC())
	{
		const Shader::FComponentBounds Bounds = InPreparedType.GetBounds(RequestedType);
		if (Shader::IsWithinBounds(Bounds, Shader::GetValueComponentTypeDescription(Shader::EValueComponentType::Float).Bounds))
		{
			Type = Shader::MakeNonLWCType(Type);
		}
	}

	if (TryMergePreparedType(Context, Type))
	{
		if (Type.IsNumericScalar())
		{
			if (RequestedType.IsNumericVectorRequested())
			{
				PreparedType.MergeComponent(0, InPreparedType.GetComponent(0));
			}
		}
		else
		{
			const int32 NumComponents = Type.GetNumComponents();
			for (int32 Index = 0; Index < NumComponents; ++Index)
			{
				if (RequestedType.IsComponentRequested(Index))
				{
					PreparedType.MergeComponent(Index, InPreparedType.GetComponent(Index));
				}
			}
		}
		return true;
	}
	return false;
}

void FStatement::EmitShader(FEmitContext& Context, FEmitScope& Scope) const
{
	check(false);
}

void FStatement::EmitPreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> Scopes, Shader::FPreshaderData& OutPreshader) const
{
	check(false);
}

void FExpression::ComputeAnalyticDerivatives(FTree& Tree, FExpressionDerivatives& OutResult) const
{
	// nop
}

const FExpression* FExpression::ComputePreviousFrame(FTree& Tree, const FRequestedType& RequestedType) const
{
	return nullptr;
}

const FExpression* FExpression::GetPreviewExpression(FTree& Tree) const
{
	return this;
}

void FExpression::EmitValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValueShaderResult& OutResult) const
{
	check(false);
}

void FExpression::EmitValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, FEmitValuePreshaderResult& OutResult) const
{
	check(false);
}

bool FExpression::EmitValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	return false;
}

bool FExpression::EmitCustomHLSLParameter(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, const TCHAR* ParameterName, FEmitCustomHLSLParameterResult& OutResult) const
{
	return false;
}

FEmitShaderExpression* FExpression::GetValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const FPreparedType& PreparedType, const Shader::FType& ResultType) const
{
	FXxHash64 Hash;
	{
		FHasher Hasher;
		AppendHash(Hasher, this);
		AppendHash(Hasher, &Scope);
		AppendHash(Hasher, RequestedType);
		AppendHash(Hasher, ResultType);
		Hash = Hasher.Finalize();
	}

	FEmitShaderExpression** Found = Context.EmitValueMap.Find(Hash);
	if (Found)
	{
		return *Found;
	}

	FEmitOwnerScope OwnerScope(Context, this);

	const EExpressionEvaluation Evaluation = PreparedType.GetEvaluation(Scope, RequestedType);
	check(Evaluation != EExpressionEvaluation::None && Evaluation != EExpressionEvaluation::Unknown);

	FEmitShaderExpression* Value = nullptr;
	if (Evaluation == EExpressionEvaluation::ConstantZero)
	{
		Value = Context.EmitConstantZero(Scope, ResultType);
	}
	else if (Evaluation == EExpressionEvaluation::Constant || Evaluation == EExpressionEvaluation::Preshader)
	{
		Value = Context.EmitPreshaderOrConstant(Scope, RequestedType, ResultType, this);
	}
	else
	{
		FEmitValueShaderResult Result;
		EmitValueShader(Context, Scope, RequestedType, Result);
		check(Result.Code);
		Value = Result.Code;
	}
	Value = Context.EmitCast(Scope, Value, ResultType);

	Context.EmitValueMap.Add(Hash, Value);
	return Value;
}

FEmitShaderExpression* FExpression::GetValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const Shader::FType& ResultType) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValueShader(Context, Scope, RequestedType, PreparedType, ResultType);
}

FEmitShaderExpression* FExpression::GetValueShader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValueShader(Context, Scope, RequestedType, PreparedType, PreparedType.GetResultType());
}

FEmitShaderExpression* FExpression::GetValueShader(FEmitContext& Context, FEmitScope& Scope, const Shader::FType& ResultType) const
{
	return GetValueShader(Context, Scope, ResultType, ResultType);
}

FEmitShaderExpression* FExpression::GetValueShader(FEmitContext& Context, FEmitScope& Scope, Shader::EValueType ResultType) const
{
	return GetValueShader(Context, Scope, ResultType, ResultType);
}

Shader::FType FExpression::GetValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const FPreparedType& PreparedType, const Shader::FType& ResultType, Shader::FPreshaderData& OutPreshader) const
{
	FEmitOwnerScope OwnerScope(Context, this);
	
	const int32 PrevStackPosition = Context.PreshaderStackPosition;
	const EExpressionEvaluation Evaluation = PreparedType.GetEvaluation(Scope, RequestedType);
	check(Evaluation != EExpressionEvaluation::None && Evaluation != EExpressionEvaluation::Unknown);

	FEmitValuePreshaderResult Result(OutPreshader);
	if (Evaluation == EExpressionEvaluation::ConstantZero)
	{
		check(!ResultType.IsVoid());
		Context.PreshaderStackPosition++;
		Result.Type = ResultType;
		OutPreshader.WriteOpcode(Shader::EPreshaderOpcode::ConstantZero).Write(ResultType);
	}
	else if (Evaluation == EExpressionEvaluation::Constant)
	{
		const Shader::FValue ConstantValue = GetValueConstant(Context, Scope, RequestedType, ResultType);
		Context.PreshaderStackPosition++;
		OutPreshader.WriteOpcode(Shader::EPreshaderOpcode::Constant).Write(ConstantValue);
		Result.Type = ConstantValue.Type;
	}
	else
	{
		check(Evaluation != EExpressionEvaluation::Shader);
		EmitValuePreshader(Context, Scope, RequestedType, Result);
	}
	check(Context.PreshaderStackPosition == PrevStackPosition + 1);
	return Result.Type;
}

Shader::FType FExpression::GetValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const Shader::FType& ResultType, Shader::FPreshaderData& OutPreshader) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValuePreshader(Context, Scope, RequestedType, PreparedType, ResultType, OutPreshader);
}

Shader::FType FExpression::GetValuePreshader(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, Shader::FPreshaderData& OutPreshader) const
{
	const FPreparedType& PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValuePreshader(Context, Scope, RequestedType, PreparedType, PreparedType.GetResultType(), OutPreshader);
}

Shader::FType FExpression::GetValuePreshader(FEmitContext& Context, FEmitScope& Scope, const Shader::FType& ResultType, Shader::FPreshaderData& OutPreshader) const
{
	return GetValuePreshader(Context, Scope, ResultType, ResultType, OutPreshader);
}

Shader::FType FExpression::GetValuePreshader(FEmitContext& Context, FEmitScope& Scope, Shader::EValueType ResultType, Shader::FPreshaderData& OutPreshader) const\
{
	return GetValuePreshader(Context, Scope, ResultType, ResultType, OutPreshader);
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const FPreparedType& PreparedType, const Shader::FType& ResultType) const
{
	FEmitOwnerScope OwnerScope(Context, this);

	const EExpressionEvaluation Evaluation = PreparedType.GetEvaluation(Scope, RequestedType);
	if (Evaluation == EExpressionEvaluation::ConstantZero)
	{
		return Shader::FValue(ResultType);
	}
	else
	{
		check(Evaluation == EExpressionEvaluation::Constant || Evaluation == EExpressionEvaluation::ConstantLoop);
		Shader::FPreshaderData ConstantPreshader;
		{
			const int32 PrevPreshaderStackPosition = Context.PreshaderStackPosition;
			FEmitValuePreshaderResult PreshaderResult(ConstantPreshader);
			EmitValuePreshader(Context, Scope, RequestedType, PreshaderResult);
			check(Context.PreshaderStackPosition == PrevPreshaderStackPosition + 1);
			Context.PreshaderStackPosition--;
		}

		// Evaluate the constant preshader and store its value
		Shader::FPreshaderStack Stack;
		const Shader::FPreshaderValue PreshaderValue = ConstantPreshader.EvaluateConstant(*Context.Material, Stack);
		Shader::FValue Result = PreshaderValue.AsShaderValue(Context.TypeRegistry);
		return Shader::Cast(Result, ResultType);
	}
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const FPreparedType& PreparedType) const
{
	return GetValueConstant(Context, Scope, RequestedType, PreparedType, PreparedType.GetResultType());
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType, const Shader::FType& ResultType) const
{
	const FPreparedType PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValueConstant(Context, Scope, RequestedType, PreparedType, ResultType);
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FRequestedType& RequestedType) const
{
	const FPreparedType PreparedType = Context.GetPreparedType(this, RequestedType);
	return GetValueConstant(Context, Scope, RequestedType, PreparedType, PreparedType.GetResultType());
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FPreparedType& PreparedType, const Shader::FType& ResultType) const
{
	return GetValueConstant(Context, Scope, FRequestedType(ResultType), PreparedType, ResultType);
}

Shader::FValue FExpression::GetValueConstant(FEmitContext& Context, FEmitScope& Scope, const FPreparedType& PreparedType, Shader::EValueType ResultType) const
{
	return GetValueConstant(Context, Scope, FRequestedType(ResultType), PreparedType, ResultType);
}

bool FExpression::GetValueObject(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName, void* OutObjectBase) const
{
	const FPreparedType PreparedType = Context.GetPreparedType(this, FRequestedType(ObjectTypeName));
	check(PreparedType.Type.ObjectType == ObjectTypeName);
	return EmitValueObject(Context, Scope, ObjectTypeName, OutObjectBase);
}

bool FExpression::CheckObjectSupportsCustomHLSL(FEmitContext& Context, FEmitScope& Scope, const FName& ObjectTypeName) const
{
	const FPreparedType PreparedType = Context.GetPreparedType(this, FRequestedType(ObjectTypeName));
	check(PreparedType.Type.ObjectType == ObjectTypeName);

	FEmitCustomHLSLParameterResult UnusedResult;
	return EmitCustomHLSLParameter(Context, Scope, ObjectTypeName, nullptr, UnusedResult);
}

void FExpression::GetObjectCustomHLSLParameter(FEmitContext& Context,
	FEmitScope& Scope,
	const FName& ObjectTypeName,
	const TCHAR* ParameterName,
	FStringBuilderBase& OutDeclarationCode,
	FStringBuilderBase& OutForwardCode) const
{
	FEmitCustomHLSLParameterResult Result;
	Result.DeclarationCode = &OutDeclarationCode;
	Result.ForwardCode = &OutForwardCode;
	const bool bResult = EmitCustomHLSLParameter(Context, Scope, ObjectTypeName, ParameterName, Result);
	check(bResult);
}

bool FScope::HasParentScope(const FScope& InParentScope) const
{
	const FScope* CurrentScope = this;
	while (CurrentScope)
	{
		if (CurrentScope == &InParentScope)
		{
			return true;
		}
		CurrentScope = CurrentScope->ParentScope;
	}
	return false;
}

void FScope::AddPreviousScope(FScope& Scope)
{
	check(NumPreviousScopes < MaxNumPreviousScopes);
	PreviousScope[NumPreviousScopes++] = &Scope;
}

FTree* FTree::Create(FMemStackBase& Allocator)
{
	FTree* Tree = new(Allocator) FTree();
	Tree->Allocator = &Allocator;
	Tree->RootScope = Tree->NewNode<FScope>();
	return Tree;
}

void FTree::Destroy(FTree* Tree)
{
	if (Tree)
	{
		FNode* Node = Tree->Nodes;
		while (Node)
		{
			FNode* Next = Node->NextNode;
			Node->~FNode();
			Node = Next;
		}
		Tree->~FTree();
		FMemory::Memzero(*Tree);
	}
}

void FTree::PushOwner(UObject* Owner)
{
	OwnerStack.Add(Owner);
}

UObject* FTree::PopOwner()
{
	return OwnerStack.Pop(EAllowShrinking::No);
}

UObject* FTree::GetCurrentOwner() const
{
	return (OwnerStack.Num() > 0) ? OwnerStack.Last() : nullptr;
}

bool FTree::Finalize()
{
	// Resolve values for any PHI nodes that were generated
	// Resolving a PHI may produce additional PHIs
	while (PHIExpressions.Num() > 0)
	{
		FExpressionLocalPHI* Expression = PHIExpressions.Pop(EAllowShrinking::No);
		for (int32 i = 0; i < Expression->NumValues; ++i)
		{
			const FExpression* LocalValue = AcquireLocal(*Expression->Scopes[i], Expression->LocalName);
			if (!LocalValue)
			{
				//Errorf(TEXT("Local %s is not assigned on all control paths"), *Expression->LocalName.ToString());
				return false;
			}

			for(const FLocalPHIChainEntry& Entry : Expression->Chain)
			{
				const ELocalPHIChainType ChainType = Entry.Type;
				if (ChainType == ELocalPHIChainType::Ddx || ChainType == ELocalPHIChainType::Ddy)
				{
					const FExpressionDerivatives Derivatives = GetAnalyticDerivatives(LocalValue);
					LocalValue = (ChainType == ELocalPHIChainType::Ddx) ? Derivatives.ExpressionDdx : Derivatives.ExpressionDdy;
				}
				else
				{
					check(ChainType == ELocalPHIChainType::PreviousFrame && LocalValue);

					const FExpression* PrevLocalValue = LocalValue;
					LocalValue = GetPreviousFrame(LocalValue, Entry.RequestedType);

					// TODO: Revisit. Is this correct? The intention is falling back to current frame if previous frame value is invalid
					if (!LocalValue)
					{
						LocalValue = PrevLocalValue;
					}
				}
			}

			// May be nullptr if derivatives are not valid
			Expression->Values[i] = LocalValue;
		}
	}

	PHIExpressions.Shrink();
	return true;
}

bool FTree::EmitShader(FEmitContext& Context, FStringBuilderBase& OutCode) const
{
	FEmitScope* EmitRootScope = Context.InternalEmitScope(RootScope);
	if (EmitRootScope)
	{
		// Link all nodes to their proper scope
		for (FEmitShaderNode* EmitNode : Context.EmitNodes)
		{
			FEmitScope* EmitScope = EmitNode->Scope;
			if (EmitScope)
			{
				EmitNode->NextScopedNode = EmitScope->FirstNode;
				EmitScope->FirstNode = EmitNode;
			}
		}

		{
			FEmitShaderScopeStack Stack;
			TStringBuilder<2048> ScopeCode;
			Stack.Emplace(EmitRootScope, 1, ScopeCode);
			EmitRootScope->EmitShaderCode(Stack);
			check(Stack.Num() == 1);
			OutCode.Append(ScopeCode.ToView());
		}
	}

	Context.Finalize();

	return true;
}

void FTree::RegisterNode(FNode* Node)
{
	Node->NextNode = Nodes;
	Nodes = Node;
}

FExpression* FTree::FindExpression(FXxHash64 Hash)
{
	FExpression* const* FoundExpression = ExpressionMap.Find(Hash);
	if (FoundExpression)
	{
		return *FoundExpression;
	}
	return nullptr;
}

void FTree::RegisterExpression(FExpression* Expression, FXxHash64 Hash)
{
	Expression->Owners.Add(GetCurrentOwner());
	ExpressionMap.Add(Hash, Expression);
}

void FTree::RegisterExpression(FExpressionLocalPHI* Expression, FXxHash64 Hash)
{
	PHIExpressions.Add(Expression);
	RegisterExpression(static_cast<FExpression*>(Expression), Hash);
}

void FTree::AddCurrentOwner(FExpression* Expression)
{
	Expression->Owners.AddUnique(GetCurrentOwner());
}

void FTree::RegisterStatement(FScope& Scope, FStatement* Statement)
{
	check(!Scope.ContainedStatement)
	check(!Statement->ParentScope);
	Statement->Owner = GetCurrentOwner();
	Statement->ParentScope = &Scope;
	Scope.ContainedStatement = Statement;
}

void FTree::AssignLocal(FScope& Scope, const FName& LocalName, const FExpression* Value)
{
	check(Value);
	Scope.LocalMap.Add(LocalName, Value);
}

const FExpression* FTree::AcquireLocal(FScope& Scope, const FName& LocalName)
{
	FExpression const* const* FoundExpression = Scope.LocalMap.Find(LocalName);
	if (FoundExpression)
	{
		check(*FoundExpression);
		return *FoundExpression;
	}

	const TArrayView<FScope*> PreviousScopes = Scope.GetPreviousScopes();
	if (PreviousScopes.Num() > 1)
	{
		const FExpression* Expression = NewExpression<FExpressionLocalPHI>(LocalName, PreviousScopes);
		Scope.LocalMap.Add(LocalName, Expression);
		return Expression;
	}

	if (PreviousScopes.Num() == 1)
	{
		return AcquireLocal(*PreviousScopes[0], LocalName);
	}

	return nullptr;
}

const FExpression* FTree::NewFunctionCall(FScope& Scope, FFunction* Function, int32 OutputIndex)
{
	FScope* CalledScope = &Scope;
	if (Function->CalledScope)
	{
		CalledScope = FScope::FindSharedParent(CalledScope, Function->CalledScope);
		check(CalledScope);
	}
	Function->CalledScope = CalledScope;
	return NewExpression<FExpressionFunctionCall>(Function, OutputIndex);
}

FExpressionDerivatives FTree::GetAnalyticDerivatives(const FExpression* InExpression)
{
	FExpressionDerivatives Derivatives;
	if (InExpression)
	{
		//FOwnerScope OwnerScope(*this, InExpression->GetOwner()); // Associate any newly created nodes with the same owner as the input expression
		
		FExpressionDerivatives* Found = ExpressionDerivativesMap.Find(InExpression);
		if (Found)
		{
			Derivatives = *Found;
		}
		else
		{
			InExpression->ComputeAnalyticDerivatives(*this, Derivatives);
			ExpressionDerivativesMap.Add(InExpression, Derivatives);
		}
	}
	return Derivatives;
}

const FExpression* FTree::GetPreviousFrame(const FExpression* InExpression, const FRequestedType& RequestedType)
{
	const FExpression* Result = InExpression;
	if (Result && !RequestedType.IsVoid())
	{
		//FOwnerScope OwnerScope(*this, InExpression->GetOwner()); // Associate any newly created nodes with the same owner as the input expression
		FHasher Hasher;
		Hasher.AppendData(&InExpression, sizeof(InExpression));
		AppendHash(Hasher, RequestedType);
		const FXxHash64 KeyHash = Hasher.Finalize();
		const FExpression** Found = PreviousFrameExpressionMap.Find(KeyHash);

		const FExpression* PrevFrameExpression;
		if (Found)
		{
			PrevFrameExpression = *Found;
		}
		else
		{
			PrevFrameExpression = InExpression->ComputePreviousFrame(*this, RequestedType);
			PreviousFrameExpressionMap.Add(KeyHash, PrevFrameExpression);
		}

		if (PrevFrameExpression)
		{
			Result = PrevFrameExpression;
		}
	}
	return Result;
}

const FExpression* FTree::GetPreview(const FExpression* InExpression)
{
	// Cache result?
	return InExpression ? InExpression->GetPreviewExpression(*this) : nullptr;
}

FScope* FTree::NewScope(FScope& Scope)
{
	FScope* NewScope = NewNode<FScope>();
	NewScope->ParentScope = &Scope;
	NewScope->NestedLevel = Scope.NestedLevel + 1;
	NewScope->NumPreviousScopes = 0;
	return NewScope;
}

FScope* FTree::NewOwnedScope(FStatement& Owner)
{
	FScope* NewScope = NewNode<FScope>();
	NewScope->OwnerStatement = &Owner;
	NewScope->ParentScope = Owner.ParentScope;
	NewScope->NestedLevel = NewScope->ParentScope->NestedLevel + 1;
	NewScope->NumPreviousScopes = 0;
	return NewScope;
}

FFunction* FTree::NewFunction()
{
	FFunction* NewFunction = NewNode<FFunction>();
	NewFunction->RootScope = NewNode<FScope>();
	return NewFunction;
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
