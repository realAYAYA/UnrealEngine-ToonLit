// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClientSessionHistoryController.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "ConcertMessages.h"

class IConcertClientSession;
class IConcertSyncClient;
struct FConcertSessionClientInfo;
class SDockTab;
class SExpandableArea;

/**
 * Displays the multi-users active session clients and activity, enables the client
 * to leave the session, open the same level as another client or teleport to another
 * client presence.
 */
class SActiveSession : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SActiveSession) { }
	SLATE_END_ARGS();

	/**
	* Constructs the active session tab.
	*
	* @param InArgs The Slate argument list.
	*/
	void Construct(const FArguments& InArgs, TSharedPtr<IConcertSyncClient> InConcertSyncClient);

private:

	/** Generate a new client row */
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FConcertSessionClientInfo> InClientInfo, const TSharedRef<STableViewBase>& OwnerTable) const;

	/** Handle a session startup */
	void HandleSessionStartup(TSharedRef<IConcertClientSession> InClientSession);

	/** Handle a session shutdown */
	void HandleSessionShutdown(TSharedRef<IConcertClientSession> InClientSession);

	/** Handle a session client change */
	void HandleSessionClientChanged(IConcertClientSession&, EConcertClientStatus ClientStatus, const FConcertSessionClientInfo& ClientInfo);

	/** Polls the local client info and detect if it changed in order to update its representation in real time.*/
	EActiveTimerReturnType HandleLocalClientInfoChangePollingTimer(double InCurrentTime, float InDeltaTime);

	/** Update the list of clients while keeping the alphabetical sorting */
	void UpdateSessionClientListView(const FConcertSessionClientInfo* Client = nullptr, EConcertClientStatus Status = EConcertClientStatus::Updated);

	/** Set the selected client in the clients list view*/
	void SetSelectedClient(const FGuid& InClientEndpointId, ESelectInfo::Type SelectInfo = ESelectInfo::Direct);

	/** Find a client with its endpoint id */
	TSharedPtr<FConcertSessionClientInfo> FindAvailableClient(const FGuid& InClientEndpointId) const;
	
	/** Handling for the status icon and text */
	const FButtonStyle& GetConnectionIconStyle() const;
	FSlateColor GetConnectionIconColor() const;
	FSlateFontInfo GetConnectionIconFontInfo() const;
	FText GetConnectionStatusText() const;

	/** Handling for the suspend, resume and leave session buttons */
	bool IsStatusBarSuspendSessionVisible() const;
	bool IsStatusBarResumeSessionVisible() const;
	bool IsStatusBarLeaveSessionVisible() const;
	FReply OnClickSuspendSession();
	FReply OnClickResumeSession();
	FReply OnClickLeaveSession();

	/** Handles how much space the 'Clients' area uses with respect to its expansion state. */
	SSplitter::ESizeRule GetClientAreaSizeRule() const { return bClientAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
	void OnClientAreaExpansionChanged(bool bExpanded) { bClientAreaExpanded = bExpanded; }

	/** Handles how much space the 'History' area uses with respect to its expansion state. */
	SSplitter::ESizeRule GetHistoryAreaSizeRule() const { return bHistoryAreaExpanded ? SSplitter::ESizeRule::FractionOfParent : SSplitter::ESizeRule::SizeToContent; }
	void OnHistoryAreaExpansionChanged(bool bExpanded) { bHistoryAreaExpanded = bExpanded; }

private:
	
	/** Pointer on the client sync. */
	TWeakPtr<IConcertSyncClient> WeakConcertSyncClient;

	/** Holds a concert client session. */
	TWeakPtr<IConcertClientSession> WeakSessionPtr;

	/** List view for AvailableClients. */
	TSharedPtr<SListView<TSharedPtr<FConcertSessionClientInfo>>> ClientsListView;

	/** List of clients for the current session. */
	TArray<TSharedPtr<FConcertSessionClientInfo>> Clients;

	/** Information about the machine's client. */
	TSharedPtr<FConcertSessionClientInfo> ClientInfo;

	/** Holds a concert activity log. */
	TSharedPtr<FClientSessionHistoryController> SessionHistoryController;

	/** The 'Clients' expandable area. */
	TSharedPtr<SExpandableArea> ClientArea;

	/** The 'History' expandable area. */
	TSharedPtr<SExpandableArea> HistoryArea;

	/** Keeps the status of 'Clients' area expansion. */
	bool bClientAreaExpanded = true;

	/** Keep the status of 'History' area expansion. */
	bool bHistoryAreaExpanded = true;
};
