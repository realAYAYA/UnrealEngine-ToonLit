// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetPlacementEdMode.h"
#include "AssetPlacementEdModeToolkit.h"
#include "InteractiveToolManager.h"
#include "AssetPlacementEdModeStyle.h"
#include "AssetPlacementEdModeCommands.h"
#include "AssetPlacementSettings.h"
#include "EdModeInteractiveToolsContext.h"
#include "EditorModeManager.h"
#include "Selection.h"
#include "EngineUtils.h"
#include "AssetPlacementEdModeModule.h"

#include "Tools/AssetEditorContextInterface.h"
#include "Tools/PlacementSelectTool.h"
#include "Tools/PlacementLassoSelectTool.h"
#include "Tools/PlacementPlaceTool.h"
#include "Tools/PlacementPlaceSingleTool.h"
#include "Tools/PlacementEraseTool.h"

#include "Factories/AssetFactoryInterface.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementCommonActions.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/SMInstance/SMInstanceElementData.h" // For SMInstanceElementDataUtil::SMInstanceElementsEnabled

#include "Settings/LevelEditorMiscSettings.h"
#include "Modes/PlacementModeSubsystem.h"
#include "AssetPlacementEdModeModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetPlacementEdMode)

#define LOCTEXT_NAMESPACE "AssetPlacementEdMode"

constexpr TCHAR UAssetPlacementEdMode::AssetPlacementEdModeID[];

UAssetPlacementEdMode::UAssetPlacementEdMode()
{
	TAttribute<bool> IsEnabledAttr = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&UAssetPlacementEdMode::IsEnabled));
	Info = FEditorModeInfo(UAssetPlacementEdMode::AssetPlacementEdModeID,
		LOCTEXT("AssetPlacementEdModeName", "Placement"),
		FSlateIcon(FAssetPlacementEdModeStyle::Get().GetStyleSetName(), "LevelEditor.AssetPlacementEdMode"),
		MoveTemp(IsEnabledAttr));
}

UAssetPlacementEdMode::~UAssetPlacementEdMode()
{
}

void UAssetPlacementEdMode::Enter()
{
	Super::Enter();
	if (UPlacementModeSubsystem* PlacementModeSubsystem = GEditor->GetEditorSubsystem<UPlacementModeSubsystem>())
	{
		SettingsObjectAsPlacementSettings = PlacementModeSubsystem->GetMutableModeSettingsObject();
		SettingsObjectAsPlacementSettings->LoadSettings();
		Toolkit->SetModeSettingsObject(SettingsObjectAsPlacementSettings.Get());
	}

	const FAssetPlacementEdModeCommands& PlacementModeCommands = FAssetPlacementEdModeCommands::Get();
	RegisterTool(PlacementModeCommands.Select, UPlacementModeSelectTool::ToolName, NewObject<UPlacementModeSelectToolBuilder>(this));
	RegisterTool(PlacementModeCommands.PlaceSingle, UPlacementModePlaceSingleTool::ToolName, NewObject<UPlacementModePlaceSingleToolBuilder>(this));

#if !UE_IS_COOKED_EDITOR
	if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
	{
		RegisterTool(PlacementModeCommands.Place, UPlacementModePlacementTool::ToolName, NewObject<UPlacementModePlacementToolBuilder>(this));
		RegisterTool(PlacementModeCommands.LassoSelect, UPlacementModeLassoSelectTool::ToolName, NewObject<UPlacementModeLassoSelectToolBuilder>(this));
		RegisterTool(PlacementModeCommands.Erase, UPlacementModeEraseTool::ToolName, NewObject<UPlacementModeEraseToolBuilder>(this));
	}
#endif // !UE_IS_COOKED_EDITOR

	// Stash the current editor selection, since this mode will modify it.
	Owner->StoreSelection(AssetPlacementEdModeID);
	bIsInSelectionTool = false;

	// Disable undo tracking so that we can't accidentally undo ourselves out of the select mode and into an invalid state.
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);

	// Enable the select tool by default.
	GetInteractiveToolsContext()->StartTool(UPlacementModeSelectTool::ToolName);

#if !UE_IS_COOKED_EDITOR
	if (AssetPlacementEdModeUtil::AreInstanceWorkflowsEnabled())
	{
		SMInstanceElementDataUtil::OnSMInstanceElementsEnabledChanged().AddUObject(this, &UAssetPlacementEdMode::OnSMIsntancedElementsEnabledChanged);
	}
#endif // !UE_IS_COOKED_EDITOR
}

