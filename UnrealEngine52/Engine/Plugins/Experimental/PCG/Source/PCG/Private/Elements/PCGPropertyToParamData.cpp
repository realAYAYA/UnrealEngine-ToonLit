// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPropertyToParamData.h"

#include "GameFramework/Actor.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPropertyToParamData)

#define LOCTEXT_NAMESPACE "PCGPropertyToParamDataElement"

#if WITH_EDITOR
void UPCGPropertyToParamDataSettings::GetTrackedActorTags(FPCGTagToSettingsMap& OutTagToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (ActorSelector.ActorSelection == EPCGActorSelection::ByTag &&
		ActorSelector.ActorFilter == EPCGActorFilter::AllWorldActors)
	{
		OutTagToSettings.FindOrAdd(ActorSelector.ActorSelectionTag).Emplace({ this, bTrackActorsOnlyWithinBounds });
	}

}
#endif // WITH_EDITOR

void UPCGPropertyToParamDataSettings::PostLoad()
{
	Super::PostLoad();

	// Migrate deprecated actor selection settings to struct if needed
	if (ActorSelection_DEPRECATED != EPCGActorSelection::ByTag ||
		ActorSelectionTag_DEPRECATED != NAME_None ||
		ActorSelectionName_DEPRECATED != NAME_None ||
		ActorSelectionClass_DEPRECATED != TSubclassOf<AActor>{} ||
		ActorFilter_DEPRECATED != EPCGActorFilter::Self ||
		bIncludeChildren_DEPRECATED != false)
	{
		ActorSelector.ActorSelection = ActorSelection_DEPRECATED;
		ActorSelector.ActorSelectionTag = ActorSelectionTag_DEPRECATED;
		ActorSelector.ActorSelectionClass = ActorSelectionClass_DEPRECATED;
		ActorSelector.ActorFilter = ActorFilter_DEPRECATED;
		ActorSelector.bIncludeChildren = bIncludeChildren_DEPRECATED;

		ActorSelection_DEPRECATED = EPCGActorSelection::ByTag;
		ActorSelectionTag_DEPRECATED = NAME_None;
		ActorSelectionName_DEPRECATED = NAME_None;
		ActorSelectionClass_DEPRECATED = TSubclassOf<AActor>{};
		ActorFilter_DEPRECATED = EPCGActorFilter::Self;
		bIncludeChildren_DEPRECATED = false;
	}
}

TArray<FPCGPinProperties> UPCGPropertyToParamDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGElementPtr UPCGPropertyToParamDataSettings::CreateElement() const
{
	return MakeShared<FPCGPropertyToParamDataElement>();
}

bool FPCGPropertyToParamDataElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPropertyToParamDataElement::Execute);

	check(Context);

	const UPCGPropertyToParamDataSettings* Settings = Context->GetInputSettings<UPCGPropertyToParamDataSettings>();
	check(Settings);

	// Early out if arguments are not specified
	if (Settings->PropertyName == NAME_None || (Settings->bSelectComponent && !Settings->ComponentClass))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("ParametersMissing", "Some parameters are missing, aborting"));
		return true;
	}

#if !WITH_EDITOR
	// If we have no output connected, nothing to do
	// Optimization possibly only in non-editor builds, otherwise we could poison the input-driven cache
	if (!Context->Node || !Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel))
	{
		PCGE_LOG(Verbose, LogOnly, LOCTEXT("UnconnectedNode", "Node is not connected, nothing to do"));
		return true;
	}
