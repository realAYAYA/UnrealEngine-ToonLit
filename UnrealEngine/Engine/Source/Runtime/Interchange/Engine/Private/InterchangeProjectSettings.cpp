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

void FInterchangeProjectSettingsUtils::SetDefaultPipelineStackName(const bool bIsSceneImport, const UInterchangeSourceData& SourceData, const FName StackName)
{
	FInterchangeImportSettings& ImportSettings = GetMutableDefaultImportSettings(bIsSceneImport);

	if (!ImportSettings.PipelineStacks.Contains(StackName))
	{
		//The new stack name must be valid
		return;
	}

	FName DefaultPipelineStack = ImportSettings.DefaultPipelineStack;

	if (!bIsSceneImport)
	{
		UE::Interchange::FScopedTranslator ScopedTranslator(&SourceData);

		if (UInterchangeTranslatorBase* Translator = ScopedTranslator.GetTranslator())
		{
			EInterchangeTranslatorAssetType SupportedAssetTypes = Translator->GetSupportedAssetTypes();

			FInterchangeContentImportSettings& ContentImportSettings = GetMutableDefault<UInterchangeProjectSettings>()->ContentImportSettings;
			for (TMap<EInterchangeTranslatorAssetType, FName>::TIterator StackOverridesIt = ContentImportSettings.DefaultPipelineStackOverride.CreateIterator(); StackOverridesIt; ++StackOverridesIt)
			{
				if ((SupportedAssetTypes ^ StackOverridesIt->Key) < StackOverridesIt->Key)
				{
					//Update the override stack name and save the config
					StackOverridesIt->Value = StackName;
					GetMutableDefault<UInterchangeProjectSettings>()->SaveConfig();
					return;
				}
			}
		}
	}

	//We do not have any override default stack, simply change the DefaultPipelineStack and save the config
	ImportSettings.DefaultPipelineStack = StackName;
	GetMutableDefault<UInterchangeProjectSettings>()->SaveConfig();
}

bool FInterchangeProjectSettingsUtils::ShouldShowPipelineStacksConfigurationDialog(const bool bIsSceneImport, const UInterchangeSourceData& SourceData)
{
	const FInterchangeImportSettings& ImportSettings = GetDefaultImportSettings(bIsSceneImport);

	bool bShowImportDialog = ImportSettings.bShowImportDialog;

	if (!bIsSceneImport)
	{
		UE::Interchange::FScopedTranslator ScopedTranslator(&SourceData);

		if (UInterchangeTranslatorBase* Translator = ScopedTranslator.GetTranslator())
		{
			EInterchangeTranslatorAssetType SupportedAssetTypes = Translator->GetSupportedAssetTypes();
			const UClass* TranslatorClass = Translator->GetClass();

			//Iterate all override, if there is at least one override that show the imnport dialog we will show it.
			bool bFoundOverride = false;
			bool bShowFromOverrideStack = false;
			const FInterchangeContentImportSettings& ContentImportSettings = GetDefault<UInterchangeProjectSettings>()->ContentImportSettings;
			for (const TPair<EInterchangeTranslatorAssetType, FInterchangeDialogOverride>& ShowImportDialog : ContentImportSettings.ShowImportDialogOverride)
			{
				if ((ShowImportDialog.Key == EInterchangeTranslatorAssetType::None && SupportedAssetTypes == EInterchangeTranslatorAssetType::None)
					|| (static_cast<uint8>(ShowImportDialog.Key & SupportedAssetTypes) > 0))
				{
					//Look if there is a per translator override
					const FInterchangeDialogOverride& InterchangeDialogOverride = ShowImportDialog.Value;
					const FInterchangePerTranslatorDialogOverride* FindPerTranslatorOverride = InterchangeDialogOverride.PerTranslatorImportDialogOverride.FindByPredicate([&TranslatorClass](const FInterchangePerTranslatorDialogOverride& InterchangePerTranslatorDialogOverride)
						{
							return (InterchangePerTranslatorDialogOverride.Translator.Get() == TranslatorClass);
						});
					if (FindPerTranslatorOverride)
					{
						bShowFromOverrideStack |= FindPerTranslatorOverride->bShowImportDialog;
					}
					else
					{
						//Simple override
						bShowFromOverrideStack |= InterchangeDialogOverride.bShowImportDialog;
					}
					bFoundOverride = true;
				}
			}
			if (bFoundOverride)
			{
				bShowImportDialog = bShowFromOverrideStack;
			}
		}
	}

	return bShowImportDialog;
}