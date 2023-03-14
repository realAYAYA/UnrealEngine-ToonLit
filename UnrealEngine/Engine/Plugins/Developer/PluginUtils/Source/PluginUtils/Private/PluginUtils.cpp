// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "PluginUtils.h"
#include "SourceControlHelpers.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "GameProjectUtils.h"
#include "Interfaces/IProjectManager.h"
#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "DesktopPlatformModule.h"
#include "PackageTools.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/FeedbackContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogPluginUtils, Log, All);

#define LOCTEXT_NAMESPACE "PluginUtils"

namespace PluginUtils
{
	// The text macro to replace with the actual plugin name when copying files
	const FString PLUGIN_NAME = TEXT("PLUGIN_NAME");

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FPluginUtils::FLoadPluginParams ConvertToLoadPluginParams(const FPluginUtils::FMountPluginParams& MountParams, FText* FailReason)
	{
		FPluginUtils::FLoadPluginParams LoadParams;
		LoadParams.bSelectInContentBrowser = MountParams.bSelectInContentBrowser;
		LoadParams.bEnablePluginInProject = MountParams.bEnablePluginInProject;
		LoadParams.bUpdateProjectPluginSearchPath = MountParams.bUpdateProjectPluginSearchPath;
		LoadParams.OutFailReason = FailReason;
		return LoadParams;
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	bool CopyPluginTemplateFolder(const TCHAR* DestinationDirectory, const TCHAR* Source, const FString& PluginName, TArray<FString>& InOutFilePathsWritten, FText* OutFailReason = nullptr)
	{
		check(DestinationDirectory);
		check(Source);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		FString DestDir(DestinationDirectory);
		FPaths::NormalizeDirectoryName(DestDir);

		FString SourceDir(Source);
		FPaths::NormalizeDirectoryName(SourceDir);

		// Does Source dir exist?
		if (!PlatformFile.DirectoryExists(*SourceDir))
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("InvalidPluginTemplateFolder", "Plugin template folder doesn't exist\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(SourceDir)));
			}
			return false;
		}

		// Destination directory exists already or can be created ?
		if (!PlatformFile.DirectoryExists(*DestDir) && !PlatformFile.CreateDirectoryTree(*DestDir))
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailedToCreateDestinationFolder", "Failed to create destination folder\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(DestDir)));
			}
			return false;
		}

		// Copy all files and directories, renaming specific sections to the plugin name
		struct FCopyPluginFilesAndDirs : public IPlatformFile::FDirectoryVisitor
		{
			IPlatformFile& PlatformFile;
			const TCHAR* SourceRoot;
			const TCHAR* DestRoot;
			const FString& PluginName;
			TArray<FString> NameReplacementFileTypes;
			TArray<FString> IgnoredFileTypes;
			TArray<FString> CopyUnmodifiedFileTypes;
			TArray<FString>& FilePathsWritten;
			FText* FailReason;

			FCopyPluginFilesAndDirs(IPlatformFile& InPlatformFile, const TCHAR* InSourceRoot, const TCHAR* InDestRoot, const FString& InPluginName, TArray<FString>& InFilePathsWritten, FText* InFailReason)
				: PlatformFile(InPlatformFile)
				, SourceRoot(InSourceRoot)
				, DestRoot(InDestRoot)
				, PluginName(InPluginName)
				, FilePathsWritten(InFilePathsWritten)
				, FailReason(InFailReason)
			{
				// Which file types we want to replace instances of PLUGIN_NAME with the new Plugin Name
				NameReplacementFileTypes.Add(TEXT("cs"));
				NameReplacementFileTypes.Add(TEXT("cpp"));
				NameReplacementFileTypes.Add(TEXT("h"));
				NameReplacementFileTypes.Add(TEXT("vcxproj"));
				NameReplacementFileTypes.Add(TEXT("uplugin"));

				// Which file types do we want to ignore
				IgnoredFileTypes.Add(TEXT("opensdf"));
				IgnoredFileTypes.Add(TEXT("sdf"));
				IgnoredFileTypes.Add(TEXT("user"));
				IgnoredFileTypes.Add(TEXT("suo"));

				// Which file types do we want to copy completely unmodified
				CopyUnmodifiedFileTypes.Add(TEXT("uasset"));
				CopyUnmodifiedFileTypes.Add(TEXT("umap"));
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
			{
				FString NewName(FilenameOrDirectory);
				// change the root and rename paths/files
				NewName.RemoveFromStart(SourceRoot);
				NewName = NewName.Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);
				NewName = FPaths::Combine(DestRoot, *NewName);

				if (bIsDirectory)
				{
					// create new directory structure
					if (!PlatformFile.CreateDirectoryTree(*NewName) && !PlatformFile.DirectoryExists(*NewName))
					{
						if (FailReason)
						{
							*FailReason = FText::Format(LOCTEXT("FailedToCreatePluginSubFolder", "Failed to create plugin subfolder\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(NewName)));
						}
						return false;
					}
				}
				else
				{
					FString NewExt = FPaths::GetExtension(FilenameOrDirectory);

					if (!IgnoredFileTypes.Contains(NewExt))
					{
						if (CopyUnmodifiedFileTypes.Contains(NewExt))
						{
							// Copy unmodified files with the original name, but do rename the directories
							FString CleanFilename = FPaths::GetCleanFilename(FilenameOrDirectory);
							FString CopyToPath = FPaths::GetPath(NewName);

							NewName = FPaths::Combine(CopyToPath, CleanFilename);
						}

						if (PlatformFile.FileExists(*NewName))
						{
							// Delete destination file if it exists
							PlatformFile.DeleteFile(*NewName);
						}

						// If file of specified extension - open the file as text and replace PLUGIN_NAME in there before saving
						if (NameReplacementFileTypes.Contains(NewExt))
						{
							FString OutFileContents;
							if (!FFileHelper::LoadFileToString(OutFileContents, FilenameOrDirectory))
							{
								if (FailReason)
								{
									*FailReason = FText::Format(LOCTEXT("FailedToReadPluginTemplateFile", "Failed to read plugin template file\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(FilenameOrDirectory)));
								}
								return false;
							}

							OutFileContents = OutFileContents.Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);

							// For some content, we also want to export a PLUGIN_NAME_API text macro, which requires that the plugin name
							// be all capitalized

							FString PluginNameAPI = PluginName + TEXT("_API");

							OutFileContents = OutFileContents.Replace(*PluginNameAPI, *PluginNameAPI.ToUpper(), ESearchCase::CaseSensitive);

							if (!FFileHelper::SaveStringToFile(OutFileContents, *NewName))
							{
								if (FailReason)
								{
									*FailReason = FText::Format(LOCTEXT("FailedToWritePluginFile", "Failed to write plugin file\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(NewName)));
								}
								return false;
							}
						}
						else
						{
							// Copy file from source
							if (!PlatformFile.CopyFile(*NewName, FilenameOrDirectory))
							{
								// Not all files could be copied
								if (FailReason)
								{
									*FailReason = FText::Format(LOCTEXT("FailedToCopyPluginTemplateFile", "Failed to copy plugin template file\nFrom: {0}\nTo: {1}"), FText::FromString(FPaths::ConvertRelativePathToFull(FilenameOrDirectory)), FText::FromString(FPaths::ConvertRelativePathToFull(NewName)));
								}
								return false;
							}
						}
						FilePathsWritten.Add(NewName);
					}
				}
				return true; // continue searching
			}
		};

		// copy plugin files and directories visitor
		FCopyPluginFilesAndDirs CopyFilesAndDirs(PlatformFile, *SourceDir, *DestDir, PluginName, InOutFilePathsWritten, OutFailReason);

		// create all files subdirectories and files in subdirectories!
		return PlatformFile.IterateDirectoryRecursively(*SourceDir, CopyFilesAndDirs);
	}

	void FixupPluginTemplateAssets(const FString& PluginName, TMap<FString, FString>& OutModifiedAssetPaths)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);

		struct FFixupPluginAssets : public IPlatformFile::FDirectoryVisitor
		{
			IPlatformFile& PlatformFile;
			const FString& PluginName;
			const FString& PluginBaseDir;

			TArray<FString> FilesToScan;

			FFixupPluginAssets(IPlatformFile& InPlatformFile, const FString& InPluginName, const FString& InPluginBaseDir)
				: PlatformFile(InPlatformFile)
				, PluginName(InPluginName)
				, PluginBaseDir(InPluginBaseDir)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (!bIsDirectory)
				{
					FString Extension = FPaths::GetExtension(FilenameOrDirectory);

					// Only interested in fixing up uassets and umaps...anything else we leave alone
					if (Extension == TEXT("uasset") || Extension == TEXT("umap"))
					{
						FilesToScan.Add(FilenameOrDirectory);
					}
				}

				return true;
			}

			/**
			 * Fixes up any assets that contain the PLUGIN_NAME text macro, since those need to be renamed by the engine for the change to
			 * stick (as opposed to just renaming the file) as well as regenerating the localization ids for the assets in the plugin.
			 */
			void PerformFixup(TMap<FString, FString>& OutModifiedAssetPaths)
			{
				TArray<FAssetRenameData> AssetRenameData;

				if (FilesToScan.Num() > 0)
				{
					IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
					AssetRegistry.ScanFilesSynchronous(FilesToScan);

					TMap<TWeakObjectPtr<UObject>, FString> AssetToOriginalFilePathMap;
					for (const FString& File : FilesToScan)
					{
						TArray<FAssetData> Assets;

						FString PackageName;
						if (FPackageName::TryConvertFilenameToLongPackageName(File, PackageName))
						{
							AssetRegistry.GetAssetsByPackageName(*PackageName, Assets);
						}

						for (FAssetData AssetData : Assets)
						{
							const FString AssetName = AssetData.AssetName.ToString().Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);
							const FString AssetPath = AssetData.PackagePath.ToString().Replace(*PLUGIN_NAME, *PluginName, ESearchCase::CaseSensitive);
							AssetToOriginalFilePathMap.Add(AssetData.GetAsset(), File);
							FAssetRenameData RenameData(AssetData.GetAsset(), AssetPath, AssetName);

							AssetRenameData.Add(RenameData);

							if (UObject* Asset = AssetData.GetAsset())
							{
								Asset->Modify();
								TextNamespaceUtil::ClearPackageNamespace(Asset);
								TextNamespaceUtil::EnsurePackageNamespace(Asset);
							}
						}
					}

					if (AssetRenameData.Num() > 0)
					{
						FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
						AssetToolsModule.Get().RenameAssetsWithDialog(AssetRenameData);

						for(const FAssetRenameData& RenamedAsset : AssetRenameData)
						{
							if (RenamedAsset.Asset.IsValid())
							{
								if (FString* OldPath = AssetToOriginalFilePathMap.Find(RenamedAsset.Asset))
								{
									if (const UPackage* Package = RenamedAsset.Asset->GetPackage())
									{
										FString PackageFilename;
										if (FPackageName::TryConvertLongPackageNameToFilename(Package->GetName(), PackageFilename, Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension()))
										{
											FString FullOldPath = FPaths::ConvertRelativePathToFull(*OldPath);
											FPaths::RemoveDuplicateSlashes(FullOldPath);
											OutModifiedAssetPaths.Add(FullOldPath, PackageFilename);
										}
									}
								}
							}
						}
					}
				}
			}
		};

		if (Plugin.IsValid())
		{
			const FString PluginBaseDir = Plugin->GetBaseDir();
			FFixupPluginAssets FixupPluginAssets(PlatformFile, PluginName, PluginBaseDir);
			PlatformFile.IterateDirectoryRecursively(*PluginBaseDir, FixupPluginAssets);
			FixupPluginAssets.PerformFixup(OutModifiedAssetPaths);
		}
	}

	TSharedPtr<IPlugin> LoadPluginInternal(const FString& PluginName, const FString& PluginLocation, const FString& PluginFilePath, FPluginUtils::FLoadPluginParams& LoadParams, const bool bIsNewPlugin)
	{
		LoadParams.bOutAlreadyLoaded = false;

		if (LoadParams.bUpdateProjectPluginSearchPath)
		{
			FPluginUtils::AddToPluginSearchPathIfNeeded(PluginLocation, /*bRefreshPlugins=*/false, /*bUpdateProjectFile=*/true);
			IPluginManager::Get().RefreshPluginsList();
		}
		else
		{
			if (!IPluginManager::Get().AddToPluginsList(PluginFilePath, LoadParams.OutFailReason))
			{
				return nullptr;
			}
		}

		// Find the plugin in the manager.
		TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		if (!Plugin)
		{
			if (LoadParams.OutFailReason)
			{
				*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToRegisterPlugin", "Failed to register plugin\n{0}"), FText::FromString(PluginFilePath));
			}
			return nullptr;
		}

		// Double check the path matches
		if (!FPaths::IsSamePath(Plugin->GetDescriptorFileName(), PluginFilePath))
		{
			if (LoadParams.OutFailReason)
			{
				const FString PluginFilePathFull = FPaths::ConvertRelativePathToFull(Plugin->GetDescriptorFileName());
				*LoadParams.OutFailReason = FText::Format(LOCTEXT("PluginNameAlreadyUsed", "There's already a plugin named {0} at this location:\n{1}"), FText::FromString(PluginName), FText::FromString(PluginFilePathFull));
			}
			return nullptr;
		}

		const FString PluginRootFolder = Plugin->CanContainContent() ? Plugin->GetMountedAssetPath() : FString();

		LoadParams.bOutAlreadyLoaded = !bIsNewPlugin && Plugin->IsEnabled() && (PluginRootFolder.IsEmpty() || FPackageName::MountPointExists(PluginRootFolder));

		// Enable this plugin in the project
		if (LoadParams.bEnablePluginInProject)
		{
			FText FailReason;
			if (!IProjectManager::Get().SetPluginEnabled(PluginName, true, FailReason))
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToEnablePluginInProject", "Failed to enable plugin in current project\n{0}"), FailReason);
				}
				return nullptr;
			}
		}

		if (!LoadParams.bOutAlreadyLoaded)
		{
			// Mount the new plugin (mount content folder if any and load modules if any)
			if (bIsNewPlugin)
			{
				IPluginManager::Get().MountNewlyCreatedPlugin(PluginName);
			}
			else
			{
				IPluginManager::Get().MountExplicitlyLoadedPlugin(PluginName);
			}

			if (!Plugin->IsEnabled())
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToEnablePlugin", "Failed to enable plugin because it is not configured as bExplicitlyLoaded=true\n{0}"), FText::FromString(PluginFilePath));
				}
				return nullptr;
			}
		}

		const bool bSelectInContentBrowser = LoadParams.bSelectInContentBrowser && !IsRunningCommandlet();

		if ((bSelectInContentBrowser || LoadParams.bSynchronousAssetsScan) && !PluginRootFolder.IsEmpty() && (LoadParams.bOutAlreadyLoaded || FPackageName::MountPointExists(PluginRootFolder)))
		{
			if (LoadParams.bSynchronousAssetsScan)
			{
				// Scan plugin assets
				IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
				AssetRegistry.ScanPathsSynchronous({ PluginRootFolder }, /*bForceRescan=*/ true);
			}

			if (bSelectInContentBrowser)
			{
				// Select plugin root folder in content browser
				IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser").Get();
				const bool bIsEnginePlugin = FPaths::IsUnderDirectory(PluginLocation, FPaths::EnginePluginsDir());
				ContentBrowser.ForceShowPluginContent(bIsEnginePlugin);
				ContentBrowser.SetSelectedPaths({ PluginRootFolder }, /*bNeedsRefresh*/ true);
			}
		}

		return Plugin;
	}
}

