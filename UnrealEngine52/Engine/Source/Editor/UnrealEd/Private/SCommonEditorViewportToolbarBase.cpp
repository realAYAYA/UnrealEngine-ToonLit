// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCommonEditorViewportToolbarBase.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/STextComboBox.h"
#include "Styling/AppStyle.h"

#include "STransformViewportToolbar.h"
#include "EditorShowFlags.h"
#include "SEditorViewport.h"
#include "EditorViewportCommands.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportToolBarButton.h"
#include "SEditorViewportViewMenu.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Scalability.h"
#include "SceneView.h"
#include "SScalabilitySettings.h"
#include "AssetEditorViewportLayout.h"
#include "SAssetEditorViewport.h"

#define LOCTEXT_NAMESPACE "SCommonEditorViewportToolbarBase"

//////////////////////////////////////////////////////////////////////////
// SCommonEditorViewportToolbarBase

SCommonEditorViewportToolbarBase::~SCommonEditorViewportToolbarBase()
{
	if (PreviewProfileController)
	{
		PreviewProfileController->OnPreviewProfileListChanged().RemoveAll(this);
		PreviewProfileController->OnPreviewProfileChanged().RemoveAll(this);
	}
}


void SCommonEditorViewportToolbarBase::Construct(const FArguments& InArgs, TSharedPtr<class ICommonEditorViewportToolbarInfoProvider> InInfoProvider)
{
	InfoProviderPtr = InInfoProvider;
	PreviewProfileController = InArgs._PreviewProfileController;

	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	TSharedPtr<SHorizontalBox> MainBoxPtr;

	if (PreviewProfileController)
	{
		PreviewProfileController->OnPreviewProfileListChanged().AddRaw(this, &SCommonEditorViewportToolbarBase::UpdateAssetViewerProfileList);
		PreviewProfileController->OnPreviewProfileChanged().AddRaw(this, &SCommonEditorViewportToolbarBase::UpdateAssetViewerProfileSelection);
		UpdateAssetViewerProfileList();
	}

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);
	const FMargin ToolbarButtonPadding(4.0f, 0.0f);

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SAssignNew( MainBoxPtr, SHorizontalBox )
		]
	];

	// Options menu
	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Image("EditorViewportToolBar.OptionsDropdown")
			.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateOptionsMenu)
		];

	// Camera mode menu
	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Cursor(EMouseCursor::Default)
			.Label(this, &SCommonEditorViewportToolbarBase::GetCameraMenuLabel)
			.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateCameraMenu)
		];

	// View menu
	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			MakeViewMenu()
		];

	// Show menu
	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.Label(LOCTEXT("ShowMenuTitle", "Show"))
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(SharedThis(this))
			.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateShowMenu)
		];

	// Profile menu (Controls the Preview Scene Settings)
	if (InArgs._PreviewProfileController)
	{
		MainBoxPtr->AddSlot()
			.AutoWidth()
			.Padding(ToolbarSlotPadding)
			[
				MakeAssetViewerProfileComboBox()
			];
	}

	// Realtime button
	if (InArgs._AddRealtimeButton)
	{
		MainBoxPtr->AddSlot()
			.AutoWidth()
			.Padding(ToolbarSlotPadding)
			[
				SNew(SEditorViewportToolBarButton)
				.Cursor(EMouseCursor::Default)
				.ButtonType(EUserInterfaceActionType::Button)
				.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
				.OnClicked(this, &SCommonEditorViewportToolbarBase::OnRealtimeWarningClicked)
				.Visibility(this, &SCommonEditorViewportToolbarBase::GetRealtimeWarningVisibility)
				.ToolTipText(LOCTEXT("RealtimeOff_ToolTip", "This viewport is not updating in realtime.  Click to turn on realtime mode."))
				.Content()
				[
					SNew(STextBlock)
					.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
					.Text(LOCTEXT("RealtimeOff", "Realtime Off"))
				]
			];
	}

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			SNew(SEditorViewportToolbarMenu)
			.Label(LOCTEXT("ViewParamMenuTitle", "View Mode Options"))
			.Cursor(EMouseCursor::Default)
			.ParentToolBar(SharedThis(this))
			.Visibility(this, &SCommonEditorViewportToolbarBase::GetViewModeOptionsVisibility)
			.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GenerateViewModeOptionsMenu)
		];

	MainBoxPtr->AddSlot()
		.AutoWidth()
		.Padding(ToolbarSlotPadding)
		[
			// Button to show scalability warnings
			SNew(SEditorViewportToolbarMenu)
			.ParentToolBar(SharedThis(this))
			.Label(this, &SCommonEditorViewportToolbarBase::GetScalabilityWarningLabel)
			.MenuStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
			.OnGetMenuContent(this, &SCommonEditorViewportToolbarBase::GetScalabilityWarningMenuContent)
			.Visibility(this, &SCommonEditorViewportToolbarBase::GetScalabilityWarningVisibility)
			.ToolTipText(LOCTEXT("ScalabilityWarning_ToolTip", "Non-default scalability settings could be affecting what is shown in this viewport.\nFor example you may experience lower visual quality, reduced particle counts, and other artifacts that don't match what the scene would look like when running outside of the editor. Click to make changes."))
		];

	// Add optional toolbar slots to be added by child classes inherited from this common viewport toolbar
	ExtendLeftAlignedToolbarSlots(MainBoxPtr, SharedThis(this));

	// Transform toolbar
	MainBoxPtr->AddSlot()
		.Padding(ToolbarSlotPadding)
		.HAlign(HAlign_Right)
		[
			SNew(STransformViewportToolBar)
			.Viewport(ViewportRef)
			.CommandList(ViewportRef->GetCommandList())
			.Extenders(GetInfoProvider().GetExtenders())
			.Visibility(ViewportRef, &SEditorViewport::GetTransformToolbarVisibility)
		];

	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

