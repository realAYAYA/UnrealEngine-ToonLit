// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeaturePluginOperationResult.h"


namespace UE::GameFeatures
{
	FResult::FResult(FErrorCodeType ErrorCodeIn)
		: ErrorCode(MoveTemp(ErrorCodeIn))
		, OptionalErrorText()
	{
	}

	FResult::FResult(FErrorCodeType ErrorCodeIn, FText ErrorTextIn)
		: ErrorCode(MoveTemp(ErrorCodeIn))
		, OptionalErrorText(MoveTemp(ErrorTextIn))
	{
	}

	FString ToString(const FResult& Result)
	{
		return Result.HasValue() ? FString(TEXT("Success")) : (FString(TEXT("Failure, ErrorCode=")) + Result.GetError() + FString(TEXT(", OptionalErrorText=")) + Result.OptionalErrorText.ToString());
	}
}	// namespace UE::GameFeatures