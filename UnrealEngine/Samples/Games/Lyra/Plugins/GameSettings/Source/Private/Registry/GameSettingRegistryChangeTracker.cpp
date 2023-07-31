// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameSettingRegistryChangeTracker.h"

#include "Delegates/Delegate.h"
#include "GameSetting.h"
#include "GameSettingRegistry.h"
#include "GameSettingValue.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "Templates/Tuple.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtr.h"

#define LOCTEXT_NAMESPACE "GameSetting"

FGameSettingRegistryChangeTracker::FGameSettingRegistryChangeTracker()
{
}

FGameSettingRegistryChangeTracker::~FGameSettingRegistryChangeTracker()
{
	if (UGameSettingRegistry* StrongRegistry = Registry.Get())
	{
		StrongRegistry->OnSettingChangedEvent.RemoveAll(this);
	}
}

void FGameSettingRegistryChangeTracker::WatchRegistry(UGameSettingRegistry* InRegistry)
{
	ClearDirtyState();
	StopWatchingRegistry();

	if (Registry.Get() != InRegistry)
	{
		Registry = InRegistry;
		InRegistry->OnSettingChangedEvent.AddRaw(this, &FGameSettingRegistryChangeTracker::HandleSettingChanged);
	}
}

void FGameSettingRegistryChangeTracker::StopWatchingRegistry()
{
	if (UGameSettingRegistry* StrongRegistry = Registry.Get())
	{
		StrongRegistry->OnSettingChangedEvent.RemoveAll(this);
		Registry.Reset();
	}
}

void FGameSettingRegistryChangeTracker::ClearDirtyState()
{
	ensure(!bRestoringSettings);
	if (bRestoringSettings)
	{
		return;
	}

	bSettingsChanged = false;
	DirtySettings.Reset();
}

void FGameSettingRegistryChangeTracker::ApplyChanges()
{
	for (auto Entry : DirtySettings)
	{
		if (UGameSettingValue* SettingValue = Cast<UGameSettingValue>(Entry.Value))
		{
			SettingValue->Apply();
			SettingValue->StoreInitial();
		}
	}

	ClearDirtyState();
}

void FGameSettingRegistryChangeTracker::RestoreToInitial()
{
	ensure(!bRestoringSettings);
	if (bRestoringSettings)
	{
		return;
	}

	{
		TGuardValue<bool> LocalGuard(bRestoringSettings, true);
		for (auto Entry : DirtySettings)
		{
			if (UGameSettingValue* SettingValue = Cast<UGameSettingValue>(Entry.Value))
			{
				SettingValue->RestoreToInitial();
			}
		}
	}

	ClearDirtyState();
}

void FGameSettingRegistryChangeTracker::HandleSettingChanged(UGameSetting* Setting, EGameSettingChangeReason Reason)
{
	if (bRestoringSettings)
	{
		return;
	}

	bSettingsChanged = true;
	DirtySettings.Add(FObjectKey(Setting), Setting);
}

#undef LOCTEXT_NAMESPACE
