// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SDMXPixelMappingSurface.h"

#include "DMXPixelMappingComponentReference.h"

#include "EditorUndoClient.h"
#include "Templates/SharedPointer.h"

class FDMXPixelMappingComponentReference;
class FDMXPixelMappingDragDropOp;
class FDMXPixelMappingToolkit;
class IDMXPixelMappingOutputComponentWidgetInterface;
class SDMXPixelMappingRuler;
class SDMXPixelMappingSourceTextureViewport;
class SDMXPixelMappingTransformHandle;
class UDMXPixelMappingBaseComponent;
class UDMXPixelMappingOutputComponent;
class UDMXPixelMappingRendererComponent;

class FHittestGrid;
struct FOptionalSize;
class SBorder;
class SBox;
class SCanvas;
class SConstraintCanvas;
class SOverlay;


class SDMXPixelMappingDesignerView
	: public SDMXPixelMappingSurface
	, public FEditorUndoClient
{
private:
	struct FComponentHitResult
	{
	public:
		TWeakObjectPtr<UDMXPixelMappingBaseComponent> Component;
		FArrangedWidget WidgetArranged;
		FName NamedSlot;

	public:
		FComponentHitResult()
			: WidgetArranged(SNullWidget::NullWidget, FGeometry())
		{}
	};
public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingDesignerView) { }
	SLATE_END_ARGS()

	/** Destructor */
	virtual ~SDMXPixelMappingDesignerView();

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit);

	/** The width of the preview screen for the UI */
	FOptionalSize GetPreviewAreaWidth() const;

	/** The height of the preview screen for the UI */
	FOptionalSize GetPreviewAreaHeight() const;

	/** Gets the scale of the preview screen */
	float GetPreviewScale() const;

protected:
	// Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	// End of SWidget interface

	//~ Begin SDMXPixelMappingSurface interface
	virtual FSlateRect ComputeAreaBounds() const override;
	virtual int32 GetGraphRulePeriod() const override;
	virtual float GetGridScaleAmount() const override;
	virtual int32 GetSnapGridSize() const override;
	//~ End SDMXPixelMappingSurface interface

	//~ Begin FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// End of FEditorUndoClient

private:
	/** Rebuilds the designer widget */
	void RebuildDesigner();

	/** Creates the extension widgets for the selected component */
	void CreateExtensionWidgetsForSelection();

	/** Clears the extension widgets */
	void ClearExtensionWidgets();

	/** Returns if the extension widget canvas should be visible */
	EVisibility GetExtensionCanvasVisibility() const;

	/** Gets the positioin of the extension widget */
	FVector2D GetExtensionPosition(TSharedPtr<SDMXPixelMappingTransformHandle> Handle);

	/** Gets the size of the extension widget */
	FVector2D GetExtensionSize(TSharedPtr<SDMXPixelMappingTransformHandle> Handle);

	/** Returns text for the currently hovered component */
	FText GetHoveredComponentNameText() const;

	/** Returns text for the currently hovered component's parent */
	FText GetHoveredComponentParentNameText() const;

	/** Returns the visibility of the side rulers */
	EVisibility GetRulerVisibility() const;

	/** Returns the cursor position as text */
	FText GetCursorPositionText() const;

	/** Returns the visibility of the cursor position text block */
	EVisibility GetCursorPositionTextVisibility() const;

	/** Returns the visibility of the ZoomPan widget */
	EVisibility GetZoomPanVisibility() const;

private:
	/** Called when zoom to fit was clicked */
	FReply OnZoomToFitClicked();

	/** Called when a component was added */
	void OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when a component was removed */
	void OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component);

	/** Called when the selected component in the toolkit changed */
	void OnSelectedComponentsChanged();

	/** Adds any pending selected Components to the selection set */
	void ResolvePendingSelectedComponents(bool bClearPreviousSelection = false);

	void PopulateWidgetGeometryCache(FArrangedWidget& Root);

	void PopulateWidgetGeometryCache_Loop(FArrangedWidget& Parent);

