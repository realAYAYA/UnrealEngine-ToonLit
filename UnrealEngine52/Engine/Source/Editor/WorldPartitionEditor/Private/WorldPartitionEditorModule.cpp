// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartitionEditorModule.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHLODsBuilder.h"
#include "WorldPartition/WorldPartitionMiniMapBuilder.h"
#include "WorldPartition/WorldPartitionLandscapeSplineMeshesBuilder.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/HLOD/HLODLayerAssetTypeActions.h"
#include "WorldPartition/SWorldPartitionEditor.h"
#include "WorldPartition/SWorldPartitionEditorGridSpatialHash.h"
#include "WorldPartition/Customizations/WorldPartitionDetailsCustomization.h"
#include "WorldPartition/Customizations/WorldPartitionHLODDetailsCustomization.h"
#include "WorldPartition/Customizations/WorldPartitionRuntimeSpatialHashDetailsCustomization.h"
#include "WorldPartition/SWorldPartitionConvertDialog.h"
#include "WorldPartition/WorldPartitionConvertOptions.h"
#include "WorldPartition/WorldPartitionEditorSettings.h"
#include "WorldPartition/HLOD/SWorldPartitionBuildHLODsDialog.h"

#include "LevelEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Engine/Level.h"

#include "Misc/MessageDialog.h"
#include "Misc/StringBuilder.h"
#include "Misc/ScopedSlowTask.h"
#include "Internationalization/Regex.h"

#include "Interfaces/IMainFrameModule.h"
#include "Widgets/SWindow.h"
#include "Commandlets/WorldPartitionConvertCommandlet.h"
#include "FileHelpers.h"
#include "ToolMenus.h"
#include "IContentBrowserSingleton.h"
#include "IDirectoryWatcher.h"
#include "DirectoryWatcherModule.h"
#include "ContentBrowserModule.h"
#include "EditorDirectories.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PropertyEditorModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Widgets/Docking/SDockTab.h"

#include "Styling/AppStyle.h"
#include "WorldPartition/ContentBundle/SContentBundleBrowser.h"
#include "Selection.h"
#include "WorldPartition/ContentBundle/ContentBundleEditorSubsystem.h"
#include "UObject/UObjectBase.h"
#include "WorldPartition/ContentBundle/ContentBundleEditor.h"

IMPLEMENT_MODULE( FWorldPartitionEditorModule, WorldPartitionEditor );

#define LOCTEXT_NAMESPACE "WorldPartition"

const FName WorldPartitionEditorTabId("WorldBrowserPartitionEditor");
const FName ContentBundleBrowserTabId("ContentBundleBrowser");

DEFINE_LOG_CATEGORY_STATIC(LogWorldPartitionEditor, All, All);

static void OnSelectedWorldPartitionVolumesToggleLoading(TArray<TWeakObjectPtr<AActor>> Volumes, bool bLoad)
{
	for (TWeakObjectPtr<AActor> Actor: Volumes)
	{
		if (Actor->Implements<UWorldPartitionActorLoaderInterface>())
		{
			IWorldPartitionActorLoaderInterface::ILoaderAdapter* LoaderAdapter = Cast<IWorldPartitionActorLoaderInterface>(Actor)->GetLoaderAdapter();

			if (bLoad)
			{
				LoaderAdapter->Load();
			}
			else
			{
				LoaderAdapter->Unload();
			}
		}
	}
}

static void CreateLevelViewportContextMenuEntries(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<AActor>> Volumes)
{
	MenuBuilder.BeginSection("WorldPartition", LOCTEXT("WorldPartition", "World Partition"));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("WorldPartitionLoad", "Load selected volumes"),
		LOCTEXT("WorldPartitionLoad_Tooltip", "Load selected volumes"),
		FSlateIcon(),
		FExecuteAction::CreateStatic(OnSelectedWorldPartitionVolumesToggleLoading, Volumes, true),
		NAME_None,
		EUserInterfaceActionType::Button);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("WorldPartitionUnload", "Unload selected volumes"),
		LOCTEXT("WorldPartitionUnload_Tooltip", "Load selected volumes"),
		FSlateIcon(),
		FExecuteAction::CreateStatic(OnSelectedWorldPartitionVolumesToggleLoading, Volumes, false),
		NAME_None,
		EUserInterfaceActionType::Button);

	MenuBuilder.EndSection();
}

