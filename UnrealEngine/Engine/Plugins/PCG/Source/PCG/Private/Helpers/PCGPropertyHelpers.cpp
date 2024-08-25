// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGPropertyHelpers.h"

#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGModule.h"
#include "PCGParamData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"

#include "Engine/UserDefinedStruct.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "PCGPropertyHelpers"

namespace PCGPropertyHelpers
{
	static constexpr uint64 ExcludePropertyFlags = CPF_DisableEditOnInstance;
	static constexpr uint64 IncludePropertyFlags = CPF_BlueprintVisible;

	void LogError(const FText& ErrorMessage, FPCGContext* InOptionalContext)
	{
		if (InOptionalContext)
		{
			PCGE_LOG_C(Error, GraphAndLog, InOptionalContext, ErrorMessage);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("%s"), *ErrorMessage.ToString());
		}
	}

	/**
	* Expands container locations to their contents when the property passed in is an array.
	* This is useful to allow extraction downstream of properties inside of arrays and also to generate the list of addresses/values to look at
	* when extracting the values to the attribute set.
	* 
	* @param InArrayProperty   Property that drives the container expansion. Can be null, in which case we'll copy the container locations directly.
	* @param InContainers      Container locations to expand
	* @param OutContainers     Expanded container locations. Expected to be a different array than InContainers.
	*/
	template<typename FirstArrayType, typename SecondArrayType>
	void ExpandContainers(const FArrayProperty* InArrayProperty, const FirstArrayType& InContainers, SecondArrayType& OutContainers)
	{
		check(OutContainers.IsEmpty());

		if (InArrayProperty)
		{
			for (const void* Container : InContainers)
			{
				FScriptArrayHelper_InContainer Helper(InArrayProperty, Container);
				int32 Offset = OutContainers.Num();
				OutContainers.SetNumUninitialized(OutContainers.Num() + Helper.Num());
				for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
				{
					OutContainers[Offset + DynamicIndex] = Helper.GetRawPtr(DynamicIndex);
				}
			}
		}
		else
		{
			OutContainers = InContainers;
		}
	}

	/**
	* Recursive function to go down the property chain to find the property and its container addresses.
	* @param CurrentClass            Struct/Class for the current container
	* @param CurrentName             Property name to look for in the container class.
	* @param NextNames               List of property names to continue extracting at a deeper level.
	* @param bNeedsToBeVisible       Discard properties that are not visibile in Blueprint
	* @param OutContainers           Raw addresses for the containers. Will be written to at each recursive call.
	* @param OptionalContext         Optional context used for logging.
	* @param OptionalObjectTraversed Optional set to store all object that we traversed, to be able to react to those objects changes.
	* @returns                 The last property of the chain (and its container address is in OutContainer)
	*/
	const FProperty* ExtractPropertyChain(const UStruct* CurrentClass, const FName CurrentName, TArrayView<const FString> NextNames, const bool bNeedsToBeVisible, TArray<const void*>& OutContainers, FPCGContext* OptionalContext, TSet<FSoftObjectPath>* OptionalObjectTraversed)
	{
		check(CurrentClass);

		const FProperty* Property = nullptr;
		// Try to get the property. If it is coming from a user struct, we need to iterate on all properties because the property name is mangled
		if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(CurrentClass))
		{
			for (TFieldIterator<const FProperty> PropIt(UserDefinedStruct, EFieldIterationFlags::IncludeSuper); PropIt; ++PropIt)
			{
				const FName PropertyName = *UserDefinedStruct->GetAuthoredNameForField(*PropIt);
				if (PropertyName == CurrentName)
				{
					Property = *PropIt;
					break;
				}
			}
		}
		else
		{
			Property = FindFProperty<FProperty>(CurrentClass, CurrentName);
		}

		if (!Property)
		{
			LogError(FText::Format(LOCTEXT("PropertyDoesNotExist", "Property '{0}' does not exist in {1}."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
			return nullptr;
		}

		// Make sure the property is visible, if requested
		if (bNeedsToBeVisible && (Property->HasAnyPropertyFlags(ExcludePropertyFlags) || !Property->HasAnyPropertyFlags(IncludePropertyFlags)))
		{
			LogError(FText::Format(LOCTEXT("PropertyExistsButNotVisible", "Property '{0}' does exist in {1}, but is not visible."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
			return nullptr;
		}

		if (!NextNames.IsEmpty())
		{
			UStruct* NextClass = nullptr;
			bool bPropertyNotExtractable = false;

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				NextClass = StructProperty->Struct;
				for (const void*& OutContainer : OutContainers)
				{
					OutContainer = StructProperty->ContainerPtrToValuePtr<void>(OutContainer);
				}
			}
			else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				NextClass = ObjectProperty->PropertyClass;
				for (const void*& OutContainer : OutContainers)
				{
					OutContainer = ObjectProperty->GetObjectPropertyValue_InContainer(OutContainer);
				}
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				const FProperty* InnerProperty = ArrayProperty->Inner;
				if (const FStructProperty* InnerStructProperty = CastField<FStructProperty>(InnerProperty))
				{
					NextClass = InnerStructProperty->Struct;
				}
				else if (const FObjectProperty* InnerObjectProperty = CastField<FObjectProperty>(InnerProperty))
				{
					NextClass = InnerObjectProperty->PropertyClass;
				}
				else
				{
					bPropertyNotExtractable = true;
				}

				// If contents of the array are extractable, do so now by replacing the container entry (e.g. the array) with the pointer to its contents
				if (!bPropertyNotExtractable)
				{
					TArray<const void*> Subcontainers;
					ExpandContainers(ArrayProperty, OutContainers, Subcontainers);
					OutContainers = MoveTemp(Subcontainers);
				}
			}
			else
			{
				bPropertyNotExtractable = true;
			}
			
			if(bPropertyNotExtractable)
			{
				LogError(FText::Format(LOCTEXT("PropertyIsNotExtractable", "Property '{0}' does exist in {1}, but is not extractable."), FText::FromName(CurrentName), FText::FromName(CurrentClass->GetFName())), OptionalContext);
				return nullptr;
			}

			return ExtractPropertyChain(NextClass, FName(NextNames[0]), NextNames.RightChop(1), bNeedsToBeVisible, OutContainers, OptionalContext, OptionalObjectTraversed);
		}
		else
		{
			return Property;
		}
	}
}

EPCGMetadataTypes PCGPropertyHelpers::GetMetadataTypeFromProperty(const FProperty* InProperty)
{
	if (!InProperty)
	{
		return EPCGMetadataTypes::Unknown;
	}

	// Object are not yet supported as accessors
	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(InProperty))
	{
		return EPCGMetadataTypes::String;
	}

	TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(InProperty);

	return PropertyAccessor.IsValid() ? EPCGMetadataTypes(PropertyAccessor->GetUnderlyingType()) : EPCGMetadataTypes::Unknown;
}

UPCGParamData* PCGPropertyHelpers::ExtractPropertyAsAttributeSet(const PCGPropertyHelpers::FExtractorParameters& Parameters, FPCGContext* OptionalContext, TSet<FSoftObjectPath>* OptionalObjectTraversed)
{
	check(Parameters.Container && Parameters.Class);

	TArray<const void*> Containers = { Parameters.Container };
	const FProperty* Property = nullptr;
	const FName PropertyName = Parameters.PropertySelector.GetName();
	const bool ExtractRoot = (PropertyName == NAME_None);
	// If Name is none, extract the container as-is, using Parameters.Class, otherwise, extract the chain.
	if (!ExtractRoot)
	{
		Property = ExtractPropertyChain(Parameters.Class, PropertyName, Parameters.PropertySelector.GetExtraNames(), Parameters.bPropertyNeedsToBeVisible, Containers, OptionalContext, OptionalObjectTraversed);
		if (!Property)
		{
			return nullptr;
		}
	}

	// If the property is an array, we will work on the underlying property, and extract each element as an entry in the param data
	const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property);
	if (ArrayProperty)
	{
		Property = ArrayProperty->Inner;
	}

	using ExtractablePropertyTuple = TTuple<FName, const FProperty*>;
	TArray<ExtractablePropertyTuple> ExtractableProperties;

	using GetAddressFunc = TFunction<const void* (const void*)>;
	GetAddressFunc AddressFunc;

	// Force extraction if the property is not supported by accessors.
	const bool bShouldExtract = Parameters.bShouldExtract || !PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(Property);

	// Keep track if the extracted property is an object or not
	const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);

	// Special case where the property is a struct/object, that is not supported by our metadata, we will try to break it down to multiple attributes in the resulting param data, if asked.
	if (ExtractRoot || ((Property->IsA<FStructProperty>() || Property->IsA<FObjectProperty>()) && bShouldExtract))
	{
		const UStruct* UnderlyingClass = nullptr;

		if (ExtractRoot)
		{
			UnderlyingClass = Parameters.Class;
			// Identity
			AddressFunc = [](const void* InAddress) { return InAddress; };
		}
		else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UnderlyingClass = StructProperty->Struct;
			AddressFunc = [StructProperty](const void* InAddress) { return StructProperty->ContainerPtrToValuePtr<void>(InAddress); };
		}
		else if (ObjectProperty)
		{
			UnderlyingClass = ObjectProperty->PropertyClass;
			AddressFunc = [ObjectProperty](const void* InAddress) { return ObjectProperty->GetObjectPropertyValue_InContainer(InAddress); };
		}

		check(UnderlyingClass);
		check(!!AddressFunc);

		// Re-use code from overridable params
		// Limit ourselves to not recurse into more structs.
		PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
		Config.bUseSeed = true;
		Config.bExcludeSuperProperties = true;
		Config.MaxStructDepth = 0;
		// Can only get exposed properties and visible if requested
		if (Parameters.bPropertyNeedsToBeVisible)
		{
			Config.ExcludePropertyFlags = ExcludePropertyFlags;
			Config.IncludePropertyFlags = IncludePropertyFlags;
		}
		TArray<FPCGSettingsOverridableParam> AllChildProperties = PCGSettingsHelpers::GetAllOverridableParams(UnderlyingClass, Config);

		for (const FPCGSettingsOverridableParam& Param : AllChildProperties)
		{
			if (ensure(!Param.PropertiesNames.IsEmpty()))
			{
				const FName ChildPropertyName = Param.PropertiesNames[0];
				if (const FProperty* ChildProperty = UnderlyingClass->FindPropertyByName(ChildPropertyName))
				{
					// We use authored name as attribute name to avoid issue with noisy property names, like in UUserDefinedStructs, where some random number is appended to the property name.
					// By default, it will just return the property name anyway.
					const FString AuthoredName = UnderlyingClass->GetAuthoredNameForField(ChildProperty);
					ExtractableProperties.Emplace(FName(AuthoredName), ChildProperty);
				}
			}
		}
	}
	else
	{
		// For non struct/object, there is just a single property to extract with no shenanigans for address indirection.
		const FName AttributeName = (Parameters.OutputAttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName) ? Property->GetFName() : Parameters.OutputAttributeName;
		ExtractableProperties.Emplace(AttributeName, Property);
		// Identity
		AddressFunc = [](const void* InAddress) { return InAddress; };
	}

	if (ExtractableProperties.IsEmpty())
	{
		LogError(LOCTEXT("NoPropertiesFound", "No properties found to extract"), OptionalContext);
		return nullptr;
	}

	// Before we need to compute all the addresses for each entry in our array (or just a single entry if there is no array)
	TArray<const void*, TInlineAllocator<16>> ElementAddresses;
	ExpandContainers(ArrayProperty, Containers, ElementAddresses);

	// From there, we should be able to create the data.
	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = ParamData->MutableMetadata();
	check(Metadata);

	bool bValidOperation = true;

	for (const void* ElementAddress : ElementAddresses)
	{
		if (!bValidOperation)
		{
			break;
		}

		// Add a new entry for all elements
		PCGMetadataEntryKey EntryKey = Metadata->AddEntry();

		// Offset the address if needed
		const void* ContainerPtr = AddressFunc(ElementAddress);

		for (ExtractablePropertyTuple& ExtractableProperty : ExtractableProperties)
		{
			const FName AttributeName = ExtractableProperty.Get<0>();
			const FProperty* FinalProperty = ExtractableProperty.Get<1>();

			if (!Metadata->SetAttributeFromDataProperty(AttributeName, EntryKey, ContainerPtr, FinalProperty, /*bCreate=*/ true))
			{
				LogError(FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating an attribute for property '{0}'. Either the property type is not supported by PCG or attribute creation failed."), FText::FromString(FinalProperty->GetName())), OptionalContext);
				bValidOperation = false;
				break;
			}
		}

		if (bValidOperation && bShouldExtract && OptionalObjectTraversed && ObjectProperty)
		{
			const UObject* Object = ObjectProperty->GetPropertyValue_InContainer(ElementAddress);
			if (IsValid(Object))
			{
				OptionalObjectTraversed->Add(Object);
			}
		}
	}

	return bValidOperation ? ParamData : nullptr;
}

#undef LOCTEXT_NAMESPACE