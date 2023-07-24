// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"


class FPoseCorrectivesEditorController;
class FUICommandList;
class SEditableTextBox;
class SSearchBox;
template<typename T> class SComboBox;


class FPoseCorrectivesGroupItem
{
public:
	FText ControlName;
	bool IsCurve;

	static TSharedRef<FPoseCorrectivesGroupItem> Make(const FText& InName, bool InIsCurve)
	{
		return MakeShareable(new FPoseCorrectivesGroupItem(InName, InIsCurve));
	}

protected:
	/** Hidden constructor, always use Make above */
	FPoseCorrectivesGroupItem(const FText& InName, bool InIsCurve)
		: ControlName(InName), IsCurve(InIsCurve)
	{}

};

typedef SListView< TSharedPtr<FPoseCorrectivesGroupItem> > SPoseCorrectivesGroupListType;

class SPoseCorrectivesGroupsListRow : public SMultiColumnTableRow< TSharedPtr<FPoseCorrectivesGroupItem> >
{
public:

	SLATE_BEGIN_ARGS(SPoseCorrectivesGroupsListRow) {}

	/** The item for this row **/
	SLATE_ARGUMENT(TSharedPtr<FPoseCorrectivesGroupItem>, Item)

		SLATE_ARGUMENT(class TWeakPtr<class SPoseCorrectivesGroups>, PoseGroupsViewerPtr)

		/** Filter text typed by the user into the parent tree's search widget */
		SLATE_ARGUMENT(FText, FilterText);

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	void OnDriverSelectChecked(ECheckBoxState InState, bool curve);
	void OnCorrectiveSelectChecked(ECheckBoxState InState, bool curve);
	ECheckBoxState IsDriverSelectChangedChecked(bool curve) const;
	ECheckBoxState IsCorrectiveSelectChangedChecked(bool curve) const;

	FText GetItemName() const;

	/** Text the user typed into the search box - used for text highlighting */
	FText FilterText;

	TWeakPtr<SPoseCorrectivesGroups> PoseGroupsViewerPtr;
	TSharedPtr<FPoseCorrectivesGroupItem> Item;

};

class SPoseCorrectivesGroups : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SPoseCorrectivesGroups) {}
	SLATE_END_ARGS()

    ~SPoseCorrectivesGroups() {};

	void Construct(const FArguments& InArgs, TSharedRef<FPoseCorrectivesEditorController> InEditorController);
	FReply CreateNewGroup();
	FReply DeleteGroup();

	void PopulateCorrectiveBoneNames();
	void PopulateCorrectiveCurveNames();

	TSharedRef<ITableRow> GenerateAnimCurveRow(TSharedPtr<FPoseCorrectivesGroupItem> InInfo, const TSharedRef<STableViewBase>& OwnerTable, bool IsCurve);
	void HandleMeshChanged();

private:
	void OnCurveFilterTextChanged(const FText& SearchText);
	void OnBoneFilterTextChanged(const FText& SearchText);
	TSharedPtr<SWidget> OnGetCurveContextMenuContent() const;
	TSharedPtr<SWidget> OnGetBoneContextMenuContent() const;
	void BindCommands();

	void OnMultiSelectCorrectiveCurves();
	void OnMultiSelectDriverCurves();
	void OnMultiSelectCorrectiveBones();
	void OnMultiSelectDriverBones();
	void OnMultiDeselectCorrectiveCurves();
	void OnMultiDeselectDriverCurves();
	void OnMultiDeselectCorrectiveBones();
	void OnMultiDeselectDriverBones();
	bool CanMultiSelectCurves();
	bool CanMultiSelectBones();

	FText CurveFilterText;
	FText BoneFilterText;

	void FilterCurvesList(const FString& SearchText);
	void FilterBonesList(const FString& SearchText);

	void SyncWithAsset();

	TSharedPtr<SSearchBox>	NameFilterBox;
	TSharedPtr<FUICommandList> UICommandList;

	TWeakPtr<FPoseCorrectivesEditorController> EditorController;

	TArray<TSharedPtr<FText>> GroupNames;
	TSharedPtr<SEditableTextBox> NewGroupBox;
	TSharedPtr<SComboBox<TSharedPtr<FText>>> GroupsComboBox;
	TSharedPtr<FText> SelectedGroup;

	TSharedPtr<SPoseCorrectivesGroupListType> AnimCurveListView;
	TSharedPtr<SPoseCorrectivesGroupListType> AnimBoneListView;
	TArray< TSharedPtr<FPoseCorrectivesGroupItem> > AnimCurveList;
	TArray< TSharedPtr<FPoseCorrectivesGroupItem> > AnimBoneList;

	friend class SPoseCorrectivesGroupsListRow;
};
