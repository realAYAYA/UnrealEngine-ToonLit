// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "NiagaraSimCache.h"

class FJsonObject;

/** Experimental Json interop for Niagara Sim Caches. */
struct FNiagaraSimCacheJson
{
	NIAGARA_API static void DumpToFile(const UNiagaraSimCache& SimCache, const FString& FullPath);

	NIAGARA_API static TSharedPtr<FJsonObject> ToJson(const UNiagaraSimCache& SimCache);
	NIAGARA_API static TSharedPtr<FJsonObject> EmitterFrameToJson(const UNiagaraSimCache& SimCache, int EmitterIndex, int FrameIndex);
};
