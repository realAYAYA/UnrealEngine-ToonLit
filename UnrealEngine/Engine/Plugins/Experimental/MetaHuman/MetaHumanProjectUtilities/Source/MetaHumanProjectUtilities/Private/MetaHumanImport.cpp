// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanImport.h"
#include "MetaHumanTypes.h"
#include "MetaHumanProjectUtilities.h"
#include "MetaHumanImportUI.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IProjectManager.h"
#include "Internationalization/Text.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PackageTools.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Linker.h"
#include "UObject/Object.h"
#include "UObject/SavePackage.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "MetaHumanImport"
DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanImport, Log, All)

namespace UE::MetaHumanImport::Private
{
	// Helper functions *************************************

	// Calculate which assets to add to the project, which to replace, which to update and which to skip
	FAssetOperationPaths DetermineAssetOperations(const TMap<FString, FMetaHumanAssetVersion>& SourceVersionInfo, const FImportPaths& ImportPaths, bool ForceUpdate)
	{
		FScopedSlowTask AssetScanProgress(SourceVersionInfo.Num(), FText::FromString(TEXT("Scanning existing assets")), true);
		AssetScanProgress.MakeDialog();
		static const FName MetaHumanAssetVersionKey = TEXT("MHAssetVersion");
		FAssetOperationPaths AssetOperations;

		for (const TTuple<FString, FMetaHumanAssetVersion>& SourceAssetInfo : SourceVersionInfo)
		{
			AssetScanProgress.EnterProgressFrame();
			// If there is no existing asset, we add it
			if (!IFileManager::Get().FileExists(*ImportPaths.GetDestinationFile(SourceAssetInfo.Key)))
			{
				AssetOperations.Add.Add(SourceAssetInfo.Key);
				continue;
			}

			// If we are doing a force update or the asset is unique to the MetaHuman we always replace it
			if (ForceUpdate || !SourceAssetInfo.Key.StartsWith(TEXT("MetaHumans/Common/")))
			{
				AssetOperations.Replace.Add(SourceAssetInfo.Key);
				continue;
			}

			// If the asset is part of the common assets, we only update it if the source asset has a greater version number
			// If the file has no metadata then we assume it is old and will update it.
			FString TargetVersion = TEXT("0.0");
			if (const UObject* Asset = LoadObject<UObject>(nullptr, *ImportPaths.GetDestinationAsset(SourceAssetInfo.Key)))
			{
				if (const TMap<FName, FString>* Metadata = UMetaData::GetMapForObject(Asset))
				{
					if (const FString* VersionMetaData = Metadata->Find(MetaHumanAssetVersionKey))
					{
						TargetVersion = *VersionMetaData;
					}
				}
			}

			const FMetaHumanAssetVersion OldVersion = FMetaHumanAssetVersion::FromString(TargetVersion);
			const FMetaHumanAssetVersion NewVersion = SourceAssetInfo.Value;
			if (NewVersion > OldVersion)
			{
				AssetOperations.Update.Add(SourceAssetInfo.Key);
				AssetOperations.UpdateReasons.Add({OldVersion, NewVersion});
			}
			else
			{
				AssetOperations.Skip.Add(SourceAssetInfo.Key);
			}
		}

		return AssetOperations;
	}


	// Check if the project contains any incompatible MetaHuman characters
	TSet<FString> CheckVersionCompatibility(const FSourceMetaHuman& SourceMetaHuman, const TArray<FInstalledMetaHuman>& InstalledMetaHumans)
	{
		TSet<FString> IncompatibleCharacters;
		const FMetaHumanVersion& SourceVersion = SourceMetaHuman.GetVersion();
		for (const FInstalledMetaHuman& InstalledMetaHuman : InstalledMetaHumans)
		{
			if (!SourceVersion.IsCompatible(InstalledMetaHuman.GetVersion()))
			{
				IncompatibleCharacters.Emplace(InstalledMetaHuman.GetName());
			}
		}
		return IncompatibleCharacters;
	}

