// Copyright Epic Games, Inc. All Rights Reserved.


#include "SAnimViewportToolBar.h"

#include "AnimPreviewInstance.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "EngineGlobals.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Engine.h"
#include "Styling/AppStyle.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Preferences/PersonaOptions.h"
#include "EditorViewportCommands.h"
#include "AnimViewportMenuCommands.h"
#include "AnimViewportShowCommands.h"
#include "AnimViewportLODCommands.h"
#include "AnimViewportPlaybackCommands.h"
#include "SAnimPlusMinusSlider.h"
#include "SEditorViewportToolBarMenu.h"
#include "STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "AssetViewerSettings.h"
#include "PersonaPreviewSceneDescription.h"
#include "Engine/PreviewMeshCollection.h"
#include "PreviewSceneCustomizations.h"
#include "ClothingSimulation.h"
#include "SimulationEditorExtender.h"
#include "ClothingSimulationFactory.h"
#include "ClothingSystemEditorInterfaceModule.h"
#include "Widgets/SWidget.h"
#include "Types/ISlateMetaData.h"
#include "Textures/SlateIcon.h"
#include "ShowFlagMenuCommands.h"
#include "BufferVisualizationMenuCommands.h"
#include "IPinnedCommandList.h"
#include "UICommandList_Pinnable.h"
#include "BoneSelectionWidget.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Animation/MirrorDataTable.h"
#include "ScopedTransaction.h"
#include "SNameComboBox.h"

#define LOCTEXT_NAMESPACE "AnimViewportToolBar"

//Class definition which represents widget to modify strength of wind for clothing
class SClothWindSettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SClothWindSettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget from its declaration */
	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		this->ChildSlot
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SNumericEntryBox<float>)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.ToolTipText(LOCTEXT("WindStrength_ToolTip", "Change wind strength"))
					.MinValue(0)
					.AllowSpin(true)
					.MinSliderValue(0)
					.MaxSliderValue(10)
					.Value(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetWindStrengthSliderValue)
					.OnValueChanged(SSpinBox<float>::FOnValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetWindStrength))
				]
			]
		];
	}

protected:

	/** Callback function which determines whether this widget is enabled */
	bool IsWindEnabled() const
	{
		return AnimViewportPtr.Pin()->IsApplyingClothWind();
	}

protected:
	/** The viewport hosting this widget */
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify gravity for preview
class SGravitySettings : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SGravitySettings)
	{}

		SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		this->ChildSlot
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SSpinBox<float>)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.ToolTipText(LOCTEXT("GravityScale_ToolTip", "Change gravity scale"))
					.MinValue(0)
					.MaxValue(4)
					.Value(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetGravityScaleSliderValue)
					.OnValueChanged(SSpinBox<float>::FOnValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetGravityScale))
				]
			]
		];
	}

protected:
	FReply OnDecreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() - DeltaValue );
		return FReply::Handled();
	}

	FReply OnIncreaseGravityScale()
	{
		const float DeltaValue = 0.025f;
		AnimViewportPtr.Pin()->SetGravityScale( AnimViewportPtr.Pin()->GetGravityScaleSliderValue() + DeltaValue );
		return FReply::Handled();
	}

protected:
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

//Class definition which represents widget to modify Bone Draw Size in viewport
class SBoneDrawSizeSetting : public SCompoundWidget
{
	
public:

	SLATE_BEGIN_ARGS(SBoneDrawSizeSetting) {}
	SLATE_ARGUMENT(TWeakPtr<SAnimationEditorViewportTabBody>, AnimEditorViewport)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs )
	{
		AnimViewportPtr = InArgs._AnimEditorViewport;

		this->ChildSlot
		[
			SNew(SBox)
			.HAlign(HAlign_Right)
			[
				SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.WidthOverride(100.0f)
				[
					SNew(SSpinBox<float>)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.ToolTipText(LOCTEXT("BoneDrawSize_ToolTip", "Change bone size in viewport."))
					.MinValue(0)
					.MaxSliderValue(10)
					.SupportDynamicSliderMaxValue(true)
					.Value(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::GetBoneDrawSize)
					.OnValueChanged(SSpinBox<float>::FOnValueChanged::CreateSP(AnimViewportPtr.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::SetBoneDrawSize))
				]
			]
		];
	}

protected:
	TWeakPtr<SAnimationEditorViewportTabBody> AnimViewportPtr;
};

///////////////////////////////////////////////////////////
// SAnimViewportToolBar

