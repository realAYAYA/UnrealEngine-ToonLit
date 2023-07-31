// Copyright Epic Games, Inc. All Rights Reserved.

#include "Filters.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Docking/SDockTab.h"

// Insights
#include "Insights/InsightsStyle.h"
#include "Insights/ViewModels/FilterConfigurator.h"
#include "Insights/Widgets/SAdvancedFilter.h"

#define LOCTEXT_NAMESPACE "SFilterService"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FFilter)
INSIGHTS_IMPLEMENT_RTTI(FFilterWithSuggestions)

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<FFilterService> FFilterService::Instance;

FName const FFilterService::FilterConfiguratorTabId(TEXT("FilterConfigurator"));

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterService::FFilterService()
{
	RegisterTabSpawner();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FFilterService::~FFilterService()
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedPtr<SWidget> FFilterService::CreateFilterConfiguratorWidget(TSharedPtr<FFilterConfigurator> FilterConfiguratorViewModel)
{
	SAssignNew(PendingWidget, SAdvancedFilter, FilterConfiguratorViewModel);

	if (FGlobalTabmanager::Get()->HasTabSpawner(FilterConfiguratorTabId))
	{
		FGlobalTabmanager::Get()->TryInvokeTab(FilterConfiguratorTabId);
	}

	return PendingWidget;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> FFilterService::SpawnTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	const TSharedPtr<SWindow>& OwnerWindow = Args.GetOwnerWindow();
	if (OwnerWindow.IsValid())
	{
		const float DPIScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(10.0f, 10.0f);
		OwnerWindow->Resize(FVector2D(600 * DPIScaleFactor, 400 * DPIScaleFactor));
	}

	DockTab->SetContent(PendingWidget.ToSharedRef());
	PendingWidget->SetParentTab(DockTab);

	PendingWidget = nullptr;
	return DockTab;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

void FFilterService::RegisterTabSpawner()
{
	FTabSpawnerEntry& TabSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(FilterConfiguratorTabId,
		FOnSpawnTab::CreateRaw(this, &FFilterService::SpawnTab))
		.SetDisplayName(LOCTEXT("FilterConfiguratorTabTitle", "Filter Configurator"))
		.SetMenuType(ETabSpawnerMenuType::Hidden)
		.SetIcon(FSlateIcon(FInsightsStyle::GetStyleSetName(), "Icons.ClassicFilterConfig"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE