// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetFolderContextMenu.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Styling/AppStyle.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "AssetViewUtils.h"
//#include "ContentBrowserLog.h"
//#include "ContentBrowserSingleton.h"
//#include "ContentBrowserUtils.h"
#include "SourceControlWindows.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
//#include "Widgets/Colors/SColorPicker.h"
#include "Framework/Commands/GenericCommands.h"
//#include "NativeClassHierarchy.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
//#include "ContentBrowserCommands.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "IContentBrowserDataModule.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserDataMenuContexts.h"
#include "Stats/Stats.h"
#include "AssetContextMenuUtils.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

void FAssetFolderContextMenu::MakeContextMenu(UToolMenu* InMenu, const TArray<FString>& InSelectedPackagePaths)
{
	MakeContextMenu(InMenu, InSelectedPackagePaths, TArray<FString>());
}

void FAssetFolderContextMenu::MakeContextMenu(UToolMenu* InMenu, const TArray<FString>& InSelectedPackagePaths, const TArray<FString>& InSelectedPackages)
{
	SelectedPaths = InSelectedPackagePaths;
	SelectedPackages = InSelectedPackages;

	if (SelectedPaths.Num() > 0)
	{
		AddMenuOptions(InMenu);
	}
}

void FAssetFolderContextMenu::AddMenuOptions(UToolMenu* Menu)
{
	UContentBrowserDataMenuContext_FolderMenu* Context = Menu->FindContext<UContentBrowserDataMenuContext_FolderMenu>();
	checkf(Context, TEXT("Required context UContentBrowserDataMenuContext_FolderMenu was missing!"));

	// Cache any vars that are used in determining if you can execute any actions.
	// Useful for actions whose "CanExecute" will not change or is expensive to calculate.
	StartProcessCanExecuteVars();
	TWeakPtr<FAssetFolderContextMenu> WeakThisPtr = AsShared();
	Menu->Context.AddCleanup([WeakThisPtr]()
		{
			if (TSharedPtr<FAssetFolderContextMenu> SharedThis = WeakThisPtr.Pin())
			{
				SharedThis->StopProcessCanExecuteVars();
			}
		});

	if(Context->bCanBeModified)
	{
		// Bulk operations section //
		{
			FToolMenuSection& Section = Menu->AddSection("PathContextBulkOperations", LOCTEXT("AssetTreeBulkMenuHeading", "Bulk Operations") );

			// Fix Up Redirectors in Folder
			{
				FToolMenuEntry& Entry = Section.AddMenuEntry(
					"FixUpRedirectorsInFolder",
					LOCTEXT("FixUpRedirectorsInFolder", "Fix Up Redirectors"),
					LOCTEXT("FixUpRedirectorsInFolderTooltip", "Finds referencers to all redirectors in the selected items and resaves them if possible, then deletes any redirectors that had all their referencers fixed."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Adjust"),
					FUIAction(FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteFixUpRedirectorsInFolder))
					);
				Entry.InsertPosition = FToolMenuInsert("Delete", EToolMenuInsertType::After);
			}

			if (!SelectedPaths.IsEmpty() || !SelectedPackages.IsEmpty())
			{
				// Migrate Folder
				FToolMenuEntry& Entry = Section.AddMenuEntry(
					"MigrateFolder",
					LOCTEXT("MigrateFolder", "Migrate..."),
					LOCTEXT("MigrateFolderTooltip", "Copies assets found in the selection and their dependencies to another content folder."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.Migrate"),
					FUIAction( FExecuteAction::CreateSP( this, &FAssetFolderContextMenu::ExecuteMigrateFolder ) )
					);
				Entry.InsertPosition = FToolMenuInsert("FixUpRedirectorsInFolder", EToolMenuInsertType::After);
			}
		}

		// Source control section //
		{
			FToolMenuSection& Section = Menu->AddSection("PathContextSourceControl", LOCTEXT("AssetTreeSCCMenuHeading", "Source Control"));

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			if (SourceControlProvider.IsEnabled())
			{
				using namespace UE::ContentBrowserAssetDataSource::Private;

				// Check out
				AddAsyncMenuEntry(
					Section,
					"FolderSCCCheckOut",
					LOCTEXT("FolderSCCCheckOut", "Check Out"),
					LOCTEXT("FolderSCCCheckOutTooltip", "Checks out all assets from source control which are in this selection."),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteSCCCheckOut),
						FCanExecuteAction::CreateSP(this, &FAssetFolderContextMenu::CanExecuteSCCCheckOut)
					),
					FIsAsyncProcessingActive::CreateSP(this, &FAssetFolderContextMenu::IsProcessingSCCCheckOut)
				);

				// Open for Add
				AddAsyncMenuEntry(
					Section,
					"FolderSCCOpenForAdd",
					LOCTEXT("FolderSCCOpenForAdd", "Mark For Add"),
					LOCTEXT("FolderSCCOpenForAddTooltip", "Adds all assets to source control that are in this selection and not already added."),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteSCCOpenForAdd),
						FCanExecuteAction::CreateSP(this, &FAssetFolderContextMenu::CanExecuteSCCOpenForAdd)
					),
					FIsAsyncProcessingActive::CreateSP(this, &FAssetFolderContextMenu::IsProcessingSCCOpenForAdd)
				);

				// Check in
				AddAsyncMenuEntry(
					Section,
					"FolderSCCCheckIn",
					LOCTEXT("FolderSCCCheckIn", "Check In"),
					LOCTEXT("FolderSCCCheckInTooltip", "Checks in all assets to source control which are in this selection."),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteSCCCheckIn),
						FCanExecuteAction::CreateSP(this, &FAssetFolderContextMenu::CanExecuteSCCCheckIn)
					),
					FIsAsyncProcessingActive::CreateSP(this, &FAssetFolderContextMenu::IsProcessingSCCCheckIn)
				);

				// Sync
				Section.AddMenuEntry(
					"FolderSCCSync",
					LOCTEXT("FolderSCCSync", "Sync"),
					LOCTEXT("FolderSCCSyncTooltip", "Syncs all the assets in this selection to the latest version."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteSCCSync),
						FCanExecuteAction::CreateSP(this, &FAssetFolderContextMenu::CanExecuteSCCSync)
					)
				);
			}
			else
			{
				Section.AddMenuEntry(
					"FolderSCCConnect",
					LOCTEXT("FolderSCCConnect", "Connect To Source Control"),
					LOCTEXT("FolderSCCConnectTooltip", "Connect to source control to allow source control operations to be performed on content and levels."),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "MainFrame.ConnectToSourceControl"),
					FUIAction(
						FExecuteAction::CreateSP(this, &FAssetFolderContextMenu::ExecuteSCCConnect),
						FCanExecuteAction::CreateSP(this, &FAssetFolderContextMenu::CanExecuteSCCConnect)
					)
				);
			}
		}
	}
}

