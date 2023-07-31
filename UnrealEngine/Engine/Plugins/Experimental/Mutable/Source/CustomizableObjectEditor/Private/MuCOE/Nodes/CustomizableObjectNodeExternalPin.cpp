// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeExternalPin.h"

#include "Delegates/Delegate.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/CustomizableObjectPin.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExposePin.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeRemapPinsCustomExternalPin::RemapPins(const TArray<UEdGraphPin*>& OldPins, const TArray<UEdGraphPin*>& NewPins, TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap, TArray<UEdGraphPin*>& PinsToOrphan)
{
	if (OldPins.Num()) 
	{
		UEdGraphPin* const OldPin = OldPins[0];
		if (NewPins.Num())
		{
			PinsToRemap.Add(OldPin, NewPins[0]);
		}

		if (!Node->GetNodeExposePin())
		{
			PinsToOrphan.Add(OldPin);
		}
	}
}


void UCustomizableObjectNodeExternalPin::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNodeExternalPin::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();
	
	// Get the pin type from the actual pin.
	if (GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::BeforeCustomVersionWasAdded)
	{
		if (PinType.IsNone())
		{
			PinType = Pins[0]->PinType.PinCategory;
		}
	}
}


void UCustomizableObjectNodeExternalPin::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	if (UCustomizableObjectNodeExposePin* NodeExposePin = GetNodeExposePin())
	{
		OnNameChangedDelegateHandle = NodeExposePin->OnNameChangedDelegate.AddUObject(this, &Super::ReconstructNode);
		DestroyNodeDelegateHandle = NodeExposePin->DestroyNodeDelegate.AddUObject(this, &Super::ReconstructNode);
	}

	// Reconstruct the node since the NodeExposePin pin name may have changed while not loaded.
	ReconstructNode();
}


void UCustomizableObjectNodeExternalPin::SetExternalObjectNodeId(FGuid Guid)
{
	if (UCustomizableObjectNodeExposePin* NodeExposePin = GetNodeExposePin())
	{
		NodeExposePin->OnNameChangedDelegate.Remove(OnNameChangedDelegateHandle);
		NodeExposePin->DestroyNodeDelegate.Remove(DestroyNodeDelegateHandle);
	}

	ExternalObjectNodeId = Guid;

	if (UCustomizableObjectNodeExposePin* NodeExposePin = GetNodeExposePin())
	{
		OnNameChangedDelegateHandle = NodeExposePin->OnNameChangedDelegate.AddUObject(this, &Super::ReconstructNode);
		DestroyNodeDelegateHandle = NodeExposePin->DestroyNodeDelegate.AddUObject(this, &Super::ReconstructNode);
	}

	ReconstructNode();
}


UEdGraphPin* UCustomizableObjectNodeExternalPin::GetExternalPin() const
{
	const TArray<UEdGraphPin*> NonOrphanPins = GetAllNonOrphanPins();
	if (NonOrphanPins.Num())
	{
		return NonOrphanPins[0];
	}
	else
	{
		return nullptr;
	}
}


UCustomizableObjectNodeExposePin* UCustomizableObjectNodeExternalPin::GetNodeExposePin() const
{
	return GetCustomizableObjectExternalNode<UCustomizableObjectNodeExposePin>(ExternalObject, ExternalObjectNodeId);
}


UEdGraphPin* UCustomizableObjectNodeExternalPin::CreateExternalPin(const UCustomizableObjectNodeExposePin* NodeExposePin)
{
	FName PinName = NodeExposePin ? FName(NodeExposePin->GetNodeName()) : FName("Object");
	const bool bIsArrayPinCategory = PinType == UEdGraphSchema_CustomizableObject::PC_GroupProjector;

	return CustomCreatePin(EGPD_Output, PinType, PinName, bIsArrayPinCategory);
}


void UCustomizableObjectNodeExternalPin::BeginConstruct()
{
	// Create a pin already orphaned since the node does not have a linked Expose Pin node yet.
	UEdGraphPin* Pin = CreateExternalPin(GetNodeExposePin());
	OrphanPin(*Pin);
}


void UCustomizableObjectNodeExternalPin::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// Pass information to the remap pins action context.
	if (UCustomizableObjectNodeRemapPinsCustomExternalPin* RemapPinsCustom = Cast<UCustomizableObjectNodeRemapPinsCustomExternalPin>(RemapPins))
	{
		RemapPinsCustom->Node = this;
	}

	if (UCustomizableObjectNodeExposePin* NodeExposePin = GetNodeExposePin())
	{
		CreateExternalPin(NodeExposePin);
	}
}


FText UCustomizableObjectNodeExternalPin::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FText PinTypeName = UEdGraphSchema_CustomizableObject::GetPinCategoryName(PinType);

	if (TitleType == ENodeTitleType::ListView || !ExternalObject)
	{
		return FText::Format(LOCTEXT("External_Pin_Title", "Import {0} Pin"), PinTypeName);
	}
	else
	{
		return FText::Format(LOCTEXT("External_Pin_Title_WithName", "{0}\nImport {1} Pin"), FText::FromString(ExternalObject->GetName()), PinTypeName);
	}
}


FLinearColor UCustomizableObjectNodeExternalPin::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(PinType);
}


FText UCustomizableObjectNodeExternalPin::GetTooltipText() const
{
	return LOCTEXT("Import_Pin_Tooltip", "Make use of a value defined elsewhere in this Customizable Object hierarchy.");
}

bool UCustomizableObjectNodeExternalPin::CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	// Chech the pin types do match
	bOutArePinsCompatible = Super::CanConnect(InOwnedInputPin, InOutputPin, bOutIsOtherNodeBlocklisted, bOutArePinsCompatible);

	// Check the type of the other node to make sure it is not one we do not want to allow the connection with
	bOutIsOtherNodeBlocklisted = Cast<UCustomizableObjectNodeExposePin>(InOutputPin->GetOwningNode()) != nullptr;

	return bOutArePinsCompatible && !bOutIsOtherNodeBlocklisted;
}


void UCustomizableObjectNodeExternalPin::BeginPostDuplicate(bool bDuplicateForPIE)
{
	Super::BeginPostDuplicate(bDuplicateForPIE);

	if (ExternalObjectNodeId.IsValid())
	{
		if (UCustomizableObjectGraph* CEdGraph = Cast<UCustomizableObjectGraph>(GetGraph()))
		{
			ExternalObjectNodeId = CEdGraph->RequestNotificationForNodeIdChange(ExternalObjectNodeId, NodeGuid);
		}
	}
}


void UCustomizableObjectNodeExternalPin::UpdateReferencedNodeId(const FGuid& NewGuid)
{
	ExternalObjectNodeId = NewGuid;
}


UCustomizableObjectNodeRemapPins* UCustomizableObjectNodeExternalPin::CreateRemapPinsDefault() const
{
	return NewObject<UCustomizableObjectNodeRemapPinsCustomExternalPin>();
}


#undef LOCTEXT_NAMESPACE
