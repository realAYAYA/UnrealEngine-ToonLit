// Copyright Epic Games, Inc. All Rights Reserved.

#include "SProjectDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "GameProjectGenerationModule.h"
#include "TemplateCategory.h"
#include "SProjectBrowser.h"
#include "Widgets/Layout/SSeparator.h"
#include "IDocumentation.h"
#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"
#include "TemplateItem.h"
#include "SourceCodeNavigation.h"
#include "GameProjectUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "Internationalization/BreakIterator.h"
#include "Settings/EditorSettings.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "HardwareTargetingModule.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "GameProjectGenerationLog.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "Dialogs/SOutputLogDialog.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "ProjectDescriptor.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "SWarningOrErrorBox.h"
#include "SGetSuggestedIDEWidget.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "SProjectDialog.h"
#include "LauncherPlatformModule.h"
#include "SPrimaryButton.h"
#include "Styling/StyleColors.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "GameProjectGeneration"

namespace NewProjectDialogDefs
{
	constexpr float MajorItemWidth = 304;
	constexpr float MajorItemHeight = 104;
	constexpr float TemplateTileHeight = 153;
	constexpr float TemplateTileWidth = 102;
	constexpr float ThumbnailSize = 64.0f, ThumbnailPadding = 5.f;
	const FName DefaultCategoryName = "Games";
	const FName BlankCategoryKey = "Default";
}

TUniquePtr<FSlateBrush> SProjectDialog::CustomTemplateBrush;

class SMajorCategoryTile : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SMajorCategoryTile) {}
		SLATE_ATTRIBUTE(bool, IsSelected)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, TSharedPtr<FTemplateCategory> Item)
	{
		IsSelected = InArgs._IsSelected;

		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image(Item->Icon)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(STextBlock)
				.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
				.ColorAndOpacity(FLinearColor(1, 1, 1, .9f))
				.TransformPolicy(ETextTransformPolicy::ToUpper)
				.ShadowOffset(FVector2D(1, 1))
				.ShadowColorAndOpacity(FLinearColor(0, 0, 0, .75))
				.Text(Item->DisplayName)
				.WrapTextAt(250.0f)
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.Image(this, &SMajorCategoryTile::GetSelectionOutlineBrush)
			]
		];
	}

private:
	const FSlateBrush* GetSelectionOutlineBrush() const
	{
		const bool bIsSelected = IsSelected.Get();
		const bool bIsTileHovered = IsHovered();

		if (bIsSelected && bIsTileHovered)
		{
			static const FName SelectedHover("ProjectBrowser.ProjectTile.SelectedHoverBorder");
			return FAppStyle::Get().GetBrush(SelectedHover);
		}
		else if (bIsSelected)
		{
			static const FName Selected("ProjectBrowser.ProjectTile.SelectedBorder");
			return FAppStyle::Get().GetBrush(Selected);
		}
		else if (bIsTileHovered)
		{
			static const FName Hovered("ProjectBrowser.ProjectTile.HoverBorder");
			return FAppStyle::Get().GetBrush(Hovered);
		}

		return FStyleDefaults::GetNoBrush();
	}
private:
	TAttribute<bool> IsSelected;
};


/** Slate tile widget for template projects */
class STemplateTile : public STableRow<TSharedPtr<FTemplateItem>>
{
public:
	SLATE_BEGIN_ARGS( STemplateTile ){}
		SLATE_ARGUMENT(TSharedPtr<FTemplateItem>, Item)
	SLATE_END_ARGS()

private:
	TWeakPtr<FTemplateItem> Item;

public:
	/** Static build function */
	static TSharedRef<ITableRow> BuildTile(TSharedPtr<FTemplateItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		if (!ensure(Item.IsValid()))
		{
			return SNew(STableRow<TSharedPtr<FTemplateItem>>, OwnerTable);
		}

		return SNew(STemplateTile, OwnerTable).Item(Item);
	}

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable )
	{
		check(InArgs._Item.IsValid())
		Item = InArgs._Item;

		STableRow::FArguments TableRowArguments;
		TableRowArguments._SignalSelectionMode = ETableRowSignalSelectionMode::Instantaneous;

		STableRow::Construct(
			TableRowArguments
			.Style(FAppStyle::Get(), "ProjectBrowser.TableRow")
			.Padding(2.0f)
			.Content()
			[
				SNew(SBorder)
				.Padding(FMargin(0.0f, 0.0f, 5.0f, 5.0f))
				.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.DropShadow"))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SNew(SVerticalBox)
						// Thumbnail
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(102.0f)
							.HeightOverride(102.0f)
							[
								SNew(SBorder)
								.Padding(0.0f)
								.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.ThumbnailAreaBackground"))
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(SImage)
									.Image(this, &STemplateTile::GetThumbnail)
									.DesiredSizeOverride(InArgs._Item->bThumbnailAsIcon ? TOptional<FVector2D>() : FVector2D(NewProjectDialogDefs::ThumbnailSize, NewProjectDialogDefs::ThumbnailSize))
								]
							]
						]
						// Name
						+ SVerticalBox::Slot()
						[
							SNew(SBorder)
							.Padding(FMargin(NewProjectDialogDefs::ThumbnailPadding, 0))
							.VAlign(VAlign_Top)
							.Padding(FMargin(3.0f, 3.0f))
							.BorderImage_Lambda
							(
								[this]()
								{
									const bool bIsSelected = IsSelected();
									const bool bIsRowHovered = IsHovered();

									if (bIsSelected && bIsRowHovered)
									{
										static const FName SelectedHover("ProjectBrowser.ProjectTile.NameAreaSelectedHoverBackground");
										return FAppStyle::Get().GetBrush(SelectedHover);
									}
									else if (bIsSelected)
									{
										static const FName Selected("ProjectBrowser.ProjectTile.NameAreaSelectedBackground");
										return FAppStyle::Get().GetBrush(Selected);
									}
									else if (bIsRowHovered)
									{
										static const FName Hovered("ProjectBrowser.ProjectTile.NameAreaHoverBackground");
										return FAppStyle::Get().GetBrush(Hovered);
									}

									return FAppStyle::Get().GetBrush("ProjectBrowser.ProjectTile.NameAreaBackground");
								}
							)
							[
								SNew(STextBlock)
								.Font(FAppStyle::Get().GetFontStyle("ProjectBrowser.ProjectTile.Font"))
								.WrapTextAt(NewProjectDialogDefs::TemplateTileWidth-4.0f)
								.LineBreakPolicy(FBreakIterator::CreateCamelCaseBreakIterator())
								.Text(InArgs._Item->Name)
								.ColorAndOpacity_Lambda
								(
									[this]()
									{
										const bool bIsSelected = IsSelected();
										const bool bIsRowHovered = IsHovered();

										if (bIsSelected || bIsRowHovered)
										{
											return FStyleColors::White;
										}

										return FSlateColor::UseForeground();
									}
								)
							]
						]
					]
					+ SOverlay::Slot()
					[
						SNew(SImage)
						.Visibility(EVisibility::HitTestInvisible)
						.Image_Lambda
						(
							[this]()
							{
								const bool bIsSelected = IsSelected();
								const bool bIsRowHovered = IsHovered();

								if (bIsSelected && bIsRowHovered)
								{
									static const FName SelectedHover("ProjectBrowser.ProjectTile.SelectedHoverBorder");
									return FAppStyle::Get().GetBrush(SelectedHover);
								}
								else if (bIsSelected)
								{
									static const FName Selected("ProjectBrowser.ProjectTile.SelectedBorder");
									return FAppStyle::Get().GetBrush(Selected);
								}
								else if (bIsRowHovered)
								{
									static const FName Hovered("ProjectBrowser.ProjectTile.HoverBorder");
									return FAppStyle::Get().GetBrush(Hovered);
								}

								return FStyleDefaults::GetNoBrush();
							}
						)
					]
				]
			],
			OwnerTable
		);
	}

private:

	/** Get this item's thumbnail or return the default */
	const FSlateBrush* GetThumbnail() const
	{
		TSharedPtr<FTemplateItem> ItemPtr = Item.Pin();
		if (ItemPtr.IsValid() && ItemPtr->Thumbnail.IsValid())
		{
			return ItemPtr->Thumbnail.Get();
		}
		return FAppStyle::GetBrush("UnrealDefaultThumbnail");
	}
	
};

