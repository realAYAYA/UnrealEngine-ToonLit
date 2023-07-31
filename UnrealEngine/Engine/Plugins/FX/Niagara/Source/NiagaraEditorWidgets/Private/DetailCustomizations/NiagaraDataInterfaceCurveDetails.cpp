// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCurveDetails.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVector2DCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceVector4Curve.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEditorWidgetsModule.h"
#include "NiagaraSystem.h"
#include "NiagaraEditorSettings.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraCurveSelectionViewModel.h"

#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/Optional.h"
#include "Brushes/SlateColorBrush.h"
#include "Modules/ModuleManager.h"

#include "CurveEditor.h"
#include "Tree/SCurveEditorTree.h"
#include "Tree/ICurveEditorTreeItem.h"
#include "Tree/SCurveEditorTreePin.h"
#include "RichCurveEditorModel.h"
#include "SCurveEditorPanel.h"
#include "SCurveKeyDetailPanel.h"
#include "CurveEditorCommands.h"

#include "Widgets/SVerticalResizeBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "ScopedTransaction.h"
#include "SColorGradientEditor.h"

#include "EditorFontGlyphs.h"
#include "Widgets/Input/SSegmentedControl.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceCurveDetails"

struct FNiagaraCurveDetailsTreeItem : public ICurveEditorTreeItem, TSharedFromThis<FNiagaraCurveDetailsTreeItem>
{
	FNiagaraCurveDetailsTreeItem(UObject* InCurveOwnerObject, UNiagaraDataInterfaceCurveBase::FCurveData InCurveData)
		: CurveOwnerObject(InCurveOwnerObject)
		, CurveData(InCurveData)
	{
	}

	UObject* GetCurveOwnerObject() const
	{
		return CurveOwnerObject.Get();
	}

	FRichCurve* GetCurve() const
	{
		return CurveOwnerObject.IsValid() ? CurveData.Curve : nullptr;
	}

	virtual TSharedPtr<SWidget> GenerateCurveEditorTreeWidget(const FName& InColumnName, TWeakPtr<FCurveEditor> InCurveEditor, FCurveEditorTreeItemID InTreeItemID, const TSharedRef<ITableRow>& TableRow) override
	{
		if (InColumnName == ColumnNames.Label)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 5, 0)
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.8"))
					.Text(FEditorFontGlyphs::Circle)
					.ColorAndOpacity(FSlateColor(CurveData.Color))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.MinDesiredHeight(22)
					[
						SNew(STextBlock)
						.Text(FText::FromName(CurveData.Name))
						.TextStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.CurveOverview.CurveComponentText")
					]
				];
		}
		else if (InColumnName == ColumnNames.PinHeader)
		{
			return SNew(SCurveEditorTreePin, InCurveEditor, InTreeItemID, TableRow);
		}

		return nullptr;
	}

	virtual void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override
	{
		if (CurveOwnerObject.IsValid() == false)
		{
			return;
		}

		TUniquePtr<FRichCurveEditorModelRaw> NewCurve = MakeUnique<FRichCurveEditorModelRaw>(CurveData.Curve, CurveOwnerObject.Get());
		if(CurveData.Name != NAME_None)
		{
			NewCurve->SetShortDisplayName(FText::FromName(CurveData.Name));
		}
		NewCurve->SetColor(CurveData.Color);
		NewCurve->OnCurveModified().AddSP(this, &FNiagaraCurveDetailsTreeItem::CurveChanged);
		OutCurveModels.Add(MoveTemp(NewCurve));
	}

	virtual bool PassesFilter(const FCurveEditorTreeFilter* InFilter) const override { return true; }

	FSimpleMulticastDelegate& OnCurveChanged()
	{
		return CurveChangedDelegate;
	}

private:
	void CurveChanged()
	{
		CurveChangedDelegate.Broadcast();
	}

private:
	TWeakObjectPtr<UObject> CurveOwnerObject;
	UNiagaraDataInterfaceCurveBase::FCurveData CurveData;
	FSimpleMulticastDelegate CurveChangedDelegate;
};

FRichCurve* GetCurveFromPropertyHandle(TSharedPtr<IPropertyHandle> Handle)
{
	TArray<void*> RawData;
	Handle->AccessRawData(RawData);
	return RawData.Num() == 1 ? static_cast<FRichCurve*>(RawData[0]) : nullptr;
}

class SNiagaraDataInterfaceCurveKeySelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceCurveKeySelector) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FCurveEditor> InCurveEditor, const TArray<FCurveEditorTreeItemID>& InCurveTreeItemIds, TSharedPtr<SCurveEditorTree> InCurveEditorTree)
	{
		CurveEditor = InCurveEditor;
		OrderedCurveTreeItemIds = InCurveTreeItemIds;
		CurveEditorTree = InCurveEditorTree;
		ChildSlot
		[
			SNew(SHorizontalBox)

			// Zoom to fit button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::ZoomToFitClicked)
				.ToolTipText(LOCTEXT("ZoomToFitToolTip", "Zoom to fit all keys"))
				.Content()
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Text(FEditorFontGlyphs::Expand)
				]
			]

			// Previous curve button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 1, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::PreviousCurveClicked)
				.ToolTipText(LOCTEXT("PreviousCurveToolTip", "Select the previous curve"))
				.Visibility(this, &SNiagaraDataInterfaceCurveKeySelector::GetNextPreviousCurveButtonVisibility)
				.Content()
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.ArrowUp"))
				]
			]

			// Next curve button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::NextCurveClicked)
				.ToolTipText(LOCTEXT("NextCurveToolTip", "Select the next curve"))
				.Visibility(this, &SNiagaraDataInterfaceCurveKeySelector::GetNextPreviousCurveButtonVisibility)
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.ArrowDown"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Previous key button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 1, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::PreviousKeyClicked)
				.ToolTipText(LOCTEXT("PreviousKeyToolTip", "Select the previous key for the selected curve."))
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.ArrowLeft"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Next key button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::NextKeyClicked)
				.ToolTipText(LOCTEXT("NextKeyToolTip", "Select the next key for the selected curve."))
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.ArrowRight"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Add key button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 1, 0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::AddKeyClicked)
				.ToolTipText(LOCTEXT("AddKeyToolTip", "Add a key to the selected curve."))
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]

			// Delete key button
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &SNiagaraDataInterfaceCurveKeySelector::DeleteKeyClicked)
				.ToolTipText(LOCTEXT("DeleteKeyToolTip", "Delete the currently selected keys."))
				.Content()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
	}

