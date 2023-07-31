// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Curves/CurveBase.h"
#include "CurveVector.generated.h"

USTRUCT(BlueprintType)
struct ENGINE_API FRuntimeVectorCurve
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FRichCurve VectorCurves[3];

	UPROPERTY(EditAnywhere, Category = RuntimeFloatCurve)
	TObjectPtr<class UCurveVector> ExternalCurve = nullptr;

	FVector GetValue(float InTime) const;
	
	/** Get the current curve struct */
    FRichCurve* GetRichCurve(int32 Index);
	const FRichCurve* GetRichCurveConst(int32 Index) const;
};

UCLASS(BlueprintType, MinimalAPI)
class UCurveVector : public UCurveBase
{
	GENERATED_UCLASS_BODY()

	/** Keyframe data, one curve for X, Y and Z */
	UPROPERTY()
	FRichCurve FloatCurves[3];

	/** Evaluate this float curve at the specified time */
	UFUNCTION(BlueprintCallable, Category="Math|Curves")
	ENGINE_API FVector GetVectorValue(float InTime) const;

	// Begin FCurveOwnerInterface
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;

	/** Determine if Curve is the same */
	ENGINE_API bool operator == (const UCurveVector& Curve) const;

	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;
};
