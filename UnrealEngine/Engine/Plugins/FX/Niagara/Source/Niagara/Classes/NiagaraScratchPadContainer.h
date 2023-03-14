// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NiagaraScript.h"
#include "NiagaraScratchPadContainer.generated.h"

/**
 * Wrapper class for scratch pad scripts in an emitter. This is needed because each emitter version can have it's own copy of a scratch pad with the same name and they can't all be outered to the emitter that way.
 */
UCLASS()
class NIAGARA_API UNiagaraScratchPadContainer : public UObject
{
	GENERATED_BODY()
	
public:
	virtual void PostLoad() override;
	void CheckConsistency();
	void SetScripts(const TArray<TObjectPtr<UNiagaraScript>>& InScripts);
	void AppendScripts(const TArray<TObjectPtr<UNiagaraScript>>& InScripts);
	void AppendScripts(TObjectPtr<UNiagaraScratchPadContainer> InScripts);
	
	// The scripts get added in specific ordering through inheritance. These accessors just allow us to deal with checks 
	// for known scripts 
	int32 FindIndexForScript(UNiagaraScript* InScript) const;
	UNiagaraScript* FindScriptAtIndex(int32 Index) const;

#if WITH_EDITORONLY_DATA

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraScript>> Scripts;

#endif
};
