// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class IKRIGEDITOR_API FIKRigEditor : public IModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

private:
	TSharedPtr<class FAssetTypeActions_AnimationAssetRetarget> RetargetAnimationAssetAction;
	TSharedPtr<class FAssetTypeActions_IKRigDefinition> IKRigDefinitionAssetAction;
	TSharedPtr<class FAssetTypeActions_IKRetargeter> IKRetargeterAssetAction;

	TArray<FName> ClassesToUnregisterOnShutdown;
};

DECLARE_LOG_CATEGORY_EXTERN(LogIKRigEditor, Warning, All);
