// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LensDataCurveModel.h"

/**
 * Handles image center curves to be displayed
 */
class FLensImageCenterCurveModel : public FLensDataCurveModel
{
public:

	FLensImageCenterCurveModel(ULensFile* InOwner, float InFocus, int32 InParameterIndex);

	//~ Begin FRichCurveEditorModel interface
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType) override;
	//~ End FRichCurveEditorModel interface

	//~ Begin FLensDataCurveModel interface
	virtual FText GetValueLabel() const override;
	//~ End FLensDataCurveModel interface

private:

	/** Current focus to display curve for */
	float Focus = 0.0f;

	/**
	 *Active parameter index
	 * 0: Cx
	 * 1: Cy
	 */
	int32 ParameterIndex = INDEX_NONE;
};
