// Copyright Epic Games, Inc. All Rights Reserved.

#include "UncontrolledChangelistsModule.h"

#include "CoreGlobals.h"
#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/Find.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/CoreDelegates.h"
#include "PackagesDialog.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "SourceControlPreferences.h"
#include "Styling/SlateTypes.h"
#include "UObject/ObjectSaveContext.h"
#include "Engine/AssetManager.h"

#define LOCTEXT_NAMESPACE "UncontrolledChangelists"

void FUncontrolledChangelistsModule::FStartupTask::DoWork()
{
	double StartTime = FPlatformTime::Seconds();
	UE_LOG(LogSourceControl, Log, TEXT("Uncontrolled asset enumeration started..."));

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	const bool bIncludeOnlyOnDiskAssets = true;
	AssetRegistry.GetAllAssets(Assets, bIncludeOnlyOnDiskAssets);
	for(const FAssetData& AssetData : Assets) 
	{ 
		if (IsEngineExitRequested())
		{
			break;
		}
		Owner->OnAssetAddedInternal(AssetData, AddedAssetsCache, true);
	}
	
	UE_LOG(LogSourceControl, Log, TEXT("Uncontrolled asset enumeration finished in %s seconds (Found %d uncontrolled assets)"), *FString::SanitizeFloat(FPlatformTime::Seconds() - StartTime), AddedAssetsCache.Num());
}

void FUncontrolledChangelistsModule::StartupModule()
{
	bIsEnabled = USourceControlPreferences::AreUncontrolledChangelistsEnabled();
	
	if (!IsEnabled())
	{
		return;
	}

	// Adds Default Uncontrolled Changelist if it is not already present.
	GetDefaultUncontrolledChangelistState();

	LoadState();

	OnObjectPreSavedDelegateHandle = FCoreUObjectDelegates::OnObjectPreSave.AddLambda([](UObject* InAsset, const FObjectPreSaveContext& InPreSaveContext) { Get().OnObjectPreSaved(InAsset, InPreSaveContext); });
	OnEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddLambda([]() { Get().OnEndFrame(); });

	// Create initial scan event object
	InitialScanEvent = MakeShared<FInitialScanEvent>();

	UAssetManager::CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateLambda([this, WeakScanEvent = InitialScanEvent->AsWeak()]()
	{
		// Weak here allows us to check if module as been shutdown before using [this]
		if(!WeakScanEvent.IsValid())
		{
			return;
		}

		StartupTask = MakeUnique<FAsyncTask<FStartupTask>>(this);
		StartupTask->StartBackgroundTask();

		InitialScanEvent = nullptr;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		OnAssetAddedDelegateHandle = AssetRegistry.OnAssetAdded().AddLambda([](const struct FAssetData& AssetData) { Get().OnAssetAdded(AssetData); });
	}));

}

void FUncontrolledChangelistsModule::ShutdownModule()
{
	// This will make sure callback for initial scan early outs if module was shutdown
	InitialScanEvent = nullptr;

	if (StartupTask)
	{
		StartupTask->EnsureCompletion();
		StartupTask = nullptr;
	}

	if (bIsStateDirty)
	{
		SaveState();
		bIsStateDirty = false;
	}

	FAssetRegistryModule* AssetRegistryModulePtr = static_cast<FAssetRegistryModule*>(FModuleManager::Get().GetModule(TEXT("AssetRegistry")));

	// Check in case AssetRegistry has already been shutdown.
	if (AssetRegistryModulePtr != nullptr)
	{
		AssetRegistryModulePtr->Get().OnAssetAdded().Remove(OnAssetAddedDelegateHandle);
	}

	FCoreUObjectDelegates::OnObjectPreSave.Remove(OnObjectPreSavedDelegateHandle);
	FCoreDelegates::OnEndFrame.Remove(OnEndFrameDelegateHandle);
}

bool FUncontrolledChangelistsModule::IsEnabled() const
{
	return bIsEnabled && ISourceControlModule::Get().GetProvider().UsesUncontrolledChangelists();
}

