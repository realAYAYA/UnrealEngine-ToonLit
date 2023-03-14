// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectEditorViewportToolBar.h"

#include "AssetViewerSettings.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/UnrealEdTypes.h"
#include "EditorViewportCommands.h"
#include "Engine/Engine.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Text/TextLayout.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "LevelEditor.h"
#include "Math/Color.h"
#include "Math/Rotator.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditorViewportClient.h"
#include "MuCOE/CustomizableObjectEditorViewportLODCommands.h"
#include "MuCOE/CustomizableObjectEditorViewportMenuCommands.h"
#include "MuCOE/ICustomizableObjectInstanceEditor.h"
#include "MuCOE/SCustomizableObjectEditorViewport.h"
#include "Preferences/PersonaOptions.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportViewMenu.h"
#include "SViewportToolBarComboMenu.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Types/ISlateMetaData.h"
#include "Types/SlateStructs.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealNames.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditorViewportToolBar"


//Class definition which represents widget to modify viewport's background color.
//class SBackgroundColorSettings : public SCompoundWidget
//{
//public:
//
//	SLATE_BEGIN_ARGS(SBackgroundColorSettings)
//	{}
//	SLATE_ARGUMENT(TWeakPtr<SCustomizableObjectEditorViewport>, AnimEditorViewport)
//		SLATE_END_ARGS()
//
//		/** Constructs this widget from its declaration */
//		void Construct(const FArguments& InArgs)
//	{
//		AnimViewportPtr = InArgs._AnimEditorViewport;
//
//		TSharedPtr<SWidget> ExtraWidget =
//			SNew(SBorder)
//			.BorderImage(FAppStyle::GetBrush("FilledBorder"))
//			[
//				SNew(SHorizontalBox)
//				+ SHorizontalBox::Slot()
//			.AutoWidth()
//			.Padding(1)
//			[
//				SNew(SColorBlock)
//				.Color(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetViewportBackgroundColor)
//			.IgnoreAlpha(true)
//			.ToolTipText(LOCTEXT("ColorBlock_ToolTip", "Select background color"))
//			.OnMouseButtonDown(this, &SBackgroundColorSettings::OnColorBoxClicked)
//			]
//			];
//
//		this->ChildSlot
//			[
//				SNew(SAnimPlusMinusSlider)
//				.Label(LOCTEXT("BrightNess", "Brightness:"))
//			.IsEnabled(this, &SBackgroundColorSettings::IsBrightnessSliderEnabled)
//			.OnMinusClicked(this, &SBackgroundColorSettings::OnDecreaseBrightness)
//			.MinusTooltip(LOCTEXT("DecreaseBrightness_ToolTip", "Decrease brightness"))
//			.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetBackgroundBrightness)
//			.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::SetBackgroundBrightness)
//			.SliderTooltip(LOCTEXT("BackgroundBrightness_ToolTip", "Change background brightness"))
//			.OnPlusClicked(this, &SBackgroundColorSettings::OnIncreaseBrightness)
//			.PlusTooltip(LOCTEXT("IncreaseBrightness_ToolTip", "Increase brightness"))
//			.ExtraWidget(ExtraWidget)
//			];
//	}
//
//protected:
//
//	/** Function to open color picker window when selected from context menu */
//	FReply OnColorBoxClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
//	{
//		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
//		{
//			return FReply::Unhandled();
//		}
//
//		FSlateApplication::Get().DismissAllMenus();
//
//		FLinearColor BackgroundColor = AnimViewportPtr.Pin()->GetViewportBackgroundColor();
//		TArray<FLinearColor*> LinearColorArray;
//		LinearColorArray.Add(&BackgroundColor);
//
//		FColorPickerArgs PickerArgs;
//		PickerArgs.bIsModal = true;
//		PickerArgs.ParentWidget = AnimViewportPtr.Pin();
//		PickerArgs.bOnlyRefreshOnOk = true;
//		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
//		PickerArgs.LinearColorArray = &LinearColorArray;
//		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::SetViewportBackgroundColor);
//
//		if (OpenColorPicker(PickerArgs))
//		{
//			AnimViewportPtr.Pin()->RefreshViewport();
//		}
//
//		return FReply::Handled();
//	}
//
//	/** Callback function for decreasing color brightness */
//	FReply OnDecreaseBrightness()
//	{
//		const float DeltaValue = 0.05f;
//		AnimViewportPtr.Pin()->SetBackgroundBrightness(AnimViewportPtr.Pin()->GetBackgroundBrightness() - DeltaValue);
//		return FReply::Handled();
//	}
//
//	/** Callback function for increasing color brightness */
//	FReply OnIncreaseBrightness()
//	{
//		const float DeltaValue = 0.05f;
//		AnimViewportPtr.Pin()->SetBackgroundBrightness(AnimViewportPtr.Pin()->GetBackgroundBrightness() + DeltaValue);
//		return FReply::Handled();
//	}
//
//	/** Callback function which determines whether this widget is enabled */
//	bool IsBrightnessSliderEnabled() const
//	{
//		const UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
//		const UEditorPerProjectUserSettings* PerProjectUserSettings = GetDefault<UEditorPerProjectUserSettings>();
//		const int32 ProfileIndex = Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex) ? PerProjectUserSettings->AssetViewerProfileIndex : 0;
//
//		ensureMsgf(Settings->Profiles.IsValidIndex(PerProjectUserSettings->AssetViewerProfileIndex), TEXT("Invalid default settings pointer or current profile index"));
//
//		return !Settings->Profiles[ProfileIndex].bShowEnvironment;
//	}
//
//protected:
//
//	/** The viewport hosting this widget */
//	TWeakPtr<SCustomizableObjectEditorViewport> AnimViewportPtr;
//};