/**
 * Simple widget used to display a folder path, and a name of a file
 */
class SFilepath : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SFilepath )
		: _IsReadOnly(false)
	{}
		/** Attribute specifying the text to display in the folder input */
		SLATE_ATTRIBUTE(FText, FolderPath)

		/** Attribute specifying the text to display in the name input */
		SLATE_ATTRIBUTE(FText, Name)

		SLATE_ATTRIBUTE(FText, WarningText)

		SLATE_ARGUMENT(bool, IsReadOnly)

		/** Event that is triggered when the browser for folder button is clicked */
		SLATE_EVENT(FOnClicked, OnBrowseForFolder)

		/** Events for when the name field is manipulated */
		SLATE_EVENT(FOnTextChanged, OnNameChanged)
		SLATE_EVENT(FOnTextCommitted, OnNameCommitted)
		
		/** Events for when the folder field is manipulated */
		SLATE_EVENT(FOnTextChanged, OnFolderChanged)
		SLATE_EVENT(FOnTextCommitted, OnFolderCommitted)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct( const FArguments& InArgs )
	{
		WarningText = InArgs._WarningText;

		ChildSlot
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 4.0f, 8.0f, 8.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ProjectLocation", "Project Location"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(595.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SEditableTextBox)
						.IsReadOnly(InArgs._IsReadOnly)
						.Text(InArgs._FolderPath)
						.OnTextChanged(InArgs._OnFolderChanged)
						.OnTextCommitted(InArgs._OnFolderCommitted)
					]
					+ SVerticalBox::Slot()
					.Padding(0.0f, 8.0f)
					[	
						SNew(SWarningOrErrorBox)
						.Visibility(this, &SFilepath::GetWarningVisibility)
						.IconSize(FVector2D(16,16))
						.Padding(FMargin(8.0f, 4.0f, 4.0f, 4.0f))
						.Message(WarningText)
						.MessageStyle(EMessageStyle::Error)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Top)
			.Padding(2.0f, 2.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(InArgs._OnBrowseForFolder)
				.ToolTipText(LOCTEXT("BrowseForFolder", "Browse for a folder"))
				.Visibility(InArgs._IsReadOnly ? EVisibility::Hidden : EVisibility::Visible)
				.ContentPadding(0.0f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.FolderClosed"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				.AutoWidth()
				.Padding(32.0f, 4.0f, 8.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ProjectName", "Project Name"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Top)
				[
					SNew(SBox)
					.WidthOverride(275.0f)
					[
						SNew(SEditableTextBox)
						.IsReadOnly(InArgs._IsReadOnly)
						.Text(InArgs._Name)
						.OnTextChanged(InArgs._OnNameChanged)
						.OnTextCommitted(InArgs._OnNameCommitted)
					]
				]
			]
		];
	}

private:
	EVisibility GetWarningVisibility() const
	{
		return WarningText.Get().IsEmpty() ? EVisibility::Hidden : EVisibility::HitTestInvisible;
	}
private:
	TAttribute<FText> WarningText;
};

void SProjectDialog::Construct(const FArguments& InArgs, EProjectDialogModeMode Mode)
{
	bLastGlobalValidityCheckSuccessful = true;
	bLastNameAndLocationValidityCheckSuccessful = true;

	PopulateTemplateCategories();

	ProjectBrowser = SNew(SProjectBrowser);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(16.0f, 8.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				MakeHybridView(Mode)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(-16.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(EOrientation::Orient_Horizontal)
				.Thickness(2.0f)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(145.0f)
				[
					SAssignNew(PathAreaSwitcher, SWidgetSwitcher)
					+SWidgetSwitcher::Slot()
					[
						MakeNewProjectPathArea()
					]
					+SWidgetSwitcher::Slot()
					[
						MakeOpenProjectPathArea()
					]
				]
			]
		]
	];

	RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateLambda(
		[this](double, float)
		{
			UpdateProjectFileValidity();
			return EActiveTimerReturnType::Continue;
		}));

	if (Mode == EProjectDialogModeMode::OpenProject)
	{
		PathAreaSwitcher->SetActiveWidgetIndex(1);
	}
	else if (Mode == EProjectDialogModeMode::Hybrid && ProjectBrowser->HasProjects())
	{
		// Select recent projects
		OnRecentProjectsClicked();
	}
	else if(!ProjectBrowser->HasProjects() || Mode == EProjectDialogModeMode::NewProject)
	{
		// Select the first template category
		MajorCategoryList->SetSelection(TemplateCategories[0]);
	}

}

SProjectDialog::~SProjectDialog()
{
	// remove any UTemplateProjectDefs we were keeping alive
	for (const TPair<FName, TArray<TSharedPtr<FTemplateItem>>>& Pair : Templates)
	{
		for (const TSharedPtr<FTemplateItem>& Template : Pair.Value)
		{
			if (Template->CodeTemplateDefs != nullptr)
			{
				Template->CodeTemplateDefs->RemoveFromRoot();
			}

			if (Template->BlueprintTemplateDefs != nullptr)
			{
				Template->BlueprintTemplateDefs->RemoveFromRoot();
			}
		}
	}
}

void SProjectDialog::PopulateTemplateCategories()
{
	TemplateCategories.Empty();
	CurrentCategory.Reset();

	TemplateCategories = GetAllTemplateCategories();
}

TSharedRef<SWidget> SProjectDialog::MakeNewProjectDialogButtons()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SGetSuggestedIDEWidget)
			.VisibilityOverride(this, &SProjectDialog::GetSuggestedIDEButtonVisibility)
		]
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SGetDisableIDEWidget)
			.Visibility(this, &SProjectDialog::GetDisableIDEButtonVisibility)
		]
		+SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 8.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SPrimaryButton)
			.Visibility(this, &SProjectDialog::GetCreateButtonVisibility)
			.Text(LOCTEXT("CreateNewProject", "Create"))
			.IsEnabled(this, &SProjectDialog::CanCreateProject)
			.OnClicked_Lambda([this](){CreateAndOpenProject(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Text(LOCTEXT("CancelNewProjectCreation", "Cancel"))
			.OnClicked(this, &SProjectDialog::OnCancel)
		];
}

TSharedRef<SWidget> SProjectDialog::MakeOpenProjectDialogButtons()
{
	TSharedRef<SProjectBrowser> ProjectBrowserRef = ProjectBrowser.ToSharedRef();

	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 16.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("MarketplaceToolTip", "Check out the Marketplace to find new projects!"))
			.Visibility(FLauncherPlatformModule::Get()->CanOpenLauncher(true) ? EVisibility::Visible : EVisibility::Collapsed)
			.OnClicked(ProjectBrowserRef, &SProjectBrowser::OnOpenMarketplace)
			[	
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(4.0f,0.0f)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("LevelEditor.OpenMarketplace"))
					.DesiredSizeOverride(FVector2D(16,16))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
				+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OpenMarketplace", "Marketplace..."))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Text(LOCTEXT("BrowseForProjects", "Browse..."))
			.OnClicked(ProjectBrowserRef, &SProjectBrowser::OnBrowseToProject)
		]
		+SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SPrimaryButton)
			.Visibility(this, &SProjectDialog::GetCreateButtonVisibility)
			.Text(LOCTEXT("OpenProject", "Open"))
			.IsEnabled(ProjectBrowserRef, &SProjectBrowser::HasSelectedProjectFile)
			.OnClicked(ProjectBrowserRef, &SProjectBrowser::OnOpenProject)
		]
		+ SHorizontalBox::Slot()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Text(LOCTEXT("CancelNewProjectCreation", "Cancel"))
			.OnClicked(this, &SProjectDialog::OnCancel)
		];
}