TArray<FUncontrolledChangelistStateRef> FUncontrolledChangelistsModule::GetChangelistStates() const
{
	TArray<FUncontrolledChangelistStateRef> UncontrolledChangelistStates;

	if (IsEnabled())
	{
		Algo::Transform(UncontrolledChangelistsStateCache, UncontrolledChangelistStates, [](const auto& Pair) { return Pair.Value; });
	}

	return UncontrolledChangelistStates;
}

bool FUncontrolledChangelistsModule::OnMakeWritable(const FString& InFilename)
{
	if (!IsEnabled())
	{
		return false;
	}

	AddedAssetsCache.Add(FPaths::ConvertRelativePathToFull(InFilename));
	return true;
}

bool FUncontrolledChangelistsModule::OnNewFilesAdded(const TArray<FString>& InFilenames)
{
	return AddToUncontrolledChangelist(InFilenames);
}

bool FUncontrolledChangelistsModule::OnSaveWritable(const FString& InFilename)
{
	return AddToUncontrolledChangelist({ InFilename });
}

bool FUncontrolledChangelistsModule::OnDeleteWritable(const FString& InFilename)
{
	return AddToUncontrolledChangelist({ InFilename });
}

bool FUncontrolledChangelistsModule::AddToUncontrolledChangelist(const TArray<FString>& InFilenames)
{
	if (!IsEnabled())
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FUncontrolledChangelistsModule::AddToUncontrolledChangelist);

	TArray<FString> FullPaths;
	FullPaths.Reserve(InFilenames.Num());
	Algo::Transform(InFilenames, FullPaths, [](const FString& Filename) { return FPaths::ConvertRelativePathToFull(Filename); });

	// Remove from reconcile cache
	for (const FString& FullPath : FullPaths)
	{
		AddedAssetsCache.Remove(FullPath);
	}

	return AddFilesToDefaultUncontrolledChangelist(FullPaths, FUncontrolledChangelistState::ECheckFlags::NotCheckedOut);
}

void FUncontrolledChangelistsModule::UpdateStatus()
{
	bool bHasStateChanged = false;

	if (!IsEnabled())
	{
		return;
	}

	for (FUncontrolledChangelistsStateCache::ElementType& Pair : UncontrolledChangelistsStateCache)
	{
		FUncontrolledChangelistsStateCache::ValueType& UncontrolledChangelistState = Pair.Value;
		bHasStateChanged |= UncontrolledChangelistState->UpdateStatus();
	}

	if (bHasStateChanged)
	{
		OnStateChanged();
	}
}

FText FUncontrolledChangelistsModule::GetReconcileStatus() const
{
	if (InitialScanEvent.IsValid())
	{
		return LOCTEXT("WaitForAssetRegistryStatus", "Waiting for Asset Registry initial scan...");
	}

	if (StartupTask && !StartupTask->IsDone())
	{
		return LOCTEXT("ProcessingAssetsStatus", "Processing assets...");
	}

	return FText::Format(LOCTEXT("ReconcileStatus", "Assets to check for reconcile: {0}"), FText::AsNumber(AddedAssetsCache.Num()));
}

bool FUncontrolledChangelistsModule::OnReconcileAssets()
{
	FScopedSlowTask Scope(0, LOCTEXT("ProcessingAssetsProgress", "Processing assets"));
	const bool bShowCancelButton = false;
	const bool bAllowInPIE = false;
	Scope.MakeDialogDelayed(1.0f, bShowCancelButton, bAllowInPIE);

	if (StartupTask)
	{
		while (!StartupTask->WaitCompletionWithTimeout(0.016))
		{
			Scope.EnterProgressFrame(0.f);
		}
				
		AddedAssetsCache.Append(StartupTask->GetTask().GetAddedAssetsCache());
		StartupTask = nullptr;
	}

	if ((!IsEnabled()) || AddedAssetsCache.IsEmpty())
	{
		return false;
	}

	Scope.EnterProgressFrame(0.f, LOCTEXT("ReconcileAssetsProgress", "Reconciling assets"));

	CleanAssetsCaches();
	bool bHasStateChanged = AddFilesToDefaultUncontrolledChangelist(AddedAssetsCache.Array(), FUncontrolledChangelistState::ECheckFlags::All);

	AddedAssetsCache.Empty();

	return bHasStateChanged;
}