void SCommonEditorViewportToolbarBase::UpdateAssetViewerProfileList()
{
	if (PreviewProfileController)
	{
		// Pull the latest profile list.
		int32 CurrProfileIndex = 0;
		TArray<FString> ProfileNames = PreviewProfileController->GetPreviewProfiles(CurrProfileIndex);

		// Rebuild the combo box list.
		AssetViewerProfileNames.Empty();
		for (const FString& Profile : ProfileNames)
		{
			AssetViewerProfileNames.Add(MakeShared<FString>(Profile));
		}

		// Select the current profile item.
		if (AssetViewerProfileComboBox)
		{
			AssetViewerProfileComboBox->RefreshOptions();
			AssetViewerProfileComboBox->SetSelectedItem(AssetViewerProfileNames[CurrProfileIndex]);
		}
	}
}

void SCommonEditorViewportToolbarBase::UpdateAssetViewerProfileSelection()
{
	if (PreviewProfileController)
	{
		FString ActiveProfileName = PreviewProfileController->GetActiveProfile();
		if (TSharedPtr<FString>* Match = AssetViewerProfileNames.FindByPredicate(
			[&ActiveProfileName](const TSharedPtr<FString>& Candidate) { return *Candidate == ActiveProfileName; }))
		{
			AssetViewerProfileComboBox->SetSelectedItem(*Match);
		}
		else // The profile was likely renamed.
		{
			UpdateAssetViewerProfileList();
		}
	}
}

void SCommonEditorViewportToolbarBase::OnAssetViewerProfileComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	int32 NewSelectionIndex;
	if (AssetViewerProfileNames.Find(NewSelection, NewSelectionIndex))
	{
		// If that's the user changing the combo box, not an update coming from code to reflect a change that already occurred.
		if (SelectInfo != ESelectInfo::Direct)
		{
			PreviewProfileController->SetActiveProfile(*NewSelection);
		}
	}
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::MakeAssetViewerProfileComboBox()
{
	AssetViewerProfileComboBox = SNew(STextComboBox)
		.OptionsSource(&AssetViewerProfileNames)
		.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.Button"))
		.ContentPadding(FMargin(2, 0))
		.ToolTipText(LOCTEXT("AssetViewerProfile_ToolTip", "Changes the asset viewer profile"))
		.OnSelectionChanged(this, &SCommonEditorViewportToolbarBase::OnAssetViewerProfileComboBoxSelectionChanged)
		.Visibility_Lambda([this]() { return AssetViewerProfileNames.Num() > 1 ? EVisibility::Visible : EVisibility::Collapsed; });

	AssetViewerProfileComboBox->RefreshOptions();
	if (!AssetViewerProfileNames.IsEmpty())
	{
		AssetViewerProfileComboBox->SetSelectedItem(AssetViewerProfileNames[0]);
	}

	return AssetViewerProfileComboBox.ToSharedRef();
}

