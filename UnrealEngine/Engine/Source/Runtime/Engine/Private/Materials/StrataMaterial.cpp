// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrataMaterial.h"
#include "MaterialCompiler.h"



FString GetStrataBSDFName(uint8 BSDFType)
{
	switch (BSDFType)
	{
	case STRATA_BSDF_TYPE_SLAB:
		return TEXT("SLAB");
		break;
	case STRATA_BSDF_TYPE_VOLUMETRICFOGCLOUD:
		return TEXT("VOLUMETRICFOGCLOUD");
		break;
	case STRATA_BSDF_TYPE_UNLIT:
		return TEXT("UNLIT");
		break;
	case STRATA_BSDF_TYPE_HAIR:
		return TEXT("HAIR");
		break;
	case STRATA_BSDF_TYPE_EYE:
		return TEXT("EYE");
		break;
	case STRATA_BSDF_TYPE_SINGLELAYERWATER:
		return TEXT("SINGLELAYERWATER");
		break;
	}
	check(false);
	return "";
}

FStrataRegisteredSharedLocalBasis StrataCompilationInfoCreateNullSharedLocalBasis()
{
	return FStrataRegisteredSharedLocalBasis();
}

FStrataRegisteredSharedLocalBasis StrataCompilationInfoCreateSharedLocalBasis(FMaterialCompiler* Compiler, int32 NormalCodeChunk, int32 TangentCodeChunk)
{
	if (TangentCodeChunk == INDEX_NONE)
	{
		return Compiler->StrataCompilationInfoRegisterSharedLocalBasis(NormalCodeChunk);
	}
	return Compiler->StrataCompilationInfoRegisterSharedLocalBasis(NormalCodeChunk, TangentCodeChunk);
}


