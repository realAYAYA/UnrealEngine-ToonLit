// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_DataAsset.h"

#include "Editor.h"
#include "ClassViewerFilter.h"
#include "ContentBrowserMenuContexts.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ObjectTools.h"
#include "SDetailsDiff.h"
#include "SourceControlHelpers.h"
#include "SourceControlOperations.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Engine/Engine.h"
#include "ToolMenuSection.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "HAL/PlatformFileManager.h"
#include "UObject/Linker.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_DataAsset"

namespace MenuExtension_DataAsset
{
	class FNewNodeClassFilter : public IClassViewerFilter
	{
	public:
		FNewNodeClassFilter(UClass* InBaseClass)
			: BaseClass(InBaseClass)
		{
		}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			if (InClass != nullptr)
			{
				return InClass->IsChildOf(BaseClass);
			}
			return false;
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
		{
			return InUnloadedClassData->IsChildOf(BaseClass);
		}

	private:
		UClass* BaseClass;
	};

	bool IsChangeDataAssetClassVisible(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
			{
				if (SelectedItem.CanEdit())
				{
					return true;
				}
			}
		}
		return false;
	}

	void ExecuteChangeDataAssetClass(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			const FText TitleText = LOCTEXT("DataAsset_PickNewDataAssetClass", "Pick New DataAsset Class");
			FClassViewerInitializationOptions Options;
			Options.ClassFilters.Add(MakeShared<FNewNodeClassFilter>(UDataAsset::StaticClass()));
			UClass* OutNewDataAssetClass = nullptr;
			const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, OutNewDataAssetClass, UDataAsset::StaticClass());

			if (bPressedOk && OutNewDataAssetClass != nullptr)
			{
				TSet<FName> EditableAssets;
				{
					const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
					EditableAssets.Reserve(SelectedItems.Num());
					for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
					{
						if (SelectedItem.CanEdit())
						{
							EditableAssets.Add(SelectedItem.GetInternalPath());
						}
					}
				}
				ensure(!EditableAssets.IsEmpty());

				TArray<UDataAsset*> DataAssets = Context->LoadSelectedObjectsIf<UDataAsset>([&EditableAssets](const FAssetData& AssetData)
				{
					return EditableAssets.Contains(*AssetData.GetObjectPathString());
				});

				for (TWeakObjectPtr<UDataAsset> DataAssetPtr : DataAssets)
				{
					if (UDataAsset* OldDataAsset = DataAssetPtr.Get())
					{
						if (OldDataAsset && OldDataAsset->IsValidLowLevel())
						{
							FName ObjectName = OldDataAsset->GetFName();
							UObject* Outer = OldDataAsset->GetOuter();
							OldDataAsset->Rename(nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);

							UObject* NewDataAsset = NewObject<UObject>(Outer, OutNewDataAssetClass, ObjectName, OldDataAsset->GetFlags());

							// Migrate Data
							{
								UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyOptions;
								CopyOptions.bNotifyObjectReplacement = true;
								UEngine::CopyPropertiesForUnrelatedObjects(OldDataAsset, NewDataAsset, CopyOptions);
							}

							NewDataAsset->MarkPackageDirty();

							// Consolidate or "Replace" the old object with the new object for any living references.
							bool bShowDeleteConfirmation = false;
							TArray<UObject*> OldDataAssetArray({ (UObject*)OldDataAsset });
							ObjectTools::ConsolidateObjects(NewDataAsset, OldDataAssetArray, bShowDeleteConfirmation);
						}
					}
				}
			}
		}
	}
	
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UDataAsset::StaticClass());
	        
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateStatic([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("DataAsset_ChangeClass", "Convert to Different DataAsset Type");
					const TAttribute<FText> ToolTip = LOCTEXT("DataAsset_ChangeClassTip", "Change the class these Data Assets are subclassed from.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteChangeDataAssetClass);
					UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsChangeDataAssetClassVisible);

					InSection.AddMenuEntry("DataAsset_ChangeClass", Label, ToolTip, Icon, UIAction);
				}
			}));
		}));
	});
}


