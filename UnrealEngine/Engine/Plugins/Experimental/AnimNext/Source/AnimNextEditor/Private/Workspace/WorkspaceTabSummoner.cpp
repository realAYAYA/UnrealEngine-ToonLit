// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorkspaceTabSummoner.h"
#include "AnimNextWorkspaceEditor.h"
#include "SWorkspaceView.h"

#define LOCTEXT_NAMESPACE "WorkspaceTabSummoner"

namespace UE::AnimNext::Editor
{

FWorkspaceTabSummoner::FWorkspaceTabSummoner(TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FWorkflowTabFactory(WorkspaceTabs::WorkspaceView, InHostingApp)
{
	HostingAppPtr = InHostingApp;

	TabLabel = LOCTEXT("WorkspaceTabLabel", "Workspace");
	TabIcon = FSlateIcon("EditorStyle", "LevelEditor.Tabs.Outliner");
	ViewMenuDescription = LOCTEXT("WorkspaceTabMenuDescription", "Workspace");
	ViewMenuTooltip = LOCTEXT("WorkspaceTabToolTip", "Shows the workspace outliner tab.");
	bIsSingleton = true;

	WorkspaceView = SNew(SWorkspaceView, InHostingApp->Workspace)
		.OnAssetsOpened_Lambda([this](TConstArrayView<FAssetData> InAssets)
		{
			if(TSharedPtr<FWorkspaceEditor> HostingApp = HostingAppPtr.Pin())
			{
				HostingApp->OpenAssets(InAssets);
			}
		});
}

TSharedRef<SWidget> FWorkspaceTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return WorkspaceView.ToSharedRef();
}

FText FWorkspaceTabSummoner::GetTabToolTipText(const FWorkflowTabSpawnInfo& Info) const
{
	return ViewMenuTooltip;
}

}

#undef LOCTEXT_NAMESPACE