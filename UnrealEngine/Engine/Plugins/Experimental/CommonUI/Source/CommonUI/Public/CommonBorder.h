// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUITypes.h"
#include "Components/Border.h"
#include "CommonBorder.generated.h"

/* 
 * ---- All properties must be EditDefaultsOnly, BlueprintReadOnly !!! -----
 * We return the CDO to blueprints, so we cannot allow any changes (blueprint doesn't support const variables)
 */
UCLASS(Abstract, Blueprintable, ClassGroup = UI, meta = (Category = "Common UI"))
class COMMONUI_API UCommonBorderStyle : public UObject
{
	GENERATED_BODY()

public:
	UCommonBorderStyle();
	
	/** The brush for the background of the border */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Properties")
	FSlateBrush Background;

	UFUNCTION(BlueprintCallable, Category = "Common Border Style|Getters")
	void GetBackgroundBrush(FSlateBrush& Brush) const;
};

/**
 * Uses the border style template defined in CommonUI project settings by default
 */
UCLASS(Config = CommonUI, DefaultConfig, ClassGroup = UI, meta = (Category = "Common UI", DisplayName = "Common Border"))
class COMMONUI_API UCommonBorder : public UBorder
{
	GENERATED_UCLASS_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "Common Border")
	void SetStyle(TSubclassOf<UCommonBorderStyle> InStyle);

	/** References the border style to use */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common Border", meta = (ExposeOnSpawn = true))
	TSubclassOf<UCommonBorderStyle> Style;

	/** Turning this on will cause the safe zone size to be removed from this borders content padding down to the minimum specified */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common Border")
	bool bReducePaddingBySafezone;

	/** The minimum padding we will reduce to when the safezone grows */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common Border", meta = (EditCondition = "bReducePaddingBySafezone"))
	FMargin MinimumPadding;

#if WITH_EDITORONLY_DATA
	/** Used to track widgets that were created before changing the default style pointer to null */
	UPROPERTY()
	bool bStyleNoLongerNeedsConversion;
#endif

protected:
	virtual void PostLoad() override;

	// UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;
	// End of UWidget interface

	void SafeAreaUpdated();
	void DebugSafeAreaUpdated(const FMargin& NewSafeZone, bool bShouldRecacheMetrics) { SafeAreaUpdated(); };
#if WITH_EDITOR
	virtual void OnCreationFromPalette() override;
	const FText GetPaletteCategory() override;
	virtual void OnDesignerChanged(const FDesignerChangedEventArgs& EventArgs) override;
#endif // WITH_EDITOR

protected:
#if WITH_EDITOR
	/** The editor-only size constraint passed in by UMG Designer*/
	TOptional<FVector2D> DesignerSize;
#endif

private:
	const UCommonBorderStyle* GetStyleCDO() const;

};