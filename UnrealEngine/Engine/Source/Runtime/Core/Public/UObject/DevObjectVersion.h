// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "Misc/Guid.h"
#include "Serialization/CustomVersion.h"

struct CORE_API FDevSystemGuidRegistration
{
public:
	FDevSystemGuidRegistration(const TMap<FGuid, FGuid>& SystemGuids);
};

struct CORE_API FDevSystemGuids
{
	static const FDevSystemGuids& Get();
	static FGuid GetSystemGuid(FGuid System);
	
	FDevSystemGuids();
	const FGuid GLOBALSHADERMAP_DERIVEDDATA_VER;
	const FGuid LANDSCAPE_MOBILE_COOK_VERSION;
	const FGuid MATERIALSHADERMAP_DERIVEDDATA_VER;
	const FGuid NANITE_DERIVEDDATA_VER;
	const FGuid NIAGARASHADERMAP_DERIVEDDATA_VER;
	const FGuid Niagara_LatestScriptCompileVersion;
	const FGuid SkeletalMeshDerivedDataVersion;
	const FGuid STATICMESH_DERIVEDDATA_VER;
	const FGuid POSESEARCHDB_DERIVEDDATA_VER;
};

class CORE_API FDevVersionRegistration :  public FCustomVersionRegistration
{
public:
	/** @param InFriendlyName must be a string literal */
	template<int N>
	FDevVersionRegistration(FGuid InKey, int32 Version, const TCHAR(&InFriendlyName)[N], CustomVersionValidatorFunc InValidatorFunc = nullptr)
		: FCustomVersionRegistration(InKey, Version, InFriendlyName, InValidatorFunc)
	{
		RecordDevVersion(InKey);
	}

	/** Dumps all registered versions to log */
	static void DumpVersionsToLog();
private:
	static void RecordDevVersion(FGuid Key);
};