TSharedRef<SWidget> SProjectDialog::MakeTemplateProjectView()
{
	return
		SNew(SVerticalBox)
		// Templates list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, -12.0f)
			[
				SNew(SScrollBorder, TemplateListView.ToSharedRef())
				[
					TemplateListView.ToSharedRef()
				]
			]						
			+ SHorizontalBox::Slot()
			.Padding(0, -8.0f, 0.0f, -12.0f)
			.AutoWidth()
			[
				SNew(SSeparator)
				.Orientation(EOrientation::Orient_Vertical)
				.Thickness(2.0f)
			]
			// Selected template details
			+ SHorizontalBox::Slot()
			.Padding(8.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SVerticalBox)
				// Preview image
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 15.f))
				[
					SNew(SImage)
					.DesiredSizeOverride(FVector2D(358,160))
					.Image(this, &SProjectDialog::GetSelectedTemplatePreviewImage)
				]
				// Template Name
				+ SVerticalBox::Slot()
				.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.TextStyle(FAppStyle::Get(), "DialogButtonText")
					.Font(FAppStyle::Get().GetFontStyle("HeadingExtraSmall"))
					.ColorAndOpacity(FAppStyle::Get().GetSlateColor("Colors.White"))
					.Text(this, &SProjectDialog::GetSelectedTemplateProperty, &FTemplateItem::Name)
				]
				// Template Description
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(120.0f)
					.WidthOverride(358.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
							.AutoHeight()
							[
								SNew(STextBlock)
								.WrapTextAt(350.0f)
								.Text(this, &SProjectDialog::GetSelectedTemplateProperty, &FTemplateItem::Description)
							]
							// Asset types
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
							[
								SNew(SVerticalBox)
								.Visibility(this, &SProjectDialog::GetSelectedTemplateAssetVisibility)
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.TextStyle(FAppStyle::Get(), "DialogButtonText")
									.Text(LOCTEXT("ProjectTemplateAssetTypes", "Asset Type References"))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Text(this, &SProjectDialog::GetSelectedTemplateAssetTypes)
								]
							]
							// Class types
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 5.0f, 0.0f, 5.0f))
							[
								SNew(SVerticalBox)
								.Visibility(this, &SProjectDialog::GetSelectedTemplateClassVisibility)
								+ SVerticalBox::Slot()
								[
									SNew(STextBlock)
									.TextStyle(FAppStyle::Get(), "DialogButtonText")
									.Text(LOCTEXT("ProjectTemplateClassTypes", "Class Type References"))
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(STextBlock)
									.AutoWrapText(true)
									.Text(this, &SProjectDialog::GetSelectedTemplateClassTypes)
								]
							]
						]
					]
				]
				// Project Options
				+ SVerticalBox::Slot()
				.Expose(ProjectOptionsSlot)
			]
		];
}

TSharedRef<SWidget> SProjectDialog::MakeHybridView(EProjectDialogModeMode Mode)
{
	bCopyStarterContent = GEditor ? GetDefault<UEditorSettings>()->bCopyStarterContentPreference : true;

	SelectedHardwareClassTarget = EHardwareClass::Desktop;
	SelectedGraphicsPreset = EGraphicsPreset::Maximum;
	bIsStarterContentAvailable = true;

	// Find all template projects
	Templates = FindTemplateProjects();
	SetDefaultProjectLocation();

	TemplateListView = SNew(STileView<TSharedPtr<FTemplateItem>>)
		.ListItemsSource(&FilteredTemplateList)
		.SelectionMode(ESelectionMode::Single)
		.ClearSelectionOnClick(false)
		.ItemAlignment(EListItemAlignment::LeftAligned)
		.OnGenerateTile_Static(&STemplateTile::BuildTile)
		.ItemHeight(NewProjectDialogDefs::TemplateTileHeight+9)
		.ItemWidth(NewProjectDialogDefs::TemplateTileWidth+9)
		.OnSelectionChanged(this, &SProjectDialog::HandleTemplateListViewSelectionChanged);

	TSharedRef<SWidget> HybridView = 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[ 
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(0.0f, 0.0f, 0.0f, 16.0f)
			.AutoHeight()
			[
				SNew(SBox)
				.Visibility(Mode == EProjectDialogModeMode::Hybrid ? EVisibility::Visible : EVisibility::Collapsed)
				[	
					MakeRecentProjectsTile()
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, -4.0f, 0.0f, 0.0f)
			[
				SNew(SBorder)
				.Visibility(Mode == EProjectDialogModeMode::OpenProject ? EVisibility::Collapsed : EVisibility::Visible)
				.BorderImage(FAppStyle::Get().GetBrush("ProjectBrowser.MajorCategoryViewBorder"))
				[
					SAssignNew(MajorCategoryList, STileView<TSharedPtr<FTemplateCategory>>)
					.ListItemsSource(&TemplateCategories)
					.SelectionMode(ESelectionMode::Single)
					.ClearSelectionOnClick(false)
					.OnGenerateTile(this, &SProjectDialog::ConstructMajorCategoryTableRow)
					.ItemHeight(NewProjectDialogDefs::MajorItemHeight)
					.ItemWidth(NewProjectDialogDefs::MajorItemWidth)
					.OnSelectionChanged(this, &SProjectDialog::OnMajorTemplateCategorySelectionChanged)
				]
			]
		]
		+SHorizontalBox::Slot()
		.Padding(11.0f, 0.0f, 0.0f, 0.0f)
		[
			SAssignNew(TemplateAndRecentProjectsSwitcher, SWidgetSwitcher)
			+ SWidgetSwitcher::Slot()
			[
				MakeTemplateProjectView()
			]
			+ SWidgetSwitcher::Slot()
			[
				ProjectBrowser.ToSharedRef()
			]
		];
	
	SetCurrentMajorCategory(ActiveCategory);

	if (Mode == EProjectDialogModeMode::OpenProject)
	{
		TemplateAndRecentProjectsSwitcher->SetActiveWidgetIndex(1);
	}

	UpdateProjectFileValidity();

	return HybridView;
}

TSharedRef<SWidget> SProjectDialog::MakeProjectOptionsWidget()
{
	IHardwareTargetingModule& HardwareTargeting = IHardwareTargetingModule::Get();

	TSharedPtr<SVerticalBox> ProjectOptionsBox;

	TSharedRef<SWidget> ProjectOptionsWidget =
		SNew(SVerticalBox)
		.Visibility(this, &SProjectDialog::GetProjectSettingsVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[	
			SNew(SBorder)
			.Padding(FMargin(10.0f, 7.0f))
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Header"))
			[
				SNew(STextBlock)	
				.TextStyle(FAppStyle::Get(), "DialogButtonText")
				.Text(LOCTEXT("ProjectDefaults", "Project Defaults"))
			]
		]
		+ SVerticalBox::Slot()
		.Padding(0.0f, 20.0f, 0.0f, 0.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(ProjectOptionsBox, SVerticalBox)
			]
		];

	const TArray<ETemplateSetting>& HiddenSettings = GetSelectedTemplateProperty(&FTemplateItem::HiddenSettings);

	bool bIsBlueprintAvailable = !GetSelectedTemplateProperty(&FTemplateItem::BlueprintProjectFile).IsEmpty();
	bool bIsCodeAvailable = !GetSelectedTemplateProperty(&FTemplateItem::CodeProjectFile).IsEmpty();

	if (!HiddenSettings.Contains(ETemplateSetting::Languages))
	{
		// if neither is available, then this is a blank template, so both are available
		if (!bIsBlueprintAvailable && !bIsCodeAvailable)
		{
			bIsBlueprintAvailable = true;
			bIsCodeAvailable = true;
		}

		bShouldGenerateCode = !bIsBlueprintAvailable;

		TSharedRef<SSegmentedControl<int32>> ScriptTypeChooser =
			SNew(SSegmentedControl<int32>)
			.UniformPadding(FMargin(25.0f,4.0f))
			
			.ToolTipText(LOCTEXT("ProjectDialog_BlueprintOrCppDescription", "Choose whether to create a Blueprint or C++ project.\nNote: You can also add blueprints to a C++ project and C++ to a Blueprint project later."))
			.Value(this, &SProjectDialog::OnGetBlueprintOrCppIndex)
			.OnValueChanged(this, &SProjectDialog::OnSetBlueprintOrCppIndex);

			if (bIsBlueprintAvailable)
			{
				ScriptTypeChooser->AddSlot(0)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ProjectDialog_Blueprint", "BLUEPRINT"));
			}

			if (bIsCodeAvailable)
			{
				ScriptTypeChooser->AddSlot(1)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("ProjectDialog_Code", "C++"));
			}

			ProjectOptionsBox->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					ScriptTypeChooser
				];
	}
	else
	{
		bShouldGenerateCode = bIsCodeAvailable;
	}

	if (!HiddenSettings.Contains(ETemplateSetting::HardwareTarget))
	{
		TSharedRef<SWidget> HardwareClassTarget = HardwareTargeting.MakeHardwareClassTargetCombo(
			FOnHardwareClassChanged::CreateSP(this, &SProjectDialog::SetHardwareClassTarget),
			TAttribute<EHardwareClass>(this, &SProjectDialog::GetHardwareClassTarget));
		
		ProjectOptionsBox->AddSlot()
		.Padding(0.0f, 16.0f, 0.0f, 8.0f)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("ProjectDialog_HardwareClassTargetDescription", "Choose the closest equivalent target platform. You can change this later in the Target Hardware section of Project Settings."))
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TargetPlatform", "Target Platform"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					HardwareClassTarget
				]
			]
		];
	}

	if (!HiddenSettings.Contains(ETemplateSetting::GraphicsPreset))
	{
		TSharedRef<SWidget> GraphicsPreset = HardwareTargeting.MakeGraphicsPresetTargetCombo(
			FOnGraphicsPresetChanged::CreateSP(this, &SProjectDialog::SetGraphicsPreset),
			TAttribute<EGraphicsPreset>(this, &SProjectDialog::GetGraphicsPreset));

		ProjectOptionsBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("ProjectDialog_GraphicsPresetDescription", "Choose the performance characteristics of your project. You can change this later in the Target Hardware section of Project Settings."))
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("QualityPreset", "Quality Preset"))
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SBox)
				.WidthOverride(120.0f)
				[
					GraphicsPreset
				]
			]
		];
	}

	if (!HiddenSettings.Contains(ETemplateSetting::StarterContent))
	{
		FProjectInformation ProjectInfo = CreateProjectInfo();
		bIsStarterContentAvailable = GameProjectUtils::IsStarterContentAvailableForProject(ProjectInfo);

		ProjectOptionsBox->AddSlot()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("CopyStarterContent_ToolTip", "Enable to include an additional content pack containing simple placeable meshes with basic materials and textures.\nYou can also add the Starter Content to your project later using the Content Browser."))
				.IsEnabled(this, &SProjectDialog::IsStarterContentAvailable)
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IncludeStarterContent", "Starter Content"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SProjectDialog::GetCopyStarterContentCheckState)
					.OnCheckStateChanged(this, &SProjectDialog::OnSetCopyStarterContent)
				]
			];

		// Only add the option to add starter content if its there to add!
		if (!bIsStarterContentAvailable)
		{
			bCopyStarterContent = false;
		}
