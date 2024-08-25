// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLevelViewportToolBar.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SceneView.h"
#include "Subsystems/PanelExtensionSubsystem.h"
#include "ToolMenus.h"
#include "LevelEditorMenuContext.h"
#include "ViewportToolBarContext.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Styling/AppStyle.h"
#include "Camera/CameraActor.h"
#include "Misc/ConfigCacheIni.h"
#include "GameFramework/ActorPrimitiveColorHandler.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "STransformViewportToolbar.h"
#include "EditorShowFlags.h"
#include "LevelViewportActions.h"
#include "LevelEditorViewport.h"
#include "Layers/LayersSubsystem.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "IDeviceProfileServicesModule.h"
#include "EditorViewportCommands.h"
#include "SEditorViewportToolBarMenu.h"
#include "SEditorViewportToolBarButton.h"
#include "SEditorViewportViewMenu.h"
#include "Stats/StatsData.h"
#include "BufferVisualizationData.h"
#include "NaniteVisualizationData.h"
#include "LumenVisualizationData.h"
#include "SubstrateVisualizationData.h"
#include "GroomVisualizationData.h"
#include "VirtualShadowMapVisualizationData.h"
#include "FoliageType.h"
#include "ShowFlagMenuCommands.h"
#include "Bookmarks/BookmarkUI.h"
#include "Scalability.h"
#include "SScalabilitySettings.h"
#include "Editor/EditorPerformanceSettings.h"
#include "SEditorViewportViewMenuContext.h"
#include "Bookmarks/IBookmarkTypeTools.h"
#include "ToolMenu.h"
#include "WorldPartition/IWorldPartitionEditorModule.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/WorldPartition.h"
#include "SLevelViewport.h"
#include "SortHelper.h"
#include "Interfaces/IMainFrameModule.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SActionableMessageViewportWidget.h"

#define LOCTEXT_NAMESPACE "LevelViewportToolBar"

/** Override the view menu, just so we can specify the level viewport as active when the button is clicked */
class SLevelEditorViewportViewMenu : public SEditorViewportViewMenu
{
public:

	void Construct(const FArguments& InArgs, TSharedRef<SEditorViewport> InViewport, TSharedRef<class SViewportToolBar> InParentToolBar)
	{
		SEditorViewportViewMenu::Construct(InArgs, InViewport, InParentToolBar);
		MenuName = FName("LevelEditor.LevelViewportToolBar.View");
	}

	virtual void RegisterMenus() const override
	{
		SEditorViewportViewMenu::RegisterMenus();

		if (!UToolMenus::Get()->IsMenuRegistered("LevelEditor.LevelViewportToolBar.View"))
		{
			UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("LevelEditor.LevelViewportToolBar.View", "UnrealEd.ViewportToolbar.View");
			Menu->AddDynamicSection("LevelSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UEditorViewportViewMenuContext* Context = InMenu->FindContext<UEditorViewportViewMenuContext>();
				TSharedPtr<SLevelViewportToolBar> LevelViewportToolBar = StaticCastSharedPtr<SLevelViewportToolBar>(Context->EditorViewportViewMenu.Pin()->GetParentToolBar().Pin());
				LevelViewportToolBar->FillViewMenu(InMenu);
			}));
		}
	}

	virtual TSharedRef<SWidget> GenerateViewMenuContent() const override
	{
		SLevelViewport* LevelViewport = static_cast<SLevelViewport*>(Viewport.Pin().Get());
		LevelViewport->OnFloatingButtonClicked();
		
		return SEditorViewportViewMenu::GenerateViewMenuContent();
	}
};

static void FillShowMenuStatic(UToolMenu* Menu, TArray< FLevelViewportCommands::FShowMenuCommand > MenuCommands, int32 EntryOffset)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	// Generate entries for the standard show flags
	// Assumption: the first 'n' entries types like 'Show All' and 'Hide All' buttons, so insert a separator after them
	for (int32 EntryIndex = 0; EntryIndex < MenuCommands.Num(); ++EntryIndex)
	{
		FName EntryName = NAME_None;

		if (MenuCommands[EntryIndex].ShowMenuItem)
		{
			EntryName = MenuCommands[EntryIndex].ShowMenuItem->GetCommandName();
			ensure(Section.FindEntry(EntryName) == nullptr);
		}

		Section.AddMenuEntry(
			EntryName,
			MenuCommands[EntryIndex].ShowMenuItem,
			MenuCommands[EntryIndex].LabelOverride
		);

		if (EntryIndex == EntryOffset - 1)
		{
			Section.AddSeparator(NAME_None);
		}
	}
}

#if STATS
static void FillShowStatsSubMenus(UToolMenu* Menu, TArray< FLevelViewportCommands::FShowMenuCommand > MenuCommands, TMap< FString, TArray< FLevelViewportCommands::FShowMenuCommand > > StatCatCommands)
{
	FillShowMenuStatic(Menu, MenuCommands, 1);

	FToolMenuSection& Section = Menu->FindOrAddSection("Section");

	// Separate out stats into two list, those with and without submenus
	TArray< FLevelViewportCommands::FShowMenuCommand > SingleStatCommands;
	TMap< FString, TArray< FLevelViewportCommands::FShowMenuCommand > > SubbedStatCommands;
	for (auto StatCatIt = StatCatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
	{
		const TArray< FLevelViewportCommands::FShowMenuCommand >& ShowStatCommands = StatCatIt.Value();
		const FString& CategoryName = StatCatIt.Key();

		// If no category is specified, or there's only one category, don't use submenus
		FString NoCategory = FStatConstants::NAME_NoCategory.ToString();
		NoCategory.RemoveFromStart(TEXT("STATCAT_"));
		if (CategoryName == NoCategory || StatCatCommands.Num() == 1)
		{
			for (int32 StatIndex = 0; StatIndex < ShowStatCommands.Num(); ++StatIndex)
			{
				const FLevelViewportCommands::FShowMenuCommand& StatCommand = ShowStatCommands[StatIndex];
				SingleStatCommands.Add(StatCommand);
			}
		}
		else
		{
			SubbedStatCommands.Add(CategoryName, ShowStatCommands);
		}
	}

	// First add all the stats that don't have a sub menu
	for (auto StatCatIt = SingleStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
	{
		const FLevelViewportCommands::FShowMenuCommand& StatCommand = *StatCatIt;
		Section.AddMenuEntry(
			NAME_None,
			StatCommand.ShowMenuItem,
			StatCommand.LabelOverride
		);
	}

	// Now add all the stats that have sub menus
	for (auto StatCatIt = SubbedStatCommands.CreateConstIterator(); StatCatIt; ++StatCatIt)
	{
		const TArray< FLevelViewportCommands::FShowMenuCommand >& StatCommands = StatCatIt.Value();
		const FText CategoryName = FText::FromString(StatCatIt.Key());

		FFormatNamedArguments Args;
		Args.Add(TEXT("StatCat"), CategoryName);
		const FText CategoryDescription = FText::Format(NSLOCTEXT("UICommands", "StatShowCatName", "Show {StatCat} stats"), Args);

		Section.AddSubMenu(NAME_None, CategoryName, CategoryDescription,
			FNewToolMenuDelegate::CreateStatic(&FillShowMenuStatic, StatCommands, 0));
	}
}
#endif

void SLevelViewportToolBar::Construct( const FArguments& InArgs )
{
	Viewport = InArgs._Viewport;
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

	UViewportToolBarContext* ExtensionContextObject = NewObject<UViewportToolBarContext>();
	ExtensionContextObject->ViewportToolBar = SharedThis(this);
	ExtensionContextObject->Viewport = Viewport;

	const FMargin ToolbarSlotPadding(4.0f, 1.0f);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("EditorViewportToolBar.Background"))
		.Cursor(EMouseCursor::Default)
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(ToolbarSlotPadding)
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ToolbarSlotPadding)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Visibility(Viewport.Pin().Get(), &SLevelViewport::GetToolbarVisibility)
					.Image("EditorViewportToolBar.OptionsDropdown")
					.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.MenuDropdown")))
					.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateOptionsMenu )
				]
				+ SHorizontalBox::Slot()
				[
					SNew( SHorizontalBox )
					.Visibility(Viewport.Pin().Get(), &SLevelViewport::GetFullToolbarVisibility)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( ToolbarSlotPadding )
					[
						SNew( SEditorViewportToolbarMenu )
						.ParentToolBar( SharedThis( this ) )
						.Label( this, &SLevelViewportToolBar::GetCameraMenuLabel )
						.LabelIcon( this, &SLevelViewportToolBar::GetCameraMenuLabelIcon )
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.CameraMenu")))
						.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateCameraMenu ) 
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( ToolbarSlotPadding )
					[
						SNew( SLevelEditorViewportViewMenu, ViewportRef, SharedThis(this) )
						.MenuExtenders(GetViewMenuExtender())
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("ViewMenuButton")))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( ToolbarSlotPadding )
					[
						SNew( SEditorViewportToolbarMenu )
						.Label( LOCTEXT("ShowMenuTitle", "Show") )
						.ParentToolBar( SharedThis( this ) )
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.ShowMenu")))
						.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateShowMenu ) 
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( ToolbarSlotPadding )
					[
						SNew( SEditorViewportToolbarMenu )
						.Label( this, &SLevelViewportToolBar::GetViewModeOptionsMenuLabel )
						.ParentToolBar( SharedThis( this ) )
						.Visibility( this, &SLevelViewportToolBar::GetViewModeOptionsVisibility )
						.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("EditorViewportToolBar.ViewModeOptions")))
						.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateViewModeOptionsMenu ) 
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( ToolbarSlotPadding )
					[
						SNew( SEditorViewportToolbarMenu )
						.ParentToolBar( SharedThis( this ) )
						.Label( this, &SLevelViewportToolBar::GetDevicePreviewMenuLabel )
						.LabelIcon( this, &SLevelViewportToolBar::GetDevicePreviewMenuLabelIcon )
						.OnGetMenuContent( this, &SLevelViewportToolBar::GenerateDevicePreviewMenu )
						//@todo rendering: mobile preview in view port is not functional yet - remove this once it is.
						.Visibility(EVisibility::Collapsed)
					]
					+ SHorizontalBox::Slot()
					.Padding(ToolbarSlotPadding)
					.AutoWidth()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Fill)
					[
						SNew(SExtensionPanel)
						.ExtensionPanelID("LevelViewportToolBar.LeftExtension")
						.ExtensionContext(ExtensionContextObject)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ToolbarSlotPadding)
					[
						// Button to show that realtime is off
						SNew(SEditorViewportToolBarButton)	
						.ButtonType(EUserInterfaceActionType::Button)
						.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
						.OnClicked(this, &SLevelViewportToolBar::OnRealtimeWarningClicked)
						.Visibility(this, &SLevelViewportToolBar::GetRealtimeWarningVisibility)
						.ToolTipText(LOCTEXT("RealtimeOff_ToolTip", "This viewport is not updating in realtime.  Click to turn on realtime mode."))
						.Content()
						[
							SNew(STextBlock)
							.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("SmallText"))
							.Text(LOCTEXT("RealtimeOff", "Realtime Off"))
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ToolbarSlotPadding)
					[
						// Button to show scalability warnings
						SNew(SEditorViewportToolbarMenu)
						.ParentToolBar(SharedThis(this))
						.Label(this, &SLevelViewportToolBar::GetScalabilityWarningLabel)
						.MenuStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("EditorViewportToolBar.WarningButton"))
						.OnGetMenuContent(this, &SLevelViewportToolBar::GetScalabilityWarningMenuContent)
						.Visibility(this, &SLevelViewportToolBar::GetScalabilityWarningVisibility)
						.ToolTipText(LOCTEXT("ScalabilityWarning_ToolTip", "Non-default scalability settings could be affecting what is shown in this viewport.\nFor example you may experience lower visual quality, reduced particle counts, and other artifacts that don't match what the scene would look like when running outside of the editor. Click to make changes."))
					]
					+ SHorizontalBox::Slot()
					.Padding(ToolbarSlotPadding)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Fill)
					[
						SNew(SExtensionPanel)
						.ExtensionPanelID("LevelViewportToolBar.MiddleExtension")
						.ExtensionContext(ExtensionContextObject)
					]
					+ SHorizontalBox::Slot()
					.Padding(ToolbarSlotPadding)
					.AutoWidth()
					.HAlign(HAlign_Right)
					.VAlign(VAlign_Fill)
					[
						SNew(SExtensionPanel)
						.ExtensionPanelID("LevelViewportToolBar.RightExtension")
						.ExtensionContext(ExtensionContextObject)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.MaxWidth(TAttribute<float>::CreateSP(this, &SLevelViewportToolBar::GetTransformToolbarWidth))
					.Padding(ToolbarSlotPadding)
					.HAlign(HAlign_Right)
					[
						SAssignNew(TransformToolbar, STransformViewportToolBar)
						.Viewport(ViewportRef)
						.CommandList(ViewportRef->GetCommandList())
						.Extenders(LevelEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders())
						.Visibility(ViewportRef, &SLevelViewport::GetTransformToolbarVisibility)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					.Padding(ToolbarSlotPadding)
					[
						//The Maximize/Minimize button is only displayed when not in Immersive mode.
						SNew(SEditorViewportToolBarButton)
						.ButtonType(EUserInterfaceActionType::ToggleButton)
						.CheckBoxStyle(&FAppStyle::Get().GetWidgetStyle<FCheckBoxStyle>("EditorViewportToolBar.MaximizeRestoreButton"))
						.IsChecked(ViewportRef, &SLevelViewport::IsMaximized)
						.OnClicked(ViewportRef, &SLevelViewport::OnToggleMaximize)
						.Visibility(ViewportRef, &SLevelViewport::GetMaximizeToggleVisibility)
						.Image("EditorViewportToolBar.Maximize")
						.ToolTipText(LOCTEXT("Maximize_ToolTip", "Maximizes or restores this viewport"))
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Right)
					.AutoWidth()
					.Padding(ToolbarSlotPadding)
					[
						//The Restore from Immersive' button is only displayed when the editor is in Immersive mode.
						SNew(SEditorViewportToolBarButton)
						.ButtonType(EUserInterfaceActionType::Button)
						.OnClicked(ViewportRef, &SLevelViewport::OnToggleMaximize)
						.Visibility(ViewportRef, &SLevelViewport::GetCloseImmersiveButtonVisibility)
						.Image("EditorViewportToolBar.RestoreFromImmersive.Normal")
						.ToolTipText(LOCTEXT("RestoreFromImmersive_ToolTip", "Restore from Immersive"))
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.f, 8.f, 10.f, 0.f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ActionableMessage.Border"))
				[
					SAssignNew(ActionableMessageViewportWidget, SActionableMessageViewportWidget)
					.Visibility_Lambda([this]()
					{
						return ActionableMessageViewportWidget->GetVisibility();
					})
				]
			]
		]
	];
	
	SViewportToolBar::Construct(SViewportToolBar::FArguments());
}