static TSharedRef<FExtender> OnExtendLevelEditorMenu(const TSharedRef<FUICommandList> CommandList, TArray<AActor*> SelectedActors)
{
	TSharedRef<FExtender> Extender(new FExtender());

	TArray<TWeakObjectPtr<AActor> > Volumes;
	for (AActor* Actor : SelectedActors)
	{
		if (Actor->Implements<UWorldPartitionActorLoaderInterface>())
		{
			Volumes.Add(Actor);
		}
	}

	if (Volumes.Num())
	{
		Extender->AddMenuExtension(
			"ActorTypeTools",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&CreateLevelViewportContextMenuEntries, Volumes));
	}

	return Extender;
}

void FWorldPartitionEditorModule::StartupModule()
{
	SWorldPartitionEditorGrid::RegisterPartitionEditorGridCreateInstanceFunc(NAME_None, &SWorldPartitionEditorGrid::CreateInstance);
	SWorldPartitionEditorGrid::RegisterPartitionEditorGridCreateInstanceFunc(TEXT("SpatialHash"), &SWorldPartitionEditorGridSpatialHash::CreateInstance);
	
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWorldPartitionEditorModule::RegisterMenus));	

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	HLODLayerAssetTypeActions = MakeShareable(new FHLODLayerAssetTypeActions);
	AssetTools.RegisterAssetTypeActions(HLODLayerAssetTypeActions.ToSharedRef());

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout("WorldPartition", FOnGetDetailCustomizationInstance::CreateStatic(&FWorldPartitionDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("WorldPartitionRuntimeSpatialHash", FOnGetDetailCustomizationInstance::CreateStatic(&FWorldPartitionRuntimeSpatialHashDetails::MakeInstance));
	PropertyEditor.RegisterCustomClassLayout("WorldPartitionHLOD", FOnGetDetailCustomizationInstance::CreateStatic(&FWorldPartitionHLODDetailsCustomization::MakeInstance));
}

void FWorldPartitionEditorModule::ShutdownModule()
{
	if (!IsRunningGame())
	{
		if (FLevelEditorModule* LevelEditorModule = FModuleManager::Get().GetModulePtr<FLevelEditorModule>("LevelEditor"))
		{
			LevelEditorModule->GetAllLevelViewportContextMenuExtenders().RemoveAll([=](const FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors& In) { return In.GetHandle() == LevelEditorExtenderDelegateHandle; });

			LevelEditorModule->OnRegisterTabs().RemoveAll(this);
			LevelEditorModule->OnRegisterLayoutExtensions().RemoveAll(this);

			if (LevelEditorModule->GetLevelEditorTabManager())
			{
				LevelEditorModule->GetLevelEditorTabManager()->UnregisterTabSpawner(WorldPartitionEditorTabId);
			}

		}

		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	// Unregister the HLODLayer asset type actions
	if (HLODLayerAssetTypeActions.IsValid())
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.UnregisterAssetTypeActions(HLODLayerAssetTypeActions.ToSharedRef());
		}
		HLODLayerAssetTypeActions.Reset();
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditor.UnregisterCustomClassLayout("WorldPartition");
	}
}

void FWorldPartitionEditorModule::RegisterMenus()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TArray<FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors>& MenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportContextMenuExtenders();

	LevelEditorModule.OnRegisterTabs().AddRaw(this, &FWorldPartitionEditorModule::RegisterWorldPartitionTabs);
	LevelEditorModule.OnRegisterLayoutExtensions().AddRaw(this, &FWorldPartitionEditorModule::RegisterWorldPartitionLayout);

	MenuExtenderDelegates.Add(FLevelEditorModule::FLevelViewportMenuExtender_SelectedActors::CreateStatic(&OnExtendLevelEditorMenu));
	LevelEditorExtenderDelegateHandle = MenuExtenderDelegates.Last().GetHandle();

	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->AddSection("WorldPartition", LOCTEXT("WorldPartition", "World Partition"));
	Section.AddEntry(FToolMenuEntry::InitMenuEntry(
		"WorldPartition",
		LOCTEXT("WorldPartitionConvertTitle", "Convert Level..."),
		LOCTEXT("WorldPartitionConvertTooltip", "Converts a Level to World Partition."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
		FUIAction(FExecuteAction::CreateRaw(this, &FWorldPartitionEditorModule::OnConvertMap))
	));
}

