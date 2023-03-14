// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SControlRigFunctionLocalizationWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "DetailLayoutBuilder.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "HAL/ConsoleManager.h"
#include "RigVMModel/Nodes/RigVMFunctionReferenceNode.h"

#define LOCTEXT_NAMESPACE "SControlRigFunctionLocalizationWidget"

//////////////////////////////////////////////////////////////
/// SControlRigFunctionLocalizationItem
///////////////////////////////////////////////////////////

SControlRigFunctionLocalizationItem::SControlRigFunctionLocalizationItem(URigVMLibraryNode* InFunction)
    : Function(InFunction)
{
	FString OuterName;
	bool bIsPublic = false;

	if(URigVMFunctionLibrary* Library = Cast<URigVMFunctionLibrary>(InFunction->GetOuter()))
	{
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(Library->GetOuter()))
		{
			OuterName = Blueprint->GetName();
			bIsPublic = Blueprint->IsFunctionPublic(InFunction->GetFName());
		}
	}
	DisplayText = FText::FromString(FString::Printf(TEXT("%s :: %s"), *OuterName, *InFunction->GetName()));
	ToolTipText = bIsPublic ?
		FText::FromString(FString::Printf(TEXT("%s :: %s is public. Check this to create a local copy."), *OuterName, *InFunction->GetName())) :
		FText::FromString(FString::Printf(TEXT("%s :: %s is private. A local copy has to be created. To avoid this you can turn it public in the source Control Rig."), *OuterName, *InFunction->GetName()));
}

//////////////////////////////////////////////////////////////
/// SControlRigFunctionLocalizationTableRow
///////////////////////////////////////////////////////////

void SControlRigFunctionLocalizationTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, SControlRigFunctionLocalizationWidget* InLocalizationWidget, TSharedRef<SControlRigFunctionLocalizationItem> InFunctionItem)
{
	STableRow<TSharedPtr<SControlRigFunctionLocalizationItem>>::Construct(
		STableRow<TSharedPtr<SControlRigFunctionLocalizationItem>>::FArguments()
		.Content()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(4.0f, 4.0f, 0.f, 0.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 12.f, 0.f)
				[
					SNew(STextBlock)
					.Text(InFunctionItem->DisplayText)
					.ToolTipText(InFunctionItem->ToolTipText)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(InLocalizationWidget, &SControlRigFunctionLocalizationWidget::IsFunctionEnabled, InFunctionItem->Function)
					.OnCheckStateChanged(InLocalizationWidget, &SControlRigFunctionLocalizationWidget::SetFunctionEnabled, InFunctionItem->Function)
					.IsEnabled(InLocalizationWidget, &SControlRigFunctionLocalizationWidget::IsFunctionPublic, InFunctionItem->Function)
					.ToolTipText(InFunctionItem->ToolTipText)
				]
			]
		]
		, OwnerTable
	);
}

//////////////////////////////////////////////////////////////
/// SControlRigFunctionLocalizationWidget
///////////////////////////////////////////////////////////

void SControlRigFunctionLocalizationWidget::Construct(const FArguments& InArgs, URigVMLibraryNode* InFunctionToLocalize, UControlRigBlueprint* InTargetBlueprint)
{
	FunctionsToLocalize.Add(InFunctionToLocalize);
	FunctionItems.Reset();

	TArray<URigVMLibraryNode*> NodesToVisit;
	TArray<URigVMLibraryNode*> FunctionsForTable;

	FunctionsForTable.Add(InFunctionToLocalize);
	NodesToVisit.Add(InFunctionToLocalize);

	for(int32 NodeToVisitIndex=0; NodeToVisitIndex<NodesToVisit.Num(); NodeToVisitIndex++)
	{
		URigVMLibraryNode* NodeToVisit = NodesToVisit[NodeToVisitIndex];
		const TArray<URigVMNode*>& ContainedNodes = NodeToVisit->GetContainedNodes();
		for(URigVMNode* ContainedNode : ContainedNodes)
		{
			if(URigVMLibraryNode* ContainedLibraryNode = Cast<URigVMLibraryNode>(ContainedNode))
			{
				NodesToVisit.AddUnique(ContainedLibraryNode);
			}
		}

		if(URigVMFunctionReferenceNode* FunctionReferenceNode = Cast<URigVMFunctionReferenceNode>(NodeToVisit))
		{
			if(URigVMLibraryNode* ReferencedNode = FunctionReferenceNode->GetReferencedNode())
			{
				if(URigVMFunctionLibrary* ReferencedLibrary = ReferencedNode->GetLibrary())
				{
					if(UControlRigBlueprint* ReferencedBlueprint = Cast<UControlRigBlueprint>(ReferencedLibrary->GetOuter()))
					{
						if(ReferencedBlueprint != InTargetBlueprint)
						{
							FunctionsForTable.AddUnique(ReferencedNode);
							if(!ReferencedBlueprint->IsFunctionPublic(ReferencedNode->GetFName()))
							{
								FunctionsToLocalize.AddUnique(ReferencedNode);
							}
						}
					}
				}

				NodesToVisit.AddUnique(ReferencedNode);
			}
		}
	}

	// sort the functions to localize based on their nesting
	Algo::Sort(FunctionsForTable, [](URigVMLibraryNode* A, URigVMLibraryNode* B) -> bool
    {
        check(A);
        check(B);
        return A->Contains(B);
    });

	for (URigVMLibraryNode* FunctionForTable : FunctionsForTable)
	{
		FunctionItems.Add(MakeShared<SControlRigFunctionLocalizationItem>(FunctionForTable));
	}

	ChildSlot
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(4.0f, 4.0f, 0.f, 0.f)
			.AutoHeight()
			[
				SNew(SBorder)
				.Visibility(EVisibility::Visible)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				[
					SNew(SListView<TSharedPtr<SControlRigFunctionLocalizationItem>>)
					.ListItemsSource(&FunctionItems)
					.OnGenerateRow(this, &SControlRigFunctionLocalizationWidget::GenerateFunctionListRow)
					.SelectionMode(ESelectionMode::None)
				]
			]
		];
}

