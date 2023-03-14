// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"

#include "Containers/Queue.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Logging/MessageLog.h"
#include "Logging/TokenizedMessage.h"
#include "Math/UnrealMathSSE.h"
#include "MuCO/CustomizableObject.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeObject::UCustomizableObjectNodeObject()
	: Super()
{
	bIsBase = true;

	ObjectName = "Unnamed Object";
	NumLODs = 1;
	Identifier.Invalidate();

	if (!Identifier.IsValid())
	{
		Identifier = FGuid::NewGuid();
		IdentifierVerification = Identifier;
	}
}


void UCustomizableObjectNodeObject::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!Identifier.IsValid())
	{
		Identifier = FGuid::NewGuid();
	}

	// Update the cached flag in the main object
	UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>( GetCustomizableObjectGraph()->GetOuter() );
	if (CustomizableObject)
	{
		CustomizableObject->bIsChildObject = ParentObject != nullptr;
	}

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs"))
	{
		NumLODs = FMath::Clamp(NumLODs, 1, 64);

		ReconstructNode();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeObject::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	for (int32 i = 0; i < NumLODs; ++i)
	{
		FString LODName = FString::Printf( TEXT("LOD %d "), i );

		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(*LODName), true);
		Pin->bDefaultValueIsIgnored = true;
	}

	UEdGraphPin* ChildrenPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, FName("Children"), true );
	ChildrenPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Object, FName("Object"));

	if (bIsBase)
	{
		OutputPin->bHidden = true;
	}
}


FText UCustomizableObjectNodeObject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FFormatNamedArguments Args;	
	Args.Add(TEXT("ObjectName"), FText::FromString(ObjectName) );


	if (TitleType == ENodeTitleType::ListView)
	{
		if (!bIsBase)
		{						
			return FText::Format(LOCTEXT("Child_Object_Title_List_Deprecated", "{ObjectName} - Child Object (Deprecated)"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("Base_Object_Title_List", "{ObjectName} - Base Object"), Args);
		}
	}
	else
	{
		if (!bIsBase)
		{
			return FText::Format(LOCTEXT("Child_Object_Title_Deprecated", "{ObjectName}\nChild Object (Deprecated)"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("Base_Object_Title", "{ObjectName}\nBase Object"), Args);
		}
	}
}


FLinearColor UCustomizableObjectNodeObject::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Object);
}


void UCustomizableObjectNodeObject::PrepareForCopying()
{
	FText Msg(LOCTEXT("Cannot copy object node","There can only be one Customizable Object Node Object element per graph") );
	FMessageLog MessageLog("Mutable");
	MessageLog.Notify(Msg, EMessageSeverity::Info, true);
}


int32 UCustomizableObjectNodeObject::GetLOD(UEdGraphPin* Pin) const
{
	for (int32 LOD = 0; LOD < GetNumLODPins(); ++LOD)
	{
		if (Pin == LODPin(LOD))
		{
			return LOD;
		}
	}

	return -1;
}


bool UCustomizableObjectNodeObject::CanUserDeleteNode() const
{
	return !bIsBase;
}


bool UCustomizableObjectNodeObject::CanDuplicateNode() const
{
	return !bIsBase;
}


TArray<UCustomizableObjectNodeMaterial*> UCustomizableObjectNodeObject::GetMaterialNodes(const int LOD) const
{
	TArray<UCustomizableObjectNodeMaterial*> Result;

	TQueue<UEdGraphNode*> PotentialCustomizableNodeObjects;

	for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*LODPin(LOD)))
	{
		PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
	}

	UEdGraphNode* CurrentElement;
	while (PotentialCustomizableNodeObjects.Dequeue(CurrentElement))
	{
		if (UCustomizableObjectNodeMaterial* CurrentMaterialNode = Cast<UCustomizableObjectNodeMaterial>(CurrentElement))
		{
			Result.Add(CurrentMaterialNode);
		} 
		else if (UCustomizableObjectNodeMaterialVariation* CurrentMaterialVariationNode = Cast<UCustomizableObjectNodeMaterialVariation>(CurrentElement))
		{
			// Case of material variation. It's not a material, but a node that further references any material, add all its inputs that could be a material
			for (int numMaterialPin = 0; numMaterialPin < CurrentMaterialVariationNode->GetNumVariations(); ++numMaterialPin)
			{
				const UEdGraphPin* VariationPin = CurrentMaterialVariationNode->VariationPin(numMaterialPin);
				for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*VariationPin))
				{
					PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
				}
			}

			const UEdGraphPin* DefaultPin = CurrentMaterialVariationNode->DefaultPin();
			for (const UEdGraphPin* LinkedPin : FollowInputPinArray(*DefaultPin))
			{
				PotentialCustomizableNodeObjects.Enqueue(LinkedPin->GetOwningNode());
			}
		} 
	}

	return Result;
}


void UCustomizableObjectNodeObject::PostBackwardsCompatibleFixup()
{
	Super::PostBackwardsCompatibleFixup();

	const bool bJustBuiltCoFlag = !GetAllNonOrphanPins().Num() && !NodeGuid.IsValid();
	if (!bJustBuiltCoFlag && Identifier == IdentifierVerification)
	{
		FCustomizableObjectEditorLogger::CreateLog(LOCTEXT("ResaveNode","Please re-save this Customizable Object to avoid binary differences when packaging"))
		.Severity(EMessageSeverity::Warning)
		.Node(*this)
		.Log();
	}
}

void UCustomizableObjectNodeObject::PostPasteNode()
{
	Super::PostPasteNode();

	Identifier = FGuid::NewGuid();
}

void UCustomizableObjectNodeObject::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	Identifier = FGuid::NewGuid();
}

void UCustomizableObjectNodeObject::SetParentObject(UCustomizableObject* CustomizableParentObject)
{
	if (CustomizableParentObject != GetGraphEditor()->GetCustomizableObject())
	{
		ParentObject = CustomizableParentObject;

		// Update the cached flag in the main object
		UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(GetCustomizableObjectGraph()->GetOuter());
		if (CustomizableObject)
		{
			CustomizableObject->bIsChildObject = ParentObject != nullptr;

			TSharedPtr<ICustomizableObjectEditor> Editor = GetGraphEditor();

			if (Editor.IsValid())
			{
				Editor->UpdateObjectProperties();
			}
		}
	}
}


FText UCustomizableObjectNodeObject::GetTooltipText() const
{
	return LOCTEXT("Base_Object_Tooltip",
	"As root object: Defines a customizable object root, its basic properties and its relationship with descendant Customizable Objects.\n\nAs a child object: Defines a Customizable Object children outside of the parent asset, to ease organization of medium and large\nCustomizable Objects. (Functionally equivalent to the Child Object Node.)");
}


bool UCustomizableObjectNodeObject::IsSingleOutputNode() const
{
	return true;
}


#undef LOCTEXT_NAMESPACE
