// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraphSchema_K2.h"

#include "MVVMConversionFunctionGraphSchema.generated.h"


/**
 *
 */
UCLASS()
class UMVVMConversionFunctionGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_BODY()

public:
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override
	{
		FPinConnectionResponse ConnectionResponse = Super::CanCreateConnection(A, B);
		if (ConnectionResponse.Response == CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE || ConnectionResponse.Response == CONNECT_RESPONSE_MAKE_WITH_PROMOTION)
		{
			ConnectionResponse.Response = CONNECT_RESPONSE_DISALLOW;
		}
		return ConnectionResponse;
	}
};