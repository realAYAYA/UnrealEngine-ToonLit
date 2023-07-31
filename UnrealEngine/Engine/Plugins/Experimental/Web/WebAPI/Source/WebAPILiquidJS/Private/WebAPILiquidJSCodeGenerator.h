// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CodeGen/WebAPICodeGenerator.h"

#include "WebAPILiquidJSCodeGenerator.generated.h"

/**
 * 
 */
UCLASS()
class WEBAPILIQUIDJS_API UWebAPILiquidJSCodeGenerator
	: public UWebAPICodeGeneratorBase
{
	GENERATED_BODY()
	
public:
	virtual TFuture<bool> IsAvailable() override;
	virtual TFuture<EWebAPIGenerationResult> GenerateFile(const TWeakObjectPtr<UWebAPIDefinition>& InDefinition, const TSharedPtr<FWebAPICodeGenFile>& InFile) override;
};
