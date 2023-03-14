// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNode.h"

#include "Containers/EnumAsByte.h"
#include "Containers/Queue.h"
#include "EdGraph/EdGraph.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Internationalization/Internationalization.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/CustomizableObjectPin.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByName.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByPosition.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "Toolkits/ToolkitManager.h"

class IToolkit;
class SWidget;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/** Copy pin data from old pin to new pin. Keeps the id of the new pin. */
void CopyPin(UEdGraphPin& NewPin, UEdGraphPin& OldPin)
{
	FGuid PinId = NewPin.PinId;
	
	NewPin.CopyPersistentDataFromOldPin(OldPin);
	NewPin.PinId = PinId;
	NewPin.bHidden = OldPin.bHidden;
}


UCustomizableObjectGraph* UCustomizableObjectNode::GetCustomizableObjectGraph() const
{
	return Cast<UCustomizableObjectGraph>(GetOuter());
}

bool UCustomizableObjectNode::IsSingleOutputNode() const
{
	return false;
}


UEdGraphPin* UCustomizableObjectNode::CustomCreatePin(EEdGraphPinDirection Direction, const FName& Type, const FName& Name, bool bIsArray)
{
	UEdGraphPin* Pin = CreatePin(Direction, Type, Name);
	if (bIsArray)
	{
		Pin->PinType.ContainerType = EPinContainerType::Array;
	}

	return Pin;
}


UEdGraphPin* UCustomizableObjectNode::CustomCreatePin(EEdGraphPinDirection Direction, const FName& Type, const FName& Name, UCustomizableObjectNodePinData* PinData)
{
	UEdGraphPin* Pin = CreatePin(Direction, Type, Name);
	PinsDataId.Add(Pin->PinId, PinData);
	
	return Pin;
}


bool UCustomizableObjectNode::ShouldBreakExistingConnections(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const
{
	return IsSingleOutputNode();
}

void UCustomizableObjectNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FPostEditChangePropertyDelegateParameters Parameters;
	Parameters.Node = this;
	Parameters.FPropertyChangedEvent = &PropertyChangedEvent;

	PostEditChangePropertyDelegate.Broadcast(Parameters);
	PostEditChangePropertyRegularDelegate.Broadcast(this, PropertyChangedEvent);
}


TSharedPtr<ICustomizableObjectEditor> UCustomizableObjectNode::GetGraphEditor() const
{
	const UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(GetCustomizableObjectGraph()->GetOuter());

	TSharedPtr<ICustomizableObjectEditor> CustomizableObjectEditor;
	if (CustomizableObject)
	{
		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(CustomizableObject);
		if (FoundAssetEditor.IsValid())
		{
			return StaticCastSharedPtr<ICustomizableObjectEditor>(FoundAssetEditor);
		}
	}

	return TSharedPtr<ICustomizableObjectEditor>();
}


bool UCustomizableObjectNode::CustomRemovePin(UEdGraphPin& Pin)
{
	PinsDataId.Remove(Pin.PinId);
	
	return RemovePin(&Pin);	
}


void UCustomizableObjectNode::GetInputPins(TArray<class UEdGraphPin*>& OutInputPins) const
{
	OutInputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Input)
		{
			OutInputPins.Add(Pins[PinIndex]);
		}
	}
}


void UCustomizableObjectNode::GetOutputPins(TArray<class UEdGraphPin*>& OutOutputPins) const
{
	OutOutputPins.Empty();

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			OutOutputPins.Add(Pins[PinIndex]);
		}
	}
}


UEdGraphPin* UCustomizableObjectNode::GetOutputPin(int32 OutputIndex) const
{
	for (int32 PinIndex = 0, FoundOutputs = 0; PinIndex < Pins.Num(); PinIndex++)
	{
		if (Pins[PinIndex]->Direction == EGPD_Output)
		{
			if (OutputIndex == FoundOutputs)
			{
				return Pins[PinIndex];
			}
			else
			{
				FoundOutputs++;
			}
		}
	}

	return NULL;
}


void UCustomizableObjectNode::SetRefreshNodeWarning()
{
	if (!bHasCompilerMessage && ErrorType < EMessageSeverity::Warning)
	{
		GetGraph()->NotifyGraphChanged();
	
		bHasCompilerMessage = true;
		ErrorType = EMessageSeverity::Warning;
		ErrorMsg = GetRefreshMessage();
	}
}


void UCustomizableObjectNode::RemoveWarnings()
{
	bHasCompilerMessage = false;
	ErrorType = 0;
	ErrorMsg.Empty();
}


