// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingToolsEditorModeToolkit.h"
#include "EdModeInteractiveToolsContext.h"
#include "ModelingToolsEditorMode.h"
#include "Framework/Commands/UICommandList.h"
#include "ModelingToolsManagerActions.h"
#include "ModelingToolsEditorModeSettings.h"
#include "ModelingToolsEditorModeStyle.h"

#include "ModelingSelectionInteraction.h"
#include "Selection/GeometrySelectionManager.h"

#include "ModelingComponentsSettings.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"

#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "STransformGizmoNumericalUIOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SComboButton.h"

// for showing toast notifications
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "ToolMenus.h"
// to toggle component instance selection

#define LOCTEXT_NAMESPACE "FModelingToolsEditorModeToolkit"


namespace UELocal
{

void MakeSubMenu_QuickSettings(FMenuBuilder& MenuBuilder)
{
	const FUIAction OpenModelingModeProjectSettings(
		FExecuteAction::CreateLambda([]
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->ShowViewer("Project", "Plugins", "ModelingMode");
			}
		}), FCanExecuteAction(), FIsActionChecked());
	MenuBuilder.AddMenuEntry(LOCTEXT("ModelingModeProjectSettings", "Modeling Mode (Project)"), 
		LOCTEXT("ModelingModeProjectSettings_Tooltip", "Jump to the Project Settings for Modeling Mode. Project Settings are Project-specific."),
		FSlateIcon(), OpenModelingModeProjectSettings, NAME_None, EUserInterfaceActionType::Button);

	const FUIAction OpenModelingModeEditorSettings(
		FExecuteAction::CreateLambda([]
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->ShowViewer("Editor", "Plugins", "ModelingMode");
			}
		}), FCanExecuteAction(), FIsActionChecked());
	MenuBuilder.AddMenuEntry(LOCTEXT("ModelingModeEditorSettings", "Modeling Mode (Editor)"), 
		LOCTEXT("ModelingModeEditorSettings_Tooltip", "Jump to the Editor Settings for Modeling Mode. Editor Settings apply across all Projects."),
		FSlateIcon(), OpenModelingModeEditorSettings, NAME_None, EUserInterfaceActionType::Button);

	const FUIAction OpenModelingToolsProjectSettings(
		FExecuteAction::CreateLambda([]
		{
			if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
			{
				SettingsModule->ShowViewer("Project", "Plugins", "Modeling Mode Tools");
			}
		}), FCanExecuteAction(), FIsActionChecked());
	MenuBuilder.AddMenuEntry(LOCTEXT("ModelingToolsProjectSettings", "Modeling Tools (Project)"), 
		LOCTEXT("ModelingToolsProjectSettings_Tooltip", "Jump to the Project Settings for Modeling Tools. Project Settings are Project-specific."),
		FSlateIcon(), OpenModelingToolsProjectSettings, NAME_None, EUserInterfaceActionType::Button);

}

void MakeSubMenu_GizmoVisibilityMode(FModelingToolsEditorModeToolkit* Toolkit, FMenuBuilder& MenuBuilder)
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	UEditorInteractiveToolsContext* Context = Toolkit->GetScriptableEditorMode()->GetInteractiveToolsContext(EToolsContextScope::EdMode);

	// toggle for Combined/Separate Gizmo Mode
	const FUIAction GizmoMode_Combined(
		FExecuteAction::CreateLambda([Settings, Context]
		{
			Settings->bRespectLevelEditorGizmoMode = ! Settings->bRespectLevelEditorGizmoMode;
			Context->SetForceCombinedGizmoMode(Settings->bRespectLevelEditorGizmoMode == false);
			Settings->SaveConfig();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Settings]()
		{
			return Settings->bRespectLevelEditorGizmoMode == false;
		}));
	MenuBuilder.AddMenuEntry(LOCTEXT("GizmoMode_Combined", "Combined Gizmo"), 
		LOCTEXT("GizmoMode_Combined_Tooltip", "Ignore Level Editor Gizmo Mode and always use a Combined Transform Gizmo in Modeling Tools"),
		FSlateIcon(), GizmoMode_Combined, NAME_None, EUserInterfaceActionType::ToggleButton);

	// toggle for Absolute Grid Snapping mode in World Coordinates
	const FUIAction GizmoMode_AbsoluteWorldSnap(
		FExecuteAction::CreateLambda([Settings, Context]
		{
			Context->SetAbsoluteWorldSnappingEnabled( ! Context->GetAbsoluteWorldSnappingEnabled() );
			Settings->bEnableAbsoluteWorldSnapping = Context->GetAbsoluteWorldSnappingEnabled();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Context]()
		{
			return Context->GetAbsoluteWorldSnappingEnabled();
		}));
	MenuBuilder.AddMenuEntry(LOCTEXT("GizmoMode_AbsoluteWorldSnap", "World Grid Snapping"), 
		LOCTEXT("GizmoMode_AbsoluteWorldSnap_Tooltip", "Snap Translation/Rotation to Absolute Grid Coordinates in the World Coordinate System, instead of Relative to the initial position"),
		FSlateIcon(), GizmoMode_AbsoluteWorldSnap, NAME_None, EUserInterfaceActionType::ToggleButton);
	

	MenuBuilder.AddSubMenu(
		LOCTEXT("TransformPanelSubMenu", "Transform Panel"), LOCTEXT("TransformPanelSubMenu_ToolTip", "Configure the Gizmo Transform Panel."),
		FNewMenuDelegate::CreateLambda([Toolkit](FMenuBuilder& SubMenuBuilder) {
			TSharedPtr<STransformGizmoNumericalUIOverlay> NumericalUI = Toolkit->GetGizmoNumericalUIOverlayWidget();
			if (ensure(NumericalUI.IsValid()))
			{
				NumericalUI->MakeNumericalUISubMenu(SubMenuBuilder);
			}
		}));
}



