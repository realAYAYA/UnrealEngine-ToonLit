// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class RIGLOGICEDITOR_API FRigLogicEditor : public IModuleInterface 
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

private:
	TArray<TSharedRef<class IAssetTypeActions>> AssetTypeActions;
};
