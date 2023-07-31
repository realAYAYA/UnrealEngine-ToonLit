// Copyright Epic Games, Inc. All Rights Reserved.


#include "SVariableMappingWidget.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "SVariableMappingWidget"

static const FName ColumnId_VarLabel("Variable");
static const FName ColumnID_MappingLabel("Mapping");

void SVariableMappingTreeRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	OnVariableMappingChanged = InArgs._OnVariableMappingChanged;
	OnGetVariableMapping = InArgs._OnGetVariableMapping;
	OnGetAvailableMapping = InArgs._OnGetAvailableMapping;
	OnGetFilteredText = InArgs._OnGetFilteredText;
	OnVariableOptionAvailable = InArgs._OnVariableOptionAvailable;
	OnPinCheckStateChanged = InArgs._OnPinCheckStateChanged;
	OnPinGetCheckState = InArgs._OnPinGetCheckState;
	OnPinIsEnabledCheckState = InArgs._OnPinIsEnabledCheckState;

	check(Item.IsValid());

	VariableOptionList.Reset();
	if (OnGetAvailableMapping.IsBound())
	{
		TArray<FName> ListOfMappingVars;

		OnGetAvailableMapping.Execute(Item->GetPathName(), ListOfMappingVars);
		for (auto& MappingVar : ListOfMappingVars)
		{
			VariableOptionList.Add(MakeShareable(new FString(MappingVar.ToString())));
		}
	}
	//SMultiColumnTableRow< FVariableMappingInfoPtr >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	STableRow<TSharedPtr<FVariableMappingInfoPtr>>::Construct(
		STableRow<TSharedPtr<FVariableMappingInfoPtr>>::FArguments()
		.Padding(FMargin(3.0f, 2.0f))
		.Content()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(0.0f, 4.0f)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->GetDisplayName()))
				//.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("BoldText"))
				.HighlightText(this, &SVariableMappingTreeRow::GetFilterText)
				//.IsReadOnly(true)
				//.IsSelected(this, &SMultiColumnTableRow< FVariableMappingInfoPtr >::IsSelectedExclusively)
			]
// 			+ SHorizontalBox::Slot()
// 			.Padding(2.0f, 4.0f)
// 			.HAlign(HAlign_Right)
// 			[
// 				SNew(SHorizontalBox)
// // 				+ SHorizontalBox::Slot()
// // 				.FillWidth(0.5f)
// // 				[
// // 					SNew(STextBlock)
// // 					.Text(LOCTEXT("EmptyString", ""))
// // 				]
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 4.0f)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ExposeAsPinPropertyValue", "Use Pin "))
				]
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 4.0f)
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(this, &SVariableMappingTreeRow::IsPinChecked)
					.OnCheckStateChanged(this, &SVariableMappingTreeRow::OnPinCheckStatusChanged)
					.IsEnabled(this, &SVariableMappingTreeRow::IsPinEnabled)
				]
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 4.0f)
				.HAlign(HAlign_Right)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Optional", " OR Use Curve "))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.5f)
				[
					SAssignNew(VarOptionComboBox, SSearchableComboBox)
					.OptionsSource(&VariableOptionList)
					.OnGenerateWidget(this, &SVariableMappingTreeRow::MakeVarOptionComboWidget)
					.OnSelectionChanged(this, &SVariableMappingTreeRow::OnVarOptionSourceChanged)
					.OnComboBoxOpening(this, &SVariableMappingTreeRow::OnVarOptionComboOpening)
					.IsEnabled(this, &SVariableMappingTreeRow::IsVarOptionEnabled)
					.ContentPadding(2)
					.Content()
					[
						SNew(STextBlock)
						.Text(this, &SVariableMappingTreeRow::GetVarOptionComboBoxContent)
						//.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
						.ToolTipText(this, &SVariableMappingTreeRow::GetVarOptionComboBoxToolTip)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.OnClicked(FOnClicked::CreateSP(this, &SVariableMappingTreeRow::OnClearButtonClicked))
					.Text(FText::FromString(TEXT("x")))
				]

			//]
		],
		InOwnerTableView);
}

