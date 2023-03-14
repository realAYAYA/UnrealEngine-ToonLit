// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Core/RigUnit_CoreDispatch.h"
#include "Units/RigUnitContext.h"
#include "EulerTransform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_CoreDispatch)

TArray<FRigVMTemplateArgument> FRigDispatch_CoreEquals::GetArguments() const
{
	const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
		FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
		FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
	};
	return {
		FRigVMTemplateArgument(TEXT("A"), ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(TEXT("B"), ERigVMPinDirection::Input, ValueCategories),
		FRigVMTemplateArgument(TEXT("Result"), ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool)
	};
}

FRigVMTemplateTypeMap FRigDispatch_CoreEquals::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(TEXT("A"), InTypeIndex);
	Types.Add(TEXT("B"), InTypeIndex);
	Types.Add(TEXT("Result"), RigVMTypeUtils::TypeIndex::Bool);
	return Types;
}

FRigVMFunctionPtr FRigDispatch_CoreEquals::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const TRigVMTypeIndex TypeIndex = InTypes.FindChecked(TEXT("A"));
	check(TypeIndex == InTypes.FindChecked(TEXT("B")));
	check(InTypes.FindChecked(TEXT("Result")) == RigVMTypeUtils::TypeIndex::Bool);

	if(TypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigDispatch_CoreEquals::Equals<float>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::Double)
	{
		return &FRigDispatch_CoreEquals::Equals<double>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigDispatch_CoreEquals::Equals<int32>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return &FRigDispatch_CoreEquals::NameEquals;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return &FRigDispatch_CoreEquals::StringEquals;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FVector>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector2D>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FVector2D>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FRotator>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FRotator>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FQuat>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FQuat>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FTransform>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FTransform>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FEulerTransform>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FEulerTransform>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FLinearColor>())
	{
		return &FRigDispatch_CoreEquals::MathTypeEquals<FLinearColor>;
	}
	return &FRigDispatch_CoreEquals::Execute;
}

bool FRigDispatch_CoreEquals::AdaptResult(bool bResult, const FRigVMExtendedExecuteContext& InContext)
{
	// if the factory is the not equals factory - let's invert the result
	if(InContext.Factory->GetScriptStruct() == FRigDispatch_CoreNotEquals::StaticStruct())
	{
		return !bResult;
	}
	return bResult;
}

void FRigDispatch_CoreEquals::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles)
{
	const FProperty* PropertyA = Handles[0].GetResolvedProperty(); 
	const FProperty* PropertyB = Handles[1].GetResolvedProperty(); 
	check(PropertyA);
	check(PropertyB);
	check(PropertyA->SameType(PropertyB));
	check(Handles[2].IsBool());

	const uint8* A = Handles[0].GetData();
	const uint8* B = Handles[1].GetData();
	bool& Result = *(bool*)Handles[2].GetData();
	Result = PropertyA->Identical(A, B);
	Result = AdaptResult(Result, InContext);
}

