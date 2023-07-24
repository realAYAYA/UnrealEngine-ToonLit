// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet2/CompilerResultsLog.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "WidgetCompilerRule.generated.h"

class FCompilerResultsLog;
class UWidgetBlueprint;

/**
 * 
 */
UCLASS(Abstract)
class UMGEDITOR_API UWidgetCompilerRule : public UObject
{
	GENERATED_BODY()
public:
	UWidgetCompilerRule();

	virtual void ExecuteRule(UWidgetBlueprint* WidgetBlueprint, FCompilerResultsLog& MessageLog);
};