//Class definition which represents widget to modify strength of wind for clothing
//class SClothWindSettings : public SCompoundWidget
//{
//public:
//
//	SLATE_BEGIN_ARGS(SClothWindSettings)
//	{}
//
//	SLATE_ARGUMENT(TWeakPtr<SCustomizableObjectEditorViewport>, AnimEditorViewport)
//		SLATE_END_ARGS()
//
//		/** Constructs this widget from its declaration */
//		void Construct(const FArguments& InArgs)
//	{
//		AnimViewportPtr = InArgs._AnimEditorViewport;
//
//		TSharedPtr<SWidget> ExtraWidget = SNew(STextBlock)
//			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetWindStrengthLabel)
//			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")));
//
//		this->ChildSlot
//			[
//				SNew(SAnimPlusMinusSlider)
//				.IsEnabled(this, &SClothWindSettings::IsWindEnabled)
//			.Label(LOCTEXT("WindStrength", "Wind Strength:"))
//			.OnMinusClicked(this, &SClothWindSettings::OnDecreaseWindStrength)
//			.MinusTooltip(LOCTEXT("DecreaseWindStrength_ToolTip", "Decrease Wind Strength"))
//			.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetWindStrengthSliderValue)
//			.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::SetWindStrength)
//			.SliderTooltip(LOCTEXT("WindStrength_ToolTip", "Change wind strength"))
//			.OnPlusClicked(this, &SClothWindSettings::OnIncreaseWindStrength)
//			.PlusTooltip(LOCTEXT("IncreasetWindStrength_ToolTip", "Increase Wind Strength"))
//			.ExtraWidget(ExtraWidget)
//			];
//	}
//
//protected:
//	/** Callback function for decreasing size */
//	FReply OnDecreaseWindStrength()
//	{
//		const float DeltaValue = 0.1f;
//		AnimViewportPtr.Pin()->SetWindStrength(AnimViewportPtr.Pin()->GetWindStrengthSliderValue() - DeltaValue);
//		return FReply::Handled();
//	}
//
//	/** Callback function for increasing size */
//	FReply OnIncreaseWindStrength()
//	{
//		const float DeltaValue = 0.1f;
//		AnimViewportPtr.Pin()->SetWindStrength(AnimViewportPtr.Pin()->GetWindStrengthSliderValue() + DeltaValue);
//		return FReply::Handled();
//	}
//
//	/** Callback function which determines whether this widget is enabled */
//	bool IsWindEnabled() const
//	{
//		return AnimViewportPtr.Pin()->IsApplyingClothWind();
//	}
//
//protected:
//	/** The viewport hosting this widget */
//	TWeakPtr<SCustomizableObjectEditorViewport> AnimViewportPtr;
//};

