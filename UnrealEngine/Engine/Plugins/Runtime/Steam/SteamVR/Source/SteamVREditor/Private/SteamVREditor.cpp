/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "SteamVREditor.h"
#include "IMotionController.h"
#include "Features/IModularFeatures.h"
#include "SteamVREditorStyle.h"
#include "SteamVREditorCommands.h"
#include "LevelEditor.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "ISettingsModule.h"
#include "SteamVREditorSettings.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Input/SButton.h"
#include "DetailWidgetRow.h"
#include "Runtime/Core/Public/CoreGlobals.h"
#include "Runtime/Core/Public/Misc/ConfigCacheIni.h"
#include "ToolMenus.h"

static const FName SteamVREditorTabName("SteamVREditor");

LLM_DEFINE_TAG(SteamVR_SteamVREditor);
#define LOCTEXT_NAMESPACE "FSteamVREditorModule"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void FSteamVREditorModule::StartupModule()
{
	LLM_SCOPE_BYTAG(SteamVR_SteamVREditor);

	RegisterSettings();

	FSteamVREditorStyle::Initialize();
	FSteamVREditorStyle::ReloadTextures();

	FSteamVREditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	// Dummy action for main toolbar button
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::PluginButtonClicked),
		FCanExecuteAction());
	
	// Regenerate Action Manifest
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().JsonActionManifest,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::JsonRegenerateActionManifest),
		FCanExecuteAction());

	// Regenerate Controller Bindings
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().JsonControllerBindings,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::JsonRegenerateControllerBindings),
		FCanExecuteAction());

	// Reload Action Manifest
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().ReloadActionManifest,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::ReloadActionManifest),
		FCanExecuteAction());

	// Launch Bindings URL
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().LaunchBindingsURL,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::LaunchBindingsURL),
		FCanExecuteAction());
	
	// Add Sample Inputs
	PluginCommands->MapAction(
		FSteamVREditorCommands::Get().AddSampleInputs,
		FExecuteAction::CreateRaw(this, &FSteamVREditorModule::AddSampleInputs),
		FCanExecuteAction());

	FSteamVREditorStyle MenuStyle = FSteamVREditorStyle();
	MenuStyle.Initialize();

	UToolMenu* ToolBar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
	FToolMenuSection& Section = ToolBar->FindOrAddSection("Settings");
	Section.AddEntry(FToolMenuEntry::InitComboButton(
		"SteamVR Input",
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction(),
			FGetActionCheckState(),
			FIsActionButtonVisible::CreateRaw(this, &FSteamVREditorModule::ShowSteamVRInputToolbarDropdown)
		),
		FOnGetContent::CreateRaw(this, &FSteamVREditorModule::FillComboButton, PluginCommands),
		LOCTEXT("SteamVRInputBtn", "SteamVR Input"),
		LOCTEXT("SteamVRInputBtnTootlip", "SteamVR Input"),
		FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.PluginAction")
	));
}

