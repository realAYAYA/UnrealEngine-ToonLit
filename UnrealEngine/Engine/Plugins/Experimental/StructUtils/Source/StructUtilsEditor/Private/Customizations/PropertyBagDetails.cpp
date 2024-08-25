// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyBagDetails.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/EnumProperty.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "Engine/UserDefinedStruct.h"
#include "SPinTypeSelector.h"
#include "StructUtilsMetadata.h"
#include "Templates/ValueOrError.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PropertyBagDetails)

#define LOCTEXT_NAMESPACE "StructUtilsEditor"

////////////////////////////////////

namespace UE::StructUtils::Private
{

/** @return true property handle holds struct property of type T.  */ 
template<typename T>
bool IsScriptStruct(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle)
	{
		return false;
	}

	FStructProperty* StructProperty = CastField<FStructProperty>(PropertyHandle->GetProperty());
	return StructProperty && StructProperty->Struct->IsA(TBaseStructure<T>::Get()->GetClass());
}

/** @return true if the property is one of the known missing types. */
bool HasMissingType(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	if (!PropertyHandle)
	{
		return false;
	}

	// Handles Struct
	if (FStructProperty* StructProperty = CastField<FStructProperty>(PropertyHandle->GetProperty()))
	{
		return StructProperty->Struct == FPropertyBagMissingStruct::StaticStruct();
	}
	// Handles Object, SoftObject, Class, SoftClass.
	if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PropertyHandle->GetProperty()))
	{
		return ObjectProperty->PropertyClass == UPropertyBagMissingObject::StaticClass();
	}
	// Handles Enum
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(PropertyHandle->GetProperty()))
	{
		return EnumProperty->GetEnum() == StaticEnum<EPropertyBagMissingEnum>();
	}

	return false;
}

/** @return property bag struct common to all edited properties. */
const UPropertyBag* GetCommonBagStruct(TSharedPtr<IPropertyHandle> StructProperty)
{
	const UPropertyBag* CommonBagStruct = nullptr;

	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		StructProperty->EnumerateConstRawData([&CommonBagStruct](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				const FInstancedPropertyBag* Bag = static_cast<const FInstancedPropertyBag*>(RawData);

				const UPropertyBag* BagStruct = Bag->GetPropertyBagStruct();
				if (CommonBagStruct && CommonBagStruct != BagStruct)
				{
					// Multiple struct types on the sources - show nothing set
					CommonBagStruct = nullptr;
					return false;
				}
				CommonBagStruct = BagStruct;
			}

			return true;
		});
	}

	return CommonBagStruct;
}

/** @return property descriptors of the property bag struct common to all edited properties. */
TArray<FPropertyBagPropertyDesc> GetCommonPropertyDescs(const TSharedPtr<IPropertyHandle>& StructProperty)
{
	TArray<FPropertyBagPropertyDesc> PropertyDescs;
	
	if (const UPropertyBag* BagStruct = GetCommonBagStruct(StructProperty))
	{
		PropertyDescs = BagStruct->GetPropertyDescs();
	}
	
	return PropertyDescs;
}

/** Creates new property bag struct and sets all properties to use it, migrating over old values. */
void SetPropertyDescs(const TSharedPtr<IPropertyHandle>& StructProperty, const TConstArrayView<FPropertyBagPropertyDesc> PropertyDescs)
{
	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		// Create new bag struct
		const UPropertyBag* NewBagStruct = UPropertyBag::GetOrCreateFromDescs(PropertyDescs);

		// Migrate structs to the new type, copying values over.
		StructProperty->EnumerateRawData([&NewBagStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				if (FInstancedPropertyBag* Bag = static_cast<FInstancedPropertyBag*>(RawData))
				{
					Bag->MigrateToNewBagStruct(NewBagStruct);
				}
			}

			return true;
		});
	}
}

/** @return sanitized property name based on the input string. */
FName GetValidPropertyName(const FString& Name)
{
	FName Result;
	if (!Name.IsEmpty())
	{
		if (!FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS))
		{
			Result = MakeObjectNameFromDisplayLabel(Name, NAME_None);
		}
		else
		{
			Result = FName(Name);
		}
	}
	else
	{
		Result = FName(TEXT("Property"));
	}

	return Result;
}

FName GetPropertyNameSafe(const TSharedPtr<IPropertyHandle>& PropertyHandle)
{
	const FProperty* Property = PropertyHandle ? PropertyHandle->GetProperty() : nullptr;
	if (Property != nullptr)
	{
		return Property->GetFName();
	}
	return FName();
}

/** @return true of the property name is not used yet by the property bag structure common to all edited properties. */
bool IsUniqueName(const FName NewName, const FName OldName, const TSharedPtr<IPropertyHandle>& StructProperty)
{
	if (NewName == OldName)
	{
		return false;
	}
	
	if (!StructProperty || !StructProperty->IsValidHandle())
	{
		return false;
	}
	
	bool bFound = false;

	if (ensure(IsScriptStruct<FInstancedPropertyBag>(StructProperty)))
	{
		StructProperty->EnumerateConstRawData([&bFound, NewName](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (const FInstancedPropertyBag* Bag = static_cast<const FInstancedPropertyBag*>(RawData))
			{
				if (const UPropertyBag* BagStruct = Bag->GetPropertyBagStruct())
				{
					const bool bContains = BagStruct->GetPropertyDescs().ContainsByPredicate([NewName](const FPropertyBagPropertyDesc& Desc)
					{
						return Desc.Name == NewName;
					});
					if (bContains)
					{
						bFound = true;
						return false; // Stop iterating
					}
				}
			}

			return true;
		});
	}
	
	return !bFound;
}

