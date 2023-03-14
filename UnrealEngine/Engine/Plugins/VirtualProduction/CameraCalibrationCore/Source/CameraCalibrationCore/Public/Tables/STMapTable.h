// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "LensData.h"
#include "Tables/BaseLensTable.h"

#include "STMapTable.generated.h"



/**
 * Derived data computed from parameters or stmap
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FDerivedDistortionData
{
	GENERATED_BODY()

	/** Precomputed data about distortion */
	UPROPERTY(VisibleAnywhere, Category = "Distortion")
	FDistortionData DistortionData;

	/** Computed displacement map based on undistortion data */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Distortion")
	TObjectPtr<UTextureRenderTarget2D> UndistortionDisplacementMap = nullptr;

	/** Computed displacement map based on distortion data */
	UPROPERTY(Transient, VisibleAnywhere, Category = "Distortion")
	TObjectPtr<UTextureRenderTarget2D> DistortionDisplacementMap = nullptr;

	/** When dirty, derived data needs to be recomputed */
	bool bIsDirty = true;
};

/**
 * STMap data associated to a zoom input value
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FSTMapZoomPoint
{
	GENERATED_BODY()

public:

	/** Input zoom value for this point */
	UPROPERTY()
	float Zoom = 0.0f;

	/** Data for this zoom point */
	UPROPERTY()
	FSTMapInfo STMapInfo;

	/** Derived distortion data associated with this point */
	UPROPERTY(Transient)
	FDerivedDistortionData DerivedDistortionData;

	/** Whether this point was added in calibration along distortion */
	UPROPERTY()
	bool bIsCalibrationPoint = false;
};

/**
 * A data point associating focus and zoom to lens parameters
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FSTMapFocusPoint : public FBaseFocusPoint
{
	GENERATED_BODY()

public:
	//~ Begin FBaseFocusPoint Interface
	virtual float GetFocus() const override { return Focus; }
	virtual int32 GetNumPoints() const override;
	virtual float GetZoom(int32 Index) const override;
	//~ End FBaseFocusPoint Interface
	
	/** Returns const point for a given zoom */
	const FSTMapZoomPoint* GetZoomPoint(float InZoom) const;

	/** Returns point for a given focus */
	FSTMapZoomPoint* GetZoomPoint(float InZoom);

	/** Returns zoom value for a given float */
	bool GetPoint(float InZoom, FSTMapInfo& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Adds a new point at InZoom. Updates existing one if tolerance is met */
	bool AddPoint(float InZoom, const FSTMapInfo& InData, float InputTolerance, bool bIsCalibrationPoint);

	/** Sets an existing point at InZoom. Updates existing one if tolerance is met */
	bool SetPoint(float InZoom, const FSTMapInfo& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Removes a point corresponding to specified zoom */
	void RemovePoint(float InZoomValue);

	/** Returns true if this point is empty */
	bool IsEmpty() const;
	
public:

	/** Input focus for this point */
	UPROPERTY()
	float Focus = 0.0f;

	/** Curve used to blend displacement map together to give user more flexibility */
	UPROPERTY()
	FRichCurve MapBlendingCurve;

	/** Zoom points for this focus */
	UPROPERTY()
	TArray<FSTMapZoomPoint> ZoomPoints;
};

/**
 * STMap table containing list of points for each focus and zoom inputs
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FSTMapTable : public FBaseLensTable
{
	GENERATED_BODY()

	using FocusPointType = FSTMapFocusPoint;

protected:
	//~ Begin FBaseDataTable Interface
	virtual TMap<ELensDataCategory, FLinkPointMetadata> GetLinkedCategories() const override;
	virtual bool DoesFocusPointExists(float InFocus) const override;
	virtual bool DoesZoomPointExists(float InFocus, float InZoom, float InputTolerance = KINDA_SMALL_NUMBER) const override;
	virtual const FBaseFocusPoint* GetBaseFocusPoint(int32 InIndex) const override;
	//~ End FBaseDataTable Interface
	
public:
	//~ Begin FBaseDataTable Interface
	virtual void ForEachPoint(FFocusPointCallback InCallback) const override;
	virtual int32 GetFocusPointNum() const override { return FocusPoints.Num(); }
	virtual int32 GetTotalPointNum() const override;
	virtual UScriptStruct* GetScriptStruct() const override;
	//~ End FBaseDataTable Interface

	/** 
	 * Builds the map blending curve into OutCurve
	 * Returns true if focus point exists
	 */
	bool BuildMapBlendingCurve(float InFocus, FRichCurve& OutCurve);
	
	/** Returns const point for a given focus */
	const FSTMapFocusPoint* GetFocusPoint(float InFocus) const;

	/** Returns point for a given focus */
	FSTMapFocusPoint* GetFocusPoint(float InFocus);

	/** Returns all focus points */
	TConstArrayView<FSTMapFocusPoint> GetFocusPoints() const;

	/** Returns all focus points */
	TArrayView<FSTMapFocusPoint> GetFocusPoints();

	/** Removes a focus point identified as InFocusIdentifier */
	void RemoveFocusPoint(float InFocus);

	/** Removes a zoom point from a focus point*/
	void RemoveZoomPoint(float InFocus, float InZoom);

	/** Adds a new point in the table */
	bool AddPoint(float InFocus, float InZoom, const FSTMapInfo& InData,  float InputTolerance, bool bIsCalibrationPoint);

	/** Get the point from the table */
	bool GetPoint(const float InFocus, const float InZoom, FSTMapInfo& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Set a new point into the table */
	bool SetPoint(float InFocus, float InZoom, const FSTMapInfo& InData, float InputTolerance = KINDA_SMALL_NUMBER);

public:

	/** Lists of focus points */
	UPROPERTY()
	TArray<FSTMapFocusPoint> FocusPoints;
};

