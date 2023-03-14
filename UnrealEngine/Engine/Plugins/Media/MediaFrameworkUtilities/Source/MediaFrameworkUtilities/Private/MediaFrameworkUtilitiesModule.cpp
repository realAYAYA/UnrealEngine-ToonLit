// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaFrameworkUtilitiesModule.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileManager.h"
#include "Profile/MediaProfileSettings.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif //WITH_EDITOR


DEFINE_LOG_CATEGORY(LogMediaFrameworkUtilities);

#define LOCTEXT_NAMESPACE "MediaFrameworkUtilities"

static TAutoConsoleVariable<FString> CVarMediaUtilsStartupProfile(
	TEXT("MediaUtils.StartupProfile"),
	TEXT(""),
	TEXT("Startup Media Profile\n"),
	ECVF_ReadOnly
);

/**
 * Implements the MediaFrameworkUtilitiesModule module.
 */
class FMediaFrameworkUtilitiesModule : public IMediaFrameworkUtilitiesModule
{
	FMediaProfileManager MediaProfileManager;
	FDelegateHandle PostEngineInitHandle;

	virtual void StartupModule() override
	{
		RegisterSettings();
		ApplyStartupMediaProfile();
	}

	virtual void ShutdownModule() override
	{
		RemoveStartupMediaProfile();
		UnregisterSettings();
	}

	virtual IMediaProfileManager& GetProfileManager() override
	{
		return MediaProfileManager;
	}

	void RegisterSettings()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// register settings
			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (SettingsModule != nullptr)
			{
				SettingsModule->RegisterSettings("Project", "Plugins", "MediaProfile",
					LOCTEXT("MediaProfilesSettingsName", "Media Profile"),
					LOCTEXT("MediaProfilesDescription", "Configure the Media Profile."),
					GetMutableDefault<UMediaProfileSettings>()
				);

				SettingsModule->RegisterSettings("Editor", "General", "MediaProfile",
					LOCTEXT("MediaProfilesSettingsName", "Media Profile"),
					LOCTEXT("MediaProfilesDescription", "Configure the Media Profile."),
					GetMutableDefault<UMediaProfileEditorSettings>()
				);
			}
		}
#endif //WITH_EDITOR
	}

	void UnregisterSettings()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			// unregister settings
			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
			if (SettingsModule != nullptr)
			{
				SettingsModule->UnregisterSettings("Project", "Media", "MediaProfile");
				SettingsModule->UnregisterSettings("Editor", "Media", "MediaProfile");
			}
		}
#endif //WITH_EDITOR
	}

	void ApplyStartupMediaProfile()
	{
		auto ApplyMediaProfile = [this]()
		{
			UMediaProfile* MediaProfile = nullptr;

			// Try to load from CVar
			{
				const FString MediaProfileName = CVarMediaUtilsStartupProfile.GetValueOnGameThread();

				if (MediaProfileName.Len())
				{
					if (UObject* Object = StaticLoadObject(UMediaProfile::StaticClass(), nullptr, *MediaProfileName))
					{
						MediaProfile = CastChecked<UMediaProfile>(Object);
					}

					if (MediaProfile)
					{
						UE_LOG(LogMediaFrameworkUtilities, Display,
							TEXT("Loading Media Profile specified in CVar MediaUtils.StartupProfile: '%s'"), *MediaProfileName);
					}
				}
			}

#if WITH_EDITOR
			// Try to load from User Settings
			if (MediaProfile == nullptr)
			{
				MediaProfile = GetDefault<UMediaProfileEditorSettings>()->GetUserMediaProfile();
			}
#endif

			// Try to load from Game Settings
			if (MediaProfile == nullptr)
			{
				MediaProfile = GetDefault<UMediaProfileSettings>()->GetStartupMediaProfile();
			}

			MediaProfileManager.SetCurrentMediaProfile(MediaProfile);
		};

		if (FApp::CanEverRender() || GetDefault<UMediaProfileSettings>()->bApplyInCommandlet)
		{
			if (GEngine && GEngine->IsInitialized())
			{
				ApplyMediaProfile();
			}
			else
			{
				PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda(ApplyMediaProfile);
			}
		}
	}

	void RemoveStartupMediaProfile()
	{
		if (PostEngineInitHandle.IsValid())
		{
			FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
		}

		if (!IsEngineExitRequested())
		{
			MediaProfileManager.SetCurrentMediaProfile(nullptr);
		}
	}
};

IMPLEMENT_MODULE(FMediaFrameworkUtilitiesModule, MediaFrameworkUtilities);

#undef LOCTEXT_NAMESPACE