bool SLevelViewportToolBar::IsViewModeSupported(EViewModeIndex ViewModeIndex) const
{
	return true;
}

FLevelEditorViewportClient* SLevelViewportToolBar::GetLevelViewportClient() const
{
	if (Viewport.IsValid())
	{
		return &Viewport.Pin()->GetLevelViewportClient();
	}

	return nullptr;
}

FText SLevelViewportToolBar::GetCameraMenuLabel() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		return GetCameraMenuLabelFromViewportType( PinnedViewport->GetLevelViewportClient().ViewportType );
	}

	return LOCTEXT("CameraMenuTitle_Default", "Camera");
}

const FSlateBrush* SLevelViewportToolBar::GetCameraMenuLabelIcon() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		return GetCameraMenuLabelIconFromViewportType(PinnedViewport->GetLevelViewportClient().ViewportType );
	}

	return FStyleDefaults::GetNoBrush();
}

FText SLevelViewportToolBar::GetDevicePreviewMenuLabel() const
{
	FText Label = LOCTEXT("DevicePreviewMenuTitle_Default", "Preview");

	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if ( PinnedViewport->GetDeviceProfileString() != "Default" )
		{
			Label = FText::FromString( PinnedViewport->GetDeviceProfileString() );
		}
	}

	return Label;
}

const FSlateBrush* SLevelViewportToolBar::GetDevicePreviewMenuLabelIcon() const
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
	FString DeviceProfileName = ViewportRef->GetDeviceProfileString();

	FName PlatformIcon = NAME_None;
	if( !DeviceProfileName.IsEmpty() && DeviceProfileName != "Default" )
	{
		static FName DeviceProfileServices( "DeviceProfileServices" );

		IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
		IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();

		PlatformIcon = UIManager->GetDeviceIconName( DeviceProfileName );

		return FAppStyle::GetOptionalBrush( PlatformIcon );
	}

	return nullptr;
}

bool SLevelViewportToolBar::IsCurrentLevelViewport() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if( &( PinnedViewport->GetLevelViewportClient() ) == GCurrentLevelEditingViewportClient )
		{
			return true;
		}
	}
	return false;
}

bool SLevelViewportToolBar::IsPerspectiveViewport() const
{
	TSharedPtr< SLevelViewport > PinnedViewport( Viewport.Pin() );
	if( PinnedViewport.IsValid() )
	{
		if(  PinnedViewport->GetLevelViewportClient().IsPerspective() )
		{
			return true;
		}
	}
	return false;
}

/**
 * Called to generate the set bookmark submenu
 */
static void OnGenerateSetBookmarkMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	// Add a menu entry for each bookmark
	TSharedPtr<SLevelViewport> SharedViewport = Viewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	const int32 NumberOfBookmarks = static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
	const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

	for( int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex )
	{
		Section.AddMenuEntry(
			NAME_None,
			FLevelViewportCommands::Get().SetBookmarkCommands[BookmarkIndex],
			FBookmarkUI::GetPlainLabel(BookmarkIndex)
		);
	}

	// Only mapped bookmarks will have predefined actions.
	// So, create any additional actions we need to hit the max number of bookmarks.
	for (int32 BookmarkIndex = NumberOfMappedBookmarks; BookmarkIndex < NumberOfBookmarks; ++BookmarkIndex)
	{
		FUIAction Action;
		Action.ExecuteAction.BindSP(SharedViewport.ToSharedRef(), &SLevelViewport::OnSetBookmark, BookmarkIndex);

		Section.AddMenuEntry(
			NAME_None,
			FBookmarkUI::GetPlainLabel(BookmarkIndex),
			FBookmarkUI::GetSetTooltip(BookmarkIndex),
			FBookmarkUI::GetDefaultIcon(),
			Action);
	}
}

/**
 * Called to generate the clear bookmark submenu
 */
static void OnGenerateClearBookmarkMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	// Add a menu entry for each bookmark
	FEditorModeTools& Tools = GLevelEditorModeTools();
	TSharedPtr<SLevelViewport> SharedViewport = Viewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	const int32 NumberOfBookmarks = static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
	const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

	for( int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex )
	{
		if ( IBookmarkTypeTools::Get().CheckBookmark( BookmarkIndex , &ViewportClient ) )
		{
			Section.AddMenuEntry(
				NAME_None,
				FLevelViewportCommands::Get().ClearBookmarkCommands[ BookmarkIndex ],
				FBookmarkUI::GetPlainLabel(BookmarkIndex)
			);
		}
	}

	for (int32 BookmarkIndex = NumberOfMappedBookmarks; BookmarkIndex < NumberOfBookmarks; ++BookmarkIndex)
	{
		if ( IBookmarkTypeTools::Get().CheckBookmark(BookmarkIndex, &ViewportClient) )
		{
			FUIAction Action;
			Action.ExecuteAction.BindSP(SharedViewport.ToSharedRef(), &SLevelViewport::OnClearBookmark, BookmarkIndex);
			
			Section.AddMenuEntry(
				NAME_None,
				FBookmarkUI::GetPlainLabel(BookmarkIndex),
				FBookmarkUI::GetClearTooltip(BookmarkIndex),
				FBookmarkUI::GetDefaultIcon(),
				Action);
		}
	}
}

