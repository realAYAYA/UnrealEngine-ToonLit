// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/SIKRetargetPoseEditor.h"

#include "RetargetEditor/IKRetargetCommands.h"
#include "RetargetEditor/IKRetargetEditor.h"
#include "RetargetEditor/IKRetargetEditorController.h"

#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "SIKRetargetPoseEditor"

void SIKRetargetPoseEditor::Construct(
	const FArguments& InArgs,
	TSharedRef<FIKRetargetEditorController> InEditorController)
{
	EditorController = InEditorController;

	// the editor controller
	FIKRetargetEditorController* Controller = EditorController.Pin().Get();

	// the commands for the menus
	TSharedPtr<FUICommandList> Commands = Controller->Editor.Pin()->GetToolkitCommands();
	
	ChildSlot
	[
		SNew(SVerticalBox)

		// poses
		+SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(4,0)
				[
					SNew(SVerticalBox)

					// add pose selection label and combobox
					+SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(4,0)
					[
						SNew(STextBlock).MinDesiredWidth(180).Text(LOCTEXT("CurrentPose", "Current Retarget Pose:"))
					]
						
					// pose selection combobox
					+SVerticalBox::Slot()
					[
						SAssignNew(PoseListComboBox, SComboBox<TSharedPtr<FName>>)
						.OptionsSource(&PoseNames)
						.OnComboBoxOpening(this, &SIKRetargetPoseEditor::Refresh)
						.OnGenerateWidget_Lambda([](TSharedPtr<FName> InItem)
						{
							return SNew(STextBlock).Text(FText::FromName(*InItem.Get()));
						})
						.OnSelectionChanged(Controller, &FIKRetargetEditorController::OnPoseSelected)
						.Content()
						[
							SNew(STextBlock).Text(Controller, &FIKRetargetEditorController::GetCurrentPoseName)
						]
					]
				]

				+SHorizontalBox::Slot()
				.Padding(4,0)
				[
					SNew(SVerticalBox)

					// show retarget pose / run retarget
					+SVerticalBox::Slot()
					.Padding(0,4)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(4,0)
						[
							SNew(SButton)
							.OnClicked(Controller, &FIKRetargetEditorController::HandleShowRetargetPose)
							.IsEnabled_Lambda([Controller]
							{
								return Controller->GetRetargeterMode() != ERetargeterOutputMode::EditRetargetPose;
							})
							.Content()
							[
								SNew(SHorizontalBox)
								+SHorizontalBox::Slot()
								.AutoWidth()
								.HAlign(HAlign_Left)
								[
									SNew(SImage)
									.Image_Lambda([Controller]() -> const FSlateBrush*
									{
										if (Controller->GetRetargeterMode() == ERetargeterOutputMode::RunRetarget)
										{
											return FAppStyle::GetBrush("GenericPause");
										}
										return FAppStyle::GetBrush("GenericPlay");
									})
									.ColorAndOpacity(FSlateColor::UseForeground())
								]
						
								+SHorizontalBox::Slot()
								.HAlign(HAlign_Right)
								.Padding(4,0)
								[
									SNew(STextBlock)
									.ToolTipText(LOCTEXT("BlendPoseToolTip", "Pause playback and show the retarget pose. Use slider below to blend between reference pose (0) and retarget pose (1)."))
									.Text_Lambda([Controller]() -> const FText
									{
										if (Controller->GetRetargeterMode() == ERetargeterOutputMode::RunRetarget)
										{
											return LOCTEXT("ShowPoseText", "Show Retarget Pose");
										}
										return LOCTEXT("RunRetargetText", "Run Retargeter");
									})
								]
							]
						]
						
						+SHorizontalBox::Slot()
						.Padding(4,0)
						.AutoWidth()
						[
							SNew(SButton)
							.OnClicked_Lambda([Controller]
							{
								Controller->StopPlayback();
								return FReply::Handled();
							})
							.IsEnabled_Lambda([Controller]
							{
								return Controller->GetRetargeterMode() == ERetargeterOutputMode::RunRetarget;
							})
							.Content()
							[
								SNew(SImage)
								.ToolTipText(LOCTEXT("StopPlaybackToolTip", "Stop playback of current animation and show the reference pose."))
								.Image(FAppStyle::GetBrush("GenericStop"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]

					// blend retarget pose slider
					+SVerticalBox::Slot()
					[
						SNew(SSpinBox<float>)
						.Font(FAppStyle::Get().GetFontStyle("PropertyWindow.NormalFont"))
						.MinValue(0.0f)
						.MaxValue(1.0f)
						.Value(Controller, &FIKRetargetEditorController::GetRetargetPoseAmount)
						.OnValueChanged(Controller, &FIKRetargetEditorController::SetRetargetPoseAmount)
					]
				]
			]	
		]

		// pose editing toolbar
		+SVerticalBox::Slot()
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Center)
			[
				MakeToolbar(Commands)
			]
		]
	];
}

