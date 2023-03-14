// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMTypeIndex.h"
#include "RigVMCore/RigVMUnknownType.h"
#include "UObject/Interface.h"
#include "Engine/UserDefinedStruct.h"

namespace RigVMTypeUtils
{
	const TCHAR TArrayPrefix[] = TEXT("TArray<");
	const TCHAR TObjectPtrPrefix[] = TEXT("TObjectPtr<");
	const TCHAR TScriptInterfacePrefix[] = TEXT("TScriptInterface<");
	const TCHAR TArrayTemplate[] = TEXT("TArray<%s>");
	const TCHAR TObjectPtrTemplate[] = TEXT("TObjectPtr<%s%s>");
	const TCHAR TScriptInterfaceTemplate[] = TEXT("TScriptInterface<%s%s>");

	const FString BoolType = TEXT("bool");
	const FString FloatType = TEXT("float");
	const FString DoubleType = TEXT("double");
	const FString Int32Type = TEXT("int32");
	const FString UInt8Type = TEXT("uint8");
	const FString FNameType = TEXT("FName");
	const FString FStringType = TEXT("FString");
	const FString BoolArrayType = TEXT("TArray<bool>");
	const FString FloatArrayType = TEXT("TArray<float>");
	const FString DoubleArrayType = TEXT("TArray<double>");
	const FString Int32ArrayType = TEXT("TArray<int32>");
	const FString UInt8ArrayType = TEXT("TArray<uint8>");
	const FString FNameArrayType = TEXT("TArray<FName>");
	const FString FStringArrayType = TEXT("TArray<FString>");

	const FName BoolTypeName = *BoolType;
	const FName FloatTypeName = *FloatType;
	const FName DoubleTypeName = *DoubleType;
	const FName Int32TypeName = *Int32Type;
	const FName UInt8TypeName = *UInt8Type;
	const FName FNameTypeName = *FNameType;
	const FName FStringTypeName = *FStringType;
	const FName BoolArrayTypeName = *BoolArrayType;
	const FName FloatArrayTypeName = *FloatArrayType;
	const FName DoubleArrayTypeName = *DoubleArrayType;
	const FName Int32ArrayTypeName = *Int32ArrayType;
	const FName UInt8ArrayTypeName = *UInt8ArrayType;
	const FName FNameArrayTypeName = *FNameArrayType;
	const FName FStringArrayTypeName = *FStringArrayType;

	class RIGVM_API TypeIndex
	{
	public:
		static TRigVMTypeIndex Execute;	
		static TRigVMTypeIndex Bool;	
		static TRigVMTypeIndex Float;	
		static TRigVMTypeIndex Double;	
		static TRigVMTypeIndex Int32;	
		static TRigVMTypeIndex UInt8;	
		static TRigVMTypeIndex FName;	
		static TRigVMTypeIndex FString;
		static TRigVMTypeIndex WildCard;	
		static TRigVMTypeIndex BoolArray;	
		static TRigVMTypeIndex FloatArray;	
		static TRigVMTypeIndex DoubleArray;	
		static TRigVMTypeIndex Int32Array;	
		static TRigVMTypeIndex UInt8Array;	
		static TRigVMTypeIndex FNameArray;	
		static TRigVMTypeIndex FStringArray;	
		static TRigVMTypeIndex WildCardArray;	
	};

	// Returns true if the type specified is an array
	FORCEINLINE bool IsArrayType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TArrayPrefix);
	}

	FORCEINLINE FString ArrayTypeFromBaseType(const FString& InCPPType)
	{
		return FString::Printf(TArrayTemplate, *InCPPType);
	}

	FORCEINLINE FString BaseTypeFromArrayType(const FString& InCPPType)
	{
		return InCPPType.RightChop(7).LeftChop(1).TrimStartAndEnd();
	}

	FORCEINLINE_DEBUGGABLE FString GetUniqueStructTypeName(const UScriptStruct* InScriptStruct)
	{
		if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InScriptStruct))
		{
			return FString::Printf(TEXT("FUserDefinedStruct_%s"), *UserDefinedStruct->GetCustomGuid().ToString());
		}

		return InScriptStruct->GetStructCPPName();
	}

	FORCEINLINE FString CPPTypeFromEnum(UEnum* InEnum)
	{
		check(InEnum);

		FString CPPType = InEnum->CppType;
		if(CPPType.IsEmpty()) // this might be a user defined enum
		{
			CPPType = InEnum->GetName();
		}
		return CPPType;
	}

	FORCEINLINE bool IsUObjectType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TObjectPtrPrefix);
	}

	FORCEINLINE bool IsInterfaceType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TScriptInterfacePrefix);
	}

	static UScriptStruct* GetWildCardCPPTypeObject()
	{
		static UScriptStruct* WildCardTypeObject = FRigVMUnknownType::StaticStruct();
		return WildCardTypeObject;
	}

	static const FString& GetWildCardCPPType()
	{
		static const FString WildCardCPPType = GetUniqueStructTypeName(FRigVMUnknownType::StaticStruct()); 
		return WildCardCPPType;
	}

	static const FName& GetWildCardCPPTypeName()
	{
		static const FName WildCardCPPTypeName = *GetWildCardCPPType(); 
		return WildCardCPPTypeName;
	}

	static const FString& GetWildCardArrayCPPType()
	{
		static const FString WildCardArrayCPPType = ArrayTypeFromBaseType(GetWildCardCPPType()); 
		return WildCardArrayCPPType;
	}

	static const FName& GetWildCardArrayCPPTypeName()
	{
		static const FName WildCardArrayCPPTypeName = *GetWildCardArrayCPPType(); 
		return WildCardArrayCPPTypeName;
	}



	FORCEINLINE FString PostProcessCPPType(const FString& InCPPType, UObject* InCPPTypeObject)
	{
		FString CPPType = InCPPType;
	
		if (const UClass* Class = Cast<UClass>(InCPPTypeObject))
		{
			if (Class->IsChildOf(UInterface::StaticClass()))
			{
				CPPType = FString::Printf(RigVMTypeUtils::TScriptInterfaceTemplate, TEXT("I"), *Class->GetName());
			}
			else
			{
				CPPType = FString::Printf(RigVMTypeUtils::TObjectPtrTemplate, Class->GetPrefixCPP(), *Class->GetName());
			}
		}
		else if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCPPTypeObject))
		{
			CPPType = GetUniqueStructTypeName(ScriptStruct);
		}
		else if (UEnum* Enum = Cast<UEnum>(InCPPTypeObject))
		{
			CPPType = RigVMTypeUtils::CPPTypeFromEnum(Enum);
		}

		if(CPPType != InCPPType)
		{
			FString TemplateType = InCPPType;
			while (RigVMTypeUtils::IsArrayType(TemplateType))
			{
				CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(CPPType);
				TemplateType = RigVMTypeUtils::BaseTypeFromArrayType(TemplateType);
			}		
		}
	
		return CPPType;
	}
}