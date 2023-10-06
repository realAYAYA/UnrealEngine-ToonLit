// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMTypeIndex.h"
#include "RigVMCore/RigVMUnknownType.h"
#include "UObject/Interface.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"

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
	const FString UInt32Type = TEXT("uint32");
	const FString UInt8Type = TEXT("uint8");
	const FString FNameType = TEXT("FName");
	const FString FStringType = TEXT("FString");
	const FString BoolArrayType = TEXT("TArray<bool>");
	const FString FloatArrayType = TEXT("TArray<float>");
	const FString DoubleArrayType = TEXT("TArray<double>");
	const FString Int32ArrayType = TEXT("TArray<int32>");
	const FString UInt32ArrayType = TEXT("TArray<uint32>");
	const FString UInt8ArrayType = TEXT("TArray<uint8>");
	const FString FNameArrayType = TEXT("TArray<FName>");
	const FString FStringArrayType = TEXT("TArray<FString>");

	const FName BoolTypeName = *BoolType;
	const FName FloatTypeName = *FloatType;
	const FName DoubleTypeName = *DoubleType;
	const FName Int32TypeName = *Int32Type;
	const FName UInt32TypeName = *UInt32Type;
	const FName UInt8TypeName = *UInt8Type;
	const FName FNameTypeName = *FNameType;
	const FName FStringTypeName = *FStringType;
	const FName BoolArrayTypeName = *BoolArrayType;
	const FName FloatArrayTypeName = *FloatArrayType;
	const FName DoubleArrayTypeName = *DoubleArrayType;
	const FName Int32ArrayTypeName = *Int32ArrayType;
	const FName UInt32ArrayTypeName = *UInt32ArrayType;
	const FName UInt8ArrayTypeName = *UInt8ArrayType;
	const FName FNameArrayTypeName = *FNameArrayType;
	const FName FStringArrayTypeName = *FStringArrayType;

	class RIGVM_API TypeIndex
	{
	public:
		static TRigVMTypeIndex Execute;	
		static TRigVMTypeIndex ExecuteArray;	
		static TRigVMTypeIndex Bool;	
		static TRigVMTypeIndex Float;	
		static TRigVMTypeIndex Double;	
		static TRigVMTypeIndex Int32;	
		static TRigVMTypeIndex UInt32;	
		static TRigVMTypeIndex UInt8;	
		static TRigVMTypeIndex FName;	
		static TRigVMTypeIndex FString;
		static TRigVMTypeIndex WildCard;	
		static TRigVMTypeIndex BoolArray;	
		static TRigVMTypeIndex FloatArray;	
		static TRigVMTypeIndex DoubleArray;	
		static TRigVMTypeIndex Int32Array;	
		static TRigVMTypeIndex UInt32Array;	
		static TRigVMTypeIndex UInt8Array;	
		static TRigVMTypeIndex FNameArray;	
		static TRigVMTypeIndex FStringArray;	
		static TRigVMTypeIndex WildCardArray;	
	};

	// Returns true if the type specified is an array
	inline bool IsArrayType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TArrayPrefix);
	}

	inline FString ArrayTypeFromBaseType(const FString& InCPPType)
	{
		return FString::Printf(TArrayTemplate, *InCPPType);
	}

	inline FString BaseTypeFromArrayType(const FString& InCPPType)
	{
		return InCPPType.RightChop(7).LeftChop(1).TrimStartAndEnd();
	}

	inline FString GetUniqueStructTypeName(const UScriptStruct* InScriptStruct)
	{
		if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InScriptStruct))
		{
			return FString::Printf(TEXT("FUserDefinedStruct_%s"), *UserDefinedStruct->GetCustomGuid().ToString());
		}

		return InScriptStruct->GetStructCPPName();
	}

	inline FString CPPTypeFromEnum(const UEnum* InEnum)
	{
		check(InEnum);

		FString CPPType = InEnum->CppType;
		if(CPPType.IsEmpty()) // this might be a user defined enum
		{
			CPPType = InEnum->GetName();
		}
		return CPPType;
	}

	inline bool IsUObjectType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TObjectPtrPrefix);
	}

	inline bool IsInterfaceType(const FString& InCPPType)
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

	static bool RequiresCPPTypeObject(const FString& InCPPType)
	{
		static const TArray<FString> PrefixesRequiringCPPTypeObject = {
			TEXT("F"), 
			TEXT("TArray<F"), 
			TEXT("TArray<TArray<F"), 
			TEXT("E"), 
			TEXT("TArray<E"), 
			TEXT("TArray<TArray<E"), 
			TEXT("U"), 
			TEXT("TArray<U"),
			TEXT("TArray<TArray<U"),
			TEXT("TObjectPtr<"), 
			TEXT("TArray<TObjectPtr<"),
			TEXT("TArray<TArray<TObjectPtr<"),
			TEXT("TScriptInterface<")
		};
		static const TArray<FString> PrefixesNotRequiringCPPTypeObject = {
			TEXT("UInt"), 
			TEXT("TArray<UInt"),
			TEXT("TArray<TArray<UInt"),
			TEXT("uint"), 
			TEXT("TArray<uint"),
			TEXT("TArray<TArray<uint")
		};
		static const TArray<FString> CPPTypesNotRequiringCPPTypeObject = {
			TEXT("FString"), 
			TEXT("TArray<FString>"), 
			TEXT("TArray<TArray<FString>>"), 
			TEXT("FName"), 
			TEXT("TArray<FName>"),
			TEXT("TArray<TArray<FName>>") 
		};

		if(!CPPTypesNotRequiringCPPTypeObject.Contains(InCPPType))
		{
			for(const FString& Prefix : PrefixesNotRequiringCPPTypeObject)
			{
				if(InCPPType.StartsWith(Prefix, ESearchCase::CaseSensitive))
				{
					return false;
				}
			}
			for(const FString& Prefix : PrefixesRequiringCPPTypeObject)
			{
				if(InCPPType.StartsWith(Prefix, ESearchCase::CaseSensitive))
				{
					return true;
				}
			}
		}

		return false;
	}

	static UObject* FindObjectGlobally(const TCHAR* InObjectName, bool bUseRedirector)
	{
		// Do a global search for the CPP type. Note that searching with ANY_PACKAGE _does not_
		// apply redirectors. So only if this fails do we apply them manually below.
		UObject* Object = FindFirstObject<UField>(InObjectName, EFindFirstObjectOptions::NativeFirst);
		if(Object != nullptr)
		{
			return Object;
		}

		// If its an enum, it might be defined as a namespace with the actual enum inside (see ERigVMClampSpatialMode). 
		FString ObjectNameStr(InObjectName);
		if (!ObjectNameStr.IsEmpty() && ObjectNameStr[0] == 'E')
		{
			FString Left, Right;
			if (ObjectNameStr.Split(TEXT("::"), &Left, &Right))
			{
				ObjectNameStr = Left;
				Object = FindFirstObject<UField>(*ObjectNameStr, EFindFirstObjectOptions::NativeFirst);
				if(Object != nullptr)
				{
					return Object;
				}
			}
		}

		if (!bUseRedirector)
		{
			return nullptr;
		}

		FCoreRedirectObjectName OldObjectName (ObjectNameStr);
		FCoreRedirectObjectName NewObjectName;
		const bool bFoundRedirect = FCoreRedirects::RedirectNameAndValues(
			ECoreRedirectFlags::Type_Class | ECoreRedirectFlags::Type_Struct | ECoreRedirectFlags::Type_Enum,
			OldObjectName,
			NewObjectName,
			nullptr,
			ECoreRedirectMatchFlags::AllowPartialMatch); // AllowPartialMatch to allow redirects from one package to another

		if (!bFoundRedirect)
		{
			return nullptr;
		}

		const FString RedirectedObjectName = NewObjectName.ObjectName.ToString();
		UPackage *Package = nullptr;
		if (!NewObjectName.PackageName.IsNone())
		{
			Package = FindPackage(nullptr, *NewObjectName.PackageName.ToString());
		}
		if (Package != nullptr)
		{
			Object = FindObject<UField>(Package, *RedirectedObjectName);
		}
		if (Package == nullptr || Object == nullptr)
		{
			// Hail Mary pass.
			Object = FindFirstObject<UField>(*RedirectedObjectName, EFindFirstObjectOptions::NativeFirst);
		}
		return Object;
	}

	static FString CPPTypeFromObject(const UObject* InCPPTypeObject)
	{
		if (const UClass* Class = Cast<UClass>(InCPPTypeObject))
		{
			if (Class->IsChildOf(UInterface::StaticClass()))
			{
				return FString::Printf(RigVMTypeUtils::TScriptInterfaceTemplate, TEXT("I"), *Class->GetName());
			}
			else
			{
				return FString::Printf(RigVMTypeUtils::TObjectPtrTemplate, Class->GetPrefixCPP(), *Class->GetName());
			}
		}
		else if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InCPPTypeObject))
		{
			return GetUniqueStructTypeName(ScriptStruct);
		}
		else if (const UEnum* Enum = Cast<UEnum>(InCPPTypeObject))
		{
			return RigVMTypeUtils::CPPTypeFromEnum(Enum);
		}

		return FString();
	}

	// Finds the CPPTypeObject from the CPPType. If not found, tries to use redirectors and modifies the InOutCPPType.
	static UObject* ObjectFromCPPType(FString& InOutCPPType, bool bUseRedirector = true)
	{
		if (!RequiresCPPTypeObject(InOutCPPType))
		{
			return nullptr;
		}

		// try to find the CPPTypeObject by name
		FString BaseCPPType = InOutCPPType;
		while (IsArrayType(BaseCPPType))
		{
			BaseCPPType = BaseTypeFromArrayType(BaseCPPType);
		}
		FString CPPType = BaseCPPType;

		static const FString PrefixScriptInterface = TScriptInterfacePrefix;
		if (CPPType.StartsWith(TScriptInterfacePrefix))
		{
			// Chop the prefix + the I indicating interface class
			CPPType = CPPType.RightChop(PrefixScriptInterface.Len() + 1).LeftChop(1);
		}

		UObject* CPPTypeObject = FindObjectGlobally(*CPPType, bUseRedirector);
		if (CPPTypeObject == nullptr)
		{
			// If we've mistakenly stored the struct type with the 'F', 'U', or 'A' prefixes, we need to strip them
			// off first. Enums are always named with their prefix intact.
			if (!CPPType.IsEmpty() && (CPPType[0] == TEXT('F') || CPPType[0] == TEXT('U') || CPPType[0] == TEXT('A')))
			{
				CPPType = CPPType.Mid(1);
			}
			CPPTypeObject = RigVMTypeUtils::FindObjectGlobally(*CPPType, bUseRedirector);
		}

		if(CPPTypeObject == nullptr)
		{
			InOutCPPType.Reset();
			return nullptr;
		}

		CPPType = CPPTypeFromObject(CPPTypeObject);
		InOutCPPType.ReplaceInline(*BaseCPPType, *CPPType);
		return CPPTypeObject;
	}

	static FString PostProcessCPPType(const FString& InCPPType, UObject* InCPPTypeObject = nullptr)
	{
		FString CPPType = InCPPType;
		if (InCPPTypeObject)
		{
			CPPType = CPPTypeFromObject(InCPPTypeObject);	
			if(CPPType != InCPPType)
			{
				FString TemplateType = InCPPType;
				while (RigVMTypeUtils::IsArrayType(TemplateType))
				{
					CPPType = RigVMTypeUtils::ArrayTypeFromBaseType(CPPType);
					TemplateType = RigVMTypeUtils::BaseTypeFromArrayType(TemplateType);
				}		
			}
		}
		else if (RequiresCPPTypeObject(CPPType))
		{
			// Uses redirectors and updates the CPPType if necessary
			ObjectFromCPPType(CPPType, true);
		}
	
		return CPPType;
	}

	// helper function to retrieve an object from a path
	static UObject* FindObjectFromCPPTypeObjectPath(const FString& InObjectPath)
	{
		UObject* Result = nullptr;
		if (InObjectPath.IsEmpty())
		{
			return Result;
		}

		if (InObjectPath == FName(NAME_None).ToString())
		{
			return Result;
		}

		// we do this to avoid ambiguous searches for 
		// common names such as "transform" or "vector"
		UPackage* Package = nullptr;
		FString PackageName;
		FString CPPTypeObjectName = InObjectPath;
		if (InObjectPath.Split(TEXT("."), &PackageName, &CPPTypeObjectName))
		{
			Package = FindPackage(nullptr, *PackageName);
		}
	
		if (UObject* ObjectWithinPackage = FindObject<UObject>(Package, *CPPTypeObjectName))
		{
			Result = ObjectWithinPackage;
		}

		if (!Result)
		{
			Result = FindFirstObject<UObject>(*InObjectPath, EFindFirstObjectOptions::NativeFirst | EFindFirstObjectOptions::EnsureIfAmbiguous);
		}
		if (!Result)
		{
			const FCoreRedirectObjectName OldObjectName(InObjectPath);
			const FCoreRedirectObjectName NewObjectName = FCoreRedirects::GetRedirectedName(ECoreRedirectFlags::Type_Struct, OldObjectName);
			if (OldObjectName != NewObjectName)
			{
				Result = FindObjectFromCPPTypeObjectPath(NewObjectName.ToString());
			}
		}
		return Result;
	}
	
	template<class T>
	static T* FindObjectFromCPPTypeObjectPath(const FString& InObjectPath)
	{
		return Cast<T>(FindObjectFromCPPTypeObjectPath(InObjectPath));
	}
}