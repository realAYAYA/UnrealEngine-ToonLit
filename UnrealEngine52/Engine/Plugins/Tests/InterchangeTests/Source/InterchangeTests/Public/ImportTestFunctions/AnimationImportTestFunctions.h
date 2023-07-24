// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "AnimationImportTestFunctions.generated.h"

struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UAnimationImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of anim sequences are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedAnimSequenceCount(const TArray<UAnimSequence*>& AnimSequences, int32 ExpectedNumberOfImportedAnimSequences);

	/** Check whether the animation length (second) is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckAnimationLength(UAnimSequence* AnimSequence, float ExpectedAnimationLength);

	/** Check whether the animation frame number is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckAnimationFrameNumber(UAnimSequence* AnimSequence, int32 ExpectedFrameNumber);

	/** Check whether the given curve key time(sec) for the given curve name has the expected time(sec) */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyTime(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyTime);

	/** Check whether the given curve key value for the given curve name has the expected value */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyValue(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyValue);

	/** Check whether the given curve key arrive tangent for the given curve name has the expected arrive tangent */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyArriveTangent(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyArriveTangent);

	/** Check whether the given curve key arrive tangent weight for the given curve name has the expected arrive tangent weight */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyArriveTangentWeight(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyArriveTangentWeight);

	/** Check whether the given curve key leave tangent for the given curve name has the expected leave tangent */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyLeaveTangent(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyLeaveTangent);

	/** Check whether the given curve key leave tangent weight for the given curve name has the expected leave tangent weight */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckCurveKeyLeaveTangentWeight(UAnimSequence* AnimSequence, const FString& CurveName, int32 KeyIndex, float ExpectedCurveKeyLeaveTangentWeight);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "InterchangeTestFunction.h"
#endif
