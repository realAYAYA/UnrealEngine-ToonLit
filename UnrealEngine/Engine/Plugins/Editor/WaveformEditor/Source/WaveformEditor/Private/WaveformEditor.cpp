// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditor.h"

#include "AudioDevice.h"
#include "AssetDefinitionRegistry.h"
#include "Components/AudioComponent.h"
#include "EditorReimportHandler.h"
#include "Misc/TransactionObjectEvent.h"
#include "PropertyEditorModule.h"
#include "Sound/SoundWave.h"
#include "SparseSampledSequenceTransportCoordinator.h"
#include "STransformedWaveformViewPanel.h"
#include "Styling/AppStyle.h"
#include "SWaveformTransformationsOverlay.h"
#include "ToolMenus.h"
#include "TransformedWaveformView.h"
#include "TransformedWaveformViewFactory.h"
#include "WaveformEditorCommands.h"
#include "WaveformEditorDetailsCustomization.h"
#include "WaveformEditorLog.h"
#include "WaveformEditorSequenceDataProvider.h"
#include "WaveformEditorStyle.h"
#include "WaveformEditorToolMenuContext.h"
#include "WaveformEditorTransformationsSettings.h"
#include "WaveformEditorWaveWriter.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "WaveformEditor"

const FName FWaveformEditor::AppIdentifier("WaveformEditorApp");
const FName FWaveformEditor::PropertiesTabId("WaveformEditor_Properties");
const FName FWaveformEditor::TransformationsTabId("WaveformEditor_Transformations");
const FName FWaveformEditor::WaveformDisplayTabId("WaveformEditor_Display");
const FName FWaveformEditor::EditorName("Waveform Editor");
const FName FWaveformEditor::ToolkitFName("WaveformEditor");


bool FWaveformEditor::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, USoundWave* SoundWaveToEdit)
{
	checkf(SoundWaveToEdit, TEXT("Tried to open a Soundwave Editor from a null soundwave"));

	SoundWave = SoundWaveToEdit;

	AddDefaultTransformations();

	bool bIsInitialized = true;
	
	bIsInitialized &= CreateDetailsViews();
	bIsInitialized &= CreateTransportCoordinator();
	bIsInitialized &= InitializeZoom();
	bIsInitialized &= CreateWaveformView();
	bIsInitialized &= InitializeAudioComponent();
	bIsInitialized &= CreateTransportController();
	bIsInitialized &= CreateWaveWriter();
	bIsInitialized &= BindDelegates();
	bIsInitialized &= SetUpAssetReimport();

	bIsInitialized &= RegisterToolbar();
	bIsInitialized &= BindCommands();

	const TSharedRef<FTabManager::FLayout>  StandaloneDefaultLayout = SetupStandaloneLayout();

	GEditor->RegisterForUndo(this);

	if (bIsInitialized)
	{
		const bool bCreateDefaultStandaloneMenu = true;
		const bool bCreateDefaultToolbar = true;
		const bool bToolbarFocusable = false;
		const bool bUseSmallIcons = true;

		FAssetEditorToolkit::InitAssetEditor(
			Mode,
			InitToolkitHost,
			AppIdentifier,
			StandaloneDefaultLayout,
			bCreateDefaultStandaloneMenu,
			bCreateDefaultToolbar,
			SoundWaveToEdit,
			bToolbarFocusable,
			bUseSmallIcons);
	}

	return bIsInitialized;
}

FWaveformEditor::~FWaveformEditor()
{
	if (FReimportManager::Instance())
	{
		FReimportManager::Instance()->OnPostReimport().RemoveAll(this);
	}
}