FString FPluginUtils::GetPluginFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath)
{
	FString PluginFolder = FPaths::Combine(PluginLocation, PluginName);
	if (bFullPath)
	{
		PluginFolder = FPaths::ConvertRelativePathToFull(PluginFolder);
	}
	FPaths::MakePlatformFilename(PluginFolder);
	return PluginFolder;
}

FString FPluginUtils::GetPluginFilePath(const FString& PluginLocation, const FString& PluginName, bool bFullPath)
{
	FString PluginFilePath = FPaths::Combine(PluginLocation, PluginName, (PluginName + FPluginDescriptor::GetFileExtension()));
	if (bFullPath)
	{
		PluginFilePath = FPaths::ConvertRelativePathToFull(PluginFilePath);
	}
	FPaths::MakePlatformFilename(PluginFilePath);
	return PluginFilePath;
}

FString FPluginUtils::GetPluginContentFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath)
{
	FString PluginContentFolder = FPaths::Combine(PluginLocation, PluginName, TEXT("Content"));
	if (bFullPath)
	{
		PluginContentFolder = FPaths::ConvertRelativePathToFull(PluginContentFolder);
	}
	FPaths::MakePlatformFilename(PluginContentFolder);
	return PluginContentFolder;
}

FString FPluginUtils::GetPluginResourcesFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath)
{
	FString PluginResourcesFolder = FPaths::Combine(PluginLocation, PluginName, TEXT("Resources"));
	if (bFullPath)
	{
		PluginResourcesFolder = FPaths::ConvertRelativePathToFull(PluginResourcesFolder);
	}
	FPaths::MakePlatformFilename(PluginResourcesFolder);
	return PluginResourcesFolder;
}

