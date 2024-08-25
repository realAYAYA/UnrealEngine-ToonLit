// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "SGraphNode.h"

#include "CustomizableObjectNodeTexture.generated.h"

namespace ENodeTitleType { enum Type : int; }

class SOverlay;
class SVerticalBox;
class UCustomizableObjectNodeRemapPins;
class UCustomizableObjectNodeTexture;
class UObject;
class UTexture2D;
struct FGeometry;

// Class to render the Texture thumbnail of a CustomizableObjectNodeTexture
class SGraphNodeTexture : public SGraphNode
{
public:

	SLATE_BEGIN_ARGS(SGraphNodeTexture) {}
	SLATE_END_ARGS();

	SGraphNodeTexture() : SGraphNode() {};

	// Builds the SGraphNodeTexture when needed
	void Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode);

	// Calls the needed functions to build the SGraphNode widgets
	void UpdateGraphNode();

	// Overriden functions to build the SGraphNode widgets
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override;
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool ShouldAllowCulling() const override { return false; }

	// Callbacks for the widget
	void OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState);
	ECheckBoxState IsExpressionPreviewChecked() const;
	const FSlateBrush* GetExpressionPreviewArrow() const;
	EVisibility ExpressionPreviewVisibility() const;

public:

	// Pointer to the NodeTexture that owns this SGraphNode
	UCustomizableObjectNodeTextureBase* NodeTexture;

	// Single property that only draws the combo box widget of the Texture
	TSharedPtr<class ISinglePropertyView> TextureSelector;

	// Brush to draw the texture to the widget
	FSlateBrush TextureBrush;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTextureBase : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Texture2D)
	TObjectPtr<UTexture2D> Texture = nullptr;

	// Determines if the Node is collapsed or not
	bool bCollapsed = true;

	// Creates the SGraph Node widget for the thumbnail
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTexture : public UCustomizableObjectNodeTextureBase
{
public:
	GENERATED_BODY()

	// UCustomizableObjectNode interface
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;
	virtual void BackwardsCompatibleFixup() override;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
};
