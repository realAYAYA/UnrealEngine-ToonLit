// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageEditorModule.h"

#include "SUSDSaveDialog.h"
#include "SUSDStage.h"
#include "SUSDStageEditorStyle.h"
#include "USDLayerUtils.h"
#include "USDLog.h"
#include "USDMemory.h"
#include "USDProjectSettings.h"
#include "USDStageActor.h"

#include "Editor/TransBuffer.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineUtils.h"
#include "Framework/Docking/TabManager.h"
#include "IAssetTools.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "UnrealEdGlobals.h"
#include "UObject/ObjectSaveContext.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UsdStageEditorModule"

namespace UE::UsdStageEditorModule::Private
{
	void SaveStageActorLayersForWorld( UWorld* World, bool bForClosing, AUsdStageActor* TargetStageActor = nullptr )
	{
#if USE_USD_SDK
		if ( !World )
		{
			return;
		}

		UUsdProjectSettings* Settings = GetMutableDefault<UUsdProjectSettings>();
		if ( !Settings )
		{
			return;
		}

		// Reentrant guard here because if we ever save an anonymous layer we'll update the stage actors that use it
		// to point to the new (saved) layer, which will internally close the anonymous stage and get us back in here
		static bool bIsReentrant = false;
		if ( bIsReentrant )
		{
			return;
		}
		TGuardValue<bool> ReentrantGuard( bIsReentrant, true );

		bool bPrompt = false;
		switch ( bForClosing ? Settings->ShowSaveLayersDialogWhenClosing : Settings->ShowSaveLayersDialogWhenSaving )
		{
			case EUsdSaveDialogBehavior::NeverSave:
			{
				// Don't even do anything if we're not going to save anyway
				return;
				break;
			}
			case EUsdSaveDialogBehavior::AlwaysSave:
			{
				break;
			}
			default:
			case EUsdSaveDialogBehavior::ShowPrompt:
			{
				bPrompt = true;
				break;
			}
		}

		TArray<AUsdStageActor*> StageActorsToVisit;
		if ( TargetStageActor )
		{
			if ( TargetStageActor->GetWorld() == World )
			{
				StageActorsToVisit.Add( TargetStageActor );
			}
		}
		else
		{
			for ( TActorIterator<AUsdStageActor> It{ World }; It; ++It )
			{
				StageActorsToVisit.Add( *It );
			}
		}

		// For now lets only care about stages opened on stage actors. The user could have additional stages,
		// like opened via Python or custom C++ plugins, but lets ignore those
		TMap<FString, FUsdSaveDialogRowData> RowsByIdentifier;
		for ( AUsdStageActor* StageActor : StageActorsToVisit )
		{
			if ( !StageActor )
			{
				continue;
			}

			UE::FUsdStage UsdStage = static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage();
			if ( !UsdStage )
			{
				continue;
			}

			TArray<UE::FSdfLayer> UsedLayers = UsdStage.GetUsedLayers();
			RowsByIdentifier.Reserve( RowsByIdentifier.Num() + UsedLayers.Num() );

			for ( const UE::FSdfLayer& UsedLayer : UsedLayers )
			{
				// This comment is written to the layer when we're in the process of saving a memory-only
				// stage, and indicates that this layer is already saved (even though it will show as dirty and
				// anonymous)
				if ( UsedLayer.IsDirty() && UsedLayer.GetComment() != UnrealIdentifiers::LayerSavedComment )
				{
					FUsdSaveDialogRowData& RowData = RowsByIdentifier.FindOrAdd( UsedLayer.GetIdentifier() );
					RowData.Layer = UsedLayer;
					RowData.ConsumerStages.AddUnique( UsdStage );
					RowData.ConsumerActors.Add( StageActor );
				}
			}
		}

		if ( RowsByIdentifier.Num() == 0 )
		{
			return;
		}

		TArray< FUsdSaveDialogRowData > Rows;
		RowsByIdentifier.GenerateValueArray( Rows );

		if ( bPrompt )
		{
			Rows.Sort( []( const FUsdSaveDialogRowData& Left, const FUsdSaveDialogRowData& Right )
			{
				return ( Left.Layer && Right.Layer )
					? Left.Layer.GetIdentifier() < Right.Layer.GetIdentifier()
					// This shouldn't ever happen but just do something consistent here instead anyway
					: Left.ConsumerStages.Num() < Right.ConsumerStages.Num();
			});

			const FText WindowTitle = LOCTEXT( "SaveDialogTitle", "Save USD Layers" );
			const FText DescriptionText = bForClosing
				? LOCTEXT( "CloseDialogDescTextText", "Before closing these USD Stages, do you want to save these USD layers to disk?" )
				: LOCTEXT( "SaveDialogDescTextText", "Since you're saving the Level, do you want to save these USD layers to disk?" );

			bool bShouldSave = true;
			bool bShouldPromptAgain = true;
			Rows = SUsdSaveDialog::ShowDialog(
				Rows,
				WindowTitle,
				DescriptionText,
				&bShouldSave,
				&bShouldPromptAgain
			);

			EUsdSaveDialogBehavior* bSetting = bForClosing
				? &Settings->ShowSaveLayersDialogWhenClosing
				: &Settings->ShowSaveLayersDialogWhenSaving;

			*bSetting = bShouldPromptAgain
				? EUsdSaveDialogBehavior::ShowPrompt
				: bShouldSave
					? EUsdSaveDialogBehavior::AlwaysSave
					: EUsdSaveDialogBehavior::NeverSave;

			Settings->SaveConfig();
		}

		for ( const FUsdSaveDialogRowData& ReturnedRow : Rows )
		{
			UE::FSdfLayer PinnedLayer = ReturnedRow.Layer;
			if ( ReturnedRow.bSaveLayer && PinnedLayer )
			{
				bool bSaved = false;

				if ( PinnedLayer.IsAnonymous() )
				{
					// For now we only allow anonymous stages, and not individual layers, so we don't have to
					// patch up anything
					TOptional< FString > UsdFilePath = UsdUtils::BrowseUsdFile( UsdUtils::EBrowseFileMode::Save );
					if ( UsdFilePath )
					{
						bSaved = PinnedLayer.Export( *UsdFilePath.GetValue() );

						// If any stage actors were pointing at the in-memory versions of these stages, update them
						// to point to the saved versions
						if ( bSaved && !bForClosing )
						{
							// Even though we're potentially going to load actors and assets here, we don't need to
							// use a scoped transaction as the Save All command already clears the transaction buffer
							// anyway, and we won't get in here when closing.

							FString ExpectedIdentifier = UnrealIdentifiers::IdentifierPrefix + PinnedLayer.GetIdentifier();
							for ( AUsdStageActor* StageActor : ReturnedRow.ConsumerActors )
							{
								if ( StageActor->RootLayer.FilePath == ExpectedIdentifier
									&& static_cast<const AUsdStageActor*>(StageActor)->GetUsdStage() )
								{
									StageActor->SetRootLayer( *UsdFilePath.GetValue() );
								}
							}
						}
					}
				}
				else
				{
					const bool bForce = true;
					bSaved = PinnedLayer.Save( bForce );
				}

				if ( !bSaved )
				{
					UE_LOG( LogUsd, Warning, TEXT( "Failed to save layer '%s'" ), *PinnedLayer.GetIdentifier() );
				}
			}
		}
#endif // USE_USD_SDK
	}
}

