// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

class UNiagaraSystem;
class FNiagaraEmitterInstance;

namespace FNiagaraComponentSettings
{
	extern void UpdateSettings();
	extern NIAGARA_API bool IsSystemAllowedToRun(const UNiagaraSystem* System);
	extern NIAGARA_API bool IsEmitterAllowedToRun(const FNiagaraEmitterInstance* EmitterInstance);
};
