// Copyright Epic Games, Inc. All Rights Reserved.

#include "UserGeneratedContentLocalization.h"

#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeExit.h"
#include "JsonObjectConverter.h"
#include "Interfaces/IPluginManager.h"

#include "Internationalization/Culture.h"
#include "Internationalization/CultureFilter.h"
#include "Internationalization/TextLocalizationManager.h"
#include "TextLocalizationResourceGenerator.h"
#include "LocalizationConfigurationScript.h"
#include "LocalizationDelegates.h"
#include "LocTextHelper.h"
#include "PortableObjectFormatDOM.h"
#include "SourceControlHelpers.h"

#define LOCTEXT_NAMESPACE "UserGeneratedContentLocalization"

DEFINE_LOG_CATEGORY_STATIC(LogUGCLocalization, Log, All);

namespace UserGeneratedContentLocalization
{

bool bAlwaysExportFullGatherLog = false;
FAutoConsoleVariableRef CExportFullGatherLog(TEXT("Localization.UGC.AlwaysExportFullGatherLog"), bAlwaysExportFullGatherLog, TEXT("True to export the full gather log from running localization commandlet, even if there we no errors"));

}

void FUserGeneratedContentLocalizationDescriptor::InitializeFromProject(const ELocalizedTextSourceCategory LocalizationCategory)
{
	ELocalizationLoadFlags LoadFlags = ELocalizationLoadFlags::None;
	switch (LocalizationCategory)
	{
	case ELocalizedTextSourceCategory::Game:
		LoadFlags |= ELocalizationLoadFlags::Game;
		break;

	case ELocalizedTextSourceCategory::Engine:
		LoadFlags |= ELocalizationLoadFlags::Engine;
		break;

	case ELocalizedTextSourceCategory::Editor:
		LoadFlags |= ELocalizationLoadFlags::Editor;
		break;

	default:
		checkf(false, TEXT("Unexpected ELocalizedTextSourceCategory!"));
		break;
	}

	NativeCulture = FTextLocalizationManager::Get().GetNativeCultureName(LocalizationCategory);
	if (NativeCulture.IsEmpty())
	{
		NativeCulture = TEXT("en");
	}
	CulturesToGenerate = FTextLocalizationManager::Get().GetLocalizedCultureNames(LoadFlags);

	// Filter any cultures that are disabled in shipping or via UGC loc settings
	{
		const FCultureFilter CultureFilter(EBuildConfiguration::Shipping, ELocalizationLoadFlags::Engine | LoadFlags);
		CulturesToGenerate.RemoveAll([&CultureFilter](const FString& Culture)
		{
			return !CultureFilter.IsCultureAllowed(Culture)
				|| GetDefault<UUserGeneratedContentLocalizationSettings>()->CulturesToDisable.Contains(Culture);
		});
	}
}

bool FUserGeneratedContentLocalizationDescriptor::Validate(const FUserGeneratedContentLocalizationDescriptor& DefaultDescriptor)
{
	int32 NumCulturesFixed = 0;

	if (!DefaultDescriptor.CulturesToGenerate.Contains(NativeCulture))
	{
		++NumCulturesFixed;
		NativeCulture = DefaultDescriptor.NativeCulture;
	}

	NumCulturesFixed += CulturesToGenerate.RemoveAll([&DefaultDescriptor](const FString& Culture)
	{
		return !DefaultDescriptor.CulturesToGenerate.Contains(Culture);
	});

	return NumCulturesFixed == 0;
}

bool FUserGeneratedContentLocalizationDescriptor::ToJsonObject(TSharedPtr<FJsonObject>& OutJsonObject) const
{
	OutJsonObject = FJsonObjectConverter::UStructToJsonObject(*this);
	return OutJsonObject.IsValid();
}

bool FUserGeneratedContentLocalizationDescriptor::ToJsonString(FString& OutJsonString) const
{
	return FJsonObjectConverter::UStructToJsonObjectString(*this, OutJsonString);
}

bool FUserGeneratedContentLocalizationDescriptor::ToJsonFile(const TCHAR* InFilename) const
{
	FString UGCLocDescData;
	return ToJsonString(UGCLocDescData) 
		&& FFileHelper::SaveStringToFile(UGCLocDescData, InFilename, FFileHelper::EEncodingOptions::ForceUTF8);
}

bool FUserGeneratedContentLocalizationDescriptor::FromJsonObject(TSharedRef<const FJsonObject> InJsonObject)
{
	return FJsonObjectConverter::JsonObjectToUStruct(ConstCastSharedRef<FJsonObject>(InJsonObject), this);
}