bool SVariableMappingTreeRow::IsVarOptionEnabled() const
{
	// if pin is not checked
	if (IsPinChecked()==ECheckBoxState::Unchecked && 
		OnVariableOptionAvailable.IsBound())
	{
		return OnVariableOptionAvailable.Execute(Item->GetPathName());
	}

	return false;
}

ECheckBoxState SVariableMappingTreeRow::IsPinChecked() const
{
	if (OnPinGetCheckState.IsBound())
	{
		return OnPinGetCheckState.Execute(Item->GetPathName());
	}

	return ECheckBoxState::Unchecked;
}

void SVariableMappingTreeRow::OnPinCheckStatusChanged(ECheckBoxState NewState)
{
	if (OnPinCheckStateChanged.IsBound())
	{
		OnPinCheckStateChanged.Execute(NewState, Item->GetPathName());
	}
}

bool SVariableMappingTreeRow::IsPinEnabled() const
{
	if (OnPinIsEnabledCheckState.IsBound())
	{
		return OnPinIsEnabledCheckState.Execute(Item->GetPathName());
	}

	return false;
}


FReply SVariableMappingTreeRow::OnClearButtonClicked()
{
	if (OnVariableMappingChanged.IsBound())
	{
		OnVariableMappingChanged.Execute(Item->GetPathName(), NAME_None);
	}

	return FReply::Handled();
}

FText SVariableMappingTreeRow::GetFilterText() const
{
	if (OnGetFilteredText.IsBound())
	{
		return OnGetFilteredText.Execute();
	}

	return FText::GetEmpty();
}

void SVariableMappingTreeRow::OnVarOptionComboOpening()
{
	VariableOptionList.Reset();
	if (OnGetAvailableMapping.IsBound())
	{
		TArray<FName> ListOfMappingVars;

		OnGetAvailableMapping.Execute(Item->GetPathName(), ListOfMappingVars);
		for (auto& MappingVar : ListOfMappingVars)
		{
			VariableOptionList.Add(MakeShareable(new FString(MappingVar.ToString())));
		}
	}

	if (OnGetVariableMapping.IsBound())
	{
		FName MappedVar = OnGetVariableMapping.Execute(Item->GetPathName());
		if (MappedVar!= NAME_None)
		{
			TSharedPtr<FString> ComboStringPtr = GetVarOptionString(MappedVar);
			if (ComboStringPtr.IsValid())
			{
				VarOptionComboBox->SetSelectedItem(ComboStringPtr);
			}
		}
		else
		{
			VarOptionComboBox->ClearSelection();
		}
	}
}

void SVariableMappingTreeRow::OnVarOptionSourceChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	// if it's set from code, we did that on purpose
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		FName NewName = FName(*NewValue);

		OnVariableMappingChanged.ExecuteIfBound(Item->GetPathName(), NewName);
	}
}

FText SVariableMappingTreeRow::GetVarOptionComboBoxContent() const
{
	if (OnGetVariableMapping.IsBound())
	{
		return FText::FromName(OnGetVariableMapping.Execute(Item->GetPathName()));
	}

	return FText::FromString(TEXT("Invalid"));
}

FText SVariableMappingTreeRow::GetVarOptionComboBoxToolTip() const
{
	return LOCTEXT("VarOptionComboToolTip", "Map input/output variable to available options.");
}

TSharedPtr<FString> SVariableMappingTreeRow::GetVarOptionString(FName VarOptionName) const
{
	return TSharedPtr<FString>();
	FString VarOptionString = VarOptionName.ToString();

	// go through profile and see if it has mine
	for (int32 Index = 1; Index < VariableOptionList.Num(); ++Index)
	{
		if (VarOptionString == *VariableOptionList[Index])
		{
			return VariableOptionList[Index];
		}
	}

	return TSharedPtr<FString>();
}

TSharedRef<SWidget> SVariableMappingTreeRow::MakeVarOptionComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem));// .Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
}
//////////////////////////////////////////////////////////////////////////
// SVariableMappingWidget

