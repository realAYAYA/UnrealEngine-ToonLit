// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakeRecorderModule.h"
#include "TakeRecorderSettings.h"
#include "TakeRecorderProjectSettingsCustomization.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "TakeRecorderCommands.h"
#include "TakeRecorderStyle.h"
#include "TakePresetActions.h"
#include "Recorder/TakeRecorder.h"

#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Features/IModularFeatures.h"
#include "ITakeRecorderDropHandler.h"
#include "ISettingsModule.h"
#include "TakeRecorderSettings.h"

#include "Widgets/STakeRecorderTabContent.h"
#include "Widgets/STakeRecorderCockpit.h"
#include "Widgets/STakeRecorderPanel.h"

#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorModule.h"
#include "SequencerSettings.h"
#include "TakeMetaData.h"
#include "MovieSceneTakeSettings.h"
#include "FileHelpers.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "SerializedRecorder.h"
#include "Misc/Attribute.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "TakeRecorderModule"

FName ITakeRecorderModule::TakeRecorderTabName = "TakeRecorder";
FText ITakeRecorderModule::TakeRecorderTabLabel = LOCTEXT("TakeRecorderTab_Label", "Take Recorder");

FName ITakeRecorderModule::TakesBrowserTabName = "TakesBrowser";
FText ITakeRecorderModule::TakesBrowserTabLabel = LOCTEXT("TakesBrowserTab_Label", "Takes Browser");
FName ITakeRecorderModule::TakesBrowserInstanceName = "TakesBrowser";

IMPLEMENT_MODULE(FTakeRecorderModule, TakeRecorder);

static TAutoConsoleVariable<int32> CVarTakeRecorderSaveRecordedAssetsOverride(
	TEXT("TakeRecorder.SaveRecordedAssetsOverride"),
	0,
	TEXT("0: Save recorded assets is based on user settings\n1: Override save recorded assets to always start on"),
	ECVF_Default);

FName ITakeRecorderDropHandler::ModularFeatureName("ITakeRecorderDropHandler");

TArray<ITakeRecorderDropHandler*> ITakeRecorderDropHandler::GetDropHandlers()
{
	return IModularFeatures::Get().GetModularFeatureImplementations<ITakeRecorderDropHandler>(ModularFeatureName);
}

namespace
{
	static TSharedRef<SDockTab> SpawnTakesBrowserTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		TSharedRef<SDockTab> NewTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

		FContentBrowserConfig ContentBrowserConfig;
		{
			ContentBrowserConfig.ThumbnailLabel =  EThumbnailLabel::ClassName ;
			ContentBrowserConfig.ThumbnailScale = 0.1f;
			ContentBrowserConfig.InitialAssetViewType = EAssetViewType::Column;
			ContentBrowserConfig.bShowBottomToolbar = true;
			ContentBrowserConfig.bCanShowClasses = true;
			ContentBrowserConfig.bUseSourcesView = true;
			ContentBrowserConfig.bExpandSourcesView = true;
			ContentBrowserConfig.bUsePathPicker = true;
			ContentBrowserConfig.bCanShowFilters = true;
			ContentBrowserConfig.bCanShowAssetSearch = true;
			ContentBrowserConfig.bCanShowFolders = true;
			ContentBrowserConfig.bCanShowRealTimeThumbnails = true;
			ContentBrowserConfig.bCanShowDevelopersFolder = true;
			ContentBrowserConfig.bCanShowLockButton = true;
			ContentBrowserConfig.bCanSetAsPrimaryBrowser = false;
		}

		IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
		TSharedRef<SWidget> NewBrowser = ContentBrowser.CreateContentBrowser( FTakeRecorderModule::TakesBrowserInstanceName, NewTab, nullptr );

		NewTab->SetContent( NewBrowser );

		FString TakesDir = FPaths::GetPath(FPaths::GetPath(GetDefault<UTakeRecorderProjectSettings>()->Settings.GetTakeAssetPath()));
		TArray<FString> TakesFolders;
		TakesFolders.Push(TakesDir);
		ContentBrowser.SyncBrowserToFolders(TakesFolders, true, false, FTakeRecorderModule::TakesBrowserInstanceName);