/*

		TSharedRef<SWidget> Enum = SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SDecoratedEnumCombo<int32>, MoveTemp(StarterContentOptions))
				.SelectedEnum()
				.OnEnumChanged()
				.Orientation(Orient_Vertical)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(4)
			[
				// Warning when enabled for mobile, since the current starter content is bad for mobile
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.Warning"))
				.ToolTipText(this, &SNewProjectWizard::GetStarterContentWarningTooltip)
				.Visibility(this, &SNewProjectWizard::GetStarterContentWarningVisibility)
			];
*/

		//AddToProjectSettingsGrid(GridPanel, Enum, Description, CurrentSlot);
	}

#if 0 // @todo: XR settings cannot be shown at the moment as the setting causes issues with binary builds.
	if (!HiddenSettings.Contains(ETemplateSetting::XR))
	{
		TArray<SDecoratedEnumCombo<int32>::FComboOption> VirtualRealityOptions;
		VirtualRealityOptions.Add(SDecoratedEnumCombo<int32>::FComboOption(
			0, FSlateIcon(FAppStyle::GetAppStyleSetName(), "GameProjectDialog.XRDisabled"),
			LOCTEXT("XRDisabled", "XR Disabled")));

		VirtualRealityOptions.Add(SDecoratedEnumCombo<int32>::FComboOption(
			1, 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "GameProjectDialog.XREnabled"),
			LOCTEXT("XREnabled", "XR Enabled")));

		TSharedRef<SDecoratedEnumCombo<int32>> Enum = SNew(SDecoratedEnumCombo<int32>, MoveTemp(VirtualRealityOptions))
			.SelectedEnum(this, &SProjectDialog::OnGetXREnabled)
			.OnEnumChanged(this, &SProjectDialog::OnSetXREnabled)
			.Orientation(Orient_Vertical);

		TSharedRef<SRichTextBlock> Description = SNew(SRichTextBlock)
			.Text(LOCTEXT("ProjectDialog_XREnabledDescription", "Choose if XR should be enabled in the new project."))
			.AutoWrapText(true)
			.DecoratorStyleSet(&FAppStyle::Get());
	}
#endif 

	if (!HiddenSettings.Contains(ETemplateSetting::Raytracing))
	{
		ProjectOptionsBox->AddSlot()
			.Padding(0.0f, 8.0f, 0.0f, 0.0f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				.ToolTipText(LOCTEXT("ProjectDialog_RaytracingDescription", "Choose if real-time raytracing should be supported in the new project."))
				+ SHorizontalBox::Slot()
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RayTracing", "Raytracing"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SProjectDialog::OnGetRaytracingEnabledCheckState)
					.OnCheckStateChanged(this, &SProjectDialog::OnSetRaytracingEnabled)
				]
			];
	}

	return ProjectOptionsWidget;
}

TSharedRef<SWidget> SProjectDialog::MakeRecentProjectsTile()
{
	RecentProjectsCategory = MakeShared<FTemplateCategory>();

	RecentProjectsCategory->DisplayName = LOCTEXT("RecentProjects", "Recent Projects");
	RecentProjectsCategory->Description = FText::GetEmpty();
	RecentProjectsCategory->Key = "RecentProjects";
	RecentProjectsCategory->IsEnterprise = false;

	static const FName BrushName =  *(FAppStyle::Get().GetContentRootDir() / TEXT("/Starship/Projects/") / TEXT("RecentProjects_2x.png"));

	RecentProjectsBrush = MakeUnique<FSlateDynamicImageBrush>(BrushName, FVector2D(300, 100));
	RecentProjectsBrush->OutlineSettings.CornerRadii = FVector4(4, 4, 4, 4);
	RecentProjectsBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	RecentProjectsBrush->DrawAs = ESlateBrushDrawType::RoundedBox;

	RecentProjectsCategory->Icon = RecentProjectsBrush.Get();

	return
		SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "InvisibleButton")
		.OnClicked(this, &SProjectDialog::OnRecentProjectsClicked)
		.ForegroundColor(FLinearColor::White)
		.ContentPadding(FMargin(4.0f, 0.0f))
		[
			SNew(SMajorCategoryTile, RecentProjectsCategory)
			.IsSelected_Lambda([this]() { return RecentProjectsCategory == CurrentCategory; })
		];
}

TSharedRef<SWidget> SProjectDialog::MakeNewProjectPathArea()
{
	return 
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(25.0f, 36.0f, 0.0f, 0)
		[
			SNew(SFilepath)
			.OnBrowseForFolder(this, &SProjectDialog::HandlePathBrowseButtonClicked)
			.FolderPath(this, &SProjectDialog::GetCurrentProjectFilePath)
			.WarningText(this, &SProjectDialog::GetNameAndLocationValidityErrorText)
			.Name(this, &SProjectDialog::GetCurrentProjectFileName)
			.OnFolderChanged(this, &SProjectDialog::OnCurrentProjectFilePathChanged)
			.OnNameChanged(this, &SProjectDialog::OnCurrentProjectFileNameChanged)
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		.VAlign(VAlign_Bottom)
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SWarningOrErrorBox)
				.Padding(FMargin(8.0f, 4.0f, 4.0f, 4.0f))
				.IconSize(FVector2D(16,16))
				.MessageStyle(EMessageStyle::Error)
				.Message(this, &SProjectDialog::GetGlobalErrorLabelText)
				.Visibility(this, &SProjectDialog::GetGlobalErrorVisibility)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Bottom)
			[
				MakeNewProjectDialogButtons()
			]
		];
}

TSharedRef<SWidget> SProjectDialog::MakeOpenProjectPathArea()
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(25.0f, 36.0f, 0.0f, 0)
		[
			SNew(SFilepath)
			.IsReadOnly(true)
			.FolderPath(this, &SProjectDialog::GetCurrentProjectFilePath)
			.Name(this, &SProjectDialog::GetCurrentProjectFileName)
		]
		+SVerticalBox::Slot()
		.Padding(25.0f, 0.0f, 0.0f, 8.0f)
		.VAlign(VAlign_Bottom)	
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			[
				SNew(SCheckBox)
				.IsChecked(GetDefault<UEditorSettings>()->bLoadTheMostRecentlyLoadedProjectAtStartup ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
				.OnCheckStateChanged(ProjectBrowser.ToSharedRef(), &SProjectBrowser::OnAutoloadLastProjectChanged)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AutoloadOnStartupCheckbox", "Always load last project on startup"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Bottom)
			.HAlign(HAlign_Right)
			[
				MakeOpenProjectDialogButtons()
			]
		];
}