TSharedRef<SWidget> FWorldPartitionEditorModule::CreateWorldPartitionEditor()
{
	UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();
	return SNew(SWorldPartitionEditor).InWorld(EditorWorld);
}

TSharedRef<SWidget> FWorldPartitionEditorModule::CreateContentBundleBrowser()
{
	check(ContentBundleBrowser == nullptr);
	TSharedRef<SContentBundleBrowser> NewDataLayerBrowser = SNew(SContentBundleBrowser);
	ContentBundleBrowser = NewDataLayerBrowser;
	return NewDataLayerBrowser;
}

bool FWorldPartitionEditorModule::IsEditingContentBundle() const
{
	UContentBundleEditorSubsystem* ContentBundleEditorSubsystem = UContentBundleEditorSubsystem::Get();
	return ContentBundleEditorSubsystem && ContentBundleEditorSubsystem->IsEditingContentBundle();
}

bool FWorldPartitionEditorModule::IsEditingContentBundle(const FGuid& ContentBundleGuid) const
{
	UContentBundleEditorSubsystem* ContentBundleEditorSubsystem = UContentBundleEditorSubsystem::Get();
	return ContentBundleEditorSubsystem && ContentBundleEditorSubsystem->IsEditingContentBundle(ContentBundleGuid);
}

int32 FWorldPartitionEditorModule::GetPlacementGridSize() const
{
	// Currently shares setting with Foliage. Can be changed when exposed.
	return GetDefault<UWorldPartitionEditorSettings>()->InstancedFoliageGridSize;
}

int32 FWorldPartitionEditorModule::GetInstancedFoliageGridSize() const
{
	return GetDefault<UWorldPartitionEditorSettings>()->InstancedFoliageGridSize;
}

int32 FWorldPartitionEditorModule::GetMinimapLowQualityWorldUnitsPerPixelThreshold() const
{
	return GetDefault<UWorldPartitionEditorSettings>()->MinimapLowQualityWorldUnitsPerPixelThreshold;
}

bool FWorldPartitionEditorModule::GetDisableLoadingInEditor() const
{
	return GetDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor;
}

void FWorldPartitionEditorModule::SetDisableLoadingInEditor(bool bInDisableLoadingInEditor)
{
	GetMutableDefault<UWorldPartitionEditorSettings>()->bDisableLoadingInEditor = bInDisableLoadingInEditor;
}

bool FWorldPartitionEditorModule::GetDisablePIE() const
{
	return GetDefault<UWorldPartitionEditorSettings>()->bDisablePIE;
}

void FWorldPartitionEditorModule::SetDisablePIE(bool bInDisablePIE)
{
	GetMutableDefault<UWorldPartitionEditorSettings>()->bDisablePIE = bInDisablePIE;
}

bool FWorldPartitionEditorModule::GetDisableBugIt() const
{
	return GetDefault<UWorldPartitionEditorSettings>()->bDisableBugIt;
}

void FWorldPartitionEditorModule::SetDisableBugIt(bool bInDisableBugIt)
{
	GetMutableDefault<UWorldPartitionEditorSettings>()->bDisableBugIt = bInDisableBugIt;
}

