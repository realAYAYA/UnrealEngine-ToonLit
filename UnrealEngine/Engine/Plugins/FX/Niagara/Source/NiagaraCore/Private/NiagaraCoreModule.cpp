// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraCoreModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraDataInterfaceBase.h"

IMPLEMENT_MODULE(INiagaraCoreModule, NiagaraCore);

IMPLEMENT_TYPE_LAYOUT(FNiagaraDataInterfaceParametersCS);

IMPLEMENT_EXPORTED_INTRINSIC_TYPE_LAYOUT(TIndexedPtr<UNiagaraDataInterfaceBase>);

UNiagaraDataInterfaceBase::UNiagaraDataInterfaceBase(class FObjectInitializer const & Initializer)
{

}