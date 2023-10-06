// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraConvertInPlaceUtilityBase.h"
#include "NiagaraScript.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraConvertInPlaceEmitterAndSystemState.generated.h"

UCLASS(MinimalAPI)
class  UNiagaraConvertInPlaceEmitterAndSystemState : public UNiagaraConvertInPlaceUtilityBase
{
	GENERATED_BODY()
public:
	NIAGARAEDITOR_API virtual bool Convert(UNiagaraScript* InOldScript, UNiagaraClipboardContent* InOldClipboardContent, UNiagaraScript* InNewScript, UNiagaraStackFunctionInputCollection* InInputCollection, UNiagaraClipboardContent* InNewClipboardContent, UNiagaraNodeFunctionCall* InCallingNode, FText& OutMessage) override;
};
