// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpaceCustomization.h"

#include "Containers/StringConv.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SComboButton.h"

#define LOCTEXT_NAMESPACE "OpenColorIOColorSpaceCustomization"

void FOpenColorIOColorSpaceCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	//Reset internals
	ConfigurationFileProperty.Reset();
	CachedConfigFile.reset();
	LoadedFilePath = FFilePath();
	
	CachedProperty = InPropertyHandle;

	if (CachedProperty->GetNumPerObjectValues() == 1 && CachedProperty->IsValidHandle())
	{
		FProperty* Property = CachedProperty->GetProperty();
		check(Property && CastField<FStructProperty>(Property) && CastField<FStructProperty>(Property)->Struct && CastField<FStructProperty>(Property)->Struct->IsChildOf(FOpenColorIOColorSpace::StaticStruct()));

		TArray<void*> RawData;
		CachedProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FOpenColorIOColorSpace* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

		check(ColorSpaceValue);
		TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

		// Get the ConfigurationFile to read color space from
		ConfigurationFileProperty = GetConfigurationFileProperty();

		HeaderRow
			.NameContent()
			[
				InPropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(512)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(MakeAttributeLambda([=] { return FText::FromString(ColorSpaceValue->ToString()); }))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FOpenColorIOColorSpaceCustomization::HandleSourceComboButtonMenuContent)
					.ContentPadding(FMargin(4.0, 2.0))
				]
			].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
	}
}

void FOpenColorIODisplayViewCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	//Reset internals
	ConfigurationFileProperty.Reset();
	CachedConfigFile.reset();
	LoadedFilePath = FFilePath();

	CachedProperty = InPropertyHandle;

	if (CachedProperty->GetNumPerObjectValues() == 1 && CachedProperty->IsValidHandle())
	{
		FProperty* Property = CachedProperty->GetProperty();
		check(Property && CastField<FStructProperty>(Property) && CastField<FStructProperty>(Property)->Struct && CastField<FStructProperty>(Property)->Struct->IsChildOf(FOpenColorIODisplayView::StaticStruct()));

		TArray<void*> RawData;
		CachedProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FOpenColorIODisplayView* DisplayViewValue = reinterpret_cast<FOpenColorIODisplayView*>(RawData[0]);

		check(DisplayViewValue);
		TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();

		// Get the ConfigurationFile to read color space from
		ConfigurationFileProperty = GetConfigurationFileProperty();

		HeaderRow
			.NameContent()
			[
				InPropertyHandle->CreatePropertyNameWidget()
			]
		.ValueContent()
			.MaxDesiredWidth(512)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(MakeAttributeLambda([=] { return FText::FromString(DisplayViewValue->ToString()); }))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FOpenColorIODisplayViewCustomization::HandleSourceComboButtonMenuContent)
					.ContentPadding(FMargin(4.0, 2.0))
				]
			].IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }));
	}
}

TSharedPtr<IPropertyHandle> IPropertyTypeCustomizationOpenColorIO::GetConfigurationFileProperty() const
{
	if (CachedProperty.IsValid())
	{
		static FName NAME_ConfigFile = TEXT("OCIOConfigFile");

		//Verify if we're in an array before accessing the property directly. Need to go deeper for array properties.
		TSharedPtr<IPropertyHandle> ParentArrayHandle = CachedProperty->GetParentHandle();
		TSharedPtr<IPropertyHandleArray> ParentHandleAsArray = ParentArrayHandle->AsArray();
		if (ParentHandleAsArray.IsValid())
		{
			//Struct is in an array, parent class is access through more layers
			if (ParentArrayHandle->GetProperty()->HasMetaData(NAME_ConfigFile))
			{
				const FString& ConfigFileVariableName = ParentArrayHandle->GetProperty()->GetMetaData(NAME_ConfigFile);
				TSharedPtr<IPropertyHandle> ClassHandle = ParentArrayHandle->GetParentHandle()->GetParentHandle();
				if (ClassHandle.IsValid() && !ConfigFileVariableName.IsEmpty())
				{
					return ClassHandle->GetChildHandle(*ConfigFileVariableName);
				}
			}
		}
		else
		{
			if (CachedProperty->HasMetaData(NAME_ConfigFile))
			{
				const FString& ConfigFileVariableName = CachedProperty->GetMetaData(NAME_ConfigFile);
				TSharedPtr<IPropertyHandle> ClassHandle = CachedProperty->GetParentHandle();
				if (ClassHandle.IsValid() && !ConfigFileVariableName.IsEmpty())
				{
					return ClassHandle->GetChildHandle(*ConfigFileVariableName);
				}
			}
		}
	}

	return nullptr;
}

