// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "Styling/SlateWidgetStyle.h"
#include "AudioMeterStyle.generated.h"

/**
 * Represents the appearance of an SAudioMeter
 */
USTRUCT(BlueprintType)
struct AUDIOWIDGETS_API FAudioMeterStyle : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FAudioMeterStyle();

	virtual ~FAudioMeterStyle() {}

	virtual void GetResources( TArray< const FSlateBrush* >& OutBrushes ) const override;

	static const FName TypeName;
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FAudioMeterStyle& GetDefault();

	/** Image to use to represent the meter value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FSlateBrush MeterValueImage;
	FAudioMeterStyle& SetMeterValueImage(const FSlateBrush& InMeterValueImage){ MeterValueImage = InMeterValueImage; return *this; }

	/** Image to use to represent the background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundImage;
	FAudioMeterStyle& SetBackgroundImage(const FSlateBrush& InBackgroundImage) { BackgroundImage = InBackgroundImage; return *this; }

	/** Image to use to represent the meter background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush MeterBackgroundImage;
	FAudioMeterStyle& SetMeterBackgroundImage(const FSlateBrush& InMeterBackgroundImage) { MeterBackgroundImage = InMeterBackgroundImage; return *this; }

	/** Image to use to draw behind the meter value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush MeterValueBackgroundImage;
	FAudioMeterStyle& SetMeterValueBackgroundImage(const FSlateBrush& InMeterValueBackgroundImage) { MeterValueBackgroundImage = InMeterValueBackgroundImage; return *this; }

	/** Image to use to represent the meter peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush MeterPeakImage;
	FAudioMeterStyle& SetMeterPeakImage(const FSlateBrush& InMeterPeakImage) { MeterPeakImage = InMeterPeakImage; return *this; }

	// How thick to draw the meter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	FVector2D MeterSize;
	FAudioMeterStyle& SetMeterSize(const FVector2D& InMeterSize) { MeterSize = InMeterSize; return *this; }

	// How much padding to add around the meter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D MeterPadding;
	FAudioMeterStyle& SetMeterPadding(const FVector2D& InMeterPadding) { MeterPadding = InMeterPadding; return *this; }

	// How much padding to add around the meter value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float MeterValuePadding;
	FAudioMeterStyle& SetMeterValuePadding(float InMeterValuePadding) { MeterValuePadding = InMeterValuePadding; return *this; }

	// How wide to draw the peak value indicator
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float PeakValueWidth;
	FAudioMeterStyle& SetPeakValueWidth(float InPeakValueWidth) { PeakValueWidth = InPeakValueWidth; return *this; }

	// The minimum and maximum value to display in dB (values are clamped in this range)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FVector2D ValueRangeDb;
	FAudioMeterStyle& SetValueRangeDb(const FVector2D& InValueRangeDb) { ValueRangeDb = InValueRangeDb; return *this; }

	// Whether or not to show the decibel scale alongside the meter
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bShowScale;
	FAudioMeterStyle& SetShowScale(bool bInShowScale) { bShowScale = bInShowScale; return *this; }

	// Which side to show the scale. If vertical, true means left side, false means right side. If horizontal, true means above, false means below.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	bool bScaleSide;
	FAudioMeterStyle& SetScaleSide(bool bInScaleSide) { bScaleSide = bInScaleSide; return *this; }

	// Offset for the hashes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashOffset;
	FAudioMeterStyle& SetScaleHashOffset(float InScaleHashOffset) { ScaleHashOffset = InScaleHashOffset; return *this; }

	// The width of each hash mark
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashWidth;
	FAudioMeterStyle& SetScaleHashWidth(float InScaleHashWidth) { ScaleHashWidth = InScaleHashWidth; return *this; }

	// The height of each hash mark
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ScaleHashHeight;
	FAudioMeterStyle& SetScaleHashHeight(float InScaleHashHeight) { ScaleHashHeight = InScaleHashHeight; return *this; }

	// How wide to draw the decibel scale, if it's enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance, meta = (UIMin = "3", ClampMin="3", UIMax = "10"))
	int32 DecibelsPerHash;
	FAudioMeterStyle& SetDecibelsPerHash(float InDecibelsPerHash) { DecibelsPerHash = InDecibelsPerHash; return *this; }

	/** Font family and size to be used when displaying the meter scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateFontInfo Font;
	FAudioMeterStyle& SetFont(const FSlateFontInfo& InFont) { Font = InFont; return *this; }
	FAudioMeterStyle& SetFont(TSharedPtr<const FCompositeFont> InCompositeFont, const int32 InSize, const FName& InTypefaceFontName = NAME_None) { Font = FSlateFontInfo(InCompositeFont, InSize, InTypefaceFontName); return *this; }
	FAudioMeterStyle& SetFont(const UObject* InFontObject, const int32 InSize, const FName& InTypefaceFontName = NAME_None) { Font = FSlateFontInfo(InFontObject, InSize, InTypefaceFontName); return *this; }
	FAudioMeterStyle& SetFont(const FName& InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMeterStyle& SetFont(const FString& InFontName, uint16 InSize) { Font = FSlateFontInfo(*InFontName, InSize); return *this; }
	FAudioMeterStyle& SetFont(const WIDECHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMeterStyle& SetFont(const ANSICHAR* InFontName, uint16 InSize) { Font = FSlateFontInfo(InFontName, InSize); return *this; }
	FAudioMeterStyle& SetFontName(const FName& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMeterStyle& SetFontName(const FString& InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMeterStyle& SetFontName(const WIDECHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMeterStyle& SetFontName(const ANSICHAR* InFontName) { Font = FSlateFontInfo(InFontName, Font.Size); return *this; }
	FAudioMeterStyle& SetFontSize(uint16 InSize) { Font.Size = InSize; return *this; }
	FAudioMeterStyle& SetTypefaceFontName(const FName& InTypefaceFontName) { Font.TypefaceFontName = InTypefaceFontName; return *this; }

	/**
	 * Unlinks all colors in this style.
	 * @see FSlateColor::Unlink
	 */
	void UnlinkColors()
	{
		MeterValueImage.UnlinkColors();
		MeterBackgroundImage.UnlinkColors();
//		MeterScaleImage.UnlinkColors();
		MeterPeakImage.UnlinkColors();
	}
};