		return NewTab;
	}

	static TSharedRef<SDockTab> SpawnTakeRecorderTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		TSharedRef<STakeRecorderTabContent> Content = SNew(STakeRecorderTabContent);
		TSharedPtr<SDockTab> ContentTab = SNew(SDockTab)
			.Label(Content, &STakeRecorderTabContent::GetTitle)
			.TabRole(ETabRole::NomadTab)
			[
				Content
			];
		const TAttribute<const FSlateBrush*> TabIcon = TAttribute<const FSlateBrush*>::CreateLambda([Content]()
			{
				return Content->GetIcon();
			});
		ContentTab->SetTabIcon(TabIcon);
		return ContentTab.ToSharedRef();
	}

	static void RegisterLevelEditorLayout(FLayoutExtender& Extender)
	{
		Extender.ExtendArea("TopLevelArea",
			[](TSharedRef<FTabManager::FArea> InArea)
			{
				InArea->SplitAt(1, 
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.3f )
					->AddTab(ITakeRecorderModule::TakeRecorderTabName, ETabState::ClosedTab)
				);
			}
		);
	}

	static void RegisterTabImpl()
	{
		FTabSpawnerEntry& TabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ITakeRecorderModule::TakeRecorderTabName, FOnSpawnTab::CreateStatic(SpawnTakeRecorderTab));

		TabSpawner
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCinematicsCategory())
			.SetDisplayName(ITakeRecorderModule::TakeRecorderTabLabel)
			.SetTooltipText(LOCTEXT("TakeRecorderTab_Tooltip", "Open the main Take Recorder UI."))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "SequenceRecorder.TabIcon"));

		FTabSpawnerEntry& TBTabSpawner = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ITakeRecorderModule::TakesBrowserTabName, FOnSpawnTab::CreateStatic(SpawnTakesBrowserTab));

		TBTabSpawner
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCinematicsCategory())
			.SetDisplayName(ITakeRecorderModule::TakesBrowserTabLabel)
			.SetTooltipText(LOCTEXT("TakeBrowserTab_Tooltip", "Open the Take Browser UI"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.TabIcon"));
	}
	
	static void ModulesChangedCallback(FName ModuleName, EModuleChangeReason ReasonForChange)
	{
		static const FName LevelEditorModuleName(TEXT("LevelEditor"));
		if (ReasonForChange == EModuleChangeReason::ModuleLoaded && ModuleName == LevelEditorModuleName)
		{
			RegisterTabImpl();
		}
	}
}

void FTakeRecorderModule::RegisterMenus()
{
#if WITH_EDITOR
	if (!UToolMenus::IsToolMenuUIEnabled())
	{
		return;
	}

	FToolMenuOwnerScoped MenuOwner("TakeRecorder");
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* Menu = ToolMenus->ExtendMenu("ContentBrowser.AssetContextMenu.LevelSequence");
	if (!Menu)
	{
		return;
	}

	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry("TakeRecorderActions", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		UContentBrowserAssetContextMenuContext* Context = InSection.FindContext<UContentBrowserAssetContextMenuContext>();
		if (!Context)
		{
			return;
		}

		if (Context->SelectedAssets.Num() == 1 && Context->SelectedAssets[0].IsInstanceOf(ULevelSequence::StaticClass()))
		{
			if (ULevelSequence* LevelSequence = Cast<ULevelSequence>(Context->SelectedAssets[0].GetAsset()))
			{
				InSection.AddMenuEntry(
					"OpenInTakeRecorder_Label",
					LOCTEXT("OpenInTakeRecorder_Label", "Open in Take Recorder"),
					LOCTEXT("OpenInTakeRecorder_Tooltip", "Opens this level sequence asset in Take Recorder by copying its contents into the pending take"),
					FSlateIcon(FTakeRecorderStyle::StyleName, "TakeRecorder.TabIcon"),
					FExecuteAction::CreateLambda(
						[LevelSequence]
						{
							FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
							TSharedPtr<SDockTab> DockTab = LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(ITakeRecorderModule::TakeRecorderTabName);
							if (DockTab.IsValid())
							{
								TSharedRef<STakeRecorderTabContent> TabContent = StaticCastSharedRef<STakeRecorderTabContent>(DockTab->GetContent());

								// If this sequence has already been recorded, set it up for viewing, otherwise start recording from it.
								UTakeMetaData* TakeMetaData = LevelSequence->FindMetaData<UTakeMetaData>();
								if (!TakeMetaData || !TakeMetaData->Recorded())
								{
									TabContent->SetupForRecording(LevelSequence);
								}
								else
								{
									TabContent->SetupForViewing(LevelSequence);
								}
							}
						}
					)
				);
				InSection.AddMenuEntry(
					"RecordIntoTakeRecorder_Label",
					LOCTEXT("RecordWithTakeRecorder_Label", "Record with Take Recorder"),
					LOCTEXT("RecordWithTakeRecorder_Tooltip", "Opens this level sequence asset for recording into with Take Recorder"),
					FSlateIcon(FTakeRecorderStyle::StyleName, "TakeRecorder.TabIcon"),
					FExecuteAction::CreateLambda(
						[LevelSequence]
						{
							FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
							TSharedPtr<SDockTab> DockTab = LevelEditorModule.GetLevelEditorTabManager()->TryInvokeTab(ITakeRecorderModule::TakeRecorderTabName);
							if (DockTab.IsValid())
							{
								TSharedRef<STakeRecorderTabContent> TabContent = StaticCastSharedRef<STakeRecorderTabContent>(DockTab->GetContent());

								TabContent->SetupForRecordingInto(LevelSequence);
							}
						}
					)
				);
			}
		}
	}));
