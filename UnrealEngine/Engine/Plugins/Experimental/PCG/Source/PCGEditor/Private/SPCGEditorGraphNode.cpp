// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphNode.h"

#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorStyle.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"

#include "SGraphPin.h"
#include "Styling/SlateBrush.h"

void SPCGEditorGraphNode::Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode)
{
	GraphNode = InNode;
	PCGEditorGraphNode = InNode;

	if (InNode)
	{
		InNode->OnNodeChangedDelegate.BindSP(this, &SPCGEditorGraphNode::OnNodeChanged);
	}

	UpdateGraphNode();
}

void SPCGEditorGraphNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	check(PCGEditorGraphNode);
	UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
	// Implementation note: we do not distinguish single/multiple pins on the output since that is not relevant
	if (PCGNode && PinToAdd->GetPinObj())
	{
		if (UPCGPin* Pin = PCGNode->GetInputPin(PinToAdd->GetPinObj()->PinName))
		{
			if (Pin->Properties.bAllowMultipleConnections)
			{
				PinToAdd->SetCustomPinIcon(FAppStyle::GetBrush("Graph.ArrayPin.Connected"), FAppStyle::GetBrush("Graph.ArrayPin.Disconnected"));
			}
		}
	}

	SGraphNode::AddPin(PinToAdd);
}

void SPCGEditorGraphNode::GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	check(PCGEditorGraphNode);
	
	const FSlateBrush* DebugBrush = FPCGEditorStyle::Get().GetBrush(TEXT("PCG.NodeOverlay.Debug"));
	const FSlateBrush* InspectBrush = FPCGEditorStyle::Get().GetBrush(TEXT("PCG.NodeOverlay.Inspect"));
	
	const FVector2D HalfDebugBrushSize = DebugBrush->GetImageSize() / 2.0;
	const FVector2D HalfInspectBrushSize = InspectBrush->GetImageSize() / 2.0;
	
	FVector2D OverlayOffset(0.0, 0.0);

	if (const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode())
	{
		if (PCGNode->DefaultSettings && PCGNode->DefaultSettings->ExecutionMode == EPCGSettingsExecutionMode::Debug)
		{
			FOverlayBrushInfo BrushInfo;
			BrushInfo.Brush = DebugBrush;
			BrushInfo.OverlayOffset = OverlayOffset - HalfDebugBrushSize;
			Brushes.Add(BrushInfo);
			
			OverlayOffset.Y += HalfDebugBrushSize.Y + HalfInspectBrushSize.Y;
		}
	}

	if (PCGEditorGraphNode->GetInspected())
	{
		FOverlayBrushInfo BrushInfo;
		BrushInfo.Brush = InspectBrush;
		BrushInfo.OverlayOffset = OverlayOffset - HalfInspectBrushSize;
		Brushes.Add(BrushInfo);	
	}
}

void SPCGEditorGraphNode::OnNodeChanged()
{
	UpdateGraphNode();
}
