// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectEditorViewportToolBar.h"

#include "AssetViewerSettings.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorViewportCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorViewportClient.h"
#include "MuCOE/CustomizableObjectEditorViewportLODCommands.h"
#include "MuCOE/CustomizableObjectEditorViewportMenuCommands.h"
#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "MuCOE/SCustomizableObjectEditorViewport.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "Preferences/PersonaOptions.h"
#include "SEditorViewportViewMenu.h"
#include "SViewportToolBarComboMenu.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "UnrealEdGlobals.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SSlider.h"

class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditorViewportToolBar"


void SCustomizableObjectEditorViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<SCustomizableObjectEditorViewportTabBody> InViewport, TSharedPtr<SEditorViewport> InRealViewport)
{
	Viewport = InViewport;

	TSharedRef<SCustomizableObjectEditorViewportTabBody> ViewportRef = Viewport.Pin().ToSharedRef();

	WeakEditor = ViewportRef->CustomizableObjectEditorPtr;
	
	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)

	//// Camera Type (Perspective/Top/etc...)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(2.0f, 2.0f)
	[
		SNew(SEditorViewportToolbarMenu)
		.ParentToolBar(SharedThis(this))
		.Label(this, &SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabel)
		.LabelIcon(this, &SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabelIcon)
		.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateViewportTypeMenu)
	]

	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(FMargin(2.0f, 2.0f))
		[
			SNew( SEditorViewportToolbarMenu )
			.ParentToolBar( SharedThis( this ) )
			.Cursor( EMouseCursor::Default )
			.Image( "EditorViewportToolBar.MenuDropdown" )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
			.OnGetMenuContent( this, &SCustomizableObjectEditorViewportToolBar::GenerateOptionsMenu )
		]

	// View menu (lit, unlit, etc...)
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			SNew(SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this))
		]

	// LOD menu
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			//LOD
			SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Label(this, &SCustomizableObjectEditorViewportToolBar::GetLODMenuLabel)
				.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateLODMenu)
		]

	// View Options Menu (Camera options, Bones...)
	+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			//Show Bones
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("ViewOptionsMenuLabel","View Options"))
			.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateViewportOptionsMenu)
		];
	
	TSharedRef<SWidget> RTSButtons = GenerateRTSButtons();
	ViewportRef->SetViewportToolbarTransformWidget(RTSButtons);

	LeftToolbar->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			RTSButtons
		];

	static const FName DefaultForegroundName("DefaultForeground");

	FLinearColor ButtonColor1 = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
	FLinearColor ButtonColor2 = FLinearColor(0.2f, 0.2f, 0.2f, 0.75f);
	FLinearColor TextColor1 = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor TextColor2 = FLinearColor(0.8f, 0.8f, 0.8f, 0.8f);
	FSlateFontInfo Info = UE_MUTABLE_GET_FONTSTYLE("BoldFont");
	Info.Size += 26;

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SBorder)
				.BorderImage(UE_MUTABLE_GET_BRUSH("NoBorder"))
				.ForegroundColor(UE_MUTABLE_GET_SLATECOLOR(DefaultForegroundName))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Left)
						[
							LeftToolbar
						]
					]
				]
			]
	
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoHeight()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SAssignNew(CompileErrorLayout, SButton)
					.ButtonStyle(&FButtonStyle::GetDefault())
					.ButtonColorAndOpacity(ButtonColor2)
					.ForegroundColor(ButtonColor2)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Visibility(this, &SCustomizableObjectEditorViewportToolBar::GetShowCompileErrorOverlay)
					.Content()
					[
						SNew(STextBlock)
						.Text(this, &SCustomizableObjectEditorViewportToolBar::GetCompileErrorOverlayText)
						.Justification(ETextJustify::Center)
						.ColorAndOpacity(FSlateColor(TextColor2))
						.Font(Info)
					]
				]
			]
		];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}


EVisibility SCustomizableObjectEditorViewportToolBar::GetShowCompileErrorOverlay() const
{
	return GetCompileErrorOverlayText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}


