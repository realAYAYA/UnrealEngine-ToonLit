// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/RigDispatchFactory.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigDispatchFactory)

#if WITH_EDITOR

FString FRigDispatchFactory::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InTypeIndex == FRigVMRegistry::Get().GetTypeIndex<FEulerTransform>())
	{
		static FString DefaultValueString = GetDefaultValueForStruct(FEulerTransform::Identity); 
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