/**
 * Called to generate the jump to bookmark menu.
 */
static bool GenerateJumpToBookmarkMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	// Add a menu entry for each bookmark
	
	FEditorModeTools& Tools = GLevelEditorModeTools();
	TSharedPtr<SLevelViewport> SharedViewport = Viewport.Pin();
	FLevelEditorViewportClient& ViewportClient = SharedViewport->GetLevelViewportClient();

	const int32 NumberOfBookmarks = static_cast<int32>(IBookmarkTypeTools::Get().GetMaxNumberOfBookmarks(&ViewportClient));
	const int32 NumberOfMappedBookmarks = FMath::Min<int32>(AWorldSettings::NumMappedBookmarks, NumberOfBookmarks);

	bool bFoundAnyBookmarks = false;

	for( int32 BookmarkIndex = 0; BookmarkIndex < NumberOfMappedBookmarks; ++BookmarkIndex )
	{
		if ( IBookmarkTypeTools::Get().CheckBookmark( BookmarkIndex , &ViewportClient ) )
		{
			bFoundAnyBookmarks = true;
			Section.AddMenuEntry(
				NAME_None,
				FLevelViewportCommands::Get().JumpToBookmarkCommands[BookmarkIndex]				
			);
		}
	}

	for (int32 BookmarkIndex = NumberOfMappedBookmarks; BookmarkIndex < NumberOfBookmarks; ++BookmarkIndex)
	{
		if ( IBookmarkTypeTools::Get().CheckBookmark(BookmarkIndex, &ViewportClient) )
		{
			bFoundAnyBookmarks = true;

			FUIAction Action;
			Action.ExecuteAction.BindSP(SharedViewport.ToSharedRef(), &SLevelViewport::OnJumpToBookmark, BookmarkIndex);
			
			Section.AddMenuEntry(
				NAME_None,
				FBookmarkUI::GetJumpToLabel(BookmarkIndex),
				FBookmarkUI::GetJumpToTooltip(BookmarkIndex),
				FBookmarkUI::GetDefaultIcon(),
				Action);
		}
	}
	
	return bFoundAnyBookmarks;
}

/**
 * Called to generate the bookmark submenu
 */
static void OnGenerateBookmarkMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{
	FEditorModeTools& Tools = GLevelEditorModeTools();

	// true if a bookmark was found. 
	bool bFoundBookmark = false;

	// Get the viewport client to pass down to the CheckBookmark function
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();

	bool bFoundBookmarks = false;
	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportActiveBoookmarks", LOCTEXT("JumpToBookmarkHeader", "Active Bookmarks"));
		bFoundBookmarks = GenerateJumpToBookmarkMenu(Menu, Viewport);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportBookmarkSubmenus");
		Section.AddSubMenu(
			"SetBookmark",
			LOCTEXT("SetBookmarkSubMenu", "Set Bookmark"),
			LOCTEXT("SetBookmarkSubMenu_ToolTip", "Set viewport bookmarks"),
			FNewToolMenuDelegate::CreateStatic( &OnGenerateSetBookmarkMenu, Viewport )
			);

		const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
		Section.AddMenuEntry( Actions.CompactBookmarks );

		if( bFoundBookmarks )
		{
			Section.AddSubMenu(
				"ClearBookmark",
				LOCTEXT("ClearBookmarkSubMenu", "Clear Bookmark"),
				LOCTEXT("ClearBookmarkSubMenu_ToolTip", "Clear viewport bookmarks"),
				FNewToolMenuDelegate::CreateStatic( &OnGenerateClearBookmarkMenu, Viewport ),
				false,
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
				);

			Section.AddMenuEntry( Actions.ClearAllBookmarks );
		}
	}
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateOptionsMenu() 
{
	static const FName MenuName("LevelEditor.LevelViewportToolBar.Options");
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		Menu->AddDynamicSection("DynamicSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			ULevelViewportToolBarContext* Context = InMenu->FindContext<ULevelViewportToolBarContext>();
			Context->LevelViewportToolBarWidget.Pin()->FillOptionsMenu(InMenu);
		}));
	}

	Viewport.Pin()->OnFloatingButtonClicked();

	const FLevelViewportCommands& LevelViewportActions = FLevelViewportCommands::Get();
	TSharedRef<FUICommandList> CommandList = Viewport.Pin()->GetCommandList().ToSharedRef();

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT("LevelEditor") );
	TSharedPtr<FExtender> MenuExtender = LevelEditorModule.AssembleExtenders(CommandList, LevelEditorModule.GetAllLevelViewportOptionsMenuExtenders());

	ULevelViewportToolBarContext* ContextObject = NewObject<ULevelViewportToolBarContext>();
	ContextObject->LevelViewportToolBarWidget = SharedThis(this);

	FToolMenuContext MenuContext(CommandList, MenuExtender, ContextObject);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SLevelViewportToolBar::FillOptionsMenu(UToolMenu* Menu)
{
	const FLevelViewportCommands& LevelViewportActions = FLevelViewportCommands::Get();
	const bool bIsPerspective = Viewport.Pin()->GetLevelViewportClient().IsPerspective();

	{
		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportViewportOptions", LOCTEXT("OptionsMenuHeader", "Viewport Options"));
			Section.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);

			// Add an option to disable the temporary override if there is one
			{
				FUIAction Action;
				Action.ExecuteAction.BindSP(this, &SLevelViewportToolBar::OnDisableRealtimeOverride);
				Action.IsActionVisibleDelegate.BindSP(this, &SLevelViewportToolBar::IsRealtimeOverrideToggleVisible);

				TAttribute<FText> Tooltip(this, &SLevelViewportToolBar::GetRealtimeOverrideTooltip);

				Section.AddMenuEntry(
					"DisableRealtimeOverride",
					LOCTEXT("DisableRealtimeOverride", "Disable Realtime Override"),
					Tooltip,
					FSlateIcon(),
					Action);

				Section.AddSeparator("DisableRealtimeOverrideSeparator");
					
			}

			Section.AddMenuEntry(FEditorViewportCommands::Get().ToggleFPS);

#if STATS
			TArray< FLevelViewportCommands::FShowMenuCommand > HideStatsMenu;

			Section.AddMenuEntry(FEditorViewportCommands::Get().ToggleStats);
			Section.AddSubMenu(
				"ShowStatsMenu",
				LOCTEXT("ShowStatsMenu", "Stat"),
				LOCTEXT("ShowStatsMenu_ToolTip", "Show Stat commands"),
				FNewToolMenuDelegate::CreateStatic(&FillShowStatsSubMenus, HideStatsMenu, LevelViewportActions.ShowStatCatCommands),
				false,
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Stats"));




			FText HideAllLabel = LOCTEXT("HideAllLabel", "Hide All");
		
			// 'Hide All' button
			HideStatsMenu.Add(FLevelViewportCommands::FShowMenuCommand(LevelViewportActions.HideAllStats, HideAllLabel));
#endif

			Section.AddMenuEntry(LevelViewportActions.ToggleViewportToolbar);

			if( bIsPerspective )
			{
				Section.AddEntry(FToolMenuEntry::InitWidget("FOVAngle", GenerateFOVMenu(), LOCTEXT("FOVAngle", "Field of View (H)")));
				Section.AddEntry(FToolMenuEntry::InitWidget("FarViewPlane", GenerateFarViewPlaneMenu(), LOCTEXT("FarViewPlane", "Far View Plane")));
			}

			FEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
			Section.AddSubMenu(
				"ScreenPercentageSubMenu",
				LOCTEXT("ScreenPercentageSubMenu", "Screen Percentage"),
				LOCTEXT("ScreenPercentageSubMenu_ToolTip", "Customize the viewport's screen percentage"),
				FNewMenuDelegate::CreateStatic(&SCommonEditorViewportToolbarBase::ConstructScreenPercentageMenu, &ViewportClient));
		}

		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportViewportOptions2");

			if( bIsPerspective )
			{
				// Cinematic preview only applies to perspective
				Section.AddMenuEntry( LevelViewportActions.ToggleCinematicPreview );
			}

			Section.AddMenuEntry( LevelViewportActions.ToggleGameView );
			Section.AddMenuEntry( LevelViewportActions.ToggleImmersive );
		}


		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportBookmarks");
			if( bIsPerspective )
			{
				// Bookmarks only work in perspective viewports so only show the menu option if this toolbar is in one

				Section.AddSubMenu(
					"Bookmark",
					LOCTEXT("BookmarkSubMenu", "Bookmarks"),
					LOCTEXT("BookmarkSubMenu_ToolTip", "Viewport location bookmarking"),
					FNewToolMenuDelegate::CreateStatic(&OnGenerateBookmarkMenu, Viewport),
					false,
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.Bookmarks")
				);

				Section.AddSubMenu(
					"Camera",
					LOCTEXT("CameraSubMeun", "Create Camera Here"),
					LOCTEXT("CameraSubMenu_ToolTip", "Select a camera type to create at current viewport's location"),
					FNewToolMenuDelegate::CreateSP(this, &SLevelViewportToolBar::GenerateCameraSpawnMenu),
					false,
					FSlateIcon(FAppStyle::Get().GetStyleSetName(), "EditorViewport.SubMenu.CreateCamera")
				);
			}

			Section.AddMenuEntry(LevelViewportActions.HighResScreenshot);
		}


		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportLayouts");
			Section.AddSubMenu(
				"Configs",
				LOCTEXT("ConfigsSubMenu", "Layouts"),
				FText::GetEmpty(),
				FNewToolMenuDelegate::CreateSP(this, &SLevelViewportToolBar::GenerateViewportConfigsMenu),
				false,
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Layout")
			);
		}

		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportSettings");
			Section.AddMenuEntry(LevelViewportActions.AdvancedSettings);
		}
	}
}


