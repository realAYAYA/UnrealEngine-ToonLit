// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IOculusEditorModule.h"
#include "OculusBuildAnalytics.h"
#include "Modules/ModuleInterface.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"

class FToolBarBuilder;
class FMenuBuilder;

#define OCULUS_EDITOR_MODULE_NAME "OculusEditor"

enum class ECheckBoxState : uint8;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FOculusEditorModule : public IOculusEditorModule
{
public:
	FOculusEditorModule() : bModuleValid(false) {};

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;

	void RegisterSettings();
	void UnregisterSettings();

	void PluginButtonClicked();
	FReply PluginClickFn(bool text);

	void OnEngineLoopInitComplete();

public:
	static const FName OculusPerfTabName;
	static const FName OculusPlatToolTabName;

private:

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<class SDockTab> OnSpawnPlatToolTab(const class FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	bool bModuleValid;
	FOculusBuildAnalytics* BuildAnalytics;
};

class IDetailLayoutBuilder;

class FOculusHMDSettingsDetailsCustomization : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

	FReply PluginClickPerfFn(bool text);
	FReply PluginClickPlatFn(bool text);

	void OnEnableBuildTelemetry(ECheckBoxState NewState);
	ECheckBoxState GetBuildTelemetryCheckBoxState() const;
	bool GetBuildTelemetryCheckBoxEnabled() const;
	EVisibility GetOculusHMDAvailableWarningVisibility() const;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