void FWorldPartitionEditorModule::OnConvertMap()
{
	IContentBrowserSingleton& ContentBrowserSingleton = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
	
	FOpenAssetDialogConfig Config;
	Config.bAllowMultipleSelection = false;
	FString OutPathName;
	if (FPackageName::TryConvertFilenameToLongPackageName(FEditorDirectories::Get().GetLastDirectory(ELastDirectory::LEVEL), OutPathName))
	{
		Config.DefaultPath = OutPathName;
	}	
	Config.AssetClassNames.Add(UWorld::StaticClass()->GetClassPathName());

	TArray<FAssetData> Assets = ContentBrowserSingleton.CreateModalOpenAssetDialog(Config);
	if (Assets.Num() == 1)
	{
		ConvertMap(Assets[0].PackageName.ToString());
	}
}

static bool AskSaveDirtyPackages(const bool bSaveContentPackages)
{
	// Ask user to save dirty packages
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = true;
	const bool bFastSave = false;
	const bool bNotifyNoPackagesSaved = false;
	const bool bCanBeDeclined = false;
	return FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages, bFastSave, bNotifyNoPackagesSaved, bCanBeDeclined);
}

static bool UnloadCurrentMap(FString& MapPackageName)
{
	UPackage* WorldPackage = FindPackage(nullptr, *MapPackageName);

	// Make sure we handle the case where the world package was renamed on save (for temp world for example)
	if (WorldPackage)
	{
		MapPackageName = WorldPackage->GetLoadedPath().GetPackageName();
	}

	// Unload any loaded map
	if (!UEditorLoadingAndSavingUtils::NewBlankMap(/*bSaveExistingMap*/false))
	{
		return false;
	}

	return true;
}

static void RescanAssets(const FString& MapToScan)
{
	// Force a directory watcher tick for the asset registry to get notified of the changes
	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	DirectoryWatcherModule.Get()->Tick(-1.0f);

	// Force update
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FString> ExternalObjectsPaths = ULevel::GetExternalObjectsPaths(MapToScan);

	AssetRegistry.ScanModifiedAssetFiles({ MapToScan });
	AssetRegistry.ScanPathsSynchronous(ExternalObjectsPaths, true);
}

static void LoadMap(const FString& MapToLoad)
{
	FEditorFileUtils::LoadMap(MapToLoad);

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World || World->GetPackage()->GetLoadedPath().GetPackageName() != MapToLoad)
	{
		UE_LOG(LogWorldPartitionEditor, Error, TEXT("Failed to reopen world."));
	}
}

