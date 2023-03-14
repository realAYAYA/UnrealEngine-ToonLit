// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class FDMXPixelMappingOutputComponentModel;
class FDMXPixelMappingScreenComponentModel;
class FDMXPixelMappingToolkit;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingScreenComponent;

class SBorder;
class SBox;
class STextBlock;


/** Interface for Output Component Widgets */
class IDMXPixelMappingOutputComponentWidgetInterface
{
public:
	/** Destructor */
	virtual ~IDMXPixelMappingOutputComponentWidgetInterface();

	/** Adds the widgets to a canvas. If it already resides in a canvas it is removed from that first. */
	void AddToCanvas(const TSharedRef<SConstraintCanvas>& InCanvas);

	/** Removes the widget from the canvas, if it was added to one. */
	void RemoveFromCanvas();

	/** Returns the actual widget implementation */
	virtual TSharedRef<SWidget> AsWidget() = 0;

	/** Returns true if the widet equals the component */
	virtual bool Equals(UDMXPixelMappingBaseComponent* Component) const = 0;

protected:
	/** Returns the position of the widget */
	virtual FVector2D GetPosition() const = 0;

private:
	/** When added to a parent, the canvas it was added to */
	TSharedPtr<SConstraintCanvas> ParentCanvas;

	/** The canvas slot of the component widget */
	SConstraintCanvas::FSlot* Slot = nullptr;
};


/** Widget that draws an Output Component. For Screen Component, see SDMXPixelMappingScreenComponent */
class SDMXPixelMappingOutputComponent
	: public IDMXPixelMappingOutputComponentWidgetInterface
	, public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingOutputComponent)
	{}
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingOutputComponent> OutputComponent);

	//~ Begin IDMXPixelMappingOutputComponentWidgetInterface
	virtual TSharedRef<SWidget> AsWidget() override { return AsShared(); };
	virtual bool Equals(UDMXPixelMappingBaseComponent* Component) const override;
protected:
	virtual FVector2D GetPosition() const override;
	//~ End IDMXPixelMappingOutputComponentWidgetInterface

private:
	/** Updates the child slots */
	void UpdateChildSlots();

	/** Creates the child slot that displays the component name, above the child slot */
	void CreateComponentNameChildSlotAbove();

	/** Creates the child slot that displays the component name, inside the child slot */
	void CreateComponentNameChildSlotInside();

	/** Creates the child slot that displays the Fixture ID of the component */
	void CreateCellIDChildSlot();

	/** Creates the child slot that displays info about the patch such as Addresses or the Fixture ID */
	void CreatePatchInfoChildSlot();

	/** Brush for the outermost Bborder */
	FSlateBrush BorderBrush;

	/** The box that is shown */
	TSharedPtr<SBox> ComponentBox;

	/** The box that is shown */
	TSharedPtr<STextBlock> IDTextBlock;

	/** Border for content to display names above the widget */
	TSharedPtr<SBorder> AboveContentBorder;

	/** Border for content to display names in the top row of the widget */
	TSharedPtr<SBorder> TopContentBorder;

	/** Border for content to display names in the middle of the widget */
	TSharedPtr<SBorder> MiddleContentBorder;

	/** Border for content to display names in the top row of the widget */
	TSharedPtr<SBorder> BottomContentBorder;

	/** The model for this widget */
	TSharedPtr<FDMXPixelMappingOutputComponentModel> Model;

	/** The toolkit that owns */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
};

/** Widget that draws a Screen Component. */
class SDMXPixelMappingScreenComponent
	: public IDMXPixelMappingOutputComponentWidgetInterface
	, public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXPixelMappingScreenComponent)
	{}
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TSharedRef<FDMXPixelMappingToolkit>& InToolkit, TWeakObjectPtr<UDMXPixelMappingScreenComponent> ScreenComponent);

	//~ Begin IDMXPixelMappingOutputComponentWidgetInterface
	virtual TSharedRef<SWidget> AsWidget() override { return AsShared(); };
	virtual bool Equals(UDMXPixelMappingBaseComponent* Component) const override;
protected:
	virtual FVector2D GetPosition() const override;
	//~ End IDMXPixelMappingOutputComponentWidgetInterface

	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget interface

private:
	/** Updates the content of the widget */
	void UpdateContent();

	/** True if them model currently draws a simplistic view of the component */
	bool bDrawsSimplisticView = false;

	/** When added to a parent, the canvas it was added to */
	TSharedPtr<SConstraintCanvas> ParentCanvas;

	/** The canvas slot of the component widget */
	SConstraintCanvas::FSlot* Slot = nullptr;

	/** Brush for the outermost Bborder */
	FSlateBrush BorderBrush;

	/** Border that holds the content */
	TSharedPtr<SBorder> ContentBorder;

	/** The model for this widget */
	TSharedPtr<FDMXPixelMappingScreenComponentModel> Model;

	/** The toolkit that owns */
	TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
};
