// Copyright Epic Games, Inc. All Rights Reserved.

#include "LensDataCurveModel.h"

#include "LensFile.h"

#define LOCTEXT_NAMESPACE "LensDataCurveModel"


ECurveEditorViewID FLensDataCurveModel::ViewId = ECurveEditorViewID::Invalid;
FLensDataCurveModel::FLensDataCurveModel(ULensFile* InOwner)
	: FRichCurveEditorModel(InOwner)
	, LensFile(InOwner)
{
	check(InOwner);

	SupportedViews = ViewId;
}

void FLensDataCurveModel::AddKeys(TArrayView<const FKeyPosition> InKeyPositions, TArrayView<const FKeyAttributes> InAttributes, TArrayView<TOptional<FKeyHandle>>* OutKeyHandles)
{
	//Don't support adding keys from curve editor by default. Specific models can override
}

void FLensDataCurveModel::RemoveKeys(TArrayView<const FKeyHandle> InKeys)
{
	//Don't support removing keys from curve editor by default. Specific models can override
}

void FLensDataCurveModel::SetKeyAttributes(TArrayView<const FKeyHandle> InKeys,	TArrayView<const FKeyAttributes> InAttributes, EPropertyChangeType::Type ChangeType)
{
	//Don't support changing interp attributes from curve editor by default. Specific models can override
}

bool FLensDataCurveModel::IsValid() const
{
	return bIsCurveValid;
}

FRichCurve& FLensDataCurveModel::GetRichCurve()
{
	return CurrentCurve;
}

const FRichCurve& FLensDataCurveModel::GetReadOnlyRichCurve() const
{
	return CurrentCurve;
}

UObject* FLensDataCurveModel::GetOwningObject() const
{
	return LensFile.Get();
}

FText FLensDataCurveModel::GetKeyLabel() const
{
	return LOCTEXT("XAxisLabel", "Raw Zoom");
}

FText FLensDataCurveModel::GetValueLabel() const
{
	return FText();
}

FText FLensDataCurveModel::GetValueUnitPrefixLabel() const
{
	return FText();
}

FText FLensDataCurveModel::GetValueUnitSuffixLabel() const
{
	return FText();
}

#undef LOCTEXT_NAMESPACE
