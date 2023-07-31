// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Components/Widget.h"
#include "ProgressBar.generated.h"

class USlateBrushAsset;

/**
 * The progress bar widget is a simple bar that fills up that can be restyled to fit any number of uses.
 *
 * * No Children
 */
UCLASS()
class UMG_API UProgressBar : public UWidget
{
	GENERATED_UCLASS_BODY()
	
public:
	/** The progress bar style */
	UE_DEPRECATED(5.1, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Style" ))
	FProgressBarStyle WidgetStyle;

	/** Used to determine the fill position of the progress bar ranging 0..1 */
	UE_DEPRECATED(5.1, "Direct access to Percent is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Setter, BlueprintSetter="SetPercent", Category="Progress", meta = (UIMin = "0", UIMax = "1"))
	float Percent;

	/** Defines the direction in which the progress bar fills */
	UE_DEPRECATED(5.1, "Direct access to BarFillType is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	TEnumAsByte<EProgressBarFillType::Type> BarFillType;

	/** Defines the visual style of the progress bar fill - scale or mask */
	UE_DEPRECATED(5.1, "Direct access to BarFillStyle is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	TEnumAsByte<EProgressBarFillStyle::Type> BarFillStyle;
	
	/** */
	UE_DEPRECATED(5.1, "Direct access to bIsMarquee is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter="UseMarquee", Setter="SetIsMarquee", BlueprintSetter="SetIsMarquee", Category="Progress")
	bool bIsMarquee;

	/** */
	UE_DEPRECATED(5.1, "Direct access to BorderPadding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	FVector2D BorderPadding;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetFloat PercentDelegate;

	/** Fill Color and Opacity */
	UE_DEPRECATED(5.1, "Direct access to FillColorAndOpacity is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Setter, BlueprintSetter="SetFillColorAndOpacity", Category="Appearance")
	FLinearColor FillColorAndOpacity;

	/** */
	UPROPERTY()
	FGetLinearColor FillColorAndOpacityDelegate;

public:
	
	/** */
	const FProgressBarStyle& GetWidgetStyle() const;

	/**/
	void SetWidgetStyle(const FProgressBarStyle& InStyle);

	/** */
	float GetPercent() const;

	/** Sets the current value of the ProgressBar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetPercent(float InPercent);

	/** */
	EProgressBarFillType::Type GetBarFillType() const;

	/** */
	void SetBarFillType(EProgressBarFillType::Type InBarFillType);

	/** */
	EProgressBarFillStyle::Type GetBarFillStyle() const;

	/** */
	void SetBarFillStyle(EProgressBarFillStyle::Type InBarFillStyle);

	/** */
	bool UseMarquee() const;

	/** Sets the progress bar to show as a marquee. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetIsMarquee(bool InbIsMarquee);

	/** */
	FVector2D GetBorderPadding() const;

	/** */
	void SetBorderPadding(FVector2D InBorderPadding);

	/** */
	FLinearColor GetFillColorAndOpacity() const;

	/** Sets the fill color of the progress bar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	void SetFillColorAndOpacity(FLinearColor InColor);

public:
	
	//~ Begin UWidget Interface
	virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	//~ End UWidget Interface
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SProgressBar> MyProgressBar;

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, FillColorAndOpacity);
};
