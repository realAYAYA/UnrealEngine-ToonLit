// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Views/OutputMapping/GraphNodes/SDisplayClusterConfiguratorBaseNode.h"

#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class IDisplayClusterConfiguratorOutputMappingSlot;
class FDisplayClusterConfiguratorBlueprintEditor;
class FDisplayClusterConfiguratorOutputMappingBuilder;
class UDisplayClusterConfiguratorCanvasNode;
class UDisplayClusterConfigurationCluster;

class SDisplayClusterConfiguratorCanvasNode
	: public SDisplayClusterConfiguratorBaseNode
{
public:
	SLATE_BEGIN_ARGS(SDisplayClusterConfiguratorCanvasNode)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UDisplayClusterConfiguratorCanvasNode* InNode, const TSharedRef<FDisplayClusterConfiguratorBlueprintEditor>& InToolkit);

	//~ SGraphNode interface
	virtual void UpdateGraphNode() override;
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty = true) override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FVector2D GetPosition() const override;
	virtual TArray<FOverlayWidgetInfo> GetOverlayWidgets(bool bSelected, const FVector2D& WidgetSize) const override;
	//~ End SGraphNode interface

	//~ Begin SDisplayClusterConfiguratorBaseNode interface
	virtual bool CanNodeBeResized() const { return false; }
	//~ End of SDisplayClusterConfiguratorBaseNode interface

private:
	const FSlateBrush* GetSelectedBrush() const;
	FMargin GetBackgroundPosition() const;
	FText GetCanvasSizeText() const;

private:
	TSharedPtr<SWidget> CanvasSizeTextWidget;

	FMargin CanvasPadding;
};
