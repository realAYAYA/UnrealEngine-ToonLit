// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusEditorModule.h"
#include "OculusToolStyle.h"
#include "OculusToolCommands.h"
#include "OculusToolWidget.h"
#include "OculusPlatformToolWidget.h"
#include "OculusAssetDirectory.h"
#include "OculusHMDRuntimeSettings.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "PropertyEditorModule.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISettingsModule.h"
#include "OculusEditorSettings.h"

#define LOCTEXT_NAMESPACE "OculusEditor"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

const FName FOculusEditorModule::OculusPerfTabName = FName("OculusPerfCheck");
const FName FOculusEditorModule::OculusPlatToolTabName = FName("OculusPlatormTool");

void FOculusEditorModule::PostLoadCallback()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
}

void FOculusEditorModule::StartupModule()
{
	bModuleValid = true;
	RegisterSettings();
	FOculusAssetDirectory::LoadForCook();

	if (!IsRunningCommandlet())
	{
		FOculusToolStyle::Initialize();
		FOculusToolStyle::ReloadTextures();

		FOculusToolCommands::Register();

		PluginCommands = MakeShareable(new FUICommandList);

		PluginCommands->MapAction(
			FOculusToolCommands::Get().OpenPluginWindow,
			FExecuteAction::CreateRaw(this, &FOculusEditorModule::PluginButtonClicked),
			FCanExecuteAction());

		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

		// Adds an option to launch the tool to Window->Developer Tools.
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("Miscellaneous", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FOculusEditorModule::AddMenuExtension));
		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);

		/*
		// If you want to make the tool even easier to launch, and add a toolbar button.
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Launch", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FOculusEditorModule::AddToolbarExtension));
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
		*/

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OculusPerfTabName, FOnSpawnTab::CreateRaw(this, &FOculusEditorModule::OnSpawnPluginTab))
			.SetDisplayName(LOCTEXT("FOculusEditorTabTitle", "Oculus Performance Check"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OculusPlatToolTabName, FOnSpawnTab::CreateRaw(this, &FOculusEditorModule::OnSpawnPlatToolTab))
			.SetDisplayName(LOCTEXT("FOculusPlatfToolTabTitle", "Oculus Platform Tool"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);

		FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FOculusEditorModule::OnEngineLoopInitComplete);
	}
}

void FOculusEditorModule::ShutdownModule()
{
	if (!bModuleValid)
	{
		return;
	}

	if (!IsRunningCommandlet())
	{
		FOculusToolStyle::Shutdown();
		FOculusToolCommands::Unregister();
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OculusPerfTabName);
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OculusPlatToolTabName);
		FOculusBuildAnalytics::Shutdown();
	}

	FOculusAssetDirectory::ReleaseAll();
	if (UObjectInitialized())
	{
		UnregisterSettings();
	}
}

TSharedRef<SDockTab> FOculusEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	auto myTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOculusToolWidget)
		];


	return myTab;
}

TSharedRef<SDockTab> FOculusEditorModule::OnSpawnPlatToolTab(const FSpawnTabArgs& SpawnTabArgs)
{
	auto myTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOculusPlatformToolWidget)
		];

	return myTab;
}

void FOculusEditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "OculusVR",
			LOCTEXT("RuntimeSettingsName", "OculusVR"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the OculusVR plugin"),
			GetMutableDefault<UDEPRECATED_UOculusHMDRuntimeSettings>()
		);

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UDEPRECATED_UOculusHMDRuntimeSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FOculusHMDSettingsDetailsCustomization::MakeInstance));
	}
}

void FOculusEditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "OculusVR");
	}
}

FReply FOculusEditorModule::PluginClickFn(bool text)
{
	PluginButtonClicked();
	return FReply::Handled();
}

void FOculusEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(OculusPerfTabName);
}

void FOculusEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	bool v = false;
	GConfig->GetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bAddMenuOption"), v, GEditorIni);
	if (v)
	{
		Builder.AddMenuEntry(FOculusToolCommands::Get().OpenPluginWindow);
	}
}

void FOculusEditorModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FOculusToolCommands::Get().OpenPluginWindow);
}

void FOculusEditorModule::OnEngineLoopInitComplete()
{
	BuildAnalytics = FOculusBuildAnalytics::GetInstance();
}

TSharedRef<IDetailCustomization> FOculusHMDSettingsDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FOculusHMDSettingsDetailsCustomization);
}

FReply FOculusHMDSettingsDetailsCustomization::PluginClickPerfFn(bool text)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FOculusEditorModule::OculusPerfTabName);
	return FReply::Handled();
}

FReply FOculusHMDSettingsDetailsCustomization::PluginClickPlatFn(bool text)
{
	FGlobalTabmanager::Get()->TryInvokeTab(FOculusEditorModule::OculusPlatToolTabName);
	return FReply::Handled();
}

