// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOEditorSettings.h"

#include "IOpenColorIOEditorModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenColorIOEditorSettings)

const FOpenColorIODisplayConfiguration* UOpenColorIOLevelViewportSettings::GetViewportSettings(FName ViewportIdentifier) const
{
	const FPerViewportDisplaySettingPair* Pair = ViewportsSettings.FindByPredicate([ViewportIdentifier](const FPerViewportDisplaySettingPair& Other)
		{
			return Other.ViewportIdentifier == ViewportIdentifier;
		});

	if (Pair)
	{
		return &Pair->DisplayConfiguration;
	}

	return nullptr;
}

void UOpenColorIOLevelViewportSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (GConfig && FPaths::FileExists(GEditorPerProjectIni))
	{
		const TCHAR* SectionName = TEXT("/Script/OpenColorIOEditor.OpenColorIOLevelViewportSettings");

		if (GConfig->DoesSectionExist(SectionName, GEditorPerProjectIni))
		{
			LoadConfig(UOpenColorIOLevelViewportSettings::StaticClass(), *GEditorPerProjectIni);

			SaveConfig();

			GConfig->EmptySection(SectionName, *GEditorPerProjectIni);

			UE_LOG(LogOpenColorIOEditor, Warning, TEXT("Migrated EditorPerProjectUserSettings OpenColorIO settings to plugin-specific config file: %s."), *GetClass()->GetConfigName());
		}
	}
}

void UOpenColorIOLevelViewportSettings::SetViewportSettings(FName ViewportIdentifier, const FOpenColorIODisplayConfiguration& Configuration)
{
	FPerViewportDisplaySettingPair* Pair = ViewportsSettings.FindByPredicate([ViewportIdentifier](const FPerViewportDisplaySettingPair& Other)
		{
			return Other.ViewportIdentifier == ViewportIdentifier;
		});

	if (Pair)
	{
		Pair->DisplayConfiguration = Configuration;
	}
	else
	{
		//Add new entry if viewport is not found
		FPerViewportDisplaySettingPair NewEntry;
		NewEntry.ViewportIdentifier = ViewportIdentifier;
		NewEntry.DisplayConfiguration = Configuration;
		ViewportsSettings.Emplace(MoveTemp(NewEntry));
	}
}