class FUsdStageEditorModule : public IUsdStageEditorModule
{
public:
#if USE_USD_SDK
	static TSharedRef< SDockTab > SpawnUsdStageTab( const FSpawnTabArgs& SpawnTabArgs )
	{
		LLM_SCOPE_BYTAG(Usd);

		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label( LOCTEXT( "USDStage", "USD Stage" ) )
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SNew( SUsdStage )
				]
			];
	}

	virtual void StartupModule() override
	{
		LLM_SCOPE_BYTAG(Usd);

		FUsdStageEditorStyle::Initialize();

		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked< FLevelEditorModule >( "LevelEditor" );
		LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddLambda(
			[]()
			{
				FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked< FLevelEditorModule >( "LevelEditor" );
				TSharedPtr< FTabManager > LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

				const FSlateIcon LayersIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.USDStage");

				LevelEditorTabManager->RegisterTabSpawner( TEXT("USDStage"), FOnSpawnTab::CreateStatic( &FUsdStageEditorModule::SpawnUsdStageTab ) )
					.SetDisplayName( LOCTEXT( "USDStage", "USD Stage" ) )
					.SetTooltipText( LOCTEXT( "USDStageTab", "Open USD Stage tab" ) )
					.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorVirtualProductionCategory())
					.SetIcon( LayersIcon );
			});

		// Prompt to save modified USD layers when closing the editor
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		EditorCanCloseDelegate = MainFrame.RegisterCanCloseEditor( IMainFrameModule::FMainFrameCanCloseEditor::CreateLambda( []() -> bool
		{
			if ( GEditor && GEngine )
			{
				UWorld* EditorWorld = nullptr;
				for ( const FWorldContext& Context : GEngine->GetWorldContexts() )
				{
					if ( Context.WorldType == EWorldType::Editor )
					{
						EditorWorld = Context.World();
					}
				}

				const bool bForClosing = true;
				UE::UsdStageEditorModule::Private::SaveStageActorLayersForWorld( EditorWorld, bForClosing );
			}

			// We won't actually ever block the save
			return true;
		}));

		// Prompt to save modified USD Layers when closing stageactor stages
		StageActorLoadedHandle = AUsdStageActor::OnActorLoaded.AddLambda([this](AUsdStageActor* StageActor)
		{
			if ( !StageActor )
			{
				return;
			}

			// We never want to prompt when undoing or redoing.
			// We have to subscribe to this here as the UTransBuffer doesn't exist by the time the module is
			// initializing
			if ( UTransBuffer* TransBuffer = GUnrealEd ? Cast<UTransBuffer>( GUnrealEd->Trans ) : nullptr )
			{
				if ( !OnTransactionStateChangedHandle.IsValid() )
				{
					OnTransactionStateChangedHandle = TransBuffer->OnTransactionStateChanged().AddLambda(
						[this]( const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState )
						{
							if ( InTransactionState == ETransactionStateEventType::UndoRedoStarted )
							{
								bUndoRedoing = true;
							}
							else if ( InTransactionState == ETransactionStateEventType::UndoRedoFinalized )
							{
								bUndoRedoing = false;
							}
						});
				}
			}

			StageActor->OnPreStageChanged.AddLambda( [this, StageActor]()
			{
				if ( !bUndoRedoing && StageActor && static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage() )
				{
					const bool bForClosing = true;
					UE::UsdStageEditorModule::Private::SaveStageActorLayersForWorld( StageActor->GetWorld(), bForClosing, StageActor );
				}
			});
		});

		// Prompt to save modified USD layers when saving the world
		PreSaveWorldEditorDelegateHandle = FEditorDelegates::PreSaveWorldWithContext.AddLambda( []( UWorld* World, FObjectPreSaveContext InContext )
		{
			// Detect if we should actually do anything (check for autosaves, cooking, etc.)
			if ( InContext.GetSaveFlags() & ESaveFlags::SAVE_FromAutosave || InContext.IsProceduralSave() )
			{
				return;
			}

			const bool bForClosing = false;
			UE::UsdStageEditorModule::Private::SaveStageActorLayersForWorld( World, bForClosing );
		});
	}

	virtual void ShutdownModule() override
	{
		FEditorDelegates::PreSaveWorldWithContext.Remove( PreSaveWorldEditorDelegateHandle );

		AUsdStageActor::OnActorLoaded.Remove( StageActorLoadedHandle );

		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		MainFrame.UnregisterCanCloseEditor( EditorCanCloseDelegate );

		if ( LevelEditorTabManagerChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded( "LevelEditor" ) )
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( "LevelEditor" );
			LevelEditorModule.OnTabManagerChanged().Remove( LevelEditorTabManagerChangedHandle );
		}

		FUsdStageEditorStyle::Shutdown();
	}

private:
	bool bUndoRedoing = false;

	FDelegateHandle LevelEditorTabManagerChangedHandle;
	FDelegateHandle PreSaveWorldEditorDelegateHandle;
	FDelegateHandle EditorCanCloseDelegate;
	FDelegateHandle StageActorLoadedHandle;
	FDelegateHandle OnTransactionStateChangedHandle;
#endif // #if USE_USD_SDK
};

IMPLEMENT_MODULE_USD( FUsdStageEditorModule, USDStageEditor );

#undef LOCTEXT_NAMESPACE
