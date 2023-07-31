// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "LiveLinkTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "EditorUndoClient.h"

class ITableRow;
class FLiveLinkClient;
struct FLiveLinkSourceUIEntry;
struct FLiveLinkSubjectUIEntry;
class FMenuBuilder;
class FUICommandList;
class SLiveLinkDataView;
class ULiveLinkSourceFactory;
class SLiveLinkSourceListView;
class STableViewBase;

typedef TSharedPtr<FLiveLinkSourceUIEntry> FLiveLinkSourceUIEntryPtr;
typedef TSharedPtr<FLiveLinkSubjectUIEntry> FLiveLinkSubjectUIEntryPtr;

class SLiveLinkClientPanel : public SCompoundWidget, public FGCObject, public FEditorUndoClient
{
	SLATE_BEGIN_ARGS(SLiveLinkClientPanel){}
	SLATE_END_ARGS()

	~SLiveLinkClientPanel();

	void Construct(const FArguments& Args, FLiveLinkClient* InClient);

	// FGCObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SLiveLinkClientPanel");
	}
	// End of FGCObject interface

	// FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess);
	virtual void PostRedo(bool bSuccess);
	// End FEditorUndoClient interface

private:

	void BindCommands();

	void RefreshSourceData(bool bRefreshUI);

	void RetrieveFactorySourcePanel(FMenuBuilder& MenuBuilder, ULiveLinkSourceFactory* FactoryCDO);

	FReply OnCloseSourceSelectionPanel(ULiveLinkSourceFactory* FactoryCDO, bool bMakeSource);

	void HandleOnAddFromFactory(ULiveLinkSourceFactory* InSourceFactory);

	// Starts adding a new virtual subject to the client
	void AddVirtualSubject();

	// Handles adding a virtual subject after the user has picked a name
	void HandleAddVirtualSubject(const FText& NewSubjectName, ETextCommit::Type CommitInfo);

	// Callback when property changes on source settings
	void OnPropertyChanged(const FPropertyChangedEvent& InEvent);

	// Remove source command handlers
	void HandleRemoveSource();
	bool CanRemoveSource();

	// Remove all sources command handlers
	void HandleRemoveAllSources();
	bool HasSource() const;

	// Remove subject command handlers
	bool CanRemoveSubject() const;
	void HandleRemoveSubject();

	// Registered with the client and called when client's sources change
	void OnSourcesChangedHandler();

	// Registered with the client and called when client's subjects change
	void OnSubjectsChangedHandler();

	// Controls whether the editor performance throttling warning should be visible
	EVisibility ShowEditorPerformanceThrottlingWarning() const;

	// Handle disabling of editor performance throttling
	FReply DisableEditorPerformanceThrottling();

	// Return the message count text
	FText GetMessageCountText() const;

	// Return the occurrence count and last time occurred text
	FText GetSelectedMessageOccurrenceText() const;

private:
	int32 GetDetailWidgetIndex() const;

	TSharedRef<ITableRow> MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedPtr<SWidget> OnSourceConstructContextMenu();

	// Helper functions for building the subject tree UI
	TSharedRef<ITableRow> MakeTreeRowWidget(FLiveLinkSubjectUIEntryPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	void GetChildrenForInfo(FLiveLinkSubjectUIEntryPtr InInfo, TArray< FLiveLinkSubjectUIEntryPtr >& OutChildren);
	void RebuildSubjectList();

	// Handler for the source list selection changing
	void OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const;

	// Handler for the subject tree selection changing
	void OnSubjectTreeSelectionChanged(FLiveLinkSubjectUIEntryPtr BoneInfo, ESelectInfo::Type SelectInfo);

	// Handler for the subject tree context menu opening
	TSharedPtr<SWidget> OnOpenVirtualSubjectContextMenu();

	// Source list widget
	TSharedPtr<SLiveLinkSourceListView> SourceListView;

	// Source list items
	TArray<FLiveLinkSourceUIEntryPtr> SourceData;

	// Subject tree widget
	TSharedPtr<STreeView<FLiveLinkSubjectUIEntryPtr>> SubjectsTreeView;

	// Subject tree items
	TArray<FLiveLinkSubjectUIEntryPtr> SubjectData;

	TSharedPtr<FUICommandList> CommandList;

	FLiveLinkClient* Client;

	// Reference to connection settings struct details panel
	TSharedPtr<class IDetailsView> SettingsDetailsView;

	// Reference to the data value struct details panel
	TSharedPtr<class SLiveLinkDataView> DataDetailsView;

	// Handle to delegate when client sources list has changed */
	FDelegateHandle OnSourcesChangedHandle;

	// Handle to delegate when a client subjects list has changed */
	FDelegateHandle OnSubjectsChangedHandle;

	// Map to cover 
	TMap<UClass*, UObject*> DetailsPanelEditorObjects;

	// Details index
	int32 DetailWidgetIndex;

	// Guard from reentrant selection
	mutable bool bSelectionChangedGuard;
};
