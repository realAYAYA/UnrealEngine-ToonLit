// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/ExtensionData.h"

#include "MuR/MutableTrace.h"
#include "MuR/SerialisationPrivate.h"
#include "Templates/TypeHash.h"

namespace mu
{

//---------------------------------------------------------------------------------------------
void ExtensionData::Serialise(const ExtensionData* Data, OutputArchive& Archive)
{
	Archive << *Data;
}


//---------------------------------------------------------------------------------------------
ExtensionDataPtr ExtensionData::StaticUnserialise(InputArchive& Archive)
{
	MUTABLE_CPUPROFILER_SCOPE(ExtensionDataUnserialise);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

	ExtensionDataPtr Result = new ExtensionData();
	Archive >> *Result;

	return Result;
}


//---------------------------------------------------------------------------------------------
uint32 ExtensionData::Hash() const
{
	uint32 Result = ::GetTypeHash(Index);
	HashCombine(Result, ::GetTypeHash((uint8)Origin));

	return Result;
}


//---------------------------------------------------------------------------------------------
void ExtensionData::Serialise(OutputArchive& Archive) const
{
	Archive << Index;

	uint8 OriginByte = (uint8)Origin;
	Archive << OriginByte;
}


//---------------------------------------------------------------------------------------------
void ExtensionData::Unserialise(InputArchive& Archive)
{
	Archive >> Index;

	uint8 OriginByte;
	Archive >> OriginByte;
	Origin = (EOrigin)OriginByte;
}


//---------------------------------------------------------------------------------------------
int32 ExtensionData::GetDataSize() const
{
	return sizeof(ExtensionData);
}


}