FText SCommonEditorViewportToolbarBase::GetCameraMenuLabel() const
{
	return GetCameraMenuLabelFromViewportType( GetViewportClient().GetViewportType() );
}


EVisibility SCommonEditorViewportToolbarBase::GetViewModeOptionsVisibility() const
{
	const FEditorViewportClient& ViewClient = GetViewportClient();
	if (ViewClient.GetViewMode() == VMI_MeshUVDensityAccuracy || ViewClient.GetViewMode() == VMI_MaterialTextureScaleAccuracy || ViewClient.GetViewMode() == VMI_RequiredTextureResolution)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateViewModeOptionsMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();
	FEditorViewportClient& ViewClient = GetViewportClient();
	const UWorld* World = ViewClient.GetWorld();
	return BuildViewModeOptionsMenu(ViewportRef->GetCommandList(), ViewClient.GetViewMode(), World ? World->FeatureLevel.GetValue() : GMaxRHIFeatureLevel, ViewClient.GetViewModeParamNameMap());
}


TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateOptionsMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bIsPerspective = GetViewportClient().GetViewportType() == LVT_Perspective;
	
	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder OptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{
		OptionsMenuBuilder.BeginSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options") );
		{
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleRealTime );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleStats );
			OptionsMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().ToggleFPS );

			if (bIsPerspective)
			{
				OptionsMenuBuilder.AddWidget( GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)") );
				OptionsMenuBuilder.AddWidget( GenerateFarViewPlaneMenu(), LOCTEXT("FarViewPlane", "Far View Plane") );
			}

			OptionsMenuBuilder.AddWidget(GenerateScreenPercentageMenu(), LOCTEXT("ScreenPercentage", "Screen Percentage"));
		}
		OptionsMenuBuilder.EndSection();

 		TSharedPtr<SAssetEditorViewport> AssetEditorViewportPtr = StaticCastSharedRef<SAssetEditorViewport>(ViewportRef);
 		if (AssetEditorViewportPtr.IsValid())
		{
			OptionsMenuBuilder.BeginSection("EditorViewportLayouts");
			{
				OptionsMenuBuilder.AddSubMenu(
					LOCTEXT("ConfigsSubMenu", "Layouts"),
					FText::GetEmpty(),
					FNewMenuDelegate::CreateSP(AssetEditorViewportPtr.Get(), &SAssetEditorViewport::GenerateLayoutMenu));
			}
			OptionsMenuBuilder.EndSection();
		}

		ExtendOptionsMenu(OptionsMenuBuilder);
	}

	return OptionsMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateCameraMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder CameraMenuBuilder( bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList() );

	// Camera types
	CameraMenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().Perspective );

	CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic") );
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
		CameraMenuBuilder.EndSection();

	return CameraMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateShowMenu() const
{
	GetInfoProvider().OnFloatingButtonClicked();
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

#if 0
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

	const TArray<FShowFlagData>& ShowFlagData = GetShowFlagMenuItems();

	TArray<CommonEditorViewportUtils::FShowMenuCommand> ShowMenu[SFG_Max];

	// Get each show flag command and put them in their corresponding groups
	for( int32 ShowFlag = 0; ShowFlag < ShowFlagData.Num(); ++ShowFlag )
	{
		const FShowFlagData& SFData = ShowFlagData[ShowFlag];
		
		ShowMenu[SFData.Group].Add( Actions.ShowFlagCommands[ ShowFlag ] );
	}
#endif

	const bool bInShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder ShowMenuBuilder(bInShouldCloseWindowAfterMenuSelection, ViewportRef->GetCommandList());
	{

#if 0
		ShowMenuBuilder.AddMenuEntry( Actions.UseDefaultShowFlags );
		
		if( ShowMenu[SFG_Normal].Num() > 0 )
		{
			// Generate entries for the standard show flags
			ShowMenuBuilder.BeginSection("LevelViewportShowFlagsCommon", LOCTEXT("CommonShowFlagHeader", "Common Show Flags") );
			{
				for( int32 EntryIndex = 0; EntryIndex < ShowMenu[SFG_Normal].Num(); ++EntryIndex )
				{
					ShowMenuBuilder.AddMenuEntry( ShowMenu[SFG_Normal][ EntryIndex ].ShowMenuItem, NAME_None, ShowMenu[SFG_Normal][ EntryIndex ].LabelOverride );
				}
			}
			ShowMenuBuilder.EndSection();
		}

		// Generate entries for the different show flags groups
		ShowMenuBuilder.BeginSection("LevelViewportShowFlags");
		{
			ShowMenuBuilder.AddSubMenu( LOCTEXT("PostProcessShowFlagsMenu", "Post Processing"), LOCTEXT("PostProcessShowFlagsMenu_ToolTip", "Post process show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_PostProcess], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightTypesShowFlagsMenu", "Light Types"), LOCTEXT("LightTypesShowFlagsMenu_ToolTip", "Light Types show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightTypes], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingComponentsShowFlagsMenu", "Lighting Components"), LOCTEXT("LightingComponentsShowFlagsMenu_ToolTip", "Lighting Components show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightingComponents], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("LightingFeaturesShowFlagsMenu", "Lighting Features"), LOCTEXT("LightingFeaturesShowFlagsMenu_ToolTip", "Lighting Features show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_LightingFeatures], 0));

			ShowMenuBuilder.AddSubMenu(LOCTEXT("LumenShowFlagsMenu", "Lumen"), LOCTEXT("LumenShowFlagsMenu_ToolTip", "Lumen show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Lumen], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("DeveloperShowFlagsMenu", "Developer"), LOCTEXT("DeveloperShowFlagsMenu_ToolTip", "Developer show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Developer], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("VisualizeShowFlagsMenu", "Visualize"), LOCTEXT("VisualizeShowFlagsMenu_ToolTip", "Visualize show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Visualize], 0));

			ShowMenuBuilder.AddSubMenu( LOCTEXT("AdvancedShowFlagsMenu", "Advanced"), LOCTEXT("AdvancedShowFlagsMenu_ToolTip", "Advanced show flags"),
				FNewMenuDelegate::CreateStatic(&CommonEditorViewportUtils::FillShowMenu, ShowMenu[SFG_Advanced], 0));
		}
		ShowMenuBuilder.EndSection();


		FText ShowAllLabel = LOCTEXT("ShowAllLabel", "Show All");
		FText HideAllLabel = LOCTEXT("HideAllLabel", "Hide All");

		// Show Volumes sub-menu
		{
			TArray< FLevelViewportCommands::FShowMenuCommand > ShowVolumesMenu;

			// 'Show All' and 'Hide All' buttons
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.ShowAllVolumes, ShowAllLabel ) );
			ShowVolumesMenu.Add( FLevelViewportCommands::FShowMenuCommand( Actions.HideAllVolumes, HideAllLabel ) );

			// Get each show flag command and put them in their corresponding groups
			ShowVolumesMenu += Actions.ShowVolumeCommands;

			ShowMenuBuilder.AddSubMenu( LOCTEXT("ShowVolumesMenu", "Volumes"), LOCTEXT("ShowVolumesMenu_ToolTip", "Show volumes flags"),
				FNewMenuDelegate::CreateStatic( &FillShowMenu, ShowVolumesMenu, 2 ) );
		}
#endif
	}

	return ShowMenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateFOVMenu() const
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
				SNew ( SBorder )
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<float>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.Font( FAppStyle::GetFontStyle( TEXT( "MenuItem.Font" ) ) )
					.MinValue(FOVMin)
					.MaxValue(FOVMax)
					.Value(this, &SCommonEditorViewportToolbarBase::OnGetFOVValue)
					.OnValueChanged(this, &SCommonEditorViewportToolbarBase::OnFOVValueChanged)
				]
			]
		];
}