//Class definition which represents widget to modify gravity for preview
//class SGravitySettings : public SCompoundWidget
//{
//public:
//
//	SLATE_BEGIN_ARGS(SGravitySettings)
//	{}
//
//	SLATE_ARGUMENT(TWeakPtr<SCustomizableObjectEditorViewport>, AnimEditorViewport)
//		SLATE_END_ARGS()
//
//		void Construct(const FArguments& InArgs)
//	{
//		AnimViewportPtr = InArgs._AnimEditorViewport;
//
//		TSharedPtr<SWidget> ExtraWidget = SNew(STextBlock)
//			.Text(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetGravityScaleLabel)
//			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")));
//
//		this->ChildSlot
//			[
//				SNew(SAnimPlusMinusSlider)
//				.Label(LOCTEXT("Gravity Scale", "Gravity Scale Preview:"))
//			.OnMinusClicked(this, &SGravitySettings::OnDecreaseGravityScale)
//			.MinusTooltip(LOCTEXT("DecreaseGravitySize_ToolTip", "Decrease Gravity Scale"))
//			.SliderValue(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::GetGravityScaleSliderValue)
//			.OnSliderValueChanged(AnimViewportPtr.Pin().ToSharedRef(), &SCustomizableObjectEditorViewport::SetGravityScale)
//			.SliderTooltip(LOCTEXT("GravityScale_ToolTip", "Change Gravity Scale"))
//			.OnPlusClicked(this, &SGravitySettings::OnIncreaseGravityScale)
//			.PlusTooltip(LOCTEXT("IncreaseGravityScale_ToolTip", "Increase Gravity Scale"))
//			.ExtraWidget(ExtraWidget)
//			];
//	}
//
//protected:
//	FReply OnDecreaseGravityScale()
//	{
//		const float DeltaValue = 0.025f;
//		AnimViewportPtr.Pin()->SetGravityScale(AnimViewportPtr.Pin()->GetGravityScaleSliderValue() - DeltaValue);
//		return FReply::Handled();
//	}
//
//	FReply OnIncreaseGravityScale()
//	{
//		const float DeltaValue = 0.025f;
//		AnimViewportPtr.Pin()->SetGravityScale(AnimViewportPtr.Pin()->GetGravityScaleSliderValue() + DeltaValue);
//		return FReply::Handled();
//	}
//
//protected:
//	TWeakPtr<SCustomizableObjectEditorViewport> AnimViewportPtr;
//};

///////////////////////////////////////////////////////////
// SCustomizableObjectEditorViewportToolBar


void SCustomizableObjectEditorViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SCustomizableObjectEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport)
{
	Viewport = InViewport;

	TSharedRef<SCustomizableObjectEditorViewportTabBody> ViewportRef = Viewport.Pin().ToSharedRef();

	//TSharedRef<SCustomizableObjectEditorTransformViewportToolbar> Toolbar = SNew(SCustomizableObjectEditorTransformViewportToolbar)
	//	.Viewport(InRealViewport.ToSharedRef())
	//	.ViewportTapBody(ViewportRef);

	//TSharedRef<SHorizontalBox> RightToolbar = SNew(SHorizontalBox)
	//	+ SHorizontalBox::Slot()
	//	.AutoWidth()
	//	.Padding(2.0f, 2.0f)
	//	[
	//		Toolbar
	//	];

	//ViewportRef->SetEditorTransformViewportToolbar(Toolbar);


	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
	//	// Generic viewport options
	//	+ SHorizontalBox::Slot()
	//	.AutoWidth()
	//	.Padding(2.0f, 2.0f)
	//	[
	//		//Menu
	//		SNew(SEditorViewportToolbarMenu)
	//		.ParentToolBar(SharedThis(this))
	//	.Image("EditorViewportToolBar.MenuDropdown")
	//	.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateViewMenu)
	//	]

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

	// Show flags menu
	//+ SHorizontalBox::Slot()
	//	.AutoWidth()
	//	.Padding(2.0f, 2.0f)
	//	[
	//		SNew(SEditorViewportToolbarMenu)
	//		.ParentToolBar(SharedThis(this))
	//	.Label(LOCTEXT("ShowMenu", "Show"))
	//	.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateShowMenu)
	//	]

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

	// Camera Mode menu (Free or Orbital camera)
	+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			//Camera Mode
			SNew(SEditorViewportToolbarMenu)
				.ParentToolBar(SharedThis(this))
				.Label(this, &SCustomizableObjectEditorViewportToolBar::GetCameraModeMenuLabel)
				.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateCameraModeMenu)
		];
	
	TSharedRef<SWidget> RTSButtons = GenerateRTSButtons();
	ViewportRef->SetViewportToolbarTransformWidget(RTSButtons);

	LeftToolbar->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 2.0f)
		[
			RTSButtons
		];

	// Playback speed menu
	//+ SHorizontalBox::Slot()
	//	.AutoWidth()
	//	.Padding(2.0f, 2.0f)
	//	[
	//		SNew(SEditorViewportToolbarMenu)
	//		.ParentToolBar(SharedThis(this))
	//	.Label(this, &SCustomizableObjectEditorViewportToolBar::GetPlaybackMenuLabel)
	//	.LabelIcon(FAppStyle::GetBrush("AnimViewportMenu.PlayBackSpeed"))
	//	.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GeneratePlaybackMenu)
	//	]

	//+ SHorizontalBox::Slot()
	//	.Padding(3.0f, 1.0f)
	//	.HAlign(HAlign_Right)
	//	[
	//		SNew(STransformViewportToolBar)
	//		.Viewport(InRealViewport)
	//	.CommandList(InRealViewport->GetCommandList())
	//	.Visibility(this, &SCustomizableObjectEditorViewportToolBar::GetTransformToolbarVisibility)
	//	];
	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();

	static const FName DefaultForegroundName("DefaultForeground");

	FLinearColor ButtonColor1 = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
	FLinearColor ButtonColor2 = FLinearColor(0.2f, 0.2f, 0.2f, 0.75f);
	FLinearColor TextColor1 = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	FLinearColor TextColor2 = FLinearColor(0.8f, 0.8f, 0.8f, 0.8f);
	FSlateFontInfo Info = FAppStyle::GetFontStyle("BoldFont");
	Info.Size += 26;

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("NoBorder"))
				.ForegroundColor(FAppStyle::GetSlateColor(DefaultForegroundName))
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

	//CompileErrorLayout.Get()->SetVisibility(EVisibility::Hidden);
}