void SAnimViewportToolBar::Construct(const FArguments& InArgs, TSharedPtr<class SAnimationEditorViewportTabBody> InViewport, TSharedPtr<class SEditorViewport> InRealViewport)
{
	bShowShowMenu = InArgs._ShowShowMenu;
	bShowCharacterMenu= InArgs._ShowCharacterMenu;
	bShowLODMenu = InArgs._ShowLODMenu;
	bShowPlaySpeedMenu = InArgs._ShowPlaySpeedMenu;
	bShowFloorOptions = InArgs._ShowFloorOptions;
	bShowTurnTable = InArgs._ShowTurnTable;
	bShowPhysicsMenu = InArgs._ShowPhysicsMenu;

	CommandList = InRealViewport->GetCommandList();
	Extenders = InArgs._Extenders;
	Extenders.Add(GetViewMenuExtender(InRealViewport));

	// If we have no extender, make an empty one
	if (Extenders.Num() == 0)
	{
		Extenders.Add(MakeShared<FExtender>());
	}

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	const FMargin ToolbarButtonPadding(4.0f, 1.0f);

	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ViewMenuTooltip", "View Options.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Image("EditorViewportToolBar.OptionsDropdown")
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewMenu)
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ViewportMenuTooltip", "Viewport Options. Use this to switch between different orthographic or perspective views."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(this, &SAnimViewportToolBar::GetCameraMenuLabel)
			.LabelIcon( this, &SAnimViewportToolBar::GetCameraMenuLabelIcon )
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.CameraMenu")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateViewportTypeMenu)
		]
		// View menu (lit, unlit, etc...)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportViewMenu, InRealViewport.ToSharedRef(), SharedThis(this))
			.ToolTipText(LOCTEXT("ViewModeMenuTooltip", "View Mode Options. Use this to change how the view is rendered, e.g. Lit/Unlit."))
			.MenuExtenders(FExtender::Combine(Extenders))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("ShowMenuTooltip", "Show Options. Use this enable/disable the rendering of types of scene elements."))
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(LOCTEXT("ShowMenu", "Show"))
			.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewMenuButton")))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateShowMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("PhysicsMenuTooltip", "Physics Options. Use this to control the physics of the scene."))
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("Physics", "Physics"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePhysicsMenu)
			.Visibility(bShowPhysicsMenu ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("CharacterMenuTooltip", "Character Options. Control character-related rendering options.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(LOCTEXT("Character", "Character"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateCharacterMenu)
			.Visibility(bShowCharacterMenu ? EVisibility::Visible : EVisibility::Collapsed)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			//LOD
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("LODMenuTooltip", "LOD Options. Control how LODs are displayed.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetLODMenuLabel)
			.OnGetMenuContent(this, &SAnimViewportToolBar::GenerateLODMenu)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ToolTipText(LOCTEXT("PlaybackSpeedMenuTooltip", "Playback Speed Options. Control the time dilation of the scene's update.\nShift-clicking items will 'pin' them to the toolbar."))
			.ParentToolBar(SharedThis(this))
			.Label(this, &SAnimViewportToolBar::GetPlaybackMenuLabel)
			.LabelIcon(FAppStyle::GetBrush("AnimViewportMenu.PlayBackSpeed"))
			.OnGetMenuContent(this, &SAnimViewportToolBar::GeneratePlaybackMenu)
		]
		+ SHorizontalBox::Slot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			SNew(STransformViewportToolBar)
			.Viewport(InRealViewport)
			.CommandList(InRealViewport->GetCommandList())
			.Visibility(this, &SAnimViewportToolBar::GetTransformToolbarVisibility)
			.OnCamSpeedChanged(this, &SAnimViewportToolBar::OnCamSpeedChanged)
			.OnCamSpeedScalarChanged(this, &SAnimViewportToolBar::OnCamSpeedScalarChanged)
		];
			
	
	//@TODO: Need clipping horizontal box: LeftToolbar->AddWrapButton();

	// Create our pinned commands before we bind commands
	IPinnedCommandListModule& PinnedCommandListModule = FModuleManager::LoadModuleChecked<IPinnedCommandListModule>(TEXT("PinnedCommandList"));
	PinnedCommands = PinnedCommandListModule.CreatePinnedCommandList((InArgs._ContextName != NAME_None) ? InArgs._ContextName : TEXT("PersonaViewport"));
	PinnedCommands->SetStyle(&FAppStyle::Get(), TEXT("ViewportPinnedCommandList"));

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
			.Cursor(EMouseCursor::Default)
			[
				LeftToolbar
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			PinnedCommands.ToSharedRef()
		]
		+SVerticalBox::Slot()
		.Padding(FMargin(4.0f, 3.0f, 0.0f, 0.0f))
		[
			// Display text (e.g., item being previewed)
			SNew(SRichTextBlock)
			.DecoratorStyleSet(&FAppStyle::Get())
			.Text(InViewport.Get(), &SAnimationEditorViewportTabBody::GetDisplayString)
			.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("AnimViewport.MessageText"))
		]
	];
	
	SViewportToolBar::Construct(SViewportToolBar::FArguments());

	// Register all the custom widgets we can use here
	PinnedCommands->RegisterCustomWidget(IPinnedCommandList::FOnGenerateCustomWidget::CreateSP(this, &SAnimViewportToolBar::MakeFloorOffsetWidget), TEXT("FloorOffsetWidget"), LOCTEXT("FloorHeightOffset", "Floor Height Offset"));
	PinnedCommands->RegisterCustomWidget(IPinnedCommandList::FOnGenerateCustomWidget::CreateSP(this, &SAnimViewportToolBar::MakeFOVWidget), TEXT("FOVWidget"), LOCTEXT("Viewport_FOVLabel", "Field Of View"));

	PinnedCommands->BindCommandList(InViewport->GetCommandList().ToSharedRef());

	// We assign the viewport pointer here rather than initially, as SViewportToolbar::Construct 
	// ends up calling through and attempting to perform operations on the not-yet-full-constructed viewport
	Viewport = InViewport;
}

