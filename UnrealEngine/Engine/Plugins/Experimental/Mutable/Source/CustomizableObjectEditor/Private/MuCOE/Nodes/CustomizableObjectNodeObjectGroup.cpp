// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"

#include "EdGraph/EdGraphPin.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"

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
}

void UCustomizableObjectNodeObjectGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeObjectGroup::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ObjectsPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, FName("Objects"), true );
	ObjectsPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ProjectorsPin = CustomCreatePin(EGPD_Input, Schema->PC_GroupProjector, FName("Projectors"), true );
	ObjectsPin->bDefaultValueIsIgnored = true;

	CustomCreatePin(EGPD_Output, Schema->PC_Object, FName("Group"));
}


FText UCustomizableObjectNodeObjectGroup::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("GroupName"), FText::FromString(GroupName));

	return FText::Format(LOCTEXT("Group_Object_Title", "{GroupName}\nGroup"), Args);			
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


#undef LOCTEXT_NAMESPACE
