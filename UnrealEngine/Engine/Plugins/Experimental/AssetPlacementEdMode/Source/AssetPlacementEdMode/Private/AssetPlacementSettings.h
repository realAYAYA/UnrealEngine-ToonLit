// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FoliageType.h"
#include "PlacementPaletteItem.h"

#include "AssetPlacementSettings.generated.h"

class UPlacementPaletteAsset;
class UPlacementPaletteClient;

UCLASS(config = EditorPerProjectUserSettings)
class UAssetPlacementSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Set to true if the tool should automatically align placement to an axis. */
	UPROPERTY(config, EditAnywhere, Category = "Axis Alignment", meta = (InlineEditConditionToggle))
	bool bAlignToNormal = true;

	/** The axis which the tool should use for alignment calculations. This axis is positive unless Invert Normal Axis is set. */
	UPROPERTY(config, EditAnywhere, Category = "Axis Alignment", meta = (DisplayName = "Align To Normal", EditCondition = "bAlignToNormal"))
	TEnumAsByte<EAxis::Type> AxisToAlignWithNormal = EAxis::Type::Z;

	/** Set to true if the tool should negate the alignment axis. */
	UPROPERTY(config, EditAnywhere, Category = "Axis Alignment")
	bool bInvertNormalAxis = false;

	/** Set to true if the tool should generate a random rotation on the X axis */
	UPROPERTY(config, EditAnywhere, Category = "Rotation", meta = (InlineEditConditionToggle))
	bool bUseRandomRotationX = false;

	/** The range in degrees at which the tool should generate a random rotation on the X axis. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Rotation", meta = (EditCondition = "bUseRandomRotationX", UIMin = "0.0", UIMax = "180.0"))
	FFloatInterval RandomRotationX = FFloatInterval(0.0f, 180.0f);

	/** Set to true if the tool should generate a negatively signed rotation on the X axis. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Rotation")
	bool bAllowNegativeRotationX = true;

	/** Set to true if the tool should generate a random rotation on the Y axis */
	UPROPERTY(config, EditAnywhere, Category = "Rotation", meta = (InlineEditConditionToggle))
	bool bUseRandomRotationY = false;

	/** The range in degrees at which the tool should generate a random rotation on the Y axis. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Rotation", meta = (EditCondition = "bUseRandomRotationY", UIMin = "0.0", UIMax = "180.0"))
	FFloatInterval RandomRotationY = FFloatInterval(0.0f, 180.0f);

	/** Set to true if the tool should generate a negatively signed rotation on the Y axis. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Rotation")
	bool bAllowNegativeRotationY = true;

	/** Set to true if the tool should generate a random rotation on the Z axis */
	UPROPERTY(config, EditAnywhere, Category = "Rotation", meta = (InlineEditConditionToggle))
	bool bUseRandomRotationZ = true;

	/** The range in degrees at which the tool should generate a random rotation on the Z axis. */
	UPROPERTY(config, EditAnywhere, Category = "Rotation", meta = (EditCondition = "bUseRandomRotationZ", UIMin = "0.0", UIMax = "180.0"))
	FFloatInterval RandomRotationZ = FFloatInterval(0.0f, 180.0f);
	
	/** Set to true if the tool should generate a negatively signed rotation on the Z axis. */
	UPROPERTY(config, EditAnywhere, Category = "Rotation")
	bool bAllowNegativeRotationZ = true;

	/** Which axes scale should be applied to. Any locked axes will be scaled to 1.0f. */
	UPROPERTY(config, EditAnywhere, Category = "Scale")
	EFoliageScaling ScalingType = EFoliageScaling::Uniform;
	
	/** The min and max at which a random scale is generated, or a scale is applied. */
	UPROPERTY(config, EditAnywhere, Category = "Scale", meta = (UIMin = ".01"))
	FFloatInterval ScaleRange = FFloatInterval(.5f, 2.0f);

	/** Set to true if the tool should randomly scale any generated assets */
	UPROPERTY(config, EditAnywhere, Category = "Scale", meta = (InlineEditConditionToggle))
	bool bUseRandomScale = true;

	/** Set to true if the tool should generate a negatively signed scale. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Scale")
	bool bAllowNegativeScale = false;

	/** The spawn location offset in relative space. */
	UPROPERTY(config, EditAnywhere, Category = "Location")
	FVector RelativeLocationOffset = FVector::ZeroVector;

	/** If the relative location offset should match scale with the placed asset. */
	UPROPERTY(config, EditAnywhere, Category = "Location")
	bool bScaleRelativeLocationOffset = true;

	/** The spawn rotation offset in world space. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Location")
	FVector WorldLocationOffset = FVector::ZeroVector;

	/** If the world location offset should match scale with the placed asset. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, Category = "Location")
	bool bScaleWorldLocationOffset = false;

	/** If the tool should place on landscape surfaces. */
	UPROPERTY(config, EditAnywhere, Category = "Place on Filters")
	bool bLandscape = true;

	/** If the tool should place on static mesh surfaces. */
	UPROPERTY(config, EditAnywhere, Category = "Place on Filters")
	bool bStaticMeshes = true;

	/** If the tool should place on BSP surfaces. */
	UPROPERTY(config, EditAnywhere, Category = "Place on Filters")
	bool bBSP = true;

	/** If the tool should place on foliage surfaces. */
	UPROPERTY(config, EditAnywhere, Category = "Place on Filters")
	bool bFoliage = false;

	/** If the tool should place on translucent surfaces. */
	UPROPERTY(config, EditAnywhere, Category = "Place on Filters")
	bool bTranslucent = false;

	UPROPERTY(config)
	bool bUseContentBrowserSelection = true;

	virtual bool CanEditChange(const FProperty* InProperty) const override;

	void SetPaletteAsset(UPlacementPaletteAsset* InPaletteAsset);
	UPlacementPaletteClient* AddClientToActivePalette(const FAssetData& InAssetData);
	int32 RemoveClientFromActivePalette(const FAssetData& InAssetData);
	TArrayView<const TObjectPtr<UPlacementPaletteClient>> GetActivePaletteItems() const;
	const FGuid GetActivePaletteGuid() const;
	void ClearActivePaletteItems();
	void SaveActivePalette();
	FSoftObjectPath GetActivePalettePath() const;
	void LoadSettings();
	void SaveSettings();

	/**
	 * Verifies if the given element handle is supported by the current mode settings' palette.
	 *
	 * @returns true if the element can be placed by the mode.
	 */
	bool DoesActivePaletteSupportElement(const FTypedElementHandle& InElementToCheck) const;

protected:
	const UPlacementPaletteAsset* GetActivePalette() const;
	UPlacementPaletteAsset* GetMutableActivePalette();

	UPROPERTY(Transient)
	TObjectPtr<UPlacementPaletteAsset> ActivePalette;

	UPROPERTY(Transient)
	TObjectPtr<UPlacementPaletteAsset> UserPalette;

	UPROPERTY(config)
	FGuid UserGridGuid;

	UPROPERTY(config)
	FSoftObjectPath LastActivePalettePath;
};