private:
	void GetActiveCurveModelAndSelectedKeys(TOptional<FCurveModelID>& OutActiveCurveModelId, TArray<FKeyHandle>& OutSelectedKeyHandles)
	{
		const TMap<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& CurveTreeSelection = CurveEditor->GetTree()->GetSelection();
		if (CurveTreeSelection.Num() != 0)
		{
			// If there are curves selected in the tree use those first.
			FCurveEditorTreeItemID FirstSelectedCurveTreeItemId;
			for (int32 i = OrderedCurveTreeItemIds.Num() - 1; i >= 0; i--)
			{
				const FCurveEditorTreeItemID& CurveTreeItemId = OrderedCurveTreeItemIds[i];
				if (CurveTreeSelection.Contains(CurveTreeItemId))
				{
					FirstSelectedCurveTreeItemId = CurveTreeItemId;
					break;
				}
			}

			TArrayView<const FCurveModelID> CurveModelIds = CurveEditor->GetTreeItem(FirstSelectedCurveTreeItemId).GetOrCreateCurves(CurveEditor.Get());
			if (CurveModelIds.Num() == 1)
			{
				OutActiveCurveModelId = CurveModelIds[0];

				const FKeyHandleSet* SelectedKeyHandleSet = CurveEditor->GetSelection().GetAll().Find(OutActiveCurveModelId.GetValue());
				if (SelectedKeyHandleSet != nullptr)
				{
					OutSelectedKeyHandles = SelectedKeyHandleSet->AsArray();
				}
			}
		}
		else
		{
			// Otherwise check if there are keys selected and if so use first curve with selected keys.
			FCurveEditorSelection& CurveEditorSelection = CurveEditor->GetSelection();
			if (CurveEditorSelection.IsEmpty() == false)
			{
				for (int32 i = OrderedCurveTreeItemIds.Num() - 1; i >= 0; i--)
				{
					const FCurveEditorTreeItemID& CurveTreeItemId = OrderedCurveTreeItemIds[i];
					TArrayView<const FCurveModelID> CurveModelIds = CurveEditor->GetTreeItem(CurveTreeItemId).GetCurves();
					if (CurveModelIds.Num() == 1)
					{
						const FKeyHandleSet* SelectedKeyHandleSet = CurveEditorSelection.GetAll().Find(CurveModelIds[0]);
						if (SelectedKeyHandleSet != nullptr)
						{
							OutActiveCurveModelId = CurveModelIds[0];
							OutSelectedKeyHandles = SelectedKeyHandleSet->AsArray();
							break;
						}
					}
				}
			}
			else
			{
				// Otherwise just use the first pinned curve.
				const TSet<FCurveModelID>& PinnedCurveIds = CurveEditor->GetPinnedCurves();
				if (PinnedCurveIds.Num() > 0)
				{
					for (FCurveEditorTreeItemID& CurveTreeItemId : OrderedCurveTreeItemIds)
					{
						TArrayView<const FCurveModelID> CurveModelIds = CurveEditor->GetTreeItem(CurveTreeItemId).GetOrCreateCurves(CurveEditor.Get());
						if (CurveModelIds.Num() == 1 && PinnedCurveIds.Contains(CurveModelIds[0]))
						{
							OutActiveCurveModelId = CurveModelIds[0];
							break;
						}
					}
				}
			}
		}
	}

	struct FKeyHandlePositionPair
	{
		FKeyHandle Handle;
		FKeyPosition Position;
	};

	void GetSortedKeyHandlessAndPositionsForModel(FCurveModel& InCurveModel, TArray<FKeyHandlePositionPair>& OutSortedKeyHandlesAndPositions)
	{
		TArray<FKeyHandle> KeyHandles;
		InCurveModel.GetKeys(*CurveEditor.Get(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);

		TArray<FKeyPosition> KeyPositions;
		KeyPositions.AddDefaulted(KeyHandles.Num());
		InCurveModel.GetKeyPositions(KeyHandles, KeyPositions);

		for (int32 i = 0; i < KeyHandles.Num(); i++)
		{
			FKeyHandlePositionPair& KeyHandlePositionPair = OutSortedKeyHandlesAndPositions.AddDefaulted_GetRef();
			KeyHandlePositionPair.Handle = KeyHandles[i];
			KeyHandlePositionPair.Position = KeyPositions[i];
		}

		OutSortedKeyHandlesAndPositions.Sort([](const FKeyHandlePositionPair& A, const FKeyHandlePositionPair& B) { return A.Position.InputValue < B.Position.InputValue; });
	}

	void GetOrderedActiveCurveModelIds(TArray<FCurveModelID>& OutOrderedActiveCurveModelIds)
	{
		if (CurveEditor->GetTreeSelection().Num() > 0)
		{
			// If there are curves selected in the tree then those are the only active ones.
			for (const FCurveEditorTreeItemID& OrderedCurveTreeItemId : OrderedCurveTreeItemIds)
			{
				if(CurveEditor->GetTreeSelectionState(OrderedCurveTreeItemId) != ECurveEditorTreeSelectionState::None)
				{
					TArrayView<const FCurveModelID> CurveModelIds = CurveEditor->GetTreeItem(OrderedCurveTreeItemId).GetOrCreateCurves(CurveEditor.Get());
					if (CurveModelIds.Num() == 1)
					{
						OutOrderedActiveCurveModelIds.Add(CurveModelIds[0]);
					}
				}
			}
		}
		else
		{
			// Otherwise the active curves are the pinned curves.
			for (const FCurveEditorTreeItemID& OrderedCurveTreeItemId : OrderedCurveTreeItemIds)
			{
				TArrayView<const FCurveModelID> CurveModelIds = CurveEditor->GetTreeItem(OrderedCurveTreeItemId).GetCurves();
				if (CurveModelIds.Num() == 1 && CurveEditor->IsCurvePinned(CurveModelIds[0]))
				{
					OutOrderedActiveCurveModelIds.Add(CurveModelIds[0]);
				}
			}
		}
	}

	enum class ENavigateDirection
	{
		Previous,
		Next
	};

	void NavigateToAdjacentCurve(ENavigateDirection Direction)
	{
		if (CurveEditorTree.IsValid())
		{
			if (CurveEditor->GetTreeSelection().Num() == 0)
			{
				CurveEditorTree->SetItemSelection(OrderedCurveTreeItemIds[0], true);
			}
			else
			{
				int32 TargetSelectedTreeItemIndex = INDEX_NONE;
				int32 StartIndex = Direction == ENavigateDirection::Previous ? 0 : OrderedCurveTreeItemIds.Num() - 1;
				int32 EndIndex = Direction == ENavigateDirection::Previous ? OrderedCurveTreeItemIds.Num() : -1;
				int32 IndexOffset = Direction == ENavigateDirection::Previous ? 1 : -1;
				for (int32 i = StartIndex; i != EndIndex; i += IndexOffset)
				{
					if (CurveEditor->GetTreeSelectionState(OrderedCurveTreeItemIds[i]) != ECurveEditorTreeSelectionState::None)
					{
						TargetSelectedTreeItemIndex = i;
						break;
					}
				}

				if (TargetSelectedTreeItemIndex != INDEX_NONE)
				{
					int32 IndexToSelect = Direction == ENavigateDirection::Previous 
						? TargetSelectedTreeItemIndex - 1
						: TargetSelectedTreeItemIndex + 1;

					if (IndexToSelect > OrderedCurveTreeItemIds.Num() - 1)
					{
						IndexToSelect = 0;
					}
					else if(IndexToSelect < 0)
					{
						IndexToSelect = OrderedCurveTreeItemIds.Num() - 1;
					}

					CurveEditorTree->ClearSelection();
					CurveEditorTree->SetItemSelection(OrderedCurveTreeItemIds[IndexToSelect], true);
				}
			}
		}
	}

	void NavigateToAdjacentKey(ENavigateDirection Direction)
	{
		TOptional<FCurveModelID> ActiveCurveModelId;
		TArray<FKeyHandle> SelectedKeyHandles;
		GetActiveCurveModelAndSelectedKeys(ActiveCurveModelId, SelectedKeyHandles);

		TOptional<FCurveModelID> CurveModelIdToSelect;
		TOptional<FKeyHandle> KeyHandleToSelect;
		if (ActiveCurveModelId.IsSet())
		{
			FCurveModel* ActiveCurveModel = CurveEditor->GetCurves()[ActiveCurveModelId.GetValue()].Get();
			TArray<FKeyHandlePositionPair> ActiveSortedKeyHandlePositionPairs;
			GetSortedKeyHandlessAndPositionsForModel(*ActiveCurveModel, ActiveSortedKeyHandlePositionPairs);

			if (SelectedKeyHandles.Num() != 0)
			{
				// If there's currently a selected key on the active curve then we want to use that as the target for navigating.
				int32 TargetSelectedKeyIndex = INDEX_NONE;
				int32 StartIndex = Direction == ENavigateDirection::Previous ? 0 : ActiveSortedKeyHandlePositionPairs.Num() - 1;
				int32 EndIndex = Direction == ENavigateDirection::Previous ? ActiveSortedKeyHandlePositionPairs.Num() : -1;
				int32 IndexOffset = Direction == ENavigateDirection::Previous ? 1 : -1;
				for (int32 i = StartIndex; i != EndIndex; i += IndexOffset)
				{
					if (SelectedKeyHandles.Contains(ActiveSortedKeyHandlePositionPairs[i].Handle))
					{
						TargetSelectedKeyIndex = i;
						break;
					}
				}

				if (TargetSelectedKeyIndex != INDEX_NONE)
				{
					if(Direction == ENavigateDirection::Previous && TargetSelectedKeyIndex > 0)
					{
						// If we're navigating previous and we're not at the first key we can just select the previous key on this curve.
						CurveModelIdToSelect = ActiveCurveModelId;
						KeyHandleToSelect = ActiveSortedKeyHandlePositionPairs[TargetSelectedKeyIndex - 1].Handle;
					}
					else if (Direction == ENavigateDirection::Next && TargetSelectedKeyIndex < ActiveSortedKeyHandlePositionPairs.Num() - 1)
					{
						// If we're navigating next and we're not at the last key we can just select the next key on this curve.
						CurveModelIdToSelect = ActiveCurveModelId;
						KeyHandleToSelect = ActiveSortedKeyHandlePositionPairs[TargetSelectedKeyIndex + 1].Handle;
					}
					else
					{
						// Otherwise we're going to need to navigate to another curve since we're at the start or end of the current curve.
						TArray<FCurveModelID> OrderedActiveCurveModelIds;
						GetOrderedActiveCurveModelIds(OrderedActiveCurveModelIds);

						// Find the adjacent curve with keys so we can select a key there.
						FCurveModel* CurveModelToSelect = nullptr;
						int32 CurrentIndex = OrderedActiveCurveModelIds.IndexOfByKey(ActiveCurveModelId);
						int32 CurrentIndexOffset = Direction == ENavigateDirection::Previous ? -1 : 1;
						while (CurveModelIdToSelect.IsSet() == false)
						{
							CurrentIndex += CurrentIndexOffset;
							if (CurrentIndex < 0)
							{
								CurrentIndex = OrderedActiveCurveModelIds.Num() - 1;
							}
							else if (CurrentIndex > OrderedActiveCurveModelIds.Num() - 1)
							{
								CurrentIndex = 0;
							}

							FCurveModel* CurrentCurveModel = CurveEditor->GetCurves()[OrderedActiveCurveModelIds[CurrentIndex]].Get();
							if (CurrentCurveModel->GetNumKeys() > 0)
							{
								CurveModelIdToSelect = OrderedActiveCurveModelIds[CurrentIndex];
								CurveModelToSelect = CurrentCurveModel;
							}
						}

						if (CurveModelIdToSelect == ActiveCurveModelId)
						{
							// There were no other active curves with keys to select from so just wrap around the currently active curve.
							if (Direction == ENavigateDirection::Previous)
							{
								KeyHandleToSelect = ActiveSortedKeyHandlePositionPairs.Last().Handle;
							}
							else
							{
								KeyHandleToSelect = ActiveSortedKeyHandlePositionPairs[0].Handle;
							}
						}
						else if(CurveModelToSelect != nullptr)
						{
							// We're selecting a key on a different curve so we need to sort the positions and select the first or last based
							// on the navigation direction.
							TArray<FKeyHandlePositionPair> SortedKeyHandlePositionPairs;
							GetSortedKeyHandlessAndPositionsForModel(*CurveModelToSelect, SortedKeyHandlePositionPairs);
							if (Direction == ENavigateDirection::Previous)
							{
								KeyHandleToSelect = SortedKeyHandlePositionPairs.Last().Handle;
							}
							else
							{
								KeyHandleToSelect = SortedKeyHandlePositionPairs[0].Handle;
							}
						}
					}
				}
			}
			else if(ActiveSortedKeyHandlePositionPairs.Num() > 0)
			{
				// There weren't any keys already selected so just select the first key on the active curve.
				CurveModelIdToSelect = ActiveCurveModelId;
				KeyHandleToSelect = ActiveSortedKeyHandlePositionPairs[0].Handle;
			}
		}

		if (CurveModelIdToSelect.IsSet() && KeyHandleToSelect.IsSet())
		{
			FCurveEditorSelection& CurveEditorSelection = CurveEditor->GetSelection();
			CurveEditorSelection.Clear();
			CurveEditorSelection.Add(CurveModelIdToSelect.GetValue(), ECurvePointType::Key, KeyHandleToSelect.GetValue());

			TArray<FCurveModelID> CurvesToFit;
			CurvesToFit.Add(CurveModelIdToSelect.GetValue());
			CurveEditor->ZoomToFitCurves(CurvesToFit);
		}
	}

	FReply ZoomToFitClicked()
	{
		TArray<FCurveModelID> ActiveCurveModelIds;
		GetOrderedActiveCurveModelIds(ActiveCurveModelIds);
		CurveEditor->ZoomToFitCurves(ActiveCurveModelIds);
		return FReply::Handled();
	}

	FReply PreviousCurveClicked()
	{
		NavigateToAdjacentCurve(ENavigateDirection::Previous);
		return FReply::Handled();
	}

	FReply NextCurveClicked()
	{
		NavigateToAdjacentCurve(ENavigateDirection::Next);
		return FReply::Handled();
	}

	FReply PreviousKeyClicked()
	{
		NavigateToAdjacentKey(ENavigateDirection::Previous);
		return FReply::Handled();
	}

	FReply NextKeyClicked()
	{
		NavigateToAdjacentKey(ENavigateDirection::Next);
		return FReply::Handled();
	}

	EVisibility GetNextPreviousCurveButtonVisibility() const
	{
		return OrderedCurveTreeItemIds.Num() == 1 ? EVisibility::Collapsed : EVisibility::Visible;
	}

	FReply AddKeyClicked()
	{
		TOptional<FCurveModelID> CurveModelIdForAdd;
		TArray<FKeyHandle> SelectedKeyHandles;
		GetActiveCurveModelAndSelectedKeys(CurveModelIdForAdd, SelectedKeyHandles);

		if(CurveModelIdForAdd.IsSet())
		{
			FCurveModel* CurveModelForAdd = CurveEditor->GetCurves()[CurveModelIdForAdd.GetValue()].Get();

			FKeyPosition NewKeyPosition;
			FKeyAttributes NewKeyAttributes = CurveEditor->GetDefaultKeyAttributes().Get();
			NewKeyAttributes.SetInterpMode(RCIM_Cubic);
			NewKeyAttributes.SetTangentMode(RCTM_Auto);
			if (CurveModelForAdd->GetNumKeys() == 0)
			{
				// If there are no keys, add one at 0, 0.
				NewKeyPosition.InputValue = 0.0f;
				NewKeyPosition.OutputValue = 0.0f;
			}
			else if (CurveModelForAdd->GetNumKeys() == 1)
			{
				// If there's a single key, add the new key at the same value, but time + 1.
				TArray<FKeyHandle> KeyHandles;
				CurveModelForAdd->GetKeys(*CurveEditor.Get(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);
				
				TArray<FKeyPosition> KeyPositions;
				KeyPositions.AddDefaulted();
				CurveModelForAdd->GetKeyPositions(KeyHandles, KeyPositions);
						
				NewKeyPosition.InputValue = KeyPositions[0].InputValue + 1;
				NewKeyPosition.OutputValue = KeyPositions[0].OutputValue;
			}
			else
			{
				TArray<FKeyHandlePositionPair> SortedKeyHandlePositionPairs;
				GetSortedKeyHandlessAndPositionsForModel(*CurveModelForAdd, SortedKeyHandlePositionPairs);

				int32 IndexToAddAfter = INDEX_NONE;
				if (SelectedKeyHandles.Num() > 0)
				{
					for (int32 i = SortedKeyHandlePositionPairs.Num() - 1; i >= 0; i--)
					{
						if (SelectedKeyHandles.Contains(SortedKeyHandlePositionPairs[i].Handle))
						{
							IndexToAddAfter = i;
							break;
						}
					}
				}

				if(IndexToAddAfter == INDEX_NONE)
				{
					IndexToAddAfter = SortedKeyHandlePositionPairs.Num() - 1;
				}

				if (IndexToAddAfter == SortedKeyHandlePositionPairs.Num() - 1)
				{
					const FKeyPosition& TargetKeyPosition = SortedKeyHandlePositionPairs[IndexToAddAfter].Position;
					const FKeyPosition& PreviousKeyPosition = SortedKeyHandlePositionPairs[IndexToAddAfter - 1].Position;
					NewKeyPosition.InputValue = TargetKeyPosition.InputValue + (TargetKeyPosition.InputValue - PreviousKeyPosition.InputValue);
					NewKeyPosition.OutputValue = TargetKeyPosition.OutputValue + (TargetKeyPosition.OutputValue - PreviousKeyPosition.OutputValue);
				}
				else
				{
					const FKeyPosition& TargetKeyPosition = SortedKeyHandlePositionPairs[IndexToAddAfter].Position;
					const FKeyPosition& NextKeyPosition = SortedKeyHandlePositionPairs[IndexToAddAfter + 1].Position;
					NewKeyPosition.InputValue = TargetKeyPosition.InputValue + ((NextKeyPosition.InputValue - TargetKeyPosition.InputValue) / 2);
					NewKeyPosition.OutputValue = TargetKeyPosition.OutputValue + ((NextKeyPosition.OutputValue - TargetKeyPosition.OutputValue) / 2);
				}
			}

			FScopedTransaction Transaction(LOCTEXT("AddKey", "Add Key"));
			TOptional<FKeyHandle> NewKeyHandle = CurveModelForAdd->AddKey(NewKeyPosition, NewKeyAttributes);
			if(NewKeyHandle.IsSet())
			{
				FCurveEditorSelection& CurveEditorSelection = CurveEditor->GetSelection();
				CurveEditorSelection.Clear();
				CurveEditorSelection.Add(CurveModelIdForAdd.GetValue(), ECurvePointType::Key, NewKeyHandle.GetValue());

				TArray<FCurveModelID> CurvesToFit;
				CurvesToFit.Add(CurveModelIdForAdd.GetValue());
				CurveEditor->ZoomToFitCurves(CurvesToFit);
			}
		}
		return FReply::Handled();
	}

	FReply DeleteKeyClicked()
	{
		CurveEditor->DeleteSelection();
		return FReply::Handled();
	}

private:
	TSharedPtr<FCurveEditor> CurveEditor;
	TArray<FCurveEditorTreeItemID> OrderedCurveTreeItemIds;
	TSharedPtr<SCurveEditorTree> CurveEditorTree;
};

class SNiagaraCurveThumbnail : public SLeafWidget
{
	SLATE_BEGIN_ARGS(SNiagaraCurveThumbnail)
		: _Width(16)
		, _Height(8) 
		{}
		SLATE_ARGUMENT(float, Width)
		SLATE_ARGUMENT(float, Height)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FRichCurve* CurveToDisplay)
	{
		Width = InArgs._Width;
		Height = InArgs._Height;

		float TimeMin;
		float TimeMax;
		float ValueMin;
		float ValueMax;
		CurveToDisplay->GetTimeRange(TimeMin, TimeMax);
		CurveToDisplay->GetValueRange(ValueMin, ValueMax);

		float TimeRange = TimeMax - TimeMin;
		float ValueRange = ValueMax - ValueMin;

		int32 Points = 13;
		float TimeIncrement = TimeRange / (Points - 1);
		for (int32 i = 0; i < Points; i++)
		{
			float Time = TimeMin + i * TimeIncrement;
			float Value = CurveToDisplay->Eval(Time);

			float NormalizedX = (Time - TimeMin) / TimeRange;
			float NormalizedY = (Value - ValueMin) / ValueRange;
			CurvePoints.Add(FVector2D(NormalizedX * Width, (1 - NormalizedY) * Height));
		}
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), CurvePoints, ESlateDrawEffect::None, InWidgetStyle.GetForegroundColor(), true, 2.0f);
		return LayerId;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(Width, Height);
	}

	TArray<FVector2D> CurvePoints;
	float Width;
	float Height;
};