void SProjectDialog::OnSetCopyStarterContent(ECheckBoxState NewState)
{
	bCopyStarterContent = NewState == ECheckBoxState::Checked;
}

bool SProjectDialog::CanCreateProject() const
{
	return bLastGlobalValidityCheckSuccessful && bLastNameAndLocationValidityCheckSuccessful;
}

FReply SProjectDialog::OnCancel() const
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	Window->RequestDestroyWindow();

	return FReply::Handled();
}

void SProjectDialog::OnSetRaytracingEnabled(ECheckBoxState NewState)
{
	bEnableRaytracing = NewState == ECheckBoxState::Checked;
}

void SProjectDialog::OnSetBlueprintOrCppIndex(int32 Index)
{
	bShouldGenerateCode = Index == 1;
	UpdateProjectFileValidity();
	FProjectInformation ProjectInfo = CreateProjectInfo();
	bIsStarterContentAvailable = GameProjectUtils::IsStarterContentAvailableForProject(ProjectInfo);
	bCopyStarterContent &= bIsStarterContentAvailable;
}

void SProjectDialog::SetHardwareClassTarget(EHardwareClass InHardwareClass)
{
	SelectedHardwareClassTarget = InHardwareClass;
}

void SProjectDialog::SetGraphicsPreset(EGraphicsPreset InGraphicsPreset)
{
	SelectedGraphicsPreset = InGraphicsPreset;
}

void SProjectDialog::HandleTemplateListViewSelectionChanged(TSharedPtr<FTemplateItem> TemplateItem, ESelectInfo::Type SelectInfo)
{
	if (TemplateItem.IsValid())
	{
		if (TemplateItem->HiddenSettings.Contains(ETemplateSetting::StarterContent))
		{
			bCopyStarterContent = false;
		}
	}

	(*ProjectOptionsSlot)
	[
		MakeProjectOptionsWidget()
	];

	UpdateProjectFileValidity();
}

TSharedPtr<FTemplateItem> SProjectDialog::GetSelectedTemplateItem() const
{
	TArray<TSharedPtr<FTemplateItem>> SelectedItems = TemplateListView->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		return SelectedItems[0];
	}

	return nullptr;
}

namespace
{
	FString MakeSortKey(const FString& TemplateKey)
	{
		FString Output = TemplateKey;

#if PLATFORM_LINUX
		// Paths with a leading "/" would get sorted before the magic value used for blank projects: "_1"
		Output.RemoveFromStart("/");
#endif

		return Output;
	}
}

TMap<FName, TArray<TSharedPtr<FTemplateItem>> > SProjectDialog::FindTemplateProjects()
{
	// Clear the list out first - or we could end up with duplicates
	TMap<FName, TArray<TSharedPtr<FTemplateItem>>> Templates;

	// Now discover and all data driven templates
	TArray<FString> TemplateRootFolders;

	// @todo rocket make template folder locations extensible.
	TemplateRootFolders.Add(FPaths::RootDir() + TEXT("Templates"));

	// Add the Enterprise templates
	TemplateRootFolders.Add(FPaths::EnterpriseDir() + TEXT("Templates"));

	// Allow plugins to define templates
	TArray<TSharedRef<IPlugin>> Plugins = IPluginManager::Get().GetEnabledPlugins();
	for (const TSharedRef<IPlugin>& Plugin : Plugins)
	{
		FString PluginDirectory = Plugin->GetBaseDir();
		if (!PluginDirectory.IsEmpty())
		{
			const FString PluginTemplatesDirectory = FPaths::Combine(*PluginDirectory, TEXT("Templates"));

			if (IFileManager::Get().DirectoryExists(*PluginTemplatesDirectory))
			{
				TemplateRootFolders.Add(PluginTemplatesDirectory);
			}
		}
	}

	// Form a list of all folders that could contain template projects
	TArray<FString> AllTemplateFolders;
	for (const FString& Root : TemplateRootFolders)
	{
		const FString SearchString = Root / TEXT("*");
		TArray<FString> TemplateFolders;
		IFileManager::Get().FindFiles(TemplateFolders, *SearchString, /*Files=*/false, /*Directories=*/true);

		for (const FString& Folder : TemplateFolders)
		{
			AllTemplateFolders.Add(Root / Folder);
		}
	}

	TArray<TSharedPtr<FTemplateItem>> FoundTemplates;

	// Add a template item for every discovered project
	for (const FString& Root : AllTemplateFolders)
	{
		const FString SearchString = Root / TEXT("*.") + FProjectDescriptor::GetExtension();
		TArray<FString> FoundProjectFiles;
		IFileManager::Get().FindFiles(FoundProjectFiles, *SearchString, /*Files=*/true, /*Directories=*/false);

		if (FoundProjectFiles.Num() == 0 || !ensure(FoundProjectFiles.Num() == 1))
		{
			continue;
		}

		// Make sure a TemplateDefs.ini file exists
		UTemplateProjectDefs* TemplateDefs = GameProjectUtils::LoadTemplateDefs(Root);
		if (TemplateDefs == nullptr)
		{
			continue;
		}

		// we don't have an appropriate referencing UObject to keep these alive with, so we need to keep these template defs alive from GC
		TemplateDefs->AddToRoot();

		// Ignore any templates whose definition says we cannot use to create a project
		if (TemplateDefs->bAllowProjectCreation == false)
		{
			continue;
		}

		const FString ProjectFile = Root / FoundProjectFiles[0];

		// If no template category was specified, use the default category
		TArray<FName> TemplateCategoryNames = TemplateDefs->Categories;
		if (TemplateCategoryNames.Num() == 0)
		{
			TemplateCategoryNames.Add(NewProjectDialogDefs::DefaultCategoryName);
		}

		// Find a duplicate project, eg. "Foo" and "FooBP"
		FString TemplateKey = Root;
		TemplateKey.RemoveFromEnd("BP");

		TSharedPtr<FTemplateItem>* ExistingTemplate = FoundTemplates.FindByPredicate([&TemplateKey](TSharedPtr<FTemplateItem> Item)
			{
				return Item->Key == TemplateKey;
			});

		TSharedPtr<FTemplateItem> Template;

		// Create a new template if none was found
		if (ExistingTemplate != nullptr)
		{
			Template = *ExistingTemplate;
		}
		else
		{
			Template = MakeShareable(new FTemplateItem());
		}

		if (TemplateDefs->GeneratesCode(Root))
		{
			Template->CodeProjectFile = ProjectFile;
			Template->CodeTemplateDefs = TemplateDefs;
		}
		else
		{
			Template->BlueprintProjectFile = ProjectFile;
			Template->BlueprintTemplateDefs = TemplateDefs;
		}

		// The rest has already been set by the existing template, so skip it.
		if (ExistingTemplate != nullptr)
		{
			continue;
		}

		// Did not find an existing template. Create a new one to add to the template list.
		Template->Key = TemplateKey;

		// @todo: These are all basically just copies of what's in UTemplateProjectDefs, but ignore differences between code and BP 
		Template->Categories = TemplateCategoryNames;
		Template->Description = TemplateDefs->GetLocalizedDescription();
		Template->ClassTypes = TemplateDefs->ClassTypes;
		Template->AssetTypes = TemplateDefs->AssetTypes;
		Template->HiddenSettings = TemplateDefs->HiddenSettings;
		Template->bIsEnterprise = TemplateDefs->bIsEnterprise;
		Template->bIsBlankTemplate = TemplateDefs->bIsBlank;
		Template->bThumbnailAsIcon = TemplateDefs->bThumbnailAsIcon;

		Template->Name = TemplateDefs->GetDisplayNameText();
		if (Template->Name.IsEmpty())
		{
			Template->Name = FText::FromString(TemplateKey);
		}

		const FString ThumbnailPNGFile = (Root + TEXT("/Media/") + FoundProjectFiles[0]).Replace(TEXT(".uproject"), TEXT(".png"));
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*ThumbnailPNGFile))
		{
			const FName BrushName = FName(*ThumbnailPNGFile);
			Template->Thumbnail = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(128, 128)));
		}

		TSharedPtr<FSlateDynamicImageBrush> PreviewBrush;
		const FString PreviewPNGFile = (Root + TEXT("/Media/") + FoundProjectFiles[0]).Replace(TEXT(".uproject"), TEXT("_Preview.png"));
		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*PreviewPNGFile))
		{
			const FName BrushName = FName(*PreviewPNGFile);
			Template->PreviewImage = MakeShareable(new FSlateDynamicImageBrush(BrushName, FVector2D(512, 256)));
		}

		Template->SortKey = TemplateDefs->SortKey;
		if (Template->SortKey.IsEmpty())
		{
			Template->SortKey = MakeSortKey(TemplateKey);
		}

		FoundTemplates.Add(Template);
	}

	for (const TSharedPtr<FTemplateItem>& Template : FoundTemplates)
	{
		for (const FName& Category : Template->Categories)
		{
			Templates.FindOrAdd(Category).Add(Template);
		}
	}

	TArray<TSharedPtr<FTemplateCategory>> AllTemplateCategories = GetAllTemplateCategories();

	// Validate that all our templates have a category defined
	TArray<FName> CategoryKeys;
	Templates.GetKeys(CategoryKeys);
	for (const FName& CategoryKey : CategoryKeys)
	{
		bool bCategoryExists = AllTemplateCategories.ContainsByPredicate([&CategoryKey](const TSharedPtr<FTemplateCategory>& Category)
			{
				return Category->Key == CategoryKey;
			});

		if (!bCategoryExists)
		{
			UE_LOG(LogGameProjectGeneration, Warning, TEXT("Failed to find category definition named '%s', it is not defined in any TemplateCategories.ini."), *CategoryKey.ToString());
		}
	}

	// Add blank template to empty categories
	{
		TSharedPtr<FTemplateItem> BlankTemplate = MakeShareable(new FTemplateItem());
		BlankTemplate->Name = LOCTEXT("BlankProjectName", "Blank");
		BlankTemplate->Description = LOCTEXT("BlankProjectDescription", "A clean empty project with no code and default settings.");
		BlankTemplate->Key = TEXT("Blank");
		BlankTemplate->SortKey = TEXT("_1");
		BlankTemplate->Thumbnail = MakeShareable(new FSlateBrush(*FAppStyle::GetBrush("GameProjectDialog.BlankProjectThumbnail")));
		BlankTemplate->PreviewImage = MakeShareable(new FSlateBrush(*FAppStyle::GetBrush("GameProjectDialog.BlankProjectPreview")));
		BlankTemplate->BlueprintProjectFile = TEXT("");
		BlankTemplate->CodeProjectFile = TEXT("");
		BlankTemplate->bIsEnterprise = false;
		BlankTemplate->bIsBlankTemplate = true;

		for (const TSharedPtr<FTemplateCategory>& Category : AllTemplateCategories)
		{
			const TArray<TSharedPtr<FTemplateItem>>* CategoryEntry = Templates.Find(Category->Key);
			if (CategoryEntry == nullptr)
			{
				Templates.Add(Category->Key).Add(BlankTemplate);
			}
		}
	}

	return Templates;
}