FText SCustomizableObjectEditorViewportToolBar::GetCompileErrorOverlayText() const
{
	const TSharedPtr<ICustomizableObjectInstanceEditor> Editor = WeakEditor.Pin();
	if (!Editor)
	{
		return {};
	}

	bool bLoading = false;

	const UObject* EditingObject = (*Editor->GetObjectsCurrentlyBeingEdited())[0];
	if (const UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(EditingObject))
	{
		bLoading = CustomizableObject->GetPrivate()->Status.Get() == FCustomizableObjectStatusTypes::EState::Loading;
	}
	else if (const UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(EditingObject))
	{
		const UCustomizableObject* InstanceCustomizableObject = Instance->GetCustomizableObject();
		bLoading = InstanceCustomizableObject && InstanceCustomizableObject->GetPrivate()->Status.Get() == FCustomizableObjectStatusTypes::EState::Loading;
	}

	if (bLoading)
	{
		return LOCTEXT("LoadingAssets", "Loading Customizable Object");
	}

	const UCustomizableObjectInstance* Instance = Editor->GetPreviewInstance();
	if (!Instance)
	{
		return LOCTEXT("NoPreviewInstance", "No Preview Instance");
	}

	const UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstanceChecked();
	
	if (System->IsUpdating(Instance))
	{
		return LOCTEXT("UpdatingSkeletalMesh", "Updating Skeletal Mesh");
	}

	const UCustomizableInstancePrivate* PrivateInstance = Instance->GetPrivate();
	
	switch (PrivateInstance->SkeletalMeshStatus)
	{
	case ESkeletalMeshStatus::NotGenerated:
		return LOCTEXT("NoSkeletalMeshGenerated", "No Skeletal Mesh Generated");
				
	case ESkeletalMeshStatus::Error:
		return LOCTEXT("ErrorUpdatingSkeletalMesh", "Error Updating Skeletal Mesh");

	case ESkeletalMeshStatus::Success:
		return {};

	default:
		unimplemented();
		return {};
	}
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateViewMenu() const
{
	const FCustomizableObjectEditorViewportMenuCommands& Actions = FCustomizableObjectEditorViewportMenuCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());

	return ViewMenuBuilder.MakeWidget();
}


