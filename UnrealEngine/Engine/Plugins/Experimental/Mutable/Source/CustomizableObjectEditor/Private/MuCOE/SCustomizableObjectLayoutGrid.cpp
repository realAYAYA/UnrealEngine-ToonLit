// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectLayoutGrid.h"

#include "BatchedElements.h"
#include "CanvasTypes.h"
#include "Engine/World.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GenericPlatform/GenericApplication.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "HitProxies.h"
#include "Input/CursorReply.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Geometry.h"
#include "Layout/PaintGeometry.h"
#include "Layout/WidgetPath.h"
#include "Math/Box2D.h"
#include "Math/IntRect.h"
#include "Math/Vector.h"
#include "Misc/Guid.h"
// Required for engine branch preprocessor defines.
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "RHI.h"
#include "Rendering/DrawElements.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderTransform.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Textures/SlateIcon.h"
#include "Types/SlateStructs.h"
#include "UnrealClient.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"

class FExtender;
class FPaintArgs;
class FRHICommandListImmediate;
class FSlateRect;
class FWidgetStyle;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

/** Simple representation of the backbuffer for drawin UVs. */
class FSlateCanvasRenderTarget : public FRenderTarget
{
public:
	FIntPoint GetSizeXY() const override
	{
		return ViewRect.Size();
	}

	/** Sets the texture that this target renders to */
	void SetRenderTargetTexture(FTexture2DRHIRef& InRHIRef)
	{
		RenderTargetTextureRHI = InRHIRef;
	}

	/** Clears the render target texture */
	void ClearRenderTargetTexture()
	{
		RenderTargetTextureRHI.SafeRelease();
	}

	/** Sets the viewport rect for the render target */
	void SetViewRect(const FIntRect& InViewRect)
	{
		ViewRect = InViewRect;
	}

	/** Gets the viewport rect for the render target */
	const FIntRect& GetViewRect() const
	{
		return ViewRect;
	}

	/** Sets the clipping rect for the render target */
	void SetClippingRect(const FIntRect& InClippingRect)
	{
		ClippingRect = InClippingRect;
	}

	/** Gets the clipping rect for the render target */
	const FIntRect& GetClippingRect() const
	{
		return ClippingRect;
	}

private:

	FIntRect ViewRect;
	FIntRect ClippingRect;
};


/** Custom Slate drawing element. Holds a copy of all information required to draw UVs. */
class FUVCanvasDrawer : public ICustomSlateElement
{
public:
	~FUVCanvasDrawer()
	{
		delete RenderTarget;
	};

	/** Set the canvas area and all required data to paint the UVs.
	 * 
	 * All data will be copied.
	 */
	void Initialize(const FIntRect& InCanvasRect, const FIntRect& InClippingRect, const FVector2D& InOrigin, const FVector2D& InSize, const FIntPoint& InGridSize, const float InCellSize);
	void InitializeDrawingData(const TArray<FVector2f>& InUVLayout, const TArray<FVector2f>& InUnassignedUVs, const TArray<FCustomizableObjectLayoutBlock>& InBlocks, const TArray<FGuid>& InSelectedBlocks);

	/** Sets the layout mode to know what to draw */
	void SetLayoutMode(ELayoutGridMode Mode);

private:

	void DrawRenderThread(FRHICommandListImmediate& RHICmdList, const void* RenderTarget) override;

	/** Basic function to draw a block in the canvas */
	void DrawBlock(FBatchedElements* BatchedElements, const FHitProxyId HitProxyId, const FRect2D& BlockRect, FColor Color);

	/** SlateElement initialized, can Draw during the DrawRenderThread call. */
	bool Initialized = false;

	/** Drawing origin. */
	FVector2D Origin;

	/** Drawing size. */
	FVector2D Size;

	/** Size of the Layout Grid */
	FIntPoint GridSize;

	/** Cell Size */
	float CellSize;

	/** Drawing Data. */
	TArray<FVector2D> UVLayout;
	TArray<FVector2D> UnassignedUVs;
	TArray<FCustomizableObjectLayoutBlock> Blocks;
	TArray<FGuid> SelectedBlocks;

	/** Layout Mode */
	ELayoutGridMode LayoutMode;

	FSlateCanvasRenderTarget* RenderTarget = new FSlateCanvasRenderTarget();
};


void SCustomizableObjectLayoutGrid::Construct( const FArguments& InArgs )
{
	GridSize = InArgs._GridSize;
	Blocks = InArgs._Blocks;
	UVLayout = InArgs._UVLayout;
	UnassignedUVLayoutVertices = InArgs._UnassignedUVLayoutVertices;
	Mode = InArgs._Mode;
	BlockChangedDelegate = InArgs._OnBlockChanged;
	SelectionChangedDelegate = InArgs._OnSelectionChanged;
	SelectionColor = InArgs._SelectionColor;
	DeleteBlocksDelegate = InArgs._OnDeleteBlocks;
	AddBlockAtDelegate = InArgs._OnAddBlockAt;
	OnSetBlockPriority = InArgs._OnSetBlockPriority;

	HasDragged = false;
	Dragging = false;
	Resizing = false;
	ResizeCursor = false;
	Selecting = false;

	PaddingAmount = FVector2D::Zero();
	DistanceFromOrigin = FVector2D::Zero();
	Zoom = 1;

	UVCanvasDrawer = TSharedPtr<FUVCanvasDrawer, ESPMode::ThreadSafe>(new FUVCanvasDrawer());
	UVCanvasDrawer->SetLayoutMode(Mode);
}


