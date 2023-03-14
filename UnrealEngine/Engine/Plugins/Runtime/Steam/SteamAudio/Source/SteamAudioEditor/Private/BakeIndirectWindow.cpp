//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "BakeIndirectWindow.h"

#include "PhononCommon.h"
#include "PhononSourceComponent.h"
#include "PhononProbeVolume.h"
#include "TickableNotification.h"
#include "IndirectBaker.h"

#include "Framework/Docking/TabManager.h"
#include "DetailLayoutBuilder.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Async/Async.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Slate/Public/Widgets/Notifications/SNotificationList.h"
#include "Runtime/Slate/Public/Framework/Notifications/NotificationManager.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FBakeIndirectWindow
	//==============================================================================================================================================

	static const FName BakeIndirectTabName("BakeIndirectTab");

	FBakeIndirectWindow::FBakeIndirectWindow()
	{
		FGlobalTabmanager::Get()->RegisterTabSpawner(BakeIndirectTabName, FOnSpawnTab::CreateRaw(this, &FBakeIndirectWindow::SpawnTab))
			.SetDisplayName(FText::FromString(TEXT("Bake Indirect Sound")));
	}

	FBakeIndirectWindow::~FBakeIndirectWindow()
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner(BakeIndirectTabName);
	}

	TSharedRef<SDockTab> FBakeIndirectWindow::SpawnTab(const FSpawnTabArgs& TabSpawnArgs)
	{
		RefreshBakedSources();

		BakedSourcesListView = SNew(SListView<TSharedPtr<FBakedSource>>)
			.ListItemsSource(&BakedSources)
			.ScrollbarVisibility(EVisibility::Visible)
			.OnGenerateRow(this, &FBakeIndirectWindow::OnGenerateBakedSourceRow)
			.HeaderRow(
				SNew(SHeaderRow)
				+ SHeaderRow::Column("Unique Identifier")
				.DefaultLabel(FText::FromString(TEXT("Unique Identifier")))
				.FillWidth(0.5f)
				+ SHeaderRow::Column("Baked Data Size")
				.DefaultLabel(FText::FromString(TEXT("Baked Data Size")))
				.FillWidth(0.5f)
			)
			.SelectionMode(ESelectionMode::Multi);

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					BakedSourcesListView.ToSharedRef()
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(3)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled(this, &FBakeIndirectWindow::IsBakeEnabled)
					.OnClicked(this, &FBakeIndirectWindow::OnBakeSelected)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SteamAudio", "BakeSelected", "Bake Selected"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			]
		];

		return SpawnedTab;
	}

	/**
	 * Convenience function that computes the cumulative baked data size (in bytes) for a given source ID across the provided probe volumes.
	 */
	static int32 ComputeSourceDataSize(const TArray<AActor*>& PhononProbeVolumes, const FName& SourceUID)
	{
		int32 SourceDataSize = 0;

		for (auto PhononProbeVolumeActor : PhononProbeVolumes)
		{
			auto PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);
			SourceDataSize += PhononProbeVolume->GetDataSizeForSource(SourceUID);
		}

		return SourceDataSize;
	}

	/**
	 * Constructs a table row for the given baked source.
	 */
	TSharedRef<ITableRow> FBakeIndirectWindow::OnGenerateBakedSourceRow(TSharedPtr<FBakedSource> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		auto World = GEditor->GetLevelViewportClients()[0]->GetWorld();

		TArray<AActor*> PhononProbeVolumes;
		UGameplayStatics::GetAllActorsOfClass(World, APhononProbeVolume::StaticClass(), PhononProbeVolumes);

		Item->DataSize = ComputeSourceDataSize(PhononProbeVolumes, Item->Name);

		return SNew(STableRow<TSharedPtr<FString>>, OwnerTable)
			.Padding(4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(FText::FromName(Item->Name))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
					.Text(FText::AsMemory(Item->DataSize))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
	}

	void FBakeIndirectWindow::OnBakedSourceUpdated(FName UniqueIdentifier)
	{
		AsyncTask(ENamedThreads::GameThread, [=]()
		{
			RefreshBakedSources();
			BakedSourcesListView->RequestListRefresh();
		});
	}

	/**
	 * Initiates the bake for selected phonon sources and possibly reverb.
	 */
	FReply FBakeIndirectWindow::OnBakeSelected()
	{
		// Grab the currently selected items from the baked sources listview
		TArray<TSharedPtr<FBakedSource>> SelectedSources;
		BakedSourcesListView->GetSelectedItems(SelectedSources);

		// Collect the phonon source components and determine whether we want to bake reverb
		TArray<UPhononSourceComponent*> Components;
		bool ShouldBakeReverb = false;
		for (const auto& Source : SelectedSources)
		{
			if (Source->Name == "__reverb__")
			{
				ShouldBakeReverb = true;
				continue;
			}

			Components.Add(Source->PhononSourceComponent);
		}

		// Begin the bake
		FBakedSourceUpdated Callback;
		Callback.BindRaw(this, &FBakeIndirectWindow::OnBakedSourceUpdated);
		Bake(Components, ShouldBakeReverb, Callback);

		return FReply::Handled();
	}

	/**
	 * Spawns the window.
	 */
	void FBakeIndirectWindow::Invoke()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(BakeIndirectTabName);
	}

	/**
	 * Populates BakedSources with up-to-date information about all phonon sources and any baked reverb present in the level.
	 */
	void FBakeIndirectWindow::RefreshBakedSources()
	{
		auto World = GEditor->GetLevelViewportClients()[0]->GetWorld();

		// Clear old data
		BakedSources.Empty();

		// Grab all probe volumes
		TArray<AActor*> PhononProbeVolumes;
		UGameplayStatics::GetAllActorsOfClass(World, APhononProbeVolume::StaticClass(), PhononProbeVolumes);

		// Add baked reverb
		BakedSources.Add(MakeShareable(new FBakedSource("__reverb__", ComputeSourceDataSize(PhononProbeVolumes, "__reverb__"), nullptr)));

		// Add all baked phonon sources
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			TArray<UActorComponent*> Components;
			ActorItr->GetComponents(UPhononSourceComponent::StaticClass(), Components);
			for (auto PhononSourceItr : Components)
			{
				if (PhononSourceItr && PhononSourceItr->IsValidLowLevel())
				{
					UPhononSourceComponent* PhononSourceComponent = Cast<UPhononSourceComponent>(PhononSourceItr);

					if (PhononSourceComponent && PhononSourceComponent->IsValidLowLevel())
					{
						auto SourceDataSize = ComputeSourceDataSize(PhononProbeVolumes, PhononSourceComponent->UniqueIdentifier);
						BakedSources.Add(MakeShareable(new FBakedSource(PhononSourceComponent->UniqueIdentifier, SourceDataSize, PhononSourceComponent)));
					}
				}
			}
		}
	}

	bool FBakeIndirectWindow::IsBakeEnabled() const
	{
		return !GIsBaking.Load();
	}

	//==============================================================================================================================================
	// FBakedSource
	//==============================================================================================================================================

	FBakedSource::FBakedSource()
		: Name("")
		, DataSize(0)
		, PhononSourceComponent(nullptr)
	{}

	FBakedSource::FBakedSource(const FName& Name, const uint32 DataSize, UPhononSourceComponent* PhononSourceComponent)
		: Name(Name)
		, DataSize(DataSize)
		, PhononSourceComponent(PhononSourceComponent)
	{}
}