TSharedPtr<IPlugin> FPluginUtils::CreateAndLoadNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParams& CreationParams, FLoadPluginParams& LoadParams)
{
	FNewPluginParamsWithDescriptor ExCreationParams;
	ExCreationParams.PluginIconPath = CreationParams.PluginIconPath;
	ExCreationParams.TemplateFolders = CreationParams.TemplateFolders;

	ExCreationParams.Descriptor.FriendlyName = CreationParams.FriendlyName.Len() > 0 ? CreationParams.FriendlyName : PluginName;
	ExCreationParams.Descriptor.Version = 1;
	ExCreationParams.Descriptor.VersionName = TEXT("1.0");
	ExCreationParams.Descriptor.Category = TEXT("Other");
	ExCreationParams.Descriptor.CreatedBy = CreationParams.CreatedBy;
	ExCreationParams.Descriptor.CreatedByURL = CreationParams.CreatedByURL;
	ExCreationParams.Descriptor.Description = CreationParams.Description;
	ExCreationParams.Descriptor.bIsBetaVersion = CreationParams.bIsBetaVersion;
	ExCreationParams.Descriptor.bCanContainContent = CreationParams.bCanContainContent;
	ExCreationParams.Descriptor.bCanContainVerse = CreationParams.bCanContainVerse;
	ExCreationParams.Descriptor.EnabledByDefault = CreationParams.EnabledByDefault;
	ExCreationParams.Descriptor.bExplicitlyLoaded = CreationParams.bExplicitelyLoaded;
	ExCreationParams.Descriptor.VersePath = CreationParams.VersePath;

	if (CreationParams.bHasModules)
	{
		ExCreationParams.Descriptor.Modules.Add(FModuleDescriptor(*PluginName, CreationParams.ModuleDescriptorType, CreationParams.LoadingPhase));
	}

	return CreateAndLoadNewPlugin(PluginName, PluginLocation, ExCreationParams, LoadParams);
}

