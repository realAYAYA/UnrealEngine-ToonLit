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
UCLASS(MinimalAPI)
class UProgressBar : public UWidget
{
	GENERATED_UCLASS_BODY()
	
public:
	UE_DEPRECATED(5.1, "Direct access to WidgetStyle is deprecated. Please use the getter or setter.")
	/** The progress bar style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Style", meta=( DisplayName="Style" ))
	FProgressBarStyle WidgetStyle;

	UE_DEPRECATED(5.1, "Direct access to Percent is deprecated. Please use the getter or setter.")
	/** Used to determine the fill position of the progress bar ranging 0..1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Setter, BlueprintSetter="SetPercent", Category="Progress", meta = (UIMin = "0", UIMax = "1"))
	float Percent;

	UE_DEPRECATED(5.1, "Direct access to BarFillType is deprecated. Please use the getter or setter.")
	/** Defines the direction in which the progress bar fills */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	TEnumAsByte<EProgressBarFillType::Type> BarFillType;

	UE_DEPRECATED(5.1, "Direct access to BarFillStyle is deprecated. Please use the getter or setter.")
	/** Defines the visual style of the progress bar fill - scale or mask */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	TEnumAsByte<EProgressBarFillStyle::Type> BarFillStyle;
	
	UE_DEPRECATED(5.1, "Direct access to bIsMarquee is deprecated. Please use the getter or setter.")
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter="UseMarquee", Setter="SetIsMarquee", BlueprintSetter="SetIsMarquee", Category="Progress")
	bool bIsMarquee;

	UE_DEPRECATED(5.1, "Direct access to BorderPadding is deprecated. Please use the getter or setter.")
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category="Progress")
	FVector2D BorderPadding;

	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetFloat PercentDelegate;

	UE_DEPRECATED(5.1, "Direct access to FillColorAndOpacity is deprecated. Please use the getter or setter.")
	/** Fill Color and Opacity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, FieldNotify, Getter, Setter, BlueprintSetter="SetFillColorAndOpacity", Category="Appearance")
	FLinearColor FillColorAndOpacity;

	/** */
	UPROPERTY()
	FGetLinearColor FillColorAndOpacityDelegate;

public:
	
	/** */
	UMG_API const FProgressBarStyle& GetWidgetStyle() const;

	/**/
	UMG_API void SetWidgetStyle(const FProgressBarStyle& InStyle);

	/** */
	UMG_API float GetPercent() const;

	/** Sets the current value of the ProgressBar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	UMG_API void SetPercent(float InPercent);

	/** */
	UMG_API EProgressBarFillType::Type GetBarFillType() const;

	/** */
	UMG_API void SetBarFillType(EProgressBarFillType::Type InBarFillType);

	/** */
	UMG_API EProgressBarFillStyle::Type GetBarFillStyle() const;

	/** */
	UMG_API void SetBarFillStyle(EProgressBarFillStyle::Type InBarFillStyle);

	/** */
	UMG_API bool UseMarquee() const;

	/** Sets the progress bar to show as a marquee. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	UMG_API void SetIsMarquee(bool InbIsMarquee);

	/** */
	UMG_API FVector2D GetBorderPadding() const;

	/** */
	UMG_API void SetBorderPadding(FVector2D InBorderPadding);

	/** */
	UMG_API FLinearColor GetFillColorAndOpacity() const;

	/** Sets the fill color of the progress bar. */
	UFUNCTION(BlueprintCallable, Category="Progress")
	UMG_API void SetFillColorAndOpacity(FLinearColor InColor);

public:
	
	//~ Begin UWidget Interface
	UMG_API virtual void SynchronizeProperties() override;
	//~ End UWidget Interface

	//~ Begin UVisual Interface
	UMG_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UVisual Interface

#if WITH_EDITOR
	//~ Begin UWidget Interface
	UMG_API virtual const FText GetPaletteCategory() override;
	UMG_API virtual void OnCreationFromPalette() override;
	//~ End UWidget Interface
#endif

protected:
	/** Native Slate Widget */
	TSharedPtr<SProgressBar> MyProgressBar;

	//~ Begin UWidget Interface
	UMG_API virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget Interface

	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, FillColorAndOpacity);
};
