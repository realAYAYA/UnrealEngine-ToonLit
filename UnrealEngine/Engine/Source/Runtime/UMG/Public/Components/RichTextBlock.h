// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SWidget.h"
#include "Components/TextWidgetTypes.h"
#include "Engine/DataTable.h"
#include "RichTextBlock.generated.h"

class SRichTextBlock;
class UDataTable;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class URichTextBlockDecorator;

/** Simple struct for rich text styles */
USTRUCT()
struct FRichTextStyleRow : public FTableRowBase
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Appearance)
	FTextBlockStyle TextStyle;
};

/**
 * The rich text block
 *
 * * Fancy Text
 * * No Children
 */
UCLASS()
class UMG_API URichTextBlock : public UTextLayoutWidget
{
	GENERATED_BODY()

public:
	/**  
	 * Sets the color and opacity of the default text in this rich text block
	 * @param InColorAndOpacity		The new text color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetDefaultColorAndOpacity(FSlateColor InColorAndOpacity);

	/**  
	 * Sets the color and opacity of the default text drop shadow
	 * Note: if opacity is zero no shadow will be drawn
	 * @param InShadowColorAndOpacity		The new drop shadow color and opacity
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetDefaultShadowColorAndOpacity(FLinearColor InShadowColorAndOpacity);

	/**  
	 * Sets the offset that the default text drop shadow should be drawn at
	 * @param InShadowOffset		The new offset
	 */
	UFUNCTION(BlueprintCallable, Category="Appearance")
	void SetDefaultShadowOffset(FVector2D InShadowOffset);

	/**
	 * Dynamically set the default font info for this rich text block
	 * @param InFontInfo The new font info
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetDefaultFont(FSlateFontInfo InFontInfo);

	/**
	 * Dynamically set the default strike brush for this rich text block
	 * @param InStrikeBrush The new brush to use to strike through text
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetDefaultStrikeBrush(FSlateBrush& InStrikeBrush);

	/**
	 *  Set the minimum desired width for this rich text block
	 *  @param InMinDesiredWidth new minimum desired width
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetMinDesiredWidth(float InMinDesiredWidth);
	
	/**
	 * Set the auto wrap for this rich text block
	 * @param InAutoTextWrap to turn wrap on or off
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetAutoWrapText(bool InAutoTextWrap);

	/**
	 * Set the text transformation policy for this text block.
	 * @param InTransformPolicy the new text transformation policy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextTransformPolicy(ETextTransformPolicy InTransformPolicy);

	/**
	* Set the text overflow policy for this text block.
	* @param InOverflowPolicy the new text overflow policy.
	*/
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetTextOverflowPolicy(ETextOverflowPolicy InOverflowPolicy);

	/** 
	 * Wholesale override of the currently established default text style
	 * @param InDefaultTextStyle The new text style to apply to all default (i.e. undecorated) text in the block
	 */
	UFUNCTION(BlueprintCallable, Category = Appearance)
	void SetDefaultTextStyle(const FTextBlockStyle& InDefaultTextStyle);

	UFUNCTION(BlueprintCallable, Category = Appearance)
	void SetDefaultMaterial(UMaterialInterface* InMaterial);

	/** Remove all overrides made to the default text style and return to the style specified in the style set data table */
	UFUNCTION()
	void ClearAllDefaultStyleOverrides();

	/**
	 * Creates a dynamic material for the default font or returns it if it already
	 * exists
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	UMaterialInstanceDynamic* GetDefaultDynamicMaterial();

	/**
	 * Replaces the existing decorators with the list provided
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetDecorators(const TArray<TSubclassOf<URichTextBlockDecorator>>& InDecoratorClasses);

public:
	URichTextBlock(const FObjectInitializer& ObjectInitializer);

	//~ Begin UTextLayoutWidget Interface
	virtual void SetJustification(ETextJustify::Type InJustification) override;
	//~ End UTextLayoutWidget Interface

	// UWidget interface
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	// UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	// End of UVisual interface

#if WITH_EDITOR
	// UWidget interface
	virtual const FText GetPaletteCategory() override;
	virtual void OnCreationFromPalette() override;
	virtual void ValidateCompiledDefaults(IWidgetCompilerLog& CompileLog) const override;
	// End UWidget interface
#endif

	/**
	 * Returns widgets text.
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	FText GetText() const;
	
	/**
	 * Directly sets the widget text.
	 * Warning: This will wipe any binding created for the Text property!
	 * @param InText The text to assign to the widget
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	virtual void SetText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "Widget")
	UDataTable* GetTextStyleSet() const;

	UFUNCTION(BlueprintCallable, Category = "Widget")
	void SetTextStyleSet(UDataTable* NewTextStyleSet);

	const FTextBlockStyle& GetDefaultTextStyle() const;
	const FTextBlockStyle& GetCurrentDefaultTextStyle() const;

	/**  */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	URichTextBlockDecorator* GetDecoratorByClass(TSubclassOf<URichTextBlockDecorator> DecoratorClass);

	/**
	 * Causes the text to reflow it's layout and re-evaluate any decorators
	 */
	UFUNCTION(BlueprintCallable, Category = "Widget")
	void RefreshTextLayout();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	
	virtual void UpdateStyleData();
	void RebuildStyleInstance();
	virtual void CreateDecorators(TArray< TSharedRef< class ITextDecorator > >& OutDecorators);
	virtual TSharedPtr< class IRichTextMarkupParser > CreateMarkupParser();
	virtual TSharedPtr< class IRichTextMarkupWriter > CreateMarkupWriter();

	void BeginDefaultStyleOverride();
	virtual void ApplyUpdatedDefaultTextStyle();

protected:
	/** The text to display */
	UPROPERTY(EditAnywhere, Category=Content, meta=( MultiLine="true" ))
	FText Text;

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance, meta=(RequiredAssetDataTags = "RowStructure=/Script/UMG.RichTextStyleRow"))
	TObjectPtr<UDataTable> TextStyleSet;

	/**  */
	UPROPERTY(EditAnywhere, Category=Appearance)
	TArray<TSubclassOf<URichTextBlockDecorator>> DecoratorClasses;

	/** True to specify the default text style for this rich text inline, overriding any default provided in the style set table */
	UPROPERTY(EditAnywhere, Category = Appearance)
	bool bOverrideDefaultStyle = false;

	/** Text style to apply by default to text in this block */
	UPROPERTY(EditAnywhere, Category=Appearance, meta = (EditCondition = bOverrideDefaultStyle))
	FTextBlockStyle DefaultTextStyleOverride;

	/** The minimum desired size for the text */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance)
	float MinDesiredWidth;

	/** The text transformation policy to apply to this text block */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Appearance, meta=(DisplayName="Transform Policy"))
	ETextTransformPolicy TextTransformPolicy;

	/** Sets what happens to text that is clipped and doesn't fit within the clip rect for this widget */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Clipping, AdvancedDisplay, meta = (DisplayName = "Overflow Policy"))
	ETextOverflowPolicy TextOverflowPolicy;

	UPROPERTY(Transient)
	FTextBlockStyle DefaultTextStyle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URichTextBlockDecorator>> InstanceDecorators;
	TSharedPtr<class FSlateStyleSet> StyleInstance;
	TSharedPtr<SRichTextBlock> MyRichTextBlock;
};