SCustomizableObjectLayoutGrid::~SCustomizableObjectLayoutGrid()
{
	// UVCanvasDrawer can only be destroyed after drawing the last command
	ENQUEUE_RENDER_COMMAND(SafeDeletePreviewElement)(
		[UVCanvasDrawer = UVCanvasDrawer](FRHICommandListImmediate& RHICmdList) mutable
		{
			UVCanvasDrawer.Reset();
		}
	);
}


int32 SCustomizableObjectLayoutGrid::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 RetLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, LayerId,InWidgetStyle, bParentEnabled );

	bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	// Paint inside the border only. 
	const FVector2D BorderPadding = FVector2D(2,2);
	FPaintGeometry ForegroundPaintGeometry = AllottedGeometry.ToInflatedPaintGeometry( -BorderPadding );
	
	const FIntPoint GridSizePoint = GridSize.Get();
	const float OffsetX = BorderPadding.X;
	const FVector2D AreaSize =  AllottedGeometry.GetLocalSize() - 2.0f * BorderPadding;
	const float GridRatio = float(GridSizePoint.X) / float(GridSizePoint.Y);
	FVector2D Size;
	if ( AreaSize.X/GridRatio > AreaSize.Y )
	{
		Size.Y = AreaSize.Y;
		Size.X = AreaSize.Y*GridRatio;
	}
	else
	{
		Size.X =  AreaSize.X;
		Size.Y =  AreaSize.X/GridRatio;
	}

	FVector2D OldSize = Size;
	Size *= Zoom;

	float AuxCellSize = Size.X / GridSizePoint.X;
	
	// Drawing Offsets
	FVector2D Offset = FVector2D((AreaSize - Size).X / 2.0f, 0.0f);
	FVector2D ZoomOffset = ((Size - OldSize) / 2.0f);
	
	// Drawing Origin
	FVector2D Origin = BorderPadding + Offset + PaddingAmount - DistanceFromOrigin;

	// Setting Canvas Drawing Rectangles
	FSlateRect SlateCanvasRect = AllottedGeometry.GetLayoutBoundingRect();
	FSlateRect ClippedCanvasRect = SlateCanvasRect.IntersectionWith(MyClippingRect);

	FIntRect CanvasRect(
		FMath::TruncToInt(FMath::Max(0.0f, SlateCanvasRect.Left)),
		FMath::TruncToInt(FMath::Max(0.0f, SlateCanvasRect.Top)),
		FMath::TruncToInt(FMath::Max(0.0f, SlateCanvasRect.Right)),
		FMath::TruncToInt(FMath::Max(0.0f, SlateCanvasRect.Bottom)));

	FIntRect ClippingRect(
		FMath::TruncToInt(FMath::Max(0.0f, ClippedCanvasRect.Left)),
		FMath::TruncToInt(FMath::Max(0.0f, ClippedCanvasRect.Top)),
		FMath::TruncToInt(FMath::Max(0.0f, ClippedCanvasRect.Right)),
		FMath::TruncToInt(FMath::Max(0.0f, ClippedCanvasRect.Bottom)));

	
	UVCanvasDrawer->InitializeDrawingData(UVLayout, UnassignedUVLayoutVertices, Blocks.Get(), SelectedBlocks);
	UVCanvasDrawer->Initialize(CanvasRect, ClippingRect, Origin * AllottedGeometry.Scale, Size * AllottedGeometry.Scale, GridSizePoint, AuxCellSize * AllottedGeometry.Scale);
	FSlateDrawElement::MakeCustom(OutDrawElements, RetLayerId, UVCanvasDrawer);

	const auto MakeYellowSquareLine = [&](const TArray<FVector2D>& Points) -> void
	{
		FSlateDrawElement::MakeLines(OutDrawElements, RetLayerId, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, FColor(250, 230, 43, 255), true, 2.0);
	};

	// Drawing Multi-Selection rect
	if (Mode == ELGM_Edit && Selecting)
	{
		TArray<FVector2D> SelectionSquarePoints;
		SelectionSquarePoints.SetNum(2);

		FVector2D RectMin = FVector2D(SelectionRect.Min);
		FVector2D RectSize = FVector2D(SelectionRect.Size);

		FVector2D TopLeft = RectMin;
		FVector2D TopRight = RectMin + FVector2D(RectSize.X, 0.0f);
		FVector2D BottomRight = RectMin + RectSize;
		FVector2D BottomLeft = RectMin + FVector2D(0.0f, RectSize.Y);

		SelectionSquarePoints[0] = TopLeft;
		SelectionSquarePoints[1] = TopRight;
		MakeYellowSquareLine(SelectionSquarePoints);

		SelectionSquarePoints[0] = BottomRight;
		MakeYellowSquareLine(SelectionSquarePoints);

		SelectionSquarePoints[1] = BottomLeft;
		MakeYellowSquareLine(SelectionSquarePoints);

		SelectionSquarePoints[0] = TopLeft;
		MakeYellowSquareLine(SelectionSquarePoints);
	}

	RetLayerId++;

	return RetLayerId - 1;
}


