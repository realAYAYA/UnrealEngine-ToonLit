// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUSDStage.h"

#include "SUSDLayersTreeView.h"
#include "SUSDPrimInfo.h"
#include "SUSDStageTreeView.h"
#include "UnrealUSDWrapper.h"
#include "USDClassesModule.h"
#include "USDConversionUtils.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDProjectSettings.h"
#include "USDSchemasModule.h"
#include "USDSchemaTranslator.h"
#include "USDStageActor.h"
#include "USDStageEditorSettings.h"
#include "USDStageImportContext.h"
#include "USDStageImporter.h"
#include "USDStageImporterModule.h"
#include "USDStageImportOptions.h"
#include "USDStageModule.h"
#include "USDTypesConversion.h"

#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/UsdStage.h"

#include "ActorTreeItem.h"
#include "Async/Async.h"
#include "DesktopPlatformModule.h"
#include "Dialogs/DlgPickPath.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineAnalytics.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SceneOutlinerModule.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SSplitter.h"

#define LOCTEXT_NAMESPACE "SUsdStage"

#if USE_USD_SDK

namespace SUSDStageConstants
{
	static const FMargin SectionPadding( 1.f, 1.f, 1.f, 1.f );
}

namespace SUSDStageImpl
{
#if PLATFORM_LINUX
	struct FCaseSensitiveStringSetFuncs : BaseKeyFuncs<FString, FString>
	{
		static FORCEINLINE const FString& GetSetKey( const FString& Element )
		{
			return Element;
		}
		static FORCEINLINE bool Matches( const FString& A, const FString& B )
		{
			return A.Equals( B, ESearchCase::CaseSensitive );
		}
		static FORCEINLINE uint32 GetKeyHash( const FString& Key )
		{
			return FCrc::StrCrc32<TCHAR>( *Key );
		}
	};
#endif

	/**
	 * Makes sure that AllLayers includes all the external references of any of its members.
	 * Example: Receives AllLayers [a.usda], that references b.usda and c.usda, while c.usda also references d.usda -> Modifies AllLayers to be [a.usda, b.usda, c.usda, d.usda]
	 */
	void AppendAllExternalReferences( TArray<UE::FSdfLayer>& AllLayers )
	{
		// Consider paths in a case-sensitive way for linux
#if PLATFORM_LINUX
		TSet<FString, FCaseSensitiveStringSetFuncs> LoadedLayers;
#else
		TSet<FString> LoadedLayers;
#endif

		LoadedLayers.Reserve( AllLayers.Num() );
		for ( const UE::FSdfLayer& Layer : AllLayers )
		{
			FString LayerPath = Layer.GetRealPath();
			FPaths::NormalizeFilename( LayerPath );
			LoadedLayers.Add( LayerPath );
		}

		for ( int32 Index = 0; Index < AllLayers.Num(); ++Index )
		{
			UE::FSdfLayer& Layer = AllLayers[ Index ];

			TArray< UE::FSdfLayer > NewLayers;
			for ( const FString& AssetDependency : Layer.GetCompositionAssetDependencies() )
			{
				FString AbsoluteReference = Layer.ComputeAbsolutePath( AssetDependency );
				FPaths::NormalizeFilename( AbsoluteReference );

				if ( !LoadedLayers.Contains( AbsoluteReference ) )
				{
					NewLayers.Add( UE::FSdfLayer::FindOrOpen( *AbsoluteReference ) );
					LoadedLayers.Add( AbsoluteReference );
				}
			}
			AllLayers.Append( NewLayers );
		}
	}

	void SelectGeneratedComponentsAndActors( AUsdStageActor* StageActor, const TArray<FString>& PrimPaths )
	{
		if ( !StageActor )
		{
			return;
		}

		TSet<USceneComponent*> ComponentsToSelect;
		for ( const FString& PrimPath : PrimPaths )
		{
			if ( USceneComponent* GeneratedComponent = StageActor->GetGeneratedComponent( PrimPath ) )
			{
				ComponentsToSelect.Add( GeneratedComponent );
			}
		}

		TSet<AActor*> ActorsToSelect;
		for ( TSet<USceneComponent*>::TIterator It( ComponentsToSelect ); It; ++It )
		{
			if ( AActor* Owner = ( *It )->GetOwner() )
			{
				// We always need the parent actor selected to select a component
				ActorsToSelect.Add( Owner );

				// If we're going to select a root component, select the actor instead. This is useful
				// because if we later press "F" to focus on the prim, having the actor selected will use the entire
				// actor's bounding box and focus on something. If all we have selected is an empty scene component
				// however, the camera won't focus on anything
				if ( *It == Owner->GetRootComponent() )
				{
					It.RemoveCurrent();
				}
			}
		}

		// Don't deselect anything if we're not going to select anything
		if ( ActorsToSelect.Num() == 0 && ComponentsToSelect.Num() == 0 )
		{
			return;
		}

		// Don't use GEditor->SelectNone() as that will affect *every type of selection in the editor*,
		// including even some UI menus, brushes, etc.
		if ( USelection* SelectedActors = GEditor->GetSelectedActors() )
		{
			SelectedActors->DeselectAll( AActor::StaticClass() );
		}
		if ( USelection* SelectedComponents = GEditor->GetSelectedComponents() )
		{
			SelectedComponents->DeselectAll( UActorComponent::StaticClass() );
		}

		const bool bSelected = true;
		const bool bNotifySelectionChanged = true;
		for ( AActor* Actor : ActorsToSelect )
		{
			GEditor->SelectActor( Actor, bSelected, bNotifySelectionChanged );
		}

		for ( USceneComponent* Component : ComponentsToSelect )
		{
			GEditor->SelectComponent( Component, bSelected, bNotifySelectionChanged );
		}
	}
}

void SUsdStage::Construct( const FArguments& InArgs )
{
	OnActorLoadedHandle = AUsdStageActor::OnActorLoaded.AddSP( SharedThis( this ), &SUsdStage::OnStageActorLoaded );

	OnViewportSelectionChangedHandle = USelection::SelectionChangedEvent.AddRaw( this, &SUsdStage::OnViewportSelectionChanged );

	IUsdStageModule& UsdStageModule = FModuleManager::Get().LoadModuleChecked< IUsdStageModule >( "UsdStage" );
	ViewModel.UsdStageActor = UsdStageModule.FindUsdStageActor( GWorld );

	bUpdatingViewportSelection = false;
	bUpdatingPrimSelection = false;

	UE::FUsdStage UsdStage;

	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( StageActor )
	{
		UsdStage = static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage();
	}

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
		[
			SNew( SVerticalBox )

			// Menu
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.FillWidth( 1 )
				[
					MakeMainMenu()
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SBox )
					.HAlign( HAlign_Right )
					.VAlign( VAlign_Fill )
					.Padding( FMargin( 0.0f, 0.0f, 10.0f, 0.0f ) )
					[
						MakeIsolateWarningButton()
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SBox )
					.HAlign( HAlign_Right )
					.VAlign( VAlign_Fill )
					[
						MakeActorPickerMenu()
					]
				]
			]

			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding( SUSDStageConstants::SectionPadding )
			[
				SNew( SSplitter )
				.Orientation( Orient_Vertical )

				+SSplitter::Slot()
				.Value( 0.7f )
				[
					SNew( SSplitter )
					.Orientation( Orient_Horizontal )

					// Stage tree
					+SSplitter::Slot()
					[
						SNew( SBorder )
						.BorderImage( FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")) )
						[
							SAssignNew( UsdStageTreeView, SUsdStageTreeView )
							.OnPrimSelectionChanged( this, &SUsdStage::OnPrimSelectionChanged )
						]
					]

					// Prim details
					+SSplitter::Slot()
					[
						SNew( SBorder )
						.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
						[
							SAssignNew( UsdPrimInfoWidget, SUsdPrimInfo )
						]
					]
				]

				// Layers tree
				+SSplitter::Slot()
				.Value( 0.3f )
				[
					SNew(SBorder)
					.BorderImage( FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")) )
					[
						SAssignNew( UsdLayersTreeView, SUsdLayersTreeView )
						.OnLayerIsolated( this, &SUsdStage::OnLayerIsolated )
					]
				]
			]
		]
	];

	SetupStageActorDelegates();

	// We're opening the USD Stage editor for the first time and we already have a stage: Display it immediately
	if ( UsdStage )
	{
		Refresh();
	}
}