void MakeSubMenu_SelectionSupport(FModelingToolsEditorModeToolkit* Toolkit, FMenuBuilder& MenuBuilder)
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();

	// toggle for Combined/Separate Gizmo Mode
	const FUIAction EnableSelectionsToggle(
		FExecuteAction::CreateLambda([Toolkit, Settings]
		{
			Settings->bEnablePersistentSelections = ! Settings->bEnablePersistentSelections;
			Settings->SaveConfig();
			Toolkit->NotifySelectionSystemEnabledStateModified();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Settings]()
		{
			return Settings->bEnablePersistentSelections;
		}));
	MenuBuilder.AddMenuEntry(LOCTEXT("MeshElementSelectionToggle", "Mesh Element Selection"), 
		LOCTEXT("MeshElementSelectionToggle_Tooltip", "Enable support for in-viewport Mesh Element Selection in Modeling Mode"),
		FSlateIcon(), EnableSelectionsToggle, NAME_None, EUserInterfaceActionType::ToggleButton);
}




void MakeSubMenu_ModeToggles(FMenuBuilder& MenuBuilder)
{
	// add toggle for Instance Selection cvar
	const FUIAction ToggleInstancesEverywhereAction(
		FExecuteAction::CreateLambda([]
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TypedElements.EnableViewportSMInstanceSelection")); 
			int32 CurValue = CVar->GetInt();
			CVar->Set(CurValue == 0 ? 1 : 0);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("TypedElements.EnableViewportSMInstanceSelection"));
			return (CVar->GetInt() == 1);
		}));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ToggleInstancesSelection", "Instance Selection"), 
		LOCTEXT("ToggleInstancesSelection_Tooltip", "Enable/Disable support for direct selection of InstancedStaticMeshComponent Instances (via TypedElements.EnableViewportInstanceSelection cvar)"),
		FSlateIcon(), ToggleInstancesEverywhereAction, NAME_None, EUserInterfaceActionType::ToggleButton);

	// add toggle for Volume Snapping cvar
	const FUIAction ToggleVolumeSnappingAction(
		FExecuteAction::CreateLambda([]
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("modeling.EnableVolumeSnapping")); 
			bool CurValue = CVar->GetBool();
			CVar->Set(CurValue ? false : true);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
		{
			IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("modeling.EnableVolumeSnapping"));
			return (CVar->GetBool());
		}));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ToggleVolumeSnapping", "Volume Snapping"), 
		LOCTEXT("ToggleVolumeSnapping_Tooltip", "Enable Vertex/Face Snapping and Ray-hits against Volumes in the Level. Note that if your level contains many overlapping large volumes, some Modeling functionality may not work correctly"),
		FSlateIcon(), ToggleVolumeSnappingAction, NAME_None, EUserInterfaceActionType::ToggleButton);


}


