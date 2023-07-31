// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPropertyToParamData.h"

#include "PCGCommon.h"
#include "PCGContext.h"
#include "PCGComponent.h"
#include "PCGParamData.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGActorHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttributeTraits.h"

#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace PCGPropertyToParamDataHelpers
{
	// Need to pass a pointer of pointer to the found actor. The lambda will capture this pointer and modify its value when an actor is found.
	TFunction<bool(AActor*)> GetFilteringFunction(const UPCGPropertyToParamDataSettings* InSettings, AActor*& InFoundActor)
	{
		check(InSettings);

		switch (InSettings->ActorSelection)
		{
		case EPCGActorSelection::ByTag:
			return [ActorSelectionTag = InSettings->ActorSelectionTag, &InFoundActor](AActor* Actor) -> bool
				{
					if (Actor->ActorHasTag(ActorSelectionTag))
					{
						InFoundActor = Actor;
						return false;
					}

					return true;
				};

		case EPCGActorSelection::ByName:
			return [ActorSelectionName = InSettings->ActorSelectionName, &InFoundActor](AActor* Actor) -> bool
				{
					if (Actor->GetFName().IsEqual(ActorSelectionName, ENameCase::IgnoreCase, /*bCompareNumber=*/ false))
					{
						InFoundActor = Actor;
						return false;
					}

					return true;
				};

		case EPCGActorSelection::ByClass:
			return [ActorSelectionClass = InSettings->ActorSelectionClass, &InFoundActor](AActor* Actor) -> bool
				{
					if (Actor->IsA(ActorSelectionClass))
					{
						InFoundActor = Actor;
						return false;
					}

					return true;
				};

		default:
			break;
		}

		return [](AActor* Actor) -> bool { return false; };
	}

	AActor* FindActor(FPCGContext& InContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGPropertyToParamDataHelpers::FindActor);

		AActor* FoundActor = nullptr;

		if (!InContext.SourceComponent.IsValid())
		{
			return FoundActor;
		}

		UPCGComponent* OriginalComponent = UPCGBlueprintHelpers::GetOriginalComponent(InContext);
		check(OriginalComponent);

		UWorld* World = OriginalComponent->GetWorld();
		if (!World)
		{
			return FoundActor;
		}

		const UPCGPropertyToParamDataSettings* Settings = InContext.GetInputSettings<UPCGPropertyToParamDataSettings>();
		check(Settings);

		// Early out if we have not the information necessary
		if ((Settings->ActorSelection == EPCGActorSelection::ByTag && Settings->ActorSelectionTag == NAME_None) ||
			(Settings->ActorSelection == EPCGActorSelection::ByName && Settings->ActorSelectionName == NAME_None) ||
			(Settings->ActorSelection == EPCGActorSelection::ByClass && !Settings->ActorSelectionClass))
		{
			return FoundActor;
		}

		// We pass FoundActor ref, that will be captured by the FilteringFunction
		// It will modify the FoundActor pointer to the found actor, if found.
		TFunction<bool(AActor*)> FilteringFunction = PCGPropertyToParamDataHelpers::GetFilteringFunction(Settings, FoundActor);

		// In case of iterating over all actors in the world, call our filtering function and get out.
		if (Settings->ActorFilter == EPCGActorFilter::AllWorldActors)
		{
			UPCGActorHelpers::ForEachActorInWorld<AActor>(World, FilteringFunction);

			// FoundActor is set by the FilteringFunction (captured)
			return FoundActor;
		}

		// Otherwise, gather all the actors we need to check
		TArray<AActor*> ActorsToCheck;
		switch (Settings->ActorFilter)
		{
		case EPCGActorFilter::Self:
			if (AActor* Owner = OriginalComponent->GetOwner())
			{
				ActorsToCheck.Add(Owner);
			}
			break;

		case EPCGActorFilter::Parent:
			if (AActor* Owner = OriginalComponent->GetOwner())
			{
				if (AActor* Parent = Owner->GetParentActor())
				{
					ActorsToCheck.Add(Parent);
				}
				else
				{
					// If there is no parent, set the owner as the parent.
					ActorsToCheck.Add(Owner);
				}
			}
			break;

		case EPCGActorFilter::Root:
		{
			AActor* Current = OriginalComponent->GetOwner();
			while (Current != nullptr)
			{
				AActor* Parent = Current->GetParentActor();
				if (Parent == nullptr)
				{
					ActorsToCheck.Add(Current);
					break;
				}
				Current = Parent;
			}

			break;
		}

		//case EPCGActorFilter::TrackedActors:
			//	//TODO
			//	break;

		default:
			break;
		}

		if (Settings->bIncludeChildren)
		{
			int32 InitialCount = ActorsToCheck.Num();
			for (int32 i = 0; i < InitialCount; ++i)
			{
				ActorsToCheck[i]->GetAttachedActors(ActorsToCheck, /*bResetArray=*/ false, /*bRecursivelyIncludeAttachedActors=*/ true);
			}
		}

		for (AActor* Actor : ActorsToCheck)
		{
			// FoundActor is set by the FilteringFunction (captured)
			if (!FilteringFunction(Actor))
			{
				break;
			}
		}

		return FoundActor;
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
		PCGE_LOG(Error, "Some parameters are missing, abort.");
		return true;
	}

	// If we have no output connected, nothing to do
	if (!Context->Node || !Context->Node->IsOutputPinConnected(PCGPinConstants::DefaultOutputLabel))
	{
		PCGE_LOG(Verbose, "Node is not connected, nothing to do");
		return true;
	}

	// First find the actor depending on the selection
	AActor* FoundActor = PCGPropertyToParamDataHelpers::FindActor(*Context);

	if (!FoundActor)
	{
		PCGE_LOG(Error, "No matching actor was found.");
		return true;
	}

	// From there, we either check the actor, or the component attached to it.
	UObject* ObjectToInspect = FoundActor;
	if (Settings->bSelectComponent)
	{
		ObjectToInspect = FoundActor->GetComponentByClass(Settings->ComponentClass);
		if (!ObjectToInspect)
		{
			PCGE_LOG(Error, "Component doesn't exist in the found actor.");
			return true;
		}
	}

	// Try to get the property
	FProperty* Property = FindFProperty<FProperty>(ObjectToInspect->GetClass(), Settings->PropertyName);
	if (!Property)
	{
		PCGE_LOG(Error, "Property doesn't exist in the found actor.");
		return true;
	}

	// From there, we should be able to create the data.
	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = ParamData->MutableMetadata();
	check(Metadata);
	PCGMetadataEntryKey EntryKey = Metadata->AddEntry();

	if (!Metadata->SetAttributeFromProperty(Settings->OutputAttributeName, EntryKey, ObjectToInspect, Property, /*bCreate=*/ true))
	{
		PCGE_LOG(Error, "Error while creating an attribute. Either the property type is not supported by PCG or attribute creation failed.");
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = ParamData;

	return true;
}