void SCustomizableObjectLayoutGrid::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	const FVector2D BorderPadding = FVector2D(2,2);
	const FVector2D AreaSize =  AllottedGeometry.Size - 2.0f * BorderPadding;
	const float GridRatio = float(GridSize.Get().X)/float(GridSize.Get().Y);
	FVector2D Size;
	if ( AreaSize.X/GridRatio > AreaSize.Y )
	{
		Size.Y = AreaSize.Y;
		Size.X = AreaSize.Y*GridRatio;
	}
	else
	{
		Size.X =  AreaSize.X;
		Size.Y =  AreaSize.X/GridRatio;
	}

	FVector2D OldSize = Size;
	Size *= Zoom;
	CellSize = Size.X/GridSize.Get().X;
	FVector2D Offset = FVector2D((AreaSize - Size).X / 2.0f, 0.0f);
	FVector2D ZoomOffset = (Size - OldSize) / 2.0f;
	FVector2D Origin = BorderPadding + Offset + PaddingAmount - DistanceFromOrigin;
	DrawOrigin = Origin;

	BlockRects.Empty();

	const TArray<FCustomizableObjectLayoutBlock>& CurrentBlocks = Blocks.Get();
	for (const FCustomizableObjectLayoutBlock& Block : CurrentBlocks)
	{
		const FVector2f BlockMin(Block.Min);
		const FVector2f BlockMax(Block.Max);

		FBlockWidgetData BlockData;
		BlockData.Rect.Min = FVector2f(Origin) + BlockMin * CellSize + CellSize * 0.1f;
		BlockData.Rect.Size = (BlockMax - BlockMin) * CellSize - CellSize * 0.2f;

		float HandleRectSize = FMath::Log2(float(GridSize.Get().X))/10.0f;
		BlockData.HandleRect.Size = FVector2f(CellSize) * HandleRectSize;
		BlockData.HandleRect.Min = BlockData.Rect.Min + BlockData.Rect.Size - BlockData.HandleRect.Size;

		BlockRects.Add(Block.Id, BlockData);
	}

	// Update selection list
	for (int i=0; i<SelectedBlocks.Num();)
	{
		bool Found = false;
		for (const FCustomizableObjectLayoutBlock& Block : CurrentBlocks)
		{
			if (Block.Id == SelectedBlocks[i])
			{
				Found = true;
			}
		}

		if ( !Found )
		{
			SelectedBlocks.RemoveAt(i);
		}
		else
		{
			++i;
		}
	}

	if (Selecting)
	{
		CalculateSelectionRect();
	}

	SCompoundWidget::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}


FReply SCustomizableObjectLayoutGrid::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (Mode == ELGM_Edit)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			HasDragged = false;
			Dragging = false;
			Resizing = false;

			// To know if we clicked on a block
			bool ClickOnBlock = false;

			FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			InitSelectionRect = Pos;

			//Reset Selection Rect
			SelectionRect.Size = FVector2f::Zero();
			SelectionRect.Min = FVector2f(Pos);

			// Handles selection must be detected on mouse down
			// We also check if we click on a block
			TArray<FGuid> SelectedBlockHandles;

			for (const FGuid& BlockId : SelectedBlocks)
			{
				if (MouseOnBlock(BlockId, Pos, true))
				{
					SelectedBlockHandles.Add(BlockId);
				}

				if (MouseOnBlock(BlockId, Pos))
				{
					if (SelectedBlocks.Contains(BlockId))
					{
						ClickOnBlock = true;
					}
				}
			}

			if (SelectedBlocks.Num() && ClickOnBlock)
			{
				Dragging = true;
				DragStart = Pos;

				if (SelectedBlocks.Num() == 1 && SelectedBlockHandles.Contains(SelectedBlocks[0]))
				{
					Resizing = true;
				}
			}
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		{
			//Mouse position
			FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			FVector2D CellDelta = (Pos - DrawOrigin) / CellSize;

			// Create context menu
			const bool CloseAfterSelection = true;
			FMenuBuilder MenuBuilder(CloseAfterSelection, NULL, TSharedPtr<FExtender>(), false, &FCoreStyle::Get(), false);

			MenuBuilder.BeginSection("Block Management", LOCTEXT("GridActionsTitle", "Grid Actions"));
			{
				if (SelectedBlocks.Num())
				{
					FUIAction DeleteAction(FExecuteAction::CreateSP(this, &SCustomizableObjectLayoutGrid::DeleteSelectedBlocks));
					MenuBuilder.AddMenuEntry(LOCTEXT("DeleteBlocksLabel", "Delete"), LOCTEXT("DeleteBlocksTooltip", "Delete Selected Blocks"), FSlateIcon(), DeleteAction);

					FUIAction DuplicateAction(FExecuteAction::CreateSP(this, &SCustomizableObjectLayoutGrid::DuplicateBlocks));
					MenuBuilder.AddMenuEntry(LOCTEXT("DuplicateBlocksLabel", "Duplicate"), LOCTEXT("DuplicateBlocksTooltip", "Duplicate Selected Blocks"), FSlateIcon(), DuplicateAction);
				}
				else
				{
					FUIAction AddNewBlockAction(FExecuteAction::CreateSP(this, &SCustomizableObjectLayoutGrid::GenerateNewBlock, CellDelta));
					MenuBuilder.AddMenuEntry(LOCTEXT("AddNewBlockLabel", "Add Block"), LOCTEXT("AddNewBlockTooltip", "Add New Block"), FSlateIcon(), AddNewBlockAction);
				}
			}
			MenuBuilder.EndSection();

			MenuBuilder.BeginSection("Fixed Layout Strategy", LOCTEXT("BlockActionsTitle", "Fixed Layout Actions"));
			{
				if (SelectedBlocks.Num() && LayoutStrategy == ECustomizableObjectTextureLayoutPackingStrategy::Fixed)
				{
					MenuBuilder.AddWidget(
						SNew(SBox)
						.WidthOverride(125.0f)
						[
							SNew(SNumericEntryBox<int32>)
							.MinValue(0)
							.MaxValue(INT_MAX)
							.MaxSliderValue(100)
							.AllowSpin(SelectedBlocks.Num() == 1)
							.Value(this, &SCustomizableObjectLayoutGrid::GetBlockPriortyValue)
							.UndeterminedString(LOCTEXT("MultipleValues", "Multiples Values"))
							.OnValueChanged(this, &SCustomizableObjectLayoutGrid::OnBlockPriorityChanged)
							.ToolTipText(LOCTEXT("SetBlockPriorityTooltip", "Sets the block priority for a Fixed Layout Strategy"))
							.EditableTextBoxStyle(&FAppStyle::GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
						]
					, FText::FromString("Block Priority"), true);
				}
			}
			MenuBuilder.EndSection();

			FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
		else if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
		{
			if (Zoom == 2)
			{
				Padding = true;
				PaddingStart = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			}
		}
	}

	return SCompoundWidget::OnMouseButtonDown( MyGeometry, MouseEvent );
}


