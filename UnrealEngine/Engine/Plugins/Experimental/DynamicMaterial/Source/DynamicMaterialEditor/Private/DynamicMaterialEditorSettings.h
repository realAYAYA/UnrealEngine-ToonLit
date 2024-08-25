// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Containers/Array.h"
#include "Engine/Texture.h"
#include "PropertyEditorDelegates.h"
#include "UObject/SoftObjectPtr.h"
#include "DynamicMaterialEditorSettings.generated.h"

class FModifierKeysState;
class UMaterialFunctionInterface;
struct FPropertyChangedEvent;
enum EOrientation : int;

USTRUCT(BlueprintType)
struct FDMMaterialEffectList
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FString Name;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TArray<TSoftObjectPtr<UMaterialFunctionInterface>> Effects;
};

/**
 * Material Designer Settings
 */
UCLASS(Config=EditorPerProjectUserSettings, meta = (DisplayName = "Material Designer"))
class UDynamicMaterialEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDynamicMaterialEditorSettings();
	virtual ~UDynamicMaterialEditorSettings() override {}

	static UDynamicMaterialEditorSettings* Get();

	/** Changes the currently active material in the designer following actor/object selection. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bFollowSelection;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Default;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Shift;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_AltShift;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_Cmd;

	/** Adjusts the the spin box value sensitivity. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Layout", meta = (
		ClampMin = "0.01", UIMin = "0.01", ClampMax = "1.0", UIMax = "1.0"))
	float SpinBoxValueMultiplier_AltCmd;

	/** Adjusts the vertical size of the material layer view. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (
		ClampMin = "0.05", UIMin = "0.05", ClampMax = "0.95", UIMax = "0.95"))
	float SplitterLocation;

	/** Sets the maximum width for float-based sliders in the editor */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWRite, Category = "Layout", meta = (
		ClampMin = "100", UIMin = "100", ClampMax = "1000", UIMax = "1000"))
	float MaxFloatSliderWidth;

	/** The size of the material layer preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "32", UIMin = "32", ClampMax = "128", UIMax = "128"))
	int32 LayerPreviewSize;

	/** The size of the material slot preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "40", UIMin = "40", ClampMax = "128", UIMax = "128"))
	int32 SlotPreviewSize;

	/** The size of the material layer details preview images. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta = (ClampMin = "32", UIMin = "32", ClampMax = "128", UIMax = "128"))
	int32 DetailsPreviewSize;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	bool bPreviewImagesUseTextureUVs;

	/**
	 * If true, will display the hovered preview image in an enlarged format inside the tooltip.
	 * 
	 * NOTE: The Material Designer must be re-opened for changes to take effect.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ToolTips")
	bool bShowTooltipPreview;

	/** The size of the preview image when displayed in the tooltip. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "ToolTips", meta = (EditCondition = "bShowTooltipPreview == true", ClampMin = "128", UIMin = "128", ClampMax = "2048", UIMax = "2048"))
	int32 TooltipTextureSize;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview", meta=(DisplayName = "Default RGB Texture"))
	TSoftObjectPtr<UTexture> DefaultRGBTexture;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	TSoftObjectPtr<UTexture> DefaultOpaqueTexture;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	TSoftObjectPtr<UTexture> DefaultPreviewCanvasTexture;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Preview")
	TSoftObjectPtr<UTexture> DefaultOpacitySlotMask;

	/*
	 * Add paths to search for custom effects.
	 *
	 * Format examples:
	 * - /Game/Some/Path
	 * - /Plugin/Some/Path
	 *
	 * The assets must be in a sub-folder of the base path. The sub-folder
	 * will be used as the category name.
	 *
	 * Asset Examples:
	 * - /Game/Some/Path/UV/Asset.Asset -> Category: UV
	 * - /Plugin/Some/Path/Color/OtherAsset.OtherAsset -> Category: Color
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TArray<FName> CustomEffectsFolders;

	FOnFinishedChangingProperties OnSettingsChanged;

	//~ Begin UObject
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	//~ End UObject

	void OpenEditorSettingsWindow() const;

	void ResetAllLayoutSettings();

	float GetSpinboxValueChangeMultiplier(const FModifierKeysState& InModifierKeys) const;

	TArray<FDMMaterialEffectList> GetEffectList() const;
};