void FWorldPartitionEditorModule::RunCommandletAsExternalProcess(const FString& InCommandletArgs, const FText& InOperationDescription, int32& OutResult, bool& bOutCancelled, FString& OutCommandletOutput)
{
	OutResult = 0;
	bOutCancelled = false;
	OutCommandletOutput.Empty();

	FProcHandle ProcessHandle;

	FScopedSlowTask SlowTask(1.0f, InOperationDescription);
	SlowTask.MakeDialog(true);

	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	verify(FPlatformProcess::CreatePipe(ReadPipe, WritePipe));

	FString CurrentExecutableName = FPlatformProcess::ExecutablePath();

	// Try to provide complete Path, if we can't try with project name
	FString ProjectPath = FPaths::IsProjectFilePathSet() ? FPaths::GetProjectFilePath() : FApp::GetProjectName();
		
	TArray<FString> CommandletArgsArray;
	CommandletArgsArray.Add(TEXT("\"") + ProjectPath + TEXT('"'));
	CommandletArgsArray.Add(TEXT("-BaseDir=\"") + FString(FPlatformProcess::BaseDir()) + TEXT('"'));
	CommandletArgsArray.Add(TEXT("-Unattended"));
	CommandletArgsArray.Add(TEXT("-RunningFromUnrealEd"));
	CommandletArgsArray.Add(InCommandletArgs);

	OnExecuteCommandletEvent.Broadcast(CommandletArgsArray);

	FString Arguments;
	for (const FString& AdditionalArg : CommandletArgsArray)
	{
		Arguments += " ";
		Arguments += AdditionalArg;
	}
	
	UE_LOG(LogWorldPartitionEditor, Display, TEXT("Running commandlet: %s %s"), *CurrentExecutableName, *Arguments);

	uint32 ProcessID;
	const bool bLaunchDetached = true;
	const bool bLaunchHidden = true;
	const bool bLaunchReallyHidden = true;
	ProcessHandle = FPlatformProcess::CreateProc(*CurrentExecutableName, *Arguments, bLaunchDetached, bLaunchHidden, bLaunchReallyHidden, &ProcessID, 0, nullptr, WritePipe, ReadPipe);

	while (FPlatformProcess::IsProcRunning(ProcessHandle))
	{
		if (SlowTask.ShouldCancel() || GEditor->GetMapBuildCancelled())
		{
			bOutCancelled = true;
			FPlatformProcess::TerminateProc(ProcessHandle);
			break;
		}

		const FString LogString = FPlatformProcess::ReadPipe(ReadPipe);
		if (!LogString.IsEmpty())
		{
			OutCommandletOutput.Append(LogString);
		}

		// Parse output, look for progress indicator in the log (in the form "Display: [i / N] Msg...\n")
		const FRegexPattern LogProgressPattern(TEXT("Display:\\s\\[([0-9]+)\\s\\/\\s([0-9]+)\\]\\s(.+)?(?=\\.{3}$)"));
		FRegexMatcher Regex(LogProgressPattern, *LogString);
		while (Regex.FindNext())
		{
			// Update slow task progress & message
			SlowTask.CompletedWork = FCString::Atoi(*Regex.GetCaptureGroup(1));
			SlowTask.TotalAmountOfWork = FCString::Atoi(*Regex.GetCaptureGroup(2));
			SlowTask.DefaultMessage = FText::FromString(Regex.GetCaptureGroup(3));
		}

		SlowTask.EnterProgressFrame(0);
		FPlatformProcess::Sleep(0.1);
	}


	FPlatformProcess::GetProcReturnCode(ProcessHandle, &OutResult);

	UE_LOG(LogWorldPartitionEditor, Display, TEXT("#### Begin commandlet output ####\n%s"), *OutCommandletOutput);

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (OutResult == 0)
	{
		UE_LOG(LogWorldPartitionEditor, Display, TEXT("#### Commandlet Succeeded ####"));
	}
	else
	{
		UE_LOG(LogWorldPartitionEditor, Error, TEXT("#### Commandlet Failed ####"));
		UE_LOG(LogWorldPartitionEditor, Error, TEXT("%s %s"), *CurrentExecutableName, *Arguments);
		UE_LOG(LogWorldPartitionEditor, Error, TEXT("Return Code: %i"), OutResult);
	}
}

