// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolkits/MediaPlateEditorToolkit.h"

#include "Editor.h"
#include "Styling/AppStyle.h"
#include "EditorReimportHandler.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"
#include "MediaPlateEditorModule.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "Models/MediaPlateEditorCommands.h"
#include "SlateOptMacros.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SMediaPlateEditorDetails.h"
#include "Widgets/SMediaPlateEditorMediaDetails.h"
#include "Widgets/SMediaPlateEditorPlaylist.h"
#include "Widgets/SMediaPlayerEditorViewer.h"

#define LOCTEXT_NAMESPACE "FMediaPlateEditorToolkit"

namespace MediaPlateEditorToolkit
{
	static const FName AppIdentifier("MediaPlateEditorApp");
	static const FName DetailsTabId("Details");
	static const FName MediaDetailsTabId("MediaDetails");
	static const FName PlaylistTabId("Playlist");
	static const FName ViewerTabId("Viewer");
}

/* FMediaPlateEditorToolkit structors
 *****************************************************************************/

FMediaPlateEditorToolkit::FMediaPlateEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: MediaPlate(nullptr)
	, Style(InStyle)
{
}

FMediaPlateEditorToolkit::~FMediaPlateEditorToolkit()
{
	FReimportManager::Instance()->OnPreReimport().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo(this);
}

/* FMediaPlateEditorToolkit interface
 *****************************************************************************/

void FMediaPlateEditorToolkit::Initialize(UMediaPlateComponent* InMediaPlate, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost)
{
	MediaPlate = InMediaPlate;

	if (MediaPlate == nullptr)
	{
		return;
	}

	// support undo/redo
	MediaPlate->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	BindCommands();

	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_MediaPlateEditor_v1.3")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
						->SetOrientation(Orient_Horizontal)
						->SetSizeCoefficient(0.7f)
						->Split
						(
							// viewer
							FTabManager::NewStack()
								->AddTab(MediaPlateEditorToolkit::ViewerTabId, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.6f)
						)	
						->Split
						(
							FTabManager::NewSplitter()
							->SetOrientation(Orient_Vertical)
							->SetSizeCoefficient(0.4f)
							->Split
							(
								// Media details tab.
								FTabManager::NewStack()
								->AddTab(MediaPlateEditorToolkit::MediaDetailsTabId, ETabState::OpenedTab)
								->SetSizeCoefficient(0.2f)
							)
							->Split
							(
								// Details tab.
								FTabManager::NewStack()
								->AddTab(MediaPlateEditorToolkit::DetailsTabId, ETabState::OpenedTab)
								->SetSizeCoefficient(0.8f)
							)
						)
				)
				->Split
				(
					// Details tab.
					FTabManager::NewStack()
						->AddTab(MediaPlateEditorToolkit::PlaylistTabId, ETabState::OpenedTab)
						->SetSizeCoefficient(0.3f)
				)
		);

	FAssetEditorToolkit::InitAssetEditor(
		InMode,
		InToolkitHost,
		MediaPlateEditorToolkit::AppIdentifier,
		Layout,
		true,
		true,
		InMediaPlate
	);
	
	ExtendToolBar();
	RegenerateMenusAndToolbars();

	// Tell the editor module that this media plate is playing.
	FMediaPlateEditorModule* EditorModule = FModuleManager::LoadModulePtr<FMediaPlateEditorModule>("MediaPlateEditor");
	if (EditorModule != nullptr)
	{
		EditorModule->MediaPlateStartedPlayback(MediaPlate);
	}
}

/* FAssetEditorToolkit interface
 *****************************************************************************/

FString FMediaPlateEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT("WorkingWithMedia/IntegratingMedia/MediaFramework"));
}

void FMediaPlateEditorToolkit::OnClose()
{
	TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Close();
	}
}

void FMediaPlateEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MediaPlateEditor", "Media Plate Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// Details tab.
	InTabManager->RegisterTabSpawner(MediaPlateEditorToolkit::DetailsTabId, FOnSpawnTab::CreateSP(this, &FMediaPlateEditorToolkit::HandleTabManagerSpawnTab, MediaPlateEditorToolkit::DetailsTabId))
		.SetDisplayName(LOCTEXT("DetailsTabName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	// Media details tab.
	InTabManager->RegisterTabSpawner(MediaPlateEditorToolkit::MediaDetailsTabId, FOnSpawnTab::CreateSP(this, &FMediaPlateEditorToolkit::HandleTabManagerSpawnTab, MediaPlateEditorToolkit::MediaDetailsTabId))
		.SetDisplayName(LOCTEXT("MediaDetailsTabName", "Media Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	// Playlist tab.
	InTabManager->RegisterTabSpawner(MediaPlateEditorToolkit::PlaylistTabId, FOnSpawnTab::CreateSP(this, &FMediaPlateEditorToolkit::HandleTabManagerSpawnTab, MediaPlateEditorToolkit::PlaylistTabId))
		.SetDisplayName(LOCTEXT("PlaylistTabName", "Playlist"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	// Viewer tab.
	InTabManager->RegisterTabSpawner(MediaPlateEditorToolkit::ViewerTabId, FOnSpawnTab::CreateSP(this, &FMediaPlateEditorToolkit::HandleTabManagerSpawnTab, MediaPlateEditorToolkit::ViewerTabId))
		.SetDisplayName(LOCTEXT("PlayerTabName", "Player"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlateEditor.Tabs.Player"));
}

void FMediaPlateEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(MediaPlateEditorToolkit::ViewerTabId);
	InTabManager->UnregisterTabSpawner(MediaPlateEditorToolkit::PlaylistTabId);
	InTabManager->UnregisterTabSpawner(MediaPlateEditorToolkit::MediaDetailsTabId);
	InTabManager->UnregisterTabSpawner(MediaPlateEditorToolkit::DetailsTabId);
}

/* IToolkit interface
 *****************************************************************************/

FText FMediaPlateEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Media Plate Editor");
}

FName FMediaPlateEditorToolkit::GetToolkitFName() const
{
	return FName("MediaPlateEditor");
}

FLinearColor FMediaPlateEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

FString FMediaPlateEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MediaPlate ").ToString();
}

/* FGCObject interface
 *****************************************************************************/

void FMediaPlateEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaPlate);
}

/* FEditorUndoClient interface
*****************************************************************************/

void FMediaPlateEditorToolkit::PostUndo(bool bSuccess)
{
	// do nothing
}

void FMediaPlateEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

/* FMediaPlayerEditorToolkit implementation
 *****************************************************************************/

void FMediaPlateEditorToolkit::BindCommands()
{
	const FMediaPlateEditorCommands& Commands = FMediaPlateEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.CloseMedia,
		FExecuteAction::CreateLambda([this] { MediaPlate->GetMediaPlayer()->Close(); }),
		FCanExecuteAction::CreateLambda([this] { return !MediaPlate->GetMediaPlayer()->GetUrl().IsEmpty(); })
	);

	ToolkitCommands->MapAction(
		Commands.ForwardMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->GetMediaPlayer()->SetRate(GetForwardRate()); }),
		FCanExecuteAction::CreateLambda([this]{
			TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
			return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetForwardRate(), false);
		})
	);

	ToolkitCommands->MapAction(
		Commands.NextMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->Next(); }),
		FCanExecuteAction::CreateLambda([this]{
			return (MediaPlate->MediaPlaylist != nullptr) &&
				(MediaPlate->MediaPlaylist->Num() > 1);
		})
	);

	ToolkitCommands->MapAction(
		Commands.OpenMedia,
		FExecuteAction::CreateLambda([this] { MediaPlate->Open(); }),
		FCanExecuteAction::CreateLambda([this] { return true; })
	);

	ToolkitCommands->MapAction(
		Commands.PauseMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->Pause(); }),
		FCanExecuteAction::CreateLambda([this]{
			TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
			return MediaPlayer->CanPause() && !MediaPlayer->IsPaused();
		})
	);

	ToolkitCommands->MapAction(
		Commands.PlayMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->GetMediaPlayer()->Play(); }),
		FCanExecuteAction::CreateLambda([this]{
			TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer(); 
			return MediaPlayer->IsReady() && (!MediaPlayer->IsPlaying() || (MediaPlayer->GetRate() != 1.0f));
		})
	);

	ToolkitCommands->MapAction(
		Commands.PreviousMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->Previous(); }),
		FCanExecuteAction::CreateLambda([this]{
			return (MediaPlate->MediaPlaylist != nullptr) &&
					(MediaPlate->MediaPlaylist->Num() > 1);
		})
	);

	ToolkitCommands->MapAction(
		Commands.ReverseMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->GetMediaPlayer()->SetRate(GetReverseRate()); } ),
		FCanExecuteAction::CreateLambda([this]{
			TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
			return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetReverseRate(), false);
		})
	);

	ToolkitCommands->MapAction(
		Commands.RewindMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlate->GetMediaPlayer()->Rewind(); }),
		FCanExecuteAction::CreateLambda([this]{
			TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer(); 
			return MediaPlate->GetMediaPlayer()->IsReady() && MediaPlayer->SupportsSeeking() && MediaPlayer->GetTime() > FTimespan::Zero();
		})
	);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMediaPlateEditorToolkit::ExtendToolBar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, const TSharedRef<FUICommandList> ToolkitCommands)
		{
			ToolbarBuilder.BeginSection("PlaybackControls");
			{
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().PreviousMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().RewindMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().ReverseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().PlayMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().PauseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().ForwardMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().NextMedia);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("MediaControls");
			{
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().OpenMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlateEditorCommands::Get().CloseMedia);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, GetToolkitCommands())
	);

	AddToolbarExtender(ToolbarExtender);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

float FMediaPlateEditorToolkit::GetForwardRate() const
{
	TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
	float Rate = MediaPlayer->GetRate();

	if (Rate < 1.0f)
	{
		Rate = 1.0f;
	}

	return 2.0f * Rate;
}

float FMediaPlateEditorToolkit::GetReverseRate() const
{
	TObjectPtr<UMediaPlayer> MediaPlayer = MediaPlate->GetMediaPlayer();
	float Rate = MediaPlayer->GetRate();

	if (Rate > -1.0f)
	{
		return -1.0f;
	}

	return 2.0f * Rate;
}

/* FMediaPlayerEditorToolkit callbacks
 *****************************************************************************/

TSharedRef<SDockTab> FMediaPlateEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
{
	UMediaPlayer* MediaPlayer = MediaPlate->GetMediaPlayer();
	UMediaTexture* MediaTexture = MediaPlate->GetMediaTexture();
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (TabIdentifier == MediaPlateEditorToolkit::DetailsTabId)
	{
		TabWidget = SNew(SMediaPlateEditorDetails, *MediaPlate, Style);
	}
	else if (TabIdentifier == MediaPlateEditorToolkit::MediaDetailsTabId)
	{
		TabWidget = SNew(SMediaPlateEditorMediaDetails, *MediaPlate);
	}
	else if (TabIdentifier == MediaPlateEditorToolkit::PlaylistTabId)
	{
		TabWidget = SNew(SMediaPlateEditorPlaylist, *MediaPlate, Style);
	}
	else if (TabIdentifier == MediaPlateEditorToolkit::ViewerTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorViewer, *MediaPlayer, MediaTexture, Style, false);
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