TSharedRef<SWidget> SLevelViewportToolBar::GenerateDevicePreviewMenu() const
{
	static const FName MenuName("LevelEditor.LevelViewportToolBar.DevicePreview");
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		Menu->AddDynamicSection("DynamicSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			ULevelViewportToolBarContext* Context = InMenu->FindContext<ULevelViewportToolBarContext>();
			Context->LevelViewportToolBarWidget.Pin()->FillDevicePreviewMenu(InMenu);
		}));
	}

	ULevelViewportToolBarContext* ContextObject = NewObject<ULevelViewportToolBarContext>();
	ContextObject->LevelViewportToolBarWidget = ConstCastSharedRef<SLevelViewportToolBar>(SharedThis(this));

	FToolMenuContext MenuContext(Viewport.Pin()->GetCommandList(), TSharedPtr<FExtender>(), ContextObject);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SLevelViewportToolBar::FillDevicePreviewMenu(UToolMenu* Menu) const
{
	IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
	IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();

	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();

	// Default menu - clear all settings
	{
		FToolMenuSection& Section = Menu->AddSection("DevicePreview", LOCTEXT("DevicePreviewMenuTitle", "Device Preview"));
		FUIAction Action( FExecuteAction::CreateSP( const_cast<SLevelViewportToolBar*>(this), &SLevelViewportToolBar::SetLevelProfile, FString( "Default" ) ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, FString( "Default" ) ) );
		Section.AddMenuEntry("DevicePreviewMenuClear", LOCTEXT("DevicePreviewMenuClear", "Off"), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::Button);
		}

	// Recent Device Profiles	
	{
	FToolMenuSection& Section = Menu->AddSection("Recent", LOCTEXT("RecentMenuHeading", "Recent"));

	const FString INISection = "SelectedProfile";
	const FString INIKeyBase = "ProfileItem";
	const int32 MaxItems = 4; // Move this into a config file
	FString CurItem;
	for( int32 ItemIdx = 0 ; ItemIdx < MaxItems; ++ItemIdx )
	{
		// Build the menu from the contents of the game ini
		//@todo This should probably be using GConfig->GetText [10/21/2013 justin.sargent]
		if ( GConfig->GetString( *INISection, *FString::Printf( TEXT("%s%d"), *INIKeyBase, ItemIdx ), CurItem, GEditorPerProjectIni ) )
		{
			const FName PlatformIcon = UIManager->GetDeviceIconName( CurItem );

			FUIAction Action( FExecuteAction::CreateSP( const_cast<SLevelViewportToolBar*>(this), &SLevelViewportToolBar::SetLevelProfile, CurItem ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, CurItem ) );
			Section.AddMenuEntry(NAME_None, FText::FromString(CurItem), FText(), FSlateIcon(FAppStyle::GetAppStyleSetName(), PlatformIcon), Action, EUserInterfaceActionType::Button );
		}
	}
	}

	// Device List
	{
	FToolMenuSection& Section = Menu->AddSection("Devices", LOCTEXT("DevicesMenuHeading", "Devices"));

	const TArray<TSharedPtr<FString> > PlatformList = UIManager->GetPlatformList();
	for ( int32 Index = 0; Index < PlatformList.Num(); Index++ )
	{
		TArray< UDeviceProfile* > DeviceProfiles;
		UIManager->GetProfilesByType( DeviceProfiles, *PlatformList[Index] );
		if ( DeviceProfiles.Num() > 0 )
		{
			const FString PlatformNameStr = DeviceProfiles[0]->DeviceType;
			const FName PlatformIcon =  UIManager->GetPlatformIconName( PlatformNameStr );
			Section.AddSubMenu(
				NAME_None,
				FText::FromString( PlatformNameStr ),
				FText::GetEmpty(),
				FNewToolMenuDelegate::CreateRaw( const_cast<SLevelViewportToolBar*>(this), &SLevelViewportToolBar::MakeDevicePreviewSubMenu, DeviceProfiles ),
				false,
				FSlateIcon(FAppStyle::GetAppStyleSetName(), PlatformIcon)
				);
		}
	}
	}
}

void SLevelViewportToolBar::MakeDevicePreviewSubMenu(UToolMenu* Menu, TArray< class UDeviceProfile* > InProfiles)
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
	FToolMenuSection& Section = Menu->AddSection("Section");

	for ( int32 Index = 0; Index < InProfiles.Num(); Index++ )
	{
		FUIAction Action( FExecuteAction::CreateSP( this, &SLevelViewportToolBar::SetLevelProfile, InProfiles[ Index ]->GetName() ),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsDeviceProfileStringSet, InProfiles[ Index ]->GetName() ) );

		Section.AddMenuEntry(NAME_None, FText::FromString( InProfiles[ Index ]->GetName() ), FText(), FSlateIcon(), Action, EUserInterfaceActionType::RadioButton );
	}
}

void SLevelViewportToolBar::SetLevelProfile( FString DeviceProfileName )
{
	TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
	ViewportRef->SetDeviceProfileString( DeviceProfileName );

	IDeviceProfileServicesModule& ScreenDeviceProfileUIServices = FModuleManager::LoadModuleChecked<IDeviceProfileServicesModule>(TEXT( "DeviceProfileServices"));
	IDeviceProfileServicesUIManagerPtr UIManager = ScreenDeviceProfileUIServices.GetProfileServicesManager();
	UIManager->SetProfile( DeviceProfileName );
}

void SLevelViewportToolBar::GeneratePlacedCameraMenuEntries(FToolMenuSection& Section, TArray<ACameraActor*> Cameras) const
{
	FSlateIcon CameraIcon( FAppStyle::GetAppStyleSetName(), "ClassIcon.CameraComponent" );

	// Sort the cameras to make the ordering predictable for users.
	Cameras.StableSort([](const ACameraActor& Left, const ACameraActor& Right)
	{
		// Do "natural sorting" via SceneOutliner::FNumericStringWrapper to make more sense to humans (also matches the Scene Outliner). This sorts "Camera2" before "Camera10" which a normal lexicographical sort wouldn't.
		SceneOutliner::FNumericStringWrapper LeftWrapper(FString(Left.GetActorLabel()));
		SceneOutliner::FNumericStringWrapper RightWrapper(FString(Right.GetActorLabel()));

		return LeftWrapper < RightWrapper;
	});

	for( ACameraActor* CameraActor : Cameras )
	{
		// Needed for the delegate hookup to work below
		AActor* GenericActor = CameraActor;

		FText ActorDisplayName = FText::FromString(CameraActor->GetActorLabel());
		FUIAction LookThroughCameraAction(
			FExecuteAction::CreateSP(Viewport.Pin().ToSharedRef(), &SLevelViewport::OnActorLockToggleFromMenu, GenericActor),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(Viewport.Pin().ToSharedRef(), &SLevelViewport::IsActorLocked, MakeWeakObjectPtr(GenericActor))
			);

		Section.AddMenuEntry( NAME_None, ActorDisplayName, FText::Format(LOCTEXT("LookThroughCameraActor_ToolTip", "Look through and pilot {0}"), ActorDisplayName), CameraIcon, LookThroughCameraAction, EUserInterfaceActionType::RadioButton );
	}
}

void SLevelViewportToolBar::GeneratePlacedCameraMenuEntries(UToolMenu* Menu, TArray<ACameraActor*> Cameras) const
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	GeneratePlacedCameraMenuEntries(Section, Cameras);
}

void SLevelViewportToolBar::GenerateViewportTypeMenu(FToolMenuSection& Section) const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	LevelEditorModule.IterateViewportTypes([&](FName ViewportTypeName, const FViewportTypeDefinition& InDefinition)
	{
		if (InDefinition.ActivationCommand.IsValid())
		{
			Section.AddMenuEntry(*FString::Printf(TEXT("ViewportType_%s"), *ViewportTypeName.ToString()), InDefinition.ActivationCommand);
		}
	});
}

void SLevelViewportToolBar::GenerateViewportTypeMenu(UToolMenu* Menu) const
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	GenerateViewportTypeMenu(Section);
}

void SLevelViewportToolBar::GenerateCameraSpawnMenu(UToolMenu* Menu) const
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();

	for (TSharedPtr<FUICommandInfo> Camera : Actions.CreateCameras)
	{
		Section.AddMenuEntry(NAME_None, Camera);
	}
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateCameraMenu() const
{
	static const FName MenuName("LevelEditor.LevelViewportToolBar.Camera");
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		Menu->AddDynamicSection("DynamicSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			ULevelViewportToolBarContext* Context = InMenu->FindContext<ULevelViewportToolBarContext>();
			Context->LevelViewportToolBarWidget.Pin()->FillCameraMenu(InMenu);
		}));
	}

	Viewport.Pin()->OnFloatingButtonClicked();

	ULevelViewportToolBarContext* ContextObject = NewObject<ULevelViewportToolBarContext>();
	ContextObject->LevelViewportToolBarWidget = ConstCastSharedRef<SLevelViewportToolBar>(SharedThis(this));

	FToolMenuContext MenuContext(Viewport.Pin()->GetCommandList(), TSharedPtr<FExtender>(), ContextObject);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SLevelViewportToolBar::FillCameraMenu(UToolMenu* Menu) const
{
	// Camera types
	{
		FToolMenuSection& Section = Menu->AddSection("CameraTypes");
		Section.AddMenuEntry(FEditorViewportCommands::Get().Perspective);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportCameraType_Ortho", LOCTEXT("CameraTypeHeader_Ortho", "Orthographic"));
		Section.AddMenuEntry(FEditorViewportCommands::Get().Top);
		Section.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
		Section.AddMenuEntry(FEditorViewportCommands::Get().Left);
		Section.AddMenuEntry(FEditorViewportCommands::Get().Right);
		Section.AddMenuEntry(FEditorViewportCommands::Get().Front);
		Section.AddMenuEntry(FEditorViewportCommands::Get().Back);
	}

	TArray<ACameraActor*> Cameras;

	for( TActorIterator<ACameraActor> It(GetWorld().Get()); It; ++It )
	{
		Cameras.Add( *It );
	}

	FText CameraActorsHeading = LOCTEXT("CameraActorsHeading", "Placed Cameras");

	// Don't add too many cameras to the top level menu or else it becomes too large
	const uint32 MaxCamerasInTopLevelMenu = 10;
	if( Cameras.Num() > MaxCamerasInTopLevelMenu )
	{
		FToolMenuSection& Section = Menu->AddSection("CameraActors");
		Section.AddSubMenu("CameraActors", CameraActorsHeading, LOCTEXT("LookThroughPlacedCameras_ToolTip", "Look through and pilot placed cameras"), FNewToolMenuDelegate::CreateSP(this, &SLevelViewportToolBar::GeneratePlacedCameraMenuEntries, Cameras ) );
	}
	else
	{
		FToolMenuSection& Section = Menu->AddSection("CameraActors", CameraActorsHeading);
		GeneratePlacedCameraMenuEntries(Section, Cameras);
	}

	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));

		int32 NumCustomViewportTypes = 0;
		LevelEditorModule.IterateViewportTypes([&](FName, const FViewportTypeDefinition&){ ++NumCustomViewportTypes; });

		FText ViewportTypesHeading = LOCTEXT("ViewportTypes", "Viewport Type");
		const uint32 MaxViewportTypesInTopLevelMenu = 4;
		if( NumCustomViewportTypes > MaxViewportTypesInTopLevelMenu )
		{
			FToolMenuSection& Section = Menu->AddSection("ViewportTypes");
			Section.AddSubMenu("ViewportTypes", ViewportTypesHeading, FText(), FNewToolMenuDelegate::CreateSP(this, &SLevelViewportToolBar::GenerateViewportTypeMenu) );
		}
		else
		{
			FToolMenuSection& Section = Menu->AddSection("ViewportTypes", ViewportTypesHeading);
			GenerateViewportTypeMenu(Section);
		}
	}
}

