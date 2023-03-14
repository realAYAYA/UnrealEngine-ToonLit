// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/Color.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "SGraphNode.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

#include "CustomizableObjectNodeTexture.generated.h"

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
	UCustomizableObjectNodeTexture* NodeTexture;

	// Single property that only draws the combo box widget of the Texture
	TSharedPtr<class ISinglePropertyView> TextureSelector;

	// Brush to draw the texture to the widget
	FSlateBrush TextureBrush;

};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeTexture : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TObjectPtr<UTexture2D> Texture = nullptr;

	// Begin EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;

	// UCustomizableObjectNode interface
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Creates the SGraph Node widget for the thumbnail
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	
	// Determines if the Node is collapsed or not
	bool bCollapsed = true;
};