void FAssetFolderContextMenu::ExecuteMigrateFolder()
{
	const FString& SourcesPath = GetFirstSelectedPath();
	if ( ensure(SourcesPath.Len()) )
	{
		// @todo Make sure the asset registry has completed discovering assets, or else GetAssetsByPath() will not find all the assets in the folder! Add some UI to wait for this with a cancel button
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		if ( AssetRegistryModule.Get().IsLoadingAssets() )
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT( "MigrateFolderAssetsNotDiscovered", "You must wait until asset discovery is complete to migrate a folder" ));
			return;
		}

		// Get a list of package names for input into MigratePackages
		TArray<FAssetData> AssetDataList;
		TArray<FName> PackageNames;
		AssetViewUtils::GetAssetsInPaths(SelectedPaths, AssetDataList);
		for ( auto AssetIt = AssetDataList.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			// Don't collect the external packages here
			if (AssetIt->GetOptionalOuterPathName().IsNone())
			{
				PackageNames.Add((*AssetIt).PackageName);
			}
		}

		PackageNames.Append(SelectedPackages);

		// Load all the assets in the selected paths
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().MigratePackages( PackageNames );
	}
}

void FAssetFolderContextMenu::ExecuteFixUpRedirectorsInFolder()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Form a filter from the paths
	FARFilter Filter;
	Filter.bRecursivePaths = true;

	Filter.PackagePaths.Reserve(SelectedPaths.Num());
	for (const FString& Path : SelectedPaths)
	{
		Filter.PackagePaths.Emplace(*Path);
	}

	if (!SelectedPaths.IsEmpty())
	{
		Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	}
	
	// Query for a list of assets in the selected paths
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	Filter.Clear();

	Filter.PackageNames.Reserve(SelectedPackages.Num());
	for (const FString& PackageName : SelectedPackages)
	{
		Filter.PackageNames.Emplace(*PackageName);
	}

	if (!SelectedPackages.IsEmpty())
	{
		Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
	}

	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	if (AssetList.Num() == 0)
	{
		return;
	}
	
	TArray<FString> ObjectPaths;
	for (const FAssetData& Asset : AssetList)
	{
		ObjectPaths.Add(Asset.GetObjectPathString());
	}

	FScopedSlowTask SlowTask(3, LOCTEXT("FixupRedirectorsSlowTask", "Fixing up redirectors"));
	SlowTask.MakeDialog();

	TArray<UObject*> Objects;
	const bool bAllowedToPromptToLoadAssets = true;
	const bool bLoadRedirects = true;
	SlowTask.EnterProgressFrame(1, LOCTEXT("FixupRedirectors_LoadAssets", "Loading Assets..."));
	if (AssetViewUtils::LoadAssetsIfNeeded(ObjectPaths, Objects, bAllowedToPromptToLoadAssets, bLoadRedirects))
	{
		// Transform Objects array to ObjectRedirectors array
		TArray<UObjectRedirector*> Redirectors;
		for (UObject* Object : Objects)
		{
			Redirectors.Add(CastChecked<UObjectRedirector>(Object));
		}

		SlowTask.EnterProgressFrame(1, LOCTEXT("FixupRedirectors_FixupReferencers", "Fixing up referencers..."));
		// Load the asset tools module
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetToolsModule.Get().FixupReferencers(Redirectors);
	}
}