void FWaveformEditor::AddDefaultTransformations()
{
	if (!ensure(SoundWave))
	{
		return;
	}

	if (SoundWave->Transformations.Num() == 0)
	{
		const UWaveformEditorTransformationsSettings* TransformationsSettings = GetWaveformEditorTransformationsSettings();
		for (const TSubclassOf<UWaveformTransformationBase>& TransformationClass : TransformationsSettings->LaunchTransformations)
		{
			if (TransformationClass)
			{
				EObjectFlags MaskedOuterFlags = SoundWave ? SoundWave->GetMaskedFlags(RF_PropagateToSubObjects) : RF_NoFlags;

				if (SoundWave->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
				{
					MaskedOuterFlags |= RF_ArchetypeObject;
				}

				UWaveformTransformationBase* TransformationToAdd = NewObject<UWaveformTransformationBase>(SoundWave, TransformationClass.Get(), NAME_None, MaskedOuterFlags);
				SoundWave->Transformations.Add(TransformationToAdd);
			}
			else
			{
				SoundWave->Transformations.Add(nullptr);
			}
		}

		TransformationChainConfig = SoundWave->UpdateTransformations();
	}
}

bool FWaveformEditor::InitializeAudioComponent()
{
	if (!ensure(SoundWave))
	{
		return false;
	}

	if (AudioComponent == nullptr)
	{
		if (FAudioDeviceManager* AudioDeviceManager = FAudioDeviceManager::Get())
		{
			if (FAudioDevice* AudioDevice = AudioDeviceManager->GetMainAudioDeviceRaw())
			{
				USoundBase* SoundBase = Cast<USoundBase>(SoundWave);
				AudioComponent = FAudioDevice::CreateComponent(SoundBase);
			}
		}

		if (AudioComponent == nullptr)
		{
			return false;
		}
	}

	AudioComponent->bAutoDestroy = false;
	AudioComponent->bIsUISound = true;
	AudioComponent->bAllowSpatialization = false;
	AudioComponent->bReverb = false;
	AudioComponent->bCenterChannelOnly = false;
	AudioComponent->bIsPreviewSound = true;

	return true;
}

bool FWaveformEditor::CreateTransportController()
{
	if (!ensure(AudioComponent))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup transport controls with a null audio component"));
		return false;
	}

	TransportController = MakeShared<FWaveformEditorTransportController>(AudioComponent);
	return TransportController != nullptr;
}

bool FWaveformEditor::InitializeZoom()
{
	ZoomManager = MakeShared<FWaveformEditorZoomController>();

	check(TransportCoordinator)
	ZoomManager->OnZoomRatioChanged.AddSP(TransportCoordinator.ToSharedRef(), &FSparseSampledSequenceTransportCoordinator::SetZoomRatio);

	return ZoomManager != nullptr;
}

bool FWaveformEditor::BindDelegates()
{
	if (!ensure(AudioComponent))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Failed to bind to playback percentage change, audio component is null"));
		return false;
	}

	AudioComponent->OnAudioPlaybackPercentNative.AddSP(this, &FWaveformEditor::HandlePlaybackPercentageChange);
	AudioComponent->OnAudioPlayStateChangedNative.AddSP(this, &FWaveformEditor::HandleAudioComponentPlayStateChanged);
	TransportCoordinator->OnFocusPointScrubUpdate.AddSP(this, &FWaveformEditor::HandlePlayheadScrub);
	return true;
}

bool FWaveformEditor::SetUpAssetReimport()
{
	if (!FReimportManager::Instance())
	{
		return false;
	}

	FReimportManager::Instance()->OnPostReimport().AddSP(this, &FWaveformEditor::OnAssetReimport);
	return true;
}

void FWaveformEditor::ExecuteReimport()
{
	if (!CanExecuteReimport())
	{
		return;
	}

	const bool bSelectNewAsset = ReimportMode == EWaveEditorReimportMode::SelectFile;

	TArray<UObject*> CopyOfSelectedAssets;
	CopyOfSelectedAssets.Add(SoundWave);
	FReimportManager::Instance()->ValidateAllSourceFileAndReimport(CopyOfSelectedAssets, true, -1, bSelectNewAsset);
}

void FWaveformEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_WaveformEditor", "Sound Wave Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_Properties))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(TransformationsTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_Transformations))
		.SetDisplayName(LOCTEXT("ProcessingTab", "Processing"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(WaveformDisplayTabId, FOnSpawnTab::CreateSP(this, &FWaveformEditor::SpawnTab_WaveformDisplay))
		.SetDisplayName(LOCTEXT("WaveformDisplayTab", "WaveformDisplay"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

}

void FWaveformEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(WaveformDisplayTabId);
}

bool FWaveformEditor::RegisterToolbar()
{
	const FName MenuName = FAssetEditorToolkit::GetToolMenuToolbarName();

	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		const FWaveformEditorCommands& Commands = FWaveformEditorCommands::Get();
		UToolMenu* ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, "AssetEditor.DefaultToolBar", EMultiBoxType::ToolBar);

		if (ToolBar == nullptr)
		{
			return false;
		}

		FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
		FToolMenuSection& PlayBackSection = ToolBar->AddSection("Transport Controls", TAttribute<FText>(), InsertAfterAssetSection);

		FToolMenuEntry PlayEntry = FToolMenuEntry::InitToolBarButton(
			Commands.PlaySoundWave,
			LOCTEXT("WaveformEditorPlayButton", ""),
			LOCTEXT("WaveformEditorPlayButtonTooltip", "Plays this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PlayInViewport")
		);

		PlayEntry.StyleNameOverride = FName("Toolbar.BackplateLeftPlay");
		
		FToolMenuEntry PauseEntry = FToolMenuEntry::InitToolBarButton(
			Commands.PauseSoundWave,
			LOCTEXT("WaveformEditorPauseButton", ""),
			LOCTEXT("WaveformEditorPauseButtonTooltip", "Pauses this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PausePlaySession.Small")
		);

		PauseEntry.StyleNameOverride = FName("Toolbar.BackplateCenter");

		FToolMenuEntry StopEntry = FToolMenuEntry::InitToolBarButton(
			Commands.StopSoundWave,
			LOCTEXT("WaveformEditorStopButton", ""),
			LOCTEXT("WaveformEditorStopButtonTooltip", "Stops this SoundWave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StopPlaySession.Small")
		);

		StopEntry.StyleNameOverride = FName("Toolbar.BackplateRight");

		PlayBackSection.AddEntry(PlayEntry);
		PlayBackSection.AddEntry(PauseEntry);
		PlayBackSection.AddEntry(StopEntry);


		FToolMenuInsert InsertAfterPlaybackSection("Transport Controls", EToolMenuInsertType::After);
		FToolMenuSection& ZoomSection = ToolBar->AddSection("Zoom Controls", TAttribute<FText>(), InsertAfterPlaybackSection);

		FToolMenuEntry ZoomInEntry = FToolMenuEntry::InitToolBarButton(
			Commands.ZoomIn,
			LOCTEXT("WaveformEditorZoomIn", ""),
			LOCTEXT("WaveformEditorZoomInButtonTooltip", "Zooms into the soundwave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus")
		);

		FToolMenuEntry ZoomOutEntry = FToolMenuEntry::InitToolBarButton(
			Commands.ZoomOut,
			LOCTEXT("WaveformEditorZoomOut", ""),
			LOCTEXT("WaveformEditorZoomOutButtonTooltip", "Zooms out the soundwave"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Minus")
		);

		ZoomSection.AddEntry(ZoomInEntry);
		ZoomSection.AddEntry(ZoomOutEntry);

		FToolMenuInsert InsertAfterZoomSection("Zoom Controls", EToolMenuInsertType::After);
		FToolMenuSection& ExportSection = ToolBar->AddSection("Export Controls", TAttribute<FText>(), InsertAfterZoomSection);


		ExportSection.AddDynamicEntry("ExportButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection) 
		{
			UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (Context && Context->WaveformEditor.IsValid())
			{
				FToolMenuEntry ExportEntry = FToolMenuEntry::InitToolBarButton(
					Commands.ExportWaveform,
					LOCTEXT("WaveformEditorRender", ""),
					TAttribute< FText >::CreateRaw(Context->WaveformEditor.Pin().Get(), &FWaveformEditor::GetExportButtonToolTip),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll")
				);

				InSection.AddEntry(ExportEntry);
			}
		}));

		ExportSection.AddDynamicEntry("ExportOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection) 
		{
				UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();
				
				if (Context && Context->WaveformEditor.IsValid())
				{
					FToolMenuEntry ExportOptionsEntry = FToolMenuEntry::InitComboButton(
						"ExportsOptionsCombo",
						FToolUIActionChoice(FUIAction()),
						FNewToolMenuChoice(FOnGetContent::CreateSP(Context->WaveformEditor.Pin().Get(), &FWaveformEditor::GenerateExportOptionsMenu)),
						LOCTEXT("ExportsOptions_Label", "Export Options"),
						LOCTEXT("ExportsOptions_ToolTip", "Export options for this waveform"),
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.ExportAll"),
						true
					);

					InSection.AddEntry(ExportOptionsEntry);
				}				
		}));

		ExportSection.AddDynamicEntry("ImportButton", FNewToolMenuSectionDelegate::CreateLambda([this, Commands](FToolMenuSection& InSection)
		{
			UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (Context && Context->WaveformEditor.IsValid())
			{
				FToolMenuEntry ReimportEntry = FToolMenuEntry::InitToolBarButton(
					Commands.ReimportAsset,
					LOCTEXT("WaveformEditorReimport", ""),
					TAttribute< FText >::CreateRaw(Context->WaveformEditor.Pin().Get(), &FWaveformEditor::GetReimportButtonToolTip),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"));

				InSection.AddEntry(ReimportEntry);
			}
		}));

		ExportSection.AddDynamicEntry("ReimportOptions", FNewToolMenuSectionDelegate::CreateLambda([this](FToolMenuSection& InSection) 
		{
			UWaveformEditorToolMenuContext* Context = InSection.FindContext<UWaveformEditorToolMenuContext>();

			if (Context && Context->WaveformEditor.IsValid())
			{
				FToolMenuEntry ReimportOptionsEntry = FToolMenuEntry::InitComboButton(
					"ReimportOptionsCombo",
					FToolUIActionChoice(FUIAction()),
					FNewToolMenuChoice(FOnGetContent::CreateSP(Context->WaveformEditor.Pin().Get(), &FWaveformEditor::GenerateImportOptionsMenu)),
					LOCTEXT("ReimportOptions_Label", "Reimport Options"),
					LOCTEXT("ReimportOptions_ToolTip", "Reimport options for this USoundWave"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
					true
				);

				InSection.AddEntry(ReimportOptionsEntry);
			}

		}));
	}

	return true;
}

TSharedRef<SWidget> FWaveformEditor::GenerateExportOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());
	
	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChannelSection_Label", "Export Channel Format"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ExportFormatMono);
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ExportFormatStereo);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FWaveformEditor::GenerateImportOptionsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, GetToolkitCommands());

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ReimportMode_Label", "Reimport Mode"));
	{
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ReimportModeSameFile);
		MenuBuilder.AddMenuEntry(FWaveformEditorCommands::Get().ReimportModeNewFile);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool FWaveformEditor::CanExecuteReimport() const
{
	if (FReimportManager::Instance() == nullptr)
	{
		return false;
	}

	return FReimportManager::Instance()->CanReimport(SoundWave);
}

