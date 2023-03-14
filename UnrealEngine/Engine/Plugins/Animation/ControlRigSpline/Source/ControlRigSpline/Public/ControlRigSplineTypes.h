// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// Define to switch between spline using the thirdparty TinySpline implementation or
// our native spline implementation
#define USE_TINYSPLINE 0

#include "CoreMinimal.h"
#include "ControlRigDefines.h"
#if USE_TINYSPLINE
#include "tinysplinecxx.h"
#endif
#include "ControlRigSplineTypes.generated.h"

class UControlRig;

UENUM()
enum class ESplineType : uint8
{
	/** BSpline */
	/** The smooth curve will pass through the first and last control points */
	BSpline,

	/** Hermite: */
	/** The curve will pass through the control points */
	Hermite,

	/** MAX - invalid */
	Max UMETA(Hidden),
};

#if !(USE_TINYSPLINE)
// Reading material
// 
// https://pages.mtu.edu/~shene/COURSES/cs3621/NOTES/notes.html
// https://github.com/caadxyz/DeBoorAlgorithmNurbs
// https://stackoverflow.com/questions/32897943/how-do-catmull-rom-and-hermite-splines-relate

// https://stackoverflow.com/questions/30748316/%20catmull-rom-interpolation-on-svg-paths/30826434#30826434
// https://stackoverflow.com/questions/32897943/how-do-catmull-rom-and-hermite-splines-relate
// https://www.cs.cmu.edu/~fp/courses/graphics/asst5/catmullRom.pdf

class ControlRigBaseSpline
{

protected:
	TArray<FVector> ControlPoints;
	uint16 Degree;
	
public:

	ControlRigBaseSpline(const TArrayView<const FVector>& InControlPoints, uint16 InDegree);
	virtual ~ControlRigBaseSpline(){}
	
	virtual FVector GetPointAtParam(float Param) = 0;

	virtual void SetControlPoints(const TArrayView<const FVector>& InControlPoints) { ControlPoints = InControlPoints; }

	TArray<FVector>& GetControlPoints() { return ControlPoints; }
};

class ControlRigBSpline : public ControlRigBaseSpline
{
	TArray<float> KnotVector;
	
public:
	ControlRigBSpline(const TArrayView<const FVector>& InControlPoints, const uint16 InDegree, const bool bInClamped);
	
	virtual FVector GetPointAtParam(float Param);

};

class ControlRigHermite : public ControlRigBaseSpline
{
	TArray<FVector> SegmentPoints;

	uint16 NumSegments;
	
public:
	ControlRigHermite(const TArrayView<const FVector>& InControlPoints);

	virtual void SetControlPoints(const TArrayView<const FVector>& InControlPoints) override;
	
	virtual FVector GetPointAtParam(float Param);

};

#endif

USTRUCT()
struct CONTROLRIGSPLINE_API FControlRigSplineImpl
{
	GENERATED_BODY()

	FControlRigSplineImpl()	
	{
		SplineMode = ESplineType::BSpline;
#if !(USE_TINYSPLINE)
		Spline = nullptr;
#endif
		SamplesPerSegment = 16;
	}

	virtual ~FControlRigSplineImpl();

	// Spline type
	ESplineType SplineMode;

#if USE_TINYSPLINE
	// The control points to construct the spline
	TArray<FVector> ControlPoints;
#endif

	// The initial lengths between samples
	TArray<float> InitialLengths;

	// The actual spline
#if USE_TINYSPLINE
	tinyspline::BSpline Spline;
#else
	ControlRigBaseSpline* Spline;
#endif

	// Samples per segment, where segment is the portion between two control points
	int32 SamplesPerSegment;

	// The allowed length compression (1.f being do not allow compression`). If 0, no restriction wil be applied.
	float Compression;

	// The allowed length stretch (1.f being do not allow stretch`). If 0, no restriction wil be applied.
	float Stretch;

	// Positions along the "real" curve (no samples in the first and last segments of a hermite spline)
	TArray<FVector> SamplesArray;

	// Accumulated length along the spline given by samples
	TArray<float> AccumulatedLenth;

	// Returns a reference to the control points that were used to create this spline 
	TArray<FVector>& GetControlPoints();
};

USTRUCT(BlueprintType)
struct CONTROLRIGSPLINE_API FControlRigSpline 
{
	GENERATED_BODY()

	FControlRigSpline()	{}

	virtual ~FControlRigSpline() {}

	FControlRigSpline(const FControlRigSpline& InOther);
	FControlRigSpline& operator =(const FControlRigSpline& InOther);

	TSharedPtr<FControlRigSplineImpl> SplineData;

	/**
	* Sets the control points in the spline. It will build the spline if needed, or forceRebuild is true,
	* or will update the points if building from scratch is not necessary. The type of spline to build will
	* depend on what is set in SplineMode.
	*
	* @param InPoints	The control points to set.
	* @param SplineMode	The type of spline
	* @param SamplesPerSegment The samples to cache for every segment defined between two control rig
	* @param Compression The allowed length compression (1.f being do not allow compression`). If 0, no restriction wil be applied.
	* @param Stretch The allowed length stretch (1.f being do not allow stretch). If 0, no restriction wil be applied.
	*/
	void SetControlPoints(const TArrayView<const FVector>& InPoints, const ESplineType SplineMode = ESplineType::BSpline, const int32 SamplesPerSegment = 16, const float Compression = 1.f, const float Stretch = 1.f);

	/**
	* Given an InParam float in [0, 1], will return the position of the spline at that point.
	*
	* @param InParam	The parameter between [0, 1] to query.
	* @return			The position in the spline.
	*/
	FVector PositionAtParam(const float InParam) const;

	/**
	* Given an InParam float in [0, 1], will return the tangent vector of the spline at that point. 
	* Note that this vector is not normalized.
	*
	* @param InParam	The parameter between [0, 1] to query.
	* @return			The tangent of the spline at InParam.
	*/
	FVector TangentAtParam(const float InParam) const;
};
