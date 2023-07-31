// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialGraphNode_Root.cpp
=============================================================================*/

#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "MaterialShared.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialEditorUtilities.h"
#include "GraphEditorSettings.h"

#define LOCTEXT_NAMESPACE "MaterialGraphNode_Root"

/////////////////////////////////////////////////////
// UMaterialGraphNode_Root

UMaterialGraphNode_Root::UMaterialGraphNode_Root(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UMaterialGraphNode_Root::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FMaterialEditorUtilities::GetOriginalObjectName(this->GetGraph());
}

FLinearColor UMaterialGraphNode_Root::GetNodeTitleColor() const
{
	return GetDefault<UGraphEditorSettings>()->ResultNodeTitleColor;
}

FText UMaterialGraphNode_Root::GetTooltipText() const
{
	return LOCTEXT("MaterialNode", "Result node of the Material");
}

void UMaterialGraphNode_Root::PostPlacedNewNode()
{
	if (Material)
	{
		NodePosX = Material->EditorX;
		NodePosY = Material->EditorY;
	}
}

UObject* UMaterialGraphNode_Root::GetMaterialNodeOwner() const
{
	return Material;
}

int32 UMaterialGraphNode_Root::GetSourceIndexForInputIndex(int32 InputIndex) const
{
	const UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
	for (int32 SourceIndex = 0; SourceIndex < MaterialGraph->MaterialInputs.Num(); ++SourceIndex)
	{
		// InputIndex will be the EMaterialProperty of the input
		// SourceIndex will be an index into the 'MaterialInputs' array
		if (InputIndex == (int32)MaterialGraph->MaterialInputs[SourceIndex].GetProperty())
		{
			return SourceIndex;
		}
	}
	return INDEX_NONE;
}

uint32 UMaterialGraphNode_Root::GetPinMaterialType(const UEdGraphPin* Pin) const
{
	if (Pin->PinType.PinCategory == UMaterialGraphSchema::PC_Exec)
	{
		return MCT_Execution;
	}

	const UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());
	const FMaterialInputInfo& MaterialInput = MaterialGraph->MaterialInputs[Pin->SourceIndex];
	EMaterialProperty Property = MaterialInput.GetProperty();

	uint32 MaterialType = 0u;
	if (Property == MP_MaterialAttributes)
	{
		MaterialType = MCT_MaterialAttributes;
	}
	else if (Property == MP_FrontMaterial)
	{
		MaterialType = MCT_Strata;
	}
	else
	{
		MaterialType = FMaterialAttributeDefinitionMap::GetValueType(Property);
	}
	return MaterialType;
}

void UMaterialGraphNode_Root::CreateInputPins()
{
	UMaterialGraph* MaterialGraph = CastChecked<UMaterialGraph>(GetGraph());

	if (Material->IsUsingControlFlow())
	{
		// Create the execution pin
		UEdGraphPin* NewPin = CreatePin(EGPD_Input, UMaterialGraphSchema::PC_Exec, NAME_None, NAME_None);
		NewPin->SourceIndex = 0;
		// Makes sure pin has a name for lookup purposes but user will never see it
		NewPin->PinName = CreateUniquePinName(TEXT("Input"));
		NewPin->PinFriendlyName = LOCTEXT("Space", " ");
	}

	for (int32 Index = 0; Index < MaterialGraph->MaterialInputs.Num(); ++Index)
	{
		const FMaterialInputInfo& MaterialInput = MaterialGraph->MaterialInputs[Index];
		EMaterialProperty Property = MaterialInput.GetProperty();
	
		UEdGraphPin* InputPin = CreatePin(EGPD_Input, UMaterialGraphSchema::PC_MaterialInput, *FString::Printf(TEXT("%d"), (int32)Property), *MaterialInput.GetName().ToString());
		InputPin->SourceIndex = Index;
	}

}

#undef LOCTEXT_NAMESPACE