bool FWorldPartitionEditorModule::ConvertMap(const FString& InLongPackageName)
{
	if (ULevel::GetIsLevelPartitionedFromPackage(FName(*InLongPackageName)))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ConvertMapMsg", "Map is already using World Partition"));
		return true;
	}

	UWorldPartitionConvertOptions* DefaultConvertOptions = GetMutableDefault<UWorldPartitionConvertOptions>();
	DefaultConvertOptions->CommandletClass = GetDefault<UWorldPartitionEditorSettings>()->CommandletClass;
	DefaultConvertOptions->bInPlace = false;
	DefaultConvertOptions->bSkipStableGUIDValidation = false;
	DefaultConvertOptions->LongPackageName = InLongPackageName;

	TSharedPtr<SWindow> DlgWindow =
		SNew(SWindow)
		.Title(LOCTEXT("ConvertWindowTitle", "Convert Settings"))
		.ClientSize(SWorldPartitionConvertDialog::DEFAULT_WINDOW_SIZE)
		.SizingRule(ESizingRule::UserSized)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::FixedSize);

	TSharedRef<SWorldPartitionConvertDialog> ConvertDialog =
		SNew(SWorldPartitionConvertDialog)
		.ParentWindow(DlgWindow)
		.ConvertOptions(DefaultConvertOptions);

	DlgWindow->SetContent(ConvertDialog);

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	FSlateApplication::Get().AddModalWindow(DlgWindow.ToSharedRef(), MainFrameModule.GetParentWindow());

	if (ConvertDialog->ClickedOk())
	{
		// Ask user to save dirty packages
		if (!AskSaveDirtyPackages(/*bAskSaveContentPackages=*/false))
		{
			return false;
		}

		if (!UnloadCurrentMap(DefaultConvertOptions->LongPackageName))
		{
			return false;
		}

		const FString CommandletArgs = *DefaultConvertOptions->ToCommandletArgs();
		const FText OperationDescription = LOCTEXT("ConvertProgress", "Converting map to world partition...");
		
		int32 Result;
		bool bCancelled;
		FString CommandletOutput;
		RunCommandletAsExternalProcess(CommandletArgs, OperationDescription, Result, bCancelled, CommandletOutput);
		if (!bCancelled && Result == 0)
		{	
#if	PLATFORM_DESKTOP
			if (DefaultConvertOptions->bGenerateIni)
			{
				const FString PackageFilename = FPackageName::LongPackageNameToFilename(DefaultConvertOptions->LongPackageName);
				const FString PackageDirectory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(PackageFilename));
				FPlatformProcess::ExploreFolder(*PackageDirectory);
			}
#endif				
				
			FString MapToLoad = DefaultConvertOptions->LongPackageName;
			if (!DefaultConvertOptions->bInPlace)
			{
				MapToLoad += UWorldPartitionConvertCommandlet::GetConversionSuffix(DefaultConvertOptions->bOnlyMergeSubLevels);
			}

			RescanAssets(MapToLoad);
			LoadMap(MapToLoad);
		}
		else if (bCancelled)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ConvertMapCancelled", "Conversion cancelled!"));
		}
		else if(Result != 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ConvertMapFailed", "Conversion failed! See log for details."));
		}
	}

	return false;
}

bool IWorldPartitionEditorModule::RunBuilder(TSubclassOf<UWorldPartitionBuilder> InWorldPartitionBuilder, UWorld* InWorld)
{
	FRunBuilderParams Params;
	Params.BuilderClass = InWorldPartitionBuilder;
	Params.World = InWorld;
	return RunBuilder(Params);
}

bool FWorldPartitionEditorModule::RunBuilder(const FRunBuilderParams& InParams)
{
	// Ideally this should be improved to automatically register all builders & present their options in a consistent way...

	if (InParams.BuilderClass == UWorldPartitionHLODsBuilder::StaticClass())
	{
		return BuildHLODs(InParams);
	}
	
	if (InParams.BuilderClass == UWorldPartitionMiniMapBuilder::StaticClass())
	{
		return BuildMinimap(InParams);
	}

	if (InParams.BuilderClass == UWorldPartitionLandscapeSplineMeshesBuilder::StaticClass())
	{
		return BuildLandscapeSplineMeshes(InParams.World);
	}

	return Build(InParams);
}

bool FWorldPartitionEditorModule::BuildHLODs(const FRunBuilderParams& InParams)
{
	TSharedPtr<SWindow> DlgWindow =
		SNew(SWindow)
		.Title(LOCTEXT("BuildHLODsWindowTitle", "Build HLODs"))
		.ClientSize(SWorldPartitionBuildHLODsDialog::DEFAULT_WINDOW_SIZE)
		.SizingRule(ESizingRule::UserSized)
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::FixedSize);

	TSharedRef<SWorldPartitionBuildHLODsDialog> BuildHLODsDialog =
		SNew(SWorldPartitionBuildHLODsDialog)
		.ParentWindow(DlgWindow);

	DlgWindow->SetContent(BuildHLODsDialog);

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	FSlateApplication::Get().AddModalWindow(DlgWindow.ToSharedRef(), MainFrameModule.GetParentWindow());

	if (BuildHLODsDialog->GetDialogResult() != SWorldPartitionBuildHLODsDialog::DialogResult::Cancel)
	{
		FRunBuilderParams ParamsCopy(InParams);
		ParamsCopy.ExtraArgs = BuildHLODsDialog->GetDialogResult() == SWorldPartitionBuildHLODsDialog::DialogResult::BuildHLODs ? "-SetupHLODs -BuildHLODs -AllowCommandletRendering" : "-DeleteHLODs";
		ParamsCopy.OperationDescription = LOCTEXT("HLODBuildProgress", "Building HLODs...");

		return Build(ParamsCopy);
	}

	return false;
}


