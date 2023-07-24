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
#include "Metadata/Accessors/PCGPropertyAccessor.h"
#include "Metadata/Accessors/PCGAttributeExtractor.h"

#include "UObject/EnumProperty.h"

namespace PCGAttributeAccessorHelpers
{
	void ExtractMetadataAtribute(UPCGData* InData, FName Name, UPCGMetadata*& OutMetadata, FPCGMetadataAttributeBase*& OutAttribute)
	{
		OutMetadata = nullptr;
		OutAttribute = nullptr;

		if (UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(InData))
		{
			OutMetadata = SpatialData->Metadata;
		}
		else if (UPCGParamData* ParamData = Cast<UPCGParamData>(InData))
		{
			OutMetadata = ParamData->Metadata;
		}

		if (OutMetadata)
		{
			// If Name is None, try ot get the latest attribute
			if (Name == NAME_None)
			{
				Name = OutMetadata->GetLatestAttributeNameOrNone();
			}

			OutAttribute = OutMetadata->GetMutableAttribute(Name);
		}
	}

	void ExtractMetadataAtribute(const UPCGData* InData, FName Name, UPCGMetadata const*& OutMetadata, FPCGMetadataAttributeBase const*& OutAttribute)
	{
		UPCGMetadata* Metadata = nullptr;
		FPCGMetadataAttributeBase* Attribute = nullptr;
		ExtractMetadataAtribute(const_cast<UPCGData*>(InData), Name, Metadata, Attribute);
		OutMetadata = Metadata;
		OutAttribute = Attribute;
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
			if constexpr (PCG::Private::IsOfTypes<AccessorType, FTransform>())
			{
				return PCGAttributeExtractor::CreateTransformExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			if constexpr (PCG::Private::IsOfTypes<AccessorType, FQuat>())
			{
				return PCGAttributeExtractor::CreateQuatExtractor(std::move(Accessor), Name, bOutSuccess);
			}
			if constexpr (PCG::Private::IsOfTypes<AccessorType, FRotator>())
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
				return Functor(Signature<FPCGPropertyPathAccessor<FSoftObjectPath>>{}, InProperty);
			}
			else if (StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
			{
				return Functor(Signature<FPCGPropertyPathAccessor<FSoftClassPath>>{}, InProperty);
			}
		}

		return ReturnType{};
	}

	template <typename ClassType, typename Func>
	decltype(auto) DispatchPropertyTypes(const FName InPropertyName, const ClassType* InClass, Func&& Functor)
	{
		if (InClass)
		{
			if (const FProperty* Property = InClass->FindPropertyByName(InPropertyName))
			{
				return DispatchPropertyTypes(Property, std::forward<Func>(Functor));
			}
		}

		using ReturnType = decltype(Functor(static_cast<FPCGPropertyPathAccessor<FSoftObjectPath>*>(nullptr), static_cast<const FProperty*>(nullptr)));
		return ReturnType{};
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

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreatePropertyAccessor(const FName InPropertyName, const UClass* InClass)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InClass, [](auto SignatureDummy, const auto* TypedProperty) -> TUniquePtr<IPCGAttributeAccessor>
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

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FProperty* InProperty)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InProperty, [](auto, const auto*) -> bool
	{
		return true;
	});
}