float SCommonEditorViewportToolbarBase::OnGetFOVValue() const
{
	return GetViewportClient().ViewFOV;
}

void SCommonEditorViewportToolbarBase::OnFOVValueChanged(float NewValue) const
{
	FEditorViewportClient& ViewportClient = GetViewportClient();
	ViewportClient.FOVAngle = NewValue;
	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateScreenPercentageMenu() const
{
	const int32 PreviewScreenPercentageMin = static_cast<int32>(ISceneViewFamilyScreenPercentage::kMinTSRResolutionFraction * 100.0f);
	const int32 PreviewScreenPercentageMax = static_cast<int32>(ISceneViewFamilyScreenPercentage::kMaxTSRResolutionFraction * 100.0f);

	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		.IsEnabled(this, &SCommonEditorViewportToolbarBase::OnScreenPercentageIsEnabled)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew ( SBorder )
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<int32>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.MinSliderValue(PreviewScreenPercentageMin)
					.MaxSliderValue(PreviewScreenPercentageMax)
					.Value(this, &SCommonEditorViewportToolbarBase::OnGetScreenPercentageValue)
					.OnValueChanged(const_cast<SCommonEditorViewportToolbarBase*>(this), &SCommonEditorViewportToolbarBase::OnScreenPercentageValueChanged)
				]
			]
		];
}

