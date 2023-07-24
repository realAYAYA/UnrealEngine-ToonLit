// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraStackTableRow.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "Styling/AppStyle.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraEditorWidgetsUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraStackCommandContext.h"
#include "Stack/SNiagaraStackIndent.h"
#include "Stack/SNiagaraStackIssueIcon.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "Styling/StyleColors.h"
#include "ScopedTransaction.h"
#include "NiagaraEmitterEditorData.h"

#define LOCTEXT_NAMESPACE "NiagaraStackTableRow"

void SNiagaraStackTableRow::Construct(const FArguments& InArgs, UNiagaraStackViewModel* InStackViewModel, UNiagaraStackEntry* InStackEntry, TSharedRef<FNiagaraStackCommandContext> InStackCommandContext, const TSharedRef<STreeView<UNiagaraStackEntry*>>& InOwnerTree)
{
	ContentPadding = InArgs._ContentPadding;
	bIsCategoryIconHighlighted = InArgs._IsCategoryIconHighlighted;
	bShowExecutionCategoryIcon = InArgs._ShowExecutionCategoryIcon;
	NameColumnWidth = InArgs._NameColumnWidth;
	ValueColumnWidth = InArgs._ValueColumnWidth;
	NameColumnWidthChanged = InArgs._OnNameColumnWidthChanged;
	ValueColumnWidthChanged = InArgs._OnValueColumnWidthChanged;
	IssueIconVisibility = InArgs._IssueIconVisibility;
	StackViewModel = InStackViewModel;
	StackEntry = InStackEntry;
	StackCommandContext = InStackCommandContext;
	OwnerTree = InOwnerTree;

	NameVerticalAlignment = VAlign_Center;
	NameHorizontalAlignment = HAlign_Fill;

	ExpandedImage = FCoreStyle::Get().GetBrush("TreeArrow_Expanded");
	CollapsedImage = FCoreStyle::Get().GetBrush("TreeArrow_Collapsed");

	IndicatorColor = InArgs._IndicatorColor;

	ExecutionCategoryToolTipText = (InStackEntry->GetExecutionSubcategoryName() != NAME_None)
		? FText::Format(LOCTEXT("ExecutionCategoryToolTipFormat", "{0} - {1}"), FText::FromName(InStackEntry->GetExecutionCategoryName()), FText::FromName(InStackEntry->GetExecutionSubcategoryName()))
		: FText::FromName(InStackEntry->GetExecutionCategoryName());

	ConstructInternal(
		STableRow<UNiagaraStackEntry*>::FArguments()
			.Style(InArgs._Style)
			.OnDragDetected(InArgs._OnDragDetected)
			.OnDragLeave(InArgs._OnDragLeave)
			.OnCanAcceptDrop(InArgs._OnCanAcceptDrop)
			.OnAcceptDrop(InArgs._OnAcceptDrop)
		, OwnerTree.ToSharedRef());
}

void SNiagaraStackTableRow::SetOverrideNameWidth(TOptional<float> InMinWidth, TOptional<float> InMaxWidth)
{
	NameMinWidth = InMinWidth;
	NameMaxWidth = InMaxWidth;
}

void SNiagaraStackTableRow::SetOverrideNameAlignment(EHorizontalAlignment InHAlign, EVerticalAlignment InVAlign)
{
	NameHorizontalAlignment = InHAlign;
	NameVerticalAlignment = InVAlign;
}

void SNiagaraStackTableRow::SetOverrideValueWidth(TOptional<float> InMinWidth, TOptional<float> InMaxWidth)
{
	ValueMinWidth = InMinWidth;
	ValueMaxWidth = InMaxWidth;
}

void SNiagaraStackTableRow::SetOverrideValueAlignment(EHorizontalAlignment InHAlign, EVerticalAlignment InVAlign)
{
	ValueHorizontalAlignment = InHAlign;
	ValueVerticalAlignment = InVAlign;
}

FMargin SNiagaraStackTableRow::GetContentPadding() const
{
	return ContentPadding;
}

void SNiagaraStackTableRow::SetContentPadding(FMargin InContentPadding)
{
	ContentPadding = InContentPadding;
}

