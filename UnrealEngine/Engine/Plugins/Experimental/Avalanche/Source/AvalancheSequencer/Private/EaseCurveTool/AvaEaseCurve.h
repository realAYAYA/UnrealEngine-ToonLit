// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaEaseCurveTangents.h"
#include "Curves/CurveFloat.h"
#include "AvaEaseCurve.generated.h"

UCLASS()
class UAvaEaseCurve : public UCurveFloat 
{
	GENERATED_BODY()

public:
	UAvaEaseCurve();

	FKeyHandle GetStartKeyHandle() const;
	FKeyHandle GetEndKeyHandle() const;

	FRichCurveKey& GetStartKey();
	FRichCurveKey& GetEndKey();

	float GetStartKeyTime() const;
	float GetStartKeyValue() const;
	float GetEndKeyTime() const;
	float GetEndKeyValue() const;

	FAvaEaseCurveTangents GetTangents() const;
	void SetTangents(const FAvaEaseCurveTangents& InTangents);
	void SetStartTangent(const float InTangent, const float InTangentWeight);
	void SetEndTangent(const float InTangent, const float InTangentWeight);

	void FlattenOrStraightenTangents(const FKeyHandle InKeyHandle, const bool bInFlattenTangents);

	void SetInterpMode(const ERichCurveInterpMode InInterpMode, const ERichCurveTangentMode InTangentMode);

	void BroadcastUpdate();

	//~ Begin UCurveFloat
	virtual void OnCurveChanged(const TArray<FRichCurveEditInfo>& InRichCurveEditInfos) override;
	//~ End UCurveFloat

protected:
	FKeyHandle StartKeyHandle;
	FKeyHandle EndKeyHandle;
};