EVisibility SCustomizableObjectEditorViewportToolBar::GetShowCompileErrorOverlay() const
{
	if ((Viewport.IsValid()) && (Viewport.Pin().Get() != nullptr) &&
		Viewport.Pin()->CustomizableObjectEditorPtr.IsValid() &&
		Viewport.Pin().Get()->GetViewportClient().IsValid() &&
		(Viewport.Pin().Get()->GetViewportClient().Get() != nullptr))
	{
		if (Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetAssetRegistryLoaded())
		{
			UCustomizableObjectInstance* Instance = Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetPreviewInstance();

			if ((Instance != nullptr) && !Instance->bEditorPropertyChanged && !Viewport.Pin().Get()->GetViewportClient()->GetGizmoIsProjectorParameterSelected())
			{
				if ((Instance->SkeletalMeshStatus == ESkeletalMeshState::UpdateError) ||
					(Instance->SkeletalMeshStatus == ESkeletalMeshState::PostUpdateError) ||
					(Instance->SkeletalMeshStatus == ESkeletalMeshState::AsyncUpdatePending))
				{
					return EVisibility::Visible;
				}
				else
				{
					return EVisibility::Hidden;
				}
			}

			if ((Instance == nullptr) &&
				!Viewport.Pin().Get()->GetViewportClient()->GetGizmoIsProjectorParameterSelected())
			{
				return EVisibility::Visible;
			}
		}
		else
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}


EVisibility SCustomizableObjectEditorViewportToolBar::GetShowCompileInfoOverlay() const
{
	if ((Viewport.IsValid()) && (Viewport.Pin().Get() != nullptr) &&
		(Viewport.Pin()->CustomizableObjectEditorPtr.IsValid()) &&
		Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetAssetRegistryLoaded())
	{
		if (Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetAssetRegistryLoaded())
		{
			UCustomizableObjectInstance* Instance = Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetPreviewInstance();

			if (Instance != nullptr)
			{
				if ((Instance->SkeletalMeshStatus == ESkeletalMeshState::UpdateError) ||
					(Instance->SkeletalMeshStatus == ESkeletalMeshState::PostUpdateError) ||
					(Instance->SkeletalMeshStatus == ESkeletalMeshState::AsyncUpdatePending))
				{
					return EVisibility::Visible;
				}
				else
				{
					return EVisibility::Hidden;
				}
			}
		}
		else
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}


FText SCustomizableObjectEditorViewportToolBar::GetCompileErrorOverlayText() const
{
	if ((Viewport.IsValid()) && (Viewport.Pin().Get() != nullptr) &&
		(Viewport.Pin()->CustomizableObjectEditorPtr.IsValid()))
	{
		if (Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetAssetRegistryLoaded())
		{
			UCustomizableObjectInstance* Instance = Viewport.Pin()->CustomizableObjectEditorPtr.Pin()->GetPreviewInstance();

			if (Instance != nullptr)
			{
				if (Instance->SkeletalMeshStatus == ESkeletalMeshState::UpdateError)
				{
					return FText::FromString("Error updating skeletal mesh");
				}
				else if (Instance->SkeletalMeshStatus == ESkeletalMeshState::PostUpdateError)
				{
					return FText::FromString("Post Update Error: check the output log for more information.");
				}
				else if (Instance->SkeletalMeshStatus == ESkeletalMeshState::AsyncUpdatePending)
				{
					TSharedPtr<FCustomizableObjectEditorViewportClient> viewport = Viewport.Pin()->GetViewportClient();
					if (viewport.IsValid())
					{
						const GizmoRTSProxy& gizmo = viewport->GetGizmoProxy();
						if (gizmo.AnyGizmoSelected)
						{
							if (gizmo.AssignedDataIsFromNode)
							{
								return FText::FromString("Changing default projector");
							}
							else
							{
								return FText::FromString("Changing current projector instance");
							}
						}
					}
					return FText::FromString("Updating skeletal mesh");
				}
			}
			else
			{
				return FText::FromString("No Skeletal Mesh generated");
			}
		}
		else
		{
			return FText::FromString("Loading assets");
		}
	}
	return FText::FromString("");
}


EVisibility SCustomizableObjectEditorViewportToolBar::GetTransformToolbarVisibility() const
{
	return EVisibility::Hidden;
	//return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}


TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateViewMenu() const
{
	const FCustomizableObjectEditorViewportMenuCommands& Actions = FCustomizableObjectEditorViewportMenuCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	//{
	//	ViewMenuBuilder.BeginSection("AnimViewportSceneSetup", LOCTEXT("ViewMenu_SceneSetupLabel", "Scene Setup"));
	//	{
	//		ViewMenuBuilder.AddMenuEntry(FCustomizableObjectEditorViewportMenuCommands::Get().PreviewSceneSettings);

	//		// We cant allow animation configuration via the viewport menu in 'old regular' Persona
	//		if (GetDefault<UPersonaOptions>()->bUseStandaloneAnimationEditors)
	//		{
	//			ViewMenuBuilder.AddSubMenu(
	//				LOCTEXT("SceneSetupLabel", "Scene Setup"),
	//				LOCTEXT("SceneSetupTooltip", "Set up preview meshes, animations etc."),
	//				FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::GenerateSceneSetupMenu),
	//				false,
	//				FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.SceneSetup")
	//			);
	//		}

	//		ViewMenuBuilder.AddSubMenu(
	//			LOCTEXT("TurnTableLabel", "Turn Table"),
	//			LOCTEXT("TurnTableTooltip", "Set up auto-rotation of preview."),
	//			FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::GenerateTurnTableMenu),
	//			false,
	//			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.TurnTableSpeed")
	//		);
	//	}
	//	ViewMenuBuilder.EndSection();

	//	ViewMenuBuilder.BeginSection("AnimViewportCamera", LOCTEXT("ViewMenu_CameraLabel", "Camera"));
	//	{
	//		ViewMenuBuilder.AddMenuEntry(FCustomizableObjectEditorViewportMenuCommands::Get().CameraFollow);
	//	}
	//	ViewMenuBuilder.EndSection();
	//}

	return ViewMenuBuilder.MakeWidget();
}

//TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateShowMenu() const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	const bool bInShouldCloseWindowAfterMenuSelection = true;
//	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
//	{
//		ShowMenuBuilder.BeginSection("AnimViewportFOV", LOCTEXT("Viewport_FOVLabel", "Field Of View"));
//		{
//			const float FOVMin = 5.f;
//			const float FOVMax = 170.f;
//
//			TSharedPtr<SWidget> FOVWidget = SNew(SSpinBox<float>)
//				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
//				.MinValue(FOVMin)
//				.MaxValue(FOVMax)
//				.Value(this, &SCustomizableObjectEditorViewportToolBar::OnGetFOVValue)
//				.OnValueChanged(this, &SCustomizableObjectEditorViewportToolBar::OnFOVValueChanged)
//				.OnValueCommitted(this, &SCustomizableObjectEditorViewportToolBar::OnFOVValueCommitted);
//
//			ShowMenuBuilder.AddWidget(FOVWidget.ToSharedRef(), FText());
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.BeginSection("AnimViewportAudio", LOCTEXT("Viewport_AudioLabel", "Audio"));
//		{
//			ShowMenuBuilder.AddMenuEntry(Actions.MuteAudio);
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.BeginSection("AnimViewportRootMotion", LOCTEXT("Viewport_RootMotionLabel", "Root Motion"));
//		{
//			ShowMenuBuilder.AddMenuEntry(Actions.ProcessRootMotion);
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.BeginSection("AnimViewportMesh", LOCTEXT("ShowMenu_Actions_Mesh", "Mesh"));
//		{
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowRetargetBasePose);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowBound);
//			ShowMenuBuilder.AddMenuEntry(Actions.UseInGameBound);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowPreviewMesh);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowMorphTargets);
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.BeginSection("AnimViewportAnimation", LOCTEXT("ShowMenu_Actions_Asset", "Asset"));
//		{
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowRawAnimation);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowNonRetargetedAnimation);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowAdditiveBaseBones);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowSourceRawAnimation);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowBakedAnimation);
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.BeginSection("AnimViewportPreviewBones", LOCTEXT("ShowMenu_Actions_Bones", "Hierarchy"));
//		{
//			ShowMenuBuilder.AddSubMenu(
//				LOCTEXT("AnimViewportBoneDrawSubMenu", "Bone"),
//				LOCTEXT("AnimViewportBoneDrawSubMenuToolTip", "Bone Draw Option"),
//				FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::FillShowBoneDrawMenu)
//			);
//
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowSockets);
//			ShowMenuBuilder.AddMenuEntry(Actions.ShowBoneNames);
//
//			ShowMenuBuilder.AddSubMenu(
//				LOCTEXT("AnimViewportOverlayDrawSubMenu", "Mesh Overlay"),
//				LOCTEXT("AnimViewportOverlayDrawSubMenuToolTip", "Options"),
//				FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::FillShowOverlayDrawMenu)
//			);
//		}
//		ShowMenuBuilder.EndSection();
//
//		ShowMenuBuilder.AddMenuSeparator();
//		ShowMenuBuilder.AddSubMenu(
//			LOCTEXT("AnimviewportInfo", "Display Info"),
//			LOCTEXT("AnimviewportInfoSubMenuToolTip", "Display Mesh Info in Viewport"),
//			FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::FillShowDisplayInfoMenu));
//
//
//		ShowMenuBuilder.AddMenuSeparator();
//
//		ShowMenuBuilder.AddSubMenu(
//			LOCTEXT("AnimViewportSceneSubMenu", "Scene Setup"),
//			LOCTEXT("AnimViewportSceneSubMenuToolTip", "Options relating to the preview scene"),
//			FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::FillShowSceneMenu));
//
//		ShowMenuBuilder.AddSubMenu(
//			LOCTEXT("AnimViewportAdvancedSubMenu", "Advanced"),
//			LOCTEXT("AnimViewportAdvancedSubMenuToolTip", "Advanced options"),
//			FNewMenuDelegate::CreateRaw(this, &SCustomizableObjectEditorViewportToolBar::FillShowAdvancedMenu));
//	}
//
//	return ShowMenuBuilder.MakeWidget();
//
//}

//void SCustomizableObjectEditorViewportToolBar::FillShowSceneMenu(FMenuBuilder& MenuBuilder) const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	MenuBuilder.BeginSection("AnimViewportAccessory", LOCTEXT("Viewport_AccessoryLabel", "Accessory"));
//	{
//		MenuBuilder.AddMenuEntry(Actions.AutoAlignFloorToMesh);
//	}
//	MenuBuilder.EndSection();
//
//	MenuBuilder.BeginSection("AnimViewportFloorOffset", LOCTEXT("Viewport_FloorOffsetLabel", "Floor Height Offset"));
//	{
//		TSharedPtr<SWidget> FloorOffsetWidget = SNew(SNumericEntryBox<float>)
//			.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
//			.Value(this, &SCustomizableObjectEditorViewportToolBar::OnGetFloorOffset)
//			.OnValueChanged(this, &SCustomizableObjectEditorViewportToolBar::OnFloorOffsetChanged)
//			.ToolTipText(LOCTEXT("FloorOffsetToolTip", "Height offset for the floor mesh (stored per-mesh)"));
//
//		MenuBuilder.AddWidget(FloorOffsetWidget.ToSharedRef(), FText());
//	}
//	MenuBuilder.EndSection();
//
//	MenuBuilder.BeginSection("AnimViewportGrid", LOCTEXT("Viewport_GridLabel", "Grid"));
//	{
//		MenuBuilder.AddMenuEntry(Actions.ToggleGrid);
//	}
//	MenuBuilder.EndSection();
//
//	MenuBuilder.BeginSection("AnimViewportBackground", LOCTEXT("Viewport_BackgroundLabel", "Background"));
//	{
//		TSharedPtr<SWidget> BackgroundColorWidget = SNew(SBackgroundColorSettings).AnimEditorViewport(Viewport);
//		MenuBuilder.AddWidget(BackgroundColorWidget.ToSharedRef(), FText());
//	}
//	MenuBuilder.EndSection();
//}

