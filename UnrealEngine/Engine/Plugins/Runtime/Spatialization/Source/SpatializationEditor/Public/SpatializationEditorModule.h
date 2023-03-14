// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "AssetTypeActions_Base.h"
#include "Modules/ModuleManager.h"
#include "ITDSpatializer.h"

class FSpatializationEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

private:
	TArray<TSharedPtr<FAssetTypeActions_Base>> AssetActions;
};
