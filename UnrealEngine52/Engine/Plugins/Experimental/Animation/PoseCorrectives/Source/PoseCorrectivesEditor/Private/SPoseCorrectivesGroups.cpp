// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPoseCorrectivesGroups.h"

#include "PoseCorrectivesEditorController.h"
#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesCommands.h"

#include "AnimPreviewInstance.h"
#include "Dialogs/Dialogs.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Preferences/PersonaOptions.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SPoseCorrectivesGroups"

static const FName ColumnId_AnimControlNameLabel("Control Name");
static const FName ColumnID_AnimCurveSelectDriverLabel("Selected Driver");
static const FName ColumnID_AnimCurveSelectCorrectiveLabel("Selected Corrective");

void SPoseCorrectivesGroupsListRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	PoseGroupsViewerPtr = InArgs._PoseGroupsViewerPtr;
	FilterText = InArgs._FilterText;

	check(Item.IsValid());

	SMultiColumnTableRow< TSharedPtr<FPoseCorrectivesGroupItem> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
}

TSharedRef< SWidget > SPoseCorrectivesGroupsListRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ColumnId_AnimControlNameLabel)
	{
		TSharedPtr<SPoseCorrectivesGroups> AnimCurveViewer = PoseGroupsViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &SPoseCorrectivesGroupsListRow::GetItemName)
					.HighlightText(FilterText)
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else if (ColumnName == ColumnID_AnimCurveSelectDriverLabel)
	{
		TSharedPtr<SPoseCorrectivesGroups> AnimCurveViewer = PoseGroupsViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SPoseCorrectivesGroupsListRow::OnDriverSelectChecked, Item->IsCurve)
					.IsChecked(this, &SPoseCorrectivesGroupsListRow::IsDriverSelectChangedChecked, Item->IsCurve)
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else if (ColumnName == ColumnID_AnimCurveSelectCorrectiveLabel)
	{
		TSharedPtr<SPoseCorrectivesGroups> AnimCurveViewer = PoseGroupsViewerPtr.Pin();
		if (AnimCurveViewer.IsValid())
		{
			return
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SPoseCorrectivesGroupsListRow::OnCorrectiveSelectChecked, Item->IsCurve)
					.IsChecked(this, &SPoseCorrectivesGroupsListRow::IsCorrectiveSelectChangedChecked, Item->IsCurve)
				];
		}
		else
		{
			return SNullWidget::NullWidget;
		}
	}
	else
	{
		return SNullWidget::NullWidget;
	}
}

void SPoseCorrectivesGroupsListRow::OnDriverSelectChecked(ECheckBoxState InState, bool curve)
{
	if (PoseGroupsViewerPtr.Pin()->SelectedGroup == nullptr)
	{
		return;
	}

	FString Group = PoseGroupsViewerPtr.Pin()->SelectedGroup->ToString();
	bool bNewData = (InState == ECheckBoxState::Checked);

	if (curve)
	{
		if (bNewData)
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverCurves.Add(FName(Item->ControlName.ToString()));
		}
		else
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverCurves.Remove(FName(Item->ControlName.ToString()));
		}
	}
	else
	{
		if (bNewData)
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverBones.Add(FName(Item->ControlName.ToString()));
		}
		else
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverBones.Remove(FName(Item->ControlName.ToString()));
		}
	}

	PoseGroupsViewerPtr.Pin()->EditorController.Pin()->HandleGroupEdited(FName(Group));
}

void SPoseCorrectivesGroupsListRow::OnCorrectiveSelectChecked(ECheckBoxState InState, bool curve)
{
	if (PoseGroupsViewerPtr.Pin()->SelectedGroup == nullptr)
	{
		return;
	}

	FString Group = PoseGroupsViewerPtr.Pin()->SelectedGroup->ToString();
	bool bNewData = (InState == ECheckBoxState::Checked);

	if (curve)
	{
		if (bNewData)
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveCurves.Add(FName(Item->ControlName.ToString()));
		}
		else
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveCurves.Remove(FName(Item->ControlName.ToString()));
		}
	}
	else
	{
		if (bNewData)
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveBones.Add(FName(Item->ControlName.ToString()));
		}
		else
		{
			PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveBones.Remove(FName(Item->ControlName.ToString()));
		}
	}

	PoseGroupsViewerPtr.Pin()->EditorController.Pin()->HandleGroupEdited(FName(Group));
}