FReply SCustomizableObjectLayoutGrid::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (Mode == ELGM_Show)
	{
		return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
	}

	if ( MouseEvent.GetEffectingButton()==EKeys::LeftMouseButton)
	{
		Dragging = false;
		Resizing = false;

		// Left Shif is pressed for multi selection
		bool bLeftShift = MouseEvent.GetModifierKeys().IsLeftShiftDown();

		// Screen to Widget Position
		FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		
		// Selection before reset
		TArray<FGuid> OldSelection = SelectedBlocks;
		TArray<FGuid> OldPossibleSelection = PossibleSelectedBlocks;

		PossibleSelectedBlocks.Reset();

		// Reset selection if multi selection is not enabled
		if (Mode == ELGM_Edit && !bLeftShift && !HasDragged)
		{
			// Only one selected block allowed in edit mode.
			SelectedBlocks.Reset();
		}

		if (!Selecting)
		{
			if (!HasDragged)
			{
				//Backward iteration to select the block rendered in front of the rest
				const TArray<FCustomizableObjectLayoutBlock>& CurrentBlocks = Blocks.Get();
				for (int32 i = CurrentBlocks.Num() - 1; i > -1; --i)
				{
					if (MouseOnBlock(CurrentBlocks[i].Id, Pos))
					{
						PossibleSelectedBlocks.Add(CurrentBlocks[i].Id);
					}
				}

				bool bSameSelection = PossibleSelectedBlocks == OldPossibleSelection;

				for (int32 i = 0; i < PossibleSelectedBlocks.Num(); ++i)
				{
					if (bLeftShift || Mode == ELGM_Select)
					{
						if (PossibleSelectedBlocks.Num() == 1)
						{
							if (SelectedBlocks.Contains(PossibleSelectedBlocks[i]))
							{
								SelectedBlocks.Remove(PossibleSelectedBlocks[i]);
							}
							else
							{
								SelectedBlocks.Add(PossibleSelectedBlocks[i]);
								break;
							}
						}
						else
						{
							if (!SelectedBlocks.Contains(PossibleSelectedBlocks[i]))
							{
								SelectedBlocks.Add(PossibleSelectedBlocks[i]);
								break;
							}
						}
					}
					else
					{
						if (OldSelection.Num() == 0)
						{
							SelectedBlocks.Add(PossibleSelectedBlocks[0]);
						}

						if (bSameSelection)
						{
							if (OldSelection.Contains(PossibleSelectedBlocks[i]))
							{
								SelectedBlocks.Remove(PossibleSelectedBlocks[i]);

								if (i == PossibleSelectedBlocks.Num() - 1)
								{
									SelectedBlocks.Add(PossibleSelectedBlocks[0]);
									break;
								}
								else
								{
									SelectedBlocks.Add(PossibleSelectedBlocks[i + 1]);
								}
							}
						}
						else
						{
							if (OldSelection.Contains(PossibleSelectedBlocks[i]) && PossibleSelectedBlocks.Num() > 1)
							{
								SelectedBlocks.Remove(PossibleSelectedBlocks[i]);
							}
							else
							{
								SelectedBlocks.AddUnique(PossibleSelectedBlocks[i]);
								break;
							}
						}
					}
				}
			}
		}
		else
		{
			FBox2D SelectRect(FVector2D(SelectionRect.Min), FVector2D(SelectionRect.Min + SelectionRect.Size) );
			
			const TArray<FCustomizableObjectLayoutBlock>& CurrentBlocks = Blocks.Get();
			for (int32 i = 0; i < CurrentBlocks.Num(); ++i)
			{
				FBox2D CurrentBlock(FVector2D(BlockRects[CurrentBlocks[i].Id].Rect.Min), FVector2D(BlockRects[CurrentBlocks[i].Id].Rect.Min + BlockRects[CurrentBlocks[i].Id].Rect.Size));
				
				if (SelectedBlocks.Contains(CurrentBlocks[i].Id))
				{
					if (!SelectRect.Intersect(CurrentBlock) && !bLeftShift)
					{
						SelectedBlocks.Remove(CurrentBlocks[i].Id);
					}
				}
				else
				{
					if (SelectRect.Intersect(CurrentBlock))
					{
						SelectedBlocks.Add(CurrentBlocks[i].Id);
					}
				}
			}
		}

		// Executing selection delegate
		if (OldSelection != SelectedBlocks)
		{
			SelectionChangedDelegate.ExecuteIfBound(SelectedBlocks);
		}

		HasDragged = false;
		Selecting = false;
	}
	else if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		Padding = false;
	}

	return SCompoundWidget::OnMouseButtonUp( MyGeometry, MouseEvent );
}