EVisibility SAnimViewportToolBar::GetTransformToolbarVisibility() const
{
	return Viewport.Pin()->CanUseGizmos() ? EVisibility::Visible : EVisibility::Hidden;
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFloorOffsetWidget() const
{
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SSpinBox<float>)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.MinSliderValue(-100.0f)
				.MaxSliderValue(100.0f)
				.Value(this, &SAnimViewportToolBar::OnGetFloorOffset)
				.OnBeginSliderMovement(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnBeginSliderMovementFloorOffset)
				.OnValueChanged(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFloorOffsetChanged)
				.OnValueCommitted(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFloorOffsetCommitted)
				.ToolTipText(LOCTEXT("FloorOffsetToolTip", "Height offset for the floor mesh (stored per-mesh)"))
			]
		];
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFOVWidget() const
{
	const float FOVMin = 5.f;
	const float FOVMax = 170.f;

	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew(SNumericEntryBox<float>)
				.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
				.AllowSpin(true)
				.MinValue(FOVMin)
				.MaxValue(FOVMax)
				.MinSliderValue(FOVMin)
				.MaxSliderValue(FOVMax)
				.Value(this, &SAnimViewportToolBar::OnGetFOVValue)
				.OnValueChanged(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFOVValueChanged)
				.OnValueCommitted(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnFOVValueCommitted)
			]
		];
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFollowBoneComboWidget() const
{
	TSharedRef<SComboButton> ComboButton = SNew(SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), "ViewportPinnedCommandList.ComboButton")
		.ContentPadding(0.0f)
		.ButtonContent()
		[
			SNew(STextBlock)
			.TextStyle(FAppStyle::Get(), "ViewportPinnedCommandList.Label")
			.Text_Lambda([this]()
			{ 
				const FName BoneName = Viewport.Pin()->GetCameraFollowBoneName();
				if(BoneName != NAME_None)
				{
					return FText::Format(LOCTEXT("FollowingBoneMenuTitleFormat", "Following Bone: {0}"), FText::FromName(BoneName));
				}
				else
				{
					return LOCTEXT("FollowBoneMenuTitle", "Focus On Bone");
				}
			})
		];

	TWeakPtr<SComboButton> WeakComboButton = ComboButton;
	ComboButton->SetOnGetMenuContent(FOnGetContent::CreateSP(this, &SAnimViewportToolBar::MakeFollowBoneWidget, WeakComboButton));

	return ComboButton;
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFollowBoneWidget() const
{
	return MakeFollowBoneWidget(nullptr);
}

