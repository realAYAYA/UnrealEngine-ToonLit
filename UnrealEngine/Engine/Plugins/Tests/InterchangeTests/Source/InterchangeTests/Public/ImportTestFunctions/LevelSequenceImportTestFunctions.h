// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/RealCurve.h"
#include "ImportTestFunctionsBase.h"
#include "LevelSequenceImportTestFunctions.generated.h"

class ULevelSequence;
struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API ULevelSequenceImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of level sequences are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckLevelSequenceCount(const TArray<ULevelSequence*>& LevelSequences, int32 ExpectedNumberOfLevelSequences);

	/** Check whether the level sequence length (second) is as expected */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSequenceLength(const ULevelSequence* LevelSequence, float ExpectedLevelSequenceLength);

	/** Check whether the imported level sequence has the expected number of sections */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSectionCount(const ULevelSequence* LevelSequence, int32 ExpectedNumberOfSections);

	/** Check whether the imported level sequence has the expected interpolation mode for the given section */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckSectionInterpolationMode(const ULevelSequence* LevelSequence, int32 SectionIndex, ERichCurveInterpMode ExpectedInterpolationMode);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "InterchangeTestFunction.h"
#endif