void SUsdStage::SetupStageActorDelegates()
{
	ClearStageActorDelegates();

	if ( ViewModel.UsdStageActor.IsValid() )
	{
		OnPrimChangedHandle = ViewModel.UsdStageActor->OnPrimChanged.AddLambda(
			[ this ]( const FString& PrimPath, bool bResync )
			{
				// The USD notices may come from a background USD TBB thread, but we should only update slate from the main/slate threads.
				// We can't retrieve the FSlateApplication singleton here (because that can also only be used from the main/slate threads),
				// so we must use core tickers here
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda( [this, PrimPath, bResync]( float Time )
					{
						if ( this->UsdStageTreeView )
						{
							this->UsdStageTreeView->RefreshPrim( PrimPath, bResync );
							this->UsdStageTreeView->RequestTreeRefresh();
						}

						const bool bViewingTheUpdatedPrim = SelectedPrimPath.Equals( PrimPath, ESearchCase::IgnoreCase );
						const bool bViewingStageProperties = SelectedPrimPath.IsEmpty() || SelectedPrimPath == TEXT("/");
						const bool bStageUpdated = PrimPath == TEXT("/");

						if ( this->UsdPrimInfoWidget &&
							 ViewModel.UsdStageActor.IsValid() &&
							 ( bViewingTheUpdatedPrim || ( bViewingStageProperties && bStageUpdated ) ) )
						{
							this->UsdPrimInfoWidget->SetPrimPath( GetCurrentStage(), *PrimPath);
						}

						// If we resynced our selected prim or our ancestor and have selection sync enabled, try to refresh it so that we're
						// still selecting the same actor/component that corresponds to the prim we have currently selected
						if ( bResync && SelectedPrimPath.StartsWith( PrimPath ) )
						{
							OnPrimSelectionChanged( { SelectedPrimPath } );
						}

						// Returning false means this is a one-off, and won't repeat
						return false;
					})
				);
			}
		);

		// Fired when we switch which is the currently opened stage
		OnStageChangedHandle = ViewModel.UsdStageActor->OnStageChanged.AddLambda(
			[ this ]()
			{
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda( [this]( float Time )
					{
						// So we can reset even if our actor is being destroyed right now
						const bool bEvenIfPendingKill = true;
						if ( ViewModel.UsdStageActor.IsValid( bEvenIfPendingKill ) )
						{
							// Reset our selection to the stage root
							SelectedPrimPath = TEXT( "/" );

							if ( this->UsdPrimInfoWidget )
							{
								this->UsdPrimInfoWidget->SetPrimPath( GetCurrentStage(), TEXT( "/" ) );
							}

							if ( this->UsdStageTreeView )
							{
								this->UsdStageTreeView->ClearSelection();
								this->UsdStageTreeView->RequestTreeRefresh();
							}
						}

						this->Refresh();

						// Returning false means this is a one-off, and won't repeat
						return false;
					})
				);
			}
		);

		OnActorDestroyedHandle = ViewModel.UsdStageActor->OnActorDestroyed.AddLambda(
			[ this ]()
			{
				// Refresh widgets on game thread, but close the stage right away. In some contexts this is important, for example when
				// running a Python script: If our USD Stage Editor is open and our script deletes an actor, it will trigger OnActorDestroyed.
				// If we had this CloseStage() call inside the ticker, it would only take place when the script has finished running
				// (and control flow returned to the game thread) which can lead to some weird results (and break automated tests).
				// We could get around this on the Python script's side by just yielding, but there may be other scenarios
				ClearStageActorDelegates();
				this->ViewModel.CloseStage();

				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda( [this]( float Time )
					{
						this->Refresh();

						// Returning false means this is a one-off, and won't repeat
						return false;
					})
				);
			}
		);

		OnStageEditTargetChangedHandle = ViewModel.UsdStageActor->GetUsdListener().GetOnStageEditTargetChanged().AddLambda(
			[ this ]()
			{
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda( [this]( float Time )
					{
						AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
						if ( this->UsdLayersTreeView && StageActor )
						{
							constexpr bool bResync = false;
							UsdLayersTreeView->Refresh(
								StageActor->GetBaseUsdStage(),
								StageActor->GetIsolatedUsdStage(),
								bResync
							);
						}

						// Returning false means this is a one-off, and won't repeat
						return false;
					})
				);
			}
		);

		OnLayersChangedHandle = ViewModel.UsdStageActor->GetUsdListener().GetOnLayersChanged().AddLambda(
			[ this ]( const TArray< FString >& LayersNames )
			{
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda( [this]( float Time )
					{
						AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
						if ( this->UsdLayersTreeView && StageActor )
						{
							constexpr bool bResync = false;
							UsdLayersTreeView->Refresh(
								StageActor->GetBaseUsdStage(),
								StageActor->GetIsolatedUsdStage(),
								bResync
							);
						}

						// Returning false means this is a one-off, and won't repeat
						return false;
					})
				);
			}
		);
	}
}

void SUsdStage::ClearStageActorDelegates()
{
	const bool bEvenIfPendingKill = true;
	if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get( bEvenIfPendingKill ) )
	{
		StageActor->OnStageChanged.Remove( OnStageChangedHandle );
		StageActor->OnPrimChanged.Remove( OnPrimChangedHandle );
		StageActor->OnActorDestroyed.Remove ( OnActorDestroyedHandle );

		StageActor->GetUsdListener().GetOnStageEditTargetChanged().Remove( OnStageEditTargetChangedHandle );
		StageActor->GetUsdListener().GetOnLayersChanged().Remove( OnLayersChangedHandle );
	}
}

SUsdStage::~SUsdStage()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove( OnStageActorPropertyChangedHandle );
	AUsdStageActor::OnActorLoaded.Remove( OnActorLoadedHandle );
	USelection::SelectionChangedEvent.Remove( OnViewportSelectionChangedHandle );

	ClearStageActorDelegates();

	ActorPickerMenu.Reset();
}

TSharedRef< SWidget > SUsdStage::MakeMainMenu()
{
	FMenuBarBuilder MenuBuilder( nullptr );
	{
		// File
		MenuBuilder.AddPullDownMenu(
			LOCTEXT( "FileMenu", "File" ),
			LOCTEXT( "FileMenu_ToolTip", "Opens the file menu" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillFileMenu ) );

		// Actions
		MenuBuilder.AddPullDownMenu(
			LOCTEXT( "ActionsMenu", "Actions" ),
			LOCTEXT( "ActionsMenu_ToolTip", "Opens the actions menu" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillActionsMenu ) );

		// Options
		MenuBuilder.AddPullDownMenu(
			LOCTEXT( "OptionsMenu", "Options" ),
			LOCTEXT( "OptionsMenu_ToolTip", "Opens the options menu" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillOptionsMenu ) );
	}

	// Create the menu bar
	TSharedRef< SWidget > MenuBarWidget = MenuBuilder.MakeWidget();
	MenuBarWidget->SetVisibility( EVisibility::Visible ); // Work around for menu bar not showing on Mac

	return MenuBarWidget;
}

TSharedRef< SWidget > SUsdStage::MakeActorPickerMenu()
{
	return SNew( SComboButton )
		.ComboButtonStyle( FAppStyle::Get(), "SimpleComboButton" )
		.OnGetMenuContent( this, &SUsdStage::MakeActorPickerMenuContent )
		.MenuPlacement( EMenuPlacement::MenuPlacement_BelowRightAnchor )
		.ToolTipText( LOCTEXT( "ActorPicker_ToolTip", "Switch the active stage actor" ) )
		.ButtonContent()
		[
			SNew( STextBlock )
			.Text_Lambda( [this]()
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					return FText::FromString( StageActor->GetActorLabel() );
				}

				return FText::FromString( "None" );
			})
		];
}

