// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WebAPIMessageResponse.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct WEBAPI_API FWebAPIMessageResponse
{
	GENERATED_BODY()

	virtual ~FWebAPIMessageResponse() = default;

	virtual const FText& GetMessage() const;
};

inline const FText& FWebAPIMessageResponse::GetMessage() const
{
	return FText::GetEmpty();
}
