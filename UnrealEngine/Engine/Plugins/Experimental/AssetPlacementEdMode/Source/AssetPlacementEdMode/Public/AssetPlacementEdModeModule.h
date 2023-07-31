// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IAssetTypeActions;

#if !UE_IS_COOKED_EDITOR
namespace AssetPlacementEdModeUtil
{
	bool AreInstanceWorkflowsEnabled();
}
#endif // !UE_IS_COOKED_EDITOR

class FAssetPlacementEdMode : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	TSharedPtr<IAssetTypeActions> PaletteAssetActions;
};