bool FWaveformEditor::BindCommands()
{
	const FWaveformEditorCommands& Commands = FWaveformEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.PlaySoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Play),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanPressPlayButton));

	ToolkitCommands->MapAction(
		Commands.StopSoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Stop),
		FCanExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::CanStop));

	ToolkitCommands->MapAction(
		Commands.TogglePlayback,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::TogglePlayback));

	ToolkitCommands->MapAction(
		Commands.PauseSoundWave,
		FExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::Pause),
		FCanExecuteAction::CreateSP(TransportController.ToSharedRef(), &FWaveformEditorTransportController::IsPlaying));

	ToolkitCommands->MapAction(
		Commands.ZoomIn,
		FExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::ZoomIn),
		FCanExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::CanZoomIn));

	ToolkitCommands->MapAction(
		Commands.ZoomOut,
		FExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::ZoomOut),
		FCanExecuteAction::CreateSP(ZoomManager.ToSharedRef(), &FWaveformEditorZoomController::CanZoomOut));

	ToolkitCommands->MapAction(
		Commands.ExportWaveform,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ExportWaveform),
		FCanExecuteAction::CreateSP(WaveWriter.ToSharedRef(), &FWaveformEditorWaveWriter::CanCreateSoundWaveAsset));

	ToolkitCommands->MapAction(
		Commands.ExportFormatMono,
		FExecuteAction::CreateLambda([this] { (WaveWriter->SetExportChannelsFormat(WaveformEditorWaveWriter::EChannelFormat::Mono)); }),
		FCanExecuteAction::CreateLambda([this] {return WaveWriter.IsValid(); }),
		FIsActionChecked::CreateLambda([this] {return WaveWriter->GetExportChannelsFormat() == WaveformEditorWaveWriter::EChannelFormat::Mono; }));

	ToolkitCommands->MapAction(
		Commands.ExportFormatStereo,
		FExecuteAction::CreateLambda([this] { (WaveWriter->SetExportChannelsFormat(WaveformEditorWaveWriter::EChannelFormat::Stereo)); }),
		FCanExecuteAction::CreateLambda([this] {return WaveWriter.IsValid(); }),
		FIsActionChecked::CreateLambda([this] {return WaveWriter->GetExportChannelsFormat() == WaveformEditorWaveWriter::EChannelFormat::Stereo; }));

	ToolkitCommands->MapAction(
		Commands.ReimportAsset,
		FExecuteAction::CreateSP(this, &FWaveformEditor::ExecuteReimport),
		FCanExecuteAction::CreateSP(this, &FWaveformEditor::CanExecuteReimport));

	ToolkitCommands->MapAction(
		Commands.ReimportModeSameFile,
		FExecuteAction::CreateLambda([this] { ReimportMode = EWaveEditorReimportMode::SameFile; ExecuteReimport(); }),
		FCanExecuteAction::CreateLambda([this] { return CanExecuteReimport(); }),
		FIsActionChecked::CreateLambda([this] {return ReimportMode == EWaveEditorReimportMode::SameFile; }));

	ToolkitCommands->MapAction(
		Commands.ReimportModeNewFile,
		FExecuteAction::CreateLambda([this] { ReimportMode = EWaveEditorReimportMode::SelectFile; ExecuteReimport(); }),
		FCanExecuteAction::CreateLambda([this] { return CanExecuteReimport(); }),
		FIsActionChecked::CreateLambda([this] {return ReimportMode == EWaveEditorReimportMode::SelectFile; }));

	return true;
}