FReply SCustomizableObjectLayoutGrid::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	CurrentMousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

	if (Mode != ELGM_Edit)
	{
		return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
	}

	if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

		if (Dragging && SelectedBlocks.Num())
		{
			FVector2D CellDelta = (Pos - DragStart) / CellSize;

			int CellDeltaX = CellDelta.X;
			int CellDeltaY = CellDelta.Y;

			DragStart += FVector2D(CellDeltaX * CellSize, CellDeltaY * CellSize);

			if (CellDeltaX || CellDeltaY)
			{
				HasDragged = true;

				const TArray<FCustomizableObjectLayoutBlock>& CurrentBlocks = Blocks.Get();

				if (!Resizing)
				{
					FIntRect TotalBlock;
					bool bFirstBlock = true;

					for (const FCustomizableObjectLayoutBlock& B : CurrentBlocks)
					{
						FIntRect Block(B.Min, B.Max);

						if (SelectedBlocks.Contains(B.Id))
						{
							if (bFirstBlock)
							{
								TotalBlock = Block;
								bFirstBlock = false;
							}

							TotalBlock.Min.X = FMath::Min(TotalBlock.Min.X, Block.Min.X);
							TotalBlock.Min.Y = FMath::Min(TotalBlock.Min.Y, Block.Min.Y);
							TotalBlock.Max.X = FMath::Max(TotalBlock.Max.X, Block.Max.X);
							TotalBlock.Max.Y = FMath::Max(TotalBlock.Max.Y, Block.Max.Y);
						}
					}

					FIntPoint Grid = GridSize.Get();
					FIntRect BlockMovement = TotalBlock;
					BlockMovement.Min.X = FMath::Max(0, FMath::Min(TotalBlock.Min.X + CellDeltaX, Grid.X - TotalBlock.Size().X));
					BlockMovement.Min.Y = FMath::Max(0, FMath::Min(TotalBlock.Min.Y + CellDeltaY, Grid.Y - TotalBlock.Size().Y));

					BlockMovement.Max = BlockMovement.Min + TotalBlock.Size();

					FIntRect AddMovement = BlockMovement - TotalBlock;

					for (const FCustomizableObjectLayoutBlock& B : CurrentBlocks)
					{
						if (SelectedBlocks.Find(B.Id) != INDEX_NONE)
						{
							FIntRect ResultBlock(B.Min, B.Max);
							ResultBlock.Max += AddMovement.Max;
							ResultBlock.Min += AddMovement.Min;

							BlockChangedDelegate.ExecuteIfBound(B.Id, ResultBlock);
						}
					}
				}
				else
				{
					for (const FCustomizableObjectLayoutBlock& B : CurrentBlocks)
					{
						FIntRect Block;
						for (const FGuid& Id : SelectedBlocks)
						{
							if (B.Id != Id)
							{
								continue;
							}

							Block.Min = B.Min;
							Block.Max = B.Max;

							FIntRect InitialBlock = Block;

							FIntPoint Grid = GridSize.Get();

							FIntPoint BlockSize = Block.Size();
							Block.Max.X = FMath::Max(Block.Min.X + 1, FMath::Min(Block.Max.X + CellDeltaX, Grid.X));
							Block.Max.Y = FMath::Max(Block.Min.Y + 1, FMath::Min(Block.Max.Y + CellDeltaY, Grid.Y));

							if (Block != InitialBlock)
							{
								BlockChangedDelegate.ExecuteIfBound(Id, Block);
							}

							break;
						}
					}
				}
			}
		}

		if (!Selecting && !Dragging)
		{
			bool ClickOnBlock = false;

			for (const FGuid& BlockId : SelectedBlocks)
			{
				if (MouseOnBlock(BlockId, Pos))
				{
					if (SelectedBlocks.Contains(BlockId))
					{
						ClickOnBlock = true;
					}
				}
			}

			int32 MovementSensitivity = 4;
			FVector2D MouseDiference = InitSelectionRect - Pos;
			MouseDiference = MouseDiference.GetAbs();

			if (!ClickOnBlock && (MouseDiference.X > MovementSensitivity || MouseDiference.Y > MovementSensitivity))
			{
				HasDragged = true;
				Selecting = true;
			}
		}
	}
	
	if (!Dragging && !Resizing && SelectedBlocks.Num()==1)
	{
		const TArray<FCustomizableObjectLayoutBlock>& CurrentBlocks = Blocks.Get();
		for (int i = CurrentBlocks.Num() - 1; i > -1; --i)
		{
			// Check for new created blocks
			if (BlockRects.Contains(CurrentBlocks[i].Id) && SelectedBlocks.Contains(CurrentBlocks[i].Id))
			{
				FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				if (MouseOnBlock(CurrentBlocks[i].Id, Pos, true))
				{
					ResizeCursor = true;
					break;
				}
			}

			ResizeCursor = false;
		}
	}

	// In case we lose focus
	if (Padding)
	{
		if (MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
		{
			FVector2D Pos = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			PaddingAmount += Pos - PaddingStart;
			PaddingStart = Pos;
		}
		else
		{
			Padding = false;
		}
	}

	if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		Selecting = false;
		Dragging = false;

		if (Resizing)
		{
			ResizeCursor = false;
			Resizing = false;
		}
	}

	return SCompoundWidget::OnMouseMove( MyGeometry, MouseEvent );
}


FReply SCustomizableObjectLayoutGrid::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (Mode == ELGM_Edit)
	{
		if (MouseEvent.GetWheelDelta() > 0)
		{
			if (Zoom < 2)
			{
				FVector2D GridCenter = DrawOrigin + (FVector2D((float)GridSize.Get().X, (float)GridSize.Get().Y) / 2.0f) * CellSize;
				DistanceFromOrigin = CurrentMousePosition - GridCenter;

				Zoom++;
			}
		}
		else
		{
			if (Zoom > 1)
			{
				DistanceFromOrigin = FVector2D::Zero();
				PaddingAmount = FVector2D::Zero();

				Zoom--;
			}
		}
	}

	return SCompoundWidget::OnMouseWheel(MyGeometry, MouseEvent);
}