bool FWorldPartitionEditorModule::BuildMinimap(const FRunBuilderParams& InParams)
{
	FRunBuilderParams ParamsCopy(InParams);
	ParamsCopy.ExtraArgs = "-AllowCommandletRendering";
	ParamsCopy.OperationDescription = LOCTEXT("MinimapBuildProgress", "Building minimap...");
	return Build(ParamsCopy);
}

bool FWorldPartitionEditorModule::Build(const FRunBuilderParams& InParams)
{
	FRunBuilderParams ParamsCopy(InParams);
	OnPreExecuteCommandletEvent.Broadcast(ParamsCopy);

	// Ask user to save dirty packages
	if (!AskSaveDirtyPackages(/*bAskSaveContentPackages=*/true))
	{
		return false;
	}

	// Unload map if required
	FString MapPackage = ParamsCopy.World->GetPackage()->GetName();
	if (ParamsCopy.bUnloadMap && !UnloadCurrentMap(MapPackage))
	{
		return false;
	}

	// Close assets editors as the external UE process may try to update those same assets
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllAssetEditors();

	TStringBuilder<512> CommandletArgsBuilder;
	CommandletArgsBuilder.Append(MapPackage);
	CommandletArgsBuilder.Append(" -run=WorldPartitionBuilderCommandlet -Builder=");
	CommandletArgsBuilder.Append(ParamsCopy.BuilderClass->GetName());
		
	if (!ParamsCopy.ExtraArgs.IsEmpty())
	{
		CommandletArgsBuilder.Append(" ");
		CommandletArgsBuilder.Append(ParamsCopy.ExtraArgs);
	}

	const FText OperationDescription = ParamsCopy.OperationDescription.IsEmptyOrWhitespace() ? LOCTEXT("BuildProgress", "Building...") : ParamsCopy.OperationDescription;

	int32 Result;
	bool bCancelled;
	FString CommandletOutput;

	FString CommandletArgs(CommandletArgsBuilder.ToString());
	RunCommandletAsExternalProcess(CommandletArgs, OperationDescription, Result, bCancelled, CommandletOutput);

	RescanAssets(MapPackage);

	if (ParamsCopy.bUnloadMap)
	{
		LoadMap(MapPackage);
	}
	else
	{
		if (UWorldPartition* WorldPartition = ParamsCopy.World->GetWorldPartition())
		{
			WorldPartition->Update();
		}
	}

	if (bCancelled)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BuildCancelled", "Build cancelled!"));
	}
	else if (Result != 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("BuildFailed", "Build failed! See log for details."));
	}

	OnPostExecuteCommandletEvent.Broadcast();

	return !bCancelled && Result == 0;
}

bool FWorldPartitionEditorModule::BuildLandscapeSplineMeshes(UWorld* InWorld)
{
	if (!UWorldPartitionLandscapeSplineMeshesBuilder::RunOnInitializedWorld(InWorld))
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LandscapeSplineMeshesBuildFailed", "Landscape Spline Meshes build failed! See log for details."));
		return false;
	}
	return true;
}

TSharedRef<SDockTab> FWorldPartitionEditorModule::SpawnWorldPartitionTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> NewTab =
		SNew(SDockTab)
		.Label(NSLOCTEXT("LevelEditor", "WorldBrowserPartitionTabTitle", "World Partition"))
		[
			CreateWorldPartitionEditor()
		];

	WorldPartitionTab = NewTab;
	return NewTab;
}

TSharedRef<SDockTab> FWorldPartitionEditorModule::SpawnContentBundleTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> NewTab =
		SNew(SDockTab)
		.Label(NSLOCTEXT("LevelEditor", "ContentBundleTabTitle", "Content Bundles"))
		[
			CreateContentBundleBrowser()
		];

	ContentBundleTab = NewTab;
	return NewTab;
}

