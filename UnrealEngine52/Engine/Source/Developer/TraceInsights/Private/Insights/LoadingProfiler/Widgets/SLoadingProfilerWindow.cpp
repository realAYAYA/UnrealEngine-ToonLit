// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLoadingProfilerWindow.h"

#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "SlateOptMacros.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "Widgets/Docking/SDockTab.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/LoadingProfiler/LoadingProfilerManager.h"
#include "Insights/LoadingProfiler/Widgets/SLoadingProfilerToolbar.h"
#include "Insights/Table/Widgets/SUntypedTableTreeView.h"
#include "Insights/Widgets/STimingView.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "SLoadingProfilerWindow"

////////////////////////////////////////////////////////////////////////////////////////////////////

const FName FLoadingProfilerTabs::TimingViewID(TEXT("TimingView"));
const FName FLoadingProfilerTabs::EventAggregationTreeViewID(TEXT("EventAggregation"));
const FName FLoadingProfilerTabs::ObjectTypeAggregationTreeViewID(TEXT("ObjectTypeAggregation"));
const FName FLoadingProfilerTabs::PackageDetailsTreeViewID(TEXT("PackageDetails"));
const FName FLoadingProfilerTabs::ExportDetailsTreeViewID(TEXT("ExportDetails"));
const FName FLoadingProfilerTabs::RequestsTreeViewID(TEXT("Requests"));

////////////////////////////////////////////////////////////////////////////////////////////////////

SLoadingProfilerWindow::SLoadingProfilerWindow()
	: SMajorTabWindow(FInsightsManagerTabs::LoadingProfilerTabId)
	, SelectionStartTime(0.0f)
	, SelectionEndTime(0.0f)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////

