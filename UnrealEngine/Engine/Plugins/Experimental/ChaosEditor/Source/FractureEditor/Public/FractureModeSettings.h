// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"

#include "GeometryCollection/GeometryCollectionConvexUtility.h"

#include "FractureModeSettings.generated.h"

class FGeometryCollection;

/**
 * Settings for the Fracture Editor Mode.
 */
UCLASS(config = Editor)
class FRACTUREEDITOR_API UFractureModeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// UDeveloperSettings overrides

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Plugins"); }
	virtual FName GetSectionName() const override { return FName("FractureMode"); }

	virtual FText GetSectionText() const override { return NSLOCTEXT("FractureModeSettings", "FractureModeSettingsName", "Fracture Mode"); }
	virtual FText GetSectionDescription() const override { return NSLOCTEXT("FractureModeSettings", "FractureModeSettingsDescription", "Configure the Fracture Editor Mode plugin"); }

public:

	/** Default fraction of geometry volume by which a cluster's convex hull volume can exceed the actual geometry volume before instead using the hulls of the children.  0 means the convex volume cannot exceed the geometry volume; 1 means the convex volume is allowed to be 100% larger (2x) the geometry volume. */
	UPROPERTY(config, EditAnywhere, Category = "Fracture Mode|Convex Generation Defaults", meta = (ClampMin = "0", DisplayName = "Allow Larger Hull Fraction"))
	float ConvexCanExceedFraction = .5;

	/** Default simplification threshold for convex hulls of new geometry collections */
	UPROPERTY(config, EditAnywhere, Category = "Fracture Mode|Convex Generation Defaults", meta = (ClampMin = "0", DisplayName = "Simplification Distance Threshold"))
	float ConvexSimplificationDistanceThreshold = 10;

	/** Default overlap removal setting for convex hulls of new geometry collections */
	UPROPERTY(config, EditAnywhere, Category = "Fracture Mode|Convex Generation Defaults|Overlap Removal", meta = (DisplayName = "Remove Overlaps"))
	EConvexOverlapRemoval ConvexRemoveOverlaps = EConvexOverlapRemoval::All;

	/** Default overlap removal shrink percent (in range 0-100) for convex hulls of new geometry collections. Overlap removal will be computed assuming convex shapes will be scaled down by this percentage. */
	UPROPERTY(config, EditAnywhere, Category = "Fracture Mode|Convex Generation Defaults|Overlap Removal", meta = (UIMin = "0", ClampMax = "99.9", DisplayName = "Overlap Removal Shrink Percent"))
	float ConvexOverlapRemovalShrinkPercent = 0;

	/** Default fraction of convex hulls for a transform that we can remove before using the hulls of the children */
	UPROPERTY(config, EditAnywhere, Category = "Fracture Mode|Convex Generation Defaults|Overlap Removal", meta = (DisplayName = "Max Removal Fraction", ClampMin = ".01", ClampMax = "1"))
	double ConvexFractionAllowRemove = .5;

	// Apply Convex Generation Defaults to a GeometryCollection
	void ApplyDefaultConvexSettings(FGeometryCollection& GeometryCollection) const;


	DECLARE_MULTICAST_DELEGATE_TwoParams(UFractureModeSettingsModified, UObject*, FProperty*);
	UFractureModeSettingsModified OnModified;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		OnModified.Broadcast(this, PropertyChangedEvent.Property);

		Super::PostEditChangeProperty(PropertyChangedEvent);
	}


};