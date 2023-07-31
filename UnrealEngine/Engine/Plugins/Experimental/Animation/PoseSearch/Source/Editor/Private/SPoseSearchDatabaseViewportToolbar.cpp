// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPoseSearchDatabaseViewportToolbar.h"
#include "SPoseSearchDatabaseViewport.h"
#include "PreviewProfileController.h"
#include "PoseSearchDatabaseEditorCommands.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseViewportToolBar"

namespace UE::PoseSearch
{
	void SPoseSearchDatabaseViewportToolBar::Construct(
		const FArguments& InArgs,
		TSharedPtr<SDatabaseViewport> InViewport)
	{
		SCommonEditorViewportToolbarBase::Construct(
			SCommonEditorViewportToolbarBase::FArguments()
			.AddRealtimeButton(false)
			.PreviewProfileController(MakeShared<FPreviewProfileController>()),
			InViewport);
	}

	TSharedRef<SWidget> SPoseSearchDatabaseViewportToolBar::GenerateShowMenu() const
	{
		GetInfoProvider().OnFloatingButtonClicked();

		TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
		{
			ShowMenuBuilder.AddSubMenu(
				LOCTEXT("ShowMenu_PoseFeaturesDrawSubMenu", "Pose Features"),
				LOCTEXT("ShowMenu_PoseFeaturesDrawSubMenuToolTip", "Pose Feature Drawing Options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
			{
				const FDatabaseEditorCommands& Commands = FDatabaseEditorCommands::Get();

				SubMenuBuilder.BeginSection("PoseFeatures", LOCTEXT("ShowMenu_PoseFeaturesLabel", "Pose Features"));
				{
					SubMenuBuilder.AddMenuEntry(Commands.ShowPoseFeaturesNone);
					SubMenuBuilder.AddMenuEntry(Commands.ShowPoseFeaturesAll);
				}
				SubMenuBuilder.EndSection();
			})
			);

			ShowMenuBuilder.AddSubMenu(
				LOCTEXT("ShowMenu_AnimationsSubMenu", "Animations"),
				LOCTEXT("ShowMenu_AnimationsSubMenuToolTip", "Animation Preview Options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
			{
				const FDatabaseEditorCommands& Commands = FDatabaseEditorCommands::Get();

				SubMenuBuilder.BeginSection("Animations", LOCTEXT("ShowMenu_AnimationsLabel", "Animations"));
				{
					SubMenuBuilder.AddMenuEntry(Commands.ShowAnimationNone);
					SubMenuBuilder.AddMenuEntry(Commands.ShowAnimationOriginalOnly);
					SubMenuBuilder.AddMenuEntry(Commands.ShowAnimationOriginalAndMirrored);
				}
				SubMenuBuilder.EndSection();
			})
			);

		}

		return ShowMenuBuilder.MakeWidget();
	}
}

#undef LOCTEXT_NAMESPACE
