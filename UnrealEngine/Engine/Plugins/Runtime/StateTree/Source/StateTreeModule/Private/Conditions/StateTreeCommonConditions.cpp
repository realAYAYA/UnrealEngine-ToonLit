// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/StateTreeCommonConditions.h"

#include "StateTreeExecutionContext.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeLinker.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeCommonConditions)

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::Conditions
{

#if WITH_EDITOR
FText GetOperatorText(const EGenericAICheck Operator)
{
	switch (Operator)
	{
	case EGenericAICheck::Equal:
		return FText::FromString(TEXT("=="));
		break;
	case EGenericAICheck::NotEqual:
		return FText::FromString(TEXT("!="));
		break;
	case EGenericAICheck::Less:
		return FText::FromString(TEXT("&lt;"));
		break;
	case EGenericAICheck::LessOrEqual:
		return FText::FromString(TEXT("&lt;="));
		break;
	case EGenericAICheck::Greater:
		return FText::FromString(TEXT("&gt;"));
		break;
	case EGenericAICheck::GreaterOrEqual:
		return FText::FromString(TEXT("&gt;="));
		break;
	default:
		return FText::FromString(TEXT("??"));
		break;
	}
	return FText::GetEmpty();
}
#endif // WITH_EDITOR

template<typename T>
bool CompareNumbers(const T Left, const T Right, const EGenericAICheck Operator)
{
	switch (Operator)
	{
	case EGenericAICheck::Equal:
		return Left == Right;
		break;
	case EGenericAICheck::NotEqual:
		return Left != Right;
		break;
	case EGenericAICheck::Less:
		return Left < Right;
		break;
	case EGenericAICheck::LessOrEqual:
		return Left <= Right;
		break;
	case EGenericAICheck::Greater:
		return Left > Right;
		break;
	case EGenericAICheck::GreaterOrEqual:
		return Left >= Right;
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled operator %d"), Operator);
		return false;
		break;
	}
	return false;
}

} // UE::StateTree::Conditions


//----------------------------------------------------------------------//
//  FStateTreeCompareIntCondition
//----------------------------------------------------------------------//

bool FStateTreeCompareIntCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bResult = UE::StateTree::Conditions::CompareNumbers<int32>(InstanceData.Left, InstanceData.Right, Operator);
	return bResult ^ bInvert;
}


//----------------------------------------------------------------------//
//  FStateTreeCompareFloatCondition
//----------------------------------------------------------------------//

bool FStateTreeCompareFloatCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bResult = UE::StateTree::Conditions::CompareNumbers<double>(InstanceData.Left, InstanceData.Right, Operator);
	return bResult ^ bInvert;
}


//----------------------------------------------------------------------//
//  FStateTreeCompareBoolCondition
//----------------------------------------------------------------------//

bool FStateTreeCompareBoolCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	return (InstanceData.bLeft == InstanceData.bRight) ^ bInvert;
}


//----------------------------------------------------------------------//
//  FStateTreeCompareEnumCondition
//----------------------------------------------------------------------//

bool FStateTreeCompareEnumCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return (InstanceData.Left == InstanceData.Right) ^ bInvert;
}

#if WITH_EDITOR
void FStateTreeCompareEnumCondition::OnBindingChanged(const FGuid& ID, FStateTreeDataView InstanceData, const FStateTreeEditorPropertyPath& SourcePath, const FStateTreeEditorPropertyPath& TargetPath, const IStateTreeBindingLookup& BindingLookup)
{
	if (!TargetPath.IsValid())
	{
		return;
	}

	FInstanceDataType& Instance = InstanceData.GetMutable<FInstanceDataType>();

	// Left has changed, update enums from the leaf property.
	if (TargetPath.Path.Last() == GET_MEMBER_NAME_STRING_CHECKED(FInstanceDataType, Left))
	{
		if (const FProperty* LeafProperty = BindingLookup.GetPropertyPathLeafProperty(SourcePath))
		{
			// Handle both old stype namespace enums and new class enum properties.
			UEnum* NewEnum = nullptr;
			if (const FByteProperty* ByteProperty = CastField<FByteProperty>(LeafProperty))
			{
				NewEnum = ByteProperty->GetIntPropertyEnum();
			}
			else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(LeafProperty))
			{
				NewEnum = EnumProperty->GetEnum();
			}

			if (Instance.Left.Enum != NewEnum)
			{
				Instance.Left.Initialize(NewEnum);
			}
		}
		else
		{
			Instance.Left.Initialize(nullptr);
		}

		if (Instance.Right.Enum != Instance.Left.Enum)
		{
			Instance.Right.Initialize(Instance.Left.Enum);
		}
	}
}

#endif


//----------------------------------------------------------------------//
//  FStateTreeCompareDistanceCondition
//----------------------------------------------------------------------//

bool FStateTreeCompareDistanceCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const FVector::FReal Left = FVector::DistSquared(InstanceData.Source, InstanceData.Target);
	const FVector::FReal Right = FMath::Square(InstanceData.Distance);
	const bool bResult = UE::StateTree::Conditions::CompareNumbers<FVector::FReal>(Left, Right, Operator);
	return bResult ^ bInvert;
}


//----------------------------------------------------------------------//
//  FStateTreeRandomCondition
//----------------------------------------------------------------------//

bool FStateTreeRandomCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	return FMath::FRandRange(0.0f, 1.0f) < InstanceData.Threshold;
}


#undef LOCTEXT_NAMESPACE