void SProjectDialog::SetDefaultProjectLocation()
{
	FString DefaultProjectFilePath;

	// First, try and use the first previously used path that still exists
	for (const FString& CreatedProjectPath : GetDefault<UEditorSettings>()->CreatedProjectPaths)
	{
		if (IFileManager::Get().DirectoryExists(*CreatedProjectPath))
		{
			DefaultProjectFilePath = CreatedProjectPath;
			break;
		}
	}

	if (DefaultProjectFilePath.IsEmpty())
	{
		// No previously used path, decide a default path.
		DefaultProjectFilePath = FDesktopPlatformModule::Get()->GetDefaultProjectCreationPath();
		IFileManager::Get().MakeDirectory(*DefaultProjectFilePath, true);
	}

	if (DefaultProjectFilePath.EndsWith(TEXT("/")))
	{
		DefaultProjectFilePath.LeftChopInline(1);
	}

	FPaths::NormalizeFilename(DefaultProjectFilePath);
	FPaths::MakePlatformFilename(DefaultProjectFilePath);
	const FString GenericProjectName = LOCTEXT("DefaultProjectName", "MyProject").ToString();
	FString ProjectName = GenericProjectName;

	// Check to make sure the project file doesn't already exist
	FText FailReason;
	if (!GameProjectUtils::IsValidProjectFileForCreation(DefaultProjectFilePath / ProjectName / ProjectName + TEXT(".") + FProjectDescriptor::GetExtension(), FailReason))
	{
		// If it exists, find an appropriate numerical suffix
		const int MaxSuffix = 1000;
		int32 Suffix;
		for (Suffix = 2; Suffix < MaxSuffix; ++Suffix)
		{
			ProjectName = GenericProjectName + FString::Printf(TEXT("%d"), Suffix);
			if (GameProjectUtils::IsValidProjectFileForCreation(DefaultProjectFilePath / ProjectName / ProjectName + TEXT(".") + FProjectDescriptor::GetExtension(), FailReason))
			{
				// Found a name that is not taken. Break out.
				break;
			}
		}

		if (Suffix >= MaxSuffix)
		{
			UE_LOG(LogGameProjectGeneration, Warning, TEXT("Failed to find a suffix for the default project name"));
			ProjectName = TEXT("");
		}
	}

	if (!DefaultProjectFilePath.IsEmpty())
	{
		CurrentProjectFileName = ProjectName;
		CurrentProjectFilePath = DefaultProjectFilePath;
		FPaths::MakePlatformFilename(CurrentProjectFilePath);
		LastBrowsePath = CurrentProjectFilePath;
	}
}

void SProjectDialog::SetCurrentMajorCategory(FName Category)
{
	FilteredTemplateList = Templates.FindRef(Category);

	// Sort the template folders
	FilteredTemplateList.Sort([](const TSharedPtr<FTemplateItem>& A, const TSharedPtr<FTemplateItem>& B) {
		return A->SortKey < B->SortKey;
		});

	if (FilteredTemplateList.Num() > 0)
	{
		TemplateListView->SetSelection(FilteredTemplateList[0]);
	}
	TemplateListView->RequestListRefresh();

	ActiveCategory = Category;
}

FReply SProjectDialog::OnRecentProjectsClicked()
{
	CurrentCategory = RecentProjectsCategory;

	ActiveCategory = NAME_None;

	MajorCategoryList->ClearSelection();

	TemplateAndRecentProjectsSwitcher->SetActiveWidgetIndex(1);
	PathAreaSwitcher->SetActiveWidgetIndex(1);

	return FReply::Handled();
}

FProjectInformation SProjectDialog::CreateProjectInfo() const
{
	TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();
	if (!SelectedTemplate.IsValid())
	{
		return FProjectInformation();
	}

	FProjectInformation ProjectInfo;
	ProjectInfo.bShouldGenerateCode = bShouldGenerateCode;
	ProjectInfo.bCopyStarterContent = bCopyStarterContent;
	ProjectInfo.TemplateFile = bShouldGenerateCode ? SelectedTemplate->CodeProjectFile : SelectedTemplate->BlueprintProjectFile;
	ProjectInfo.TemplateCategory = ActiveCategory;
	ProjectInfo.bIsEnterpriseProject = SelectedTemplate->bIsEnterprise;
	ProjectInfo.bIsBlankTemplate = SelectedTemplate->bIsBlankTemplate;

	if (bShouldGenerateCode)
	{
		if (SelectedTemplate->CodeTemplateDefs != nullptr)
		{
			ProjectInfo.StarterContent = SelectedTemplate->CodeTemplateDefs->StarterContent;
		}
	}
	else
	{
		if (SelectedTemplate->BlueprintTemplateDefs != nullptr)
		{
			ProjectInfo.StarterContent = SelectedTemplate->BlueprintTemplateDefs->StarterContent;
		}
	}

	const TArray<ETemplateSetting>& HiddenSettings = SelectedTemplate->HiddenSettings;

	if (!HiddenSettings.Contains(ETemplateSetting::All))
	{
		if (!HiddenSettings.Contains(ETemplateSetting::HardwareTarget))
		{
			ProjectInfo.TargetedHardware = SelectedHardwareClassTarget;
		}

		if (!HiddenSettings.Contains(ETemplateSetting::GraphicsPreset))
		{
			ProjectInfo.DefaultGraphicsPerformance = SelectedGraphicsPreset;
		}

		if (!HiddenSettings.Contains(ETemplateSetting::XR))
		{
			ProjectInfo.bEnableXR = bEnableXR;
		}

		if (!HiddenSettings.Contains(ETemplateSetting::Raytracing))
		{
			ProjectInfo.bEnableRaytracing = bEnableRaytracing;
		}
	}

	return MoveTemp(ProjectInfo);
}

