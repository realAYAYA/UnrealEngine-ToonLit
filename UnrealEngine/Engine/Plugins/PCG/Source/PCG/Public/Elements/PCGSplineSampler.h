// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"

#include "Curves/CurveFloat.h"

#include "PCGSplineSampler.generated.h"

struct FPCGContext;
struct FPCGProjectionParams;
class UPCGPolyLineData;
class UPCGSpatialData;
class UPCGPointData;

namespace PCGSplineSamplerConstants
{
	const FName SplineLabel = TEXT("Spline");
	const FName BoundingShapeLabel = TEXT("Bounding Shape");
}

UENUM()
enum class EPCGSplineSamplingMode : uint8
{
	Subdivision = 0,
	Distance,
	NumberOfSamples UMETA(Tooltip = "Samples a specified number of times, evenly spaced around the spline.")
};

UENUM()
enum class EPCGSplineSamplingDimension : uint8
{
	OnSpline = 0,
	OnHorizontal,
	OnVertical,
	OnVolume,
	OnInterior
};

UENUM()
enum class EPCGSplineSamplingFill : uint8
{
	Fill = 0,
	EdgesOnly = 1
};

UENUM()
enum class EPCGSplineSamplingInteriorOrientation : uint8
{
	Uniform = 0,
	FollowCurvature = 1
};