bool IPropertyTypeCustomizationOpenColorIO::LoadConfigurationFile(const FFilePath& InFilePath)
{
#if WITH_OCIO
#if !PLATFORM_EXCEPTIONS_DISABLED
	try
#endif
	{
		FString FullPath;
		FString ConfigurationFilePath = InFilePath.FilePath;
		if (ConfigurationFilePath.Contains(TEXT("{Engine}")))
		{
			ConfigurationFilePath = FPaths::ConvertRelativePathToFull(ConfigurationFilePath.Replace(TEXT("{Engine}"), *FPaths::EngineDir()));
		}    

		if (!FPaths::IsRelative(ConfigurationFilePath))
		{
			FullPath = ConfigurationFilePath;
		}
		else
		{
			const FString AbsoluteGameDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
			FullPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(AbsoluteGameDir, ConfigurationFilePath));
		}

		CachedConfigFile = OCIO_NAMESPACE::Config::CreateFromFile(StringCast<ANSICHAR>(*FullPath).Get());
		if (!CachedConfigFile)
		{
			return false;
		}
	}
#if !PLATFORM_EXCEPTIONS_DISABLED
	catch (OCIO_NAMESPACE::Exception&)
	{
		return false;
	}
#endif

	return true;
#else //WITH_OCIO
	return false;
#endif //WITH_OCIO
}

bool IPropertyTypeCustomizationOpenColorIO::CheckValidConfiguration()
{
	if (ConfigurationFileProperty.IsValid())
	{
		TArray<void*> RawData;
		ConfigurationFileProperty->AccessRawData(RawData);

		check(RawData.Num() == 1);
		FFilePath* ConfigFilePath = reinterpret_cast<FFilePath*>(RawData[0]);
		check(ConfigFilePath);

		if (!ConfigFilePath->FilePath.IsEmpty() && ConfigFilePath->FilePath != LoadedFilePath.FilePath)
		{
			return LoadConfigurationFile(*ConfigFilePath);
		}
	}

	return false;
}

void FOpenColorIOColorSpaceCustomization::ProcessColorSpaceForMenuGeneration(FMenuBuilder& InMenuBuilder, const int32 InMenuDepth, const FString& InPreviousFamilyHierarchy, const FOpenColorIOColorSpace& InColorSpace, TArray<FString>& InOutExistingMenuFilter)
{
	const FString NextFamilyName = InColorSpace.GetFamilyNameAtDepth(InMenuDepth);
	if (!NextFamilyName.IsEmpty())
	{
		if (!InOutExistingMenuFilter.Contains(NextFamilyName))
		{
			//Only add the previous family and delimiter if there was one. First family doesn't need it.
			const FString PreviousHierarchyToAdd = !InPreviousFamilyHierarchy.IsEmpty() ? InPreviousFamilyHierarchy + FOpenColorIOColorSpace::FamilyDelimiter : TEXT("");
			const FString NewHierarchy = PreviousHierarchyToAdd + NextFamilyName;;
			const int32 NextMenuDepth = InMenuDepth + 1;
			InMenuBuilder.AddSubMenu(
				FText::FromString(NextFamilyName),
				LOCTEXT("OpensFamilySubMenu", "ColorSpace Family Sub Menu"),
				FNewMenuDelegate::CreateRaw(this, &FOpenColorIOColorSpaceCustomization::PopulateSubMenu, NextMenuDepth, NewHierarchy)
			);

			InOutExistingMenuFilter.Add(NextFamilyName);
		}
	}
	else
	{

		AddMenuEntry(InMenuBuilder, InColorSpace);
	}
}