/** @return Blueprint pin type from property descriptor. */
FEdGraphPinType GetPropertyDescAsPin(const FPropertyBagPropertyDesc& Desc)
{
	UEnum* PropertyTypeEnum = StaticEnum<EPropertyBagPropertyType>();
	check(PropertyTypeEnum);
	const UPropertyBagSchema* Schema = GetDefault<UPropertyBagSchema>();
	check(Schema);

	FEdGraphPinType PinType;
	PinType.PinSubCategory = NAME_None;

	// Container type
	//@todo: Handle nested containers in property selection.
	const EPropertyBagContainerType ContainerType = Desc.ContainerTypes.GetFirstContainerType();
	switch (ContainerType)
	{
	case EPropertyBagContainerType::Array:
		PinType.ContainerType = EPinContainerType::Array;
		break;
	default:
		PinType.ContainerType = EPinContainerType::None;
	}

	// Value type
	switch (Desc.ValueType)
	{
	case EPropertyBagPropertyType::Bool:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		break;
	case EPropertyBagPropertyType::Byte:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		break;
	case EPropertyBagPropertyType::Int32:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		break;
	case EPropertyBagPropertyType::Int64:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		break;
	case EPropertyBagPropertyType::Float:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		break;
	case EPropertyBagPropertyType::Double:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
		PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		break;
	case EPropertyBagPropertyType::Name:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		break;
	case EPropertyBagPropertyType::String:
		PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		break;
	case EPropertyBagPropertyType::Text:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		break;
	case EPropertyBagPropertyType::Enum:
		// @todo: some pin coloring is not correct due to this (byte-as-enum vs enum). 
		PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Struct:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Object:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftObject:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::Class:
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::SoftClass:
		PinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
		PinType.PinSubCategoryObject = const_cast<UObject*>(Desc.ValueTypeObject.Get());
		break;
	case EPropertyBagPropertyType::UInt32:	// Warning : Type only partially supported (Blueprint does not support unsigned type)
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		break;
	case EPropertyBagPropertyType::UInt64:	// Warning : Type only partially supported (Blueprint does not support unsigned type)
		PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		break;
	default:
		ensureMsgf(false, TEXT("Unhandled value type %s"), *UEnum::GetValueAsString(Desc.ValueType));
		break;
	}

	return PinType;
}