TSharedRef< SWidget > SUsdStage::MakeActorPickerMenuContent()
{
	if ( !ActorPickerMenu.IsValid() )
	{
		FSceneOutlinerInitializationOptions InitOptions;
		InitOptions.bShowHeaderRow = false;
		InitOptions.bShowSearchBox = true;
		InitOptions.bShowCreateNewFolder = false;
		InitOptions.bFocusSearchBoxWhenOpened = true;
		InitOptions.ColumnMap.Add( FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo( ESceneOutlinerColumnVisibility::Visible, 0 ) );
		InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(
			FActorTreeItem::FFilterPredicate::CreateLambda(
				[]( const AActor* Actor )
				{
					return Actor && Actor->IsA<AUsdStageActor>();
				}
			)
		);

		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>( "SceneOutliner" );
		ActorPickerMenu = SceneOutlinerModule.CreateActorPicker(
			InitOptions,
			FOnActorPicked::CreateLambda(
				[this]( AActor* Actor )
				{
					if ( Actor && Actor->IsA<AUsdStageActor>() )
					{
						this->SetActor( Cast<AUsdStageActor>( Actor ) );

						FSlateApplication::Get().DismissAllMenus();
					}
				}
			)
		);
	}

	if ( ActorPickerMenu.IsValid() )
	{
		ActorPickerMenu->FullRefresh();

		return SNew(SBox)
			.Padding( FMargin( 1 ) )    // Add a small margin or else we'll get dark gray on dark gray which can look a bit confusing
			.MinDesiredWidth( 400.0f )  // Force a min width or else the tree view item text will run up right to the very edge pixel of the menu
			.HAlign( HAlign_Fill )
			[
				ActorPickerMenu.ToSharedRef()
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef< SWidget > SUsdStage::MakeIsolateWarningButton()
{
	return SNew( SButton )
		.ButtonStyle( &FAppStyle::Get().GetWidgetStyle<FButtonStyle>( "EditorViewportToolBar.WarningButton" ) )
		.ButtonColorAndOpacity( FLinearColor{ 1.0f, 0.4f, 0.0f, 1.0f } )
		.OnClicked_Lambda( [this]() -> FReply
		{
			// Isolating nothing is how we un-isolate
			OnLayerIsolated( UE::FSdfLayerWeak{} );
			return FReply::Handled();
		})
		.Visibility_Lambda( [this]() -> EVisibility
		{
			if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
			{
				if ( StageActor->GetIsolatedUsdStage() )
				{
					return EVisibility::Visible;
				}
			}
			return EVisibility::Hidden;
		})
		.ToolTipText_Lambda( [this]() -> FText
		{
			if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
			{
				if ( const UE::FUsdStage& CurrentStage = StageActor->GetUsdStage() )
				{
					return FText::Format(
						LOCTEXT( "IsolateWarningButtonToolTip", "The USD Stage editor and the Unreal level are displaying an isolated stage with '{0}' as its root layer.\nPress this button to revert to displaying the entire composed stage." ),
						FText::FromString( CurrentStage.GetRootLayer().GetDisplayName() )
					);
				}
			}

			return FText::GetEmpty();
		})
		.Content()
		[
			SNew( STextBlock )
			.Text( LOCTEXT( "IsolateWarningButtonText", "Isolated Mode" ) )
		];
}

void SUsdStage::FillFileMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "File", LOCTEXT("File", "File") );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("New", "New"),
			LOCTEXT("New_ToolTip", "Creates a new layer and opens the stage with it at its root"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnNew ),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Open", "Open..."),
			LOCTEXT("Open_ToolTip", "Opens a USD file"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnOpen ),
				FCanExecuteAction()
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Save", "Save"),
			LOCTEXT("Save_ToolTip", "Saves the stage"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnSave ),
				FCanExecuteAction::CreateLambda( [this]()
				{
					if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
						{
							return true;
						}
					}

					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddSubMenu(
			LOCTEXT( "Export", "Export" ),
			FText::GetEmpty(),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillExportSubMenu )
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "Reload", LOCTEXT( "Reload", "Reload" ) );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Reload", "Reload"),
			LOCTEXT("Reload_ToolTip", "Reloads the stage from disk, keeping aspects of the session intact"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnReloadStage ),
				FCanExecuteAction::CreateLambda( [this]()
				{
					if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
						{
							if ( !Stage.GetRootLayer().IsAnonymous() )
							{
								return true;
							}
						}
					}

					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT( "ResetState", "Reset state" ),
			LOCTEXT( "ResetState_ToolTip", "Resets the session layer and other options like edit target and muted layers" ),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnResetStage ),
				FCanExecuteAction::CreateLambda( [this]()
				{
					if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
						{
							return true;
						}
					}

					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( "Close", LOCTEXT( "Close", "Close" ) );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Close", "Close"),
			LOCTEXT("Close_ToolTip", "Closes the opened stage"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnClose ),
				FCanExecuteAction::CreateLambda( [this]()
				{
					if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
						{
							return true;
						}
					}

					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

void SUsdStage::FillActionsMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.BeginSection( "Actions", LOCTEXT("Actions", "Actions") );
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Import", "Import..."),
			LOCTEXT("Import_ToolTip", "Imports the stage as Unreal assets"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SUsdStage::OnImport ),
				FCanExecuteAction::CreateLambda( [this]()
				{
					if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
						{
							return true;
						}
					}

					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::Button
		);
	}
	MenuBuilder.EndSection();
}

void SUsdStage::FillOptionsMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection( TEXT( "StageOptions" ), LOCTEXT( "StageOptions", "Stage options" ) );
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("Payloads", "Payloads"),
			LOCTEXT("Payloads_ToolTip", "What to do with payloads when initially opening the stage"),
			FNewMenuDelegate::CreateSP(this, &SUsdStage::FillPayloadsSubMenu));

		MenuBuilder.AddSubMenu(
			LOCTEXT("PurposesToLoad", "Purposes to load"),
			LOCTEXT("PurposesToLoad_ToolTip", "Only load prims with these specific purposes from the USD stage"),
			FNewMenuDelegate::CreateSP(this, &SUsdStage::FillPurposesToLoadSubMenu));

		MenuBuilder.AddSubMenu(
			LOCTEXT("RenderContext", "Render context"),
			LOCTEXT("RenderContext_ToolTip", "Choose which render context to use when parsing materials"),
			FNewMenuDelegate::CreateSP(this, &SUsdStage::FillRenderContextSubMenu));

		MenuBuilder.AddSubMenu(
			LOCTEXT( "MaterialPurpose", "Material purpose" ),
			LOCTEXT( "MaterialPurpose_ToolTip", "Material purpose to use when parsing material bindings in addition to the \"allPurpose\" fallback" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillMaterialPurposeSubMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "RootMotionHandling", "Root motion handling" ),
			LOCTEXT( "RootMotionHandling_ToolTip", "Choose how to handle root motion for generated AnimSequences and LevelSequences" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillRootMotionSubMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "Collapsing", "Collapsing" ),
			LOCTEXT( "Collapsing_ToolTip", "Whether to try to combine individual assets and components of the same type on a Kind-per-Kind basis, like multiple Mesh prims into a single Static Mesh" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillCollapsingSubMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "InterpolationType", "Interpolation type" ),
			LOCTEXT( "InterpolationType_ToolTip", "Whether to interpolate between time samples linearly or with 'held' (i.e. constant) interpolation" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillInterpolationTypeSubMenu ) );
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection( TEXT( "EditorOptions" ), LOCTEXT( "EditorOptions", "Editor options" ) );
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT( "SelectionText", "Selection" ),
			LOCTEXT( "SelectionText_ToolTip", "How the selection of prims, actors and components should behave" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillSelectionSubMenu ) );

		MenuBuilder.AddSubMenu(
			LOCTEXT( "NaniteSettings", "Nanite" ),
			LOCTEXT( "NaniteSettings_ToolTip", "Configure how to use Nanite for generated assets" ),
			FNewMenuDelegate::CreateSP( this, &SUsdStage::FillNaniteThresholdSubMenu ),
			false,
			FSlateIcon(),
			false );
	}
	MenuBuilder.EndSection();
}

