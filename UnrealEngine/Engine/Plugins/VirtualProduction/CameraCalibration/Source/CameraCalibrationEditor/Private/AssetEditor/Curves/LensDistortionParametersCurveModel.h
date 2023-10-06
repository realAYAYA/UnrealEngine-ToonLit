// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LensDataCurveModel.h"

#include "Curves/RichCurve.h"


/**
 * Handles curves associated to distortion parameters
 */
class FLensDistortionParametersCurveModel : public FLensDataCurveModel
{
public:

	FLensDistortionParametersCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex);

	//~ Begin FRichCurveEditorModel interface
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType = EPropertyChangeType::Unspecified) override;
	//~ End FRichCurveEditorModel interface

	//~ Begin FLensDataCurveModel interface
	virtual FText GetValueLabel() const override;
	//~ End FLensDataCurveModel interface

private:

	/** Focus value we are displaying a curve for */
	float Focus = 0.0f;

	/**
	 * Parameter we are displaying a curve for
	 * INDEX_NONE means we are showing the map blending curve
	 */	
	int32 ParameterIndex = INDEX_NONE; 
};
