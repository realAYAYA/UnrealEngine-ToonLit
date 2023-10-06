// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomEditorViewportToolBar.h"
#include "AnalyticsEventAttribute.h"
#include "ComponentReregisterContext.h"
#include "EngineAnalytics.h"
#include "GroomComponent.h"
#include "GroomEditorCommands.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "SEditorViewportToolBarMenu.h"
#include "Slate/SceneViewport.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SGroomEditorViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "GroomVisualizationMenuCommands.h"
#include "EditorViewportCommands.h"

#define LOCTEXT_NAMESPACE "GroomEditorViewportToolbar"

///////////////////////////////////////////////////////////
// SGroomEditorViewportToolbar


void SGroomEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InInfoProvider);
}

// SCommonEditorViewportToolbarBase interface

TSharedRef<SWidget> SGroomEditorViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		const FEditorViewportCommands& ViewportCommands = FEditorViewportCommands::Get();
		ViewMenuBuilder.AddMenuEntry(ViewportCommands.LitMode);
		ViewMenuBuilder.AddMenuSeparator();
		FGroomVisualizationMenuCommands::BuildVisualisationSubMenuForGroomEditor(ViewMenuBuilder);
	}

	return ViewMenuBuilder.MakeWidget();
}


// SCommonEditorViewportToolbarBase interface
void SGroomEditorViewportToolbar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const 
{
	const FMargin ToolbarSlotPadding(2.0f, 2.0f);

	if (!MainBoxPtr.IsValid())
	{
		return;
	}

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.Label(this, &SGroomEditorViewportToolbar::GetLODMenuLabel)
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(ParentToolBarPtr)
			.OnGetMenuContent(this, &SGroomEditorViewportToolbar::GenerateLODMenu)
		];
}

FText SGroomEditorViewportToolbar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");

	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SGroomEditorViewport> ViewportRef = StaticCastSharedRef<SGroomEditorViewport, SEditorViewport>(BaseViewportRef);

	int32 LODSelectionType = ViewportRef->GetLODSelection();

	if (LODSelectionType >= 0)
	{
		FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType);
		Label = FText::FromString(TitleLabel);
	}

	return Label;
}

TSharedRef<SWidget> SGroomEditorViewportToolbar::GenerateLODMenu() const
{
	const FGroomViewportLODCommands& Actions = FGroomViewportLODCommands::Get();

	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SGroomEditorViewport> ViewportRef = StaticCastSharedRef<SGroomEditorViewport, SEditorViewport>(BaseViewportRef);

	TSharedPtr<FExtender> MenuExtender = GetInfoProvider().GetExtenders();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(ViewportRef->GetCommandList().ToSharedRef());
	if (MenuExtender.IsValid())
	{ 
		InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	}

	{
		// LOD Models
		InMenuBuilder.BeginSection("GroomViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs"));
		{
			InMenuBuilder.AddMenuEntry(Actions.LODAuto);
			InMenuBuilder.AddMenuEntry(Actions.LOD0);

			int32 LODCount = ViewportRef->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT(" LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(ViewportRef, &SGroomEditorViewport::OnSetLODModel, LODId),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(ViewportRef, &SGroomEditorViewport::IsLODModelSelected, LODId));

				InMenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}
		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	if (MenuExtender.IsValid())
	{
		InMenuBuilder.PopExtender();
	}

	return InMenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
