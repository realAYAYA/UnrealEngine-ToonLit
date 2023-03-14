// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeTextureLayer.h"

#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Serialization/Archive.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"

class UCustomizableObjectNodeRemapPins;
class UEdGraph;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

TArray<UCustomizableObjectNodeTextureLayer::TextureLayerLayersData> UCustomizableObjectNodeTextureLayer::ArrayDestroyedNodes;

UCustomizableObjectNodeTextureLayer::UCustomizableObjectNodeTextureLayer()
	: Super()
{

}


void UCustomizableObjectNodeTextureLayer::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("Layers") )
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeTextureLayer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PinsNamesImageToTexture)
	{
		OutputPinReference = FEdGraphPinReference(FindPin(TEXT("Image")));
	}
}


void UCustomizableObjectNodeTextureLayer::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* OutputPin = CustomCreatePin(EGPD_Output, Schema->PC_Image, FName(TEXT("Texture")));
	OutputPin->bDefaultValueIsIgnored = true;

	OutputPinReference = FEdGraphPinReference(OutputPin);

	for (int LayerIndex = Layers.Num() - 1; LayerIndex >=0 ; --LayerIndex )
	{
		FString PinName = FString::Printf( TEXT("Layer %d "), LayerIndex );
		UEdGraphPin* LayerPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, *PinName);
		LayerPin->bDefaultValueIsIgnored = true;

		PinName = FString::Printf( TEXT("Mask %d "), LayerIndex );
		UEdGraphPin* MaskPin = CustomCreatePin(EGPD_Input, Schema->PC_Image, *PinName);
		MaskPin->bDefaultValueIsIgnored = true;
	}

	UEdGraphPin* BasePin = CustomCreatePin(EGPD_Input, Schema->PC_Image, FName(TEXT("Base")));
	BasePin->bDefaultValueIsIgnored = true;
}


FText UCustomizableObjectNodeTextureLayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Texture_Layer", "Texture Layer");
}


FLinearColor UCustomizableObjectNodeTextureLayer::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Image);
}


void UCustomizableObjectNodeTextureLayer::PrepareForCopying()
{
	// Destroy previous copy with possible out of date information just in case
	int32 IndexToRemove = GetIndexInArrayDestroyedNodes(this);

	if (IndexToRemove != -1)
	{
		ArrayDestroyedNodes.RemoveAt(IndexToRemove);
	}

	TextureLayerLayersData Data;
	Data.Name = GetName();
	Data.Graph = GetGraph();
	Data.Layers = Layers;
	ArrayDestroyedNodes.Add(Data);
}

void UCustomizableObjectNodeTextureLayer::PostPasteNode()
{
	int32 IndexToRemove = GetIndexInArrayDestroyedNodes(this);

	if (IndexToRemove != -1)
	{
		Layers = ArrayDestroyedNodes[IndexToRemove].Layers;
		ArrayDestroyedNodes.RemoveAt(IndexToRemove);
	}
}


bool UCustomizableObjectNodeTextureLayer::CanConnect(const UEdGraphPin* InOwnedInputPin, const UEdGraphPin* InOutputPin, bool& bOutIsOtherNodeBlocklisted, bool& bOutArePinsCompatible) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	// In layer pins we accept both colors and images
	for ( int LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex )
	{
		if (LayerPin(LayerIndex) == InOwnedInputPin )
		{
			return (InOutputPin->PinType.PinCategory==Helper_GetPinCategory(Schema->PC_Image))
				|| (InOutputPin->PinType.PinCategory==Helper_GetPinCategory(Schema->PC_Color));
		}
	}

	return Super::CanConnect( InOwnedInputPin, InOutputPin, bOutIsOtherNodeBlocklisted, bOutArePinsCompatible);
}


UEdGraphPin* UCustomizableObjectNodeTextureLayer::OutputPin() const
{
	return OutputPinReference.Get();
}


int32 UCustomizableObjectNodeTextureLayer::GetIndexInArrayDestroyedNodes(UCustomizableObjectNodeTextureLayer* Node)
{
	const int32 MaxIndex = ArrayDestroyedNodes.Num();
	FString NodeName = Node->GetName();
	UEdGraph* NodeGraph = Node->GetGraph();
	for (int32 i = 0; i < MaxIndex; ++i)
	{
		TextureLayerLayersData& DataTemp = ArrayDestroyedNodes[i];

		//if ((DataTemp.Name == NodeName) && (DataTemp.Graph == NodeGraph))
		if (DataTemp.Name == NodeName) // Copying between graphs would yeld incorredct array data, are the names of the elements unique?
		{
			return i;
		}
	}

	return -1;
}


FText UCustomizableObjectNodeTextureLayer::GetTooltipText() const
{
	return LOCTEXT("Texture_Layer_Tooltip", "Combines multiple textures into one.");
}

#undef LOCTEXT_NAMESPACE

