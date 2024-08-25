// Copyright Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetEditorToolkit.h"

#include "PersonaModule.h"
#include "AssetEditorModeManager.h"
#include "Modules/ModuleManager.h"
#include "IPersonaEditorModeManager.h"
#include "PersonaTabs.h"

void FPersonaAssetEditorToolkit::CreateEditorModeManager()
{
	EditorModeManager = MakeShareable(FModuleManager::LoadModuleChecked<FPersonaModule>("Persona").CreatePersonaEditorModeManager());

	// Make sure we get told when the editor mode changes so we can switch to the appropriate tab
	// if there's a toolbox available.
	GetEditorModeManager().OnEditorModeIDChanged().AddSP(this, &FPersonaAssetEditorToolkit::OnEditorModeIdChanged);
}

void FPersonaAssetEditorToolkit::OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit)
{
	if (ensure(!HostedToolkit.IsValid()))
	{
		HostedToolkit = Toolkit;
		OnAttachToolkit.Broadcast(Toolkit);
	}
}

void FPersonaAssetEditorToolkit::OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit)
{
	if (ensure(Toolkit == HostedToolkit))
	{
		HostedToolkit.Reset();
		OnDetachToolkit.Broadcast(Toolkit);
	}
}

void FPersonaAssetEditorToolkit::OnEditorModeIdChanged(const FEditorModeID& ModeChangedID, bool bIsEnteringMode)
{
	if (GetEditorModeManager().IsDefaultMode(ModeChangedID))
	{
		return;
	}

	if (bIsEnteringMode)
	{
		if (HostedToolkit.IsValid())
		{
			TabManager->TryInvokeTab(FPersonaTabs::ToolboxID);
		}
	}
	else
	{
		const TSharedPtr<SDockTab> ToolboxTab = TabManager->FindExistingLiveTab(FPersonaTabs::ToolboxID);
		if (ToolboxTab.IsValid())
		{
			ToolboxTab->RequestCloseTab();
		}
	}
}