TSharedRef<SWidget> SAnimViewportToolBar::MakeFollowBoneWidget(TWeakPtr<SComboButton> InWeakComboButton) const
{
	TSharedPtr<SBoneTreeMenu> BoneTreeMenu;

	TSharedRef<SWidget> MenuWidget =
		SNew(SBox)
		.MaxDesiredHeight(400.0f)
		[
			SAssignNew(BoneTreeMenu, SBoneTreeMenu)
			.bShowVirtualBones(true)
			.OnBoneSelectionChanged_Lambda([this](FName InBoneName)
			{
				Viewport.Pin()->SetCameraFollowMode(EAnimationViewportCameraFollowMode::Bone, InBoneName);
				FSlateApplication::Get().DismissAllMenus();

				PinnedCommands->AddCustomWidget(TEXT("FollowBoneWidget"));
			})
			.SelectedBone(Viewport.Pin()->GetCameraFollowBoneName())
			.OnGetReferenceSkeleton_Lambda([this]() -> const FReferenceSkeleton&
			{
				USkeletalMesh* PreviewMesh = Viewport.Pin()->GetPreviewScene()->GetPreviewMesh();
				if (PreviewMesh)
				{
					return PreviewMesh->GetRefSkeleton();
				}

				static FReferenceSkeleton EmptySkeleton;
				return EmptySkeleton;
			})
		];

	if(InWeakComboButton.IsValid())
	{
		InWeakComboButton.Pin()->SetMenuContentWidgetToFocus(BoneTreeMenu->GetFilterTextWidget());
	}

	return MenuWidget;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewMenu() const
{
	const FAnimViewportMenuCommands& Actions = FAnimViewportMenuCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	InMenuBuilder.BeginSection("AnimViewportSceneSetup", LOCTEXT("ViewMenu_SceneSetupLabel", "Scene Setup"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().PreviewSceneSettings);
		InMenuBuilder.PopCommandList();

		if(bShowFloorOptions)
		{
			InMenuBuilder.AddWidget(MakeFloorOffsetWidget(), LOCTEXT("FloorHeightOffset", "Floor Height Offset"));

			InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
			InMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().AutoAlignFloorToMesh);
			InMenuBuilder.PopCommandList();
		}
			
		if (bShowTurnTable)
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("TurnTableLabel", "Turn Table"),
				LOCTEXT("TurnTableTooltip", "Set up auto-rotation of preview."),
				FNewMenuDelegate::CreateRaw(this, &SAnimViewportToolBar::GenerateTurnTableMenu),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.TurnTableSpeed")
				);
		}
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportCamera", LOCTEXT("ViewMenu_CameraLabel", "Camera"));
	{
		InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().FocusViewportToSelection);
		InMenuBuilder.AddWidget(MakeFOVWidget(), LOCTEXT("Viewport_FOVLabel", "Field Of View"));
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().TogglePauseAnimationOnCameraMove);

		InMenuBuilder.AddSubMenu(
			LOCTEXT("CameraFollowModeLabel", "Follow Mode"),
			LOCTEXT("CameraFollowModeTooltip", "Set various camera follow modes"),
			FNewMenuDelegate::CreateLambda([this](FMenuBuilder& InSubMenuBuilder)
			{
				InSubMenuBuilder.BeginSection("AnimViewportCameraFollowMode", LOCTEXT("ViewMenu_CameraFollowModeLabel", "Camera Follow Mode"));
				{
					InSubMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());

					InSubMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollowNone);
					InSubMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollowRoot);
					InSubMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().CameraFollowBounds);
					InSubMenuBuilder.AddSubMenu(
						LOCTEXT("CameraFollowBone_DisplayName", "Orbit Bone"),
						LOCTEXT("CameraFollowBone_ToolTip", "Select a bone for the camera to follow and orbit around"),
						FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder)
						{
							SubMenuBuilder.BeginSection("CameraFollowModeBoneSubmenu", LOCTEXT("CameraFollowModeBoneSubmenu_Label", "Follow Bone Options"));
							SubMenuBuilder.AddWidget(MakeFollowBoneWidget(), FText());
							SubMenuBuilder.AddMenuEntry(
								LOCTEXT("LockRotation_DisplayName", "Lock Rotation"),
								LOCTEXT("LockRotation_ToolTip", "Keep viewport camera rotation aligned to the orbited bone."),
								FSlateIcon(),
								FUIAction(
									FExecuteAction::CreateLambda([this]() {
										Viewport.Pin()->ToggleRotateCameraToFollowBone();
									}),
									FCanExecuteAction(),
									FIsActionChecked::CreateLambda([this]()
									{
										return Viewport.Pin()->GetShouldRotateCameraToFollowBone();
									})
								),
								NAME_None,
								EUserInterfaceActionType::ToggleButton
							);
							SubMenuBuilder.EndSection();
						}),
						FUIAction(
							FExecuteAction(),
							FCanExecuteAction::CreateLambda([this]()
							{
								return Viewport.Pin()->CanChangeCameraMode();
							}),
							FIsActionChecked::CreateLambda([this]()
							{
								return Viewport.Pin()->IsCameraFollowEnabled(EAnimationViewportCameraFollowMode::Bone);
							})
						),
						"CameraFollowBone",
						EUserInterfaceActionType::RadioButton,
						/* bInOpenSubMenuOnClick = */ false,
						FSlateIcon()
					);
					InSubMenuBuilder.PopCommandList();
				}
				InSubMenuBuilder.EndSection();
			}),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "AnimViewportMenu.CameraFollow")
			);
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("AnimViewportDefaultCamera", LOCTEXT("ViewMenu_DefaultCameraLabel", "Default Camera"));
	{
		InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().JumpToDefaultCamera);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SaveCameraAsDefault);
		InMenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().ClearDefaultCamera);
		InMenuBuilder.PopCommandList();
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePhysicsMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	{
		InMenuBuilder.BeginSection("AnimViewportPhysicsMenu", LOCTEXT("ViewMenu_AnimViewportPhysicsMenu", "Physics Menu"));
		InMenuBuilder.EndSection();
	}
	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();
	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateCharacterMenu() const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	{
		InMenuBuilder.BeginSection("AnimViewportSceneElements", LOCTEXT("CharacterMenu_SceneElements", "Scene Elements"));
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("CharacterMenu_MeshSubMenu", "Mesh"),
				LOCTEXT("CharacterMenu_MeshSubMenuToolTip", "Mesh-related options"),
				FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("AnimViewportMesh", LOCTEXT("CharacterMenu_Actions_Mesh", "Mesh"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowRetargetBasePose );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBound );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().UseInGameBound);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().UseFixedBounds);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().UsePreSkinnedBounds);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowPreviewMesh );
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargets );
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportMeshInfo", LOCTEXT("CharacterMenu_Actions_MeshInfo", "Mesh Info"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoBasic);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoDetailed);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowDisplayInfoSkelControls);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().HideDisplayInfo);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportPreviewOverlayDraw", LOCTEXT("CharacterMenu_Actions_Overlay", "Mesh Overlay Drawing"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowOverlayNone);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneWeight);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowMorphTargetVerts);
					}
					SubMenuBuilder.EndSection();
				})
			);

			InMenuBuilder.AddSubMenu(
				LOCTEXT("CharacterMenu_AnimationSubMenu", "Animation"),
				LOCTEXT("CharacterMenu_AnimationSubMenuToolTip", "Animation-related options"),
				FNewMenuDelegate::CreateLambda([this, WeakSharedViewport = Viewport](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("AnimViewportRootMotion", LOCTEXT("CharacterMenu_RootMotionLabel", "Root Motion"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().DoNotProcessRootMotion);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ProcessRootMotionLoop);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ProcessRootMotionLoopAndReset);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportAnimation", LOCTEXT("CharacterMenu_Actions_AnimationAsset", "Animation"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowRawAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowNonRetargetedAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAdditiveBaseBones);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSourceRawAnimation);

						if (WeakSharedViewport.IsValid())
						{
							if ( UDebugSkelMeshComponent* PreviewComponent = WeakSharedViewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent())
							{
								FUIAction DisableUnlessPreviewInstance(
									FExecuteAction::CreateLambda([](){}),
									FCanExecuteAction::CreateLambda([PreviewComponent]()
									{
										return (PreviewComponent->PreviewInstance && (PreviewComponent->PreviewInstance == PreviewComponent->GetAnimInstance() ) );
									})
								);

								SubMenuBuilder.AddSubMenu(
									LOCTEXT("CharacterMenu_AnimationSubMenu_MirrorSubMenu", "Mirror"),
									LOCTEXT("CharacterMenu_AnimationSubMenu_MirrorSubMenuToolTip", "Mirror the animation using the selected mirror data table"),
									FNewMenuDelegate::CreateRaw(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::FillCharacterMirrorMenu),
									DisableUnlessPreviewInstance,
									NAME_None,
									EUserInterfaceActionType::Button,
									false,
									FSlateIcon(),
									false);
							}
						}
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBakedAnimation);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().DisablePostProcessBlueprint);
					}
					SubMenuBuilder.EndSection();
					if ( WeakSharedViewport.IsValid())
					{
						SubMenuBuilder.BeginSection("SkinWeights", LOCTEXT("SkinWeights_Label", "Skin Weight Profiles"));
						{
							SubMenuBuilder.AddWidget(WeakSharedViewport.Pin()->SkinWeightCombo.ToSharedRef(), FText());
						}
						SubMenuBuilder.EndSection();
					}
				})
			);

			InMenuBuilder.AddSubMenu(
				LOCTEXT("CharacterMenu_BoneDrawSubMenu", "Bones"),
				LOCTEXT("CharacterMenu_BoneDrawSubMenuToolTip", "Bone Drawing Options"),
				FNewMenuDelegate::CreateLambda([this](FMenuBuilder& SubMenuBuilder)
				{
					SubMenuBuilder.BeginSection("BonesAndSockets", LOCTEXT("CharacterMenu_BonesAndSocketsLabel", "Show"));
					{
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowSockets);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowAttributes);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneNames);
					}
					SubMenuBuilder.EndSection();

					SubMenuBuilder.BeginSection("AnimViewportPreviewHierarchyBoneDraw", LOCTEXT("CharacterMenu_Actions_BoneDrawing", "Bone Drawing"));
					{
						TSharedPtr<SWidget> BoneSizeWidget = SNew(SBoneDrawSizeSetting).AnimEditorViewport(Viewport);
						SubMenuBuilder.AddWidget(BoneSizeWidget.ToSharedRef(), LOCTEXT("CharacterMenu_Actions_BoneDrawSize", "Bone Draw Size:"));
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawAll);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelected);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndParents);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndChildren);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawSelectedAndParentsAndChildren);
						SubMenuBuilder.AddMenuEntry(FAnimViewportShowCommands::Get().ShowBoneDrawNone);
					}
					SubMenuBuilder.EndSection();
				})
				);

			UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();

			if(PreviewComp)
			{
				InMenuBuilder.AddSubMenu(
					LOCTEXT("CharacterMenu_ClothingSubMenu", "Clothing"),
					LOCTEXT("CharacterMenu_ClothingSubMenuToolTip", "Options relating to clothing"),
					FNewMenuDelegate::CreateRaw(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::FillCharacterClothingMenu));
			}
		}

		InMenuBuilder.AddSubMenu(
			LOCTEXT("CharacterMenu_AudioSubMenu", "Audio"),
			LOCTEXT("CharacterMenu_AudioSubMenuToolTip", "Audio options"),
			FNewMenuDelegate::CreateLambda([&Actions](FMenuBuilder& SubMenuBuilder)
			{
				SubMenuBuilder.BeginSection("AnimViewportAudio", LOCTEXT("CharacterMenu_Audio", "Audio"));
				{
					SubMenuBuilder.AddMenuEntry(Actions.MuteAudio);
					SubMenuBuilder.AddMenuEntry(Actions.UseAudioAttenuation);
				}
				SubMenuBuilder.EndSection();
			}));

		InMenuBuilder.AddSubMenu(
			LOCTEXT("CharacterMenu_AdvancedSubMenu", "Advanced"),
			LOCTEXT("CharacterMenu_AdvancedSubMenuToolTip", "Advanced options"),
			FNewMenuDelegate::CreateRaw(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::FillCharacterAdvancedMenu));

		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