TSharedPtr<IPlugin> FPluginUtils::CreateAndLoadNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParamsWithDescriptor& CreationParams, FLoadPluginParams& LoadParams)
{
	// Early validations on new plugin params
	if (PluginName.IsEmpty())
	{
		if (LoadParams.OutFailReason)
		{
			*LoadParams.OutFailReason = LOCTEXT("CreateNewPluginParam_NoPluginName", "Missing plugin name");
		}
		return nullptr;
	}

	if (PluginLocation.IsEmpty())
	{
		if (LoadParams.OutFailReason)
		{
			*LoadParams.OutFailReason = LOCTEXT("CreateNewPluginParam_NoPluginLocation", "Missing plugin location");
		}
		return nullptr;
	}

	if ((CreationParams.Descriptor.Modules.Num() > 0) && (CreationParams.TemplateFolders.Num() == 0))
	{
		if (LoadParams.OutFailReason)
		{
			*LoadParams.OutFailReason = LOCTEXT("CreateNewPluginParam_NoTemplateFolder", "A template folder must be specified to create a plugin with code");
		}
		return nullptr;
	}

	if (!FPluginUtils::ValidateNewPluginNameAndLocation(PluginName, PluginLocation, LoadParams.OutFailReason))
	{
		return nullptr;
	}

	const FString PluginFolder = FPluginUtils::GetPluginFolder(PluginLocation, PluginName, /*bFullPath*/ true);

	TSharedPtr<IPlugin> NewPlugin;
	bool bSucceeded = true;
	do
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		TArray<FString> NewFilePaths;

		if (!PlatformFile.DirectoryExists(*PluginFolder) && !PlatformFile.CreateDirectoryTree(*PluginFolder))
		{
			if (LoadParams.OutFailReason)
			{
				*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToCreatePluginFolder", "Failed to create plugin folder\n{0}"), FText::FromString(PluginFolder));
			}
			bSucceeded = false;
			break;
		}

		if (CreationParams.Descriptor.bCanContainContent)
		{
			const FString PluginContentFolder = FPluginUtils::GetPluginContentFolder(PluginLocation, PluginName, /*bFullPath*/ true);
			if (!PlatformFile.DirectoryExists(*PluginContentFolder) && !PlatformFile.CreateDirectory(*PluginContentFolder))
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToCreatePluginContentFolder", "Failed to create plugin Content folder\n{0}"), FText::FromString(PluginContentFolder));
				}
				bSucceeded = false;
				break;
			}
		}

		// Write the uplugin file
		const FString PluginFilePath = FPluginUtils::GetPluginFilePath(PluginLocation, PluginName, /*bFullPath*/ true);
		if (!CreationParams.Descriptor.Save(PluginFilePath, LoadParams.OutFailReason))
		{
			bSucceeded = false;
			break;
		}
		NewFilePaths.Add(PluginFilePath);

		// Copy plugin icon
		if (!CreationParams.PluginIconPath.IsEmpty())
		{
			const FString ResourcesFolder = FPluginUtils::GetPluginResourcesFolder(PluginLocation, PluginName, /*bFullPath*/ true);
			const FString DestinationPluginIconPath = FPaths::Combine(ResourcesFolder, TEXT("Icon128.png"));
			if (IFileManager::Get().Copy(*DestinationPluginIconPath, *CreationParams.PluginIconPath, /*bReplaceExisting=*/ false) != COPY_OK)
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToCopyPluginIcon", "Failed to copy plugin icon\nFrom: {0}\nTo: {1}"), FText::FromString(FPaths::ConvertRelativePathToFull(CreationParams.PluginIconPath)), FText::FromString(DestinationPluginIconPath));
				}
				bSucceeded = false;
				break;
			}
			NewFilePaths.Add(DestinationPluginIconPath);
		}

		// Copy template files
		if (CreationParams.TemplateFolders.Num() > 0)
		{
			GWarn->BeginSlowTask(LOCTEXT("CopyingPluginTemplate", "Copying plugin template files..."), /*ShowProgressDialog*/ true, /*bShowCancelButton*/ false);
			for (const FString& TemplateFolder : CreationParams.TemplateFolders)
			{
				if (!PluginUtils::CopyPluginTemplateFolder(*PluginFolder, *TemplateFolder, PluginName, NewFilePaths, LoadParams.OutFailReason))
				{
					if (LoadParams.OutFailReason)
					{
						*LoadParams.OutFailReason = FText::Format(LOCTEXT("FailedToCopyPluginTemplate", "Failed to copy plugin template files\nFrom: {0}\nTo: {1}\n{2}"), FText::FromString(FPaths::ConvertRelativePathToFull(TemplateFolder)), FText::FromString(PluginFolder), *LoadParams.OutFailReason);
					}
					bSucceeded = false;
					break;
				}
			}
			GWarn->EndSlowTask();
		}

		if (!bSucceeded)
		{
			break;
		}

		// Compile plugin code
		if (CreationParams.Descriptor.Modules.Num() > 0)
		{
			const FString ProjectFileName = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::GetProjectFilePath());

			// Internal plugins will be found and built automatically, and using the -Plugin param will mark them as external plugins causing them to fail to load.
			const bool bIsEnginePlugin = FPaths::IsUnderDirectory(PluginFilePath, FPaths::EnginePluginsDir());
			const bool bIsProjectPlugin = FPaths::IsUnderDirectory(PluginFilePath, FPaths::ProjectPluginsDir());
			const FString ExternalPluginArgument = !(bIsEnginePlugin || bIsProjectPlugin) ? FString::Printf(TEXT("-Plugin=\"%s\" "), *PluginFilePath) : FString();

			const FString Arguments = FString::Printf(TEXT("%s %s %s %s-Project=\"%s\" -Progress -NoHotReloadFromIDE"), FPlatformMisc::GetUBTTargetName(), FModuleManager::Get().GetUBTConfiguration(), FPlatformMisc::GetUBTPlatform(), *ExternalPluginArgument, *ProjectFileName);

			if (!FDesktopPlatformModule::Get()->RunUnrealBuildTool(FText::Format(LOCTEXT("CompilingPlugin", "Compiling {0} plugin..."), FText::FromString(PluginName)), FPaths::RootDir(), Arguments, GWarn))
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = LOCTEXT("FailedToCompilePlugin", "Failed to compile plugin source code. See output log for more information.");
				}
				bSucceeded = false;
				break;
			}

			// Reset the module paths cache. For unique build environments, the modules may be generated to the project binaries directory.
			FModuleManager::Get().ResetModulePathsCache();

			// Generate project files if we happen to be using a project file.
			if (!FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn))
			{
				if (LoadParams.OutFailReason)
				{
					*LoadParams.OutFailReason = LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files");
				}
				bSucceeded = false;
				break;
			}
		}

		// Load the new plugin
		NewPlugin = PluginUtils::LoadPluginInternal(PluginName, PluginLocation, PluginFilePath, LoadParams, /*bIsNewPlugin*/ true);
		if (!NewPlugin)
		{
			bSucceeded = false;
			break;
		}

		// Fix any content that was added to the plugin
		if (CreationParams.Descriptor.bCanContainContent)
		{	
			GWarn->BeginSlowTask(LOCTEXT("LoadingContent", "Loading Content..."), /*ShowProgressDialog*/ true, /*bShowCancelButton*/ false);
			TMap<FString, FString> ModifiedAssetPaths;
			PluginUtils::FixupPluginTemplateAssets(PluginName, ModifiedAssetPaths);

			for (FString& CopiedFilePath : NewFilePaths)
			{
				FString FullCopiedPath = FPaths::ConvertRelativePathToFull(CopiedFilePath);
				FPaths::RemoveDuplicateSlashes(FullCopiedPath);
				if (FString* NewPath = ModifiedAssetPaths.Find(FullCopiedPath))
				{
					CopiedFilePath = *NewPath;
				}
			}
			GWarn->EndSlowTask();
		}

		// Add the plugin files to source control if the project is configured for it
		if (USourceControlHelpers::IsAvailable())
		{
			GWarn->BeginSlowTask(LOCTEXT("AddingFilesToSourceControl", "Adding to Source Control..."), /*ShowProgressDialog*/ true, /*bShowCancelButton*/ false);
			USourceControlHelpers::MarkFilesForAdd(NewFilePaths);
			GWarn->EndSlowTask();
		}
	} while (false);

	if (!bSucceeded)
	{
		// Delete the plugin folder is something goes wrong during the plugin creation.
		IFileManager::Get().DeleteDirectory(*PluginFolder, /*RequireExists*/ false, /*Tree*/ true);
		if (NewPlugin)
		{
			// Refresh plugins if the new plugin was registered, but we decide to delete its files.
			IPluginManager::Get().RefreshPluginsList();
			NewPlugin.Reset();
			ensure(!IPluginManager::Get().FindPlugin(PluginName));
		}
	}

	return NewPlugin;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<IPlugin> FPluginUtils::CreateAndMountNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParams& CreationParams, const FMountPluginParams& MountParams, FText& FailReason)
{
	FLoadPluginParams LoadParams = PluginUtils::ConvertToLoadPluginParams(MountParams, &FailReason);
	return CreateAndLoadNewPlugin(PluginName, PluginLocation, CreationParams, LoadParams);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<IPlugin> FPluginUtils::CreateAndMountNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParamsWithDescriptor& CreationParams, const FMountPluginParams& MountParams, FText& FailReason)
{
	FLoadPluginParams LoadParams = PluginUtils::ConvertToLoadPluginParams(MountParams, &FailReason);
	return CreateAndLoadNewPlugin(PluginName, PluginLocation, CreationParams, LoadParams);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TSharedPtr<IPlugin> FPluginUtils::LoadPlugin(const FString& PluginName, const FString& PluginLocation, FLoadPluginParams& LoadParams)
{
	return LoadPlugin(FPluginUtils::GetPluginFilePath(PluginLocation, PluginName), LoadParams);
}

TSharedPtr<IPlugin> FPluginUtils::LoadPlugin(const FString& PluginFileName, FLoadPluginParams& LoadParams)
{
	// Valide that the uplugin file exists.
	if (!FPaths::FileExists(PluginFileName))
	{
		if (LoadParams.OutFailReason)
		{
			*LoadParams.OutFailReason = FText::Format(LOCTEXT("PluginFileDoesNotExist", "Plugin file does not exist\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(PluginFileName)));
		}
		return nullptr;
	}

	const FString PluginName = FPaths::GetBaseFilename(PluginFileName);
	if (!IsValidPluginName(PluginName, LoadParams.OutFailReason))
	{
		return nullptr;
	}

	const FString PluginLocation = FPaths::GetPath(FPaths::GetPath(PluginFileName));
	return PluginUtils::LoadPluginInternal(PluginName, PluginLocation, PluginFileName, LoadParams, /*bIsNewPlugin*/ false);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedPtr<IPlugin> FPluginUtils::MountPlugin(const FString& PluginName, const FString& PluginLocation, const FMountPluginParams& MountParams, FText& FailReason)
{
	FLoadPluginParams LoadParams = PluginUtils::ConvertToLoadPluginParams(MountParams, &FailReason);
	return LoadPlugin(PluginName, PluginLocation, LoadParams);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

TSharedPtr<IPlugin> FPluginUtils::FindLoadedPlugin(const FString& PluginDescriptorFileName)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(FPaths::GetBaseFilename(PluginDescriptorFileName));
	if (Plugin &&
		Plugin->IsEnabled() &&
		FPaths::IsSamePath(Plugin->GetDescriptorFileName(), PluginDescriptorFileName) &&
		(!Plugin->CanContainContent() || FPackageName::MountPointExists(Plugin->GetMountedAssetPath())))
	{
		return Plugin;
	}
	return TSharedPtr<IPlugin>();
}

bool FPluginUtils::UnloadPlugin(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason /*= nullptr*/)
{
	return UnloadPlugins({ Plugin }, OutFailReason);
}

bool FPluginUtils::UnloadPlugin(const FString& PluginName, FText* OutFailReason /*= nullptr*/)
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		return UnloadPlugin(Plugin.ToSharedRef(), OutFailReason);
	}
	return true;
}

bool FPluginUtils::UnloadPlugins(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason /*= nullptr*/)
{
	if (Plugins.Num() == 0)
	{
		return true;
	}

	{
		FText ErrorMsg;
		if (!UnloadPluginsAssets(Plugins, &ErrorMsg))
		{
			// If some assets fail to unload, log an error, but unmount the plugins anyway
			UE_LOG(LogPluginUtils, Error, TEXT("Failed to unload some assets prior to unmounting plugins\n%s"), *ErrorMsg.ToString());
		}
	}

	// Unmount the plugins
	//
	bool bSuccess = true;
	{
		FTextBuilder ErrorBuilder;
		bool bPluginUnmounted = false;

		for (const TSharedRef<IPlugin>& Plugin : Plugins)
		{
			if (Plugin->IsEnabled())
			{
				bPluginUnmounted = true;

				FText FailReason;
				if (!IPluginManager::Get().UnmountExplicitlyLoadedPlugin(Plugin->GetName(), &FailReason))
				{
					UE_LOG(LogPluginUtils, Error, TEXT("Plugin %s cannot be unloaded: %s"), *Plugin->GetName(), *FailReason.ToString());
					ErrorBuilder.AppendLine(FailReason);
					bSuccess = false;
				}
			}
		}

		if (bPluginUnmounted)
		{
			IPluginManager::Get().RefreshPluginsList();
		}

		if (!bSuccess && OutFailReason)
		{
			*OutFailReason = ErrorBuilder.ToText();
		}
	}
	return bSuccess;
}

bool FPluginUtils::UnloadPlugins(const TConstArrayView<FString> PluginNames, FText* OutFailReason /*= nullptr*/)
{
	TArray<TSharedRef<IPlugin>> Plugins;
	Plugins.Reserve(PluginNames.Num());
	for (const FString& PluginName : PluginNames)
	{
		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
		{
			Plugins.Add(Plugin.ToSharedRef());
		}
	}
	return UnloadPlugins(Plugins, OutFailReason);
}

bool FPluginUtils::UnloadPluginAssets(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason /*= nullptr*/)
{
	return UnloadPluginsAssets({ Plugin }, OutFailReason);
}

bool FPluginUtils::UnloadPluginAssets(const FString& PluginName, FText* OutFailReason /*= nullptr*/)
{
	if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		return UnloadPluginAssets(Plugin.ToSharedRef(), OutFailReason);
	}
	return true;
}

bool FPluginUtils::UnloadPluginsAssets(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason /*= nullptr*/)
{
	if (Plugins.IsEmpty())
	{
		return true;
	}

	TArray<FString> PluginContentMountPoints;
	TSet<FString> PluginNames;
	PluginContentMountPoints.Reserve(Plugins.Num());
	PluginNames.Reserve(Plugins.Num());

	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		if (Plugin->IsEnabled())
		{
			FString PluginContentMountPoint = Plugin->GetMountedAssetPath();
			if (FPackageName::MountPointExists(PluginContentMountPoint))
			{
				PluginContentMountPoints.Add(MoveTemp(PluginContentMountPoint));
				PluginNames.Add(Plugin->GetName());
			}
		}
	}

	if (PluginContentMountPoints.IsEmpty())
	{
		return true;
	}

	// Synchronous scan plugins to make sure we find all their assets.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName).Get();
	AssetRegistry.ScanPathsSynchronous(PluginContentMountPoints, /*bForceRescan=*/ true);

	// Unload plugin packages
	{
		FARFilter ARFilter;
		ARFilter.PackagePaths.Reserve(PluginContentMountPoints.Num());
		for (const FString& PluginContentMountPoint : PluginContentMountPoints)
		{
			FString PluginRoot = PluginContentMountPoint;
			PluginRoot.RemoveFromEnd(TEXT("/"));
			ARFilter.PackagePaths.Add(*PluginRoot);
		}
		ARFilter.bRecursivePaths = true;

		TArray<FAssetData> PluginAssets;
		if (AssetRegistry.GetAssets(ARFilter, PluginAssets))
		{
			TSet<UPackage*> PackagesToUnload;
			PackagesToUnload.Reserve(PluginAssets.Num());
			for (const FAssetData& AssetData : PluginAssets)
			{
				if (UPackage* Package = FindPackage(NULL, *AssetData.PackageName.ToString()))
				{
					PackagesToUnload.Add(Package);
				}
			}

			if (PackagesToUnload.Num() > 0)
			{
				FText ErrorMsg;
				UPackageTools::UnloadPackages(PackagesToUnload.Array(), ErrorMsg, /*bUnloadDirtyPackages=*/true);

				// @note UnloadPackages returned bool indicates whether some packages were unloaded
				// To tell whether all packages were successfully unloaded we must check the ErrorMsg output param
				if (!ErrorMsg.IsEmpty())
				{
					if (OutFailReason)
					{
						*OutFailReason = MoveTemp(ErrorMsg);
					}
					return false;
				}
			}
		}
	}

	return true;
}

