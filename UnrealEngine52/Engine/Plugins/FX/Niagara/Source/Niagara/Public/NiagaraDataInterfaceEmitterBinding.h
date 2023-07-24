// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceEmitterBinding.generated.h"

class UNiagaraEmitter;
class UNiagaraSystem;

UENUM()
enum class ENiagaraDataInterfaceEmitterBindingMode
{
	/** Attempt to bind to the emitter the data interface is used with, this may not be possible in all situations (i.e. user parameter). */
	Self,
	/** Attempt to bind to an emitter using it's name. */
	Other
};

USTRUCT()
struct FNiagaraDataInterfaceEmitterBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Binding")
	ENiagaraDataInterfaceEmitterBindingMode	BindingMode = ENiagaraDataInterfaceEmitterBindingMode::Other;
	
	UPROPERTY(EditAnywhere, Category = "Binding")
	FName EmitterName;

	/** Resolves the emitter binding, this can return nullptr if we failed to resolve */
	FNiagaraEmitterInstance* Resolve(FNiagaraSystemInstance* SystemInstance, UNiagaraDataInterface* DataInterface);
	/** Resolves the emitter binding, only returns a valid result when in Named mode as we can not determine the source otherwise. */
	UNiagaraEmitter* Resolve(UNiagaraSystem* NiagaraSystem);

	bool operator==(const FNiagaraDataInterfaceEmitterBinding& Other) const
	{
		return BindingMode == Other.BindingMode && EmitterName == Other.EmitterName;
	}
};