#endif // WITH_EDITOR
}

FTakeRecorderModule::FTakeRecorderModule()
	: SequencerSettings(nullptr)
{
}

UTakePreset* FTakeRecorderModule::GetPendingTake() const
{
	return FindObject<UTakePreset>(nullptr, TEXT("/Temp/TakeRecorder/PendingTake.PendingTake"));
}

void FTakeRecorderModule::StartupModule()
{
	FTakeRecorderStyle::Get();
	FTakeRecorderCommands::Register();

	RegisterDetailCustomizations();
	RegisterLevelEditorExtensions();
	RegisterAssetTools();
	RegisterSettings();
	RegisterSerializedRecorder();

#if WITH_EDITOR
	if (GIsEditor)
	{
		if (UToolMenus::TryGet())
		{
			RegisterMenus();
		}
		else
		{
			FCoreDelegates::OnPostEngineInit.AddRaw(this, &FTakeRecorderModule::RegisterMenus);
		}
	}

	if (GEditor)
	{
		GEditor->OnEditorClose().AddRaw(this, &FTakeRecorderModule::OnEditorClose);
	}
#endif
}

void FTakeRecorderModule::OnEditorClose()
{
	if (UTakeRecorder* ActiveRecorder = UTakeRecorder::GetActiveRecorder())
	{
		ActiveRecorder->Stop();
	}
}

void FTakeRecorderModule::ShutdownModule()
{
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->OnEditorClose().RemoveAll(this);
	}

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
#endif

	FTakeRecorderCommands::Unregister();

	UnregisterDetailCustomizations();
	UnregisterLevelEditorExtensions();
	UnregisterAssetTools();
	UnregisterSettings();
	UnregisterSerializedRecorder();
}

void FTakeRecorderModule::RegisterExternalObject(UObject* InExternalObject)
{
	ExternalObjects.Add(InExternalObject);
	ExternalObjectAddRemoveEvent.Broadcast(InExternalObject, true);
}

void FTakeRecorderModule::UnregisterExternalObject(UObject* InExternalObject)
{
	ExternalObjectAddRemoveEvent.Broadcast(InExternalObject, false);
	ExternalObjects.Remove(InExternalObject);
}

void FTakeRecorderModule::RegisterDetailCustomizations()
{
#if WITH_EDITOR

	if (GIsEditor)
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		ProjectSettingsName = UTakeRecorderProjectSettings::StaticClass()->GetFName();

		PropertyEditorModule.RegisterCustomClassLayout(ProjectSettingsName, FOnGetDetailCustomizationInstance::CreateLambda(&MakeShared<FTakeRecorderProjectSettingsCustomization>));
	}

#endif
}

void FTakeRecorderModule::UnregisterDetailCustomizations()
{
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		PropertyEditorModule->UnregisterCustomClassLayout(ProjectSettingsName);
	}
}

FDelegateHandle FTakeRecorderModule::RegisterSourcesMenuExtension(const FOnExtendSourcesMenu& InExtension)
{
	return SourcesMenuExtenderEvent.Add(InExtension);
}

void FTakeRecorderModule::UnregisterSourcesMenuExtension(FDelegateHandle Handle)
{
	SourcesMenuExtenderEvent.Remove(Handle);
}

void FTakeRecorderModule::RegisterSettingsObject(UObject* InSettingsObject)
{
	GetMutableDefault<UTakeRecorderProjectSettings>()->AdditionalSettings.Add(InSettingsObject);
}

