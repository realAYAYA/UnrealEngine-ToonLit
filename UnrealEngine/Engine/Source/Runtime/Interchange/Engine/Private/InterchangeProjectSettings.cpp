// Copyright Epic Games, Inc. All Rights Reserved.
#include "InterchangeProjectSettings.h"

#include "InterchangeManager.h"
#include "InterchangeSourceData.h"

const FInterchangeImportSettings& FInterchangeProjectSettingsUtils::GetImportSettings(const UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport)
{
	if (bIsSceneImport)
	{
		return InterchangeProjectSettings.SceneImportSettings;
	}
	else
	{
		return InterchangeProjectSettings.ContentImportSettings;
	}
}

FInterchangeImportSettings& FInterchangeProjectSettingsUtils::GetMutableImportSettings(UInterchangeProjectSettings& InterchangeProjectSettings, const bool bIsSceneImport)
{
	if (bIsSceneImport)
	{
		return InterchangeProjectSettings.SceneImportSettings;
	}
	else
	{
		return InterchangeProjectSettings.ContentImportSettings;
	}
}

const FInterchangeImportSettings& FInterchangeProjectSettingsUtils::GetDefaultImportSettings(const bool bIsSceneImport)
{
	return GetImportSettings(*GetDefault<UInterchangeProjectSettings>(), bIsSceneImport);
}

FInterchangeImportSettings& FInterchangeProjectSettingsUtils::GetMutableDefaultImportSettings(const bool bIsSceneImport)
{
	return GetMutableImportSettings(*GetMutableDefault<UInterchangeProjectSettings>(), bIsSceneImport);
}

FName FInterchangeProjectSettingsUtils::GetDefaultPipelineStackName(const bool bIsSceneImport, const UInterchangeSourceData& SourceData)
{
	const FInterchangeImportSettings& ImportSettings = GetDefaultImportSettings(bIsSceneImport);

	FName DefaultPipelineStack = ImportSettings.DefaultPipelineStack;

	if (!bIsSceneImport)
	{
		UE::Interchange::FScopedTranslator ScopedTranslator(&SourceData);

		if (UInterchangeTranslatorBase* Translator = ScopedTranslator.GetTranslator())
		{
			EInterchangeTranslatorAssetType SupportedAssetTypes = Translator->GetSupportedAssetTypes();

			const FInterchangeContentImportSettings& ContentImportSettings = GetDefault<UInterchangeProjectSettings>()->ContentImportSettings;
			for (TMap<EInterchangeTranslatorAssetType, FName>::TConstIterator StackOverridesIt = ContentImportSettings.DefaultPipelineStackOverride.CreateConstIterator(); StackOverridesIt; ++StackOverridesIt)
			{
				if ((SupportedAssetTypes ^ StackOverridesIt->Key) < StackOverridesIt->Key)
				{
					DefaultPipelineStack = StackOverridesIt->Value;
					break;
				}
			}
		}
	}

	return DefaultPipelineStack;
}

bool FInterchangeProjectSettingsUtils::ShouldShowPipelineStacksConfigurationDialog(const bool bIsSceneImport, const UInterchangeSourceData& SourceData)
{
	const FInterchangeImportSettings& ImportSettings = GetDefaultImportSettings(bIsSceneImport);

	bool bShowPipelineStacksConfigurationDialog = ImportSettings.bShowPipelineStacksConfigurationDialog;

	if (!bIsSceneImport)
	{
		UE::Interchange::FScopedTranslator ScopedTranslator(&SourceData);

		if (UInterchangeTranslatorBase* Translator = ScopedTranslator.GetTranslator())
		{
			EInterchangeTranslatorAssetType SupportedAssetTypes = Translator->GetSupportedAssetTypes();

			const FInterchangeContentImportSettings& ContentImportSettings = GetDefault<UInterchangeProjectSettings>()->ContentImportSettings;
			for (TMap<EInterchangeTranslatorAssetType, bool>::TConstIterator StackOverridesIt = ContentImportSettings.ShowPipelineStacksConfigurationDialogOverride.CreateConstIterator(); StackOverridesIt; ++StackOverridesIt)
			{
				if ((SupportedAssetTypes ^ StackOverridesIt->Key) < StackOverridesIt->Key)
				{
					bShowPipelineStacksConfigurationDialog = StackOverridesIt->Value;
					break;
				}
			}
		}
	}

	return bShowPipelineStacksConfigurationDialog;
}