ECheckBoxState SPoseCorrectivesGroupsListRow::IsDriverSelectChangedChecked(bool curve) const
{
	if (PoseGroupsViewerPtr.Pin()->SelectedGroup == nullptr)
	{
		return ECheckBoxState::Unchecked;
	}

	FString Group = PoseGroupsViewerPtr.Pin()->SelectedGroup->ToString();

	bool bData = false;
	if (curve)
	{
		bData = PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverCurves.Contains(FName(Item->ControlName.ToString()));
	}
	else
	{
		bData = PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverBones.Contains(FName(Item->ControlName.ToString()));
	}

	return (bData) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SPoseCorrectivesGroupsListRow::IsCorrectiveSelectChangedChecked(bool curve) const
{
	if (PoseGroupsViewerPtr.Pin()->SelectedGroup == nullptr)
	{
		return ECheckBoxState::Unchecked;
	}

	FString Group = PoseGroupsViewerPtr.Pin()->SelectedGroup->ToString();

	bool bData = false;
	if (curve)
	{
		bData = PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveCurves.Contains(FName(Item->ControlName.ToString()));
	}
	else
	{
		bData = PoseGroupsViewerPtr.Pin()->EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveBones.Contains(FName(Item->ControlName.ToString()));
	}

	return (bData) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText SPoseCorrectivesGroupsListRow::GetItemName() const
{
	return Item->ControlName;
}

void SPoseCorrectivesGroups::Construct(const FArguments& InArgs, TSharedRef<FPoseCorrectivesEditorController> InEditorController)
{
	EditorController = InEditorController;
	EditorController.Pin()->SetPoseCorrectivesGroupsView(SharedThis(this));

	BindCommands();

	ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(5))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.Padding(0.5f)
			[
				SAssignNew(NewGroupBox, SEditableTextBox)
			.HintText(LOCTEXT("NewGroup", "Enter new group name"))
			]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SPoseCorrectivesGroups::CreateNewGroup)
				[
					SNew(STextBlock)
				.Text(LOCTEXT("AddGroupLabel", "Add Group"))
				]
			]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.Padding(0.5f)
			[

				SAssignNew(GroupsComboBox, SComboBox<TSharedPtr<FText>>)
				.InitiallySelectedItem(SelectedGroup)
			.OptionsSource(&GroupNames)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
				{
					SelectedGroup = NewSelection;
				})
			.OnGenerateWidget_Lambda([](TSharedPtr<FText> Option)
				{
					return SNew(STextBlock)
						.Text(*Option);
				})
					[
						SNew(STextBlock)
					.Text_Lambda([this]()
						{
							return SelectedGroup.IsValid() ? *SelectedGroup : FText::GetEmpty();
						})
					]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SPoseCorrectivesGroups::DeleteGroup)
				[
					SNew(STextBlock)
				.Text(LOCTEXT("DeleteBtn", "Delete Group"))
				]
			]
		]
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				// Filter entry
			+ SHorizontalBox::Slot()
			.FillWidth(1)
				[
					SNew(SSearchBox)
					.SelectAllTextWhenFocused(true)
				.OnTextChanged(this, &SPoseCorrectivesGroups::OnCurveFilterTextChanged)
				]
			]
		+ SVerticalBox::Slot()
			.Padding(2.0f)
			.FillHeight(1.0f)		// This is required to make the scrollbar work, as content overflows Slate containers by default
			[
				SAssignNew(AnimCurveListView, SPoseCorrectivesGroupListType)
				.ListItemsSource(&AnimCurveList)
			.OnGenerateRow(this, &SPoseCorrectivesGroups::GenerateAnimCurveRow, true)
			.OnContextMenuOpening(this, &SPoseCorrectivesGroups::OnGetCurveContextMenuContent)
			.ItemHeight(22.0f)
			.SelectionMode(ESelectionMode::Multi)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ColumnId_AnimControlNameLabel)
				.FillWidth(1.f)
				.DefaultLabel(LOCTEXT("AnimControlNameLabel", "Curve Name"))

				+ SHeaderRow::Column(ColumnID_AnimCurveSelectDriverLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimControlSelectDriverLabel", "Driver Select"))

				+ SHeaderRow::Column(ColumnID_AnimCurveSelectCorrectiveLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimControlSelectCorrectiveLabel", "Corrective Select"))
			)
			]
		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)
				// Filter entry
			+ SHorizontalBox::Slot()
			.FillWidth(1)
				[
					SNew(SSearchBox)
					.SelectAllTextWhenFocused(true)
				.OnTextChanged(this, &SPoseCorrectivesGroups::OnBoneFilterTextChanged)
				//.OnTextCommitted(this, &SPoseCorrectivesGroups::OnFilterTextCommitted)
				]
			]
		+ SVerticalBox::Slot()
			.Padding(2.0f)
			.FillHeight(1.0f)		// This is required to make the scrollbar work, as content overflows Slate containers by default
			[
				SAssignNew(AnimBoneListView, SPoseCorrectivesGroupListType)
				.ListItemsSource(&AnimBoneList)
			.OnGenerateRow(this, &SPoseCorrectivesGroups::GenerateAnimCurveRow, false)
			.OnContextMenuOpening(this, &SPoseCorrectivesGroups::OnGetBoneContextMenuContent)
			.ItemHeight(22.0f)
			.SelectionMode(ESelectionMode::Multi)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(ColumnId_AnimControlNameLabel)
				.FillWidth(1.f)
				.DefaultLabel(LOCTEXT("AnimBoneNameLabel", "Bone Name"))

				+ SHeaderRow::Column(ColumnID_AnimCurveSelectDriverLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimCurveSelectDriverLabel", "Driver Select"))

				+ SHeaderRow::Column(ColumnID_AnimCurveSelectCorrectiveLabel)
				.FillWidth(0.25f)
				.DefaultLabel(LOCTEXT("AnimCurveSelectCorrectiveLabel", "Corrective Select"))
			)
			]
			]
		];

		SyncWithAsset();
}