void FAssetFolderContextMenu::ExecuteSCCCheckOut()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);
	PackageNames.Append(SelectedPackages);

	TArray<UPackage*> PackagesToCheckOut;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		if ( FPackageName::DoesPackageExist(*PackageIt) )
		{
			// Since the file exists, create the package if it isn't loaded or just find the one that is already loaded
			// No need to load unloaded packages. It isn't needed for the checkout process
			UPackage* Package = CreatePackage(**PackageIt);
			PackagesToCheckOut.Add( CreatePackage(**PackageIt) );
		}
	}

	if ( PackagesToCheckOut.Num() > 0 )
	{
		// Update the source control status of all potentially relevant packages
		ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToCheckOut);

		// Now check them out
		FEditorFileUtils::CheckoutPackages(PackagesToCheckOut);
	}
}

void FAssetFolderContextMenu::ExecuteSCCOpenForAdd()
{
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);
	PackageNames.Append(SelectedPackages);

	TArray<FString> PackagesToAdd;
	TArray<UPackage*> PackagesToSave;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIt), EStateCacheUsage::Use);
		if ( SourceControlState.IsValid() && !SourceControlState->IsSourceControlled() )
		{
			PackagesToAdd.Add(*PackageIt);

			// Make sure the file actually exists on disk before adding it
			FString Filename;
			if ( !FPackageName::DoesPackageExist(*PackageIt, &Filename) )
			{
				UPackage* Package = FindPackage(NULL, **PackageIt);
				if ( Package )
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	if ( PackagesToAdd.Num() > 0 )
	{
		// If any of the packages are new, save them now
		if ( PackagesToSave.Num() > 0 )
		{
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			TArray<UPackage*> FailedPackages;
			const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave, &FailedPackages);
			if(FailedPackages.Num() > 0)
			{
				// don't try and add files that failed to save - remove them from the list
				for(auto FailedPackageIt = FailedPackages.CreateConstIterator(); FailedPackageIt; FailedPackageIt++)
				{
					PackagesToAdd.Remove((*FailedPackageIt)->GetName());
				}
			}
		}

		if ( PackagesToAdd.Num() > 0 )
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlHelpers::PackageFilenames(PackagesToAdd));
		}
	}
}

void FAssetFolderContextMenu::ExecuteSCCCheckIn()
{
	// Get a list of package names in the selected paths
	TArray<FString> PackageNames;
	GetPackageNamesInSelectedPaths(PackageNames);
	PackageNames.Append(SelectedPackages);

	// Form a list of loaded packages to prompt for save
	TArray<UPackage*> LoadedPackages;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		UPackage* Package = FindPackage(NULL, **PackageIt);
		if ( Package )
		{
			LoadedPackages.Add(Package);
		}
	}

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave( LoadedPackages, true, true );

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined );
	if ( bShouldProceed )
	{
		TArray<FString> PendingDeletePaths;
		for (const auto& Path : SelectedPaths)
		{
			PendingDeletePaths.Add(FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path + TEXT("/"))));
		}

		const bool bUseSourceControlStateCache = false;
		FSourceControlWindows::PromptForCheckin(bUseSourceControlStateCache, PackageNames, PendingDeletePaths);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.") );
		}
	}
}

void FAssetFolderContextMenu::ExecuteSCCSync() const
{
	AssetViewUtils::SyncPathsFromSourceControl(SelectedPaths);
	AssetViewUtils::SyncPackagesFromSourceControl(SelectedPackages);
}

void FAssetFolderContextMenu::ExecuteSCCConnect() const
{
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless);
}

bool FAssetFolderContextMenu::CanExecuteSCCCheckOut() const
{
	return bCanExecuteSCCCheckOut && (!SelectedPaths.IsEmpty() || !SelectedPackages.IsEmpty());
}

bool FAssetFolderContextMenu::CanExecuteSCCOpenForAdd() const
{
	return bCanExecuteSCCOpenForAdd && (!SelectedPaths.IsEmpty() || !SelectedPackages.IsEmpty());
}