void FTakeRecorderModule::RegisterLevelEditorExtensions()
{
#if WITH_EDITOR

	if (GIsEditor)
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		LevelEditorLayoutExtensionHandle = LevelEditorModule.OnRegisterLayoutExtensions().AddStatic(RegisterLevelEditorLayout);

		if (LevelEditorModule.GetLevelEditorTabManager())
		{
			RegisterTabImpl();
		}
		else
		{
			LevelEditorTabManagerChangedHandle = LevelEditorModule.OnTabManagerChanged().AddStatic(RegisterTabImpl);
		}

		if (!FModuleManager::Get().IsModuleLoaded("LevelEditor"))
		{
			ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddStatic(ModulesChangedCallback);
		}
	}

#endif
}

void FTakeRecorderModule::UnregisterLevelEditorExtensions()
{
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TakeRecorderTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TakesBrowserTabName);
	}
#endif

	if(FLevelEditorModule* LevelEditorModulePtr = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModulePtr->OnTabManagerChanged().Remove(LevelEditorTabManagerChangedHandle);
	}

	FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
}

void FTakeRecorderModule::RegisterAssetTools()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	{
		TakePresetActions = MakeShared<FTakePresetActions>();
		AssetTools.RegisterAssetTypeActions(TakePresetActions.ToSharedRef());
	}
}

void FTakeRecorderModule::UnregisterAssetTools()
{
	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		AssetToolsModule->Get().UnregisterAssetTypeActions(TakePresetActions.ToSharedRef());
	}
}

void FTakeRecorderModule::RegisterSettings()
{
	RegisterSettingsObject(GetMutableDefault<UMovieSceneTakeSettings>());

	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");

	SettingsModule.RegisterSettings("Project", "Plugins", "Take Recorder",
		LOCTEXT("ProjectSettings_Label", "Take Recorder"),
		LOCTEXT("ProjectSettings_Description", "Configure project-wide defaults for take recorder."),
		GetMutableDefault<UTakeRecorderProjectSettings>()
	);

	SettingsModule.RegisterSettings("Editor", "ContentEditors", "Take Recorder",
		LOCTEXT("UserSettings_Label", "Take Recorder"),
		LOCTEXT("UserSettings_Description", "Configure user-specific settings for take recorder."),
		GetMutableDefault<UTakeRecorderUserSettings>()
	);

	SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("TakeRecorderSequenceEditor"));

	GetMutableDefault<UTakeRecorderUserSettings>()->LoadConfig();
	const bool bSaveRecordedAssetsOverride = CVarTakeRecorderSaveRecordedAssetsOverride.GetValueOnGameThread() != 0;
	if (bSaveRecordedAssetsOverride)
	{
		GetMutableDefault<UTakeRecorderUserSettings>()->Settings.bSaveRecordedAssets = bSaveRecordedAssetsOverride;
	}

	SettingsModule.RegisterSettings("Editor", "ContentEditors", "TakeRecorderSequenceEditor",
		LOCTEXT("TakeRecorderSequenceEditorSettingsName", "Take Recorder Sequence Editor"),
		LOCTEXT("TakeRecorderSequenceEditorSettingsDescription", "Configure the look and feel of the Take Recorder Sequence Editor."),
		SequencerSettings);
}

void FTakeRecorderModule::UnregisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Take Recorder");
		SettingsModule->UnregisterSettings("Editor",  "ContentEditors", "Take Recorder");
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "TakeRecorderSequenceEditor");
	}
}

void FTakeRecorderModule::PopulateSourcesMenu(TSharedRef<FExtender> InExtender, UTakeRecorderSources* InSources)
{
	SourcesMenuExtenderEvent.Broadcast(InExtender, InSources);
}

void FTakeRecorderModule::RegisterSerializedRecorder()
{
	SerializedRecorder = MakeShared<FSerializedRecorder>();
	IModularFeatures::Get().RegisterModularFeature(FSerializedRecorder::ModularFeatureName, SerializedRecorder.Get());
}

void FTakeRecorderModule::UnregisterSerializedRecorder()
{
	IModularFeatures::Get().UnregisterModularFeature(FSerializedRecorder::ModularFeatureName, SerializedRecorder.Get());
}

void FTakeRecorderModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SequencerSettings)
	{
		Collector.AddReferencedObject(SequencerSettings);
	}
}

#undef LOCTEXT_NAMESPACE
