// Copyright Epic Games, Inc. All Rights Reserved.

#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include "PCGData.h"
#include "PCGParamData.h"
#include "PCGSettings.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Metadata/Accessors/PCGPropertyAccessor.h"
#include "Metadata/Accessors/PCGAttributeExtractor.h"

#include "Engine/UserDefinedStruct.h"
#include "UObject/EnumProperty.h"

namespace PCGAttributeAccessorHelpers
{
	/** Retrieves metadata object and the corresponding attribute if any for the provided data. */
	void ExtractMetadataAttribute(UPCGData* InData, FName Name, UPCGMetadata*& OutMetadata, FPCGMetadataAttributeBase*& OutAttribute)
	{
		OutMetadata = InData->MutableMetadata();
		OutAttribute = OutMetadata ? OutMetadata->GetMutableAttribute(Name) : nullptr;
	}

	// This template creates a const or non-const accessor on an attribute depending on the provided templated type. 
	template <
		typename AccessorType,
		typename = typename std::enable_if_t<
		std::is_same_v<IPCGAttributeAccessor, std::remove_const_t<AccessorType>>
		>
	>
	TUniquePtr<AccessorType> CreateAttributeAccessorImpl(FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, bool bForceReadOnly = false)
	{
		auto CreateTypedAccessor = [Attribute, Metadata, bForceReadOnly](auto Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AttributeType = decltype(Dummy);
			if constexpr (std::is_const_v<AccessorType>)
			{
				return MakeUnique<FPCGAttributeAccessor<AttributeType>>(static_cast<const FPCGMetadataAttribute<AttributeType>*>(Attribute), Metadata, bForceReadOnly);
			}
			else
			{
				return MakeUnique<FPCGAttributeAccessor<AttributeType>>(static_cast<FPCGMetadataAttribute<AttributeType>*>(Attribute), Metadata, bForceReadOnly);
			}
		};

		if (Attribute && Metadata)
		{
			return PCGMetadataAttribute::CallbackWithRightType(Attribute->GetTypeId(), CreateTypedAccessor);
		}
		else
		{
			return TUniquePtr<AccessorType>();
		}
	}