//void SCustomizableObjectEditorViewportToolBar::FillShowBoneDrawMenu(FMenuBuilder& MenuBuilder) const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyBoneDraw", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes"));
//	{
//		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawAll);
//		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawSelected);
//		MenuBuilder.AddMenuEntry(Actions.ShowBoneDrawNone);
//	}
//	MenuBuilder.EndSection();
//}
//
//void SCustomizableObjectEditorViewportToolBar::FillShowOverlayDrawMenu(FMenuBuilder& MenuBuilder) const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	MenuBuilder.BeginSection("AnimViewportPreviewOverlayDraw", LOCTEXT("ShowMenu_Actions_Overlay", "Overlay Draw"));
//	{
//		MenuBuilder.AddMenuEntry(Actions.ShowOverlayNone);
//		MenuBuilder.AddMenuEntry(Actions.ShowBoneWeight);
//		MenuBuilder.AddMenuEntry(Actions.ShowMorphTargetVerts);
//	}
//	MenuBuilder.EndSection();
//}
//
//void SCustomizableObjectEditorViewportToolBar::FillShowAdvancedMenu(FMenuBuilder& MenuBuilder) const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	// Draw UVs
//	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
//	{
//		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().AnimSetDrawUVs);
//		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
//	}
//	MenuBuilder.EndSection();
//
//	MenuBuilder.BeginSection("Skinning", LOCTEXT("Skinning_Label", "Skinning"));
//	{
//		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetCPUSkinning);
//	}
//	MenuBuilder.EndSection();
//
//
//	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
//	{
//		// Vertex debug flags
//		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
//		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
//		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
//	}
//	MenuBuilder.EndSection();
//
//	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes"));
//	{
//		MenuBuilder.AddMenuEntry(Actions.ShowLocalAxesAll);
//		MenuBuilder.AddMenuEntry(Actions.ShowLocalAxesSelected);
//		MenuBuilder.AddMenuEntry(Actions.ShowLocalAxesNone);
//	}
//	MenuBuilder.EndSection();
//}
//
//void SCustomizableObjectEditorViewportToolBar::FillShowClothingMenu(FMenuBuilder& MenuBuilder) const
//{
//}
//
//void SCustomizableObjectEditorViewportToolBar::FillShowDisplayInfoMenu(FMenuBuilder& MenuBuilder) const
//{
//	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();
//
//	// display info levels
//	{
//		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoBasic);
//		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoDetailed);
//		MenuBuilder.AddMenuEntry(Actions.ShowDisplayInfoSkelControls);
//		MenuBuilder.AddMenuEntry(Actions.HideDisplayInfo);
//	}
//}

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
			.Style(ToolBarStyle)
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
	//const FCustomizableObjectEditorViewportPlaybackCommands& Actions = FCustomizableObjectEditorViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder PlaybackMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	//{
	//	// View modes
	//	{
	//		PlaybackMenuBuilder.BeginSection("AnimViewportPlaybackSpeed", LOCTEXT("PlaybackMenu_SpeedLabel", "Playback Speed"));
	//		{
	//			for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
	//			{
	//				PlaybackMenuBuilder.AddMenuEntry(Actions.PlaybackSpeedCommands[i]);
	//			}
	//		}
	//		PlaybackMenuBuilder.EndSection();
	//	}
	//}

	return PlaybackMenuBuilder.MakeWidget();

}