FName FWaveformEditor::GetEditorName() const
{
	return EditorName;
}

FName FWaveformEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FWaveformEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Waveform Editor");
}

FString FWaveformEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Waveform Editor").ToString();
}

FLinearColor FWaveformEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

void FWaveformEditor::OnAssetReimport(UObject* ReimportedObject, bool bSuccessfullReimport)
{
	if (!bSuccessfullReimport)
	{
		return;
	}

	if (ReimportedObject->IsA<USoundWave>() && ReimportedObject->GetPathName() == SoundWave->GetPathName())
	{
		CreateWaveformView();
		WaveformView.DataProvider->RequestSequenceView(TransportCoordinator->GetDisplayRange());
		WaveformView.ViewWidget->SetPlayheadRatio(TransportCoordinator->GetFocusPoint());
		this->TabManager->FindExistingLiveTab(WaveformDisplayTabId)->SetContent(WaveformView.ViewWidget.ToSharedRef());
	}
}

void FWaveformEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
{	
	TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* PropertyNode = PropertyThatChanged->GetActiveMemberNode();

	if (!PropertyNode)
	{
		return;
	}
	
	bool bIsTransformation = false;

	do 
	{
		bIsTransformation |= PropertyNode->GetValue()->GetName() == TEXT("Transformations");
		PropertyNode = PropertyNode->GetPrevNode();
	} while (PropertyNode != nullptr);

	if (!bIsTransformation)
	{
		return;
	}
	
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		bIsInteractingWithTransformations = true;

		if (TransportController->IsPlaying() || TransportController->IsPaused())
		{
			TransformInteractionPlayState = AudioComponent->GetPlayState();
			PlaybackTimeBeforeTransformInteraction = LastReceivedPlaybackPercent * (TransformationChainConfig.EndTime - TransformationChainConfig.StartTime);
			StartTimeBeforeTransformInteraction = TransformationChainConfig.StartTime;
			AudioComponent->Stop();
		}
	}

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		if (!TransportController->IsPlaying())
		{
			const float StartTimeDifference = StartTimeBeforeTransformInteraction - TransformationChainConfig.StartTime;
			const float AdjustedPlaybackTime = PlaybackTimeBeforeTransformInteraction + StartTimeDifference;

			switch (TransformInteractionPlayState)
			{
			case EAudioComponentPlayState::Playing:
				TransportController->Play(AdjustedPlaybackTime);
				TransformInteractionPlayState = EAudioComponentPlayState::Stopped;
				break;
			case EAudioComponentPlayState::Paused:
				TransportController->CacheStartTime(AdjustedPlaybackTime);
				TransformInteractionPlayState = EAudioComponentPlayState::Stopped;
			default:
				break;
			}
		}

		bIsInteractingWithTransformations = false;
	}

	const bool bUpdateTransformationChain = PropertyChangedEvent.GetPropertyName() == TEXT("Transformations");

	if (bUpdateTransformationChain)
	{
		WaveformView.DataProvider->GenerateLayersChain();
	}

	WaveformView.DataProvider->UpdateRenderElements();
	TransformationChainConfig = SoundWave->GetTransformationChainConfig();
}



void FWaveformEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		WaveformView.DataProvider->GenerateLayersChain();
		WaveformView.DataProvider->UpdateRenderElements();
	}
}

void FWaveformEditor::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

bool FWaveformEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	bool bShoouldUndo = false;
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
	{
		UObject* Object = TransactionObjectPair.Key;
		const UClass* ObjectClass = Object->GetClass();

		if (ObjectClass)
		{
			bShoouldUndo = ObjectClass->IsChildOf(UWaveformTransformationBase::StaticClass()) || ObjectClass->IsChildOf(USoundWave::StaticClass());
		}
	}

	return bShoouldUndo;
}

void FWaveformEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UWaveformEditorToolMenuContext* Context = NewObject<UWaveformEditorToolMenuContext>();
	Context->WaveformEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

bool FWaveformEditor::CreateDetailsViews()
{
	if (!ensure(SoundWave)) 
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup wav editor properties view from a null SoundWave"));
		return false;
	}

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;

	PropertiesDetails = PropertyModule.CreateDetailView(Args);
	PropertiesDetails->SetObject(SoundWave);

	TransformationsDetails = PropertyModule.CreateDetailView(Args);
	FOnGetDetailCustomizationInstance TransformationsDetailsCustomizationInstance = FOnGetDetailCustomizationInstance::CreateLambda([]() {
			return MakeShared<FWaveformTransformationsDetailsCustomization>();
		}
	);

	TransformationsDetails->RegisterInstancedCustomPropertyLayout(SoundWave->GetClass(), TransformationsDetailsCustomizationInstance);
	TransformationsDetails->SetObject(SoundWave);

	return true;
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_WaveformDisplay(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == WaveformDisplayTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("WaveformDisplayTitle", "Waveform Display"))
		[
			WaveformView.ViewWidget.ToSharedRef()
		];
}

const TSharedRef<FTabManager::FLayout> FWaveformEditor::SetupStandaloneLayout()
{
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_WaveformEditor_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(TransformationsTabId, ETabState::OpenedTab)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
					->SetForegroundTab(TransformationsTabId)

				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->SetHideTabWell(true)
					->AddTab(WaveformDisplayTabId, ETabState::OpenedTab)
				)
			)
		);

	return StandaloneDefaultLayout;
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PropertiesTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("SoundWaveDetailsTitle", "Details"))
		[
			PropertiesDetails.ToSharedRef()
		];
}