bool FAssetFolderContextMenu::CanExecuteSCCCheckIn() const
{
	bool bUsesFileRevisions = true;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if (ISourceControlModule::Get().IsEnabled())
	{
		bUsesFileRevisions = SourceControlProvider.UsesFileRevisions();
	}

	return bCanExecuteSCCCheckIn && (!SelectedPaths.IsEmpty() || !SelectedPackages.IsEmpty()) && bUsesFileRevisions;
}

bool FAssetFolderContextMenu::CanExecuteSCCSync() const
{
	if (!SelectedPaths.IsEmpty() || !SelectedPackages.IsEmpty())
	{
		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		if (SourceControlProvider.IsEnabled() && SourceControlProvider.UsesFileRevisions())
		{
			return true;
		}
	}
	return false;
}

bool FAssetFolderContextMenu::CanExecuteSCCConnect() const
{
	return (!ISourceControlModule::Get().IsEnabled() || !ISourceControlModule::Get().GetProvider().IsAvailable()) && SelectedPaths.Num() > 0;
}

bool FAssetFolderContextMenu::IsProcessingSCCCheckOut() const
{
	return IsTickable() && !CanExecuteSCCCheckOut();
}

bool FAssetFolderContextMenu::IsProcessingSCCOpenForAdd() const
{
	return IsTickable() && !CanExecuteSCCOpenForAdd();
}

bool FAssetFolderContextMenu::IsProcessingSCCCheckIn() const
{
	return IsTickable() && !CanExecuteSCCCheckIn();
}


void FAssetFolderContextMenu::StartProcessCanExecuteVars()
{
	// Start Cache whether we can execute any of the source control commands
	StopProcessCanExecuteVars();

	bCanExecuteSCCCheckOut = false;
	bCanExecuteSCCOpenForAdd = false;
	bCanExecuteSCCCheckIn = false;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( SourceControlProvider.IsEnabled() && SourceControlProvider.IsAvailable() )
	{
		GetPackageNamesInSelectedPaths(PackageNamesToProcess);
		PackageNamesToProcess.Append(SelectedPackages);
		CurrentPackageIndex = 0;
		ProcessCanExecuteVars();
	}
}

void FAssetFolderContextMenu::GetPackageNamesInSelectedPaths(TArray<FString>& OutPackageNames) const
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Form a filter from the paths
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
	{
		const FString& Path = SelectedPaths[PathIdx];
		Filter.PackagePaths.Add(FName(*Path));
	}

	// Query for a list of assets in the selected paths
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	// Form a list of unique package names from the assets
	TSet<FName> UniquePackageNames;
	for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx)
	{
		UniquePackageNames.Add(AssetList[AssetIdx].PackageName);
	}

	// Add all unique package names to the output
	for ( auto PackageIt = UniquePackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		OutPackageNames.Add( (*PackageIt).ToString() );
	}
}

FString FAssetFolderContextMenu::GetFirstSelectedPath() const
{
	return SelectedPaths.Num() > 0 ? SelectedPaths[0] : TEXT("");
}

void FAssetFolderContextMenu::Tick(float DeltaTime)
{
	ProcessCanExecuteVars();
}

TStatId FAssetFolderContextMenu::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAssetFolderContextMenu, STATGROUP_Tickables);
}

void FAssetFolderContextMenu::ProcessCanExecuteVars()
{
	const int32 StartPackageIndex = CurrentPackageIndex;
	constexpr int32 MaxToProcessThisFrame = 500;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	while (PackageNamesToProcess.Num() > CurrentPackageIndex
		&& (CurrentPackageIndex - StartPackageIndex) <= MaxToProcessThisFrame)
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageNamesToProcess[CurrentPackageIndex]), EStateCacheUsage::Use);
		++CurrentPackageIndex;
		if (SourceControlState.IsValid())
		{
			if (!bCanExecuteSCCCheckOut && SourceControlState->CanCheckout())
			{
				bCanExecuteSCCCheckOut = true;
			}
			else if (!bCanExecuteSCCOpenForAdd && !SourceControlState->IsSourceControlled())
			{
				bCanExecuteSCCOpenForAdd = true;
			}
			else if (!bCanExecuteSCCCheckIn && SourceControlState->CanCheckIn())
			{
				bCanExecuteSCCCheckIn = true;
			}
		}

		if (bCanExecuteSCCCheckOut && bCanExecuteSCCOpenForAdd && bCanExecuteSCCCheckIn)
		{
			// All SCC options are available, no need to keep iterating
			StopProcessCanExecuteVars();
			break;
		}
	}

	if (PackageNamesToProcess.Num() <= CurrentPackageIndex)
	{
		// Everything was processed stop ticking
		StopProcessCanExecuteVars();
	}
}

void FAssetFolderContextMenu::StopProcessCanExecuteVars()
{
	PackageNamesToProcess.Empty();
}


#undef LOCTEXT_NAMESPACE
