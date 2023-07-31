// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "AssetTypeActions_Base.h"
#include "CurveEditorTypes.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAudioModulationEditor, Log, All);

class FAudioModulationEditorModule : public IModuleInterface
{
public:
	FAudioModulationEditorModule();

	TSharedPtr<FExtensibilityManager> GetModulationPatchMenuExtensibilityManager();
	TSharedPtr<FExtensibilityManager> GetModulationPatchToolbarExtensibilityManager();

	virtual void StartupModule() override;

	void RegisterCustomPropertyLayouts();

	virtual void ShutdownModule() override;

private:
	void SetIcon(const FString& ClassName);

	TArray<TSharedPtr<FAssetTypeActions_Base>> AssetActions;

	TSharedPtr<FExtensibilityManager> ModulationPatchMenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ModulationPatchToolBarExtensibilityManager;

	TSharedPtr<FSlateStyleSet> StyleSet;
};