//seems very silly to have 3 functions to do filtering...
void SPoseCorrectivesGroups::OnCurveFilterTextChanged(const FText& SearchText)
{
	CurveFilterText = SearchText;
	FilterCurvesList(SearchText.ToString());
}

void SPoseCorrectivesGroups::OnBoneFilterTextChanged(const FText& SearchText)
{
	BoneFilterText = SearchText;
	FilterBonesList(SearchText.ToString());
}

FReply SPoseCorrectivesGroups::CreateNewGroup()
{
	FName GroupName = FName(NewGroupBox->GetText().ToString());
	if (EditorController.Pin()->Asset->AddGroup(GroupName))
	{
		GroupNames.Add(MakeShareable(new FText(NewGroupBox->GetText())));
		GroupsComboBox->RefreshOptions();
		SelectedGroup = GroupNames.Last();
		EditorController.Pin()->HandleGroupListChanged();
	}

	return FReply::Handled();
}

FReply SPoseCorrectivesGroups::DeleteGroup()
{
	GroupNames.Remove(SelectedGroup);
	EditorController.Pin()->Asset->RemoveGroup(FName(SelectedGroup->ToString()));
	SelectedGroup = nullptr;

	GroupsComboBox->RefreshOptions();

	EditorController.Pin()->HandleGroupListChanged();

	return FReply::Handled();
}

TSharedRef<ITableRow> SPoseCorrectivesGroups::GenerateAnimCurveRow(TSharedPtr<FPoseCorrectivesGroupItem> InInfo, const TSharedRef<STableViewBase>& OwnerTable, bool IsCurve)
{
	check(InInfo.IsValid());

	FText FilterText;
	if (IsCurve)
		FilterText = CurveFilterText;
	else
		FilterText = BoneFilterText;

	return
		SNew(SPoseCorrectivesGroupsListRow, OwnerTable)
		.Item(InInfo)
		.PoseGroupsViewerPtr(SharedThis(this))
		.FilterText(FilterText);
}

