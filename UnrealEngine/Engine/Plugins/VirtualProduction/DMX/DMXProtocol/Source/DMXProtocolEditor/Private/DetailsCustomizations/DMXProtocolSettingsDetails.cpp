// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXProtocolSettingsDetails.h"

#include "DMXProtocolSettings.h"

#include "DetailLayoutBuilder.h"
#include "Interfaces/IPluginManager.h"


namespace UE::DMXProtocolEditor::Private::DMXProtocolSettingsDetails
{
	bool IsDMXEnginePluginEnabled()
	{
		static const FString DMXEnginePluginName = TEXT("DMXEngine");

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(DMXEnginePluginName);
		return Plugin.IsValid() && Plugin->IsEnabled();
	}
}

FDMXProtocolSettingsDetails::FDMXProtocolSettingsDetails()
	: bIsDMXEnginePluginEnabled(UE::DMXProtocolEditor::Private::DMXProtocolSettingsDetails::IsDMXEnginePluginEnabled())
{}

TSharedRef<IDetailCustomization> FDMXProtocolSettingsDetails::MakeInstance()
{
	const TSharedRef<FDMXProtocolSettingsDetails> ProtocolSettingsDetails = MakeShared<FDMXProtocolSettingsDetails>();
	return ProtocolSettingsDetails;
}

void FDMXProtocolSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (!bIsDMXEnginePluginEnabled)
	{
		DetailBuilder.HideProperty(UDMXProtocolSettings::GetAllFixturePatchesReceiveDMXInEditorPropertyName());
	}
}