	// Don't be afraid of this enormous template!
	// We want to be able to create a TUniquePtr<const IPCGAttributeAccessor> for const accessors and TUniquePtr<IPCGAttributeAccessor> for mutable accessors.
	// This template just says that:
	// * AccessorType needs to be IPCGAttributeAccessor or const IPCGAttributeAccessor
	// * DataType needs to be UPCGData or const UPCGData
	// * And if one of them is const, they both need to be const.
	// It allows us to factorise the same logic for const and non-const version (with a few const_cast in the process, but we still have something const at the end)
	template <
		typename AccessorType, 
		typename DataType, 
		typename = typename std::enable_if_t<
			std::is_same_v<IPCGAttributeAccessor, std::remove_const_t<AccessorType>> &&
			std::is_same_v<UPCGData, std::remove_const_t<DataType>> &&
			std::is_const_v<AccessorType> == std::is_const_v<DataType>
		>
	>
	TUniquePtr<AccessorType> CreateAccessorImpl(DataType* InData, const FPCGAttributePropertySelector& InSelector, bool bQuiet)
	{
		const FName Name = InSelector.GetName();
		TUniquePtr<IPCGAttributeAccessor> Accessor;

		if (InSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
		{
			if (!InData || InData->template IsA<UPCGPointData>())
			{
				if (const FProperty* Property = FPCGPoint::StaticStruct()->FindPropertyByName(Name))
				{
					Accessor = CreatePropertyAccessor(Property);
				}
				else if (FPCGPoint::HasCustomPropertyGetterSetter(Name))
				{
					Accessor = FPCGPoint::CreateCustomPropertyAccessor(Name);
				}
			}
		}
		else if (InSelector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			Accessor = CreateExtraAccessor(InSelector.GetExtraProperty());

			if (!Accessor.IsValid())
			{
				if (!bQuiet)
				{
					UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::ConstAccessor] Expected to select an extra property but the data doesn't support this property."));
				}
				
				return TUniquePtr<IPCGAttributeAccessor>();
			}
		}

		// At this point, it is not a point data or we didn't find a property.
		// We can't continue if it is a property wanted.
		if (InSelector.GetSelection() == EPCGAttributePropertySelection::PointProperty && !Accessor.IsValid())
		{
			if (!bQuiet)
			{
				UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Expected to select a property but the data doesn't support this property."));
			}
			
			return TUniquePtr<IPCGAttributeAccessor>();
		}

		if (InSelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			UPCGMetadata* Metadata = nullptr;
			FPCGMetadataAttributeBase* Attribute = nullptr;

			// It is OK to const_cast here, since we will create a const accessor if the input is const.
			ExtractMetadataAttribute(const_cast<UPCGData*>(InData), Name, Metadata, Attribute);

			// To simplify the code here & below we'll get a non-const accessor but force readonly if it should be
			Accessor = CreateAttributeAccessorImpl<std::remove_const_t<AccessorType>>(Attribute, Metadata, /*bForceReadOnly=*/std::is_const_v<AccessorType>);
		}

		if (!Accessor.IsValid())
		{
			return TUniquePtr<AccessorType>();
		}

		// At this point, check if we have chain accessors
		for (const FString& ExtraName : InSelector.GetExtraNames())
		{
			bool bSuccess = false;
			Accessor = CreateChainAccessor(std::move(Accessor), FName(ExtraName), bSuccess);
			if (!bSuccess)
			{
				if (!bQuiet)
				{
					UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Extra selectors don't match existing properties."));
				}
				
				return TUniquePtr<AccessorType>();
			}
		}

		// Can't create mutable accessor that are read only.
		if constexpr (!std::is_const_v<AccessorType>)
		{
			if (Accessor && Accessor->IsReadOnly())
			{
				if (!bQuiet)
				{
					UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Attribute can not be written into, since it is read-only."));
				}
				
				return TUniquePtr<AccessorType>();
			}

			// Set the cached selector
			InData->SetLastSelector(InSelector);
		}

		return Accessor;
	}

	// Same thing for the keys. Check CreateAccessorImpl for more info about this big template.
	template <
		typename KeysType,
		typename DataType,
		typename = typename std::enable_if_t<
			std::is_same_v<IPCGAttributeAccessorKeys, std::remove_const_t<KeysType>> &&
			std::is_same_v<UPCGData, std::remove_const_t<DataType>> &&
			std::is_const_v<KeysType> == std::is_const_v<DataType>
		>
	>
	TUniquePtr<KeysType> CreateKeysImpl(DataType* InData, const FPCGAttributePropertySelector& InSelector)
	{
		if (UPCGPointData* PointData = Cast<UPCGPointData>(const_cast<UPCGData*>(InData)))
		{
			if constexpr (std::is_const_v<KeysType>)
			{
				return MakeUnique<FPCGAttributeAccessorKeysPoints>(PointData->GetPoints());
			}
			else
			{
				TArrayView<FPCGPoint> View(PointData->GetMutablePoints());
				return MakeUnique<FPCGAttributeAccessorKeysPoints>(View);
			}
		}
		else if (InData) // e.g. is UPCGData and non-null
		{
			if constexpr (std::is_const_v<KeysType>)
			{
				if (const UPCGMetadata* Metadata = InData->ConstMetadata())
				{
					return MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);
				}
			}
			else
			{
				if (UPCGMetadata* Metadata = InData->MutableMetadata())
				{
					return MakeUnique<FPCGAttributeAccessorKeysEntries>(Metadata);
				}
			}
		}

