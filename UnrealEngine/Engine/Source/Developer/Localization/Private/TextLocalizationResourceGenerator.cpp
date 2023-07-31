// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextLocalizationResourceGenerator.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "Misc/ConfigCacheIni.h"
#include "LocTextHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResourceGenerator, Log, All);

bool FTextLocalizationResourceGenerator::GenerateLocMeta(const FLocTextHelper& InLocTextHelper, const FString& InResourceName, FTextLocalizationMetaDataResource& OutLocMeta)
{
	// Populate the meta-data
	OutLocMeta.NativeCulture = InLocTextHelper.GetNativeCulture();
	OutLocMeta.NativeLocRes = OutLocMeta.NativeCulture / InResourceName;
	OutLocMeta.CompiledCultures = InLocTextHelper.GetAllCultures();
	OutLocMeta.CompiledCultures.Sort();

	return true;
}

bool FTextLocalizationResourceGenerator::GenerateLocRes(const FLocTextHelper& InLocTextHelper, const FString& InCultureToGenerate, const EGenerateLocResFlags InGenerateFlags, const FTextKey& InLocResID, FTextLocalizationResource& OutPlatformAgnosticLocRes, TMap<FName, TSharedRef<FTextLocalizationResource>>& OutPerPlatformLocRes, const int32 InPriority)
{
	const bool bIsNativeCulture = InCultureToGenerate == InLocTextHelper.GetNativeCulture();
	FCulturePtr Culture = FInternationalization::Get().GetCulture(InCultureToGenerate);

	TArray<FString> InheritedCultures = Culture->GetPrioritizedParentCultureNames();
	InheritedCultures.Remove(Culture->GetName());

	// Always add the split platforms so that they generate an empty LocRes if there are no entries for that platform in the platform agnostic manifest
	for (const FString& SplitPlatformName : InLocTextHelper.GetPlatformsToSplit())
	{
		const FName SplitPlatformFName = *SplitPlatformName;
		if (!OutPerPlatformLocRes.Contains(SplitPlatformFName))
		{
			OutPerPlatformLocRes.Add(SplitPlatformFName, MakeShared<FTextLocalizationResource>());
		}
	}

	// Add each manifest entry to the LocRes file
	InLocTextHelper.EnumerateSourceTexts([&InLocTextHelper, &InCultureToGenerate, InGenerateFlags, &InLocResID, &OutPlatformAgnosticLocRes, &OutPerPlatformLocRes, InPriority, bIsNativeCulture, Culture, &InheritedCultures](TSharedRef<FManifestEntry> InManifestEntry) -> bool
	{
		// For each context, we may need to create a different or even multiple LocRes entries.
		for (const FManifestContext& Context : InManifestEntry->Contexts)
		{
			// Find the correct translation based upon the native source text
			FLocItem TranslationText;
			InLocTextHelper.GetRuntimeText(InCultureToGenerate, InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, ELocTextExportSourceMethod::NativeText, InManifestEntry->Source, TranslationText, EnumHasAnyFlags(InGenerateFlags, EGenerateLocResFlags::AllowStaleTranslations));

			// Is this entry considered translated? Native entries are always translated
			bool bIsTranslated = bIsNativeCulture || !InManifestEntry->Source.IsExactMatch(TranslationText);
			if (!bIsTranslated && InheritedCultures.Num() > 0)
			{
				// If this entry has parent languages then we also need to test whether the current translation is different from any parent that we have translations for, 
				// as it may be that the translation was explicitly changed back to being the native text for some reason (eg, es-419 needs something in English that es translates)
				for (const FString& InheritedCulture : InheritedCultures)
				{
					if (InLocTextHelper.HasArchive(InheritedCulture))
					{
						FLocItem InheritedText;
						InLocTextHelper.GetRuntimeText(InheritedCulture, InManifestEntry->Namespace, Context.Key, Context.KeyMetadataObj, ELocTextExportSourceMethod::NativeText, InManifestEntry->Source, InheritedText, EnumHasAnyFlags(InGenerateFlags, EGenerateLocResFlags::AllowStaleTranslations));
						if (!InheritedText.IsExactMatch(TranslationText))
						{
							bIsTranslated = true;
							break;
						}
					}
				}
			}

			if (bIsTranslated)
			{
				// Validate translations that look like they could be format patterns
				if (EnumHasAnyFlags(InGenerateFlags, EGenerateLocResFlags::ValidateFormatPatterns) && Culture && TranslationText.Text.Contains(TEXT("{"), ESearchCase::CaseSensitive))
				{
					const FTextFormat FmtPattern = FTextFormat::FromString(TranslationText.Text);

					TArray<FString> ValidationErrors;
					if (!FmtPattern.ValidatePattern(Culture, ValidationErrors))
					{
						FString Message = FString::Printf(TEXT("Format pattern '%s' (%s,%s) generated the following validation errors for '%s':"), *TranslationText.Text, *InManifestEntry->Namespace.GetString(), *Context.Key.GetString(), *InCultureToGenerate);
						for (const FString& ValidationError : ValidationErrors)
						{
							Message += FString::Printf(TEXT("\n  - %s"), *ValidationError);
						}
						UE_LOG(LogTextLocalizationResourceGenerator, Warning, TEXT("%s"), *FLocTextHelper::SanitizeLogOutput(Message));
					}
				}

				// Validate that text doesn't have leading or trailing whitespace
				if (EnumHasAnyFlags(InGenerateFlags, EGenerateLocResFlags::ValidateSafeWhitespace) && TranslationText.Text.Len() > 0)
				{
					auto IsUnsafeWhitespace = [](const TCHAR InChar)
					{
						// Unsafe whitespace is any whitespace character, except new-lines
						return FText::IsWhitespace(InChar) && !(InChar == TEXT('\r') || InChar == TEXT('\n'));
					};

					if (IsUnsafeWhitespace(TranslationText.Text[0]) || IsUnsafeWhitespace(TranslationText.Text[TranslationText.Text.Len() - 1]))
					{
						UE_LOG(LogTextLocalizationResourceGenerator, Warning, TEXT("%s"), *FLocTextHelper::SanitizeLogOutput(FString::Printf(TEXT("Translation '%s' (%s,%s) has leading or trailing whitespace for '%s'"), *TranslationText.Text, *InManifestEntry->Namespace.GetString(), *Context.Key.GetString(), *InCultureToGenerate)));
					}
				}

				// Find the LocRes to update
				FTextLocalizationResource* LocResToUpdate = &OutPlatformAgnosticLocRes;
				if (!Context.PlatformName.IsNone())
				{
					if (TSharedRef<FTextLocalizationResource>* PerPlatformLocRes = OutPerPlatformLocRes.Find(Context.PlatformName))
					{
						LocResToUpdate = &PerPlatformLocRes->Get();
					}
				}
				check(LocResToUpdate);

				// Add this entry to the LocRes
				LocResToUpdate->AddEntry(InManifestEntry->Namespace.GetString(), Context.Key.GetString(), InManifestEntry->Source.Text, TranslationText.Text, InPriority, InLocResID);
			}
		}

		return true; // continue enumeration
	}, true);

	return true;
}

