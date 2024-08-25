// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"

#include "Containers/Queue.h"
#include "Logging/MessageLog.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/CustomizableObjectExtension.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/ICustomizableObjectEditor.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMaterialVariation.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

const FName UCustomizableObjectNodeObject::ChildrenPinName(TEXT("Children"));
const FName UCustomizableObjectNodeObject::OutputPinName(TEXT("Object"));
const TCHAR* UCustomizableObjectNodeObject::LODPinNamePrefix = TEXT("LOD ");

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


void UCustomizableObjectNodeObject::BackwardsCompatibleFixup()
{
	Super::BackwardsCompatibleFixup();

	const int32 CustomizableObjectCustomVersion = GetLinkerCustomVersion(FCustomizableObjectCustomVersion::GUID);
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::StateTextureCompressionStrategyEnum)
	{
		for (FCustomizableObjectState& State : States)
		{
			if (State.TextureCompressionStrategy== ETextureCompressionStrategy::None
				&&
				State.bDontCompressRuntimeTextures_DEPRECATED)
			{
				State.bDontCompressRuntimeTextures_DEPRECATED = false;
				State.TextureCompressionStrategy = ETextureCompressionStrategy::DontCompressRuntime;
			}
		}
	}
	
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::RegenerateNodeObjectsIds)
	{
		// This will regenerate all the Node Object Guids to finally remove the duplicated Guids warning.
		// It is safe to do this here as Node Object do not use its node guid to link themeselves to other nodes.
		CreateNewGuid();
	}

	// Update state never-stream flag from deprecated enum
	if (CustomizableObjectCustomVersion < FCustomizableObjectCustomVersion::CustomizableObjectStateHasSeparateNeverStreamFlag)
	{
		for (FCustomizableObjectState& s : States)
		{
			s.bDisableTextureStreaming = s.TextureCompressionStrategy != ETextureCompressionStrategy::None;
		}
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
		CustomizableObject->GetPrivate()->SetIsChildObject(ParentObject != nullptr);
	}

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumLODs"))
	{
		NumLODs = FMath::Clamp(NumLODs, 1, 64);

		for (int32 CompSetIndex = 0; CompSetIndex < ComponentSettings.Num(); ++CompSetIndex)
		{
			ComponentSettings[CompSetIndex].LODReductionSettings.SetNum(NumLODs);
		}

		ReconstructNode();
	}

	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("NumMeshComponents"))
	{
		ComponentSettings.SetNum(NumMeshComponents);

		for (int32 CompSetIndex = 0; CompSetIndex < ComponentSettings.Num(); ++CompSetIndex)
		{
			ComponentSettings[CompSetIndex].LODReductionSettings.SetNum(NumLODs);
		}
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeObject::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// NOTE: Ensure all built-in pins are handled in UCustomizableObjectNodeObject::IsBuiltInPin

	for (int32 i = 0; i < NumLODs; ++i)
	{
		FString LODName = FString::Printf(TEXT("%s%d "), LODPinNamePrefix, i);

		UEdGraphPin* Pin = CustomCreatePin(EGPD_Input, Schema->PC_Material, FName(*LODName), true);
		Pin->bDefaultValueIsIgnored = true;
	}

	UEdGraphPin* ChildrenPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, ChildrenPinName, true );
	ChildrenPin->bDefaultValueIsIgnored = true;

	for (const FRegisteredObjectNodeInputPin& Pin : ICustomizableObjectModule::Get().GetAdditionalObjectNodePins())
	{
		// Use the global pin name here to prevent extensions using the same pin names from
		// interfering with each other.
		//
		// This also prevents extension pins from clashing with the built-in pins from this node,
		// such as "Object".
		UEdGraphPin* GraphPin = CustomCreatePin(EGPD_Input, Pin.InputPin.PinType, Pin.GlobalPinName, Pin.InputPin.bIsArray);

		GraphPin->PinFriendlyName = Pin.InputPin.DisplayName;
	}

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Object, OutputPinName);

	if (bIsBase)
	{
		OutputPin->bHidden = true;
	}
}


FText UCustomizableObjectNodeObject::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView ||
		ObjectName.IsEmpty())
	{
		if (bIsBase)
		{
			return LOCTEXT("Base_Object", "Base Object");
		}
		else
		{
			return LOCTEXT("Base_Object_Deprecated", "Base Object (Deprecated)");
		}
	}
	else
	{
		FFormatNamedArguments Args;	
		Args.Add(TEXT("ObjectName"), FText::FromString(ObjectName) );

		if (bIsBase)
		{
			return FText::Format(LOCTEXT("Base_Object_Title", "{ObjectName}\nBase Object"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("Child_Object_Title_Deprecated", "{ObjectName}\nChild Object (Deprecated)"), Args);
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
		.Context(*this)
		.Log();
	}

	// Fix up ComponentSettings
	if (ComponentSettings.IsEmpty())
	{
		FComponentSettings ComponentSettingsTemplate;
		ComponentSettingsTemplate.LODReductionSettings.SetNum(NumLODs);

		ComponentSettings.Init(ComponentSettingsTemplate, NumMeshComponents);
	}

	// Reconstruct in case any extension pins have changed
	ReconstructNode();
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
			CustomizableObject->GetPrivate()->SetIsChildObject(ParentObject != nullptr);

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

bool UCustomizableObjectNodeObject::IsBuiltInPin(FName PinName)
{
	return PinName == ChildrenPinName
		|| PinName == OutputPinName
		|| PinName.ToString().StartsWith(LODPinNamePrefix);
}

#undef LOCTEXT_NAMESPACE