bool FPluginUtils::UnloadPluginsAssets(const TConstArrayView<FString> PluginNames, FText* OutFailReason /*= nullptr*/)
{
	TArray<TSharedRef<IPlugin>> Plugins;
	Plugins.Reserve(PluginNames.Num());
	for (const FString& PluginName : PluginNames)
	{
		if (TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName))
		{
			Plugins.Add(Plugin.ToSharedRef());
		}
	}
	return UnloadPluginsAssets(Plugins, OutFailReason);
}

bool FPluginUtils::AddToPluginSearchPathIfNeeded(const FString& Dir, bool bRefreshPlugins, bool bUpdateProjectFile)
{
	bool bSearchPathChanged = false;

	const bool bIsEnginePlugin = FPaths::IsUnderDirectory(Dir, FPaths::EnginePluginsDir());
	const bool bIsProjectPlugin = FPaths::IsUnderDirectory(Dir, FPaths::ProjectPluginsDir());
	const bool bIsModPlugin = FPaths::IsUnderDirectory(Dir, FPaths::ProjectModsDir());
	const bool bIsEnterprisePlugin = FPaths::IsUnderDirectory(Dir, FPaths::EnterprisePluginsDir());
	
	if (bIsEnginePlugin || bIsProjectPlugin || bIsModPlugin || bIsEnterprisePlugin)
	{
		return false;
	}
	
	if (bUpdateProjectFile)
	{
		bool bNeedToUpdate = true;
		for (const FString& AdditionalDir : IProjectManager::Get().GetAdditionalPluginDirectories())
		{
			if (FPaths::IsUnderDirectory(Dir, AdditionalDir))
			{
				bNeedToUpdate = false;
				break;
			}
		}

		if (bNeedToUpdate)
		{
			bSearchPathChanged = GameProjectUtils::UpdateAdditionalPluginDirectory(Dir, /*bAdd*/ true);
		}
	}
	else
	{
		bool bNeedToUpdate = true;
		for (const FString& AdditionalDir : IPluginManager::Get().GetAdditionalPluginSearchPaths())
		{
			if (FPaths::IsUnderDirectory(Dir, AdditionalDir))
			{
				bNeedToUpdate = false;
				break;
			}
		}			

		if (bNeedToUpdate)
		{
			bSearchPathChanged = IPluginManager::Get().AddPluginSearchPath(Dir, /*bShouldRefresh*/ false);
		}
	}

	if (bSearchPathChanged && bRefreshPlugins)
	{
		IPluginManager::Get().RefreshPluginsList();
	}

	return bSearchPathChanged;
}

