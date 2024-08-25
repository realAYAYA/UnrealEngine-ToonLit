// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SClothEditor3DViewportToolBar.h"
#include "ChaosClothAsset/SClothEditor3DViewport.h"
#include "ChaosClothAsset/ClothEditor3DViewportClient.h"
#include "ChaosClothAsset/ClothEditorSimulationVisualization.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorStyle.h"
#include "ChaosClothAsset/ClothEditorPreviewScene.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "EditorViewportCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "SEditorViewportViewMenu.h"


#define LOCTEXT_NAMESPACE "SChaosClothAssetEditor3DViewportToolBar"

void SChaosClothAssetEditor3DViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SChaosClothAssetEditor3DViewport> InChaosClothAssetEditor3DViewport)
{
	SCommonEditorViewportToolbarBase::Construct(SCommonEditorViewportToolbarBase::FArguments(), InChaosClothAssetEditor3DViewport);

	ChaosClothAssetEditor3DViewportPtr = InChaosClothAssetEditor3DViewport;
	CommandList = InArgs._CommandList;

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	TSharedPtr<SHorizontalBox> MainBoxPtr;

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SAssignNew( MainBoxPtr, SHorizontalBox )
			]
			+SVerticalBox::Slot()
			.Padding(FMargin(4.0f, 3.0f, 0.0f, 0.0f))
			[
				// Display text (e.g., item being previewed)
				SNew(SRichTextBlock)
				.DecoratorStyleSet(&FAppStyle::Get())
				.Text(this, &SChaosClothAssetEditor3DViewportToolBar::GetDisplayString)
				.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimViewport.MessageText"))
			]
		]
	];

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			MakeOptionsMenu()
		];

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Left)
		[
			MakeDisplayToolBar(InArgs._Extenders)
		];

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Left)
		[
			SNew(SEditorViewportToolbarMenu)
			.Label(this, &SChaosClothAssetEditor3DViewportToolBar::GetLODMenuLabel)
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(SharedThis(this))
			.OnGetMenuContent(this, &SChaosClothAssetEditor3DViewportToolBar::MakeLODMenu)
		];

	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			MakeToolBar(InArgs._Extenders)
		];
}

TSharedRef<SWidget> SChaosClothAssetEditor3DViewportToolBar::MakeOptionsMenu()
{
	return SNew(SEditorViewportToolbarMenu)
		.ParentToolBar(SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.Image("EditorViewportToolBar.OptionsDropdown")
		.OnGetMenuContent(this, &SChaosClothAssetEditor3DViewportToolBar::GenerateClothViewportOptionsMenu);
}

TSharedRef<SWidget> SChaosClothAssetEditor3DViewportToolBar::MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders)
{
	TSharedRef<SEditorViewport> ViewportRef = StaticCastSharedPtr<SEditorViewport>(ChaosClothAssetEditor3DViewportPtr.Pin()).ToSharedRef();

	return SNew(SEditorViewportViewMenu, ViewportRef, SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.MenuExtenders(InExtenders);
}

TSharedRef<SWidget> SChaosClothAssetEditor3DViewportToolBar::MakeToolBar(const TSharedPtr<FExtender> InExtenders)
{
	using namespace UE::Chaos::ClothAsset;

	// The following is modeled after portions of STransformViewportToolBar, which gets 
	// used in SCommonEditorViewportToolbarBase.

	// The buttons are hooked up to actual functions via command bindings in SChaosClothAssetEditor3DViewport::BindCommands(),
	// and the toolbar gets built in SChaosClothAssetEditor3DViewport::MakeViewportToolbar().

	FSlimHorizontalToolBarBuilder ToolbarBuilder(CommandList, FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.BeginSection("Visualization");
	ToolbarBuilder.BeginBlockGroup();
	{
		// TODO: Add button to toggle between sim and render meshes

		ToolbarBuilder.AddToolBarButton(FChaosClothAssetEditorCommands::Get().TogglePreviewWireframe,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(),
			TEXT("ToggleRenderMeshWireframe"));
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Sim Controls");
	ToolbarBuilder.BeginBlockGroup();
	{
		ToolbarBuilder.AddToolBarButton(FChaosClothAssetEditorCommands::Get().HardResetSimulation,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(),
			FName(*FChaosClothAssetEditorCommands::HardResetSimulationIdentifier));

		ToolbarBuilder.AddToolBarButton(FChaosClothAssetEditorCommands::Get().SoftResetSimulation,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(),
			FName(FChaosClothAssetEditorCommands::SoftResetSimulationIdentifier));

		ToolbarBuilder.AddToolBarButton(FChaosClothAssetEditorCommands::Get().ToggleSimulationSuspended,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>(),
			FName(FChaosClothAssetEditorCommands::ToggleSimulationSuspendedIdentifier));
	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

FText SChaosClothAssetEditor3DViewportToolBar::GetDisplayString() const
{
	using namespace UE::Chaos::ClothAsset;
	TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient = StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(ChaosClothAssetEditor3DViewportPtr.Pin()->GetViewportClient()).ToSharedRef();
	if (FClothEditorSimulationVisualization* const Visualization = ViewportClient->GetSimulationVisualization().Pin().Get())
	{
		return Visualization->GetDisplayString(ViewportClient->GetPreviewClothComponent());
	}
	return FText();
}

FText SChaosClothAssetEditor3DViewportToolBar::GetLODMenuLabel() const
{
	using namespace UE::Chaos::ClothAsset;

	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto"); 

	TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient = 
		StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(ChaosClothAssetEditor3DViewportPtr.Pin()->GetViewportClient()).ToSharedRef();

	const int32 LODSelectionType = ViewportClient->GetLODModel();
	if (LODSelectionType >= 0)
	{
		const FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType);
		Label = FText::FromString(TitleLabel);
	}
	return Label;
}

TSharedRef<SWidget> SChaosClothAssetEditor3DViewportToolBar::MakeLODMenu() const
{
	using namespace UE::Chaos::ClothAsset;
	TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient =
		StaticCastSharedPtr<FChaosClothAssetEditor3DViewportClient>(ChaosClothAssetEditor3DViewportPtr.Pin()->GetViewportClient()).ToSharedRef();

	constexpr bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

	MenuBuilder.PushCommandList(CommandList.ToSharedRef());
	const int32 NumLODs = ViewportClient->GetNumLODs();

	MenuBuilder.BeginSection("ClothAssetPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs"));
	{
		MenuBuilder.AddMenuEntry(FChaosClothAssetEditorCommands::Get().LODAuto);
		MenuBuilder.AddMenuEntry(FChaosClothAssetEditorCommands::Get().LOD0);

		for (int32 LODIndex = 1; LODIndex < NumLODs; ++LODIndex)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODIndex);

			FUIAction Action(FExecuteAction::CreateSP(ViewportClient, &FChaosClothAssetEditor3DViewportClient::SetLODModel, LODIndex),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(ViewportClient, &FChaosClothAssetEditor3DViewportClient::IsLODModelSelected, LODIndex));

			MenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
		}
	}
	MenuBuilder.EndSection();
	MenuBuilder.PopCommandList();
	return MenuBuilder.MakeWidget();
}


#undef LOCTEXT_NAMESPACE
