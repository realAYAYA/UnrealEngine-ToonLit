// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDEditorViewportViewMenu.h"

#include "EditorViewportCommands.h"
#include "Engine/EngineBaseTypes.h"
#include "SViewportToolBar.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "UObject/NameTypes.h"

void SChaosVDEditorViewportViewMenu::Construct(const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<SViewportToolBar> InParentToolBar)
{
	SEditorViewportViewMenu::Construct(SEditorViewportViewMenu::FArguments(), InViewport, InParentToolBar);
	MenuName = FName("ChaosVD.ViewportToolbar.View");
}

void SChaosVDEditorViewportViewMenu::RegisterMenus() const
{
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		if (UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName))
		{
			Menu->AddDynamicSection("BaseSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				if (InMenu)
				{
					const FEditorViewportCommands& BaseViewportActions = FEditorViewportCommands::Get();
					FToolMenuSection& Section = InMenu->AddSection("ViewMode", NSLOCTEXT("ChaosVisualDebugger","ViewModeHeader", "View Mode"));
					{
						Section.AddMenuEntry(BaseViewportActions.LitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Lit));
						Section.AddMenuEntry(BaseViewportActions.UnlitMode, UViewModeUtils::GetViewModeDisplayName(VMI_Unlit));
						Section.AddMenuEntry(BaseViewportActions.WireframeMode, UViewModeUtils::GetViewModeDisplayName(VMI_BrushWireframe));
					}
				}
			}));
		}
	}
}