FReply SCustomizableObjectLayoutGrid::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (Mode != ELGM_Edit)
	{
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}
	
	if (InKeyEvent.IsLeftControlDown())
	{
		if (InKeyEvent.GetKey() == EKeys::D)
		{
			DuplicateBlocks();
		}
		else if (InKeyEvent.GetKey() == EKeys::N)
		{
			FVector2D MouseToCellPosition = (CurrentMousePosition - DrawOrigin) / CellSize;
			GenerateNewBlock(MouseToCellPosition);
		}
		else if (InKeyEvent.GetKey() == EKeys::F)
		{
			SetBlockSizeToMax();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		DeleteSelectedBlocks();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}


FCursorReply SCustomizableObjectLayoutGrid::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (ResizeCursor)
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeSouthEast);
	}
	else
	{
		return FCursorReply::Cursor(EMouseCursor::Default);
	}

	return FCursorReply::Unhandled();
}


FVector2D SCustomizableObjectLayoutGrid::ComputeDesiredSize(float NotUsed) const
{
	return FVector2D(200.0f, 200.0f);
}


void SCustomizableObjectLayoutGrid::SetSelectedBlock(FGuid block )
{
	SelectedBlocks.Reset();
	SelectedBlocks.Add( block );
}


void SCustomizableObjectLayoutGrid::SetSelectedBlocks( const TArray<FGuid>& blocks )
{
	SelectedBlocks = blocks;
}


TArray<FGuid> SCustomizableObjectLayoutGrid::GetSelectedBlocks() const
{
	return SelectedBlocks;
}


void SCustomizableObjectLayoutGrid::DeleteSelectedBlocks()
{
	DeleteBlocksDelegate.ExecuteIfBound();
}


void SCustomizableObjectLayoutGrid::GenerateNewBlock(FVector2D MousePosition)
{
	if (MousePosition.X > 0 && MousePosition.Y > 0 && MousePosition.X < GridSize.Get().X && MousePosition.Y < GridSize.Get().Y)
	{
		FIntPoint Min = FIntPoint(MousePosition.X, MousePosition.Y);
		FIntPoint Max = Min + FIntPoint(1, 1);

		AddBlockAtDelegate.ExecuteIfBound(Min, Max);

		SelectedBlocks.Add(Blocks.Get().Last().Id);
	}
}


void SCustomizableObjectLayoutGrid::DuplicateBlocks()
{
	if (SelectedBlocks.Num())
	{
		for (const FCustomizableObjectLayoutBlock& Block : Blocks.Get())
		{
			if (SelectedBlocks.Find(Block.Id) != INDEX_NONE)
			{
				AddBlockAtDelegate.ExecuteIfBound(Block.Min, Block.Max);
			}
		}
	}
}


void SCustomizableObjectLayoutGrid::SetBlockSizeToMax()
{
	if (SelectedBlocks.Num())
	{
		for (const FCustomizableObjectLayoutBlock& Block : Blocks.Get())
		{
			if (SelectedBlocks.Find(Block.Id) != INDEX_NONE)
			{
				FIntRect FinalBlock;

				FinalBlock.Min = FIntPoint(0, 0);
				FinalBlock.Max = GridSize.Get();

				BlockChangedDelegate.ExecuteIfBound(Block.Id, FinalBlock);
			}
		}
	}
}


void SCustomizableObjectLayoutGrid::CalculateSelectionRect()
{
	if (InitSelectionRect.X <= CurrentMousePosition.X)
	{
		if (InitSelectionRect.Y <= CurrentMousePosition.Y)
		{
			SelectionRect.Min = FVector2f(InitSelectionRect);
			SelectionRect.Size = FVector2f(CurrentMousePosition - InitSelectionRect);
		}
		else
		{
			SelectionRect.Min = FVector2f(InitSelectionRect.X, CurrentMousePosition.Y);

			FVector2f AuxVector(CurrentMousePosition.X, InitSelectionRect.Y);
			SelectionRect.Size = AuxVector - SelectionRect.Min;
		}
	}
	else
	{
		if (InitSelectionRect.Y <= CurrentMousePosition.Y)
		{
			SelectionRect.Min = FVector2f(CurrentMousePosition.X, InitSelectionRect.Y);

			FVector2f AuxVector(InitSelectionRect.X, CurrentMousePosition.Y);
			SelectionRect.Size = AuxVector - SelectionRect.Min;
		}
		else
		{
			SelectionRect.Min = FVector2f(CurrentMousePosition);
			SelectionRect.Size = FVector2f(InitSelectionRect - CurrentMousePosition);
		}
	}

}


void SCustomizableObjectLayoutGrid::SetBlocks(const FIntPoint& InGridSize, const TArray<FCustomizableObjectLayoutBlock>& InBlocks)
{
	GridSize = InGridSize;
	Blocks = InBlocks;
}


bool SCustomizableObjectLayoutGrid::MouseOnBlock(FGuid BlockId, FVector2D MousePosition, bool CheckResizeBlock) const
{
	FVector2f Min, Max;
	if (CheckResizeBlock)
	{
		Min = BlockRects[BlockId].HandleRect.Min;
		Max = Min + BlockRects[BlockId].HandleRect.Size;
	}
	else
	{
		Min = BlockRects[BlockId].Rect.Min;
		Max = Min + BlockRects[BlockId].Rect.Size;
	}

	if (MousePosition.X > Min.X && MousePosition.X<Max.X && MousePosition.Y>Min.Y && MousePosition.Y < Max.Y)
	{
		return true;
	}

	return false;
}


TOptional<int32> SCustomizableObjectLayoutGrid::GetBlockPriortyValue() const
{
	if (SelectedBlocks.Num())
	{
		TArray<FCustomizableObjectLayoutBlock> CurrentSelectedBlocks;

		for (const FCustomizableObjectLayoutBlock& Block : Blocks.Get())
		{
			if (SelectedBlocks.Contains(Block.Id))
			{
				CurrentSelectedBlocks.Add(Block);
			}
		}

		int32 BlockPriority = CurrentSelectedBlocks[0].Priority;
		bool bSamePriority = true;

		for (const FCustomizableObjectLayoutBlock& Block : CurrentSelectedBlocks)
		{
			if (Block.Priority != BlockPriority)
			{
				bSamePriority = false;
				break;
			}
		}

		if (bSamePriority)
		{
			return BlockPriority;
		}
	}

	return TOptional<int32>();
}