TSharedRef<ITableRow> SControlRigFunctionLocalizationWidget::GenerateFunctionListRow(TSharedPtr<SControlRigFunctionLocalizationItem> InItem, const TSharedRef<STableViewBase>& InOwningTable)
{
	TSharedRef<SControlRigFunctionLocalizationTableRow> TableRow = SNew(SControlRigFunctionLocalizationTableRow, InOwningTable, this, InItem.ToSharedRef());
	TableRows.Add(InItem->Function, TableRow);
	return TableRow;
}

ECheckBoxState SControlRigFunctionLocalizationWidget::IsFunctionEnabled(URigVMLibraryNode* InFunction) const
{
	return FunctionsToLocalize.Contains(InFunction) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SControlRigFunctionLocalizationWidget::SetFunctionEnabled(ECheckBoxState NewState, URigVMLibraryNode* InFunction)
{
	if(NewState == ECheckBoxState::Checked)
	{
		FunctionsToLocalize.AddUnique(InFunction);
	}
	else
	{
		FunctionsToLocalize.Remove(InFunction);
	}
}

bool SControlRigFunctionLocalizationWidget::IsFunctionPublic(URigVMLibraryNode* InFunction) const
{
	if(URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(InFunction->GetOuter()))
	{
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(FunctionLibrary->GetOuter()))
		{
			return Blueprint->IsFunctionPublic(InFunction->GetFName());
		}
	}
	return true;	
}

void SControlRigFunctionLocalizationDialog::Construct(const FArguments& InArgs)
{
	UserResponse = EAppReturnType::Cancel;

	static const FString DialogText = TEXT(
        "This action requires a function to be localized.\n\n" 
        "Check the functions you want to create copies of below.\n\n"
        "Note that private functions have to be copied,\n"
        "while public functions can be referenced.\n\n"
        "Checking all functions removes the dependencies completely."
    );
	
	SWindow::Construct(SWindow::FArguments()
        .Title(LOCTEXT("SControlRigFunctionLocalizationDialog_Title", "Select function(s) to localize"))
        .SupportsMinimize(false)
        .SupportsMaximize(false)
        .SizingRule( ESizingRule::Autosized )
        .ClientSize(FVector2D(450, 450))
        [
	        SNew(SHorizontalBox)

	        +SHorizontalBox::Slot()
	        .AutoWidth()
	        .HAlign(HAlign_Left)
	        .VAlign(VAlign_Top)
	        .Padding(2)
	        [
				SNew(SVerticalBox)

                +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(8)
                [

                    SNew(STextBlock)
                    .Text(FText::FromString(DialogText))
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
				
                +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Left)
                .Padding(2)
                [
                    SAssignNew(FunctionsWidget, SControlRigFunctionLocalizationWidget, InArgs._Function, InArgs._TargetBlueprint)
                ]

                +SVerticalBox::Slot()
                .AutoHeight()
                .HAlign(HAlign_Right)
                .Padding(5)
                [
                    SNew(SUniformGridPanel)
                    .SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
                    .MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
                    .MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
                    +SUniformGridPanel::Slot(0, 0)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
                        .Text(LOCTEXT("OK", "OK"))
                        .OnClicked(this, &SControlRigFunctionLocalizationDialog::OnButtonClick, EAppReturnType::Ok)
                        .IsEnabled(this, &SControlRigFunctionLocalizationDialog::IsOkButtonEnabled)
                    ]
                    +SUniformGridPanel::Slot(1, 0)
                    [
                        SNew(SButton)
                        .HAlign(HAlign_Center)
                        .ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
                        .Text(LOCTEXT("Cancel", "Cancel"))
                        .OnClicked(this, &SControlRigFunctionLocalizationDialog::OnButtonClick, EAppReturnType::Cancel)
                    ]
                ]
			]
		]);
}

FReply SControlRigFunctionLocalizationDialog::OnButtonClick(EAppReturnType::Type ButtonID)
{
	UserResponse = ButtonID;
	RequestDestroyWindow();

	return FReply::Handled();
}

bool SControlRigFunctionLocalizationDialog::IsOkButtonEnabled() const
{
	return FunctionsWidget->FunctionsToLocalize.Num() > 0;
}

EAppReturnType::Type SControlRigFunctionLocalizationDialog::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

TArray<URigVMLibraryNode*>& SControlRigFunctionLocalizationDialog::GetFunctionsToLocalize()
{
	return FunctionsWidget->FunctionsToLocalize;
}

#undef LOCTEXT_NAMESPACE
