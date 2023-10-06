// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdvancedPreviewSceneModule.h"

#include "AdvancedPreviewSceneCommands.h"
#include "Modules/ModuleManager.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UObject;

void FAdvancedPreviewSceneModule::StartupModule()
{
	FAdvancedPreviewSceneCommands::Register();
}

void FAdvancedPreviewSceneModule::ShutdownModule()
{
}

TSharedRef<SWidget> FAdvancedPreviewSceneModule::CreateAdvancedPreviewSceneSettingsWidget(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene, UObject* InAdditionalSettings, const TArray<FDetailCustomizationInfo>& InDetailCustomizations, const TArray<FPropertyTypeCustomizationInfo>& InPropertyTypeCustomizations, const TArray<FDetailDelegates>& InDelegates)
{
	return SNew(SAdvancedPreviewDetailsTab, InPreviewScene)
		.AdditionalSettings(InAdditionalSettings)
		.DetailCustomizations(InDetailCustomizations)
		.PropertyTypeCustomizations(InPropertyTypeCustomizations)
		.Delegates(InDelegates);

}

IMPLEMENT_MODULE(FAdvancedPreviewSceneModule, AdvancedPreviewScene);
