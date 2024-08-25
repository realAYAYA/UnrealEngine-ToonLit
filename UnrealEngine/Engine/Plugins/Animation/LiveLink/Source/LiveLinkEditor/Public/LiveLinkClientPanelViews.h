// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Input/Events.h"
#include "Layout/Geometry.h"
#include "LiveLinkRole.h"
#include "LiveLinkSourceSettings.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"

class FLiveLinkClient;
struct FLiveLinkSourceUIEntry;
class FLiveLinkSourcesView;
struct FLiveLinkSubjectUIEntry;
class FLiveLinkSubjectsView;
class FUICommandList;
class IDetailsView;
class ITableRow;
struct FPropertyChangedEvent;
class SLiveLinkSourceListView;
class SLiveLinkDataView;
class STableViewBase;
class SWidget;

typedef TSharedPtr<FLiveLinkSourceUIEntry> FLiveLinkSourceUIEntryPtr;
typedef TSharedPtr<FLiveLinkSubjectUIEntry> FLiveLinkSubjectUIEntryPtr;

namespace UE::LiveLink
{
	TSharedPtr<IDetailsView> CreateSourcesDetailsView(const TSharedPtr<FLiveLinkSourcesView>& InSourcesView, const TAttribute<bool>& bInReadOnly);
	TSharedPtr<SLiveLinkDataView> CreateSubjectsDetailsView(FLiveLinkClient* InLiveLinkClient, const TAttribute<bool>& bInReadOnly);
}

// Structure that defines a single entry in the subject UI
struct FLiveLinkSubjectUIEntry
{
	FLiveLinkSubjectUIEntry(const FLiveLinkSubjectKey& InSubjectKey, FLiveLinkClient* InClient);

	// Subject key
	FLiveLinkSubjectKey SubjectKey;
	// LiveLink Client
	FLiveLinkClient* Client;

	// Children (if this entry represents a source instead of a specific subject
	TArray<TSharedPtr<FLiveLinkSubjectUIEntry>> Children;

	// Whether the subject entry is a subject
	bool IsSubject() const;
	// Whether the subject entry is a source
	bool IsSource() const;
	// Whether the subject is virtual
	bool IsVirtualSubject() const;
	// Get the subject or source settings
	UObject* GetSettings() const;
	// Whether the subject is enabled
	bool IsSubjectEnabled() const;
	// Whether the subject is valid
	bool IsSubjectValid() const;
	// Enable or disable a subject
	void SetSubjectEnabled(bool bIsEnabled);
	// Get a textual representation of the ui entry 
	FText GetItemText() const;
	// Get the livelink role of this entry
	TSubclassOf<ULiveLinkRole> GetItemRole() const;
	// Remove the subject or source from the livelink client
	void RemoveFromClient() const;

private:
	// Whether the subject is virtual
	bool bIsVirtualSubject = false;
};

// Structure that defines a single entry in the source UI
struct FLiveLinkSourceUIEntry
{
public:
	FLiveLinkSourceUIEntry(FGuid InEntryGuid, FLiveLinkClient* InClient)
		: EntryGuid(InEntryGuid)
		, Client(InClient)
	{}

	// Get the guid of the source
	FGuid GetGuid() const;
	// Get the type of the source
	FText GetSourceType() const;
	// Get the machine name of the source
	FText GetMachineName() const;
	// Get the source's status
	FText GetStatus() const;
	// Get the source's settings
	ULiveLinkSourceSettings* GetSourceSettings() const;
	// Remove the source from the client
	void RemoveFromClient() const;
	// Get the display name of the source
	FText GetDisplayName() const;

private:
	// GUID of the source
	FGuid EntryGuid;
	// Pointer to the livelink client
	FLiveLinkClient* Client = nullptr;
};

namespace UE::LiveLink
{
	/** Base class for live link list/tree views, handles removing list element by pressing delete. */
	template <typename ListType, typename ListElementType>
	class SLiveLinkListView : public ListType
	{
	public:
		virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace)
			{
				if (!bReadOnly.Get())
				{
					TArray<ListElementType> SelectedItem = ListType::GetSelectedItems();
					for (ListElementType Item : SelectedItem)
					{
						Item->RemoveFromClient();
					}
				}
				return FReply::Handled();
			}

			return FReply::Unhandled();
		}

		// Whether the panel is in read-only mode or not.
		TAttribute<bool> bReadOnly;
	};
}

class SLiveLinkSourceListView : public UE::LiveLink::SLiveLinkListView<SListView<FLiveLinkSourceUIEntryPtr>, FLiveLinkSourceUIEntryPtr>
{
public:
	void Construct(const FArguments& InArgs, TAttribute<bool> bInReadOnly)
	{
		bReadOnly = bInReadOnly;

		UE::LiveLink::SLiveLinkListView<SListView<FLiveLinkSourceUIEntryPtr>, FLiveLinkSourceUIEntryPtr>::Construct(InArgs);
	}
};

