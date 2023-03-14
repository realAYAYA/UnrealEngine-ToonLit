// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusHelpers.h"

#include "OptimusDataTypeRegistry.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ShaderParameterMetadata.h"
#include "Engine/UserDefinedStruct.h"

FName Optimus::GetUniqueNameForScope(UObject* InScopeObj, FName InName)
{
	// If there's already an object with this name, then attempt to make the name unique.
	// For some reason, MakeUniqueObjectName does not already do this check, hence this function.
	if (StaticFindObjectFast(UObject::StaticClass(), InScopeObj, InName) != nullptr)
	{
		InName = MakeUniqueObjectName(InScopeObj, UObject::StaticClass(), InName);
	}

	return InName;
}

FName Optimus::GetSanitizedNameForHlsl(FName InName)
{
	// Sanitize the name
	FString Name = InName.ToString();
	for (int32 i = 0; i < Name.Len(); ++i)
	{
		TCHAR& C = Name[i];

		const bool bGoodChar =
			FChar::IsAlpha(C) ||											// Any letter (upper and lowercase) anytime
			(C == '_') ||  													// _  
			((i > 0) && FChar::IsDigit(C));									// 0-9 after the first character

		if (!bGoodChar)
		{
			C = '_';
		}
	}

	return *Name;
}

bool Optimus::RenameObject(UObject* InObjectToRename, const TCHAR* InNewName, UObject* InNewOuter)
{
	return InObjectToRename->Rename(InNewName, InNewOuter, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
}

TArray<UClass*> Optimus::GetClassObjectsInPackage(UPackage* InPackage)
{
	TArray<UObject*> Objects;
	GetObjectsWithOuter(InPackage, Objects, false);

	TArray<UClass*> ClassObjects;
	for (UObject* Object : Objects)
	{
		if (UClass* Class = Cast<UClass>(Object))
		{
			ClassObjects.Add(Class);
		}
	}

	return ClassObjects;
}

FText Optimus::GetTypeDisplayName(UScriptStruct* InStructType)
{
#if WITH_EDITOR
	FText DisplayName = InStructType->GetDisplayNameText();
#else
	FText DisplayName = FText::FromName(InStructType->GetFName());
#endif

	return DisplayName;
}

FName Optimus::GetMemberPropertyShaderName(UScriptStruct* InStruct, const FProperty* InMemberProperty)
{
	if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InStruct))
	{
		// Remove Spaces
		FString ShaderMemberName = InStruct->GetAuthoredNameForField(InMemberProperty).Replace(TEXT(" "), TEXT(""));

		if (ensure(!ShaderMemberName.IsEmpty()))
		{
			// Sanitize the name, user defined struct can have members with names that start with numbers
			if (!FChar::IsAlpha(ShaderMemberName[0]) && !FChar::IsUnderscore(ShaderMemberName[0]))
			{
				ShaderMemberName = FString(TEXT("_")) + ShaderMemberName;
			}
		}

		return *ShaderMemberName;
	}

	return InMemberProperty->GetFName();
}

FName Optimus::GetTypeName(UScriptStruct* InStructType, bool bInShouldGetUniqueNameForUserDefinedStruct)
{
	if (UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InStructType))
	{
		if (bInShouldGetUniqueNameForUserDefinedStruct)
		{
			return FName(*FString::Printf(TEXT("FUserDefinedStruct_%s"), *UserDefinedStruct->GetCustomGuid().ToString()));
		}
	}
	
	return FName(*InStructType->GetStructCPPName());
}

