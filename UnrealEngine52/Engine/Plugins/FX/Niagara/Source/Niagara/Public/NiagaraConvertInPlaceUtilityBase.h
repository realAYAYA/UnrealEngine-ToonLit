// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Internationalization/Text.h"
#include "NiagaraConvertInPlaceUtilityBase.generated.h"

class UNiagaraScript;
class UNiagaraNodeFunctionCall;
class UNiagaraClipboardContent;
class UNiagaraStackFunctionInputCollection;

UCLASS(Abstract)
class NIAGARA_API UNiagaraConvertInPlaceUtilityBase : public UObject
{
	GENERATED_BODY()
public:
	virtual bool Convert(UNiagaraScript* InOldScript, UNiagaraClipboardContent* InOldClipboardContent, UNiagaraScript* InNewScript, UNiagaraStackFunctionInputCollection* InInputCollection, UNiagaraClipboardContent* InNewClipboardContent, UNiagaraNodeFunctionCall* InCallingNode, FText& OutMessage) { return true; };
};