/** Sets property descriptor based on a Blueprint pin type. */
void SetPropertyDescFromPin(FPropertyBagPropertyDesc& Desc, const FEdGraphPinType& PinType)
{
	const UPropertyBagSchema* Schema = GetDefault<UPropertyBagSchema>();
	check(Schema);

	// remove any existing containers
	Desc.ContainerTypes.Reset();

	// Fill Container types, if any
	switch (PinType.ContainerType)
	{
	case EPinContainerType::Array:
		Desc.ContainerTypes.Add(EPropertyBagContainerType::Array);
		break;
	case EPinContainerType::Set:
		ensureMsgf(false, TEXT("Unsuported container type [Set] "));
		break;
	case EPinContainerType::Map:
		ensureMsgf(false, TEXT("Unsuported container type [Map] "));
		break;
	default:
		break;
	}
	
	// Value type
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean)
	{
		Desc.ValueType = EPropertyBagPropertyType::Bool;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Byte)
	{
		if (UEnum* Enum = Cast<UEnum>(PinType.PinSubCategoryObject))
		{
			Desc.ValueType = EPropertyBagPropertyType::Enum;
			Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
		}
		else
		{
			Desc.ValueType = EPropertyBagPropertyType::Byte;
			Desc.ValueTypeObject = nullptr;
		}
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		Desc.ValueType = EPropertyBagPropertyType::Int32;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Int64)
	{
		Desc.ValueType = EPropertyBagPropertyType::Int64;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Real)
	{
		if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Float)
		{
			Desc.ValueType = EPropertyBagPropertyType::Float;
			Desc.ValueTypeObject = nullptr;
		}
		else if (PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double)
		{
			Desc.ValueType = EPropertyBagPropertyType::Double;
			Desc.ValueTypeObject = nullptr;
		}		
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Name)
	{
		Desc.ValueType = EPropertyBagPropertyType::Name;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_String)
	{
		Desc.ValueType = EPropertyBagPropertyType::String;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Text)
	{
		Desc.ValueType = EPropertyBagPropertyType::Text;
		Desc.ValueTypeObject = nullptr;
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Enum)
	{
		Desc.ValueType = EPropertyBagPropertyType::Enum;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
	{
		Desc.ValueType = EPropertyBagPropertyType::Struct;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Object)
	{
		Desc.ValueType = EPropertyBagPropertyType::Object;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftObject)
	{
		Desc.ValueType = EPropertyBagPropertyType::SoftObject;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_Class)
	{
		Desc.ValueType = EPropertyBagPropertyType::Class;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else if (PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass)
	{
		Desc.ValueType = EPropertyBagPropertyType::SoftClass;
		Desc.ValueTypeObject = PinType.PinSubCategoryObject.Get();
	}
	else
	{
		ensureMsgf(false, TEXT("Unhandled pin category %s"), *PinType.PinCategory.ToString());
	}
}

template<typename TFunc>
void ApplyChangesToPropertyDescs(const FText& SessionName, const TSharedPtr<IPropertyHandle>& StructProperty, const TSharedPtr<IPropertyUtilities>& PropUtils, TFunc&& Function)
{
	if (!StructProperty || !PropUtils)
	{
		return;
	}
	
	FScopedTransaction Transaction(SessionName);
	TArray<FPropertyBagPropertyDesc> PropertyDescs = GetCommonPropertyDescs(StructProperty);
	StructProperty->NotifyPreChange();

	Function(PropertyDescs);

	SetPropertyDescs(StructProperty, PropertyDescs);
	
	StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
	StructProperty->NotifyFinishedChangingProperties();
	if (PropUtils)
	{
		PropUtils->ForceRefresh();
	}
}

bool CanHaveMemberVariableOfType(const FEdGraphPinType& PinType)
{
	if (PinType.PinCategory == UEdGraphSchema_K2::PC_Exec 
		|| PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard
		|| PinType.PinCategory == UEdGraphSchema_K2::PC_MCDelegate
		|| PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate
		|| PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
	{
		return false;
	}
	
	return true;
}

bool FindUserFunction(TSharedPtr<IPropertyHandle> InStructProperty, FName InFuncMetadataName, UFunction*& OutFunc, UObject*& OutTarget)
{
	FProperty* MetadataProperty = InStructProperty->GetMetaDataProperty();

	OutFunc = nullptr;
	OutTarget = nullptr;

	if (!MetadataProperty || !MetadataProperty->HasMetaData(InFuncMetadataName))
	{
		return false;
	}

	FString FunctionName = MetadataProperty->GetMetaData(InFuncMetadataName);
	if (FunctionName.IsEmpty())
	{
		return false;
	}

	TArray<UObject*> OutObjects;
	InStructProperty->GetOuterObjects(OutObjects);

	// Check for external function references, taken from GetOptions
	if (FunctionName.Contains(TEXT(".")))
	{
		OutFunc = FindObject<UFunction>(nullptr, *FunctionName, true);

		if (ensureMsgf(OutFunc && OutFunc->HasAnyFunctionFlags(EFunctionFlags::FUNC_Static), TEXT("[%s] Didn't find function %s or expected it to be static"), *InFuncMetadataName.ToString(), *FunctionName))
		{
			UObject* GetOptionsCDO = OutFunc->GetOuterUClass()->GetDefaultObject();
			OutTarget = GetOptionsCDO;
		}
	}
	else if (OutObjects.Num() > 0)
	{
		OutTarget = OutObjects[0];
		OutFunc = OutTarget->GetClass() ? OutTarget->GetClass()->FindFunctionByName(*FunctionName) : nullptr;
	}

	// Only support native functions
	if (!ensureMsgf(OutFunc && OutFunc->IsNative(), TEXT("[%s] Didn't find function %s or expected it to be native"), *InFuncMetadataName.ToString(), *FunctionName))
	{
		OutFunc = nullptr;
		OutTarget = nullptr;
	}

	return OutTarget != nullptr && OutFunc != nullptr;
}

// UFunction calling helpers. 
// Use our "own" hardcoded reflection system for types used in UFunctions calls in this file.
template<typename T> struct TypeName { static const TCHAR* Get() = delete; };

#define DEFINE_TYPENAME(InType) template<> struct TypeName<InType> { static const TCHAR *Get() { return TEXT(#InType); }};
DEFINE_TYPENAME(bool)
DEFINE_TYPENAME(FGuid)
DEFINE_TYPENAME(FName)
DEFINE_TYPENAME(FEdGraphPinType)
#undef DEFINE_TYPENAME

// Wrapper around a param that store an address (const_cast for const ptr, be careful of that)
// a string identifiying the underlying cpp type and if the input value is const, mark it const.
struct FFuncParam
{
	void* Value = nullptr;
	const TCHAR* CppType;
	bool bIsConst = false;

	template<typename T>
	static FFuncParam Make(T& Value)
	{
		FFuncParam Result;
		Result.Value = &Value;
		Result.CppType = TypeName<T>::Get();
		return Result;
	}

	template<typename T>
	static FFuncParam Make(const T& Value)
	{
		FFuncParam Result;
		Result.Value = &const_cast<T&>(Value);
		Result.CppType = TypeName<T>::Get();
		Result.bIsConst = true;
		return Result;
	}
};

// Validate that the function pass as parameter has signature ReturnType(ArgsTypes...)
template <typename ReturnType, typename... ArgsTypes>
bool ValidateFunctionSignature(UFunction* InFunc)
{
	if (!InFunc)
	{
		return false;
	}

	constexpr int32 NumParms = std::is_same_v <ReturnType, void> ? sizeof...(ArgsTypes) : (sizeof...(ArgsTypes) + 1);

	if (NumParms != InFunc->NumParms)
	{
		return false;
	}

	const TCHAR* ArgsCppTypes[NumParms] = { TypeName<ArgsTypes>::Get() ... };
	
	// If we have a return type, put it at the end. UFunction will have the return type after InArgs in the field iterator.
	if constexpr (!std::is_same_v<ReturnType, void>)
	{
		ArgsCppTypes[NumParms - 1] = TypeName<ReturnType>::Get();
	}
	else
	{
		// Otherwise, check that the function doesn't have a return param
		if (InFunc->GetReturnProperty() != nullptr)
		{
			return false;
		}
	}

	int32 ArgCppTypesIndex = 0;
	for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(EPropertyFlags::CPF_Parm); ++It)
	{
		const FString PropertyCppType = It->GetCPPType();
		if (PropertyCppType != ArgsCppTypes[ArgCppTypesIndex])
		{
			return false;
		}

		// Also making sure that the last param is a return param, if we have a return value
		if constexpr (!std::is_same_v<ReturnType, void>)
		{
			if (ArgCppTypesIndex == NumParms - 1 && !It->HasAnyPropertyFlags(EPropertyFlags::CPF_ReturnParm))
			{
				return false;
			}
		}

		ArgCppTypesIndex++;
	}

	return true;
}

template <typename ReturnType, typename... ArgsTypes>
TValueOrError<ReturnType, void> CallFunc(UObject* InTargetObject, UFunction* InFunc, ArgsTypes&& ...InArgs)
{
	if (!InTargetObject || !InFunc)
	{
		return MakeError();
	}

	constexpr int32 NumParms = std::is_same_v <ReturnType, void> ? sizeof...(ArgsTypes) : (sizeof...(ArgsTypes) + 1);

	if (NumParms != InFunc->NumParms)
	{
		return MakeError();
	}

	FFuncParam InParams[NumParms] = { FFuncParam::Make(std::forward<ArgsTypes>(InArgs)) ... };

	auto Invoke = [InTargetObject, InFunc, &InParams, NumParms](ReturnType* OutResult) -> bool
	{
		// Validate that the function has a return property if the return type is not void.
		if (std::is_same_v<ReturnType, void> != (InFunc->GetReturnProperty() == nullptr))
		{
			return false;
		}

		// Allocating our "stack" for the function call on the stack (will be freed when this function is exited)
		uint8* StackMemory = (uint8*)FMemory_Alloca(InFunc->ParmsSize);
		FMemory::Memzero(StackMemory, InFunc->ParmsSize);

		if constexpr (!std::is_same_v<ReturnType, void>)
		{
			check(OutResult != nullptr);
			InParams[NumParms - 1] = FFuncParam::Make(*OutResult);
		}

		bool bValid = true;
		int32 ParamIndex = 0;

		// Initializing our "stack" with our parameters. Use the property system to make sure more complex types
		// are constructed before being set.
		for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* LocalProp = *It;
			if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
			{
				LocalProp->InitializeValue_InContainer(StackMemory);
			}

			if (bValid)
			{
				if (ParamIndex >= NumParms)
				{
					bValid = false;
					continue;
				}

				FFuncParam& Param = InParams[ParamIndex++];

				if (LocalProp->GetCPPType() != Param.CppType)
				{
					bValid = false;
					continue;
				}

				LocalProp->SetValue_InContainer(StackMemory, Param.Value);
			}
		}

		if (bValid)
		{
			FFrame Stack(InTargetObject, InFunc, StackMemory, nullptr, InFunc->ChildProperties);
			InFunc->Invoke(InTargetObject, Stack, OutResult);
		}

		ParamIndex = 0;
		// Copy back all non-const out params (that is not the return param, this one is already set by the invoke call)
		// from the stack, also making sure that the constructed types are destroyed accordingly.
		for (TFieldIterator<FProperty> It(InFunc); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* LocalProp = *It;
			FFuncParam& Param = InParams[ParamIndex++];

			if (LocalProp->HasAnyPropertyFlags(CPF_OutParm) && !LocalProp->HasAnyPropertyFlags(CPF_ReturnParm) && !Param.bIsConst)
			{
				LocalProp->GetValue_InContainer(StackMemory, Param.Value);
			}

			LocalProp->DestroyValue_InContainer(StackMemory);
		}

		return bValid;
	};

	if constexpr (std::is_same_v<ReturnType, void>)
	{
		if (Invoke(nullptr))
		{
			return MakeValue();
		}
		else
		{
			return MakeError();
		}
	}
	else
	{
		ReturnType OutResult{};
		if (Invoke(&OutResult))
		{
			return MakeValue(std::move(OutResult));
		}
		else
		{
			return MakeError();
		}
	}
}