TSharedRef<SDockTab> FWaveformEditor::SpawnTab_Transformations(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TransformationsTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("SoundWaveProcessingTitle", "Processing"))
		[
			TransformationsDetails.ToSharedRef()
		];
}

bool FWaveformEditor::CreateWaveformView()
{
	if (!ensure(SoundWave))
	{
		UE_LOG(LogWaveformEditor, Warning, TEXT("Trying to setup waveform panel from a null SoundWave"));
		return false;
	}
	
	if (WaveformView.IsValid())
	{
		RemoveWaveformViewDelegates(*WaveformView.DataProvider, *WaveformView.ViewWidget);
	}

	WaveformView = FTransformedWaveformViewFactory::Get().GetTransformedView(SoundWave, TransportCoordinator.ToSharedRef(), this, ZoomManager);

	check(ZoomManager)

	BindWaveformViewDelegates(*WaveformView.DataProvider, *WaveformView.ViewWidget);

	ZoomManager->OnZoomRatioChanged.AddSP(TransportCoordinator.ToSharedRef(), &FSparseSampledSequenceTransportCoordinator::SetZoomRatio);
	TransportCoordinator->OnDisplayRangeUpdated.AddSP(this, &FWaveformEditor::HandleDisplayRangeUpdate);

	return WaveformView.IsValid();
}

void FWaveformEditor::BindWaveformViewDelegates(FWaveformEditorSequenceDataProvider& ViewDataProvider, STransformedWaveformViewPanel& ViewWidget)
{
	check(TransportCoordinator)

	ViewDataProvider.OnRenderElementsUpdated.AddSP(this, &FWaveformEditor::HandleRenderDataUpdate);
	TransportCoordinator->OnFocusPointMoved.AddSP(&ViewWidget, &STransformedWaveformViewPanel::SetPlayheadRatio);
}

void FWaveformEditor::RemoveWaveformViewDelegates(FWaveformEditorSequenceDataProvider& ViewDataProvider, STransformedWaveformViewPanel& ViewWidget)
{
	check(TransportCoordinator)

	ViewDataProvider.OnRenderElementsUpdated.RemoveAll(this);
	TransportCoordinator->OnFocusPointMoved.RemoveAll(&ViewWidget);
}

bool FWaveformEditor::CreateTransportCoordinator()
{
	TransportCoordinator = MakeShared<FSparseSampledSequenceTransportCoordinator>();
	TransportCoordinator->OnDisplayRangeUpdated.AddSP(this, &FWaveformEditor::HandleDisplayRangeUpdate);

	return TransportCoordinator != nullptr;
}

void FWaveformEditor::HandlePlaybackPercentageChange(const UAudioComponent* InComponent, const USoundWave* InSoundWave, const float InPlaybackPercentage)
{
	const bool bIsStopped = AudioComponent->GetPlayState() == EAudioComponentPlayState::Stopped;
	const bool bIsPaused = AudioComponent->GetPlayState() == EAudioComponentPlayState::Paused;
	const bool bPropagatePercentage = !bIsStopped && !bIsPaused;
	LastReceivedPlaybackPercent = InPlaybackPercentage;
	
	if (InComponent == AudioComponent && bPropagatePercentage)
	{
		if (TransportCoordinator.IsValid())
		{
			const float ClampedPlayBackPercentage = FGenericPlatformMath::Fmod(InPlaybackPercentage, 1.f);
			TransportCoordinator->SetProgressRatio(ClampedPlayBackPercentage);
		}
	}
}

void FWaveformEditor::HandleAudioComponentPlayStateChanged(const UAudioComponent* InAudioComponent, EAudioComponentPlayState NewPlayState)
{
	if (InAudioComponent != AudioComponent)
	{
		return;
	}

	switch (NewPlayState)
	{
	default:
		break;
	case EAudioComponentPlayState::Stopped:
		const bool bIgnoreStopState = TransportCoordinator->IsScrubbing() || bIsInteractingWithTransformations;

		if (!bIgnoreStopState)
		{
			TransportController->CacheStartTime(0.f);
			TransportCoordinator->Stop();
		}

		break;
	}
}