void UCustomizableObjectNode::AllocateDefaultPins()
{
	AllocateDefaultPins(nullptr);
}


void UCustomizableObjectNode::ReconstructNode()
{
	ReconstructNode(CreateRemapPinsDefault());
}


void UCustomizableObjectNode::ReconstructNode(UCustomizableObjectNodeRemapPins* RemapPinsAction)
{
	Modify();

	// Break any single sided links. All connections must always be present in both nodes
	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];
		TArray<class UEdGraphPin*>& LinkedToRef = Pin->LinkedTo;
		for (int32 LinkIdx=0; LinkIdx < LinkedToRef.Num(); LinkIdx++)
		{
			UEdGraphPin* OtherPin = LinkedToRef[LinkIdx];
			// If we are linked to a pin that its owner doesn't know about, break that link
			if (OtherPin && !OtherPin->GetOwningNode()->Pins.Contains(OtherPin))
			{
				Pin->LinkedTo.Remove(OtherPin);
			}
		}
	}

	RemoveWarnings();

	// Move the existing orphan and non orphan pins to a saved array
	TArray<UEdGraphPin*> OldPins(Pins); // We can not empty Pins at this point since it will break all FEdGraphPinReference during the reconstruction.

	// Recreate the new pins
	AllocateDefaultPins(RemapPinsAction);
	
	// Try to remap orphan and non orphan pins.
	TArray<UEdGraphPin*> NewPins;
	NewPins.Reset(Pins.Num() - OldPins.Num());
	for (UEdGraphPin* Pin : Pins)
	{
		if (!OldPins.Contains(Pin))
		{
			NewPins.Add(Pin);
		}
	}

	TMap<UEdGraphPin*, UEdGraphPin*> PinsToRemap;
	TArray<UEdGraphPin*> PinsToOrphan;
	RemapPinsAction->RemapPins(OldPins, NewPins, PinsToRemap, PinsToOrphan);

	// Check only.
	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		check(NewPins.Contains(Pair.Value)); // Can only remap and old pin to a new pin.
		check(OldPins.Contains(Pair.Key));
	}

	RemapPins(PinsToRemap);
	RemapPinsData(PinsToRemap);
	
	// Check only.
	for (UEdGraphPin* Pin : PinsToOrphan)
	{
		check(OldPins.Contains(Pin)); // Can only orphan old pins.
	}

	bool bOrphanedPin = false;
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPin->Modify();

		if (PinsToOrphan.Contains(OldPin))
		{
			bOrphanedPin = bOrphanedPin || !OldPin->bOrphanedPin;
			OrphanPin(*OldPin);
		
			// Move pin to the end.
			Pins.RemoveSingle(OldPin);
			Pins.Add(OldPin);
		}
		else
		{
			// Remove the old pin
			OldPin->BreakAllPinLinks();
			
			CustomRemovePin(*OldPin);
		}
	}

	if (UEdGraph* Graph = GetCustomizableObjectGraph())
	{
		if (bOrphanedPin)
		{
			FCustomizableObjectEditorLogger::CreateLog(LOCTEXT("OrphanPinsWarningReconstruct", "Failed to remap old pins"))
			.Severity(EMessageSeverity::Warning)
			.Node(*this)
			.Log();
		}

		Graph->NotifyGraphChanged();
	}
	
	PostReconstructNodeDelegate.Broadcast();
}


void UCustomizableObjectNode::DestroyNode()
{
	Super::DestroyNode();
	DestroyNodeDelegate.Broadcast();
}


void UCustomizableObjectNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (!FromPin)
	{
		return;
	}

	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	for ( int PinIndex=0; PinIndex<Pins.Num(); ++PinIndex )
	{
		if ( FromPin->PinType.PinCategory == Pins[PinIndex]->PinType.PinCategory 
			&&
			FromPin->Direction != Pins[PinIndex]->Direction )
		{
			UEdGraphNode* OwningNode = FromPin->GetOwningNode(); // TryCreateConnection can reconstruct the node invalidating the FromPin. Save the OwningNode here.
			if (Schema->TryCreateConnection(FromPin, Pins[PinIndex]))
			{
				OwningNode->NodeConnectionListChanged();
				NodeConnectionListChanged();
			}
			break;
		}
	}
}


void UCustomizableObjectNode::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	NodeConnectionListChangedDelegate.Broadcast();
}



void UCustomizableObjectNode::PostInitProperties()
{
	Super::PostInitProperties();

	RemoveWarnings();
}