void SVariableMappingWidget::Construct(const FArguments& InArgs/*, FSimpleMulticastDelegate& InOnPostUndo*/)
{
	OnGetAvailableMappingDelegate = InArgs._OnGetAvailableMapping;
	OnGetVariableMappingDelegate = InArgs._OnGetVariableMapping;
	OnVariableMappingChangedDelegate = InArgs._OnVariableMappingChanged;
	OnCreateVariableMappingDelegate = InArgs._OnCreateVariableMapping;
	OnVariableOptionAvailableDelegate = InArgs._OnVariableOptionAvailable;
	OnPinCheckStateChangedDelegate = InArgs._OnPinCheckStateChanged;
	OnPinGetCheckStateDelegate = InArgs._OnPinGetCheckState;
	OnPinIsEnabledCheckStateDelegate = InArgs._OnPinIsEnabledCheckState;

	//InOnPostUndo.Add(FSimpleDelegate::CreateSP(this, &SVariableMappingWidget::PostUndo));

	ChildSlot
		[
			SNew(SVerticalBox)

			// now show bone mapping
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				// Filter entry
				+ SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SAssignNew(NameFilterBox, SSearchBox)
					.SelectAllTextWhenFocused(true)
					.OnTextChanged(this, &SVariableMappingWidget::OnFilterTextChanged)
					.OnTextCommitted(this, &SVariableMappingWidget::OnFilterTextCommitted)
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)		// This is required to make the scrollbar work, as content overflows Slate containers by default
			[
				SAssignNew(VariableMappingTreeView, SVariableMappingTreeView)
				.TreeItemsSource(&VariableMappingList)
				.OnGenerateRow(this, &SVariableMappingWidget::MakeTreeRowWidget)
				.OnGetChildren(this, &SVariableMappingWidget::GetChildrenForInfo)
				.ItemHeight(22.0f)
			]
		];

	RefreshVariableMappingList();
}

TSharedRef<ITableRow> SVariableMappingWidget::MakeTreeRowWidget(TSharedPtr<FVariableMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SVariableMappingTreeRow, OwnerTable)
		.Item(InInfo)
		.OnVariableMappingChanged(OnVariableMappingChangedDelegate)
		.OnGetVariableMapping(OnGetVariableMappingDelegate)
		.OnGetAvailableMapping(OnGetAvailableMappingDelegate)
		.OnGetFilteredText(this, &SVariableMappingWidget::GetFilterText)
		.OnVariableOptionAvailable(OnVariableOptionAvailableDelegate)
		.OnPinCheckStateChanged(OnPinCheckStateChangedDelegate)
		.OnPinGetCheckState(OnPinGetCheckStateDelegate)
		.OnPinIsEnabledCheckState(OnPinIsEnabledCheckStateDelegate);
}

void SVariableMappingWidget::GetChildrenForInfo(TSharedPtr<FVariableMappingInfo> InInfo, TArray< TSharedPtr<FVariableMappingInfo> >& OutChildren)
{
	OutChildren = InInfo->Children;
}

void SVariableMappingWidget::OnFilterTextChanged(const FText& SearchText)
{
	// need to make sure not to have the same text go
	// otherwise, the widget gets recreated multiple times causing 
	// other issue
	if (FilterText.CompareToCaseIgnored(SearchText) != 0)
	{
		FilterText = SearchText;
		RefreshVariableMappingList();
	}
}

void SVariableMappingWidget::OnFilterTextCommitted(const FText& SearchText, ETextCommit::Type CommitInfo)
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged(SearchText);
}

TSharedRef<ITableRow> SVariableMappingWidget::GenerateVariableMappingRow(TSharedPtr<FVariableMappingInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(InInfo.IsValid());

	return
		SNew(SVariableMappingTreeRow, OwnerTable)
		.Item(InInfo)
		.OnVariableMappingChanged(OnVariableMappingChangedDelegate)
		.OnGetVariableMapping(OnGetVariableMappingDelegate)
		.OnGetAvailableMapping(OnGetAvailableMappingDelegate)
		.OnGetFilteredText(this, &SVariableMappingWidget::GetFilterText);
}

void SVariableMappingWidget::RefreshVariableMappingList()
{
	OnCreateVariableMappingDelegate.ExecuteIfBound(FilterText.ToString(), VariableMappingList);

	VariableMappingTreeView->RequestListRefresh();
}

// void SVariableMappingWidget::PostUndo()
// {
// 	RefreshVariableMappingList();
// }

#undef LOCTEXT_NAMESPACE

