// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveConcertSessionTab.h"

#include "ConcertFrontendStyle.h"
#include "IConcertSession.h"
#include "LiveServerSessionHistoryController.h"
#include "PackageViewer/ConcertSessionPackageViewerController.h"
#include "SConcertLiveSessionTabView.h"

#include "Widgets/Docking/SDockTab.h"

FLiveConcertSessionTab::FLiveConcertSessionTab(TSharedRef<IConcertServerSession> InspectedSession, TSharedRef<IConcertSyncServer> SyncServer, TAttribute<TSharedRef<SWindow>> ConstructUnderWindow, FShowConnectedClients OnConnectedClientsClicked)
	: FConcertSessionTabBase(InspectedSession->GetSessionInfo().SessionId, SyncServer)
	, InspectedSession(MoveTemp(InspectedSession))
	, ConstructUnderWindow(MoveTemp(ConstructUnderWindow))
	, OnConnectedClientsClicked(MoveTemp(OnConnectedClientsClicked))
	, SessionHistoryController(MakeShared<FLiveServerSessionHistoryController>(InspectedSession, SyncServer))
	, PackageViewerController(MakeShared<FConcertSessionPackageViewerController>(InspectedSession, SyncServer))
{}

void FLiveConcertSessionTab::CreateDockContent(const TSharedRef<SDockTab>& InDockTab)
{
	const SConcertLiveSessionTabView::FRequiredWidgets WidgetArgs
	{
		InDockTab,
		ConstructUnderWindow.Get(),
		SessionHistoryController->GetSessionHistory(),
		PackageViewerController->GetPackageViewer()
	};
	InDockTab->SetContent(
		SNew(SConcertLiveSessionTabView, WidgetArgs, *GetTabId())
		.OnConnectedClientsClicked_Lambda([this]()
		{
			OnConnectedClientsClicked.ExecuteIfBound(InspectedSession);
		}));
}

const FSlateBrush* FLiveConcertSessionTab::GetTabIconBrush() const
{
	return FConcertFrontendStyle::Get()->GetBrush("Concert.ActiveSession.Icon");
}

void FLiveConcertSessionTab::OnOpenTab()
{
	SessionHistoryController->ReloadActivities();
	PackageViewerController->ReloadActivities();
}