void FOpenColorIOColorSpaceCustomization::PopulateSubMenu(FMenuBuilder& InMenuBuilder, int32 InMenuDepth, FString InPreviousFamilyHierarchy)
{
	//Submenus should always be at a certain depth level
	check(InMenuDepth > 0);

	// To keep track of submenus that were already added
	TArray<FString> ExistingSubMenus;

	const int32 ColorSpaceCount = CachedConfigFile->getNumColorSpaces(OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_ACTIVE);
	for (int32 i = 0; i < ColorSpaceCount; ++i)
	{
		const char* ColorSpaceName = CachedConfigFile->getColorSpaceNameByIndex(OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_ACTIVE, i);
		OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = CachedConfigFile->getColorSpace(ColorSpaceName);
		if (!LibColorSpace)
		{
			continue;
		}

		FOpenColorIOColorSpace ColorSpace;
		ColorSpace.ColorSpaceIndex = i;
		ColorSpace.ColorSpaceName = StringCast<TCHAR>(ColorSpaceName).Get();
		ColorSpace.FamilyName = StringCast<TCHAR>(LibColorSpace->getFamily()).Get();
		
		//Filter out color spaces that don't belong to this hierarchy
		if (InPreviousFamilyHierarchy.IsEmpty() || ColorSpace.FamilyName.Contains(InPreviousFamilyHierarchy))
		{
			ProcessColorSpaceForMenuGeneration(InMenuBuilder, InMenuDepth, InPreviousFamilyHierarchy, ColorSpace, ExistingSubMenus);
		}
	}
}

void FOpenColorIOColorSpaceCustomization::AddMenuEntry(FMenuBuilder& InMenuBuilder, const FOpenColorIOColorSpace& InColorSpace)
{
	InMenuBuilder.AddMenuEntry(
		FText::FromString(InColorSpace.ToString()),
		FText::FromString(InColorSpace.ToString()),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]
			{
				if (FStructProperty* StructProperty = CastField<FStructProperty>(CachedProperty->GetProperty()))
				{
					TArray<void*> RawData;
					CachedProperty->AccessRawData(RawData);
					FOpenColorIOColorSpace* PreviousColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);

					FString TextValue;
					StructProperty->Struct->ExportText(TextValue, &InColorSpace, PreviousColorSpaceValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
					ensure(CachedProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
				}
			}
			),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]
			{
				TArray<void*> RawData;
				CachedProperty->AccessRawData(RawData);
				FOpenColorIOColorSpace* ColorSpaceValue = reinterpret_cast<FOpenColorIOColorSpace*>(RawData[0]);
				return *ColorSpaceValue == InColorSpace;
			})
		),
		NAME_None,
		EUserInterfaceActionType::RadioButton
		);
}