void SCustomizableObjectEditorViewportToolBar::GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const
{
	//const FCustomizableObjectEditorViewportPlaybackCommands& Actions = FCustomizableObjectEditorViewportPlaybackCommands::Get();

	//const bool bInShouldCloseWindowAfterMenuSelection = true;

	//MenuBuilder.BeginSection("AnimViewportTurnTableMode", LOCTEXT("TurnTableMenu_ModeLabel", "Turn Table Mode"));
	//{
	//	MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePlay);
	//	MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePause);
	//	MenuBuilder.AddMenuEntry(Actions.PersonaTurnTableStop);
	//}
	//MenuBuilder.EndSection();

	//MenuBuilder.BeginSection("AnimViewportTurnTableSpeed", LOCTEXT("TurnTableMenu_SpeedLabel", "Turn Table Speed"));
	//{
	//	for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
	//	{
	//		MenuBuilder.AddMenuEntry(Actions.TurnTableSpeeds[i]);
	//	}
	//}
	//MenuBuilder.EndSection();
}

void SCustomizableObjectEditorViewportToolBar::GenerateSceneSetupMenu(FMenuBuilder& MenuBuilder)
{
	//FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	//FDetailsViewArgs Args(false, false, false, FDetailsViewArgs::HideNameArea, true, nullptr, false, "PersonaPreviewSceneDescription");
	//Args.bShowScrollBar = false;

	//TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(Args);

	//DetailsView->RegisterInstancedCustomPropertyLayout(UPersonaPreviewSceneDescription::StaticClass(), FOnGetDetailCustomizationInstance::CreateSP(this, &SCustomizableObjectEditorViewportToolBar::CustomizePreviewSceneDescription));
	//PropertyEditorModule.RegisterCustomPropertyTypeLayout(FPreviewMeshCollectionEntry::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateSP(this, &SCustomizableObjectEditorViewportToolBar::CustomizePreviewMeshCollectionEntry), nullptr, DetailsView);

	//DetailsView->SetObject(StaticCastSharedRef<FAnimationEditorPreviewScene>(Viewport.Pin()->GetPreviewScene())->GetPreviewSceneDescription());

	//MenuBuilder.AddWidget(DetailsView, FText(), true);
}