void SLevelViewportToolBar::GenerateViewportConfigsMenu(UToolMenu* Menu) const
{
	check (Viewport.IsValid());
	TSharedPtr<FUICommandList> CommandList = Viewport.Pin()->GetCommandList();

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportOnePaneConfigs", LOCTEXT("OnePaneConfigHeader", "One Pane"));

		FSlimHorizontalToolBarBuilder OnePaneButton(CommandList, FMultiBoxCustomization::None);
		OnePaneButton.SetLabelVisibility(EVisibility::Collapsed);
		OnePaneButton.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		OnePaneButton.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_OnePane);

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportOnePaneConfigs",
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				OnePaneButton.MakeWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportTwoPaneConfigs", LOCTEXT("TwoPaneConfigHeader", "Two Panes"));
		FSlimHorizontalToolBarBuilder TwoPaneButtons(CommandList, FMultiBoxCustomization::None);
		TwoPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		TwoPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesH, NAME_None, FText());
		TwoPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_TwoPanesV, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportTwoPaneConfigs",
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				TwoPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportThreePaneConfigs", LOCTEXT("ThreePaneConfigHeader", "Three Panes"));
		FSlimHorizontalToolBarBuilder ThreePaneButtons(CommandList, FMultiBoxCustomization::None);
		ThreePaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		ThreePaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesLeft, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesRight, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesTop, NAME_None, FText());
		ThreePaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_ThreePanesBottom, NAME_None, FText());

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportThreePaneConfigs",
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				ThreePaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportFourPaneConfigs", LOCTEXT("FourPaneConfigHeader", "Four Panes"));
		FSlimHorizontalToolBarBuilder FourPaneButtons(CommandList, FMultiBoxCustomization::None);
		FourPaneButtons.SetLabelVisibility(EVisibility::Collapsed);
		FourPaneButtons.SetStyle(&FAppStyle::Get(), "ViewportLayoutToolbar");

		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanes2x2, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesLeft, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesRight, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesTop, NAME_None, FText());
		FourPaneButtons.AddToolBarButton(FLevelViewportCommands::Get().ViewportConfig_FourPanesBottom, NAME_None, FText());	

		Section.AddEntry(FToolMenuEntry::InitWidget(
			"LevelViewportFourPaneConfigs",
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				FourPaneButtons.MakeWidget()
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNullWidget::NullWidget
			],
			FText::GetEmpty(), true
			));
	}
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateShowMenu() const
{
	static const FName MenuName("LevelEditor.LevelViewportToolbar.Show");
	if (!UToolMenus::Get()->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = UToolMenus::Get()->RegisterMenu(MenuName);
		Menu->AddDynamicSection("LevelDynamicSection", FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
		{
			ULevelViewportToolBarContext* Context = InMenu->FindContext<ULevelViewportToolBarContext>();
			Context->LevelViewportToolBarWidget.Pin()->FillShowMenu(InMenu);
		}));
	}

	Viewport.Pin()->OnFloatingButtonClicked();

	TSharedRef<FUICommandList> CommandList = Viewport.Pin()->GetCommandList().ToSharedRef();

	// Get all menu extenders for this context menu from the level editor module
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	TSharedPtr<FExtender> MenuExtender = LevelEditorModule.AssembleExtenders(CommandList, LevelEditorModule.GetAllLevelViewportShowMenuExtenders());

	ULevelViewportToolBarContext* ContextObject = NewObject<ULevelViewportToolBarContext>();
	ContextObject->LevelViewportToolBarWidget = ConstCastSharedRef<SLevelViewportToolBar>(SharedThis(this));

	FToolMenuContext MenuContext(CommandList, MenuExtender, ContextObject);
	return UToolMenus::Get()->GenerateWidget(MenuName, MenuContext);
}

void SLevelViewportToolBar::FillShowMenu(UToolMenu* Menu) const
{
	const FLevelViewportCommands& Actions = FLevelViewportCommands::Get();
	{
		{
			FToolMenuSection& Section = Menu->AddSection("UseDefaultShowFlags");
			Section.AddMenuEntry(Actions.UseDefaultShowFlags);
		}

		FShowFlagMenuCommands::Get().BuildShowFlagsMenu(Menu);

		FText ShowAllLabel = LOCTEXT("ShowAllLabel", "Show All");
		FText HideAllLabel = LOCTEXT("HideAllLabel", "Hide All");

		FLevelEditorViewportClient& ViewClient = Viewport.Pin()->GetLevelViewportClient();
		const UWorld* World = ViewClient.GetWorld();

		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportEditorShow", LOCTEXT("EditorShowHeader", "Editor"));
			// Show Volumes sub-menu
			{
				TArray< FLevelViewportCommands::FShowMenuCommand > ShowVolumesMenu;

				// 'Show All' and 'Hide All' buttons
				ShowVolumesMenu.Add(FLevelViewportCommands::FShowMenuCommand(Actions.ShowAllVolumes, ShowAllLabel));
				ShowVolumesMenu.Add(FLevelViewportCommands::FShowMenuCommand(Actions.HideAllVolumes, HideAllLabel));

				// Get each show flag command and put them in their corresponding groups
				ShowVolumesMenu += Actions.ShowVolumeCommands;

				Section.AddSubMenu("ShowVolumes", LOCTEXT("ShowVolumesMenu", "Volumes"), LOCTEXT("ShowVolumesMenu_ToolTip", "Show volumes flags"),
					FNewToolMenuDelegate::CreateStatic(&FillShowMenuStatic, ShowVolumesMenu, 2), false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Volumes"));
			}

			// Show Layers sub-menu is dynamically generated when the user enters 'show' menu
			if (!UWorld::IsPartitionedWorld(World))
			{
				Section.AddSubMenu("ShowLayers", LOCTEXT("ShowLayersMenu", "Layers"), LOCTEXT("ShowLayersMenu_ToolTip", "Show layers flags"),
					FNewToolMenuDelegate::CreateStatic(&SLevelViewportToolBar::FillShowLayersMenu, Viewport), false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Layers"));
			}

			// Show Sprites sub-menu
			{
				TArray< FLevelViewportCommands::FShowMenuCommand > ShowSpritesMenu;

				// 'Show All' and 'Hide All' buttons
				ShowSpritesMenu.Add(FLevelViewportCommands::FShowMenuCommand(Actions.ShowAllSprites, ShowAllLabel));
				ShowSpritesMenu.Add(FLevelViewportCommands::FShowMenuCommand(Actions.HideAllSprites, HideAllLabel));

				// Get each show flag command and put them in their corresponding groups
				ShowSpritesMenu += Actions.ShowSpriteCommands;

				Section.AddSubMenu("ShowSprites", LOCTEXT("ShowSpritesMenu", "Sprites"), LOCTEXT("ShowSpritesMenu_ToolTip", "Show sprites flags"),
					FNewToolMenuDelegate::CreateStatic(&FillShowMenuStatic, ShowSpritesMenu, 2), false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.Sprites"));
			}

			// Show 'Foliage types' sub-menu is dynamically generated when the user enters 'show' menu
			{
				Section.AddSubMenu("ShowFoliageTypes", LOCTEXT("ShowFoliageTypesMenu", "Foliage Types"), LOCTEXT("ShowFoliageTypesMenu_ToolTip", "Show/hide specific foliage types"),
					FNewToolMenuDelegate::CreateStatic(&SLevelViewportToolBar::FillShowFoliageTypesMenu, Viewport), false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.FoliageTypes"));
			}

			// Show 'HLODs' sub-menu is dynamically generated when the user enters 'show' menu
			if (World->IsPartitionedWorld())
			{
				Section.AddSubMenu("ShowHLODsMenu", LOCTEXT("ShowHLODsMenu", "HLODs"), LOCTEXT("ShowHLODsMenu_ToolTip", "Settings for HLODs in editor"),
					FNewToolMenuDelegate::CreateSP(this, &SLevelViewportToolBar::FillShowHLODsMenu), false, FSlateIcon(FAppStyle::Get().GetStyleSetName(), "ShowFlagsMenu.SubMenu.HLODs"));
			}
		}
	}
}

EVisibility SLevelViewportToolBar::GetViewModeOptionsVisibility() const
{
	const FLevelEditorViewportClient& ViewClient = Viewport.Pin()->GetLevelViewportClient();
	if (ViewClient.GetViewMode() == VMI_MeshUVDensityAccuracy || ViewClient.GetViewMode() == VMI_MaterialTextureScaleAccuracy || ViewClient.GetViewMode() == VMI_RequiredTextureResolution)
	{
		return EVisibility::SelfHitTestInvisible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FText SLevelViewportToolBar::GetViewModeOptionsMenuLabel() const
{
	Viewport.Pin()->OnFloatingButtonClicked();
	const FLevelEditorViewportClient& ViewClient = Viewport.Pin()->GetLevelViewportClient();
	return ::GetViewModeOptionsMenuLabel(ViewClient.GetViewMode());
}


TSharedRef<SWidget> SLevelViewportToolBar::GenerateViewModeOptionsMenu() const
{
	Viewport.Pin()->OnFloatingButtonClicked();
	FLevelEditorViewportClient& ViewClient = Viewport.Pin()->GetLevelViewportClient();
	const UWorld* World = ViewClient.GetWorld();
	return BuildViewModeOptionsMenu(Viewport.Pin()->GetCommandList(), ViewClient.GetViewMode(), World ? World->GetFeatureLevel() : GMaxRHIFeatureLevel, ViewClient.GetViewModeParamNameMap());
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateFOVMenu() const
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
					.Value( this, &SLevelViewportToolBar::OnGetFOVValue )
					.OnValueChanged( const_cast<SLevelViewportToolBar*>(this), &SLevelViewportToolBar::OnFOVValueChanged )
				]
			]
		];
}

float SLevelViewportToolBar::OnGetFOVValue( ) const
{
	return Viewport.Pin()->GetLevelViewportClient().ViewFOV;
}

void SLevelViewportToolBar::OnFOVValueChanged( float NewValue )
{
	bool bUpdateStoredFOV = true;
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	if (ViewportClient.GetActiveActorLock().IsValid())
	{
		ACameraActor* CameraActor = Cast< ACameraActor >( ViewportClient.GetActiveActorLock().Get() );
		if( CameraActor != NULL )
		{
			CameraActor->GetCameraComponent()->FieldOfView = NewValue;
			bUpdateStoredFOV = false;
		}
	}

	if ( bUpdateStoredFOV )
	{
		ViewportClient.FOVAngle = NewValue;
	}

	ViewportClient.ViewFOV = NewValue;
	ViewportClient.Invalidate();
}

TSharedRef<SWidget> SLevelViewportToolBar::GenerateFarViewPlaneMenu() const
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
					.Value(this, &SLevelViewportToolBar::OnGetFarViewPlaneValue)
					.OnValueChanged(const_cast<SLevelViewportToolBar*>(this), &SLevelViewportToolBar::OnFarViewPlaneValueChanged)
				]
			]
		];
}

