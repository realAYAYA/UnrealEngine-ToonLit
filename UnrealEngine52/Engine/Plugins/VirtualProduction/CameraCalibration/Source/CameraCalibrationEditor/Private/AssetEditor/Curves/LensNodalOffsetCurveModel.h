// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LensDataCurveModel.h"

/**
 * Handles nodal offset curves to be displayed
 */
class FLensNodalOffsetCurveModel : public FLensDataCurveModel
{
public:

	FLensNodalOffsetCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex, EAxis::Type InAxis);

	//~ Begin FRichCurveEditorModel interface
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType) override;
	//~ End FRichCurveEditorModel interface

	//~ Begin FLensDataCurveModel interface
	virtual FText GetValueLabel() const override;
	virtual FText GetValueUnitSuffixLabel() const override;
	//~ End FLensDataCurveModel interface

private:

	/** Current focus to display curve for */
	float Focus = 0.0f;

	/**
	 * Active parameter index
	 * 0: Location offset
	 * 1: Rotation offset
	 */
	int32 ParameterIndex = INDEX_NONE;

	/** Axis to display */
	EAxis::Type Axis = EAxis::None;
};