void SCustomizableObjectLayoutGrid::OnBlockPriorityChanged(int32 InValue)
{
	if (SelectedBlocks.Num())
	{
		OnSetBlockPriority.ExecuteIfBound(InValue);
	}
}


void SCustomizableObjectLayoutGrid::SetLayoutStrategy(ECustomizableObjectTextureLayoutPackingStrategy Strategy)
{
	LayoutStrategy = Strategy;
}


// Canvas Drawer --------------------------------------------------------------

void FUVCanvasDrawer::Initialize(const FIntRect& InCanvasRect, const FIntRect& InClippingRect, const FVector2D& InOrigin, const FVector2D& InSize, const FIntPoint& InGridSize, const float InCellSize)
{
	Initialized = InCanvasRect.Size().X > 0 && InCanvasRect.Size().Y > 0;
	if (Initialized)
	{
		RenderTarget->SetViewRect(InCanvasRect);
		RenderTarget->SetClippingRect(InClippingRect);

		Origin = InOrigin;
		Size = InSize;
		CellSize = InCellSize;
		GridSize = InGridSize;
	}
}


void FUVCanvasDrawer::InitializeDrawingData(const TArray<FVector2f>& InUVLayout, const TArray<FVector2f>& InUnassignedUVs, const TArray<FCustomizableObjectLayoutBlock>& InBlocks, const TArray<FGuid>& InSelectedBlocks)
{
	Blocks = InBlocks;
	SelectedBlocks = InSelectedBlocks;

	// Convert data
	UVLayout.SetNum(InUVLayout.Num());
	for (int32 Index = 0; Index < InUVLayout.Num(); ++Index)
	{
		UVLayout[Index] = FVector2D(InUVLayout[Index]);
	}

	UnassignedUVs.SetNum(InUnassignedUVs.Num());
	for (int32 Index = 0; Index < UnassignedUVs.Num(); ++Index)
	{
		UnassignedUVs[Index] = FVector2D(InUnassignedUVs[Index]);
	}
}


void FUVCanvasDrawer::SetLayoutMode(ELayoutGridMode Mode)
{
	LayoutMode = Mode;
}


