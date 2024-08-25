// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMCore/RigVMTypeIndex.h"
#include "RigVMCore/RigVMUnknownType.h"
#include "UObject/Interface.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

struct FRigVMTemplateArgumentType;

struct RIGVM_API FRigVMUserDefinedTypeResolver
{
	FRigVMUserDefinedTypeResolver() = default;
	explicit FRigVMUserDefinedTypeResolver(const TFunction<UObject*(const FString&)>& InResolver) : Resolver(InResolver) {}
	explicit FRigVMUserDefinedTypeResolver(TMap<FString, FSoftObjectPath>&& InObjectMap) : ObjectMap(InObjectMap) {} 
	
	UObject* GetTypeObjectByName(const FString& InTypeName) const
	{
		if (Resolver)
		{
			return Resolver(InTypeName);
		}
		
		if (const FSoftObjectPath *ObjectPath = ObjectMap.Find(InTypeName))
		{
			return ObjectPath->TryLoad();
		}
		return nullptr;
	}

	bool IsValid() const
	{
		return Resolver || !ObjectMap.IsEmpty();
	}
	
private:
	TFunction<UObject*(const FString&)> Resolver;
	TMap<FString, FSoftObjectPath> ObjectMap;
};

namespace RigVMTypeUtils
{
	const TCHAR TArrayPrefix[] = TEXT("TArray<");
	const TCHAR TObjectPtrPrefix[] = TEXT("TObjectPtr<");
	const TCHAR TSubclassOfPrefix[] = TEXT("TSubclassOf<");
	const TCHAR TScriptInterfacePrefix[] = TEXT("TScriptInterface<");
	const TCHAR TArrayTemplate[] = TEXT("TArray<%s>");
	const TCHAR TObjectPtrTemplate[] = TEXT("TObjectPtr<%s%s>");
	const TCHAR TSubclassOfTemplate[] = TEXT("TSubclassOf<%s%s>");
	const TCHAR TScriptInterfaceTemplate[] = TEXT("TScriptInterface<%s%s>");

	const inline TCHAR* BoolType = TEXT("bool");
	const inline TCHAR* FloatType = TEXT("float");
	const inline TCHAR* DoubleType = TEXT("double");
	const inline TCHAR* Int32Type = TEXT("int32");
	const inline TCHAR* UInt32Type = TEXT("uint32");
	const inline TCHAR* UInt8Type = TEXT("uint8");
	const inline TCHAR* FNameType = TEXT("FName");
	const inline TCHAR* FStringType = TEXT("FString");
	const inline TCHAR* FTextType = TEXT("FText");
	const inline TCHAR* BoolArrayType = TEXT("TArray<bool>");
	const inline TCHAR* FloatArrayType = TEXT("TArray<float>");
	const inline TCHAR* DoubleArrayType = TEXT("TArray<double>");
	const inline TCHAR* Int32ArrayType = TEXT("TArray<int32>");
	const inline TCHAR* UInt32ArrayType = TEXT("TArray<uint32>");
	const inline TCHAR* UInt8ArrayType = TEXT("TArray<uint8>");
	const inline TCHAR* FNameArrayType = TEXT("TArray<FName>");
	const inline TCHAR* FStringArrayType = TEXT("TArray<FString>");
	const inline TCHAR* FTextArrayType = TEXT("TArray<FText>");

	const FLazyName BoolTypeName(BoolType);
	const FLazyName FloatTypeName(FloatType);
	const FLazyName DoubleTypeName(DoubleType);
	const FLazyName Int32TypeName(Int32Type);
	const FLazyName UInt32TypeName(UInt32Type);
	const FLazyName UInt8TypeName(UInt8Type);
	const FLazyName FNameTypeName(FNameType);
	const FLazyName FStringTypeName(FStringType);
	const FLazyName FTextTypeName(FTextType);
	const FLazyName BoolArrayTypeName(BoolArrayType);
	const FLazyName FloatArrayTypeName(FloatArrayType);
	const FLazyName DoubleArrayTypeName(DoubleArrayType);
	const FLazyName Int32ArrayTypeName(Int32ArrayType);
	const FLazyName UInt32ArrayTypeName(UInt32ArrayType);
	const FLazyName UInt8ArrayTypeName(UInt8ArrayType);
	const FLazyName FNameArrayTypeName(FNameArrayType);
	const FLazyName FStringArrayTypeName(FStringArrayType);

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

	inline FString GetUniqueStructTypeName(const FGuid& InStructGuid)
	{
		return FString::Printf(TEXT("FUserDefinedStruct_%s"), *InStructGuid.ToString());
	}