class SLiveLinkSubjectsTreeView : public UE::LiveLink::SLiveLinkListView<STreeView<FLiveLinkSubjectUIEntryPtr>, FLiveLinkSubjectUIEntryPtr>
{
public:
	void Construct(const FArguments& InArgs, TAttribute<bool> bInReadOnly)
	{
		bReadOnly = bInReadOnly;

		UE::LiveLink::SLiveLinkListView<STreeView<FLiveLinkSubjectUIEntryPtr>, FLiveLinkSubjectUIEntryPtr>::Construct(InArgs);
	}
};

class LIVELINKEDITOR_API FLiveLinkSubjectsView : public TSharedFromThis<FLiveLinkSubjectsView>
{
public:
	DECLARE_DELEGATE_TwoParams(FOnSubjectSelectionChanged, FLiveLinkSubjectUIEntryPtr, ESelectInfo::Type);

	FLiveLinkSubjectsView(FOnSubjectSelectionChanged InOnSubjectSelectionChanged, const TSharedPtr<FUICommandList>& InCommandList, TAttribute<bool> bInReadOnly);

	// Helper functions for building the subject tree UI
	TSharedRef<ITableRow> MakeTreeRowWidget(FLiveLinkSubjectUIEntryPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	// Get a subjectsview's children
	void GetChildrenForInfo(FLiveLinkSubjectUIEntryPtr InInfo, TArray< FLiveLinkSubjectUIEntryPtr >& OutChildren);
	// Handle subject selection changed
	void OnSubjectSelectionChanged(FLiveLinkSubjectUIEntryPtr SubjectEntry, ESelectInfo::Type SelectInfo);
	// Handler for the subject tree context menu opening
	TSharedPtr<SWidget> OnOpenVirtualSubjectContextMenu(TSharedPtr<FUICommandList> InCommandList);
	// Return whether a subject can be removed
	bool CanRemoveSubject() const;
	// Refresh the list of subjects using the livelink client.
	void RefreshSubjects();

private:
	// Create the subjects tree view
	void CreateSubjectsTreeView(const TSharedPtr<FUICommandList>& InCommandList);

public:
	// Subject tree widget
	TSharedPtr<class SLiveLinkSubjectsTreeView> SubjectsTreeView;
	// Subject tree items
	TArray<FLiveLinkSubjectUIEntryPtr> SubjectData;
	// Subject Selection Changed delegate
	FOnSubjectSelectionChanged SubjectSelectionChangedDelegate;
	// Returns whether the panel is in read-only mode.
	TAttribute<bool> bReadOnly;
};

class FLiveLinkSourcesView : public TSharedFromThis<FLiveLinkSourcesView>
{
public:
	DECLARE_DELEGATE_TwoParams(FOnSourceSelectionChanged, FLiveLinkSourceUIEntryPtr, ESelectInfo::Type);

	FLiveLinkSourcesView(FLiveLinkClient* InLiveLinkClient, TSharedPtr<FUICommandList> InCommandList, TAttribute<bool> bInReadOnly, FOnSourceSelectionChanged InOnSourceSelectionChanged);

	// Gather information about all sources and update the list view 
	void RefreshSourceData(bool bRefreshUI);
	// Handler that creates a widget row for a given ui entry
	TSharedRef<ITableRow> MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	// Handles constructing a context menu for the sources
	TSharedPtr<SWidget> OnSourceConstructContextMenu(TSharedPtr<FUICommandList> InCommandList);
	// Return whether a source can be removed
	bool CanRemoveSource();
	// Removes a livelink source from the livelink client
	void HandleRemoveSource();
	// Callback when property changes on source settings
	void OnPropertyChanged(const FPropertyChangedEvent& InEvent);
	// Handle selection change, triggering the OnSourceSelectionChangedDelegate delegate.
	void OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const;
private:
	// Create the sources list view
	void CreateSourcesListView(const TSharedPtr<FUICommandList>& InCommandList);
public:
	// Holds the data that will be displayed in the list view
	TArray<FLiveLinkSourceUIEntryPtr> SourceData;
	// Holds the sources list view widget
	TSharedPtr<SLiveLinkSourceListView> SourcesListView;
	// LiveLink Client
	FLiveLinkClient* Client;
	// Source selection changed delegate
	FOnSourceSelectionChanged OnSourceSelectionChangedDelegate;
	// Returns whether the panel is in read-only mode.
	TAttribute<bool> bReadOnly;
};