void FWorldPartitionEditorModule::RegisterWorldPartitionTabs(TSharedPtr<FTabManager> InTabManager)
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	const FSlateIcon WorldPartitionIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.WorldPartition");

	InTabManager->RegisterTabSpawner(WorldPartitionEditorTabId,
		FOnSpawnTab::CreateRaw(this, &FWorldPartitionEditorModule::SpawnWorldPartitionTab))
		.SetDisplayName(NSLOCTEXT("LevelEditorTabs", "WorldPartitionEditor", "World Partition Editor"))
		.SetTooltipText(NSLOCTEXT("LevelEditorTabs", "WorldPartitionEditorTooltipText", "Open the World Partition Editor."))
		.SetGroup(MenuStructure.GetLevelEditorWorldPartitionCategory())
		.SetIcon(WorldPartitionIcon);

	constexpr TCHAR PLACEHOLDER_ContentBundleIcon[] = TEXT("LevelEditor.Tabs.DataLayers"); // todo_ow: create placeholder icon for content bundle tab
	const FSlateIcon DataLayersIcon(FAppStyle::GetAppStyleSetName(), PLACEHOLDER_ContentBundleIcon);
	InTabManager->RegisterTabSpawner(ContentBundleBrowserTabId,
		FOnSpawnTab::CreateRaw(this, &FWorldPartitionEditorModule::SpawnContentBundleTab))
		.SetDisplayName(NSLOCTEXT("LevelEditorTabs", "LevelEditorContentBundleBrowser", "Content Bundles Outliner"))
		.SetTooltipText(NSLOCTEXT("LevelEditorTabs", "LevelEditorContentBundleBrowserTooltipText", "Open the Content Bundles Outliner."))
		.SetGroup(MenuStructure.GetLevelEditorWorldPartitionCategory())
		.SetIcon(DataLayersIcon);
}

void FWorldPartitionEditorModule::RegisterWorldPartitionLayout(FLayoutExtender& Extender)
{
	Extender.ExtendLayout(FTabId("LevelEditorSelectionDetails"), ELayoutExtensionPosition::After, FTabManager::FTab(WorldPartitionEditorTabId, ETabState::ClosedTab));
}

UWorldPartitionEditorSettings::UWorldPartitionEditorSettings()
{
	CommandletClass = UWorldPartitionConvertCommandlet::StaticClass();
	InstancedFoliageGridSize = 25600;
	MinimapLowQualityWorldUnitsPerPixelThreshold = 12800;
	bDisableLoadingInEditor = false;
	bDisablePIE = false;
	bDisableBugIt = false;
}

FString UWorldPartitionConvertOptions::ToCommandletArgs() const
{
	TStringBuilder<1024> CommandletArgsBuilder;
	CommandletArgsBuilder.Appendf(TEXT("-run=%s %s -AllowCommandletRendering"), *CommandletClass->GetName(), *LongPackageName);
	
	if (!bInPlace)
	{
		CommandletArgsBuilder.Append(TEXT(" -ConversionSuffix"));
	}

	if (bSkipStableGUIDValidation)
	{
		CommandletArgsBuilder.Append(TEXT(" -SkipStableGUIDValidation"));
	}

	if (bDeleteSourceLevels)
	{
		CommandletArgsBuilder.Append(TEXT(" -DeleteSourceLevels"));
	}
	
	if (bGenerateIni)
	{
		CommandletArgsBuilder.Append(TEXT(" -GenerateIni"));
	}
	
	if (bReportOnly)
	{
		CommandletArgsBuilder.Append(TEXT(" -ReportOnly"));
	}
	
	if (bVerbose)
	{
		CommandletArgsBuilder.Append(TEXT(" -Verbose"));
	}

	if (bOnlyMergeSubLevels)
	{
		CommandletArgsBuilder.Append(TEXT(" -OnlyMergeSubLevels"));
	}

	if (bSaveFoliageTypeToContentFolder)
	{
		CommandletArgsBuilder.Append(TEXT(" -FoliageTypePath=/Game/FoliageTypes"));
	}
	
	return CommandletArgsBuilder.ToString();
}

#undef LOCTEXT_NAMESPACE