bool SProjectDialog::CreateProject(const FString& ProjectFile)
{
	// Get the selected template
	TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();

	if (!ensure(SelectedTemplate.IsValid()))
	{
		// A template must be selected.
		return false;
	}

	FText FailReason, FailLog;

	FProjectInformation ProjectInfo = CreateProjectInfo();
	ProjectInfo.ProjectFilename = ProjectFile;

	if (!GameProjectUtils::CreateProject(ProjectInfo, FailReason, FailLog))
	{
		SOutputLogDialog::Open(LOCTEXT("CreateProject", "Create Project"), FailReason, FailLog, FText::GetEmpty());
		return false;
	}

	// Successfully created the project. Update the last created location string.
	FString CreatedProjectPath = FPaths::GetPath(FPaths::GetPath(ProjectFile));

	// If the original path was the drives root (ie: C:/) the double path call strips the last /
	if (CreatedProjectPath.EndsWith(":"))
	{
		CreatedProjectPath.AppendChar('/');
	}

	UEditorSettings* Settings = GetMutableDefault<UEditorSettings>();
	Settings->CreatedProjectPaths.Remove(CreatedProjectPath);
	Settings->CreatedProjectPaths.Insert(CreatedProjectPath, 0);
	Settings->bCopyStarterContentPreference = bCopyStarterContent;
	Settings->PostEditChange();

	return true;
}

void SProjectDialog::CreateAndOpenProject()
{
	if (!CanCreateProject())
	{
		return;
	}

	FString ProjectFile = GetProjectFilenameWithPath();
	if (!CreateProject(ProjectFile))
	{
		return;
	}

	if (bShouldGenerateCode)
	{
		// If the engine is installed it is already compiled, so we can try to build and open a new project immediately. Non-installed situations might require building
		// the engine (especially the case when binaries came from P4), so we only open the IDE for that.
		if (FApp::IsEngineInstalled())
		{
			if (GameProjectUtils::BuildCodeProject(ProjectFile))
			{
				OpenCodeIDE(ProjectFile);
				OpenProject(ProjectFile);
			}
			else
			{
				// User will have already been prompted to open the IDE
			}
		}
		else
		{
			OpenCodeIDE(ProjectFile);
		}
	}
	else
	{
		OpenProject(ProjectFile);
	}
}

bool SProjectDialog::OpenProject(const FString& ProjectFile)
{
	FText FailReason;
	if (GameProjectUtils::OpenProject(ProjectFile, FailReason))
	{
		// Successfully opened the project, the editor is closing.
		// Close this window in case something prevents the editor from closing (save dialog, quit confirmation, etc)
		CloseWindowIfAppropriate();
		return true;
	}

	DisplayError(FailReason);
	return false;
}

void SProjectDialog::CloseWindowIfAppropriate(bool ForceClose)
{
	if (ForceClose || FApp::HasProjectName())
	{
		TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());

		if (ContainingWindow.IsValid())
		{
			ContainingWindow->RequestDestroyWindow();
		}
	}
}

void SProjectDialog::DisplayError(const FText& ErrorText)
{
	FString ErrorString = ErrorText.ToString();
	UE_LOG(LogGameProjectGeneration, Log, TEXT("%s"), *ErrorString);
	if(ErrorString.Contains("\n"))
	{
		FMessageDialog::Open(EAppMsgType::Ok, ErrorText);
	}
	else
	{
		PersistentGlobalErrorLabelText = ErrorText;
	}
}

bool SProjectDialog::OpenCodeIDE(const FString& ProjectFile)
{
	FText FailReason;
    
#if PLATFORM_MAC
    // Modern Xcode projects are different based on Desktop/Mobile
    FString Extension = FPaths::GetExtension(ProjectFile);
    FString ModernXcodeProjectFile = ProjectFile;
    if (SelectedHardwareClassTarget == EHardwareClass::Desktop)
    {
        ModernXcodeProjectFile.RemoveFromEnd(TEXT(".") + Extension);
        ModernXcodeProjectFile += TEXT(" (Mac).") + Extension;
    }
    else if (SelectedHardwareClassTarget == EHardwareClass::Mobile)
    {
        ModernXcodeProjectFile.RemoveFromEnd(TEXT(".") + Extension);
        ModernXcodeProjectFile += TEXT(" (IOS).") + Extension;
    }
#endif
    
	if (
#if PLATFORM_MAC
        GameProjectUtils::OpenCodeIDE(ModernXcodeProjectFile, FailReason) ||
        // if modern failed, try again with legacy project name
#endif
        GameProjectUtils::OpenCodeIDE(ProjectFile, FailReason))
	{
		// Successfully opened code editing IDE, the editor is closing
		// Close this window in case something prevents the editor from closing (save dialog, quit confirmation, etc)
		CloseWindowIfAppropriate(true);
		return true;
	}

	DisplayError(FailReason);
	return false;
}


TArray<TSharedPtr<FTemplateCategory>> SProjectDialog::GetAllTemplateCategories()
{
	TArray<TSharedPtr<FTemplateCategory>> AllTemplateCategories;
	FGameProjectGenerationModule::Get().GetAllTemplateCategories(AllTemplateCategories);

	if (AllTemplateCategories.Num() == 0)
	{
		static const FName BrushName = *(FAppStyle::Get().GetContentRootDir() / TEXT("/Starship/Projects/") / TEXT("CustomTemplate_2x.png"));

		if (!CustomTemplateBrush)
		{
			CustomTemplateBrush = MakeUnique<FSlateDynamicImageBrush>(BrushName, FVector2D(300, 100));
			CustomTemplateBrush->OutlineSettings.CornerRadii = FVector4(4, 4, 4, 4);
			CustomTemplateBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			CustomTemplateBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
		}

		TSharedPtr<FTemplateCategory> DefaultCategory = MakeShared<FTemplateCategory>();
		DefaultCategory->Key = NewProjectDialogDefs::BlankCategoryKey;
		DefaultCategory->DisplayName = LOCTEXT("ProjectDialog_DefaultCategoryName", "Blank Project");
		DefaultCategory->Description = LOCTEXT("ProjectDialog_DefaultCategoryDescription", "Create a new blank Unreal project.");
		DefaultCategory->Icon = CustomTemplateBrush.Get();

		AllTemplateCategories.Add(DefaultCategory);
	}

	return AllTemplateCategories;
}



FText SProjectDialog::GetGlobalErrorLabelText() const
{
	if (!PersistentGlobalErrorLabelText.IsEmpty())
	{
		return PersistentGlobalErrorLabelText;
	}

	if (!bLastGlobalValidityCheckSuccessful)
	{
		return LastGlobalValidityErrorText;
	}

	return FText::GetEmpty();
}

FText SProjectDialog::GetNameAndLocationValidityErrorText() const
{
	if (GetGlobalErrorLabelText().IsEmpty())
	{
		return bLastNameAndLocationValidityCheckSuccessful == false ? LastNameAndLocationValidityErrorText : FText::GetEmpty();
	}

	return FText::GetEmpty();
}

