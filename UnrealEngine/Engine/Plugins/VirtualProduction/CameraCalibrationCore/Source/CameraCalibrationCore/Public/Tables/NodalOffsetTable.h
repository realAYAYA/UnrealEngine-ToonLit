// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RichCurve.h"
#include "LensData.h"
#include "Tables/BaseLensTable.h"

#include "NodalOffsetTable.generated.h"


/**
 * Focus point for nodal offset curves
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FNodalOffsetFocusPoint : public FBaseFocusPoint
{
	GENERATED_BODY()

public:
	//~ Begin FBaseFocusPoint Interface
	virtual float GetFocus() const override { return Focus; }
	virtual int32 GetNumPoints() const override;
	virtual float GetZoom(int32 Index) const override;
	//~ End FBaseFocusPoint Interface

	/** Returns data type copy value for a given float */
	bool GetPoint(float InZoom, FNodalPointOffset& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Adds a new point at InZoom. Updates existing one if tolerance is met */
	bool AddPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance, bool bIsCalibrationPoint);

	/** Sets an existing point at InZoom. Updates existing one if tolerance is met */
	bool SetPoint(float InZoom, const FNodalPointOffset& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
	/** Removes a point corresponding to specified zoom */
	void RemovePoint(float InZoomValue);

	/** Returns true if there are no points */
	bool IsEmpty() const;

public:

	/** Dimensions of our location offset curves */
	static constexpr uint32 LocationDimension = 3;
	
	/** Dimensions of our rotation offset curves */
	static constexpr uint32 RotationDimension = 3;

	/** Input focus for this point */
	UPROPERTY()
	float Focus = 0.0f;

	/** XYZ offsets curves mapped to zoom */
	UPROPERTY()
	FRichCurve LocationOffset[LocationDimension];

	/** Yaw, Pitch and Roll offset curves mapped to zoom */
	UPROPERTY()
	FRichCurve RotationOffset[RotationDimension];
};

/**
 * Table containing nodal offset mapping to focus and zoom
 */
USTRUCT()
struct CAMERACALIBRATIONCORE_API FNodalOffsetTable : public FBaseLensTable
{
	GENERATED_BODY()

	using FocusPointType = FNodalOffsetFocusPoint;

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
	* Fills OutCurve with all points contained in the given focus 
	* Returns false if FocusIdentifier is not found or ParameterIndex isn't valid
	*/
	bool BuildParameterCurve(float InFocus, int32 ParameterIndex, EAxis::Type InAxis, FRichCurve& OutCurve) const;

	/** Returns const point for a given focus */
	const FNodalOffsetFocusPoint* GetFocusPoint(float InFocus) const;

	/** Returns point for a given focus */
	FNodalOffsetFocusPoint* GetFocusPoint(float InFocus);

	/** Returns all focus points */
	TConstArrayView<FNodalOffsetFocusPoint> GetFocusPoints() const;

	/** Returns all focus points */
	TArray<FNodalOffsetFocusPoint>& GetFocusPoints();

	/** Removes a focus point identified as InFocusIdentifier */
	void RemoveFocusPoint(float InFocus);

	/** Removes a zoom point from a focus point*/
	void RemoveZoomPoint(float InFocus, float InZoom);

	/** Adds a new point in the table */
	bool AddPoint(float InFocus, float InZoom, const FNodalPointOffset& InData,  float InputTolerance, bool bIsCalibrationPoint);

	/** Get the point from the table */
	bool GetPoint(const float InFocus, const float InZoom, FNodalPointOffset& OutData, float InputTolerance = KINDA_SMALL_NUMBER) const;

	/** Set a new point into the table */
	bool SetPoint(float InFocus, float InZoom, const FNodalPointOffset& InData, float InputTolerance = KINDA_SMALL_NUMBER);
	
public:

	/** Lists of focus points */
	UPROPERTY()
	TArray<FNodalOffsetFocusPoint> FocusPoints;
};

