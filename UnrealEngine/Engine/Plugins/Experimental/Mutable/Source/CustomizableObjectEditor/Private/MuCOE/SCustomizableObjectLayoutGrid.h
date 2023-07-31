// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Input/CursorReply.h"
#include "Input/Reply.h"
#include "Math/Color.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
struct FGeometry;
struct FGuid;
struct FKeyEvent;
struct FPointerEvent;
//#include "PreviewScene.h"


struct FRect2D
{
	FVector2f Min;
	FVector2f Size;
};

typedef enum
{
	ELGM_Show,
	ELGM_Edit,
	ELGM_Select
} ELayoutGridMode;

struct FBlockWidgetData
{
	FRect2D Rect;
	FRect2D HandleRect;
};

/** */
class SCustomizableObjectLayoutGrid : public SCompoundWidget
{

public:
	DECLARE_DELEGATE_TwoParams(FBlockChangedDelegate, FGuid /*BlockId*/, FIntRect /*Block*/);
	DECLARE_DELEGATE_OneParam(FBlockSelectionChangedDelegate, const TArray<FGuid>& );
	DECLARE_DELEGATE(FDeleteBlockDelegate);
	DECLARE_DELEGATE_TwoParams(FAddBlockAtDelegate, FIntPoint, FIntPoint);
	DECLARE_DELEGATE_OneParam(FSetBlockPriority, int32);

	SLATE_BEGIN_ARGS( SCustomizableObjectLayoutGrid ){}

		SLATE_ATTRIBUTE( FIntPoint, GridSize )
		SLATE_ATTRIBUTE( TArray<FCustomizableObjectLayoutBlock>, Blocks )
		SLATE_ARGUMENT( TArray<FVector2f>, UVLayout  )
		SLATE_ARGUMENT( TArray<FVector2f>, UnassignedUVLayoutVertices )
		SLATE_ARGUMENT( ELayoutGridMode, Mode  )
		SLATE_ARGUMENT( FColor, SelectionColor  )
		SLATE_EVENT( FBlockChangedDelegate, OnBlockChanged )
		SLATE_EVENT( FBlockSelectionChangedDelegate, OnSelectionChanged )
		SLATE_EVENT(FDeleteBlockDelegate, OnDeleteBlocks)
		SLATE_EVENT(FAddBlockAtDelegate, OnAddBlockAt)
		SLATE_EVENT(FSetBlockPriority, OnSetBlockPriority)

	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );
	~SCustomizableObjectLayoutGrid();

	// SWidgetInterface
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	bool SupportsKeyboardFocus() const override { return true; }
	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	FVector2D ComputeDesiredSize(float) const override;

	// Own interface

	/** Set the currently selected block */
	void SetSelectedBlock( FGuid block );
	void SetSelectedBlocks( const TArray<FGuid>& blocks );

	/** */
	TArray<FGuid> GetSelectedBlocks() const;

	/** Calls the delegate to delete the selected blocks */
	void DeleteSelectedBlocks();

	/** Generates a new block at mouse position */
	void GenerateNewBlock(FVector2D MousePosition);

	/** Duplicates the selected blocks */
	void DuplicateBlocks();

	/** Sets the size of the selected blocks to the size of the Grid */
	void SetBlockSizeToMax();

	void CalculateSelectionRect();

	FColor SelectionColor;

	/** Set the grid and blocks to show in the widget. */
	void SetBlocks( const FIntPoint& GridSize, const TArray<FCustomizableObjectLayoutBlock>& Blocks);

	/** Gets the priority value of the selected blocks */
	TOptional<int32> GetBlockPriortyValue() const;

	/** Callback when the priority of a block changes */
	void OnBlockPriorityChanged(int32 InValue);

	/** Sets the packing strategy of the current layout */
	void SetLayoutStrategy(ECustomizableObjectTextureLayoutPackingStrategy Strategy);

private:

	bool MouseOnBlock(FGuid BlockId, FVector2D MousePosition, bool CheckResizeBlock = false) const;

private:

	/** A delegate to report block changes */
	FBlockChangedDelegate BlockChangedDelegate;
	FBlockSelectionChangedDelegate SelectionChangedDelegate;
	FDeleteBlockDelegate DeleteBlocksDelegate;
	FAddBlockAtDelegate AddBlockAtDelegate;
	FSetBlockPriority OnSetBlockPriority;

	/** Size of the grid in blocks */
	TAttribute<FIntPoint> GridSize;

	/** Array with all the blocks of the layout */
	TAttribute< TArray<FCustomizableObjectLayoutBlock> > Blocks;

	/** Array with all the UVs to draw in the layout */
	TArray<FVector2f> UVLayout;

	/** Array with all the unassigned UVs */
	TArray<FVector2f> UnassignedUVLayoutVertices;

	/** Layout mode */
	ELayoutGridMode Mode;

	float CellSize;

	/** Map to relate Block ids with blocks data */
	TMap<FGuid,FBlockWidgetData> BlockRects;

	/** Interaction status. */
	TArray<FGuid> SelectedBlocks;
	TArray<FGuid> PossibleSelectedBlocks;

	/** Bools needed for the Block Management */
	/** Indicates when we have dragged the mouse after click */
	bool HasDragged;

	/** Indicates when we are dragging the mouse */
	bool Dragging;
	
	/** Indicates when we are resizing a block */
	bool Resizing;
	
	/** Indicates when we have to change the mouse cursor */
	bool ResizeCursor;
	
	/** Indicates when we are making a selection */
	bool Selecting;
	
	/** Indicates when we are padding */
	bool Padding;

	/** Position where the drag started */
	FVector2D DragStart;

	/** Position where the layout grid starts to be drawn */
	FVector2D DrawOrigin;

	/** Ammount of padding since start draging */
	FVector2D PaddingAmount;

	/** Position where the padding started */
	FVector2D PaddingStart;

	/** Distance from the origin in the padding movement */
	FVector2D DistanceFromOrigin;

	/** Level of zoom */
	int32 Zoom;

	/** Selection Rectangle */
	FRect2D SelectionRect;

	/** Position where the Selection Rectangle started */
	FVector2D InitSelectionRect;

	/** Current mouse position */
	FVector2D CurrentMousePosition;

	/** Custom Slate drawing element. Used to improve the UVs drawing performance. */
	TSharedPtr<class FUVCanvasDrawer, ESPMode::ThreadSafe> UVCanvasDrawer;

	/** Current packing strategy of the represented layout */
	ECustomizableObjectTextureLayoutPackingStrategy LayoutStrategy;
};