TSharedPtr<SWidget> SPoseCorrectivesGroups::OnGetCurveContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, UICommandList);

	MenuBuilder.BeginSection("CorrectivesAction", LOCTEXT("SelectedItems", "Selected Item Actions"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiSelectCorrectiveCurvesCommand, NAME_None, LOCTEXT("SelectCorrectiveCurvesLabel", "Select Corrective Curves"), LOCTEXT("SelectCorrectiveCurvesTooltip", "Select the highlighted corrective curves"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiSelectDriverCurvesCommand, NAME_None, LOCTEXT("SelectDriverCurvesLabel", "Select Driver Curves"), LOCTEXT("SelectDriverCurvesTooltip", "Select the highlighted driver curves"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiDeselectCorrectiveCurvesCommand, NAME_None, LOCTEXT("DeselectCorrectiveCurvesLabel", "Deselect Corrective Curves"), LOCTEXT("DeselectCorrectiveCurvesTooltip", "Deselect the highlighted corrective curves"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiDeselectDriverCurvesCommand, NAME_None, LOCTEXT("DeselectDriverCurvesLabel", "Deselect Driver Curves"), LOCTEXT("DeselectDriverCurvesTooltip", "Deselect the highlighted driver curves"));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SPoseCorrectivesGroups::OnGetBoneContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, UICommandList);

	MenuBuilder.BeginSection("CorrectivesAction", LOCTEXT("SelectedItems", "Selected Item Actions"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiSelectCorrectiveBonesCommand, NAME_None, LOCTEXT("SelectCorrectiveBonesLabel", "Select Corrective Bones"), LOCTEXT("SelectCorrectiveBonesTooltip", "Select the highlighted corrective bones"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiSelectDriverBonesCommand, NAME_None, LOCTEXT("SelectDriverBonesLabel", "Select Driver Bones"), LOCTEXT("SelectDriverBonesTooltip", "Select the highlighted driver bones"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiDeselectCorrectiveBonesCommand, NAME_None, LOCTEXT("DeselectCorrectiveBonesLabel", "Deselect Corrective Bones"), LOCTEXT("DeselectCorrectiveBonesTooltip", "Deselect the highlighted corrective bones"));
	MenuBuilder.AddMenuEntry(FPoseCorrectivesCommands::Get().MultiDeselectDriverBonesCommand, NAME_None, LOCTEXT("DeselectDriverBonesLabel", "Deselect Driver Bones"), LOCTEXT("DeselectDriverBonesTooltip", "Deselect the highlighted driver bones"));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SPoseCorrectivesGroups::BindCommands()
{
	// This should not be called twice on the same instance
	check(!UICommandList.IsValid());
	UICommandList = MakeShareable(new FUICommandList);
	FUICommandList& CommandList = *UICommandList;

	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiSelectCorrectiveCurvesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiSelectCorrectiveCurves),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectCurves));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiSelectDriverCurvesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiSelectDriverCurves),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectCurves));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiSelectCorrectiveBonesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiSelectCorrectiveBones),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectBones));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiSelectDriverBonesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiSelectDriverBones),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectBones));

	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiDeselectCorrectiveCurvesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiDeselectCorrectiveCurves),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectCurves));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiDeselectDriverCurvesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiDeselectDriverCurves),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectCurves));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiDeselectCorrectiveBonesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiDeselectCorrectiveBones),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectBones));
	CommandList.MapAction(
		FPoseCorrectivesCommands::Get().MultiDeselectDriverBonesCommand,
		FExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::OnMultiDeselectDriverBones),
		FCanExecuteAction::CreateSP(this, &SPoseCorrectivesGroups::CanMultiSelectBones));
}

void SPoseCorrectivesGroups::OnMultiSelectCorrectiveCurves()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimCurveListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveCurves.Add(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiSelectDriverCurves()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimCurveListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverCurves.Add(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiSelectCorrectiveBones()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimBoneListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveBones.Add(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiSelectDriverBones()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimBoneListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverBones.Add(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiDeselectCorrectiveCurves()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimCurveListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveCurves.Remove(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiDeselectDriverCurves()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimCurveListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverCurves.Remove(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiDeselectCorrectiveBones()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimBoneListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].CorrectiveBones.Remove(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

void SPoseCorrectivesGroups::OnMultiDeselectDriverBones()
{
	TArray<TSharedPtr<FPoseCorrectivesGroupItem>> SelectedRows = AnimBoneListView->GetSelectedItems();
	FString Group = SelectedGroup->ToString();
	for (const auto& SelectedRow : SelectedRows)
	{
		EditorController.Pin()->Asset->GroupDefinitions[FName(Group)].DriverBones.Remove(FName(SelectedRow->ControlName.ToString()));
	}
	EditorController.Pin()->Asset->Modify();
}

bool SPoseCorrectivesGroups::CanMultiSelectCurves()
{
	return SelectedGroup != nullptr && AnimCurveListView->GetSelectedItems().Num() > 0;
}

bool SPoseCorrectivesGroups::CanMultiSelectBones()
{
	return SelectedGroup != nullptr && AnimBoneListView->GetSelectedItems().Num() > 0;
}

void SPoseCorrectivesGroups::HandleMeshChanged()
{
	PopulateCorrectiveBoneNames();
	PopulateCorrectiveCurveNames();

	AnimBoneListView->RequestListRefresh();
	AnimCurveListView->RequestListRefresh();
}

void SPoseCorrectivesGroups::FilterCurvesList(const FString& SearchText)
{
	AnimCurveList.Empty();

	TArray<FName> CurveNamesList = EditorController.Pin()->Asset->GetCurveNames();
	bool bDoFiltering = !SearchText.IsEmpty();

	for (const FName& CurveName : CurveNamesList)
	{
		if (bDoFiltering && !CurveName.ToString().Contains(SearchText))
		{
			continue; // Skip items that don't match our filter
		}
		TSharedRef<FPoseCorrectivesGroupItem> groupItem = FPoseCorrectivesGroupItem::Make(FText::FromName(CurveName), true);
		AnimCurveList.Add(groupItem);
	}

	AnimCurveListView->RequestListRefresh();
}

void SPoseCorrectivesGroups::FilterBonesList(const FString& SearchText)
{
	AnimBoneList.Empty();

	TArray<FName> BoneNamesList = EditorController.Pin()->Asset->GetBoneNames();
	bool bDoFiltering = !SearchText.IsEmpty();

	for (const FName& BoneName : BoneNamesList)
	{
		if (bDoFiltering && !BoneName.ToString().Contains(SearchText))
		{
			continue; // Skip items that don't match our filter
		}
		TSharedRef<FPoseCorrectivesGroupItem> groupItem = FPoseCorrectivesGroupItem::Make(FText::FromName(BoneName), true);
		AnimBoneList.Add(groupItem);
	}

	AnimBoneListView->RequestListRefresh();
}

void SPoseCorrectivesGroups::SyncWithAsset()
{
	TArray<FName> Groups = EditorController.Pin()->Asset->GetGroupNames();
	GroupNames.Empty();
	for (const auto& Group : Groups)
	{
		GroupNames.Add(MakeShareable(new FText(FText::FromName(Group))));
	}
	GroupsComboBox->RefreshOptions();
	HandleMeshChanged();
}

void SPoseCorrectivesGroups::PopulateCorrectiveBoneNames()
{
	TArray<FName> BoneNamesList = EditorController.Pin()->Asset->GetBoneNames();

	AnimBoneList.Empty();
	for (const auto& Name : BoneNamesList)
	{
		TSharedRef<FPoseCorrectivesGroupItem> groupItem = FPoseCorrectivesGroupItem::Make(FText::FromName(Name), false);
		AnimBoneList.Add(groupItem);
	}
}

void SPoseCorrectivesGroups::PopulateCorrectiveCurveNames()
{
	TArray<FName> CurveNamesList = EditorController.Pin()->Asset->GetCurveNames();

	AnimCurveList.Empty();
	for (const auto& Name : CurveNamesList)
	{
		TSharedRef<FPoseCorrectivesGroupItem> groupItem = FPoseCorrectivesGroupItem::Make(FText::FromName(Name), true);
		AnimCurveList.Add(groupItem);
	}
}

#undef LOCTEXT_NAMESPACE
