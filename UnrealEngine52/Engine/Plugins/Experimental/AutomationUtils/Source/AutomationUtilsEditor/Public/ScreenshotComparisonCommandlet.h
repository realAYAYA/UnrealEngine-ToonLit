// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Commandlets/Commandlet.h"
#include "ScreenshotComparisonCommandlet.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogScreenshotComparison, Log, All);

UCLASS(config = Editor)
class UScreenshotComparisonCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& CmdLineParams) override;
	//~ End UCommandlet Interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