void FWaveformEditor::HandleRenderDataUpdate()
{
	if (TransportCoordinator != nullptr)
	{
		TransportCoordinator->UpdatePlaybackRange(WaveformView.DataProvider->GetTransformedWaveformBounds());
		WaveformView.DataProvider->RequestSequenceView(TransportCoordinator->GetDisplayRange());
	}
}

void FWaveformEditor::HandleDisplayRangeUpdate(const TRange<double> NewRange)
{
	WaveformView.DataProvider->RequestSequenceView(NewRange);
}

void FWaveformEditor::HandlePlayheadScrub(const float InTargetPlayBackRatio, const bool bIsMoving)
{
	if (bIsMoving)
	{
		if (TransportController->IsPlaying())
		{
			TransportController->Stop();
			bWasPlayingBeforeScrubbing = true;
		}
	}
	else
	{
		const float NewTime = InTargetPlayBackRatio * SoundWave->Duration;

		if (TransportController->IsPlaying())
		{
			TransportController->Seek(NewTime);
			return;
		}

		if (bWasPlayingBeforeScrubbing)
		{
			TransportController->Play(NewTime);
			bWasPlayingBeforeScrubbing = false;
		}
		else
		{
			TransportController->CacheStartTime(NewTime);
		}

	}
}

void FWaveformEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SoundWave);
	Collector.AddReferencedObject(AudioComponent);
}

FString FWaveformEditor::GetReferencerName() const
{
	return TEXT("FWaveformEditor");
}

bool FWaveformEditor::CanPressPlayButton() const
{
	return TransportController->CanPlay() && (TransportController->IsPaused() || !TransportController->IsPlaying());
}

bool FWaveformEditor::CreateWaveWriter()
{
	if (!ensure(SoundWave))
	{
		return false;
	}

	WaveWriter = MakeShared<FWaveformEditorWaveWriter>(SoundWave);
	return WaveWriter != nullptr;
}

void FWaveformEditor::ExportWaveform()
{
	check(WaveWriter);
	WaveWriter->ExportTransformedWaveform();
}

const UWaveformEditorTransformationsSettings* FWaveformEditor::GetWaveformEditorTransformationsSettings() const
{
	const UWaveformEditorTransformationsSettings* WaveformEditorTransformationsSettings = GetDefault<UWaveformEditorTransformationsSettings>();
	check(WaveformEditorTransformationsSettings)

	return WaveformEditorTransformationsSettings;
}

FText FWaveformEditor::GetReimportButtonToolTip() const
{
	FText ReimportModeText;

	switch (ReimportMode)
	{
	case(EWaveEditorReimportMode::SelectFile):
		ReimportModeText = LOCTEXT("SelectFile", "Reimport from new file");
		break;
	case(EWaveEditorReimportMode::SameFile):
		ReimportModeText = LOCTEXT("SameFile", "Reimport from same file");
		break;
	default:
		static_assert(static_cast<int32>(EWaveEditorReimportMode::COUNT) == 2, "Possible missing switch case coverage for 'EWaveEditorReimportMode'");
		break;
	}

	return FText::Format(LOCTEXT("WaveformEditorReimportButtonTooltip", "{0}."), ReimportModeText);
}

FText FWaveformEditor::GetExportButtonToolTip() const
{

	FText ExportModeText;

	if (!WaveWriter)
	{
		return ExportModeText;
	}

	switch (WaveWriter->GetExportChannelsFormat())
	{
	case(WaveformEditorWaveWriter::EChannelFormat::Stereo):
		ExportModeText = LOCTEXT("ExportModeStereo", "stereo");
		break;
	case(WaveformEditorWaveWriter::EChannelFormat::Mono):
		ExportModeText = LOCTEXT("ExportModeMono", "mono");
		break;
	default:
		break;
	}

	return FText::Format(LOCTEXT("WaveformEditorExportButtonTooltip", "Exports the edited waveform to a {0} USoundWave asset."), ExportModeText);
}

#undef LOCTEXT_NAMESPACE
