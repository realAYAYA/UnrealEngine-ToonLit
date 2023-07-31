// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigHierarchyTabSummoner.h"
#include "Editor/SRigHierarchy.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "Editor/SRigHierarchy.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "RigHierarchyTabSummoner"

const FName FRigHierarchyTabSummoner::TabID(TEXT("RigHierarchy"));

FRigHierarchyTabSummoner::FRigHierarchyTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("RigHierarchyTabLabel", "Rig Hierarchy");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "RigHierarchy.TabIcon");

	ViewMenuDescription = LOCTEXT("RigHierarchy_ViewMenu_Desc", "Rig Hierarchy");
	ViewMenuTooltip = LOCTEXT("RigHierarchy_ViewMenu_ToolTip", "Show the Rig Hierarchy tab");
}

FTabSpawnerEntry& FRigHierarchyTabSummoner::RegisterTabSpawner(TSharedRef<FTabManager> InTabManager, const FApplicationMode* CurrentApplicationMode) const
{
	FTabSpawnerEntry& SpawnerEntry = FWorkflowTabFactory::RegisterTabSpawner(InTabManager, CurrentApplicationMode);

	SpawnerEntry.SetReuseTabMethod(FOnFindTabToReuse::CreateLambda([](const FTabId& InTabId) ->TSharedPtr<SDockTab> {
	
		return TSharedPtr<SDockTab>();

	}));

	return SpawnerEntry;
}

TSharedRef<SWidget> FRigHierarchyTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	ControlRigEditor.Pin()->RigHierarchyTabCount++;
	return SNew(SRigHierarchy, ControlRigEditor.Pin().ToSharedRef());
}

TSharedRef<SDockTab> FRigHierarchyTabSummoner::SpawnTab(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SDockTab>  DockTab = FWorkflowTabFactory::SpawnTab(Info);
	DockTab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([DockTab]()
    {
		int32 HierarchyTabCount = 0;
		if(SWidget* Content = &DockTab->GetContent().Get())
		{
			SRigHierarchy* RigHierarchy = (SRigHierarchy*)Content;
			if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
			{
				HierarchyTabCount = ControlRigEditorForTab->GetRigHierarchyTabCount();
			}
		}
        return HierarchyTabCount > 1;
    }));
	DockTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab> DockTab)
	{
		if(SWidget* Content = &DockTab->GetContent().Get())
		{
			SRigHierarchy* RigHierarchy = (SRigHierarchy*)Content;
			if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
			{
				ControlRigEditorForTab->RigHierarchyTabCount--;
			}
		}
	}));
	return DockTab;
}

#undef LOCTEXT_NAMESPACE 
