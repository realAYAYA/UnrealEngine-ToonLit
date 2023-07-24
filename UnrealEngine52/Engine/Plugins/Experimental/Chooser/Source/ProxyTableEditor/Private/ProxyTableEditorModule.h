// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

namespace UE::ProxyTableEditor
{

class FAssetTypeActions_ChooserTable;
class FAssetTypeActions_ProxyTable;

class FModule : public IModuleInterface
{
private:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TSharedPtr<FAssetTypeActions_ProxyTable> AssetTypeActions_ProxyTable;
};

}