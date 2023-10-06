// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigVMTraits.h"
#include "RigVMDefines.h"
#include "RigVMMemory.h"
#include "RigVMModule.h"
#include "RigVMTypeIndex.h"
#include "RigVMTypeUtils.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "EdGraphSchema_K2.h"
#endif
#include "RigVMExternalVariable.generated.h"

/**
 * The external variable can be used to map external / unowned
 * memory into the VM and back out
 */
USTRUCT(BlueprintType)
struct RIGVM_API FRigVMExternalVariableDef
{
	GENERATED_BODY()

	FRigVMExternalVariableDef() = default;

	static void GetTypeFromProperty(const FProperty* InProperty, FName& OutTypeName, UObject*& OutTypeObject)
	{
		if (CastField<FBoolProperty>(InProperty))
		{
			OutTypeName = TEXT("bool");
			OutTypeObject = nullptr;
		}
		else if (CastField<FIntProperty>(InProperty))
		{
			OutTypeName = TEXT("int32");
			OutTypeObject = nullptr;
		}
		else if (CastField<FFloatProperty>(InProperty))
		{
			OutTypeName = TEXT("float");
			OutTypeObject = nullptr;
		}
		else if (CastField<FDoubleProperty>(InProperty))
		{
			OutTypeName = TEXT("double");
			OutTypeObject = nullptr;
		}
		else if (CastField<FStrProperty>(InProperty))
		{
			OutTypeName = TEXT("FString");
			OutTypeObject = nullptr;
		}
		else if (CastField<FNameProperty>(InProperty))
		{
			OutTypeName = TEXT("FName");
			OutTypeObject = nullptr;
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
		{
			OutTypeName = EnumProperty->GetEnum()->GetFName();
			OutTypeObject = EnumProperty->GetEnum();
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
		{
			if (UEnum* BytePropertyEnum = ByteProperty->Enum)
			{
				OutTypeName = BytePropertyEnum->GetFName();
				OutTypeObject = BytePropertyEnum;
			}
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
			OutTypeName = *RigVMTypeUtils::GetUniqueStructTypeName(StructProperty->Struct);
			OutTypeObject = StructProperty->Struct;
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
		{
			FName InnerTypeName = NAME_None;
			GetTypeFromProperty(ArrayProperty->Inner, InnerTypeName, OutTypeObject);
			OutTypeName = *RigVMTypeUtils::ArrayTypeFromBaseType(InnerTypeName.ToString());
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
		{
			if(RigVMCore::SupportsUObjects())
			{
				OutTypeName = *FString::Printf(TEXT("TObjectPtr<%s%s>"),
					ObjectProperty->PropertyClass->GetPrefixCPP(),
					*ObjectProperty->PropertyClass->GetName());
				OutTypeObject = ObjectProperty->PropertyClass;
			}
			else
			{
				OutTypeName = NAME_None;
				OutTypeObject = nullptr;
			}
		}
		else if (const FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(InProperty))
		{
			if (RigVMCore::SupportsUInterfaces())
			{
				OutTypeName = *FString::Printf(TEXT("TScriptInterface<%s%s>"),
					InterfaceProperty->InterfaceClass->GetPrefixCPP(),
					*InterfaceProperty->InterfaceClass->GetName());
				OutTypeObject = InterfaceProperty->InterfaceClass;
			}
			else
			{
				OutTypeName = NAME_None;
				OutTypeObject = nullptr;
			}
		}
		else
		{
			checkNoEntry();
		}
	}

	bool IsValid() const
	{
		return Name.IsValid() &&
			!Name.IsNone() &&
			TypeName.IsValid() &&
			!TypeName.IsNone();
	}

	FName GetExtendedCPPType() const
	{
		if(bIsArray)
		{
			return *RigVMTypeUtils::ArrayTypeFromBaseType(TypeName.ToString());
		}
		return TypeName;
	}

	FName Name = NAME_None;
	const FProperty* Property =  nullptr;
	FName TypeName = NAME_None;
	UObject* TypeObject = nullptr;
	bool bIsArray = false;
	bool bIsPublic = false;
	bool bIsReadOnly = false;
	int32 Size = 0;
};

USTRUCT()
struct RIGVM_API FRigVMExternalVariable : public FRigVMExternalVariableDef
{
	GENERATED_BODY()
	
	FRigVMExternalVariable()
		: FRigVMExternalVariableDef()
	{
	}

	FRigVMExternalVariable(const FRigVMExternalVariableDef& Other)
		: FRigVMExternalVariableDef(Other)
		, Memory(nullptr)
	{
	}

	FRigVMExternalVariable(const FRigVMExternalVariableDef& Other, uint8* InMemory)
		: FRigVMExternalVariableDef(Other)
		, Memory(InMemory)
	{
	}

	static uint32 GetPropertyTypeHash(const FProperty *InProperty, const uint8 *InMemory)
	{
		if (!ensure(InProperty))
		{
			return 0;
		}

		if (InProperty->PropertyFlags & CPF_HasGetValueTypeHash)
		{
			return InProperty->GetValueTypeHash(InMemory);
		}

		if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
		{
			return static_cast<uint32>(BoolProperty->GetPropertyValue(InMemory));
		}
		else if (const FArrayProperty *ArrayProperty = CastField<FArrayProperty>(InProperty))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, InMemory);
			int32 Hash = ::GetTypeHash(ArrayHelper.Num());
			for (int32 Index = 0; Index < ArrayHelper.Num(); Index++)
			{
				Hash = HashCombine(Hash, GetPropertyTypeHash(ArrayProperty->Inner, ArrayHelper.GetRawPtr(Index)));
			}
			return Hash;
		}
		else if (const FMapProperty *MapProperty = CastField<FMapProperty>(InProperty))
		{
			FScriptMapHelper MapHelper(MapProperty, InMemory);
			int32 Hash = ::GetTypeHash(MapHelper.Num());
			for (int32 Index = 0; Index < MapHelper.Num(); Index++)
			{
				Hash = HashCombine(Hash, GetPropertyTypeHash(MapProperty->KeyProp, MapHelper.GetKeyPtr(Index)));
				Hash = HashCombine(Hash, GetPropertyTypeHash(MapProperty->ValueProp, MapHelper.GetValuePtr(Index)));
			}
			return Hash;
		}
		else if (const FSetProperty *SetProperty = CastField<FSetProperty>(InProperty))
		{
			FScriptSetHelper SetHelper(SetProperty, InMemory);
			int32 Hash = ::GetTypeHash(SetHelper.Num());
			for (int32 Index = 0; Index < SetHelper.Num(); Index++)
			{
				Hash = HashCombine(Hash, GetPropertyTypeHash(SetProperty->ElementProp, SetHelper.GetElementPtr(Index)));
			}
			return Hash;
		}
		else if (const FStructProperty *StructProperty = CastField<FStructProperty>(InProperty))
		{
			const UScriptStruct* StructType = StructProperty->Struct;
			// UserDefinedStruct overrides GetStructTypeHash to work without valid CppStructOps
			if (StructType->IsA<UUserDefinedStruct>() || (StructType->GetCppStructOps() && StructType->GetCppStructOps()->HasGetTypeHash()))
			{
				return StructType->GetStructTypeHash(InMemory);
			}
			else
			{
				uint32 Hash = 0;
				for (TFieldIterator<FProperty> It(StructType); It; ++It)
				{
					const FProperty* SubProperty = *It;
					Hash = HashCombine(Hash, GetPropertyTypeHash(*It, InMemory + SubProperty->GetOffset_ForInternal()));
				}
				return Hash;
			}
		}

		// If we get here, we're missing support for a property type that doesn't do its own hashing. 
		checkNoEntry();
		return 0;
	}

	uint32 GetTypeHash() const
	{
		return GetPropertyTypeHash(Property, Memory);
	}
	

	static FRigVMExternalVariable Make(const FProperty* InProperty, void* InContainer, const FName& InOptionalName = NAME_None)
	{
		check(InProperty);

		const FProperty* Property = InProperty;

		FRigVMExternalVariable ExternalVariable;
		ExternalVariable.Name = InOptionalName.IsNone() ? InProperty->GetFName() : InOptionalName;
		ExternalVariable.Property = Property;
		ExternalVariable.bIsPublic = !InProperty->HasAllPropertyFlags(CPF_DisableEditOnInstance);
		ExternalVariable.bIsReadOnly = InProperty->HasAllPropertyFlags(CPF_BlueprintReadOnly);

		if (InContainer)
		{
			ExternalVariable.Memory = (uint8*)Property->ContainerPtrToValuePtr<uint8>(InContainer);
		}

		FString TypePrefix, TypeSuffix;
		if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			ExternalVariable.bIsArray = true;
			TypePrefix = TEXT("TArray<");
			TypeSuffix = TEXT(">");
			Property = ArrayProperty->Inner;
		}

		ExternalVariable.Size = Property->GetSize();
		GetTypeFromProperty(Property, ExternalVariable.TypeName, ExternalVariable.TypeObject);

		return ExternalVariable;
	}

	static FRigVMExternalVariable Make(const FName& InName, bool& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("bool");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(bool);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<bool>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("bool");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(bool);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, int32& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("int32");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(int32);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<int32>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("int32");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(int32);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, uint8& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("uint8");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(uint8);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<uint8>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("uint8");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(uint8);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, float& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("float");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(float);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<float>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("float");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(float);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, double& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("double");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(double);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<double>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("double");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(double);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, FString& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("FString");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(FString);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<FString>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("FString");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(FString);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, FName& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("FName");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = false;
		Variable.Size = sizeof(FName);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	static FRigVMExternalVariable Make(const FName& InName, TArray<FName>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = TEXT("FName");
		Variable.TypeObject = nullptr;
		Variable.bIsArray = true;
		Variable.Size = sizeof(FName);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, T& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = StaticEnum<T>()->GetFName();
		Variable.TypeObject = StaticEnum<T>();
		Variable.bIsArray = false;
		Variable.Size = sizeof(T);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TIsEnum<T>::Value>::Type* = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, TArray<T>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = StaticEnum<T>()->GetFName();
		Variable.TypeObject = StaticEnum<T>();
		Variable.bIsArray = true;
		Variable.Size = sizeof(T);
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, T& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(TBaseStructure<T>::Get());
		Variable.TypeObject = TBaseStructure<T>::Get();
		Variable.bIsArray = false;
		Variable.Size = TBaseStructure<T>::Get()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TRigVMIsBaseStructure<T>::Value, T>::Type* = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, TArray<T>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(TBaseStructure<T>::Get());
		Variable.TypeObject = TBaseStructure<T>::Get();
		Variable.bIsArray = true;
		Variable.Size = TBaseStructure<T>::Get()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, T& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(T::StaticStruct());
		Variable.TypeObject = T::StaticStruct();
		Variable.bIsArray = false;
		Variable.Size = T::StaticStruct()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUStruct, T>>::Type * = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, TArray<T>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *RigVMTypeUtils::GetUniqueStructTypeName(T::StaticStruct());
		Variable.TypeObject = T::StaticStruct();
		Variable.bIsArray = true;
		Variable.Size = T::StaticStruct()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUClass, T>>::Type * = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, T& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *T::StaticClass()->GetStructCPPName();
		Variable.TypeObject = T::StaticClass();
		Variable.bIsArray = false;
		Variable.Size = T::StaticClass()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template <
		typename T,
		typename TEnableIf<TModels_V<CRigVMUClass, T>>::Type * = nullptr
	>
	static FRigVMExternalVariable Make(const FName& InName, TArray<T>& InValue)
	{
		FRigVMExternalVariable Variable;
		Variable.Name = InName;
		Variable.TypeName = *T::StaticClass()->GetStructCPPName();
		Variable.TypeObject = T::StaticClass();
		Variable.bIsArray = true;
		Variable.Size = T::StaticClass()->GetStructureSize();
		Variable.Memory = (uint8*)&InValue;
		return Variable;
	}

	template<typename T>
	T GetValue() const
	{
		ensure(IsValid() && !bIsArray);
		return *(T*)Memory;
	}

	template<typename T>
	T& GetRef()
	{
		ensure(IsValid() && !bIsArray);
		return *(T*)Memory;
	}

	template<typename T>
	const T& GetRef() const
	{
		ensure(IsValid() && !bIsArray);
		return *(T*)Memory;
	}

	template<typename T>
	void SetValue(const T& InValue)
	{
		ensure(IsValid() && !bIsArray);
		(*(T*)Memory) = InValue;
	}

	template<typename T>
	TArray<T> GetArray()
	{
		ensure(IsValid() && bIsArray);
		return *(TArray<T>*)Memory;
	}

	template<typename T>
	void SetArray(const TArray<T>& InValue)
	{
		ensure(IsValid() && bIsArray);
		(*(TArray<T>*)Memory) = InValue;
	}

	bool IsValid(bool bAllowNullPtr = false) const
	{
		return Super::IsValid()
			&& (bAllowNullPtr || Memory != nullptr);
	}

	TRigVMTypeIndex GetTypeIndex() const;


	template<typename VarType>
	static void MergeExternalVariable(TArray<VarType>& OutVariables, const VarType& InVariable)
	{
		if(!InVariable.IsValid(true))
		{
			return;
		}

		for(const VarType& ExistingVariable : OutVariables)
		{
			if(ExistingVariable.Name == InVariable.Name)
			{
				ensure(ExistingVariable.TypeName == InVariable.TypeName);
				ensure(ExistingVariable.TypeObject == InVariable.TypeObject);
				return;
			}
		}

		OutVariables.Add(InVariable);
	}

	uint8* Memory = nullptr;
};

inline FArchive& operator<<(FArchive& Ar, FRigVMExternalVariableDef& Variable)
{
	Ar << Variable.Name;
	Ar << Variable.TypeName;

	if (Ar.IsSaving())
	{
		FSoftObjectPath TypeObjectPath(Variable.TypeObject);
		Ar << TypeObjectPath;
	}
	else if (Ar.IsLoading())
	{
		FSoftObjectPath TypeObjectPath;
		Ar << TypeObjectPath;
		Variable.TypeObject = TypeObjectPath.ResolveObject();
	}
	
	Ar << Variable.bIsArray;
	Ar << Variable.bIsPublic;
	Ar << Variable.bIsReadOnly;
	Ar << Variable.Size;
	return Ar;
}

inline FArchive& operator<<(FArchive& Ar, FRigVMExternalVariable& Variable)
{
	FRigVMExternalVariableDef& RigVMExternalVariableDef = static_cast<FRigVMExternalVariableDef&>(Variable);
	Ar << RigVMExternalVariableDef;

	return Ar;
}

namespace RigVMTypeUtils
{
#if WITH_EDITOR
 	RIGVM_API FRigVMExternalVariable ExternalVariableFromBPVariableDescription(const FBPVariableDescription& InVariableDescription);

	RIGVM_API FRigVMExternalVariable ExternalVariableFromBPVariableDescription(const FBPVariableDescription& InVariableDescription, void* Container);

	RIGVM_API FRigVMExternalVariable ExternalVariableFromPinType(const FName& InName, const FEdGraphPinType& InPinType, bool bInPublic = false, bool bInReadonly = false);

	RIGVM_API FRigVMExternalVariable ExternalVariableFromCPPTypePath(const FName& InName, const FString& InCPPTypePath, bool bInPublic = false, bool bInReadonly = false);

	RIGVM_API FRigVMExternalVariable ExternalVariableFromCPPType(const FName& InName, const FString& InCPPType, UObject* InCPPTypeObject, bool bInPublic = false, bool bInReadonly = false);
#endif // WITH_EDITOR

	RIGVM_API TArray<FRigVMExternalVariableDef> GetExternalVariableDefs(const TArray<FRigVMExternalVariable>& ExternalVariables);
}