TSharedRef<SWidget> FOpenColorIOColorSpaceCustomization::HandleSourceComboButtonMenuContent()
{
	bool bValidConfiguration = CheckValidConfiguration();

	// Generate menu
	FMenuBuilder MenuBuilder(true, nullptr);
	TArray<FString> ExistingSubMenus;

	MenuBuilder.BeginSection("AllColorSpaces", LOCTEXT("AllColorSpacesSection", "ColorSpaces"));
	{
		if (bValidConfiguration)
		{
			const int32 ColorSpaceCount = CachedConfigFile->getNumColorSpaces(OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_ACTIVE);
			for (int32 i = 0; i < ColorSpaceCount; ++i)
			{
				const char* ColorSpaceName = CachedConfigFile->getColorSpaceNameByIndex(OCIO_NAMESPACE::SEARCH_REFERENCE_SPACE_ALL, OCIO_NAMESPACE::COLORSPACE_ACTIVE, i);
				OCIO_NAMESPACE::ConstColorSpaceRcPtr LibColorSpace = CachedConfigFile->getColorSpace(ColorSpaceName);
				if (!LibColorSpace)
				{
					continue;
				}

				FOpenColorIOColorSpace ColorSpace;
				ColorSpace.ColorSpaceIndex = i;
				ColorSpace.ColorSpaceName = StringCast<TCHAR>(ColorSpaceName).Get();
				ColorSpace.FamilyName = StringCast<TCHAR>(LibColorSpace->getFamily()).Get();

				//Top level menus have no preceding hierarchy.
				const int32 CurrentMenuDepth = 0;
				const FString PreviousHierarchy = FString();
				ProcessColorSpaceForMenuGeneration(MenuBuilder, CurrentMenuDepth, PreviousHierarchy, ColorSpace, ExistingSubMenus);
			}

			if (ColorSpaceCount <= 0)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoColorSpaceFound", "No color space found"), false, false);
			}
		}
		else
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("InvalidConfigurationFile", "Invalid configuration file"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FOpenColorIODisplayViewCustomization::PopulateViewSubMenu(FMenuBuilder& InMenuBuilder, FOpenColorIODisplayView InDisplayView)
{
	const int32 ViewCount = CachedConfigFile->getNumViews(TCHAR_TO_ANSI(*InDisplayView.Display));
	for (int32 i = 0; i < ViewCount; ++i)
	{
		const char* ViewName = CachedConfigFile->getView(TCHAR_TO_ANSI(*InDisplayView.Display), i);
		InDisplayView.View = StringCast<TCHAR>(ViewName).Get();
		
		AddMenuEntry(InMenuBuilder, InDisplayView);
	}
}

void FOpenColorIODisplayViewCustomization::AddMenuEntry(FMenuBuilder& InMenuBuilder, const FOpenColorIODisplayView& InDisplayView)
{
	InMenuBuilder.AddMenuEntry(
		FText::FromString(InDisplayView.View),
		FText::FromString(InDisplayView.ToString()),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=]
				{
					if (FStructProperty* StructProperty = CastField<FStructProperty>(CachedProperty->GetProperty()))
					{
						TArray<void*> RawData;
						CachedProperty->AccessRawData(RawData);
						FOpenColorIODisplayView* PreviousDisplayViewValue = reinterpret_cast<FOpenColorIODisplayView*>(RawData[0]);

						FString TextValue;
						StructProperty->Struct->ExportText(TextValue, &InDisplayView, PreviousDisplayViewValue, nullptr, EPropertyPortFlags::PPF_None, nullptr);
						ensure(CachedProperty->SetValueFromFormattedString(TextValue, EPropertyValueSetFlags::DefaultFlags) == FPropertyAccess::Result::Success);
					}
				}
			),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]
				{
					TArray<void*> RawData;
					CachedProperty->AccessRawData(RawData);
					FOpenColorIODisplayView* DisplayViewValue = reinterpret_cast<FOpenColorIODisplayView*>(RawData[0]);
					return *DisplayViewValue == InDisplayView;
				})
			),
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);
}

TSharedRef<SWidget> FOpenColorIODisplayViewCustomization::HandleSourceComboButtonMenuContent()
{
	bool bValidConfiguration = CheckValidConfiguration();

	// Generate menu
	FMenuBuilder MenuBuilder(true, nullptr);
	TArray<FString> ExistingSubMenus;

	MenuBuilder.BeginSection("AllDisplayViews", LOCTEXT("AllDisplayViewsSection", "Display - View"));
	{
		if (bValidConfiguration)
		{
			const int32 DisplayCount = CachedConfigFile->getNumDisplays();
			for (int32 i = 0; i < DisplayCount; ++i)
			{
				const char* DisplayName = CachedConfigFile->getDisplay(i);

				const FOpenColorIODisplayView DisplayViewValue = { StringCast<TCHAR>(DisplayName).Get(), TEXT("<Invalid>") };

				MenuBuilder.AddSubMenu(
					FText::FromString(DisplayViewValue.Display),
					LOCTEXT("OpensDisplayViewSubMenu", "Display - View Family Sub Menu"),
					FNewMenuDelegate::CreateRaw(this, &FOpenColorIODisplayViewCustomization::PopulateViewSubMenu, DisplayViewValue)
				);
			}

			if (DisplayCount <= 0)
			{
				MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("NoDisplayFound", "No display found"), false, false);
			}
		}
		else
		{
			MenuBuilder.AddWidget(SNullWidget::NullWidget, LOCTEXT("InvalidConfigurationFile", "Invalid configuration file"), false, false);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
