// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LensDataCurveModel.h"

#include "Curves/RichCurve.h"

struct FFocalLengthFocusPoint;
/**
 * Handles focal length curves to be displayed
 */
class FLensFocalLengthCurveModel : public FLensDataCurveModel
{
public:

	FLensFocalLengthCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex);

	//~ Begin FRichCurveEditorModel interface
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType) override;
	//~ End FRichCurveEditorModel interface

	//~ Begin FLensDataCurveModel interface
	virtual FText GetValueLabel() const override;
	virtual FText GetValueUnitSuffixLabel() const override;
	//~ End FLensDataCurveModel interface

protected:
	FRichCurve* GetRichCurveForParameterIndex(FFocalLengthFocusPoint& InFocusPoint) const;

private:

	/** Input focus we are currently showing */
	float Focus = 0.0f;

	/**
	 * Focal length parameter index
	 *-1: Focal length curve in mm based on Fx and SensorDimensions.Width
	 * 0: Fx
	 * 1: Fy
	 */
	int32 ParameterIndex = INDEX_NONE; //Defaults to Focal Length derived curve
};
