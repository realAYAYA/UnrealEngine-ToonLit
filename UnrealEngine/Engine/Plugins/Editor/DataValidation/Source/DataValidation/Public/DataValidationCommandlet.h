// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"
#include "DataValidationCommandlet.generated.h"

UCLASS(CustomConstructor)
class DATAVALIDATION_API UDataValidationCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	UDataValidationCommandlet(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		LogToConsole = false;
	}

	// Begin UCommandlet Interface
	virtual int32 Main(const FString& FullCommandLine) override;
	// End UCommandlet Interface

	// do the validation without creating a commandlet
	static bool ValidateData(const FString& FullCommandLine);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "UObject/Interface.h"
#endif