void FUVCanvasDrawer::DrawRenderThread(class FRHICommandListImmediate& RHICmdList, const void* InWindowBackBuffer)
{
	if (Initialized)
	{
		RenderTarget->SetRenderTargetTexture(*(FTexture2DRHIRef*)InWindowBackBuffer);

#if MUTABLE_CLEAN_ENGINE_BRANCH
		FCanvas Canvas(RenderTarget, nullptr, FGameTime(), GMaxRHIFeatureLevel);
#else
		FCanvas Canvas(RenderTarget, nullptr, 0, 0, 0, GMaxRHIFeatureLevel);
#endif
		
		Canvas.SetRenderTargetRect(RenderTarget->GetViewRect());
		Canvas.SetRenderTargetScissorRect(RenderTarget->GetClippingRect());

		// Num Lines
		const uint32 NumEdges = UVLayout.Num() / 2;
		const uint32 NumGridLines = GridSize.X + GridSize.Y + 2;
		const uint32 NumUnasignedUVs = UnassignedUVs.Num() * 4;

		// Num Vertices
		const uint32 NumVertices = LayoutMode == ELayoutGridMode::ELGM_Edit ? Blocks.Num() * 8 : Blocks.Num() * 4;

		// Num Triangles
		const uint32 NumTriangles = LayoutMode == ELayoutGridMode::ELGM_Edit ? Blocks.Num() * 4 : Blocks.Num() * 2;

		FBatchedElements* BatchedElements = Canvas.GetBatchedElements(FCanvas::ET_Line);
		BatchedElements->AddReserveLines(NumEdges + NumGridLines + NumUnasignedUVs);
		BatchedElements->AddReserveVertices(NumVertices);
		BatchedElements->AddReserveTriangles(NumTriangles, GWhiteTexture, ESimpleElementBlendMode::SE_BLEND_Translucent);

		// Color Definitions
		const FColor GridLineColor = FColor(150, 150, 150, 64);
		const FColor UVLineColor = FColor(255, 255, 255, 255);
		const FColor UnassignedUVsColor = FColor::Yellow;
		const FColor ResizeBlockColor = FColor(255, 96, 96, 255);

		const FHitProxyId HitProxyId = Canvas.GetHitProxyId();

		// Create line points
		FVector LinePoints[2];

		// Drawing Vertical Lines
		for (int32 LineIndex = 0; LineIndex < GridSize.X + 1; LineIndex++)
		{
			LinePoints[0] = FVector(FVector2D(Origin.X + LineIndex * CellSize, Origin.Y), 0.0f);
			LinePoints[1] = FVector(FVector2D(Origin.X + LineIndex * CellSize, Origin.Y + Size.Y), 0.0f);

			BatchedElements->AddTranslucentLine(LinePoints[0], LinePoints[1], GridLineColor, HitProxyId, 2.0f);
		}

		// Drawing Horizontal Lines
		for (int32 LineIndex = 0; LineIndex < GridSize.Y + 1; LineIndex++)
		{
			LinePoints[0] = FVector(FVector2D(Origin.X, Origin.Y + LineIndex * CellSize), 0.0f);
			LinePoints[1] = FVector(FVector2D(Origin.X + Size.X, Origin.Y + LineIndex * CellSize), 0.0f);

			BatchedElements->AddTranslucentLine(LinePoints[0], LinePoints[1], GridLineColor, HitProxyId, 2.0f);
		}

		// Drawing UV Lines
		for (uint32 LineIndex = 0; LineIndex < NumEdges; ++LineIndex)
		{
			LinePoints[0] = FVector(Origin + UVLayout[LineIndex * 2 + 0] * Size, 0.0f);
			LinePoints[1] = FVector(Origin + UVLayout[LineIndex * 2 + 1] * Size, 0.0f);

			BatchedElements->AddLine(LinePoints[0], LinePoints[1], UVLineColor, HitProxyId);
		}

		// Drawing Unassigned UVs
		const FVector2D CrossSize = Size * 0.01;
		for (const FVector2d& Vertex : UnassignedUVs)
		{
			LinePoints[0] = FVector(Origin + FVector2D(Vertex) * Size + FVector2D(CrossSize), 0.0f);
			LinePoints[1] = FVector(Origin + FVector2D(Vertex) * Size - FVector2D(CrossSize) * FVector2D(1.0f, -1.0f), 0.0f);
			BatchedElements->AddLine(LinePoints[0], LinePoints[1], UVLineColor, HitProxyId);

			LinePoints[0] = FVector(Origin + FVector2D(Vertex) * Size - FVector2D(CrossSize), 0.0f);
			BatchedElements->AddLine(LinePoints[0], LinePoints[1], UVLineColor, HitProxyId);

			LinePoints[1] = FVector(Origin + FVector2D(Vertex) * Size + FVector2D(CrossSize) * FVector2D(1.0f, -1.0f), 0.0f);
			BatchedElements->AddLine(LinePoints[0], LinePoints[1], UVLineColor, HitProxyId);

			LinePoints[0] = FVector(Origin + FVector2D(Vertex) * Size + FVector2D(CrossSize), 0.0f);
			BatchedElements->AddLine(LinePoints[0], LinePoints[1], UVLineColor, HitProxyId);
		}

		// Drawing Blocks
		for (const FCustomizableObjectLayoutBlock& Block : Blocks)
		{
			const FColor SelectionBlockColor = SelectedBlocks.Contains(Block.Id) ? FColor(75, 106, 230, 155) : FColor(230, 199, 75, 155);

			const FVector2f BlockMin(Block.Min);
			const FVector2f BlockMax(Block.Max);

			// Selection Block
			FRect2D SelectionBlock;
			SelectionBlock.Min = FVector2f(Origin) + BlockMin * CellSize + CellSize * 0.1f;
			SelectionBlock.Size = (BlockMax - BlockMin) * CellSize - CellSize * 0.2f;

			DrawBlock(BatchedElements, HitProxyId, SelectionBlock, SelectionBlockColor);

			if (LayoutMode == ELayoutGridMode::ELGM_Edit)
			{
				// Resize Block
				FRect2D ResizeBlock;;
				float HandleRectSize = FMath::Log2(float(GridSize.X)) / 10.0f;
				ResizeBlock.Size = FVector2f(CellSize) * HandleRectSize;
				ResizeBlock.Min = SelectionBlock.Min + SelectionBlock.Size - ResizeBlock.Size;

				DrawBlock(BatchedElements, HitProxyId, ResizeBlock, ResizeBlockColor);
			}
		}

		Canvas.Flush_RenderThread(RHICmdList, true);

		RenderTarget->ClearRenderTargetTexture();
		RHICmdList.SetScissorRect(false, 0, 0, 0, 0);
	}
}


void FUVCanvasDrawer::DrawBlock(FBatchedElements* BatchedElements, const FHitProxyId HitProxyId, const FRect2D& BlockRect, FColor Color)
{
	// Vertex positions
	FVector4 Vert0(BlockRect.Min.X, BlockRect.Min.Y, 0, 1);
	FVector4 Vert1(BlockRect.Min.X, BlockRect.Min.Y + BlockRect.Size.Y, 0, 1);
	FVector4 Vert2(BlockRect.Min.X + BlockRect.Size.X, BlockRect.Min.Y, 0, 1);
	FVector4 Vert3(BlockRect.Min.X + BlockRect.Size.X, BlockRect.Min.Y + BlockRect.Size.Y, 0, 1);

	// Brush Paint triangle
	{
		int32 V0 = BatchedElements->AddVertex(Vert0, FVector2d(0.0f, 0.0f), Color, HitProxyId);
		int32 V1 = BatchedElements->AddVertex(Vert1, FVector2d(0.0f, 1.0f), Color, HitProxyId);
		int32 V2 = BatchedElements->AddVertex(Vert2, FVector2d(1.0f, 0.0f), Color, HitProxyId);
		int32 V3 = BatchedElements->AddVertex(Vert3, FVector2d(1.0f, 1.0f), Color, HitProxyId);

		BatchedElements->AddTriangle(V0, V1, V2, GWhiteTexture, EBlendMode::BLEND_Translucent);
		BatchedElements->AddTriangle(V1, V3, V2, GWhiteTexture, EBlendMode::BLEND_Translucent);
	}

	// Drawing Outline to selected Blocks
	if (Color == FColor(75, 106, 230, 155))
	{
		BatchedElements->AddLine(Vert0, Vert1, FColor(230, 199, 75, 155), HitProxyId, 4.0f);
		BatchedElements->AddLine(Vert1, Vert3, FColor(230, 199, 75, 155), HitProxyId, 4.0f);
		BatchedElements->AddLine(Vert3, Vert2, FColor(230, 199, 75, 155), HitProxyId, 4.0f);
		BatchedElements->AddLine(Vert2, Vert0, FColor(230, 199, 75, 155), HitProxyId, 4.0f);
	}
}

#undef LOCTEXT_NAMESPACE