EVisibility SProjectDialog::GetCreateButtonVisibility() const
{
	return IsCompilerRequired() && !FSourceCodeNavigation::IsCompilerAvailable() ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SProjectDialog::GetSuggestedIDEButtonVisibility() const
{
	return IsCompilerRequired() && !FSourceCodeNavigation::IsCompilerAvailable() ? EVisibility::Visible : EVisibility::Collapsed;
}

// Allow disabling of the current IDE for platforms that dont require an IDE to run the Editor/Engine
EVisibility SProjectDialog::GetDisableIDEButtonVisibility() const
{
	if (GetSuggestedIDEButtonVisibility() == EVisibility::Visible && !IsIDERequired())
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

const FSlateBrush* SProjectDialog::GetSelectedTemplatePreviewImage() const
{
	TSharedPtr<FSlateBrush> PreviewImage = GetSelectedTemplateProperty(&FTemplateItem::PreviewImage);
	return PreviewImage.IsValid() ? PreviewImage.Get() : nullptr;
}


FText SProjectDialog::GetCurrentProjectFilePath() const
{
	return PathAreaSwitcher && PathAreaSwitcher->GetActiveWidgetIndex() == 1 ? FText::FromString(ProjectBrowser->GetSelectedProjectFile()) : FText::FromString(CurrentProjectFilePath);
}

void SProjectDialog::OnCurrentProjectFilePathChanged(const FText& InValue)
{
	CurrentProjectFilePath = InValue.ToString();
	FPaths::MakePlatformFilename(CurrentProjectFilePath);
	UpdateProjectFileValidity();
}

void SProjectDialog::OnCurrentProjectFileNameChanged(const FText& InValue)
{
	CurrentProjectFileName = InValue.ToString();
	UpdateProjectFileValidity();
}

FText SProjectDialog::GetCurrentProjectFileName() const
{
	return PathAreaSwitcher && PathAreaSwitcher->GetActiveWidgetIndex() == 1 ? ProjectBrowser->GetSelectedProjectName() : FText::FromString(CurrentProjectFileName);
}

FReply SProjectDialog::HandlePathBrowseButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOCTEXT("NewProjectBrowseTitle", "Choose a project location").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			Title,
			LastBrowsePath,
			FolderName
		);

		if (bFolderSelected)
		{
			if (!FolderName.EndsWith(TEXT("/")))
			{
				FolderName += TEXT("/");
			}

			FPaths::MakePlatformFilename(FolderName);
			LastBrowsePath = FolderName;
			CurrentProjectFilePath = FolderName;
		}
	}

	return FReply::Handled();
}

bool SProjectDialog::IsCompilerRequired() const
{
	TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();

	if (SelectedTemplate.IsValid())
	{
		return bShouldGenerateCode && !SelectedTemplate->CodeProjectFile.IsEmpty();
	}

	return false;
}

// Linux does not require an IDE to be setup to compile things
bool SProjectDialog::IsIDERequired() const
{
#if PLATFORM_LINUX
	return false;
#endif
	return true;
}

EVisibility SProjectDialog::GetProjectSettingsVisibility() const
{
	const TArray<ETemplateSetting>& HiddenSettings = GetSelectedTemplateProperty(&FTemplateItem::HiddenSettings);
	return HiddenSettings.Contains(ETemplateSetting::All) ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SProjectDialog::GetSelectedTemplateClassVisibility() const
{
	return GetSelectedTemplateProperty(&FTemplateItem::ClassTypes).IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed;
}

FText SProjectDialog::GetSelectedTemplateAssetTypes() const
{
	return FText::FromString(GetSelectedTemplateProperty(&FTemplateItem::AssetTypes));
}

FText SProjectDialog::GetSelectedTemplateClassTypes() const
{
	return FText::FromString(GetSelectedTemplateProperty(&FTemplateItem::ClassTypes));
}

EVisibility SProjectDialog::GetSelectedTemplateAssetVisibility() const
{
	return GetSelectedTemplateProperty(&FTemplateItem::AssetTypes).IsEmpty() == false ? EVisibility::Visible : EVisibility::Collapsed;
}

FString SProjectDialog::GetProjectFilenameWithPath() const
{
	if (CurrentProjectFilePath.IsEmpty())
	{
		// Don't even try to assemble the path or else it may be relative to the binaries folder!
		return TEXT("");
	}
	else
	{
		const FString ProjectName = CurrentProjectFileName;
		const FString ProjectPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CurrentProjectFilePath);
		const FString Filename = ProjectName + TEXT(".") + FProjectDescriptor::GetExtension();
		FString ProjectFilename = FPaths::Combine(*ProjectPath, *ProjectName, *Filename);
		FPaths::MakePlatformFilename(ProjectFilename);
		return ProjectFilename;
	}
}

void SProjectDialog::UpdateProjectFileValidity()
{
	// Global validity
	{
		bLastGlobalValidityCheckSuccessful = true;

		TSharedPtr<FTemplateItem> SelectedTemplate = GetSelectedTemplateItem();
		if (!SelectedTemplate.IsValid())
		{
			bLastGlobalValidityCheckSuccessful = false;
			LastGlobalValidityErrorText = LOCTEXT("NoTemplateSelected", "No Template Selected");
		}
		else
		{
			if (IsCompilerRequired())
			{
				if (!FSourceCodeNavigation::IsCompilerAvailable())
				{
					bLastGlobalValidityCheckSuccessful = false;

					if (IsIDERequired())
					{
						LastGlobalValidityErrorText = FText::Format(LOCTEXT("NoCompilerFoundProjectDialog", "No compiler was found. In order to use a C++ template, you must first install {0}."), FSourceCodeNavigation::GetSuggestedSourceCodeIDE());
					}
					else
					{
						LastGlobalValidityErrorText = FText::Format(LOCTEXT("MissingIDEProjectDialog", "Your IDE {0} is missing or incorrectly configured, please consider using {1}"),
			FSourceCodeNavigation::GetSelectedSourceCodeIDE(), FSourceCodeNavigation::GetSuggestedSourceCodeIDE());
					}
				}
				else if (!FDesktopPlatformModule::Get()->IsUnrealBuildToolAvailable())
				{
					bLastGlobalValidityCheckSuccessful = false;
					LastGlobalValidityErrorText = LOCTEXT("UBTNotFound", "Engine source code was not found. In order to use a C++ template, you must have engine source code in Engine/Source.");
				}
			}
		}
	}

	// Name and Location Validity
	{
		bLastNameAndLocationValidityCheckSuccessful = true;

		if (!FPlatformMisc::IsValidAbsolutePathFormat(CurrentProjectFilePath))
		{
			bLastNameAndLocationValidityCheckSuccessful = false;
			LastNameAndLocationValidityErrorText = LOCTEXT("InvalidFolderPath", "The folder path is invalid");
		}
		else
		{
			FText FailReason;
			if (!GameProjectUtils::IsValidProjectFileForCreation(GetProjectFilenameWithPath(), FailReason))
			{
				bLastNameAndLocationValidityCheckSuccessful = false;
				LastNameAndLocationValidityErrorText = FailReason;
			}
		}

		if (CurrentProjectFileName.Contains(TEXT("/")) || CurrentProjectFileName.Contains(TEXT("\\")))
		{
			bLastNameAndLocationValidityCheckSuccessful = false;
			LastNameAndLocationValidityErrorText = LOCTEXT("SlashOrBackslashInProjectName", "The project name may not contain a slash or backslash");
		}
		else
		{
			FText FailReason;
			if (!GameProjectUtils::IsValidProjectFileForCreation(GetProjectFilenameWithPath(), FailReason))
			{
				bLastNameAndLocationValidityCheckSuccessful = false;
				LastNameAndLocationValidityErrorText = FailReason;
			}
		}
	}
}

void SProjectDialog::OnMajorTemplateCategorySelectionChanged(TSharedPtr<FTemplateCategory> Item, ESelectInfo::Type SelectType)
{
	if(Item.IsValid())
	{
		CurrentCategory = Item;

		SetCurrentMajorCategory(Item->Key);

		TemplateAndRecentProjectsSwitcher->SetActiveWidgetIndex(0);
		PathAreaSwitcher->SetActiveWidgetIndex(0);
	}
}

TSharedRef<ITableRow> SProjectDialog::ConstructMajorCategoryTableRow(TSharedPtr<FTemplateCategory> Item, const TSharedRef<STableViewBase>& TableView)
{
	TWeakPtr<FTemplateCategory> CurrentItem = Item;

	TSharedRef<STableRow<TSharedPtr<FTemplateCategory>>> Row =
		SNew(STableRow<TSharedPtr<FTemplateCategory>>, TableView)
		.Style(FAppStyle::Get(), "ProjectBrowser.TableRow")
		.ShowSelection(false)
		.Padding(2.0f);
	TWeakPtr<STableRow<TSharedPtr<FTemplateCategory>>> RowWeakPtr = Row;

	Row->SetContent(
		SNew(SMajorCategoryTile, Item)
		.IsSelected_Lambda([CurrentItem, this]() { return CurrentItem == CurrentCategory; })
	);

	return Row;
}

#undef LOCTEXT_NAMESPACE
