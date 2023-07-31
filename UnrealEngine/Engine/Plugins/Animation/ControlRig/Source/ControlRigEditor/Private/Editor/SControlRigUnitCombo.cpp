// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SControlRigUnitCombo.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "SListViewSelectorDropdownMenu.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "ControlRigEditorStyle.h"
#include "Units/RigUnit.h"
#include "UObject/UObjectIterator.h"
#include "Styling/AppStyle.h"
#include "ControlRigBlueprintUtils.h"
#include "SGraphEditorActionMenu.h"
#include "Graph/ControlRigGraph.h"

#define LOCTEXT_NAMESPACE "SControlRigUnitCombo"

void SControlRigUnitCombo::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;

	OnRigUnitSelected = InArgs._OnRigUnitSelected;

	BuildUnitTypesList();

	FilterBox = SNew(SSearchBox);

	ListView = SNew(SListView<TSharedPtr<FRigUnitTypeItem>>)
		.ListItemsSource(&UnitTypeList)
		.OnGenerateRow(this, &SControlRigUnitCombo::GenerateListRow)
		.OnSelectionChanged(this, &SControlRigUnitCombo::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Single);

	SComboButton::FArguments Args;
	Args.ButtonContent()
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(1.0f, 1.0f)
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
			.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
			.Text(FText::FromString(FString(TEXT("\xf067"))) /*fa-plus*/)
		]
		+SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.Padding(1.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AddRigUnitButtonLabel", "Add Rig Unit"))
			.TextStyle(FAppStyle::Get(), "ContentBrowser.TopBar.Font")
		]
	]
	.IsFocusable(true)
	.ContentPadding(FMargin(5.0f, 0.0f))
	.ComboButtonStyle(FAppStyle::Get(), "ToolbarComboButton")
	.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
	.ForegroundColor(FLinearColor::White)
	.OnGetMenuContent(this, &SControlRigUnitCombo::HandleGetMenuContent);

	SComboButton::Construct(Args);
}

TSharedRef<ITableRow> SControlRigUnitCombo::GenerateListRow(TSharedPtr<FRigUnitTypeItem> InItem, const TSharedRef<STableViewBase>& InOwningTable)
{
	return SNew(SComboRow<TSharedPtr<FRigUnitTypeItem>>, InOwningTable)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(8.0f, 1.0f))
			]
			+SHorizontalBox::Slot()
			.Padding(1.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.Image(FControlRigEditorStyle::Get().GetBrush("ControlRig.RigUnit"))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SSpacer)
				.Size(FVector2D(3.0f, 1.0f))
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.HighlightText(this, &SControlRigUnitCombo::GetCurrentSearchString)
				.Text(InItem->DisplayText)
			]
		];	
}

FText SControlRigUnitCombo::GetCurrentSearchString() const
{
	return FilterBox->GetText();
}

void SControlRigUnitCombo::OnSelectionChanged(TSharedPtr<FRigUnitTypeItem> InItem, ESelectInfo::Type InSelectInfo)
{
	if(InSelectInfo != ESelectInfo::Direct)
	{
		SetIsOpen(false);

		if(InItem.IsValid())
		{
			OnRigUnitSelected.ExecuteIfBound(InItem->Struct);
		}
	}
}

void SControlRigUnitCombo::BuildUnitTypesList()
{
	FControlRigBlueprintUtils::ForAllRigUnits([this](UStruct* InStruct)
	{
		UnitTypeList.Add(MakeShared<FRigUnitTypeItem>(InStruct));
	});
}

TSharedRef<SWidget> SControlRigUnitCombo::HandleGetMenuContent()
{
	UControlRigGraph* TargetGraph = Cast<UControlRigGraph>(ControlRigEditor.Pin()->GetFocusedGraph());
	if(TargetGraph == nullptr)
	{
		UBlueprint* Blueprint = ControlRigEditor.Pin()->GetBlueprintObj();
		for(UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if(Graph->IsA<UControlRigGraph>())
			{
				TargetGraph = Cast<UControlRigGraph>(Graph);
				break;
			}
		}
	}

	// We must have at least one control rig graph!
	check(TargetGraph);

	return SNew(SGraphEditorActionMenu)
		.GraphObj(TargetGraph)
		.AutoExpandActionMenu(true);
}

#undef LOCTEXT_NAMESPACE