void SIKRetargetPoseEditor::Refresh()
{
	// get the retarget poses from the editor controller
	const FIKRetargetEditorController* Controller = EditorController.Pin().Get();
	const TMap<FName, FIKRetargetPose>& RetargetPoses = Controller->AssetController->GetRetargetPoses(Controller->GetSourceOrTarget());

	// fill list of pose names
	PoseNames.Reset();
	for (const TTuple<FName, FIKRetargetPose>& Pose : RetargetPoses)
	{
		PoseNames.Add(MakeShareable(new FName(Pose.Key)));
	}

	PoseListComboBox->RefreshOptions();
}

TSharedRef<SWidget>  SIKRetargetPoseEditor::MakeToolbar(TSharedPtr<FUICommandList> Commands)
{
	FToolBarBuilder ToolbarBuilder(Commands, FMultiBoxCustomization::None);
	
	ToolbarBuilder.BeginSection("Edit Current Pose");

	ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().EditRetargetPose,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Edit"));

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SIKRetargetPoseEditor::GenerateResetMenuContent, Commands),
		LOCTEXT("ResetPose_Label", "Reset"),
		LOCTEXT("ResetPoseToolTip_Label", "Reset bones to reference pose."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh"));

	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Create Poses");

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SIKRetargetPoseEditor::GenerateNewMenuContent, Commands),
		LOCTEXT("CreatePose_Label", "Create"),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"));

	ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().DeleteRetargetPose,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Delete"));

	ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().RenameRetargetPose,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Settings"));

	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SIKRetargetPoseEditor::GenerateResetMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetSelectedBones,
		TEXT("Reset Selected"),
		TAttribute<FText>(),
		TAttribute<FText>());

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetSelectedAndChildrenBones,
		TEXT("Reset Selected And Children"),
		TAttribute<FText>(),
		TAttribute<FText>());

	MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ResetAllBones,
		TEXT("Reset All"),
		TAttribute<FText>(),
		TAttribute<FText>());

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SIKRetargetPoseEditor::GenerateNewMenuContent(TSharedPtr<FUICommandList> Commands)
{
	FMenuBuilder MenuBuilder(true, Commands);

	MenuBuilder.BeginSection("Create", LOCTEXT("CreatePoseOperations", "Create New Retarget Pose"));
	{
		MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().NewRetargetPose,
		TEXT("Create"),
		TAttribute<FText>(),
		TAttribute<FText>());

		MenuBuilder.AddMenuEntry(
			FIKRetargetCommands::Get().DuplicateRetargetPose,
			TEXT("Create"),
			TAttribute<FText>(),
			TAttribute<FText>());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Import",LOCTEXT("ImportPoseOperations", "Import Retarget Pose"));
	{
		MenuBuilder.AddMenuEntry(
		FIKRetargetCommands::Get().ImportRetargetPose,
		TEXT("Import"),
		TAttribute<FText>(),
		TAttribute<FText>());
	
		MenuBuilder.AddMenuEntry(
			FIKRetargetCommands::Get().ImportRetargetPoseFromAnim,
			TEXT("ImportFromSequence"),
			TAttribute<FText>(),
			TAttribute<FText>());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Export",LOCTEXT("EmportPoseOperations", "Export Retarget Pose"));
	{
		MenuBuilder.AddMenuEntry(
			FIKRetargetCommands::Get().ExportRetargetPose,
			TEXT("Export"),
			TAttribute<FText>(),
			TAttribute<FText>());
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