float SLevelViewportToolBar::OnGetFarViewPlaneValue() const
{
	return Viewport.Pin()->GetLevelViewportClient().GetFarClipPlaneOverride();
}

void SLevelViewportToolBar::OnFarViewPlaneValueChanged( float NewValue )
{
	Viewport.Pin()->GetLevelViewportClient().OverrideFarClipPlane(NewValue);
}

void SLevelViewportToolBar::FillShowLayersMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{	
	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportLayers");
		Section.AddMenuEntry(FLevelViewportCommands::Get().ShowAllLayers, LOCTEXT("ShowAllLabel", "Show All"));
		Section.AddMenuEntry(FLevelViewportCommands::Get().HideAllLayers, LOCTEXT("HideAllLabel", "Hide All"));
	}

	if( Viewport.IsValid() )
	{
		TSharedRef<SLevelViewport> ViewportRef = Viewport.Pin().ToSharedRef();
		{
			FToolMenuSection& Section = Menu->AddSection("LevelViewportLayers2");
			// Get all the layers and create an entry for each of them
			TArray<FName> AllLayerNames;
			ULayersSubsystem* Layers = GEditor->GetEditorSubsystem<ULayersSubsystem>();
			Layers->AddAllLayerNamesTo(AllLayerNames);

			for( int32 LayerIndex = 0; LayerIndex < AllLayerNames.Num(); ++LayerIndex )
			{
				const FName LayerName = AllLayerNames[LayerIndex];
				//const FString LayerNameString = LayerName;

				FUIAction Action( FExecuteAction::CreateSP( ViewportRef, &SLevelViewport::ToggleShowLayer, LayerName ),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP( ViewportRef, &SLevelViewport::IsLayerVisible, LayerName ) );

				Section.AddMenuEntry(NAME_None, FText::FromName( LayerName ), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
			}
		}
	}
}

static TMap<FName, TArray<UFoliageType*>> GroupFoliageByOuter(const TArray<UFoliageType*> FoliageList)
{
	TMap<FName, TArray<UFoliageType*>> Result;

	for (UFoliageType* FoliageType : FoliageList)
	{
		if (FoliageType->IsAsset())
		{
			Result.FindOrAdd(NAME_None).Add(FoliageType);
		}
		else
		{
			FName LevelName = FoliageType->GetOutermost()->GetFName();
			Result.FindOrAdd(LevelName).Add(FoliageType);
		}
	}

	Result.KeySort([](const FName& A, const FName& B) { return (A.LexicalLess(B) && B != NAME_None); });
	return Result;
}

void SLevelViewportToolBar::FillShowFoliageTypesMenu(UToolMenu* Menu, TWeakPtr<class SLevelViewport> Viewport)
{
	auto ViewportPtr = Viewport.Pin();
	if (!ViewportPtr.IsValid())
	{
		return;
	}
		
	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportFoliageMeshes");
		// Map 'Show All' and 'Hide All' commands
		FUIAction ShowAllFoliage(FExecuteAction::CreateSP(ViewportPtr.ToSharedRef(), &SLevelViewport::ToggleAllFoliageTypes, true));
		FUIAction HideAllFoliage(FExecuteAction::CreateSP(ViewportPtr.ToSharedRef(), &SLevelViewport::ToggleAllFoliageTypes, false));
		
		Section.AddMenuEntry("ShowAll", LOCTEXT("ShowAllLabel", "Show All"), FText::GetEmpty(), FSlateIcon(), ShowAllFoliage);
		Section.AddMenuEntry("HideAll", LOCTEXT("HideAllLabel", "Hide All"), FText::GetEmpty(), FSlateIcon(), HideAllFoliage);
	}
	
	// Gather all foliage types used in this world and group them by sub-levels
	auto AllFoliageMap = GroupFoliageByOuter(GEditor->GetFoliageTypesInWorld(ViewportPtr->GetWorld()));

	for (auto& FoliagePair : AllFoliageMap)
	{
		// Name foliage group by an outer sub-level name, or empty if foliage type is an asset
		FText EntryName = (FoliagePair.Key == NAME_None ? FText::GetEmpty() : FText::FromName(FPackageName::GetShortFName(FoliagePair.Key)));
		FToolMenuSection& Section = Menu->AddSection(NAME_None, EntryName);

		TArray<UFoliageType*>& FoliageList = FoliagePair.Value;
		for (UFoliageType* FoliageType : FoliageList)
		{
			FName MeshName = FoliageType->GetDisplayFName();
			TWeakObjectPtr<UFoliageType> FoliageTypePtr = FoliageType;

			FUIAction Action(
				FExecuteAction::CreateSP(ViewportPtr.ToSharedRef(), &SLevelViewport::ToggleShowFoliageType, FoliageTypePtr),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(ViewportPtr.ToSharedRef(), &SLevelViewport::IsFoliageTypeVisible, FoliageTypePtr));

			Section.AddMenuEntry(NAME_None, FText::FromName(MeshName), FText::GetEmpty(), FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
		}
	}
}

double SLevelViewportToolBar::OnGetHLODInEditorMinDrawDistanceValue() const
{
	IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	return WorldPartitionEditorModule ? WorldPartitionEditorModule->GetHLODInEditorMinDrawDistance() : 0;
}

void SLevelViewportToolBar::OnHLODInEditorMinDrawDistanceValueChanged(double NewValue) const
{
	IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	if (WorldPartitionEditorModule)
	{
		WorldPartitionEditorModule->SetHLODInEditorMinDrawDistance(NewValue);
		GEditor->RedrawLevelEditingViewports(true);
	}
}

double SLevelViewportToolBar::OnGetHLODInEditorMaxDrawDistanceValue() const
{
	IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	return WorldPartitionEditorModule ? WorldPartitionEditorModule->GetHLODInEditorMaxDrawDistance() : 0;
}

void SLevelViewportToolBar::OnHLODInEditorMaxDrawDistanceValueChanged(double NewValue) const
{
	IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	if (WorldPartitionEditorModule)
	{
		WorldPartitionEditorModule->SetHLODInEditorMaxDrawDistance(NewValue);
		GEditor->RedrawLevelEditingViewports(true);
	}
}

