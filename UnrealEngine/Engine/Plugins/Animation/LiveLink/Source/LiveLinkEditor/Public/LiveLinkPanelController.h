// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/IDelegateInstance.h"
#include "Delegates/DelegateCombinations.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"

class IDetailsView;
class FLiveLinkClient;
struct FLiveLinkSourceUIEntry;
struct FLiveLinkSubjectKey;
struct FLiveLinkSubjectUIEntry;
class FLiveLinkSourcesView;
class SLiveLinkSourceListView;
class FLiveLinkSubjectsView;
class FUICommandList;
class SLiveLinkDataView;

namespace ESelectInfo { enum Type : int; }
typedef TSharedPtr<FLiveLinkSourceUIEntry> FLiveLinkSourceUIEntryPtr;
typedef TSharedPtr<FLiveLinkSubjectUIEntry> FLiveLinkSubjectUIEntryPtr;


/** Handles callback connections between the sources, subjects and details views. */
class LIVELINKEDITOR_API FLiveLinkPanelController : public TSharedFromThis<FLiveLinkPanelController>
{
public:
	FLiveLinkPanelController(TAttribute<bool> bInReadOnly = false);
	~FLiveLinkPanelController();

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSubjectSelectionChanged, const FLiveLinkSubjectKey&);
	/** Subject Selection changed callback. */
	FOnSubjectSelectionChanged& OnSubjectSelectionChanged()
	{
		return SubjectSelectionChangedDelegate;
	}

private:
	// Bind live link commands 
	void BindCommands();
	// Handles the source collection changing.
	void OnSourcesChangedHandler();
	// Handles the subject collection changing.
	void OnSubjectsChangedHandler();
	// Remove all sources command handlers
	bool HasSource() const;
	// Handles the remove source command.
	void HandleRemoveSource();
	// Handles the remove all sources command.
	void HandleRemoveAllSources();
	// Returns whether a source could be removed.
	bool CanRemoveSource() const;
	// Returns whether a subject can be removed.
	bool CanRemoveSubject() const;
	// Handles the remove subject command.
	void HandleRemoveSubject();
	// Recreates the source list data behind the list view.
	void RebuildSourceList();
	// Recreates the subject list data behind the tree view.
	void RebuildSubjectList();
	// Handles source selection changing.
	void OnSourceSelectionChangedHandler(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const;
	// Hadnles subject selection changing.
	void OnSubjectSelectionChangedHandler(FLiveLinkSubjectUIEntryPtr SubjectEntry, ESelectInfo::Type SelectInfo);

public:
	// Sources view
	TSharedPtr<FLiveLinkSourcesView> SourcesView;
	// Subjects view
	TSharedPtr<FLiveLinkSubjectsView> SubjectsView;
	// Reference to connection settings struct details panel
	TSharedPtr<IDetailsView> SourcesDetailsView;
	// Reference to the data value struct details panel
	TSharedPtr<SLiveLinkDataView> SubjectsDetailsView;
	// LiveLink Client
	FLiveLinkClient* Client;
	// Handle to delegate when client sources list has changed
	FDelegateHandle OnSourcesChangedHandle;
	// Handle to delegate when a client subjects list has changed
	FDelegateHandle OnSubjectsChangedHandle;
	// Guard from reentrant selection
	mutable bool bSelectionChangedGuard = false;
	// Command list
	TSharedPtr<FUICommandList> CommandList;
	// Delegate called when the subject selection changes
	FOnSubjectSelectionChanged SubjectSelectionChangedDelegate;
};
