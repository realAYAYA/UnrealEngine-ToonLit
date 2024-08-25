// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDConstraintDataInspectorTab.h"

#include "ChaosVDStyle.h"
#include "Widgets/SChaosVDConstraintDataInspector.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDConstraintDataInspectorTab::~FChaosVDConstraintDataInspectorTab()
{
}

TSharedRef<SDockTab> FChaosVDConstraintDataInspectorTab::HandleTabSpawnRequest(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DetailsPanelTab =
	SNew(SDockTab)
	.TabRole(ETabRole::MajorTab)
	.Label(LOCTEXT("ConstraintDataInspectorTab", "Constraint Inspector"))
	.ToolTipText(LOCTEXT("ConstraintDataInspectorTabToolTip", "See the details of the any non-collision constraint selected in the viewport"));

	if (const TSharedPtr<SChaosVDMainTab> MainTabPtr = OwningTabWidget.Pin())
	{
		DetailsPanelTab->SetContent
		(
			SAssignNew(ConstraintDataInspector, SChaosVDConstraintDataInspector, GetChaosVDScene())
		);
	}
	else
	{
		DetailsPanelTab->SetContent(GenerateErrorWidget());
	}

	DetailsPanelTab->SetTabIcon(FChaosVDStyle::Get().GetBrush("ConnectionIcon"));

	HandleTabSpawned(DetailsPanelTab);

	return DetailsPanelTab;
}

void FChaosVDConstraintDataInspectorTab::HandleTabClosed(TSharedRef<SDockTab> InTabClosed)
{
	FChaosVDTabSpawnerBase::HandleTabClosed(InTabClosed);

	ConstraintDataInspector.Reset();
}

#undef LOCTEXT_NAMESPACE