void FOculusHMDSettingsDetailsCustomization::OnEnableBuildTelemetry(ECheckBoxState NewState)
{
	FOculusBuildAnalytics* analytics = FOculusBuildAnalytics::GetInstance();
	// If we aren't able to get an instance for some reason, don't try an invoke it; but do update the setting
	if (analytics != NULL) {
		analytics->OnTelemetryToggled(NewState == ECheckBoxState::Checked);
	}

	GConfig->SetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bEnableOculusBuildTelemetry"), NewState == ECheckBoxState::Checked, GEditorIni);
	GConfig->Flush(0);
}

ECheckBoxState FOculusHMDSettingsDetailsCustomization::GetBuildTelemetryCheckBoxState() const
{
	bool v;
	GConfig->GetBool(TEXT("/Script/OculusEditor.OculusEditorSettings"), TEXT("bEnableOculusBuildTelemetry"), v, GEditorIni);
	return v ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

// Disable telemetry for NativeOpenXR, since it doesn't go through OVRPlugin
bool FOculusHMDSettingsDetailsCustomization::GetBuildTelemetryCheckBoxEnabled() const
{
	FString XrApi;
	GConfig->GetString(TEXT("/Script/OculusHMD.OculusHMDRuntimeSettings"), TEXT("XrApi"), XrApi, GEngineIni);
	return !XrApi.Equals(FString("NativeOpenXR"));
}

EVisibility FOculusHMDSettingsDetailsCustomization::GetOculusHMDAvailableWarningVisibility() const
{
	FString XrApi;
	GConfig->GetString(TEXT("/Script/OculusHMD.OculusHMDRuntimeSettings"), TEXT("XrApi"), XrApi, GEngineIni);
	return FOculusBuildAnalytics::IsOculusHMDAvailable() || XrApi.Equals(FString("NativeOpenXR")) ? EVisibility::Collapsed : EVisibility::Visible;
}

void FOculusHMDSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Labeled "General Oculus" instead of "General" to enable searchability. The button "Launch Oculus Utilities Window" doesn't show up if you search for "Oculus"
	IDetailCategoryBuilder& CategoryBuilder = DetailLayout.EditCategory("General Oculus", FText::GetEmpty(), ECategoryPriority::Important);
	CategoryBuilder.AddCustomRow(LOCTEXT("General Oculus", "General"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("LaunchTool", "Launch Oculus Performance Window"))
							.OnClicked(this, &FOculusHMDSettingsDetailsCustomization::PluginClickPerfFn, true)
						]
					+ SHorizontalBox::Slot().FillWidth(8)
				]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.Text(LOCTEXT("LaunchPlatTool", "Launch Oculus Platform Window"))
							.OnClicked(this, &FOculusHMDSettingsDetailsCustomization::PluginClickPlatFn, true)
						]
					+ SHorizontalBox::Slot().FillWidth(8)
				]
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("EnableBuildTelemetry", "Enable Oculus Build Telemetry"))
								.ToolTipText(LOCTEXT("EnableBuildTelemetryToolTip", "Enables detailed timing for the major build steps. This measures time spent in each build stageand transmits the time spent per stage to Oculus. This information is used by Oculus to guide work related to optimizing the build process."))
							]
						]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SBox).WidthOverride(10.f)
						]
					+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SCheckBox)
							.OnCheckStateChanged(this, &FOculusHMDSettingsDetailsCustomization::OnEnableBuildTelemetry)
							.IsChecked(this, &FOculusHMDSettingsDetailsCustomization::GetBuildTelemetryCheckBoxState)
							.IsEnabled(this, &FOculusHMDSettingsDetailsCustomization::GetBuildTelemetryCheckBoxEnabled)
						]
					+ SHorizontalBox::Slot().FillWidth(8)
				]
			+ SVerticalBox::Slot().AutoHeight().Padding(5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(4)
						[
							SNew(SImage)
							.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
							.Visibility(this, &FOculusHMDSettingsDetailsCustomization::GetOculusHMDAvailableWarningVisibility)
						]
					+ SHorizontalBox::Slot().FillWidth(8).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Text(LOCTEXT("OculusHMDNotConnected", "WARNING: Build telemetry functionality may be limited, because the Oculus HMD was not found to be connected, available, or configured correctly. Check the Devices tab of the Oculus PC app."))
							.ColorAndOpacity(FLinearColor(0.7f, 0.23f, 0.23f, 1.f))
							.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
							.Visibility(this, &FOculusHMDSettingsDetailsCustomization::GetOculusHMDAvailableWarningVisibility)
						]
				]
		];
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(FOculusEditorModule, OculusEditor);

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE