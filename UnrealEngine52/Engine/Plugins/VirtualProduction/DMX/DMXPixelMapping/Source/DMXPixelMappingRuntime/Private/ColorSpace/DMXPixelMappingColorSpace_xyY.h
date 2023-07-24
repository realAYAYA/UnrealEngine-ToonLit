// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXAttribute.h"
#include "ColorSpace/DMXPixelMappingColorSpace.h"

#include "ColorSpace.h"

#include "DMXPixelMappingColorSpace_xyY.generated.h"


UCLASS(meta = (DisplayName = "CIE 1931 xyY"))
class DMXPIXELMAPPINGRUNTIME_API UDMXPixelMappingColorSpace_xyY
	: public UDMXPixelMappingColorSpace
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXPixelMappingColorSpace_xyY();

	//~ Begin DMXPixelMappingColorSpace interface
	virtual void SetRGBA(const FLinearColor& InColor) override;
	//~ End DMXPixelMappingColorSpace interface

	/** Attribute sent for x */
	UPROPERTY(EditAnywhere, Category = "XY", Meta = (DisplayName = "x Attribute"))
	FDMXAttributeName XAttribute;

	/** Attribute sent for y */
	UPROPERTY(EditAnywhere, Category = "XY", Meta = (DisplayName = "y Attribute"))
	FDMXAttributeName YAttribute;

	/** Attribute sent for Y */
	UPROPERTY(EditAnywhere, Category = "Luminance")
	FDMXAttributeName LuminanceAttribute;

	/** Min Luminance */
	UPROPERTY(EditAnywhere, Category = "Luminance", Meta = (ClampMin = 0.0, ClampMax = 1.0, SliderMin = 0.0, SliderMax = 1.0, EditConditionHides, EditCondition = "LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromWhite || LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromAlpha"))
	float MinLuminance = 0.f;

	/** Max Luminance */
	UPROPERTY(EditAnywhere, Category = "Luminance", Meta = (ClampMin = 0.0, ClampMax = 1.0, SliderMin = 0.0, SliderMax = 1.0, EditConditionHides, EditCondition = "LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromWhite || LuminanceType == EDMXPixelMappingLuminanceType_RGBCMY::FromAlpha"))
	float MaxLuminance = 1.f;

protected:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

private:
	/** Cached sRGB color space, to avoid instantiating on each conversion */
	UE::Color::FColorSpace SRGBColorSpace;
};
