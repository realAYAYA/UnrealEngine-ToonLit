// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskSettings.h"

#include "AvaMaskLog.h"

#if WITH_EDITORONLY_DATA
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialFunctionInterface.h"
#endif

UAvaMaskSettings::UAvaMaskSettings()
{
	CategoryName = TEXT("Motion Design");
	SectionName = TEXT("Mask");
}

#if WITH_EDITORONLY_DATA
UMaterialFunctionInterface* UAvaMaskSettings::GetMaterialFunction()
{
	if (UMaterialFunctionInterface* StrongMaterialFunction = MaterialFunction.Get())
	{
		return StrongMaterialFunction;		
	}

	UMaterialFunctionInterface* DefaultMaterialFunction = GetDefaultMaterialFunction();
	MaterialFunction = DefaultMaterialFunction;
	
	return DefaultMaterialFunction;
}

UMaterialFunctionInterface* UAvaMaskSettings::GetDefaultMaterialFunction()
{
	// /Script/Engine.MaterialFunction'/GeometryMask/GeometryMask/MF_ApplyGeometryMask.MF_ApplyGeometryMask'
	// const FString AssetPath = FString(TEXT("/GeometryMask")) / TEXT("GeometryMask") / TEXT("MF_ApplyGeometryMask");
	// const FString AssetPath = TEXT("/Script/Engine.MaterialFunction'/GeometryMask/GeometryMask/MF_ApplyGeometryMask.MF_ApplyGeometryMask'");
	const FString AssetPath = TEXT("/Script/Engine.MaterialFunction'/Avalanche/MaskResources/MF_ApplyMask2D_Single.MF_ApplyMask2D_Single'");
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(AssetPath));
		!AssetData.IsValid())
	{
		UE_LOG(LogAvaMask, Error, TEXT("Default MaterialFunction asset is invalid or not found: '%s'"), *AssetPath);
		return nullptr;
	}
	
	return Cast<UMaterialFunctionInterface>(StaticLoadObject(UMaterialFunctionInterface::StaticClass(), this, *AssetPath));
}
#endif
