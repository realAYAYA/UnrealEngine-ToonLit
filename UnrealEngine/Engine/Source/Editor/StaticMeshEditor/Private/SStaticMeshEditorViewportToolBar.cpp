// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStaticMeshEditorViewportToolBar.h"
#include "SStaticMeshEditorViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/Package.h"
#include "Components/StaticMeshComponent.h"
#include "Styling/AppStyle.h"
#include "Engine/StaticMesh.h"
#include "IStaticMeshEditor.h"
#include "StaticMeshEditorActions.h"
#include "Slate/SceneViewport.h"
#include "ComponentReregisterContext.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/StaticMeshSocket.h"
#include "SEditorViewportToolBarMenu.h"
#include "StaticMeshViewportLODCommands.h"
#include "PreviewProfileController.h"

#define LOCTEXT_NAMESPACE "StaticMeshEditorViewportToolbar"

///////////////////////////////////////////////////////////
// SStaticMeshEditorViewportToolbar


void SStaticMeshEditorViewportToolbar::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments().PreviewProfileController(MakeShared<FPreviewProfileController>()), InInfoProvider);
}

// SCommonEditorViewportToolbarBase interface
TSharedRef<SWidget> SStaticMeshEditorViewportToolbar::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		auto Commands = FStaticMeshEditorCommands::Get();

		ShowMenuBuilder.AddMenuEntry(Commands.SetShowNaniteFallback);

		ShowMenuBuilder.BeginSection("MeshComponents", LOCTEXT("MeshComponments", "Mesh Components"));
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowSockets);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowVertices);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowVertexColor);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowNormals);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowTangents);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowBinormals);
		ShowMenuBuilder.AddMenuSeparator();

		ShowMenuBuilder.AddMenuEntry(Commands.SetShowPivot);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowGrid);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowBounds);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowSimpleCollision);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowComplexCollision);
		ShowMenuBuilder.AddMenuEntry(Commands.SetShowPhysicalMaterialMasks);



		//ShowMenuBuilder.AddMenuSeparator();
		//ShowMenuBuilder.AddMenuEntry(Commands.SetShowMeshEdges);
	}

	return ShowMenuBuilder.MakeWidget();
}

// SCommonEditorViewportToolbarBase interface
void SStaticMeshEditorViewportToolbar::ExtendLeftAlignedToolbarSlots(TSharedPtr<SHorizontalBox> MainBoxPtr, TSharedPtr<SViewportToolBar> ParentToolBarPtr) const 
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
			.Label(this, &SStaticMeshEditorViewportToolbar::GetLODMenuLabel)
			.OnGetMenuContent(this, &SStaticMeshEditorViewportToolbar::GenerateLODMenu)
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(ParentToolBarPtr)
		];
}

FText SStaticMeshEditorViewportToolbar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");

	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SStaticMeshEditorViewport> ViewportRef = StaticCastSharedRef<SStaticMeshEditorViewport, SEditorViewport>(BaseViewportRef);

	int32 LODSelectionType = ViewportRef->GetLODSelection();

	if (LODSelectionType > 0)
	{
		FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
		Label = FText::FromString(TitleLabel);
	}

	return Label;
}

TSharedRef<SWidget> SStaticMeshEditorViewportToolbar::GenerateLODMenu() const
{
	const FStaticMeshViewportLODCommands& Actions = FStaticMeshViewportLODCommands::Get();

	TSharedRef<SEditorViewport> BaseViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedRef<SStaticMeshEditorViewport> ViewportRef = StaticCastSharedRef<SStaticMeshEditorViewport, SEditorViewport>(BaseViewportRef);

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
		InMenuBuilder.BeginSection("StaticMeshViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs"));
		{
			InMenuBuilder.AddMenuEntry(Actions.LODAuto);
			InMenuBuilder.AddMenuEntry(Actions.LOD0);

			int32 LODCount = ViewportRef->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT(" LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(ViewportRef, &SStaticMeshEditorViewport::OnSetLODModel, LODId + 1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(ViewportRef, &SStaticMeshEditorViewport::IsLODModelSelected, LODId + 1));

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