void SUsdStage::FillExportSubMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ExportAll", "Export all layers..." ),
		LOCTEXT( "ExportAll_ToolTip", "Exports copies of all file-based layers in the stage's layer stack to a new folder" ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SUsdStage::OnExportAll ),
			FCanExecuteAction::CreateLambda( [this]()
			{
				if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
					{
						return true;
					}
				}

				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "ExportFlattened", "Export flattened stage..." ),
		LOCTEXT( "ExportFlattened_ToolTip", "Flattens the current stage to a single USD layer and exports it as a new USD file" ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP( this, &SUsdStage::OnExportFlattened ),
			FCanExecuteAction::CreateLambda( [this]()
			{
				if ( const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					if ( UE::FUsdStage Stage = StageActor->GetUsdStage() )
					{
						return true;
					}
				}

				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::Button
	);
}

void SUsdStage::FillPayloadsSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("LoadAll", "Load all"),
		LOCTEXT("LoadAll_ToolTip", "Loads all payloads initially"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					FScopedTransaction Transaction(FText::Format(
						LOCTEXT("SetLoadAllTransaction", "Set USD stage actor '{0}' actor to load all payloads initially"),
						FText::FromString(StageActor->GetActorLabel())
					));

					StageActor->Modify();
					StageActor->InitialLoadSet = EUsdInitialLoadSet::LoadAll;
				}
			}),
			FCanExecuteAction::CreateLambda([this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					return StageActor->InitialLoadSet == EUsdInitialLoadSet::LoadAll;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LoadNone", "Load none"),
		LOCTEXT("LoadNone_ToolTip", "Don't load any payload initially"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					FScopedTransaction Transaction(FText::Format(
						LOCTEXT("SetLoadNoneTransaction", "Set USD stage actor '{0}' actor to load no payloads initially"),
						FText::FromString(StageActor->GetActorLabel())
					));

					StageActor->Modify();
					StageActor->InitialLoadSet = EUsdInitialLoadSet::LoadNone;
				}
			}),
			FCanExecuteAction::CreateLambda([this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					return StageActor->InitialLoadSet == EUsdInitialLoadSet::LoadNone;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

void SUsdStage::FillPurposesToLoadSubMenu(FMenuBuilder& MenuBuilder)
{
	auto AddPurposeEntry = [&](const EUsdPurpose& Purpose, const FText& Text)
	{
		MenuBuilder.AddMenuEntry(
			Text,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, Purpose]()
				{
					if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
					{
						FScopedTransaction Transaction(FText::Format(
							LOCTEXT("PurposesToLoadTransaction", "Change purposes to load for USD stage actor '{0}'"),
							FText::FromString(StageActor->GetActorLabel())
						));

						// c.f. comment in SUsdStage::FillCollapsingSubMenu
						TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

						StageActor->Modify();
						StageActor->PurposesToLoad = (int32)((EUsdPurpose)StageActor->PurposesToLoad ^ Purpose);

						FPropertyChangedEvent PropertyChangedEvent(
							FindFieldChecked< FProperty >( StageActor->GetClass(), GET_MEMBER_NAME_CHECKED( AUsdStageActor, PurposesToLoad ) )
						);
						StageActor->PostEditChangeProperty(PropertyChangedEvent);
					}
				}),
				FCanExecuteAction::CreateLambda([this]()
				{
					return ViewModel.UsdStageActor.Get() != nullptr;
				}),
				FIsActionChecked::CreateLambda([this, Purpose]()
				{
					if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
					{
						return EnumHasAllFlags((EUsdPurpose)StageActor->PurposesToLoad, Purpose);
					}
					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	};

	AddPurposeEntry(EUsdPurpose::Proxy,  LOCTEXT("ProxyPurpose",  "Proxy"));
	AddPurposeEntry(EUsdPurpose::Render, LOCTEXT("RenderPurpose", "Render"));
	AddPurposeEntry(EUsdPurpose::Guide,  LOCTEXT("GuidePurpose",  "Guide"));
}

void SUsdStage::FillRenderContextSubMenu( FMenuBuilder& MenuBuilder )
{
	auto AddRenderContextEntry = [&](const FName& RenderContext)
	{
		FText RenderContextName = FText::FromName( RenderContext );
		if ( RenderContext.IsNone() )
		{
			RenderContextName = LOCTEXT("UniversalRenderContext", "universal");
		}

		MenuBuilder.AddMenuEntry(
			RenderContextName,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, RenderContext]()
				{
					if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						FScopedTransaction Transaction(FText::Format(
							LOCTEXT("RenderContextToLoadTransaction", "Change render context to load for USD stage actor '{0}'"),
							FText::FromString(StageActor->GetActorLabel())
						));

						// c.f. comment in SUsdStage::FillCollapsingSubMenu
						TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

						StageActor->Modify();
						StageActor->RenderContext = RenderContext;

						FPropertyChangedEvent PropertyChangedEvent(
							FindFieldChecked< FProperty >( StageActor->GetClass(), GET_MEMBER_NAME_CHECKED( AUsdStageActor, RenderContext ) )
						);
						StageActor->PostEditChangeProperty(PropertyChangedEvent);
					}
				}),
				FCanExecuteAction::CreateLambda([this]()
				{
					return ViewModel.UsdStageActor.Get() != nullptr;
				}),
				FIsActionChecked::CreateLambda([this, RenderContext]()
				{
					if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						return StageActor->RenderContext == RenderContext;
					}
					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	};

	IUsdSchemasModule& UsdSchemasModule = FModuleManager::Get().LoadModuleChecked< IUsdSchemasModule >( TEXT("USDSchemas") );

	for ( const FName& RenderContext : UsdSchemasModule.GetRenderContextRegistry().GetRenderContexts() )
	{
		AddRenderContextEntry( RenderContext );
	}
}

void SUsdStage::FillMaterialPurposeSubMenu( FMenuBuilder& MenuBuilder )
{
	MaterialPurposes = {
		MakeShared<FString>( UnrealIdentifiers::MaterialAllPurpose ),
		MakeShared<FString>( UnrealIdentifiers::MaterialPreviewPurpose ),
		MakeShared<FString>( UnrealIdentifiers::MaterialFullPurpose )
	};

	// Add additional purposes from project settings
	if ( const UUsdProjectSettings* ProjectSettings = GetDefault<UUsdProjectSettings>() )
	{
		MaterialPurposes.Reserve( MaterialPurposes.Num() + ProjectSettings->AdditionalMaterialPurposes.Num() );

		TSet<FString> ExistingEntries = {
			UnrealIdentifiers::MaterialAllPurpose,
			UnrealIdentifiers::MaterialPreviewPurpose,
			UnrealIdentifiers::MaterialFullPurpose
		};

		for (const FName& AdditionalPurpose : ProjectSettings->AdditionalMaterialPurposes )
		{
			FString AdditionalPurposeStr = AdditionalPurpose.ToString();

			if ( !ExistingEntries.Contains( AdditionalPurposeStr ) )
			{
				ExistingEntries.Add( AdditionalPurposeStr );
				MaterialPurposes.AddUnique( MakeShared<FString>( AdditionalPurposeStr ) );
			}
		}
	}

	TSharedRef<SBox> Box = SNew( SBox )
	.Padding( FMargin( 8.0f, 0.0f ) )
	.VAlign( VAlign_Center )
	[
		// We have to use TSharedPtr<FString> here as the combobox actually contains a list view.
		// Also, a regular SComboBox<FName> doesn't even call our OnGenerateWidget function in case the item is NAME_None,
		// and we do need that case for allPurpose.
		SNew( SComboBox< TSharedPtr<FString> > )
		.OptionsSource( &MaterialPurposes )
		.OnGenerateWidget_Lambda( [ & ]( TSharedPtr<FString> Option )
		{
			TSharedPtr<SWidget> Widget = SNullWidget::NullWidget;
			if ( Option )
			{
				Widget = SNew( STextBlock )
					.Text( FText::FromString( (*Option) == UnrealIdentifiers::MaterialAllPurpose
						? UnrealIdentifiers::MaterialAllPurposeText
						: *Option
					))
					.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) );
			}

			return Widget.ToSharedRef();
		})
		.OnSelectionChanged_Lambda([this]( TSharedPtr<FString> ChosenOption, ESelectInfo::Type SelectInfo )
		{
			if ( ChosenOption )
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					StageActor->SetMaterialPurpose( **ChosenOption );
				}
			}
		})
		[
			SNew( SEditableTextBox )
			.Text_Lambda([this]() -> FText
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					return FText::FromString( StageActor->MaterialPurpose == *UnrealIdentifiers::MaterialAllPurpose
						? UnrealIdentifiers::MaterialAllPurposeText
						: StageActor->MaterialPurpose.ToString()
					);
				}

				return FText::GetEmpty();
			})
			.Font( FAppStyle::GetFontStyle( "PropertyWindow.NormalFont" ) )
			.OnTextCommitted_Lambda( [this]( const FText& NewText, ETextCommit::Type CommitType )
			{
				if ( CommitType != ETextCommit::OnEnter )
				{
					return;
				}

				FString NewPurposeString = NewText.ToString();
				FName NewPurpose = *NewPurposeString;

				bool bIsNew = true;
				for ( const TSharedPtr<FString>& Purpose : MaterialPurposes )
				{
					if ( Purpose && *Purpose == NewPurposeString )
					{
						bIsNew = false;
						break;
					}
				}

				if ( bIsNew )
				{
					if ( UUsdProjectSettings* ProjectSettings = GetMutableDefault<UUsdProjectSettings>() )
					{
						ProjectSettings->AdditionalMaterialPurposes.AddUnique( NewPurpose );
						ProjectSettings->SaveConfig();
					}
				}

				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					StageActor->SetMaterialPurpose( NewPurpose );
				}
			})
		]
	];

	const bool bNoIndent = true;
	MenuBuilder.AddWidget( Box, FText::GetEmpty(), bNoIndent );
}