void UCustomizableObjectNode::BackwardsCompatibleFixup()
{
	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	// Fix UE large world coordinates automatic pin conversion. 
	// Now all pins with PinCategory == FName("float") get automatically changed to the new double type
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::FixBlueprintPinsUseRealNumbers)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			FEdGraphPinType* const PinType = &Pin->PinType;

			if (PinType->PinCategory == TEXT("real") && PinType->PinSubCategory == TEXT("double"))
			{
				PinType->PinCategory = TEXT("float");
				PinType->PinSubCategory = FName();
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::AutomaticNodeMaterialPerformance)
	{
		for (TTuple<FEdGraphPinReference, UCustomizableObjectNodePinData*> Pair : PinsData_DEPRECATED)
		{
			PinsDataId.Add(Pair.Key.Get()->PinId, Pair.Value);
		}

		PinsData_DEPRECATED.Empty();
	}

	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::FixBlueprintPinsUseRealNumbersAgain)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			FEdGraphPinType* const PinType = &Pin->PinType;

			if (PinType->PinCategory == TEXT("real") && PinType->PinSubCategory == TEXT("double"))
			{
				PinType->PinCategory = TEXT("float");
				PinType->PinSubCategory = FName();
			}
		}
	}
}


bool UCustomizableObjectNode::CanConnect( const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	bOutIsOtherNodeBlocklisted = false;

	bOutArePinsCompatible = InOwnedInputPin->PinType.PinCategory == InOutputPin->PinType.PinCategory;
	return bOutArePinsCompatible;
}

void UCustomizableObjectNode::MarkForReconstruct()
{
	TSharedPtr<FCustomizableObjectEditor> Editor = StaticCastSharedPtr<FCustomizableObjectEditor>(GetGraphEditor());

	// This could be called if a skeletal mesh asset in the graph is deleted without the editor being open. 
	if (Editor)
	{
		Editor->MarkForReconstruct(this);
	}
}


UCustomizableObjectNodeRemapPins* UCustomizableObjectNode::CreateRemapPinsDefault() const
{
	return CreateRemapPinsByName();
}


UCustomizableObjectNodeRemapPinsByName* UCustomizableObjectNode::CreateRemapPinsByName() const
{
	return NewObject<UCustomizableObjectNodeRemapPinsByName>();
}


UCustomizableObjectNodeRemapPinsByPosition* UCustomizableObjectNode::CreateRemapPinsByPosition() const
{
	return NewObject<UCustomizableObjectNodeRemapPinsByPosition>();
}


void UCustomizableObjectNode::RemapPins(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap)
{
	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		CopyPin(*Pair.Value, *Pair.Key);
	}
	
	RemapPinsDelegate.Broadcast(PinsToRemap);
}


void UCustomizableObjectNode::RemapPinsData(const TMap<UEdGraphPin*, UEdGraphPin*>& PinsToRemap)
{
	for (const TTuple<UEdGraphPin*, UEdGraphPin*>& Pair : PinsToRemap)
	{
		// Move pin data.
		if (TObjectPtr<UCustomizableObjectNodePinData>* PinDataOldPin = PinsDataId.Find(Pair.Key->PinId))
		{
			PinsDataId[Pair.Value->PinId] = *PinDataOldPin;		
		}
	}
}


void UCustomizableObjectNode::AddPinData(const UEdGraphPin& Pin, UCustomizableObjectNodePinData& PinData)
{
	PinsDataId.Add(Pin.PinId, &PinData);
}


TArray<UEdGraphPin*> UCustomizableObjectNode::GetAllOrphanPins() const
{
	TArray<UEdGraphPin*> OrphanPins;

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->bOrphanedPin)
		{
			OrphanPins.Add(Pin);
		}
	}

	return OrphanPins;
}


TArray<UEdGraphPin*> UCustomizableObjectNode::GetAllNonOrphanPins() const
{
	TArray<UEdGraphPin*> NonOrphanPins;
	NonOrphanPins.Reserve(Pins.Num());
	
	for (UEdGraphPin* Pin : Pins)
	{
		if (!Pin->bOrphanedPin)
		{
			NonOrphanPins.Add(Pin);
		}
	}

	return NonOrphanPins;
}


UCustomizableObjectNodePinData::UCustomizableObjectNodePinData()
{
	SetFlags(RF_Transactional);
}


void UCustomizableObjectNode::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	
	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);
}


void UCustomizableObjectNode::PostLoad()
{
	// Do not do work here. Do work at PostBackwardsCompatibleFixup.
	Super::PostLoad();
}


