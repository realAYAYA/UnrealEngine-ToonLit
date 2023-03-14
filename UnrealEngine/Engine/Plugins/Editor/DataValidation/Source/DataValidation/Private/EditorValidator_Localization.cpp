// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorValidator_Localization.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/PackageLocalizationUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EditorValidator_Localization)

#define LOCTEXT_NAMESPACE "AssetValidation"

UEditorValidator_Localization::UEditorValidator_Localization()
	: Super()
{
	bIsEnabled = true;
}

bool UEditorValidator_Localization::CanValidateAsset_Implementation(UObject* InAsset) const
{
	return true;
}

EDataValidationResult UEditorValidator_Localization::ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors)
{
	static const FName NAME_AssetToools = "AssetTools";
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>(NAME_AssetToools).Get();

	static const FName NAME_AssetRegistry = "AssetRegistry";
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(NAME_AssetRegistry).Get();

	const FString AssetPathName = InAsset->GetPathName();

	if (InAsset->IsLocalizedResource())
	{
		// Get the source path for this asset
		FString SourceObjectPath;
		if (FPackageLocalizationUtil::ConvertLocalizedToSource(AssetPathName, SourceObjectPath))
		{
			const FAssetData SourceAssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(SourceObjectPath));

			// Does this source asset exist?
			// It is valid to have orphan localized assets, as they may be a direct reference of another localized asset
			if (SourceAssetData.IsValid())
			{
				// Can this type of asset be localized?
				if (TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetTools.GetAssetTypeActionsForClass(InAsset->GetClass()).Pin())
				{
					if (!AssetTypeActions->CanLocalize())
					{
						AssetFails(InAsset, FText::Format(LOCTEXT("LocalizationError_LocalizedAssetTypeCannotBeLocalized", "Localized asset is of type '{0}', which is not a type that can be localized!"), FText::FromString(InAsset->GetClass()->GetName())), ValidationErrors);
						return EDataValidationResult::Invalid;
					}
				}

				// Is the source asset a redirector?
				if (SourceAssetData.IsRedirector())
				{
					AssetFails(InAsset, LOCTEXT("LocalizationError_LocalizedAssetHasSourceRedirector", "Localized asset has a source asset that is a redirector. Did you forget to rename the localized assets too?"), ValidationErrors);
					return EDataValidationResult::Invalid;
				}

				// Is the source asset the expected type?
				if (SourceAssetData.AssetClassPath != InAsset->GetClass()->GetClassPathName())
				{
					AssetFails(InAsset, FText::Format(LOCTEXT("LocalizationError_LocalizedTypeMismatchWithSourceAsset", "Localized asset is of type '{0}', but its source asset is of type '{1}'. A localized asset must have the same type as its source asset!"), FText::FromString(InAsset->GetClass()->GetPathName()), FText::FromString(SourceAssetData.AssetClassPath.ToString())), ValidationErrors);
					return EDataValidationResult::Invalid;
				}
			}

			AssetPasses(InAsset);
			return EDataValidationResult::Valid;
		}
	}
	else
	{
		FString AssetLocalizationRoot;
		if (FPackageLocalizationUtil::GetLocalizedRoot(AssetPathName, FString(), AssetLocalizationRoot))
		{
			if (const TArray<FString>* CultureNames = FindOrCacheCulturesForLocalizedRoot(AssetLocalizationRoot))
			{
				for (const FString& CultureName : *CultureNames)
				{
					// Get the localized path for this asset and culture
					FString LocalizedObjectPath;
					if (FPackageLocalizationUtil::ConvertSourceToLocalized(AssetPathName, CultureName, LocalizedObjectPath))
					{
						const FAssetData LocalizedAssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(LocalizedObjectPath));

						if (LocalizedAssetData.IsValid())
						{
							// Can this type of asset be localized?
							if (TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetTools.GetAssetTypeActionsForClass(InAsset->GetClass()).Pin())
							{
								if (!AssetTypeActions->CanLocalize())
								{
									AssetFails(InAsset, FText::Format(LOCTEXT("LocalizationError_SourceAssetTypeCannotBeLocalized", "Source asset has a localized asset for '{0}', but is of type '{1}' which is not a type that can be localized!"), FText::FromString(CultureName), FText::FromString(InAsset->GetClass()->GetName())), ValidationErrors);
									return EDataValidationResult::Invalid;
								}
							}

							// Is the source asset a redirector?
							if (InAsset->IsA<UObjectRedirector>())
							{
								AssetFails(InAsset, FText::Format(LOCTEXT("LocalizationError_SourceIsRedirector", "Source asset is a redirector but has a localized asset for '{0}'. Did you forget to rename the localized assets too?"), FText::FromString(CultureName)), ValidationErrors);
								return EDataValidationResult::Invalid;
							}

							// Is the localized asset the expected type?
							if (LocalizedAssetData.AssetClassPath != InAsset->GetClass()->GetClassPathName())
							{
								AssetFails(InAsset, FText::Format(LOCTEXT("LocalizationError_SourceTypeMismatchWithLocalizedAsset", "Source asset is of type '{0}', but its localized asset for '{1}' is of type '{2}'. A localized asset must have the same type as its source asset!"), FText::FromString(InAsset->GetClass()->GetPathName()), FText::FromString(CultureName), FText::FromString(LocalizedAssetData.AssetClassPath.ToString())), ValidationErrors);
								return EDataValidationResult::Invalid;
							}
						}
					}
				}

				AssetPasses(InAsset);
				return EDataValidationResult::Valid;
			}
		}
	}

	return EDataValidationResult::NotValidated;
}

const TArray<FString>* UEditorValidator_Localization::FindOrCacheCulturesForLocalizedRoot(const FString& InLocalizedRootPath)
{
	if (const TArray<FString>* CachedCultures = CachedCulturesForLocalizedRoots.Find(InLocalizedRootPath))
	{
		return CachedCultures;
	}

	FString LocalizedRootPathFilePath;
	if (FPackageName::TryConvertLongPackageNameToFilename(InLocalizedRootPath, LocalizedRootPathFilePath))
	{
		TArray<FString> CultureNames;
		IFileManager::Get().IterateDirectory(*LocalizedRootPathFilePath, [&CultureNames](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory)
			{
				// UE localization resource folders use "en-US" style while ICU uses "en_US"
				FString LocalizationFolder = FPaths::GetCleanFilename(FilenameOrDirectory);
				FString CanonicalName = FCulture::GetCanonicalName(LocalizationFolder);
				CultureNames.AddUnique(MoveTemp(CanonicalName));
			}
			return true;
		});
		CultureNames.Sort();

		return &CachedCulturesForLocalizedRoots.Add(InLocalizedRootPath, MoveTemp(CultureNames));
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE

