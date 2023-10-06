// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LensDataCurveModel.h"

/**
* Handles STMap curve to be displayed
*/
class FLensSTMapCurveModel : public FLensDataCurveModel
{
public:

	FLensSTMapCurveModel(ULensFile* InOwner, float InFocus);

	//~ Begin FRichCurveEditorModel interface
	virtual void SetKeyPositions(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyPosition> InKeyPositions, EPropertyChangeType::Type ChangeType) override;
	virtual void SetKeyAttributes(TArrayView<const FKeyHandle> InKeys, TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType = EPropertyChangeType::Unspecified) override;
	//~ End FRichCurveEditorModel interface

private:

	/** Focus value we are displaying a curve for */
	float Focus = 0.0f;
};