FText UAssetDefinition_DataAsset::GetAssetDisplayName(const FAssetData& AssetData) const
{
	static const FName NAME_RowStructure(TEXT("RowStructure"));
	
	if (AssetData.IsValid())
	{
		const FAssetDataTagMapSharedView::FFindTagResult RowStructureTag = AssetData.TagsAndValues.FindTag(NAME_RowStructure);
		if (RowStructureTag.IsSet())
		{
			// Handle full path names and deprecated short class names
			const FTopLevelAssetPath ClassPath = FAssetData::TryConvertShortClassNameToPathName(*RowStructureTag.GetValue(), ELogVerbosity::Log);

			if (const UScriptStruct* FoundStruct = UClass::TryFindTypeSlow<UScriptStruct>(ClassPath.ToString(), EFindFirstObjectOptions::ExactClass))
			{
				return FText::Format(LOCTEXT("DataTableWithRowType", "Data Table ({0})"), FoundStruct->GetDisplayNameText());
			}
		}
	}

	return FText::GetEmpty();
}

EAssetCommandResult UAssetDefinition_DataAsset::PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const
{
	if (DiffArgs.OldAsset == nullptr && DiffArgs.NewAsset == nullptr)
	{
		return EAssetCommandResult::Unhandled;
	}
	
	const TSharedRef<SDetailsDiff> DetailsDiff = SDetailsDiff::CreateDiffWindow(DiffArgs.OldAsset, DiffArgs.NewAsset, DiffArgs.OldRevision, DiffArgs.NewRevision, UDataAsset::StaticClass());
	// allow users to edit NewAsset if it's a local asset
	if (!FPackageName::IsTempPackage(DiffArgs.NewAsset->GetPackage()->GetName()))
	{
		DetailsDiff->SetOutputObject(DiffArgs.NewAsset);
	}
	return EAssetCommandResult::Handled;
}

bool UAssetDefinition_DataAsset::CanMerge() const
{
	return true;
}

struct ScopedMergeResolveTransaction
{
	ScopedMergeResolveTransaction(UObject* InManagedObject, EMergeFlags InFlags)
		: ManagedObject(InManagedObject)
		, Flags(InFlags)
	{
		if (Flags & MF_HANDLE_SOURCE_CONTROL)
		{
			UndoHandler = NewObject<UUndoableResolveHandler>();
			UndoHandler->SetFlags(RF_Transactional);
			UndoHandler->SetManagedObject(ManagedObject);
			
			TransactionNum = GEditor->BeginTransaction(LOCTEXT("ResolveMerge", "ResolveAutoMerge"));
			ensure(UndoHandler->Modify());
			ensure(ManagedObject->Modify());
		}
	}

	void Cancel()
	{
		bCanceled = true;
	}
	
	~ScopedMergeResolveTransaction()
	{
		if (Flags & MF_HANDLE_SOURCE_CONTROL)
		{
			if (!bCanceled)
			{
				UndoHandler->MarkResolved();
				GEditor->EndTransaction();
			}
			else
			{
				ManagedObject->GetPackage()->SetDirtyFlag(false);
				GEditor->CancelTransaction(TransactionNum);
			}
		}
	}

	UObject* ManagedObject;
	UUndoableResolveHandler* UndoHandler = nullptr;
	EMergeFlags Flags;
	int TransactionNum = 0;
	bool bCanceled = false;
};

static UPackage* LoadMergePackage(const FString& SCFile, const FString& Revision, const UPackage* LocalPackage)
{
	const FString FileWithRevision = SCFile + TEXT("#") + Revision;
	const TSharedRef<FDownloadFile, ESPMode::ThreadSafe> DownloadFileOperation = ISourceControlOperation::Create<FDownloadFile>(FPaths::DiffDir(), FDownloadFile::EVerbosity::Full);
	ISourceControlModule::Get().GetProvider().Execute(DownloadFileOperation, FileWithRevision, EConcurrency::Synchronous);
	const FString DownloadPath = FPaths::ConvertRelativePathToFull(FPaths::DiffDir() / FPaths::GetCleanFilename(FileWithRevision));
	FString CopyPath = DownloadPath;
	CopyPath.ReplaceInline(TEXT(".uasset"), TEXT(""));
	CopyPath.ReplaceCharInline('#', '-');
	CopyPath.ReplaceCharInline('.', '-');
	CopyPath = FPaths::CreateTempFilename(*FPaths::GetPath(CopyPath), *FPaths::GetBaseFilename(CopyPath), TEXT(".uasset"));

	if (FPlatformFileManager::Get().GetPlatformFile().CopyFile(*CopyPath, *DownloadPath))
	{
		return DiffUtils::LoadPackageForDiff(FPackagePath::FromLocalPath(CopyPath), LocalPackage->GetLoadedPath());
	}
	return nullptr;
}