class SNiagaraCurveTemplateBar : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SNiagaraCurveTemplateBar) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor)
	{
		CurveEditor = InCurveEditor;

		const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();

		FToolBarBuilder ToolBarBuilder(CurveEditor->GetCommands(), FMultiBoxCustomization::None, nullptr, true);
		ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);

		ToolBarBuilder.AddWidget(
			SNew(SBox)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 0, 5, 0))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CurveTemplateLabel", "Templates"))
			]);

		for (const FNiagaraCurveTemplate& CurveTemplate : Settings->GetCurveTemplates())
		{
			UCurveFloat* FloatCurveAsset = Cast<UCurveFloat>(CurveTemplate.CurveAsset.TryLoad());
			if(FloatCurveAsset != nullptr)
			{
				FText CurveDisplayName = CurveTemplate.DisplayNameOverride.IsEmpty()
					? FText::FromString(FName::NameToDisplayString(FloatCurveAsset->GetName(), false))
					: FText::FromString(CurveTemplate.DisplayNameOverride);

				ToolBarBuilder.AddWidget(
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &SNiagaraCurveTemplateBar::CurveTemplateClicked, TWeakObjectPtr<UCurveFloat>(FloatCurveAsset))
					.ToolTipText(FText::Format(LOCTEXT("ApplyCurveTemplateFormat", "{0}\nClick to apply this template to the selected curves."), CurveDisplayName))
					.ContentPadding(FMargin(3))
					.Content()
					[
						SNew(SNiagaraCurveThumbnail, &FloatCurveAsset->FloatCurve)
					]);
			}
		}

		ChildSlot
		[
			ToolBarBuilder.MakeWidget()
		];
	}

