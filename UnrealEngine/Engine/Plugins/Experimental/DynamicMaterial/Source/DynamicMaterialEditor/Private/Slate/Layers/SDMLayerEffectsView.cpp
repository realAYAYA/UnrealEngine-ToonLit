// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Layers/SDMLayerEffectsView.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialLayer.h"
#include "Slate/Layers/SDMLayerEffectsItem.h"
#include "Slate/Layers/SDMSlotLayerItem.h"
#include "Slate/SDMSlot.h"

#define LOCTEXT_NAMESPACE "SDMLayerEffectsView"

void SDMLayerEffectsView::Construct(const FArguments& InArgs, const TSharedPtr<SDMSlot>& InSlotWidget, UDMMaterialEffectStack* InMaterialEffectStack)
{
	SlotWidgetWeak = InSlotWidget;
	MaterialEffectStackWeak = InMaterialEffectStack;
	OnEffectSelected = InArgs._OnEffectSelected;

	RebuildList();

	SListView::Construct(SListView::FArguments()
		.ListItemsSource(&EffectItems)
		.Orientation(Orient_Vertical)
		.SelectionMode(ESelectionMode::Single)
		.ClearSelectionOnClick(false)
		.EnableAnimatedScrolling(true)
		.ScrollbarVisibility(EVisibility::Visible)
		.ConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible)
		.OnGenerateRow(this, &SDMLayerEffectsView::OnGenerateLayerItemWidget)
		.OnSelectionChanged(this, &SDMLayerEffectsView::OnLayerItemSelectionChanged)
		.OnContextMenuOpening(this->OnContextMenuOpening)
	);

	RequestListRefresh();

	if (IsValid(InMaterialEffectStack))
	{
		InMaterialEffectStack->GetOnUpdate().AddSP(this, &SDMLayerEffectsView::OnEffectStackUpdate);
	}
}

TSharedRef<ITableRow> SDMLayerEffectsView::OnGenerateLayerItemWidget(TSharedPtr<FDMEffectsLayerItem> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return 
		SNew(SDMLayerEffectsItem, InOwnerTable, InItem)
		.FlowDirectionPreference(EFlowDirectionPreference::LeftToRight)
		.OnEffectSelected(this, &SDMLayerEffectsView::OnLayerEffectSelected);
}

void SDMLayerEffectsView::OnLayerEffectSelected(const bool bInSelected, const TSharedRef<SDMLayerEffectsItem>& InEffectsItemWidget)
{
	if (!bInSelected)
	{
		SelectedEffectWidgets.Remove(InEffectsItemWidget);
		InEffectsItemWidget->SetEffectSelected(false);

		if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
		{
			SlotWidget->SetEditedComponent(nullptr);
			SlotWidget->SetSelectedLayer(nullptr);

			if (TSharedPtr<FDMEffectsLayerItem> SelectedEffectLayerItem = SelectedEffectWidgets.Last()->GetLayerItem())
			{
				if (UDMMaterialEffect* Effect = SelectedEffectLayerItem->MaterialEffectWeak.Get())
				{
					SlotWidget->SetEditedComponent(Effect);

					if (UDMMaterialEffectStack* EffectStack = Effect->GetEffectStack())
					{
						if (UDMMaterialLayerObject* SelectedLayer = EffectStack->GetLayer())
						{
							SlotWidget->SetSelectedLayer(nullptr);
						}
					}					
				}
			}
		}
	}
	else
	{
		SelectedEffectWidgets.AddUnique(InEffectsItemWidget);
		InEffectsItemWidget->SetEffectSelected(true);

		if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
		{
			UDMMaterialLayerObject* Layer = nullptr;
			UDMMaterialEffect* Effect = nullptr;

			if (TSharedPtr<FDMEffectsLayerItem> EffectViewItem = InEffectsItemWidget->GetLayerItem())
			{
				Effect = EffectViewItem->MaterialEffectWeak.Get();

				if (Effect)
				{
					if (UDMMaterialEffectStack* EffectStack = Effect->GetEffectStack())
					{
						Layer = EffectStack->GetLayer();
					}
				}
			}

			SlotWidget->SetEditedComponent(Effect);
			SlotWidget->SetSelectedLayer(Layer);
		}
	}
}

void SDMLayerEffectsView::OnEffectStackUpdate(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	RebuildList();
}

TSharedPtr<SDMLayerEffectsItem> SDMLayerEffectsView::WidgetFromLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InItem)
{
	return StaticCastSharedPtr<SDMLayerEffectsItem>(WidgetFromItem(InItem));
}

void SDMLayerEffectsView::OnLayerItemSelectionChanged(TSharedPtr<FDMEffectsLayerItem> InSelectedItem, ESelectInfo::Type InSelectInfo)
{
	TSharedPtr<SDMLayerEffectsItem> SlotLayerWidget = WidgetFromLayerItem(InSelectedItem);
	if (!SlotLayerWidget.IsValid())
	{
		return;
	}

	OnLayerEffectSelected(true, SlotLayerWidget.ToSharedRef());

	SelectedEffectIndex = INDEX_NONE;
	for (int32 Index = 0; Index < EffectItems.Num(); ++Index)
	{
		if (EffectItems[Index] == InSelectedItem)
		{
			SelectedEffectIndex = Index;
		}
	}
	if (SelectedEffectIndex == INDEX_NONE)
	{
		return;
	}

	SelectedEffectItems.Empty();
	SelectedEffectItems.Add(EffectItems[SelectedEffectIndex]);

	OnEffectSelected.ExecuteIfBound(InSelectedItem, SelectedEffectIndex);
}

void SDMLayerEffectsView::ScrollItemIntoView(const TSharedPtr<FDMEffectsLayerItem>& InItem)
{
	bNavigateOnScrollIntoView = true;

	RequestScrollIntoView(InItem, 0);
}

void SDMLayerEffectsView::FocusOnItem(const TSharedPtr<FDMEffectsLayerItem>& InItem)
{
	SelectorItem = InItem;
	RangeSelectionStart = InItem;
}

int32 SDMLayerEffectsView::GetLayerItemIndex(const TSharedPtr<FDMEffectsLayerItem>& Item) const
{
	return GetItems().Find(TListTypeTraits<TSharedPtr<FDMEffectsLayerItem>>::NullableItemTypeConvertToItemType(Item));
}

void SDMLayerEffectsView::RebuildList()
{
	EffectItems.Empty();

	if (UDMMaterialEffectStack* EffectStack = MaterialEffectStackWeak.Get())
	{
		for (UDMMaterialEffect* MaterialEffect : EffectStack->GetEffects())
		{
			EffectItems.Add(MakeShared<FDMEffectsLayerItem>(MaterialEffect));
		}
	}

	SListView<TSharedPtr<FDMEffectsLayerItem>>::RebuildList();
}

FCursorReply SDMLayerEffectsView::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (IsRightClickScrolling() && CursorEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		// We hide the native cursor as we'll be drawing the software EMouseCursor::GrabHandClosed cursor
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	return SListView::OnCursorQuery(MyGeometry, CursorEvent);
}

FReply SDMLayerEffectsView::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && !MouseEvent.IsTouchEvent())
	{
		// We only care about deltas along the scroll axis
		FTableViewDimensions CursorDeltaDimensions(Orientation, MouseEvent.GetCursorDelta());
		CursorDeltaDimensions.LineAxis = 0.f;

		const float ScrollByAmount = CursorDeltaDimensions.ScrollAxis / MyGeometry.Scale;

		// If scrolling with the right mouse button, we need to remember how much we scrolled.
		// If we did not scroll at all, we will bring up the context menu when the mouse is released.
		AmountScrolledWhileRightMouseDown += FMath::Abs(ScrollByAmount);

		// Has the mouse moved far enough with the right mouse button held down to start capturing
		// the mouse and dragging the view?
		if (IsRightClickScrolling())
		{
			// Make sure the active timer is registered to update the inertial scroll
			if (!bIsScrollingActiveTimerRegistered)
			{
				bIsScrollingActiveTimerRegistered = true;
				RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SDMLayerEffectsView::UpdateInertialScroll));
			}

			TickScrollDelta -= ScrollByAmount;

			const float AmountScrolled = this->ScrollBy(MyGeometry, -ScrollByAmount, AllowOverscroll);

			FReply Reply = FReply::Handled();

			// The mouse moved enough that we're now dragging the view. Capture the mouse
			// so the user does not have to stay within the bounds of the list while dragging.
			if (this->HasMouseCapture() == false)
			{
				Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
				SoftwareCursorPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
				bShowSoftwareCursor    = true;
			}

			// Check if the mouse has moved.
			if (AmountScrolled != 0)
			{
				SoftwareCursorPosition += CursorDeltaDimensions.ToVector2D();
			}

			return Reply;
		}
	}

	return SListView::OnMouseMove(MyGeometry, MouseEvent);
}

FReply SDMLayerEffectsView::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		AmountScrolledWhileRightMouseDown = 0;
		bShowSoftwareCursor = false;

		// If we have mouse capture, snap the mouse back to the closest location that is within the list's bounds
		if (HasMouseCapture())
		{
			const FSlateRect ListScreenSpaceRect = MyGeometry.GetLayoutBoundingRect();
			const FVector2D CursorPosition = MyGeometry.LocalToAbsolute(SoftwareCursorPosition);

			const FIntPoint BestPositionInList(
					FMath::RoundToInt(FMath::Clamp(CursorPosition.X, ListScreenSpaceRect.Left, ListScreenSpaceRect.Right)),
					FMath::RoundToInt(FMath::Clamp(CursorPosition.Y, ListScreenSpaceRect.Top, ListScreenSpaceRect.Bottom))
				);

			Reply.SetMousePos(BestPositionInList);
		}

		return Reply;
	}
	return SListView::OnMouseButtonUp(MyGeometry, MouseEvent);
}

bool SDMLayerEffectsView::AddLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InLayerItem)
{
	if (!InLayerItem.IsValid())
	{
		return false;
	}

	EffectItems.Add(InLayerItem);

	RequestListRefresh();

	return true;
}

bool SDMLayerEffectsView::RemoveLayerItem(const TSharedPtr<FDMEffectsLayerItem>& InLayerItem)
{
	if (!InLayerItem.IsValid())
	{
		return false;
	}

	EffectItems.Remove(InLayerItem);

	RequestListRefresh();

	return true;
}

#undef LOCTEXT_NAMESPACE