void SAnimViewportToolBar::FillCharacterAdvancedMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	// Draw UVs
	MenuBuilder.BeginSection("UVVisualization", LOCTEXT("UVVisualization_Label", "UV Visualization"));
	{
		MenuBuilder.AddWidget(Viewport.Pin()->UVChannelCombo.ToSharedRef(), FText());
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Skinning", LOCTEXT("Skinning_Label", "Skinning"));
	{
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetCPUSkinning);
	}
	MenuBuilder.EndSection();
	

	MenuBuilder.BeginSection("ShowVertex", LOCTEXT("ShowVertex_Label", "Vertex Normal Visualization"));
	{
		// Vertex debug flags
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowNormals);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowTangents);
		MenuBuilder.AddMenuEntry(FAnimViewportMenuCommands::Get().SetShowBinormals);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportPreviewHierarchyLocalAxes", LOCTEXT("ShowMenu_Actions_HierarchyAxes", "Hierarchy Local Axes") );
	{
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesAll );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesSelected );
		MenuBuilder.AddMenuEntry( Actions.ShowLocalAxesNone );
	}
	MenuBuilder.EndSection();
}

void SAnimViewportToolBar::FillCharacterMirrorMenu(FMenuBuilder& MenuBuilder) const
{
	FAssetPickerConfig AssetPickerConfig;
	UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();
	USkeletalMesh* Mesh = PreviewComp->GetSkeletalMeshAsset();
	UAnimPreviewInstance* PreviewInstance = PreviewComp->PreviewInstance; 
	if (Mesh && PreviewInstance)
	{
		USkeleton* Skeleton = Mesh->GetSkeleton();
	
		AssetPickerConfig.Filter.ClassPaths.Add(UMirrorDataTable::StaticClass()->GetClassPathName());
		AssetPickerConfig.Filter.bRecursiveClasses = false;
		AssetPickerConfig.bAllowNullSelection = true;
		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
		AssetPickerConfig.InitialAssetSelection = FAssetData(PreviewInstance->GetMirrorDataTable());
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(const_cast<SAnimViewportToolBar*>(this), &SAnimViewportToolBar::OnMirrorDataTableSelected);
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.ThumbnailScale = 0.1f;
		AssetPickerConfig.bAddFilterUI = false;
		
		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		MenuBuilder.AddWidget(
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig),
			FText::GetEmpty()
		);
	}
}