bool FPluginUtils::ValidateNewPluginNameAndLocation(const FString& PluginName, const FString& PluginLocation /*= FString()*/, FText* FailReason /*= nullptr*/)
{
	// Check whether the plugin name is valid
	if (!IsValidPluginName(PluginName, FailReason))
	{
		return false;
	}

	if (!PluginLocation.IsEmpty())
	{
		// Check if a .uplugin file exists at the specified location (if any)
		{
			const FString PluginFilePath = FPluginUtils::GetPluginFilePath(PluginLocation, PluginName);

			if (!PluginFilePath.IsEmpty() && FPaths::FileExists(PluginFilePath))
			{
				if (FailReason)
				{
					*FailReason = FText::Format(LOCTEXT("PluginPathExists", "Plugin already exists at this location\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(PluginFilePath)));
				}
				return false;
			}
		}

		// Check that the plugin location is a valid path (it doesn't have to exist; it will be created if needed)
		if (!FPaths::ValidatePath(PluginLocation, FailReason))
		{
			if (FailReason)
			{
				*FailReason = FText::Format(LOCTEXT("PluginLocationIsNotValidPath", "Plugin location is not a valid path\n{0}"), *FailReason);
			}
			return false;
		}

		// Check there isn't an existing file along the plugin folder path that would prevent creating the directory tree
		{
			FString ExistingFilePath = FPluginUtils::GetPluginFolder(PluginLocation, PluginName, true /*bFullPath*/);
			while (!ExistingFilePath.IsEmpty())
			{
				if (FPaths::FileExists(ExistingFilePath))
				{
					break;
				}
				ExistingFilePath = FPaths::GetPath(ExistingFilePath);
			}
			
			if (!ExistingFilePath.IsEmpty())
			{
				if (FailReason)
				{
					*FailReason = FText::Format(LOCTEXT("PluginLocationIsFile", "Plugin location is invalid because the following file is in the way\n{0}"), FText::FromString(ExistingFilePath));
				}
				return false;
			}
		}
	}

	// Check to see if a discovered plugin with this name exists (at any path)
	if (TSharedPtr<IPlugin> ExistingPlugin = IPluginManager::Get().FindPlugin(PluginName))
	{
		if (FailReason)
		{
			*FailReason = FText::Format(LOCTEXT("PluginNameAlreadyInUse", "Plugin name is already in use by\n{0}"), FText::FromString(FPaths::ConvertRelativePathToFull(ExistingPlugin->GetDescriptorFileName())));
		}
		return false;
	}

	return true;
}

bool FPluginUtils::IsValidPluginName(const FString& PluginName, FText* FailReason /*=nullptr*/, const FText* PluginTermReplacement /*=nullptr*/)
{
	static const FText PluginTerm = LOCTEXT("PluginTerm", "Plugin");
	const FText& PluginTermToUse = PluginTermReplacement ? *PluginTermReplacement : PluginTerm;

	// Cannot be empty
	if (PluginName.IsEmpty())
	{
		if (FailReason)
		{
			*FailReason = FText::Format(LOCTEXT("PluginNameIsEmpty", "{0} name cannot be empty"), PluginTermToUse);
		}
		return false;
	}

	// Must begin with an alphabetic character
	if (!FChar::IsAlpha(PluginName[0]))
	{
		if (FailReason)
		{
			*FailReason = FText::Format(LOCTEXT("PluginNameMustBeginWithAlphabetic", "{0} name must begin with an alphabetic character"), PluginTermToUse);
		}
		return false;
	}

	// Only allow alphanumeric characters and underscore in the name
	FString IllegalCharacters;
	for (int32 CharIdx = 0; CharIdx < PluginName.Len(); ++CharIdx)
	{
		const FString& Char = PluginName.Mid(CharIdx, 1);
		if (!FChar::IsAlnum(Char[0]) && Char != TEXT("_") && Char != TEXT("-"))
		{
			if (!IllegalCharacters.Contains(Char))
			{
				IllegalCharacters += Char;
			}
		}
	}

	if (IllegalCharacters.Len() > 0)
	{
		if (FailReason)
		{
			*FailReason = FText::Format(LOCTEXT("PluginNameContainsIllegalCharacters", "{0} name cannot contain characters such as \"{1}\""), PluginTermToUse, FText::FromString(IllegalCharacters));
		}
		return false;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	if (!AssetToolsModule.Get().IsNameAllowed(PluginName, FailReason))
	{
		return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE

#endif //if WITH_EDITOR