		return TUniquePtr<KeysType>();
	}

	TUniquePtr<IPCGAttributeAccessor> CreateChainAccessor(TUniquePtr<IPCGAttributeAccessor> InAccessor, FName Name, bool& bOutSuccess)
	{
		if (!InAccessor.IsValid())
		{
			bOutSuccess = false;
			return TUniquePtr<IPCGAttributeAccessor>();
		}

		auto Chain = [&Accessor = InAccessor, Name, &bOutSuccess](auto Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AccessorType = decltype(Dummy);

			if constexpr (PCG::Private::IsOfTypes<AccessorType, FVector2D, FVector, FVector4>())
			{
				return PCGAttributeExtractor::CreateVectorExtractor<AccessorType>(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FTransform>())
			{
				return PCGAttributeExtractor::CreateTransformExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FQuat>())
			{
				return PCGAttributeExtractor::CreateQuatExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else if constexpr (PCG::Private::IsOfTypes<AccessorType, FRotator>())
			{
				return PCGAttributeExtractor::CreateRotatorExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			else
			{
				bOutSuccess = false;
				return std::move(Accessor);
			}
		};

		return PCGMetadataAttribute::CallbackWithRightType(InAccessor->GetUnderlyingType(), Chain);
	}

	// Empty signature to passthrough types to functors.
	template <typename T>
	struct Signature
	{
		using Type = T;
	};

	template <typename Func>
	decltype(auto) DispatchPropertyTypes(const FProperty* InProperty, Func&& Functor)
	{
		// Use the FPCGPropertyPathAccessor (on soft object path) as dummy type to get the functor return type
		// because this accessor takes a generic FProperty, while others take a more specialized type (like FNumericProperty).
		using ReturnType = decltype(Functor(Signature<FPCGPropertyPathAccessor<FSoftObjectPath>>{}, InProperty));

		if (!InProperty)
		{
			return ReturnType{};
		}

		if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(InProperty))
		{
			if (NumericProperty->IsFloatingPoint())
			{
				return Functor(Signature<FPCGNumericPropertyAccessor<double>>{}, NumericProperty);
			}
			else if (NumericProperty->IsInteger())
			{
				return Functor(Signature<FPCGNumericPropertyAccessor<int64>>{}, NumericProperty);
			}
		}
		else if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<bool, FBoolProperty>>{}, BoolProperty);
		}
		else if (const FStrProperty* StringProperty = CastField<FStrProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<FString, FStrProperty>>{}, StringProperty);
		}
		else if (const FNameProperty* NameProperty = CastField<FNameProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyAccessor<FName, FNameProperty>>{}, NameProperty);
		}
		else if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
		{
			return Functor(Signature<FPCGEnumPropertyAccessor>{}, EnumProperty);
		}
		else if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertySoftClassPathAccessor>{}, SoftClassProperty);
		}
		else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertySoftObjectPathAccessor>{}, SoftObjectProperty);
		}
		else if (const FClassProperty* ClassProperty = CastField<FClassProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyObjectPtrAccessor<FClassProperty>>{}, ClassProperty);
		}
		else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
		{
			return Functor(Signature<FPCGPropertyObjectPtrAccessor<FObjectProperty>>{}, ObjectProperty);
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FVector>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FVector4>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector4>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FQuat>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FQuat>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FTransform>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FTransform>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FRotator>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FVector2D>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FSoftObjectPath>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				return Functor(Signature<FPCGPropertyStructAccessor<FSoftClassPath>>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return Functor(Signature<FPCGLinearColorAccessor>{}, StructProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FColor>::Get())
			{
				return Functor(Signature<FPCGColorAccessor>{}, StructProperty);
			}
			else if (StructProperty->Struct && StructProperty->Struct->IsChildOf(FPCGAttributePropertySelector::StaticStruct()))
			{
				return Functor(Signature<FPCGAttributePropertySelectorAccessor>{}, StructProperty);
			}
		}

		return ReturnType{};
	}

	template <typename Func>
	decltype(auto) DispatchPropertyTypes(const FName InPropertyName, const UStruct* InStruct, Func&& Functor)
	{
		if (InStruct)
		{
			if (const FProperty* Property = InStruct->FindPropertyByName(InPropertyName))
			{
				return DispatchPropertyTypes(Property, std::forward<Func>(Functor));
			}
		}

		using ReturnType = decltype(Functor(static_cast<FPCGPropertyPathAccessor<FSoftObjectPath>*>(nullptr), static_cast<const FProperty*>(nullptr)));
		return ReturnType{};
	}

	bool GetPropertyChain(const TArray<FName>& InPropertyNames, const UStruct* InStruct, TArray<const FProperty*>& OutProperties)
	{
		check(InStruct);

		const UStruct* CurrentStruct = InStruct;
		OutProperties.Reserve(InPropertyNames.Num());

		for (int32 i = 0; i < InPropertyNames.Num(); ++i)
		{
			const FProperty* Property = nullptr;
			const FName PropertyName = InPropertyNames[i];

			// Try to get the property. If it is coming from a user struct, we need to iterate on all properties because the property name is mangled
			if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentStruct))
			{
				for (TFieldIterator<const FProperty> PropIt(UserDefinedStruct, EFieldIterationFlags::IncludeSuper); PropIt; ++PropIt)
				{
					const FName TempPropertyName = *UserDefinedStruct->GetAuthoredNameForField(*PropIt);
					if (TempPropertyName == PropertyName)
					{
						Property = *PropIt;
						break;
					}
				}
			}
			else
			{
				Property = FindFProperty<FProperty>(CurrentStruct, PropertyName);
			}

			if (!Property)
			{
				UE_LOG(LogPCG, Error, TEXT("Property '%s' does not exist in %s."), *PropertyName.ToString(), *CurrentStruct->GetName());
				return false;
			}

			OutProperties.Add(Property);

			// Check for a struct or object for all properties except the last one.
			if (i < InPropertyNames.Num() - 1)
			{
				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					CurrentStruct = StructProperty->Struct;
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					CurrentStruct = ObjectProperty->PropertyClass;
				}
				else
				{
					UE_LOG(LogPCG, Error, TEXT("Property '%s' does exist in % s, but is not extractable."), *PropertyName.ToString(), *CurrentStruct->GetName());
					return false;
				}
			}
		}

		return true;
	}
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FProperty* InProperty)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperty, [](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty);
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FName InPropertyName, const UStruct* InStruct)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InStruct, [](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty);
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(TArray<const FProperty*>&& InProperties)
{
	if (InProperties.IsEmpty())
	{
		return TUniquePtr<IPCGAttributeAccessor>{};
	}

	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperties.Last(), [&InProperties](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
	{
		using TypedAccessor = typename decltype(SignatureDummy)::Type;
		return MakeUnique<TypedAccessor>(TypedProperty, std::forward<TArray<const FProperty*>>(InProperties));
	});
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(const TArray<FName>& InPropertyNames, const UStruct* InStruct)
{
	TArray<const FProperty*> PropertyChain;
	if (!PCGAttributeAccessorHelpers::GetPropertyChain(InPropertyNames, InStruct, PropertyChain))
	{
		return TUniquePtr<IPCGAttributeAccessor>{};
	}
	else
	{
		return PCGAttributeAccessorHelpers::CreatePropertyChainAccessor(std::move(PropertyChain));
	}
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FProperty* InProperty)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperty, [](auto, const auto*) -> bool
	{
		return true;
	});
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FName InPropertyName, const UStruct* InStruct)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InStruct, [](auto, const auto*) -> bool
	{
		return true;
	});
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorChainSupported(const TArray<FName>& InPropertyNames, const UStruct* InStruct)
{
	TArray<const FProperty*> PropertyChain;
	return PCGAttributeAccessorHelpers::GetPropertyChain(InPropertyNames, InStruct, PropertyChain) &&
		!PropertyChain.IsEmpty() &&
		PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(PropertyChain.Last());
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateExtraAccessor(EPCGExtraProperties InExtraProperties)
{
	switch (InExtraProperties)
	{
	case EPCGExtraProperties::Index:
		return MakeUnique<FPCGIndexAccessor>();
	default:
		return TUniquePtr<IPCGAttributeAccessor>();
	}
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParam(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, FName* OutAttributeName)
{
	if (OutAttributeName)
	{
		AccessorParamResult Result{};
		TUniquePtr<const IPCGAttributeAccessor> Accessor = CreateConstAccessorForOverrideParamWithResult(InInputData, InParam, &Result);
		*OutAttributeName = Result.AttributeName;
		return Accessor;
	}
	else
	{
		return CreateConstAccessorForOverrideParamWithResult(InInputData, InParam);
	}
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParamWithResult(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, AccessorParamResult* OutResult)
{
	bool bFromGlobalParamsPin = false;
	TArray<FPCGTaggedData> InputParamData = InInputData.GetParamsByPin(InParam.Label);
	if (InputParamData.IsEmpty())
	{
		// If it is empty, try with the Overrides pin (Global Params)
		bFromGlobalParamsPin = true;
		InputParamData = InInputData.GetParamsByPin(PCGPinConstants::DefaultParamsLabel);
	}

	if (OutResult)
	{
		OutResult->bHasMultipleAttributeSetsOnOverridePin = InputParamData.Num() > 1;
	}

	const UPCGParamData* ParamData = !InputParamData.IsEmpty() ? CastChecked<UPCGParamData>(InputParamData[0].Data) : nullptr;

	if (OutResult && ParamData && !bFromGlobalParamsPin)
	{
		OutResult->bPinConnected = true;
	}

	if (ParamData && ParamData->Metadata && ParamData->Metadata->GetAttributeCount() > 0)
	{
		// If the param only has a single attribute and is not from the global Params pin, use this one. Otherwise we need perfect name matching, either the property name or its full path if there is a name clash.
		const FName AttributeName = (ParamData->Metadata->GetAttributeCount() == 1 && !bFromGlobalParamsPin) 
			? ParamData->Metadata->GetLatestAttributeNameOrNone() 
			: (!InParam.bHasNameClash ? InParam.PropertiesNames.Last() : FName(InParam.GetPropertyPath()));

		if (OutResult)
		{
			OutResult->AttributeName = AttributeName;
		}

		FPCGAttributePropertyInputSelector InputSelector{};
		InputSelector.SetAttributeName(AttributeName);
		TUniquePtr<const IPCGAttributeAccessor> Result = PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, InputSelector);

		if (!Result)
		{
			// If we didn't find it, try some aliases.
			for (const FName& Alias : InParam.GenerateAllPossibleAliases())
			{
				InputSelector.SetAttributeName(Alias);
				Result = PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, InputSelector);
				if (Result)
				{
					if (OutResult)
					{
						OutResult->bUsedAliases = true;
						OutResult->AliasUsed = Alias;
					}

					break;
				}
			}
		}

		if (Result && OutResult && ParamData && ParamData->Metadata)
		{
			OutResult->bHasMultipleDataInAttributeSet = ParamData->Metadata->GetLocalItemCount() > 1;
		}

		return Result;
	}

	return TUniquePtr<const IPCGAttributeAccessor>{};
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector, bool bQuiet)
{
	return CreateAccessorImpl<const IPCGAttributeAccessor, const UPCGData>(InData, InSelector, bQuiet);
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const FPCGMetadataAttributeBase* InAttribute, const UPCGMetadata* InMetadata, bool bQuiet)
{
	return CreateAttributeAccessorImpl<const IPCGAttributeAccessor>(const_cast<FPCGMetadataAttributeBase*>(InAttribute), const_cast<UPCGMetadata*>(InMetadata), bQuiet);
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(UPCGData* InData, const FPCGAttributePropertySelector& InSelector, bool bQuiet)
{
	return CreateAccessorImpl<IPCGAttributeAccessor, UPCGData>(InData, InSelector, bQuiet);
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(FPCGMetadataAttributeBase* InAttribute, UPCGMetadata* InMetadata, bool bQuiet)
{
	return CreateAttributeAccessorImpl<IPCGAttributeAccessor>(InAttribute, InMetadata, bQuiet);
}

TUniquePtr<const IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateConstKeys(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	return CreateKeysImpl<const IPCGAttributeAccessorKeys, const UPCGData>(InData, InSelector);
}

TUniquePtr<IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateKeys(UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	return CreateKeysImpl<IPCGAttributeAccessorKeys, UPCGData>(InData, InSelector);
}