bool FSteamVREditorModule::ShowSteamVRInputToolbarDropdown()
{
	// Only add the toolbar if SteamVR is the currently active tracking system
	static FName SystemName(TEXT("SteamVR"));
	if (GEngine && GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
	{
		// Only show the toolbar button if enabled by the user for this project.  Setting is under Project Settings > PLugins > SteamVR now instead.
		UDEPRECATED_USteamVREditorSettings* SteamVREditorSettings = GetMutableDefault<UDEPRECATED_USteamVREditorSettings>();
		if (SteamVREditorSettings->bShowSteamVrInputToolbarButton)
		{
			return true;
		}
	}
	
	return false;
}

void FSteamVREditorModule::ShutdownModule()
{
	LLM_SCOPE_BYTAG(SteamVR_SteamVREditor);

	FSteamVREditorStyle::Shutdown();

	FSteamVREditorCommands::Unregister();
}

void FSteamVREditorModule::PluginButtonClicked()
{
	// Empty on purpose
}

void FSteamVREditorModule::JsonRegenerateActionManifest()
{
	UDEPRECATED_USteamVRInputDeviceFunctionLibrary::RegenActionManifest();
}

void FSteamVREditorModule::JsonRegenerateControllerBindings()
{
	UDEPRECATED_USteamVRInputDeviceFunctionLibrary::RegenControllerBindings();
}

void FSteamVREditorModule::ReloadActionManifest()
{
	UDEPRECATED_USteamVRInputDeviceFunctionLibrary::ReloadActionManifest();
}

void FSteamVREditorModule::LaunchBindingsURL()
{
	UDEPRECATED_USteamVRInputDeviceFunctionLibrary::LaunchBindingsURL();
}

void FSteamVREditorModule::AddSampleInputs()
{
	//// Get Existing Input Settings
	//auto DefaultInputSettings = GetDefault<UInputSettings>();
	//TArray<FInputAxisKeyMapping> ExistingAxisKeys = DefaultInputSettings->GetAxisMappings();
	//TArray<FInputActionKeyMapping> ExistingActionKeys = DefaultInputSettings->GetActionMappings();

	//// Create new Input Settings
	//UInputSettings* InputSettings = GetMutableDefault<UInputSettings>();

	//if (InputSettings->IsValidLowLevel())
	//{
	//	// Teleport
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Valve_Index_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::HTC_Vive_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::HTC_Cosmos_Controller_Joystick_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Oculus_Touch_Controller_Joystick_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), EKeys::Windows_MR_Controller_Trackpad_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportLeft")), FGamepadKeyNames::MotionController_Left_Thumbstick);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Valve_Index_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::HTC_Vive_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::HTC_Cosmos_Controller_Joystick_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Oculus_Touch_Controller_Joystick_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), EKeys::Windows_MR_Controller_Trackpad_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("TeleportRight")), FGamepadKeyNames::MotionController_Right_Thumbstick);

	//	// Grab
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Valve_Index_Controller_Grip_Grab_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::HTC_Vive_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::HTC_Cosmos_Controller_Grip_Click_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Oculus_Touch_Controller_Grip_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), EKeys::Windows_MR_Controller_Grip_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabLeft")), FGamepadKeyNames::MotionController_Left_Trigger);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Valve_Index_Controller_Grip_Grab_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::HTC_Vive_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::HTC_Cosmos_Controller_Grip_Click_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Oculus_Touch_Controller_Grip_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), EKeys::Windows_MR_Controller_Grip_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("GrabRight")), FGamepadKeyNames::MotionController_Right_Trigger);

	//	// Fire Arrow
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Valve_Index_Controller_Pinch_Grab_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::HTC_Vive_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::HTC_Cosmos_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Oculus_Touch_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), EKeys::Windows_MR_Controller_Trigger_Press_Left);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowLeft")), FGamepadKeyNames::MotionController_Left_Trigger);

	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Valve_Index_Controller_Pinch_Grab_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::HTC_Vive_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::HTC_Cosmos_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Oculus_Touch_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), EKeys::Windows_MR_Controller_Trigger_Press_Right);
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("FireArrowRight")), FGamepadKeyNames::MotionController_Right_Trigger);

	//	// HMD Proximity
	//	AddUniqueActionMapping(ExistingActionKeys, InputSettings, FName(TEXT("HeadsetOn")), GenericKeys::SteamVR_HMD_Proximity);

	//	// Move Right
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Valve_Index_Controller_Thumbstick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), EKeys::Windows_MR_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_Y")), FGamepadKeyNames::MotionController_Right_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Valve_Index_Controller_Thumbstick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), EKeys::Windows_MR_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveRight_X")), FGamepadKeyNames::MotionController_Right_Thumbstick_X);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Valve_Index_Controller_Thumbstick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), EKeys::Windows_MR_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_Y")), FGamepadKeyNames::MotionController_Left_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Valve_Index_Controller_Thumbstick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), EKeys::Windows_MR_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("MoveLeft_X")), FGamepadKeyNames::MotionController_Left_Thumbstick_X);

	//	// Teleport Direction
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Valve_Index_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), EKeys::Windows_MR_Controller_Trackpad_Y_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_Y")), FGamepadKeyNames::MotionController_Right_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Valve_Index_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), EKeys::Windows_MR_Controller_Trackpad_X_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionRight_X")), FGamepadKeyNames::MotionController_Right_Thumbstick_X);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Valve_Index_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::HTC_Vive_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::HTC_Cosmos_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Oculus_Touch_Controller_Joystick_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), EKeys::Windows_MR_Controller_Trackpad_Y_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_Y")), FGamepadKeyNames::MotionController_Left_Thumbstick_Y);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Valve_Index_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::HTC_Vive_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::HTC_Cosmos_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Oculus_Touch_Controller_Joystick_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), EKeys::Windows_MR_Controller_Trackpad_X_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("TeleportDirectionLeft_X")), FGamepadKeyNames::MotionController_Left_Thumbstick_X);

	//	// Squeeze
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Valve_Index_Controller_GripForce_Axis_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::HTC_Vive_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::HTC_Cosmos_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Oculus_Touch_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), EKeys::Windows_MR_Controller_Trigger_Pull_Left);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeLeft")), FGamepadKeyNames::MotionController_Left_TriggerAxis);

	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Valve_Index_Controller_GripForce_Axis_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::HTC_Vive_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::HTC_Cosmos_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Oculus_Touch_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), EKeys::Windows_MR_Controller_Trigger_Pull_Right);
	//	AddUniqueAxisMapping(ExistingAxisKeys, InputSettings, FName(TEXT("SqueezeRight")), FGamepadKeyNames::MotionController_Right_TriggerAxis);
	//	

	//	// Update the config file
	//	InputSettings->SaveKeyMappings();
	//	InputSettings->TryUpdateDefaultConfigFile();
	//}
}

bool FSteamVREditorModule::AddUniqueAxisMapping(TArray<FInputAxisKeyMapping> ExistingAxisKeys, UInputSettings* InputSettings, FName ActionName, FKey ActionKey)
{
	// Create new axis mapping
	FInputAxisKeyMapping NewAxisMapping = FInputAxisKeyMapping(ActionName, ActionKey);

	// Check if this mapping already exists in the project
	if (ExistingAxisKeys.Find(NewAxisMapping) < 1)
	{
		// If none, create a new one
		InputSettings->AddAxisMapping(NewAxisMapping);
		return true;
	}
	
return false;
}