private:
	FReply CurveTemplateClicked(TWeakObjectPtr<UCurveFloat> FloatCurveAssetWeak)
	{
		UCurveFloat* FloatCurveAsset = FloatCurveAssetWeak.Get();
		if (FloatCurveAsset != nullptr)
		{
			TArray<FCurveModelID> CurveModelIdsToSet;
			if(CurveEditor->GetRootTreeItems().Num() == 1)
			{
				const FCurveEditorTreeItem& TreeItem = CurveEditor->GetTreeItem(CurveEditor->GetRootTreeItems()[0]);
				for(const FCurveModelID& CurveModelId : TreeItem.GetCurves())
				{
					CurveModelIdsToSet.Add(CurveModelId);
				}
			}
			else
			{
				for (const TPair<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& TreeItemSelectionState : CurveEditor->GetTreeSelection())
				{
					if (TreeItemSelectionState.Value != ECurveEditorTreeSelectionState::None)
					{
						const FCurveEditorTreeItem& TreeItem = CurveEditor->GetTreeItem(TreeItemSelectionState.Key);
						for (const FCurveModelID& CurveModelId : TreeItem.GetCurves())
						{
							CurveModelIdsToSet.Add(CurveModelId);
						}
					}
				}
			}

			if (CurveModelIdsToSet.Num() > 0)
			{
				FScopedTransaction ApplyTemplateTransaction(LOCTEXT("ApplyCurveTemplateTransaction", "Apply curve template"));
				for (const FCurveModelID& CurveModelId : CurveModelIdsToSet)
				{
					FCurveModel* CurveModel = CurveEditor->GetCurves()[CurveModelId].Get();
					if (CurveModel != nullptr)
					{
						TArray<FKeyHandle> KeyHandles;
						CurveModel->GetKeys(*CurveEditor.Get(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), KeyHandles);
						CurveModel->RemoveKeys(KeyHandles);

						const FRichCurve& FloatCurve = FloatCurveAsset->FloatCurve;
						for (auto KeyIterator = FloatCurve.GetKeyHandleIterator(); KeyIterator; ++KeyIterator)
						{
							const FRichCurveKey& Key = FloatCurve.GetKey(*KeyIterator);
							FKeyPosition KeyPosition;
							KeyPosition.InputValue = Key.Time;
							KeyPosition.OutputValue = Key.Value;
							FKeyAttributes KeyAttributes;
							KeyAttributes.SetInterpMode(Key.InterpMode);
							KeyAttributes.SetTangentMode(Key.TangentMode);
							KeyAttributes.SetArriveTangent(Key.ArriveTangent);
							KeyAttributes.SetLeaveTangent(Key.LeaveTangent);
							CurveModel->AddKey(KeyPosition, KeyAttributes);
						}
					}
				}
				CurveEditor->ZoomToFit();
			}
		}
		return FReply::Handled();
	}