USTRUCT(BlueprintType)
struct FPCGSplineSamplerParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSplineSamplingDimension Dimension = EPCGSplineSamplingDimension::OnSpline;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingMode Mode = EPCGSplineSamplingMode::Subdivision;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension!=EPCGSplineSamplingDimension::OnSpline&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingFill Fill = EPCGSplineSamplingFill::Fill;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Mode==EPCGSplineSamplingMode::Subdivision&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	int32 SubdivisionsPerSegment = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Mode==EPCGSplineSamplingMode::Distance&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	float DistanceIncrement = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Mode==EPCGSplineSamplingMode::NumberOfSamples&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	int32 NumSamples = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnHorizontal||Dimension==EPCGSplineSamplingDimension::OnVolume"))
	int32 NumPlanarSubdivisions = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnVertical||Dimension==EPCGSplineSamplingDimension::OnVolume"))
	int32 NumHeightSubdivisions = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeDirectionDelta = false;

	/** Attribute that will contain the delta angle to the next point on the spline w.r.t to the current's point Up vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeDirectionDelta"))
	FName NextDirectionDeltaAttribute = TEXT("NextDirectionDelta");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeCurvature = false;

	/** Attribute that will contain the curvature. Note that the radius of curvature is defined as 1/Curvature, and might need you to scale to world units */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeCurvature"))
	FName CurvatureAttribute = TEXT("Curvature");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeSegmentIndex = false;

	/** Attribute that will contain the spline segment index. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeSegmentIndex"))
	FName SegmentIndexAttribute = TEXT("SegmentIndex");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeSubsegmentIndex = false;

	/** Attribute that will contain the sub-segment index of a point on the spline. When the sub-segment index is 0, the point is a control point on the actual spline. Only applies to Subdivision mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeSubsegmentIndex"))
	FName SubsegmentIndexAttribute = TEXT("SubsegmentIndex");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bComputeTangents = false;

	/** Attribute that will contain the arrive tangent vector. For control points, this will be the actual arrive tangent. For non-control points, this will only be the normalized tangent at this point. Only applies to Subdivision mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeTangents"))
	FName ArriveTangentAttribute = TEXT("ArriveTangent");

	/** Attribute that will contain the leave tangent vector. For control points, this will be the actual leave tangent. For non-control points, this will only be the normalized tangent at this point. Only applies to Subdivision mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeTangents"))
	FName LeaveTangentAttribute = TEXT("LeaveTangent");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeAlpha = false;

	/**
	 * Attribute that will contain a value in [0,1] representing how far along the point is to the end of the line. Each segment on the line represents a same-size interval.
	 * For example, if there are three segments, each segment will take up 0.333... of the interval.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeAlpha"))
	FName AlphaAttribute = TEXT("Alpha");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeDistance = false;

	/** Attribute that will contain the distance along the spline at the sample point. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeDistance"))
	FName DistanceAttribute = TEXT("Distance");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (InlineEditConditionToggle))
	bool bComputeInputKey = false;

	/** Attribute that will contain the spline input key, a float value between [0, N], where N is the number of control points. Each range [i, i+1] represents an interpolation from 0 to 1 across spline segment i. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bComputeInputKey"))
	FName InputKeyAttribute = TEXT("InputKey");

	/** If no Bounding Shape input is provided, the actor bounds are used to limit the sample generation domain.
	* This option allows ignoring the actor bounds and generating over the entire spline. Use with caution as this
	* may generate a lot of points.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUnbounded = false; 
	
	/** The space between each sample point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	float InteriorSampleSpacing = 100.0f;

	/** The space between each sample point on the spline boundary. Used for computation; lower spacing is more expensive but more accurate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior&&!bTreatSplineAsPolyline"))
	float InteriorBorderSampleSpacing = 100.0f;

	/** Use the spline points to form a polyline, instead of computing many sample points along the spline. This is more accurate if your spline is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	bool bTreatSplineAsPolyline = false;

	/** Determines the orientation of interior points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingInteriorOrientation InteriorOrientation = EPCGSplineSamplingInteriorOrientation::Uniform;

	/** Project sample points onto one possible surface given by the spline boundary. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	bool bProjectOntoSurface = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Defines the density for each sample based on its distance from the spline. X axis is normalized distance to boundary (0-1), Y axis is density value. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	FRuntimeFloatCurve InteriorDensityFalloffCurve;

	/** Each PCG point represents a discretized, volumetric region of world space. The points' Steepness value [0.0 to
	 * 1.0] establishes how "hard" or "soft" that volume will be represented. From 0, it will ramp up linearly
	 * increasing its influence over the density from the point's center to up to two times the bounds. At 1, it will
	 * represent a binary box function with the size of the point's bounds.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Points", meta=(ClampMin="0", ClampMax="1", PCG_Overridable))
	float PointSteepness = 0.5f;

	/** Controls whether we will seed the sampled points using the final world position or the local position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seeding")
	bool bSeedFromLocalPosition = false;

	/** Controls whether we will seed the sampled points using the 3D position or the 2D (XY) position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seeding")
	bool bSeedFrom2DPosition = false;
};

namespace PCGSplineSamplerHelpers
{
	/** Samples on spline or within volume around it. */
	void SampleLineData(const UPCGPolyLineData* LineData, const UPCGSpatialData* InBoundingShape, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData);

	/** Samples 2D region bounded by spline. */
	void SampleInteriorData(FPCGContext* Context, const UPCGPolyLineData* LineData, const UPCGSpatialData* InBoundingShape, const UPCGSpatialData* InProjectionTarget, const FPCGProjectionParams& InProjectionParams, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData);

	const UPCGPolyLineData* GetPolyLineData(const UPCGSpatialData* InSpatialData);

	/** Tests if a point lies inside the given polygon by casting a ray to MaxDistance and counting the intersections. */
	bool PointInsidePolygon2D(const TArray<FVector2D>& PolygonPoints, const FVector2D& Point, FVector::FReal MaxDistance);

	/** Projects a point in space onto the approximated surface defined by a closed spline. Returns the height of the point after projection. */
	FVector::FReal ProjectOntoSplineInteriorSurface(const TArray<FVector>& SplinePoints, const FVector& PointToProject);
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSplineSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SplineSampler")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSplineSamplerSettings", "NodeTitle", "Spline Sampler"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;
	virtual void ApplyDeprecationBeforeUpdatePins(UPCGNode* InOutNode, TArray<TObjectPtr<UPCGPin>>& InputPins, TArray<TObjectPtr<UPCGPin>>& OutputPins) override;
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties, PCG_Overridable))
	FPCGSplineSamplerParams SamplerParams;
};

class FPCGSplineSamplerElement : public IPCGElement
{
public:
	virtual void GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const override;
	// Worth computing a full CRC in case we can halt change propagation/re-executions
	virtual bool ShouldComputeFullOutputDataCrc(FPCGContext* Context) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