/** Checks if the value for a source property in a source struct has the same value that the target property in the target struct. */
bool ArePropertiesIdentical(
	const FPropertyBagPropertyDesc* InSourcePropertyDesc,
	const FInstancedPropertyBag& InSourceInstance,
	const FPropertyBagPropertyDesc* InTargetPropertyDesc,
	const FInstancedPropertyBag& InTargetInstance)
{
	if (!InSourceInstance.IsValid()
		|| !InTargetInstance.IsValid()
		|| !InSourcePropertyDesc
		|| !InSourcePropertyDesc->CachedProperty
		|| !InTargetPropertyDesc
		|| !InTargetPropertyDesc->CachedProperty)
	{
		return false;
	}

	if (!InSourcePropertyDesc->CompatibleType(*InTargetPropertyDesc))
	{
		return false;
	}

	const uint8* SourceValueAddress = InSourceInstance.GetValue().GetMemory() + InSourcePropertyDesc->CachedProperty->GetOffset_ForInternal();
	const uint8* TargetValueAddress = InTargetInstance.GetValue().GetMemory() + InTargetPropertyDesc->CachedProperty->GetOffset_ForInternal();

	return InSourcePropertyDesc->CachedProperty->Identical(SourceValueAddress, TargetValueAddress);
}

/** Copy the value for a source property in a source struct to the target property in the target struct. */
void CopyPropertyValue(const FPropertyBagPropertyDesc* InSourcePropertyDesc, const FInstancedPropertyBag& InSourceInstance, const FPropertyBagPropertyDesc* InTargetPropertyDesc, FInstancedPropertyBag& InTargetInstance)
{
	if (!InSourceInstance.IsValid() || !InTargetInstance.IsValid() || !InSourcePropertyDesc || !InSourcePropertyDesc->CachedProperty || !InTargetPropertyDesc || !InTargetPropertyDesc->CachedProperty)
	{
		return;
	}

	// Can't copy if they are not compatible.
	if (!InSourcePropertyDesc->CompatibleType(*InTargetPropertyDesc))
	{
		return;
	}

	const uint8* SourceValueAddress = InSourceInstance.GetValue().GetMemory() + InSourcePropertyDesc->CachedProperty->GetOffset_ForInternal();
	uint8* TargetValueAddress = InTargetInstance.GetMutableValue().GetMemory() + InTargetPropertyDesc->CachedProperty->GetOffset_ForInternal();

	InSourcePropertyDesc->CachedProperty->CopyCompleteValue(TargetValueAddress, SourceValueAddress);
}

} // UE::StructUtils::Private


//----------------------------------------------------------------//
//  FPropertyBagInstanceDataDetails
//  - StructProperty is FInstancedPropertyBag
//  - ChildPropertyHandle a child property of the FInstancedPropertyBag::Value (FInstancedStruct)  
//----------------------------------------------------------------//

FPropertyBagInstanceDataDetails::FPropertyBagInstanceDataDetails(TSharedPtr<IPropertyHandle> InStructProperty, const TSharedPtr<IPropertyUtilities>& InPropUtils, const bool bInFixedLayout, const bool bInAllowArrays)
	: FInstancedStructDataDetails(InStructProperty.IsValid() ? InStructProperty->GetChildHandle(TEXT("Value")) : nullptr)
	, BagStructProperty(InStructProperty)
	, PropUtils(InPropUtils)
	, bFixedLayout(bInFixedLayout)
	, bAllowArrays(bInAllowArrays)
{
	ensure(UE::StructUtils::Private::IsScriptStruct<FInstancedPropertyBag>(BagStructProperty));
	ensure(PropUtils != nullptr);
}

