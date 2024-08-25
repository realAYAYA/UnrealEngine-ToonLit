// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorFromLegacyUpgradeHandler.h"

#include "AssetToolsModule.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleRawFader.h"
#include "DMXEditorSettings.h"
#include "Factories/DMXControlConsoleFactory.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Misc/CoreDelegates.h"	
#include "UObject/Package.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorFromLegacyUpgradeHandler"

TWeakObjectPtr<UDMXControlConsole> FDMXControlConsoleEditorFromLegacyUpgradeHandler::UpgradePathControlConsole;

bool FDMXControlConsoleEditorFromLegacyUpgradeHandler::TryUpgradePathFromLegacy()
{
	const UDMXEditorSettings* DMXEditorSettings = GetDefault<UDMXEditorSettings>();
	if (!DMXEditorSettings)
	{
		return false;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	const TArray<FDMXOutputConsoleFaderDescriptor>& FaderDescriptorArray = DMXEditorSettings->OutputConsoleFaders_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (FaderDescriptorArray.IsEmpty())
	{
		return false;
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UDMXControlConsoleData* ControlConsoleData = CreateControlConsoleDataFromFaderDescriptorArray(FaderDescriptorArray);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	if (!ControlConsoleData)
	{
		return false;
	}

	const FString AssetPath = TEXT("/Game");
	const FString AssetName = TEXT("DefaultControlConsole");

	FString UniquePackageName;
	FString UniqueAssetName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(AssetPath / AssetName, TEXT(""), UniquePackageName, UniqueAssetName);

	const UDMXControlConsoleFactory* ControlConsoleFactory = NewObject<UDMXControlConsoleFactory>();
	UpgradePathControlConsole = ControlConsoleFactory->CreateConsoleAssetFromData(AssetPath, UniqueAssetName, ControlConsoleData);
	if (!UpgradePathControlConsole.IsValid())
	{
		return false;
	}

	UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = NewObject<UDMXControlConsoleEditorLayouts>(UpgradePathControlConsole.Get(), NAME_None, RF_Transactional);
	UDMXControlConsoleEditorGlobalLayoutBase* UserLayout = ControlConsoleLayouts->AddUserLayout(TEXT(""));
	if (UserLayout)
	{
		UserLayout->GenerateLayoutByControlConsoleData(ControlConsoleData);
	}

	UpgradePathControlConsole->ControlConsoleEditorLayouts = ControlConsoleLayouts; 
	UpgradePathControlConsole->GetOnControlConsoleSaved().AddStatic(&FDMXControlConsoleEditorFromLegacyUpgradeHandler::OnUpgradePathControlConsoleSaved);
	
	return true;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UDMXControlConsoleData* FDMXControlConsoleEditorFromLegacyUpgradeHandler::CreateControlConsoleDataFromFaderDescriptorArray(const TArray<FDMXOutputConsoleFaderDescriptor>& FaderDescriptorArray)
{
	UDMXControlConsoleData* ControlConsoleData = NewObject<UDMXControlConsoleData>(GetTransientPackage(), NAME_None, RF_Transactional);

	UDMXControlConsoleFaderGroupRow* FaderGroupRow = ControlConsoleData->AddFaderGroupRow(0);
	if (!FaderGroupRow)
	{
		return nullptr;
	}

	TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
	UDMXControlConsoleFaderGroup* FirstFaderGroup = FaderGroups.IsEmpty() ? FaderGroupRow->AddFaderGroup(0) : FaderGroups[0];
	if (!FirstFaderGroup)
	{
		return nullptr;
	}

	for (const FDMXOutputConsoleFaderDescriptor& FaderDescriptor : FaderDescriptorArray)
	{
		CreateRawFaderFromFaderDescriptor(FirstFaderGroup, FaderDescriptor);
	}

	return ControlConsoleData;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UDMXControlConsoleRawFader* FDMXControlConsoleEditorFromLegacyUpgradeHandler::CreateRawFaderFromFaderDescriptor(UDMXControlConsoleFaderGroup* FaderGroup, const FDMXOutputConsoleFaderDescriptor& FaderDescriptor)
{
	if (!FaderGroup)
	{
		return nullptr;
	}

	UDMXControlConsoleRawFader* Fader = FaderGroup->AddRawFader();
	if (!Fader)
	{
		return nullptr;
	}

	Fader->SetFaderName(FaderDescriptor.FaderName);
	Fader->SetUniverseID(FaderDescriptor.UniversID);
	Fader->SetAddressRange(FaderDescriptor.StartingAddress);
	Fader->SetValue(FaderDescriptor.Value);
	return Fader;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FDMXControlConsoleEditorFromLegacyUpgradeHandler::OnUpgradePathControlConsoleSaved(const UDMXControlConsole* ControlConsole)
{
	if (!UpgradePathControlConsole.IsValid())
	{
		return;
	}

	if (UpgradePathControlConsole == ControlConsole)
	{
		UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
		if (!DMXEditorSettings)
		{
			return;
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		DMXEditorSettings->OutputConsoleFaders_DEPRECATED.Empty();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		DMXEditorSettings->SaveConfig();
	}
}

#undef LOCTEXT_NAMESPACE