bool FTextLocalizationResourceGenerator::GenerateLocResAndUpdateLiveEntriesFromConfig(const FString& InConfigFilePath, const EGenerateLocResFlags InGenerateFlags)
{
	FInternationalization& I18N = FInternationalization::Get();

	const FString SectionName = TEXT("RegenerateResources");

	// Get native culture.
	FString NativeCulture;
	if (!GConfig->GetString(*SectionName, TEXT("NativeCulture"), NativeCulture, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No native culture specified."));
		return false;
	}

	// Get source path.
	FString SourcePath;
	if (!GConfig->GetString(*SectionName, TEXT("SourcePath"), SourcePath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No source path specified."));
		return false;
	}

	// Get destination path.
	FString DestinationPath;
	if (!GConfig->GetString(*SectionName, TEXT("DestinationPath"), DestinationPath, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No destination path specified."));
		return false;
	}

	// Get manifest name.
	FString ManifestName;
	if (!GConfig->GetString(*SectionName, TEXT("ManifestName"), ManifestName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No manifest name specified."));
		return false;
	}

	// Get archive name.
	FString ArchiveName;
	if (!GConfig->GetString(*SectionName, TEXT("ArchiveName"), ArchiveName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No archive name specified."));
		return false;
	}

	// Get resource name.
	FString ResourceName;
	if (!GConfig->GetString(*SectionName, TEXT("ResourceName"), ResourceName, InConfigFilePath))
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No resource name specified."));
		return false;
	}

	// Source path needs to be relative to Engine or Game directory
	const FString ConfigFullPath = FPaths::ConvertRelativePathToFull(InConfigFilePath);
	const FString EngineFullPath = FPaths::ConvertRelativePathToFull(FPaths::EngineConfigDir());
	const bool IsEngineManifest = ConfigFullPath.StartsWith(EngineFullPath);

	if (IsEngineManifest)
	{
		SourcePath = FPaths::Combine(*FPaths::EngineDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::EngineDir(), *DestinationPath);
	}
	else
	{
		SourcePath = FPaths::Combine(*FPaths::ProjectDir(), *SourcePath);
		DestinationPath = FPaths::Combine(*FPaths::ProjectDir(), *DestinationPath);
	}

	TArray<FString> CulturesToGenerate;
	{
		const FString CultureName = I18N.GetCurrentCulture()->GetName();
		const TArray<FString> PrioritizedCultures = I18N.GetPrioritizedCultureNames(CultureName);
		for (const FString& PrioritizedCulture : PrioritizedCultures)
		{
			if (FPaths::FileExists(SourcePath / PrioritizedCulture / ArchiveName))
			{
				CulturesToGenerate.Add(PrioritizedCulture);
			}
		}
	}

	if (CulturesToGenerate.Num() == 0)
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No cultures to generate were specified."));
		return false;
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCulture, CulturesToGenerate, nullptr);
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("%s"), *LoadError.ToString());
			return false;
		}
	}

	FTextLocalizationResource TextLocalizationResource;
	TMap<FName, TSharedRef<FTextLocalizationResource>> Unused_PerPlatformLocRes;
	for (int32 CultureIndex = 0; CultureIndex < CulturesToGenerate.Num(); ++CultureIndex)
	{
		const FString& CultureName = CulturesToGenerate[CultureIndex];

		const FString CulturePath = DestinationPath / CultureName;
		const FString ResourceFilePath = FPaths::ConvertRelativePathToFull(CulturePath / ResourceName);

		if (!GenerateLocRes(LocTextHelper, CultureName, InGenerateFlags, FTextKey(ResourceFilePath), TextLocalizationResource, Unused_PerPlatformLocRes, CultureIndex))
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Failed to generate localization resource for culture '%s'."), *CultureName);
			return false;
		}
	}
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(TextLocalizationResource);

	return true;
}
