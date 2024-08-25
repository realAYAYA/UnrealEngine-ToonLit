// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessEmitterShaders.h"

namespace NiagaraStateless
{
	IMPLEMENT_GLOBAL_SHADER(FSimulationShaderDefaultCS, "/Plugin/FX/Niagara/Private/Stateless/NiagaraStatelessSimulationDefault.usf", "StatelessMain", SF_Compute);
	IMPLEMENT_GLOBAL_SHADER(FSimulationShaderExample1CS, "/Plugin/FX/Niagara/Private/Stateless/NiagaraStatelessSimulationExample1.usf", "StatelessMain", SF_Compute);
}