void FPropertyBagInstanceDataDetails::OnChildRowAdded(IDetailPropertyRow& ChildRow)
{
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	ChildRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);

	TSharedPtr<IPropertyHandle> ChildPropertyHandle = ChildRow.GetPropertyHandle();
	
	bool bSupportedType = true;

	FText UnsupportedTypeWarning;

	TArray<FPropertyBagPropertyDesc> PropertyDescs = UE::StructUtils::Private::GetCommonPropertyDescs(BagStructProperty);
	const FProperty* Property = ChildPropertyHandle->GetProperty();
	if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
	{
		static const FName HideInDetailPanelsName(TEXT("HideInDetailPanel"));

		if (Desc->ContainerTypes.Num() > 1)
		{
			static const FText UnsupportedTypeWarningNestedContainer = LOCTEXT("NestedContainersWarning", "This property type is not supported in the property bag UI.");

			// The property editing for nested containers is not supported.
			UnsupportedTypeWarning = UnsupportedTypeWarningNestedContainer;
			bSupportedType = false;
			// Do not allow to edit the data.
			ChildRow.IsEnabled(false);
		}
		else if ((Desc->ValueType == EPropertyBagPropertyType::UInt32 || Desc->ValueType == EPropertyBagPropertyType::UInt64)
			&& !bFixedLayout)
		{
			static const FText UnsupportedTypeWarningUnsigned = LOCTEXT("UnsignedTypesWarning", "Unsigned types are not supported throught the property type selection. If you change the type, you will not be able to change it back.");

			// Warn that the unsinged types cannot be set via the type selection.
			UnsupportedTypeWarning = UnsupportedTypeWarningUnsigned;
			bSupportedType = false;
		}
		else if (Property->HasMetaData(HideInDetailPanelsName))
		{
			ChildRow.Visibility(EVisibility::Hidden);
			return;
		}
	}

	if (!bFixedLayout)
	{
		// Inline editable name
		TSharedPtr<SInlineEditableTextBlock> InlineWidget = SNew(SInlineEditableTextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.MultiLine(false)
			.Text_Lambda([ChildPropertyHandle]()
			{
				return FText::FromName(UE::StructUtils::Private::GetPropertyNameSafe(ChildPropertyHandle));
			})
			.OnVerifyTextChanged_Lambda([BagStructProperty = BagStructProperty, ChildPropertyHandle](const FText& InText, FText& OutErrorMessage)
			{
				const FName NewName = UE::StructUtils::Private::GetValidPropertyName(InText.ToString());
				const FName OldName = UE::StructUtils::Private::GetPropertyNameSafe(ChildPropertyHandle);
				bool bResult = UE::StructUtils::Private::IsUniqueName(NewName, OldName, BagStructProperty);
				if (!bResult)
				{
					OutErrorMessage = LOCTEXT("MustBeUniqueName", "Property must have unique name");
				}
				return bResult;
			})
			.OnTextCommitted_Lambda([BagStructProperty = BagStructProperty, PropUtils = PropUtils, ChildPropertyHandle](const FText& InNewText, ETextCommit::Type InCommitType)
			{
				if (InCommitType == ETextCommit::OnCleared)
				{
					return;
				}

				const FName NewName = UE::StructUtils::Private::GetValidPropertyName(InNewText.ToString());
				const FName OldName = UE::StructUtils::Private::GetPropertyNameSafe(ChildPropertyHandle);
				if (!UE::StructUtils::Private::IsUniqueName(NewName, OldName, BagStructProperty))
				{
					return;
				}

				UE::StructUtils::Private::ApplyChangesToPropertyDescs(
					LOCTEXT("OnPropertyNameChanged", "Change Property Name"), BagStructProperty, PropUtils,
					[&NewName, &ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
					{
						const FProperty* Property = ChildPropertyHandle->GetProperty();
						if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
						{
							Desc->Name = NewName;
						}
					});
			});

		NameWidget = SNew(SComboButton)
			.OnGetMenuContent(this, &FPropertyBagInstanceDataDetails::OnPropertyNameContent, ChildRow.GetPropertyHandle(), InlineWidget)
			.ContentPadding(2)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ButtonContent()
			[
				InlineWidget.ToSharedRef()
			];
	}

	ChildRow
		.CustomWidget(/*bShowChildren*/true)
		.NameContent()
		[
			SNew(SHorizontalBox)
			// Error icon
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0,0,2,0)
			[
				SNew(SBox)
				.WidthOverride(12)
				.HeightOverride(12)
				[
					SNew(SImage)
					.ToolTipText_Lambda([UnsupportedTypeWarning, bSupportedType]()
					{
						return !bSupportedType
							? UnsupportedTypeWarning
							: LOCTEXT("MissingType", "The property is missing type. The Struct, Enum, or Object may have been removed.");
					})
					.Visibility_Lambda([ChildPropertyHandle, bSupportedType]()
					{
						return UE::StructUtils::Private::HasMissingType(ChildPropertyHandle) || !bSupportedType ? EVisibility::Visible : EVisibility::Collapsed;
					})
					.Image(FAppStyle::GetBrush("Icons.Error"))
				]
			]
			// Name
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				NameWidget.ToSharedRef()
			]
		]
		.ValueContent()
		[
			ValueWidget.ToSharedRef()
		];


	if (HasPropertyOverrides())
	{
		TWeakPtr<FPropertyBagInstanceDataDetails> WeakSelf = SharedThis<FPropertyBagInstanceDataDetails>(this);

		TAttribute<bool> EditConditionValue = TAttribute<bool>::CreateLambda(
			[WeakSelf, ChildPropertyHandle]() -> bool
			{
				if (const TSharedPtr<FPropertyBagInstanceDataDetails> Self = WeakSelf.Pin())
				{
					return Self->IsPropertyOverridden(ChildPropertyHandle) == EPropertyOverrideState::Yes;
				}
				return true;
			});
		
		FOnBooleanValueChanged OnEditConditionChanged = FOnBooleanValueChanged::CreateLambda([WeakSelf, ChildPropertyHandle](bool bNewValue)
		{
			if (const TSharedPtr<FPropertyBagInstanceDataDetails> Self = WeakSelf.Pin())
			{
				Self->SetPropertyOverride(ChildPropertyHandle, bNewValue);
			}
		});

		ChildRow.EditCondition(std::move(EditConditionValue), std::move(OnEditConditionChanged));

		FIsResetToDefaultVisible IsResetVisible = FIsResetToDefaultVisible::CreateLambda([WeakSelf](TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			if (const TSharedPtr<FPropertyBagInstanceDataDetails> Self = WeakSelf.Pin())
			{
				return !Self->IsDefaultValue(PropertyHandle);
			}
			return false;
		});
		FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateLambda([WeakSelf](TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			if (const TSharedPtr<FPropertyBagInstanceDataDetails> Self = WeakSelf.Pin())
			{
				Self->ResetToDefault(PropertyHandle);
			}
		});
		FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(IsResetVisible, ResetHandler);

		ChildRow.OverrideResetToDefault(ResetOverride);
	}
}

