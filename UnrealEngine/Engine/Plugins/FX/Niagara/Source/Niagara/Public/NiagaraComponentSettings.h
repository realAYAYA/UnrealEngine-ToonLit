// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

class UNiagaraEmitter;
class UNiagaraSystem;
struct FVersionedNiagaraEmitterData;

namespace FNiagaraComponentSettings
{
	extern void UpdateSettings();
	extern NIAGARA_API bool IsSystemAllowedToRun(const UNiagaraSystem* System);
	extern NIAGARA_API bool IsEmitterAllowedToRun(const FVersionedNiagaraEmitterData& EmitterData, const UNiagaraEmitter& NiagaraEmitter);
};