bool PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(const FName InPropertyName, const UClass* InClass)
{
	return PCGAttributeAccessorHelpers::DispatchPropertyTypes(InPropertyName, InClass, [](auto, const auto*) -> bool
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

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessorForOverrideParam(const FPCGDataCollection& InInputData, const FPCGSettingsOverridableParam& InParam, FName* OutAttributeName)
{
	bool bFromGlobalParamsPin = false;
	TArray<FPCGTaggedData> InputParamData = InInputData.GetParamsByPin(InParam.Label);
	UPCGParamData* ParamData = !InputParamData.IsEmpty() ? CastChecked<UPCGParamData>(InputParamData[0].Data) : nullptr;

	// If it is empty, try with the param pin
	if (!ParamData)
	{
		bFromGlobalParamsPin = true;
		ParamData = InInputData.GetFirstParamsOnParamsPin();
	}

	if (ParamData && ParamData->Metadata && ParamData->Metadata->GetAttributeCount() > 0)
	{
		// If the param only has a single attribute and is not from the global Params pin, use this one. Otherwise we need perfect name matching.
		const FName AttributeName = (ParamData->Metadata->GetAttributeCount() == 1 && !bFromGlobalParamsPin) ? ParamData->Metadata->GetLatestAttributeNameOrNone() : InParam.Properties.Last()->GetFName();

		if (OutAttributeName)
		{
			*OutAttributeName = AttributeName;
		}

		FPCGAttributePropertySelector InputSelector{};
		InputSelector.SetAttributeName(AttributeName);
		return PCGAttributeAccessorHelpers::CreateConstAccessor(ParamData, InputSelector);
	}

	return TUniquePtr<const IPCGAttributeAccessor>{};
}

TUniquePtr<const IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateConstAccessor(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	const FName Name = InSelector.GetName();
	TUniquePtr<IPCGAttributeAccessor> Accessor;

	if (InSelector.Selection == EPCGAttributePropertySelection::PointProperty)
	{
		if(!InData || Cast<const UPCGPointData>(InData))
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

	// At this point, it is not a point data or we didn't find a property.
	// We can't continue if it is a property wanted.
	if (InSelector.Selection == EPCGAttributePropertySelection::PointProperty && !Accessor.IsValid())
	{
		UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateConstAccessor] Expected to select a property but the data doesn't support this property."));
		return TUniquePtr<const IPCGAttributeAccessor>();
	}

	if (InSelector.Selection == EPCGAttributePropertySelection::Attribute)
	{
		const UPCGMetadata* Metadata = nullptr;
		const FPCGMetadataAttributeBase* Attribute = nullptr;

		ExtractMetadataAtribute(InData, Name, Metadata, Attribute);

		auto CreateTypedAccessor = [Attribute, Metadata](auto Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AttributeType = decltype(Dummy);
			return MakeUnique<FPCGAttributeAccessor<AttributeType>>(static_cast<const FPCGMetadataAttribute<AttributeType>*>(Attribute), Metadata);
		};

		if (Attribute && Metadata)
		{
			Accessor = PCGMetadataAttribute::CallbackWithRightType(Attribute->GetTypeId(), CreateTypedAccessor);
		}
		else
		{
			return TUniquePtr<const IPCGAttributeAccessor>();
		}
	}

	if (!Accessor.IsValid())
	{
		return TUniquePtr<const IPCGAttributeAccessor>();
	}

	// At this point, check if we have chain accessors
	for (const FString& ExtraName : InSelector.ExtraNames)
	{
		bool bSuccess = false;
		Accessor = CreateChainAccessor(std::move(Accessor), FName(ExtraName), bSuccess);
		if (!bSuccess)
		{
			UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateConstAccessor] Extra selectors don't match existing properties."));
			return TUniquePtr<const IPCGAttributeAccessor>();
		}
	}

	return Accessor;
}

TUniquePtr<IPCGAttributeAccessor> PCGAttributeAccessorHelpers::CreateAccessor(UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	const FName Name = InSelector.GetName();
	TUniquePtr<IPCGAttributeAccessor> Accessor;

	if (InSelector.Selection == EPCGAttributePropertySelection::PointProperty)
	{
		if (!InData || Cast<const UPCGPointData>(InData))
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

	// At this point, it is not a point data or we didn't find a property.
	// We can't continue if it is a property wanted.
	if (InSelector.Selection == EPCGAttributePropertySelection::PointProperty && !Accessor.IsValid())
	{
		UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Expected to select a property but the data doesn't support this property."));
		return TUniquePtr<IPCGAttributeAccessor>();
	}

	if (InSelector.Selection == EPCGAttributePropertySelection::Attribute)
	{
		UPCGMetadata* Metadata = nullptr;
		FPCGMetadataAttributeBase* Attribute = nullptr;

		ExtractMetadataAtribute(InData, Name, Metadata, Attribute);

		auto CreateTypedAccessor = [Attribute, Metadata](auto Dummy) -> TUniquePtr<IPCGAttributeAccessor>
		{
			using AttributeType = decltype(Dummy);
			return MakeUnique<FPCGAttributeAccessor<AttributeType>>(static_cast<FPCGMetadataAttribute<AttributeType>*>(Attribute), Metadata);
		};

		if (Attribute && Metadata)
		{
			Accessor = PCGMetadataAttribute::CallbackWithRightType(Attribute->GetTypeId(), CreateTypedAccessor);
		}
		else
		{
			return TUniquePtr<IPCGAttributeAccessor>();
		}
	}

	if (!Accessor.IsValid())
	{
		return TUniquePtr<IPCGAttributeAccessor>();
	}

	// At this point, check if we have chain accessors
	for (const FString& ExtraName : InSelector.ExtraNames)
	{
		bool bSuccess = false;
		Accessor = CreateChainAccessor(std::move(Accessor), FName(ExtraName), bSuccess);
		if (!bSuccess)
		{
			UE_LOG(LogPCG, Error, TEXT("[PCGAttributeAccessorHelpers::CreateAccessor] Extra selectors don't match existing properties."));
			return TUniquePtr<IPCGAttributeAccessor>();
		}
	}

	return Accessor;
}

TUniquePtr<const IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateConstKeys(const UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	if (const UPCGPointData* PointData = Cast<const UPCGPointData>(InData))
	{
		return MakeUnique<FPCGAttributeAccessorKeysPoints>(PointData->GetPoints());
	}

	const UPCGMetadata* Metadata = nullptr;
	const FPCGMetadataAttributeBase* Attribute = nullptr;

	ExtractMetadataAtribute(InData, InSelector.GetName(), Metadata, Attribute);

	if (Attribute)
	{
		return MakeUnique<FPCGAttributeAccessorKeysEntries>(Attribute);
	}
	else
	{
		return TUniquePtr<IPCGAttributeAccessorKeys>();
	}
}

TUniquePtr<IPCGAttributeAccessorKeys> PCGAttributeAccessorHelpers::CreateKeys(UPCGData* InData, const FPCGAttributePropertySelector& InSelector)
{
	if (UPCGPointData* PointData = Cast<UPCGPointData>(InData))
	{
		TArrayView<FPCGPoint> View(PointData->GetMutablePoints());
		return MakeUnique<FPCGAttributeAccessorKeysPoints>(View);
	}

	UPCGMetadata* Metadata = nullptr;
	FPCGMetadataAttributeBase* Attribute = nullptr;

	ExtractMetadataAtribute(InData, InSelector.GetName(), Metadata, Attribute);

	if (Attribute)
	{
		return MakeUnique<FPCGAttributeAccessorKeysEntries>(Attribute);
	}
	else
	{
		return TUniquePtr<IPCGAttributeAccessorKeys>();
	}
}