int32 SCommonEditorViewportToolbarBase::OnGetScreenPercentageValue() const
{
	return GetViewportClient().GetPreviewScreenPercentage();
}

bool SCommonEditorViewportToolbarBase::OnScreenPercentageIsEnabled() const
{
	return GetViewportClient().SupportsPreviewResolutionFraction();
}

void SCommonEditorViewportToolbarBase::OnScreenPercentageValueChanged(int32 NewValue)
{
	FEditorViewportClient& ViewportClient = GetViewportClient();
	ViewportClient.SetPreviewScreenPercentage(NewValue);
	ViewportClient.Invalidate();
}


TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GenerateFarViewPlaneMenu() const
{
	return
		SNew(SBox)
		.HAlign(HAlign_Right)
		[
			SNew(SBox)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.WidthOverride(100.0f)
			[
				SNew ( SBorder )
				.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
				.Padding(FMargin(1.0f))
				[
					SNew(SSpinBox<float>)
					.Style(&FAppStyle::Get(), "Menu.SpinBox")
					.ToolTipText(LOCTEXT("FarViewPlaneTooltip", "Distance to use as the far view plane, or zero to enable an infinite far view plane"))
					.MinValue(0.0f)
					.MaxValue(100000.0f)
					.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
					.Value(this, &SCommonEditorViewportToolbarBase::OnGetFarViewPlaneValue)
					.OnValueChanged(const_cast<SCommonEditorViewportToolbarBase*>(this), &SCommonEditorViewportToolbarBase::OnFarViewPlaneValueChanged)
				]
			]
		];
}

float SCommonEditorViewportToolbarBase::OnGetFarViewPlaneValue() const
{
	return GetViewportClient().GetFarClipPlaneOverride();
}

void SCommonEditorViewportToolbarBase::OnFarViewPlaneValueChanged(float NewValue)
{
	GetViewportClient().OverrideFarClipPlane(NewValue);
}

FReply SCommonEditorViewportToolbarBase::OnRealtimeWarningClicked()
{
	FEditorViewportClient& ViewportClient = GetViewportClient();
	ViewportClient.SetRealtime(true);

	return FReply::Handled();
}