	void EnableMissingPlugins()
	{
		// TODO we should find a way to retrieve the required plugins from the metadata as RigLogic might not be the only one
		static const TArray<FString> NeededPluginNames({TEXT("RigLogic")});

		IPluginManager& PluginManager = IPluginManager::Get();
		IProjectManager& ProjectManager = IProjectManager::Get();

		for (const FString& PluginName : NeededPluginNames)
		{
			TSharedPtr<IPlugin> NeededPlugin = PluginManager.FindPlugin(PluginName);
			if (NeededPlugin.IsValid() && !NeededPlugin->IsEnabled())
			{
				FText FailMessage;
				bool bPluginEnabled = ProjectManager.SetPluginEnabled(NeededPlugin->GetName(), true, FailMessage);

				if (bPluginEnabled && ProjectManager.IsCurrentProjectDirty())
				{
					bPluginEnabled = ProjectManager.SaveCurrentProjectToDisk(FailMessage);
				}

				if (bPluginEnabled)
				{
					PluginManager.MountNewlyCreatedPlugin(NeededPlugin->GetName());
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, FailMessage);
				}
			}
		}
	}

	TMap<FString, FMetaHumanAssetVersion> ParseVersionInfo(const FString& AssetVersionFilePath)
	{
		FString VersionInfoString;
		FFileHelper::LoadFileToString(VersionInfoString, *AssetVersionFilePath);
		TMap<FString, FMetaHumanAssetVersion> VersionInfo;
		TSharedPtr<FJsonObject> AssetsVersionInfoObject;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(VersionInfoString), AssetsVersionInfoObject);
		TArray<TSharedPtr<FJsonValue>> AssetsVersionInfoArray = AssetsVersionInfoObject->GetArrayField(TEXT("assets"));

		for (const TSharedPtr<FJsonValue>& AssetVersionInfoObject : AssetsVersionInfoArray)
		{
			FString AssetPath = AssetVersionInfoObject->AsObject()->GetStringField(TEXT("path"));
			FMetaHumanAssetVersion AssetVersion = FMetaHumanAssetVersion::FromString(AssetVersionInfoObject->AsObject()->GetStringField(TEXT("version")));
			VersionInfo.Add(AssetPath, AssetVersion);
		}

		return VersionInfo;
	}

	void CopyFiles(const FAssetOperationPaths& AssetOperations, const FImportPaths& ImportPaths)
	{
		TArray<UPackage*> PackagesToReload;
		TArray<UPackage*> BPsToReload;

		{
			int32 CommonFilesCount = AssetOperations.Add.Num() + AssetOperations.Replace.Num() + AssetOperations.Update.Num();
			FScopedSlowTask AssetLoadProgress(CommonFilesCount, FText::FromString(TEXT("Updating assets.")), true);
			AssetLoadProgress.MakeDialog();

			IAssetRegistry& AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

			for (const FString& AssetToAdd : AssetOperations.Add)
			{
				AssetLoadProgress.EnterProgressFrame();
				IFileManager::Get().Copy(*ImportPaths.GetDestinationFile(AssetToAdd), *ImportPaths.GetSourceFile(AssetToAdd), true, true);
			}

			TArray<FString> UpdateOperations = AssetOperations.Replace;
			UpdateOperations.Append(AssetOperations.Update);
			for (const FString& AssetToUpdate : UpdateOperations)
			{
				AssetLoadProgress.EnterProgressFrame();
				if (FPaths::GetExtension(AssetToUpdate) == TEXT("uasset"))
				{
					const FSoftObjectPath AssetToReplace(ImportPaths.GetDestinationAsset(AssetToUpdate));
					FAssetData GameAssetData = AssetRegistry.GetAssetByObjectPath(AssetToReplace);
					// If the asset is not loaded we can just overwrite the file and do not need to worry about unloading
					// and reloading the package.
					if (GameAssetData.IsAssetLoaded())
					{
						UObject* ItemObject = GameAssetData.GetAsset();

						if (!ItemObject->GetPackage()->IsFullyLoaded())
						{
							FlushAsyncLoading();
							ItemObject->GetPackage()->FullyLoad();
						}

						// We are about to replace this object, so ignore any pending changes
						ItemObject->GetPackage()->ClearDirtyFlag();

						if (Cast<UBlueprint>(ItemObject) != nullptr)
						{
							BPsToReload.Add(ItemObject->GetPackage());
						}

						ResetLoaders(ItemObject->GetPackage());

						PackagesToReload.Add(ItemObject->GetPackage());
					}
				}
				IFileManager::Get().Copy(*ImportPaths.GetDestinationFile(AssetToUpdate), *ImportPaths.GetSourceFile(AssetToUpdate), true, true);
			}
		}

		FScopedSlowTask PackageReloadProgress(PackagesToReload.Num() + BPsToReload.Num(), FText::FromString(TEXT("Reloading packages.")), true);
		PackageReloadProgress.MakeDialog();

		PackageReloadProgress.EnterProgressFrame(PackagesToReload.Num());
		UPackageTools::ReloadPackages(PackagesToReload);

		for (const UPackage* Package : BPsToReload)
		{
			PackageReloadProgress.EnterProgressFrame();
			UObject* Obj = Package->FindAssetInPackage();
			if (UBlueprint* BPObject = Cast<UBlueprint>(Obj))
			{
				FKismetEditorUtilities::CompileBlueprint(BPObject, EBlueprintCompileOptions::SkipGarbageCollection);
				BPObject->PreEditChange(nullptr);
				BPObject->PostEditChange();
			}
		}
	}

	bool MHInLevel(const FString& CharacterBPPath)
	{
		const FString CharacterPathInLevel = CharacterBPPath + TEXT("_C");
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GEngine->GetWorldContexts()[0].World(), AActor::StaticClass(), FoundActors);

		for (const AActor* FoundActor : FoundActors)
		{
			FString ActorPath = FoundActor->GetClass()->GetPathName();
			if (ActorPath.Equals(CharacterPathInLevel))
			{
				return true;
			}
		}

		return false;
	}
}

