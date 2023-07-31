// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomImportOptionsWindow.h"

#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "GroomCacheImportOptions.h"
#include "GroomImportOptions.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "GroomAsset.h"

#define LOCTEXT_NAMESPACE "GroomImportOptionsWindow"

enum class EHairDescriptionStatus
{
	Valid,
	NoGroup,
	NoCurve,
	Unknown
};

static EHairDescriptionStatus GetStatus(UGroomHairGroupsPreview* Description)
{
	if (Description)
	{
		for (const FGroomHairGroupPreview& Group : Description->Groups)
		{
			if (Group.CurveCount == 0)
				return EHairDescriptionStatus::NoCurve;
		}

		if (Description->Groups.Num() == 0)
		{
			return EHairDescriptionStatus::NoGroup;
		}
		return EHairDescriptionStatus::Valid;
	}
	else
	{
		return EHairDescriptionStatus::Unknown;
	}
}

void SGroomImportOptionsWindow::Construct(const FArguments& InArgs)
{
	ImportOptions = InArgs._ImportOptions;
	GroomCacheImportOptions = InArgs._GroomCacheImportOptions;
	GroupsPreview = InArgs._GroupsPreview;
	WidgetWindow = InArgs._WidgetWindow;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(ImportOptions);
	
	DetailsView2 = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView2->SetObject(GroupsPreview);

	GroomCacheDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	GroomCacheDetailsView->SetObject(GroomCacheImportOptions);

	const EHairDescriptionStatus Status = GetStatus(GroupsPreview);

	FText ValidationText;
	FLinearColor ValidationColor(1,1,1);
	if (Status == EHairDescriptionStatus::Valid)
	{
		ValidationText = LOCTEXT("GroomOptionsWindow_ValidationText0", "Valid");
		ValidationColor = FLinearColor(0, 0.80f, 0, 1);
	}
	else if (Status == EHairDescriptionStatus::NoCurve)
	{
		ValidationText = LOCTEXT("GroomOptionsWindow_ValidationText1", "Invalid. Some groups have 0 curves.");
		ValidationColor = FLinearColor(0.80f, 0, 0, 1);
	}
	else if (Status == EHairDescriptionStatus::NoGroup)
	{
		ValidationText = LOCTEXT("GroomOptionsWindow_ValidationText2", "Invalid. The groom does not contain any group.");
		ValidationColor = FLinearColor(1, 0, 0, 1);
	}
	else
	{
		ValidationText = LOCTEXT("GroomOptionsWindow_ValidationText3", "Unknown");
	}

	const FSlateFontInfo AttributeFont = FAppStyle::GetFontStyle("CurveEd.InfoFont");
	const FSlateFontInfo AttributeResultFont = FAppStyle::GetFontStyle("CurveEd.InfoFont");
	const FLinearColor AttributeColor(0.80f, 0.80f, 0.80f);
	const FText TrueText  = LOCTEXT("GroomOptionsWindow_AttributeTrue", "True");
	const FText FalseText = LOCTEXT("GroomOptionsWindow_AttributeFalse", "False");

	FText HasRootUVText = FalseText;
	FText HasColorAttributeText = FalseText;
	FText HasRoughnessAttributeText = FalseText;
	FText HasGuideWeightsText = FalseText;

	if (GroupsPreview)
	{
		for (const FGroomHairGroupPreview& Group : GroupsPreview->Groups)
		{
			if (Group.bHasRootUV)				{ HasRootUVText = TrueText; }
			if (Group.bHasColorAttributes)		{ HasColorAttributeText = TrueText; }
			if (Group.bHasRoughnessAttributes)	{ HasRoughnessAttributeText = TrueText; }
			if (Group.bHasPrecomputedWeights)	{ HasGuideWeightsText = TrueText; }
		}
	}

	this->ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("CurrentFile", "Current File: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("CurveEd.InfoFont"))
					.Text(InArgs._FullPath)
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("CurveEd.LabelFont"))
					.Text(LOCTEXT("GroomOptionsWindow_StatusFile", "Status File: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("CurveEd.InfoFont"))
					.Text(ValidationText)
					.ColorAndOpacity(ValidationColor)
				]
			]
		]

		// Root UV
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(AttributeFont)
					.Text(LOCTEXT("GroomOptionsWindow_HasRootUV", "Has Root UV: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(AttributeResultFont)
					.Text(HasRootUVText)
					.ColorAndOpacity(AttributeColor)
				]
			]
		]


		// Color attributes
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(AttributeFont)
					.Text(LOCTEXT("GroomOptionsWindow_HasColor", "Has Color Attributes: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(AttributeResultFont)
					.Text(HasColorAttributeText)
					.ColorAndOpacity(AttributeColor)
				]
			]
		]

		// Roughness attributes
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(AttributeFont)
					.Text(LOCTEXT("GroomOptionsWindow_HasRoughness", "Has Roughness Attributes: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(AttributeResultFont)
					.Text(HasRoughnessAttributeText)
					.ColorAndOpacity(AttributeColor)
				]
			]
		]

		// Guide weights
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)
		[
			SNew(SBorder)
			.Padding(FMargin(3))
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(AttributeFont)
					.Text(LOCTEXT("GroomOptionsWindow_HasGuideWeights", "Has Pre-Computed Guides Weights: "))
				]
				+ SHorizontalBox::Slot()
				.Padding(5, 0, 0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(AttributeResultFont)
					.Text(HasGuideWeightsText)
					.ColorAndOpacity(AttributeColor)
				]
			]
		]

		+ SVerticalBox::Slot()
		.Padding(2)
		.MaxHeight(500.0f)
		[
			DetailsView->AsShared()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)		
		[
			GroomCacheDetailsView->AsShared()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2)		
		[
			DetailsView2->AsShared()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.Padding(2)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(2)
			+ SUniformGridPanel::Slot(0, 0)
			[
				SAssignNew(ImportButton, SButton)
				.HAlign(HAlign_Center)
				.Text(InArgs._ButtonLabel)
				.IsEnabled(this, &SGroomImportOptionsWindow::CanImport)
				.OnClicked(this, &SGroomImportOptionsWindow::OnImport)
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked(this, &SGroomImportOptionsWindow::OnCancel)
			]
		]
	];
}

