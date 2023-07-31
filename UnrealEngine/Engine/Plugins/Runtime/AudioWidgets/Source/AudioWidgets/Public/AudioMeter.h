// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/Widget.h"
#include "AudioMeterStyle.h"
#include "AudioMeterTypes.h"
#include "AudioMeter.generated.h"

class SAudioMeter;

/**
 * An audio meter widget.
 *
 * Supports displaying a slower moving peak-hold value as well as the current meter value.
 *
 * A clipping value is also displayed which shows a customizable color to indicate clipping.
 *
 * Internal values are stored and interacted with as linear volume values.
 * 
 */
UCLASS()
class AUDIOWIDGETS_API UAudioMeter: public UWidget
{
	GENERATED_UCLASS_BODY()

	public:

	DECLARE_DYNAMIC_DELEGATE_RetVal(TArray<FMeterChannelInfo>, FGetMeterChannelInfo);

	/** The current meter value to display. */
	UPROPERTY(EditAnywhere, Category = MeterValues)
	TArray<FMeterChannelInfo> MeterChannelInfo;

	/** A bindable delegate to allow logic to drive the value of the meter */
	UPROPERTY()
	FGetMeterChannelInfo MeterChannelInfoDelegate;

public:
	
	/** The audio meter style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Style", meta=( DisplayName="Style" ))
	FAudioMeterStyle WidgetStyle;

	/** The slider's orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	TEnumAsByte<EOrientation> Orientation;

	/** The color to draw the background. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor BackgroundColor;

	/** The color to draw the meter background. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	FLinearColor MeterBackgroundColor;

	/** The color to draw the meter value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor MeterValueColor;

	/** The color to draw the meter peak value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor MeterPeakColor;

	/** The color to draw the meter clipping value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor MeterClippingColor;

	/** The color to draw the meter scale hashes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor MeterScaleColor;

	/** The color to draw the meter scale label. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Appearance)
	FLinearColor MeterScaleLabelColor;

public:

 	/** Gets the current linear value of the meter. */
 	UFUNCTION(BlueprintCallable, Category="Behavior")
	TArray<FMeterChannelInfo> GetMeterChannelInfo() const;
 
 	/** Sets the current meter values. */
 	UFUNCTION(BlueprintCallable, Category="Behavior")
 	void SetMeterChannelInfo(const TArray<FMeterChannelInfo>& InMeterChannelInfo);

	/** Sets the background color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBackgroundColor(FLinearColor InValue);

	/** Sets the meter background color */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetMeterBackgroundColor(FLinearColor InValue);

	/** Sets the meter value color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMeterValueColor(FLinearColor InValue);

	/** Sets the meter peak color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMeterPeakColor(FLinearColor InValue);

	/** Sets the meter clipping color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMeterClippingColor(FLinearColor InValue);

	/** Sets the meter scale color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMeterScaleColor(FLinearColor InValue);

	/** Sets the meter scale color */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMeterScaleLabelColor(FLinearColor InValue);

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SAudioMeter> MyAudioMeter;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	// End of UWidget interface

	PROPERTY_BINDING_IMPLEMENTATION(TArray<FMeterChannelInfo>, MeterChannelInfo);
};