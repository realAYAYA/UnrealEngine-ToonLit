// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataSetCompiledData.generated.h"

/** Helper class defining the layout and location of an FNiagaraVariable in an FNiagaraDataBuffer-> */
USTRUCT()
struct FNiagaraVariableLayoutInfo
{
	GENERATED_BODY()

	/** Start index for the float components in the main buffer. */
	UPROPERTY()
	uint32 FloatComponentStart = 0;

	/** Start index for the int32 components in the main buffer. */
	UPROPERTY()
	uint32 Int32ComponentStart = 0;

	/** Start index for the half components in the main buffer. */
	UPROPERTY()
	uint32 HalfComponentStart = 0;

	uint32 GetNumFloatComponents()const { return LayoutInfo.FloatComponentByteOffsets.Num(); }
	uint32 GetNumInt32Components()const { return LayoutInfo.Int32ComponentByteOffsets.Num(); }
	uint32 GetNumHalfComponents()const { return LayoutInfo.HalfComponentByteOffsets.Num(); }

	/** This variable's type layout info. */
	UPROPERTY()
	FNiagaraTypeLayoutInfo LayoutInfo;
};

USTRUCT()
struct NIAGARA_API FNiagaraDataSetCompiledData
{
	GENERATED_BODY()

	/** Variables in the data set. */
	UPROPERTY()
	TArray<FNiagaraVariable> Variables;

	/** Data describing the layout of variable data. */
	UPROPERTY()
	TArray<FNiagaraVariableLayoutInfo> VariableLayouts;

	/** Unique ID for this DataSet. Used to allow referencing from other emitters and Systems. */
	UPROPERTY()
	FNiagaraDataSetID ID;

	/** Total number of components of each type in the data set. */
	UPROPERTY()
	uint32 TotalFloatComponents;

	UPROPERTY()
	uint32 TotalInt32Components;

	UPROPERTY()
	uint32 TotalHalfComponents;

	/** Whether or not this dataset require persistent IDs. */
	UPROPERTY()
	uint32 bRequiresPersistentIDs : 1;

	/** Sim target this DataSet is targeting (CPU/GPU). */
	UPROPERTY()
	ENiagaraSimTarget SimTarget;

	FNiagaraDataSetCompiledData();
	const FNiagaraVariableLayoutInfo* FindVariableLayoutInfo(const FNiagaraVariableBase& VariableDef) const;
	void BuildLayout();
	void Empty();

	static FNiagaraDataSetCompiledData DummyCompiledData;

	uint32 GetLayoutHash()const
	{
		if(CachedLayoutHash == INDEX_NONE)
		{
			CachedLayoutHash = 0;
			for(const FNiagaraVariableBase& Var : Variables)
			{
				CachedLayoutHash = HashCombine(CachedLayoutHash, GetTypeHash(Var));
			}
		}
		return CachedLayoutHash;
	}

	bool CheckHashConflict(const FNiagaraDataSetCompiledData& Other)const
	{
		//If we have the same hash ensure we have the same data.
		if(GetLayoutHash() == Other.GetLayoutHash())
		{
			return Variables != Other.Variables;			
		}
		return false;
	}

private:
	/** A hash of all variables in this layout. NOT persistent. Will differ from run to run. */
	mutable uint32 CachedLayoutHash = INDEX_NONE;

};

//////////////////////////////////////////////////////////////////////////