private:
	TSharedPtr<FCurveEditor> CurveEditor;
};

class SNiagaraDataInterfaceCurveEditor : public SCompoundWidget
{
private:
	struct FDataInterfaceCurveEditorBounds : public ICurveEditorBounds
	{
		FDataInterfaceCurveEditorBounds(TSharedRef<FNiagaraStackCurveEditorOptions> InStackCurveEditorOptions)
			: StackCurveEditorOptions(InStackCurveEditorOptions)
		{
		}

		virtual void GetInputBounds(double& OutMin, double& OutMax) const
		{
			OutMin = StackCurveEditorOptions->GetViewMinInput();
			OutMax = StackCurveEditorOptions->GetViewMaxInput();
		}

		virtual void SetInputBounds(double InMin, double InMax)
		{
			StackCurveEditorOptions->SetInputViewRange((float)InMin, (float)InMax);
		}

	private:
		TSharedRef<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions;
	};

public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceCurveEditor) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		TArray<TSharedRef<IPropertyHandle>> InCurveProperties,
		TSharedRef<FNiagaraStackCurveEditorOptions> InStackCurveEditorOptions)
	{
		CurveProperties = InCurveProperties;
		StackCurveEditorOptions = InStackCurveEditorOptions;

		TArray<UObject*> OuterObjects;
		CurveProperties[0]->GetOuterObjects(OuterObjects);
		UNiagaraDataInterfaceCurveBase* CurveOwnerObject = Cast<UNiagaraDataInterfaceCurveBase>(OuterObjects[0]);
		if (CurveOwnerObject == nullptr)
		{
			return;
		}

		if (StackCurveEditorOptions->GetNeedsInitializeView())
		{
			InitializeView();
		}

		CurveEditor = MakeShared<FCurveEditor>();
		FCurveEditorInitParams InitParams;
		CurveEditor->InitCurveEditor(InitParams);
		CurveEditor->GridLineLabelFormatXAttribute = LOCTEXT("GridXLabelFormat", "{0}");

		// Initialize our bounds at slightly larger than default to avoid clipping the tabs on the color widget.
		TUniquePtr<ICurveEditorBounds> EditorBounds = MakeUnique<FDataInterfaceCurveEditorBounds>(InStackCurveEditorOptions);
		CurveEditor->SetBounds(MoveTemp(EditorBounds));

		TSharedPtr<SCurveEditorTree> CurveEditorTree;
		CurveEditorPanel = SNew(SCurveEditorPanel, CurveEditor.ToSharedRef())
			.MinimumViewPanelHeight(50.0f)
			.TreeSplitterWidth(0.2f)
			.ContentSplitterWidth(0.8f)
			.TreeContent()
			[
				CurveProperties.Num() == 1
					? SNullWidget::NullWidget
					: SAssignNew(CurveEditorTree, SCurveEditorTree, CurveEditor)
					.SelectColumnWidth(0)
			];

		TArray<UNiagaraDataInterfaceCurveBase::FCurveData> CurveDatas;
		CurveOwnerObject->GetCurveData(CurveDatas);
		TArray<FCurveEditorTreeItemID> TreeItemIds;
		for (const UNiagaraDataInterfaceCurveBase::FCurveData& CurveData : CurveDatas)
		{
			TSharedRef<FNiagaraCurveDetailsTreeItem> TreeItem = MakeShared<FNiagaraCurveDetailsTreeItem>(CurveOwnerObject, CurveData);
			TreeItem->OnCurveChanged().AddSP(this, &SNiagaraDataInterfaceCurveEditor::CurveChanged, TWeakPtr<FNiagaraCurveDetailsTreeItem>(TreeItem));
			FCurveEditorTreeItem* NewItem = CurveEditor->AddTreeItem(FCurveEditorTreeItemID::Invalid());
			NewItem->SetStrongItem(TreeItem);
			TreeItemIds.Add(NewItem->GetID());
			for (const FCurveModelID& CurveModel : NewItem->GetOrCreateCurves(CurveEditor.Get()))
			{
				CurveEditor->PinCurve(CurveModel);
			}
		}

		float KeySelectorLeftPadding = CurveProperties.Num() == 1 ? 0 : 7;
		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(KeySelectorLeftPadding, 3, 0, 3)
			[
				SNew(SNiagaraCurveTemplateBar, CurveEditor.ToSharedRef())
			]
			+ SVerticalBox::Slot()
			.Padding(0, 0, 0, 5)
			[
				CurveEditorPanel.ToSharedRef()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(KeySelectorLeftPadding, 0, 8, 0)
				[
					SNew(SNiagaraDataInterfaceCurveKeySelector, CurveEditor, TreeItemIds, CurveEditorTree)
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(0, 0, 3, 0)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("KeyLabel", "Key Data"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.FillWidth(1.0f)
				[
					CurveEditorPanel->GetKeyDetailsView().ToSharedRef()
				]
			]
		];
	}

	TSharedPtr<FUICommandList> GetCommands()
	{
		return CurveEditor->GetCommands();
	}

private:
	void InitializeView()
	{
		bool bHasKeys = false;
		float ViewMinInput = TNumericLimits<float>::Max();
		float ViewMaxInput = TNumericLimits<float>::Lowest();
		float ViewMinOutput = TNumericLimits<float>::Max();
		float ViewMaxOutput = TNumericLimits<float>::Lowest();

		for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
		{
			FRealCurve* Curve = GetCurveFromPropertyHandle(CurveProperty);
			for(auto KeyIterator = Curve->GetKeyHandleIterator(); KeyIterator; ++KeyIterator)
			{
				float KeyTime = Curve->GetKeyTime(*KeyIterator);
				float KeyValue = Curve->GetKeyValue(*KeyIterator);
				ViewMinInput = FMath::Min(ViewMinInput, KeyTime);
				ViewMaxInput = FMath::Max(ViewMaxInput, KeyTime);
				ViewMinOutput = FMath::Min(ViewMinOutput, KeyValue);
				ViewMaxOutput = FMath::Max(ViewMaxOutput, KeyValue);
				bHasKeys = true;
			}
		}

		if (bHasKeys == false)
		{
			ViewMinInput = 0;
			ViewMaxInput = 1;
			ViewMinOutput = 0;
			ViewMaxOutput = 1;
		}

		if (FMath::IsNearlyEqual(ViewMinInput, ViewMaxInput))
		{
			if (FMath::IsWithinInclusive(ViewMinInput, 0.0f, 1.0f))
			{
				ViewMinInput = 0;
				ViewMaxInput = 1;
			}
			else
			{
				ViewMinInput -= 0.5f;
				ViewMaxInput += 0.5f;
			}
		}

		if (FMath::IsNearlyEqual(ViewMinOutput, ViewMaxOutput))
		{
			if (FMath::IsWithinInclusive(ViewMinOutput, 0.0f, 1.0f))
			{
				ViewMinOutput = 0;
				ViewMaxOutput = 1;
			}
			else
			{
				ViewMinOutput -= 0.5f;
				ViewMaxOutput += 0.5f;
			}
		}

		float ViewInputRange = ViewMaxInput - ViewMinInput;
		float ViewOutputRange = ViewMaxOutput - ViewMinOutput;
		float ViewInputPadding = ViewInputRange * .05f;
		float ViewOutputPadding = ViewOutputRange * .05f;

		StackCurveEditorOptions->InitializeView(
			ViewMinInput - ViewInputPadding,
			ViewMaxInput + ViewInputPadding,
			ViewMinOutput - ViewOutputPadding,
			ViewMaxOutput + ViewOutputPadding);
	}

	void CurveChanged(TWeakPtr<FNiagaraCurveDetailsTreeItem> TreeItemWeak)
	{
		TSharedPtr<FNiagaraCurveDetailsTreeItem> TreeItem = TreeItemWeak.Pin();
		if(TreeItem.IsValid())
		{
			UNiagaraDataInterfaceCurveBase* EditedCurveOwner = Cast<UNiagaraDataInterfaceCurveBase>(TreeItem->GetCurveOwnerObject());
			if(EditedCurveOwner != nullptr)
			{
				EditedCurveOwner->UpdateLUT(); // we need this done before notify change because of the internal copy methods
				for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
				{
					if (GetCurveFromPropertyHandle(CurveProperty) == TreeItem->GetCurve())
					{
						CurveProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
						break;
					}
				}
			}
		}
	}

private:
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	TSharedPtr<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions;

	TSharedPtr<FCurveEditor> CurveEditor;
	TSharedPtr<SCurveEditorPanel> CurveEditorPanel;
};

