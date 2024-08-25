// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/TextBlock.h"
#include "Widgets/Accessibility/SlateWidgetAccessibleTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "CommonTextBlock.generated.h"

struct FTextBlockStyle;
struct FTextScrollerOptions;

class STextScroller;

/* 
 * ---- All properties must be EditDefaultsOnly, BlueprintReadOnly !!! -----
 * We return the CDO to blueprints, so we cannot allow any changes (blueprint doesn't support const variables)
 */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonTextStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonTextStyle();

	/** The font to apply at each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Font")
	FSlateFontInfo Font;

	/** The color of the text */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Color")
	FLinearColor Color;

	/** Whether or not the style uses a drop shadow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow")
	bool bUsesDropShadow;

	/** The offset of the drop shadow at each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow", meta = (EditCondition = "bUsesDropShadow"))
	FVector2D ShadowOffset;

	/** The drop shadow color */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shadow", meta = (EditCondition = "bUsesDropShadow"))
	FLinearColor ShadowColor;

	/** The amount of blank space left around the edges of text area at each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	FMargin Margin;

	/** The brush used to draw an strike through the text (if any) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	FSlateBrush StrikeBrush;

	/** The amount to scale each lines height by at each size */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	float LineHeightPercentage;

	/** Whether to leave extra space below the last line due to line height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	bool ApplyLineHeightToBottomLine;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetFont(FSlateFontInfo& OutFont) const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetColor(FLinearColor& OutColor) const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetMargin(FMargin& OutMargin) const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	float GetLineHeightPercentage() const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	bool GetApplyLineHeightToBottomLine() const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetShadowOffset(FVector2D& OutShadowOffset) const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetShadowColor(FLinearColor& OutColor) const;

	UFUNCTION(BlueprintCallable, Category = "Common Text Style|Getters")
	void GetStrikeBrush(FSlateBrush& OutStrikeBrush) const;

	void ToTextBlockStyle(FTextBlockStyle& OutTextBlockStyle) const;

	void ApplyToTextBlock(const TSharedRef<STextBlock>& TextBlock) const;
};

/* 
 * ---- All properties must be EditDefaultsOnly, BlueprintReadOnly !!! -----
 * We return the CDO to blueprints, so we cannot allow any changes (blueprint doesn't support const variables)
 */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonTextScrollStyle : public UObject
{
	GENERATED_BODY()

public:
	FTextScrollerOptions ToScrollOptions() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	float Speed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	float StartDelay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	float EndDelay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	float FadeInDelay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	float FadeOutDelay;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Properties")
	EWidgetClipping Clipping = EWidgetClipping::OnDemand;
};

/**
 * Text block with automatic scrolling for FX / large texts, also supports a larger set of default styling, & custom mobile scaling.
 */
UCLASS(Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Text", PrioritizeCategories = "Content"))
class COMMONUI_API UCommonTextBlock : public UTextBlock
{
	GENERATED_UCLASS_BODY()
public:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetWrapTextWidth(int32 InWrapTextAt);

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetTextCase(bool bUseAllCaps);

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetLineHeightPercentage(float InLineHeightPercentage);

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetApplyLineHeightToBottomLine(bool InApplyLineHeightToBottomLine);

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetStyle(TSubclassOf<UCommonTextStyle> InStyle);

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	const FMargin& GetMargin();

	UFUNCTION(BlueprintCallable, Category = "Common Text")
	void SetMargin(const FMargin& InMargin);

	UFUNCTION(BlueprintCallable, Category = "Common Text|Mobile")
	float GetMobileFontSizeMultiplier() const;
	
	/** Sets the new value and then applies the FontSizeMultiplier */
	UFUNCTION(BlueprintCallable, Category = "Common Text|Mobile")
	void SetMobileFontSizeMultiplier(float InMobileFontSizeMultiplier);

	UFUNCTION(BlueprintCallable, Category = "Common Text|Scroll Style")
	void ResetScrollState();

	UFUNCTION(BlueprintCallable, Category = "Common Text|Scroll Style")
	void SetScrollingEnabled(bool bInIsScrollingEnabled);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;

	virtual void OnTextChanged() override;
	virtual void OnFontChanged() override;

	/** Mobile font size multiplier. Activated by default on mobile. See CVar Mobile_PreviewFontSize */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Mobile", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float MobileFontSizeMultiplier = 1.0f;
	
#if WITH_EDITOR
	virtual void OnCreationFromPalette() override;
	const FText GetPaletteCategory() override;
	virtual bool CanEditChange(const FProperty* InProperty) const;
#endif // WITH_EDITOR

private:
	/** References the text style to use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonText, meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextStyle> Style;

	/** References the scroll style asset to use, no reference disables scrolling*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonText, meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	TSubclassOf<UCommonTextScrollStyle> ScrollStyle;

	/** If scrolling is enabled/disabled initially, this can be updated in blueprint */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonText, meta = (ExposeOnSpawn = true, AllowPrivateAccess = true))
	bool bIsScrollingEnabled = true;

	/** True to always display text in ALL CAPS */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "bDisplayAllCaps is deprecated. Please use TextTransformPolicy instead."))
	bool bDisplayAllCaps_DEPRECATED = false;

	/** True to automatically collapse this text block when set to display an empty string. Conversely, will be SelfHitTestInvisible when showing a non-empty string. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = CommonText, meta = (AllowPrivateAccess = true))
	bool bAutoCollapseWithEmptyText = false;

#if WITH_EDITORONLY_DATA
	/** Used to track widgets that were created before changing the default style pointer to null */
	UPROPERTY()
	bool bStyleNoLongerNeedsConversion;
#endif

	void UpdateFromStyle();
	const UCommonTextStyle* GetStyleCDO() const;
	const UCommonTextScrollStyle* GetScrollStyleCDO() const;

	TSharedPtr<class STextScroller> TextScroller;

	void ApplyFontSizeMultiplier() const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CommonUITypes.h"
#include "Widgets/Layout/SScrollBox.h"
#endif