void SAnimViewportToolBar::OnMirrorDataTableSelected(const FAssetData& SelectedMirrorTableData)
{
	UMirrorDataTable* MirrorDataTable = Cast<UMirrorDataTable>(SelectedMirrorTableData.GetAsset());
	if (Viewport.Pin().IsValid())
	{
		UDebugSkelMeshComponent* PreviewComp = Viewport.Pin()->GetPreviewScene()->GetPreviewMeshComponent();
		USkeletalMesh* Mesh = PreviewComp->GetSkeletalMeshAsset();
		UAnimPreviewInstance* PreviewInstance = PreviewComp->PreviewInstance; 
		if (Mesh && PreviewInstance)
		{
			PreviewInstance->SetMirrorDataTable(MirrorDataTable);
		}
	}
}

void SAnimViewportToolBar::FillCharacterClothingMenu(FMenuBuilder& MenuBuilder)
{
	const FAnimViewportShowCommands& Actions = FAnimViewportShowCommands::Get();

	MenuBuilder.BeginSection("ClothPreview", LOCTEXT("ClothPreview_Label", "Simulation"));
	{
		MenuBuilder.AddMenuEntry(Actions.EnableClothSimulation);
		MenuBuilder.AddMenuEntry(Actions.ResetClothSimulation);

		TSharedPtr<SWidget> WindWidget = SNew(SClothWindSettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(WindWidget.ToSharedRef(), LOCTEXT("ClothPreview_WindStrength", "Wind Strength:"));

		TSharedPtr<SWidget> GravityWidget = SNew(SGravitySettings).AnimEditorViewport(Viewport);
		MenuBuilder.AddWidget(GravityWidget.ToSharedRef(), LOCTEXT("ClothPreview_GravityScale", "Gravity Scale:"));

		MenuBuilder.AddMenuEntry(Actions.EnableCollisionWithAttachedClothChildren);
		MenuBuilder.AddMenuEntry(Actions.PauseClothWithAnim);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("ClothAdditionalVisualization", LOCTEXT("ClothAdditionalVisualization_Label", "Sections Display Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowAllSections);
		MenuBuilder.AddMenuEntry(Actions.ShowOnlyClothSections);
		MenuBuilder.AddMenuEntry(Actions.HideOnlyClothSections);
	}
	MenuBuilder.EndSection();

	// Call into the clothing editor module to customize the menu (this is mainly for debug visualizations and sim-specific options)
	TSharedPtr<SAnimationEditorViewportTabBody> SharedViewport = Viewport.Pin();
	if(SharedViewport.IsValid())
	{
		TSharedRef<IPersonaPreviewScene> PreviewScene = SharedViewport->GetAnimationViewportClient()->GetPreviewScene();
		if(UDebugSkelMeshComponent* PreviewComponent = PreviewScene->GetPreviewMeshComponent())
		{
			FClothingSystemEditorInterfaceModule& ClothingEditorModule = FModuleManager::LoadModuleChecked<FClothingSystemEditorInterfaceModule>(TEXT("ClothingSystemEditorInterface"));

			if(ISimulationEditorExtender* Extender = ClothingEditorModule.GetSimulationEditorExtender(PreviewComponent->ClothingSimulationFactory->GetFName()))
			{
				Extender->ExtendViewportShowMenu(MenuBuilder, PreviewScene);
			}
		}
	}

}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateShowMenu() const
{
	static const FName MenuName("Persona.AnimViewportToolBar");

	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		Menu->AddDynamicSection("AnimSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			// Only include helpful show flags.
			static const FShowFlagFilter ShowFlagFilter = FShowFlagFilter(FShowFlagFilter::ExcludeAllFlagsByDefault)
				// General
				.IncludeFlag(FEngineShowFlags::SF_AntiAliasing)
				.IncludeFlag(FEngineShowFlags::SF_Collision)
				.IncludeFlag(FEngineShowFlags::SF_Grid)
				.IncludeFlag(FEngineShowFlags::SF_Particles)
				.IncludeFlag(FEngineShowFlags::SF_Translucency)
				// Post Processing
				.IncludeFlag(FEngineShowFlags::SF_Bloom)
				.IncludeFlag(FEngineShowFlags::SF_DepthOfField)
				.IncludeFlag(FEngineShowFlags::SF_EyeAdaptation)
				.IncludeFlag(FEngineShowFlags::SF_HMDDistortion)
				.IncludeFlag(FEngineShowFlags::SF_MotionBlur)
				.IncludeFlag(FEngineShowFlags::SF_Tonemapper)
				// Lighting Components
				.IncludeGroup(SFG_LightingComponents)
				// Lighting Features
				.IncludeFlag(FEngineShowFlags::SF_AmbientCubemap)
				.IncludeFlag(FEngineShowFlags::SF_DistanceFieldAO)
				.IncludeFlag(FEngineShowFlags::SF_IndirectLightingCache)
				.IncludeFlag(FEngineShowFlags::SF_LightFunctions)
				.IncludeFlag(FEngineShowFlags::SF_LightShafts)
				.IncludeFlag(FEngineShowFlags::SF_ReflectionEnvironment)
				.IncludeFlag(FEngineShowFlags::SF_ScreenSpaceAO)
				.IncludeFlag(FEngineShowFlags::SF_ContactShadows)
				.IncludeFlag(FEngineShowFlags::SF_ScreenSpaceReflections)
				.IncludeFlag(FEngineShowFlags::SF_SubsurfaceScattering)
				.IncludeFlag(FEngineShowFlags::SF_TexturedLightProfiles)
				// Developer
				.IncludeFlag(FEngineShowFlags::SF_Refraction)
				// Advanced
				.IncludeFlag(FEngineShowFlags::SF_DeferredLighting)
				.IncludeFlag(FEngineShowFlags::SF_Selection)
				.IncludeFlag(FEngineShowFlags::SF_SeparateTranslucency)
				.IncludeFlag(FEngineShowFlags::SF_TemporalAA)
				.IncludeFlag(FEngineShowFlags::SF_VertexColors)
				.IncludeFlag(FEngineShowFlags::SF_MeshEdges)
				;

			FShowFlagMenuCommands::Get().BuildShowFlagsMenu(InMenu, ShowFlagFilter);
		}));
	}

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);
	FToolMenuContext MenuContext(CommandList, MenuExtender);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