private:
	/** Returns the component under the cursor */
	UDMXPixelMappingOutputComponent* GetComponentUnderCursor() const;

	/** Returns the selected components */
	TSet<FDMXPixelMappingComponentReference> GetSelectedComponents() const;

	/** Returns the first best selected component */
	FDMXPixelMappingComponentReference GetSelectedComponent() const;

	/** Gets the Geometry of the component, returns true if successful */
	bool GetComponentGeometry(FDMXPixelMappingComponentReference InComponentReference, FGeometry& OutGeometry);

	/** Gets the Geometry of the component, returns true if successful */
	bool GetComponentGeometry(UDMXPixelMappingBaseComponent* InBaseComponent, FGeometry& OutGeometry);

	/** Returns the cursor position in graph space */
	bool GetGraphSpaceCursorPosition(FVector2D& OutGraphSpaceCursorPosition) const;

	void GetPreviewAreaAndSize(FVector2D& Area, FVector2D& Size) const;

	FGeometry GetDesignerGeometry() const;

	FGeometry MakeGeometryWindowLocal(const FGeometry& WidgetGeometry) const;

	/** Updates the drag drop op to use the patches selected in the Palette */
	void HandleDragEnterFromDetailsOrPalette(const TSharedPtr<FDMXPixelMappingDragDropOp>& DragDropOp);

private:
	/** The component that should be selected, but isn't selected yet */
	TWeakObjectPtr<UDMXPixelMappingBaseComponent> PendingSelectedComponent;

	/** The drag over DragDropOp that should be handled on the next tick*/
	TSharedPtr<FDMXPixelMappingDragDropOp> PendingDragDropOp;

	/** Canvas that holds the extension widget */
	TSharedPtr<SCanvas> ExtensionWidgetCanvas;

	/** Widget that holds the source texture */
	TSharedPtr<SDMXPixelMappingSourceTextureViewport> SourceTextureViewport;

	/** Canvas that holds the component widgets */
	TSharedPtr<SConstraintCanvas> DesignCanvas;

	TSharedPtr<SBox> PreviewSizeConstraint;

	TSharedPtr<SOverlay> PreviewHitTestRoot;

	/** Borer that contains the Design Canvas */
	TSharedPtr<SBorder> DesignCanvasBorder;

	TSharedPtr<FHittestGrid> HittestGrid;

	TMap<TWeakPtr<SWidget>, FArrangedWidget> CachedWidgetGeometry;

	TWeakObjectPtr<UDMXPixelMappingRendererComponent> CachedRendererComponent;

	/** The ruler bar at the top of the designer. */
	TSharedPtr<SDMXPixelMappingRuler> TopRuler;

	/** The ruler bar on the left side of the designer. */
	TSharedPtr<SDMXPixelMappingRuler> SideRuler;

	/** The position in local space where the user began dragging a widget */
	FVector2D DragAnchor;

	/** Oupput component views currently displayed */
	TArray<TSharedRef<IDMXPixelMappingOutputComponentWidgetInterface>> OutputComponentWidgets;

	TArray<TSharedPtr<SDMXPixelMappingTransformHandle>> TransformHandles;

	/** Helper class to restore selection on scope */
	class FScopedRestoreSelection
	{
	public:
		FScopedRestoreSelection(TSharedRef<FDMXPixelMappingToolkit> ToolkitPtr, TSharedRef<SDMXPixelMappingDesignerView> DesignerView);
		~FScopedRestoreSelection();

	private:
		TWeakPtr<FDMXPixelMappingToolkit> WeakToolkit;
		TWeakPtr<SDMXPixelMappingDesignerView> WeakDesignerView;
		TArray<TWeakObjectPtr<UDMXPixelMappingBaseComponent>> CachedSelectedComponents;
	};
};