EVisibility SCommonEditorViewportToolbarBase::GetRealtimeWarningVisibility() const
{
	FEditorViewportClient& ViewportClient = GetViewportClient();
	// If the viewport is not realtime and there is no override then realtime is off
	return !ViewportClient.IsRealtime() && !ViewportClient.IsRealtimeOverrideSet() ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedPtr<FExtender> SCommonEditorViewportToolbarBase::GetCombinedExtenderList(TSharedRef<FExtender> MenuExtender) const
{
	TSharedPtr<FExtender> HostEditorExtenders = GetInfoProvider().GetExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	Extenders.Reserve(2);
	Extenders.Add(HostEditorExtenders);
	Extenders.Add(MenuExtender);

	return FExtender::Combine(Extenders);
}

TSharedPtr<FExtender> SCommonEditorViewportToolbarBase::GetViewMenuExtender() const
{
	TSharedRef<FExtender> ViewModeExtender(new FExtender());
	ViewModeExtender->AddMenuExtension(
		TEXT("ViewMode"),
		EExtensionHook::After,
		GetInfoProvider().GetViewportWidget()->GetCommandList(),
		FMenuExtensionDelegate::CreateSP(const_cast<SCommonEditorViewportToolbarBase*>(this), &SCommonEditorViewportToolbarBase::CreateViewMenuExtensions));

	return GetCombinedExtenderList(ViewModeExtender);
}

void SCommonEditorViewportToolbarBase::CreateViewMenuExtensions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("LevelViewportDeferredRendering", LOCTEXT("DeferredRenderingHeader", "Deferred Rendering") );
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("LevelViewportCollision", LOCTEXT("CollisionViewModeHeader", "Collision") );
	{
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionPawn, NAME_None, LOCTEXT("CollisionPawnViewModeDisplayName", "Player Collision") );
		MenuBuilder.AddMenuEntry( FEditorViewportCommands::Get().CollisionVisibility, NAME_None, LOCTEXT("CollisionVisibilityViewModeDisplayName", "Visibility Collision") );
	}
	MenuBuilder.EndSection();

//FINDME
// 	MenuBuilder.BeginSection("LevelViewportLandscape", LOCTEXT("LandscapeHeader", "Landscape") );
// 	{
// 		MenuBuilder.AddSubMenu(LOCTEXT("LandscapeLODDisplayName", "LOD"), LOCTEXT("LandscapeLODMenu_ToolTip", "Override Landscape LOD in this viewport"), FNewMenuDelegate::CreateStatic(&Local::BuildLandscapeLODMenu, this), /*Default*/false, FSlateIcon());
// 	}
// 	MenuBuilder.EndSection();
}

ICommonEditorViewportToolbarInfoProvider& SCommonEditorViewportToolbarBase::GetInfoProvider() const
{
	return *InfoProviderPtr.Pin().Get();
}

FEditorViewportClient& SCommonEditorViewportToolbarBase::GetViewportClient() const
{
	return *GetInfoProvider().GetViewportWidget()->GetViewportClient().Get();
}

TSharedRef<SEditorViewportViewMenu> SCommonEditorViewportToolbarBase::MakeViewMenu()
{
	TSharedRef<SEditorViewport> ViewportRef = GetInfoProvider().GetViewportWidget();

	return SNew(SEditorViewportViewMenu, ViewportRef, SharedThis(this))
		.Cursor(EMouseCursor::Default)
		.MenuExtenders(GetViewMenuExtender());
}

FText SCommonEditorViewportToolbarBase::GetScalabilityWarningLabel() const
{
	const int32 QualityLevel = Scalability::GetQualityLevels().GetMinQualityLevel();
	if (QualityLevel >= 0)
	{
		return FText::Format(LOCTEXT("ScalabilityWarning", "Scalability: {0}"), Scalability::GetScalabilityNameFromQualityLevel(QualityLevel));
	}

	return FText::GetEmpty();
}

EVisibility SCommonEditorViewportToolbarBase::GetScalabilityWarningVisibility() const
{
	//This method returns magic numbers. 3 means epic
	return GetDefault<UEditorPerformanceSettings>()->bEnableScalabilityWarningIndicator && GetShowScalabilityMenu() && Scalability::GetQualityLevels().GetMinQualityLevel() != 3 ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SCommonEditorViewportToolbarBase::GetScalabilityWarningMenuContent() const
{
	return
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(SScalabilitySettings)
		];
}

#undef LOCTEXT_NAMESPACE
