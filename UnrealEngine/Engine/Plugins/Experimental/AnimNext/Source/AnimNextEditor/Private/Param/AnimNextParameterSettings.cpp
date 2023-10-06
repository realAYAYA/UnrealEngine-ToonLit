// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextParameterSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

UAnimNextParameterSettings::UAnimNextParameterSettings()
{
}

const FAnimNextParamType& UAnimNextParameterSettings::GetLastParameterType() const
{
	return LastParameterType;
}

void UAnimNextParameterSettings::SetLastParameterType(const FAnimNextParamType& InLastParameterType)
{
	LastParameterType = InLastParameterType;
}

FAssetData UAnimNextParameterSettings::GetLastLibrary() const
{
	IAssetRegistry& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	return AssetRegistryModule.GetAssetByObjectPath(LastLibrary);
}

void UAnimNextParameterSettings::SetLastLibrary(const FAssetData& InLastLibrary)
{
	LastLibrary = InLastLibrary.GetSoftObjectPath();

	SaveConfig();
}