#endif

	// First find the actor depending on the selection
	UPCGComponent* OriginalComponent = UPCGBlueprintHelpers::GetOriginalComponent(*Context);
	auto NoBoundsCheck = [](const AActor*) -> bool { return true; };
	AActor* FoundActor = PCGActorSelector::FindActor(Settings->ActorSelector, OriginalComponent, NoBoundsCheck);

	if (!FoundActor)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoActorFound", "No matching actor was found"));
		return true;
	}

	// From there, we either check the actor, or the component attached to it.
	UObject* ObjectToInspect = FoundActor;
	if (Settings->bSelectComponent)
	{
		ObjectToInspect = FoundActor->GetComponentByClass(Settings->ComponentClass);
		if (!ObjectToInspect)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ComponentDoesNotExist", "Component does not exist in the found actor"));
			return true;
		}
	}

	// Try to get the property
	FProperty* Property = FindFProperty<FProperty>(ObjectToInspect->GetClass(), Settings->PropertyName);
	if (!Property)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("PropertyDoesNotExist", "Property does not exist in the found actor"));
		return true;
	}

	using ExtractablePropertyTuple = TTuple<FName, const void*, const FProperty*>;
	TArray<ExtractablePropertyTuple> ExtractableProperties;

	// Special case where the property is a struct/object, that is not supported by our metadata, we will try to break it down to multiple attributes in the resulting param data, if asked.
	if (!PCGAttributeAccessorHelpers::IsPropertyAccessorSupported(Property) && (Property->IsA<FStructProperty>() || Property->IsA<FObjectProperty>()) && Settings->bExtractObjectAndStruct)
	{
		UScriptStruct* UnderlyingStruct = nullptr;
		UClass* UnderlyingClass = nullptr;
		const void* ObjectAddress = nullptr;

		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			UnderlyingStruct = StructProperty->Struct;
			ObjectAddress = StructProperty->ContainerPtrToValuePtr<void>(ObjectToInspect);
		}
		else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			UnderlyingClass = ObjectProperty->PropertyClass;
			ObjectAddress = ObjectProperty->GetObjectPropertyValue_InContainer(ObjectToInspect);
		}
		
		check(UnderlyingStruct || UnderlyingClass);
		check(ObjectAddress);

		// Re-use code from overridable params
		// Limit ourselves to not recurse into more structs.
		PCGSettingsHelpers::FPCGGetAllOverridableParamsConfig Config;
		Config.bUseSeed = true;
		Config.bExcludeSuperProperties = true;
		Config.MaxStructDepth = 0;
		TArray<FPCGSettingsOverridableParam> AllChildProperties = UnderlyingStruct ? PCGSettingsHelpers::GetAllOverridableParams(UnderlyingStruct, Config) : PCGSettingsHelpers::GetAllOverridableParams(UnderlyingClass, Config);

		for (const FPCGSettingsOverridableParam& Param : AllChildProperties)
		{
			if (ensure(!Param.PropertiesNames.IsEmpty()))
			{
				const FName ChildPropertyName = Param.PropertiesNames[0];
				if (const FProperty* ChildProperty = (UnderlyingStruct ? UnderlyingStruct->FindPropertyByName(ChildPropertyName) : UnderlyingClass->FindPropertyByName(ChildPropertyName)))
				{
					// We use authored name as attribute name to avoid issue with noisy property names, like in UUserDefinedStructs, where some random number is appended to the property name.
					// By default, it will just return the property name anyway.
					const FString AuthoredName = UnderlyingStruct ? UnderlyingStruct->GetAuthoredNameForField(ChildProperty) : UnderlyingClass->GetAuthoredNameForField(ChildProperty);
					ExtractableProperties.Emplace(FName(AuthoredName), ObjectAddress, ChildProperty);
				}
			}
		}
	}
	else
	{
		ExtractableProperties.Emplace(Settings->OutputAttributeName, ObjectToInspect, Property);
	}

	if (ExtractableProperties.IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPropertiesFound", "No properties found to extract"));
		return true;
	}

	// From there, we should be able to create the data.
	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = ParamData->MutableMetadata();
	check(Metadata);
	PCGMetadataEntryKey EntryKey = Metadata->AddEntry();
	bool bValidOperation = false;

	for (ExtractablePropertyTuple& ExtractableProperty : ExtractableProperties)
	{
		const FName AttributeName = ExtractableProperty.Get<0>();
		const void* ContainerPtr = ExtractableProperty.Get<1>();
		const FProperty* FinalProperty = ExtractableProperty.Get<2>();

		if (!Metadata->SetAttributeFromDataProperty(AttributeName, EntryKey, ContainerPtr, FinalProperty, /*bCreate=*/ true))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating an attribute for property '{0}'. Either the property type is not supported by PCG or attribute creation failed."), FText::FromString(FinalProperty->GetName())));
			continue;
		}

		bValidOperation = true;
	}

	if (bValidOperation)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = ParamData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
