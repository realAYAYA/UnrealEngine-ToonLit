// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheToolkit.h"
#include "NiagaraEditorModule.h"

// Widgets & ViewModels
#include "Widgets/Docking/SDockTab.h"
#include "NiagaraSimCacheViewModel.h"
#include "SNiagaraSimCacheViewTimeline.h"
#include "SNiagaraSimCacheViewTransportControls.h"

#define LOCTEXT_NAMESPACE "NiagaraSimCacheToolkit"

const FName FNiagaraSimCacheToolkit::NiagaraSimCacheSpreadsheetTabId(TEXT("NiagaraSimCacheEditor_Spreadsheet"));

//TODO Preview

FNiagaraSimCacheToolkit::FNiagaraSimCacheToolkit()
{
		
}

FNiagaraSimCacheToolkit::~FNiagaraSimCacheToolkit()
{

}

void FNiagaraSimCacheToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraSimCacheEditor", "Niagara"));
	
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(NiagaraSimCacheSpreadsheetTabId, FOnSpawnTab::CreateSP(this, &FNiagaraSimCacheToolkit::SpawnTab_SimCacheSpreadsheet))
		.SetDisplayName(LOCTEXT("SpreadsheetTab", "Cache Spreadsheet"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FNiagaraSimCacheToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(NiagaraSimCacheSpreadsheetTabId);
}

void FNiagaraSimCacheToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraSimCache* InSimCache)
{
	SimCache = InSimCache;
	if (SimCache.IsValid())
	{
		SimCacheViewModel = MakeShared<FNiagaraSimCacheViewModel>();
		
		SimCacheViewModel->Initialize(SimCache);
		

		const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_SimCache_Layout_V1")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.1f)
					->AddTab(NiagaraSimCacheSpreadsheetTabId, ETabState::OpenedTab)
				)
			);

		constexpr bool bCreateDefaultStandaloneMenu = true;
		constexpr bool bCreateDefaultToolbar = true;
		FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, SimCache.Get());

		RegenerateMenusAndToolbars();
	}
}

FName FNiagaraSimCacheToolkit::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraSimCacheToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraSimCacheToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}

FLinearColor FNiagaraSimCacheToolkit::GetWorldCentricTabColorScale() const
{
	return FNiagaraEditorModule::WorldCentricTabColorScale;
}

TSharedRef<SDockTab> FNiagaraSimCacheToolkit::SpawnTab_SimCacheSpreadsheet(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == NiagaraSimCacheSpreadsheetTabId);

	SimCacheSpreadsheetView =
		SNew(SNiagaraSimCacheView)
		.SimCacheViewModel(SimCacheViewModel);

	const TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);

	Contents->AddSlot()
		.FillHeight(1.0f)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SimCacheSpreadsheetView->AsShared()
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SNiagaraSimCacheViewTransportControls)
					.WeakViewModel(SimCacheViewModel->AsWeak())
				]
				+SHorizontalBox::Slot()
				.Padding(4.0f)
				[
					SNew(SNiagaraSimCacheViewTimeline)
					.WeakViewModel(SimCacheViewModel->AsWeak())
				]
				
			]
			
		];

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Contents
		];
	
	return SpawnedTab;
}

#undef LOCTEXT_NAMESPACE /** NiagaraSimCacheToolkit **/