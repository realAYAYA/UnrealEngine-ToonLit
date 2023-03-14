// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ContentWidget.h"
#include "BackgroundBlur.generated.h"

/**
 * A background blur is a container widget that can contain one child widget, providing an opportunity 
 * to surround it with adjustable padding and apply a post-process Gaussian blur to all content beneath the widget.
 *
 * * Single Child
 * * Blur Effect
 */
UCLASS()
class UMG_API UBackgroundBlur : public UContentWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The padding area between the slot and the content it contains. */
	UE_DEPRECATED(5.1, "Direct access to Padding is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetPadding, Category = Content)
	FMargin Padding;

	/** The alignment of the content horizontally. */
	UE_DEPRECATED(5.1, "Direct access to HorizontalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetHorizontalAlignment, Category = Content)
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment;

	/** The alignment of the content vertically. */
	UE_DEPRECATED(5.1, "Direct access to VerticalAlignment is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetVerticalAlignment, Category = Content)
	TEnumAsByte<EVerticalAlignment> VerticalAlignment;

	/** True to modulate the strength of the blur based on the widget alpha. */
	UE_DEPRECATED(5.1, "Direct access to bApplyAlphaToBlur is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter=GetApplyAlphaToBlur, Setter=SetApplyAlphaToBlur, BlueprintSetter=SetApplyAlphaToBlur, Category = Content)
	bool bApplyAlphaToBlur;

	/**
	 * How blurry the background is.  Larger numbers mean more blurry but will result in larger runtime cost on the GPU.
	 */
	UE_DEPRECATED(5.1, "Direct access to BlurStrength is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetBlurStrength, Category = Appearance, meta = (ClampMin = 0, ClampMax = 100))
	float BlurStrength;

	/** When OverrideAutoRadiusCalculation is set to true, BlurRadius is used for the radius of the blur. When false, it's automatically calculated using the BlurStength value. */
	UE_DEPRECATED(5.1, "Direct access to bOverrideAutoRadiusCalculation is deprecated. Please use the getter or setter.")
	UPROPERTY(Getter = GetOverrideAutoRadiusCalculation, Setter = SetOverrideAutoRadiusCalculation)
	bool bOverrideAutoRadiusCalculation;

	/**
	 * This is the number of pixels which will be weighted in each direction from any given pixel when computing the blur
	 * A larger value is more costly but allows for stronger blurs.
	 */
	UE_DEPRECATED(5.1, "Direct access to BlurRadius is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetBlurRadius, Category = Appearance, meta = (ClampMin = 0, ClampMax = 255, EditCondition = "bOverrideAutoRadiusCalculation"))
	int32 BlurRadius;

	/**
	 * This is the number of pixels which will be weighted in each direction from any given pixel when computing the blur
	 * A larger value is more costly but allows for stronger blurs.  
	 */
	UE_DEPRECATED(5.1, "Direct access to CornerRadius is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetCornerRadius, Category = Appearance)
	FVector4 CornerRadius;

	/**
	 * An image to draw instead of applying a blur when low quality override mode is enabled. 
	 * You can enable low quality mode for background blurs by setting the cvar Slate.ForceBackgroundBlurLowQualityOverride to 1. 
	 * This is usually done in the project's scalability settings
	 */
	UE_DEPRECATED(5.1, "Direct access to LowQualityFallbackBrush is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter=SetLowQualityFallbackBrush, Category = Appearance)
	FSlateBrush LowQualityFallbackBrush;

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual const FText GetPaletteCategory() override;
#endif

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetPadding(FMargin InPadding);

	FMargin GetPadding() const;

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	EHorizontalAlignment GetHorizontalAlignment() const;

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

	EVerticalAlignment GetVerticalAlignment() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetApplyAlphaToBlur(bool bInApplyAlphaToBlur);

	bool GetApplyAlphaToBlur() const;

	void SetOverrideAutoRadiusCalculation(bool InOverrideAutoRadiusCalculation);

	bool GetOverrideAutoRadiusCalculation() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetBlurRadius(int32 InBlurRadius);

	int32 GetBlurRadius() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	virtual void SetBlurStrength(float InStrength);

	float GetBlurStrength() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	virtual void SetCornerRadius(FVector4 InCornerRadius);
	
	FVector4 GetCornerRadius() const;

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetLowQualityFallbackBrush(const FSlateBrush& InBrush);

	FSlateBrush GetLowQualityFallbackBrush() const;

	/** UObject interface */
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

protected:
	/** UWidget interface */
	virtual UClass* GetSlotClass() const override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

	/** UPanelWidget interface */
	virtual void OnSlotAdded(UPanelSlot* Slot) override;
	virtual void OnSlotRemoved(UPanelSlot* Slot) override;

protected:
	TSharedPtr<class SBackgroundBlur> MyBackgroundBlur;
};