bool FUserGeneratedContentLocalizationDescriptor::FromJsonString(const FString& InJsonString)
{
	return FJsonObjectConverter::JsonObjectStringToUStruct(InJsonString, this);
}

bool FUserGeneratedContentLocalizationDescriptor::FromJsonFile(const TCHAR* InFilename)
{
	FString UGCLocDescData;
	return FFileHelper::LoadFileToString(UGCLocDescData, InFilename)
		&& FromJsonString(UGCLocDescData);
}

namespace UserGeneratedContentLocalization
{

FString GetLocalizationScratchDirectory()
{
	return FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Localization"));
}

FString GetLocalizationScratchDirectory(const TSharedRef<IPlugin>& Plugin)
{
	return FPaths::Combine(GetLocalizationScratchDirectory(), Plugin->GetName());
}

FString GetLocalizationTargetDirectory(const FString& PluginName, const FString& PluginContentDirectory)
{
	return FPaths::Combine(PluginContentDirectory, TEXT("Localization"), PluginName);
}

FString GetLocalizationTargetDirectory(const TSharedRef<IPlugin>& Plugin)
{
	return GetLocalizationTargetDirectory(Plugin->GetName(), Plugin->GetContentDir());
}

void PreWriteFileWithSCC(const FString& Filename)
{
	if (USourceControlHelpers::IsEnabled())
	{
		// If the file already already exists, then check it out before writing to it
		if (FPaths::FileExists(Filename) && !USourceControlHelpers::CheckOutFile(Filename))
		{
			UE_LOG(LogUGCLocalization, Error, TEXT("Failed to check out file '%s'. %s"), *Filename, *USourceControlHelpers::LastErrorMsg().ToString());
		}
	}
}

void PostWriteFileWithSCC(const FString& Filename)
{
	if (USourceControlHelpers::IsEnabled())
	{
		// If the file didn't exist before then this will add it, otherwise it will do nothing
		if (USourceControlHelpers::CheckOutOrAddFile(Filename))
		{
			// Discard the checkout if the file has no changes
			USourceControlHelpers::RevertUnchangedFile(Filename, /*bSilent*/true);
		}
		else
		{
			UE_LOG(LogUGCLocalization, Error, TEXT("Failed to check out file '%s'. %s"), *Filename, *USourceControlHelpers::LastErrorMsg().ToString());
		}
	}
}

bool ExportLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FExportLocalizationOptions& ExportOptions, TFunctionRef<int32(const FString&, FString&)> CommandletExecutor)
{
	if (ExportOptions.UGCLocDescriptor.NativeCulture.IsEmpty())
	{
		UE_LOG(LogUGCLocalization, Error, TEXT("Localization export options did not have a 'NativeCulture' set"));
		return false;
	}

	// Create a scratch directory for the temporary localization data
	const FString RootLocalizationScratchDirectory = GetLocalizationScratchDirectory();
	IFileManager::Get().MakeDirectory(*RootLocalizationScratchDirectory, /*bTree*/true);
	ON_SCOPE_EXIT
	{
		if (ExportOptions.bAutoCleanup)
		{
			// Delete the entire scratch directory
			IFileManager::Get().DeleteDirectory(*RootLocalizationScratchDirectory, /*bRequireExists*/false, /*bTree*/true);
		}
	};

	// Make sure we're also exporting localization for the native culture
	TArray<FString> CulturesToGenerate = ExportOptions.UGCLocDescriptor.CulturesToGenerate;
	CulturesToGenerate.AddUnique(ExportOptions.UGCLocDescriptor.NativeCulture);

	// Localization data stored per-plugin
	TArray<FString, TInlineAllocator<1>> GatherConfigFilenames;
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		const FString PluginLocalizationScratchDirectory = GetLocalizationScratchDirectory(Plugin);
		const FString PluginLocalizationTargetDirectory = GetLocalizationTargetDirectory(Plugin);

		// Seed the scratch directory with the current PO files for this plugin, so that the loc gather will import and preserve any existing translation data
		{
			bool bCopiedAllFiles = true;
			IFileManager::Get().IterateDirectoryRecursively(*PluginLocalizationTargetDirectory, [&Plugin, &PluginLocalizationScratchDirectory, &PluginLocalizationTargetDirectory, &bCopiedAllFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
			{
				if (!bIsDirectory && FPathViews::GetExtension(FilenameOrDirectory) == TEXTVIEW("po"))
				{
					FString DestinationFilename = FilenameOrDirectory;
					if (DestinationFilename.ReplaceInline(*PluginLocalizationTargetDirectory, *PluginLocalizationScratchDirectory) > 0)
					{
						if (IFileManager::Get().Copy(*DestinationFilename, FilenameOrDirectory) == COPY_OK)
						{
							UE_LOG(LogUGCLocalization, Log, TEXT("Imported existing .po file for '%s': %s"), *Plugin->GetName(), FilenameOrDirectory);
						}
						else
						{
							bCopiedAllFiles = false;
							UE_LOG(LogUGCLocalization, Warning, TEXT("Failed to import existing .po file for '%s': %s"), *Plugin->GetName(), FilenameOrDirectory);
						}
					}
				}
				return true;
			});
			if (!bCopiedAllFiles)
			{
				return false;
			}
		}

		// Build the gather config
		{
			// Build up a basic localization config that will do the following:
			//  1) Gather source/assets in the current plugin
			//  2) Import any existing PO file data
			//  3) Export new PO file data

			FLocalizationConfigurationScript GatherConfig;
			int32 GatherStepIndex = 0;

			// Common
			{
				FConfigSection ConfigSection;

				ConfigSection.Add(TEXT("SourcePath"), FPaths::ConvertRelativePathToFull(PluginLocalizationScratchDirectory));
				ConfigSection.Add(TEXT("DestinationPath"), FPaths::ConvertRelativePathToFull(PluginLocalizationScratchDirectory));

				ConfigSection.Add(TEXT("ManifestName"), FString::Printf(TEXT("%s.manifest"), *Plugin->GetName()));
				ConfigSection.Add(TEXT("ArchiveName"), FString::Printf(TEXT("%s.archive"), *Plugin->GetName()));
				ConfigSection.Add(TEXT("PortableObjectName"), FString::Printf(TEXT("%s.po"), *Plugin->GetName()));

				ConfigSection.Add(TEXT("GatheredSourceBasePath"), FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()));

				ConfigSection.Add(TEXT("CopyrightNotice"), ExportOptions.CopyrightNotice);

				ConfigSection.Add(TEXT("NativeCulture"), ExportOptions.UGCLocDescriptor.NativeCulture);
				for (const FString& CultureToGenerate : CulturesToGenerate)
				{
					ConfigSection.Add(TEXT("CulturesToGenerate"), *CultureToGenerate);
				}
				
				GatherConfig.AddCommonSettings(MoveTemp(ConfigSection));
			}

			// Gather source
			if (ExportOptions.bGatherSource)
			{
				const FString PluginConfigDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config")));
				const FString PluginSourceDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Source")));

				TArray<FString, TInlineAllocator<2>> SearchDirectoryPaths;
				if (FPaths::DirectoryExists(PluginConfigDir))
				{
					SearchDirectoryPaths.Add(PluginConfigDir);
				}
				if (FPaths::DirectoryExists(PluginSourceDir))
				{
					SearchDirectoryPaths.Add(PluginSourceDir);
				}

				// Only gather from source if there's valid paths to gather from, as otherwise the commandlet will error
				if (SearchDirectoryPaths.Num() > 0)
				{
					FConfigSection ConfigSection;
					ConfigSection.Add(TEXT("CommandletClass"), TEXT("GatherTextFromSource"));

					ConfigSection.Add(TEXT("FileNameFilters"), TEXT("*.h"));
					ConfigSection.Add(TEXT("FileNameFilters"), TEXT("*.cpp"));
					ConfigSection.Add(TEXT("FileNameFilters"), TEXT("*.inl"));
					ConfigSection.Add(TEXT("FileNameFilters"), TEXT("*.ini"));

					for (const FString& SearchDirectoryPath : SearchDirectoryPaths)
					{
						ConfigSection.Add(TEXT("SearchDirectoryPaths"), SearchDirectoryPath);
					}
					
					GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
				}
			}

			// Gather assets
			if (ExportOptions.bGatherAssets && Plugin->CanContainContent())
			{
				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("GatherTextFromAssets"));

				ConfigSection.Add(TEXT("PackageFileNameFilters"), TEXT("*.uasset"));
				ConfigSection.Add(TEXT("PackageFileNameFilters"), TEXT("*.umap"));

				ConfigSection.Add(TEXT("IncludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("*"))));

				ConfigSection.Add(TEXT("ExcludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("Localization"), TEXT("*"))));
				ConfigSection.Add(TEXT("ExcludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("L10N"), TEXT("*"))));

				if (const FString* CollectionFilter = ExportOptions.PluginNameToCollectionNameFilter.Find(Plugin->GetName()))
				{
					ConfigSection.Add(TEXT("CollectionFilters"), *CollectionFilter);
				}
				
				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Gather Verse
			if (ExportOptions.bGatherVerse && Plugin->CanContainVerse())
			{
				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("GatherTextFromVerse"));

				ConfigSection.Add(TEXT("IncludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), TEXT("*"))));

				ConfigSection.Add(TEXT("ExcludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("Localization"), TEXT("*"))));
				ConfigSection.Add(TEXT("ExcludePathFilters"), FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("L10N"), TEXT("*"))));
				
				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Generate manifest
			{
				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("GenerateGatherManifest"));
				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Generate archive
			{
				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("GenerateGatherArchive"));
				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Import PO
			{
				// Read the UGC localization descriptor settings that were used to generate this localization data, as we should import against those
				FUserGeneratedContentLocalizationDescriptor UGCLocDescriptorForImport;
				{
					const FString UGCLocFilename = FPaths::Combine(PluginLocalizationTargetDirectory, FString::Printf(TEXT("%s.ugcloc"), *Plugin->GetName()));
					if (!UGCLocDescriptorForImport.FromJsonFile(*UGCLocFilename))
					{
						UGCLocDescriptorForImport = ExportOptions.UGCLocDescriptor;
					}
				}

				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("InternationalizationExport"));

				ConfigSection.Add(TEXT("bImportLoc"), TEXT("true"));

				ConfigSection.Add(TEXT("POFormat"), StaticEnum<EPortableObjectFormat>()->GetNameStringByValue((int64)UGCLocDescriptorForImport.PoFormat));

				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Export PO
			{
				FConfigSection ConfigSection;
				ConfigSection.Add(TEXT("CommandletClass"), TEXT("InternationalizationExport"));

				ConfigSection.Add(TEXT("bExportLoc"), TEXT("true"));

				ConfigSection.Add(TEXT("POFormat"), StaticEnum<EPortableObjectFormat>()->GetNameStringByValue((int64)ExportOptions.UGCLocDescriptor.PoFormat));

				ConfigSection.Add(TEXT("ShouldPersistCommentsOnExport"), TEXT("true"));

				GatherConfig.AddGatherTextStep(GatherStepIndex++, MoveTemp(ConfigSection));
			}

			// Write config
			{
				GatherConfig.Dirty = true;

				FString GatherConfigFilename = FPaths::ConvertRelativePathToFull(RootLocalizationScratchDirectory / FString::Printf(TEXT("%s.ini"), *Plugin->GetName()));
				if (GatherConfig.Write(GatherConfigFilename))
				{
					GatherConfigFilenames.Add(MoveTemp(GatherConfigFilename));
				}
				else
				{
					UE_LOG(LogUGCLocalization, Error, TEXT("Failed to write gather config for '%s': %s"), *Plugin->GetName(), *GatherConfigFilename);
					return false;
				}
			}
		}
	}
	
	// Run the commandlet
	if (GatherConfigFilenames.Num() > 0)
	{
		FString CommandletOutput;
		const int32 ReturnCode = CommandletExecutor(FString::Join(GatherConfigFilenames, TEXT(";")), CommandletOutput);

		// Verify the commandlet finished cleanly
		bool bGatherFailed = true;
		if (ReturnCode == 0)
		{
			bGatherFailed = false;
		}
		else
		{
			// The commandlet can sometimes exit with a non-zero return code for reasons unrelated to the localization export
			// If this happens, check to see whether the GatherText commandlet itself exited with a zero return code
			if (CommandletOutput.Contains(TEXT("GatherText completed with exit code 0"), ESearchCase::CaseSensitive))
			{
				bGatherFailed = false;
				UE_LOG(LogUGCLocalization, Warning, TEXT("Localization commandlet finished with a non-zero exit code, but GatherText finished with a zero exit code. Considering the export a success, but there may be errors or omissions in the exported data."));
			}
		}

		// Log the output and result of the commandlet
		{
			UE_LOG(LogUGCLocalization, Display, TEXT("Localization commandlet finished with exit code %d"), ReturnCode);

			if (bGatherFailed || UserGeneratedContentLocalization::bAlwaysExportFullGatherLog)
			{
				TArray<FString> CommandletOutputLines;
				CommandletOutput.ParseIntoArrayLines(CommandletOutputLines);

				for (const FString& CommandletOutputLine : CommandletOutputLines)
				{
					UE_LOG(LogUGCLocalization, Display, TEXT("    %s"), *CommandletOutputLine);
				}
			}
		}

		// If the gather failed then skip the rest of the process
		if (bGatherFailed)
		{
			return false;
		}
	}

	// Copy any updated PO files back to the plugins and write out the localization settings used to generate them
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		const FString PluginLocalizationScratchDirectory = GetLocalizationScratchDirectory(Plugin);
		const FString PluginLocalizationTargetDirectory = GetLocalizationTargetDirectory(Plugin);

		// Write the UGC localization descriptor settings that were used to generate this localization data
		// That will be needed to handle compilation, but also to handle import correctly if the descriptor settings change later
		{
			const FString UGCLocFilename = FPaths::Combine(PluginLocalizationTargetDirectory, FString::Printf(TEXT("%s.ugcloc"), *Plugin->GetName()));

			PreWriteFileWithSCC(UGCLocFilename);
			if (ExportOptions.UGCLocDescriptor.ToJsonFile(*UGCLocFilename))
			{
				PostWriteFileWithSCC(UGCLocFilename);
				UE_LOG(LogUGCLocalization, Log, TEXT("Updated .ugcloc file for '%s': %s"), *Plugin->GetName(), *UGCLocFilename);
			}
			else
			{
				UE_LOG(LogUGCLocalization, Warning, TEXT("Failed to update .ugcloc file for '%s': %s"), *Plugin->GetName(), *UGCLocFilename);
			}
		}

		IFileManager::Get().IterateDirectoryRecursively(*PluginLocalizationScratchDirectory, [&Plugin, &PluginLocalizationScratchDirectory, &PluginLocalizationTargetDirectory](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (!bIsDirectory && FPathViews::GetExtension(FilenameOrDirectory) == TEXTVIEW("po"))
			{
				FString DestinationFilename = FilenameOrDirectory;
				if (DestinationFilename.ReplaceInline(*PluginLocalizationScratchDirectory, *PluginLocalizationTargetDirectory) > 0)
				{
					PreWriteFileWithSCC(DestinationFilename);
					if (IFileManager::Get().Copy(*DestinationFilename, FilenameOrDirectory) == COPY_OK)
					{
						PostWriteFileWithSCC(DestinationFilename);
						UE_LOG(LogUGCLocalization, Log, TEXT("Updated .po file for '%s': %s"), *Plugin->GetName(), *DestinationFilename);
					}
					else
					{
						UE_LOG(LogUGCLocalization, Warning, TEXT("Failed to update .po file for '%s': %s"), *Plugin->GetName(), *DestinationFilename);
					}
				}
			}
			return true;
		});

		if (ExportOptions.bUpdatePluginDescriptor)
		{
			FPluginDescriptor PluginDescriptor = Plugin->GetDescriptor();
			if (!PluginDescriptor.LocalizationTargets.ContainsByPredicate([&Plugin](const FLocalizationTargetDescriptor& LocalizationTargetDescriptor) { return LocalizationTargetDescriptor.Name == Plugin->GetName(); }))
			{
				FLocalizationTargetDescriptor& LocalizationTargetDescriptor = PluginDescriptor.LocalizationTargets.AddDefaulted_GetRef();
				LocalizationTargetDescriptor.Name = Plugin->GetName();
				switch (ExportOptions.LocalizationCategory)
				{
				case ELocalizedTextSourceCategory::Game:
					LocalizationTargetDescriptor.LoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::Game;
					break;

				case ELocalizedTextSourceCategory::Engine:
					LocalizationTargetDescriptor.LoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::Always;
					break;

				case ELocalizedTextSourceCategory::Editor:
					LocalizationTargetDescriptor.LoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::Editor;
					break;

				default:
					checkf(false, TEXT("Unexpected ELocalizedTextSourceCategory!"));
					break;
				}

				FText DescriptorUpdateFailureReason;
				PreWriteFileWithSCC(Plugin->GetDescriptorFileName());
				if (Plugin->UpdateDescriptor(PluginDescriptor, DescriptorUpdateFailureReason))
				{
					PostWriteFileWithSCC(Plugin->GetDescriptorFileName());
					UE_LOG(LogUGCLocalization, Log, TEXT("Updated .uplugin file for '%s'"), *Plugin->GetName());
				}
				else
				{
					UE_LOG(LogUGCLocalization, Warning, TEXT("Failed to update .uplugin file for '%s': %s"), *Plugin->GetName(), *DescriptorUpdateFailureReason.ToString());
				}
			}
		}

		LocalizationDelegates::OnLocalizationTargetDataUpdated.Broadcast(PluginLocalizationTargetDirectory);
	}

	return true;
}

bool CompileLocalizationTarget(const FString& LocalizationTargetDirectory, const FLocTextHelper& LocTextHelper)
{
	const FString LocMetaName = FString::Printf(TEXT("%s.locmeta"), *LocTextHelper.GetTargetName());
	const FString LocResName = FString::Printf(TEXT("%s.locres"), *LocTextHelper.GetTargetName());

	// Generate the LocMeta file
	{
		FTextLocalizationMetaDataResource LocMeta;
		if (FTextLocalizationResourceGenerator::GenerateLocMeta(LocTextHelper, LocResName, LocMeta))
		{
			if (!LocMeta.SaveToFile(LocalizationTargetDirectory / LocMetaName))
			{
				UE_LOG(LogUGCLocalization, Error, TEXT("Failed to save LocMeta file for '%s'"), *LocTextHelper.GetTargetName());
				return false;
			}
		}
		else
		{
			UE_LOG(LogUGCLocalization, Error, TEXT("Failed to generate LocMeta file for '%s'"), *LocTextHelper.GetTargetName());
			return false;
		}
	}

	// Generate the LocRes files
	for (const FString& CultureToGenerate : LocTextHelper.GetAllCultures())
	{
		FTextLocalizationResource LocRes;
		TMap<FName, TSharedRef<FTextLocalizationResource>> PerPlatformLocRes;
		if (FTextLocalizationResourceGenerator::GenerateLocRes(LocTextHelper, CultureToGenerate, EGenerateLocResFlags::None, LocalizationTargetDirectory / CultureToGenerate / LocResName, LocRes, PerPlatformLocRes))
		{
			checkf(PerPlatformLocRes.Num() == 0, TEXT("UGC localization does not support per-platform LocRes!"));

			if (!LocRes.SaveToFile(LocalizationTargetDirectory / CultureToGenerate / LocResName))
			{
				UE_LOG(LogUGCLocalization, Error, TEXT("Failed to save LocRes file for '%s' (culture '%s')"), *LocTextHelper.GetTargetName(), *CultureToGenerate);
				return false;
			}
		}
		else
		{
			UE_LOG(LogUGCLocalization, Error, TEXT("Failed to generate LocRes file for '%s' (culture '%s')"), *LocTextHelper.GetTargetName(), *CultureToGenerate);
			return false;
		}
	}

	LocalizationDelegates::OnLocalizationTargetDataUpdated.Broadcast(LocalizationTargetDirectory);

	return true;
}

bool CompileLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor)
{
	// Localization data is stored per-plugin
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		const FString PluginLocalizationTargetDirectory = GetLocalizationTargetDirectory(Plugin);
		if (!CompileLocalization(Plugin->GetName(), PluginLocalizationTargetDirectory, PluginLocalizationTargetDirectory, DefaultDescriptor))
		{
			return false;
		}
	}

	return true;
}

bool CompileLocalization(const FString& PluginName, const FString& PluginInputContentDirectory, const FString& PluginOutputContentDirectory, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor)
{
	// Load the localization data so that we can compile it
	TSharedPtr<FLocTextHelper> LocTextHelper;
	const ELoadLocalizationResult LoadResult = LoadLocalization(PluginName, PluginInputContentDirectory, LocTextHelper, DefaultDescriptor);
	if (LoadResult == ELoadLocalizationResult::NoData)
	{
		// Nothing to do
		return true;
	}
	if (LoadResult == ELoadLocalizationResult::Failed)
	{
		// Failed to load, so can't compile
		return false;
	}
	check(LoadResult == ELoadLocalizationResult::Success);

	const FString PluginLocalizationOutputDirectory = GetLocalizationTargetDirectory(PluginName, PluginOutputContentDirectory);
	return CompileLocalizationTarget(PluginLocalizationOutputDirectory, *LocTextHelper);
}

bool ImportPortableObject(const FString& LocalizationTargetDirectory, const FString& CultureToLoad, const EPortableObjectFormat PoFormat, FLocTextHelper& LocTextHelper)
{
	const bool bIsNativeCulture = CultureToLoad == LocTextHelper.GetNativeCulture();

	const FString POFilename = LocalizationTargetDirectory / CultureToLoad / FString::Printf(TEXT("%s.po"), *LocTextHelper.GetTargetName());

	FString POFileData;
	FPortableObjectFormatDOM POFile;
	if (!FFileHelper::LoadFileToString(POFileData, *POFilename) || !POFile.FromString(POFileData))
	{
		return false;
	}

	// Process each PO entry
	for (auto EntryPairIter = POFile.GetEntriesIterator(); EntryPairIter; ++EntryPairIter)
	{
		const TSharedPtr<FPortableObjectEntry>& POEntry = EntryPairIter->Value;
		if (POEntry->MsgId.IsEmpty() || POEntry->MsgStr.Num() == 0 || POEntry->MsgStr[0].IsEmpty())
		{
			// We ignore the header entry or entries with no translation.
			continue;
		}

		FString Namespace;
		FString Key;
		FString SourceText;
		FString Translation;
		PortableObjectPipeline::ParseBasicPOFileEntry(*POEntry, Namespace, Key, SourceText, Translation, ELocalizedTextCollapseMode::IdenticalTextIdAndSource, PoFormat);

		// PO files don't contain the key meta-data so we can't reconstruct this
		// Key meta-data only exists to force the PO file export an ID that contains both the namespace AND key though, so it doesn't matter if it's lost here as it won't affect the LocRes generation
		TSharedPtr<FLocMetadataObject> KeyMetadataObj = nullptr;

		// Not all formats contain the source string, so if the source is empty then 
		// we'll assume the translation was made against the most up-to-date source
		if (SourceText.IsEmpty())
		{
			if (bIsNativeCulture)
			{
				SourceText = Translation;
			}
			else if (const TSharedPtr<FArchiveEntry> NativeEntry = LocTextHelper.FindTranslation(LocTextHelper.GetNativeCulture(), Namespace, Key, KeyMetadataObj))
			{
				SourceText = NativeEntry->Translation.Text;
			}
		}

		// If this is the native culture then we also add it as source in the manifest
		if (bIsNativeCulture)
		{
			FManifestContext ManifestContext;
			ManifestContext.SourceLocation = POEntry->ReferenceComments.Num() > 0 ? POEntry->ReferenceComments[0] : FString();
			ManifestContext.Key = Key;
			ManifestContext.KeyMetadataObj = KeyMetadataObj;
			LocTextHelper.AddSourceText(Namespace, FLocItem(SourceText), ManifestContext);
		}

		// All cultures add this info as a translation
		LocTextHelper.AddTranslation(CultureToLoad, Namespace, Key, KeyMetadataObj, FLocItem(SourceText), FLocItem(Translation), /*bIsOptional*/false);
	}

	return true;
}

ELoadLocalizationResult LoadLocalization(const FString& PluginName, const FString& PluginContentDirectory, TSharedPtr<FLocTextHelper>& OutLocTextHelper, const FUserGeneratedContentLocalizationDescriptor* DefaultDescriptor)
{
	const FString PluginLocalizationTargetDirectory = GetLocalizationTargetDirectory(PluginName, PluginContentDirectory);

	const FString UGCLocFilename = FPaths::Combine(PluginLocalizationTargetDirectory, FString::Printf(TEXT("%s.ugcloc"), *PluginName));
	if (!FPaths::FileExists(UGCLocFilename))
	{
		// Nothing to do
		return ELoadLocalizationResult::NoData;
	}

	// Read the UGC localization descriptor settings that were used to generate this localization data
	FUserGeneratedContentLocalizationDescriptor UGCLocDescriptor;
	if (!UGCLocDescriptor.FromJsonFile(*UGCLocFilename))
	{
		UE_LOG(LogUGCLocalization, Error, TEXT("Failed to load localization descriptor for '%s'"), *PluginName);
		return ELoadLocalizationResult::Failed;
	}

	// Validate the loaded settings against the given default
	// This will remove/reset any invalid data
	if (DefaultDescriptor)
	{
		UGCLocDescriptor.Validate(*DefaultDescriptor);
	}

	// Create in-memory versions of the manifest/archives that we will populate below
	OutLocTextHelper = MakeShared<FLocTextHelper>(PluginLocalizationTargetDirectory, FString::Printf(TEXT("%s.manifest"), *PluginName), FString::Printf(TEXT("%s.archive"), *PluginName), UGCLocDescriptor.NativeCulture, UGCLocDescriptor.CulturesToGenerate, nullptr);
	OutLocTextHelper->LoadAll(ELocTextHelperLoadFlags::Create);

	// Import each PO file data, as we'll use it to generate the LocRes (via FLocTextHelper)
	// We always process the native culture first as it's also used to populate the manifest with the source texts
	if (!ImportPortableObject(PluginLocalizationTargetDirectory, OutLocTextHelper->GetNativeCulture(), UGCLocDescriptor.PoFormat, *OutLocTextHelper))
	{
		UE_LOG(LogUGCLocalization, Error, TEXT("Failed to load PO file for '%s' (culture '%s')"), *PluginName, *OutLocTextHelper->GetNativeCulture());
		return ELoadLocalizationResult::Failed;
	}
	for (const FString& CultureToGenerate : OutLocTextHelper->GetAllCultures())
	{
		if (CultureToGenerate == OutLocTextHelper->GetNativeCulture())
		{
			continue;
		}

		if (!ImportPortableObject(PluginLocalizationTargetDirectory, CultureToGenerate, UGCLocDescriptor.PoFormat, *OutLocTextHelper))
		{
			UE_LOG(LogUGCLocalization, Error, TEXT("Failed to load PO file for '%s' (culture '%s')"), *PluginName, *CultureToGenerate);
			return ELoadLocalizationResult::Failed;
		}
	}

	return ELoadLocalizationResult::Success;
}

void CleanupLocalization(TArrayView<const TSharedRef<IPlugin>> Plugins, const FUserGeneratedContentLocalizationDescriptor& DefaultDescriptor, const bool bSilent)
{
	// Make sure we also consider localization for the native culture
	TArray<FString> CulturesToGenerate = DefaultDescriptor.CulturesToGenerate;
	if (!DefaultDescriptor.NativeCulture.IsEmpty())
	{
		CulturesToGenerate.AddUnique(DefaultDescriptor.NativeCulture);
	}

	TArray<FString> LocalizationFilesToCleanup;
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		const FString PluginLocalizationTargetDirectory = GetLocalizationTargetDirectory(Plugin);

		// Find any leftover PO files to cleanup
		const FString PluginPOFilename = Plugin->GetName() + TEXT(".po");
		IFileManager::Get().IterateDirectory(*PluginLocalizationTargetDirectory, [&LocalizationFilesToCleanup, &PluginPOFilename, &CulturesToGenerate](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
		{
			if (bIsDirectory)
			{
				// Note: This looks for PO files rather than the folders, as the folders may just be empty vestiges from a P4 sync without rmdir set
				const FString PluginPOFile = FilenameOrDirectory / PluginPOFilename;
				if (FPaths::FileExists(PluginPOFile))
				{
					const FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
					const FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
					if (!CulturesToGenerate.Contains(CanonicalName))
					{
						LocalizationFilesToCleanup.Add(PluginPOFile);
					}
				}
			}
			return true;
		});

		// If we aren't exporting any cultures, then also cleanup any existing descriptor file
		if (CulturesToGenerate.Num() == 0)
		{
			const FString PluginUGCLocFilename = FPaths::Combine(PluginLocalizationTargetDirectory, FString::Printf(TEXT("%s.ugcloc"), *Plugin->GetName()));
			if (FPaths::FileExists(PluginUGCLocFilename))
			{
				LocalizationFilesToCleanup.Add(PluginUGCLocFilename);
			}
		}
	}

	if (LocalizationFilesToCleanup.Num() > 0)
	{
		auto GetCleanupLocalizationMessage = [&LocalizationFilesToCleanup]()
		{
			FTextBuilder CleanupLocalizationMessageBuilder;
			CleanupLocalizationMessageBuilder.AppendLine(LOCTEXT("CleanupLocalization.Message", "Would you like to cleanup the following localization data?"));
			for (const FString& LeftoverPOFile : LocalizationFilesToCleanup)
			{
				CleanupLocalizationMessageBuilder.AppendLineFormat(LOCTEXT("CleanupLocalization.MessageLine", "    \u2022 {0}"), FText::AsCultureInvariant(LeftoverPOFile));
			}
			return CleanupLocalizationMessageBuilder.ToText();
		};

		if (bSilent || FMessageDialog::Open(EAppMsgType::YesNo, GetCleanupLocalizationMessage(), LOCTEXT("CleanupLocalization.Title", "Cleanup localization data?")) == EAppReturnType::Yes)
		{
			// Cleanup the files
			if (USourceControlHelpers::IsEnabled())
			{
				USourceControlHelpers::MarkFilesForDelete(LocalizationFilesToCleanup);
			}
			else
			{
				for (const FString& LocalizationFileToCleanup : LocalizationFilesToCleanup)
				{
					IFileManager::Get().Delete(*LocalizationFileToCleanup);
				}
			}

			// Cleanup the folders containing those files (will do nothing if the folder isn't actually empty)
			for (const FString& LocalizationFileToCleanup : LocalizationFilesToCleanup)
			{
				const FString LocalizationPathToCleanup = FPaths::GetPath(LocalizationFileToCleanup);
				IFileManager::Get().DeleteDirectory(*LocalizationPathToCleanup);
			}

			// If we aren't exporting any cultures, then also cleanup any plugin references to the localization data
			if (CulturesToGenerate.Num() == 0)
			{
				for (const TSharedRef<IPlugin>& Plugin : Plugins)
				{
					FPluginDescriptor PluginDescriptor = Plugin->GetDescriptor();
					if (PluginDescriptor.LocalizationTargets.RemoveAll([&Plugin](const FLocalizationTargetDescriptor& LocalizationTargetDescriptor) { return LocalizationTargetDescriptor.Name == Plugin->GetName(); }) > 0)
					{
						FText DescriptorUpdateFailureReason;
						PreWriteFileWithSCC(Plugin->GetDescriptorFileName());
						if (Plugin->UpdateDescriptor(PluginDescriptor, DescriptorUpdateFailureReason))
						{
							PostWriteFileWithSCC(Plugin->GetDescriptorFileName());
						}
					}
				}
			}
		}
	}
}

}

#undef LOCTEXT_NAMESPACE
