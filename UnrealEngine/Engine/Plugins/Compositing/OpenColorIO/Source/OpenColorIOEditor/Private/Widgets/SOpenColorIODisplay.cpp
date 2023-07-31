// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOpenColorIODisplay.h"

#include "Framework/Application/SlateApplication.h"
#include "OpenColorIOConfiguration.h"
#include "PropertyCustomizationHelpers.h"
#include "SOpenColorIOColorSpacePicker.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"


#define LOCTEXT_NAMESPACE "SOpenColorIODisplay"


namespace OpenColorIODisplayMenuUtils
{
	static const FName DisplayConfigMenuName("OpenColorIODisplay.DisplayConfiguration");
}

void SOpenColorIODisplay::Construct(const FArguments& InArgs)
{
	Viewport = InArgs._Viewport;
	Configuration = InArgs._InitialConfiguration;
	OnDisplayConfigurationChanged = InArgs._OnConfigurationChanged;
	check(Viewport);

	// Register configuration menu if it hasn't been yet
	UToolMenu* Menu = UToolMenus::Get()->FindMenu(OpenColorIODisplayMenuUtils::DisplayConfigMenuName);
	if (Menu == nullptr)
	{
		Menu = UToolMenus::Get()->RegisterMenu(OpenColorIODisplayMenuUtils::DisplayConfigMenuName);
	}

	check(Menu);

	// Add section to enable/disable the display look
	FToolMenuSection& Section = Menu->AddSection("DisplayConfiguration", FText::GetEmpty());
	Section.AddMenuEntry(
		"EnableDisplay",
		LOCTEXT("EnableDisplay_Label", "Enable Display"),
		LOCTEXT("EnableDisplay_ToolTip", "Toggle Display color conversion"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateRaw(this, &SOpenColorIODisplay::ToggleEnableDisplay),
			FCanExecuteAction::CreateRaw(this, &SOpenColorIODisplay::CanEnableDisplayConfiguration),
			FIsActionChecked::CreateRaw(this, &SOpenColorIODisplay::GetDisplayConfigurationState)),
		EUserInterfaceActionType::ToggleButton);

	// Add a section to select the OCIO configuration asset from which color spaces will be taken from
	Section.AddEntry(FToolMenuEntry::InitSubMenu(
		"OCIOAssetSelection",
		TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SOpenColorIODisplay::GetConfigurationText)),
		LOCTEXT("OCIOAssetSelection_Tooltip", "Select the asset from which color spaces will be fetched"),
		FNewToolMenuDelegate::CreateRaw(this, &SOpenColorIODisplay::SelectConfigSubMenu),
		FUIAction(),
		EUserInterfaceActionType::None
	));

	// Source color space picker widget
	SourceColorSpace =
		SNew(SOpenColorIOColorSpacePicker)
		.Config(Configuration.ColorConfiguration.ConfigurationSource)
		.InitialColorSpace(Configuration.ColorConfiguration.SourceColorSpace)
		.RestrictedColor(Configuration.ColorConfiguration.DestinationColorSpace)
		.InitialDisplayView(FOpenColorIODisplayView())
		.IsDestination(false)
		.OnColorSpaceChanged(FOnColorSpaceChanged::CreateSP(this, &SOpenColorIODisplay::OnSourceColorSpaceChanged));

	// Source color space picker widget
	DestinationColorSpace =
		SNew(SOpenColorIOColorSpacePicker)
		.Config(Configuration.ColorConfiguration.ConfigurationSource)
		.InitialColorSpace(Configuration.ColorConfiguration.DestinationColorSpace)
		.RestrictedColor(Configuration.ColorConfiguration.SourceColorSpace)
		.InitialDisplayView(Configuration.ColorConfiguration.DestinationDisplayView)
		.IsDestination(true)
		.OnColorSpaceChanged(FOnColorSpaceChanged::CreateSP(this, &SOpenColorIODisplay::OnDestinationColorSpaceChanged));
	
	Section.AddEntry(FToolMenuEntry::InitWidget("SourceColor", SourceColorSpace.ToSharedRef(), FText::GetEmpty(), true, false));
	Section.AddEntry(FToolMenuEntry::InitWidget("DestinationColor", DestinationColorSpace.ToSharedRef(), FText::GetEmpty(), true, false));

	TSharedRef<SWidget> Widget = UToolMenus::Get()->GenerateWidget(Menu);
	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0)
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(0.f, 0.f, 4.f, 0.f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					Widget
				]
			]
		];
}

