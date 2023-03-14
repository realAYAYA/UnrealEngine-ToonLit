// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class IAssetTypeActions;

class FContextualAnimationEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FContextualAnimationEditorModule& Get();

private:

	FDelegateHandle MovieSceneAnimNotifyTrackEditorHandle;

	TSharedPtr<IAssetTypeActions> ContextualAnimAssetActions;
};