class FNiagaraColorCurveDataInterfaceCurveOwner : public FCurveOwnerInterface
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnColorCurveChanged, TArray<FRichCurve*> /* ChangedCurves */);

public:
	FNiagaraColorCurveDataInterfaceCurveOwner(UNiagaraDataInterfaceColorCurve& InDataInterface)
	{
		DataInterfaceWeak = &InDataInterface;

		ConstCurves.Add(FRichCurveEditInfoConst(&InDataInterface.RedCurve, "Red"));
		ConstCurves.Add(FRichCurveEditInfoConst(&InDataInterface.GreenCurve, "Green"));
		ConstCurves.Add(FRichCurveEditInfoConst(&InDataInterface.BlueCurve, "Blue"));
		ConstCurves.Add(FRichCurveEditInfoConst(&InDataInterface.AlphaCurve, "Alpha"));

		Curves.Add(FRichCurveEditInfo(&InDataInterface.RedCurve, "Red"));
		Curves.Add(FRichCurveEditInfo(&InDataInterface.GreenCurve, "Green"));
		Curves.Add(FRichCurveEditInfo(&InDataInterface.BlueCurve, "Blue"));
		Curves.Add(FRichCurveEditInfo(&InDataInterface.AlphaCurve, "Alpha"));

		Owners.Add(&InDataInterface);
	}

	/** FCurveOwnerInterface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override
	{
		return ConstCurves;
	}

	virtual TArray<FRichCurveEditInfo> GetCurves() override
	{
		return Curves;
	}

	virtual void ModifyOwner() override
	{
		if (DataInterfaceWeak.IsValid())
		{
			DataInterfaceWeak->Modify();
		}
	}

	virtual TArray<const UObject*> GetOwners() const override
	{
		return Owners;
	}

	virtual void MakeTransactional() override { }

	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos) override
	{
		TArray<FRichCurve*> ChangedCurves;
		for (const FRichCurveEditInfo& ChangedCurveEditInfo : ChangedCurveEditInfos)
		{
			ChangedCurves.Add((FRichCurve*)ChangedCurveEditInfo.CurveToEdit);
		}
		if(ChangedCurves.Num() > 0)
		{
			OnColorCurveChangedDelegate.Broadcast(ChangedCurves);
		}
	}

	virtual bool IsLinearColorCurve() const override 
	{
		return true;
	}

	virtual FLinearColor GetLinearColorValue(float InTime) const override
	{
		if (DataInterfaceWeak.IsValid())
		{
			return FLinearColor(
				DataInterfaceWeak->RedCurve.Eval(InTime),
				DataInterfaceWeak->GreenCurve.Eval(InTime),
				DataInterfaceWeak->BlueCurve.Eval(InTime),
				DataInterfaceWeak->AlphaCurve.Eval(InTime));
		}
		return FLinearColor::White;
	}

	virtual bool HasAnyAlphaKeys() const override 
	{ 
		return DataInterfaceWeak.IsValid() && DataInterfaceWeak->AlphaCurve.GetNumKeys() != 0;
	}

	virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override 
	{
		return Curves.Contains(CurveInfo);
	}

	virtual FLinearColor GetCurveColor(FRichCurveEditInfo CurveInfo) const override 
	{
		return FLinearColor::White;
	}

	FOnColorCurveChanged& OnColorCurveChanged()
	{
		return OnColorCurveChangedDelegate;
	}

private:
	TWeakObjectPtr<UNiagaraDataInterfaceColorCurve> DataInterfaceWeak;
	TArray<FRichCurveEditInfoConst> ConstCurves;
	TArray<FRichCurveEditInfo> Curves;
	TArray<const UObject*> Owners;
	FOnColorCurveChanged OnColorCurveChangedDelegate;
};

class SNiagaraDataInterfaceGradientEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceGradientEditor) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		UNiagaraDataInterfaceCurveBase* InDataInterface,
		TArray<TSharedRef<IPropertyHandle>> InCurveProperties)
	{
		UNiagaraDataInterfaceColorCurve* ColorCurveDataInterface = Cast<UNiagaraDataInterfaceColorCurve>(InDataInterface);
		if(ensureMsgf(ColorCurveDataInterface != nullptr, TEXT("Only UNiagaraDataInterfaceColorCurve is currently supported.")))
		{
			ColorCurveDataInterfaceWeak = ColorCurveDataInterface;
			CurveProperties = InCurveProperties;
			ColorCurveOwner = MakeShared<FNiagaraColorCurveDataInterfaceCurveOwner>(*ColorCurveDataInterface);
			ColorCurveOwner->OnColorCurveChanged().AddSP(this, &SNiagaraDataInterfaceGradientEditor::ColorCurveChanged);
			TSharedRef<SColorGradientEditor> GradientEditor = SNew(SColorGradientEditor)
				.ViewMinInput(0.0f)
				.ViewMaxInput(1.0f);

			GradientEditor->SetCurveOwner(ColorCurveOwner.Get());
			ChildSlot
			[
				SNew(SBox)
				.Padding(FMargin(0, 0, 10, 0))
				[
					GradientEditor
				]
			];
		}
	}

private:
	void ClampKeyTimes(FRichCurve& Curve)
	{
		for (auto It = Curve.GetKeyHandleIterator(); It; ++It)
		{
			if (Curve.GetKeyTime(*It) < 0)
			{
				Curve.SetKeyTime(*It, 0);
			}
			if (Curve.GetKeyTime(*It) > 1)
			{
				Curve.SetKeyTime(*It, 1);
			}
		}
	}

	void ColorCurveChanged(TArray<FRichCurve*> ChangedCurves)
	{
		UNiagaraDataInterfaceColorCurve* ColorCurveDataInterface = ColorCurveDataInterfaceWeak.Get();
		if(ColorCurveDataInterface != nullptr)
		{
			ClampKeyTimes(ColorCurveDataInterface->RedCurve);
			ClampKeyTimes(ColorCurveDataInterface->GreenCurve);
			ClampKeyTimes(ColorCurveDataInterface->BlueCurve);
			ClampKeyTimes(ColorCurveDataInterface->AlphaCurve);

			ColorCurveDataInterface->UpdateLUT();
			for (FRichCurve* ChangedCurve : ChangedCurves)
			{
				for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
				{
					if (GetCurveFromPropertyHandle(CurveProperty) == ChangedCurve)
					{
						CurveProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
					}
				}
			}
		}
	}

private:
	TWeakObjectPtr<UNiagaraDataInterfaceColorCurve> ColorCurveDataInterfaceWeak;
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	TSharedPtr<FNiagaraColorCurveDataInterfaceCurveOwner> ColorCurveOwner;
};



// Curve Base
void FNiagaraDataInterfaceCurveDetailsBase::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomDetailBuilder = &DetailBuilder;
	FNiagaraDataInterfaceDetailsBase::CustomizeDetails(DetailBuilder);

	// Only support single objects.
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}

	UNiagaraDataInterfaceCurveBase* CurveDataInterface = Cast<UNiagaraDataInterfaceCurveBase>(ObjectsBeingCustomized[0].Get());
	TSharedRef<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions = FNiagaraEditorWidgetsModule::Get().GetOrCreateStackCurveEditorOptionsForObject(
		ObjectsBeingCustomized[0].Get(), GetDefaultHeight());

	CurveDataInterfaceWeak = CurveDataInterface;
	StackCurveEditorOptionsWeak = StackCurveEditorOptions;

	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	GetCurveProperties(DetailBuilder, CurveProperties);

	// Make sure all property handles are valid.
	for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
	{
		if (CurveProperty->IsValidHandle() == false)
		{
			return;
		}
	}

	for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
	{
		CurveProperty->MarkHiddenByCustomization();
	}

	TSharedPtr<SNiagaraDataInterfaceCurveEditor> CurveEditor;
	TSharedRef<SWidget> CurveDataInterfaceCurveEditor = 
		SNew(SVerticalResizeBox)
		.ContentHeight(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::GetHeight)
		.ContentHeightChanged(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::SetHeight)
		.Content()
		[
			SAssignNew(CurveEditor, SNiagaraDataInterfaceCurveEditor, CurveProperties, StackCurveEditorOptions)
		];

	TSharedPtr<SWidget> CurveDataInterfaceEditor;
	if (GetIsColorCurve())
	{
		CurveDataInterfaceEditor = 
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &FNiagaraDataInterfaceCurveDetailsBase::GetGradientCurvesSwitcherIndex)
			+ SWidgetSwitcher::Slot()
			[
				SNew(SNiagaraDataInterfaceGradientEditor, CurveDataInterface, CurveProperties)
			]
			+ SWidgetSwitcher::Slot()
			[
				CurveDataInterfaceCurveEditor
			];
	}
	else
	{
		CurveDataInterfaceEditor = CurveDataInterfaceCurveEditor;
	}

	FToolBarBuilder ToolBarBuilder(CurveEditor->GetCommands(), FMultiBoxCustomization::None, nullptr, true);
	ToolBarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolbar");

	ToolBarBuilder.AddComboButton(
		FUIAction(), 
		FOnGetContent::CreateSP(this, &FNiagaraDataInterfaceCurveDetailsBase::GetCurveToCopyMenu),
		NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "Import", "Import"),
		NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CopyCurveAsset", "Import data from another Curve asset"),
		FSlateIcon(FNiagaraEditorWidgetsStyle::Get().GetStyleSetName(), "NiagaraEditor.CurveDetails.Import"));

	ToolBarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &FNiagaraDataInterfaceCurveDetailsBase::OnShowInCurveEditor)),
		NAME_None,
		LOCTEXT("CurveOverviewLabel", "Curve Overview"),
		LOCTEXT("ShowInCurveOverviewToolTip", "Show this curve in the curve overview."),
		FSlateIcon(FNiagaraEditorWidgetsStyle::Get().GetStyleSetName(), "NiagaraEditor.CurveDetails.ShowInOverview"));

	ToolBarBuilder.AddSeparator();

	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ToggleInputSnapping);
	ToolBarBuilder.AddToolBarButton(FCurveEditorCommands::Get().ToggleOutputSnapping);

	if (GetIsColorCurve())
	{
		ToolBarBuilder.AddSeparator();

		ToolBarBuilder.AddWidget(
			SNew(SBox)
			.Padding(FMargin(0, 0, 2, 0))
			[
				SNew(SSegmentedControl<bool>)
				.OnValueChanged(this, &FNiagaraDataInterfaceCurveDetailsBase::SetGradientVisibility)
				.Value(this, &FNiagaraDataInterfaceCurveDetailsBase::IsGradientVisible)
				+ SSegmentedControl<bool>::Slot(false)
				.Text(LOCTEXT("CurvesButtonLabel", "Curves"))
				.ToolTip(LOCTEXT("ShowCurvesToolTip", "Show the curve editor."))
				+ SSegmentedControl<bool>::Slot(true)
				.Text(LOCTEXT("GradientButtonLabel", "Gradient"))
				.ToolTip(LOCTEXT("ShowGradientToolTip", "Show the gradient editor."))
			]);
	}

	IDetailCategoryBuilder& CurveCategory = DetailBuilder.EditCategory("Curve");
	CurveCategory.AddCustomRow(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CurveOptions", "Options")).WholeRowContent() [ ToolBarBuilder.MakeWidget() ];
	CurveCategory.AddCustomRow(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CurveFilterText", "Curve")).WholeRowContent() [ CurveDataInterfaceEditor.ToSharedRef() ];
}

void FNiagaraDataInterfaceCurveDetailsBase::SetGradientVisibility(bool bInShowGradient)
{
	TSharedPtr<FNiagaraStackCurveEditorOptions> CurveEditorOptions = StackCurveEditorOptionsWeak.Pin();
	if (CurveEditorOptions.IsValid())
	{
		CurveEditorOptions->SetIsGradientVisible(bInShowGradient);
	}
}

int32 FNiagaraDataInterfaceCurveDetailsBase::GetGradientCurvesSwitcherIndex() const
{
	return StackCurveEditorOptionsWeak.IsValid() && StackCurveEditorOptionsWeak.Pin()->GetIsGradientVisible() ? 0 : 1;
}

bool FNiagaraDataInterfaceCurveDetailsBase::IsGradientVisible() const
{
	return StackCurveEditorOptionsWeak.IsValid() ? StackCurveEditorOptionsWeak.Pin()->GetIsGradientVisible() : false;
}



void FNiagaraDataInterfaceCurveDetailsBase::OnShowInCurveEditor() const
{
	UNiagaraDataInterfaceCurveBase* CurveDataInterface = CurveDataInterfaceWeak.Get();
	if (CurveDataInterface != nullptr)
	{
		UNiagaraSystem* OwningSystem = CurveDataInterface->GetTypedOuter<UNiagaraSystem>();
		if (OwningSystem != nullptr)
		{
			TSharedPtr<FNiagaraSystemViewModel> SystemViewModel = FNiagaraEditorModule::Get().GetExistingViewModelForSystem(OwningSystem);
			if(SystemViewModel.IsValid())
			{
				SystemViewModel->GetCurveSelectionViewModel()->FocusAndSelectCurveDataInterface(*CurveDataInterface);
			}
		}
	}
}

void FNiagaraDataInterfaceCurveDetailsBase::ImportSelectedAsset(UObject* SelectedAsset)
{
	if (CurveDataInterfaceWeak.IsValid() == false)
	{
		return;
	}

	TArray<FRichCurve> FloatCurves;
	GetFloatCurvesFromAsset(SelectedAsset, FloatCurves);
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	GetCurveProperties(*CustomDetailBuilder, CurveProperties);
	if (FloatCurves.Num() == CurveProperties.Num())
	{
		FScopedTransaction ImportTransaction(LOCTEXT("ImportCurveTransaction", "Import curve"));
		CurveDataInterfaceWeak->Modify();
		for (int i = 0; i < CurveProperties.Num(); i++)
		{
			if (CurveProperties[i]->IsValidHandle())
			{
				*GetCurveFromPropertyHandle(CurveProperties[i]) = FloatCurves[i];
			}
		}
		CurveDataInterfaceWeak->UpdateLUT(); // we need this done before notify change because of the internal copy methods
		for (auto CurveProperty : CurveProperties)
		{
			CurveProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}
}

TSharedRef<SWidget> FNiagaraDataInterfaceCurveDetailsBase::GetCurveToCopyMenu()
{
	FTopLevelAssetPath ClassName = GetSupportedAssetClassName();
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FNiagaraDataInterfaceCurveDetailsBase::CurveToCopySelected);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassPaths.Add(ClassName);
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	return SNew(SBox)
		.WidthOverride(300.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
	
	return SNullWidget::NullWidget;
}

void FNiagaraDataInterfaceCurveDetailsBase::CurveToCopySelected(const FAssetData& AssetData)
{
	ImportSelectedAsset(AssetData.GetAsset());
	FSlateApplication::Get().DismissAllMenus();
}

// Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceCurveDetails>();
}

void FNiagaraDataInterfaceCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& CurveProperties) const
{
	CurveProperties.Add(DetailBuilder.GetProperty(FName("Curve"), UNiagaraDataInterfaceCurve::StaticClass()));
}

FTopLevelAssetPath FNiagaraDataInterfaceCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveFloat::StaticClass()->GetClassPathName();
}

void FNiagaraDataInterfaceCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveFloat* CurveAsset = Cast<UCurveFloat>(SelectedAsset);
	FloatCurves.Add(CurveAsset->FloatCurve);
}

// Vector 2D Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector2DCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector2DCurveDetails>();
}

void FNiagaraDataInterfaceVector2DCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVector2DCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVector2DCurve::StaticClass()));
}

FTopLevelAssetPath FNiagaraDataInterfaceVector2DCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveVector::StaticClass()->GetClassPathName();
}

void FNiagaraDataInterfaceVector2DCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveVector* CurveAsset = Cast<UCurveVector>(SelectedAsset);
	for (int i = 0; i < 2; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}


// Vector Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVectorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVectorCurveDetails>();
}

void FNiagaraDataInterfaceVectorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("ZCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
}

FTopLevelAssetPath FNiagaraDataInterfaceVectorCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveVector::StaticClass()->GetClassPathName();
}

void FNiagaraDataInterfaceVectorCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveVector* CurveAsset = Cast<UCurveVector>(SelectedAsset);
	for (int i = 0; i < 3; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}


// Vector 4 Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector4CurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector4CurveDetails>();
}

void FNiagaraDataInterfaceVector4CurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("ZCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("WCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
}

FTopLevelAssetPath FNiagaraDataInterfaceVector4CurveDetails::GetSupportedAssetClassName() const
{
	return UCurveLinearColor::StaticClass()->GetClassPathName();
}

void FNiagaraDataInterfaceVector4CurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveLinearColor* CurveAsset = Cast<UCurveLinearColor>(SelectedAsset);
	for (int i = 0; i < 4; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}

// Color Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceColorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceColorCurveDetails>();
}

void FNiagaraDataInterfaceColorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("RedCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("GreenCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("BlueCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("AlphaCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
}

FTopLevelAssetPath FNiagaraDataInterfaceColorCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveLinearColor::StaticClass()->GetClassPathName();
}

void FNiagaraDataInterfaceColorCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveLinearColor* CurveAsset = Cast<UCurveLinearColor>(SelectedAsset);
	for (int i = 0; i < 4; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}
#undef LOCTEXT_NAMESPACE