	inline FString GetUniqueStructTypeName(const UScriptStruct* InScriptStruct)
	{
		if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InScriptStruct))
		{
			return GetUniqueStructTypeName(UserDefinedStruct->GetCustomGuid());
		}

		return InScriptStruct->GetStructCPPName();
	}

	inline FString CPPTypeFromEnum(const UEnum* InEnum)
	{
		FString CPPType = InEnum->CppType;
		if(CPPType.IsEmpty()) // this might be a user defined enum
		{
			CPPType = FString::Printf(TEXT("EUserDefinedEnum_%s_%08x"), *InEnum->GetName(), GetTypeHash(InEnum->GetPathName()));
		}
		return CPPType;
	}

	inline bool IsUClassType(const FString& InCPPType)
	{
		return InCPPType.StartsWith(TSubclassOfPrefix);
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
			TEXT("TSubclassOf<")
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

	// A UClass argument is used to signify both the object type and its class type.
	// This argument differentiates between the two
	enum class EClassArgType
	{
		// This type signifies a class
		AsClass,

		// This type signifies an object 
		AsObject
	};

	static FString CPPTypeFromObject(const UObject* InCPPTypeObject, EClassArgType InClassArgType = EClassArgType::AsObject)
	{
		if (const UClass* Class = Cast<UClass>(InCPPTypeObject))
		{
			if (InClassArgType == EClassArgType::AsClass)
			{
				return FString::Printf(RigVMTypeUtils::TSubclassOfPrefix, Class->GetPrefixCPP(), *Class->GetName());
			}
			else if (Class->IsChildOf(UInterface::StaticClass()))
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

	// Finds the CPPTypeObject from a CPP type of a potentially missing / unloaded user defined struct or enum
	RIGVM_API UObject* UserDefinedTypeFromCPPType(FString& InOutCPPType, const FRigVMUserDefinedTypeResolver* InTypeResolver = nullptr);

	// Finds the CPPTypeObject from the CPPType. If not found, tries to use redirectors and modifies the InOutCPPType.
	static UObject* ObjectFromCPPType(FString& InOutCPPType, bool bUseRedirector = true, const FRigVMUserDefinedTypeResolver* InTypeResolver = nullptr)
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
		const bool bIsClass = CPPType.StartsWith(TSubclassOfPrefix);

		static const FString PrefixObjectPtr = TObjectPtrPrefix;
		static const FString PrefixSubclassOf = TSubclassOfPrefix;
		static const FString PrefixScriptInterface = TScriptInterfacePrefix;

		if (CPPType.StartsWith(PrefixObjectPtr))
		{
			// Chop the prefix + the U indicating object class
			CPPType = CPPType.RightChop(PrefixObjectPtr.Len() + 1).LeftChop(1);
		}
		else if (CPPType.StartsWith(PrefixSubclassOf))
		{
			// Chop the prefix + the U indicating object class
			CPPType = CPPType.RightChop(PrefixSubclassOf.Len() + 1).LeftChop(1);
		}
		else if (CPPType.StartsWith(TScriptInterfacePrefix))
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
			CPPType = BaseCPPType;
			CPPTypeObject = UserDefinedTypeFromCPPType(CPPType, InTypeResolver);
		}

		if(CPPTypeObject == nullptr)
		{
			InOutCPPType.Reset();
			return nullptr;
		}

		CPPType = CPPTypeFromObject(CPPTypeObject, bIsClass ? EClassArgType::AsClass : EClassArgType::AsObject);
		InOutCPPType.ReplaceInline(*BaseCPPType, *CPPType);
		return CPPTypeObject;
	}

	static FString PostProcessCPPType(const FString& InCPPType, UObject* InCPPTypeObject = nullptr, const FRigVMUserDefinedTypeResolver* InResolvalInfo = nullptr)
	{
		FString CPPType = InCPPType;
		if (InCPPTypeObject)
		{
			const bool bIsClass = CPPType.StartsWith(TSubclassOfPrefix);
			CPPType = CPPTypeFromObject(InCPPTypeObject, bIsClass ? EClassArgType::AsClass : EClassArgType::AsObject);
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
			ObjectFromCPPType(CPPType, true, InResolvalInfo);
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

	static bool AreCompatible(const FProperty* InSourceProperty, const FProperty* InTargetProperty)
	{
		bool bCompatible = InSourceProperty->SameType(InTargetProperty);
		if (!bCompatible)
		{
			if(const FFloatProperty* TargetFloatProperty = CastField<FFloatProperty>(InTargetProperty))
			{
				bCompatible = InSourceProperty->IsA<FDoubleProperty>();
			}
			else if(const FDoubleProperty* TargetDoubleProperty = CastField<FDoubleProperty>(InTargetProperty))
			{
				bCompatible = InSourceProperty->IsA<FFloatProperty>();
			}
			else if (const FByteProperty* TargetByteProperty = CastField<FByteProperty>(InTargetProperty))
			{
				bCompatible = InSourceProperty->IsA<FEnumProperty>();
			}
			else if (const FEnumProperty* TargetEnumProperty = CastField<FEnumProperty>(InTargetProperty))
			{
				bCompatible = InSourceProperty->IsA<FByteProperty>();
			}
			else if(const FArrayProperty* TargetArrayProperty = CastField<FArrayProperty>(InTargetProperty))
			{
				if(const FArrayProperty* SourceArrayProperty = CastField<FArrayProperty>(InSourceProperty))
				{
					if(TargetArrayProperty->Inner->IsA<FFloatProperty>())
					{
						bCompatible = SourceArrayProperty->Inner->IsA<FDoubleProperty>();
					}
					else if(TargetArrayProperty->Inner->IsA<FDoubleProperty>())
					{
						bCompatible = SourceArrayProperty->Inner->IsA<FFloatProperty>();
					}
					else if(FByteProperty* TargetArrayInnerByteProperty = CastField<FByteProperty>(TargetArrayProperty->Inner))
					{
						bCompatible = SourceArrayProperty->Inner->IsA<FEnumProperty>();
					}
					else if(FEnumProperty* TargetArrayInnerEnumProperty = CastField<FEnumProperty>(TargetArrayProperty->Inner))
					{
						bCompatible = SourceArrayProperty->Inner->IsA<FByteProperty>();
					}
				}
			}
		}
		return bCompatible;
	}
}