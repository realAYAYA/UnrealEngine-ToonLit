// Copyright Epic Games, Inc. All Rights Reserved.

#include "SChooserTableRow.h"
#include "Chooser.h"
#include "ChooserEditorStyle.h"
#include "ChooserTableEditor.h"
#include "IContentBrowserSingleton.h"
#include "IObjectChooser.h"
#include "ObjectChooserWidgetFactories.h"
#include "ObjectChooser_Asset.h"
#include "SChooserRowHandle.h"
#include "ScopedTransaction.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "ChooserTableRow"

namespace UE::ChooserEditor
{
	void SChooserTableRow::Construct(const FArguments& Args, const TSharedRef<STableViewBase>& OwnerTableView)
	{
		RowIndex = Args._Entry;
		Chooser = Args._Chooser;
		Editor = Args._Editor;

		SMultiColumnTableRow<TSharedPtr<FChooserTableRow>>::Construct(
			FSuperRowType::FArguments(),
			OwnerTableView
		);

		if (RowIndex->RowIndex >=0)
		SetContent(
				SNew(SOverlay)
						+ SOverlay::Slot()
						[
							Content.Pin().ToSharedRef()
						]
						+ SOverlay::Slot().VAlign(VAlign_Bottom)
						[
							SNew(SSeparator)
							.SeparatorImage(FAppStyle::GetBrush("PropertyEditor.HorizontalDottedLine"))
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(bDropSupported ? EStyleColor::Select : EStyleColor::Error); })
							.Visibility_Lambda([this]() { return bDragActive && !bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						]
						+ SOverlay::Slot().VAlign(VAlign_Top)
						[
							SNew(SSeparator)
							.SeparatorImage(FAppStyle::GetBrush("PropertyEditor.HorizontalDottedLine"))
							.ColorAndOpacity_Lambda([this]() { return FSlateColor(bDropSupported ? EStyleColor::Select : EStyleColor::Error); }) 
							.Visibility_Lambda([this]() { return bDragActive && bDropAbove ? EVisibility::Visible : EVisibility::Hidden; })
						]
			);
		else if (RowIndex->RowIndex == SpecialIndex_Fallback)
		{
			SetContent(
					SNew(SOverlay)
							+ SOverlay::Slot()
							[
								Content.Pin().ToSharedRef()
							]
							+ SOverlay::Slot().VAlign(VAlign_Top)
							[
								SNew(SSeparator)
								.SeparatorImage(FAppStyle::GetBrush("PropertyEditor.HorizontalDottedLine"))
								.ColorAndOpacity_Lambda([this]() { return FSlateColor(bDropSupported ? EStyleColor::Select : EStyleColor::Error); }) 
								.Visibility_Lambda([this]() { return bDragActive ? EVisibility::Visible : EVisibility::Hidden; })
							]
				);
		}
	}

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list view. */
	TSharedRef<SWidget> SChooserTableRow::GenerateWidgetForColumn(const FName& ColumnName)
	{
		static FName Result = "Result";
		static FName Handles = "Handles";
	
		if (Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
		{
			if (ColumnName == Handles)
			{
				// row drag handle
				return SNew(SChooserRowHandle).ChooserEditor(Editor).RowIndex(RowIndex->RowIndex);
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex->RowIndex].GetMutableMemory(), Chooser->ResultsStructs[RowIndex->RowIndex].GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this, RowIndex=RowIndex->RowIndex](const UScriptStruct* ChosenStruct)
				{
					UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->ResultsStructs[RowIndex].InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->ResultsStructs[RowIndex].GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder);
				}),
				&CacheBorder
				);
			
				return ResultWidget.ToSharedRef();
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), RowIndex->RowIndex);
				
					if (ColumnWidget.IsValid())
					{
						return SNew(SOverlay)
						+ SOverlay::Slot()
						[
							ColumnWidget.ToSharedRef()
						]
						+ SOverlay::Slot()
						[
							SNew(SColorBlock).Visibility(EVisibility::HitTestInvisible).Color_Lambda(
									[this,Column]()
									{
										if (Chooser->bDebugTestValuesValid && Column->HasFilters())
										{
											if (Column->EditorTestFilter(RowIndex->RowIndex))
											{
												return FLinearColor(0.0,1.0,0.0,0.30);
											}
											else
											{
												return FLinearColor(1.0,0.0,0.0,0.20);
											}
										}
										return FLinearColor::Transparent;
									})
						];
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_Fallback)
		{
			if (ColumnName == Handles)
			{
				return SNew(SBox).Padding(0.0f) .HAlign(HAlign_Center) .VAlign(VAlign_Center) .WidthOverride(16.0f)
					[
						SNew(SImage)
						.Image(FChooserEditorStyle::Get().GetBrush("ChooserEditor.FallbackIcon"))
						.ToolTipText(LOCTEXT("FallbackTooltip","Fallback result:  Returned if all rows failed."))
					];
			}
			else if (ColumnName == Result) 
			{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				TSharedPtr<SWidget> ResultWidget = FObjectChooserWidgetFactories::CreateWidget(false,ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), Chooser->FallbackResult.GetScriptStruct(), ContextOwner->OutputObjectType,
				FOnStructPicked::CreateLambda([this](const UScriptStruct* ChosenStruct)
				{
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
					const FScopedTransaction Transaction(LOCTEXT("Change Row Result Type", "Change Row Result Type"));
					Chooser->Modify(true);
					Chooser->FallbackResult.InitializeAs(ChosenStruct);
					FObjectChooserWidgetFactories::CreateWidget(false, ContextOwner, FObjectChooserBase::StaticStruct(), Chooser->FallbackResult.GetMutableMemory(), ChosenStruct, ContextOwner->OutputObjectType, FOnStructPicked(), &CacheBorder
						,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)"));
				}),
				&CacheBorder
				,FChooserWidgetValueChanged(), LOCTEXT("Fallback Result", "Fallback Result: (None)")
				);
				
				return ResultWidget.ToSharedRef();
			}
			else
			{
				const int ColumnIndex = ColumnName.GetNumber() - 1;
				if (ColumnIndex < Chooser->ColumnsStructs.Num() && ColumnIndex >=0)
				{
					FChooserColumnBase* Column = &Chooser->ColumnsStructs[ColumnIndex].GetMutable<FChooserColumnBase>();
					const UStruct * ColumnStruct = Chooser->ColumnsStructs[ColumnIndex].GetScriptStruct();

					TSharedPtr<SWidget> ColumnWidget = FObjectChooserWidgetFactories::CreateColumnWidget(Column, ColumnStruct, Chooser->GetContextOwner(), -2);
				
					if (ColumnWidget.IsValid())
					{
						return ColumnWidget.ToSharedRef();
					}
				}
			}
		}
		else if (RowIndex->RowIndex == SpecialIndex_AddRow)
		{
			// on the row past the end, show an Add button in the result column
			if (ColumnName == Result)
			{
				return Editor->GetCreateRowComboButton().ToSharedRef();
			}
		}

		return SNullWidget::NullWidget;
	}

	void SChooserTableRow::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		bDropSupported = false;
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			bDropSupported = true;
		}
		else if (TSharedPtr<FAssetDragDropOp> ContentDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>())
		{
			if (Chooser->ResultType == EObjectChooserResultType::ObjectResult)
			{
				bDropSupported = true;
			
				UChooserTable* ContextOwner = Chooser->GetContextOwner();
				if (ContextOwner->OutputObjectType) // if OutputObjectType is null, then any kind of object is supported, don't need to check all of them
				{
					for (const FAssetData& Asset : ContentDragDropOp->GetAssets())
					{
						const UClass* AssetClass = Asset.GetClass();

						if(AssetClass->IsChildOf(UChooserTable::StaticClass()))
						{
							const UChooserTable* DraggedChooserTable = Cast<UChooserTable>(Asset.GetAsset());

							// verify dragged chooser result type matches this chooser result type
							if (DraggedChooserTable->ResultType == EObjectChooserResultType::ClassResult
							    || DraggedChooserTable->OutputObjectType == nullptr
							    || !DraggedChooserTable->OutputObjectType->IsChildOf(ContextOwner->OutputObjectType))
							{
								bDropSupported = false;
								break;
							}
						}
						else if(!AssetClass->IsChildOf(ContextOwner->OutputObjectType))
                        {
							bDropSupported = false;
							break;
                        }
					}
				}
			}
		}
		
		float Center = MyGeometry.Position.Y + MyGeometry.Size.Y;
		bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
		bDragActive = true;
	}
	void SChooserTableRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
	{
		bDragActive = false;
	}

	FReply SChooserTableRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		if (bDropSupported)
		{
			bDragActive = true;
			float Center = MyGeometry.AbsolutePosition.Y + MyGeometry.Size.Y/2;
			bDropAbove = DragDropEvent.GetScreenSpacePosition().Y < Center;
			return FReply::Handled();
		}
		else
		{
			return FReply::Unhandled();
		}
	}

	FReply SChooserTableRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		bDragActive = false;
		
		if (!bDropSupported)
		{
			return FReply::Unhandled();
		}
		
		if (TSharedPtr<FChooserRowDragDropOp> Operation = DragDropEvent.GetOperationAs<FChooserRowDragDropOp>())
		{
			if (Chooser == Operation->ChooserEditor->GetChooser())
			{
				int NewRowIndex;
				if (!Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
				{
					// for special (negative) indices, move to the end
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, Chooser->ResultsStructs.Num());
				}
				else if (bDropAbove)
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex);
				}
				else
				{
					NewRowIndex = Editor->MoveRow(Operation->RowIndex, RowIndex->RowIndex+1);
				}
				Editor->SelectRow(NewRowIndex);
			}
		}
		else if (TSharedPtr<FAssetDragDropOp> ContentDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>())
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("DragDropAssets","Drag and Drop Assets into Chooser"));
			Chooser->Modify(true);
			
			if (Chooser->ResultType == EObjectChooserResultType::ObjectResult)
			{
				int InsertRowIndex = RowIndex->RowIndex;
				if (!Chooser->ResultsStructs.IsValidIndex(RowIndex->RowIndex))
				{
					// for special (negative) indices, move to the end
					InsertRowIndex = Chooser->ResultsStructs.Num();
				}
				else if (!bDropAbove)
				{
					InsertRowIndex = RowIndex->RowIndex + 1;
				}
				
				if (bDropSupported)
				{
					TArray<FInstancedStruct> NewResults;
					const TArray<FAssetData>& AssetList = ContentDragDropOp->GetAssets();
					NewResults.Reserve(AssetList.Num());
					
					for (const FAssetData& Asset : AssetList)
					{
						const UClass* AssetClass = Asset.GetClass();
						
						NewResults.SetNum(NewResults.Num()+1);
						FInstancedStruct& NewResult = NewResults.Last();

						if(AssetClass->IsChildOf(UChooserTable::StaticClass()))
						{
							NewResult.InitializeAs(FEvaluateChooser::StaticStruct());
							NewResult.GetMutable<FEvaluateChooser>().Chooser = Cast<UChooserTable>(Asset.GetAsset());
						}
						else
						{
							UChooserTable* ContextOwner = Chooser->GetContextOwner();
							if (ContextOwner->OutputObjectType == nullptr || ensure(AssetClass->IsChildOf(ContextOwner->OutputObjectType)))
							{
								NewResult.InitializeAs(FAssetChooser::StaticStruct());
								NewResult.GetMutable<FAssetChooser>().Asset = Asset.GetAsset();
							}
						}
					}

					Chooser->ResultsStructs.Insert(NewResults, InsertRowIndex);
					
					// Make sure each column has the same number of row datas as there are results
					for(FInstancedStruct& ColumnData : Chooser->ColumnsStructs)
					{
						FChooserColumnBase& Column = ColumnData.GetMutable<FChooserColumnBase>();
						Column.InsertRows(InsertRowIndex, NewResults.Num());
					}

					Editor->RefreshAll();
					
					Editor->ClearSelectedRows();
					for(int Index = InsertRowIndex; Index < InsertRowIndex + NewResults.Num(); Index++)
					{
						Editor->SelectRow(Index, false);
					}
				}
			}
		}
				
		return FReply::Handled();		
	}

}

#undef LOCTEXT_NAMESPACE