EAssetCommandResult UAssetDefinition_DataAsset::Merge(const FAssetAutomaticMergeArgs& MergeArgs) const
{
	if (!ensure(MergeArgs.LocalAsset))
	{
		return EAssetCommandResult::Unhandled;
	}
	
	FAssetManualMergeArgs ManualMergeArgs;
	ManualMergeArgs.LocalAsset = MergeArgs.LocalAsset;
	ManualMergeArgs.ResolutionCallback = MergeArgs.ResolutionCallback;
	ManualMergeArgs.Flags = MergeArgs.Flags;
	const UPackage* LocalPackage = ManualMergeArgs.LocalAsset->GetPackage();
	
	
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	const TSharedRef<FUpdateStatus, ESPMode::ThreadSafe> UpdateStatusOperation = ISourceControlOperation::Create<FUpdateStatus>();
	UpdateStatusOperation->SetUpdateHistory(true);
	SourceControlProvider.Execute(UpdateStatusOperation, LocalPackage);
	
	// Get the SCC state
	const FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(LocalPackage, EStateCacheUsage::Use);

	// If we have an asset and its in SCC..
	if( SourceControlState.IsValid() && SourceControlState->IsSourceControlled() )
	{
		const ISourceControlState::FResolveInfo ResolveInfo = SourceControlState->GetResolveInfo();
		check(ResolveInfo.IsValid());
		
		if(UPackage* TempPackage = LoadMergePackage(ResolveInfo.RemoteFile, ResolveInfo.RemoteRevision, LocalPackage))
		{
			// Grab the old asset from that old package
			ManualMergeArgs.RemoteAsset = FindObject<UObject>(TempPackage, *ManualMergeArgs.LocalAsset->GetName());

			// Recovery for package names that don't match
			if (ManualMergeArgs.RemoteAsset == nullptr)
			{
				ManualMergeArgs.RemoteAsset = TempPackage->FindAssetInPackage();
			}
		}
		
		if(UPackage* TempPackage = LoadMergePackage(ResolveInfo.BaseFile, ResolveInfo.BaseRevision, LocalPackage))
		{
			// Grab the old asset from that old package
			ManualMergeArgs.BaseAsset = FindObject<UObject>(TempPackage, *ManualMergeArgs.LocalAsset->GetName());

			// Recovery for package names that don't match
			if (ManualMergeArgs.BaseAsset == nullptr)
			{
				ManualMergeArgs.BaseAsset = TempPackage->FindAssetInPackage();
			}
		}
	}

	// single asset merging is only supported for assets in a conflicted state in source control
	if (!ensure(ManualMergeArgs.BaseAsset && ManualMergeArgs.RemoteAsset && ManualMergeArgs.LocalAsset))
	{
		return Super::Merge(MergeArgs);
	}
	
	return Merge(ManualMergeArgs);
}


	
static TArray<uint8> SerializeToBinary(UObject* Object, UObject* Default = nullptr)
{
	TArray<uint8> Result;
	FObjectWriter ObjectWriter(Result);
	ObjectWriter.SetIsPersistent(true);
	Default = Default ? ToRawPtr(Default) : ToRawPtr(Object->GetClass()->ClassDefaultObject);
	Object->GetClass()->SerializeTaggedProperties( ObjectWriter, reinterpret_cast<uint8*>(Object), Default->GetClass(), reinterpret_cast<uint8*>(Default));
	return Result;
};