void SUsdStage::FillRootMotionSubMenu( FMenuBuilder& MenuBuilder )
{
	auto AddRootMotionEntry = [&](const EUsdRootMotionHandling& HandlingStrategy, const FText& Text)
	{
		UEnum* Enum = StaticEnum< EUsdRootMotionHandling >();
		FText ToolTip = Enum->GetToolTipTextByIndex( Enum->GetIndexByValue( static_cast< uint8 >( HandlingStrategy ) ) );

		MenuBuilder.AddMenuEntry(
			Text,
			ToolTip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, HandlingStrategy]()
				{
					if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
					{
						FScopedTransaction Transaction(FText::Format(
							LOCTEXT("RootMotionHandlingTransaction", "Change root motion handling strategy for USD stage actor '{0}'"),
							FText::FromString(StageActor->GetActorLabel())
						));

						// c.f. comment in SUsdStage::FillCollapsingSubMenu
						TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

						StageActor->Modify();
						StageActor->SetRootMotionHandling( HandlingStrategy );
					}
				}),
				FCanExecuteAction::CreateLambda([this]()
				{
					return ViewModel.UsdStageActor.Get() != nullptr;
				}),
				FIsActionChecked::CreateLambda([this, HandlingStrategy]()
				{
					if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
					{
						return StageActor->RootMotionHandling == HandlingStrategy;
					}
					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
	};

	AddRootMotionEntry( EUsdRootMotionHandling::NoAdditionalRootMotion, LOCTEXT("NoAdditionalRootMotionText", "No additional root motion"));
	AddRootMotionEntry( EUsdRootMotionHandling::UseMotionFromSkelRoot, LOCTEXT("UseMotionFromSkelRootText", "Use motion from SkelRoot"));
	AddRootMotionEntry( EUsdRootMotionHandling::UseMotionFromSkeleton, LOCTEXT("UseMotionFromSkeletonText", "Use motion from Skeleton"));
}

void SUsdStage::FillCollapsingSubMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "MergeIdenticalMaterialSlots", "Merge identical material slots" ),
		LOCTEXT( "MergeIdenticalMaterialSlots_ToolTip", "If enabled, when multiple mesh prims are collapsed into a single static mesh, identical material slots are merged into one slot.\nOtherwise, material slots are simply appended to the list." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda( [this]()
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					FScopedTransaction Transaction( FText::Format(
						LOCTEXT( "MergeIdenticalMaterialSlotsTransaction", "Toggle bMergeIdenticalMaterialSlots on USD stage actor '{1}'" ),
						FText::FromString( StageActor->GetActorLabel() )
					) );

					TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );
					StageActor->SetMergeIdenticalMaterialSlots( !StageActor->bMergeIdenticalMaterialSlots );
				}
			}),
			FCanExecuteAction::CreateLambda( [this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda( [this]()
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					return StageActor->bMergeIdenticalMaterialSlots;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "CollapseTopLevelPointInstancers", "Collapse top-level point instancers" ),
		LOCTEXT( "CollapseTopLevelPointInstancersToolTip", "If true, will cause us to collapse any point instancer prim into a single static mesh and static mesh component.\nIf false, will cause us to use HierarchicalInstancedStaticMeshComponents to replicate the instancing behavior.\nPoint instancers inside other point instancer prototypes are * always * collapsed into the prototype's static mesh." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda( [this]()
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					FScopedTransaction Transaction( FText::Format(
						LOCTEXT( "CollapseTopLevelPointInstancersTransaction", "Toggle bCollapseTopLevelPointInstancers on USD stage actor '{1}'" ),
						FText::FromString( StageActor->GetActorLabel() )
					) );

					TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );
					StageActor->SetCollapseTopLevelPointInstancers( !StageActor->bCollapseTopLevelPointInstancers );
				}
			}),
			FCanExecuteAction::CreateLambda( [this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda( [this]()
			{
				if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
				{
					return StageActor->bCollapseTopLevelPointInstancers;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	auto AddKindToCollapseEntry = [&]( const EUsdDefaultKind Kind, const FText& Text, FCanExecuteAction CanExecuteAction )
	{
		MenuBuilder.AddMenuEntry(
			Text,
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda( [this, Text, Kind]()
				{
					if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						FScopedTransaction Transaction( FText::Format(
							LOCTEXT( "ToggleCollapsingTransaction", "Toggle asset and component collapsing for kind '{0}' on USD stage actor '{1}'" ),
							Text,
							FText::FromString( StageActor->GetActorLabel() )
						) );

						// When we change our kinds to collapse, we'll end up loading the stage again and refreshing our selection.
						// We'll take care to try to re-select the same prims within SUsdStage::Refresh after the refresh is complete,
						// but if selection sync is on we'll also later on try updating our prim selection to match our component selection.
						// Not only is this "selection spam" is very visible on the USD Stage Editor, if our originally selected component
						// was just collapsed away, we'd end up updating our prim selection to point to the parent prim instead
						// (the collapsing root), which is not ideal.
						TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

						int32 NewKindsToCollapse = ( int32 ) ( ( EUsdDefaultKind ) StageActor->KindsToCollapse ^ Kind );
						StageActor->SetKindsToCollapse( NewKindsToCollapse );
					}
				}),
				CanExecuteAction,
				FIsActionChecked::CreateLambda( [this, Kind]()
				{
					if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
					{
						return EnumHasAllFlags( ( EUsdDefaultKind ) StageActor->KindsToCollapse, Kind );
					}
					return false;
				})
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	};

	MenuBuilder.BeginSection( "Kinds to collapse", LOCTEXT( "KindsToCollapse", "Kinds to collapse" ) );

	AddKindToCollapseEntry(
		EUsdDefaultKind::Model,
		LOCTEXT( "ModelKind", "Model" ),
		FCanExecuteAction::CreateLambda( [this]()
		{
			return ViewModel.UsdStageActor.Get() != nullptr;
		})
	);

	AddKindToCollapseEntry(
		EUsdDefaultKind::Component,
		LOCTEXT( "ModelComponent", "   Component" ),
		FCanExecuteAction::CreateLambda( [this]()
		{
			if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
			{
				// If we're collapsing all "model" kinds, the all "component"s should be collapsed anyway
				if ( !EnumHasAnyFlags( ( EUsdDefaultKind ) StageActor->KindsToCollapse, EUsdDefaultKind::Model ) )
				{
					return true;
				}
			}

			return false;
		})
	);

	AddKindToCollapseEntry(
		EUsdDefaultKind::Group,
		LOCTEXT( "ModelGroup", "   Group" ),
		FCanExecuteAction::CreateLambda( [this]()
		{
			if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
			{
				if ( !EnumHasAnyFlags( ( EUsdDefaultKind ) StageActor->KindsToCollapse, EUsdDefaultKind::Model ) )
				{
					return true;
				}
			}

			return false;
		})
	);

	AddKindToCollapseEntry(
		EUsdDefaultKind::Assembly,
		LOCTEXT( "ModelAssembly", "      Assembly" ),
		FCanExecuteAction::CreateLambda( [this]()
		{
			if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
			{
				if ( !EnumHasAnyFlags( ( EUsdDefaultKind ) StageActor->KindsToCollapse, EUsdDefaultKind::Model ) && !EnumHasAnyFlags( ( EUsdDefaultKind ) StageActor->KindsToCollapse, EUsdDefaultKind::Group ) )
				{
					return true;
				}
			}

			return false;
		})
	);

	AddKindToCollapseEntry(
		EUsdDefaultKind::Subcomponent,
		LOCTEXT( "ModelSubcomponent", "Subcomponent" ),
		FCanExecuteAction::CreateLambda( [this]()
		{
			return ViewModel.UsdStageActor.Get() != nullptr;
		})
	);

	MenuBuilder.EndSection();
}

void SUsdStage::FillInterpolationTypeSubMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("LinearType", "Linear"),
		LOCTEXT("LinearType_ToolTip", "Attribute values are linearly interpolated between authored values"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					FScopedTransaction Transaction(FText::Format(
						LOCTEXT("SetLinearInterpolationType", "Set USD stage actor '{0}' to linear interpolation"),
						FText::FromString(StageActor->GetActorLabel())
					));

					// c.f. comment in SUsdStage::FillCollapsingSubMenu
					TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

					StageActor->SetInterpolationType( EUsdInterpolationType::Linear );
				}
			}),
			FCanExecuteAction::CreateLambda([this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					return StageActor->InterpolationType == EUsdInterpolationType::Linear;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HeldType", "Held"),
		LOCTEXT("HeldType_ToolTip", "Attribute values are held constant between authored values. An attribute's value will be equal to the nearest preceding authored value. If there is no preceding authored value, the value will be equal to the nearest subsequent value."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					FScopedTransaction Transaction(FText::Format(
						LOCTEXT("SetHeldInterpolationType", "Set USD stage actor '{0}' to held interpolation"),
						FText::FromString(StageActor->GetActorLabel())
					));

					// c.f. comment in SUsdStage::FillCollapsingSubMenu
					TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

					StageActor->SetInterpolationType( EUsdInterpolationType::Held );
				}
			}),
			FCanExecuteAction::CreateLambda([this]()
			{
				return ViewModel.UsdStageActor.Get() != nullptr;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				if(AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get())
				{
					return StageActor->InterpolationType == EUsdInterpolationType::Held;
				}
				return false;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
	);
}

void SUsdStage::FillSelectionSubMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT( "SynchronizeText", "Synchronize with Editor" ),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				if ( UUsdStageEditorSettings* Settings = GetMutableDefault<UUsdStageEditorSettings>() )
				{
					Settings->bSelectionSynced = !Settings->bSelectionSynced;
					Settings->SaveConfig();
				}

				// Immediately sync the selection, but only if the results are obvious (i.e. either our prim selection or viewport selection are empty)
				if ( GEditor )
				{
					int32 NumPrimsSelected = UsdStageTreeView->GetNumItemsSelected();
					int32 NumViewportSelected = FMath::Max( GEditor->GetSelectedComponentCount(), GEditor->GetSelectedActorCount() );

					if ( NumPrimsSelected == 0 )
					{
						USelection* Selection = GEditor->GetSelectedComponentCount() > 0
							? GEditor->GetSelectedComponents()
							: GEditor->GetSelectedActors();

						OnViewportSelectionChanged(Selection);
					}
					else if ( NumViewportSelected == 0 )
					{
						TGuardValue<bool> SelectionLoopGuard( bUpdatingViewportSelection, true );

						SUSDStageImpl::SelectGeneratedComponentsAndActors( ViewModel.UsdStageActor.Get(), UsdStageTreeView->GetSelectedPrims() );
					}
				}
			}),
			FCanExecuteAction::CreateLambda([]()
			{
				return true;
			}),
			FIsActionChecked::CreateLambda([this]()
			{
				const UUsdStageEditorSettings* Settings = GetDefault<UUsdStageEditorSettings>();
				return Settings && Settings->bSelectionSynced;
			})
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}

