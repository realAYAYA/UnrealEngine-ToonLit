// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "NiagaraModule.h"
#include "NiagaraDataSet.h"

class FNiagaraBoundsCalculator
{
public:
	virtual ~FNiagaraBoundsCalculator() { }
	void InitAccessors(const FNiagaraDataSet& DataSet) { InitAccessors(&DataSet.GetCompiledData()); }
	virtual void InitAccessors(const FNiagaraDataSetCompiledData* CompiledData) = 0;
	virtual FBox CalculateBounds(const FTransform& SystemTransform, const FNiagaraDataSet& DataSet, const int32 NumInstances) const = 0;
};