void FUncontrolledChangelistsModule::OnAssetAdded(const FAssetData& AssetData)
{
	if (!IsEnabled())
	{
		return;
	}

	OnAssetAddedInternal(AssetData, AddedAssetsCache, false);
}

void FUncontrolledChangelistsModule::OnAssetAddedInternal(const FAssetData& AssetData, TSet<FString>& InAddedAssetsCache, bool bInStartupTask)
{
	if (AssetData.HasAnyPackageFlags(PKG_Cooked))
	{
		return;
	}

	FPackagePath PackagePath;
	if (!FPackagePath::TryFromPackageName(AssetData.PackageName, PackagePath))
	{
		return;
	}

	// No need to check for existence when running startup task
	if (!bInStartupTask)
	{
		if (FPackageName::IsTempPackage(PackagePath.GetPackageName()))
		{
			return; // Ignore temp packages
		}

		if(!FPackageName::DoesPackageExist(PackagePath, &PackagePath))
		{
			return; // If the package does not exist on disk there is nothing more to do
		}
	}

	const FString LocalFullPath(PackagePath.GetLocalFullPath());

	if (LocalFullPath.IsEmpty())
	{
		return;
	}

	FString Fullpath = FPaths::ConvertRelativePathToFull(LocalFullPath);

	if (Fullpath.IsEmpty())
	{
		return;
	}

	if (ISourceControlModule::Get().GetProvider().UsesLocalReadOnlyState() && !IFileManager::Get().IsReadOnly(*Fullpath))
	{
		InAddedAssetsCache.Add(MoveTemp(Fullpath));
	}
}

static bool ExecuteRevertOperation(const TArray<FString>& InFilenames)
{
	ISourceControlModule& SourceControlModule = ISourceControlModule::Get();
	ISourceControlProvider& SourceControlProvider = SourceControlModule.GetProvider();
	TArray<FSourceControlStateRef> UpdatedFilestates;

	auto BuildFileString = [](const TArray<FString>& Files) -> FString
	{
		TStringBuilder<2048> Builder;
		Builder.Join(Files, TEXT(", "));
		return Builder.ToString();
	};

	if (SourceControlProvider.GetState(InFilenames, UpdatedFilestates, EStateCacheUsage::ForceUpdate) != ECommandResult::Succeeded)
	{
		UE_LOG(LogSourceControl, Error, TEXT("Failed to update the revision control files states for %s."), *BuildFileString(InFilenames));
		return false;
	}

	TArray<FString> FilesToDelete;
	TArray<FString> FilesToRevert;

	for (const FSourceControlStateRef& Filestate : UpdatedFilestates)
	{
		if (Filestate->IsSourceControlled())
		{
			FilesToRevert.Add(Filestate->GetFilename());
		}
		else
		{
			FilesToDelete.Add(Filestate->GetFilename());
		}
	}

	if (!FilesToRevert.IsEmpty())
	{
		TSharedRef<FSync> ForceSyncOperation = ISourceControlOperation::Create<FSync>();
		ForceSyncOperation->SetForce(true);
		ForceSyncOperation->SetLastSyncedFlag(true);

		if (SourceControlProvider.Execute(ForceSyncOperation, FilesToRevert) != ECommandResult::Succeeded)
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to sync the following files to a previous version: %s."), *BuildFileString(FilesToRevert));
			return false;
		}
	}

	IFileManager& FileManager = IFileManager::Get();
	bool bSuccess = true;

	for (const FString& FileToDelete : FilesToDelete)
	{
		const bool bRequireExists = true;
		const bool bEvenReadOnly = false;
		const bool bQuiet = false;

		if (!FileManager.Delete(*FileToDelete, bRequireExists, bEvenReadOnly, bQuiet))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Failed to delete %s."), *FileToDelete);
			bSuccess = false;
		}
	}

	SourceControlModule.GetOnFilesDeleted().Broadcast(FilesToDelete);

	return bSuccess;
}

bool FUncontrolledChangelistsModule::OnRevert(const TArray<FString>& InFilenames)
{
	bool bSuccess = false;

	if (!IsEnabled() || InFilenames.IsEmpty())
	{
		return true;
	}

	bSuccess = SourceControlHelpers::ApplyOperationAndReloadPackages(InFilenames, ExecuteRevertOperation);

	UpdateStatus();

	return bSuccess;
}

void FUncontrolledChangelistsModule::OnObjectPreSaved(UObject* InObject, const FObjectPreSaveContext& InPreSaveContext)
{
	if (!IsEnabled())
	{
		return;
	}

	// Make sure we are catching the top level asset object to avoid processing same package multiple times
	if (!InObject || !InObject->IsAsset())
	{
		return;
	}

	// Ignore procedural save and autosaves
	if (InPreSaveContext.IsProceduralSave() || ((InPreSaveContext.GetSaveFlags() & SAVE_FromAutosave) != 0))
	{
		return;
	}

	FString Fullpath = FPaths::ConvertRelativePathToFull(InPreSaveContext.GetTargetFilename());

	if (Fullpath.IsEmpty())
	{
		return;
	}

	AddedAssetsCache.Add(MoveTemp(Fullpath));
}

void FUncontrolledChangelistsModule::MoveFilesToUncontrolledChangelist(const TArray<FSourceControlStateRef>& InControlledFileStates, const TArray<FSourceControlStateRef>& InUncontrolledFileStates, const FUncontrolledChangelist& InUncontrolledChangelist)
{
	bool bHasStateChanged = false;

	if (!IsEnabled())
	{
		return;
	}

	FUncontrolledChangelistsStateCache::ValueType* ChangelistState = UncontrolledChangelistsStateCache.Find(InUncontrolledChangelist);

	if (ChangelistState == nullptr)
	{
		return;
	}

	TArray<FString> Filenames;
	Algo::Transform(InControlledFileStates, Filenames, [](const FSourceControlStateRef& State) { return State->GetFilename(); });

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOperation = ISourceControlOperation::Create<FRevert>();

	// Revert controlled files
	RevertOperation->SetSoftRevert(true);
	SourceControlProvider.Execute(RevertOperation, Filenames);

	// Removes selected Uncontrolled Files from their Uncontrolled Changelists
	for (const auto& Pair : UncontrolledChangelistsStateCache)
	{
		const FUncontrolledChangelistStateRef& UncontrolledChangelistState = Pair.Value;
		UncontrolledChangelistState->RemoveFiles(InUncontrolledFileStates);
	}

	Algo::Transform(InUncontrolledFileStates, Filenames, [](const FSourceControlStateRef& State) { return State->GetFilename(); });

	// Add all files to their UncontrolledChangelist
	bHasStateChanged = (*ChangelistState)->AddFiles(Filenames, FUncontrolledChangelistState::ECheckFlags::None);

	if (bHasStateChanged)
	{
		OnStateChanged();
	}
}

void FUncontrolledChangelistsModule::MoveFilesToUncontrolledChangelist(const TArray<FString>& InControlledFiles, const FUncontrolledChangelist& InUncontrolledChangelist)
{
	bool bHasStateChanged = false;

	if (!IsEnabled())
	{
		return;
	}

	FUncontrolledChangelistsStateCache::ValueType* ChangelistState = UncontrolledChangelistsStateCache.Find(InUncontrolledChangelist);

	if (ChangelistState == nullptr)
	{
		return;
	}

	const TArray<FString>& Filenames = InControlledFiles;

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TSharedRef<FRevert, ESPMode::ThreadSafe> RevertOperation = ISourceControlOperation::Create<FRevert>();

	// Revert controlled files
	RevertOperation->SetSoftRevert(true);
	SourceControlProvider.Execute(RevertOperation, Filenames);

	// Add all files to their UncontrolledChangelist
	bHasStateChanged = (*ChangelistState)->AddFiles(Filenames, FUncontrolledChangelistState::ECheckFlags::None);

	if (bHasStateChanged)
	{
		OnStateChanged();
	}
}

