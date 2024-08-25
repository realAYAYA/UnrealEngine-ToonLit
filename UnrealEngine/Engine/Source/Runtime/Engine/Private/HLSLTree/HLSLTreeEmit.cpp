// Copyright Epic Games, Inc. All Rights Reserved.
#if WITH_EDITOR

#include "HLSLTree/HLSLTreeEmit.h"
#include "HLSLTree/HLSLTree.h"
#include "Misc/MemStackUtility.h"
#include "MaterialShared.h" // TODO - split preshader out into its own module
#include "Misc/LargeWorldRenderPosition.h"
#include "Shader/PreshaderTypes.h"

namespace UE::HLSLTree
{

FEmitShaderNode::FEmitShaderNode(FEmitScope& InScope, TArrayView<FEmitShaderNode*> InDependencies)
	: Scope(&InScope)
	, Dependencies(InDependencies)
{
	for (FEmitShaderNode* Dependency : InDependencies)
	{
		check(Dependency);
	}
}

namespace Private
{

void WriteIndent(int32 IndentLevel, FStringBuilderBase& InOutString)
{
	const int32 Offset = InOutString.AddUninitialized(IndentLevel);
	TCHAR* Result = InOutString.GetData() + Offset;
	for (int32 i = 0; i < IndentLevel; ++i)
	{
		*Result++ = TCHAR('\t');
	}
}

void EmitShaderCode(FEmitShaderNode* EmitNode, FEmitShaderScopeStack& Stack)
{
	if (EmitNode && EmitNode->Scope)
	{
		const FEmitScope* Scope = EmitNode->Scope;
		FEmitShaderScopeEntry EmitEntry;
		for (int32 Index = Stack.Num() - 1; Index >= 0; --Index)
		{
			FEmitShaderScopeEntry& CheckEntry = Stack[Index];
			if (CheckEntry.Scope == Scope)
			{
				EmitEntry = CheckEntry;
				break;
			}
		}

		// LocalPHI can sometimes generate circular dependency on expressions that execute in the future
		// Should revist this once dependencies are cleaned up
		if (EmitEntry.Code)
		{
			EmitNode->Scope = nullptr; // only emit code once
			for (FEmitShaderNode* Dependency : EmitNode->Dependencies)
			{
				Private::EmitShaderCode(Dependency, Stack);
			}
			EmitNode->EmitShaderCode(Stack, EmitEntry.Indent, *EmitEntry.Code);
		}
	}
}

} // namespace Private

void FEmitShaderExpression::EmitShaderCode(FEmitShaderScopeStack& Stack, int32 Indent, FStringBuilderBase& OutString)
{
	// Don't need a declaration for inline values
	if (!IsInline())
	{
		Private::WriteIndent(Indent, OutString);
		OutString.Appendf(TEXT("const %s %s = %s;\n"), Type.GetName(), Reference, Value);
	}
}

void FEmitShaderStatement::EmitShaderCode(FEmitShaderScopeStack& Stack, int32 Indent, FStringBuilderBase& OutString)
{
	TStringBuilder<2048> ScopeCode;
	for (int32 i = 0; i < 2; ++i)
	{
		bool bNeedToCloseScope = false;
		int32 NestedScopeIndent = Indent;

		if (Code[i].Len() > 0)
		{
			Private::WriteIndent(Indent, ScopeCode);
			ScopeCode.Append(Code[i]);
			ScopeCode.AppendChar(TEXT('\n'));

			// If ScopeFormat is set to 'Scoped', we need to emit an empty {}, even if our NestedScope is nullptr
			if (ScopeFormat == EEmitScopeFormat::Scoped)
			{
				Private::WriteIndent(Indent, ScopeCode);
				ScopeCode.Append(TEXT("{\n"));
				bNeedToCloseScope = true;
				NestedScopeIndent++;
			}
		}

		FEmitScope* NestedScope = NestedScopes[i];
		if (NestedScope)
		{
			Stack.Emplace(NestedScopes[i], NestedScopeIndent, ScopeCode);
			NestedScope->EmitShaderCode(Stack);
			Stack.Pop(EAllowShrinking::No);
		}

		if (bNeedToCloseScope)
		{
			Private::WriteIndent(Indent, ScopeCode);
			ScopeCode.Append(TEXT("}\n"));
		}
	}
	OutString.Append(ScopeCode.ToView());
}

void FEmitScope::EmitShaderCode(FEmitShaderScopeStack& Stack)
{
	FEmitShaderNode* EmitNode = FirstNode;
	while (EmitNode)
	{
		Private::EmitShaderCode(EmitNode, Stack);
		EmitNode = EmitNode->NextScopedNode;
	}
}

FEmitScope* FEmitScope::FindSharedParent(FEmitScope* Lhs, FEmitScope* Rhs)
{
	FEmitScope* Scope0 = Lhs;
	FEmitScope* Scope1 = Rhs;
	if (!Scope0)
	{
		return Scope1;
	}

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

bool FEmitScope::HasParent(const FEmitScope* InParent) const
{
	if (InParent)
	{
		const FEmitScope* CheckParent = this;
		while (CheckParent)
		{
			if (CheckParent == InParent)
			{
				return true;
			}
			CheckParent = CheckParent->ParentScope;
		}
	}
	return false;
}

bool FEmitScope::IsLoop() const
{
	return OwnerStatement && OwnerStatement->IsLoop();
}

FEmitScope* FEmitScope::FindLoop()
{
	FEmitScope* CheckParent = this;
	while (CheckParent)
	{
		if (CheckParent->IsLoop())
		{
			return CheckParent;
		}
		CheckParent = CheckParent->ParentScope;
	}
	return nullptr;
}

FEmitContext::FEmitContext(FMemStackBase& InAllocator,
	const FTargetParameters& InTargetParameters,
	FErrorHandlerInterface& InErrors,
	const Shader::FStructTypeRegistry& InTypeRegistry)
	: Allocator(&InAllocator)
	, Errors(&InErrors)
	, TypeRegistry(&InTypeRegistry)
	, TargetParameters(InTargetParameters)
{
}

FEmitContext::~FEmitContext()
{
	for (auto& It : CustomDataMap)
	{
		delete It.Value;
	}
	for (TMap<FXxHash64, FPrepareValueResult*>::TIterator It(PrepareValueMap); It; ++It)
	{
		FPrepareValueResult* Value = It.Value();
		if (Value)
		{
			Value->~FPrepareValueResult();
		}
	}
	ResetPastRequestedTypes();
}

bool FEmitContext::InternalError(FStringView ErrorMessage)
{
	if (Errors)
	{
		TConstArrayView<UObject*> CurrentOwners;
		if (OwnerStack.Num() > 0)
		{
			CurrentOwners = OwnerStack.Last()->GetOwners();
		}
		Errors->AddErrorInternal(CurrentOwners, ErrorMessage);
	}
	NumErrors++;
	return false;
}

void FEmitContext::InternalRegisterData(FXxHash64 Hash, FCustomDataWrapper* Data)
{
	CustomDataMap.Add(Hash, Data);
}

FEmitContext::FCustomDataWrapper* FEmitContext::InternalFindData(FXxHash64 Hash) const
{
	FCustomDataWrapper* const* PrevData = CustomDataMap.Find(Hash);
	return PrevData ? *PrevData : nullptr;
}

namespace Private
{
void EmitCustomHLSL(const FEmitCustomHLSL& EmitCustomHLSL, const TCHAR* ParametersTypeName, FStringBuilderBase& OutCode)
{
	const Shader::FStructType* OutputStruct = EmitCustomHLSL.OutputType;
	
	OutCode.Append(EmitCustomHLSL.DeclarationCode);
	OutCode.Append(TEXT("\n"));

	// Function that wraps custom HLSL, provides the interface expected by custom HLSL
	// * LWC inputs have both LWC version and non-LWC version
	// * First output is given through return value, additional outputs use inout function parameters
	OutCode.Appendf(TEXT("%s CustomExpressionInternal%d(%s Parameters"), OutputStruct->Fields[0].Type.GetName(), EmitCustomHLSL.Index, ParametersTypeName);
	for (const FEmitCustomHLSLInput& Input : EmitCustomHLSL.Inputs)
	{
		Shader::FType InputType = Input.Type;
		if (InputType.IsObject())
		{
			check(Input.ObjectDeclarationCode.Len() > 0);
			OutCode.Append(TEXT(", "));
			OutCode.Append(Input.ObjectDeclarationCode);
		}
		else
		{
			if (InputType.IsNumericLWC())
			{
				// Add an additional input for LWC type with the LWC-prefix
				OutCode.Appendf(TEXT(", %s WS"), InputType.GetName());
				OutCode.Append(Input.Name);
				// Regular, unprefixed input uses non-LWC type
				InputType = InputType.GetNonLWCType();
			}

			OutCode.Appendf(TEXT(", %s "), InputType.GetName());
			OutCode.Append(Input.Name);
		}
	}
	for (int32 OutputIndex = 1; OutputIndex < OutputStruct->Fields.Num(); ++OutputIndex)
	{
		const Shader::FStructField& OutputField = OutputStruct->Fields[OutputIndex];
		OutCode.Appendf(TEXT(", inout %s %s"), OutputField.Type.GetName(), OutputField.Name);
	}
	OutCode.Append(TEXT(")\n{\n"));
	OutCode.Append(EmitCustomHLSL.FunctionCode);
	OutCode.Append(TEXT("\n}\n"));

	// Function that calls the above wrapper, provides the interface expected by HLSLTree
	// * All inputs types are passed through directly
	// * All outputs are provided through a 'struct' type
	OutCode.Appendf(TEXT("%s CustomExpression%d(%s Parameters"), OutputStruct->Name, EmitCustomHLSL.Index, ParametersTypeName);
	for (const FEmitCustomHLSLInput& Input : EmitCustomHLSL.Inputs)
	{
		Shader::FType InputType = Input.Type;
		if (InputType.IsObject())
		{
			OutCode.Append(TEXT(", "));
			OutCode.Append(Input.ObjectDeclarationCode);
		}
		else
		{
			OutCode.Appendf(TEXT(", %s "), InputType.GetName());
			OutCode.Append(Input.Name);
		}
	}
	OutCode.Append(TEXT(")\n{\n"));
	OutCode.Appendf(TEXT("\t%s Result = (%s)0;\n"), OutputStruct->Name, OutputStruct->Name);
	OutCode.Appendf(TEXT("\tResult.%s = CustomExpressionInternal%d(Parameters"), OutputStruct->Fields[0].Name, EmitCustomHLSL.Index);
	for (const FEmitCustomHLSLInput& Input : EmitCustomHLSL.Inputs)
	{
		OutCode.Append(TEXT(", "));
		if (Input.Type.IsObject() &&
			Input.ObjectForwardCode.Len() > 0)
		{
			// CustomHLSL object parameters may have dedicated code to forward parameters from 1 function to another
			OutCode.Append(Input.ObjectForwardCode);
		}
		else
		{
			OutCode.Append(Input.Name);
			if (Input.Type.IsNumericLWC())
			{
				OutCode.Append(TEXT(", WSDemote("));
				OutCode.Append(Input.Name);
				OutCode.Append(TEXT(")"));
			}
		}
	}
	for (int32 OutputIndex = 1; OutputIndex < OutputStruct->Fields.Num(); ++OutputIndex)
	{
		const Shader::FStructField& OutputField = OutputStruct->Fields[OutputIndex];
		OutCode.Appendf(TEXT(", Result.%s"), OutputField.Name);
	}
	OutCode.Append(TEXT(");\n"));
	OutCode.Append(TEXT("\treturn Result;\n"));
	OutCode.Append(TEXT("}\n"));
}

FXxHash64 GetPrepareValueHash(const FExpression* Expression, bool bRequestedStruct)
{
	FXxHash64Builder Hasher;
	Hasher.Update(&Expression, sizeof(Expression));
	Hasher.Update(&bRequestedStruct, sizeof(bRequestedStruct));
	return Hasher.Finalize();
}

FXxHash64 GetPrepareValueHash(const FExpression* Expression, const FRequestedType& RequestedType)
{
	return GetPrepareValueHash(Expression, RequestedType.IsStruct());
}
} // namespace Private

void FEmitContext::EmitDeclarationsCode(FStringBuilderBase& OutCode)
{
	for(const auto& It : EmitCustomHLSLMap)
	{
		const FEmitCustomHLSL& EmitCustomHLSL = It.Value;
		uint32 ShaderFrequencyMask = EmitCustomHLSL.ShaderFrequencyMask;
		if (ShaderFrequencyMask & (1u << SF_Vertex))
		{
			// Emit VS version if needed
			Private::EmitCustomHLSL(EmitCustomHLSL, TEXT("FMaterialVertexParameters"), OutCode);
			ShaderFrequencyMask &= ~(1u << SF_Vertex);
		}
		if (ShaderFrequencyMask != 0u)
		{
			// All other frequencies use PS version
			Private::EmitCustomHLSL(EmitCustomHLSL, TEXT("FMaterialPixelParameters"), OutCode);
		}
	}
}

FPreparedType FEmitContext::GetPreparedType(const FExpression* Expression, const FRequestedType& RequestedType) const
{
	const FXxHash64 Hash = Private::GetPrepareValueHash(Expression, RequestedType);
	FPrepareValueResult const* const* PrevResult = PrepareValueMap.Find(Hash);
	return PrevResult ? (*PrevResult)->PreparedType : FPreparedType();
}

Shader::FType FEmitContext::GetResultType(const FExpression* Expression, const FRequestedType& RequestedType) const
{
	return GetPreparedType(Expression, RequestedType).GetResultType();
}

Shader::FType FEmitContext::GetTypeForPinColoring(const FExpression* Expression) const
{
	FXxHash64 Hash = Private::GetPrepareValueHash(Expression, true);
	FPrepareValueResult const* const* Found = PrepareValueMap.Find(Hash);
	if (!Found)
	{
		Hash = Private::GetPrepareValueHash(Expression, false);
		Found = PrepareValueMap.Find(Hash);
	}
	return Found ? (*Found)->PreparedType.GetResultType() : Shader::FType();
}

EExpressionEvaluation FEmitContext::GetEvaluation(const FExpression* Expression, const FEmitScope& Scope, const FRequestedType& RequestedType) const
{
	return GetPreparedType(Expression, RequestedType).GetEvaluation(Scope, RequestedType);
}

bool AllComponentsRequestedInPast(const FRequestedType& RequestedType, const FRequestedType& PastRequestedType)
{
	bool bResult = true;
	TBitArray<>::FConstWordIterator ItA(RequestedType.RequestedComponents);
	TBitArray<>::FConstWordIterator ItB(PastRequestedType.RequestedComponents);
	for (; ItA && ItB; ++ItA, ++ItB)
	{
		if ((ItA.GetWord() & ItB.GetWord()) != ItA.GetWord())
		{
			bResult = false;
			break;
		}
	}
	for (; ItA; ++ItA)
	{
		if (ItA.GetWord() != 0u)
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

FPreparedType FEmitContext::PrepareExpression(const FExpression* InExpression, FEmitScope& Scope, const FRequestedType& RequestedType)
{
	if (!InExpression || RequestedType.IsVoid())
	{
		return FPreparedType();
	}

	FXxHash64 Hash = Private::GetPrepareValueHash(InExpression, RequestedType);
	FPrepareValueResult** PrevResult = PrepareValueMap.Find(Hash);
	FPrepareValueResult* Result = PrevResult ? *PrevResult : nullptr;
	if (!Result)
	{
		check(!bMarkLiveValues); // value should already be prepared at this point
		Result = new(*Allocator) FPrepareValueResult();
		PrepareValueMap.Add(Hash, Result);
	}

	if (RequestedType.IsEmpty() && !Result->PreparedType.IsVoid())
	{
		// If RequestedType is 'Empty' (nothing requested), we can skip all but the first call to Prepare
		return Result->PreparedType;
	}

	if (Result->bPreparingValue)
	{
		// Valid for this to be called reentrantly
		// Code should ensure that the type is set before the reentrant call, otherwise type will not be valid here
		// LocalPHI nodes rely on this to break loops
		if (!bMarkLiveValues)
		{
			FEmitScope* LoopScope = Scope.FindLoop();
			check(LoopScope);
			Result->PreparedType.SetLoopEvaluation(*LoopScope, RequestedType);
		}
		return Result->PreparedType;
	}

	{
		FXxHash64Builder Hasher;
		Hasher.Update(&InExpression, sizeof(InExpression));
		Hasher.Update(&bMarkLiveValues, sizeof(bMarkLiveValues));
		Hasher.Update(&RequestedType.Type.StructType, sizeof(RequestedType.Type.StructType)); //-V568
		Hash = Hasher.Finalize();
	}

	FRequestedType** PastRequestedTypePtr = RequestedTypeTracker.Find(Hash);
	if (PastRequestedTypePtr)
	{
		FRequestedType& PastRequestedType = **PastRequestedTypePtr;
		if (RequestedType.Type.IsAny() || PastRequestedType.Type.IsAny())
		{
			if (PastRequestedType.Type.IsAny())
			{
				return Result->PreparedType;
			}
			else
			{
				PastRequestedType = FRequestedType(Shader::EValueType::Any, false);
			}
		}
		else
		{
			checkf(!RequestedType.Type.IsNumericMatrix() || RequestedType.Type.ValueType != Shader::EValueType::DoubleInverse4x4,
				TEXT("DoubleInverse4x4 shouldn't be explicitly requested"));
			const Shader::FType CombinedType = Shader::CombineTypes(RequestedType.Type, PastRequestedType.Type, true);
			check(!CombinedType.IsVoid());

			if (CombinedType == PastRequestedType.Type && AllComponentsRequestedInPast(RequestedType, PastRequestedType))
			{
				return Result->PreparedType;
			}
			else
			{
				PastRequestedType.Type = CombinedType;
				PastRequestedType.RequestedComponents.CombineWithBitwiseOR(RequestedType.RequestedComponents, EBitwiseOperatorFlags::MaxSize);
			}
		}
	}
	else
	{
		FRequestedType* StackRequestedType = new (*Allocator) FRequestedType(RequestedType);
		// Turn Bool into Int
		StackRequestedType->Type = Shader::CombineTypes(StackRequestedType->Type, StackRequestedType->Type);
		check(!StackRequestedType->Type.IsVoid());
		RequestedTypeTracker.Add(Hash, StackRequestedType);
	}

	bool bResult = false;
	{
		FEmitOwnerScope OwnerScope(*this, InExpression);

		Result->bPreparingValue = true;
		bResult = InExpression->PrepareValue(*this, Scope, RequestedType, *Result);
		Result->bPreparingValue = false;
	}

	FPreparedType ResultType;
	if (bResult)
	{
		ResultType = Result->PreparedType;
		check(!ResultType.IsVoid());
		MarkInputType(InExpression, RequestedType.Type.GetConcreteType());
	}
	
	return ResultType;
}

void FEmitContext::MarkInputType(const FExpression* InExpression, const Shader::FType& Type)
{
	if (OwnerStack.Num() > 0 && !Type.IsVoid())
	{
		const FOwnedNode* InputOwner = OwnerStack.Last();
		for (UObject* InputObject : InputOwner->GetOwners())
		{
			const FConnectionKey Key(InputObject, InExpression);
			ConnectionMap.Add(Key, Type);
		}
	}
}

FEmitScope* FEmitContext::InternalPrepareScope(FScope* InScope, FScope* InParentScope)
{
	if (!InScope)
	{
		return nullptr;
	}

	TArray<FEmitScope*, TInlineAllocator<16>> EmitScopeChain;
	{
		TArray<FScope*, TInlineAllocator<16>> ScopeChain;
		ScopeChain.Add(InScope);
		{
			FScope* CurrentParentScope = InParentScope;
			while (CurrentParentScope)
			{
				ScopeChain.Add(CurrentParentScope);
				CurrentParentScope = CurrentParentScope->ParentScope;
			}
		}

		EmitScopeChain.Reserve(ScopeChain.Num());
		FEmitScope* EmitParentScope = nullptr;
		bool bFoundUninitializedScope = false;
		for (int32 Index = ScopeChain.Num() - 1; Index >= 0; --Index)
		{
			FScope* CurrentScope = ScopeChain[Index];
			FEmitScope* EmitScope = AcquireEmitScopeWithParent(CurrentScope, EmitParentScope);
			if (EmitScope->State == EEmitScopeState::Initializing)
			{
				// If we hit an initializing scope, reset the chain
				// We only want the chain to include scopes *below* the last initializing scope
				EmitScopeChain.Reset();
				// Don't expect any uninitialized scopes *above* the initializing scope (scopes are initialized top down)
				check(!bFoundUninitializedScope);
			}
			else
			{
				if (EmitScope->State == EEmitScopeState::Uninitialized)
				{
					bFoundUninitializedScope = true;
					EmitScope->State = EEmitScopeState::Initializing;
				}
				EmitScopeChain.Add(EmitScope);
			}
			EmitParentScope = EmitScope;
		}

		if (EmitScopeChain.Num() == 0)
		{
			// This can happen if the current scope is initializing
			check(EmitParentScope);
			check(EmitParentScope->State == EEmitScopeState::Initializing);
			return EmitParentScope;
		}
	}

	bool bMarkDead = false;
	for (int32 Index = 0; Index < EmitScopeChain.Num(); ++Index)
	{
		FEmitScope* EmitScope = EmitScopeChain[Index];
		FEmitScope* EmitParentScope = EmitScope->ParentScope;
		FStatement* Statement = EmitScope->OwnerStatement;

		if (bMarkDead)
		{
			EmitScope->Evaluation = EExpressionEvaluation::None;
			EmitScope->State = EEmitScopeState::Dead;
		}
		else if (EmitScope->State == EEmitScopeState::Initializing)
		{
			if (Statement)
			{
				FEmitOwnerScope OwnerScope(*this, Statement);
				check(EmitParentScope);
				if (Statement->Prepare(*this, *EmitParentScope))
				{
					// Disable 'V547: Expression is always true'
					// EmitScope->State may be set to 'Dead' via Statement->Prepare
					if (EmitScope->State == EEmitScopeState::Initializing)  //-V547
					{
						check(EmitScope->Evaluation != EExpressionEvaluation::None);
						EmitScope->State = EEmitScopeState::Live;
					}
					else
					{
						check(EmitScope->State == EEmitScopeState::Dead);
					}
				}
				else
				{
					EmitScope->State = EEmitScopeState::Uninitialized;
					EmitScope->Evaluation = EExpressionEvaluation::Unknown;
				}
			}
			else
			{
				EmitScope->State = EEmitScopeState::Live;
			}
		}
		else if (Statement && EmitScope->State == EEmitScopeState::Live && bMarkLiveValues)
		{
			// Need to Prepare statement a second time to mark live values
			FEmitOwnerScope OwnerScope(*this, Statement);
			check(EmitParentScope);
			verify(Statement->Prepare(*this, *EmitParentScope));
		}

		if (EmitScope->State == EEmitScopeState::Dead)
		{
			EmitScope->Evaluation = EExpressionEvaluation::None;
			bMarkDead = true;
		}
	}

	EExpressionEvaluation CurrentEvaluation = EExpressionEvaluation::Constant;
	for (int32 Index = EmitScopeChain.Num() - 1; Index >= 0; --Index)
	{
		FEmitScope* EmitScope = EmitScopeChain[Index];
		if (EmitScope->State == EEmitScopeState::Dead)
		{
			check(EmitScope->Evaluation == EExpressionEvaluation::None);
			CurrentEvaluation = EExpressionEvaluation::Constant;
		}
		else if (EmitScope->State == EEmitScopeState::Uninitialized || CurrentEvaluation == EExpressionEvaluation::Unknown)
		{
			CurrentEvaluation = EExpressionEvaluation::Unknown;
			EmitScope->Evaluation = EExpressionEvaluation::Unknown;
		}
		else
		{
			CurrentEvaluation = CombineEvaluations(CurrentEvaluation, EmitScope->Evaluation);
			EmitScope->Evaluation = CurrentEvaluation;
		}
	}

	return EmitScopeChain.Last();
}

FEmitScope* FEmitContext::PrepareScope(FScope* Scope)
{
	FEmitScope* EmitScope = InternalPrepareScope(Scope, Scope ? Scope->ParentScope : nullptr);
	return EmitScope && EmitScope->State != EEmitScopeState::Dead ? EmitScope : nullptr;
}

FEmitScope* FEmitContext::PrepareScopeWithParent(FScope* Scope, FScope* ParentScope)
{
	FEmitScope* EmitScope = InternalPrepareScope(Scope, ParentScope);
	return EmitScope && EmitScope->State != EEmitScopeState::Dead ? EmitScope : nullptr;
}

void FEmitContext::MarkScopeEvaluation(FEmitScope& EmitParentScope, FScope* Scope, EExpressionEvaluation Evaluation)
{
	FEmitScope* EmitScope = AcquireEmitScopeWithParent(Scope, &EmitParentScope);
	if (EmitScope && EmitScope->State != EEmitScopeState::Dead)
	{
		// Don't set loop evaluations on scopes
		EmitScope->Evaluation = CombineEvaluations(EmitScope->Evaluation, MakeNonLoopEvaluation(Evaluation));
	}
}

void FEmitContext::MarkScopeDead(FEmitScope& EmitParentScope, FScope* Scope)
{
	FEmitScope* EmitScope = AcquireEmitScopeWithParent(Scope, &EmitParentScope);
	if (EmitScope)
	{
		EmitScope->State = EEmitScopeState::Dead;
		EmitScope->Evaluation = EExpressionEvaluation::None;
	}
}

void FEmitContext::EmitPreshaderScope(const FScope* Scope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> PreshaderScopes, Shader::FPreshaderData& OutPreshader)
{
	FEmitScope* EmitScope = FindEmitScope(Scope);
	if (EmitScope)
	{
		EmitPreshaderScope(*EmitScope, RequestedType, PreshaderScopes, OutPreshader);
	}
}

void FEmitContext::EmitPreshaderScope(FEmitScope& EmitScope, const FRequestedType& RequestedType, TArrayView<const FEmitPreshaderScope> PreshaderScopes, Shader::FPreshaderData& OutPreshader)
{
	for (const FEmitPreshaderScope& PreshaderScope : PreshaderScopes)
	{
		if (PreshaderScope.Scope == &EmitScope)
		{
			PreshaderScope.Value->GetValuePreshader(*this, EmitScope, RequestedType, OutPreshader);
			check(PreshaderStackPosition > 0);
			PreshaderStackPosition--;
			OutPreshader.WriteOpcode(Shader::EPreshaderOpcode::Assign); // assign the new value
			break;
		}
	}

	if (EmitScope.ContainedStatement)
	{
		EmitScope.ContainedStatement->EmitPreshader(*this, EmitScope, RequestedType, PreshaderScopes, OutPreshader);
	}
}

FEmitScope* FEmitContext::AcquireEmitScopeWithParent(const FScope* Scope, FEmitScope* EmitParentScope)
{
	FEmitScope* EmitScope = nullptr;
	if (Scope)
	{
		FEmitScope* const* PrevEmitScope = EmitScopeMap.Find(Scope);
		EmitScope = PrevEmitScope ? *PrevEmitScope : nullptr;
		if (!EmitScope)
		{
			EmitScope = new(*Allocator) FEmitScope();
			EmitScope->OwnerStatement = Scope->OwnerStatement;
			EmitScope->ContainedStatement = Scope->ContainedStatement;
			EmitScope->ParentScope = EmitParentScope;
			EmitScope->NestedLevel = EmitParentScope ? EmitParentScope->NestedLevel + 1 : 0;
			EmitScope->Evaluation = EExpressionEvaluation::Unknown;
			EmitScope->State = EEmitScopeState::Uninitialized;
			EmitScopeMap.Add(Scope, EmitScope);
		}
		check(!EmitParentScope || EmitScope->ParentScope == EmitParentScope);
	}
	return EmitScope;
}

FEmitScope* FEmitContext::AcquireEmitScope(const FScope* Scope)
{
	FEmitScope* EmitParentScope = Scope ? AcquireEmitScope(Scope->ParentScope) : nullptr;
	return AcquireEmitScopeWithParent(Scope, EmitParentScope);
}

FEmitScope* FEmitContext::FindEmitScope(const FScope* Scope)
{
	FEmitScope* const* PrevEmitScope = EmitScopeMap.Find(Scope);
	return PrevEmitScope ? *PrevEmitScope : nullptr;
}

FEmitScope* FEmitContext::InternalEmitScope(const FScope* Scope)
{
	FEmitScope* EmitScope = FindEmitScope(Scope);
	if (EmitScope && EmitScope->State != EEmitScopeState::Dead)
	{
		if (Scope->ContainedStatement)
		{
			Scope->ContainedStatement->EmitShader(*this, *EmitScope);
		}
		return EmitScope;
	}
	return nullptr;
}

namespace Private
{
void MoveToScope(FEmitShaderNode* EmitNode, FEmitScope& Scope)
{
	if (EmitNode->Scope != &Scope)
	{
		FEmitScope* NewScope = &Scope;
		if (EmitNode->Scope)
		{
			NewScope = FEmitScope::FindSharedParent(EmitNode->Scope, &Scope);
			check(NewScope);
		}

		EmitNode->Scope = NewScope;
		for (FEmitShaderNode* Dependency : EmitNode->Dependencies)
		{
			MoveToScope(Dependency, *NewScope);
		}
	}
}

void FormatArg_ShaderValue(FEmitShaderExpression* ShaderValue, FEmitShaderDependencies& OutDependencies, FStringBuilderBase& OutCode)
{
	OutDependencies.AddUnique(ShaderValue);
	OutCode.Append(ShaderValue->Reference);
}

int32 InternalFormatString(FStringBuilderBase* OutString, FEmitShaderDependencies& OutDependencies, FStringView Format, TArrayView<const FFormatArgVariant> ArgList, int32 BaseArgIndex)
{
	int32 ArgIndex = BaseArgIndex;
	if (Format.Len() > 0)
	{
		check(OutString);
		for (TCHAR Char : Format)
		{
			if (Char == TEXT('%'))
			{
				const FFormatArgVariant& Arg = ArgList[ArgIndex++];
				switch (Arg.Type)
				{
				case EFormatArgType::ShaderValue: FormatArg_ShaderValue(Arg.ShaderValue, OutDependencies, *OutString); break;
				case EFormatArgType::String: OutString->Append(Arg.String); break;
				case EFormatArgType::Int: OutString->Appendf(TEXT("%d"), Arg.Int); break;
				case EFormatArgType::Uint: OutString->Appendf(TEXT("%u"), Arg.Uint); break;
				case EFormatArgType::Float: OutString->Appendf(TEXT("%#.9gf"), Arg.Float); break;
				case EFormatArgType::Bool: OutString->Append(Arg.Bool ? TEXT("true") : TEXT("false")); break;
				default:
					checkNoEntry();
					break;
				}
			}
			else
			{
				OutString->AppendChar(Char);
			}
		}
	}
	return ArgIndex;
}

void InternalFormatStrings(FStringBuilderBase* OutString0, FStringBuilderBase* OutString1, FEmitShaderDependencies& OutDependencies, FStringView Format0, FStringView Format1, const FFormatArgList& ArgList)
{
	int32 ArgIndex = 0;
	ArgIndex = InternalFormatString(OutString0, OutDependencies, Format0, ArgList, ArgIndex);
	ArgIndex = InternalFormatString(OutString1, OutDependencies, Format1, ArgList, ArgIndex);
	checkf(ArgIndex == ArgList.Num(), TEXT("%d args were provided, but %d were used"), ArgList.Num(), ArgIndex);
}

} // namespace Private

FEmitShaderExpression* FEmitContext::InternalEmitExpression(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, bool bInline, const Shader::FType& Type, FStringView Code)
{
	FEmitShaderExpression* ShaderValue = nullptr;

	FXxHash64Builder Hasher;
	Hasher.Update(Code.GetData(), Code.Len() * sizeof(TCHAR));
	if (bInline)
	{
		uint8 InlineFlag = 1;
		Hasher.Update(&InlineFlag, 1);
	}

	// Check to see if we've already generated code for an equivalent expression
	const FXxHash64 ShaderHash = Hasher.Finalize();
	FEmitShaderExpression** const PrevShaderValue = EmitExpressionMap.Find(ShaderHash);
	if (PrevShaderValue)
	{
		ShaderValue = *PrevShaderValue;
		check(ShaderValue->Type == Type);
		Private::MoveToScope(ShaderValue, Scope);

		// Check to see if we have any new dependencies to add to the previous expression
		TArray<FEmitShaderNode*, TInlineAllocator<32>> NewDependencies;
		bool bHasNewDependencies = false;
		for (FEmitShaderNode* Dependency : Dependencies)
		{
			if (!ShaderValue->Dependencies.Contains(Dependency))
			{
				if (!bHasNewDependencies)
				{
					NewDependencies.Reserve(ShaderValue->Dependencies.Num() + Dependencies.Num());
					NewDependencies.Append(ShaderValue->Dependencies.GetData(), ShaderValue->Dependencies.Num());
					bHasNewDependencies = true;
				}
				NewDependencies.Add(Dependency);
			}
		}
		if (bHasNewDependencies)
		{
			ShaderValue->Dependencies = MemStack::AllocateArrayView(*Allocator, MakeArrayView(NewDependencies));
		}
	}
	else
	{
		ShaderValue = new(*Allocator) FEmitShaderExpression(Scope, MemStack::AllocateArrayView(*Allocator, Dependencies), Type, ShaderHash);
		if (bInline)
		{
			ShaderValue->Reference = MemStack::AllocateString(*Allocator, Code);
		}
		else
		{
			ShaderValue->Reference = MemStack::AllocateStringf(*Allocator, TEXT("Local%d"), NumExpressionLocals++);
			ShaderValue->Value = MemStack::AllocateString(*Allocator, Code);
		}
		EmitExpressionMap.Add(ShaderHash, ShaderValue);
		EmitNodes.Add(ShaderValue);
	}

	return ShaderValue;
}

FEmitShaderStatement* FEmitContext::InternalEmitStatement(FEmitScope& Scope, TArrayView<FEmitShaderNode*> Dependencies, EEmitScopeFormat ScopeFormat, FEmitScope* NestedScope0, FEmitScope* NestedScope1, FStringView Code0, FStringView Code1)
{
	FEmitShaderStatement* EmitStatement = new(*Allocator) FEmitShaderStatement(Scope, MemStack::AllocateArrayView(*Allocator, Dependencies));
	EmitStatement->ScopeFormat = ScopeFormat;
	EmitStatement->NestedScopes[0] = NestedScope0;
	EmitStatement->NestedScopes[1] = NestedScope1;
	EmitStatement->Code[0] = MemStack::AllocateString(*Allocator, Code0);
	EmitStatement->Code[1] = MemStack::AllocateString(*Allocator, Code1);

	EmitNodes.Add(EmitStatement);
	return EmitStatement;
}

namespace Private
{
void WriteMaterialUniformAccess(Shader::EValueComponentType ComponentType, uint32 NumComponents, uint32 UniformOffset, FStringBuilderBase& OutResult)
{
	static const TCHAR IndexToMask[] = TEXT("xyzw");
	uint32 RegisterIndex = UniformOffset / 4;
	uint32 RegisterOffset = UniformOffset % 4;
	uint32 NumComponentsToWrite = NumComponents;
	bool bConstructor = false;

	check(ComponentType == Shader::EValueComponentType::Float || ComponentType == Shader::EValueComponentType::Int);
	const bool bIsInt = (ComponentType == Shader::EValueComponentType::Int);

	while (NumComponentsToWrite > 0u)
	{
		const uint32 NumComponentsInRegister = FMath::Min(NumComponentsToWrite, 4u - RegisterOffset);
		if (NumComponentsInRegister < NumComponents && !bConstructor)
		{
			// Uniform will be split across multiple registers, so add the constructor to concat them together
			OutResult.Appendf(TEXT("%s%d("), Shader::GetComponentTypeName(ComponentType), NumComponents);
			bConstructor = true;
		}

		if (bIsInt)
		{
			// PreshaderBuffer is typed as float4, so reinterpret as 'int' if needed
			OutResult.Append(TEXT("asint("));
		}

		OutResult.Appendf(TEXT("Material.PreshaderBuffer[%u]"), RegisterIndex);
		// Can skip writing mask if we're taking all 4 components from the register
		if (NumComponentsInRegister < 4u)
		{
			OutResult.AppendChar(TCHAR('.'));
			for (uint32 i = 0u; i < NumComponentsInRegister; ++i)
			{
				OutResult.AppendChar(IndexToMask[RegisterOffset + i]);
			}
		}

		if (bIsInt)
		{
			OutResult.Append(TEXT(")"));
		}

		NumComponentsToWrite -= NumComponentsInRegister;
		RegisterIndex++;
		RegisterOffset = 0u;
		if (NumComponentsToWrite > 0u)
		{
			OutResult.Append(TEXT(", "));
		}
	}
	if (bConstructor)
	{
		OutResult.Append(TEXT(")"));
	}
}

void EmitPreshaderField(
	FEmitContext& Context,
	TMemoryImageArray<FMaterialUniformPreshaderHeader>& UniformPreshaders,
	TMemoryImageArray<FMaterialUniformPreshaderField>& UniformPreshaderFields,
	Shader::FPreshaderData& UniformPreshaderData,
	FMaterialUniformPreshaderHeader*& PreshaderHeader,
	TFunction<void (FEmitValuePreshaderResult&)> EmitPreshaderOpcode,
	const Shader::FValueTypeDescription& TypeDesc,
	int32 ComponentIndex,
	FStringBuilderBase& FormattedCode)
{
	// Only need to allocate uniform buffer for non-constant components
	// Constant components can have their value inlined into the shader directly
	if (!PreshaderHeader)
	{
		// Allocate a preshader header the first time we hit a non-constant field
		PreshaderHeader = &UniformPreshaders.AddDefaulted_GetRef();
		PreshaderHeader->FieldIndex = UniformPreshaderFields.Num();
		PreshaderHeader->NumFields = 0u;
		PreshaderHeader->OpcodeOffset = UniformPreshaderData.Num();
		FEmitValuePreshaderResult PreshaderResult(UniformPreshaderData);
		EmitPreshaderOpcode(PreshaderResult);
		PreshaderHeader->OpcodeSize = UniformPreshaderData.Num() - PreshaderHeader->OpcodeOffset;
	}

	FMaterialUniformPreshaderField& PreshaderField = UniformPreshaderFields.AddDefaulted_GetRef();
	PreshaderField.ComponentIndex = ComponentIndex;
	PreshaderField.Type = TypeDesc.ValueType;
	PreshaderHeader->NumFields++;

	const int32 NumFieldComponents = TypeDesc.NumComponents;
	
	if (TypeDesc.ComponentType == Shader::EValueComponentType::Bool)
	{
		// 'Bool' uniforms are packed into bits
		if (Context.CurrentNumBoolComponents + NumFieldComponents > 32u)
		{
			Context.CurrentBoolUniformOffset = Context.UniformPreshaderOffset++;
			Context.CurrentNumBoolComponents = 0u;
		}

		const uint32 RegisterIndex = Context.CurrentBoolUniformOffset / 4;
		const uint32 RegisterOffset = Context.CurrentBoolUniformOffset % 4;
		FormattedCode.Appendf(TEXT("UnpackUniform_%s(asuint(Material.PreshaderBuffer[%u][%u]), %u)"),
			TypeDesc.Name,
			RegisterIndex,
			RegisterOffset,
			Context.CurrentNumBoolComponents);

		PreshaderField.BufferOffset = Context.CurrentBoolUniformOffset * 32u + Context.CurrentNumBoolComponents;
		Context.CurrentNumBoolComponents += NumFieldComponents;
	}
	else if (TypeDesc.ComponentType == Shader::EValueComponentType::Double)
	{
		// Double uniforms are split into Tile/Offset components to make FLWCScalar/FLWCVectors
		PreshaderField.BufferOffset = Context.UniformPreshaderOffset;

		if (NumFieldComponents > 1)
		{
			FormattedCode.Appendf(TEXT("MakeLWCVector%d("), NumFieldComponents);
		}
		else
		{
			FormattedCode.Append(TEXT("MakeLWCScalar("));
		}

		// Write the tile uniform
		Private::WriteMaterialUniformAccess(Shader::EValueComponentType::Float, NumFieldComponents, Context.UniformPreshaderOffset, FormattedCode);
		Context.UniformPreshaderOffset += NumFieldComponents;
		FormattedCode.Append(TEXT(", "));

		// Write the offset uniform
		Private::WriteMaterialUniformAccess(Shader::EValueComponentType::Float, NumFieldComponents, Context.UniformPreshaderOffset, FormattedCode);
		Context.UniformPreshaderOffset += NumFieldComponents;
		FormattedCode.Append(TEXT(")"));
	}
	else
	{
		// Float/Int uniforms are written directly to the uniform buffer
		const uint32 RegisterOffset = Context.UniformPreshaderOffset % 4;
		if (RegisterOffset + NumFieldComponents > 4u)
		{
			// If this uniform would span multiple registers, align offset to the next register to avoid this
			// TODO - we could keep track of this empty padding space, and pack other smaller uniform types here
			Context.UniformPreshaderOffset = Align(Context.UniformPreshaderOffset, 4u);
		}

		PreshaderField.BufferOffset = Context.UniformPreshaderOffset;
		Private::WriteMaterialUniformAccess(TypeDesc.ComponentType, NumFieldComponents, Context.UniformPreshaderOffset, FormattedCode);
		Context.UniformPreshaderOffset += NumFieldComponents;
	}
}

} // namespace Private

FEmitShaderExpression* FEmitContext::EmitPreshaderOrConstant(FEmitScope& Scope, const FRequestedType& RequestedType, const Shader::FType& ResultType, const FExpression* Expression)
{
	if (RequestedType.IsVoid())
	{
		return EmitConstantZero(Scope, ResultType);
	}

	Shader::FPreshaderData LocalPreshader;
	Shader::FType Type = ResultType;
	{
		FEmitValuePreshaderResult PreshaderResult(LocalPreshader);
		Expression->EmitValuePreshader(*this, Scope, RequestedType, PreshaderResult);
		check(PreshaderResult.Type.IsNumeric() || PreshaderResult.Type == Type);
	}

	FXxHash64Builder Hasher;
	Hasher.Update(&Type.StructType, sizeof(Type.StructType)); //-V568
	Hasher.Update(&Type.ObjectType, sizeof(Type.ObjectType));
	Hasher.Update(&Type.ValueType, sizeof(Type.ValueType));
	LocalPreshader.AppendHash(Hasher);
	const FXxHash64 Hash = Hasher.Finalize();
	FEmitShaderExpression* const* PrevShaderValue = EmitPreshaderMap.Find(Hash);
	if (PrevShaderValue)
	{
		FEmitShaderExpression* ShaderValue = *PrevShaderValue;
		check(ShaderValue->Type == Type);
		Private::MoveToScope(ShaderValue, Scope);
		return ShaderValue;
	}

	const FPreparedType& PreparedType = GetPreparedType(Expression, RequestedType);

	Shader::FPreshaderStack Stack;
	const Shader::FPreshaderValue PreshaderConstantValue = LocalPreshader.EvaluateConstant(*Material, Stack);
	const Shader::FValue ConstantValue = Shader::Cast(PreshaderConstantValue.AsShaderValue(TypeRegistry), Type);

	TStringBuilder<1024> FormattedCode;
	if (Type.IsStruct())
	{
		FormattedCode.Append(TEXT("{ "));
	}

	FMaterialUniformPreshaderHeader* PreshaderHeader = nullptr;

	int32 ComponentIndex = 0;
	for (int32 FieldIndex = 0; FieldIndex < Type.GetNumFlatFields(); ++FieldIndex)
	{
		if (FieldIndex > 0)
		{
			FormattedCode.Append(TEXT(", "));
		}

		const Shader::EValueType FieldType = ResultType.GetFlatFieldType(FieldIndex);
		const Shader::FValueTypeDescription& TypeDesc = Shader::GetValueTypeDescription(FieldType);
		const int32 NumFieldComponents = TypeDesc.NumComponents;

		// If this is a struct, use GetFieldEvaluation()
		// If this isn't a struct, use GetEvaluation() instead, which will properly handle promoting scalar types
		const EExpressionEvaluation FieldEvaluation = Type.IsStruct()
			? PreparedType.GetFieldEvaluation(Scope, RequestedType, ComponentIndex, NumFieldComponents)
			: PreparedType.GetEvaluation(Scope, RequestedType);

		if (FieldEvaluation == EExpressionEvaluation::Preshader)
		{
			FUniformExpressionSet& UniformExpressionSet = MaterialCompilationOutput->UniformExpressionSet;

			Private::EmitPreshaderField(
				*this,
				UniformExpressionSet.UniformPreshaders,
				UniformExpressionSet.UniformPreshaderFields,
				UniformExpressionSet.UniformPreshaderData,
				PreshaderHeader,
				[Expression, &Context = *this, &Scope, &RequestedType](FEmitValuePreshaderResult& OutResult)
				{
					Expression->EmitValuePreshader(Context, Scope, RequestedType, OutResult);
				},
				TypeDesc,
				ComponentIndex,
				FormattedCode);
		}
		else
		{
			// We allow FieldEvaluation to be 'None', since in that case we still need to fill in a value for the HLSL initializer
			check(FieldEvaluation == EExpressionEvaluation::ConstantZero ||
				FieldEvaluation == EExpressionEvaluation::Constant ||
				FieldEvaluation == EExpressionEvaluation::ConstantLoop ||
				FieldEvaluation == EExpressionEvaluation::None);

			// The type generated by the preshader might not match the expected type
			// In the future, with new HLSLTree, preshader could potentially include explicit cast opcodes, and avoid implicit conversions
			Shader::FValue FieldConstantValue(Type.GetComponentType(ComponentIndex), NumFieldComponents);
			for (int32 i = 0; i < NumFieldComponents; ++i)
			{
				if (ConstantValue.Component.Num() == 1)
				{
					// Allow replicating scalar values
					FieldConstantValue.Component[i] = ConstantValue.Component[0];
				}
				else if (ConstantValue.Component.IsValidIndex(ComponentIndex + i))
				{
					FieldConstantValue.Component[i] = ConstantValue.Component[ComponentIndex + i];
				}
				// else component defaults to 0
			}

			if (TypeDesc.ComponentType == Shader::EValueComponentType::Double)
			{
				const Shader::FDoubleValue DoubleValue = FieldConstantValue.AsDouble();
				TStringBuilder<256> ValueHigh;
				TStringBuilder<256> ValueLow;
				for (int32 Index = 0; Index < NumFieldComponents; ++Index)
				{
					if (Index > 0)
					{
						ValueHigh.Append(TEXT(", "));
						ValueLow.Append(TEXT(", "));
					}

					const FDFScalar Value(DoubleValue[Index]);
					ValueHigh.Appendf(TEXT("%#.9gf"), Value.High);
					ValueLow.Appendf(TEXT("%#.9gf"), Value.Low);
				}

				if (NumFieldComponents > 1)
				{
					FormattedCode.Appendf(TEXT("DFToWS(MakeDFVector%d(float%d(%s), float%d(%s)))"), NumFieldComponents, NumFieldComponents, ValueHigh.ToString(), NumFieldComponents, ValueLow.ToString());
				}
				else
				{
					FormattedCode.Appendf(TEXT("DFToWS(MakeDFScalar(%s, %s))"), ValueHigh.ToString(), ValueLow.ToString());
				}
			}
			else
			{
				const Shader::FValue CastFieldConstantValue = Shader::Cast(FieldConstantValue, FieldType);
				if (NumFieldComponents > 1)
				{
					FormattedCode.Appendf(TEXT("%s("), TypeDesc.Name);
				}
				for (int32 Index = 0; Index < NumFieldComponents; ++Index)
				{
					if (Index > 0)
					{
						FormattedCode.Append(TEXT(", "));
					}
					CastFieldConstantValue.Component[Index].ToString(TypeDesc.ComponentType, FormattedCode);
				}
				if (NumFieldComponents > 1)
				{
					FormattedCode.Append(TEXT(")"));
				}
			}
		}
		ComponentIndex += NumFieldComponents;
	}
	check(ComponentIndex == Type.GetNumComponents());

	if (Type.IsStruct())
	{
		FormattedCode.Append(TEXT(" }"));
	}

	const bool bInline = !Type.IsStruct(); // struct declarations can't be inline, due to HLSL syntax
	FEmitShaderExpression* ShaderValue = InternalEmitExpression(Scope, TArrayView<FEmitShaderNode*>(), bInline, Type, FormattedCode.ToView());
	EmitPreshaderMap.Add(Hash, ShaderValue);

	return ShaderValue;
}

FEmitShaderExpression* FEmitContext::EmitConstantZero(FEmitScope& Scope, const Shader::FType& Type)
{
	check(!Type.IsVoid());
	return EmitInlineExpression(Scope, Type, TEXT("((%)0)"), Type.GetName());
}

FEmitShaderExpression* FEmitContext::EmitCast(FEmitScope& Scope, FEmitShaderExpression* ShaderValue, const Shader::FType& DestType, EEmitCastFlags Flags)
{
	check(ShaderValue);
	check(!DestType.IsVoid());

	// TODO - could handle generic types here, cast to 'Any' should just return ShaderValue, cast to 'Numeric' could check number of components
	// Not sure if this is needed in practice
	check(!DestType.IsGeneric());

	if (ShaderValue->Type == DestType)
	{
		return ShaderValue;
	}

	const Shader::FValueTypeDescription SourceTypeDesc = Shader::GetValueTypeDescription(ShaderValue->Type);
	const Shader::FValueTypeDescription DestTypeDesc = Shader::GetValueTypeDescription(DestType);

	TStringBuilder<1024> FormattedCode;
	Shader::FType IntermediateType = DestType;
	bool bInline = true;

	if (SourceTypeDesc.NumComponents > 0 && DestTypeDesc.NumComponents > 0)
	{
		const bool bIsSourceLWC = SourceTypeDesc.ComponentType == Shader::EValueComponentType::Double;
		const bool bIsLWC = DestTypeDesc.ComponentType == Shader::EValueComponentType::Double;

		if (bIsLWC != bIsSourceLWC)
		{
			if (bIsLWC)
			{
				// float->LWC
				ShaderValue = EmitCast(Scope, ShaderValue, Shader::MakeValueType(Shader::EValueComponentType::Float, DestTypeDesc.NumComponents));
				FormattedCode.Appendf(TEXT("WSPromote(%s)"), ShaderValue->Reference);
			}
			else
			{
				//LWC->float
				FormattedCode.Appendf(TEXT("WSDemote(%s)"), ShaderValue->Reference);
				IntermediateType = Shader::MakeValueType(Shader::EValueComponentType::Float, SourceTypeDesc.NumComponents);
				bInline = false; // LWCToFloat has non-zero cost
			}
		}
		else
		{
			const bool bReplicateScalar = (SourceTypeDesc.NumComponents == 1) && !EnumHasAnyFlags(Flags, EEmitCastFlags::ZeroExtendScalar);

			check(SourceTypeDesc.NumComponents <= 4);
			check(DestTypeDesc.NumComponents <= 4);

			int32 NumComponents = 0;
			bool bNeedClosingParen = false;
			if (bIsLWC)
			{
				FormattedCode.Append(TEXT("MakeWSVector("));
				bNeedClosingParen = true;
			}
			else
			{
				if (SourceTypeDesc.NumComponents == DestTypeDesc.NumComponents)
				{
					NumComponents = DestTypeDesc.NumComponents;
					FormattedCode.Append(ShaderValue->Reference);
				}
				else if (bReplicateScalar)
				{
					NumComponents = DestTypeDesc.NumComponents;
					// Cast the scalar to the correct type, HLSL language will replicate the scalar if needed when performing this cast
					FormattedCode.Appendf(TEXT("((%s)%s)"), DestTypeDesc.Name, ShaderValue->Reference);
				}
				else
				{
					NumComponents = FMath::Min(SourceTypeDesc.NumComponents, DestTypeDesc.NumComponents);
					if (NumComponents < DestTypeDesc.NumComponents)
					{
						FormattedCode.Appendf(TEXT("%s("), DestTypeDesc.Name);
						bNeedClosingParen = true;
					}
					if (NumComponents == SourceTypeDesc.NumComponents)
					{
						// If we're taking all the components from the source, can avoid adding a swizzle
						FormattedCode.Append(ShaderValue->Reference);
					}
					else
					{
						// Truncate using a swizzle
						static const TCHAR* Mask[] = { nullptr, TEXT("x"), TEXT("xy"), TEXT("xyz"), TEXT("xyzw") };
						FormattedCode.Appendf(TEXT("%s.%s"), ShaderValue->Reference, Mask[NumComponents]);
					}
				}
			}

			if (bNeedClosingParen)
			{
				const Shader::FValue ZeroValue(DestTypeDesc.ComponentType, 1);
				for (int32 ComponentIndex = NumComponents; ComponentIndex < DestTypeDesc.NumComponents; ++ComponentIndex)
				{
					if (ComponentIndex > 0u)
					{
						FormattedCode.Append(TEXT(","));
					}
					if (bIsLWC)
					{
						if (!bReplicateScalar && ComponentIndex >= SourceTypeDesc.NumComponents)
						{
							FormattedCode.Append(TEXT("WSPromote(0.0f)"));
						}
						else
						{
							FormattedCode.Appendf(TEXT("WSGetComponent(%s, %d)"), ShaderValue->Reference, bReplicateScalar ? 0 : ComponentIndex);
						}
					}
					else
					{
						// Non-LWC case should only be zero-filling here, other cases should have already been handled
						check(!bReplicateScalar);
						check(ComponentIndex >= SourceTypeDesc.NumComponents);
						ZeroValue.ToString(Shader::EValueStringFormat::HLSL, FormattedCode);
					}
				}
				NumComponents = DestTypeDesc.NumComponents;
				FormattedCode.Append(TEXT(")"));
			}

			check(NumComponents == DestTypeDesc.NumComponents);
		}
	}
	else
	{
		Errorf(TEXT("Cannot cast between non-numeric types %s to %s."), SourceTypeDesc.Name, DestTypeDesc.Name);
		FormattedCode.Appendf(TEXT("((%s)0)"), DestType.GetName());
	}

	check(IntermediateType != ShaderValue->Type);
	if (bInline)
	{
		ShaderValue = EmitInlineExpressionWithDependency(Scope, ShaderValue, IntermediateType, FormattedCode.ToView());
	}
	else
	{
		ShaderValue = EmitExpressionWithDependency(Scope, ShaderValue, IntermediateType, FormattedCode.ToView());
	}

	if (ShaderValue->Type != DestType)
	{
		// May need to cast through multiple intermediate types to reach our destination type
		ShaderValue = EmitCast(Scope, ShaderValue, DestType);
	}
	return ShaderValue;
}

FEmitShaderExpression* FEmitContext::EmitCustomHLSL(FEmitScope& Scope, FStringView DeclarationCode, FStringView FunctionCode, TConstArrayView<FCustomHLSLInput> Inputs, const Shader::FStructType* OutputType)
{
	TArray<FEmitCustomHLSLInput, TInlineAllocator<8>> EmitInputs;
	EmitInputs.Reserve(Inputs.Num());

	FHasher Hasher;
	for (const FCustomHLSLInput& Input : Inputs)
	{
		const Shader::FType InputType = GetResultType(Input.Expression, Shader::EValueType::Any);
		AppendHash(Hasher, Input.Name);
		AppendHash(Hasher, InputType);

		FStringView ObjectDeclarationCode;
		FStringView ObjectForwardCode;
		if (InputType.IsObject())
		{
			TStringBuilder<256> FormattedDeclarationCode;
			TStringBuilder<256> FormattedForwardCode;
			Input.Expression->GetObjectCustomHLSLParameter(*this, Scope, InputType.ObjectType, Input.Name.GetData(), FormattedDeclarationCode, FormattedForwardCode);
			ObjectDeclarationCode = MemStack::AllocateStringView(*Allocator, FormattedDeclarationCode.ToView());
			ObjectForwardCode = MemStack::AllocateStringView(*Allocator, FormattedForwardCode.ToView());
			AppendHash(Hasher, ObjectDeclarationCode);
			AppendHash(Hasher, ObjectForwardCode);
		}

		EmitInputs.Add(FEmitCustomHLSLInput{ Input.Name, ObjectDeclarationCode, ObjectForwardCode, InputType });
	}

	AppendHash(Hasher, DeclarationCode);
	AppendHash(Hasher, FunctionCode);
	AppendHash(Hasher, OutputType);
	const FXxHash64 Hash = Hasher.Finalize();

	FEmitCustomHLSL* EmitCustomHLSL = EmitCustomHLSLMap.Find(Hash);
	if (!EmitCustomHLSL)
	{
		const int32 Index = EmitCustomHLSLMap.Num();
		EmitCustomHLSL = &EmitCustomHLSLMap.Emplace(Hash);
		EmitCustomHLSL->DeclarationCode = DeclarationCode;
		EmitCustomHLSL->FunctionCode = FunctionCode;
		EmitCustomHLSL->Inputs = MemStack::AllocateArrayView(*Allocator, MakeArrayView(EmitInputs));
		EmitCustomHLSL->OutputType = OutputType;
		EmitCustomHLSL->Index = Index;
	}

	// Track the different shader frequencies that call this expression
	EmitCustomHLSL->ShaderFrequencyMask |= (1u << ShaderFrequency);

	FEmitShaderDependencies Dependencies;
	Dependencies.Reserve(Inputs.Num());

	TStringBuilder<1024> FormattedCode;
	FormattedCode.Appendf(TEXT("CustomExpression%d(Parameters"), EmitCustomHLSL->Index);
	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FCustomHLSLInput& Input = Inputs[InputIndex];
		FEmitShaderExpression* EmitInputExpression = Input.Expression->GetValueShader(*this, Scope, EmitInputs[InputIndex].Type);
		FormattedCode.Appendf(TEXT(", %s"), EmitInputExpression->Reference);
		Dependencies.Add(EmitInputExpression);
	}
	FormattedCode.AppendChar(TEXT(')'));

	return EmitExpressionWithDependencies(Scope, Dependencies, OutputType, FormattedCode.ToView());
}

void FEmitContext::Finalize()
{
	// Unlink all nodes from scopes
	for (FEmitShaderNode* EmitNode : EmitNodes)
	{
		EmitNode->Scope = nullptr;
		EmitNode->NextScopedNode = nullptr;
	}
	
	// Don't reset Expression/Preshader maps, allow future passes to share matching preshaders/expressions

	for (TMap<FXxHash64, FPrepareValueResult*>::TIterator It(PrepareValueMap); It; ++It)
	{
		FPrepareValueResult* Value = It.Value();
		if (Value)
		{
			Value->~FPrepareValueResult();
		}
	}
	PrepareValueMap.Reset();
	ResetPastRequestedTypes();
	EmitScopeMap.Reset();
	EmitFunctionMap.Reset();
	EmitLocalPHIMap.Reset();
	EmitValueMap.Reset();

	MaterialCompilationOutput->UniformExpressionSet.UniformPreshaderBufferSize = (UniformPreshaderOffset + 3u) / 4u;
}

void FEmitContext::ResetPastRequestedTypes()
{
	for (TMap<FXxHash64, FRequestedType*>::TIterator It(RequestedTypeTracker); It; ++It)
	{
		FRequestedType* Value = It.Value();
		if (Value)
		{
			Value->~FRequestedType();
		}
	}
	RequestedTypeTracker.Reset();
}

} // namespace UE::HLSLTree

#endif // WITH_EDITOR