static void DeserializeFromBinary(UObject* Object, const TArray<uint8>& Data, UObject* Default = nullptr)
{
	FObjectReader ObjectReader(Data);
	ObjectReader.SetIsPersistent(true);
	Default = Default ? ToRawPtr(Default) : ToRawPtr(Object->GetClass()->ClassDefaultObject);
	Object->GetClass()->SerializeTaggedProperties(ObjectReader, reinterpret_cast<uint8*>(Object), Default->GetClass(), reinterpret_cast<uint8*>(Default));
};

EAssetCommandResult UAssetDefinition_DataAsset::Merge(const FAssetManualMergeArgs& MergeArgs) const
{
	auto NotifyResolution = [&MergeArgs](EAssetMergeResult Result)
	{
		FAssetMergeResults Results;
		Results.Result = Result;
		Results.MergedPackage = MergeArgs.LocalAsset->GetPackage();
		MergeArgs.ResolutionCallback.ExecuteIfBound(Results);
		return EAssetCommandResult::Handled;
	};
	
	TArray<UObject*> SubObjects;
	GetObjectsWithOuter(MergeArgs.LocalAsset, SubObjects);
	if (!ensure(SubObjects.IsEmpty()))
	{
		TArray<FString> SubObjectNames;
		for (const UObject* SubObject : SubObjects)
		{
			SubObjectNames.Add(SubObject->GetName());
		}
		UE_LOG(LogEngine, Warning, TEXT("Merge is not currently supported with sub-objects: [{0}]"), *FString::Join(SubObjectNames, TEXT(",")));
		
		FAssetMergeResults Results;
		Results.Result = EAssetMergeResult::Cancelled;
		Results.MergedPackage = MergeArgs.LocalAsset->GetPackage();
		MergeArgs.ResolutionCallback.ExecuteIfBound(Results);
		
		return NotifyResolution(EAssetMergeResult::Cancelled);
	}

	auto AutoMerge = [](UObject* HeadRevision, UObject* BranchA, UObject* BranchB, FName Name) -> UObject*
	{
		// get delta of BranchB relative to HeadRevision
		const TArray<uint8> Delta = SerializeToBinary(BranchB, HeadRevision);
		
		// apply delta on top of BranchA
		Name = MakeUniqueObjectName(nullptr, BranchA->GetClass(), Name, EUniqueObjectNameOptions::GloballyUnique);
		UObject* Merged = DuplicateObject(BranchA, nullptr, Name);
		DeserializeFromBinary(Merged, Delta, BranchA);
		return Merged;
	};

	// apply changes in different orders to come up with two possible merge options
	UObject* FavorRemote = AutoMerge(MergeArgs.BaseAsset,MergeArgs.LocalAsset, MergeArgs.RemoteAsset, TEXT("FavorRemote"));
	UObject* FavorLocal = AutoMerge(MergeArgs.BaseAsset,MergeArgs.RemoteAsset,MergeArgs.LocalAsset, TEXT("FavorLocal"));
	const TArray<uint8> FavorRemoteBinary = SerializeToBinary(FavorRemote, MergeArgs.BaseAsset);
	const TArray<uint8> FavorLocalBinary = SerializeToBinary(FavorLocal, MergeArgs.BaseAsset);

	// if both merge options are the same, we have no conflicts
	if (FavorRemoteBinary == FavorLocalBinary)
	{
		// copy changes over to the local asset
		ScopedMergeResolveTransaction(MergeArgs.LocalAsset, MergeArgs.Flags);
		DeserializeFromBinary(MergeArgs.LocalAsset, FavorLocalBinary);
		return NotifyResolution(EAssetMergeResult::Completed);
	}
	
	// conflicts detected. We need to ask the user to manually resolve them
	
	if (!(MergeArgs.Flags & MF_NO_GUI))
	{
		const TSharedRef<SDetailsDiff> DiffView = SDetailsDiff::CreateDiffWindow(FavorRemote, FavorLocal, {}, {}, UDataAsset::StaticClass());
		DiffView->SetOutputObject(MergeArgs.LocalAsset);

		FObjectReader ObjectReader(FavorLocalBinary);
		DiffView->RequestModifications(ObjectReader);
		DiffView->OnWindowClosedEvent.AddLambda([MergeArgs](TSharedRef<SDetailsDiff> DiffView)
		{
			// serialize the changes made by the user while the diff view was open
			TArray<uint8> DiffViewModifications;
			FObjectWriter ObjectWriter(DiffViewModifications);
			DiffView->GetModifications(ObjectWriter);
			
			// begin undoable transaction
			ScopedMergeResolveTransaction(MergeArgs.LocalAsset, MergeArgs.Flags);
			
			// apply manual changes made within the diff tool
			DeserializeFromBinary(MergeArgs.LocalAsset, DiffViewModifications);
			
			// notify caller that merge is complete
			FAssetMergeResults Results;
			Results.Result = EAssetMergeResult::Completed;
			Results.MergedPackage = MergeArgs.LocalAsset->GetPackage();
			MergeArgs.ResolutionCallback.ExecuteIfBound(Results);
		});
		
		return EAssetCommandResult::Handled;
	}
	
	return NotifyResolution(EAssetMergeResult::Cancelled);
}

