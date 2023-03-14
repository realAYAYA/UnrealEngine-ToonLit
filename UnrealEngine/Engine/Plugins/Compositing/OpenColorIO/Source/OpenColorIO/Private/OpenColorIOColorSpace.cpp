// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenColorIOColorSpace.h"

#include "OpenColorIOConfiguration.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenColorIOColorSpace)

/*
 * FOpenColorIOColorSpace implementation
 */

const TCHAR* FOpenColorIOColorSpace::FamilyDelimiter = TEXT("/");

FOpenColorIOColorSpace::FOpenColorIOColorSpace()
	: ColorSpaceIndex(INDEX_NONE)
{ }

FOpenColorIOColorSpace::FOpenColorIOColorSpace(const FString& InColorSpaceName, int32 InColorSpaceIndex, const FString& InFamilyName)
	: ColorSpaceName(InColorSpaceName)
	, ColorSpaceIndex(InColorSpaceIndex)
	, FamilyName(InFamilyName)
{ }

FString FOpenColorIOColorSpace::ToString() const
{
	if (IsValid())
	{
		return ColorSpaceName;
	}
	return TEXT("<Invalid>");
}

bool FOpenColorIOColorSpace::IsValid() const
{
	return ColorSpaceIndex != INDEX_NONE && !ColorSpaceName.IsEmpty();
}

void FOpenColorIOColorSpace::Reset()
{
	ColorSpaceIndex = INDEX_NONE;
	ColorSpaceName.Reset();
	FamilyName.Reset();
}

FString FOpenColorIOColorSpace::GetFamilyNameAtDepth(int32 InDepth) const
{
	FString ReturnName;

	TArray<FString> Families;
	FamilyName.ParseIntoArray(Families, FamilyDelimiter);
	if (Families.IsValidIndex(InDepth))
	{
		ReturnName = Families[InDepth];
	}
	else
	{
		//No separator found, does it want the first family?
		if (InDepth == 0 && !FamilyName.IsEmpty())
		{
			ReturnName = FamilyName;
		}
	}

	return ReturnName;
}

/*
 * FOpenColorIODisplayView implementation
 */

FOpenColorIODisplayView::FOpenColorIODisplayView()
	: Display()
	, View()
{
}

FOpenColorIODisplayView::FOpenColorIODisplayView(FStringView InDisplayName, FStringView InViewName)
	: Display(InDisplayName)
	, View(InViewName)
{
}

FString FOpenColorIODisplayView::ToString() const
{
	if (IsValid())
	{
		return Display + TEXT(" - ") + View;
	}

	return TEXT("<Invalid>");
}

bool FOpenColorIODisplayView::IsValid() const
{
	return !Display.IsEmpty() && !View.IsEmpty();
}

void FOpenColorIODisplayView::Reset()
{
	Display.Reset();
	View.Reset();
}

/*
 * FOpenColorIOColorConversionSettings implementation
 */

FOpenColorIOColorConversionSettings::FOpenColorIOColorConversionSettings()
	: ConfigurationSource(nullptr)
{

}

FString FOpenColorIOColorConversionSettings::ToString() const
{
	if (ConfigurationSource)
	{
		if (IsDisplayView())
		{
			switch (DisplayViewDirection)
			{
			case EOpenColorIOViewTransformDirection::Forward:
				return FString::Printf(TEXT("%s config - %s to %s"), *ConfigurationSource->GetName(), *SourceColorSpace.ToString(), *DestinationDisplayView.ToString());
			case EOpenColorIOViewTransformDirection::Inverse:
				return FString::Printf(TEXT("%s config - %s to %s"), *ConfigurationSource->GetName(), *DestinationDisplayView.ToString(), *SourceColorSpace.ToString());
			default:
				checkNoEntry();
				return FString();
			}
		}
		else
		{
			return FString::Printf(TEXT("%s config - %s to %s"), *ConfigurationSource->GetName(), *SourceColorSpace.ToString(), *DestinationColorSpace.ToString());
		}
	}
	return TEXT("<Invalid Conversion>");
}

bool FOpenColorIOColorConversionSettings::IsValid() const
{
	if (ConfigurationSource)
	{
		if (IsDisplayView())
		{
			switch (DisplayViewDirection)
			{
			case EOpenColorIOViewTransformDirection::Forward:
				return ConfigurationSource->HasTransform(SourceColorSpace.ColorSpaceName, DestinationDisplayView.Display, DestinationDisplayView.View, EOpenColorIOViewTransformDirection::Forward);
			case EOpenColorIOViewTransformDirection::Inverse:
				return ConfigurationSource->HasTransform(SourceColorSpace.ColorSpaceName, DestinationDisplayView.Display, DestinationDisplayView.View, EOpenColorIOViewTransformDirection::Inverse);
			default:
				checkNoEntry();
				return false;
			}
		}
		else
		{
			return ConfigurationSource->HasTransform(SourceColorSpace.ColorSpaceName, DestinationColorSpace.ColorSpaceName);
		}
	}

	return false;
}

void FOpenColorIOColorConversionSettings::ValidateColorSpaces()
{
	if (ConfigurationSource)
	{
		if (!ConfigurationSource->HasDesiredColorSpace(SourceColorSpace))
		{
			SourceColorSpace.Reset();
		}
		if (!ConfigurationSource->HasDesiredColorSpace(DestinationColorSpace))
		{
			DestinationColorSpace.Reset();
		}
		if (!ConfigurationSource->HasDesiredDisplayView(DestinationDisplayView))
		{
			DestinationDisplayView.Reset();
		}
	}
	else
	{
		SourceColorSpace.Reset();
		DestinationColorSpace.Reset();
		DestinationDisplayView.Reset();
	}
}

bool FOpenColorIOColorConversionSettings::IsDisplayView() const
{
	return DestinationDisplayView.IsValid();
}