void UAssetPlacementEdMode::Exit()
{
	SMInstanceElementDataUtil::OnSMInstanceElementsEnabledChanged().RemoveAll(this);

	Super::Exit();

	SettingsObjectAsPlacementSettings->SaveSettings();
	SettingsObjectAsPlacementSettings.Reset();

	// Restore the selection to the original state after all the tools have shutdown in UEdMode::Exit()
	// Since they can continue messing with selection states.
	Owner->RestoreSelection(AssetPlacementEdModeID);
}

void UAssetPlacementEdMode::CreateToolkit()
{
	Toolkit = MakeShared<FAssetPlacementEdModeToolkit>();
}

bool UAssetPlacementEdMode::UsesToolkits() const
{
	return true;
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UAssetPlacementEdMode::GetModeCommands() const
{
	return FAssetPlacementEdModeCommands::Get().GetCommands();
}

void UAssetPlacementEdMode::BindCommands()
{
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
	const FAssetPlacementEdModeCommands& PlacementModeCommands = FAssetPlacementEdModeCommands::Get();
}

bool UAssetPlacementEdMode::IsSelectionAllowed(AActor* InActor, bool bInSelection) const
{
	// Always allow deselection, for stashing selection set.
	if (!bInSelection)
	{
		return true;
	}

	// Otherwise, need to be in selection tool for selection to be allowed
	return bIsInSelectionTool;
}

void UAssetPlacementEdMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	Super::OnToolStarted(Manager, Tool);

	bool bWasInSelectionTool = bIsInSelectionTool;
	FString ActiveToolName = GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	bool bIsSinglePlaceTool = ActiveToolName == UPlacementModePlaceSingleTool::ToolName;
	if (ActiveToolName == UPlacementModeSelectTool::ToolName || ActiveToolName == UPlacementModeLassoSelectTool::ToolName || bIsSinglePlaceTool)
	{
		bIsInSelectionTool = true;
	}
	else
	{
		bIsInSelectionTool = false;
	}

	// Restore the selection if we're going into the selection tools.
	// Allow the selection to be empty if we're going into single place tool for a clean slate.
	bool bRestoreSelectionState = bIsInSelectionTool && !bWasInSelectionTool && !bIsSinglePlaceTool;
	if (bRestoreSelectionState)
	{
		Owner->RestoreSelection(UPlacementModeSelectTool::ToolName);
	}
	else if (!bIsInSelectionTool)
	{
		// If we can't select, clear out the selection set for the active tool.
		ClearSelection();
	}
}

void UAssetPlacementEdMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	Super::OnToolEnded(Manager, Tool);

	// Always store the most recent selection, even if we are leaving single placement tool to preserve what the user was doing last.
	if (bIsInSelectionTool)
	{
		constexpr bool bClearSelection = false;
		Owner->StoreSelection(UPlacementModeSelectTool::ToolName, false);
	}
}

bool UAssetPlacementEdMode::UsesPropertyWidgets() const
{
	return IsInSelectionTool();
}

bool UAssetPlacementEdMode::ShouldDrawWidget() const
{
	if (!IsInSelectionTool())
	{
		return false;
	}

	// Disable the widget in the lasso tool. The drag operations currently fight with the widget.
	if (GetToolManager()->GetActiveToolName(EToolSide::Mouse) == UPlacementModeLassoSelectTool::ToolName)
	{
		return false;
	}

	return Super::ShouldDrawWidget();
}

bool UAssetPlacementEdMode::IsEnabled()
{
	return SMInstanceElementDataUtil::SMInstanceElementsEnabled();
}

void UAssetPlacementEdMode::ClearSelection()
{
	GetToolManager()->BeginUndoTransaction(LOCTEXT("PlacementClearSelection", "Clear Selection"));
	if (UTypedElementSelectionSet* SelectionSet = Owner->GetEditorSelectionSet())
	{
		SelectionSet->ClearSelection(FTypedElementSelectionOptions());
	}
	GetToolManager()->EndUndoTransaction();
}

bool UAssetPlacementEdMode::HasAnyAssetsInPalette() const
{
	return SettingsObjectAsPlacementSettings.IsValid() ? (SettingsObjectAsPlacementSettings->GetActivePaletteItems().Num() > 0) : false;
}

bool UAssetPlacementEdMode::HasActiveSelection() const
{
	if (!HasAnyAssetsInPalette())
	{
		return false;
	}

	if (Owner->GetEditorSelectionSet()->HasSelectedElements())
	{
		return true;
	}

	return false;
}

bool UAssetPlacementEdMode::IsInSelectionTool() const
{
	return bIsInSelectionTool;
}

void UAssetPlacementEdMode::OnSMIsntancedElementsEnabledChanged()
{
	// Disable this mode if the SM instance element cvar changed
	Owner->DeactivateMode(UAssetPlacementEdMode::AssetPlacementEdModeID);
}

#undef LOCTEXT_NAMESPACE

