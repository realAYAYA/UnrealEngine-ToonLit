// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"

#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeObjectGroup::UCustomizableObjectNodeObjectGroup()
	: Super()
{
	GroupName = "Unnamed Group";
}

void UCustomizableObjectNodeObjectGroup::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	UEdGraphPin* groupPin = GroupProjectorsPin();
	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::GroupProjectorPinTypeAdded
		&& groupPin
		&& groupPin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Projector)
	{
		groupPin->PinType.PinCategory = UEdGraphSchema_CustomizableObject::PC_GroupProjector;
	}

	LastGroupName = GroupName;
}


void UCustomizableObjectNodeObjectGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeObjectGroup, GroupName))
	{
		for (UEdGraphPin* LinkedPin : FollowOutputPinArray(*GroupPin()))
		{
			UCustomizableObjectNode* Root = CastChecked<UCustomizableObjectNode>(LinkedPin->GetOwningNode());

			if (UCustomizableObjectNodeObject* CurrentRootNode = Cast<UCustomizableObjectNodeObject>(Root))
			{
				if (CurrentRootNode->ParentObject)
				{
					TArray<UCustomizableObject*> VisitedObjects;
					CurrentRootNode = GetFullGraphRootNodeObject(CurrentRootNode, VisitedObjects);
				}
				if (!CurrentRootNode->ParentObject)
				{
					for (FCustomizableObjectState& State : CurrentRootNode->States)
					{
						for (int32 p = 0; p < State.RuntimeParameters.Num(); ++p)
						{
							if (State.RuntimeParameters[p].Equals(LastGroupName))
							{
								State.RuntimeParameters[p]= GroupName;
							}
						}
						if (State.ForcedParameterValues.Contains(GroupName))
						{
							// Forced parameter already contains the NEW name of the parameter. We currently allow inconsistencies while working before compile time, so no warning needed here. When this changes to ID based instead of String based, this would become a warning worthy check (TODO: MTBL-1071).
						}
						else if (State.ForcedParameterValues.Contains(LastGroupName))
						{
							FString LastForcedValue = State.ForcedParameterValues.FindAndRemoveChecked(LastGroupName);
							State.ForcedParameterValues.Emplace(GroupName, LastForcedValue);
						}
					}
				}
			}
		}
	}
	LastGroupName = GroupName;
}


FText UCustomizableObjectNodeObjectGroup::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView ||
		GroupName.IsEmpty())
	{
		return LOCTEXT("Object_Group", "Object Group");
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("GroupName"), FText::FromString(GroupName));

		return FText::Format(LOCTEXT("Group_Object_Title", "{GroupName}\nObject Group"), Args);
	}
}


FLinearColor UCustomizableObjectNodeObjectGroup::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Object);
}


FText UCustomizableObjectNodeObjectGroup::GetTooltipText() const
{
	return LOCTEXT("Grpup_Object_Tooltip",
	"Define one or multiple parameters that are a collection of Customizable Objects that share a mutual relationship: they either are\nexclusive from each other, at most one of them can be active, or at least one of them has to be, or any combination of them can be\nenabled, or they define materials that will always be shown together.");
}


void UCustomizableObjectNodeObjectGroup::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ObjectsPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, FName("Objects"), true);
	ObjectsPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ProjectorsPin = CustomCreatePin(EGPD_Input, Schema->PC_GroupProjector, FName("Projectors"), true);
	ObjectsPin->bDefaultValueIsIgnored = true;

	CustomCreatePin(EGPD_Output, Schema->PC_Object, FName("Group"));
}


bool UCustomizableObjectNodeObjectGroup::IsSingleOutputNode() const
{
	return true;
}


UEdGraphPin* UCustomizableObjectNodeObjectGroup::ObjectsPin() const
{
	return FindPin(TEXT("Objects"));
}


UEdGraphPin* UCustomizableObjectNodeObjectGroup::GroupProjectorsPin() const
{
	return FindPin(TEXT("Projectors"));
}


UEdGraphPin* UCustomizableObjectNodeObjectGroup::GroupPin() const
{
	return FindPin(TEXT("Group"));
}


#undef LOCTEXT_NAMESPACE