void SLevelViewportToolBar::FillShowHLODsMenu(UToolMenu* Menu) const
{
	auto ViewportPtr = Viewport.Pin();
	if (!ViewportPtr.IsValid())
	{
		return;
	}

	UWorld* World = ViewportPtr->GetWorld();
	UWorldPartition* WorldPartition = World ? World->GetWorldPartition() : nullptr;
	IWorldPartitionEditorModule* WorldPartitionEditorModule = FModuleManager::GetModulePtr<IWorldPartitionEditorModule>("WorldPartitionEditor");
	
	if (WorldPartition == nullptr || WorldPartitionEditorModule == nullptr)
	{
		return;
	}

	FText HLODInEditorDisallowedReason;
	const bool bHLODInEditorAllowed = WorldPartitionEditorModule->IsHLODInEditorAllowed(World, &HLODInEditorDisallowedReason);

	// Show HLODs
	{
		FToolUIAction UIAction;
		UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([WorldPartitionEditorModule](const FToolMenuContext& InContext) { WorldPartitionEditorModule->SetShowHLODsInEditor(!WorldPartitionEditorModule->GetShowHLODsInEditor()); });
		UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([bHLODInEditorAllowed](const FToolMenuContext& InContext) { return bHLODInEditorAllowed; });
		UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([WorldPartitionEditorModule](const FToolMenuContext& InContext) { return WorldPartitionEditorModule->GetShowHLODsInEditor() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
		FToolMenuEntry MenuEntry = FToolMenuEntry::InitMenuEntry("ShowHLODs", LOCTEXT("ShowHLODs", "Show HLODs"), bHLODInEditorAllowed ? LOCTEXT("ShowHLODsToolTip", "Show/Hide HLODs") : HLODInEditorDisallowedReason, FSlateIcon(), UIAction, EUserInterfaceActionType::ToggleButton);
		Menu->AddMenuEntry(NAME_None, MenuEntry);
	}

	// Show HLODs Over Loaded Regions
	{
		FToolUIAction UIAction;
		UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([WorldPartitionEditorModule](const FToolMenuContext& InContext) { WorldPartitionEditorModule->SetShowHLODsOverLoadedRegions(!WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions()); });
		UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([bHLODInEditorAllowed](const FToolMenuContext& InContext) { return bHLODInEditorAllowed; });
		UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([WorldPartitionEditorModule](const FToolMenuContext& InContext) { return WorldPartitionEditorModule->GetShowHLODsOverLoadedRegions() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; });
		FToolMenuEntry ShowHLODsEntry = FToolMenuEntry::InitMenuEntry("ShowHLODsOverLoadedRegions", LOCTEXT("ShowHLODsOverLoadedRegions", "Show HLODs Over Loaded Regions"), bHLODInEditorAllowed ? LOCTEXT("ShowHLODsOverLoadedRegions_ToolTip", "Show/Hide HLODs over loaded actors or regions") : HLODInEditorDisallowedReason, FSlateIcon(), UIAction, EUserInterfaceActionType::ToggleButton);
		Menu->AddMenuEntry(NAME_None, ShowHLODsEntry);
	}

	// Min/Max Draw Distance
	{
		const double MinDrawDistanceMinValue = 0;
		const double MinDrawDistanceMaxValue = 102400;

		const double MaxDrawDistanceMinValue = 0;
		const double MaxDrawDistanceMaxValue = 1638400;

		TSharedRef<SSpinBox<double>> MinDrawDistanceSpinBox = SNew(SSpinBox<double>)
			.MinValue(MinDrawDistanceMinValue)
			.MaxValue(MinDrawDistanceMaxValue)
			.IsEnabled(bHLODInEditorAllowed)
			.Value(this, &SLevelViewportToolBar::OnGetHLODInEditorMinDrawDistanceValue)
			.OnValueChanged(this, &SLevelViewportToolBar::OnHLODInEditorMinDrawDistanceValueChanged)
			.ToolTipText(bHLODInEditorAllowed ? LOCTEXT("HLODsInEditor_MinDrawDistance_Tooltip", "Sets the minimum distance at which HLOD will be rendered") : HLODInEditorDisallowedReason)
			.OnBeginSliderMovement_Lambda([this]()
			{
				// Disable Slate throttling during slider drag to ensure immediate updates while moving the slider.
				FSlateThrottleManager::Get().DisableThrottle(true);
			})
			.OnEndSliderMovement_Lambda([this](float)
			{
				FSlateThrottleManager::Get().DisableThrottle(false);
			});

		TSharedRef<SSpinBox<double>> MaxDrawDistanceSpinBox = SNew(SSpinBox<double>)
			.MinValue(MaxDrawDistanceMinValue)
			.MaxValue(MaxDrawDistanceMaxValue)
			.IsEnabled(bHLODInEditorAllowed)
			.Value(this, &SLevelViewportToolBar::OnGetHLODInEditorMaxDrawDistanceValue)
			.OnValueChanged(this, &SLevelViewportToolBar::OnHLODInEditorMaxDrawDistanceValueChanged)
			.ToolTipText(bHLODInEditorAllowed ? LOCTEXT("HLODsInEditor_MaxDrawDistance_Tooltip", "Sets the maximum distance at which HLODs will be rendered") : HLODInEditorDisallowedReason)
			.OnBeginSliderMovement_Lambda([this]()
			{
				// Disable Slate throttling during slider drag to ensure immediate updates while moving the slider.
				FSlateThrottleManager::Get().DisableThrottle(true);
			})
			.OnEndSliderMovement_Lambda([this](float)
			{
				FSlateThrottleManager::Get().DisableThrottle(false);
			});

		auto CreateDrawDistanceWidget = [](TSharedRef<SSpinBox<double>> InSpinBoxWidget)
		{
			return SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SBox)
						.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
						.WidthOverride(100.0f)
						[
							SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("Menu.WidgetBorder"))
								.Padding(FMargin(1.0f))
								[
									InSpinBoxWidget
								]
						]
				];
		};
			
		FToolMenuEntry MinDrawDistanceMenuEntry = FToolMenuEntry::InitWidget("Min Draw Distance", CreateDrawDistanceWidget(MinDrawDistanceSpinBox), LOCTEXT("MinDrawDistance", "Min Draw Distance"));
		Menu->AddMenuEntry(NAME_None, MinDrawDistanceMenuEntry);

		FToolMenuEntry MaxDrawDistanceMenuEntry = FToolMenuEntry::InitWidget("Max Draw Distance", CreateDrawDistanceWidget(MaxDrawDistanceSpinBox), LOCTEXT("MaxDrawDistance", "Max Draw Distance"));
		Menu->AddMenuEntry(NAME_None, MaxDrawDistanceMenuEntry);
	}
}

TWeakObjectPtr<UWorld> SLevelViewportToolBar::GetWorld() const
{
	if (Viewport.IsValid())
	{
		return Viewport.Pin()->GetWorld();
	}
	return NULL;
}

TSharedPtr<FExtender> SLevelViewportToolBar::GetViewMenuExtender()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
	return LevelEditorModule.GetMenuExtensibilityManager()->GetAllExtenders();
}