void SNiagaraStackTableRow::SetNameAndValueContent(TSharedRef<SWidget> InNameWidget, TSharedPtr<SWidget> InValueWidget)
{
	FSlateColor IconColor = FNiagaraEditorWidgetsStyle::Get().GetColor(FNiagaraStackEditorWidgetsUtilities::GetColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName()));
	if (bIsCategoryIconHighlighted)
	{
		IconColor = FNiagaraEditorWidgetsStyle::Get().GetColor(FNiagaraStackEditorWidgetsUtilities::GetIconColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName()));
	}

	FName IconName = FNiagaraStackEditorWidgetsUtilities::GetIconNameForExecutionSubcategory(StackEntry->GetExecutionSubcategoryName(), bIsCategoryIconHighlighted);
	const FSlateBrush* IconBrush = nullptr;

	if(IconName != NAME_None)
	{
		IconBrush = FNiagaraEditorWidgetsStyle::Get().GetBrush(IconName);
	}
	
	TSharedRef<SHorizontalBox> NameContent = SNew(SHorizontalBox)
	.Clipping(EWidgetClipping::OnDemand)
	// Indent
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(0)
	[
		SNew(SNiagaraStackIndent, StackEntry, ENiagaraStackIndentMode::Name)
	]
	// Expand button
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(0)
	[
		SNew(SButton)
		.ButtonStyle(FNiagaraEditorWidgetsStyle::Get(), "NiagaraEditor.Stack.SimpleButton")
		.Visibility(this, &SNiagaraStackTableRow::GetExpanderVisibility)
		.OnClicked(this, &SNiagaraStackTableRow::ExpandButtonClicked)
		.ContentPadding(FMargin(1, 2, 1, 2))
		.HAlign(HAlign_Center)
		[
			SNew(SImage)
			.Image(this, &SNiagaraStackTableRow::GetExpandButtonImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
		]
	]
	// Execution sub-category icon
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(FMargin(1, 1, 2, 1))
	.VAlign(EVerticalAlignment::VAlign_Center)
	.HAlign(EHorizontalAlignment::HAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(FNiagaraEditorWidgetsStyle::Get().GetFloat("NiagaraEditor.Stack.IconHighlightedSize"))
		.HAlign(EHorizontalAlignment::HAlign_Center)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.ToolTipText(ExecutionCategoryToolTipText)
		.Visibility(this, &SNiagaraStackTableRow::GetExecutionCategoryIconVisibility)
		.IsEnabled_UObject(StackEntry, &UNiagaraStackEntry::GetIsEnabledAndOwnerIsEnabled)
		[
			SNew(SImage)
			.Visibility(this, &SNiagaraStackTableRow::GetExecutionCategoryIconVisibility)
			.Image(IconBrush ? IconBrush : FCoreStyle::Get().GetDefaultBrush())
			.ColorAndOpacity(IconColor)
		]
	]
	// Name content
	+ SHorizontalBox::Slot()
	.HAlign(NameHorizontalAlignment)
	.VAlign(NameVerticalAlignment)
	.Padding(FMargin(0, ContentPadding.Top, 0, ContentPadding.Bottom))
	[
		InNameWidget
	];

	TSharedPtr<SWidget> ChildContent;
	if (InValueWidget.IsValid())
	{
		ChildContent = SNew(SSplitter)
		.Style(FAppStyle::Get(), "DetailsView.Splitter")
		.PhysicalSplitterHandleSize(1.0f)
		.HitDetectionSplitterHandleSize(5.0f)

		+ SSplitter::Slot()
		.Value(NameColumnWidth)
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SNiagaraStackTableRow::OnNameColumnWidthChanged))
		[
			SNew(SBox)
			.Padding(FMargin(ContentPadding.Left, 0, 5, 0))
			.MinDesiredWidth(NameMinWidth.IsSet() ? NameMinWidth.GetValue() : FOptionalSize())
			.MaxDesiredWidth(NameMaxWidth.IsSet() ? NameMaxWidth.GetValue() : FOptionalSize())
			[
				NameContent
			]
		]

		// Value
		+ SSplitter::Slot()
		.Value(ValueColumnWidth)
		.OnSlotResized(SSplitter::FOnSlotResized::CreateSP(this, &SNiagaraStackTableRow::OnValueColumnWidthChanged))
		[
			SNew(SBox)
			.Padding(FMargin(4, ContentPadding.Top, ContentPadding.Right, ContentPadding.Bottom))
			.HAlign(ValueHorizontalAlignment)
			.VAlign(ValueVerticalAlignment)
			.MinDesiredWidth(ValueMinWidth.IsSet() ? ValueMinWidth.GetValue() : FOptionalSize())
			.MaxDesiredWidth(ValueMaxWidth.IsSet() ? ValueMaxWidth.GetValue() : FOptionalSize())
			[
				InValueWidget.ToSharedRef()
			]
		];
	}
	else
	{
		ChildContent = SNew(SBox)
		.Padding(FMargin(ContentPadding.Left, 0, ContentPadding.Right, 0))
		.MinDesiredWidth(NameMinWidth.IsSet() ? NameMinWidth.GetValue() : FOptionalSize())
		.MaxDesiredWidth(NameMaxWidth.IsSet() ? NameMaxWidth.GetValue() : FOptionalSize())
		[
			NameContent
		];
	}

	FName AccentColorName = FNiagaraStackEditorWidgetsUtilities::GetColorNameForExecutionCategory(StackEntry->GetExecutionCategoryName());
	TOptional<FSlateColor> AccentColor = (AccentColorName != NAME_None) ? FNiagaraEditorWidgetsStyle::Get().GetColor(AccentColorName) : FStyleColors::Transparent;

	TSharedRef<SOverlay> RowOverlay = SNew(SOverlay)
		.Visibility(this, &SNiagaraStackTableRow::GetRowVisibility)
		+ SOverlay::Slot()
		[
			// Row content
			SNew(SBorder)
			.BorderImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("Niagara.TableViewRowBorder"))
			.ToolTipText_UObject(StackEntry, &UNiagaraStackEntry::GetTooltipText)
			.Padding(FMargin(5, 0, 0, 1))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.0f)
				[
					ChildContent.ToSharedRef()
				]
				// Issue Icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(3, 0)
				[
					SNew(SNiagaraStackIssueIcon, StackViewModel, StackEntry)
					.Visibility(IssueIconVisibility)
				]
			]
		];

	if (AccentColor.IsSet())
	{
		RowOverlay->AddSlot()
		.HAlign(HAlign_Left)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(AccentColor.GetValue())
			.Padding(0)
			[
				SNew(SBox)
				.WidthOverride(4)
			]
		];
	}

	if (IndicatorColor.IsSet())
	{
		RowOverlay->AddSlot()
		.HAlign(HAlign_Right)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(IndicatorColor.GetValue())
			.Padding(0)
			[
				SNew(SBox)
				.WidthOverride(2)
			]
		];
	}

	// Search result indicator.
	RowOverlay->AddSlot()
	[
		SNew(SBorder)
		.BorderImage(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.Stack.SearchResult"))
		.Visibility(this, &SNiagaraStackTableRow::GetSearchResultBorderVisibility)
	];

	ChildSlot
	[
		RowOverlay
	];
}

