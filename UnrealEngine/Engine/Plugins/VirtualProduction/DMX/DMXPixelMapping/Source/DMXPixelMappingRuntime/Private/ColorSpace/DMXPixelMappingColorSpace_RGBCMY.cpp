// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorSpace/DMXPixelMappingColorSpace_RGBCMY.h"


UDMXPixelMappingColorSpace_RGBCMY::UDMXPixelMappingColorSpace_RGBCMY()
	: RedAttribute("Red")
	, GreenAttribute("Green")
	, BlueAttribute("Blue")
	, LuminanceAttribute("Dimmer")
{}

void UDMXPixelMappingColorSpace_RGBCMY::SetRGBA(const FLinearColor& InColor)
{
	if (bUseWorkingColorSpaceForInput && !InputColorSpace.Equals(UE::Color::FColorSpace::GetWorking()))
	{
		// Update in case the working color space changed
		UpdateColorSpaceAndTransform();
	}

	const FLinearColor CalibratedColor = ColorSpaceTransform->Apply(InColor);

	if (RedAttribute.IsValid())
	{
		const float Value = bSendCyan ? FMath::Abs(CalibratedColor.R - 1.f) : CalibratedColor.R;
		SetAttributeValue(RedAttribute, Value);
	}

	if (GreenAttribute.IsValid())
	{
		const float Value = bSendMagenta ? FMath::Abs(CalibratedColor.G - 1.f) : CalibratedColor.G;
		SetAttributeValue(GreenAttribute, Value);
	}

	if (BlueAttribute.IsValid())
	{
		const float Value = bSendYellow ? FMath::Abs(CalibratedColor.B - 1.f) : CalibratedColor.B;
		SetAttributeValue(BlueAttribute, Value);
	}

	if (LuminanceAttribute.IsValid())
	{
		if (LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromColor)
		{
			const float LuminanceFromColor = OutputColorSpace.GetLuminance(CalibratedColor);
			SetAttributeValue(LuminanceAttribute, FMath::Clamp(LuminanceFromColor, MinLuminance, MaxLuminance));
		}
		else if (LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::Constant)
		{
			SetAttributeValue(LuminanceAttribute, FMath::Clamp(Luminance, MinLuminance, MaxLuminance));
		}
		else if (LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromAlpha)
		{
			SetAttributeValue(LuminanceAttribute, FMath::Clamp(InColor.A, MinLuminance, MaxLuminance));
		}
	}
}

void UDMXPixelMappingColorSpace_RGBCMY::PostLoad()
{
	Super::PostLoad();

	UpdateColorSpaceAndTransform();
}

#if WITH_EDITOR
void UDMXPixelMappingColorSpace_RGBCMY::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FDMXAttributeName, Name) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace_RGBCMY, LuminanceType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace_RGBCMY, LuminanceAttribute))
	{
		ClearCachedAttributeValues();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace_RGBCMY, PixelMappingOutputColorSpace) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace, bUseWorkingColorSpaceForInput))
	{
		UpdateColorSpaceAndTransform();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace_RGBCMY, MinLuminance))
	{
		if (MaxLuminance < MinLuminance)
		{
			Modify();
			MaxLuminance = MinLuminance;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDMXPixelMappingColorSpace_RGBCMY, MaxLuminance))
	{
		if (MinLuminance > MaxLuminance)
		{
			Modify();
			MinLuminance = MaxLuminance;
		}
	}
}
#endif // WITH_EDITOR

void UDMXPixelMappingColorSpace_RGBCMY::UpdateColorSpaceAndTransform()
{
	using namespace UE::Color;

	InputColorSpace = bUseWorkingColorSpaceForInput ? FColorSpace::GetWorking() : FColorSpace(EColorSpace::sRGB);

	const EColorSpace SelectedOutputColorSpace = ConvertToOutputColorSpaceEnum(PixelMappingOutputColorSpace);
	OutputColorSpace = FColorSpace(SelectedOutputColorSpace);

	ColorSpaceTransform = MakeUnique<FColorSpaceTransform>(InputColorSpace, OutputColorSpace);
}

UE::Color::EColorSpace UDMXPixelMappingColorSpace_RGBCMY::ConvertToOutputColorSpaceEnum(EDMXPixelMappingOutputColorSpace_RGBCMY InPixelMappingOutputColorSpaceEnum) const
{
	using namespace UE::Color;
	if (InPixelMappingOutputColorSpaceEnum == EDMXPixelMappingOutputColorSpace_RGBCMY::sRGB)
	{
		return EColorSpace::sRGB;
	}
	else if (InPixelMappingOutputColorSpaceEnum == EDMXPixelMappingOutputColorSpace_RGBCMY::Plasa)
	{
		return EColorSpace::PLASA_E1_54;
	}
	else if (InPixelMappingOutputColorSpaceEnum == EDMXPixelMappingOutputColorSpace_RGBCMY::Rec2020)
	{
		return EColorSpace::Rec2020;
	}
	else if (InPixelMappingOutputColorSpaceEnum == EDMXPixelMappingOutputColorSpace_RGBCMY::P3DCI)
	{
		return EColorSpace::P3DCI;
	}
	else if (InPixelMappingOutputColorSpaceEnum == EDMXPixelMappingOutputColorSpace_RGBCMY::P3D65)
	{
		return EColorSpace::P3D65;
	}

	return EColorSpace::None;
}