// FMetaHumanImport Definition *****************************************
TSharedPtr<FMetaHumanImport> FMetaHumanImport::MetaHumanImportInst;

TSharedPtr<FMetaHumanImport> FMetaHumanImport::Get()
{
	if (!MetaHumanImportInst.IsValid())
	{
		MetaHumanImportInst = MakeShareable(new FMetaHumanImport);
	}
	return MetaHumanImportInst;
}

void FMetaHumanImport::SetAutomationHandler(IMetaHumanProjectUtilitiesAutomationHandler* Handler)
{
	AutomationHandler = Handler;
}

void FMetaHumanImport::SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler)
{
	BulkImportHandler = Handler;
}

void FMetaHumanImport::ImportAsset(const FMetaHumanAssetImportDescription& ImportDescription)
{
	using namespace UE::MetaHumanImport::Private;
	UE_LOG(LogMetaHumanImport, Display, TEXT("Importing MetaHuman: %s"), *ImportDescription.CharacterName);

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Helpers for managing source data
	const FImportPaths ImportPaths(ImportDescription);
	const FSourceMetaHuman SourceMetaHuman{ImportPaths.SourceMetaHumansFilePath, ImportDescription.CharacterName};

	// Determine what other MetaHumans are installed and if any are incompatible
	const TArray<FInstalledMetaHuman> InstalledMetaHumans = FInstalledMetaHuman::GetInstalledMetaHumans(ImportPaths);
	const TSet<FString> IncompatibleCharacters = CheckVersionCompatibility(SourceMetaHuman, InstalledMetaHumans);

	// Get the names of all installed MetaHumans and see if the MetaHuman we are trying to install is among them
	TSet<FString> InstalledMetaHumanNames;
	Algo::Transform(InstalledMetaHumans, InstalledMetaHumanNames, &FInstalledMetaHuman::GetName);
	bool bIsNewCharacter = !InstalledMetaHumanNames.Contains(ImportDescription.CharacterName);


	// Get Manifest of files and version information included in downloaded MetaHuman
	IFileManager& FileManager = IFileManager::Get();
	const FString SourceAssetVersionFilePath = FPaths::Combine(ImportPaths.SourceMetaHumansFilePath, TEXT("MHAssetVersions.txt"));
	if (!FileManager.FileExists(*SourceAssetVersionFilePath))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString(TEXT("The downloaded MetaHuman is corrupted and can not be imported. Please re-generate and re-download the MetaHuman and try again."))));
		return;
	}
	const FAssetOperationPaths AssetOperations = DetermineAssetOperations(ParseVersionInfo(SourceAssetVersionFilePath), ImportPaths, ImportDescription.bForceUpdate);

	// If we are updating common files, have incompatible characters and are not updating all of them, then ask the user if they want to continue.
	if (IncompatibleCharacters.Num() > 0 && !ImportDescription.bIsBatchImport && !AssetOperations.Update.IsEmpty())
	{
		if (AutomationHandler)
		{
			if (!AutomationHandler->ShouldContinueWithBreakingMetaHumans(IncompatibleCharacters.Array(), AssetOperations.Update))
			{
				return;
			}
		}
		else
		{
			TSet<FString> AvailableMetaHumans;
			for (const FQuixelAccountMetaHumanEntry& Entry : ImportDescription.AccountMetaHumans)
			{
				if (!Entry.bIsLegacy)
				{
					AvailableMetaHumans.Add(Entry.Name);
				}
			}
			EImportOperationUserResponse Response = DisplayUpgradeWarning(SourceMetaHuman, IncompatibleCharacters, InstalledMetaHumans, AvailableMetaHumans, AssetOperations);
			if (Response == EImportOperationUserResponse::Cancel)
			{
				return;
			}

			if (Response == EImportOperationUserResponse::BulkImport && BulkImportHandler)
			{
				TArray<FString> ImportIds{ImportDescription.QuixelId};
				for (const FString& Name : IncompatibleCharacters)
				{
					for (const FQuixelAccountMetaHumanEntry& Entry : ImportDescription.AccountMetaHumans)
					{
						// TODO - this just selects the first entry that matches the MetaHuman's name. We need to handle more complex mapping between Ids and entry in the UI
						if (!Entry.bIsLegacy && Entry.Name == Name)
						{
							ImportIds.Add(Entry.Id);
							break;
						}
					}
				}
				BulkImportHandler->DoBulkImport(ImportIds);
				return;
			}
		}
	}

	// If the user is changing the export quality level of the MetaHuman then warn them that they are doing do
	if (!bIsNewCharacter && ImportDescription.bWarnOnQualityChange)
	{
		const FInstalledMetaHuman TargetMetaHuman(ImportDescription.CharacterName, ImportPaths.DestinationMetaHumansFilePath);
		const EQualityLevel SourceQualityLevel = SourceMetaHuman.GetQualityLevel();
		const EQualityLevel TargetQualityLevel = TargetMetaHuman.GetQualityLevel();
		if (SourceQualityLevel != TargetQualityLevel)
		{
			const bool bContinue = DisplayQualityLevelChangeWarning(SourceQualityLevel, TargetQualityLevel);
			if (!bContinue)
			{
				return;
			}
		}
	}

	TSet<FString> TouchedAssets;
	TouchedAssets.Reserve(AssetOperations.Update.Num() + AssetOperations.Replace.Num() + AssetOperations.Add.Num());
	TouchedAssets.Append(AssetOperations.Update);
	TouchedAssets.Append(AssetOperations.Replace);
	TouchedAssets.Append(AssetOperations.Add);

	// TODO: Confirm this is still the case
	// NOTE: the RigLogic plugin (and maybe others) must be loaded and added to the project before loading the asset
	// otherwise we get rid of the RigLogic nodes, resulting in leaving the asset in an undefined state. In the context
	// of ControlRig assets, graphs will remove the RigLogic nodes if the plugin is not enabled because the
	// FRigUnit_RigLogic_Data won't be available
	EnableMissingPlugins();

	FText CharacterCopyMsgDialogMessage = FText::FromString((bIsNewCharacter ? TEXT("Importing : ") : TEXT("Re-Importing : ")) + ImportDescription.CharacterName);
	const bool bRequiresRedirects = ImportDescription.DestinationPath != ImportDescription.SourcePath;
	FScopedSlowTask ImportProgress(bRequiresRedirects ? 3.0f : 2.0f, CharacterCopyMsgDialogMessage, true);
	ImportProgress.MakeDialog();

	// If required, set up redirects
	TArray<FCoreRedirect> Redirects;
	if (bRequiresRedirects)
	{
		for (const FString& AssetFilePath : TouchedAssets)
		{
			if (FPaths::GetExtension(AssetFilePath) == TEXT("uasset"))
			{
				const FString PackageName = AssetFilePath.LeftChop(7); // ".uasset"
				Redirects.Emplace(ECoreRedirectFlags::Type_Package, FPaths::Combine(ImportDescription.SourcePath, PackageName), FPaths::Combine(ImportDescription.DestinationPath, PackageName));
			}
		}
		FCoreRedirects::AddRedirectList(Redirects, TEXT("MetaHumanImportTool"));
	}

	// Update assets
	ImportProgress.EnterProgressFrame();
	CopyFiles(AssetOperations, ImportPaths);

	// Copy in text version files
	const FString VersionFile = TEXT("VersionInfo.txt");
	FileManager.Copy(*FPaths::Combine(ImportPaths.DestinationCharacterFilePath, VersionFile), *FPaths::Combine(ImportPaths.SourceCharacterFilePath, VersionFile), true, true);
	FileManager.Copy(*FPaths::Combine(ImportPaths.DestinationCommonFilePath, VersionFile), *FPaths::Combine(ImportPaths.SourceCommonFilePath, VersionFile), true, true);

	// Copy in optional DNA files
	const FString SourceAssetsFolder = TEXT("SourceAssets");
	const FString SourceAssetsPath = FPaths::Combine(ImportPaths.SourceCharacterFilePath, SourceAssetsFolder);
	if (FileManager.DirectoryExists(*SourceAssetsPath))
	{
		FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*FPaths::Combine(ImportPaths.DestinationCharacterFilePath, SourceAssetsFolder), *SourceAssetsPath, true);
	}

	// Refresh asset registry
	TArray<FString> AssetBasePaths;
	AssetBasePaths.Add(ImportPaths.DestinationMetaHumansAssetPath);
	AssetBasePaths.Add(ImportPaths.DestinationCharacterAssetPath);
	ImportProgress.EnterProgressFrame();
	AssetRegistryModule.Get().ScanPathsSynchronous(AssetBasePaths, true);


	if (bRequiresRedirects)
	{
		// Re save assets to bake-in new reference paths
		ImportProgress.EnterProgressFrame();
		FScopedSlowTask MetaDataWriteProgress(TouchedAssets.Num(), FText::FromString(TEXT("Finalizing imported assets")));
		MetaDataWriteProgress.MakeDialog();
		for (const FString& AssetToUpdate : TouchedAssets)
		{
			MetaDataWriteProgress.EnterProgressFrame();
			const FString FullFilePath = ImportPaths.GetDestinationFile(AssetToUpdate);
			if (!FileManager.FileExists(*FullFilePath))
			{
				continue;
			}
			const FString AssetPath = ImportPaths.GetDestinationAsset(AssetToUpdate);
			if (UObject* ItemObject = LoadObject<UObject>(nullptr, *AssetPath))
			{
				if (UPackage* Package = ItemObject->GetOutermost())
				{
					Package->FullyLoad();
					FSavePackageArgs SaveArgs;
					SaveArgs.TopLevelFlags = RF_Standalone;
					UPackage::Save(Package, nullptr, *FullFilePath, SaveArgs);
				}
			}
		}

		// Remove Redirects
		FCoreRedirects::RemoveRedirectList(Redirects, TEXT("MetaHumanImportTool"));
	}
}

#undef LOCTEXT_NAMESPACE