int32 UCustomizableObjectNode::GetLOD() const
{
	// Search recursively all parent nodes until a UCustomizableObjectNodeObject is found.
	// Once found, obtain the matching LOD.
	TQueue<const UCustomizableObjectNode*> PotentialCustomizableNodeObjects;
	PotentialCustomizableNodeObjects.Enqueue(this);

	const UCustomizableObjectNode* CurrentElement;
	while (PotentialCustomizableNodeObjects.Dequeue(CurrentElement))
	{
		for (UEdGraphPin* Pin : CurrentElement->GetAllNonOrphanPins())
		{
			if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (UCustomizableObjectNodeObject* CurrentCustomizableObjectNode = Cast<UCustomizableObjectNodeObject>(LinkedPin->GetOwningNode()))
					{
						return CurrentCustomizableObjectNode->GetLOD(LinkedPin);
					}
					else
					{
						const UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(LinkedPin->GetOwningNode());
						check(Node); // All nodes inherit from UCustomizableObjectNode
						PotentialCustomizableNodeObjects.Enqueue(Node);
					}
				}
			}
		}
	}

	return -1; // UCustomizableObjectNodeObject not found.
}


TArray<UCustomizableObjectNodeObject*> UCustomizableObjectNode::GetParentObjectNodes(const int LOD) const
{
	// Search recursively all parent nodes. Add all Object nodes found.
	check(LOD >= 0);

	TArray<UCustomizableObjectNodeObject*> Result;

	TQueue<const UCustomizableObjectNode*> PotentialCustomizableNodeObjects;
	PotentialCustomizableNodeObjects.Enqueue(this);

	const UCustomizableObjectNode* CurrentElement;
	while (PotentialCustomizableNodeObjects.Dequeue(CurrentElement))
	{
		for (UEdGraphPin* Pin : CurrentElement->GetAllNonOrphanPins())
		{
			if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : FollowOutputPinArray(*Pin))
				{
					UCustomizableObjectNode* Node = Cast<UCustomizableObjectNode>(LinkedPin->GetOwningNode());
					check(Node); // All nodes inherit from UCustomizableObjectNode.
					PotentialCustomizableNodeObjects.Enqueue(Node);

					if (UCustomizableObjectNodeObject* CurrentCustomizableObjectNode = Cast<UCustomizableObjectNodeObject>(Node))
					{
						const int32 LODPin = CurrentCustomizableObjectNode->GetLOD(LinkedPin);
						if (LODPin == -1 || LODPin == LOD) // Add to parents if it is not connected to a LOD pin or it is connected to the given LOD pin.
						{
							Result.Add(CurrentCustomizableObjectNode);

							// Explore external CO.
							if (CurrentCustomizableObjectNode->bIsBase)
							{
								if (UCustomizableObjectNodeObjectGroup* ObjectGroupNode = GetCustomizableObjectExternalNode<UCustomizableObjectNodeObjectGroup>(CurrentCustomizableObjectNode->ParentObject, CurrentCustomizableObjectNode->ParentObjectGroupId))
								{
									Result.Append(ObjectGroupNode->GetParentObjectNodes(LOD)); // Recursive call.
								}
							}
						}
					}
				}
			}
		}
	}

	return Result;
}


void UCustomizableObjectNode::SetPinHidden(UEdGraphPin& Pin, bool bHidden)
{
	Pin.SafeSetHidden(bHidden && CanPinBeHidden(Pin));

	GetGraph()->NotifyGraphChanged();
}


void UCustomizableObjectNode::SetPinHidden(const TArray<UEdGraphPin*>& PinsToHide, bool bHidden)
{
	for (UEdGraphPin* Pin : PinsToHide)
	{
		Pin->SafeSetHidden(bHidden && CanPinBeHidden(*Pin));
	}

	GetGraph()->NotifyGraphChanged();
}


bool UCustomizableObjectNode::CanPinBeHidden(const UEdGraphPin& Pin) const
{
	return !Pin.LinkedTo.Num() && !Pin.bOrphanedPin;
}


TSharedPtr<SWidget> UCustomizableObjectNode::CustomizePinDetails(UEdGraphPin& Pin)
{
	return nullptr;
}


UCustomizableObjectNodePinData* UCustomizableObjectNode::GetPinData(const UEdGraphPin& Pin) const
{
	if (TObjectPtr<UCustomizableObjectNodePinData> const* Result = PinsDataId.Find(Pin.PinId))
	{
		return *Result;	
	}
	else
	{
		return nullptr;
	}
}


#undef LOCTEXT_NAMESPACE