FText SAnimViewportToolBar::GetLODMenuLabel() const
{
	FText Label = LOCTEXT("LODMenu_AutoLabel", "LOD Auto");
	if (Viewport.IsValid())
	{
		int32 LODSelectionType = Viewport.Pin()->GetLODSelection();

		if (Viewport.Pin()->IsTrackingAttachedMeshLOD())
		{
			Label = FText::Format(LOCTEXT("LODMenu_DebugLabel", "LOD Debug ({0})"), FText::AsNumber(LODSelectionType - 1));
		}
		else
		{
			if (LODSelectionType > 0)
			{
				FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODSelectionType - 1);
				Label = FText::FromString(TitleLabel);
			}
		}
	}
	return Label;
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateLODMenu() const
{
	const FAnimViewportLODCommands& Actions = FAnimViewportLODCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	{
		// LOD Models
		InMenuBuilder.BeginSection("AnimViewportPreviewLODs", LOCTEXT("ShowLOD_PreviewLabel", "Preview LODs") );
		{
			InMenuBuilder.AddMenuEntry( Actions.LODDebug );
			InMenuBuilder.AddMenuEntry( Actions.LODAuto );
			InMenuBuilder.AddMenuEntry( Actions.LOD0 );

			int32 LODCount = Viewport.Pin()->GetLODModelCount();
			for (int32 LODId = 1; LODId < LODCount; ++LODId)
			{
				FString TitleLabel = FString::Printf(TEXT("LOD %d"), LODId);

				FUIAction Action(FExecuteAction::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::OnSetLODModel, LODId + 1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(Viewport.Pin().ToSharedRef(), &SAnimationEditorViewportTabBody::IsLODModelSelected, LODId + 1));

				InMenuBuilder.AddMenuEntry(FText::FromString(TitleLabel), FText::GetEmpty(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::RadioButton);
			}
		}
		InMenuBuilder.EndSection();
	}

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GenerateViewportTypeMenu() const
{

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList, MenuExtender);
	InMenuBuilder.SetStyle(&FAppStyle::Get(), "Menu");
	InMenuBuilder.PushCommandList(CommandList.ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());

	// Camera types
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

	InMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
	InMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
	InMenuBuilder.EndSection();

	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SAnimViewportToolBar::GeneratePlaybackMenu() const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder InMenuBuilder(bInShouldCloseWindowAfterMenuSelection, Viewport.Pin()->GetCommandList(), MenuExtender);

	InMenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	InMenuBuilder.PushExtender(MenuExtender.ToSharedRef());
	{
		// View modes
		{
			InMenuBuilder.BeginSection("AnimViewportPlaybackSpeed", LOCTEXT("PlaybackMenu_SpeedLabel", "Playback Speed") );
			{
				for(int32 PlaybackSpeedIndex = 0; PlaybackSpeedIndex < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++PlaybackSpeedIndex)
				{
					InMenuBuilder.AddMenuEntry( Actions.PlaybackSpeedCommands[PlaybackSpeedIndex] );
				}
			}
			InMenuBuilder.EndSection();
		}
	}
	InMenuBuilder.PopCommandList();
	InMenuBuilder.PopExtender();

	return InMenuBuilder.MakeWidget();
}

void SAnimViewportToolBar::GenerateTurnTableMenu(FMenuBuilder& MenuBuilder) const
{
	const FAnimViewportPlaybackCommands& Actions = FAnimViewportPlaybackCommands::Get();

	const bool bInShouldCloseWindowAfterMenuSelection = true;

	MenuBuilder.PushCommandList(Viewport.Pin()->GetCommandList().ToSharedRef());
	MenuBuilder.BeginSection("AnimViewportTurnTableMode", LOCTEXT("TurnTableMenu_ModeLabel", "Turn Table Mode"));
	{
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePlay);
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTablePause);
		MenuBuilder.AddMenuEntry(Actions.PersonaTurnTableStop);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AnimViewportTurnTableSpeed", LOCTEXT("TurnTableMenu_SpeedLabel", "Turn Table Speed"));
	{
		for (int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			MenuBuilder.AddMenuEntry(Actions.TurnTableSpeeds[i]);
		}
	}
	MenuBuilder.EndSection();
	MenuBuilder.PopCommandList();
}

FSlateColor SAnimViewportToolBar::GetFontColor() const
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
		FLinearColor Color = Settings->Profiles[ProfileIndex].EnvironmentColor * Settings->Profiles[ProfileIndex].EnvironmentIntensity;

		// see if it's dark, if V is less than 0.2
		if (Color.B < 0.3f )
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

