// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GenerateTextLocalizationResourceCommandlet.h"

#include "Commandlets/Commandlet.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Internationalization/CulturePointer.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Internationalization/TextKey.h"
#include "Internationalization/TextLocalizationResource.h"
#include "LocTextHelper.h"
#include "LocalizedAssetUtil.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/Paths.h"
#include "Templates/SharedPointer.h"
#include "Templates/Tuple.h"
#include "TextLocalizationResourceGenerator.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGenerateTextLocalizationResourceCommandlet, Log, All);

UGenerateTextLocalizationResourceCommandlet::UGenerateTextLocalizationResourceCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UGenerateTextLocalizationResourceCommandlet::Main(const FString& Params)
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	// Set config file.
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;

	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	// Set config section.
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	// Get source path.
	FString SourcePath;
	if( !( GetPathFromConfig( *SectionName, TEXT("SourcePath"), SourcePath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No source path specified."));
		return -1;
	}

	// Get manifest name.
	FString ManifestName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ManifestName"), ManifestName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No manifest name specified."));
		return -1;
	}

	// Get archive name.
	FString ArchiveName;
	if (!GetStringFromConfig(*SectionName, TEXT("ArchiveName"), ArchiveName, GatherTextConfigPath))
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No archive name specified."));
		return -1;
	}

	// Get cultures to generate.
	FString NativeCultureName;
	if( !( GetStringFromConfig( *SectionName, TEXT("NativeCulture"), NativeCultureName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No native culture specified."));
		return -1;
	}

	// Get cultures to generate.
	TArray<FString> CulturesToGenerate;
	GetStringArrayFromConfig( *SectionName, TEXT("CulturesToGenerate"), CulturesToGenerate, GatherTextConfigPath );

	if( CulturesToGenerate.Num() == 0 )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No cultures specified for generation."));
		return -1;
	}

	for(int32 i = 0; i < CulturesToGenerate.Num(); ++i)
	{
		if( FInternationalization::Get().GetCulture( CulturesToGenerate[i] ).IsValid() )
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Verbose, TEXT("Specified culture is not a valid runtime culture, but may be a valid base language: %s"), *(CulturesToGenerate[i]));
		}
	}

	// Get destination path.
	FString DestinationPath;
	if( !( GetPathFromConfig( *SectionName, TEXT("DestinationPath"), DestinationPath, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No destination path specified."));
		return -1;
	}

	// Get resource name.
	FString ResourceName;
	if( !( GetStringFromConfig( *SectionName, TEXT("ResourceName"), ResourceName, GatherTextConfigPath ) ) )
	{
		UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("No resource name specified."));
		return -1;
	}

	EGenerateLocResFlags GenerateFlags = EGenerateLocResFlags::None;

	// Get whether to skip the source check.
	{
		bool bSkipSourceCheck = false;
		GetBoolFromConfig(*SectionName, TEXT("bSkipSourceCheck"), bSkipSourceCheck, GatherTextConfigPath);
		GenerateFlags |= (bSkipSourceCheck ? EGenerateLocResFlags::AllowStaleTranslations : EGenerateLocResFlags::None);
	}

	// Get whether to validate format patterns.
	{
		bool bValidateFormatPatterns = false;
		GetBoolFromConfig(*SectionName, TEXT("bValidateFormatPatterns"), bValidateFormatPatterns, GatherTextConfigPath);
		GenerateFlags |= (bValidateFormatPatterns ? EGenerateLocResFlags::ValidateFormatPatterns : EGenerateLocResFlags::None);
	}

	// Get whether to validate whitespace.
	{
		bool bValidateSafeWhitespace = false;
		GetBoolFromConfig(*SectionName, TEXT("bValidateSafeWhitespace"), bValidateSafeWhitespace, GatherTextConfigPath);
		GenerateFlags |= (bValidateSafeWhitespace ? EGenerateLocResFlags::ValidateSafeWhitespace : EGenerateLocResFlags::None);
	}

	// Load the manifest and all archives
	FLocTextHelper LocTextHelper(SourcePath, ManifestName, ArchiveName, NativeCultureName, CulturesToGenerate, GatherManifestHelper->GetLocFileNotifies(), GatherManifestHelper->GetPlatformSplitMode());
	LocTextHelper.SetCopyrightNotice(GatherManifestHelper->GetCopyrightNotice());
	{
		FText LoadError;
		if (!LocTextHelper.LoadAll(ELocTextHelperLoadFlags::LoadOrCreate, &LoadError))
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("%s"), *LoadError.ToString());
			return -1;
		}
	}

	// Generate the LocMeta file for all cultures
	{
		const FString TextLocalizationMetaDataResourcePath = DestinationPath / FPaths::GetBaseFilename(ResourceName) + TEXT(".locmeta");

		const bool bLocMetaFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, TextLocalizationMetaDataResourcePath, [&LocTextHelper, &ResourceName](const FString& InSaveFileName) -> bool
		{
			FTextLocalizationMetaDataResource LocMeta;
			return FTextLocalizationResourceGenerator::GenerateLocMeta(LocTextHelper, ResourceName, LocMeta) && LocMeta.SaveToFile(InSaveFileName);
		});

		if (!bLocMetaFileSaved)
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Could not write file %s"), *TextLocalizationMetaDataResourcePath);
			return -1;
		}
	}

	// Generate the LocRes file for each culture
	for (const FString& CultureName : CulturesToGenerate)
	{
		auto GenerateSingleLocRes = [this, &DestinationPath, &CultureName, &ResourceName](const FTextLocalizationResource& InLocRes, const FName InPlatformName) -> bool
		{
			FString TextLocalizationResourcePath;
			if (InPlatformName.IsNone())
			{
				TextLocalizationResourcePath = DestinationPath / CultureName / ResourceName;
			}
			else
			{
				TextLocalizationResourcePath = DestinationPath / FPaths::GetPlatformLocalizationFolderName() / InPlatformName.ToString() / CultureName / ResourceName;
			}

			const bool bLocResFileSaved = FLocalizedAssetSCCUtil::SaveFileWithSCC(SourceControlInfo, TextLocalizationResourcePath, [&InLocRes](const FString& InSaveFileName) -> bool
			{
				return InLocRes.SaveToFile(InSaveFileName);
			});

			if (!bLocResFileSaved)
			{
				UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Could not write file %s"), *TextLocalizationResourcePath);
				return false;
			}

			return true;
		};

		FTextLocalizationResource PlatformAgnosticLocRes;
		TMap<FName, TSharedRef<FTextLocalizationResource>> PerPlatformLocRes;
		const FTextKey LocResId = DestinationPath / CultureName / ResourceName;
		if (!FTextLocalizationResourceGenerator::GenerateLocRes(LocTextHelper, CultureName, GenerateFlags, LocResId, PlatformAgnosticLocRes, PerPlatformLocRes))
		{
			UE_LOG(LogGenerateTextLocalizationResourceCommandlet, Error, TEXT("Failed to generate LocRes %s"), LocResId.GetChars());
			return false;
		}
	
		bool bSuccess = GenerateSingleLocRes(PlatformAgnosticLocRes, FName());
		for (const auto& PerPlatformLocResPair : PerPlatformLocRes)
		{
			bSuccess &= GenerateSingleLocRes(*PerPlatformLocResPair.Value, PerPlatformLocResPair.Key);
		}
		if (!bSuccess)
		{
			return -1;
		}
	}

	return 0;
}