void FUncontrolledChangelistsModule::MoveFilesToControlledChangelist(const TArray<FSourceControlStateRef>& InUncontrolledFileStates, const FSourceControlChangelistPtr& InChangelist, TFunctionRef<bool(const TArray<FSourceControlStateRef>&)> InOpenConflictDialog)
{
	if (!IsEnabled())
	{
		return;
	}

	TArray<FString> UncontrolledFilenames;
	
	Algo::Transform(InUncontrolledFileStates, UncontrolledFilenames, [](const FSourceControlStateRef& State) { return State->GetFilename(); });
	MoveFilesToControlledChangelist(UncontrolledFilenames, InChangelist, InOpenConflictDialog);
}

void FUncontrolledChangelistsModule::MoveFilesToControlledChangelist(const TArray<FString>& InUncontrolledFiles, const FSourceControlChangelistPtr& InChangelist, TFunctionRef<bool(const TArray<FSourceControlStateRef>&)> InOpenConflictDialog)
{
	if (!IsEnabled())
	{
		return;
	}

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	TArray<FSourceControlStateRef> UpdatedFilestates;

	// Get updated filestates to check Checkout capabilities.
	SourceControlProvider.GetState(InUncontrolledFiles, UpdatedFilestates, EStateCacheUsage::ForceUpdate);

	TArray<FSourceControlStateRef> FilesConflicts;
	TArray<FString> FilesToAdd;
	TArray<FString> FilesToCheckout;
	TArray<FString> FilesToDelete;

	// Check if we can Checkout files or mark for add
	for (const FSourceControlStateRef& Filestate : UpdatedFilestates)
	{
		const FString& Filename = Filestate->GetFilename();

		if (!Filestate->IsSourceControlled())
		{
			FilesToAdd.Add(Filename);
		}
		else if (!IFileManager::Get().FileExists(*Filename))
		{
			FilesToDelete.Add(Filename);
		}
		else if (Filestate->CanCheckout())
		{
			FilesToCheckout.Add(Filename);
		}
		else
		{
			FilesConflicts.Add(Filestate);
			FilesToCheckout.Add(Filename);
		}
	}

	bool bCanProceed = true;

	// If we detected conflict, asking user if we should proceed.
	if (!FilesConflicts.IsEmpty())
	{
		bCanProceed = InOpenConflictDialog(FilesConflicts);
	}

	if (bCanProceed)
	{
		if (!FilesToCheckout.IsEmpty())
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), InChangelist, FilesToCheckout);
		}

		if (!FilesToAdd.IsEmpty())
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), InChangelist, FilesToAdd);
		}

		if (!FilesToDelete.IsEmpty())
		{
			SourceControlProvider.Execute(ISourceControlOperation::Create<FDelete>(), InChangelist, FilesToDelete);
		}

		// UpdateStatus so UncontrolledChangelists can remove files from their cache if they were present before checkout.
		UpdateStatus();
	}
}

TOptional<FUncontrolledChangelist> FUncontrolledChangelistsModule::CreateUncontrolledChangelist(const FText& InDescription)
{
	if (!IsEnabled())
	{
		return TOptional<FUncontrolledChangelist>();
	}

	// Default constructor will generate a new GUID.
	FUncontrolledChangelist NewUncontrolledChangelist;
	UncontrolledChangelistsStateCache.Emplace(NewUncontrolledChangelist, MakeShared<FUncontrolledChangelistState>(NewUncontrolledChangelist, InDescription));

	OnStateChanged();

	return NewUncontrolledChangelist;
}

void FUncontrolledChangelistsModule::EditUncontrolledChangelist(const FUncontrolledChangelist& InUncontrolledChangelist, const FText& InNewDescription)
{
	if (!IsEnabled())
	{
		return;
	}

	if (InUncontrolledChangelist.IsDefault())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot edit Default Uncontrolled Changelist."));
		return;
	}

	FUncontrolledChangelistStateRef* UncontrolledChangelistState = UncontrolledChangelistsStateCache.Find(InUncontrolledChangelist);

	if (UncontrolledChangelistState == nullptr)
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot find Uncontrolled Changelist %s in cache."), *InUncontrolledChangelist.ToString());
		return;
	}

	(*UncontrolledChangelistState)->SetDescription(InNewDescription);

	OnStateChanged();
}