FText SAnimViewportToolBar::GetPlaybackMenuLabel() const
{
	FText Label = LOCTEXT("PlaybackError", "Error");
	if (Viewport.IsValid())
	{
		for(int i = 0; i < EAnimationPlaybackSpeeds::NumPlaybackSpeeds; ++i)
		{
			if (Viewport.Pin()->IsPlaybackSpeedSelected(i))
			{
				int32 NumFractionalDigits = (i == EAnimationPlaybackSpeeds::Quarter) ? 2 : 1;

				const FNumberFormattingOptions FormatOptions = FNumberFormattingOptions()
					.SetMinimumFractionalDigits(NumFractionalDigits)
					.SetMaximumFractionalDigits(NumFractionalDigits);

				Label = FText::Format(LOCTEXT("AnimViewportPlaybackMenuLabel", "x{0}"), FText::AsNumber(EAnimationPlaybackSpeeds::Values[i], &FormatOptions));
			}
		}
	}
	return Label;
}

FText SAnimViewportToolBar::GetCameraMenuLabel() const
{
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport(Viewport.Pin());
	if( PinnedViewport.IsValid() )
	{
		return GetCameraMenuLabelFromViewportType( PinnedViewport->GetLevelViewportClient().ViewportType );
	}

	return LOCTEXT("Viewport_Default", "Camera");
}

const FSlateBrush* SAnimViewportToolBar::GetCameraMenuLabelIcon() const
{
	TSharedPtr< SAnimationEditorViewportTabBody > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		return GetCameraMenuLabelIconFromViewportType(PinnedViewport->GetLevelViewportClient().ViewportType );
	}

	return FAppStyle::Get().GetBrush("NoBrush");
}

TOptional<float> SAnimViewportToolBar::OnGetFOVValue() const
{
	if(Viewport.IsValid())
	{
		return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
	}
	return 0.0f;
}

void SAnimViewportToolBar::OnFOVValueChanged( float NewValue )
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	ViewportClient.FOVAngle = NewValue;

	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetViewFOV(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();

	PinnedCommands->AddCustomWidget(TEXT("FOVWidget"));
}

void SAnimViewportToolBar::OnFOVValueCommitted( float NewValue, ETextCommit::Type CommitInfo )
{
	//OnFOVValueChanged will be called... nothing needed here.
}

void SAnimViewportToolBar::OnCamSpeedChanged(int32 NewValue)
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetCameraSpeed(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());
}

void SAnimViewportToolBar::OnCamSpeedScalarChanged(float NewValue)
{
	FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)(ViewportClient);
	AnimViewportClient.ConfigOption->SetCameraSpeedScalar(AnimViewportClient.GetAssetEditorToolkit()->GetEditorName(), NewValue, AnimViewportClient.GetViewportIndex());
}

float SAnimViewportToolBar::OnGetFloorOffset() const
{
	if(Viewport.IsValid())
	{
		FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();
		return AnimViewportClient.GetFloorOffset();
	}

	return 0.0f;
}

void SAnimViewportToolBar::OnBeginSliderMovementFloorOffset()
{
	// This value is saved in a UPROPERTY for the floor mesh, so changes are transactional
	PendingTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("SetFloorOffset", "Set Floor Offset"));
	PinnedCommands->AddCustomWidget(TEXT("FloorOffsetWidget"));
}

void SAnimViewportToolBar::OnFloorOffsetChanged( float NewValue )
{
	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	AnimViewportClient.SetFloorOffset( NewValue );

	PinnedCommands->AddCustomWidget(TEXT("FloorOffsetWidget"));
}

void SAnimViewportToolBar::OnFloorOffsetCommitted( float NewValue, ETextCommit::Type CommitType )
{
	if (!PendingTransaction)
	{
		// Create the transaction here if it doesn't already exist. This can happen when changes come via text entry to the slider.
		PendingTransaction = MakeUnique<FScopedTransaction>(LOCTEXT("SetFloorOffset", "Set Floor Offset"));
	}

	FAnimationViewportClient& AnimViewportClient = (FAnimationViewportClient&)Viewport.Pin()->GetLevelViewportClient();

	AnimViewportClient.SetFloorOffset( NewValue );

	PinnedCommands->AddCustomWidget(TEXT("FloorOffsetWidget"));

	PendingTransaction.Reset();
}

void SAnimViewportToolBar::AddMenuExtender(FName MenuToExtend, FMenuExtensionDelegate MenuBuilderDelegate)
{
	TSharedRef<FExtender> Extender(new FExtender());
	Extender->AddMenuExtension(
		MenuToExtend,
		EExtensionHook::After,
		CommandList,
		MenuBuilderDelegate
	);
	Extenders.Add(Extender);
}

TSharedRef<FExtender> SAnimViewportToolBar::GetViewMenuExtender(TSharedPtr<class SEditorViewport> InRealViewport)
{
	TSharedRef<FExtender> Extender(new FExtender());

	Extender->AddMenuExtension(
		TEXT("ViewMode"),
		EExtensionHook::After,
		InRealViewport->GetCommandList(),
		FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddSubMenu(
				LOCTEXT("VisualizeBufferViewModeDisplayName", "Buffer Visualization"),
				LOCTEXT("BufferVisualizationMenu_ToolTip", "Select a mode for buffer visualization"),
				FNewMenuDelegate::CreateStatic(&FBufferVisualizationMenuCommands::BuildVisualisationSubMenu),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([this]()
					{
						const TSharedPtr<SAnimationEditorViewportTabBody> ViewportPtr = Viewport.Pin();
						if (ViewportPtr.IsValid())
						{
							const FEditorViewportClient& ViewportClient = ViewportPtr->GetViewportClient();
							return ViewportClient.IsViewModeEnabled(VMI_VisualizeBuffer);
						}
						return false;
					})
				),
				"VisualizeBufferViewMode",
				EUserInterfaceActionType::RadioButton,
				/* bInOpenSubMenuOnClick = */ false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeBufferMode")
				);
		}));

	return Extender;
}

#undef LOCTEXT_NAMESPACE