void SLevelViewportToolBar::FillViewMenu(UToolMenu* Menu)
{
	FToolMenuInsert InsertPosition("ViewMode", EToolMenuInsertType::After);

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportDeferredRendering", LOCTEXT("DeferredRenderingHeader", "Deferred Rendering"), InsertPosition);
	}

	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeBufferViewMode",
			LOCTEXT("VisualizeBufferViewModeDisplayName", "Buffer Visualization"),
			LOCTEXT("BufferVisualizationMenu_ToolTip", "Select a mode for buffer visualization"),
			FNewMenuDelegate::CreateStatic(&FBufferVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
				{
					const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
					const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
					check(ViewportClient.IsValid());
					return ViewportClient->IsViewModeEnabled(VMI_VisualizeBuffer);
				})
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeBufferMode")
		);
	}

	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeNaniteViewMode",
			LOCTEXT("VisualizeNaniteViewModeDisplayName", "Nanite Visualization"),
			LOCTEXT("NaniteVisualizationMenu_ToolTip", "Select a mode for Nanite visualization"),
			FNewMenuDelegate::CreateStatic(&FNaniteVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
				{
					const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
					const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
					check(ViewportClient.IsValid());
					return ViewportClient->IsViewModeEnabled(VMI_VisualizeNanite);
				})
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeNaniteMode")
		);
	}

	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeLumenViewMode",
			LOCTEXT("VisualizeLumenViewModeDisplayName", "Lumen"),
			LOCTEXT("LumenVisualizationMenu_ToolTip", "Select a mode for Lumen visualization"),
			FNewMenuDelegate::CreateStatic(&FLumenVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeLumen);
					})
			),
			EUserInterfaceActionType::RadioButton,
						/* bInOpenSubMenuOnClick = */ false,
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeLumenMode")
						);
	}

	if (Substrate::IsSubstrateEnabled())
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeSubstrateViewMode",
			LOCTEXT("VisualizeSubstrateViewModeDisplayName", "Substrate"),
			LOCTEXT("SubstrateVisualizationMenu_ToolTip", "Select a mode for Substrate visualization"),
			FNewMenuDelegate::CreateStatic(&FSubstrateVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeSubstrate);
					})
			),
			EUserInterfaceActionType::RadioButton,
						/* bInOpenSubMenuOnClick = */ false,
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeSubstrateMode")
						);
	}

	if (IsGroomEnabled())
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeGroomViewMode",
			LOCTEXT("VisualizeGroomViewModeDisplayName", "Groom"),
			LOCTEXT("GroomVisualizationMenu_ToolTip", "Select a mode for Groom visualization"),
			FNewMenuDelegate::CreateStatic(&FGroomVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeGroom);
					})
			),
			EUserInterfaceActionType::RadioButton,
						/* bInOpenSubMenuOnClick = */ false,
						FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeGroomMode")
						);
	}

	{
		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"VisualizeVirtualShadowMapViewMode",
			LOCTEXT("VisualizeVirtualShadowMapViewModeDisplayName", "Virtual Shadow Map"),
			LOCTEXT("VirtualShadowMapVisualizationMenu_ToolTip", "Select a mode for virtual shadow map visualization. Select a light component in the world outliner to visualize that light."),
			FNewMenuDelegate::CreateStatic(&FVirtualShadowMapVisualizationMenuCommands::BuildVisualisationSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
					{
						const TSharedRef<SEditorViewport> ViewportRef = Viewport.Pin().ToSharedRef();
						const TSharedPtr<FEditorViewportClient> ViewportClient = ViewportRef->GetViewportClient();
						check(ViewportClient.IsValid());
						return ViewportClient->IsViewModeEnabled(VMI_VisualizeVirtualShadowMap);
					})
			),
			EUserInterfaceActionType::RadioButton,
			/* bInOpenSubMenuOnClick = */ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.VisualizeVirtualShadowMapMode")
			);
	}

	{
		auto BuildActorColorationMenu = [this](UToolMenu* Menu, SLevelViewportToolBar* Toolbar)
		{
			FToolMenuSection& SubMenuSection = Menu->AddSection("LevelViewportActorColoration", LOCTEXT("ActorColorationHeader", "Actor Coloration"));

			TArray<FActorPrimitiveColorHandler::FPrimitiveColorHandler> PrimitiveColorHandlers;
			FActorPrimitiveColorHandler::Get().GetRegisteredPrimitiveColorHandlers(PrimitiveColorHandlers);

			for (const FActorPrimitiveColorHandler::FPrimitiveColorHandler& PrimitiveColorHandler : PrimitiveColorHandlers)
			{
				SubMenuSection.AddMenuEntry(
					NAME_None,
					PrimitiveColorHandler.HandlerText,
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, PrimitiveColorHandler]()
						{
							if (FLevelEditorViewportClient* ViewportClient = GetLevelViewportClient())
							{
								const bool bActorColorationEnabled = ViewportClient->HandleIsShowFlagEnabled(FEngineShowFlags::EShowFlag::SF_ActorColoration);

								if (PrimitiveColorHandler.HandlerName.IsNone())
								{
									if (bActorColorationEnabled)
									{
										ViewportClient->HandleToggleShowFlag(FEngineShowFlags::EShowFlag::SF_ActorColoration);
									}
								}
								else
								{
									if (!bActorColorationEnabled)
									{
										ViewportClient->HandleToggleShowFlag(FEngineShowFlags::EShowFlag::SF_ActorColoration);
									}

									FActorPrimitiveColorHandler::Get().SetActivePrimitiveColorHandler(PrimitiveColorHandler.HandlerName, GWorld);
								}
							}
						}),
						FCanExecuteAction::CreateLambda([this]()
						{
							if (FLevelEditorViewportClient* ViewportClient = GetLevelViewportClient())
							{
								return true;
							}
							return false;
						}),
						FGetActionCheckState::CreateLambda([this, PrimitiveColorHandler]()
						{
							if (FLevelEditorViewportClient* ViewportClient = GetLevelViewportClient())
							{
								const bool bActorColorationEnabled = ViewportClient->HandleIsShowFlagEnabled(FEngineShowFlags::EShowFlag::SF_ActorColoration);

								if (PrimitiveColorHandler.HandlerName.IsNone())
								{
									return bActorColorationEnabled ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
								}
								else
								{
									if (bActorColorationEnabled)
									{
										return FActorPrimitiveColorHandler::Get().GetActivePrimitiveColorHandler() == PrimitiveColorHandler.HandlerName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
									}
								}								
							}

							return ECheckBoxState::Unchecked;
						})
					),
					EUserInterfaceActionType::RadioButton
				);
			}
		};

		FToolMenuSection& Section = Menu->FindOrAddSection("ViewMode");
		Section.AddSubMenu(
			"ActorColoration",
			LOCTEXT("ActorColorationDisplayName", "Actor Coloration"),
			LOCTEXT("ActorColorationMenu_ToolTip", "Override Actor Coloration mode"),
			FNewToolMenuDelegate::CreateLambda(BuildActorColorationMenu, this),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]()
				{
					if (FLevelEditorViewportClient* ViewportClient = GetLevelViewportClient())
					{
						return ViewportClient->HandleIsShowFlagEnabled(FEngineShowFlags::EShowFlag::SF_ActorColoration);
					}
					return false;
				})
			),
			EUserInterfaceActionType::RadioButton,
			/*bInOpenSubMenuOnClick=*/ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LODColorationMode")
		);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("LevelViewportLandscape", LOCTEXT("LandscapeHeader", "Landscape"), InsertPosition);

		auto BuildLandscapeLODMenu = [](UToolMenu* Menu, SLevelViewportToolBar* Toolbar)
		{
			FToolMenuSection& SubMenuSection = Menu->AddSection("LevelViewportLandScapeLOD", LOCTEXT("LandscapeLODHeader", "Landscape LOD"));

			SubMenuSection.AddMenuEntry(
				"LandscapeLODAuto",
				LOCTEXT("LandscapeLODAuto", "Auto"),
				FText(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, -1),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, -1)
				),
				EUserInterfaceActionType::RadioButton
			);

			SubMenuSection.AddSeparator("LandscapeLODSeparator");

			static const FText FormatString = LOCTEXT("LandscapeLODFixed", "Fixed at {0}");
			for (int32 i = 0; i < 8; ++i)
			{
				SubMenuSection.AddMenuEntry(
					NAME_None,
					FText::Format(FormatString, FText::AsNumber(i)),
					FText(),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(Toolbar, &SLevelViewportToolBar::OnLandscapeLODChanged, i),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(Toolbar, &SLevelViewportToolBar::IsLandscapeLODSettingChecked, i)
					),
					EUserInterfaceActionType::RadioButton
				);
			}
		};

		Section.AddSubMenu(
			"LandscapeLOD",
			LOCTEXT("LandscapeLODDisplayName", "LOD"),
			LOCTEXT("LandscapeLODMenu_ToolTip", "Override Landscape LOD in this viewport"),
			FNewToolMenuDelegate::CreateLambda(BuildLandscapeLODMenu, this),
			/*bInOpenSubMenuOnClick=*/ false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LOD")
		);
	}
}


void SLevelViewportToolBar::OnDisableRealtimeOverride()
{
	if (TSharedPtr<SLevelViewport> ViewportPinned = Viewport.Pin())
	{
		ViewportPinned->GetLevelViewportClient().PopRealtimeOverride();
	}
}

bool SLevelViewportToolBar::IsRealtimeOverrideToggleVisible() const
{
	if (TSharedPtr<SLevelViewport> ViewportPinned = Viewport.Pin())
	{
		return ViewportPinned->GetLevelViewportClient().IsRealtimeOverrideSet();
	}

	return false;
}

FText SLevelViewportToolBar::GetRealtimeOverrideTooltip() const
{
	if (TSharedPtr<SLevelViewport> ViewportPinned = Viewport.Pin())
	{
		return FText::Format(LOCTEXT("DisableRealtimeOverrideToolTip", "Realtime is currently overridden by \"{0}\".  Click to remove that override"), ViewportPinned->GetLevelViewportClient().GetRealtimeOverrideMessage());
	}

	return FText::GetEmpty();
}

float SLevelViewportToolBar::GetTransformToolbarWidth() const
{
	if (TransformToolbar)
	{
		const float TransformToolbarWidth = TransformToolbar->GetDesiredSize().X;
		if (TransformToolbar_CachedMaxWidth == 0.0f)
		{
			TransformToolbar_CachedMaxWidth = TransformToolbarWidth;
		}

		{
			FLevelEditorViewportClient* LevelEditorViewportClient = GetLevelViewportClient();
			if (LevelEditorViewportClient && LevelEditorViewportClient->Viewport)
			{
				const float ViewportWidth = static_cast<float>(LevelEditorViewportClient->Viewport->GetSizeXY().X);
				const float ToolbarWidthMinusPreviousTransformToolbar = GetDesiredSize().X - TransformToolbar_CachedMaxWidth;
				const float ToolbarWidthEstimate = ToolbarWidthMinusPreviousTransformToolbar + TransformToolbarWidth;

				float DpiScale = 1.0f;
				{
					IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
					const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
					if (MainFrameParentWindow.IsValid())
					{
						DpiScale = MainFrameParentWindow->GetDPIScaleFactor();
					}
				}

				const float OverflowWidth = ToolbarWidthEstimate * DpiScale - ViewportWidth;
				if (OverflowWidth > 0.0f)
				{
					// There isn't enough space in the viewport to show the toolbar!
					// Try and shrink the transform toolbar (which has an overflow area) to make things fit
					TransformToolbar_CachedMaxWidth = FMath::Max(FMath::Min(4.0f, TransformToolbarWidth), TransformToolbarWidth - OverflowWidth / DpiScale);
				}
				else
				{
					TransformToolbar_CachedMaxWidth = TransformToolbarWidth;
				}
			}
		}
		
		return TransformToolbar_CachedMaxWidth;
	}

	return 0.0f;
}

bool SLevelViewportToolBar::IsLandscapeLODSettingChecked(int32 Value) const
{
	return Viewport.Pin()->GetLevelViewportClient().LandscapeLODOverride == Value;
}

void SLevelViewportToolBar::OnLandscapeLODChanged(int32 NewValue)
{
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	ViewportClient.LandscapeLODOverride = NewValue;
	ViewportClient.Invalidate();
}


FReply SLevelViewportToolBar::OnRealtimeWarningClicked()
{
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	ViewportClient.SetRealtime(true);

	return FReply::Handled();
}

EVisibility SLevelViewportToolBar::GetRealtimeWarningVisibility() const
{
	FLevelEditorViewportClient& ViewportClient = Viewport.Pin()->GetLevelViewportClient();
	// If the viewport is not realtime and there is no override then realtime is off
	return !ViewportClient.IsRealtime() && !ViewportClient.IsRealtimeOverrideSet() && ViewportClient.IsPerspective() ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SLevelViewportToolBar::GetScalabilityWarningLabel() const
{
	const int32 QualityLevel = Scalability::GetQualityLevels().GetMinQualityLevel();
	if (QualityLevel >= 0)
	{
		return FText::Format(LOCTEXT("ScalabilityWarning", "Scalability: {0}"), Scalability::GetScalabilityNameFromQualityLevel(QualityLevel));
	}
	
	return FText::GetEmpty();
}

EVisibility SLevelViewportToolBar::GetScalabilityWarningVisibility() const
{
	//This method returns magic numbers. 3 means epic
	return GetDefault<UEditorPerformanceSettings>()->bEnableScalabilityWarningIndicator && Scalability::GetQualityLevels().GetMinQualityLevel() != 3 ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SLevelViewportToolBar::GetScalabilityWarningMenuContent() const
{
	return
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(SScalabilitySettings)
		];
}

FLevelEditorViewportClient* ULevelViewportToolBarContext::GetLevelViewportClient()
{
	if (LevelViewportToolBarWidget.IsValid())
	{
		return LevelViewportToolBarWidget.Pin()->GetLevelViewportClient();
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