void SUsdStage::FillNaniteThresholdSubMenu( FMenuBuilder& MenuBuilder )
{
	if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
	{
		CurrentNaniteThreshold = StageActor->NaniteTriangleThreshold;
	}

	TSharedRef<SSpinBox<int32>> Slider = SNew( SSpinBox<int32> )
		.MinValue( 0 )
		.ToolTipText( LOCTEXT( "TriangleThresholdTooltip", "Try enabling Nanite for static meshes that are generated with at least this many triangles" ) )
		.Value( this, &SUsdStage::GetNaniteTriangleThresholdValue )
		.OnValueChanged(this, &SUsdStage::OnNaniteTriangleThresholdValueChanged )
		.SupportDynamicSliderMaxValue( true )
		.IsEnabled_Lambda( [this]() -> bool
		{
			return ViewModel.UsdStageActor.Get() != nullptr;
		})
		.OnValueCommitted( this, &SUsdStage::OnNaniteTriangleThresholdValueCommitted );

	const bool bNoIndent = true;
	MenuBuilder.AddWidget( Slider, FText::FromString( TEXT( "Triangle threshold: " ) ), bNoIndent );
}

void SUsdStage::OnNew()
{
	ViewModel.NewStage();
}

void SUsdStage::OnOpen()
{
	TOptional< FString > UsdFilePath = UsdUtils::BrowseUsdFile( UsdUtils::EBrowseFileMode::Open );

	if ( UsdFilePath )
	{
		OpenStage( *UsdFilePath.GetValue() );
	}
}

void SUsdStage::OnSave()
{
	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( !StageActor )
	{
		return;
	}

	UE::FUsdStage UsdStage = static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage();
	if ( !UsdStage )
	{
		return;
	}

	if ( UE::FSdfLayer RootLayer = UsdStage.GetRootLayer() )
	{
		FString RealPath = RootLayer.GetRealPath();
		if ( FPaths::FileExists( RealPath ) )
		{
			ViewModel.SaveStage();
		}
		else
		{
			TOptional< FString > UsdFilePath = UsdUtils::BrowseUsdFile( UsdUtils::EBrowseFileMode::Save );
			if ( UsdFilePath )
			{
				ViewModel.SaveStageAs( *UsdFilePath.GetValue() );
			}
		}
	}
}

