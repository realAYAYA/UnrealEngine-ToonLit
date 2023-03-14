// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

class FAssetTypeActions_MaterialInstanceDynamic : public FAssetTypeActions_MaterialInterface
{
public:
	virtual UClass* GetSupportedClass() const override { return UMaterialInstanceDynamic::StaticClass(); }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};
