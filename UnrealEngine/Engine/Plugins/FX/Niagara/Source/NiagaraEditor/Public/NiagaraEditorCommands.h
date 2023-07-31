// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

/**
* Defines commands for the niagara editor.
*/
class NIAGARAEDITOR_API FNiagaraEditorCommands : public TCommands<FNiagaraEditorCommands>
{
public:
	FNiagaraEditorCommands()
		: TCommands<FNiagaraEditorCommands>
		(
			TEXT("NiagaraEditor"),
			NSLOCTEXT("Contexts", "NiagaraEditor", "Niagara Editor"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
		)
	{ }

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Apply;
	TSharedPtr<FUICommandInfo> ApplyScratchPadChanges;
	TSharedPtr<FUICommandInfo> Discard;
	TSharedPtr<FUICommandInfo> Compile;
	TSharedPtr<FUICommandInfo> RefreshNodes;
	TSharedPtr<FUICommandInfo> ModuleVersioning;
	TSharedPtr<FUICommandInfo> EmitterVersioning;
	TSharedPtr<FUICommandInfo> ResetSimulation;
	TSharedPtr<FUICommandInfo> SelectNextUsage;
	TSharedPtr<FUICommandInfo> CreateAssetFromSelection;

	TSharedPtr<FUICommandInfo> OpenAddEmitterMenu;
	
	/** Toggles the preview pane's grid */
	TSharedPtr<FUICommandInfo> TogglePreviewGrid;
	TSharedPtr<FUICommandInfo> ToggleInstructionCounts;
	TSharedPtr<FUICommandInfo> ToggleParticleCounts;
	TSharedPtr<FUICommandInfo> ToggleEmitterExecutionOrder;
	TSharedPtr<FUICommandInfo> ToggleGpuTickInformation;

	/** Toggles the preview pane's background */
	TSharedPtr< FUICommandInfo > TogglePreviewBackground;

	/** Toggles the locking/unlocking of refreshing from changes*/
	TSharedPtr<FUICommandInfo> ToggleUnlockToChanges;

	TSharedPtr<FUICommandInfo> ToggleOrbit;
	TSharedPtr<FUICommandInfo> ToggleBounds;
	TSharedPtr<FUICommandInfo> ToggleBounds_SetFixedBounds_SelectedEmitters;
	TSharedPtr<FUICommandInfo> ToggleBounds_SetFixedBounds_System;
	TSharedPtr<FUICommandInfo> SaveThumbnailImage;

	TSharedPtr<FUICommandInfo> ToggleStatPerformance;
	TSharedPtr<FUICommandInfo> ToggleStatPerformanceGPU;
	TSharedPtr<FUICommandInfo> ClearStatPerformance;
	TSharedPtr<FUICommandInfo> ToggleStatPerformanceTypeAvg;
	TSharedPtr<FUICommandInfo> ToggleStatPerformanceTypeMax;
	TSharedPtr<FUICommandInfo> ToggleStatPerformanceModePercent;
	TSharedPtr<FUICommandInfo> ToggleStatPerformanceModeAbsolute;

	TSharedPtr<FUICommandInfo> OpenDebugHUD;
	TSharedPtr<FUICommandInfo> OpenDebugOutliner;
	TSharedPtr<FUICommandInfo> OpenAttributeSpreadsheet;

	TSharedPtr<FUICommandInfo> ToggleAutoPlay;
	TSharedPtr<FUICommandInfo> ToggleResetSimulationOnChange;
	TSharedPtr<FUICommandInfo> ToggleResimulateOnChangeWhilePaused;
	TSharedPtr<FUICommandInfo> ToggleResetDependentSystems;

	TSharedPtr<FUICommandInfo> IsolateSelectedEmitters;
	TSharedPtr<FUICommandInfo> DisableSelectedEmitters;
	TSharedPtr<FUICommandInfo> HideDisabledModules;

	TSharedPtr<FUICommandInfo> CollapseStackToHeaders;

	TSharedPtr<FUICommandInfo> FindInCurrentView;

	TSharedPtr<FUICommandInfo> ZoomToFit;
	TSharedPtr<FUICommandInfo> ZoomToFitAll;
};