void FUncontrolledChangelistsModule::DeleteUncontrolledChangelist(const FUncontrolledChangelist& InUncontrolledChangelist)
{
	if (!IsEnabled())
	{
		return;
	}

	if (InUncontrolledChangelist.IsDefault())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot delete Default Uncontrolled Changelist."));
		return;
	}

	FUncontrolledChangelistStateRef* UncontrolledChangelistState = UncontrolledChangelistsStateCache.Find(InUncontrolledChangelist);

	if (UncontrolledChangelistState == nullptr)
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot find Uncontrolled Changelist %s in cache."), *InUncontrolledChangelist.ToString());
		return;
	}

	if ((*UncontrolledChangelistState)->ContainsFiles())
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot delete Uncontrolled Changelist %s while it contains files."), *InUncontrolledChangelist.ToString());
		return;
	}

	// Get Deleted Offline files and move them to the Default UCL so that we don't lose them
	GetDefaultUncontrolledChangelistState()->AddFiles((*UncontrolledChangelistState)->GetDeletedOfflineFiles().Array(), FUncontrolledChangelistState::ECheckFlags::None);

	UncontrolledChangelistsStateCache.Remove(InUncontrolledChangelist);

	OnStateChanged();
}

void FUncontrolledChangelistsModule::OnStateChanged()
{
	bIsStateDirty = true;
}

void FUncontrolledChangelistsModule::OnEndFrame()
{
	if (StartupTask && StartupTask->IsDone())
	{
		AddedAssetsCache.Append(StartupTask->GetTask().GetAddedAssetsCache());
		StartupTask = nullptr;
	}

	if (bIsStateDirty)
	{
		OnUncontrolledChangelistModuleChanged.Broadcast();
		SaveState();
		bIsStateDirty = false;
	}
}

void FUncontrolledChangelistsModule::CleanAssetsCaches()
{
	// Remove files we are already tracking in Uncontrolled Changelists
	for (FUncontrolledChangelistsStateCache::ElementType& Pair : UncontrolledChangelistsStateCache)
	{
		FUncontrolledChangelistsStateCache::ValueType& UncontrolledChangelistState = Pair.Value;
		UncontrolledChangelistState->RemoveDuplicates(AddedAssetsCache);
	}
}

bool FUncontrolledChangelistsModule::AddFilesToDefaultUncontrolledChangelist(const TArray<FString>& InFilenames, const FUncontrolledChangelistState::ECheckFlags InCheckFlags)
{
	bool bHasStateChanged = false;

	FUncontrolledChangelistStateRef UncontrolledChangelistState = GetDefaultUncontrolledChangelistState();

	// Try to add files, they will be added only if they pass the required checks
	bHasStateChanged = UncontrolledChangelistState->AddFiles(InFilenames, InCheckFlags);

	if (bHasStateChanged)
	{
		OnStateChanged();
	}

	return bHasStateChanged;
}

FUncontrolledChangelistStateRef FUncontrolledChangelistsModule::GetDefaultUncontrolledChangelistState()
{
	FUncontrolledChangelist DefaultUncontrolledChangelist(FUncontrolledChangelist::DEFAULT_UNCONTROLLED_CHANGELIST_GUID);
	FUncontrolledChangelistStateRef* DefaultUncontrolledChangelistState = UncontrolledChangelistsStateCache.Find(DefaultUncontrolledChangelist);

	if (DefaultUncontrolledChangelistState != nullptr)
	{
		return *DefaultUncontrolledChangelistState;
	}

	return UncontrolledChangelistsStateCache.Emplace(MoveTemp(DefaultUncontrolledChangelist), MakeShared<FUncontrolledChangelistState>(DefaultUncontrolledChangelist, FUncontrolledChangelistState::DEFAULT_UNCONTROLLED_CHANGELIST_DESCRIPTION));
}

