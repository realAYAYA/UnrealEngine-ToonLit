// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Builtin/BlueprintOnly/ParentFilter.h"
#include "AndFilter.generated.h"

UCLASS(meta = (InternalSnapshotFilter))
class LEVELSNAPSHOTFILTERS_API UAndFilter : public UParentFilter
{
	GENERATED_BODY()
public:

	//~ Begin ULevelSnapshotFilter Interface
	virtual EFilterResult::Type IsActorValid(const FIsActorValidParams& Params) const override;
	virtual EFilterResult::Type IsPropertyValid(const FIsPropertyValidParams& Params) const override;
	virtual EFilterResult::Type IsDeletedActorValid(const FIsDeletedActorValidParams& Params) const override;
	virtual EFilterResult::Type IsAddedActorValid(const FIsAddedActorValidParams& Params) const override;
	//~ End ULevelSnapshotFilter Interface

private:

	EFilterResult::Type EvaluateAndChain(TFunction<EFilterResult::Type(ULevelSnapshotFilter*)> EvaluateFilterCallback) const;
};
