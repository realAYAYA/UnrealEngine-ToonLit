// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorSettings.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Viewport/AvaCineCameraActor.h"

#define LOCTEXT_NAMESPACE "AvaEditorSettings"

UAvaEditorSettings::UAvaEditorSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName = TEXT("Editor");
}

UAvaEditorSettings* UAvaEditorSettings::Get()
{
	UAvaEditorSettings* DefaultSettings = GetMutableDefault<UAvaEditorSettings>();
	static bool bInitialized = false;
	if (!bInitialized)
	{
		bInitialized = true;
		DefaultSettings->SetFlags(RF_Transactional);
	}
	return DefaultSettings;
}

void UAvaEditorSettings::OpenEditorSettingsWindow() const
{
	static ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>(TEXT("Settings"));
	SettingsModule.ShowViewer(GetContainerName(), CategoryName, SectionName);
}

void UAvaEditorSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (const FName PresetNameNoLumen = TEXT("No Lumen");
		!ViewportQualityPresets.Contains(PresetNameNoLumen))
	{
		ViewportQualityPresets.Add(PresetNameNoLumen, FAvaViewportQualitySettings::Preset(PresetNameNoLumen));
	}

	if (const FName PresetNameReduced = TEXT("Reduced");
		!ViewportQualityPresets.Contains(PresetNameReduced))
	{
		ViewportQualityPresets.Add(PresetNameReduced, FAvaViewportQualitySettings::Preset(PresetNameReduced));
	}

	// Verifying integrity and sorting will ensure any new/removed entries are handled correctly.
	DefaultViewportQualitySettings.VerifyIntegrity();
	DefaultViewportQualitySettings.SortFeaturesByDisplayText();

	for (TPair<FName, FAvaViewportQualitySettings>& Preset : ViewportQualityPresets)
	{
		FAvaViewportQualitySettings& Settings = Preset.Value;
		Settings.VerifyIntegrity();
		Settings.SortFeaturesByDisplayText();
	}
}

void UAvaEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnChanged.Broadcast(this, PropertyChangedEvent.GetPropertyName());
}

#undef LOCTEXT_NAMESPACE