FText SOpenColorIODisplay::GetConfigurationText() const
{
	if (Configuration.ColorConfiguration.ConfigurationSource)
	{
		return FText::FromName(Configuration.ColorConfiguration.ConfigurationSource->GetFName());
	}
	else
	{
		return LOCTEXT("OCIOAssetSelection_Label", "Select an OCIO Asset");
	}
}

void SOpenColorIODisplay::SelectConfigSubMenu(UToolMenu* Menu)
{
	FAssetData CurrentAssetData = Configuration.ColorConfiguration.ConfigurationSource ? FAssetData(Configuration.ColorConfiguration.ConfigurationSource) : FAssetData();

	TArray<const UClass*> ClassFilters;
	ClassFilters.Add(UOpenColorIOConfiguration::StaticClass());

	FToolMenuSection& AssetSelectionMenu = Menu->AddSection("OCIOAssetSelection", LOCTEXT("OCIOAssetSelection_GenericLabel", "Asset selection"));
	ClassPicker = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		CurrentAssetData,
		Configuration.ColorConfiguration.ConfigurationSource != nullptr,
		false,
		ClassFilters,
		TArray<UFactory*>(),
		FOnShouldFilterAsset::CreateLambda([CurrentAssetData](const FAssetData& InAssetData) { return InAssetData == CurrentAssetData; }),
		FOnAssetSelected::CreateSP(this, &SOpenColorIODisplay::OnConfigSelected),
		FSimpleDelegate()
	);
	
	AssetSelectionMenu.AddEntry(FToolMenuEntry::InitWidget("OCIOAssetPickerWidget",
		ClassPicker.ToSharedRef(),
		FText::GetEmpty(),
		true, //NoIndent
		false)); //Searchable
}

void SOpenColorIODisplay::OnConfigSelected(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissMenuByWidget(ClassPicker.ToSharedRef());

	UOpenColorIOConfiguration* NewConfig = Cast<UOpenColorIOConfiguration>(AssetData.GetAsset());
	Configuration.ColorConfiguration.ConfigurationSource = NewConfig;
	SourceColorSpace->SetConfiguration(NewConfig);
	DestinationColorSpace->SetConfiguration(NewConfig);

	OnDisplayConfigurationChanged.ExecuteIfBound(Configuration);
}

void SOpenColorIODisplay::ToggleEnableDisplay()
{
	Configuration.bIsEnabled = !Configuration.bIsEnabled;

	OnDisplayConfigurationChanged.ExecuteIfBound(Configuration);
}

bool SOpenColorIODisplay::CanEnableDisplayConfiguration()
{
	//For now, always allow it to be enabled. We could add filters based on current view modes
	return true;
}

bool SOpenColorIODisplay::GetDisplayConfigurationState()
{
	return Configuration.bIsEnabled;
}

void SOpenColorIODisplay::OnSourceColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView&)
{
	//When color space changes, assign it, update restriction on the other picker and let know our listeners we changed
	Configuration.ColorConfiguration.SourceColorSpace = NewColorSpace;
	DestinationColorSpace->SetRestrictedColorSpace(NewColorSpace);
	OnDisplayConfigurationChanged.ExecuteIfBound(Configuration);
}

void SOpenColorIODisplay::OnDestinationColorSpaceChanged(const FOpenColorIOColorSpace& NewColorSpace, const FOpenColorIODisplayView& NewDisplayView)
{
	Configuration.ColorConfiguration.DestinationColorSpace = NewColorSpace;
	Configuration.ColorConfiguration.DestinationDisplayView = NewDisplayView;
	OnDisplayConfigurationChanged.ExecuteIfBound(Configuration);
}

#undef LOCTEXT_NAMESPACE