TSharedRef<SWidget> SCustomizableObjectEditorViewportToolBar::GenerateCameraModeMenu() const
{
	const FCustomizableObjectEditorViewportLODCommands& Actions = FCustomizableObjectEditorViewportLODCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList());
	{
		// LOD Models
		ShowMenuBuilder.BeginSection("NAME_None");
		{
			ShowMenuBuilder.AddMenuEntry(Actions.OrbitalCamera);
			ShowMenuBuilder.AddMenuEntry(Actions.FreeCamera);
		}
		ShowMenuBuilder.EndSection();
	}

	return ShowMenuBuilder.MakeWidget();
}

FText SCustomizableObjectEditorViewportToolBar::GetCameraModeMenuLabel() const
{
	FText Label = FText::FromString("Camera Mode");
	if (Viewport.IsValid())
	{
		bool CameraMode = Viewport.Pin()->GetViewportClient()->IsOrbitalCameraActive();

		if (CameraMode)
		{
			Label = FText::FromString("Camera Mode: Orbital");
		}
		else
		{
			Label = FText::FromString("Camera Mode: Free");
		}
	}
	return Label;
}

//TSharedRef<class IDetailCustomization> SCustomizableObjectEditorViewportToolBar::CustomizePreviewSceneDescription()
//{
//	return MakeShareable(new FPreviewSceneDescriptionCustomization(FAssetData(&Viewport.Pin()->GetSkeletonTree()->GetEditableSkeleton()->GetSkeleton()).GetExportTextName(), Viewport.Pin()->GetPreviewScene()->GetPersonaToolkit()));
//}
//
//TSharedRef<class IPropertyTypeCustomization> SCustomizableObjectEditorViewportToolBar::CustomizePreviewMeshCollectionEntry()
//{
//	return MakeShareable(new FPreviewMeshCollectionEntryCustomization(Viewport.Pin()->GetPreviewScene()));
//}

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
	FText Label = LOCTEXT("PlaybackError", "Error");
	//if (Viewport.IsValid())
	//{
	//	for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
	//	{
	//		if (Viewport.Pin()->IsPlaybackSpeedSelected(i))
	//		{
	//			Label = FText::FromString(FString::Printf(
	//				(i == EAnimationPlaybackSpeeds::Quarter) ? TEXT("x%.2f") : TEXT("x%.1f"),
	//				EAnimationPlaybackSpeeds::Values[i]
	//			));
	//			break;
	//		}
	//	}
	//}
	return Label;
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

	return FAppStyle::GetBrush(Icon);
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

void SCustomizableObjectEditorViewportToolBar::OnFOVValueCommitted(float NewValue, ETextCommit::Type CommitInfo)
{
	//OnFOVValueChanged will be called... nothing needed here.
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
				.Font( FAppStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.Value( this, &SCustomizableObjectEditorViewportToolBar::OnGetFOVValue )
				.OnValueChanged( this, &SCustomizableObjectEditorViewportToolBar::OnFOVValueChanged )
			]
		];
}


#undef LOCTEXT_NAMESPACE
