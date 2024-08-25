// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/RigVMDispatch_Core.h"
#include "RigVMCore/RigVMRegistry.h"
#include "EulerTransform.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMDispatch_Core)


const FName FRigVMDispatch_CoreEquals::AName = TEXT("A");
const FName FRigVMDispatch_CoreEquals::BName = TEXT("B");
const FName FRigVMDispatch_CoreEquals::ResultName = TEXT("Result");


FName FRigVMDispatch_CoreEquals::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] = {
		AName,
		BName,
		ResultName
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CoreEquals::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if (Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};
		Infos.Emplace(AName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(BName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	return Infos;
}

FRigVMTemplateTypeMap FRigVMDispatch_CoreEquals::OnNewArgumentType(const FName& InArgumentName,
	TRigVMTypeIndex InTypeIndex) const
{
	FRigVMTemplateTypeMap Types;
	Types.Add(AName, InTypeIndex);
	Types.Add(BName, InTypeIndex);
	Types.Add(ResultName, RigVMTypeUtils::TypeIndex::Bool);
	return Types;
}

FRigVMFunctionPtr FRigVMDispatch_CoreEquals::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const TRigVMTypeIndex TypeIndex = InTypes.FindChecked(AName);
	check(TypeIndex == InTypes.FindChecked(BName));
	check(InTypes.FindChecked(ResultName) == RigVMTypeUtils::TypeIndex::Bool);

	if(TypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigVMDispatch_CoreEquals::Equals<float>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::Double)
	{
		return &FRigVMDispatch_CoreEquals::Equals<double>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigVMDispatch_CoreEquals::Equals<int32>;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return &FRigVMDispatch_CoreEquals::NameEquals;
	}
	if(TypeIndex == RigVMTypeUtils::TypeIndex::FString)
	{
		return &FRigVMDispatch_CoreEquals::StringEquals;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FVector>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector2D>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FVector2D>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FRotator>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FRotator>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FQuat>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FQuat>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FTransform>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FTransform>;
	}
	if(TypeIndex == FRigVMRegistry::Get().GetTypeIndex<FLinearColor>())
	{
		return &FRigVMDispatch_CoreEquals::MathTypeEquals<FLinearColor>;
	}
	return &FRigVMDispatch_CoreEquals::Execute;
}

bool FRigVMDispatch_CoreEquals::AdaptResult(bool bResult, const FRigVMExtendedExecuteContext& InContext)
{
	// if the factory is the not equals factory - let's invert the result
	if(InContext.Factory->GetScriptStruct() == FRigVMDispatch_CoreNotEquals::StaticStruct())
	{
		return !bResult;
	}
	return bResult;
}

void FRigVMDispatch_CoreEquals::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates)
{
	const FProperty* PropertyA = Handles[0].GetResolvedProperty(); 
	const FProperty* PropertyB = Handles[1].GetResolvedProperty(); 
	check(PropertyA);
	check(PropertyB);
	check(PropertyA->SameType(PropertyB));
	check(Handles[2].IsBool());

	const uint8* A = Handles[0].GetData();
	const uint8* B = Handles[1].GetData();
	bool& Result = *reinterpret_cast<bool*>(Handles[2].GetData());
	Result = PropertyA->Identical(A, B);
	Result = AdaptResult(Result, InContext);
}

// duplicate the code here so that the FRigVMDispatch_CoreNotEquals has it's own static variables
// to store the types are registration time.
const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CoreNotEquals::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if (Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories = {
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};
		Infos.Emplace(AName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(BName, ERigVMPinDirection::Input, ValueCategories);
		Infos.Emplace(ResultName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	return Infos;
}