bool SGroomImportOptionsWindow::CanImport()  const
{
	if (GroupsPreview)
	{
		for (const FGroomHairGroupPreview& Group : GroupsPreview->Groups)
		{
			if (Group.CurveCount == 0)
				return false;
		}

		if (GroupsPreview->Groups.Num() == 0)
		{
			return false;
		}
	}

	// For the GroomCache import, if we don't import the groom asset, a compatible replacement must be specified
	// If neither the asset nor the cache is imported, then there's nothing to import
	if (GroomCacheImportOptions && !GroomCacheImportOptions->ImportSettings.bImportGroomAsset &&
		(!GroomCacheImportOptions->ImportSettings.GroomAsset.IsValid() || !GroomCacheImportOptions->ImportSettings.bImportGroomCache))
	{
		// TODO: Test for compatibility between cache and selected groom asset
		return false;
	}
	return true;
}

enum class EGroomOptionsVisibility : uint8
{
	None = 0x00,
	ConversionOptions = 0x01,
	BuildOptions = 0x02,
	All = ConversionOptions | BuildOptions
};

ENUM_CLASS_FLAGS(EGroomOptionsVisibility);

TSharedPtr<SGroomImportOptionsWindow> DisplayOptions(
	UGroomImportOptions* ImportOptions, 
	UGroomCacheImportOptions* GroomCacheImportOptions, 
	UGroomHairGroupsPreview* GroupsPreview,
	const FString& FilePath, 
	EGroomOptionsVisibility VisibilityFlag, 
	FText WindowTitle, 
	FText InButtonLabel)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<SGroomImportOptionsWindow> OptionsWindow;

	FProperty* ConversionOptionsProperty = FindFProperty<FProperty>(ImportOptions->GetClass(), GET_MEMBER_NAME_CHECKED(UGroomImportOptions, ConversionSettings));
	if (ConversionOptionsProperty)
	{
		if (EnumHasAnyFlags(VisibilityFlag, EGroomOptionsVisibility::ConversionOptions))
		{
			ConversionOptionsProperty->SetMetaData(TEXT("ShowOnlyInnerProperties"), TEXT("1"));
			ConversionOptionsProperty->SetMetaData(TEXT("Category"), TEXT("Conversion"));
		}
		else
		{
			// Note that UGroomImportOptions HideCategories named "Hidden",
			// but the hiding doesn't work with ShowOnlyInnerProperties 
			ConversionOptionsProperty->RemoveMetaData(TEXT("ShowOnlyInnerProperties"));
			ConversionOptionsProperty->SetMetaData(TEXT("Category"), TEXT("Hidden"));
		}
	}

	FString FileName = FPaths::GetCleanFilename(FilePath);
	Window->SetContent
	(
		SAssignNew(OptionsWindow, SGroomImportOptionsWindow)
		.ImportOptions(ImportOptions)
		.GroomCacheImportOptions(GroomCacheImportOptions)
		.GroupsPreview(GroupsPreview)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FileName))
		.ButtonLabel(InButtonLabel)
	);

	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	return OptionsWindow;
}

TSharedPtr<SGroomImportOptionsWindow> SGroomImportOptionsWindow::DisplayImportOptions(UGroomImportOptions* ImportOptions, UGroomCacheImportOptions* GroomCacheImportOptions, UGroomHairGroupsPreview* GroupsPreview, const FString& FilePath)
{
	// If there's no groom cache to import, don't show its import options
	UGroomCacheImportOptions* GroomCacheOptions = GroomCacheImportOptions && GroomCacheImportOptions->ImportSettings.bImportGroomCache ? GroomCacheImportOptions : nullptr;
	return DisplayOptions(ImportOptions, GroomCacheOptions, GroupsPreview, FilePath, EGroomOptionsVisibility::All, LOCTEXT("GroomImportWindowTitle", "Groom Import Options"), LOCTEXT("Import", "Import"));
}

TSharedPtr<SGroomImportOptionsWindow> SGroomImportOptionsWindow::DisplayRebuildOptions(UGroomImportOptions* ImportOptions, UGroomHairGroupsPreview* GroupsPreview, const FString& FilePath)
{
	return DisplayOptions(ImportOptions, nullptr, GroupsPreview, FilePath, EGroomOptionsVisibility::BuildOptions, LOCTEXT("GroomRebuildWindowTitle ", "Groom Build Options"), LOCTEXT("Build", "Build"));
}


#undef LOCTEXT_NAMESPACE
