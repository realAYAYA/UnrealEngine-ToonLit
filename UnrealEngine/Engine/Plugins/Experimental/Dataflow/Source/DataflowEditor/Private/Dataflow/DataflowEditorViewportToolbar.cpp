// Copyright Epic Games, Inc. All Rights Reserved.
#include "Dataflow/DataflowEditorViewportToolbar.h"

#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorViewport.h"
#include "EditorViewportCommands.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Layout/SBorder.h"

bool bDataflowPrototypeSelectionMenu = false;
FAutoConsoleVariableRef CVarDataflowPrototypeSelectionMenu(TEXT("p.Dataflow.prototype.selection"), bDataflowPrototypeSelectionMenu, TEXT("Work in progress development for selection controls in the 3D Viewport.[def:false]"));


void SDataflowViewportSelectionToolBar::Construct(const FArguments& InArgs)
{
	EditorViewport = InArgs._EditorViewport;
	static const FName DefaultForegroundName("DefaultForeground");

	this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.ForegroundColor(FAppStyle::GetSlateColor(DefaultForegroundName))
			[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				.HAlign(HAlign_Right)
				[
					MakeSelectionModeToolBar()
				]
			]
		];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

TSharedRef<SWidget> SDataflowViewportSelectionToolBar::MakeSelectionModeToolBar()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilder(EditorViewport.Pin()->GetCommandList(), FMultiBoxCustomization::None);

	const FName ToolBarStyle = TEXT("EditorViewportToolBar");
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection("Selection");
	{
		ToolbarBuilder.BeginBlockGroup();

		if (bDataflowPrototypeSelectionMenu)
		{
			ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().ToggleObjectSelection,
											NAME_None,
											TAttribute<FText>(),
											TAttribute<FText>(),
											FSlateIcon(FDataflowEditorStyle::Get().GetStyleSetName(), TEXT("Dataflow.SelectObject")),
											FName(TEXT("SelectObjectMode"))
			);
			ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().ToggleFaceSelection,
											NAME_None,
											TAttribute<FText>(),
											TAttribute<FText>(),
											FSlateIcon(FDataflowEditorStyle::Get().GetStyleSetName(), TEXT("Dataflow.SelectFace")),
											FName(TEXT("SelectFaceMode"))
			);
			ToolbarBuilder.AddToolBarButton(FDataflowEditorCommands::Get().ToggleVertexSelection,
											NAME_None,
											TAttribute<FText>(),
											TAttribute<FText>(),
											FSlateIcon(FDataflowEditorStyle::Get().GetStyleSetName(), TEXT("Dataflow.SelectVertex")),
											FName(TEXT("SelectVertexMode"))
			);
		}

		ToolbarBuilder.EndBlockGroup();
	}

	ToolbarBuilder.EndSection();
	ToolbarBuilder.AddSeparator();

	return ToolbarBuilder.MakeWidget();
}