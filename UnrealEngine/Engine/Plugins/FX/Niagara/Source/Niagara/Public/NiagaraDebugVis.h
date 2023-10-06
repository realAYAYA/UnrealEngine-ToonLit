// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FNiagaraTypeRegistry;

#if !IS_MONOLITHIC
namespace NiagaraDebugVisHelper
{
	extern NIAGARA_API const FNiagaraTypeRegistry* GCoreTypeRegistrySingletonPtr;
}
#endif