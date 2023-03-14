// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "PCGElement.h"
#include "Curves/CurveFloat.h"

#include "PCGSplineSampler.generated.h"

class UPCGPolyLineData;
class UPCGSpatialData;
class UPCGPointData;

UENUM()
enum class EPCGSplineSamplingMode : uint8
{
	Subdivision = 0,
	Distance
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
struct PCG_API FPCGSplineSamplerParams
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingMode Mode = EPCGSplineSamplingMode::Subdivision;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGSplineSamplingDimension Dimension = EPCGSplineSamplingDimension::OnSpline;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension!=EPCGSplineSamplingDimension::OnSpline&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingFill Fill = EPCGSplineSamplingFill::Fill;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Mode==EPCGSplineSamplingMode::Subdivision&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	int32 SubdivisionsPerSegment = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Mode==EPCGSplineSamplingMode::Distance&&Dimension!=EPCGSplineSamplingDimension::OnInterior"))
	float DistanceIncrement = 100.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnHorizontal||Dimension==EPCGSplineSamplingDimension::OnVolume"))
	int32 NumPlanarSubdivisions = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnVertical||Dimension==EPCGSplineSamplingDimension::OnVolume"))
	int32 NumHeightSubdivisions = 8;

	/** The space between each sample point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	float InteriorSampleSpacing = 100.0f;

	/** The space between each sample point on the spline boundary. Used for computation; lower spacing is more expensive but more accurate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ClampMin = "0.1", EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior&&!bTreatSplineAsPolyline"))
	float InteriorBorderSampleSpacing = 100.0f;

	/** Use the spline points to form a polyline, instead of computing many sample points along the spline. This is more accurate if your spline is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	bool bTreatSplineAsPolyline = false;

	/** Determines the orientation of interior points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	EPCGSplineSamplingInteriorOrientation InteriorOrientation = EPCGSplineSamplingInteriorOrientation::Uniform;

	/** Project sample points onto one possible surface given by the spline boundary */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	bool bProjectOntoSurface = false;

	// TODO: DirtyCache for OnDependencyChanged when this float curve is an external asset
	/** Defines the density for each sample based on its distance from the spline */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "Dimension==EPCGSplineSamplingDimension::OnInterior"))
	FRuntimeFloatCurve InteriorDensityFalloffCurve;
};

namespace PCGSplineSamplerHelpers
{
	/** Tests if a point lies inside the given polygon by casting a ray to MaxDistance and counting the intersections */
	bool PointInsidePolygon2D(const TArray<FVector2D>& PolygonPoints, const FVector2D& Point, FVector::FReal MaxDistance);
}

namespace PCGSplineSampler
{
	void SampleLineData(const UPCGPolyLineData* LineData, const UPCGSpatialData* SpatialData, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData);
	void SampleInteriorData(const UPCGPolyLineData* LineData, const UPCGSpatialData* SpatialData, const FPCGSplineSamplerParams& Params, UPCGPointData* OutPointData);
	const UPCGPolyLineData* GetPolyLineData(const UPCGSpatialData* InSpatialData);
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSplineSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	// ~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SplineSamplerNode")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }

protected:
	virtual FPCGElementPtr CreateElement() const override;
	// ~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ShowOnlyInnerProperties))
	FPCGSplineSamplerParams Params;
};

class FPCGSplineSamplerElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};