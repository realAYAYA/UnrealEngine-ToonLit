// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCharacterFXEditorModule.h"

class FLayoutExtender;
namespace UE::Chaos::ClothAsset
{
	class FAssetTypeActions_ClothAsset;
	class FAssetTypeActions_ClothPreset;
}

class CHAOSCLOTHASSETEDITOR_API FChaosClothAssetEditorModule : public FBaseCharacterFXEditorModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	UE::Chaos::ClothAsset::FAssetTypeActions_ClothAsset* AssetTypeActions_ClothAsset;
	UE::Chaos::ClothAsset::FAssetTypeActions_ClothPreset* AssetTypeActions_ClothPreset;

};