void MakeSubMenu_DefaultMeshObjectPhysicsSettings(FMenuBuilder& MenuBuilder)
{
	UModelingComponentsSettings* Settings = GetMutableDefault<UModelingComponentsSettings>();

	const FUIAction ToggleEnableCollisionAction = FUIAction(
		FExecuteAction::CreateLambda([Settings]
		{
			Settings->bEnableCollision = !Settings->bEnableCollision;
			Settings->SaveConfig();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([Settings]()
		{
			return Settings->bEnableCollision;
		}));

	MenuBuilder.AddMenuEntry(LOCTEXT("DefaultMeshObjectEnableCollision", "Enable Collision"), TAttribute<FText>(), FSlateIcon(), ToggleEnableCollisionAction, NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.BeginSection("CollisionTypeSection", LOCTEXT("CollisionTypeSection", "Collision Mode"));

	auto MakeCollisionTypeAction = [Settings](ECollisionTraceFlag FlagType)
	{
		return FUIAction(
			FExecuteAction::CreateLambda([Settings, FlagType]
			{
				Settings->CollisionMode = FlagType;
				Settings->SaveConfig();
			}),
			FCanExecuteAction::CreateLambda([Settings]() { return Settings->bEnableCollision; }),
			FIsActionChecked::CreateLambda([Settings, FlagType]()
			{
				return Settings->CollisionMode == FlagType;
			}));
	};
	const FUIAction CollisionMode_Default = MakeCollisionTypeAction(ECollisionTraceFlag::CTF_UseDefault);
	MenuBuilder.AddMenuEntry(LOCTEXT("DefaultMeshObject_CollisionDefault", "Default"), LOCTEXT("DefaultMeshObject_CollisionDefault_Tooltip", "Configure the new Mesh Object to have the Project Default collision settings (CTF_UseDefault)"),
		FSlateIcon(), CollisionMode_Default, NAME_None, EUserInterfaceActionType::ToggleButton);
	const FUIAction CollisionMode_Both = MakeCollisionTypeAction(ECollisionTraceFlag::CTF_UseSimpleAndComplex);
	MenuBuilder.AddMenuEntry(LOCTEXT("DefaultMeshObject_CollisionBoth", "Simple And Complex"), LOCTEXT("DefaultMeshObject_CollisionBoth_Tooltip", "Configure the new Mesh Object to have both Simple and Complex collision (CTF_UseSimpleAndComplex)"),
		FSlateIcon(), CollisionMode_Both, NAME_None, EUserInterfaceActionType::ToggleButton);
	const FUIAction CollisionMode_NoComplex = MakeCollisionTypeAction(ECollisionTraceFlag::CTF_UseSimpleAsComplex);
	MenuBuilder.AddMenuEntry(LOCTEXT("DefaultMeshObject_CollisionNoComplex", "Simple Only"), LOCTEXT("DefaultMeshObject_CollisionNoComplex_Tooltip", "Configure the new Mesh Object to have only Simple collision (CTF_UseSimpleAsComplex)"),
		FSlateIcon(), CollisionMode_NoComplex, NAME_None, EUserInterfaceActionType::ToggleButton);
	const FUIAction CollisionMode_ComplexOnly = MakeCollisionTypeAction(ECollisionTraceFlag::CTF_UseComplexAsSimple);
	MenuBuilder.AddMenuEntry(LOCTEXT("DefaultMeshObject_CollisionComplexOnly", "Complex Only"), LOCTEXT("DefaultMeshObject_CollisionComplexOnly_Tooltip", "Configure the new Mesh Object to have only Complex collision (CTF_UseComplexAsSimple)"),
		FSlateIcon(), CollisionMode_ComplexOnly, NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.EndSection();
}


void MakeSubMenu_DefaultMeshObjectType(FMenuBuilder& MenuBuilder)
{
	UModelingToolsEditorModeSettings* Settings = GetMutableDefault<UModelingToolsEditorModeSettings>();

	MenuBuilder.BeginSection("DefaultObjectTypeSection", LOCTEXT("DefaultObjectTypeSection", "Default Object Type"));

	auto ShowMeshObjectDefaultChangeToast = []()
	{
		FNotificationInfo Info(LOCTEXT("ChangeDefaultMeshObjectTypeToast", 
			"Changing the Default Mesh Object Type will take effect the next time the Editor is started"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	};


	MenuBuilder.AddMenuEntry(
		LOCTEXT("DefaultMeshObjectType_StaticMesh", "Static Mesh"), 
		TAttribute<FText>(), 
		FSlateIcon(), 
		FUIAction(
			FExecuteAction::CreateLambda([Settings, ShowMeshObjectDefaultChangeToast]
			{
				Settings->DefaultMeshObjectType = EModelingModeDefaultMeshObjectType::StaticMeshAsset;
				UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::StaticMeshIdentifier;
				Settings->SaveConfig();
				ShowMeshObjectDefaultChangeToast();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Settings]()
			{
				return Settings->DefaultMeshObjectType == EModelingModeDefaultMeshObjectType::StaticMeshAsset;
			})),
		NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DefaultMeshObjectType_DynamicMesh", "Dynamic Mesh Actor"),
		TAttribute<FText>(), 
		FSlateIcon(), 
		FUIAction(
			FExecuteAction::CreateLambda([Settings, ShowMeshObjectDefaultChangeToast]
			{
				Settings->DefaultMeshObjectType = EModelingModeDefaultMeshObjectType::DynamicMeshActor;
				UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::DynamicMeshActorIdentifier;
				Settings->SaveConfig();
				ShowMeshObjectDefaultChangeToast();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Settings]()
			{
				return Settings->DefaultMeshObjectType == EModelingModeDefaultMeshObjectType::DynamicMeshActor;
			})),
		NAME_None, EUserInterfaceActionType::ToggleButton );

	MenuBuilder.AddMenuEntry(
		LOCTEXT("DefaultMeshObjectType_Volume", "Gameplay Volume"),
		TAttribute<FText>(), 
		FSlateIcon(), 
		FUIAction(
			FExecuteAction::CreateLambda([Settings, ShowMeshObjectDefaultChangeToast]
			{
				Settings->DefaultMeshObjectType = EModelingModeDefaultMeshObjectType::VolumeActor;
				UCreateMeshObjectTypeProperties::DefaultObjectTypeIdentifier = UCreateMeshObjectTypeProperties::VolumeIdentifier;
				Settings->SaveConfig();
				ShowMeshObjectDefaultChangeToast();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([Settings]()
			{
				return Settings->DefaultMeshObjectType == EModelingModeDefaultMeshObjectType::VolumeActor;
			})),
		NAME_None, EUserInterfaceActionType::ToggleButton);


	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MakeSubMenu_DefaultMeshObjectPhysicsSettings(MenuBuilder);
}

void MakeSubMenu_Selection_DragMode(FModelingToolsEditorModeToolkit* Toolkit, FMenuBuilder& MenuBuilder)
{
	UModelingSelectionInteraction* SelectionInteraction = Cast<UModelingToolsEditorMode>(Toolkit->GetScriptableEditorMode())->GetSelectionInteraction();

	auto MakeDragModeOptionAction = [SelectionInteraction](EModelingSelectionInteraction_DragMode DragMode)
	{
		return FUIAction(
			FExecuteAction::CreateLambda([SelectionInteraction, DragMode]
			{
				SelectionInteraction->SetActiveDragMode(DragMode);
				UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();
				ModelingEditorSettings->LastMeshSelectionDragMode = static_cast<int>(DragMode);
				ModelingEditorSettings->SaveConfig();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([SelectionInteraction, DragMode]()
			{
				return SelectionInteraction->GetActiveDragMode() == DragMode;
			}));
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_DragInput_None", "None"), 
		LOCTEXT("Selection_DragInput_None_Tooltip", "No drag input"),
		FSlateIcon(), MakeDragModeOptionAction(EModelingSelectionInteraction_DragMode::NoDragInteraction), NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_DragInput_Path", "Path"), 
		LOCTEXT("Selection_DragInput_Path_Tooltip", "Path Drag Input"),
		FSlateIcon(), MakeDragModeOptionAction(EModelingSelectionInteraction_DragMode::PathInteraction), NAME_None, EUserInterfaceActionType::ToggleButton);

	// marquee mode is not functional yet, so this is disabled
	//MenuBuilder.AddMenuEntry(
	//	LOCTEXT("Selection_DragInput_Marquee", "Rectangle"), 
	//	LOCTEXT("Selection_DragInput_Marquee_Tooltip", "Rectangle Marquee"),
	//	FSlateIcon(), MakeDragModeOptionAction(EModelingSelectionInteraction_DragMode::RectangleMarqueeInteraction), NAME_None, EUserInterfaceActionType::ToggleButton);

}


void MakeSubMenu_Selection_MeshType(FModelingToolsEditorModeToolkit* Toolkit, FMenuBuilder& MenuBuilder)
{
	UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(Toolkit->GetScriptableEditorMode());
	FUIAction ToggleVolumesAction(
		FExecuteAction::CreateLambda([ModelingMode]
		{
			ModelingMode->bEnableVolumeElementSelection = ! ModelingMode->bEnableVolumeElementSelection;

			UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();
			ModelingEditorSettings->bLastMeshSelectionVolumeToggle = ModelingMode->bEnableVolumeElementSelection;
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([ModelingMode]()
		{
			return ModelingMode->bEnableVolumeElementSelection;
		}));
	FUIAction ToggleStaticMeshesAction(
		FExecuteAction::CreateLambda([ModelingMode]
		{
			ModelingMode->bEnableStaticMeshElementSelection = ! ModelingMode->bEnableStaticMeshElementSelection;

			UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();
			ModelingEditorSettings->bLastMeshSelectionStaticMeshToggle = ModelingMode->bEnableStaticMeshElementSelection;
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([ModelingMode]()
		{
			return ModelingMode->bEnableStaticMeshElementSelection;
		}));


	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_MeshTypes_Volumes", "Volumes"), 
		LOCTEXT("Selection_MeshTypes_Volumes_Tooltip", "Toggle whether Volume mesh elements can be selected in the Viewport"),
		FSlateIcon(), ToggleVolumesAction, NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_MeshTypes_StaticMesh", "Static Meshes"), 
		LOCTEXT("Selection_MeshTypes_StaticMesh_Tooltip", "Toggle whether Static Mesh mesh elements can be selected in the Viewport"),
		FSlateIcon(), ToggleStaticMeshesAction, NAME_None, EUserInterfaceActionType::ToggleButton);

}
	
void MakeSubMenu_Selection_LocalFrameMode(FModelingToolsEditorModeToolkit* Toolkit, FMenuBuilder& MenuBuilder)
{
		
	UModelingSelectionInteraction* SelectionInteraction = Cast<UModelingToolsEditorMode>(Toolkit->GetScriptableEditorMode())->GetSelectionInteraction();
	auto ToggleFromGeometryAction = [SelectionInteraction](EModelingSelectionInteraction_LocalFrameMode LocalFrameMode)
	{
		return FUIAction(
			FExecuteAction::CreateLambda([SelectionInteraction, LocalFrameMode]
			{
				SelectionInteraction->SetLocalFrameMode(LocalFrameMode);
				UModelingToolsModeCustomizationSettings* ModelingEditorSettings = GetMutableDefault<UModelingToolsModeCustomizationSettings>();
				ModelingEditorSettings->LastMeshSelectionLocalFrameMode = static_cast<int>(LocalFrameMode);
				ModelingEditorSettings->SaveConfig();
			}),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([SelectionInteraction, LocalFrameMode]()
			{
				return (SelectionInteraction->GetLocalFrameMode() == LocalFrameMode);
			}));
	};
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_LocalFrameMode_FromGeometry", "From Geometry"), 
		LOCTEXT("Selection_LocalFrameMode_FromGeometry_Tooltip", "Gizmo Orientation Based on Selected Geometry"),
		FSlateIcon(), ToggleFromGeometryAction(EModelingSelectionInteraction_LocalFrameMode::FromGeometry), NAME_None, EUserInterfaceActionType::ToggleButton);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Selection_LocalFrameMode_FromObject", "From Object"), 
		LOCTEXT("Selection_LocalFrameMode_FromObject_Tooltip", "Gizmo Orientation Based on Object"),
		FSlateIcon(), ToggleFromGeometryAction(EModelingSelectionInteraction_LocalFrameMode::FromObject), NAME_None, EUserInterfaceActionType::ToggleButton);
}

TSharedRef<SWidget> MakeMenu_SelectionConfigSettings(FModelingToolsEditorModeToolkit* Toolkit)
{
	FMenuBuilder MenuBuilder(true, TSharedPtr<FUICommandList>());

	MenuBuilder.BeginSection("Section_DragMode", LOCTEXT("Section_DragMode", "Drag Mode"));
	MakeSubMenu_Selection_DragMode(Toolkit, MenuBuilder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Section_MeshTypes", LOCTEXT("Section_MeshTypes", "Selectable Mesh Types"));
	MakeSubMenu_Selection_MeshType(Toolkit, MenuBuilder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Section_LocalFrameMode", LOCTEXT("Section_LocalFrameMode", "Local Frame Mode"));
	MakeSubMenu_Selection_LocalFrameMode(Toolkit, MenuBuilder);
	MenuBuilder.EndSection();

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


TSharedRef<SWidget> MakeMenu_SelectionEdits(FModelingToolsEditorModeToolkit* Toolkit)
{
	FMenuBuilder MenuBuilder(true, Toolkit->GetToolkitCommands());

	MenuBuilder.BeginSection("Section_SelectionEdits", LOCTEXT("Section_SelectionEdits", "Selection Edits"));

	const FModelingToolsManagerCommands& Commands = FModelingToolsManagerCommands::Get();

	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_SelectAll);
	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_ExpandToConnected);
	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_Invert);
	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_InvertConnected);
	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_Expand);
	MenuBuilder.AddMenuEntry(Commands.BeginSelectionAction_Contract);

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}


} // end namespace UELocal


TSharedRef<SWidget> FModelingToolsEditorModeToolkit::MakeMenu_ModelingModeConfigSettings()
{
	using namespace UELocal;

	FMenuBuilder MenuBuilder(true, TSharedPtr<FUICommandList>());

	MenuBuilder.BeginSection("Section_Gizmo", LOCTEXT("Section_Gizmo", "Gizmo"));
	MakeSubMenu_GizmoVisibilityMode(this, MenuBuilder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Section_SelectionSupport", LOCTEXT("Section_SelectionSupport", "Mesh Element Selection"));
	MakeSubMenu_SelectionSupport(this, MenuBuilder);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Section_MeshObjects", LOCTEXT("Section_MeshObjects", "New Mesh Objects"));
	MenuBuilder.AddSubMenu(
		LOCTEXT("MeshObjectTypeSubMenu", "New Mesh Settings"), LOCTEXT("MeshObjectTypeSubMenu_ToolTip", "Configure default settings for new Mesh Object Types"),
		FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
			MakeSubMenu_DefaultMeshObjectType(SubMenuBuilder);
		}));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Section_ModeToggles", LOCTEXT("Section_ModeToggles", "Mode Settings"));
	MakeSubMenu_ModeToggles(MenuBuilder);
	MenuBuilder.EndSection();

	// only show settings UI quick access in non-Restrictive mode
	const UModelingToolsEditorModeSettings* Settings = GetDefault<UModelingToolsEditorModeSettings>();
	if (!Settings->InRestrictiveMode())
	{
		MenuBuilder.BeginSection("Section_Settings", LOCTEXT("Section_Settings", "Quick Settings"));
		MenuBuilder.AddSubMenu(
			LOCTEXT("QuickSettingsSubMenu", "Jump To Settings"), LOCTEXT("QuickSettingsSubMenu_ToolTip", "Jump to sections of the Settings dialogs relevant to Modeling Mode"),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder) {
				MakeSubMenu_QuickSettings(SubMenuBuilder);
				}));
		MenuBuilder.EndSection();
	}

	TSharedRef<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

void FModelingToolsEditorModeToolkit::ExtendSecondaryModeToolbar(UToolMenu *InModeToolbarMenu)
{
	return;		// disable for now

	/*
	 * Sanity check because the toolbar extension should happen after the ModeUILayer setup which is currently manual
	 * for standalone asset editors
	 */
	if( !ModeUILayer.IsValid() )
	{
		return;
	}

	UModelingToolsEditorModeSettings* ModelingModeSettings = GetMutableDefault<UModelingToolsEditorModeSettings>();
	const bool bEnableSelectionUI = ModelingModeSettings && ModelingModeSettings->GetMeshSelectionsEnabled();
	if ( !bEnableSelectionUI )
	{
		return;
	}
	
	FToolMenuSection& Section = InModeToolbarMenu->FindOrAddSection("SelectionPalette");

	/*
	 * NOTE: Any commands used in the SecondaryModeToolbar need to be present in the command list that lives in the
	 * ModeUILayer. Currently this is done automatically by parenting said command list to the toolkit's command list
	 */
	const FModelingToolsManagerCommands& Commands = FModelingToolsManagerCommands::Get();

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_NoSelection));

	Section.AddSeparator(NAME_None);

	TSharedRef<SWidget> TrianglesTextWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TriangleSelectionText", "Triangles"))
		];

	Section.AddEntry(FToolMenuEntry::InitWidget("TrianglesTextWidget", TrianglesTextWidget, FText()));
	
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_MeshTriangles));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_MeshEdges));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_MeshVertices));

	Section.AddSeparator(NAME_None);

	TSharedRef<SWidget> PolyGroupsTextWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PolyGroupsSelectionText", "PolyGroups"))
		];

	Section.AddEntry(FToolMenuEntry::InitWidget("PolyGroupsTextWidget", PolyGroupsTextWidget, FText()));
	
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_GroupFaces));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_GroupEdges));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.MeshSelectionModeAction_GroupCorners));

	Section.AddSeparator(NAME_None);

	TSharedRef<SWidget> SelectionEditTextWidget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SelectionEditText", "Selection Edit"))
		];

	Section.AddEntry(FToolMenuEntry::InitWidget("SelectionEditTextWidget", SelectionEditTextWidget, FText()));

	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_SelectAll));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_Invert));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_ExpandToConnected));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_InvertConnected));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_Expand));
	Section.AddEntry(FToolMenuEntry::InitToolBarButton(Commands.BeginSelectionAction_Contract));
	
	Section.AddSeparator(NAME_None);

	TSharedRef<SWidget> DefaultSettingsWidget = SNew(SComboButton)
		.HasDownArrow(false)
		.MenuPlacement(EMenuPlacement::MenuPlacement_MenuRight)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
		{
			return UELocal::MakeMenu_SelectionConfigSettings(this);
		}))
		.ContentPadding(FMargin(3.0f, 1.0f))
		.ButtonContent()
		[
			SNew(SImage)
			.Image(FModelingToolsEditorModeStyle::Get()->GetBrush("ModelingMode.DefaultSettings"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];

	Section.AddEntry(FToolMenuEntry::InitWidget("DefaultSettings", DefaultSettingsWidget, FText()));
	
	Section.AddSeparator(NAME_None);

}




void FModelingToolsEditorModeToolkit::MakeSelectionPaletteOverlayWidget()
{
	FVerticalToolBarBuilder ToolbarBuilder(
		GetToolkitCommands(),
		FMultiBoxCustomization::None,
		TSharedPtr<FExtender>(), /*InForceSmallIcons=*/ true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetStyle(FModelingToolsEditorModeStyle::Get().Get(), "SelectionToolBar");

	const FModelingToolsManagerCommands& Commands = FModelingToolsManagerCommands::Get();

	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_NoSelection);
	ToolbarBuilder.AddWidget(SNew(SSpacer).Size(FVector2D(1, 1)));
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_MeshTriangles);
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_MeshEdges);
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_MeshVertices);
	ToolbarBuilder.AddWidget(SNew(SSpacer).Size(FVector2D(1, 1)));
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_GroupFaces);
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_GroupEdges);
	ToolbarBuilder.AddToolBarButton(Commands.MeshSelectionModeAction_GroupCorners);
	ToolbarBuilder.AddWidget(SNew(SSpacer).Size(FVector2D(1, 8)));


	ToolbarBuilder.AddWidget(
		SNew(SComboButton)
		.HasDownArrow(false)
		.MenuPlacement(EMenuPlacement::MenuPlacement_MenuRight)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.IsEnabled_Lambda([this]() { return IsInActiveTool() == false; })
		.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
			{
				return UELocal::MakeMenu_SelectionEdits(this);
			}))
		.ContentPadding(FMargin(1.0f, 1.0f))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FModelingToolsEditorModeStyle::Get()->GetBrush("ModelingModeSelection.Edits_Right"))
				]);

	ToolbarBuilder.AddWidget(
		SNew(SComboButton)
		.HasDownArrow(false)
		.MenuPlacement(EMenuPlacement::MenuPlacement_MenuRight)
		.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
		.IsEnabled_Lambda([this]() { return IsInActiveTool() == false; })
		.OnGetMenuContent(FOnGetContent::CreateLambda([this]()
			{
				return UELocal::MakeMenu_SelectionConfigSettings(this);
			}))
		.ContentPadding(FMargin(1.0f, 1.0f))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FModelingToolsEditorModeStyle::Get()->GetBrush("ModelingMode.DefaultSettings"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]);

	ToolbarBuilder.AddWidget(SNew(SSpacer).Size(FVector2D(1, 16)));

	//
	// locking toggle button is implemented using 3 separate buttons because we cannot 
	// control enabled/disabled or background color for a toolbar button
	//

	FUIAction UnlockTargetAction(
		FExecuteAction::CreateLambda([this] { GetMeshSelectionManager()->SetCurrentTargetsLockState(false); }),
		FCanExecuteAction::CreateLambda([this] { return GetMeshSelectionManager()->GetAnyCurrentTargetsLocked() && IsInActiveTool() == false; }),
		FIsActionChecked::CreateLambda([] { return false; }),
		FIsActionButtonVisible::CreateLambda([this] {
			return GetMeshSelectionManager()->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None
			&& GetMeshSelectionManager()->GetAnyCurrentTargetsLockable()
			&& GetMeshSelectionManager()->GetAnyCurrentTargetsLocked();
			})
	);
	FUIAction LockTargetAction(
		FExecuteAction::CreateLambda([this] { GetMeshSelectionManager()->SetCurrentTargetsLockState(true); }),
		FCanExecuteAction::CreateLambda([this] { return !GetMeshSelectionManager()->GetAnyCurrentTargetsLocked() && IsInActiveTool() == false; }),
		FIsActionChecked::CreateLambda([]() { return false; }),
		FIsActionButtonVisible::CreateLambda([this] {
			return GetMeshSelectionManager()->GetMeshTopologyMode() != UGeometrySelectionManager::EMeshTopologyMode::None
			&& GetMeshSelectionManager()->GetAnyCurrentTargetsLockable()
			&& (GetMeshSelectionManager()->GetAnyCurrentTargetsLocked() == false);
			})
	);
	FUIAction DisabledLockTargetAction(
		FExecuteAction::CreateLambda([] {}),
		FCanExecuteAction::CreateLambda([] { return false; }),
		FIsActionChecked::CreateLambda([]() { return false; }),
		FIsActionButtonVisible::CreateLambda([this] {
			return GetMeshSelectionManager()->GetMeshTopologyMode() == UGeometrySelectionManager::EMeshTopologyMode::None
			|| GetMeshSelectionManager()->GetAnyCurrentTargetsLockable() == false;
			})
	);

	ToolbarBuilder.BeginStyleOverride(FName("SelectionToolBar.RedButton"));
	ToolbarBuilder.AddToolBarButton(UnlockTargetAction, NAME_None,
		LOCTEXT("Selection_UnlockTarget", "Unlock"),
		LOCTEXT("Selection_UnlockTarget_Tooltip", "Click to Unlock the Selected Object and allow Mesh Selections"),
		FSlateIcon(FModelingToolsEditorModeStyle::GetStyleSetName(), "SelectionToolBarIcons.LockedTarget"));
	ToolbarBuilder.EndStyleOverride();
	ToolbarBuilder.BeginStyleOverride(FName("SelectionToolBar.GreenButton"));
	ToolbarBuilder.AddToolBarButton(LockTargetAction, NAME_None,
		LOCTEXT("Selection_LockTarget", "Lock"),
		LOCTEXT("Selection_LockTarget_Tooltip", "Click to Lock the Selected Object"),
		FSlateIcon(FModelingToolsEditorModeStyle::GetStyleSetName(), "SelectionToolBarIcons.UnlockedTarget"));
	ToolbarBuilder.EndStyleOverride();
	ToolbarBuilder.AddToolBarButton(DisabledLockTargetAction, NAME_None,
		LOCTEXT("Selection_LockTargetDisabled", "(No Locking)"),
		LOCTEXT("Selection_LockTargetDisabled_Tooltip", "No Active Selection Targets are Lockable"),
		FSlateIcon(FModelingToolsEditorModeStyle::GetStyleSetName(), "SelectionToolBarIcons.UnlockedTarget"));


	TSharedRef<SWidget> Toolbar = ToolbarBuilder.MakeWidget();

	SAssignNew(SelectionPaletteOverlayWidget, SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2.0f, 0.0f, 0.f, 0.f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewport.OverlayBrush"))
			.Padding(FMargin(3.0f, 6.0f, 3.f, 6.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0, 0.f, 0.f, 0.f))
				[
					Toolbar
				]
			]
		];

	SelectionPaletteOverlayWidget->SetVisibility( TAttribute<EVisibility>::CreateLambda([this]()
	{
		if ( UModelingToolsEditorMode* ModelingMode = Cast<UModelingToolsEditorMode>(GetScriptableEditorMode()) )
		{
			return ModelingMode->GetMeshElementSelectionSystemEnabled() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	}) );


}




#undef LOCTEXT_NAMESPACE