void SUsdStage::OnExportAll()
{
	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( !StageActor )
	{
		return;
	}

	UE::FUsdStage UsdStage = StageActor->GetUsdStage();
	if ( !UsdStage )
	{
		return;
	}

	const bool bIncludeClipLayers = false;
	TArray<UE::FSdfLayer> LayerStack = UsdStage.GetUsedLayers( bIncludeClipLayers );
	if ( LayerStack.Num() < 1 )
	{
		return;
	}

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if ( !DesktopPlatform )
	{
		return;
	}

	TSharedPtr< SWindow > ParentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared() );
	void* ParentWindowHandle = ( ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid() )
		? ParentWindow->GetNativeWindow()->GetOSWindowHandle()
		: nullptr;

	FString TargetFolderPath;
	if ( !DesktopPlatform->OpenDirectoryDialog( ParentWindowHandle, LOCTEXT( "ChooseFolder", "Choose output folder" ).ToString(), TEXT( "" ), TargetFolderPath ) )
	{
		return;
	}
	TargetFolderPath = FPaths::ConvertRelativePathToFull( TargetFolderPath );

	double StartTime = FPlatformTime::Cycles64();

	// Manually load any layer that is referenced by any of the stage's layers, but not currently loaded.
	// This can happen if e.g. an unselected variant appends a reference or has a payload.
	// We will need to actually load these as opposed to just copy-paste the files as we may need to update references/paths
	SUSDStageImpl::AppendAllExternalReferences( LayerStack );

	// Discard session layers
	for ( int32 Index = LayerStack.Num() - 1; Index >= 0; --Index )
	{
		UE::FSdfLayer& Layer = LayerStack[ Index ];
		if ( Layer.IsAnonymous() )
		{
			const int32 Count = 1;
			const bool bAllowShrinking = false;
			LayerStack.RemoveAt( Index, Count, bAllowShrinking );
		}
	}

#if PLATFORM_LINUX
	TMap<FString, FString, FDefaultSetAllocator, UsdUtils::FCaseSensitiveStringMapFuncs< FString > > OldPathToSavedPath;
#else
	TMap<FString, FString> OldPathToSavedPath;
#endif

	// If the stage has layers referencing each other, we will need to remap those to point exclusively at the newly saved files
	TSet<FString> SavedPaths;
	for ( const UE::FSdfLayer& Layer : LayerStack )
	{
		FString LayerPath = Layer.GetRealPath();
		FPaths::NormalizeFilename( LayerPath );

		FString TargetPath = FPaths::Combine( TargetFolderPath, Layer.GetDisplayName() );
		FPaths::NormalizeFilename( TargetPath );

		// Filename collision (should be rare, but possible given that we're discarding the folder structure)
		if ( SavedPaths.Contains( TargetPath ) )
		{
			FString TargetPathNoExt = FPaths::SetExtension( TargetPath, TEXT( "" ) );
			FString Ext = FPaths::GetExtension( TargetPath );
			int32 Suffix = 0;

			do
			{
				TargetPath = FString::Printf( TEXT( "%s_%d.%s" ), *TargetPathNoExt, Suffix++, *Ext );
			} while ( SavedPaths.Contains( TargetPath ) );
		}

		OldPathToSavedPath.Add( LayerPath, TargetPath );
		SavedPaths.Add( TargetPath );
	}

	// Actually save the output layers
	for ( UE::FSdfLayer& Layer : LayerStack )
	{
		FString LayerPath = Layer.GetRealPath();
		FPaths::NormalizeFilename( LayerPath );

		if ( FString* TargetLayerPath = OldPathToSavedPath.Find( LayerPath ) )
		{
			// Clone the layer so that we don't modify the currently opened stage
			UE::FSdfLayer OutputLayer = UE::FSdfLayer::CreateNew( **TargetLayerPath );
			OutputLayer.TransferContent( Layer );

			// Update references to assets (e.g. textures) so that they're absolute and also work from the new file
			UsdUtils::ConvertAssetRelativePathsToAbsolute( OutputLayer, Layer );

			// Remap references to layers so that they point at the other newly saved files. Note that SUSDStageImpl::AppendAllExternalReferences
			// will have collected all external references already, so OldPathToSavedPath should have entries for all references we'll find
			for ( const FString& AssetDependency : OutputLayer.GetCompositionAssetDependencies() )
			{
				FString AbsRef = FPaths::ConvertRelativePathToFull( FPaths::GetPath( LayerPath ), AssetDependency ); // Relative to the original file
				FPaths::NormalizeFilename( AbsRef );

				if ( FString* SavedReference = OldPathToSavedPath.Find( AbsRef ) )
				{
					OutputLayer.UpdateCompositionAssetDependency( *AssetDependency, **SavedReference );
				}
			}

			bool bForce = true;
			OutputLayer.Save( bForce );
		}
	}

	// Send analytics
	if ( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;

		EventAttributes.Emplace( TEXT( "NumLayersExported" ), LayerStack.Num() );

		bool bAutomated = false;
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		FString Extension = FPaths::GetExtension( UsdStage.GetRootLayer().GetDisplayName() );
		IUsdClassesModule::SendAnalytics(
			MoveTemp( EventAttributes ),
			TEXT( "ExportStageLayers" ),
			bAutomated,
			ElapsedSeconds,
			UsdUtils::GetUsdStageNumFrames( UsdStage ),
			Extension
		);
	}
}

void SUsdStage::OnExportFlattened()
{
	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( !StageActor )
	{
		return;
	}

	UE::FUsdStage UsdStage = StageActor->GetUsdStage();
	if ( !UsdStage )
	{
		return;
	}

	TOptional< FString > UsdFilePath = UsdUtils::BrowseUsdFile( UsdUtils::EBrowseFileMode::Save );
	if ( !UsdFilePath.IsSet() )
	{
		return;
	}

	double StartTime = FPlatformTime::Cycles64();

	UsdStage.Export( *( UsdFilePath.GetValue() ) );

	// Send analytics
	if ( FEngineAnalytics::IsAvailable() )
	{
		bool bAutomated = false;
		double ElapsedSeconds = FPlatformTime::ToSeconds64( FPlatformTime::Cycles64() - StartTime );
		FString Extension = FPaths::GetExtension( UsdFilePath.GetValue() );
		IUsdClassesModule::SendAnalytics(
			{},
			TEXT( "ExportStageFlattened" ),
			bAutomated,
			ElapsedSeconds,
			UsdUtils::GetUsdStageNumFrames( UsdStage ),
			Extension
		);
	}
}

void SUsdStage::OnReloadStage()
{
	FScopedTransaction Transaction( LOCTEXT( "ReloadTransaction", "Reload USD stage" ) );

	ViewModel.ReloadStage();

	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();

	if ( UsdLayersTreeView && StageActor )
	{
		const bool bResync = true;
		UsdLayersTreeView->Refresh( StageActor->GetBaseUsdStage(), StageActor->GetIsolatedUsdStage(), bResync );
	}
}

void SUsdStage::OnResetStage()
{
	ViewModel.ResetStage();

	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();

	if ( UsdLayersTreeView && StageActor )
	{
		const bool bResync = true;
		UsdLayersTreeView->Refresh( StageActor->GetBaseUsdStage(), StageActor->GetIsolatedUsdStage(), bResync );
	}
}

void SUsdStage::OnClose()
{
	FScopedTransaction Transaction(LOCTEXT("CloseTransaction", "Close USD stage"));

	ViewModel.CloseStage();
	Refresh();
}

void SUsdStage::OnLayerIsolated( const UE::FSdfLayer& IsolatedLayer )
{
	if ( AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get() )
	{
		StageActor->IsolateLayer( IsolatedLayer );

		Refresh();
	}
}

void SUsdStage::OnImport()
{
	ViewModel.ImportStage();
}

