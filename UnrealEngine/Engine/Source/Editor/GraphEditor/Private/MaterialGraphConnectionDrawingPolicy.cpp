// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialGraphConnectionDrawingPolicy.h"

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditorSettings.h"
#include "Layout/ArrangedWidget.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialGraphNode_Knot.h"
#include "Math/Color.h"
#include "SGraphPin.h"
#include "Shader/ShaderTypes.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FSlateRect;

/////////////////////////////////////////////////////
// FMaterialGraphConnectionDrawingPolicy

FMaterialGraphConnectionDrawingPolicy::FMaterialGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements)
	, MaterialGraph(CastChecked<UMaterialGraph>(InGraphObj))
	, MaterialGraphSchema(CastChecked<UMaterialGraphSchema>(InGraphObj->GetSchema()))
{
	// Don't want to draw ending arrowheads
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;

	// Still need to be able to perceive the graph while dragging connectors, esp over comment boxes
	HoverDeemphasisDarkFraction = 0.4f;
}



bool FMaterialGraphConnectionDrawingPolicy::FindPinCenter(UEdGraphPin* Pin, FVector2D& OutCenter) const
{
	if (const TSharedPtr<SGraphPin>* pPinWidget = PinToPinWidgetMap.Find(Pin))
	{
		if (FArrangedWidget* pPinEntry = PinGeometries->Find((*pPinWidget).ToSharedRef()))
		{
			OutCenter = FGeometryHelper::CenterOf(pPinEntry->Geometry);
			return true;
		}
	}

	return false;
}

bool FMaterialGraphConnectionDrawingPolicy::GetAverageConnectedPosition(class UMaterialGraphNode_Knot* Knot, EEdGraphPinDirection Direction, FVector2D& OutPos) const
{
	FVector2D Result = FVector2D::ZeroVector;
	int32 ResultCount = 0;

	UEdGraphPin* Pin = (Direction == EGPD_Input) ? Knot->GetInputPin() : Knot->GetOutputPin();
	for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
	{
		FVector2D CenterPoint;
		if (FindPinCenter(LinkedPin, /*out*/ CenterPoint))
		{
			Result += CenterPoint;
			ResultCount++;
		}
	}

	if (ResultCount > 0)
	{
		OutPos = Result * (1.0f / ResultCount);
		return true;
	}
	else
	{
		return false;
	}
}

bool FMaterialGraphConnectionDrawingPolicy::ShouldChangeTangentForKnot(UMaterialGraphNode_Knot* Knot)
{
	if (bool* pResult = KnotToReversedDirectionMap.Find(Knot))
	{
		return *pResult;
	}
	else
	{
		bool bPinReversed = false;

		FVector2D AverageLeftPin;
		FVector2D AverageRightPin;
		FVector2D CenterPin;
		bool bCenterValid = FindPinCenter(Knot->GetOutputPin(), /*out*/ CenterPin);
		bool bLeftValid = GetAverageConnectedPosition(Knot, EGPD_Input, /*out*/ AverageLeftPin);
		bool bRightValid = GetAverageConnectedPosition(Knot, EGPD_Output, /*out*/ AverageRightPin);

		if (bLeftValid && bRightValid)
		{
			bPinReversed = AverageRightPin.X < AverageLeftPin.X;
		}
		else if (bCenterValid)
		{
			if (bLeftValid)
			{
				bPinReversed = CenterPin.X < AverageLeftPin.X;
			}
			else if (bRightValid)
			{
				bPinReversed = AverageRightPin.X < CenterPin.X;
			}
		}

		KnotToReversedDirectionMap.Add(Knot, bPinReversed);

		return bPinReversed;
	}
}

void FMaterialGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;
	Params.WireColor = MaterialGraphSchema->ActivePinColor;

	UE::Shader::EValueType InputType = UE::Shader::EValueType::Void;
	UE::Shader::EValueType OutputType = UE::Shader::EValueType::Void;
	bool bInactivePin = false;
	bool bExecPin = false;
	bool bValueTypePin = false;

	// Have to consider both pins as the input will be an 'output' when previewing a connection
	if (OutputPin)
	{
		if (!MaterialGraph->IsInputActive(OutputPin))
		{
			bInactivePin = true;
		}

		if (OutputPin->PinType.PinCategory == UMaterialGraphSchema::PC_Exec)
		{
			bExecPin = true;
		}
		else if (OutputPin->PinType.PinCategory == UMaterialGraphSchema::PC_Void)
		{
			bValueTypePin = true;
		}
		else if (OutputPin->PinType.PinCategory == UMaterialGraphSchema::PC_ValueType)
		{
			OutputType = UE::Shader::FindValueType(OutputPin->PinType.PinSubCategory);
			bValueTypePin = true;
		}

		UEdGraphNode* OutputNode = OutputPin->GetOwningNode();
		if (UMaterialGraphNode_Knot* OutputKnotNode = Cast<UMaterialGraphNode_Knot>(OutputNode))
		{
			if (ShouldChangeTangentForKnot(OutputKnotNode))
			{
				Params.StartDirection = EGPD_Input;
			}
		}
		else if (!OutputNode->IsNodeEnabled() || OutputNode->IsDisplayAsDisabledForced() || OutputNode->IsNodeUnrelated())
		{
			bInactivePin = true;
		}
	}
	if (InputPin)
	{
		if (!MaterialGraph->IsInputActive(InputPin))
		{
			bInactivePin = true;
		}

		if (InputPin->PinType.PinCategory == UMaterialGraphSchema::PC_Exec)
		{
			bExecPin = true;
		}
		else if (InputPin->PinType.PinCategory == UMaterialGraphSchema::PC_Void)
		{
			bValueTypePin = true;
		}
		else if (InputPin->PinType.PinCategory == UMaterialGraphSchema::PC_ValueType)
		{
			InputType = UE::Shader::FindValueType(InputPin->PinType.PinSubCategory);
			bValueTypePin = true;
		}

		UEdGraphNode* InputNode = InputPin->GetOwningNode();
		if (UMaterialGraphNode_Knot* InputKnotNode = Cast<UMaterialGraphNode_Knot>(InputNode))
		{
			if (ShouldChangeTangentForKnot(InputKnotNode))
			{
				Params.EndDirection = EGPD_Output;
			}
		}
		else if (!InputNode->IsNodeEnabled() || InputNode->IsDisplayAsDisabledForced() || InputNode->IsNodeUnrelated())
		{
			bInactivePin = true;
		}
	}

	if (bInactivePin)
	{
		Params.WireColor = MaterialGraphSchema->InactivePinColor;
	}
	else if (bExecPin)
	{
		Params.WireColor = Settings->ExecutionPinTypeColor;
		Params.WireThickness = Settings->DefaultExecutionWireThickness;
	}
	else if (bValueTypePin)
	{
		// TODO - how to handle InputPin being nullptr, when does this occur?
		if (InputPin)
		{
			Params.WireColor = MaterialGraphSchema->GetPinTypeColor(InputPin->PinType);
		}
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin, InputPin, /*inout*/ Params.WireThickness, /*inout*/ Params.WireColor);
	}
}
