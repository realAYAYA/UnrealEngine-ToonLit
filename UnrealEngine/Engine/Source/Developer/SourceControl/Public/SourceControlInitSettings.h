// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"

class FString;

class SOURCECONTROL_API FSourceControlInitSettings
{
public:
	enum class EBehavior
	{
		/** All existing settings will be overridden via the contents of FSourceControlInitSettings. Settings that are not found will be reset to default states */
		OverrideAll,
		/** Only the settings found in FSourceControlInitSettings will be overridden. Settings not found will be left with their current values. */
		OverrideExisting,
	};

	enum class EConfigBehavior
	{
		/** Can both read from, and save to the ini file*/
		ReadWrite,
		/** Will only read settings from the ini file, settings determined at runtime will not be saved to the ini file */
		ReadOnly,
		/** The settings will not be saved to the ini file, nor will they be read from the ini file  */
		None
	};

	FSourceControlInitSettings(EBehavior Behavior);
	~FSourceControlInitSettings() = default;

	void SetConfigBehavior(EConfigBehavior Behavior);

	bool CanWriteToConfigFile() const;
	bool CanReadFromConfigFile() const;
		
	void AddSetting(FStringView SettingName, FStringView SettingValue);
	void OverrideSetting(FStringView SettingName, FString& InOutSettingValue);

	bool HasOverrides() const;
	bool IsOverridden(FStringView SettingName) const;

private:

	EBehavior OverrideBehavior = EBehavior::OverrideAll;
	EConfigBehavior ConfigBehavior = EConfigBehavior::ReadWrite;

	TMap<FString, FString> Settings;
};