SLoadingProfilerWindow::~SLoadingProfilerWindow()
{
	CloseAllOpenTabs();

	check(RequestsTreeView == nullptr);
	check(ExportDetailsTreeView == nullptr);
	check(PackageDetailsTreeView == nullptr);
	check(ObjectTypeAggregationTreeView == nullptr);
	check(EventAggregationTreeView == nullptr);
	check(TimingView == nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

const TCHAR* SLoadingProfilerWindow::GetAnalyticsEventName() const
{
	return TEXT("Insights.Usage.LoadingProfiler");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::Reset()
{
	if (TimingView)
	{
		TimingView->Reset();
	}

	if (EventAggregationTreeView)
	{
		EventAggregationTreeView->Reset();
	}

	if (ObjectTypeAggregationTreeView)
	{
		ObjectTypeAggregationTreeView->Reset();
	}

	if (PackageDetailsTreeView)
	{
		PackageDetailsTreeView->Reset();
	}

	if (ExportDetailsTreeView)
	{
		ExportDetailsTreeView->Reset();
	}

	if (RequestsTreeView)
	{
		RequestsTreeView->Reset();
	}

	UpdateTableTreeViews();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdateTableTreeViews()
{
	UpdateEventAggregationTreeView();
	UpdateObjectTypeAggregationTreeView();
	UpdatePackageDetailsTreeView();
	UpdateExportDetailsTreeView();
	UpdateRequestsTreeView();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdateEventAggregationTreeView()
{
	if (EventAggregationTreeView)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			TraceServices::ITable<TraceServices::FLoadTimeProfilerAggregatedStats>* EventAggregationTable = LoadTimeProfilerProvider.CreateEventAggregation(SelectionStartTime, SelectionEndTime);
			EventAggregationTreeView->UpdateSourceTable(MakeShareable(EventAggregationTable));
		}
		else
		{
			EventAggregationTreeView->UpdateSourceTable(nullptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdateObjectTypeAggregationTreeView()
{
	if (ObjectTypeAggregationTreeView)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			TraceServices::ITable<TraceServices::FLoadTimeProfilerAggregatedStats>* ObjectTypeAggregationTable = LoadTimeProfilerProvider.CreateObjectTypeAggregation(SelectionStartTime, SelectionEndTime);
			ObjectTypeAggregationTreeView->UpdateSourceTable(MakeShareable(ObjectTypeAggregationTable));
		}
		else
		{
			ObjectTypeAggregationTreeView->UpdateSourceTable(nullptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdatePackageDetailsTreeView()
{
	if (PackageDetailsTreeView)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			TraceServices::ITable<TraceServices::FPackagesTableRow>* PackageDetailsTable = LoadTimeProfilerProvider.CreatePackageDetailsTable(SelectionStartTime, SelectionEndTime);
			PackageDetailsTreeView->UpdateSourceTable(MakeShareable(PackageDetailsTable));
		}
		else
		{
			PackageDetailsTreeView->UpdateSourceTable(nullptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdateExportDetailsTreeView()
{
	if (ExportDetailsTreeView)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			TraceServices::ITable<TraceServices::FExportsTableRow>* ExportDetailsTable = LoadTimeProfilerProvider.CreateExportDetailsTable(SelectionStartTime, SelectionEndTime);
			ExportDetailsTreeView->UpdateSourceTable(MakeShareable(ExportDetailsTable));
		}
		else
		{
			ExportDetailsTreeView->UpdateSourceTable(nullptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::UpdateRequestsTreeView()
{
	if (RequestsTreeView)
	{
		TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
		if (Session.IsValid() && TraceServices::ReadLoadTimeProfilerProvider(*Session.Get()))
		{
			TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());
			const TraceServices::ILoadTimeProfilerProvider& LoadTimeProfilerProvider = *TraceServices::ReadLoadTimeProfilerProvider(*Session.Get());

			TraceServices::ITable<TraceServices::FRequestsTableRow>* RequestsTable = LoadTimeProfilerProvider.CreateRequestsTable(SelectionStartTime, SelectionEndTime);
			RequestsTreeView->UpdateSourceTable(MakeShareable(RequestsTable));
		}
		else
		{
			RequestsTreeView->UpdateSourceTable(nullptr);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_TimingView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetTimingViewVisible(true);

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(TimingView, STimingView, FInsightsManagerTabs::LoadingProfilerTabId)
		];

	TimingView->Reset(true);
	TimingView->OnSelectionChanged().AddSP(this, &SLoadingProfilerWindow::OnTimeSelectionChanged);
	TimingView->SelectTimeInterval(SelectionStartTime, SelectionEndTime - SelectionStartTime);

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnTimingViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnTimingViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetTimingViewVisible(false);
	if (TimingView)
	{
		TimingView->OnSelectionChanged().RemoveAll(this);
		TimingView = nullptr;
	}

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_EventAggregationTreeView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetEventAggregationTreeViewVisible(true);

	TSharedRef<Insights::FUntypedTable> Table = MakeShared<Insights::FUntypedTable>();
	Table->SetDisplayName(LOCTEXT("EventAggregation_TableName", "Event Aggregation"));

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(EventAggregationTreeView, Insights::SUntypedTableTreeView, Table)
		];

	EventAggregationTreeView->SetLogListingName(FLoadingProfilerManager::Get()->GetLogListingName());
	UpdateEventAggregationTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnEventAggregationTreeViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnEventAggregationTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetEventAggregationTreeViewVisible(false);
	EventAggregationTreeView = nullptr;

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_ObjectTypeAggregationTreeView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetObjectTypeAggregationTreeViewVisible(true);

	TSharedRef<Insights::FUntypedTable> Table = MakeShared<Insights::FUntypedTable>();
	Table->SetDisplayName(LOCTEXT("ObjectTypeAggregation_TableName", "Object Type Aggregation"));

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(ObjectTypeAggregationTreeView, Insights::SUntypedTableTreeView, Table)
		];

	ObjectTypeAggregationTreeView->SetLogListingName(FLoadingProfilerManager::Get()->GetLogListingName());
	UpdateObjectTypeAggregationTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnObjectTypeAggregationTreeViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnObjectTypeAggregationTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetObjectTypeAggregationTreeViewVisible(false);
	ObjectTypeAggregationTreeView = nullptr;

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_PackageDetailsTreeView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetPackageDetailsTreeViewVisible(true);

	TSharedRef<Insights::FUntypedTable> Table = MakeShared<Insights::FUntypedTable>();
	Table->SetDisplayName(LOCTEXT("PackageDetails_TableName", "Package Details"));

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(PackageDetailsTreeView, Insights::SUntypedTableTreeView, Table)
		];

	PackageDetailsTreeView->SetLogListingName(FLoadingProfilerManager::Get()->GetLogListingName());
	UpdatePackageDetailsTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnPackageDetailsTreeViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnPackageDetailsTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetPackageDetailsTreeViewVisible(false);
	PackageDetailsTreeView = nullptr;

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_ExportDetailsTreeView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetExportDetailsTreeViewVisible(true);

	TSharedRef<Insights::FUntypedTable> Table = MakeShared<Insights::FUntypedTable>();
	Table->SetDisplayName(LOCTEXT("ExportDetails_TableName", "Export Details"));

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(ExportDetailsTreeView, Insights::SUntypedTableTreeView, Table)
		];

	ExportDetailsTreeView->SetLogListingName(FLoadingProfilerManager::Get()->GetLogListingName());
	UpdateExportDetailsTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnExportDetailsTreeViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnExportDetailsTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetExportDetailsTreeViewVisible(false);
	ExportDetailsTreeView = nullptr;

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<SDockTab> SLoadingProfilerWindow::SpawnTab_RequestsTreeView(const FSpawnTabArgs& Args)
{
	FLoadingProfilerManager::Get()->SetRequestsTreeViewVisible(true);

	TSharedRef<Insights::FUntypedTable> Table = MakeShared<Insights::FUntypedTable>();
	Table->SetDisplayName(LOCTEXT("Requests_TableName", "Requests"));

	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.ShouldAutosize(false)
		.TabRole(ETabRole::PanelTab)
		[
			SAssignNew(RequestsTreeView, Insights::SUntypedTableTreeView, Table)
		];

	RequestsTreeView->SetLogListingName(FLoadingProfilerManager::Get()->GetLogListingName());
	UpdateRequestsTreeView();

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &SLoadingProfilerWindow::OnRequestsTreeViewTabClosed));
	AddOpenTab(DockTab);

	return DockTab;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnRequestsTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed)
{
	FLoadingProfilerManager::Get()->SetRequestsTreeViewVisible(false);
	RequestsTreeView = nullptr;

	RemoveOpenTab(TabBeingClosed);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& ConstructUnderMajorTab, const TSharedPtr<SWindow>& ConstructUnderWindow)
{
	TSharedPtr<FLoadingProfilerManager> LoadingProfilerManager = FLoadingProfilerManager::Get();
	ensure(LoadingProfilerManager.IsValid());

	SetCommandList(LoadingProfilerManager->GetCommandList());

	SMajorTabWindow::FArguments Args;
	SMajorTabWindow::Construct(Args, ConstructUnderMajorTab, ConstructUnderWindow);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<FWorkspaceItem> SLoadingProfilerWindow::CreateWorkspaceMenuGroup()
{
	return GetTabManager()->AddLocalWorkspaceMenuCategory(LOCTEXT("LoadingProfilerMenuGroupName", "Asset Loading Insights"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::RegisterTabSpawners()
{
	check(GetTabManager().IsValid());
	FTabManager* TabManagerPtr = GetTabManager().Get();
	check(GetWorkspaceMenuGroup().IsValid());
	const TSharedRef<FWorkspaceItem> Group = GetWorkspaceMenuGroup().ToSharedRef();

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::TimingViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_TimingView))
		.SetDisplayName(LOCTEXT("TimingViewTabTitle", "Timing View"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TimingView"))
		.SetGroup(Group);

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::EventAggregationTreeViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_EventAggregationTreeView))
		.SetDisplayName(LOCTEXT("EventAggregationTreeViewTabTitle", "Event Aggregation"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TableTreeView"))
		.SetGroup(Group);

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::ObjectTypeAggregationTreeViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_ObjectTypeAggregationTreeView))
		.SetDisplayName(LOCTEXT("ObjectTypeAggregationTreeViewTabTitle", "Object Type Aggregation"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TableTreeView"))
		.SetGroup(Group);

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::PackageDetailsTreeViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_PackageDetailsTreeView))
		.SetDisplayName(LOCTEXT("PackageDetailsTreeViewTabTitle", "Package Details"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TableTreeView"))
		.SetGroup(Group);

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::ExportDetailsTreeViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_ExportDetailsTreeView))
		.SetDisplayName(LOCTEXT("ExportDetailsTreeViewTabTitle", "Export Details"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TableTreeView"))
		.SetGroup(Group);

	TabManagerPtr->RegisterTabSpawner(FLoadingProfilerTabs::RequestsTreeViewID, FOnSpawnTab::CreateRaw(this, &SLoadingProfilerWindow::SpawnTab_RequestsTreeView))
		.SetDisplayName(LOCTEXT("RequestsTreeViewTabTitle", "Requests"))
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.TableTreeView"))
		.SetGroup(Group);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
TSharedRef<FTabManager::FLayout> SLoadingProfilerWindow::CreateDefaultTabLayout() const
{
	return FTabManager::NewLayout("InsightsLoadingProfilerLayout_v1.2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.5f)
				->SetHideTabWell(true)
				->AddTab(FLoadingProfilerTabs::TimingViewID, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.35f)
				->AddTab(FLoadingProfilerTabs::EventAggregationTreeViewID, ETabState::OpenedTab)
				->AddTab(FLoadingProfilerTabs::ObjectTypeAggregationTreeViewID, ETabState::OpenedTab)
				->AddTab(FLoadingProfilerTabs::PackageDetailsTreeViewID, ETabState::OpenedTab)
				->AddTab(FLoadingProfilerTabs::ExportDetailsTreeViewID, ETabState::OpenedTab)
				->AddTab(FLoadingProfilerTabs::RequestsTreeViewID, ETabState::OpenedTab)
				->SetForegroundTab(FLoadingProfilerTabs::PackageDetailsTreeViewID)
			)
		);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> SLoadingProfilerWindow::CreateToolbar(TSharedPtr<FExtender> Extender)
{
	return SNew(SLoadingProfilerToolbar).ToolbarExtender(Extender);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void SLoadingProfilerWindow::OnTimeSelectionChanged(Insights::ETimeChangedFlags InFlags, double InStartTime, double InEndTime)
{
	if (InFlags != Insights::ETimeChangedFlags::Interactive)
	{
		if (InStartTime < InEndTime)
		{
			SelectionStartTime = InStartTime;
			SelectionEndTime = InEndTime;
			UpdateTableTreeViews();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
