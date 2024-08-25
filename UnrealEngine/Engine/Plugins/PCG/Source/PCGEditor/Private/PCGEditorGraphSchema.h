// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "ConnectionDrawingPolicy.h"

#include "PCGEditorGraphSchema.generated.h"

enum class EPCGElementType : uint8;

class UPCGEditorGraph;

UCLASS()
class UPCGEditorGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	void GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder, const EPCGElementType InPCGElementTypeFilter) const;

	//~ Begin EdGraphSchema Interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	//~ End EdGraphSchema Interface

private:
	void GetNativeElementActions(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph = nullptr) const;
	void GetBlueprintElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;
	void GetSubgraphElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;
	void GetSettingsElementActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bIsContextual) const;
	void GetExtraElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;
	void GetNamedRerouteUsageActions(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph = nullptr) const;
	void GetDataAssetActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;

	virtual bool TryCreateConnectionInternal(UEdGraphPin* A, UEdGraphPin* B, bool bAddConversionNodeIfNeeded) const;
};

class FPCGEditorConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FPCGEditorConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraph);
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override;

protected:
	bool UpdateParamsIfDebugging(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params);

	UPCGEditorGraph* Graph;
};