void SUsdStage::OnPrimSelectionChanged( const TArray<FString>& PrimPaths )
{
	if ( bUpdatingPrimSelection )
	{
		return;
	}

	AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( !StageActor )
	{
		return;
	}

	if ( UsdPrimInfoWidget )
	{
		UE::FUsdStage UsdStage = static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage();

		SelectedPrimPath = PrimPaths.Num() == 1 ? PrimPaths[ 0 ] : TEXT( "" );
		UsdPrimInfoWidget->SetPrimPath( UsdStage, *SelectedPrimPath );
	}

	// We don't need to check for actors and components in case we're just selecting the stage root
	// Note: This is also a temp fix that prevents an ensure: When we're switching to a new stage,
	// AUsdStageActor::LoadUsdStage calls AUsdStageActor::OpenUsdStage, which triggers OnStageChanged
	// and gets us here. Ideally this wouldn't happen at the same time as AUsdStageActor::LoadUsdStage
	// because we need the InfoCache to be complete at this point. Strangely though, the SlowTask.EnterProgressFrame
	// within AUsdStageActor::LoadUsdStage triggers a full slate tick right there, that calls us before
	// LoadUsdStage is complete... in the future I think we can fix this by changing our delegates from
	// AsyncTask( ENamedThreads::GameThread ) to finishing up on the next tick instead, but for now
	// this will do
	const bool bIsRootSelection = PrimPaths.Num() == 1 && PrimPaths[0] == TEXT("/");

	const UUsdStageEditorSettings* Settings = GetDefault<UUsdStageEditorSettings>();
	if ( Settings && Settings->bSelectionSynced && GEditor && !bIsRootSelection )
	{
		TGuardValue<bool> SelectionLoopGuard( bUpdatingViewportSelection, true );

		SUSDStageImpl::SelectGeneratedComponentsAndActors( StageActor, PrimPaths );
	}
}

void SUsdStage::OpenStage( const TCHAR* FilePath )
{
	// Create the transaction before calling UsdStageModule.GetUsdStageActor as that may create the actor, and we want
	// the actor spawning to be part of the transaction
	FScopedTransaction Transaction( FText::Format(
		LOCTEXT( "OpenStageTransaction", "Open USD stage '{0}'" ),
		FText::FromString( FilePath )
	) );

	if ( !ViewModel.UsdStageActor.IsValid() )
	{
		IUsdStageModule& UsdStageModule = FModuleManager::Get().LoadModuleChecked< IUsdStageModule >( "UsdStage" );
		SetActor( &UsdStageModule.GetUsdStageActor( GWorld ) );
	}

	ViewModel.OpenStage( FilePath );
}

void SUsdStage::SetActor( AUsdStageActor* InUsdStageActor )
{
	// Call this first so that we clear all of our delegates from the previous actor before we switch actors
	ClearStageActorDelegates();

	ViewModel.UsdStageActor = InUsdStageActor;

	SetupStageActorDelegates();

	if( InUsdStageActor )
	{
		UE::FUsdStage UsdStage = static_cast< const AUsdStageActor* >( InUsdStageActor )->GetUsdStage();

		if ( this->UsdPrimInfoWidget && InUsdStageActor )
		{
			// Just reset to the pseudoroot for now
			this->UsdPrimInfoWidget->SetPrimPath( UsdStage, TEXT( "/" ) );
		}
	}

	Refresh();
}

void SUsdStage::Refresh()
{
	// May be nullptr, but that is ok. Its how the widgets are reset
	AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();

	if ( UsdLayersTreeView )
	{
		UE::FUsdStageWeak IsolatedStage = StageActor ? StageActor->GetIsolatedUsdStage() : UE::FUsdStage{};
		UE::FUsdStageWeak BaseStage = StageActor ? StageActor->GetBaseUsdStage() : UE::FUsdStage{};

		// The layers tree view always needs to receive both the full stage as well as the isolated
		const bool bResync = true;
		UsdLayersTreeView->Refresh( BaseStage, IsolatedStage, bResync );
	}

	if ( UsdStageTreeView )
	{
		UE::FUsdStageWeak CurrentStage = StageActor
			? static_cast< const AUsdStageActor* >( StageActor )->GetUsdStage()
			: UE::FUsdStage{};

		UsdStageTreeView->Refresh( CurrentStage );

		// Refresh will generate brand new RootItems, so let's immediately select the new item
		// that corresponds to the prim we're supposed to be selecting
		UsdStageTreeView->SelectPrims( { SelectedPrimPath } );

		UsdStageTreeView->RequestTreeRefresh();
	}

	if ( UsdPrimInfoWidget )
	{
		UsdPrimInfoWidget->SetPrimPath( GetCurrentStage(), *SelectedPrimPath );
	}
}

void SUsdStage::OnStageActorLoaded( AUsdStageActor* InUsdStageActor )
{
	if ( ViewModel.UsdStageActor == InUsdStageActor )
	{
		return;
	}

	ClearStageActorDelegates();
	ViewModel.UsdStageActor = InUsdStageActor;
	SetupStageActorDelegates();

	// Refresh here because we may be receiving an actor that has a stage already loaded,
	// like during undo/redo
	Refresh();
}

void SUsdStage::OnViewportSelectionChanged( UObject* NewSelection )
{
	// This may be called when first opening a project, before the our widgets are fully initialized
	if ( !UsdStageTreeView )
	{
		return;
	}

	const UUsdStageEditorSettings* Settings = GetDefault<UUsdStageEditorSettings>();
	if ( !Settings || !Settings->bSelectionSynced || bUpdatingViewportSelection )
	{
		return;
	}

	AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	USelection* Selection = Cast<USelection>( NewSelection );
	if ( !Selection || !StageActor )
	{
		return;
	}

	TGuardValue<bool> SelectionLoopGuard( bUpdatingPrimSelection, true );

	TArray<USceneComponent*> SelectedComponents;
	{
		Selection->GetSelectedObjects<USceneComponent>( SelectedComponents );

		TArray<AActor*> SelectedActors;
		Selection->GetSelectedObjects<AActor>( SelectedActors );

		for ( AActor* SelectedActor : SelectedActors )
		{
			if ( SelectedActor )
			{
				SelectedComponents.Add( SelectedActor->GetRootComponent() );
			}
		}
	}

	TArray<FString> PrimPaths;
	for ( USceneComponent* Component : SelectedComponents )
	{
		FString FoundPrimPath = StageActor->GetSourcePrimPath( Component );
		if ( !FoundPrimPath.IsEmpty() )
		{
			PrimPaths.Add( FoundPrimPath );
		}
	}

	if ( PrimPaths.Num() > 0 )
	{
		UsdStageTreeView->SelectPrims( PrimPaths );

		if ( UsdPrimInfoWidget )
		{
			UsdPrimInfoWidget->SetPrimPath( GetCurrentStage(), *PrimPaths[ PrimPaths.Num() - 1 ] );
		}
	}
}

int32 SUsdStage::GetNaniteTriangleThresholdValue() const
{
	return CurrentNaniteThreshold;
}

void SUsdStage::OnNaniteTriangleThresholdValueChanged( int32 InValue )
{
	CurrentNaniteThreshold = InValue;
}

void SUsdStage::OnNaniteTriangleThresholdValueCommitted( int32 InValue, ETextCommit::Type InCommitType )
{
	AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( !StageActor )
	{
		return;
	}

	FScopedTransaction Transaction( FText::Format(
		LOCTEXT( "NaniteTriangleThresholdCommittedTransaction", "Change Nanite triangle threshold for USD stage actor '{0}'" ),
		FText::FromString( StageActor->GetActorLabel() )
	) );

	// c.f. comment in SUsdStage::FillCollapsingSubMenu
	TGuardValue<bool> MaintainSelectionGuard( bUpdatingViewportSelection, true );

	StageActor->SetNaniteTriangleThreshold( InValue );
	CurrentNaniteThreshold = InValue;
}

UE::FUsdStageWeak SUsdStage::GetCurrentStage() const
{
	const AUsdStageActor* StageActor = ViewModel.UsdStageActor.Get();
	if ( StageActor )
	{
		return StageActor->GetUsdStage();
	}

	return UE::FUsdStageWeak{};
}

#endif // #if USE_USD_SDK

#undef LOCTEXT_NAMESPACE