void SNiagaraStackTableRow::AddFillRowContextMenuHandler(FOnFillRowContextMenu FillRowContextMenuHandler)
{
	OnFillRowContextMenuHanders.Add(FillRowContextMenuHandler);
}

FReply SNiagaraStackTableRow::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Unhandled();
}

FReply SNiagaraStackTableRow::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		TSharedPtr<ITypedTableView<UNiagaraStackEntry*>> OwnerTable = OwnerTablePtr.Pin();
		if (OwnerTable.IsValid())
		{
			if (OwnerTable->GetSelectedItems().Contains(StackEntry) == false)
			{
				OwnerTable->Private_ClearSelection();
				OwnerTable->Private_SetItemSelection(StackEntry, true, true);
				OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
			}
		}

		FMenuBuilder MenuBuilder(true, StackCommandContext->GetCommands());
		for (FOnFillRowContextMenu& OnFillRowContextMenuHandler : OnFillRowContextMenuHanders)
		{
			OnFillRowContextMenuHandler.ExecuteIfBound(MenuBuilder);
		}

		FNiagaraStackEditorWidgetsUtilities::AddStackEntryAssetContextMenuActions(MenuBuilder, *StackEntry);
		StackCommandContext->AddEditMenuItems(MenuBuilder);

		TArray<UNiagaraStackEntry*> EntriesToProcess;
		TArray<UNiagaraStackEntry*> NavigationEntries;
		StackViewModel->GetPathForEntry(StackEntry, EntriesToProcess);
		for (UNiagaraStackEntry* Parent : EntriesToProcess)
		{
			UNiagaraStackItemGroup* GroupParent = Cast<UNiagaraStackItemGroup>(Parent);
			UNiagaraStackItem* ItemParent = Cast<UNiagaraStackItem>(Parent);
			if (GroupParent != nullptr || ItemParent != nullptr)
			{
				MenuBuilder.BeginSection("StackRowNavigation", LOCTEXT("NavigationMenuSection", "Navigation"));
				{
					if (GroupParent != nullptr)
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("TopOfSection", "Top of Section"),
							FText::Format(LOCTEXT("NavigateToFormatted", "Navigate to {0}"), Parent->GetDisplayName()),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::NavigateTo, Parent)));
					}
					if (ItemParent != nullptr)
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("TopOfModule", "Top of Module"),
							FText::Format(LOCTEXT("NavigateToFormatted", "Navigate to {0}"), Parent->GetDisplayName()),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::NavigateTo, Parent)));
					}
				}
				MenuBuilder.EndSection();
			}
		}

		MenuBuilder.BeginSection("StackActions", LOCTEXT("StackActions", "Stack Actions"));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ExpandAllItems", "Expand All"),
			LOCTEXT("ExpandAllItemsToolTip", "Expand all items under this header."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::ExpandChildren)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CollapseAllItems", "Collapse All"),
			LOCTEXT("CollapseAllItemsToolTip", "Collapse all items under this header."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::CollapseChildren)));
		MenuBuilder.EndSection();


		if (IsValidForSummaryView())
		{
			FUIAction ShowHideSummaryViewAction(
			FExecuteAction::CreateSP(this, &SNiagaraStackTableRow::ToggleShowInSummaryView),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SNiagaraStackTableRow::ShouldShowInSummaryView));
		
			MenuBuilder.BeginSection("SummaryViewActions", LOCTEXT("SummaryViewActions", "Emitter Summary"));
			MenuBuilder.AddMenuEntry(
				LOCTEXT("SummaryViewShow", "Show in Emitter Summary"),
				LOCTEXT("SummaryViewShowTooltip", "Should this parameter be visible in the emitter summary?"),
				FSlateIcon(),
				ShowHideSummaryViewAction,
				NAME_None, EUserInterfaceActionType::ToggleButton);
			MenuBuilder.EndSection();
		}

		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return FReply::Handled();
	}
	return STableRow<UNiagaraStackEntry*>::OnMouseButtonUp(MyGeometry, MouseEvent);
}

