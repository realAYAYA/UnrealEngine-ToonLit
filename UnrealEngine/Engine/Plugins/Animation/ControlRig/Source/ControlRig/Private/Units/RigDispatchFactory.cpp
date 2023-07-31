// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigDispatchFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigDispatchFactory)

#if WITH_EDITOR

FString FRigDispatchFactory::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(FRigVMRegistry::Get().IsArrayType(InTypeIndex))
	{
		static const FString EmptyArrayString = TEXT("()");
		return EmptyArrayString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Bool)
	{
		static const FString FalseString = TEXT("False");
		return FalseString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Float || InTypeIndex == RigVMTypeUtils::TypeIndex::Double)
	{
		static const FString ZeroString = TEXT("0.000000");
		return ZeroString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		static const FString ZeroString = TEXT("0");
		return ZeroString;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return FName(NAME_None).ToString();
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector2D>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FVector2D::ZeroVector);
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FVector>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FVector::ZeroVector); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FRotator>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FRotator::ZeroRotator); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FQuat>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FQuat::Identity); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FTransform>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FTransform::Identity); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FEulerTransform>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FEulerTransform::Identity); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FLinearColor>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FLinearColor::White); 
		return DefaultValueString;
	}
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FRigElementKey>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FRigElementKey(NAME_None, ERigElementType::Bone)); 
		return DefaultValueString;
	}
	return FRigVMDispatchFactory::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

#endif
