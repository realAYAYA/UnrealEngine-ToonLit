// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/CoreMiscDefines.h"

#include "NiagaraTypes.h"

class UNiagaraDataInterface;
class UNiagaraSystem;

#if WITH_EDITORONLY_DATA

namespace FNiagaraResolveDIHelpers
{
	void ResolveDIs(UNiagaraSystem* System,	TArray<FText>& OutErrorMessages);
}

#endif