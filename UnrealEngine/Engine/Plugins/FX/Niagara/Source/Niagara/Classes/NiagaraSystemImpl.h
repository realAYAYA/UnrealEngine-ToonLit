// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfacePlatformSet.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "NiagaraSystem.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceUtilities.h"


template<typename TAction>
void UNiagaraSystem::ForEachScript(TAction Func) const
{
	Func(SystemSpawnScript);
	Func(SystemUpdateScript);

	for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (Handle.GetIsEnabled())
		{
			if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
			{
				EmitterData->ForEachScript(Func);
			}
		}
	}
}

/** Performs the passed action for all FNiagaraPlatformSets in this system. */
template<typename TAction>
void UNiagaraSystem::ForEachPlatformSet(TAction Func)
{
	//Handle our scalability overrides
	for (FNiagaraSystemScalabilityOverride& Override : SystemScalabilityOverrides.Overrides)
	{
		Func(Override.Platforms);
	}

	//Handle and platform set User DIs.
	for (UNiagaraDataInterface* DI : GetExposedParameters().GetDataInterfaces())
	{
		if (UNiagaraDataInterfacePlatformSet* PlatformSetDI = Cast<UNiagaraDataInterfacePlatformSet>(DI))
		{
			Func(PlatformSetDI->Platforms);
		}
	}

	//Handle all platform set DIs held in scripts for this system.
	auto HandleScript = [Func](UNiagaraScript* NiagaraScript)
	{
		if (NiagaraScript)
		{
			for (const FNiagaraScriptResolvedDataInterfaceInfo& DataInterfaceInfo : NiagaraScript->GetResolvedDataInterfaces())
			{
				if (UNiagaraDataInterfacePlatformSet* PlatformSetDI = Cast<UNiagaraDataInterfacePlatformSet>(DataInterfaceInfo.ResolvedDataInterface))
				{
					Func(PlatformSetDI->Platforms);
				}
			}
		}
	};
	HandleScript(SystemSpawnScript);
	HandleScript(SystemUpdateScript);

	//Finally handle all our emitters.
	for (FNiagaraEmitterHandle& Handle : EmitterHandles)
	{
		if (FVersionedNiagaraEmitterData* Emitter = Handle.GetEmitterData())
		{
			Emitter->ForEachPlatformSet(Func);
		}
	}
}
