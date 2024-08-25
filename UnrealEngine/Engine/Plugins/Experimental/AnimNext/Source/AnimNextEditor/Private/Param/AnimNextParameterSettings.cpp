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