FPropertyBagInstanceDataDetails::EPropertyOverrideState FPropertyBagInstanceDataDetails::IsPropertyOverridden(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const
{
	if (!ChildPropertyHandle)
	{
		return EPropertyOverrideState::Undetermined;;
	}

	int32 NumValues = 0;
	int32 NumOverrides = 0; 

	const FProperty* Property = ChildPropertyHandle->GetProperty();
	check(Property);

	EnumeratePropertyBags(BagStructProperty,
		[Property, &NumValues, &NumOverrides]
		(const FInstancedPropertyBag& DefaultPropertyBag, const FInstancedPropertyBag& PropertyBag, const IPropertyBagOverrideProvider& OverrideProvider)
		{
			NumValues++;
			if (const UPropertyBag* Bag = PropertyBag.GetPropertyBagStruct())
			{
				const FPropertyBagPropertyDesc* PropertyDesc = Bag->FindPropertyDescByProperty(Property);
				if (PropertyDesc && OverrideProvider.IsPropertyOverridden(PropertyDesc->ID))
				{
					NumOverrides++;
				}
			}

			return true;
		});
	
	if (NumOverrides == 0)
	{
		return EPropertyOverrideState::No;
	}
	else if (NumOverrides == NumValues)
	{
		return EPropertyOverrideState::Yes;
	}			
	return EPropertyOverrideState::Undetermined;
}
	
void FPropertyBagInstanceDataDetails::SetPropertyOverride(TSharedPtr<IPropertyHandle> ChildPropertyHandle, const bool bIsOverridden)
{
	if (!ChildPropertyHandle)
	{
		return;
	}

	const FProperty* Property = ChildPropertyHandle->GetProperty();
	check(Property);

	FScopedTransaction Transaction(FText::Format(LOCTEXT("OverrideChange", "Change Override for {0}"), FText::FromName(ChildPropertyHandle->GetProperty()->GetFName())));
	
	PreChangeOverrides();
	
	EnumeratePropertyBags(
		BagStructProperty,
		[Property, bIsOverridden]
		(const FInstancedPropertyBag& DefaultPropertyBag, const FInstancedPropertyBag& PropertyBag, const IPropertyBagOverrideProvider& OverrideProvider)
		{
			if (const UPropertyBag* Bag = PropertyBag.GetPropertyBagStruct())
			{
				if (const FPropertyBagPropertyDesc* PropertyDesc = Bag->FindPropertyDescByProperty(Property))
				{
					OverrideProvider.SetPropertyOverride(PropertyDesc->ID, bIsOverridden);
				}
			}

			return true;
		});

	PostChangeOverrides();
}

bool FPropertyBagInstanceDataDetails::IsDefaultValue(TSharedPtr<IPropertyHandle> ChildPropertyHandle) const
{
	if (!ChildPropertyHandle)
	{
		return true;
	}

	int32 NumValues = 0;
	int32 NumOverridden = 0;
	int32 NumIdentical = 0;

	const FProperty* Property = ChildPropertyHandle->GetProperty();
	check(Property);

	EnumeratePropertyBags(
		BagStructProperty,
		[Property, &NumValues, &NumOverridden, &NumIdentical]
		(const FInstancedPropertyBag& DefaultPropertyBag, const FInstancedPropertyBag& PropertyBag, const IPropertyBagOverrideProvider& OverrideProvider)
		{
			NumValues++;

			const UPropertyBag* DefaultBag = DefaultPropertyBag.GetPropertyBagStruct();
			const UPropertyBag* Bag = PropertyBag.GetPropertyBagStruct();
			if (Bag && DefaultBag)
			{
				const FPropertyBagPropertyDesc* PropertyDesc = Bag->FindPropertyDescByProperty(Property);
				const FPropertyBagPropertyDesc* DefaultPropertyDesc = DefaultBag->FindPropertyDescByProperty(Property);
				if (PropertyDesc
					&& DefaultPropertyDesc
					&& OverrideProvider.IsPropertyOverridden(PropertyDesc->ID))
				{
					NumOverridden++;
					if (UE::StructUtils::Private::ArePropertiesIdentical(DefaultPropertyDesc, DefaultPropertyBag, PropertyDesc, PropertyBag))
					{
						NumIdentical++;
					}
				}
			}
			return true;
		});

	if (NumOverridden == NumIdentical)
	{
		return true;
	}
	
	return false;
}

void FPropertyBagInstanceDataDetails::ResetToDefault(TSharedPtr<IPropertyHandle> ChildPropertyHandle)
{
	if (!ChildPropertyHandle)
	{
		return;
	}
	
	const FProperty* Property = ChildPropertyHandle->GetProperty();
	check(Property);

	FScopedTransaction Transaction(FText::Format(LOCTEXT("ResetToDefault", "Reset {0} to default value"), FText::FromName(ChildPropertyHandle->GetProperty()->GetFName())));
	ChildPropertyHandle->NotifyPreChange();
	
	EnumeratePropertyBags(
		BagStructProperty,
		[Property]
		(const FInstancedPropertyBag& DefaultPropertyBag, FInstancedPropertyBag& PropertyBag, const IPropertyBagOverrideProvider& OverrideProvider)
		{
			const UPropertyBag* DefaultBag = DefaultPropertyBag.GetPropertyBagStruct();
			const UPropertyBag* Bag = PropertyBag.GetPropertyBagStruct();
			if (Bag && DefaultBag)
			{
				const FPropertyBagPropertyDesc* PropertyDesc = Bag->FindPropertyDescByProperty(Property);
				const FPropertyBagPropertyDesc* DefaultPropertyDesc = DefaultBag->FindPropertyDescByProperty(Property);
				if (PropertyDesc
					&& DefaultPropertyDesc
					&& OverrideProvider.IsPropertyOverridden(PropertyDesc->ID))
				{
					UE::StructUtils::Private::CopyPropertyValue(DefaultPropertyDesc, DefaultPropertyBag, PropertyDesc, PropertyBag);
				}
			}
			return true;
		});

	ChildPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	ChildPropertyHandle->NotifyFinishedChangingProperties();
}

TSharedRef<SWidget> FPropertyBagInstanceDataDetails::OnPropertyNameContent(TSharedPtr<IPropertyHandle> ChildPropertyHandle, TSharedPtr<SInlineEditableTextBlock> InlineWidget) const
{
	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, nullptr);

	auto GetFilteredVariableTypeTree = [BagStructProperty = BagStructProperty](TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>>& TypeTree, ETypeTreeFilter TypeTreeFilter)
	{
		// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
    	if (!BagStructProperty || !BagStructProperty->IsValidHandle())
    	{
    		return;
    	}

		UFunction* IsPinTypeAcceptedFunc = nullptr;
		UObject* IsPinTypeAcceptedTarget = nullptr;
		if (UE::StructUtils::Private::FindUserFunction(BagStructProperty, UE::StructUtils::Metadata::IsPinTypeAcceptedName, IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget))
		{
			check(IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget);

			// We need to make sure the signature matches perfectly: bool(FEdGraphPinType, bool)
			bool bFuncIsValid = UE::StructUtils::Private::ValidateFunctionSignature<bool, FEdGraphPinType, bool>(IsPinTypeAcceptedFunc);
			if (!ensureMsgf(bFuncIsValid, TEXT("[%s] Function %s does not have the right signature."), *UE::StructUtils::Metadata::IsPinTypeAcceptedName.ToString(), *IsPinTypeAcceptedFunc->GetName()))
			{
				return;
			}
		}

		auto IsPinTypeAccepted = [IsPinTypeAcceptedFunc, IsPinTypeAcceptedTarget](const FEdGraphPinType& InPinType, bool bInIsChild) -> bool
		{
			if (IsPinTypeAcceptedFunc && IsPinTypeAcceptedTarget)
			{
				const TValueOrError<bool, void> bIsValid = UE::StructUtils::Private::CallFunc<bool>(IsPinTypeAcceptedTarget, IsPinTypeAcceptedFunc, InPinType, bInIsChild);
				return bIsValid.HasValue() && bIsValid.GetValue();
			}
			else
			{
				return true;
			}
		};

		check(GetDefault<UEdGraphSchema_K2>());
		TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TempTypeTree;
		GetDefault<UPropertyBagSchema>()->GetVariableTypeTree(TempTypeTree, TypeTreeFilter);

		// Filter
		for (TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& PinType : TempTypeTree)
		{
			if (!PinType.IsValid() || !IsPinTypeAccepted(PinType->GetPinType(/*bForceLoadSubCategoryObject*/false), /*bInIsChild=*/ false))
			{
				continue;
			}

			for (int32 ChildIndex = 0; ChildIndex < PinType->Children.Num(); )
			{
				TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo> Child = PinType->Children[ChildIndex];
				if (Child.IsValid())
				{
					const FEdGraphPinType& ChildPinType = Child->GetPinType(/*bForceLoadSubCategoryObject*/false);

					if (!UE::StructUtils::Private::CanHaveMemberVariableOfType(ChildPinType) || !IsPinTypeAccepted(ChildPinType, /*bInIsChild=*/ true))
					{
						PinType->Children.RemoveAt(ChildIndex);
						continue;
					}
				}
				++ChildIndex;
			}

			TypeTree.Add(PinType);
		}
	};

	auto GetPinInfo = [BagStructProperty = BagStructProperty, ChildPropertyHandle]()
	{
		// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
		if (!BagStructProperty || !BagStructProperty->IsValidHandle() || !ChildPropertyHandle || !ChildPropertyHandle->IsValidHandle())
		{
			return FEdGraphPinType();
		}
		
		TArray<FPropertyBagPropertyDesc> PropertyDescs = UE::StructUtils::Private::GetCommonPropertyDescs(BagStructProperty);

		const FProperty* Property = ChildPropertyHandle->GetProperty();
		if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
		{
			return UE::StructUtils::Private::GetPropertyDescAsPin(*Desc);
		}
	
		return FEdGraphPinType();
	};

	auto PinInfoChanged = [BagStructProperty = BagStructProperty, PropUtils = PropUtils, ChildPropertyHandle](const FEdGraphPinType& PinType)
	{
		// The SPinTypeSelector popup might outlive this details view, so bag struct property can be invalid here.
		if (!BagStructProperty || !BagStructProperty->IsValidHandle() || !ChildPropertyHandle || !ChildPropertyHandle->IsValidHandle())
		{
			return;
		}
		
		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
			LOCTEXT("OnPropertyTypeChanged", "Change Property Type"), BagStructProperty, PropUtils,
			[&PinType, &ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
			{
				// Find and change struct type
				const FProperty* Property = ChildPropertyHandle ? ChildPropertyHandle->GetProperty() : nullptr;
				if (FPropertyBagPropertyDesc* Desc = PropertyDescs.FindByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; }))
				{
					UE::StructUtils::Private::SetPropertyDescFromPin(*Desc, PinType);
				}
			});
	};

	auto RemoveProperty = [BagStructProperty = BagStructProperty, PropUtils = PropUtils, ChildPropertyHandle]()
	{
		if (!BagStructProperty || !BagStructProperty->IsValidHandle() || !ChildPropertyHandle || !ChildPropertyHandle->IsValidHandle())
		{
			return;
		}

		// Extra check provided by the user to cancel a remove action. Useful to provide the user a possibility to cancel the action if
		// the given property is in use elsewhere.
		UFunction* CanRemovePropertyFunc = nullptr;
		UObject* CanRemovePropertyTarget = nullptr;
		if (UE::StructUtils::Private::FindUserFunction(BagStructProperty, UE::StructUtils::Metadata::CanRemovePropertyName, CanRemovePropertyFunc, CanRemovePropertyTarget))
		{
			check(CanRemovePropertyFunc && CanRemovePropertyTarget);

			FName PropertyName = ChildPropertyHandle->GetProperty()->GetFName();
			const UPropertyBag* PropertyBag = UE::StructUtils::Private::GetCommonBagStruct(BagStructProperty);
			const FPropertyBagPropertyDesc* PropertyDesc = PropertyBag ? PropertyBag->FindPropertyDescByName(PropertyName) : nullptr;

			if (!PropertyDesc)
			{
				return;
			}

			// We need to make sure the signature matches perfectly: bool(FGuid, FName)s
			bool bFuncIsValid = UE::StructUtils::Private::ValidateFunctionSignature<bool, FGuid, FName>(CanRemovePropertyFunc);
			if(!ensureMsgf(bFuncIsValid, TEXT("[%s] Function %s does not have the right signature."), *UE::StructUtils::Metadata::CanRemovePropertyName.ToString(), *CanRemovePropertyFunc->GetName()))
			{
				return;
			}

			const TValueOrError<bool, void> bCanRemove = UE::StructUtils::Private::CallFunc<bool>(CanRemovePropertyTarget, CanRemovePropertyFunc, PropertyDesc->ID, PropertyDesc->Name);

			if (bCanRemove.HasError() || !bCanRemove.GetValue())
			{
				return;
			}
		}

		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
			LOCTEXT("OnPropertyRemoved", "Remove Property"), BagStructProperty, PropUtils,
			[&ChildPropertyHandle](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
			{
				const FProperty* Property = ChildPropertyHandle ? ChildPropertyHandle->GetProperty() : nullptr;
				PropertyDescs.RemoveAll([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; });
			});
	};

	auto MoveProperty = [BagStructProperty = BagStructProperty, PropUtils = PropUtils, ChildPropertyHandle](const int32 Delta)
	{
		if (!BagStructProperty || !BagStructProperty->IsValidHandle() || !ChildPropertyHandle || !ChildPropertyHandle->IsValidHandle())
		{
			return;
		}
		
		UE::StructUtils::Private::ApplyChangesToPropertyDescs(
		LOCTEXT("OnPropertyMoved", "Move Property"), BagStructProperty, PropUtils,
		[&ChildPropertyHandle, &Delta](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
		{
			// Move
			if (PropertyDescs.Num() > 1)
			{
				const FProperty* Property = ChildPropertyHandle ? ChildPropertyHandle->GetProperty() : nullptr;
				const int32 PropertyIndex = PropertyDescs.IndexOfByPredicate([Property](const FPropertyBagPropertyDesc& Desc){ return Desc.CachedProperty == Property; });
				if (PropertyIndex != INDEX_NONE)
				{
					const int32 NewPropertyIndex = FMath::Clamp(PropertyIndex + Delta, 0, PropertyDescs.Num() - 1);
					PropertyDescs.Swap(PropertyIndex, NewPropertyIndex);
				}
			}
		});
	};


	MenuBuilder.AddWidget(
		SNew(SBox)
		.HAlign(HAlign_Right)
		.Padding(FMargin(12, 0, 12, 0))
		[
			SNew(SPinTypeSelector, FGetPinTypeTree::CreateLambda(GetFilteredVariableTypeTree))
				.TargetPinType_Lambda(GetPinInfo)
				.OnPinTypeChanged_Lambda(PinInfoChanged)
				.Schema(GetDefault<UPropertyBagSchema>())
				.bAllowArrays(bAllowArrays)
				.TypeTreeFilter(ETypeTreeFilter::None)
				.Font( IDetailLayoutBuilder::GetDetailFont() )
		],
		FText::GetEmpty());
	
	MenuBuilder.AddSeparator();
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Rename", "Rename"),
		LOCTEXT("Rename_ToolTip", "Rename property"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
		FUIAction(FExecuteAction::CreateLambda([InlineWidget]()  { InlineWidget->EnterEditingMode(); }))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Remove", "Remove"),
		LOCTEXT("Remove_ToolTip", "Remove property"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
		FUIAction(FExecuteAction::CreateLambda(RemoveProperty))
	);
	
	MenuBuilder.AddSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MoveUp", "Move Up"),
		LOCTEXT("MoveUp_ToolTip", "Move property up in the list"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowUp"),
		FUIAction(FExecuteAction::CreateLambda(MoveProperty, -1))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MoveDown", "Move Down"),
		LOCTEXT("MoveDown_ToolTip", "Move property down in the list"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.ArrowDown"),
		FUIAction(FExecuteAction::CreateLambda(MoveProperty, +1))
	);

	return MenuBuilder.MakeWidget();
}


////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FPropertyBagDetails::MakeInstance()
{
	return MakeShared<FPropertyBagDetails>();
}

void FPropertyBagDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();
	
	StructProperty = StructPropertyHandle;
	check(StructProperty);
	
	static const FName NAME_ShowOnlyInnerProperties = "ShowOnlyInnerProperties";
	
	if (const FProperty* MetaDataProperty = StructProperty->GetMetaDataProperty())
	{
		bFixedLayout = MetaDataProperty->HasMetaData(UE::StructUtils::Metadata::FixedLayoutName);

		bAllowArrays = MetaDataProperty->HasMetaData(UE::StructUtils::Metadata::AllowArraysName)
			? MetaDataProperty->GetBoolMetaData(UE::StructUtils::Metadata::AllowArraysName)
			: true;

		// Don't show the header if ShowOnlyInnerProperties is set
		if (MetaDataProperty->HasMetaData(NAME_ShowOnlyInnerProperties))
		{
			return;
		}

		if (MetaDataProperty->HasMetaData(UE::StructUtils::Metadata::DefaultTypeName))
		{
			if (UEnum* Enum = StaticEnum<EPropertyBagPropertyType>())
			{
				int32 EnumIndex = Enum->GetIndexByNameString(MetaDataProperty->GetMetaData(UE::StructUtils::Metadata::DefaultTypeName));
				if (EnumIndex != INDEX_NONE)
				{
					DefaultType = EPropertyBagPropertyType(Enum->GetValueByIndex(EnumIndex));
				}
			}
		}
	}

	TSharedPtr<SWidget> ValueWidget = SNullWidget::NullWidget;
	if (!bFixedLayout)
	{
		ValueWidget = MakeAddPropertyWidget(StructProperty, PropUtils, DefaultType);
	}
	
	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.VAlign(VAlign_Center)
		[
			ValueWidget.ToSharedRef()
		]
		.ShouldAutoExpand(true);
}

void FPropertyBagDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Show the Value (FInstancedStruct) as child rows.
	const TSharedRef<FPropertyBagInstanceDataDetails> InstanceDetails = MakeShareable(new FPropertyBagInstanceDataDetails(StructProperty, PropUtils, bFixedLayout, bAllowArrays));
	StructBuilder.AddCustomBuilder(InstanceDetails);
}