void FUncontrolledChangelistsModule::SaveState() const
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> UncontrolledChangelistsArray;

	RootObject->SetNumberField(VERSION_NAME, VERSION_NUMBER);

	for (const auto& Pair : UncontrolledChangelistsStateCache)
	{
		const FUncontrolledChangelist& UncontrolledChangelist = Pair.Key;
		FUncontrolledChangelistStateRef UncontrolledChangelistState = Pair.Value;
		TSharedPtr<FJsonObject> UncontrolledChangelistObject = MakeShareable(new FJsonObject);

		UncontrolledChangelist.Serialize(UncontrolledChangelistObject.ToSharedRef());
		UncontrolledChangelistState->Serialize(UncontrolledChangelistObject.ToSharedRef());

		UncontrolledChangelistsArray.Add(MakeShareable(new FJsonValueObject(UncontrolledChangelistObject)));
	}

	RootObject->SetArrayField(CHANGELISTS_NAME, UncontrolledChangelistsArray);

	using FStringWriter = TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;
	using FStringWriterFactory = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

	FString RootObjectStr;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&RootObjectStr);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FFileHelper::SaveStringToFile(RootObjectStr, *GetPersistentFilePath());
}

void FUncontrolledChangelistsModule::LoadState()
{
	FString ImportJsonString;
	TSharedPtr<FJsonObject> RootObject;
	uint32 VersionNumber = 0;
	const TArray<TSharedPtr<FJsonValue>>* UncontrolledChangelistsArray = nullptr;

	if (!FFileHelper::LoadFileToString(ImportJsonString, *GetPersistentFilePath()))
	{
		return;
	}

	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ImportJsonString);

	if (!FJsonSerializer::Deserialize(JsonReader, RootObject))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot deserialize RootObject."));
		return;
	}

	if (!RootObject->TryGetNumberField(VERSION_NAME, VersionNumber))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot get field %s."), VERSION_NAME);
		return;
	}

	if (VersionNumber > VERSION_NUMBER)
	{
		UE_LOG(LogSourceControl, Error, TEXT("Version number is invalid (file: %u, current: %u)."), VersionNumber, VERSION_NUMBER);
		return;
	}

	if (!RootObject->TryGetArrayField(CHANGELISTS_NAME, UncontrolledChangelistsArray))
	{
		UE_LOG(LogSourceControl, Error, TEXT("Cannot get field %s."), CHANGELISTS_NAME);
		return;
	}

	for (const TSharedPtr<FJsonValue>& JsonValue : *UncontrolledChangelistsArray)
	{
		FUncontrolledChangelist TempKey;
		TSharedRef<FJsonObject> JsonObject = JsonValue->AsObject().ToSharedRef();

		if (!TempKey.Deserialize(JsonObject))
		{
			UE_LOG(LogSourceControl, Error, TEXT("Cannot deserialize FUncontrolledChangelist."));
			continue;
		}

		FUncontrolledChangelistsStateCache::ValueType& UncontrolledChangelistState = UncontrolledChangelistsStateCache.FindOrAdd(MoveTemp(TempKey), MakeShared<FUncontrolledChangelistState>(TempKey));

		UncontrolledChangelistState->Deserialize(JsonObject);
	}

	UE_LOG(LogSourceControl, Display, TEXT("Uncontrolled Changelist persistency file loaded %s"), *GetPersistentFilePath());
}

FString FUncontrolledChangelistsModule::GetPersistentFilePath() const
{
	return FPaths::ProjectSavedDir() + TEXT("SourceControl/UncontrolledChangelists.json");
}

FString FUncontrolledChangelistsModule::GetUObjectPackageFullpath(const UObject* InObject) const
{
	FString Fullpath = TEXT("");

	if (InObject == nullptr)
	{
		return Fullpath;
	}

	const UPackage* Package = InObject->GetPackage();

	if (Package == nullptr)
	{
		return Fullpath;
	}

	const FString LocalFullPath(Package->GetLoadedPath().GetLocalFullPath());

	if (LocalFullPath.IsEmpty())
	{
		return Fullpath;
	}

	Fullpath = FPaths::ConvertRelativePathToFull(LocalFullPath);

	return Fullpath;
}

IMPLEMENT_MODULE(FUncontrolledChangelistsModule, UncontrolledChangelists);

#undef LOCTEXT_NAMESPACE