void UUndoableResolveHandler::SetManagedObject(UObject* Object)
{
	ManagedObject = Object;
	const UPackage* Package = ManagedObject->GetPackage();
	const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
	
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	const FSourceControlStatePtr SourceControlState = Provider.GetState(Package, EStateCacheUsage::Use);
	const ISourceControlState::FResolveInfo ResolveInfo = SourceControlState->GetResolveInfo();
	BaseRevisionNumber = SourceControlState->GetResolveInfo().BaseRevision;
	if (const TSharedPtr<class ISourceControlRevision, ESPMode::ThreadSafe> CurrentRevision = SourceControlState->GetCurrentRevision())
	{
		CurrentRevisionNumber = FString::FromInt(CurrentRevision->GetRevisionNumber());
	}
	else
	{
		CurrentRevisionNumber = {};
	}
	CheckinIdentifier = SourceControlState->GetCheckInIdentifier();

	// save package and copy the package to a temp file so it can be reverted
	const FString BaseFilename = FPaths::GetBaseFilename(Filepath);
	BackupFilepath = FPaths::CreateTempFilename(*(FPaths::ProjectSavedDir()/TEXT("Temp")), *BaseFilename.Left(32));
	ensure(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*BackupFilepath, *Filepath));
}

void UUndoableResolveHandler::MarkResolved()
{
	const UPackage* Package = ManagedObject->GetPackage();
	const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
	
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	Provider.Execute(ISourceControlOperation::Create<FResolve>(), TArray{Filepath}, EConcurrency::Synchronous);
	bShouldBeResolved = true;
}

void UUndoableResolveHandler::PostEditUndo()
{
	if (bShouldBeResolved) // redo resolution
	{
		MarkResolved();
	}
	else // undo resolution
	{
		UPackage* Package = ManagedObject->GetPackage();
		const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
		
		if (BaseRevisionNumber.IsEmpty() || CurrentRevisionNumber.IsEmpty())
		{
			ensure(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Filepath, *BackupFilepath));
			return;
		}
		
		// to force the file to revert to it's pre-resolved state, we must revert, sync back to base revision,
		// apply the conflicting changes, then sync forward again.
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
		{
			const TSharedRef<FSync> SyncOperation = ISourceControlOperation::Create<FSync>();
			SyncOperation->SetRevision(BaseRevisionNumber);
			Provider.Execute(SyncOperation, Filepath, EConcurrency::Synchronous);
		}
		
		ResetLoaders(Package);
		Provider.Execute( ISourceControlOperation::Create<FRevert>(), Filepath, EConcurrency::Synchronous);

		{
			const TSharedRef<FCheckOut> CheckoutOperation = ISourceControlOperation::Create<FCheckOut>();
			Provider.Execute(CheckoutOperation, CheckinIdentifier, {Filepath}, EConcurrency::Synchronous);
		}

		ensure(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Filepath, *BackupFilepath));
		
		{
			const TSharedRef<FSync> SyncOperation = ISourceControlOperation::Create<FSync>();
			SyncOperation->SetRevision(CurrentRevisionNumber);
			Provider.Execute(SyncOperation, Filepath, EConcurrency::Synchronous);
		}

		Provider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), {Filepath}, EConcurrency::Asynchronous);
	}
	UObject::PostEditUndo();
}

#undef LOCTEXT_NAMESPACE
