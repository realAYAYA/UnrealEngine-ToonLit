// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ModularRigModelTabSummoner.h"
#include "Editor/SModularRigModel.h"
#include "ControlRigEditorStyle.h"
#include "Editor/ControlRigEditor.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ModularRigHierarchyTabSummoner"

const FName FModularRigModelTabSummoner::TabID(TEXT("ModularRigModel"));

FModularRigModelTabSummoner::FModularRigModelTabSummoner(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FWorkflowTabFactory(TabID, InControlRigEditor)
	, ControlRigEditor(InControlRigEditor)
{
	TabLabel = LOCTEXT("ModularRigHierarchyTabLabel", "Module Hierarchy");
	TabIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ModularRigHierarchy.TabIcon");

	ViewMenuDescription = LOCTEXT("ModularRigHierarchy_ViewMenu_Desc", "Module Hierarchy");
	ViewMenuTooltip = LOCTEXT("ModularRigHierarchy_ViewMenu_ToolTip", "Show the Module Hierarchy tab");
}

FTabSpawnerEntry& FModularRigModelTabSummoner::RegisterTabSpawner(TSharedRef<FTabManager> InTabManager, const FApplicationMode* CurrentApplicationMode) const
{
	FTabSpawnerEntry& SpawnerEntry = FWorkflowTabFactory::RegisterTabSpawner(InTabManager, CurrentApplicationMode);

	SpawnerEntry.SetReuseTabMethod(FOnFindTabToReuse::CreateLambda([](const FTabId& InTabId) ->TSharedPtr<SDockTab> {
	
		return TSharedPtr<SDockTab>();

	}));

	return SpawnerEntry;
}

TSharedRef<SWidget> FModularRigModelTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	ControlRigEditor.Pin()->ModularRigHierarchyTabCount++;
	return SNew(SModularRigModel, ControlRigEditor.Pin().ToSharedRef());
}

TSharedRef<SDockTab> FModularRigModelTabSummoner::SpawnTab(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SDockTab>  DockTab = FWorkflowTabFactory::SpawnTab(Info);
	TWeakPtr<SDockTab> WeakDockTab = DockTab;
	DockTab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([WeakDockTab]()
    {
		int32 HierarchyTabCount = 0;
		if (TSharedPtr<SDockTab> SharedDocTab = WeakDockTab.Pin())
		{
			if(SWidget* Content = &SharedDocTab->GetContent().Get())
			{
				SModularRigModel* RigHierarchy = (SModularRigModel*)Content;
				if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
				{
					HierarchyTabCount = ControlRigEditorForTab->GetModularRigHierarchyTabCount();
				}
			}
		}
		return HierarchyTabCount > 0;
    }));
	DockTab->SetOnTabClosed( SDockTab::FOnTabClosedCallback::CreateLambda([](TSharedRef<SDockTab> DockTab)
	{
		if(SWidget* Content = &DockTab->GetContent().Get())
		{
			SModularRigModel* RigHierarchy = (SModularRigModel*)Content;
			if(FControlRigEditor* ControlRigEditorForTab = RigHierarchy->GetControlRigEditor())
			{
				ControlRigEditorForTab->ModularRigHierarchyTabCount--;
			}
		}
	}));
	return DockTab;
}

#undef LOCTEXT_NAMESPACE 