TSharedPtr<SWidget> FPropertyBagDetails::MakeAddPropertyWidget(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyUtilities> InPropUtils, EPropertyBagPropertyType DefaultType)
{
	return SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("AddProperty_Tooltip", "Add new property"))
			.OnClicked_Lambda([StructProperty = InStructProperty, PropUtils = InPropUtils, DefaultType]()
			{
				constexpr int32 MaxIterations = 100;
				FName NewName(TEXT("NewProperty"));
				int32 Number = 1;
				while (!UE::StructUtils::Private::IsUniqueName(NewName, FName(), StructProperty) && Number < MaxIterations)
				{
					Number++;
					NewName.SetNumber(Number);
				}
				if (Number == MaxIterations)
				{
					return FReply::Handled();
				}

				UE::StructUtils::Private::ApplyChangesToPropertyDescs(
					LOCTEXT("OnPropertyAdded", "Add Property"), StructProperty, PropUtils,
					[&NewName, DefaultType](TArray<FPropertyBagPropertyDesc>& PropertyDescs)
					{
						PropertyDescs.Emplace(NewName, DefaultType);
					});
					
				return FReply::Handled();

			})
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.PlusCircle"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];

}

////////////////////////////////////

bool UPropertyBagSchema::SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction,
		const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const
{
	return ContainerType == EPinContainerType::None || ContainerType == EPinContainerType::Array;
}

#undef LOCTEXT_NAMESPACE