void SetExpansionStateRecursive(UNiagaraStackEntry* StackEntry, bool bIsExpanded)
{
	if (StackEntry->GetCanExpand())
	{
		StackEntry->SetIsExpanded(bIsExpanded);
	}

	TArray<UNiagaraStackEntry*> Children;
	StackEntry->GetUnfilteredChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		SetExpansionStateRecursive(Child, bIsExpanded);
	}
}

void SNiagaraStackTableRow::CollapseChildren()
{
	bool bIsExpanded = false;
	SetExpansionStateRecursive(StackEntry, bIsExpanded);
}

void SNiagaraStackTableRow::ExpandChildren()
{
	bool bIsExpanded = true;
	SetExpansionStateRecursive(StackEntry, bIsExpanded);
}

EVisibility SNiagaraStackTableRow::GetRowVisibility() const
{
	return StackEntry->GetShouldShowInStack()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SNiagaraStackTableRow::GetExecutionCategoryIconVisibility() const
{
	return bShowExecutionCategoryIcon && (StackEntry->GetExecutionSubcategoryName() != NAME_None)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SNiagaraStackTableRow::GetExpanderVisibility() const
{
	if (StackEntry->GetCanExpand())
	{
		TArray<UNiagaraStackEntry*> Children;
		StackEntry->GetFilteredChildren(Children);
		return Children.Num() > 0
			? EVisibility::Visible
			: EVisibility::Hidden;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SNiagaraStackTableRow::ExpandButtonClicked()
{
	const bool bWillBeExpanded = !StackEntry->GetIsExpanded();
	// Recurse the expansion if "shift" is being pressed
	const FModifierKeysState ModKeyState = FSlateApplication::Get().GetModifierKeys();
	if (ModKeyState.IsShiftDown())
	{
		StackEntry->SetIsExpanded_Recursive(bWillBeExpanded);
	}
	else
	{
		StackEntry->SetIsExpanded(bWillBeExpanded);
	}
	return FReply::Handled();
}

const FSlateBrush* SNiagaraStackTableRow::GetExpandButtonImage() const
{
	return StackEntry->GetIsExpanded() ? ExpandedImage : CollapsedImage;
}

void SNiagaraStackTableRow::OnNameColumnWidthChanged(float Width)
{
	NameColumnWidthChanged.ExecuteIfBound(Width);
}

void SNiagaraStackTableRow::OnValueColumnWidthChanged(float Width)
{
	ValueColumnWidthChanged.ExecuteIfBound(Width);
}

EVisibility SNiagaraStackTableRow::GetSearchResultBorderVisibility() const
{
	return StackViewModel->GetCurrentFocusedEntry() == StackEntry ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

void SNiagaraStackTableRow::NavigateTo(UNiagaraStackEntry* Item)
{
	OwnerTree->RequestNavigateToItem(Item, 0);
}

bool SNiagaraStackTableRow::IsValidForSummaryView() const
{
	UNiagaraStackFunctionInput* FunctionInput = Cast<UNiagaraStackFunctionInput>(StackEntry);
	const UNiagaraEmitterEditorData* EditorData = (FunctionInput && FunctionInput->GetEmitterViewModel())? &FunctionInput->GetEmitterViewModel()->GetEditorData() : nullptr;

	if (EditorData)
	{
		UNiagaraStackFunctionInput* ParentInput = FNiagaraStackEditorWidgetsUtilities::GetParentInputForSummaryView(FunctionInput);
		return FNiagaraStackEditorWidgetsUtilities::GetSummaryViewInputKeyForFunctionInput(ParentInput).IsSet();
	}
	return false;
}

void SNiagaraStackTableRow::ToggleShowInSummaryView()
{
	UNiagaraStackFunctionInput* FunctionInput = Cast<UNiagaraStackFunctionInput>(StackEntry);
	UNiagaraEmitterEditorData* EditorData = (FunctionInput && FunctionInput->GetEmitterViewModel())? &FunctionInput->GetEmitterViewModel()->GetOrCreateEditorData() : nullptr;
	
	if (EditorData)
	{
		UNiagaraStackFunctionInput* ParentInput = FNiagaraStackEditorWidgetsUtilities::GetParentInputForSummaryView(FunctionInput);
		TOptional<FFunctionInputSummaryViewKey> Key = FNiagaraStackEditorWidgetsUtilities::GetSummaryViewInputKeyForFunctionInput(ParentInput);
	
		if (Key.IsSet())
		{
			// TODO: Move this parent handling to the UNiagaraStackFunctionInput and merge manager.
			bool bHasParentSummaryData = false;;
			FVersionedNiagaraEmitter ParentEmitter = FunctionInput->GetEmitterViewModel()->GetEmitter().GetEmitterData()->GetParent();
			if (ParentEmitter.GetEmitterData())
			{
				if (UNiagaraEmitterEditorData* ParentEmitterEditorData = Cast<UNiagaraEmitterEditorData>(ParentEmitter.GetEmitterData()->GetEditorData()))
				{
					bHasParentSummaryData = ParentEmitterEditorData->GetSummaryViewMetaData(Key.GetValue()).IsSet();
				}
			}

			TOptional<FFunctionInputSummaryViewMetadata> SummaryViewMetaData = EditorData->GetSummaryViewMetaData(Key.GetValue());
			if (SummaryViewMetaData.IsSet() == false)
			{
				SummaryViewMetaData = FFunctionInputSummaryViewMetadata();
			}

			FScopedTransaction ScopedTransaction(FText::Format(LOCTEXT("SummaryViewChangedInputVisibility", "Changed summary view visibility for {0}"), FunctionInput->GetDisplayName()));
			EditorData->Modify();
			
			SummaryViewMetaData->bVisible = !SummaryViewMetaData->bVisible;
			if (bHasParentSummaryData == false && SummaryViewMetaData->bVisible == false)
			{
				// If there is no parent summary data, and the input is no longer visible, reset the optional value
				// to remove the input from the summary.
				SummaryViewMetaData.Reset();
			}
			EditorData->SetSummaryViewMetaData(Key.GetValue(), SummaryViewMetaData);
		}
	}	
}

bool SNiagaraStackTableRow::ShouldShowInSummaryView() const
{
	UNiagaraStackFunctionInput* FunctionInput = Cast<UNiagaraStackFunctionInput>(StackEntry);
	const UNiagaraEmitterEditorData* EditorData = (FunctionInput && FunctionInput->GetEmitterViewModel())? &FunctionInput->GetEmitterViewModel()->GetEditorData() : nullptr;
	
	if (EditorData)
	{
		UNiagaraStackFunctionInput* ParentInput = FNiagaraStackEditorWidgetsUtilities::GetParentInputForSummaryView(FunctionInput);
		TOptional<FFunctionInputSummaryViewKey> Key = FNiagaraStackEditorWidgetsUtilities::GetSummaryViewInputKeyForFunctionInput(ParentInput);
	
		if (Key.IsSet())
		{
			TOptional<FFunctionInputSummaryViewMetadata> Metadata = EditorData->GetSummaryViewMetaData(Key.GetValue());
			return Metadata.IsSet() && Metadata->bVisible;
		}			
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
