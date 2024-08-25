// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXControlConsoleFactory.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetTypeCategories.h"
#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorModule.h"
#include "FileHelpers.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleFactory"

UDMXControlConsoleFactory::UDMXControlConsoleFactory()
{
	SupportedClass = UDMXControlConsole::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

FName UDMXControlConsoleFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("DMXControlConsole.TabIcon");
}

UObject* UDMXControlConsoleFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UDMXControlConsole* DMXControlConsole = NewObject<UDMXControlConsole>(InParent, Name, Flags);
	return DMXControlConsole;
}

uint32 UDMXControlConsoleFactory::GetMenuCategories() const
{
	return (uint32)FDMXControlConsoleEditorModule::GetDMXEditorAssetCategory();
}

UDMXControlConsole* UDMXControlConsoleFactory::CreateConsoleAssetFromData(const FString& SavePackagePath, const FString& SaveAssetName, UDMXControlConsoleData* SourceConsoleData) const
{
	if (!ensureMsgf(FPackageName::IsValidLongPackageName(SavePackagePath / SaveAssetName), TEXT("Invalid package name when trying to create Control Console asset. Failed to create asset.")))
	{
		return nullptr;
	}

	const FString PackageName = SavePackagePath / SaveAssetName;
	UPackage* Package = CreatePackage(*PackageName);
	check(Package);
	Package->FullyLoad();

	UDMXControlConsole* NewConsole = NewObject<UDMXControlConsole>(Package, FName(SaveAssetName), RF_Public | RF_Standalone | RF_Transactional);
	if (SourceConsoleData)
	{
		NewConsole->CopyControlConsoleData(SourceConsoleData);
	}

	constexpr bool bCheckDirty = false;
	constexpr bool bPromptToSave = false;
	FEditorFileUtils::EPromptReturnCode PromptReturnCode = FEditorFileUtils::PromptForCheckoutAndSave({ NewConsole->GetPackage() }, bCheckDirty, bPromptToSave);
	if (PromptReturnCode != FEditorFileUtils::EPromptReturnCode::PR_Success)
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetRegistryModule::AssetCreated(NewConsole);

	return NewConsole;
}

#undef LOCTEXT_NAMESPACE
