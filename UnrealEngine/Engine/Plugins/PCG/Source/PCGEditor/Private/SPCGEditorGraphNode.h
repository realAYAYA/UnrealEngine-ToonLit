// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"

#include "PCGCommon.h"

struct FOverlayBrushInfo;

class UPCGEditorGraphNodeBase;

class SPCGEditorGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphNode){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UPCGEditorGraphNodeBase* InNode);
	void CreateAddPinButtonWidget();

	//~ Begin SGraphNode Interface
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetNodeBodyBrush() const override;
	virtual void RequestRenameOnSpawn() override { /* Empty to avoid the default behavior to rename on node spawn */ }
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual TSharedRef<SWidget> CreateTitleWidget(TSharedPtr<SNodeTitle> InNodeTitle) override;
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* Pin) const override;
	virtual EVisibility IsAddPinButtonVisible() const override;
	virtual FReply OnAddPin() override;
	//~ End SGraphNode Interface

	//~ Begin SNodePanel::SNode Interface
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	virtual void GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const override;
	//~ End SNodePanel::SNode Interface

protected:
	void OnNodeChanged();
	void OnNodeRenameInitiated();

	/** Set up node in 'compact' mode */
	void UpdateCompactNode();

private:
	static FLinearColor GetGridLabelColor(EPCGHiGenGrid NodeGrid);

	/**
	* Get the border brush for the given combination of grid sizes and enabled state. All a big workaround for FSlateRoundedBoxBrush not respecting
	* the tint colour.
	*/
	const FSlateBrush* GetBorderBrush(EPCGHiGenGrid InspectedGrid, EPCGHiGenGrid NodeGrid) const;

	UPCGEditorGraphNodeBase* PCGEditorGraphNode = nullptr;
};
