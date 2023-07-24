// Copyright Epic Games, Inc. All Rights Reserved.

#include "VPCustomUIHandler.h"

#include "Blueprint/UserWidget.h"
#include "EditorUtilityWidget.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditorActions.h"
#include "IVREditorModule.h"
#include "LevelEditorActions.h"
#include "VREditorMode.h"
#include "UI/VREditorUISystem.h"
#include "UObject/ConstructorHelpers.h"
#include "VREditorStyle.h"
#include "VPUtilitiesEditorModule.h"
#include "VPUtilitiesEditorSettings.h"
#include "WidgetBlueprint.h"
#include "Widgets/SWidget.h"
#include "UI/VREditorFloatingUI.h"
#include "VPScoutingSubsystem.h"
#include "VPSettings.h"

#define LOCTEXT_NAMESPACE "VPCustomUIHandler"

void UVPCustomUIHandler::Init()
{
	VRRadialMenuWindowsExtension = IVREditorModule::Get().GetRadialMenuExtender()->AddMenuExtension(
		"Windows",
		EExtensionHook::After,
		nullptr, 
		FMenuExtensionDelegate::CreateUObject(this, &UVPCustomUIHandler::FillVRRadialMenuWindows));
}


void UVPCustomUIHandler::Uninit()
{
	if (IVREditorModule::IsAvailable())
	{
		IVREditorModule::Get().GetRadialMenuExtender()->RemoveExtension(VRRadialMenuWindowsExtension.ToSharedRef());
	}
}


void UVPCustomUIHandler::FillVRRadialMenuWindows(FMenuBuilder& MenuBuilder)
{	
	const TSoftClassPtr<UEditorUtilityWidget> WidgetClass = GetDefault<UVPUtilitiesEditorSettings>()->VirtualScoutingUI;
	WidgetClass.LoadSynchronous();
	if (WidgetClass.IsValid())
	{
		VirtualProductionWidget = WidgetClass.Get();
	}
	
	if (VirtualProductionWidget == nullptr)
	{
		UE_LOG(LogVPUtilitiesEditor, Warning, TEXT("UVPCustomUIHandler::FillVRRadialMenuWindows - Could not get default 'Virtual Scouting User Interface' from Virtual Production settings."));
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("VirtualProductionTools", "Virtual Production"),
		FText(),
		FSlateIcon(), // FSlateIcon(FVREditorStyle::GetStyleSetName(), "VREditorStyle.WorldSpace"),
		FUIAction
		(
			FExecuteAction::CreateUObject(this, &UVPCustomUIHandler::UpdateUMGUIForVR, VirtualProductionWidget, UVPScoutingSubsystem::GetVProdPanelID(EVProdPanelIDs::Main), FVector2D(800.0f, 600.0f)),
			FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::DefaultCanExecuteAction)
		),
		NAME_None,
		EUserInterfaceActionType::CollapsedButton
	);
}


void UVPCustomUIHandler::UpdateUMGUIForVR(TSubclassOf<UUserWidget> InWidget, FName Name, FVector2D InSize)
{
	bool bPanelVisible = IVREditorModule::Get().GetVRMode()->GetUISystem().IsShowingEditorUIPanel(Name);
	FVREditorFloatingUICreationContext CreationContext;
	CreationContext.PanelID = Name;
	CreationContext.PanelSize = InSize;

	if (bPanelVisible)
	{	
		IVREditorModule::Get().UpdateExternalUMGUI(CreationContext);
	}
	else
	{
		CreationContext.WidgetClass = InWidget;
		IVREditorModule::Get().UpdateExternalUMGUI(CreationContext);
	}
}


void UVPCustomUIHandler::UpdateSlateUIForVR(TSharedRef<SWidget> InWidget, FName Name, FVector2D InSize)
{
	bool bPanelVisible = IVREditorModule::Get().GetVRMode()->GetUISystem().IsShowingEditorUIPanel(Name);

	if (bPanelVisible)
	{
		IVREditorModule::Get().UpdateExternalSlateUI(SNullWidget::NullWidget, Name);
	}
	else
	{
		IVREditorModule::Get().UpdateExternalSlateUI(InWidget, Name);
	}
}


#undef LOCTEXT_NAMESPACE