FText SCustomizableObjectEditorViewportToolBar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");
	if (Viewport.IsValid())
	{
		int32 LODSelectionType = Viewport.Pin()->GetLODSelection();

		if (LODSelectionType > 0)
		{
			FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
			Label = FText::FromString(TitleLabel);
		}
	}
	return Label;
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateRTSButtons()
{
	const FCustomizableObjectEditorViewportLODCommands& Actions = FCustomizableObjectEditorViewportLODCommands::Get();

	TSharedPtr< FExtender > InExtenders;
	FToolBarBuilder ToolbarBuilder(Viewport.Pin()->GetCommandList(), FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection("Transform");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Move Mode
		static FName TranslateModeName = FName(TEXT("TranslateMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateModeName);

		// Rotate Mode
		static FName RotateModeName = FName(TEXT("RotateMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().RotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), RotateModeName);

		// Scale Mode
		static FName ScaleModeName = FName(TEXT("ScaleMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().ScaleMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ScaleModeName);

	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("RotationGridSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FCustomizableObjectEditorViewportLODCommands::Get().RotationGridSnap.Get();

		static FName RotationSnapName = FName(TEXT("RotationSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(SNew(SViewportToolBarComboMenu)
			.Cursor(EMouseCursor::Default)
			.IsChecked(this, &SCustomizableObjectEditorViewportToolBar::IsRotationGridSnapChecked)
			.OnCheckStateChanged(this, &SCustomizableObjectEditorViewportToolBar::HandleToggleRotationGridSnap)
			.Label(this, &SCustomizableObjectEditorViewportToolBar::GetRotationGridLabel)
			.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::FillRotationGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("RotationGridSnap_ToolTip", "Set the Rotation Grid Snap value"))
			.Icon(Command->GetIcon())
			.ParentToolBar(SharedThis(this))
			, RotationSnapName);
	}

	ToolbarBuilder.EndSection();

	ToolbarBuilder.SetIsFocusable(true);

	return ToolbarBuilder.MakeWidget();
}

TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateLODMenu() const
{
	const FCustomizableObjectEditorViewportLODCommands& Actions = FCustomizableObjectEditorViewportLODCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
		// LOD Models
		ShowMenuBuilder.BeginSection("AnimViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs"));
		{
			ShowMenuBuilder.AddMenuEntry(Actions.LODAuto);
			ShowMenuBuilder.AddMenuEntry(Actions.LOD0);

			int32 LODCount = Viewport.Pin()->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(Viewport.Pin().ToSharedRef(), &SCustomizableObjectEditorViewportTabBody::OnSetLODModel, LODId + 1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(Viewport.Pin().ToSharedRef(), &SCustomizableObjectEditorViewportTabBody::IsLODModelSelected, LODId + 1));

				ShowMenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}
		ShowMenuBuilder.EndSection();
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateViewportTypeMenu() const
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());

	// Camera types
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GeneratePlaybackMenu() const
{
	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder PlaybackMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	
	return PlaybackMenuBuilder.MakeWidget();

}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateViewportOptionsMenu() const
{
	const FCustomizableObjectEditorViewportLODCommands& Actions = FCustomizableObjectEditorViewportLODCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
		ShowMenuBuilder.AddSubMenu(LOCTEXT("OptionsMenu_CameraOptions", "Camera Mode"),
			LOCTEXT("OptionsMenu_CameraOptionsTooltip", "Select the camera mode"),
		FNewMenuDelegate::CreateLambda([this, &Actions](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.BeginSection("Camera");
			{
				TSharedPtr<SWidget> BoneSizeWidget = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(FMargin(20.0f, 5.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OptionsMenu_CameraOptions_CameraSpeed_Text", "Camera Speed"))
					.Font(UE_MUTABLE_GET_FONTSTYLE(TEXT("MenuItem.Font")))
				]

				+SVerticalBox::Slot().AutoHeight()
				.HAlign(HAlign_Left)
				.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SBox).WidthOverride(100.0f)
						[
							SNew(SSlider)
							.Value_Lambda([this]() {return Viewport.Pin().Get()->GetViewportCameraSpeed(); })
							.MinValue(1).MaxValue(4)
							.OnValueChanged_Lambda([this](int32 Value) { Viewport.Pin().Get()->SetViewportCameraSpeed(Value); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(STextBlock).Text_Lambda([this]() { return FText::AsNumber(Viewport.Pin().Get()->GetViewportCameraSpeed()); })
					]
				];
				
				SubMenuBuilder.AddMenuEntry(Actions.OrbitalCamera);
				SubMenuBuilder.AddMenuEntry(Actions.FreeCamera);
				SubMenuBuilder.AddWidget(BoneSizeWidget.ToSharedRef(), LOCTEXT("OptionMenu_CameraOptions_CameraSpeed", ""));
			}
			SubMenuBuilder.EndSection();
		}));

		ShowMenuBuilder.AddSubMenu(LOCTEXT("OptionsMenu_BoneOptions", "Bones"), LOCTEXT("OptionsMenu_BoneOptionsTooltip", "Show/hide bone hierarchy"),
		FNewMenuDelegate::CreateLambda([&Actions](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.BeginSection("Bones");
			{
				SubMenuBuilder.AddMenuEntry(Actions.ShowBones);
			}
			SubMenuBuilder.EndSection();
		}));
	}

	return ShowMenuBuilder.MakeWidget();
}


FSlateColor SCustomizableObjectEditorViewportToolBar::GetFontColor() const
{
	const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
	const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;

	ensureMsgf(Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));

	FLinearColor FontColor;
	if (Settings->Profiles[ProfileIndex].bShowEnvironment)
	{
		FontColor = FLinearColor::White;
	}
	else
	{
		FLinearColor BackgroundColorInHSV = Viewport.Pin()->GetViewportBackgroundColor().LinearRGBToHSV();

		// see if it's dark, if V is less than 0.2
		if (BackgroundColorInHSV.B < 0.3f)
		{
			FontColor = FLinearColor::White;
		}
		else
		{
			FontColor = FLinearColor::Black;
		}
	}

	return FontColor;
}

FText SCustomizableObjectEditorViewportToolBar::GetPlaybackMenuLabel() const
{
	return LOCTEXT("PlaybackError", "Error");
}

FText SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabel() const
{
	FText Label = LOCTEXT("Viewport_Default", "Camera");
	TSharedPtr< SCustomizableObjectEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if (PinnedViewport.IsValid())
	{
		switch (PinnedViewport->GetViewportClient()->ViewportType)
		{
		case LVT_Perspective:
			Label = LOCTEXT("CameraMenuTitle_Perspective", "Perspective");
			break;

		case LVT_OrthoXY:
			Label = LOCTEXT("CameraMenuTitle_Top", "Top");
			break;

		case LVT_OrthoNegativeXZ:
			Label = LOCTEXT("CameraMenuTitle_Left", "Left");
			break;

		case LVT_OrthoNegativeYZ:
			Label = LOCTEXT("CameraMenuTitle_Front", "Front");
			break;

		case LVT_OrthoNegativeXY:
			Label = LOCTEXT("CameraMenuTitle_Bottom", "Bottom");
			break;

		case LVT_OrthoXZ:
			Label = LOCTEXT("CameraMenuTitle_Right", "Right");
			break;

		case LVT_OrthoYZ:
			Label = LOCTEXT("CameraMenuTitle_Back", "Back");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return Label;
}

const FSlateBrush* SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabelIcon() const
{
	FName Icon = NAME_None;
	TSharedPtr< SCustomizableObjectEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if (PinnedViewport.IsValid())
	{
		switch (PinnedViewport->GetViewportClient()->ViewportType)
		{
		case LVT_Perspective:
			Icon = FName("EditorViewport.Perspective");
			break;

		case LVT_OrthoXY:
			Icon = FName("EditorViewport.Top");
			break;

		case LVT_OrthoYZ:
			Icon = FName("EditorViewport.Back");
			break;

		case LVT_OrthoXZ:
			Icon = FName("EditorViewport.Right");
			break;

		case LVT_OrthoNegativeXY:
			Icon = FName("EditorViewport.Bottom");
			break;

		case LVT_OrthoNegativeYZ:
			Icon = FName("EditorViewport.Front");
			break;

		case LVT_OrthoNegativeXZ:
			Icon = FName("EditorViewport.Left");
			break;
		case LVT_OrthoFreelook:
			break;
		}
	}

	return UE_MUTABLE_GET_BRUSH(Icon);
}

float SCustomizableObjectEditorViewportToolBar::OnGetFOVValue() const
{
	return Viewport.Pin()->GetViewportClient()->ViewFOV;
}


void SCustomizableObjectEditorViewportToolBar::OnFOVValueChanged(float NewValue) const
{
	TSharedPtr<FCustomizableObjectEditorViewportClient> ViewportClient = Viewport.Pin()->GetViewportClient();

	ViewportClient->FOVAngle = NewValue;
	// \todo: this editor name should be somewhere else.
	FString EditorName("CustomizableObjectEditor");
	int ViewportIndex=0;
	ViewportClient->ConfigOption->SetViewFOV(FName(*EditorName),NewValue,ViewportIndex);

	ViewportClient->ViewFOV = NewValue;
	ViewportClient->Invalidate();
}


TOptional<float> SCustomizableObjectEditorViewportToolBar::OnGetFloorOffset() const
{
	TSharedPtr<FCustomizableObjectEditorViewportClient> ViewportClient = Viewport.Pin()->GetViewportClient();

	return ViewportClient->GetFloorOffset();
}


void SCustomizableObjectEditorViewportToolBar::OnFloorOffsetChanged(float NewValue)
{
	TSharedPtr<FCustomizableObjectEditorViewportClient> ViewportClient = Viewport.Pin()->GetViewportClient();

	ViewportClient->SetFloorOffset(NewValue);
}


ECheckBoxState SCustomizableObjectEditorViewportToolBar::IsRotationGridSnapChecked() const
{
	return GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


void SCustomizableObjectEditorViewportToolBar::HandleToggleRotationGridSnap(ECheckBoxState InState)
{
	GUnrealEd->Exec(GEditor->GetEditorWorldContext().World(), *FString::Printf(TEXT("MODE ROTGRID=%d"), !GetDefault<ULevelEditorViewportSettings>()->RotGridEnabled ? 1 : 0));
}


FText SCustomizableObjectEditorViewportToolBar::GetRotationGridLabel() const
{
	return FText::Format(LOCTEXT("GridRotation - Number - DegreeSymbol", "{0}\u00b0"), FText::AsNumber(GEditor->GetRotGridSize().Pitch));
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::FillRotationGridSnapMenu()
{
	const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();

	return SNew(SUniformGridPanel)

		+ SUniformGridPanel::Slot(0, 0)
		[
			BuildRotationGridCheckBoxList("Common", LOCTEXT("RotationCommonText", "Common"), ViewportSettings->CommonRotGridSizes, GridMode_Common)
		]

		+ SUniformGridPanel::Slot(1, 0)
		[
			BuildRotationGridCheckBoxList("Div360", LOCTEXT("RotationDivisions360DegreesText", "Divisions of 360\u00b0"), ViewportSettings->DivisionsOf360RotGridSizes, GridMode_DivisionsOf360)
		];
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::BuildRotationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes, ERotationGridMode InGridMode) const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder RotationGridMenuBuilder(bShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());

	RotationGridMenuBuilder.BeginSection(InExtentionHook, InHeading);
	for (int32 CurGridAngleIndex = 0; CurGridAngleIndex < InGridSizes.Num(); ++CurGridAngleIndex)
	{
		const float CurGridAngle = InGridSizes[CurGridAngleIndex];

		FText MenuName = FText::Format(LOCTEXT("RotationGridAngle", "{0}\u00b0"), FText::AsNumber(CurGridAngle)); /*degree symbol*/
		FText ToolTipText = FText::Format(LOCTEXT("RotationGridAngle_ToolTip", "Sets rotation grid angle to {0}"), MenuName); /*degree symbol*/

		RotationGridMenuBuilder.AddMenuEntry(
			MenuName,
			ToolTipText,
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&SCustomizableObjectEditorViewportTabBody::SetRotationGridSize, CurGridAngleIndex, InGridMode),
				FCanExecuteAction(),
				FIsActionChecked::CreateStatic(&SCustomizableObjectEditorViewportTabBody::IsRotationGridSizeChecked, CurGridAngleIndex, InGridMode)),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
	RotationGridMenuBuilder.EndSection();

	return RotationGridMenuBuilder.MakeWidget();
}


FReply SCustomizableObjectEditorViewportToolBar::OnMenuClicked()
{
	// If the menu button is clicked toggle the state of the menu anchor which will open or close the menu
	if (MenuAnchor->ShouldOpenDueToClick())
	{
		MenuAnchor->SetIsOpen(true);
		this->SetOpenMenu(MenuAnchor);
	}
	else
	{
		MenuAnchor->SetIsOpen(false);
		TSharedPtr<SMenuAnchor> NullAnchor;
		this->SetOpenMenu(MenuAnchor);
	}

	return FReply::Handled();
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateOptionsMenu() const
{
	const FCustomizableObjectEditorViewportLODCommands& LevelViewportActions = FCustomizableObjectEditorViewportLODCommands::Get();

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TArray<FLevelEditorModule::FLevelEditorMenuExtender> MenuExtenderDelegates = LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders();
	
	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(Viewport.Pin()->GetCommandList().ToSharedRef()));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bIsPerspective = Viewport.Pin()->GetViewportClient()->IsPerspective();
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder( bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender );
	{
		OptionsMenuBuilder.AddWidget( GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)") );
		OptionsMenuBuilder.AddMenuEntry( LevelViewportActions.HighResScreenshot );
	}

	return OptionsMenuBuilder.MakeWidget();
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateFOVMenu() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew( SBox )
		.HAlign( HAlign_Right )
		[
			SNew( SBox )
			.Padding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
			.WidthOverride( 100.0f )
			[
				SNew(SSpinBox<float>)
				.Font( UE_MUTABLE_GET_FONTSTYLE( TEXT( "MenuItem.Font" ) ) )
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value( this, &SCustomizableObjectEditorViewportToolBar::OnGetFOVValue )
				.OnValueChanged( this, &SCustomizableObjectEditorViewportToolBar::OnFOVValueChanged )
			]
		];
}


#undef LOCTEXT_NAMESPACE