bool FSteamVREditorModule::AddUniqueActionMapping(TArray<FInputActionKeyMapping> ExistingActionKeys, UInputSettings* InputSettings, FName ActionName, FKey ActionKey)
{
	// Create new action mapping
	FInputActionKeyMapping NewActionMapping = FInputActionKeyMapping(ActionName, ActionKey);

	// Check if this mapping already exists in the project
	if (ExistingActionKeys.Find(NewActionMapping) < 1)
	{
		// If none, create a new one
		InputSettings->AddActionMapping(NewActionMapping);
		return true;
	}

	return false;
}

TSharedRef<SWidget> FSteamVREditorModule::FillComboButton(TSharedPtr<class FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().JsonActionManifest, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.JsonActionManifest"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().JsonControllerBindings, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.JsonControllerBindings"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().ReloadActionManifest, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.ReloadActionManifest"));
	MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().LaunchBindingsURL, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.LaunchBindingsURL"));
	//MenuBuilder.AddMenuEntry(FSteamVREditorCommands::Get().AddSampleInputs, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FSteamVREditorStyle::GetStyleSetName(), "SteamVREditor.AddSampleInputs"));

	return MenuBuilder.MakeWidget();
}

void FSteamVREditorModule::RegisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->RegisterSettings("Project", "Plugins", "SteamVR",
			LOCTEXT("SteamVREditorSettingsName", "SteamVR"),
			LOCTEXT("SteamVREditorSettingsDescription", "Configure the SteamVR plugin"),
			GetMutableDefault<UDEPRECATED_USteamVREditorSettings>()
		);

		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(UDEPRECATED_USteamVREditorSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FSteamVRSettingsDetailsCustomization::MakeInstance));
	}
}

void FSteamVREditorModule::UnregisterSettings()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "SteamVR");
	}
}

	
IMPLEMENT_MODULE(FSteamVREditorModule, SteamVREditor)

TSharedRef<IDetailCustomization> FSteamVRSettingsDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FSteamVRSettingsDetailsCustomization);
}

void FSteamVRSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	
	IDetailCategoryBuilder& CategoryBuilder = DetailLayout.EditCategory("SteamVR Input", FText::GetEmpty());
	CategoryBuilder.AddCustomRow(LOCTEXT("SteamVR Input Category", "SteamVR Input"))
		.WholeRowContent()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Regenerate Action Manifest", "Regenerate Action Manifest"))
		.OnClicked_Raw(this, &FSteamVRSettingsDetailsCustomization::RegenActionManifest)
		]
	+ SHorizontalBox::Slot().FillWidth(8)
		]
	+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Regenerate Controller Bindings", "Regenerate Controller Bindings"))
		.OnClicked_Raw(this, &FSteamVRSettingsDetailsCustomization::RegenerateControllerBindings)
		]
	+ SHorizontalBox::Slot().FillWidth(8)
		]

	+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Reload Action Manifest", "Reload Action Manifest"))
		.OnClicked_Raw(this, &FSteamVRSettingsDetailsCustomization::ReloadActionManifest)
		]
	+ SHorizontalBox::Slot().FillWidth(8)
		]

	+ SVerticalBox::Slot().AutoHeight().Padding(2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Launch SteamVR Bindings Dashboard", "Launch SteamVR Bindings Dashboard"))
		.OnClicked_Raw(this, &FSteamVRSettingsDetailsCustomization::LaunchBindingsURL)
		]
	+ SHorizontalBox::Slot().FillWidth(8)
		]

		];
}

FReply FSteamVRSettingsDetailsCustomization::RegenActionManifest()
{
	FSteamVREditorModule& SteamVREditorModule = FModuleManager::LoadModuleChecked<FSteamVREditorModule>("SteamVREditor");
	SteamVREditorModule.JsonRegenerateActionManifest();
	return FReply::Handled();
}

FReply FSteamVRSettingsDetailsCustomization::RegenerateControllerBindings()
{
	FSteamVREditorModule& SteamVREditorModule = FModuleManager::LoadModuleChecked<FSteamVREditorModule>("SteamVREditor");
	SteamVREditorModule.JsonRegenerateControllerBindings();
	return FReply::Handled();
}

FReply FSteamVRSettingsDetailsCustomization::ReloadActionManifest()
{
	FSteamVREditorModule& SteamVREditorModule = FModuleManager::LoadModuleChecked<FSteamVREditorModule>("SteamVREditor");
	SteamVREditorModule.ReloadActionManifest();
	return FReply::Handled();
}

FReply FSteamVRSettingsDetailsCustomization::LaunchBindingsURL()
{
	FSteamVREditorModule& SteamVREditorModule = FModuleManager::LoadModuleChecked<FSteamVREditorModule>("SteamVREditor");
	SteamVREditorModule.LaunchBindingsURL();
	return FReply::Handled();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
