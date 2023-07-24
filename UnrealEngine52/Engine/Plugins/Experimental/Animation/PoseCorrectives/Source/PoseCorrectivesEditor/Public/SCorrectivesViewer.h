// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesEditorController.h"

#include "IEditableSkeleton.h"
#include "IPersonaPreviewScene.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"


class FPoseCorrectivesEditorController;
class SCorrectivesViewer;
class SSearchBox;

//////////////////////////////////////////////////////////////////////////
// FDisplayedPoseInfo

class FDisplayedCorrectiveInfo
{
public:
	FName Name;
	FName Group;

	/** Static function for creating a new item, but ensures that you can only have a TSharedRef to one */
	static TSharedRef<FDisplayedCorrectiveInfo> Make(const FName& Name, const FName& Group)
	{
		return MakeShareable(new FDisplayedCorrectiveInfo(Name, Group));
	}

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE(FOnRenameRequested);
	FOnRenameRequested OnRenameRequested;

protected:
	/** Hidden constructor, always use Make above */
	FDisplayedCorrectiveInfo(const FName& InName, const FName& InGroup)
		: Name(InName)
		, Group(InGroup)
	{}

	/** Hidden constructor, always use Make above */
	FDisplayedCorrectiveInfo() {}
};

typedef SListView< TSharedPtr<FDisplayedCorrectiveInfo> > SCorrectivesListType;

//////////////////////////////////////////////////////////////////////////
// SPoseListRow

class SCorrectiveListRow : public SMultiColumnTableRow< TSharedPtr<FDisplayedCorrectiveInfo> >
{
public:

	SLATE_BEGIN_ARGS(SCorrectiveListRow) {}

		/** The item for this row **/
		SLATE_ARGUMENT(TSharedPtr<FDisplayedCorrectiveInfo>, Item)

		/* The SPoseViewer that we push the morph target weights into */
		SLATE_ARGUMENT(TWeakPtr<SCorrectivesViewer>, CorrectivesViewer)

		/** Filter text typed by the user into the parent tree's search widget */
		SLATE_ARGUMENT(FText, FilterText);

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView, const TSharedRef<IPersonaPreviewScene>& InPreviewScene);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:
	FText GetName() const;
	void OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const;
	bool OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage);

	TWeakPtr<SCorrectivesViewer> CorrectivesViewerPtr;

	/** The name and weight of the morph target */
	TSharedPtr<FDisplayedCorrectiveInfo>	Item;
	
	/** Text the user typed into the search box - used for text highlighting */
	FText FilterText;

	/** The preview scene we are viewing */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
};


//////////////////////////////////////////////////////////////////////////
// SPoseViewer

class SCorrectivesViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCorrectivesViewer)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FPoseCorrectivesEditorController>& InEditorController, const TSharedRef<class IPersonaPreviewScene>& InPreviewScene);
	virtual ~SCorrectivesViewer();

	void HighlightCorrective(const FName& CorrectiveName);
	void ClearHighlightedItems();

	//quick HACKKKK, was in private
	void OnCorrectivesAssetModified();


private:

	/** SWidget interface */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	/**
	* Is registered with Persona to handle when its preview mesh is changed.
	*
	* @param NewPreviewMesh - The new preview mesh being used by Persona
	*
	*/
	void OnPreviewMeshChanged(class USkeletalMesh* OldPreviewMesh, class USkeletalMesh* NewPreviewMesh);

	/**
	* Filters the SListView when the user changes the search text box (NameFilterBox)
	*
	* @param SearchText - The text the user has typed
	*
	*/
	void OnFilterTextChanged(const FText& SearchText);

	/**
	* Filters the SListView when the user hits enter or clears the search box
	* Simply calls OnFilterTextChanged
	*
	* @param SearchText - The text the user has typed
	* @param CommitInfo - Not used
	*
	*/
	void OnFilterTextCommitted(const FText& SearchText, ETextCommit::Type CommitInfo);

	/**
	* Accessor so our rows can grab the filtertext for highlighting
	*
	*/
	FText& GetFilterText() { return FilterText; }

	TSharedRef<ITableRow> GenerateCorrectiveRow(TSharedPtr<FDisplayedCorrectiveInfo> InInfo, const TSharedRef<STableViewBase>& OwnerTable);	

	bool IsCorrectiveSelected() const;
	bool IsSingleCorrectiveSelected() const;
	void OnDeleteCorrectives();
	void OnRenameCorrective();

	bool ModifyName(FName OldName, FName NewName);
	
	void BindCommands();
	
	TSharedPtr<SWidget> OnGetContextMenuContent() const;
	void OnListDoubleClick(TSharedPtr<FDisplayedCorrectiveInfo> InItem);


	/**
	* Clears and rebuilds the table, according to an optional search string
	*
	* @param SearchText - Optional search string
	*
	*/
	void CreateCorrectivesList(const FString& SearchText = FString());
	void PopulateGroupsList();
	
	/** Pointer to the preview scene we are viewing */
	TWeakPtr<class IPersonaPreviewScene> PreviewScenePtr;
	TWeakPtr<FPoseCorrectivesEditorController> EditorControllerPtr;
	TWeakObjectPtr<UPoseCorrectivesAsset> PoseCorrectivesAssetPtr;

	/** Box to filter to a specific morph target name */
	TSharedPtr<SSearchBox>	NameFilterBox;

	TSharedPtr<SCorrectivesListType> CorrectivesListView;
	TArray< TSharedPtr<FDisplayedCorrectiveInfo> > CorrectivesList;
	TArray<TSharedPtr<FName>> GroupNames;

	/** Current text typed into NameFilterBox */
	FText FilterText;

	/** Commands that are bound to delegates*/
	TSharedPtr<FUICommandList> UICommandList;

	FDelegateHandle OnDelegateCorrectivesListChangedDelegateHandle;

	friend class SCorrectiveListRow;
};
