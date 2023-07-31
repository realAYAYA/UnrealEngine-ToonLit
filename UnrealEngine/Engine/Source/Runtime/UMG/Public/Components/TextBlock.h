// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "TextBlock.generated.h"

class STextBlock;
class UMaterialInterface;

/**
 * A simple static text widget.
 *
 * * No Children
 * * Text
 */
UCLASS(meta=(DisplayName="Text"))
class UMG_API UTextBlock : public UTextLayoutWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** The text to display */
	UE_DEPRECATED(5.1, "Direct access to Text is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintGetter="GetText", BlueprintSetter="SetText", Category="Content", meta = (MultiLine = "true"))
	FText Text;
	
	/** A bindable delegate to allow logic to drive the text of the widget */
	UPROPERTY()
	FGetText TextDelegate;

	/** The color of the text */
	UE_DEPRECATED(5.1, "Direct access to ColorAndOpacity is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetColorAndOpacity", Category="Appearance")
	FSlateColor ColorAndOpacity;

	/** A bindable delegate for the ColorAndOpacity. */
	UPROPERTY()
	FGetSlateColor ColorAndOpacityDelegate;
	
	/** The font to render the text with */
	UE_DEPRECATED(5.1, "Direct access to Font is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetFont", Category="Appearance")
	FSlateFontInfo Font;

	/** The brush to strike through text with */
	UE_DEPRECATED(5.1, "Direct access to StrikeBrush is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetStrikeBrush", Category="Appearance")
	FSlateBrush StrikeBrush;

	/** The direction the shadow is cast */
	UE_DEPRECATED(5.1, "Direct access to ShadowOffset is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetShadowOffset", Category="Appearance")
	FVector2D ShadowOffset;

	/** The color of the shadow */
	UE_DEPRECATED(5.1, "Direct access to ShadowColorAndOpacity is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetShadowColorAndOpacity", Category="Appearance", meta=( DisplayName="Shadow Color" ))
	FLinearColor ShadowColorAndOpacity;

	/** A bindable delegate for the ShadowColorAndOpacity. */
	UPROPERTY()
	FGetLinearColor ShadowColorAndOpacityDelegate;

	/** The minimum desired size for the text */
	UE_DEPRECATED(5.1, "Direct access to MinDesiredWidth is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetMinDesiredWidth", Category="Appearance")
	float MinDesiredWidth;

	/** If true, it will automatically wrap this text widget with an invalidation panel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Performance", AdvancedDisplay)
	bool bWrapWithInvalidationPanel;

	/** The text transformation policy to apply to this text block. */
	UE_DEPRECATED(5.1, "Direct access to TextTransformPolicy is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetTextTransformPolicy", Category="Appearance", meta=(DisplayName="Transform Policy"))
	ETextTransformPolicy TextTransformPolicy;

	/** Sets what happens to text that is clipped and doesn't fit within the clip rect for this widget */
	UE_DEPRECATED(5.1, "Direct access to TextOverflowPolicy is deprecated. Please use the getter or setter.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, BlueprintSetter="SetTextOverflowPolicy", Category="Clipping", AdvancedDisplay, meta = (DisplayName="Overflow Policy"))
	ETextOverflowPolicy TextOverflowPolicy;

public:
	/** 
	 * Gets the widget text
	 * @return The widget text
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="GetText (Text)"))
	FText GetText() const;

	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category="Widget", meta=(DisplayName="SetText (Text)"))
	virtual void SetText(FText InText);

	/** @return the color and opacity of the text in this text block. */
	FSlateColor GetColorAndOpacity() const;

	/**  
	 * Sets the color and opacity of the text in this text block
	 *
	 * @param InColorAndOpacity		The new text color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetColorAndOpacity(FSlateColor InColorAndOpacity);

	/**
	* Sets the opacity of the text in this text block
	*
	* @param InOpacity		The new text opacity
	*/
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetOpacity(float InOpacity);

	/** @return the color and opacity of the text drop shadow. */
	FLinearColor GetShadowColorAndOpacity() const;

	/**  
	 * Sets the color and opacity of the text drop shadow
	 * Note: if opacity is zero no shadow will be drawn
	 *
	 * @param InShadowColorAndOpacity		The new drop shadow color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity);

	/** @return the offset that the text drop shadow should be drawn at. */
	FVector2D GetShadowOffset() const;

	/**  
	 * Sets the offset that the text drop shadow should be drawn at
	 *
	 * @param InShadowOffset		The new offset
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetShadowOffset(FVector2D InShadowOffset);

	/** @return the font info for this text block. */
	const FSlateFontInfo& GetFont() const;

	/**
	 * Dynamically set the font info for this text block
	 * 
	 * @param InFontInfo The new font info
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetFont(FSlateFontInfo InFontInfo);

	/** @return the brush to use to strike through text.  */
	const FSlateBrush& GetStrikeBrush() const;

	/**
	 * Dynamically set the strike brush for this text block
	 *
	 * @param InStrikeBrush The new brush to use to strike through text
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetStrikeBrush(FSlateBrush InStrikeBrush);

	/** @return the minimum desired width for this text block */
	float GetMinDesiredWidth() const;

	/**
	 *  Set the minimum desired width for this text block
	 *
	 *  @param InMinDesiredWidth new minimum desired width
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetMinDesiredWidth(float InMinDesiredWidth);
	
	/**
	 * Set the auto wrap for this text block.
	 *
	 * @param InAutoTextWrap to turn wrap on or off.
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetAutoWrapText(bool InAutoTextWrap);

	/** @return the text transformation policy for this text block. */
	ETextTransformPolicy GetTextTransformPolicy() const;

	/**
	 * Set the text transformation policy for this text block.
	 *
	 * @param InTransformPolicy the new text transformation policy.
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetTextTransformPolicy(ETextTransformPolicy InTransformPolicy);

	/** @return the text overflow policy for this text block. */
	ETextOverflowPolicy GetTextOverflowPolicy() const;

	/**
	 * Set the text overflow policy for this text block.
	 *
	 * @param InOverflowPolicy the new text overflow policy.
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetTextOverflowPolicy(ETextOverflowPolicy InOverflowPolicy);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetFontMaterial(UMaterialInterface* InMaterial);

	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetFontOutlineMaterial(UMaterialInterface* InMaterial);

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicFontMaterial();

	/**  */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	UMaterialInstanceDynamic* GetDynamicOutlineMaterial();

public:
	//~ Begin UTextLayoutWidget Interface
	virtual void SetJustification(ETextJustify::Type InJustification) override;
	//~ End UTextLayoutWidget Interface

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
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	//~ End UWidget Interface

	virtual FString GetLabelMetadata() const override;

	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitteType);
#endif

protected:
	/**
	 * If this is enabled, text shaping, wrapping, justification are disabled in favor of much faster text layout and measurement.
	 * This feature is only suitable for "simple" text (ie, text containing only numbers or basic ASCII) as it disables the complex text rendering support required for certain languages (such as Arabic and Thai).
	 * It is significantly faster for text that can take advantage of it (particularly if that text changes frequently), but shouldn't be used for localized user-facing text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Performance, AdvancedDisplay, meta=(AllowPrivateAccess, DesignerRebuild))
	bool bSimpleTextMode;

	//~ Begin UWidget Interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnBindingChanged(const FName& Property) override;
	//~ End UWidget Interface

	/** Get the text that should be displayed in the internal Slate widget (allows flags to mutate the display text without modifying the persistent designer property data) */
	virtual TAttribute<FText> GetDisplayText();

	EVisibility GetTextWarningImageVisibility() const;

#if WITH_ACCESSIBILITY
	virtual TSharedPtr<SWidget> GetAccessibleWidget() const override;
#endif

protected:

	TSharedPtr<STextBlock> MyTextBlock;
	
	PROPERTY_BINDING_IMPLEMENTATION(FText, Text);
	PROPERTY_BINDING_IMPLEMENTATION(FSlateColor, ColorAndOpacity);
	PROPERTY_BINDING_IMPLEMENTATION(FLinearColor, ShadowColorAndOpacity);
};
